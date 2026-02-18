#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
    #include <io.h>
    #define PATH_SEP '\\'
    #define PATH_SEP_STR "\\"
    #define mkdir(path, mode) _mkdir(path)
    #define rmdir _rmdir
    #define stat _stat
    #define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
    #define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#else
    #include <unistd.h>
    #include <dirent.h>
    #define PATH_SEP '/'
    #define PATH_SEP_STR "/"
#endif

#include "./natives.h"

typedef struct {
    uint8_t* data;
    size_t capacity;
    size_t length;
    size_t position;
    ZymValue position_ref;
    bool auto_grow;
    int endianness;
} BufferData;

typedef enum {
    FILE_MODE_READ,
    FILE_MODE_WRITE,
    FILE_MODE_APPEND,
    FILE_MODE_READ_BINARY,
    FILE_MODE_WRITE_BINARY,
    FILE_MODE_APPEND_BINARY,
    FILE_MODE_READ_WRITE,
    FILE_MODE_READ_WRITE_BIN
} FileMode;

typedef struct {
    FILE* handle;
    char* path;
    FileMode mode;
    bool is_open;
    size_t position;
    ZymValue position_ref;
} FileData;

void file_cleanup(ZymVM* vm, void* ptr) {
    FileData* file = (FileData*)ptr;
    if (file->is_open && file->handle) {
        fclose(file->handle);
    }
    free(file->path);
    free(file);
}

static const char* file_mode_to_str(FileMode mode) {
    switch (mode) {
        case FILE_MODE_READ: return "r";
        case FILE_MODE_WRITE: return "w";
        case FILE_MODE_APPEND: return "a";
        case FILE_MODE_READ_BINARY: return "rb";
        case FILE_MODE_WRITE_BINARY: return "wb";
        case FILE_MODE_APPEND_BINARY: return "ab";
        case FILE_MODE_READ_WRITE: return "r+";
        case FILE_MODE_READ_WRITE_BIN: return "rb+";
        default: return "r";
    }
}

static bool parse_file_mode(const char* mode_str, FileMode* out_mode) {
    if (strcmp(mode_str, "r") == 0) { *out_mode = FILE_MODE_READ; return true; }
    if (strcmp(mode_str, "w") == 0) { *out_mode = FILE_MODE_WRITE; return true; }
    if (strcmp(mode_str, "a") == 0) { *out_mode = FILE_MODE_APPEND; return true; }
    if (strcmp(mode_str, "rb") == 0) { *out_mode = FILE_MODE_READ_BINARY; return true; }
    if (strcmp(mode_str, "wb") == 0) { *out_mode = FILE_MODE_WRITE_BINARY; return true; }
    if (strcmp(mode_str, "ab") == 0) { *out_mode = FILE_MODE_APPEND_BINARY; return true; }
    if (strcmp(mode_str, "r+") == 0) { *out_mode = FILE_MODE_READ_WRITE; return true; }
    if (strcmp(mode_str, "rb+") == 0 || strcmp(mode_str, "r+b") == 0) {
        *out_mode = FILE_MODE_READ_WRITE_BIN;
        return true;
    }
    return false;
}

static void sync_file_position(FileData* file) {
    if (file->is_open && file->handle) {
        long pos = ftell(file->handle);
        if (pos >= 0) {
            file->position = (size_t)pos;
            file->position_ref = zym_newNumber((double)pos);
        }
    }
}

ZymValue file_read(ZymVM* vm, ZymValue context) {
    FileData* file = (FileData*)zym_getNativeData(context);

    if (!file->is_open || !file->handle) {
        zym_runtimeError(vm, "File is not open");
        return ZYM_ERROR;
    }

    long original_pos = ftell(file->handle);

    fseek(file->handle, 0, SEEK_END);
    long size = ftell(file->handle);
    fseek(file->handle, original_pos, SEEK_SET);

    if (size < 0) {
        zym_runtimeError(vm, "Failed to determine file size");
        return ZYM_ERROR;
    }

    long remaining = size - original_pos;
    if (remaining <= 0) {
        return zym_newString(vm, "");
    }

    char* buffer = malloc(remaining + 1);
    if (!buffer) {
        zym_runtimeError(vm, "Out of memory");
        return ZYM_ERROR;
    }

    size_t bytes_read = fread(buffer, 1, remaining, file->handle);
    buffer[bytes_read] = '\0';

    sync_file_position(file);

    ZymValue result = zym_newString(vm, buffer);
    free(buffer);
    return result;
}

ZymValue file_readBytes(ZymVM* vm, ZymValue context, ZymValue countVal) {
    FileData* file = (FileData*)zym_getNativeData(context);

    if (!file->is_open || !file->handle) {
        zym_runtimeError(vm, "File is not open");
        return ZYM_ERROR;
    }

    if (!zym_isNumber(countVal)) {
        zym_runtimeError(vm, "readBytes() requires a number argument");
        return ZYM_ERROR;
    }

    size_t count = (size_t)zym_asNumber(countVal);
    if (count == 0) {
        return zym_newString(vm, "");
    }

    char* buffer = malloc(count + 1);
    if (!buffer) {
        zym_runtimeError(vm, "Out of memory");
        return ZYM_ERROR;
    }

    size_t bytes_read = fread(buffer, 1, count, file->handle);
    buffer[bytes_read] = '\0';

    sync_file_position(file);

    ZymValue result = zym_newString(vm, buffer);
    free(buffer);
    return result;
}

