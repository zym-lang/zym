#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "./natives.h"

#ifdef _WIN32
    #include <winsock2.h>
    #include <windows.h>
    #include <iphlpapi.h>
    #include <lmcons.h>
    #include <userenv.h>
    #pragma comment(lib, "iphlpapi.lib")
    #pragma comment(lib, "userenv.lib")
#else
    #include <unistd.h>
    #include <sys/types.h>
    #include <sys/utsname.h>
    #ifdef __linux__
        #include <sys/sysinfo.h>
    #endif
    #include <pwd.h>
    #include <ifaddrs.h>
    #include <netdb.h>
    #include <arpa/inet.h>

    #ifdef __APPLE__
        #include <sys/sysctl.h>
        #include <mach/mach.h>
        #include <mach-o/dyld.h>
    #endif
#endif

typedef struct {
    int dummy;
} OSData;

void os_cleanup(ZymVM* vm, void* ptr) {
    (void)vm;  // Unused
    OSData* os = (OSData*)ptr;
    free(os);
}

ZymValue os_type(ZymVM* vm, ZymValue context) {
    (void)zym_getNativeData(context);

#ifdef _WIN32
    return zym_newString(vm, "windows");
#elif defined(__linux__)
    return zym_newString(vm, "linux");
#elif defined(__APPLE__)
    return zym_newString(vm, "darwin");
#elif defined(__FreeBSD__)
    return zym_newString(vm, "freebsd");
#elif defined(__OpenBSD__)
    return zym_newString(vm, "openbsd");
#elif defined(__NetBSD__)
    return zym_newString(vm, "netbsd");
#else
    return zym_newString(vm, "unknown");
#endif
}

ZymValue os_arch(ZymVM* vm, ZymValue context) {
    (void)zym_getNativeData(context);

#if defined(_WIN64) || defined(__x86_64__) || defined(__amd64__)
    return zym_newString(vm, "x64");
#elif defined(_WIN32) || defined(__i386__) || defined(__i686__)
    return zym_newString(vm, "x86");
#elif defined(__aarch64__) || defined(_M_ARM64)
    return zym_newString(vm, "arm64");
#elif defined(__arm__) || defined(_M_ARM)
    return zym_newString(vm, "arm");
#elif defined(__powerpc64__) || defined(__ppc64__)
    return zym_newString(vm, "ppc64");
#elif defined(__s390x__)
    return zym_newString(vm, "s390x");
#else
    return zym_newString(vm, "unknown");
#endif
}

ZymValue os_version(ZymVM* vm, ZymValue context) {
    (void)zym_getNativeData(context);

#ifdef _WIN32
    OSVERSIONINFOEX osvi;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

    #pragma warning(push)
    #pragma warning(disable: 4996)
    if (GetVersionEx((OSVERSIONINFO*)&osvi)) {
        char version[256];
        snprintf(version, sizeof(version), "%lu.%lu.%lu",
                osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber);
        return zym_newString(vm, version);
    }
    #pragma warning(pop)
    return zym_newString(vm, "unknown");
#else
    struct utsname buf;
    if (uname(&buf) == 0) {
        return zym_newString(vm, buf.release);
    }
    return zym_newString(vm, "unknown");
#endif
}

ZymValue os_release(ZymVM* vm, ZymValue context) {
    (void)zym_getNativeData(context);

#ifdef _WIN32
    // On Windows, release is same as version
    return os_version(vm, context);
#else
    struct utsname buf;
    if (uname(&buf) == 0) {
        return zym_newString(vm, buf.release);
    }
    return zym_newString(vm, "unknown");
#endif
}

ZymValue os_platform(ZymVM* vm, ZymValue context) {
    return os_type(vm, context);
}

ZymValue os_homeDir(ZymVM* vm, ZymValue context) {
    (void)zym_getNativeData(context);

#ifdef _WIN32
    char path[MAX_PATH];
    HANDLE token;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        DWORD size = MAX_PATH;
        if (GetUserProfileDirectoryA(token, path, &size)) {
            CloseHandle(token);
            return zym_newString(vm, path);
        }
        CloseHandle(token);
    }

    const char* home = getenv("USERPROFILE");
    if (home) {
        return zym_newString(vm, home);
    }
    return zym_newNull();
