#include "./natives.h"

void setupNatives(ZymVM* vm)
{
    zym_defineNativeVariadic(vm, "print(...)", nativePrint);

    zym_defineNative(vm, "clock()", nativeTime_clock);
    zym_defineNative(vm, "sleep(milliseconds)", nativeTime_sleep);

    zym_defineNative(vm, "Random()", nativeRandom_create_auto);
    zym_defineNative(vm, "Random(seed)", nativeRandom_create_seeded);
    zym_defineNative(vm, "Buffer(size)", nativeBuffer_create_auto);
    zym_defineNative(vm, "Buffer(size, autoGrow)", nativeBuffer_create);
    ZymValue consoleInstance = nativeConsole_create(vm);
    zym_defineGlobal(vm, "Console", consoleInstance);
    zym_defineNative(vm, "OS()", nativeOS_create);
    zym_defineNative(vm, "ZymVM()", nativeZymVM_create);

    zym_defineNative(vm, "fileOpen(path, mode)", nativeFile_open);
    zym_defineNative(vm, "fileRead(path)", nativeFile_readFile);
    zym_defineNative(vm, "fileWrite(path, data)", nativeFile_writeFile);
    zym_defineNative(vm, "fileAppend(path, data)", nativeFile_appendFile);
    zym_defineNative(vm, "fileExists(path)", nativeFile_exists);
    zym_defineNative(vm, "fileDelete(path)", nativeFile_delete);
    zym_defineNative(vm, "fileCopy(src, dst)", nativeFile_copy);
    zym_defineNative(vm, "fileRename(oldPath, newPath)", nativeFile_rename);
    zym_defineNative(vm, "fileStat(path)", nativeFile_stat);
    zym_defineNative(vm, "fileReadBuffer(path)", nativeFile_readToNewBuffer);
    zym_defineNative(vm, "fileWriteBuffer(path, buffer)", nativeFile_writeFromNewBuffer);
    zym_defineNative(vm, "dirCreate(path)", nativeDir_create);
    zym_defineNative(vm, "dirRemove(path)", nativeDir_remove);
    zym_defineNative(vm, "dirList(path)", nativeDir_list);
    zym_defineNative(vm, "dirExists(path)", nativeDir_exists);

    zym_defineNative(vm, "pathJoin(part1, part2)", nativePath_join);
    zym_defineNative(vm, "pathDirname(path)", nativePath_dirname);
    zym_defineNative(vm, "pathBasename(path)", nativePath_basename);
    zym_defineNative(vm, "pathExtension(path)", nativePath_extension);
    zym_defineNative(vm, "pathNormalize(path)", nativePath_normalize);
    zym_defineNative(vm, "pathAbsolute(path)", nativePath_absolute);
    zym_defineNative(vm, "pathIsAbsolute(path)", nativePath_isAbsolute);

    zym_defineNative(vm, "ProcessSpawn(command)", nativeProcess_spawn_1);
    zym_defineNative(vm, "ProcessSpawn(command, args)", nativeProcess_spawn_2);
    zym_defineNative(vm, "ProcessSpawn(command, args, options)", nativeProcess_spawn);
    zym_defineNative(vm, "ProcessExec(command)", nativeProcess_exec_1);
    zym_defineNative(vm, "ProcessExec(command, args)", nativeProcess_exec_2);
    zym_defineNative(vm, "ProcessExec(command, args, options)", nativeProcess_exec);
    zym_defineNative(vm, "processCwd()", nativeProcess_getCwd);
    zym_defineNative(vm, "processSetCwd(path)", nativeProcess_setCwd);
    zym_defineNative(vm, "processEnv(key)", nativeProcess_getEnv);
    zym_defineNative(vm, "processSetEnv(key, value)", nativeProcess_setEnv);
    zym_defineNative(vm, "processEnvAll()", nativeProcess_getEnvAll);
    zym_defineNative(vm, "processPid()", nativeProcess_getPid);
    zym_defineNative(vm, "processParentPid()", nativeProcess_getParentPid);
    zym_defineNative(vm, "processExit()", nativeProcess_exit_0);
    zym_defineNative(vm, "processExit(code)", nativeProcess_exit);
}