ZymValue file_readLine(ZymVM* vm, ZymValue context) {
    FileData* file = (FileData*)zym_getNativeData(context);

    if (!file->is_open || !file->handle) {
        zym_runtimeError(vm, "File is not open");
        return ZYM_ERROR;
    }

    size_t buffer_size = 256;
    char* buffer = malloc(buffer_size);
    if (!buffer) {
        zym_runtimeError(vm, "Out of memory");
        return ZYM_ERROR;
    }

    size_t pos = 0;
    while (true) {
        int c = fgetc(file->handle);

        if (c == EOF) {
            if (pos == 0) {
                free(buffer);
                return zym_newNull();
            }
            break;
        }

        if (c == '\n') {
            break;
        }

        if (c == '\r') {
            int next = fgetc(file->handle);
            if (next != '\n' && next != EOF) {
                ungetc(next, file->handle);
            }
            break;
        }

        if (pos + 1 >= buffer_size) {
            buffer_size *= 2;
            char* new_buffer = realloc(buffer, buffer_size);
            if (!new_buffer) {
                free(buffer);
                zym_runtimeError(vm, "Out of memory");
                return ZYM_ERROR;
            }
            buffer = new_buffer;
        }

        buffer[pos++] = (char)c;
    }

    buffer[pos] = '\0';
    sync_file_position(file);

    ZymValue result = zym_newString(vm, buffer);
    free(buffer);
    return result;
}

ZymValue file_readLines(ZymVM* vm, ZymValue context) {
    FileData* file = (FileData*)zym_getNativeData(context);

    if (!file->is_open || !file->handle) {
        zym_runtimeError(vm, "File is not open");
        return ZYM_ERROR;
    }

    ZymValue list = zym_newList(vm);
    zym_pushRoot(vm, list);

    long original_pos = ftell(file->handle);

    while (true) {
        ZymValue line = file_readLine(vm, context);
        if (zym_isNull(line)) {
            break;
        }
        zym_listAppend(vm, list, line);
    }

    zym_popRoot(vm);
    return list;
}

ZymValue file_write(ZymVM* vm, ZymValue context, ZymValue dataVal) {
    FileData* file = (FileData*)zym_getNativeData(context);

    if (!file->is_open || !file->handle) {
        zym_runtimeError(vm, "File is not open");
        return ZYM_ERROR;
    }

    if (!zym_isString(dataVal)) {
        zym_runtimeError(vm, "write() requires a string argument");
        return ZYM_ERROR;
    }

    const char* data = zym_asCString(dataVal);
    size_t len = strlen(data);

    size_t written = fwrite(data, 1, len, file->handle);
    sync_file_position(file);

    if (written != len) {
        zym_runtimeError(vm, "Failed to write all bytes to file");
        return ZYM_ERROR;
    }

    return context;
}

ZymValue file_writeLine(ZymVM* vm, ZymValue context, ZymValue dataVal) {
    FileData* file = (FileData*)zym_getNativeData(context);

    if (!file->is_open || !file->handle) {
        zym_runtimeError(vm, "File is not open");
        return ZYM_ERROR;
    }

    if (!zym_isString(dataVal)) {
        zym_runtimeError(vm, "writeLine() requires a string argument");
        return ZYM_ERROR;
    }

    const char* data = zym_asCString(dataVal);
    size_t len = strlen(data);

    size_t written = fwrite(data, 1, len, file->handle);
    if (written != len) {
        zym_runtimeError(vm, "Failed to write line to file");
        return ZYM_ERROR;
    }

    fputc('\n', file->handle);
    sync_file_position(file);

    return context;
}

ZymValue file_flush(ZymVM* vm, ZymValue context) {
    FileData* file = (FileData*)zym_getNativeData(context);

    if (!file->is_open || !file->handle) {
        zym_runtimeError(vm, "File is not open");
        return ZYM_ERROR;
    }

    if (fflush(file->handle) != 0) {
        zym_runtimeError(vm, "Failed to flush file");
        return ZYM_ERROR;
    }

    return context;
}

ZymValue file_seek(ZymVM* vm, ZymValue context, ZymValue posVal) {
    FileData* file = (FileData*)zym_getNativeData(context);

    if (!file->is_open || !file->handle) {
        zym_runtimeError(vm, "File is not open");
        return ZYM_ERROR;
    }

    if (!zym_isNumber(posVal)) {
        zym_runtimeError(vm, "seek() requires a number argument");
        return ZYM_ERROR;
    }

    long pos = (long)zym_asNumber(posVal);
    if (fseek(file->handle, pos, SEEK_SET) != 0) {
        zym_runtimeError(vm, "Failed to seek in file");
        return ZYM_ERROR;
    }

    sync_file_position(file);
    return context;
}

ZymValue file_tell(ZymVM* vm, ZymValue context) {
    FileData* file = (FileData*)zym_getNativeData(context);

    if (!file->is_open || !file->handle) {
        zym_runtimeError(vm, "File is not open");
        return ZYM_ERROR;
    }

    long pos = ftell(file->handle);
    if (pos < 0) {
        zym_runtimeError(vm, "Failed to get file position");
        return ZYM_ERROR;
    }

    return zym_newNumber((double)pos);
}

ZymValue file_size(ZymVM* vm, ZymValue context) {
    FileData* file = (FileData*)zym_getNativeData(context);

    if (!file->is_open || !file->handle) {
        zym_runtimeError(vm, "File is not open");
        return ZYM_ERROR;
    }

    long original_pos = ftell(file->handle);
    fseek(file->handle, 0, SEEK_END);
    long size = ftell(file->handle);
    fseek(file->handle, original_pos, SEEK_SET);

    if (size < 0) {
        zym_runtimeError(vm, "Failed to get file size");
        return ZYM_ERROR;
    }

    return zym_newNumber((double)size);
}