#else
    const char* home = getenv("HOME");
    if (home) {
        return zym_newString(vm, home);
    }

    struct passwd* pw = getpwuid(getuid());
    if (pw && pw->pw_dir) {
        return zym_newString(vm, pw->pw_dir);
    }

    return zym_newNull();
#endif
}

ZymValue os_tmpDir(ZymVM* vm, ZymValue context) {
    (void)zym_getNativeData(context);

#ifdef _WIN32
    char path[MAX_PATH];
    DWORD len = GetTempPathA(MAX_PATH, path);
    if (len > 0 && len < MAX_PATH) {
        if (path[len - 1] == '\\') {
            path[len - 1] = '\0';
        }
        return zym_newString(vm, path);
    }
    return zym_newString(vm, "C:\\Windows\\Temp");
#else
    const char* tmpdir = getenv("TMPDIR");
    if (tmpdir) return zym_newString(vm, tmpdir);

    tmpdir = getenv("TMP");
    if (tmpdir) return zym_newString(vm, tmpdir);

    tmpdir = getenv("TEMP");
    if (tmpdir) return zym_newString(vm, tmpdir);

    return zym_newString(vm, "/tmp");
#endif
}

ZymValue os_execPath(ZymVM* vm, ZymValue context) {
    (void)zym_getNativeData(context);

#ifdef _WIN32
    char path[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, path, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        return zym_newString(vm, path);
    }
    return zym_newNull();
#elif defined(__linux__)
    char path[4096];
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len != -1) {
        path[len] = '\0';
        return zym_newString(vm, path);
    }
    return zym_newNull();
#elif defined(__APPLE__)
    char path[4096];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0) {
        return zym_newString(vm, path);
    }
    return zym_newNull();
#elif defined(__FreeBSD__)
    char path[4096];
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
    size_t len = sizeof(path);
    if (sysctl(mib, 4, path, &len, NULL, 0) == 0) {
        return zym_newString(vm, path);
    }
    return zym_newNull();
#else
    return zym_newNull();
#endif
}

ZymValue os_hostname(ZymVM* vm, ZymValue context) {
    (void)zym_getNativeData(context);

#ifdef _WIN32
    char hostname[256];
    DWORD size = sizeof(hostname);
    if (GetComputerNameA(hostname, &size)) {
        return zym_newString(vm, hostname);
    }
    return zym_newString(vm, "unknown");
#else
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        hostname[sizeof(hostname) - 1] = '\0';
        return zym_newString(vm, hostname);
    }
    return zym_newString(vm, "unknown");
#endif
}

ZymValue os_cpuCount(ZymVM* vm, ZymValue context) {
    (void)zym_getNativeData(context);

#ifdef _WIN32
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return zym_newNumber((double)sysinfo.dwNumberOfProcessors);
#elif defined(_SC_NPROCESSORS_ONLN)
    long count = sysconf(_SC_NPROCESSORS_ONLN);
    if (count > 0) {
        return zym_newNumber((double)count);
    }
    return zym_newNumber(1);
#else
    return zym_newNumber(1);
#endif
}

ZymValue os_totalMem(ZymVM* vm, ZymValue context) {
    (void)zym_getNativeData(context);

#ifdef _WIN32
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    if (GlobalMemoryStatusEx(&statex)) {
        return zym_newNumber((double)statex.ullTotalPhys);
    }
    return zym_newNumber(0);
#elif defined(__linux__)
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        return zym_newNumber((double)info.totalram * info.mem_unit);
    }
    return zym_newNumber(0);
#elif defined(__APPLE__)
    int64_t mem;
    size_t len = sizeof(mem);
    if (sysctlbyname("hw.memsize", &mem, &len, NULL, 0) == 0) {
        return zym_newNumber((double)mem);
    }
    return zym_newNumber(0);
#else
    return zym_newNumber(0);
#endif
}

ZymValue os_freeMem(ZymVM* vm, ZymValue context) {
    (void)zym_getNativeData(context);

#ifdef _WIN32
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    if (GlobalMemoryStatusEx(&statex)) {
        return zym_newNumber((double)statex.ullAvailPhys);
    }
    return zym_newNumber(0);
#elif defined(__linux__)
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        return zym_newNumber((double)info.freeram * info.mem_unit);
    }
    return zym_newNumber(0);
#elif defined(__APPLE__)
    mach_msg_type_number_t count = HOST_VM_INFO_COUNT;
    vm_statistics_data_t vmstat;
    if (host_statistics(mach_host_self(), HOST_VM_INFO, (host_info_t)&vmstat, &count) == KERN_SUCCESS) {
        return zym_newNumber((double)vmstat.free_count * vm_page_size);
    }
    return zym_newNumber(0);
