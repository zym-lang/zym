#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "./natives.h"

typedef enum {
    ENDIAN_LITTLE,
    ENDIAN_BIG
} Endianness;

typedef struct {
    uint8_t* data;
    size_t capacity;
    size_t length;
    size_t position;
    ZymValue position_ref;
    ZymValue length_ref;
    bool auto_grow;
    Endianness endianness;
} BufferData;

static inline uint16_t swap_uint16(uint16_t val) {
    return (val << 8) | (val >> 8);
}

static inline uint32_t swap_uint32(uint32_t val) {
    return ((val << 24) & 0xFF000000) |
           ((val << 8)  & 0x00FF0000) |
           ((val >> 8)  & 0x0000FF00) |
           ((val >> 24) & 0x000000FF);
}

static inline uint64_t swap_uint64(uint64_t val) {
    return ((val << 56) & 0xFF00000000000000ULL) |
           ((val << 40) & 0x00FF000000000000ULL) |
           ((val << 24) & 0x0000FF0000000000ULL) |
           ((val << 8)  & 0x000000FF00000000ULL) |
           ((val >> 8)  & 0x00000000FF000000ULL) |
           ((val >> 24) & 0x0000000000FF0000ULL) |
           ((val >> 40) & 0x000000000000FF00ULL) |
           ((val >> 56) & 0x00000000000000FFULL);
}

static bool is_little_endian() {
    uint16_t test = 1;
    return *((uint8_t*)&test) == 1;
}

void buffer_cleanup(ZymVM* vm, void* ptr) {
    BufferData* buf = (BufferData*)ptr;
    free(buf->data);
    free(buf);
}

static bool ensure_capacity(ZymVM* vm, BufferData* buf, size_t needed) {
    size_t required = buf->position + needed;

    if (required <= buf->capacity) {
        return true;
    }

    if (!buf->auto_grow) {
        zym_runtimeError(vm, "Buffer overflow: need %zu bytes, capacity is %zu",
                         required, buf->capacity);
        return false;
    }

    size_t new_capacity = buf->capacity + (buf->capacity >> 1);
    if (new_capacity < required) {
        new_capacity = required;
    }

    if (new_capacity > 100 * 1024 * 1024) {
        zym_runtimeError(vm, "Buffer exceeded maximum size (100MB)");
        return false;
    }

    uint8_t* new_data = realloc(buf->data, new_capacity);
    if (!new_data) {
        zym_runtimeError(vm, "Out of memory (failed to allocate %zu bytes)", new_capacity);
        return false;
    }

    memset(new_data + buf->capacity, 0, new_capacity - buf->capacity);

    buf->data = new_data;
    buf->capacity = new_capacity;
    return true;
}

static inline void update_length(BufferData* buf) {
    if (buf->position > buf->length) {
        buf->length = buf->position;
        buf->length_ref = zym_newNumber((double)buf->length);
    }
}

static inline void sync_position(BufferData* buf) {
    buf->position_ref = zym_newNumber((double)buf->position);
}

static inline void sync_length(BufferData* buf) {
    buf->length_ref = zym_newNumber((double)buf->length);
}

ZymValue buffer_readUInt8(ZymVM* vm, ZymValue context) {
    BufferData* buf = (BufferData*)zym_getNativeData(context);

    if (buf->position + 1 > buf->length) {
        zym_runtimeError(vm, "Read past end of buffer (pos=%zu, length=%zu)",
                         buf->position, buf->length);
        return ZYM_ERROR;
    }

    uint8_t val = buf->data[buf->position++];
    sync_position(buf);
    return zym_newNumber((double)val);
}

ZymValue buffer_readInt8(ZymVM* vm, ZymValue context) {
    BufferData* buf = (BufferData*)zym_getNativeData(context);

    if (buf->position + 1 > buf->length) {
        zym_runtimeError(vm, "Read past end of buffer");
        return ZYM_ERROR;
    }

    int8_t val = (int8_t)buf->data[buf->position++];
    sync_position(buf);
    return zym_newNumber((double)val);
}

ZymValue buffer_readUInt16(ZymVM* vm, ZymValue context) {
    BufferData* buf = (BufferData*)zym_getNativeData(context);

    if (buf->position + 2 > buf->length) {
        zym_runtimeError(vm, "Read past end of buffer");
        return ZYM_ERROR;
    }

    uint16_t val;
    memcpy(&val, buf->data + buf->position, 2);

    bool system_le = is_little_endian();
    bool need_swap = (buf->endianness == ENDIAN_LITTLE && !system_le) ||
                     (buf->endianness == ENDIAN_BIG && system_le);
    if (need_swap) {
        val = swap_uint16(val);
    }

    buf->position += 2;
    sync_position(buf);
    return zym_newNumber((double)val);
}

