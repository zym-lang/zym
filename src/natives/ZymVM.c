#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "./natives.h"
#include "./marshal.h"
#include "zym/module_loader.h"

void setupNatives(ZymVM* vm);

typedef struct {
    ZymVM* vm;
    bool loaded;
    ZymValue last_result;
    bool has_result;
} VMData;

void zymvm_cleanup(ZymVM* vm, void* ptr) {
    VMData* vmdata = (VMData*)ptr;
    if (vmdata->vm) {
        zym_freeVM(vmdata->vm);
        vmdata->vm = NULL;
    }
    free(vmdata);
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

static char* zymvm_read_file(const char* path, size_t* out_size) {
    FILE* file = fopen(path, "rb");
    if (!file) return NULL;
    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);
    char* buffer = (char*)malloc(fileSize + 1);
    if (!buffer) { fclose(file); return NULL; }
    size_t bytesRead = fread(buffer, 1, fileSize, file);
    fclose(file);
    if (bytesRead < fileSize) { free(buffer); return NULL; }
    buffer[bytesRead] = '\0';
    if (out_size) *out_size = bytesRead;
    return buffer;
}

static ModuleReadResult zymvm_module_read_callback(const char* path, void* user_data) {
    ModuleReadResult result = {NULL, NULL};
    ZymVM* vm = (ZymVM*)user_data;
    char* raw_source = zymvm_read_file(path, NULL);
    if (!raw_source) return result;

    ZymLineMap* line_map = zym_newLineMap(vm);
    const char* preprocessed = NULL;
    ZymStatus status = zym_preprocess(vm, raw_source, line_map, &preprocessed);
    free(raw_source);

    if (status != ZYM_STATUS_OK) {
        zym_freeLineMap(vm, line_map);
        free((void*)line_map);
        return result;
    }

    result.source = (char*)preprocessed;
    result.line_map = line_map;
    return result;
}

static char* zymvm_compile_source_internal(ZymVM* parent_vm, const char* source, const char* file_path, size_t* out_size) {
    ZymVM* compile_vm = zym_newVM();
    if (!compile_vm) return NULL;

    setupNatives(compile_vm);

    ZymLineMap* line_map = zym_newLineMap(compile_vm);
    const char* processed_source = NULL;

    if (zym_preprocess(compile_vm, source, line_map, &processed_source) != ZYM_STATUS_OK) {
        zym_freeLineMap(compile_vm, line_map);
        zym_freeVM(compile_vm);
        return NULL;
    }

    ModuleLoadResult* module_result = loadModules(
        compile_vm, processed_source, line_map, file_path,
        zymvm_module_read_callback, compile_vm, true, false, NULL);

    if (module_result->has_error) {
        freeModuleLoadResult(compile_vm, module_result);
        zym_freeLineMap(compile_vm, line_map);
        zym_freeVM(compile_vm);
        return NULL;
    }

    ZymChunk* chunk = zym_newChunk(compile_vm);
    ZymCompilerConfig config = { .include_line_info = 1 };
    const char* entry_file = module_result->module_count > 0 ? module_result->module_paths[0] : file_path;

    if (zym_compile(compile_vm, module_result->combined_source, chunk, module_result->line_map, entry_file, config) != ZYM_STATUS_OK) {
        freeModuleLoadResult(compile_vm, module_result);
        zym_freeChunk(compile_vm, chunk);
        zym_freeLineMap(compile_vm, line_map);
        zym_freeVM(compile_vm);
        return NULL;
    }

    freeModuleLoadResult(compile_vm, module_result);

    char* bytecode = NULL;
    size_t bytecode_size = 0;
    if (zym_serializeChunk(compile_vm, config, chunk, &bytecode, &bytecode_size) != ZYM_STATUS_OK) {
        zym_freeChunk(compile_vm, chunk);
        zym_freeLineMap(compile_vm, line_map);
        zym_freeVM(compile_vm);
        return NULL;
    }

    zym_freeChunk(compile_vm, chunk);
    zym_freeLineMap(compile_vm, line_map);
    zym_freeVM(compile_vm);

    *out_size = bytecode_size;
    return bytecode;
}

static ZymValue zymvm_bytecode_to_buffer(ZymVM* vm, const char* bytecode, size_t bytecode_size) {
    ZymValue sizeVal = zym_newNumber((double)bytecode_size);
    ZymValue autoGrow = zym_newBool(false);
    ZymValue bufObj = nativeBuffer_create(vm, sizeVal, autoGrow);
    if (bufObj == ZYM_ERROR) return ZYM_ERROR;

    ZymValue getLength = zym_mapGet(vm, bufObj, "getLength");
    if (zym_isNull(getLength)) return ZYM_ERROR;
    ZymValue bufContext = zym_getClosureContext(getLength);
    BufferData* buf = (BufferData*)zym_getNativeData(bufContext);
    if (!buf) return ZYM_ERROR;

    memcpy(buf->data, bytecode, bytecode_size);
    buf->length = bytecode_size;
    buf->position = 0;

    return bufObj;
}

