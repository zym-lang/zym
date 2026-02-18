#ifndef RUNTIME_LOADER_H
#define RUNTIME_LOADER_H

#include <stdbool.h>
#include <stddef.h>

bool has_embedded_bytecode(void);
char* get_executable_path(char* buffer, size_t size);
int runtime_main(int argc, char** argv);

#endif