ZymValue buffer_readInt16(ZymVM* vm, ZymValue context) {
    BufferData* buf = (BufferData*)zym_getNativeData(context);

    if (buf->position + 2 > buf->length) {
        zym_runtimeError(vm, "Read past end of buffer");
        return ZYM_ERROR;
    }

    int16_t val;
    memcpy(&val, buf->data + buf->position, 2);

    bool system_le = is_little_endian();
    bool need_swap = (buf->endianness == ENDIAN_LITTLE && !system_le) ||
                     (buf->endianness == ENDIAN_BIG && system_le);
    if (need_swap) {
        val = (int16_t)swap_uint16((uint16_t)val);
    }

    buf->position += 2;
    sync_position(buf);
    return zym_newNumber((double)val);
}

ZymValue buffer_readUInt32(ZymVM* vm, ZymValue context) {
    BufferData* buf = (BufferData*)zym_getNativeData(context);

    if (buf->position + 4 > buf->length) {
        zym_runtimeError(vm, "Read past end of buffer");
        return ZYM_ERROR;
    }

    uint32_t val;
    memcpy(&val, buf->data + buf->position, 4);

    bool system_le = is_little_endian();
    bool need_swap = (buf->endianness == ENDIAN_LITTLE && !system_le) ||
                     (buf->endianness == ENDIAN_BIG && system_le);
    if (need_swap) {
        val = swap_uint32(val);
    }

    buf->position += 4;
    sync_position(buf);
    return zym_newNumber((double)val);
}

ZymValue buffer_readInt32(ZymVM* vm, ZymValue context) {
    BufferData* buf = (BufferData*)zym_getNativeData(context);

    if (buf->position + 4 > buf->length) {
        zym_runtimeError(vm, "Read past end of buffer");
        return ZYM_ERROR;
    }

    int32_t val;
    memcpy(&val, buf->data + buf->position, 4);

    bool system_le = is_little_endian();
    bool need_swap = (buf->endianness == ENDIAN_LITTLE && !system_le) ||
                     (buf->endianness == ENDIAN_BIG && system_le);
    if (need_swap) {
        val = (int32_t)swap_uint32((uint32_t)val);
    }

    buf->position += 4;
    sync_position(buf);
    return zym_newNumber((double)val);
}

ZymValue buffer_readFloat(ZymVM* vm, ZymValue context) {
    BufferData* buf = (BufferData*)zym_getNativeData(context);

    if (buf->position + 4 > buf->length) {
        zym_runtimeError(vm, "Read past end of buffer");
        return ZYM_ERROR;
    }

    uint32_t bits;
    memcpy(&bits, buf->data + buf->position, 4);

    bool system_le = is_little_endian();
    bool need_swap = (buf->endianness == ENDIAN_LITTLE && !system_le) ||
                     (buf->endianness == ENDIAN_BIG && system_le);
    if (need_swap) {
        bits = swap_uint32(bits);
    }

    float val;
    memcpy(&val, &bits, 4);

    buf->position += 4;
    sync_position(buf);
    return zym_newNumber((double)val);
}

ZymValue buffer_readDouble(ZymVM* vm, ZymValue context) {
    BufferData* buf = (BufferData*)zym_getNativeData(context);

    if (buf->position + 8 > buf->length) {
        zym_runtimeError(vm, "Read past end of buffer");
        return ZYM_ERROR;
    }

    uint64_t bits;
    memcpy(&bits, buf->data + buf->position, 8);

    bool system_le = is_little_endian();
    bool need_swap = (buf->endianness == ENDIAN_LITTLE && !system_le) ||
                     (buf->endianness == ENDIAN_BIG && system_le);
    if (need_swap) {
        bits = swap_uint64(bits);
    }

    double val;
    memcpy(&val, &bits, 8);

    buf->position += 8;
    sync_position(buf);
    return zym_newNumber(val);
}

ZymValue buffer_readBytes(ZymVM* vm, ZymValue context, ZymValue countVal) {
    BufferData* buf = (BufferData*)zym_getNativeData(context);

    if (!zym_isNumber(countVal)) {
        zym_runtimeError(vm, "readBytes() requires a number argument");
        return ZYM_ERROR;
    }

    size_t count = (size_t)zym_asNumber(countVal);
    if (buf->position + count > buf->length) {
        zym_runtimeError(vm, "Read past end of buffer");
        return ZYM_ERROR;
    }

    ZymValue list = zym_newList(vm);
    zym_pushRoot(vm, list);

    for (size_t i = 0; i < count; i++) {
        zym_listAppend(vm, list, zym_newNumber((double)buf->data[buf->position++]));
    }

    sync_position(buf);
    zym_popRoot(vm);
    return list;
}

