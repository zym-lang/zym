#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "./marshal.h"
#include "./natives.h"

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

ZymValue marshal_reconstruct_list(ZymVM* caller_vm, ZymVM* source_vm, ZymVM* target_vm, ZymValue source_list) {
    int length = zym_listLength(source_list);
    ZymValue target_list = zym_newList(target_vm);
    zym_pushRoot(target_vm, target_list);

    for (int i = 0; i < length; i++) {
        ZymValue source_elem = zym_listGet(source_vm, source_list, i);

        if (zym_isReference(source_elem)) {
            source_elem = zym_deref(source_vm, source_elem);
        }

        ZymValue target_elem = marshal_reconstruct_value(caller_vm, source_vm, target_vm, source_elem);

        if (zym_isNull(target_elem) && !zym_isNull(source_elem)) {
            target_elem = zym_newNull();
        }

        if (!zym_listAppend(target_vm, target_list, target_elem)) {
            zym_popRoot(target_vm);
            return ZYM_ERROR;
        }
    }

    zym_popRoot(target_vm);
    return target_list;
}

ZymValue marshal_reconstruct_map(ZymVM* caller_vm, ZymVM* source_vm, ZymVM* target_vm, ZymValue source_map) {
    ZymValue target_map = zym_newMap(target_vm);
    zym_pushRoot(target_vm, target_map);

    typedef struct {
        ZymVM* caller_vm;
        ZymVM* source_vm;
        ZymVM* target_vm;
        ZymValue target_map;
        bool success;
    } MapReconstructContext;

    MapReconstructContext ctx = {
        .caller_vm = caller_vm,
        .source_vm = source_vm,
        .target_vm = target_vm,
        .target_map = target_map,
        .success = true
    };

    bool map_iterator(ZymVM* vm, const char* key, ZymValue source_val, void* userdata) {
        MapReconstructContext* ctx = (MapReconstructContext*)userdata;

        if (zym_isReference(source_val)) {
            source_val = zym_deref(ctx->source_vm, source_val);
        }

        ZymValue target_val = marshal_reconstruct_value(ctx->caller_vm, ctx->source_vm, ctx->target_vm, source_val);

        if (zym_isNull(target_val) && !zym_isNull(source_val)) {
            target_val = zym_newNull();
        }

        if (!zym_mapSet(ctx->target_vm, ctx->target_map, key, target_val)) {
            ctx->success = false;
            return false;
        }

        return true;
    }

    zym_mapForEach(source_vm, source_map, map_iterator, &ctx);

    zym_popRoot(target_vm);

    if (!ctx.success) {
        return ZYM_ERROR;
    }

    return target_map;
}

ZymValue marshal_reconstruct_buffer(ZymVM* source_vm, ZymVM* target_vm, ZymValue source_buffer) {
    ZymValue getLength = zym_mapGet(source_vm, source_buffer, "getLength");
    if (zym_isNull(getLength)) {
        return zym_newNull();
    }

    ZymValue bufferContext = zym_getClosureContext(getLength);
    BufferData* source_buf = (BufferData*)zym_getNativeData(bufferContext);
    if (!source_buf) {
        return zym_newNull();
    }

    ZymValue capacity = zym_newNumber((double)source_buf->capacity);
    ZymValue autoGrow = zym_newBool(source_buf->auto_grow);
    ZymValue target_buffer = nativeBuffer_create(target_vm, capacity, autoGrow);
    if (target_buffer == ZYM_ERROR) {
        return zym_newNull();
    }

    ZymValue target_getLength = zym_mapGet(target_vm, target_buffer, "getLength");
    ZymValue target_context = zym_getClosureContext(target_getLength);
    BufferData* target_buf = (BufferData*)zym_getNativeData(target_context);
    if (!target_buf) {
        return zym_newNull();
    }

    memcpy(target_buf->data, source_buf->data, source_buf->length);
    target_buf->length = source_buf->length;
    target_buf->position = source_buf->position;
    target_buf->auto_grow = source_buf->auto_grow;
    target_buf->endianness = source_buf->endianness;

    target_buf->position_ref = zym_newNumber((double)target_buf->position);
    target_buf->length_ref = zym_newNumber((double)target_buf->length);

    zym_mapSet(target_vm, target_buffer, "length", target_buf->length_ref);
    zym_mapSet(target_vm, target_buffer, "position", target_buf->position_ref);

    return target_buffer;
}

ZymValue marshal_reconstruct_value(ZymVM* caller_vm, ZymVM* source_vm, ZymVM* target_vm, ZymValue value) {
    if (zym_isReference(value)) {
        value = zym_deref(source_vm, value);
    }

    if (zym_isNull(value) || zym_isBool(value) || zym_isNumber(value)) {
        return value;
    }

    if (zym_isString(value)) {
        const char* str = zym_asCString(value);
        return zym_newString(target_vm, str);
    }

    if (zym_isList(value)) {
        return marshal_reconstruct_list(caller_vm, source_vm, target_vm, value);
    }

    if (zym_isMap(value)) {
        ZymValue as_buffer = marshal_reconstruct_buffer(source_vm, target_vm, value);
        if (!zym_isNull(as_buffer)) {
            return as_buffer;
        }

        return marshal_reconstruct_map(caller_vm, source_vm, target_vm, value);
    }

    return zym_newNull();
}
