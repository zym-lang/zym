#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <math.h>
#include "../natives.h"

typedef struct {
    uint64_t s[4];
} RandomState;

static inline uint64_t rotl(const uint64_t x, int k) {
    return (x << k) | (x >> (64 - k));
}

static uint64_t xoshiro256ss_next(RandomState* state) {
    const uint64_t result = rotl(state->s[1] * 5, 7) * 9;
    const uint64_t t = state->s[1] << 17;

    state->s[2] ^= state->s[0];
    state->s[3] ^= state->s[1];
    state->s[1] ^= state->s[2];
    state->s[0] ^= state->s[3];

    state->s[2] ^= t;
    state->s[3] = rotl(state->s[3], 45);

    return result;
}

static uint64_t splitmix64(uint64_t* state) {
    uint64_t z = (*state += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

static void random_seed(RandomState* state, uint64_t seed) {
    uint64_t sm_state = seed;
    state->s[0] = splitmix64(&sm_state);
    state->s[1] = splitmix64(&sm_state);
    state->s[2] = splitmix64(&sm_state);
    state->s[3] = splitmix64(&sm_state);
}

void random_cleanup(ZymVM* vm, void* ptr) {
    RandomState* state = (RandomState*)ptr;
    free(state);
}

ZymValue random_random(ZymVM* vm, ZymValue context) {
    RandomState* state = (RandomState*)zym_getNativeData(context);
    uint64_t x = xoshiro256ss_next(state);
    double result = (x >> 11) * 0x1.0p-53;

    return zym_newNumber(result);
}

ZymValue random_randint(ZymVM* vm, ZymValue context, ZymValue minVal, ZymValue maxVal) {
    RandomState* state = (RandomState*)zym_getNativeData(context);

    if (!zym_isNumber(minVal) || !zym_isNumber(maxVal)) {
        zym_runtimeError(vm, "randint() requires two number arguments");
        return ZYM_ERROR;
    }

    double min_d = zym_asNumber(minVal);
    double max_d = zym_asNumber(maxVal);

    int64_t min = (int64_t)min_d;
    int64_t max = (int64_t)max_d;

    if (min > max) {
        zym_runtimeError(vm, "randint() min (%lld) must be <= max (%lld)",
                        (long long)min, (long long)max);
        return ZYM_ERROR;
    }

    uint64_t range = (uint64_t)(max - min) + 1;
    uint64_t limit = UINT64_MAX - (UINT64_MAX % range);
    uint64_t x;

    do {
        x = xoshiro256ss_next(state);
    } while (x >= limit);

    int64_t result = min + (int64_t)(x % range);
    return zym_newNumber((double)result);
}

ZymValue random_uniform(ZymVM* vm, ZymValue context, ZymValue minVal, ZymValue maxVal) {
    RandomState* state = (RandomState*)zym_getNativeData(context);

    if (!zym_isNumber(minVal) || !zym_isNumber(maxVal)) {
        zym_runtimeError(vm, "uniform() requires two number arguments");
        return ZYM_ERROR;
    }

    double min = zym_asNumber(minVal);
    double max = zym_asNumber(maxVal);

    if (min >= max) {
        zym_runtimeError(vm, "uniform() min (%.6f) must be < max (%.6f)", min, max);
        return ZYM_ERROR;
    }

    uint64_t x = xoshiro256ss_next(state);
    double unit = (x >> 11) * 0x1.0p-53;
    double result = min + unit * (max - min);

    return zym_newNumber(result);
}

ZymValue random_chance(ZymVM* vm, ZymValue context, ZymValue probabilityVal) {
    RandomState* state = (RandomState*)zym_getNativeData(context);

    if (!zym_isNumber(probabilityVal)) {
        zym_runtimeError(vm, "chance() requires a number argument");
        return ZYM_ERROR;
    }

    double probability = zym_asNumber(probabilityVal);

    if (probability < 0.0 || probability > 1.0) {
        zym_runtimeError(vm, "chance() probability must be in [0, 1], got %.6f", probability);
        return ZYM_ERROR;
    }

    uint64_t x = xoshiro256ss_next(state);
    double unit = (x >> 11) * 0x1.0p-53;

    return zym_newBool(unit < probability);
}

ZymValue random_choice(ZymVM* vm, ZymValue context, ZymValue listVal) {
    RandomState* state = (RandomState*)zym_getNativeData(context);

    if (!zym_isList(listVal)) {
        zym_runtimeError(vm, "choice() requires a list argument");
        return ZYM_ERROR;
    }

    int len = zym_listLength(listVal);
    if (len == 0) {
        zym_runtimeError(vm, "choice() cannot choose from empty list");
        return ZYM_ERROR;
    }

    uint64_t x = xoshiro256ss_next(state);
    int index = (int)(x % (uint64_t)len);

    return zym_listGet(vm, listVal, index);
}

ZymValue random_shuffle(ZymVM* vm, ZymValue context, ZymValue listVal) {
    RandomState* state = (RandomState*)zym_getNativeData(context);

    if (!zym_isList(listVal)) {
        zym_runtimeError(vm, "shuffle() requires a list argument");
        return ZYM_ERROR;
    }

    int len = zym_listLength(listVal);
    if (len <= 1) {
        return zym_newNull();
    }

    for (int i = len - 1; i > 0; i--) {
        uint64_t x = xoshiro256ss_next(state);
        int j = (int)(x % (uint64_t)(i + 1));

        ZymValue temp = zym_listGet(vm, listVal, i);
        ZymValue swap = zym_listGet(vm, listVal, j);

        zym_listSet(vm, listVal, i, swap);
        zym_listSet(vm, listVal, j, temp);
    }

    return zym_newNull();
}

ZymValue random_sample(ZymVM* vm, ZymValue context, ZymValue listVal, ZymValue kVal) {
    RandomState* state = (RandomState*)zym_getNativeData(context);

    if (!zym_isList(listVal)) {
        zym_runtimeError(vm, "sample() requires a list as first argument");
        return ZYM_ERROR;
    }

    if (!zym_isNumber(kVal)) {
        zym_runtimeError(vm, "sample() requires a number as second argument");
        return ZYM_ERROR;
    }

    int len = zym_listLength(listVal);
    int k = (int)zym_asNumber(kVal);

    if (k < 0) {
        zym_runtimeError(vm, "sample() k must be non-negative, got %d", k);
        return ZYM_ERROR;
    }

    if (k > len) {
        zym_runtimeError(vm, "sample() k (%d) cannot exceed list length (%d)", k, len);
        return ZYM_ERROR;
    }

    ZymValue result = zym_newList(vm);
    zym_pushRoot(vm, result);

    if (k == 0) {
        zym_popRoot(vm);
        return result;
    }

    int* indices = malloc(len * sizeof(int));
    if (!indices) {
        zym_popRoot(vm);
        zym_runtimeError(vm, "Out of memory");
        return ZYM_ERROR;
    }

    for (int i = 0; i < len; i++) {
        indices[i] = i;
    }

    for (int i = 0; i < k; i++) {
        uint64_t x = xoshiro256ss_next(state);
        int j = i + (int)(x % (uint64_t)(len - i));

        int temp = indices[i];
        indices[i] = indices[j];
        indices[j] = temp;

        ZymValue elem = zym_listGet(vm, listVal, indices[i]);
        zym_listAppend(vm, result, elem);
    }

    free(indices);
    zym_popRoot(vm);
    return result;
}

ZymValue random_gaussian(ZymVM* vm, ZymValue context, ZymValue meanVal, ZymValue stddevVal) {
    RandomState* state = (RandomState*)zym_getNativeData(context);

    if (!zym_isNumber(meanVal) || !zym_isNumber(stddevVal)) {
        zym_runtimeError(vm, "gaussian() requires two number arguments");
        return ZYM_ERROR;
    }

    double mean = zym_asNumber(meanVal);
    double stddev = zym_asNumber(stddevVal);

    if (stddev <= 0.0) {
        zym_runtimeError(vm, "gaussian() standard deviation must be positive, got %.6f", stddev);
        return ZYM_ERROR;
    }

    uint64_t x1 = xoshiro256ss_next(state);
    uint64_t x2 = xoshiro256ss_next(state);

    double u1 = (x1 >> 11) * 0x1.0p-53;
    double u2 = (x2 >> 11) * 0x1.0p-53;

    if (u1 < 1e-10) u1 = 1e-10;

    double z0 = sqrt(-2.0 * log(u1)) * cos(2.0 * 3.14159265358979323846 * u2);
    double result = mean + z0 * stddev;

    return zym_newNumber(result);
}

ZymValue random_bytes(ZymVM* vm, ZymValue context, ZymValue countVal) {
    RandomState* state = (RandomState*)zym_getNativeData(context);

    if (!zym_isNumber(countVal)) {
        zym_runtimeError(vm, "bytes() requires a number argument");
        return ZYM_ERROR;
    }

    int count = (int)zym_asNumber(countVal);
    if (count < 0) {
        zym_runtimeError(vm, "bytes() count must be non-negative, got %d", count);
        return ZYM_ERROR;
    }

    if (count > 1000000) {
        zym_runtimeError(vm, "bytes() count too large (max 1000000), got %d", count);
        return ZYM_ERROR;
    }

    ZymValue result = zym_newList(vm);
    zym_pushRoot(vm, result);

    for (int i = 0; i < count; i++) {
        if (i % 8 == 0) {
            uint64_t x = xoshiro256ss_next(state);
            for (int j = 0; j < 8 && i + j < count; j++) {
                uint8_t byte = (uint8_t)((x >> (j * 8)) & 0xFF);
                zym_listAppend(vm, result, zym_newNumber((double)byte));
            }
            i += 7;
        }
    }

    zym_popRoot(vm);
    return result;
}

typedef struct {
    uint8_t* data;
    size_t capacity;
    size_t length;
    size_t position;
    ZymValue position_ref;
    ZymValue length_ref;
    bool auto_grow;
    int endianness;
} BufferData;

ZymValue random_bytesBuffer(ZymVM* vm, ZymValue context, ZymValue bufferVal) {
    RandomState* state = (RandomState*)zym_getNativeData(context);

    if (!zym_isMap(bufferVal)) {
        zym_runtimeError(vm, "bytesBuffer() requires a Buffer argument");
        return ZYM_ERROR;
    }

    ZymValue getLength = zym_mapGet(vm, bufferVal, "getLength");
    if (zym_isNull(getLength)) {
        zym_runtimeError(vm, "Argument is not a valid Buffer");
        return ZYM_ERROR;
    }

    ZymValue bufferContext = zym_getClosureContext(getLength);
    BufferData* buf = (BufferData*)zym_getNativeData(bufferContext);
    if (!buf) {
        zym_runtimeError(vm, "Failed to get buffer data");
        return ZYM_ERROR;
    }

    size_t available_space = buf->capacity - buf->position;
    if (available_space == 0) {
        return zym_newNumber(0.0);
    }

    size_t bytes_written = 0;
    size_t pos = buf->position;

    while (pos < buf->capacity) {
        uint64_t x = xoshiro256ss_next(state);

        for (int j = 0; j < 8 && pos < buf->capacity; j++) {
            buf->data[pos++] = (uint8_t)((x >> (j * 8)) & 0xFF);
            bytes_written++;
        }
    }

    buf->position = pos;
    if (buf->position > buf->length) {
        buf->length = buf->position;
        buf->length_ref = zym_newNumber((double)buf->length);
    }
    buf->position_ref = zym_newNumber((double)buf->position);

    return zym_newNumber((double)bytes_written);
}

ZymValue random_seed_method(ZymVM* vm, ZymValue context, ZymValue seedVal) {
    RandomState* state = (RandomState*)zym_getNativeData(context);

    if (!zym_isNumber(seedVal)) {
        zym_runtimeError(vm, "seed() requires a number argument");
        return ZYM_ERROR;
    }

    double seed_d = zym_asNumber(seedVal);
    uint64_t seed = (uint64_t)seed_d;

    if (seed == 0) {
        seed = (uint64_t)time(NULL);
    }

    random_seed(state, seed);
    return zym_newNull();
}

ZymValue nativeRandom_create(ZymVM* vm, ZymValue seedVal) {
    RandomState* state = calloc(1, sizeof(RandomState));
    if (!state) {
        zym_runtimeError(vm, "Out of memory");
        return ZYM_ERROR;
    }

    uint64_t seed;
    if (zym_isNull(seedVal)) {
        seed = (uint64_t)time(NULL) ^ (uint64_t)(uintptr_t)state;
    } else if (zym_isNumber(seedVal)) {
        seed = (uint64_t)zym_asNumber(seedVal);
    } else {
        free(state);
        zym_runtimeError(vm, "Random() seed must be a number or null");
        return ZYM_ERROR;
    }

    random_seed(state, seed);

    ZymValue context = zym_createNativeContext(vm, state, random_cleanup);
    zym_pushRoot(vm, context);

    #define CREATE_METHOD_0(name, func) \
        ZymValue name = zym_createNativeClosure(vm, #func "()", func, context); \
        zym_pushRoot(vm, name);

    #define CREATE_METHOD_1(name, func) \
        ZymValue name = zym_createNativeClosure(vm, #func "(arg)", func, context); \
        zym_pushRoot(vm, name);

    #define CREATE_METHOD_2(name, func) \
        ZymValue name = zym_createNativeClosure(vm, #func "(arg1, arg2)", func, context); \
        zym_pushRoot(vm, name);

    CREATE_METHOD_0(random, random_random);
    CREATE_METHOD_2(randint, random_randint);
    CREATE_METHOD_2(uniform, random_uniform);
    CREATE_METHOD_1(chance, random_chance);
    CREATE_METHOD_1(choice, random_choice);
    CREATE_METHOD_1(shuffle, random_shuffle);
    CREATE_METHOD_2(sample, random_sample);
    CREATE_METHOD_2(gaussian, random_gaussian);
    CREATE_METHOD_1(bytes, random_bytes);
    CREATE_METHOD_1(bytesBuffer, random_bytesBuffer);
    CREATE_METHOD_1(seedMethod, random_seed_method);

    #undef CREATE_METHOD_0
    #undef CREATE_METHOD_1
    #undef CREATE_METHOD_2

    ZymValue obj = zym_newMap(vm);
    zym_pushRoot(vm, obj);

    zym_mapSet(vm, obj, "random", random);
    zym_mapSet(vm, obj, "randint", randint);
    zym_mapSet(vm, obj, "uniform", uniform);
    zym_mapSet(vm, obj, "chance", chance);
    zym_mapSet(vm, obj, "choice", choice);
    zym_mapSet(vm, obj, "shuffle", shuffle);
    zym_mapSet(vm, obj, "sample", sample);
    zym_mapSet(vm, obj, "gaussian", gaussian);
    zym_mapSet(vm, obj, "bytes", bytes);
    zym_mapSet(vm, obj, "bytesBuffer", bytesBuffer);
    zym_mapSet(vm, obj, "seed", seedMethod);

    for (int i = 0; i < 13; i++) {
        zym_popRoot(vm);
    }

    return obj;
}

ZymValue nativeRandom_create_auto(ZymVM* vm) {
    return nativeRandom_create(vm, zym_newNull());
}

ZymValue nativeRandom_create_seeded(ZymVM* vm, ZymValue seedVal) {
    return nativeRandom_create(vm, seedVal);
}