ZymValue buffer_readString(ZymVM* vm, ZymValue context) {
    BufferData* buf = (BufferData*)zym_getNativeData(context);

    size_t start = buf->position;
    while (buf->position < buf->length && buf->data[buf->position] != 0) {
        buf->position++;
    }

    if (buf->position >= buf->length) {
        zym_runtimeError(vm, "No null terminator found");
        return ZYM_ERROR;
    }

    size_t len = buf->position - start;
    char* str = malloc(len + 1);
    if (!str) {
        zym_runtimeError(vm, "Out of memory");
        return ZYM_ERROR;
    }

    memcpy(str, buf->data + start, len);
    str[len] = '\0';
    buf->position++;  // Skip null terminator
    sync_position(buf);

    ZymValue result = zym_newString(vm, str);
    free(str);
    return result;
}

ZymValue buffer_readStringN(ZymVM* vm, ZymValue context, ZymValue countVal) {
    BufferData* buf = (BufferData*)zym_getNativeData(context);

    if (!zym_isNumber(countVal)) {
        zym_runtimeError(vm, "readStringN() requires a number argument");
        return ZYM_ERROR;
    }

    size_t count = (size_t)zym_asNumber(countVal);
    if (buf->position + count > buf->length) {
        zym_runtimeError(vm, "Read past end of buffer");
        return ZYM_ERROR;
    }

    char* str = malloc(count + 1);
    if (!str) {
        zym_runtimeError(vm, "Out of memory");
        return ZYM_ERROR;
    }

    memcpy(str, buf->data + buf->position, count);
    str[count] = '\0';
    buf->position += count;
    sync_position(buf);

    ZymValue result = zym_newString(vm, str);
    free(str);
    return result;
}

ZymValue buffer_writeUInt8(ZymVM* vm, ZymValue context, ZymValue valVal) {
    BufferData* buf = (BufferData*)zym_getNativeData(context);

    if (!zym_isNumber(valVal)) {
        zym_runtimeError(vm, "writeUInt8() requires a number argument");
        return ZYM_ERROR;
    }

    if (!ensure_capacity(vm, buf, 1)) {
        return ZYM_ERROR;
    }

    buf->data[buf->position++] = (uint8_t)zym_asNumber(valVal);
    update_length(buf);
    sync_position(buf);
    return context;
}

ZymValue buffer_writeInt8(ZymVM* vm, ZymValue context, ZymValue valVal) {
    BufferData* buf = (BufferData*)zym_getNativeData(context);

    if (!zym_isNumber(valVal)) {
        zym_runtimeError(vm, "writeInt8() requires a number argument");
        return ZYM_ERROR;
    }

    if (!ensure_capacity(vm, buf, 1)) {
        return ZYM_ERROR;
    }

    buf->data[buf->position++] = (uint8_t)(int8_t)zym_asNumber(valVal);
    update_length(buf);
    sync_position(buf);
    return context;
}

ZymValue buffer_writeUInt16(ZymVM* vm, ZymValue context, ZymValue valVal) {
    BufferData* buf = (BufferData*)zym_getNativeData(context);

    if (!zym_isNumber(valVal)) {
        zym_runtimeError(vm, "writeUInt16() requires a number argument");
        return ZYM_ERROR;
    }

    if (!ensure_capacity(vm, buf, 2)) {
        return ZYM_ERROR;
    }

    uint16_t val = (uint16_t)zym_asNumber(valVal);

    bool system_le = is_little_endian();
    bool need_swap = (buf->endianness == ENDIAN_LITTLE && !system_le) ||
                     (buf->endianness == ENDIAN_BIG && system_le);
    if (need_swap) {
        val = swap_uint16(val);
    }

    memcpy(buf->data + buf->position, &val, 2);
    buf->position += 2;
    update_length(buf);
    sync_position(buf);
    return context;
}

