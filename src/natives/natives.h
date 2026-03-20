#pragma once

#include "zym/zym.h"

void setupNatives(ZymVM* vm);

ZymValue nativePrint_01(ZymVM* vm, ZymValue value);
ZymValue nativePrint_02(ZymVM* vm, ZymValue format, ZymValue a);
ZymValue nativePrint_03(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b);
ZymValue nativePrint_04(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b, ZymValue c);
ZymValue nativePrint_05(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b, ZymValue c, ZymValue d);
ZymValue nativePrint_06(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b, ZymValue c, ZymValue d, ZymValue e);
ZymValue nativePrint_07(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b, ZymValue c, ZymValue d, ZymValue e, ZymValue f);
ZymValue nativePrint_08(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b, ZymValue c, ZymValue d, ZymValue e, ZymValue f, ZymValue g);
ZymValue nativePrint_09(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b, ZymValue c, ZymValue d, ZymValue e, ZymValue f, ZymValue g, ZymValue h);
ZymValue nativePrint_10(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b, ZymValue c, ZymValue d, ZymValue e, ZymValue f, ZymValue g, ZymValue h, ZymValue i);
ZymValue nativePrint_11(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b, ZymValue c, ZymValue d, ZymValue e, ZymValue f, ZymValue g, ZymValue h, ZymValue i, ZymValue j);
ZymValue nativePrint_12(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b, ZymValue c, ZymValue d, ZymValue e, ZymValue f, ZymValue g, ZymValue h, ZymValue i, ZymValue j, ZymValue k);
ZymValue nativePrint_13(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b, ZymValue c, ZymValue d, ZymValue e, ZymValue f, ZymValue g, ZymValue h, ZymValue i, ZymValue j, ZymValue k, ZymValue l);
ZymValue nativePrint_14(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b, ZymValue c, ZymValue d, ZymValue e, ZymValue f, ZymValue g, ZymValue h, ZymValue i, ZymValue j, ZymValue k, ZymValue l, ZymValue m);
ZymValue nativePrint_15(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b, ZymValue c, ZymValue d, ZymValue e, ZymValue f, ZymValue g, ZymValue h, ZymValue i, ZymValue j, ZymValue k, ZymValue l, ZymValue m, ZymValue n);
ZymValue nativePrint_16(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b, ZymValue c, ZymValue d, ZymValue e, ZymValue f, ZymValue g, ZymValue h, ZymValue i, ZymValue j, ZymValue k, ZymValue l, ZymValue m, ZymValue n, ZymValue o);
ZymValue nativePrint_17(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b, ZymValue c, ZymValue d, ZymValue e, ZymValue f, ZymValue g, ZymValue h, ZymValue i, ZymValue j, ZymValue k, ZymValue l, ZymValue m, ZymValue n, ZymValue o, ZymValue p);
ZymValue nativePrint_18(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b, ZymValue c, ZymValue d, ZymValue e, ZymValue f, ZymValue g, ZymValue h, ZymValue i, ZymValue j, ZymValue k, ZymValue l, ZymValue m, ZymValue n, ZymValue o, ZymValue p, ZymValue q);
ZymValue nativePrint_19(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b, ZymValue c, ZymValue d, ZymValue e, ZymValue f, ZymValue g, ZymValue h, ZymValue i, ZymValue j, ZymValue k, ZymValue l, ZymValue m, ZymValue n, ZymValue o, ZymValue p, ZymValue q, ZymValue r);
ZymValue nativePrint_20(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b, ZymValue c, ZymValue d, ZymValue e, ZymValue f, ZymValue g, ZymValue h, ZymValue i, ZymValue j, ZymValue k, ZymValue l, ZymValue m, ZymValue n, ZymValue o, ZymValue p, ZymValue q, ZymValue r, ZymValue s);
ZymValue nativePrint_21(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b, ZymValue c, ZymValue d, ZymValue e, ZymValue f, ZymValue g, ZymValue h, ZymValue i, ZymValue j, ZymValue k, ZymValue l, ZymValue m, ZymValue n, ZymValue o, ZymValue p, ZymValue q, ZymValue r, ZymValue s, ZymValue t);
ZymValue nativePrint_22(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b, ZymValue c, ZymValue d, ZymValue e, ZymValue f, ZymValue g, ZymValue h, ZymValue i, ZymValue j, ZymValue k, ZymValue l, ZymValue m, ZymValue n, ZymValue o, ZymValue p, ZymValue q, ZymValue r, ZymValue s, ZymValue t, ZymValue u);
ZymValue nativePrint_23(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b, ZymValue c, ZymValue d, ZymValue e, ZymValue f, ZymValue g, ZymValue h, ZymValue i, ZymValue j, ZymValue k, ZymValue l, ZymValue m, ZymValue n, ZymValue o, ZymValue p, ZymValue q, ZymValue r, ZymValue s, ZymValue t, ZymValue u, ZymValue v);
ZymValue nativePrint_24(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b, ZymValue c, ZymValue d, ZymValue e, ZymValue f, ZymValue g, ZymValue h, ZymValue i, ZymValue j, ZymValue k, ZymValue l, ZymValue m, ZymValue n, ZymValue o, ZymValue p, ZymValue q, ZymValue r, ZymValue s, ZymValue t, ZymValue u, ZymValue v, ZymValue w);
ZymValue nativePrint_25(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b, ZymValue c, ZymValue d, ZymValue e, ZymValue f, ZymValue g, ZymValue h, ZymValue i, ZymValue j, ZymValue k, ZymValue l, ZymValue m, ZymValue n, ZymValue o, ZymValue p, ZymValue q, ZymValue r, ZymValue s, ZymValue t, ZymValue u, ZymValue v, ZymValue w, ZymValue x);
ZymValue nativePrint_26(ZymVM* vm, ZymValue format, ZymValue a, ZymValue b, ZymValue c, ZymValue d, ZymValue e, ZymValue f, ZymValue g, ZymValue h, ZymValue i, ZymValue j, ZymValue k, ZymValue l, ZymValue m, ZymValue n, ZymValue o, ZymValue p, ZymValue q, ZymValue r, ZymValue s, ZymValue t, ZymValue u, ZymValue v, ZymValue w, ZymValue x, ZymValue y);

ZymValue nativeTime_clock(ZymVM* vm);
ZymValue nativeTime_sleep(ZymVM* vm, ZymValue milliseconds);