ZymValue zymvm_load(ZymVM* parent_vm, ZymValue context, ZymValue bufferVal) {
    VMData* vmdata = (VMData*)zym_getNativeData(context);

    if (!zym_isMap(bufferVal)) {
        zym_runtimeError(parent_vm, "load() requires a Buffer argument");
        return ZYM_ERROR;
    }

    ZymValue getLength = zym_mapGet(parent_vm, bufferVal, "getLength");
    if (zym_isNull(getLength)) {
        zym_runtimeError(parent_vm, "Invalid Buffer object");
        return ZYM_ERROR;
    }

    ZymValue bufferContext = zym_getClosureContext(getLength);
    BufferData* buf = (BufferData*)zym_getNativeData(bufferContext);
    if (!buf) {
        zym_runtimeError(parent_vm, "Failed to get buffer data");
        return ZYM_ERROR;
    }

    ZymChunk* chunk = zym_newChunk(vmdata->vm);
    if (!chunk) {
        zym_runtimeError(parent_vm, "Failed to create chunk");
        return ZYM_ERROR;
    }

    ZymStatus status = zym_deserializeChunk(vmdata->vm, chunk, (const char*)buf->data, buf->length);
    if (status != ZYM_STATUS_OK) {
        zym_freeChunk(vmdata->vm, chunk);
        return zym_newBool(false);
    }

    status = zym_runChunk(vmdata->vm, chunk);

    if (status != ZYM_STATUS_OK) {
        return zym_newBool(false);
    }

    vmdata->loaded = true;
    return zym_newBool(true);
}

ZymValue zymvm_hasFunction(ZymVM* parent_vm, ZymValue context, ZymValue nameVal, ZymValue arityVal) {
    VMData* vmdata = (VMData*)zym_getNativeData(context);

    if (!vmdata->loaded) {
        return zym_newBool(false);
    }

    if (!zym_isString(nameVal) || !zym_isNumber(arityVal)) {
        zym_runtimeError(parent_vm, "hasFunction() requires string name and number arity");
        return ZYM_ERROR;
    }

    const char* name = zym_asCString(nameVal);
    int arity = (int)zym_asNumber(arityVal);

    return zym_newBool(zym_hasFunction(vmdata->vm, name, arity));
}

ZymValue zymvm_call_0(ZymVM* parent_vm, ZymValue context, ZymValue nameVal) {
    VMData* vmdata = (VMData*)zym_getNativeData(context);

    if (!vmdata->loaded) {
        zym_runtimeError(parent_vm, "Cannot call function before loading bytecode");
        return ZYM_ERROR;
    }

    if (!zym_isString(nameVal)) {
        zym_runtimeError(parent_vm, "call() requires string function name");
        return ZYM_ERROR;
    }

    const char* name = zym_asCString(nameVal);

    ZymStatus status = zym_call(vmdata->vm, name, 0);

    if (status == ZYM_STATUS_OK) {
        ZymValue nested_result = zym_getCallResult(vmdata->vm);
        vmdata->last_result = marshal_reconstruct_value(parent_vm, vmdata->vm, parent_vm, nested_result);
        vmdata->has_result = true;
        return zym_newBool(true);
    }

    vmdata->has_result = false;
    return zym_newBool(false);
}

ZymValue zymvm_call_1(ZymVM* parent_vm, ZymValue context, ZymValue nameVal, ZymValue arg1) {
    VMData* vmdata = (VMData*)zym_getNativeData(context);

    if (!vmdata->loaded) {
        zym_runtimeError(parent_vm, "Cannot call function before loading bytecode");
        return ZYM_ERROR;
    }

    if (!zym_isString(nameVal)) {
        zym_runtimeError(parent_vm, "call() requires string function name");
        return ZYM_ERROR;
    }

    const char* name = zym_asCString(nameVal);

    ZymValue nested_arg1 = marshal_reconstruct_value(parent_vm, parent_vm, vmdata->vm, arg1);

    if (nested_arg1 == ZYM_ERROR) {
        return ZYM_ERROR;
    }
    if (zym_isNull(nested_arg1) && !zym_isNull(arg1)) {
        zym_runtimeError(parent_vm, "Cannot pass unsupported type to nested VM (functions, structs, enums not supported)");
        return ZYM_ERROR;
    }

    ZymStatus status = zym_call(vmdata->vm, name, 1, nested_arg1);

    if (status == ZYM_STATUS_OK) {
        ZymValue nested_result = zym_getCallResult(vmdata->vm);
        vmdata->last_result = marshal_reconstruct_value(parent_vm, vmdata->vm, parent_vm, nested_result);
        vmdata->has_result = true;
        return zym_newBool(true);
    }

    vmdata->has_result = false;
    return zym_newBool(false);
}