ZymValue buffer_writeInt16(ZymVM* vm, ZymValue context, ZymValue valVal) {
    BufferData* buf = (BufferData*)zym_getNativeData(context);

    if (!zym_isNumber(valVal)) {
        zym_runtimeError(vm, "writeInt16() requires a number argument");
        return ZYM_ERROR;
    }

    if (!ensure_capacity(vm, buf, 2)) {
        return ZYM_ERROR;
    }

    int16_t val = (int16_t)zym_asNumber(valVal);

    bool system_le = is_little_endian();
    bool need_swap = (buf->endianness == ENDIAN_LITTLE && !system_le) ||
                     (buf->endianness == ENDIAN_BIG && system_le);
    if (need_swap) {
        val = (int16_t)swap_uint16((uint16_t)val);
    }

    memcpy(buf->data + buf->position, &val, 2);
    buf->position += 2;
    update_length(buf);
    sync_position(buf);
    return context;
}

ZymValue buffer_writeUInt32(ZymVM* vm, ZymValue context, ZymValue valVal) {
    BufferData* buf = (BufferData*)zym_getNativeData(context);

    if (!zym_isNumber(valVal)) {
        zym_runtimeError(vm, "writeUInt32() requires a number argument");
        return ZYM_ERROR;
    }

    if (!ensure_capacity(vm, buf, 4)) {
        return ZYM_ERROR;
    }

    uint32_t val = (uint32_t)zym_asNumber(valVal);

    bool system_le = is_little_endian();
    bool need_swap = (buf->endianness == ENDIAN_LITTLE && !system_le) ||
                     (buf->endianness == ENDIAN_BIG && system_le);
    if (need_swap) {
        val = swap_uint32(val);
    }

    memcpy(buf->data + buf->position, &val, 4);
    buf->position += 4;
    update_length(buf);
    sync_position(buf);
    return context;
}

ZymValue buffer_writeInt32(ZymVM* vm, ZymValue context, ZymValue valVal) {
    BufferData* buf = (BufferData*)zym_getNativeData(context);

    if (!zym_isNumber(valVal)) {
        zym_runtimeError(vm, "writeInt32() requires a number argument");
        return ZYM_ERROR;
    }

    if (!ensure_capacity(vm, buf, 4)) {
        return ZYM_ERROR;
    }

    int32_t val = (int32_t)zym_asNumber(valVal);

    bool system_le = is_little_endian();
    bool need_swap = (buf->endianness == ENDIAN_LITTLE && !system_le) ||
                     (buf->endianness == ENDIAN_BIG && system_le);
    if (need_swap) {
        val = (int32_t)swap_uint32((uint32_t)val);
    }

    memcpy(buf->data + buf->position, &val, 4);
    buf->position += 4;
    update_length(buf);
    sync_position(buf);
    return context;
}

ZymValue buffer_writeFloat(ZymVM* vm, ZymValue context, ZymValue valVal) {
    BufferData* buf = (BufferData*)zym_getNativeData(context);

    if (!zym_isNumber(valVal)) {
        zym_runtimeError(vm, "writeFloat() requires a number argument");
        return ZYM_ERROR;
    }

    if (!ensure_capacity(vm, buf, 4)) {
        return ZYM_ERROR;
    }

    float val = (float)zym_asNumber(valVal);
    uint32_t bits;
    memcpy(&bits, &val, 4);

    bool system_le = is_little_endian();
    bool need_swap = (buf->endianness == ENDIAN_LITTLE && !system_le) ||
                     (buf->endianness == ENDIAN_BIG && system_le);
    if (need_swap) {
        bits = swap_uint32(bits);
    }

    memcpy(buf->data + buf->position, &bits, 4);
    buf->position += 4;
    update_length(buf);
    sync_position(buf);
    return context;
}

ZymValue buffer_writeDouble(ZymVM* vm, ZymValue context, ZymValue valVal) {
    BufferData* buf = (BufferData*)zym_getNativeData(context);

    if (!zym_isNumber(valVal)) {
        zym_runtimeError(vm, "writeDouble() requires a number argument");
        return ZYM_ERROR;
    }

    if (!ensure_capacity(vm, buf, 8)) {
        return ZYM_ERROR;
    }

    double val = zym_asNumber(valVal);
    uint64_t bits;
    memcpy(&bits, &val, 8);

    bool system_le = is_little_endian();
    bool need_swap = (buf->endianness == ENDIAN_LITTLE && !system_le) ||
                     (buf->endianness == ENDIAN_BIG && system_le);
    if (need_swap) {
        bits = swap_uint64(bits);
    }

    memcpy(buf->data + buf->position, &bits, 8);
    buf->position += 8;
    update_length(buf);
    sync_position(buf);
    return context;
}

