#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "runtime_loader.h"
#include "full_executor.h"
#include "zym/zym.h"

// ---- Custom Allocator Example ------------------------------------------------
// This demonstrates how to define and use a custom allocator with the Zym API.
// In practice, you could replace these with a pool allocator, arena allocator,
// or any other memory management strategy. Here we simply wrap the standard
// C library functions to show the wiring.

static void* cli_alloc(void* ctx, size_t size) {
    (void)ctx;
    return malloc(size);
}

static void* cli_calloc(void* ctx, size_t count, size_t size) {
    (void)ctx;
    return calloc(count, size);
}

static void* cli_realloc(void* ctx, void* ptr, size_t old_size, size_t new_size) {
    (void)ctx;
    (void)old_size;
    return realloc(ptr, new_size);
}

static void cli_free(void* ctx, void* ptr, size_t size) {
    (void)ctx;
    (void)size;
    free(ptr);
}

int main(int argc, char** argv)
{
    // Create a custom allocator that wraps standard malloc/free.
    // A real application could point these at a custom pool, arena, etc.
    ZymAllocator allocator = {
        .alloc   = cli_alloc,
        .calloc  = cli_calloc,
        .realloc = cli_realloc,
        .free    = cli_free,
        .ctx     = NULL
    };

    if (has_embedded_bytecode()) {
        return runtime_main(argc, argv, &allocator);
    }
    return full_main(argc, argv, &allocator);
}