ZymValue zymvm_call_2(ZymVM* parent_vm, ZymValue context, ZymValue nameVal, ZymValue arg1, ZymValue arg2) {
    VMData* vmdata = (VMData*)zym_getNativeData(context);

    if (!vmdata->loaded) {
        zym_runtimeError(parent_vm, "Cannot call function before loading bytecode");
        return ZYM_ERROR;
    }

    if (!zym_isString(nameVal)) {
        zym_runtimeError(parent_vm, "call() requires string function name");
        return ZYM_ERROR;
    }

    const char* name = zym_asCString(nameVal);

    ZymValue nested_arg1 = marshal_reconstruct_value(parent_vm, parent_vm, vmdata->vm, arg1);
    if (nested_arg1 == ZYM_ERROR) return ZYM_ERROR;
    if (zym_isNull(nested_arg1) && !zym_isNull(arg1)) {
        zym_runtimeError(parent_vm, "Cannot pass unsupported type to nested VM (functions, structs, enums not supported)");
        return ZYM_ERROR;
    }
    ZymValue nested_arg2 = marshal_reconstruct_value(parent_vm, parent_vm, vmdata->vm, arg2);
    if (nested_arg2 == ZYM_ERROR) return ZYM_ERROR;
    if (zym_isNull(nested_arg2) && !zym_isNull(arg2)) {
        zym_runtimeError(parent_vm, "Cannot pass unsupported type to nested VM (functions, structs, enums not supported)");
        return ZYM_ERROR;
    }

    ZymStatus status = zym_call(vmdata->vm, name, 2, nested_arg1, nested_arg2);

    if (status == ZYM_STATUS_OK) {
        ZymValue nested_result = zym_getCallResult(vmdata->vm);
        vmdata->last_result = marshal_reconstruct_value(parent_vm, vmdata->vm, parent_vm, nested_result);
        vmdata->has_result = true;
        return zym_newBool(true);
    }

    vmdata->has_result = false;
    return zym_newBool(false);
}

ZymValue zymvm_call_3(ZymVM* parent_vm, ZymValue context, ZymValue nameVal, ZymValue arg1, ZymValue arg2, ZymValue arg3) {
    VMData* vmdata = (VMData*)zym_getNativeData(context);

    if (!vmdata->loaded) {
        zym_runtimeError(parent_vm, "Cannot call function before loading bytecode");
        return ZYM_ERROR;
    }

    if (!zym_isString(nameVal)) {
        zym_runtimeError(parent_vm, "call() requires string function name");
        return ZYM_ERROR;
    }

    const char* name = zym_asCString(nameVal);

    ZymValue nested_arg1 = marshal_reconstruct_value(parent_vm, parent_vm, vmdata->vm, arg1);
    if (nested_arg1 == ZYM_ERROR) return ZYM_ERROR;
    if (zym_isNull(nested_arg1) && !zym_isNull(arg1)) { zym_runtimeError(parent_vm, "Cannot pass unsupported type to nested VM"); return ZYM_ERROR; }
    ZymValue nested_arg2 = marshal_reconstruct_value(parent_vm, parent_vm, vmdata->vm, arg2);
    if (nested_arg2 == ZYM_ERROR) return ZYM_ERROR;
    if (zym_isNull(nested_arg2) && !zym_isNull(arg2)) { zym_runtimeError(parent_vm, "Cannot pass unsupported type to nested VM"); return ZYM_ERROR; }
    ZymValue nested_arg3 = marshal_reconstruct_value(parent_vm, parent_vm, vmdata->vm, arg3);
    if (nested_arg3 == ZYM_ERROR) return ZYM_ERROR;
    if (zym_isNull(nested_arg3) && !zym_isNull(arg3)) { zym_runtimeError(parent_vm, "Cannot pass unsupported type to nested VM"); return ZYM_ERROR; }

    ZymStatus status = zym_call(vmdata->vm, name, 3, nested_arg1, nested_arg2, nested_arg3);

    if (status == ZYM_STATUS_OK) {
        ZymValue nested_result = zym_getCallResult(vmdata->vm);
        vmdata->last_result = marshal_reconstruct_value(parent_vm, vmdata->vm, parent_vm, nested_result);
        vmdata->has_result = true;
        return zym_newBool(true);
    }

    vmdata->has_result = false;
    return zym_newBool(false);
}

ZymValue zymvm_call_4(ZymVM* parent_vm, ZymValue context, ZymValue nameVal, ZymValue arg1, ZymValue arg2, ZymValue arg3, ZymValue arg4) {
    VMData* vmdata = (VMData*)zym_getNativeData(context);

    if (!vmdata->loaded) {
        zym_runtimeError(parent_vm, "Cannot call function before loading bytecode");
        return ZYM_ERROR;
    }

    if (!zym_isString(nameVal)) {
        zym_runtimeError(parent_vm, "call() requires string function name");
        return ZYM_ERROR;
    }

    const char* name = zym_asCString(nameVal);

    ZymValue nested_arg1 = marshal_reconstruct_value(parent_vm, parent_vm, vmdata->vm, arg1);
    if (nested_arg1 == ZYM_ERROR) return ZYM_ERROR;
    if (zym_isNull(nested_arg1) && !zym_isNull(arg1)) { zym_runtimeError(parent_vm, "Cannot pass unsupported type to nested VM"); return ZYM_ERROR; }
    ZymValue nested_arg2 = marshal_reconstruct_value(parent_vm, parent_vm, vmdata->vm, arg2);
    if (nested_arg2 == ZYM_ERROR) return ZYM_ERROR;
    if (zym_isNull(nested_arg2) && !zym_isNull(arg2)) { zym_runtimeError(parent_vm, "Cannot pass unsupported type to nested VM"); return ZYM_ERROR; }
    ZymValue nested_arg3 = marshal_reconstruct_value(parent_vm, parent_vm, vmdata->vm, arg3);
    if (nested_arg3 == ZYM_ERROR) return ZYM_ERROR;
    if (zym_isNull(nested_arg3) && !zym_isNull(arg3)) { zym_runtimeError(parent_vm, "Cannot pass unsupported type to nested VM"); return ZYM_ERROR; }
    ZymValue nested_arg4 = marshal_reconstruct_value(parent_vm, parent_vm, vmdata->vm, arg4);
    if (nested_arg4 == ZYM_ERROR) return ZYM_ERROR;
    if (zym_isNull(nested_arg4) && !zym_isNull(arg4)) { zym_runtimeError(parent_vm, "Cannot pass unsupported type to nested VM"); return ZYM_ERROR; }

    ZymStatus status = zym_call(vmdata->vm, name, 4, nested_arg1, nested_arg2, nested_arg3, nested_arg4);

    if (status == ZYM_STATUS_OK) {
        ZymValue nested_result = zym_getCallResult(vmdata->vm);
        vmdata->last_result = marshal_reconstruct_value(parent_vm, vmdata->vm, parent_vm, nested_result);
        vmdata->has_result = true;
        return zym_newBool(true);
    }

    vmdata->has_result = false;
    return zym_newBool(false);
}