ZymValue file_eof(ZymVM* vm, ZymValue context) {
    FileData* file = (FileData*)zym_getNativeData(context);

    if (!file->is_open || !file->handle) {
        return zym_newBool(true);
    }

    if (feof(file->handle)) {
        return zym_newBool(true);
    }

    long current_pos = ftell(file->handle);
    fseek(file->handle, 0, SEEK_END);
    long size = ftell(file->handle);
    fseek(file->handle, current_pos, SEEK_SET);

    return zym_newBool(current_pos >= size);
}

ZymValue file_close(ZymVM* vm, ZymValue context) {
    FileData* file = (FileData*)zym_getNativeData(context);

    if (!file->is_open || !file->handle) {
        return context;
    }

    fclose(file->handle);
    file->handle = NULL;
    file->is_open = false;

    return context;
}

ZymValue file_isOpen(ZymVM* vm, ZymValue context) {
    FileData* file = (FileData*)zym_getNativeData(context);
    return zym_newBool(file->is_open);
}

ZymValue file_getPath(ZymVM* vm, ZymValue context) {
    FileData* file = (FileData*)zym_getNativeData(context);
    return zym_newString(vm, file->path);
}

ZymValue file_getMode(ZymVM* vm, ZymValue context) {
    FileData* file = (FileData*)zym_getNativeData(context);
    return zym_newString(vm, file_mode_to_str(file->mode));
}

void file_position_set_hook(ZymVM* vm, ZymValue context, ZymValue new_value) {
    FileData* file = (FileData*)zym_getNativeData(context);
    if (zym_isNumber(new_value) && file->is_open && file->handle) {
        long pos = (long)zym_asNumber(new_value);
        if (fseek(file->handle, pos, SEEK_SET) == 0) {
            file->position = (size_t)pos;
            sync_file_position(file);
        }
    }
}

static bool is_buffer_object(ZymVM* vm, ZymValue val) {
    if (!zym_isMap(val)) {
        return false;
    }
    ZymValue pos = zym_mapGet(vm, val, "position");
    return !zym_isNull(pos) && zym_isReference(pos);
}

static BufferData* get_buffer_data(ZymVM* vm, ZymValue bufferObj) {
    ZymValue method = zym_mapGet(vm, bufferObj, "read");
    if (zym_isNull(method)) {
        return NULL;
    }

    return NULL;
}