#else
    return zym_newNumber(0);
#endif
}

ZymValue os_memory(ZymVM* vm, ZymValue context) {
    (void)zym_getNativeData(context);

    ZymValue map = zym_newMap(vm);
    zym_pushRoot(vm, map);

    double total = zym_asNumber(os_totalMem(vm, context));
    double free = zym_asNumber(os_freeMem(vm, context));
    double used = total - free;

    ZymValue totalVal = zym_newNumber(total);
    ZymValue freeVal = zym_newNumber(free);
    ZymValue usedVal = zym_newNumber(used);

    zym_mapSet(vm, map, "total", totalVal);
    zym_mapSet(vm, map, "free", freeVal);
    zym_mapSet(vm, map, "used", usedVal);
    zym_mapSet(vm, map, "available", freeVal);

    zym_popRoot(vm);
    return map;
}

ZymValue os_uptime(ZymVM* vm, ZymValue context) {
    (void)zym_getNativeData(context);

#ifdef _WIN32
    return zym_newNumber((double)GetTickCount64() / 1000.0);
#elif defined(__linux__)
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        return zym_newNumber((double)info.uptime);
    }
    return zym_newNumber(0);
#elif defined(__APPLE__) || defined(__FreeBSD__)
    struct timeval boottime;
    size_t len = sizeof(boottime);
    int mib[2] = {CTL_KERN, KERN_BOOTTIME};
    if (sysctl(mib, 2, &boottime, &len, NULL, 0) == 0) {
        time_t now = time(NULL);
        return zym_newNumber((double)(now - boottime.tv_sec));
    }
    return zym_newNumber(0);
#else
    return zym_newNumber(0);
#endif
}

ZymValue os_loadavg(ZymVM* vm, ZymValue context) {
    (void)zym_getNativeData(context);

    ZymValue list = zym_newList(vm);
    zym_pushRoot(vm, list);

#ifdef _WIN32
    // Windows doesn't have load average
    zym_listAppend(vm, list, zym_newNumber(0));
    zym_listAppend(vm, list, zym_newNumber(0));
    zym_listAppend(vm, list, zym_newNumber(0));
#else
    double loads[3];
    if (getloadavg(loads, 3) != -1) {
        zym_listAppend(vm, list, zym_newNumber(loads[0]));
        zym_listAppend(vm, list, zym_newNumber(loads[1]));
        zym_listAppend(vm, list, zym_newNumber(loads[2]));
    } else {
        zym_listAppend(vm, list, zym_newNumber(0));
        zym_listAppend(vm, list, zym_newNumber(0));
        zym_listAppend(vm, list, zym_newNumber(0));
    }
#endif

    zym_popRoot(vm);
    return list;
}

ZymValue os_userInfo(ZymVM* vm, ZymValue context) {
    (void)zym_getNativeData(context);

    ZymValue map = zym_newMap(vm);
    zym_pushRoot(vm, map);

#ifdef _WIN32
    char username[UNLEN + 1];
    DWORD size = UNLEN + 1;
    if (GetUserNameA(username, &size)) {
        zym_mapSet(vm, map, "username", zym_newString(vm, username));
    } else {
        zym_mapSet(vm, map, "username", zym_newNull());
    }

    zym_mapSet(vm, map, "uid", zym_newNumber(-1));
    zym_mapSet(vm, map, "gid", zym_newNumber(-1));
    zym_mapSet(vm, map, "shell", zym_newNull());

    ZymValue homeDir = os_homeDir(vm, context);
    zym_mapSet(vm, map, "homedir", homeDir);
#else
    struct passwd* pw = getpwuid(getuid());
    if (pw) {
        if (pw->pw_name) {
            zym_mapSet(vm, map, "username", zym_newString(vm, pw->pw_name));
        } else {
            zym_mapSet(vm, map, "username", zym_newNull());
        }

        zym_mapSet(vm, map, "uid", zym_newNumber((double)pw->pw_uid));
        zym_mapSet(vm, map, "gid", zym_newNumber((double)pw->pw_gid));

        if (pw->pw_shell) {
            zym_mapSet(vm, map, "shell", zym_newString(vm, pw->pw_shell));
        } else {
            zym_mapSet(vm, map, "shell", zym_newNull());
        }

        if (pw->pw_dir) {
            zym_mapSet(vm, map, "homedir", zym_newString(vm, pw->pw_dir));
        } else {
            zym_mapSet(vm, map, "homedir", zym_newNull());
        }
    } else {
        zym_mapSet(vm, map, "username", zym_newNull());
        zym_mapSet(vm, map, "uid", zym_newNumber(-1));
        zym_mapSet(vm, map, "gid", zym_newNumber(-1));
        zym_mapSet(vm, map, "shell", zym_newNull());
        zym_mapSet(vm, map, "homedir", zym_newNull());
    }
#endif

    zym_popRoot(vm);
    return map;
}