ZymValue zymvm_call_5(ZymVM* parent_vm, ZymValue context, ZymValue nameVal, ZymValue arg1, ZymValue arg2, ZymValue arg3, ZymValue arg4, ZymValue arg5) {
    VMData* vmdata = (VMData*)zym_getNativeData(context);

    if (!vmdata->loaded) {
        zym_runtimeError(parent_vm, "Cannot call function before loading bytecode");
        return ZYM_ERROR;
    }

    if (!zym_isString(nameVal)) {
        zym_runtimeError(parent_vm, "call() requires string function name");
        return ZYM_ERROR;
    }

    const char* name = zym_asCString(nameVal);

    ZymValue nested_arg1 = marshal_reconstruct_value(parent_vm, parent_vm, vmdata->vm, arg1);
    if (nested_arg1 == ZYM_ERROR) return ZYM_ERROR;
    if (zym_isNull(nested_arg1) && !zym_isNull(arg1)) { zym_runtimeError(parent_vm, "Cannot pass unsupported type to nested VM"); return ZYM_ERROR; }
    ZymValue nested_arg2 = marshal_reconstruct_value(parent_vm, parent_vm, vmdata->vm, arg2);
    if (nested_arg2 == ZYM_ERROR) return ZYM_ERROR;
    if (zym_isNull(nested_arg2) && !zym_isNull(arg2)) { zym_runtimeError(parent_vm, "Cannot pass unsupported type to nested VM"); return ZYM_ERROR; }
    ZymValue nested_arg3 = marshal_reconstruct_value(parent_vm, parent_vm, vmdata->vm, arg3);
    if (nested_arg3 == ZYM_ERROR) return ZYM_ERROR;
    if (zym_isNull(nested_arg3) && !zym_isNull(arg3)) { zym_runtimeError(parent_vm, "Cannot pass unsupported type to nested VM"); return ZYM_ERROR; }
    ZymValue nested_arg4 = marshal_reconstruct_value(parent_vm, parent_vm, vmdata->vm, arg4);
    if (nested_arg4 == ZYM_ERROR) return ZYM_ERROR;
    if (zym_isNull(nested_arg4) && !zym_isNull(arg4)) { zym_runtimeError(parent_vm, "Cannot pass unsupported type to nested VM"); return ZYM_ERROR; }
    ZymValue nested_arg5 = marshal_reconstruct_value(parent_vm, parent_vm, vmdata->vm, arg5);
    if (nested_arg5 == ZYM_ERROR) return ZYM_ERROR;
    if (zym_isNull(nested_arg5) && !zym_isNull(arg5)) { zym_runtimeError(parent_vm, "Cannot pass unsupported type to nested VM"); return ZYM_ERROR; }

    ZymStatus status = zym_call(vmdata->vm, name, 5, nested_arg1, nested_arg2, nested_arg3, nested_arg4, nested_arg5);

    if (status == ZYM_STATUS_OK) {
        ZymValue nested_result = zym_getCallResult(vmdata->vm);
        vmdata->last_result = marshal_reconstruct_value(parent_vm, vmdata->vm, parent_vm, nested_result);
        vmdata->has_result = true;
        return zym_newBool(true);
    }

    vmdata->has_result = false;
    return zym_newBool(false);
}