ZymValue file_readToBuffer(ZymVM* vm, ZymValue context, ZymValue bufferVal) {
    FileData* file = (FileData*)zym_getNativeData(context);

    if (!file->is_open || !file->handle) {
        zym_runtimeError(vm, "File is not open");
        return ZYM_ERROR;
    }

    if (!zym_isMap(bufferVal)) {
        zym_runtimeError(vm, "readToBuffer() requires a Buffer argument");
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

    long original_pos = ftell(file->handle);

    fseek(file->handle, 0, SEEK_END);
    long size = ftell(file->handle);
    fseek(file->handle, original_pos, SEEK_SET);

    long remaining = size - original_pos;
    if (remaining <= 0) {
        return zym_newNumber(0);
    }

    size_t available_space = buf->capacity - buf->position;
    if (available_space == 0) {
        zym_runtimeError(vm, "Buffer is full (position at capacity)");
        return ZYM_ERROR;
    }

    size_t bytes_to_read = (size_t)remaining;
    if (bytes_to_read > available_space) {
        bytes_to_read = available_space;  // Limit to buffer capacity for chunked reads
    }

    size_t bytes_read = fread(buf->data + buf->position, 1, bytes_to_read, file->handle);
    sync_file_position(file);

    buf->position += bytes_read;
    if (buf->position > buf->length) {
        buf->length = buf->position;
    }

    return zym_newNumber((double)bytes_read);
}

ZymValue file_writeFromBuffer(ZymVM* vm, ZymValue context, ZymValue bufferVal, ZymValue countVal) {
    FileData* file = (FileData*)zym_getNativeData(context);

    if (!file->is_open || !file->handle) {
        zym_runtimeError(vm, "File is not open");
        return ZYM_ERROR;
    }

    if (!zym_isMap(bufferVal)) {
        zym_runtimeError(vm, "writeFromBuffer() requires a Buffer argument");
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

    size_t bytesToWrite = buf->length - buf->position;
    if (!zym_isNull(countVal) && zym_isNumber(countVal)) {
        size_t requested = (size_t)zym_asNumber(countVal);
        if (requested < bytesToWrite) {
            bytesToWrite = requested;
        }
    }

    if (bytesToWrite == 0 || buf->position >= buf->length) {
        return zym_newNumber(0);
    }

    size_t written = fwrite(buf->data + buf->position, 1, bytesToWrite, file->handle);
    sync_file_position(file);

    buf->position += written;

    if (written != bytesToWrite) {
        zym_runtimeError(vm, "Failed to write all bytes to file");
        return ZYM_ERROR;
    }

    return zym_newNumber((double)written);
}

ZymValue nativeFile_open(ZymVM* vm, ZymValue pathVal, ZymValue modeVal) {
    if (!zym_isString(pathVal)) {
        zym_runtimeError(vm, "File.open() requires a string path");
        return ZYM_ERROR;
    }

    FileMode mode = FILE_MODE_READ;
    if (!zym_isNull(modeVal)) {
        if (!zym_isString(modeVal)) {
            zym_runtimeError(vm, "File.open() mode must be a string");
            return ZYM_ERROR;
        }
        const char* mode_str = zym_asCString(modeVal);
        if (!parse_file_mode(mode_str, &mode)) {
            zym_runtimeError(vm, "Invalid file mode: '%s'", mode_str);
            return ZYM_ERROR;
        }
    }

    const char* path = zym_asCString(pathVal);

    FileData* file = calloc(1, sizeof(FileData));
    if (!file) {
        zym_runtimeError(vm, "Out of memory");
        return ZYM_ERROR;
    }

    file->handle = fopen(path, file_mode_to_str(mode));
    if (!file->handle) {
        free(file);
        zym_runtimeError(vm, "Failed to open file '%s': %s", path, strerror(errno));
        return ZYM_ERROR;
    }

    file->path = strdup(path);
    file->mode = mode;
    file->is_open = true;
    file->position = 0;
    file->position_ref = zym_newNumber(0);

    ZymValue context = zym_createNativeContext(vm, file, file_cleanup);
    zym_pushRoot(vm, context);

    ZymValue posRef = zym_createNativeReference(vm, context,
        offsetof(FileData, position_ref), NULL, file_position_set_hook);
    zym_pushRoot(vm, posRef);

    #define CREATE_METHOD_0(name, func) \
        ZymValue name = zym_createNativeClosure(vm, #func "()", func, context); \
        zym_pushRoot(vm, name);

    #define CREATE_METHOD_1(name, func) \
        ZymValue name = zym_createNativeClosure(vm, #func "(arg)", func, context); \
        zym_pushRoot(vm, name);

    #define CREATE_METHOD_2(name, func) \
        ZymValue name = zym_createNativeClosure(vm, #func "(arg1, arg2)", func, context); \
        zym_pushRoot(vm, name);

    CREATE_METHOD_0(read, file_read);
    CREATE_METHOD_1(readBytes, file_readBytes);
    CREATE_METHOD_0(readLine, file_readLine);
    CREATE_METHOD_0(readLines, file_readLines);
    CREATE_METHOD_1(write, file_write);
    CREATE_METHOD_1(writeLine, file_writeLine);
    CREATE_METHOD_0(flush, file_flush);
    CREATE_METHOD_1(seek, file_seek);
    CREATE_METHOD_0(tell, file_tell);
    CREATE_METHOD_0(size, file_size);
    CREATE_METHOD_0(eof, file_eof);
    CREATE_METHOD_0(close, file_close);
    CREATE_METHOD_0(isOpen, file_isOpen);
    CREATE_METHOD_0(getPath, file_getPath);
    CREATE_METHOD_0(getMode, file_getMode);
    CREATE_METHOD_1(readToBuffer, file_readToBuffer);
    CREATE_METHOD_2(writeFromBuffer, file_writeFromBuffer);

    #undef CREATE_METHOD_0
    #undef CREATE_METHOD_1
    #undef CREATE_METHOD_2

    ZymValue obj = zym_newMap(vm);
    zym_pushRoot(vm, obj);

    zym_mapSet(vm, obj, "position", posRef);

    zym_mapSet(vm, obj, "read", read);
    zym_mapSet(vm, obj, "readBytes", readBytes);
    zym_mapSet(vm, obj, "readLine", readLine);
    zym_mapSet(vm, obj, "readLines", readLines);
    zym_mapSet(vm, obj, "write", write);
    zym_mapSet(vm, obj, "writeLine", writeLine);
    zym_mapSet(vm, obj, "flush", flush);
    zym_mapSet(vm, obj, "seek", seek);
    zym_mapSet(vm, obj, "tell", tell);
    zym_mapSet(vm, obj, "size", size);
    zym_mapSet(vm, obj, "eof", eof);
    zym_mapSet(vm, obj, "close", close);
    zym_mapSet(vm, obj, "isOpen", isOpen);
    zym_mapSet(vm, obj, "getPath", getPath);
    zym_mapSet(vm, obj, "getMode", getMode);
    zym_mapSet(vm, obj, "readToBuffer", readToBuffer);
    zym_mapSet(vm, obj, "writeFromBuffer", writeFromBuffer);

    // (context + posRef + 17 methods + obj = 20)
    for (int i = 0; i < 20; i++) {
        zym_popRoot(vm);
    }

    return obj;
}

ZymValue nativeFile_readFile(ZymVM* vm, ZymValue pathVal) {
    if (!zym_isString(pathVal)) {
        zym_runtimeError(vm, "File.readFile() requires a string path");
        return ZYM_ERROR;
    }

    const char* path = zym_asCString(pathVal);
    FILE* f = fopen(path, "rb");
    if (!f) {
        zym_runtimeError(vm, "Failed to open file '%s': %s", path, strerror(errno));
        return ZYM_ERROR;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size < 0) {
        fclose(f);
        zym_runtimeError(vm, "Failed to get file size");
        return ZYM_ERROR;
    }

    char* buffer = malloc(size + 1);
    if (!buffer) {
        fclose(f);
        zym_runtimeError(vm, "Out of memory");
        return ZYM_ERROR;
    }

    size_t read = fread(buffer, 1, size, f);
    buffer[read] = '\0';
    fclose(f);

    ZymValue result = zym_newString(vm, buffer);
    free(buffer);
    return result;
}

ZymValue nativeFile_writeFile(ZymVM* vm, ZymValue pathVal, ZymValue dataVal) {
    if (!zym_isString(pathVal)) {
        zym_runtimeError(vm, "File.writeFile() requires a string path");
        return ZYM_ERROR;
    }

    if (!zym_isString(dataVal)) {
        zym_runtimeError(vm, "File.writeFile() requires a string data");
        return ZYM_ERROR;
    }

    const char* path = zym_asCString(pathVal);
    const char* data = zym_asCString(dataVal);
    size_t len = strlen(data);

    FILE* f = fopen(path, "wb");
    if (!f) {
        zym_runtimeError(vm, "Failed to open file '%s': %s", path, strerror(errno));
        return ZYM_ERROR;
    }

    size_t written = fwrite(data, 1, len, f);
    fclose(f);

    if (written != len) {
        zym_runtimeError(vm, "Failed to write file");
        return ZYM_ERROR;
    }

    return zym_newNull();
}

ZymValue nativeFile_appendFile(ZymVM* vm, ZymValue pathVal, ZymValue dataVal) {
    if (!zym_isString(pathVal)) {
        zym_runtimeError(vm, "File.appendFile() requires a string path");
        return ZYM_ERROR;
    }

    if (!zym_isString(dataVal)) {
        zym_runtimeError(vm, "File.appendFile() requires a string data");
        return ZYM_ERROR;
    }

    const char* path = zym_asCString(pathVal);
    const char* data = zym_asCString(dataVal);
    size_t len = strlen(data);

    FILE* f = fopen(path, "ab");
    if (!f) {
        zym_runtimeError(vm, "Failed to open file '%s': %s", path, strerror(errno));
        return ZYM_ERROR;
    }

    size_t written = fwrite(data, 1, len, f);
    fclose(f);

    if (written != len) {
        zym_runtimeError(vm, "Failed to append to file");
        return ZYM_ERROR;
    }

    return zym_newNull();
}

ZymValue nativeFile_exists(ZymVM* vm, ZymValue pathVal) {
    if (!zym_isString(pathVal)) {
        zym_runtimeError(vm, "File.exists() requires a string path");
        return ZYM_ERROR;
    }

    const char* path = zym_asCString(pathVal);
    struct stat st;
    return zym_newBool(stat(path, &st) == 0);
}

ZymValue nativeFile_delete(ZymVM* vm, ZymValue pathVal) {
    if (!zym_isString(pathVal)) {
        zym_runtimeError(vm, "File.delete() requires a string path");
        return ZYM_ERROR;
    }

    const char* path = zym_asCString(pathVal);
    if (remove(path) != 0) {
        zym_runtimeError(vm, "Failed to delete file '%s': %s", path, strerror(errno));
        return ZYM_ERROR;
    }

    return zym_newNull();
}

ZymValue nativeFile_copy(ZymVM* vm, ZymValue srcVal, ZymValue dstVal) {
    if (!zym_isString(srcVal) || !zym_isString(dstVal)) {
        zym_runtimeError(vm, "File.copy() requires two string paths");
        return ZYM_ERROR;
    }

    const char* src = zym_asCString(srcVal);
    const char* dst = zym_asCString(dstVal);

    FILE* src_file = fopen(src, "rb");
    if (!src_file) {
        zym_runtimeError(vm, "Failed to open source file '%s': %s", src, strerror(errno));
        return ZYM_ERROR;
    }

    FILE* dst_file = fopen(dst, "wb");
    if (!dst_file) {
        fclose(src_file);
        zym_runtimeError(vm, "Failed to open destination file '%s': %s", dst, strerror(errno));
        return ZYM_ERROR;
    }

    // Copy in 4KB chunks
    char buffer[4096];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), src_file)) > 0) {
        if (fwrite(buffer, 1, bytes, dst_file) != bytes) {
            fclose(src_file);
            fclose(dst_file);
            zym_runtimeError(vm, "Failed to write to destination file");
            return ZYM_ERROR;
        }
    }

    fclose(src_file);
    fclose(dst_file);

    return zym_newNull();
}

ZymValue nativeFile_rename(ZymVM* vm, ZymValue oldPathVal, ZymValue newPathVal) {
    if (!zym_isString(oldPathVal) || !zym_isString(newPathVal)) {
        zym_runtimeError(vm, "File.rename() requires two string paths");
        return ZYM_ERROR;
    }

    const char* old_path = zym_asCString(oldPathVal);
    const char* new_path = zym_asCString(newPathVal);

    if (rename(old_path, new_path) != 0) {
        zym_runtimeError(vm, "Failed to rename file: %s", strerror(errno));
        return ZYM_ERROR;
    }

    return zym_newNull();
}

ZymValue nativeFile_stat(ZymVM* vm, ZymValue pathVal) {
    if (!zym_isString(pathVal)) {
        zym_runtimeError(vm, "File.stat() requires a string path");
        return ZYM_ERROR;
    }

    const char* path = zym_asCString(pathVal);
    struct stat st;

    if (stat(path, &st) != 0) {
        zym_runtimeError(vm, "Failed to stat file '%s': %s", path, strerror(errno));
        return ZYM_ERROR;
    }

    ZymValue info = zym_newMap(vm);
    zym_pushRoot(vm, info);

    zym_mapSet(vm, info, "size", zym_newNumber((double)st.st_size));
    zym_mapSet(vm, info, "isDirectory", zym_newBool(S_ISDIR(st.st_mode)));
    zym_mapSet(vm, info, "isFile", zym_newBool(S_ISREG(st.st_mode)));
    zym_mapSet(vm, info, "modified", zym_newNumber((double)st.st_mtime));

    zym_popRoot(vm);
    return info;
}

ZymValue nativeFile_readToNewBuffer(ZymVM* vm, ZymValue pathVal) {
    if (!zym_isString(pathVal)) {
        zym_runtimeError(vm, "fileReadBuffer() requires a string path");
        return ZYM_ERROR;
    }

    const char* path = zym_asCString(pathVal);
    FILE* f = fopen(path, "rb");
    if (!f) {
        zym_runtimeError(vm, "Failed to open file '%s': %s", path, strerror(errno));
        return ZYM_ERROR;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size < 0) {
        fclose(f);
        zym_runtimeError(vm, "Failed to get file size");
        return ZYM_ERROR;
    }

    ZymValue sizeVal = zym_newNumber((double)size);
    ZymValue buffer = nativeBuffer_create_auto(vm, sizeVal);
    if (zym_isNull(buffer)) {
        fclose(f);
        return ZYM_ERROR;
    }

    zym_pushRoot(vm, buffer);

    ZymValue getLength = zym_mapGet(vm, buffer, "getLength");
    if (zym_isNull(getLength)) {
        fclose(f);
        zym_popRoot(vm);
        zym_runtimeError(vm, "Invalid buffer object");
        return ZYM_ERROR;
    }

    ZymValue context = zym_getClosureContext(getLength);
    BufferData* buf = (BufferData*)zym_getNativeData(context);
    if (!buf) {
        fclose(f);
        zym_popRoot(vm);
        zym_runtimeError(vm, "Failed to get buffer data");
        return ZYM_ERROR;
    }

    size_t bytes_read = fread(buf->data, 1, size, f);
    fclose(f);

    if (bytes_read != (size_t)size) {
        zym_popRoot(vm);
        zym_runtimeError(vm, "Failed to read all data from file");
        return ZYM_ERROR;
    }

    buf->length = bytes_read;
    buf->position = 0;

    zym_popRoot(vm);
    return buffer;
}

ZymValue nativeFile_writeFromNewBuffer(ZymVM* vm, ZymValue pathVal, ZymValue bufferVal) {
    if (!zym_isString(pathVal)) {
        zym_runtimeError(vm, "fileWriteBuffer() requires a string path");
        return ZYM_ERROR;
    }

    if (!zym_isMap(bufferVal)) {
        zym_runtimeError(vm, "fileWriteBuffer() requires a Buffer argument");
        return ZYM_ERROR;
    }

    const char* path = zym_asCString(pathVal);

    ZymValue getLength = zym_mapGet(vm, bufferVal, "getLength");
    if (zym_isNull(getLength)) {
        zym_runtimeError(vm, "Argument is not a valid Buffer");
        return ZYM_ERROR;
    }

    ZymValue context = zym_getClosureContext(getLength);
    BufferData* buf = (BufferData*)zym_getNativeData(context);
    if (!buf) {
        zym_runtimeError(vm, "Failed to get buffer data");
        return ZYM_ERROR;
    }

    FILE* f = fopen(path, "wb");
    if (!f) {
        zym_runtimeError(vm, "Failed to open file '%s': %s", path, strerror(errno));
        return ZYM_ERROR;
    }

    size_t written = fwrite(buf->data, 1, buf->length, f);
    fclose(f);

    if (written != buf->length) {
        zym_runtimeError(vm, "Failed to write all bytes to file");
        return ZYM_ERROR;
    }

    return zym_newNull();
}

ZymValue nativeDir_create(ZymVM* vm, ZymValue pathVal) {
    if (!zym_isString(pathVal)) {
        zym_runtimeError(vm, "Dir.create() requires a string path");
        return ZYM_ERROR;
    }

    const char* path = zym_asCString(pathVal);

#ifdef _WIN32
    if (_mkdir(path) != 0) {
#else
    if (mkdir(path, 0755) != 0) {
#endif
        zym_runtimeError(vm, "Failed to create directory '%s': %s", path, strerror(errno));
        return ZYM_ERROR;
    }

    return zym_newNull();
}

ZymValue nativeDir_remove(ZymVM* vm, ZymValue pathVal) {
    if (!zym_isString(pathVal)) {
        zym_runtimeError(vm, "Dir.remove() requires a string path");
        return ZYM_ERROR;
    }

    const char* path = zym_asCString(pathVal);

    if (rmdir(path) != 0) {
        zym_runtimeError(vm, "Failed to remove directory '%s': %s", path, strerror(errno));
        return ZYM_ERROR;
    }

    return zym_newNull();
}

ZymValue nativeDir_list(ZymVM* vm, ZymValue pathVal) {
    if (!zym_isString(pathVal)) {
        zym_runtimeError(vm, "Dir.list() requires a string path");
        return ZYM_ERROR;
    }

    const char* path = zym_asCString(pathVal);
    ZymValue list = zym_newList(vm);
    zym_pushRoot(vm, list);

#ifdef _WIN32
    char search_path[MAX_PATH];
    snprintf(search_path, MAX_PATH, "%s\\*", path);

    WIN32_FIND_DATAA find_data;
    HANDLE hFind = FindFirstFileA(search_path, &find_data);

    if (hFind == INVALID_HANDLE_VALUE) {
        zym_popRoot(vm);
        zym_runtimeError(vm, "Failed to list directory '%s'", path);
        return ZYM_ERROR;
    }

    do {
        const char* name = find_data.cFileName;
        // Skip . and ..
        if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0) {
            zym_listAppend(vm, list, zym_newString(vm, name));
        }
    } while (FindNextFileA(hFind, &find_data));

    FindClose(hFind);
#else
    DIR* dir = opendir(path);
    if (!dir) {
        zym_popRoot(vm);
        zym_runtimeError(vm, "Failed to open directory '%s': %s", path, strerror(errno));
        return ZYM_ERROR;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        const char* name = entry->d_name;
        // Skip . and ..
        if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0) {
            zym_listAppend(vm, list, zym_newString(vm, name));
        }
    }

    closedir(dir);
#endif

    zym_popRoot(vm);
    return list;
}

