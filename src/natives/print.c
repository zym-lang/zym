#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#include "natives.h"

static bool printFormattedValue(ZymVM* vm, char format, ZymValue val, int argIndex) {
    switch (format) {
        case 's':
            if (!zym_isString(val)) {
                zym_runtimeError(vm, "print() format %%s at position %d expects string, got %s", argIndex, zym_typeName(val));
                return false;
            }
            printf("%s", zym_asCString(val));
            break;

        case 'n':
            if (!zym_isNumber(val)) {
                zym_runtimeError(vm, "print() format %%n at position %d expects number, got %s", argIndex, zym_typeName(val));
                return false;
            }
            {
                double num = zym_asNumber(val);
                // Check if integer safely - avoid casting NaN/Inf to long long
                if (isfinite(num) && num == (double)(long long)num && num >= -1e15 && num <= 1e15) {
                    printf("%.0f", num);
                } else {
                    printf("%g", num);
                }
            }
            break;

        case 'b':
            if (!zym_isBool(val)) {
                zym_runtimeError(vm, "print() format %%b at position %d expects bool, got %s", argIndex, zym_typeName(val));
                return false;
            }
            printf("%s", zym_asBool(val) ? "true" : "false");
            break;

        case 'l':
            if (!zym_isList(val)) {
                zym_runtimeError(vm, "print() format %%l at position %d expects list, got %s", argIndex, zym_typeName(val));
                return false;
            }
            zym_printValue(vm, val);
            break;

        case 'm':
            if (!zym_isMap(val)) {
                zym_runtimeError(vm, "print() format %%m at position %d expects map, got %s", argIndex, zym_typeName(val));
                return false;
            }
            zym_printValue(vm, val);
            break;

        case 't':
            if (!zym_isStruct(val)) {
                zym_runtimeError(vm, "print() format %%t at position %d expects struct, got %s", argIndex, zym_typeName(val));
                return false;
            }
            zym_printValue(vm, val);
            break;

        case 'e':
            if (!zym_isEnum(val)) {
                zym_runtimeError(vm, "print() format %%e at position %d expects enum, got %s", argIndex, zym_typeName(val));
                return false;
            }
            zym_printValue(vm, val);
            break;

        case 'f':
            if (!zym_isFunction(val)) {
                zym_runtimeError(vm, "print() format %%f at position %d expects function, got %s", argIndex, zym_typeName(val));
                return false;
            }
            zym_printValue(vm, val);
            break;

        case 'r':
            if (!zym_isReference(val) && !zym_isNativeReference(val)) {
                zym_runtimeError(vm, "print() format %%r at position %d expects reference, got %s", argIndex, zym_typeName(val));
                return false;
            }
            zym_printValue(vm, val);
            break;

        case 'v':
            zym_printValue(vm, val);
            break;

        default:
            zym_runtimeError(vm, "print() unknown format specifier '%%%c'", format);
            return false;
    }
    return true;
}

static ZymValue print_impl(ZymVM* vm, const char* format_str, ZymValue* args, int arg_count) {
    const char* ptr = format_str;
    int arg_index = 0;

    while (*ptr) {
        if (*ptr == '%') {
            ptr++;
            if (*ptr == '\0') {
                zym_runtimeError(vm, "print() format string ends with incomplete format specifier");
                return ZYM_ERROR;
            }

            if (*ptr == '%') {
                printf("%%");
                ptr++;
            } else {
                if (arg_index >= arg_count) {
                    zym_runtimeError(vm, "print() format string requires more arguments than provided");
                    return ZYM_ERROR;
                }

                if (!printFormattedValue(vm, *ptr, args[arg_index], arg_index + 1)) {
                    return ZYM_ERROR;
                }

                arg_index++;
                ptr++;
            }
        } else {
            printf("%c", *ptr);
            ptr++;
        }
    }

    if (arg_index < arg_count) {
        zym_runtimeError(vm, "print() provided %d arguments but format string only uses %d", arg_count, arg_index);
        return ZYM_ERROR;
    }

    printf("\n");
    return zym_newNull();
}

ZymValue nativePrint_01(ZymVM* vm, ZymValue value) {
    if (zym_isString(value)) {
        const char* str = zym_asCString(value);
        bool has_format = false;
        for (const char* p = str; *p; p++) {
            if (*p == '%' && *(p + 1) != '%') {
                has_format = true;
                break;
            }
        }

        if (has_format) {
            return print_impl(vm, str, NULL, 0);
        }
    }

    zym_printValue(vm, value);
    printf("\n");

    return zym_newNull();
}