ZymValue zymvm_call_6(ZymVM* parent_vm, ZymValue context, ZymValue nameVal, ZymValue arg1, ZymValue arg2, ZymValue arg3, ZymValue arg4, ZymValue arg5, ZymValue arg6) {
    VMData* vmdata = (VMData*)zym_getNativeData(context);

    if (!vmdata->loaded) {
        zym_runtimeError(parent_vm, "Cannot call function before loading bytecode");
        return ZYM_ERROR;
    }

    if (!zym_isString(nameVal)) {
        zym_runtimeError(parent_vm, "call() requires string function name");
        return ZYM_ERROR;
    }

    const char* name = zym_asCString(nameVal);

    ZymValue nested_arg1 = marshal_reconstruct_value(parent_vm, parent_vm, vmdata->vm, arg1);
    if (nested_arg1 == ZYM_ERROR) return ZYM_ERROR;
    if (zym_isNull(nested_arg1) && !zym_isNull(arg1)) { zym_runtimeError(parent_vm, "Cannot pass unsupported type to nested VM"); return ZYM_ERROR; }
    ZymValue nested_arg2 = marshal_reconstruct_value(parent_vm, parent_vm, vmdata->vm, arg2);
    if (nested_arg2 == ZYM_ERROR) return ZYM_ERROR;
    if (zym_isNull(nested_arg2) && !zym_isNull(arg2)) { zym_runtimeError(parent_vm, "Cannot pass unsupported type to nested VM"); return ZYM_ERROR; }
    ZymValue nested_arg3 = marshal_reconstruct_value(parent_vm, parent_vm, vmdata->vm, arg3);
    if (nested_arg3 == ZYM_ERROR) return ZYM_ERROR;
    if (zym_isNull(nested_arg3) && !zym_isNull(arg3)) { zym_runtimeError(parent_vm, "Cannot pass unsupported type to nested VM"); return ZYM_ERROR; }
    ZymValue nested_arg4 = marshal_reconstruct_value(parent_vm, parent_vm, vmdata->vm, arg4);
    if (nested_arg4 == ZYM_ERROR) return ZYM_ERROR;
    if (zym_isNull(nested_arg4) && !zym_isNull(arg4)) { zym_runtimeError(parent_vm, "Cannot pass unsupported type to nested VM"); return ZYM_ERROR; }
    ZymValue nested_arg5 = marshal_reconstruct_value(parent_vm, parent_vm, vmdata->vm, arg5);
    if (nested_arg5 == ZYM_ERROR) return ZYM_ERROR;
    if (zym_isNull(nested_arg5) && !zym_isNull(arg5)) { zym_runtimeError(parent_vm, "Cannot pass unsupported type to nested VM"); return ZYM_ERROR; }
    ZymValue nested_arg6 = marshal_reconstruct_value(parent_vm, parent_vm, vmdata->vm, arg6);
    if (nested_arg6 == ZYM_ERROR) return ZYM_ERROR;
    if (zym_isNull(nested_arg6) && !zym_isNull(arg6)) { zym_runtimeError(parent_vm, "Cannot pass unsupported type to nested VM"); return ZYM_ERROR; }

    ZymStatus status = zym_call(vmdata->vm, name, 6, nested_arg1, nested_arg2, nested_arg3, nested_arg4, nested_arg5, nested_arg6);

    if (status == ZYM_STATUS_OK) {
        ZymValue nested_result = zym_getCallResult(vmdata->vm);
        vmdata->last_result = marshal_reconstruct_value(parent_vm, vmdata->vm, parent_vm, nested_result);
        vmdata->has_result = true;
        return zym_newBool(true);
    }

    vmdata->has_result = false;
    return zym_newBool(false);
}

ZymValue zymvm_call_7(ZymVM* parent_vm, ZymValue context, ZymValue nameVal, ZymValue arg1, ZymValue arg2, ZymValue arg3, ZymValue arg4, ZymValue arg5, ZymValue arg6, ZymValue arg7) {
    VMData* vmdata = (VMData*)zym_getNativeData(context);
    if (!vmdata->loaded) {
        zym_runtimeError(parent_vm, "Cannot call function before loading bytecode");
        return ZYM_ERROR;
    }
    if (!zym_isString(nameVal)) {
        zym_runtimeError(parent_vm, "call() requires string function name");
        return ZYM_ERROR;
    }
    const char* name = zym_asCString(nameVal);

    ZymValue nested_arg1 = marshal_reconstruct_value(parent_vm, parent_vm, vmdata->vm, arg1);
    if (nested_arg1 == ZYM_ERROR) return ZYM_ERROR;
    if (zym_isNull(nested_arg1) && !zym_isNull(arg1)) { zym_runtimeError(parent_vm, "Cannot pass unsupported type to nested VM"); return ZYM_ERROR; }
    ZymValue nested_arg2 = marshal_reconstruct_value(parent_vm, parent_vm, vmdata->vm, arg2);
    if (nested_arg2 == ZYM_ERROR) return ZYM_ERROR;
    if (zym_isNull(nested_arg2) && !zym_isNull(arg2)) { zym_runtimeError(parent_vm, "Cannot pass unsupported type to nested VM"); return ZYM_ERROR; }
    ZymValue nested_arg3 = marshal_reconstruct_value(parent_vm, parent_vm, vmdata->vm, arg3);
    if (nested_arg3 == ZYM_ERROR) return ZYM_ERROR;
    if (zym_isNull(nested_arg3) && !zym_isNull(arg3)) { zym_runtimeError(parent_vm, "Cannot pass unsupported type to nested VM"); return ZYM_ERROR; }
    ZymValue nested_arg4 = marshal_reconstruct_value(parent_vm, parent_vm, vmdata->vm, arg4);
    if (nested_arg4 == ZYM_ERROR) return ZYM_ERROR;
    if (zym_isNull(nested_arg4) && !zym_isNull(arg4)) { zym_runtimeError(parent_vm, "Cannot pass unsupported type to nested VM"); return ZYM_ERROR; }
    ZymValue nested_arg5 = marshal_reconstruct_value(parent_vm, parent_vm, vmdata->vm, arg5);
    if (nested_arg5 == ZYM_ERROR) return ZYM_ERROR;
    if (zym_isNull(nested_arg5) && !zym_isNull(arg5)) { zym_runtimeError(parent_vm, "Cannot pass unsupported type to nested VM"); return ZYM_ERROR; }
    ZymValue nested_arg6 = marshal_reconstruct_value(parent_vm, parent_vm, vmdata->vm, arg6);
    if (nested_arg6 == ZYM_ERROR) return ZYM_ERROR;
    if (zym_isNull(nested_arg6) && !zym_isNull(arg6)) { zym_runtimeError(parent_vm, "Cannot pass unsupported type to nested VM"); return ZYM_ERROR; }
    ZymValue nested_arg7 = marshal_reconstruct_value(parent_vm, parent_vm, vmdata->vm, arg7);
    if (nested_arg7 == ZYM_ERROR) return ZYM_ERROR;
    if (zym_isNull(nested_arg7) && !zym_isNull(arg7)) { zym_runtimeError(parent_vm, "Cannot pass unsupported type to nested VM"); return ZYM_ERROR; }

    ZymStatus status = zym_call(vmdata->vm, name, 7, nested_arg1, nested_arg2, nested_arg3, nested_arg4, nested_arg5, nested_arg6, nested_arg7);

    if (status == ZYM_STATUS_OK) {
        ZymValue nested_result = zym_getCallResult(vmdata->vm);
        vmdata->last_result = marshal_reconstruct_value(parent_vm, vmdata->vm, parent_vm, nested_result);
        vmdata->has_result = true;
        return zym_newBool(true);
    }

    vmdata->has_result = false;
    return zym_newBool(false);
}