ZymValue nativeDir_exists(ZymVM* vm, ZymValue pathVal) {
    if (!zym_isString(pathVal)) {
        zym_runtimeError(vm, "Dir.exists() requires a string path");
        return ZYM_ERROR;
    }

    const char* path = zym_asCString(pathVal);
    struct stat st;

    if (stat(path, &st) != 0) {
        return zym_newBool(false);
    }

    return zym_newBool(S_ISDIR(st.st_mode));
}

ZymValue nativePath_join(ZymVM* vm, ZymValue part1Val, ZymValue part2Val) {
    if (!zym_isString(part1Val) || !zym_isString(part2Val)) {
        zym_runtimeError(vm, "Path.join() requires two string arguments");
        return ZYM_ERROR;
    }

    const char* part1 = zym_asCString(part1Val);
    const char* part2 = zym_asCString(part2Val);

    size_t len1 = strlen(part1);
    size_t len2 = strlen(part2);

    // Check if part1 ends with separator
    bool has_sep = (len1 > 0 && (part1[len1-1] == '/' || part1[len1-1] == '\\'));

    // Check if part2 starts with separator
    bool starts_sep = (len2 > 0 && (part2[0] == '/' || part2[0] == '\\'));

    size_t buffer_size = len1 + len2 + 2;
    char* buffer = malloc(buffer_size);
    if (!buffer) {
        zym_runtimeError(vm, "Out of memory");
        return ZYM_ERROR;
    }

    if (has_sep && starts_sep) {
        // Both have separator - skip one
        snprintf(buffer, buffer_size, "%s%s", part1, part2 + 1);
    } else if (!has_sep && !starts_sep) {
        // Neither has separator - add one
        snprintf(buffer, buffer_size, "%s%s%s", part1, PATH_SEP_STR, part2);
    } else {
        // One has separator - just concatenate
        snprintf(buffer, buffer_size, "%s%s", part1, part2);
    }

    ZymValue result = zym_newString(vm, buffer);
    free(buffer);
    return result;
}

