#include "./natives.h"

void setupNatives(ZymVM* vm)
{
    zym_defineNative(vm, "print(a)", nativePrint_01);
    zym_defineNative(vm, "print(a, b)", nativePrint_02);
    zym_defineNative(vm, "print(a, b, c)", nativePrint_03);
    zym_defineNative(vm, "print(a, b, c, d)", nativePrint_04);
    zym_defineNative(vm, "print(a, b, c, d, e)", nativePrint_05);
    zym_defineNative(vm, "print(a, b, c, d, e, f)", nativePrint_06);
    zym_defineNative(vm, "print(a, b, c, d, e, f, g)", nativePrint_07);
    zym_defineNative(vm, "print(a, b, c, d, e, f, g, h)", nativePrint_08);
    zym_defineNative(vm, "print(a, b, c, d, e, f, g, h, i)", nativePrint_09);
    zym_defineNative(vm, "print(a, b, c, d, e, f, g, h, i, j)", nativePrint_10);
    zym_defineNative(vm, "print(a, b, c, d, e, f, g, h, i, j, k)", nativePrint_11);
    zym_defineNative(vm, "print(a, b, c, d, e, f, g, h, i, j, k, l)", nativePrint_12);
    zym_defineNative(vm, "print(a, b, c, d, e, f, g, h, i, j, k, l, m)", nativePrint_13);
    zym_defineNative(vm, "print(a, b, c, d, e, f, g, h, i, j, k, l, m, n)", nativePrint_14);
    zym_defineNative(vm, "print(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o)", nativePrint_15);
    zym_defineNative(vm, "print(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p)", nativePrint_16);
    zym_defineNative(vm, "print(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q)", nativePrint_17);
    zym_defineNative(vm, "print(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r)", nativePrint_18);
    zym_defineNative(vm, "print(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s)", nativePrint_19);
    zym_defineNative(vm, "print(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t)", nativePrint_20);
    zym_defineNative(vm, "print(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u)", nativePrint_21);
    zym_defineNative(vm, "print(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v)", nativePrint_22);
    zym_defineNative(vm, "print(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w)", nativePrint_23);
    zym_defineNative(vm, "print(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x)", nativePrint_24);
    zym_defineNative(vm, "print(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y)", nativePrint_25);
    zym_defineNative(vm, "print(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z)", nativePrint_26);

    zym_defineNative(vm, "clock()", nativeTime_clock);
    zym_defineNative(vm, "sleep(milliseconds)", nativeTime_sleep);
}