ZymValue zymvm_call_8(ZymVM* parent_vm, ZymValue context, ZymValue nameVal, ZymValue arg1, ZymValue arg2, ZymValue arg3, ZymValue arg4, ZymValue arg5, ZymValue arg6, ZymValue arg7, ZymValue arg8) {
    VMData* vmdata = (VMData*)zym_getNativeData(context);
    if (!vmdata->loaded) {
        zym_runtimeError(parent_vm, "Cannot call function before loading bytecode");
        return ZYM_ERROR;
    }
    if (!zym_isString(nameVal)) {
        zym_runtimeError(parent_vm, "call() requires string function name");
        return ZYM_ERROR;
    }
    const char* name = zym_asCString(nameVal);

    ZymValue nested_arg1 = marshal_reconstruct_value(parent_vm, parent_vm, vmdata->vm, arg1);
    if (nested_arg1 == ZYM_ERROR) return ZYM_ERROR;
    if (zym_isNull(nested_arg1) && !zym_isNull(arg1)) { zym_runtimeError(parent_vm, "Cannot pass unsupported type to nested VM"); return ZYM_ERROR; }
    ZymValue nested_arg2 = marshal_reconstruct_value(parent_vm, parent_vm, vmdata->vm, arg2);
    if (nested_arg2 == ZYM_ERROR) return ZYM_ERROR;
    if (zym_isNull(nested_arg2) && !zym_isNull(arg2)) { zym_runtimeError(parent_vm, "Cannot pass unsupported type to nested VM"); return ZYM_ERROR; }
    ZymValue nested_arg3 = marshal_reconstruct_value(parent_vm, parent_vm, vmdata->vm, arg3);
    if (nested_arg3 == ZYM_ERROR) return ZYM_ERROR;
    if (zym_isNull(nested_arg3) && !zym_isNull(arg3)) { zym_runtimeError(parent_vm, "Cannot pass unsupported type to nested VM"); return ZYM_ERROR; }
    ZymValue nested_arg4 = marshal_reconstruct_value(parent_vm, parent_vm, vmdata->vm, arg4);
    if (nested_arg4 == ZYM_ERROR) return ZYM_ERROR;
    if (zym_isNull(nested_arg4) && !zym_isNull(arg4)) { zym_runtimeError(parent_vm, "Cannot pass unsupported type to nested VM"); return ZYM_ERROR; }
    ZymValue nested_arg5 = marshal_reconstruct_value(parent_vm, parent_vm, vmdata->vm, arg5);
    if (nested_arg5 == ZYM_ERROR) return ZYM_ERROR;
    if (zym_isNull(nested_arg5) && !zym_isNull(arg5)) { zym_runtimeError(parent_vm, "Cannot pass unsupported type to nested VM"); return ZYM_ERROR; }
    ZymValue nested_arg6 = marshal_reconstruct_value(parent_vm, parent_vm, vmdata->vm, arg6);
    if (nested_arg6 == ZYM_ERROR) return ZYM_ERROR;
    if (zym_isNull(nested_arg6) && !zym_isNull(arg6)) { zym_runtimeError(parent_vm, "Cannot pass unsupported type to nested VM"); return ZYM_ERROR; }
    ZymValue nested_arg7 = marshal_reconstruct_value(parent_vm, parent_vm, vmdata->vm, arg7);
    if (nested_arg7 == ZYM_ERROR) return ZYM_ERROR;
    if (zym_isNull(nested_arg7) && !zym_isNull(arg7)) { zym_runtimeError(parent_vm, "Cannot pass unsupported type to nested VM"); return ZYM_ERROR; }
    ZymValue nested_arg8 = marshal_reconstruct_value(parent_vm, parent_vm, vmdata->vm, arg8);
    if (nested_arg8 == ZYM_ERROR) return ZYM_ERROR;
    if (zym_isNull(nested_arg8) && !zym_isNull(arg8)) { zym_runtimeError(parent_vm, "Cannot pass unsupported type to nested VM"); return ZYM_ERROR; }

    ZymStatus status = zym_call(vmdata->vm, name, 8, nested_arg1, nested_arg2, nested_arg3, nested_arg4, nested_arg5, nested_arg6, nested_arg7, nested_arg8);

    if (status == ZYM_STATUS_OK) {
        ZymValue nested_result = zym_getCallResult(vmdata->vm);
        vmdata->last_result = marshal_reconstruct_value(parent_vm, vmdata->vm, parent_vm, nested_result);
        vmdata->has_result = true;
        return zym_newBool(true);
    }

    vmdata->has_result = false;
    return zym_newBool(false);
}

