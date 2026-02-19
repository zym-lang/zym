#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "runtime_loader.h"
#include "zym/zym.h"

void setupNatives(ZymVM* vm);

// Bytecode package format:
// [bytecode][4B size little-endian][8B magic "ZYMBCODE"]
#define FOOTER_MAGIC "ZYMBCODE"
#define FOOTER_SIZE 12

char* get_executable_path(char* buffer, size_t size) {
#ifdef _WIN32
    DWORD result = GetModuleFileNameA(NULL, buffer, (DWORD)size);
    if (result == 0 || result == size) {
        return NULL;
    }
    return buffer;
#else
    ssize_t len = readlink("/proc/self/exe", buffer, size - 1);
    if (len == -1) {
        return NULL;
    }
    buffer[len] = '\0';
    return buffer;
#endif
}

static char* extract_embedded_bytecode(size_t* out_size) {
    char exe_path[4096];
    if (!get_executable_path(exe_path, sizeof(exe_path))) {
        fprintf(stderr, "Error: Could not determine executable path.\n");
        return NULL;
    }

    FILE* file = fopen(exe_path, "rb");
    if (!file) {
        fprintf(stderr, "Error: Could not open executable for reading.\n");
        return NULL;
    }

    fseek(file, 0L, SEEK_END);
    long file_size = ftell(file);

    if (file_size < FOOTER_SIZE) {
        fprintf(stderr, "Error: Executable too small to contain embedded bytecode.\n");
        fclose(file);
        return NULL;
    }

    fseek(file, file_size - FOOTER_SIZE, SEEK_SET);
    unsigned char footer[FOOTER_SIZE];
    if (fread(footer, 1, FOOTER_SIZE, file) != FOOTER_SIZE) {
        fprintf(stderr, "Error: Could not read bytecode footer.\n");
        fclose(file);
        return NULL;
    }

    if (memcmp(footer + 4, FOOTER_MAGIC, 8) != 0) {
        fprintf(stderr, "Error: No embedded bytecode found (missing magic footer).\n");
        fclose(file);
        return NULL;
    }

    uint32_t bytecode_size =
        ((uint32_t)footer[0]) |
        ((uint32_t)footer[1] << 8) |
        ((uint32_t)footer[2] << 16) |
        ((uint32_t)footer[3] << 24);

    if (bytecode_size == 0 || bytecode_size > 100 * 1024 * 1024) {
        fprintf(stderr, "Error: Invalid bytecode size: %u bytes.\n", bytecode_size);
        fclose(file);
        return NULL;
    }

    long bytecode_offset = file_size - FOOTER_SIZE - bytecode_size;
    if (bytecode_offset < 0) {
        fprintf(stderr, "Error: Bytecode size exceeds file size.\n");
        fclose(file);
        return NULL;
    }

    char* bytecode = (char*)malloc(bytecode_size);
    if (!bytecode) {
        fprintf(stderr, "Error: Could not allocate memory for bytecode (%u bytes).\n", bytecode_size);
        fclose(file);
        return NULL;
    }

    fseek(file, bytecode_offset, SEEK_SET);
    size_t read_count = fread(bytecode, 1, bytecode_size, file);
    fclose(file);

    if (read_count != bytecode_size) {
        fprintf(stderr, "Error: Could not read complete bytecode.\n");
        free(bytecode);
        return NULL;
    }

    if (bytecode_size < 5 || memcmp(bytecode, "ZYM\0", 4) != 0) {
        fprintf(stderr, "Error: Invalid bytecode format (missing ZYM header).\n");
        free(bytecode);
        return NULL;
    }

    *out_size = bytecode_size;
    return bytecode;
}

bool has_embedded_bytecode(void) {
    char exe_path[4096];
    if (!get_executable_path(exe_path, sizeof(exe_path))) {
        return false;
    }

    FILE* file = fopen(exe_path, "rb");
    if (!file) {
        return false;
    }

    fseek(file, 0L, SEEK_END);
    long file_size = ftell(file);

    if (file_size < FOOTER_SIZE) {
        fclose(file);
        return false;
    }

    fseek(file, file_size - FOOTER_SIZE, SEEK_SET);
    unsigned char footer[FOOTER_SIZE];
    if (fread(footer, 1, FOOTER_SIZE, file) != FOOTER_SIZE) {
        fclose(file);
        return false;
    }
    fclose(file);

    return memcmp(footer + 4, FOOTER_MAGIC, 8) == 0;
}

int runtime_main(int argc, char** argv) {
    size_t bytecode_size = 0;
    char* bytecode = extract_embedded_bytecode(&bytecode_size);
    if (!bytecode) {
        return 1;
    }

    ZymVM* vm = zym_newVM();
    ZymChunk* chunk = zym_newChunk(vm);

    setupNatives(vm);

    if (zym_deserializeChunk(vm, chunk, bytecode, bytecode_size) != ZYM_STATUS_OK) {
        fprintf(stderr, "Error: Failed to deserialize bytecode.\n");
        free(bytecode);
        zym_freeChunk(vm, chunk);
        zym_freeVM(vm);
        return 1;
    }

    free(bytecode);

    ZymStatus result = zym_runChunk(vm, chunk);
    while (result == ZYM_STATUS_YIELD) {
        result = zym_resume(vm);
    }
    if (result != ZYM_STATUS_OK) {
        fprintf(stderr, "Error: Runtime error occurred.\n");
        zym_freeChunk(vm, chunk);
        zym_freeVM(vm);
        return 1;
    }

    ZymValue argv_list = zym_newList(vm);
    for (int i = 0; i < argc; i++) {
        zym_listAppend(vm, argv_list, zym_newString(vm, argv[i]));
    }

    if (zym_hasFunction(vm, "main", 1)) {
        ZymStatus call_result = zym_call(vm, "main", 1, argv_list);
        while (call_result == ZYM_STATUS_YIELD) {
            call_result = zym_resume(vm);
        }
        if (call_result != ZYM_STATUS_OK) {
            fprintf(stderr, "Error: main(argv) function failed.\n");
            zym_freeChunk(vm, chunk);
            zym_freeVM(vm);
            return 1;
        }
    }

    zym_freeChunk(vm, chunk);
    zym_freeVM(vm);

    return 0;
}