ZymValue buffer_writeBytes(ZymVM* vm, ZymValue context, ZymValue listVal) {
    BufferData* buf = (BufferData*)zym_getNativeData(context);

    if (!zym_isList(listVal)) {
        zym_runtimeError(vm, "writeBytes() requires a list argument");
        return ZYM_ERROR;
    }

    size_t count = zym_listLength(listVal);
    if (!ensure_capacity(vm, buf, count)) {
        return ZYM_ERROR;
    }

    for (size_t i = 0; i < count; i++) {
        ZymValue val = zym_listGet(vm, listVal, i);
        if (!zym_isNumber(val)) {
            zym_runtimeError(vm, "writeBytes() requires list of numbers");
            return ZYM_ERROR;
        }
        buf->data[buf->position++] = (uint8_t)zym_asNumber(val);
    }

    update_length(buf);
    sync_position(buf);
    return context;
}

ZymValue buffer_writeString(ZymVM* vm, ZymValue context, ZymValue strVal) {
    BufferData* buf = (BufferData*)zym_getNativeData(context);

    if (!zym_isString(strVal)) {
        zym_runtimeError(vm, "writeString() requires a string argument");
        return ZYM_ERROR;
    }

    const char* str = zym_asCString(strVal);
    size_t len = strlen(str) + 1;  // Include null terminator

    if (!ensure_capacity(vm, buf, len)) {
        return ZYM_ERROR;
    }

    memcpy(buf->data + buf->position, str, len);
    buf->position += len;
    update_length(buf);
    sync_position(buf);
    return context;
}

ZymValue buffer_writeStringRaw(ZymVM* vm, ZymValue context, ZymValue strVal) {
    BufferData* buf = (BufferData*)zym_getNativeData(context);

    if (!zym_isString(strVal)) {
        zym_runtimeError(vm, "writeStringRaw() requires a string argument");
        return ZYM_ERROR;
    }

    const char* str = zym_asCString(strVal);
    size_t len = strlen(str);

    if (!ensure_capacity(vm, buf, len)) {
        return ZYM_ERROR;
    }

    memcpy(buf->data + buf->position, str, len);
    buf->position += len;
    update_length(buf);
    sync_position(buf);
    return context;
}

ZymValue buffer_getLength(ZymVM* vm, ZymValue context) {
    BufferData* buf = (BufferData*)zym_getNativeData(context);
    return zym_newNumber((double)buf->length);
}

ZymValue buffer_getCapacity(ZymVM* vm, ZymValue context) {
    BufferData* buf = (BufferData*)zym_getNativeData(context);
    return zym_newNumber((double)buf->capacity);
}

ZymValue buffer_remaining(ZymVM* vm, ZymValue context) {
    BufferData* buf = (BufferData*)zym_getNativeData(context);
    size_t remaining = buf->position < buf->length ? buf->length - buf->position : 0;
    return zym_newNumber((double)remaining);
}

ZymValue buffer_seek(ZymVM* vm, ZymValue context, ZymValue posVal) {
    BufferData* buf = (BufferData*)zym_getNativeData(context);

    if (!zym_isNumber(posVal)) {
        zym_runtimeError(vm, "seek() requires a number argument");
        return ZYM_ERROR;
    }

    size_t pos = (size_t)zym_asNumber(posVal);
    if (pos > buf->capacity) {
        zym_runtimeError(vm, "Seek position %zu exceeds capacity %zu", pos, buf->capacity);
        return ZYM_ERROR;
    }

    buf->position = pos;
    sync_position(buf);
    return context;
}

ZymValue buffer_skip(ZymVM* vm, ZymValue context, ZymValue countVal) {
    BufferData* buf = (BufferData*)zym_getNativeData(context);

    if (!zym_isNumber(countVal)) {
        zym_runtimeError(vm, "skip() requires a number argument");
        return ZYM_ERROR;
    }

    size_t count = (size_t)zym_asNumber(countVal);
    size_t new_pos = buf->position + count;

    if (new_pos > buf->capacity) {
        zym_runtimeError(vm, "Skip would exceed buffer capacity");
        return ZYM_ERROR;
    }

    buf->position = new_pos;
    sync_position(buf);
    return context;
}

ZymValue buffer_rewind(ZymVM* vm, ZymValue context) {
    BufferData* buf = (BufferData*)zym_getNativeData(context);
    buf->position = 0;
    sync_position(buf);
    return context;
}

ZymValue buffer_clear(ZymVM* vm, ZymValue context) {
    BufferData* buf = (BufferData*)zym_getNativeData(context);
    memset(buf->data, 0, buf->capacity);
    buf->length = 0;
    buf->position = 0;
    sync_position(buf);
    return context;
}