ZymValue nativePath_dirname(ZymVM* vm, ZymValue pathVal) {
    if (!zym_isString(pathVal)) {
        zym_runtimeError(vm, "Path.dirname() requires a string path");
        return ZYM_ERROR;
    }

    const char* path = zym_asCString(pathVal);
    size_t len = strlen(path);

    if (len == 0) {
        return zym_newString(vm, ".");
    }

    // Find last separator
    const char* last_sep = NULL;
    for (size_t i = 0; i < len; i++) {
        if (path[i] == '/' || path[i] == '\\') {
            last_sep = path + i;
        }
    }

    if (!last_sep) {
        return zym_newString(vm, ".");
    }

    // Copy up to separator
    size_t dir_len = last_sep - path;
    if (dir_len == 0) {
        return zym_newString(vm, PATH_SEP_STR);
    }

    char* buffer = malloc(dir_len + 1);
    if (!buffer) {
        zym_runtimeError(vm, "Out of memory");
        return ZYM_ERROR;
    }

    memcpy(buffer, path, dir_len);
    buffer[dir_len] = '\0';

    ZymValue result = zym_newString(vm, buffer);
    free(buffer);
    return result;
}

ZymValue nativePath_basename(ZymVM* vm, ZymValue pathVal) {
    if (!zym_isString(pathVal)) {
        zym_runtimeError(vm, "Path.basename() requires a string path");
        return ZYM_ERROR;
    }

    const char* path = zym_asCString(pathVal);
    size_t len = strlen(path);

    if (len == 0) {
        return zym_newString(vm, "");
    }

    // Find last separator
    const char* last_sep = NULL;
    for (size_t i = 0; i < len; i++) {
        if (path[i] == '/' || path[i] == '\\') {
            last_sep = path + i;
        }
    }

    if (!last_sep) {
        return pathVal;
    }

    return zym_newString(vm, last_sep + 1);
}

