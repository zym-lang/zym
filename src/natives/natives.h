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

ZymValue nativeConsole_create(ZymVM* vm);

ZymValue nativeOS_create(ZymVM* vm);

ZymValue nativeTime_clock(ZymVM* vm);
ZymValue nativeTime_sleep(ZymVM* vm, ZymValue milliseconds);

ZymValue nativeRandom_create(ZymVM* vm, ZymValue seedVal);
ZymValue nativeRandom_create_auto(ZymVM* vm);
ZymValue nativeRandom_create_seeded(ZymVM* vm, ZymValue seedVal);

ZymValue nativeBuffer_create(ZymVM* vm, ZymValue sizeVal, ZymValue autoGrowVal);
ZymValue nativeBuffer_create_auto(ZymVM* vm, ZymValue lengthVal);

ZymValue nativeFile_open(ZymVM* vm, ZymValue pathVal, ZymValue modeVal);
ZymValue nativeFile_readFile(ZymVM* vm, ZymValue pathVal);
ZymValue nativeFile_writeFile(ZymVM* vm, ZymValue pathVal, ZymValue dataVal);
ZymValue nativeFile_appendFile(ZymVM* vm, ZymValue pathVal, ZymValue dataVal);
ZymValue nativeFile_exists(ZymVM* vm, ZymValue pathVal);
ZymValue nativeFile_delete(ZymVM* vm, ZymValue pathVal);
ZymValue nativeFile_copy(ZymVM* vm, ZymValue srcVal, ZymValue dstVal);
ZymValue nativeFile_rename(ZymVM* vm, ZymValue oldPathVal, ZymValue newPathVal);
ZymValue nativeFile_stat(ZymVM* vm, ZymValue pathVal);

ZymValue nativeFile_readToNewBuffer(ZymVM* vm, ZymValue pathVal);
ZymValue nativeFile_writeFromNewBuffer(ZymVM* vm, ZymValue pathVal, ZymValue bufferVal);

ZymValue nativeDir_create(ZymVM* vm, ZymValue pathVal);
ZymValue nativeDir_remove(ZymVM* vm, ZymValue pathVal);
ZymValue nativeDir_list(ZymVM* vm, ZymValue pathVal);
ZymValue nativeDir_exists(ZymVM* vm, ZymValue pathVal);

ZymValue nativePath_join(ZymVM* vm, ZymValue part1Val, ZymValue part2Val);
ZymValue nativePath_dirname(ZymVM* vm, ZymValue pathVal);
ZymValue nativePath_basename(ZymVM* vm, ZymValue pathVal);
ZymValue nativePath_extension(ZymVM* vm, ZymValue pathVal);
ZymValue nativePath_normalize(ZymVM* vm, ZymValue pathVal);
ZymValue nativePath_absolute(ZymVM* vm, ZymValue pathVal);
ZymValue nativePath_isAbsolute(ZymVM* vm, ZymValue pathVal);

ZymValue nativeProcess_spawn(ZymVM* vm, ZymValue commandVal, ZymValue argsVal, ZymValue optionsMap);
ZymValue nativeProcess_spawn_1(ZymVM* vm, ZymValue commandVal);
ZymValue nativeProcess_spawn_2(ZymVM* vm, ZymValue commandVal, ZymValue argsVal);
ZymValue nativeProcess_exec(ZymVM* vm, ZymValue commandVal, ZymValue argsVal, ZymValue optionsMap);
ZymValue nativeProcess_exec_1(ZymVM* vm, ZymValue commandVal);
ZymValue nativeProcess_exec_2(ZymVM* vm, ZymValue commandVal, ZymValue argsVal);

ZymValue nativeProcess_getCwd(ZymVM* vm);
ZymValue nativeProcess_setCwd(ZymVM* vm, ZymValue pathVal);
ZymValue nativeProcess_getEnv(ZymVM* vm, ZymValue keyVal);
ZymValue nativeProcess_setEnv(ZymVM* vm, ZymValue keyVal, ZymValue valueVal);
ZymValue nativeProcess_getEnvAll(ZymVM* vm);

ZymValue nativeProcess_getPid(ZymVM* vm);
ZymValue nativeProcess_getParentPid(ZymVM* vm);
ZymValue nativeProcess_exit(ZymVM* vm, ZymValue codeVal);
ZymValue nativeProcess_exit_0(ZymVM* vm);

ZymValue nativeZymVM_create(ZymVM* vm);