ZymValue zymvm_getCallResult(ZymVM* parent_vm, ZymValue context) {
    VMData* vmdata = (VMData*)zym_getNativeData(context);

    if (!vmdata->has_result) {
        return zym_newNull();
    }

    return vmdata->last_result;
}

ZymValue zymvm_compileFile(ZymVM* parent_vm, ZymValue context, ZymValue pathVal) {
    if (!zym_isString(pathVal)) {
        zym_runtimeError(parent_vm, "compileFile() requires a string path");
        return ZYM_ERROR;
    }

    const char* path = zym_asCString(pathVal);
    char* source = zymvm_read_file(path, NULL);
    if (!source) {
        zym_runtimeError(parent_vm, "compileFile() could not read file: %s", path);
        return ZYM_ERROR;
    }

    size_t bytecode_size = 0;
    char* bytecode = zymvm_compile_source_internal(parent_vm, source, path, &bytecode_size);
    free(source);

    if (!bytecode) {
        zym_runtimeError(parent_vm, "compileFile() compilation failed for: %s", path);
        return ZYM_ERROR;
    }

    ZymValue result = zymvm_bytecode_to_buffer(parent_vm, bytecode, bytecode_size);
    free(bytecode);
    return result;
}

ZymValue zymvm_compileSource(ZymVM* parent_vm, ZymValue context, ZymValue sourceVal) {
    if (!zym_isString(sourceVal)) {
        zym_runtimeError(parent_vm, "compileSource() requires a string source");
        return ZYM_ERROR;
    }

    const char* source = zym_asCString(sourceVal);

    size_t bytecode_size = 0;
    char* bytecode = zymvm_compile_source_internal(parent_vm, source, "script.zym", &bytecode_size);

    if (!bytecode) {
        zym_runtimeError(parent_vm, "compileSource() compilation failed");
        return ZYM_ERROR;
    }

    ZymValue result = zymvm_bytecode_to_buffer(parent_vm, bytecode, bytecode_size);
    free(bytecode);
    return result;
}

ZymValue zymvm_loadFile(ZymVM* parent_vm, ZymValue context, ZymValue pathVal) {
    VMData* vmdata = (VMData*)zym_getNativeData(context);

    if (!zym_isString(pathVal)) {
        zym_runtimeError(parent_vm, "loadFile() requires a string path");
        return ZYM_ERROR;
    }

    const char* path = zym_asCString(pathVal);
    char* source = zymvm_read_file(path, NULL);
    if (!source) {
        zym_runtimeError(parent_vm, "loadFile() could not read file: %s", path);
        return ZYM_ERROR;
    }

    size_t bytecode_size = 0;
    char* bytecode = zymvm_compile_source_internal(parent_vm, source, path, &bytecode_size);
    free(source);

    if (!bytecode) {
        zym_runtimeError(parent_vm, "loadFile() compilation failed for: %s", path);
        return ZYM_ERROR;
    }

    // Deserialize and run in the nested VM
    ZymChunk* chunk = zym_newChunk(vmdata->vm);
    ZymStatus status = zym_deserializeChunk(vmdata->vm, chunk, bytecode, bytecode_size);
    free(bytecode);

    if (status != ZYM_STATUS_OK) {
        zym_freeChunk(vmdata->vm, chunk);
        return zym_newBool(false);
    }

    status = zym_runChunk(vmdata->vm, chunk);
    if (status != ZYM_STATUS_OK) {
        return zym_newBool(false);
    }

    vmdata->loaded = true;
    return zym_newBool(true);
}