ZymValue nativePrint_02(ZymVM* vm, ZymValue format, ZymValue a) {
    if (!zym_isString(format)) {
        zym_runtimeError(vm, "print() first argument must be a string");
        return ZYM_ERROR;
    }

    ZymValue args[] = { a };
    return print_impl(vm, zym_asCString(format), args, 1);
}

ZymValue nativePrint_03(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b) {
    if (!zym_isString(format)) {
        zym_runtimeError(vm, "print() first argument must be a string");
        return ZYM_ERROR;
    }

    ZymValue args[] = { a, b };
    return print_impl(vm, zym_asCString(format), args, 2);
}

ZymValue nativePrint_04(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b, ZymValue c) {
    if (!zym_isString(format)) {
        zym_runtimeError(vm, "print() first argument must be a string");
        return ZYM_ERROR;
    }

    ZymValue args[] = { a, b, c };
    return print_impl(vm, zym_asCString(format), args, 3);
}

ZymValue nativePrint_05(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b, ZymValue c, ZymValue d) {
    if (!zym_isString(format)) {
        zym_runtimeError(vm, "print() first argument must be a string");
        return ZYM_ERROR;
    }

    ZymValue args[] = { a, b, c, d };
    return print_impl(vm, zym_asCString(format), args, 4);
}

ZymValue nativePrint_06(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b, ZymValue c, ZymValue d, ZymValue e) {
    if (!zym_isString(format)) {
        zym_runtimeError(vm, "print() first argument must be a string");
        return ZYM_ERROR;
    }

    ZymValue args[] = { a, b, c, d, e };
    return print_impl(vm, zym_asCString(format), args, 5);
}

ZymValue nativePrint_07(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b, ZymValue c, ZymValue d, ZymValue e, ZymValue f) {
    if (!zym_isString(format)) {
        zym_runtimeError(vm, "print() first argument must be a string");
        return ZYM_ERROR;
    }

    ZymValue args[] = { a, b, c, d, e, f };
    return print_impl(vm, zym_asCString(format), args, 6);
}

ZymValue nativePrint_08(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b, ZymValue c, ZymValue d, ZymValue e, ZymValue f, ZymValue g) {
    if (!zym_isString(format)) {
        zym_runtimeError(vm, "print() first argument must be a string");
        return ZYM_ERROR;
    }

    ZymValue args[] = { a, b, c, d, e, f, g };
    return print_impl(vm, zym_asCString(format), args, 7);
}

ZymValue nativePrint_09(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b, ZymValue c, ZymValue d, ZymValue e, ZymValue f, ZymValue g, ZymValue h) {
    if (!zym_isString(format)) {
        zym_runtimeError(vm, "print() first argument must be a string");
        return ZYM_ERROR;
    }

    ZymValue args[] = { a, b, c, d, e, f, g, h };
    return print_impl(vm, zym_asCString(format), args, 8);
}

ZymValue nativePrint_10(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b, ZymValue c, ZymValue d, ZymValue e, ZymValue f, ZymValue g, ZymValue h, ZymValue i) {
    if (!zym_isString(format)) {
        zym_runtimeError(vm, "print() first argument must be a string");
        return ZYM_ERROR;
    }

    ZymValue args[] = { a, b, c, d, e, f, g, h, i };
    return print_impl(vm, zym_asCString(format), args, 9);
}

ZymValue nativePrint_11(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b, ZymValue c, ZymValue d, ZymValue e, ZymValue f, ZymValue g, ZymValue h, ZymValue i, ZymValue j) {
    if (!zym_isString(format)) {
        zym_runtimeError(vm, "print() first argument must be a string");
        return ZYM_ERROR;
    }

    ZymValue args[] = { a, b, c, d, e, f, g, h, i, j };
    return print_impl(vm, zym_asCString(format), args, 10);
}

ZymValue nativePrint_12(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b, ZymValue c, ZymValue d, ZymValue e, ZymValue f, ZymValue g, ZymValue h, ZymValue i, ZymValue j, ZymValue k) {
    if (!zym_isString(format)) {
        zym_runtimeError(vm, "print() first argument must be a string");
        return ZYM_ERROR;
    }

    ZymValue args[] = { a, b, c, d, e, f, g, h, i, j, k };
    return print_impl(vm, zym_asCString(format), args, 11);
}

