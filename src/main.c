#include "runtime_loader.h"
#include "full_executor.h"

int main(int argc, char** argv)
{
    if (has_embedded_bytecode()) {
        return runtime_main(argc, argv);
    }
    return full_main(argc, argv);
}