ZymValue os_endianness(ZymVM* vm, ZymValue context) {
    (void)zym_getNativeData(context);

    union {
        uint32_t i;
        char c[4];
    } test = {0x01020304};

    if (test.c[0] == 1) {
        return zym_newString(vm, "BE");
    } else {
        return zym_newString(vm, "LE");
    }
}

ZymValue os_eol(ZymVM* vm, ZymValue context) {
    (void)zym_getNativeData(context);

#ifdef _WIN32
    return zym_newString(vm, "\r\n");
#else
    return zym_newString(vm, "\n");
#endif
}

ZymValue nativeOS_create(ZymVM* vm) {
    OSData* os = calloc(1, sizeof(OSData));
    if (!os) {
        zym_runtimeError(vm, "Out of memory");
        return ZYM_ERROR;
    }

    ZymValue context = zym_createNativeContext(vm, os, os_cleanup);
    zym_pushRoot(vm, context);

    #define CREATE_METHOD_0(name, func) \
        ZymValue name = zym_createNativeClosure(vm, #func "()", func, context); \
        zym_pushRoot(vm, name);

    CREATE_METHOD_0(type, os_type);
    CREATE_METHOD_0(arch, os_arch);
    CREATE_METHOD_0(version, os_version);
    CREATE_METHOD_0(release, os_release);
    CREATE_METHOD_0(platform, os_platform);
    CREATE_METHOD_0(homeDir, os_homeDir);
    CREATE_METHOD_0(tmpDir, os_tmpDir);
    CREATE_METHOD_0(execPath, os_execPath);
    CREATE_METHOD_0(hostname, os_hostname);
    CREATE_METHOD_0(cpuCount, os_cpuCount);
    CREATE_METHOD_0(totalMem, os_totalMem);
    CREATE_METHOD_0(freeMem, os_freeMem);
    CREATE_METHOD_0(memory, os_memory);
    CREATE_METHOD_0(uptime, os_uptime);
    CREATE_METHOD_0(loadavg, os_loadavg);
    CREATE_METHOD_0(userInfo, os_userInfo);
    CREATE_METHOD_0(endianness, os_endianness);
    CREATE_METHOD_0(eol, os_eol);

    #undef CREATE_METHOD_0

    ZymValue obj = zym_newMap(vm);
    zym_pushRoot(vm, obj);

    zym_mapSet(vm, obj, "type", type);
    zym_mapSet(vm, obj, "arch", arch);
    zym_mapSet(vm, obj, "version", version);
    zym_mapSet(vm, obj, "release", release);
    zym_mapSet(vm, obj, "platform", platform);
    zym_mapSet(vm, obj, "homeDir", homeDir);
    zym_mapSet(vm, obj, "tmpDir", tmpDir);
    zym_mapSet(vm, obj, "execPath", execPath);
    zym_mapSet(vm, obj, "hostname", hostname);
    zym_mapSet(vm, obj, "cpuCount", cpuCount);
    zym_mapSet(vm, obj, "totalMem", totalMem);
    zym_mapSet(vm, obj, "freeMem", freeMem);
    zym_mapSet(vm, obj, "memory", memory);
    zym_mapSet(vm, obj, "uptime", uptime);
    zym_mapSet(vm, obj, "loadavg", loadavg);
    zym_mapSet(vm, obj, "userInfo", userInfo);
    zym_mapSet(vm, obj, "endianness", endianness);

    ZymValue eolStr = os_eol(vm, context);
    zym_mapSet(vm, obj, "EOL", eolStr);

    // (context + 18 methods + obj = 20 total)
    for (int i = 0; i < 20; i++) {
        zym_popRoot(vm);
    }

    return obj;
}