ZymValue nativePrint_13(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b, ZymValue c, ZymValue d, ZymValue e, ZymValue f, ZymValue g, ZymValue h, ZymValue i, ZymValue j, ZymValue k, ZymValue l) {
    if (!zym_isString(format)) {
        zym_runtimeError(vm, "print() first argument must be a string");
        return ZYM_ERROR;
    }

    ZymValue args[] = { a, b, c, d, e, f, g, h, i, j, k, l };
    return print_impl(vm, zym_asCString(format), args, 12);
}

ZymValue nativePrint_14(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b, ZymValue c, ZymValue d, ZymValue e, ZymValue f, ZymValue g, ZymValue h, ZymValue i, ZymValue j, ZymValue k, ZymValue l, ZymValue m) {
    if (!zym_isString(format)) {
        zym_runtimeError(vm, "print() first argument must be a string");
        return ZYM_ERROR;
    }

    ZymValue args[] = { a, b, c, d, e, f, g, h, i, j, k, l, m };
    return print_impl(vm, zym_asCString(format), args, 13);
}

ZymValue nativePrint_15(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b, ZymValue c, ZymValue d, ZymValue e, ZymValue f, ZymValue g, ZymValue h, ZymValue i, ZymValue j, ZymValue k, ZymValue l, ZymValue m, ZymValue n) {
    if (!zym_isString(format)) {
        zym_runtimeError(vm, "print() first argument must be a string");
        return ZYM_ERROR;
    }

    ZymValue args[] = { a, b, c, d, e, f, g, h, i, j, k, l, m, n };
    return print_impl(vm, zym_asCString(format), args, 14);
}

ZymValue nativePrint_16(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b, ZymValue c, ZymValue d, ZymValue e, ZymValue f, ZymValue g, ZymValue h, ZymValue i, ZymValue j, ZymValue k, ZymValue l, ZymValue m, ZymValue n, ZymValue o) {
    if (!zym_isString(format)) {
        zym_runtimeError(vm, "print() first argument must be a string");
        return ZYM_ERROR;
    }

    ZymValue args[] = { a, b, c, d, e, f, g, h, i, j, k, l, m, n, o };
    return print_impl(vm, zym_asCString(format), args, 15);
}

ZymValue nativePrint_17(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b, ZymValue c, ZymValue d, ZymValue e, ZymValue f, ZymValue g, ZymValue h, ZymValue i, ZymValue j, ZymValue k, ZymValue l, ZymValue m, ZymValue n, ZymValue o, ZymValue p) {
    if (!zym_isString(format)) {
        zym_runtimeError(vm, "print() first argument must be a string");
        return ZYM_ERROR;
    }

    ZymValue args[] = { a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p };
    return print_impl(vm, zym_asCString(format), args, 16);
}

ZymValue nativePrint_18(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b, ZymValue c, ZymValue d, ZymValue e, ZymValue f, ZymValue g, ZymValue h, ZymValue i, ZymValue j, ZymValue k, ZymValue l, ZymValue m, ZymValue n, ZymValue o, ZymValue p, ZymValue q) {
    if (!zym_isString(format)) {
        zym_runtimeError(vm, "print() first argument must be a string");
        return ZYM_ERROR;
    }

    ZymValue args[] = { a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q };
    return print_impl(vm, zym_asCString(format), args, 17);
}

ZymValue nativePrint_19(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b, ZymValue c, ZymValue d, ZymValue e, ZymValue f, ZymValue g, ZymValue h, ZymValue i, ZymValue j, ZymValue k, ZymValue l, ZymValue m, ZymValue n, ZymValue o, ZymValue p, ZymValue q, ZymValue r) {
    if (!zym_isString(format)) {
        zym_runtimeError(vm, "print() first argument must be a string");
        return ZYM_ERROR;
    }

    ZymValue args[] = { a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r };
    return print_impl(vm, zym_asCString(format), args, 18);
}