ZymValue buffer_fill(ZymVM* vm, ZymValue context, ZymValue byteVal) {
    BufferData* buf = (BufferData*)zym_getNativeData(context);

    if (!zym_isNumber(byteVal)) {
        zym_runtimeError(vm, "fill() requires a number argument");
        return ZYM_ERROR;
    }

    uint8_t byte = (uint8_t)zym_asNumber(byteVal);
    memset(buf->data, byte, buf->capacity);
    buf->length = buf->capacity;
    return context;
}

ZymValue buffer_slice(ZymVM* vm, ZymValue context, ZymValue startVal, ZymValue endVal) {
    BufferData* buf = (BufferData*)zym_getNativeData(context);

    if (!zym_isNumber(startVal) || !zym_isNumber(endVal)) {
        zym_runtimeError(vm, "slice() requires two number arguments");
        return ZYM_ERROR;
    }

    size_t start = (size_t)zym_asNumber(startVal);
    size_t end = (size_t)zym_asNumber(endVal);

    if (start > end || end > buf->length) {
        zym_runtimeError(vm, "Invalid slice range [%zu, %zu) for buffer length %zu", start, end, buf->length);
        return ZYM_ERROR;
    }

    size_t slice_len = end - start;

    ZymValue newBuffer = nativeBuffer_create(vm, zym_newNumber((double)slice_len), zym_newBool(false));
    if (zym_isNull(newBuffer)) {
        return ZYM_ERROR;
    }

    zym_pushRoot(vm, newBuffer);
    ZymValue getLength = zym_mapGet(vm, newBuffer, "getLength");
    if (zym_isNull(getLength)) {
        zym_popRoot(vm);
        zym_runtimeError(vm, "Failed to create buffer slice");
        return ZYM_ERROR;
    }

    ZymValue newContext = zym_getClosureContext(getLength);
    BufferData* newBuf = (BufferData*)zym_getNativeData(newContext);
    if (!newBuf) {
        zym_popRoot(vm);
        zym_runtimeError(vm, "Failed to get buffer data for slice");
        return ZYM_ERROR;
    }

    memcpy(newBuf->data, buf->data + start, slice_len);
    newBuf->length = slice_len;
    newBuf->position = 0;
    sync_length(newBuf);

    zym_popRoot(vm);
    return newBuffer;
}

ZymValue buffer_toHex(ZymVM* vm, ZymValue context) {
    BufferData* buf = (BufferData*)zym_getNativeData(context);

    char* hex = malloc(buf->length * 2 + 1);
    if (!hex) {
        zym_runtimeError(vm, "Out of memory");
        return ZYM_ERROR;
    }

    for (size_t i = 0; i < buf->length; i++) {
        sprintf(hex + i * 2, "%02x", buf->data[i]);
    }
    hex[buf->length * 2] = '\0';

    ZymValue result = zym_newString(vm, hex);
    free(hex);
    return result;
}

// (up to first null or length)
ZymValue buffer_toString(ZymVM* vm, ZymValue context) {
    BufferData* buf = (BufferData*)zym_getNativeData(context);

    // Find actual string length (up to null or buffer length)
    size_t str_len = 0;
    while (str_len < buf->length && buf->data[str_len] != 0) {
        str_len++;
    }

    char* str = malloc(str_len + 1);
    if (!str) {
        zym_runtimeError(vm, "Out of memory");
        return ZYM_ERROR;
    }

    memcpy(str, buf->data, str_len);
    str[str_len] = '\0';

    ZymValue result = zym_newString(vm, str);
    free(str);
    return result;
}

ZymValue buffer_getEndianness(ZymVM* vm, ZymValue context) {
    BufferData* buf = (BufferData*)zym_getNativeData(context);
    return zym_newString(vm, buf->endianness == ENDIAN_LITTLE ? "little" : "big");
}

ZymValue buffer_setEndianness(ZymVM* vm, ZymValue context, ZymValue endianVal) {
    BufferData* buf = (BufferData*)zym_getNativeData(context);

    if (!zym_isString(endianVal)) {
        zym_runtimeError(vm, "setEndianness() requires a string argument ('little' or 'big')");
        return ZYM_ERROR;
    }

    const char* endian_str = zym_asCString(endianVal);
    if (strcmp(endian_str, "little") == 0) {
        buf->endianness = ENDIAN_LITTLE;
    } else if (strcmp(endian_str, "big") == 0) {
        buf->endianness = ENDIAN_BIG;
    } else {
        zym_runtimeError(vm, "Endianness must be 'little' or 'big', got '%s'", endian_str);
        return ZYM_ERROR;
    }

    return context;
}