ZymValue nativePath_extension(ZymVM* vm, ZymValue pathVal) {
    if (!zym_isString(pathVal)) {
        zym_runtimeError(vm, "Path.extension() requires a string path");
        return ZYM_ERROR;
    }

    const char* path = zym_asCString(pathVal);
    size_t len = strlen(path);

    // Find last dot after last separator
    const char* last_dot = NULL;
    const char* last_sep = NULL;

    for (size_t i = 0; i < len; i++) {
        if (path[i] == '/' || path[i] == '\\') {
            last_sep = path + i;
            last_dot = NULL;  // Reset dot search after separator
        } else if (path[i] == '.') {
            last_dot = path + i;
        }
    }

    if (!last_dot || (last_sep && last_dot < last_sep)) {
        return zym_newString(vm, "");
    }

    return zym_newString(vm, last_dot);
}

ZymValue nativePath_normalize(ZymVM* vm, ZymValue pathVal) {
    if (!zym_isString(pathVal)) {
        zym_runtimeError(vm, "Path.normalize() requires a string path");
        return ZYM_ERROR;
    }

    const char* path = zym_asCString(pathVal);
    size_t len = strlen(path);

    if (len == 0) {
        return zym_newString(vm, ".");
    }

    char* normalized = malloc(len + 1);
    if (!normalized) {
        zym_runtimeError(vm, "Out of memory");
        return ZYM_ERROR;
    }

    char** segments = malloc(len * sizeof(char*));
    if (!segments) {
        free(normalized);
        zym_runtimeError(vm, "Out of memory");
        return ZYM_ERROR;
    }

    bool is_absolute = false;
    char* drive_prefix = NULL;
    size_t path_start = 0;

#ifdef _WIN32
    // Handle Windows drive letter (C:, D:, etc.)
    if (len >= 2 && path[1] == ':' &&
        ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z'))) {
        drive_prefix = malloc(3);
        if (drive_prefix) {
            drive_prefix[0] = path[0];
            drive_prefix[1] = ':';
            drive_prefix[2] = '\0';
        }
        path_start = 2;
        if (len > 2 && (path[2] == '/' || path[2] == '\\')) {
            is_absolute = true;
            path_start = 3;
        }
    }
    // Handle UNC paths (\\server\share)
    else if (len >= 2 && (path[0] == '\\' || path[0] == '/') &&
             (path[1] == '\\' || path[1] == '/')) {
        is_absolute = true;
        drive_prefix = malloc(3);
        if (drive_prefix) {
            drive_prefix[0] = '\\';
            drive_prefix[1] = '\\';
            drive_prefix[2] = '\0';
        }
        path_start = 2;
    }
    else if (path[0] == '/' || path[0] == '\\') {
        is_absolute = true;
        path_start = 1;
    }
#else
    // Unix: check for leading /
    if (path[0] == '/') {
        is_absolute = true;
        path_start = 1;
    }
#endif

    // Split path into segments
    size_t segment_count = 0;
    size_t seg_start = path_start;

    for (size_t i = path_start; i <= len; i++) {
        if (i == len || path[i] == '/' || path[i] == '\\') {
            size_t seg_len = i - seg_start;

            if (seg_len > 0) {
                char* segment = malloc(seg_len + 1);
                if (!segment) {
                    for (size_t j = 0; j < segment_count; j++) {
                        free(segments[j]);
                    }
                    free(segments);
                    free(normalized);
                    if (drive_prefix) free(drive_prefix);
                    zym_runtimeError(vm, "Out of memory");
                    return ZYM_ERROR;
                }
                memcpy(segment, path + seg_start, seg_len);
                segment[seg_len] = '\0';
                segments[segment_count++] = segment;
            }

            seg_start = i + 1;
        }
    }

    // Process segments to resolve . and ..
    char** resolved = malloc(segment_count * sizeof(char*));
    if (!resolved) {
        for (size_t i = 0; i < segment_count; i++) {
            free(segments[i]);
        }
        free(segments);
        free(normalized);
        if (drive_prefix) free(drive_prefix);
        zym_runtimeError(vm, "Out of memory");
        return ZYM_ERROR;
    }

    size_t resolved_count = 0;

    for (size_t i = 0; i < segment_count; i++) {
        if (strcmp(segments[i], ".") == 0) {
            free(segments[i]);
            continue;
        } else if (strcmp(segments[i], "..") == 0) {
            // Go up one directory if possible
            if (resolved_count > 0 && strcmp(resolved[resolved_count - 1], "..") != 0) {
                // Pop the last segment
                free(resolved[--resolved_count]);
            } else if (!is_absolute) {
                // Keep .. for relative paths if we can't go up
                resolved[resolved_count++] = segments[i];
                continue;
            }
            free(segments[i]);
        } else {
            // Regular segment
            resolved[resolved_count++] = segments[i];
        }
    }

    size_t pos = 0;

    // Add drive prefix or root for absolute paths
    if (drive_prefix) {
        strcpy(normalized, drive_prefix);
        pos = strlen(drive_prefix);
        free(drive_prefix);
    }

    if (is_absolute && !drive_prefix) {
        normalized[pos++] = PATH_SEP;
    }

    // Add resolved segments
    for (size_t i = 0; i < resolved_count; i++) {
        if (i > 0 || (is_absolute && drive_prefix)) {
            normalized[pos++] = PATH_SEP;
        }
        size_t seg_len = strlen(resolved[i]);
        memcpy(normalized + pos, resolved[i], seg_len);
        pos += seg_len;
        free(resolved[i]);
    }

    // Handle empty path (becomes ".")
    if (pos == 0) {
        normalized[pos++] = '.';
    }

    normalized[pos] = '\0';

    free(segments);
    free(resolved);

    ZymValue result = zym_newString(vm, normalized);
    free(normalized);
    return result;
}