ZymValue nativePrint_20(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b, ZymValue c, ZymValue d, ZymValue e, ZymValue f, ZymValue g, ZymValue h, ZymValue i, ZymValue j, ZymValue k, ZymValue l, ZymValue m, ZymValue n, ZymValue o, ZymValue p, ZymValue q, ZymValue r, ZymValue s) {
    if (!zym_isString(format)) {
        zym_runtimeError(vm, "print() first argument must be a string");
        return ZYM_ERROR;
    }

    ZymValue args[] = { a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s };
    return print_impl(vm, zym_asCString(format), args, 19);
}

ZymValue nativePrint_21(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b, ZymValue c, ZymValue d, ZymValue e, ZymValue f, ZymValue g, ZymValue h, ZymValue i, ZymValue j, ZymValue k, ZymValue l, ZymValue m, ZymValue n, ZymValue o, ZymValue p, ZymValue q, ZymValue r, ZymValue s, ZymValue t) {
    if (!zym_isString(format)) {
        zym_runtimeError(vm, "print() first argument must be a string");
        return ZYM_ERROR;
    }

    ZymValue args[] = { a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t };
    return print_impl(vm, zym_asCString(format), args, 20);
}

ZymValue nativePrint_22(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b, ZymValue c, ZymValue d, ZymValue e, ZymValue f, ZymValue g, ZymValue h, ZymValue i, ZymValue j, ZymValue k, ZymValue l, ZymValue m, ZymValue n, ZymValue o, ZymValue p, ZymValue q, ZymValue r, ZymValue s, ZymValue t, ZymValue u) {
    if (!zym_isString(format)) {
        zym_runtimeError(vm, "print() first argument must be a string");
        return ZYM_ERROR;
    }

    ZymValue args[] = { a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u };
    return print_impl(vm, zym_asCString(format), args, 21);
}

ZymValue nativePrint_23(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b, ZymValue c, ZymValue d, ZymValue e, ZymValue f, ZymValue g, ZymValue h, ZymValue i, ZymValue j, ZymValue k, ZymValue l, ZymValue m, ZymValue n, ZymValue o, ZymValue p, ZymValue q, ZymValue r, ZymValue s, ZymValue t, ZymValue u, ZymValue v) {
    if (!zym_isString(format)) {
        zym_runtimeError(vm, "print() first argument must be a string");
        return ZYM_ERROR;
    }

    ZymValue args[] = { a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v };
    return print_impl(vm, zym_asCString(format), args, 22);
}

ZymValue nativePrint_24(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b, ZymValue c, ZymValue d, ZymValue e, ZymValue f, ZymValue g, ZymValue h, ZymValue i, ZymValue j, ZymValue k, ZymValue l, ZymValue m, ZymValue n, ZymValue o, ZymValue p, ZymValue q, ZymValue r, ZymValue s, ZymValue t, ZymValue u, ZymValue v, ZymValue w) {
    if (!zym_isString(format)) {
        zym_runtimeError(vm, "print() first argument must be a string");
        return ZYM_ERROR;
    }

    ZymValue args[] = { a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w };
    return print_impl(vm, zym_asCString(format), args, 23);
}

ZymValue nativePrint_25(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b, ZymValue c, ZymValue d, ZymValue e, ZymValue f, ZymValue g, ZymValue h, ZymValue i, ZymValue j, ZymValue k, ZymValue l, ZymValue m, ZymValue n, ZymValue o, ZymValue p, ZymValue q, ZymValue r, ZymValue s, ZymValue t, ZymValue u, ZymValue v, ZymValue w, ZymValue x) {
    if (!zym_isString(format)) {
        zym_runtimeError(vm, "print() first argument must be a string");
        return ZYM_ERROR;
    }

    ZymValue args[] = { a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x };
    return print_impl(vm, zym_asCString(format), args, 24);
}

ZymValue nativePrint_26(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b, ZymValue c, ZymValue d, ZymValue e, ZymValue f, ZymValue g, ZymValue h, ZymValue i, ZymValue j, ZymValue k, ZymValue l, ZymValue m, ZymValue n, ZymValue o, ZymValue p, ZymValue q, ZymValue r, ZymValue s, ZymValue t, ZymValue u, ZymValue v, ZymValue w, ZymValue x, ZymValue y) {
    if (!zym_isString(format)) {
        zym_runtimeError(vm, "print() first argument must be a string");
        return ZYM_ERROR;
    }

    ZymValue args[] = { a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y };
    return print_impl(vm, zym_asCString(format), args, 25);
}