void position_set_hook(ZymVM* vm, ZymValue context, ZymValue new_value) {
    BufferData* buf = (BufferData*)zym_getNativeData(context);
    if (zym_isNumber(new_value)) {
        size_t new_pos = (size_t)zym_asNumber(new_value);
        if (new_pos > buf->capacity) {
            new_pos = buf->capacity;
        }
        buf->position = new_pos;
        sync_position(buf);
    }
}

void length_set_hook(ZymVM* vm, ZymValue context, ZymValue new_value) {
    BufferData* buf = (BufferData*)zym_getNativeData(context);
    if (zym_isNumber(new_value)) {
        size_t new_len = (size_t)zym_asNumber(new_value);
        if (new_len > buf->capacity) {
            new_len = buf->capacity;
        }
        buf->length = new_len;
        sync_length(buf);
    }
}

ZymValue nativeBuffer_create(ZymVM* vm, ZymValue sizeVal, ZymValue autoGrowVal) {
    if (!zym_isNumber(sizeVal)) {
        zym_runtimeError(vm, "Buffer() requires a number argument");
        return ZYM_ERROR;
    }

    size_t size = (size_t)zym_asNumber(sizeVal);
    if (size == 0 || size > 100 * 1024 * 1024) {
        zym_runtimeError(vm, "Buffer size must be between 1 and 104857600 bytes (100MB)");
        return ZYM_ERROR;
    }

    bool auto_grow = true;
    if (!zym_isNull(autoGrowVal) && zym_isBool(autoGrowVal)) {
        auto_grow = zym_asBool(autoGrowVal);
    }

    BufferData* buf = calloc(1, sizeof(BufferData));
    if (!buf) {
        zym_runtimeError(vm, "Out of memory");
        return ZYM_ERROR;
    }

    buf->data = calloc(size, 1);
    if (!buf->data) {
        free(buf);
        zym_runtimeError(vm, "Out of memory");
        return ZYM_ERROR;
    }

    buf->capacity = size;
    buf->length = 0;
    buf->position = 0;
    buf->position_ref = zym_newNumber(0);
    buf->length_ref = zym_newNumber(0);
    buf->auto_grow = auto_grow;
    buf->endianness = ENDIAN_LITTLE;

    ZymValue context = zym_createNativeContext(vm, buf, buffer_cleanup);
    zym_pushRoot(vm, context);

    ZymValue posRef = zym_createNativeReference(vm, context,
        offsetof(BufferData, position_ref), NULL, position_set_hook);
    zym_pushRoot(vm, posRef);

    ZymValue lenRef = zym_createNativeReference(vm, context,
        offsetof(BufferData, length_ref), NULL, length_set_hook);
    zym_pushRoot(vm, lenRef);

    #define CREATE_METHOD_0(name, func) \
        ZymValue name = zym_createNativeClosure(vm, #func "()", func, context); \
        zym_pushRoot(vm, name);

    #define CREATE_METHOD_1(name, func) \
        ZymValue name = zym_createNativeClosure(vm, #func "(arg)", func, context); \
        zym_pushRoot(vm, name);

    #define CREATE_METHOD_2(name, func) \
        ZymValue name = zym_createNativeClosure(vm, #func "(arg1, arg2)", func, context); \
        zym_pushRoot(vm, name);

    CREATE_METHOD_0(readUInt8, buffer_readUInt8);
    CREATE_METHOD_0(readInt8, buffer_readInt8);
    CREATE_METHOD_0(readUInt16, buffer_readUInt16);
    CREATE_METHOD_0(readInt16, buffer_readInt16);
    CREATE_METHOD_0(readUInt32, buffer_readUInt32);
    CREATE_METHOD_0(readInt32, buffer_readInt32);
    CREATE_METHOD_0(readFloat, buffer_readFloat);
    CREATE_METHOD_0(readDouble, buffer_readDouble);
    CREATE_METHOD_1(readBytes, buffer_readBytes);
    CREATE_METHOD_0(readString, buffer_readString);
    CREATE_METHOD_1(readStringN, buffer_readStringN);

    CREATE_METHOD_1(writeUInt8, buffer_writeUInt8);
    CREATE_METHOD_1(writeInt8, buffer_writeInt8);
    CREATE_METHOD_1(writeUInt16, buffer_writeUInt16);
    CREATE_METHOD_1(writeInt16, buffer_writeInt16);
    CREATE_METHOD_1(writeUInt32, buffer_writeUInt32);
    CREATE_METHOD_1(writeInt32, buffer_writeInt32);
    CREATE_METHOD_1(writeFloat, buffer_writeFloat);
    CREATE_METHOD_1(writeDouble, buffer_writeDouble);
    CREATE_METHOD_1(writeBytes, buffer_writeBytes);
    CREATE_METHOD_1(writeString, buffer_writeString);
    CREATE_METHOD_1(writeStringRaw, buffer_writeStringRaw);

    CREATE_METHOD_0(getLength, buffer_getLength);
    CREATE_METHOD_0(getCapacity, buffer_getCapacity);
    CREATE_METHOD_0(remaining, buffer_remaining);
    CREATE_METHOD_1(seek, buffer_seek);
    CREATE_METHOD_1(skip, buffer_skip);
    CREATE_METHOD_0(rewind, buffer_rewind);
    CREATE_METHOD_0(clear, buffer_clear);
    CREATE_METHOD_1(fill, buffer_fill);
    CREATE_METHOD_2(slice, buffer_slice);
    CREATE_METHOD_0(toHex, buffer_toHex);
    CREATE_METHOD_0(toString, buffer_toString);
    CREATE_METHOD_0(getEndianness, buffer_getEndianness);
    CREATE_METHOD_1(setEndianness, buffer_setEndianness);

    #undef CREATE_METHOD_0
    #undef CREATE_METHOD_1
    #undef CREATE_METHOD_2

    ZymValue obj = zym_newMap(vm);
    zym_pushRoot(vm, obj);

    zym_mapSet(vm, obj, "position", posRef);
    zym_mapSet(vm, obj, "length", lenRef);

    zym_mapSet(vm, obj, "readUInt8", readUInt8);
    zym_mapSet(vm, obj, "readInt8", readInt8);
    zym_mapSet(vm, obj, "readUInt16", readUInt16);
    zym_mapSet(vm, obj, "readInt16", readInt16);
    zym_mapSet(vm, obj, "readUInt32", readUInt32);
    zym_mapSet(vm, obj, "readInt32", readInt32);
    zym_mapSet(vm, obj, "readFloat", readFloat);
    zym_mapSet(vm, obj, "readDouble", readDouble);
    zym_mapSet(vm, obj, "readBytes", readBytes);
    zym_mapSet(vm, obj, "readString", readString);
    zym_mapSet(vm, obj, "readStringN", readStringN);

    zym_mapSet(vm, obj, "writeUInt8", writeUInt8);
    zym_mapSet(vm, obj, "writeInt8", writeInt8);
    zym_mapSet(vm, obj, "writeUInt16", writeUInt16);
    zym_mapSet(vm, obj, "writeInt16", writeInt16);
    zym_mapSet(vm, obj, "writeUInt32", writeUInt32);
    zym_mapSet(vm, obj, "writeInt32", writeInt32);
    zym_mapSet(vm, obj, "writeFloat", writeFloat);
    zym_mapSet(vm, obj, "writeDouble", writeDouble);
    zym_mapSet(vm, obj, "writeBytes", writeBytes);
    zym_mapSet(vm, obj, "writeString", writeString);
    zym_mapSet(vm, obj, "writeStringRaw", writeStringRaw);

    zym_mapSet(vm, obj, "getLength", getLength);
    zym_mapSet(vm, obj, "getCapacity", getCapacity);
    zym_mapSet(vm, obj, "remaining", remaining);
    zym_mapSet(vm, obj, "seek", seek);
    zym_mapSet(vm, obj, "skip", skip);
    zym_mapSet(vm, obj, "rewind", rewind);
    zym_mapSet(vm, obj, "clear", clear);
    zym_mapSet(vm, obj, "fill", fill);
    zym_mapSet(vm, obj, "slice", slice);
    zym_mapSet(vm, obj, "toHex", toHex);
    zym_mapSet(vm, obj, "toString", toString);
    zym_mapSet(vm, obj, "getEndianness", getEndianness);
    zym_mapSet(vm, obj, "setEndianness", setEndianness);

    // (33 methods + 2 endian methods + posRef + lenRef + context + obj = 38 total)
    for (int i = 0; i < 38; i++) {
        zym_popRoot(vm);
    }

    return obj;
}

ZymValue nativeBuffer_create_auto(ZymVM* vm, ZymValue lengthVal) {
    return nativeBuffer_create(vm, lengthVal, zym_newBool(true));
}