ZymValue nativePath_absolute(ZymVM* vm, ZymValue pathVal) {
    if (!zym_isString(pathVal)) {
        zym_runtimeError(vm, "Path.absolute() requires a string path");
        return ZYM_ERROR;
    }

    const char* path = zym_asCString(pathVal);

#ifdef _WIN32
    char buffer[MAX_PATH];
    if (!GetFullPathNameA(path, MAX_PATH, buffer, NULL)) {
        zym_runtimeError(vm, "Failed to get absolute path");
        return ZYM_ERROR;
    }
#else
    char* buffer = realpath(path, NULL);
    if (!buffer) {
        zym_runtimeError(vm, "Failed to get absolute path: %s", strerror(errno));
        return ZYM_ERROR;
    }
#endif

    ZymValue result = zym_newString(vm, buffer);

#ifndef _WIN32
    free(buffer);
#endif

    return result;
}

ZymValue nativePath_isAbsolute(ZymVM* vm, ZymValue pathVal) {
    if (!zym_isString(pathVal)) {
        zym_runtimeError(vm, "Path.isAbsolute() requires a string path");
        return ZYM_ERROR;
    }

    const char* path = zym_asCString(pathVal);
    size_t len = strlen(path);

    if (len == 0) {
        return zym_newBool(false);
    }

#ifdef _WIN32
    // Windows: check for drive letter or UNC path
    if (len >= 2 && path[1] == ':') {
        return zym_newBool(true);
    }
    if (len >= 2 && path[0] == '\\' && path[1] == '\\') {
        return zym_newBool(true);
    }
    return zym_newBool(false);
#else
    // Unix: check for leading /
    return zym_newBool(path[0] == '/');
#endif
}
