#include <stdio.h>
#include <time.h>
#include "../natives.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

ZymValue nativeTime_clock(ZymVM* vm) {
    clock_t current = clock();
    double seconds = (double)current / CLOCKS_PER_SEC;
    return zym_newNumber(seconds);
}

ZymValue nativeTime_sleep(ZymVM* vm, ZymValue milliseconds) {
    if (!zym_isNumber(milliseconds)) {
        zym_runtimeError(vm, "sleep() requires a number argument (milliseconds)");
        return ZYM_ERROR;
    }

    double ms = zym_asNumber(milliseconds);
    if (ms < 0) {
        zym_runtimeError(vm, "sleep() requires a non-negative argument");
        return ZYM_ERROR;
    }

#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    usleep((useconds_t)(ms * 1000));
#endif

    return zym_newNull();
}