ZymValue zymvm_loadSource(ZymVM* parent_vm, ZymValue context, ZymValue sourceVal) {
    VMData* vmdata = (VMData*)zym_getNativeData(context);

    if (!zym_isString(sourceVal)) {
        zym_runtimeError(parent_vm, "loadSource() requires a string source");
        return ZYM_ERROR;
    }

    const char* source = zym_asCString(sourceVal);

    size_t bytecode_size = 0;
    char* bytecode = zymvm_compile_source_internal(parent_vm, source, "script.zym", &bytecode_size);

    if (!bytecode) {
        zym_runtimeError(parent_vm, "loadSource() compilation failed");
        return ZYM_ERROR;
    }

    // Deserialize and run in the nested VM
    ZymChunk* chunk = zym_newChunk(vmdata->vm);
    ZymStatus status = zym_deserializeChunk(vmdata->vm, chunk, bytecode, bytecode_size);
    free(bytecode);

    if (status != ZYM_STATUS_OK) {
        zym_freeChunk(vmdata->vm, chunk);
        return zym_newBool(false);
    }

    status = zym_runChunk(vmdata->vm, chunk);
    if (status != ZYM_STATUS_OK) {
        return zym_newBool(false);
    }

    vmdata->loaded = true;
    return zym_newBool(true);
}

ZymValue zymvm_end(ZymVM* parent_vm, ZymValue context) {
    VMData* vmdata = (VMData*)zym_getNativeData(context);

    if (vmdata->vm) {
        zym_freeVM(vmdata->vm);
        vmdata->vm = NULL;
        vmdata->loaded = false;
        vmdata->has_result = false;
    }

    return context;
}

ZymValue nativeZymVM_create(ZymVM* vm) {
    VMData* vmdata = calloc(1, sizeof(VMData));
    if (!vmdata) {
        zym_runtimeError(vm, "Out of memory");
        return ZYM_ERROR;
    }

    vmdata->vm = zym_newVM();
    if (!vmdata->vm) {
        free(vmdata);
        zym_runtimeError(vm, "Failed to create nested VM");
        return ZYM_ERROR;
    }

    setupNatives(vmdata->vm);

    vmdata->loaded = false;
    vmdata->has_result = false;
    vmdata->last_result = zym_newNull();

    ZymValue context = zym_createNativeContext(vm, vmdata, zymvm_cleanup);
    zym_pushRoot(vm, context);

    #define CREATE_METHOD(name, func, sig) \
        ZymValue name = zym_createNativeClosure(vm, sig, func, context); \
        zym_pushRoot(vm, name);

    CREATE_METHOD(load, zymvm_load, "load(buffer)");
    CREATE_METHOD(compileFile, zymvm_compileFile, "compileFile(path)");
    CREATE_METHOD(compileSource, zymvm_compileSource, "compileSource(source)");
    CREATE_METHOD(loadFile, zymvm_loadFile, "loadFile(path)");
    CREATE_METHOD(loadSource, zymvm_loadSource, "loadSource(source)");
    CREATE_METHOD(hasFunction, zymvm_hasFunction, "hasFunction(name, arity)");
    CREATE_METHOD(call_0, zymvm_call_0, "call(name)");
    CREATE_METHOD(call_1, zymvm_call_1, "call(name, arg)");
    CREATE_METHOD(call_2, zymvm_call_2, "call(name, arg, arg)");
    CREATE_METHOD(call_3, zymvm_call_3, "call(name, arg, arg, arg)");
    CREATE_METHOD(call_4, zymvm_call_4, "call(name, arg, arg, arg, arg)");
    CREATE_METHOD(call_5, zymvm_call_5, "call(name, arg, arg, arg, arg, arg)");
    CREATE_METHOD(call_6, zymvm_call_6, "call(name, arg, arg, arg, arg, arg, arg)");
    CREATE_METHOD(call_7, zymvm_call_7, "call(name, arg, arg, arg, arg, arg, arg, arg)");
    CREATE_METHOD(call_8, zymvm_call_8, "call(name, arg, arg, arg, arg, arg, arg, arg, arg)");
    CREATE_METHOD(getCallResult, zymvm_getCallResult, "getCallResult()");
    CREATE_METHOD(end, zymvm_end, "end()");

    #undef CREATE_METHOD

    ZymValue call_dispatcher = zym_createDispatcher(vm);
    zym_pushRoot(vm, call_dispatcher);
    zym_addOverload(vm, call_dispatcher, call_0);
    zym_addOverload(vm, call_dispatcher, call_1);
    zym_addOverload(vm, call_dispatcher, call_2);
    zym_addOverload(vm, call_dispatcher, call_3);
    zym_addOverload(vm, call_dispatcher, call_4);
    zym_addOverload(vm, call_dispatcher, call_5);
    zym_addOverload(vm, call_dispatcher, call_6);
    zym_addOverload(vm, call_dispatcher, call_7);
    zym_addOverload(vm, call_dispatcher, call_8);

    ZymValue obj = zym_newMap(vm);
    zym_pushRoot(vm, obj);

    zym_mapSet(vm, obj, "load", load);
    zym_mapSet(vm, obj, "compileFile", compileFile);
    zym_mapSet(vm, obj, "compileSource", compileSource);
    zym_mapSet(vm, obj, "loadFile", loadFile);
    zym_mapSet(vm, obj, "loadSource", loadSource);
    zym_mapSet(vm, obj, "hasFunction", hasFunction);
    zym_mapSet(vm, obj, "call", call_dispatcher);
    zym_mapSet(vm, obj, "getCallResult", getCallResult);
    zym_mapSet(vm, obj, "end", end);

    for (int i = 0; i < 18; i++) {
        zym_popRoot(vm);
    }

    return obj;
}
