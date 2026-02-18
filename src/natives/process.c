#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include "./natives.h"

#ifdef _WIN32
    #include <windows.h>
    #include <process.h>
    #include <direct.h>

    // ConPTY support (Windows 10 1809+)
    // Define if missing from MinGW headers
    #ifndef PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE
        #define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE \
            ProcThreadAttributeValue(22, FALSE, TRUE, FALSE)

        typedef VOID* HPCON;

        HRESULT WINAPI CreatePseudoConsole(COORD size, HANDLE hInput, HANDLE hOutput, DWORD dwFlags, HPCON* phPC);
        void WINAPI ClosePseudoConsole(HPCON hPC);
        HRESULT WINAPI ResizePseudoConsole(HPCON hPC, COORD size);
    #endif

    #define getcwd _getcwd
    #define chdir _chdir
    #define getpid _getpid
#else
    #include <unistd.h>
    #include <sys/types.h>
    #include <sys/wait.h>
    #include <sys/select.h>
    #include <sys/resource.h>
    #include <signal.h>
    #include <fcntl.h>
    #include <poll.h>
    #include <termios.h>

    #ifdef __linux__
        #include <pty.h>
        #include <utmp.h>
    #elif defined(__APPLE__) || defined(__FreeBSD__)
        #include <util.h>
    #endif
#endif

typedef struct {
    uint8_t* data;
    size_t capacity;
    size_t length;
    size_t position;
    ZymValue position_ref;
    ZymValue length_ref;
    bool auto_grow;
    int endianness;
} BufferData;

typedef enum {
    STDIO_PIPE,
    STDIO_INHERIT,
    STDIO_NULL,
    STDIO_PTY
} StdioMode;

typedef struct {
#ifdef _WIN32
    HANDLE hProcess;
    HANDLE hThread;
    HANDLE hStdin;
    HANDLE hStdout;
    HANDLE hStderr;
    DWORD processId;
    DWORD threadId;
    HPCON hConPTY;
    bool use_conpty;
#else
    pid_t pid;
    int stdin_fd;
    int stdout_fd;
    int stderr_fd;
    int pty_master;
    bool use_pty;
#endif

    bool is_running;
    int exit_code;
    bool exit_code_valid;
    bool stdin_open;
    bool stdout_open;
    bool stderr_open;

    char* stdout_buffer;
    char* stderr_buffer;
    size_t stdout_buf_size;
    size_t stdout_buf_len;
    size_t stderr_buf_size;
    size_t stderr_buf_len;

    char* command;
    char** args;
    int argc;
    char* cwd;
} ProcessData;

void process_cleanup(ZymVM* vm, void* ptr) {
    ProcessData* proc = (ProcessData*)ptr;

#ifdef _WIN32
    if (proc->hConPTY != INVALID_HANDLE_VALUE) {
        ClosePseudoConsole(proc->hConPTY);
    }

    if (proc->hStdin != INVALID_HANDLE_VALUE && proc->stdin_open) {
        CloseHandle(proc->hStdin);
    }
    if (proc->hStdout != INVALID_HANDLE_VALUE && proc->stdout_open) {
        CloseHandle(proc->hStdout);
    }
    if (proc->hStderr != INVALID_HANDLE_VALUE && proc->stderr_open) {
        CloseHandle(proc->hStderr);
    }

    if (proc->is_running && proc->hProcess != INVALID_HANDLE_VALUE) {
        TerminateProcess(proc->hProcess, 1);
        WaitForSingleObject(proc->hProcess, INFINITE);
    }

    if (proc->hProcess != INVALID_HANDLE_VALUE) {
        CloseHandle(proc->hProcess);
    }
    if (proc->hThread != INVALID_HANDLE_VALUE) {
        CloseHandle(proc->hThread);
    }
#else
    if (proc->stdin_fd >= 0 && proc->stdin_open) {
        close(proc->stdin_fd);
    }
    if (proc->stdout_fd >= 0 && proc->stdout_open) {
        close(proc->stdout_fd);
    }
    if (proc->stderr_fd >= 0 && proc->stderr_open) {
        close(proc->stderr_fd);
    }
    if (proc->pty_master >= 0) {
        close(proc->pty_master);
    }

    if (proc->is_running && proc->pid > 0) {
        kill(proc->pid, SIGTERM);
        usleep(100000);
        waitpid(proc->pid, NULL, WNOHANG);
        if (proc->is_running) {
            kill(proc->pid, SIGKILL);
            waitpid(proc->pid, NULL, 0);
        }
    }
#endif

    free(proc->stdout_buffer);
    free(proc->stderr_buffer);
    free(proc->command);
    free(proc->cwd);

    if (proc->args) {
        for (int i = 0; i < proc->argc; i++) {
            free(proc->args[i]);
        }
        free(proc->args);
    }

    free(proc);
}

static void set_nonblocking(int fd) {
#ifndef _WIN32
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#endif
}

static bool ensure_buffer_capacity(char** buffer, size_t* capacity, size_t needed) {
    if (needed <= *capacity) {
        return true;
    }

    size_t new_capacity = *capacity * 2;
    if (new_capacity < needed) {
        new_capacity = needed;
    }

    char* new_buffer = realloc(*buffer, new_capacity);
    if (!new_buffer) {
        return false;
    }

    *buffer = new_buffer;
    *capacity = new_capacity;
    return true;
}

#ifdef _WIN32

static bool spawn_process_windows(ZymVM* vm, ProcessData* proc, ZymValue argsVal, ZymValue optionsMap) {
    size_t cmd_len = strlen(proc->command);

    if (!zym_isNull(argsVal) && zym_isList(argsVal)) {
        size_t argc = zym_listLength(argsVal);
        for (size_t i = 0; i < argc; i++) {
            ZymValue arg = zym_listGet(vm, argsVal, i);
            if (zym_isString(arg)) {
                const char* argstr = zym_asCString(arg);
                cmd_len += strlen(argstr) + 3;  // space + potential quotes
            }
        }
    }

    cmd_len += 1;

    if (cmd_len > 32767) {
        return false;
    }

    char* cmdline = malloc(cmd_len);
    if (!cmdline) {
        return false;
    }

    strcpy(cmdline, proc->command);

    if (!zym_isNull(argsVal) && zym_isList(argsVal)) {
        size_t argc = zym_listLength(argsVal);
        for (size_t i = 0; i < argc; i++) {
            ZymValue arg = zym_listGet(vm, argsVal, i);
            if (zym_isString(arg)) {
                const char* argstr = zym_asCString(arg);
                strcat(cmdline, " ");

                bool needs_quotes = (strchr(argstr, ' ') != NULL ||
                                    strchr(argstr, '\t') != NULL ||
                                    strchr(argstr, '"') != NULL ||
                                    strlen(argstr) == 0);

                if (needs_quotes) {
                    strcat(cmdline, "\"");
                }
                strcat(cmdline, argstr);
                if (needs_quotes) {
                    strcat(cmdline, "\"");
                }
            }
        }
    }

    StdioMode stdin_mode = STDIO_PIPE;
    StdioMode stdout_mode = STDIO_PIPE;
    StdioMode stderr_mode = STDIO_PIPE;
    bool use_conpty = false;

    if (!zym_isNull(optionsMap) && zym_isMap(optionsMap)) {
        ZymValue stdinOpt = zym_mapGet(vm, optionsMap, "stdin");
        if (zym_isString(stdinOpt)) {
            const char* mode = zym_asCString(stdinOpt);
            if (strcmp(mode, "inherit") == 0) stdin_mode = STDIO_INHERIT;
            else if (strcmp(mode, "null") == 0) stdin_mode = STDIO_NULL;
            else if (strcmp(mode, "pty") == 0) { stdin_mode = STDIO_PTY; use_conpty = true; }
        }

        ZymValue stdoutOpt = zym_mapGet(vm, optionsMap, "stdout");
        if (zym_isString(stdoutOpt)) {
            const char* mode = zym_asCString(stdoutOpt);
            if (strcmp(mode, "inherit") == 0) stdout_mode = STDIO_INHERIT;
            else if (strcmp(mode, "null") == 0) stdout_mode = STDIO_NULL;
            else if (strcmp(mode, "pty") == 0) { stdout_mode = STDIO_PTY; use_conpty = true; }
        }

        ZymValue stderrOpt = zym_mapGet(vm, optionsMap, "stderr");
        if (zym_isString(stderrOpt)) {
            const char* mode = zym_asCString(stderrOpt);
            if (strcmp(mode, "inherit") == 0) stderr_mode = STDIO_INHERIT;
            else if (strcmp(mode, "null") == 0) stderr_mode = STDIO_NULL;
            else if (strcmp(mode, "pty") == 0) { stderr_mode = STDIO_PTY; use_conpty = true; }
        }
    }

    // ConPTY Mode (Windows 10+)
    if (use_conpty) {
        HANDLE hPipeIn_Read = INVALID_HANDLE_VALUE, hPipeIn_Write = INVALID_HANDLE_VALUE;
        HANDLE hPipeOut_Read = INVALID_HANDLE_VALUE, hPipeOut_Write = INVALID_HANDLE_VALUE;

        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = NULL;

        if (!CreatePipe(&hPipeIn_Read, &hPipeIn_Write, &sa, 0) ||
            !CreatePipe(&hPipeOut_Read, &hPipeOut_Write, &sa, 0)) {
            free(cmdline);
            return false;
        }

        // Make parent-side handles non-inheritable
        SetHandleInformation(hPipeIn_Write, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(hPipeOut_Read, HANDLE_FLAG_INHERIT, 0);

        // Create ConPTY (80x25 default console size)
        COORD consoleSize = {80, 25};
        HRESULT hr = CreatePseudoConsole(consoleSize, hPipeIn_Read, hPipeOut_Write, 0, &proc->hConPTY);

        if (FAILED(hr)) {
            CloseHandle(hPipeIn_Read);
            CloseHandle(hPipeIn_Write);
            CloseHandle(hPipeOut_Read);
            CloseHandle(hPipeOut_Write);
            free(cmdline);
            return false;
        }

        // Close child-side handles (ConPTY took ownership)
        CloseHandle(hPipeIn_Read);
        CloseHandle(hPipeOut_Write);

        // Store parent-side handles
        proc->hStdin = hPipeIn_Write;
        proc->hStdout = hPipeOut_Read;
        proc->hStderr = INVALID_HANDLE_VALUE;  // ConPTY merges stderr into stdout
        proc->stdin_open = true;
        proc->stdout_open = true;
        proc->stderr_open = false;
        proc->use_conpty = true;

        // Setup extended startup info for ConPTY
        SIZE_T attrListSize = 0;
        InitializeProcThreadAttributeList(NULL, 1, 0, &attrListSize);

        LPPROC_THREAD_ATTRIBUTE_LIST attrList = (LPPROC_THREAD_ATTRIBUTE_LIST)malloc(attrListSize);
        if (!attrList || !InitializeProcThreadAttributeList(attrList, 1, 0, &attrListSize)) {
            if (attrList) free(attrList);
            free(cmdline);
            ClosePseudoConsole(proc->hConPTY);
            CloseHandle(hPipeIn_Write);
            CloseHandle(hPipeOut_Read);
            return false;
        }

        // Associate ConPTY with the process
        if (!UpdateProcThreadAttribute(attrList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                                      proc->hConPTY, sizeof(HPCON), NULL, NULL)) {
            DeleteProcThreadAttributeList(attrList);
            free(attrList);
            free(cmdline);
            ClosePseudoConsole(proc->hConPTY);
            CloseHandle(hPipeIn_Write);
            CloseHandle(hPipeOut_Read);
            return false;
        }

        STARTUPINFOEXA siEx;
        ZeroMemory(&siEx, sizeof(siEx));
        siEx.StartupInfo.cb = sizeof(STARTUPINFOEXA);
        siEx.lpAttributeList = attrList;

        PROCESS_INFORMATION pi;
        ZeroMemory(&pi, sizeof(pi));

        BOOL success = CreateProcessA(
            NULL,                          // Application name
            cmdline,                       // Command line
            NULL,                          // Process security attributes
            NULL,                          // Thread security attributes
            FALSE,                         // Inherit handles (FALSE for ConPTY)
            EXTENDED_STARTUPINFO_PRESENT,  // Creation flags
            NULL,                          // Environment
            proc->cwd,                     // Working directory
            &siEx.StartupInfo,             // Startup info
            &pi                            // Process info
        );

        DeleteProcThreadAttributeList(attrList);
        free(attrList);
        free(cmdline);

        if (!success) {
            ClosePseudoConsole(proc->hConPTY);
            CloseHandle(hPipeIn_Write);
            CloseHandle(hPipeOut_Read);
            return false;
        }

        proc->hProcess = pi.hProcess;
        proc->hThread = pi.hThread;
        proc->processId = pi.dwProcessId;
        proc->threadId = pi.dwThreadId;
        proc->is_running = true;

        return true;
    }

    // Regular pipe mode (non-ConPTY)
    HANDLE hStdinRead = INVALID_HANDLE_VALUE, hStdinWrite = INVALID_HANDLE_VALUE;
    HANDLE hStdoutRead = INVALID_HANDLE_VALUE, hStdoutWrite = INVALID_HANDLE_VALUE;
    HANDLE hStderrRead = INVALID_HANDLE_VALUE, hStderrWrite = INVALID_HANDLE_VALUE;

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    // Create pipes based on mode
    if (stdin_mode == STDIO_PIPE) {
        CreatePipe(&hStdinRead, &hStdinWrite, &sa, 0);
        SetHandleInformation(hStdinWrite, HANDLE_FLAG_INHERIT, 0);
        proc->hStdin = hStdinWrite;
        proc->stdin_open = true;
    } else if (stdin_mode == STDIO_NULL) {
        hStdinRead = CreateFileA("NUL", GENERIC_READ, 0, &sa, OPEN_EXISTING, 0, NULL);
    }

    if (stdout_mode == STDIO_PIPE) {
        CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0);
        SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);
        proc->hStdout = hStdoutRead;
        proc->stdout_open = true;
    } else if (stdout_mode == STDIO_NULL) {
        hStdoutWrite = CreateFileA("NUL", GENERIC_WRITE, 0, &sa, OPEN_EXISTING, 0, NULL);
    }

    if (stderr_mode == STDIO_PIPE) {
        CreatePipe(&hStderrRead, &hStderrWrite, &sa, 0);
        SetHandleInformation(hStderrRead, HANDLE_FLAG_INHERIT, 0);
        proc->hStderr = hStderrRead;
        proc->stderr_open = true;
    } else if (stderr_mode == STDIO_NULL) {
        hStderrWrite = CreateFileA("NUL", GENERIC_WRITE, 0, &sa, OPEN_EXISTING, 0, NULL);
    }

    // Setup STARTUPINFO
    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;

    if (stdin_mode == STDIO_INHERIT) {
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    } else {
        si.hStdInput = hStdinRead;
    }

    if (stdout_mode == STDIO_INHERIT) {
        si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    } else {
        si.hStdOutput = hStdoutWrite;
    }

    if (stderr_mode == STDIO_INHERIT) {
        si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    } else {
        si.hStdError = hStderrWrite;
    }

    // Create process
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    BOOL success = CreateProcessA(
        NULL,           // Application name
        cmdline,        // Command line
        NULL,           // Process security attributes
        NULL,           // Thread security attributes
        TRUE,           // Inherit handles
        0,              // Creation flags
        NULL,           // Environment
        proc->cwd,      // Working directory
        &si,            // Startup info
        &pi             // Process info
    );

    free(cmdline);

    // Close child-side handles
    if (hStdinRead != INVALID_HANDLE_VALUE && hStdinRead != GetStdHandle(STD_INPUT_HANDLE)) {
        CloseHandle(hStdinRead);
    }
    if (hStdoutWrite != INVALID_HANDLE_VALUE && hStdoutWrite != GetStdHandle(STD_OUTPUT_HANDLE)) {
        CloseHandle(hStdoutWrite);
    }
    if (hStderrWrite != INVALID_HANDLE_VALUE && hStderrWrite != GetStdHandle(STD_ERROR_HANDLE)) {
        CloseHandle(hStderrWrite);
    }

    if (!success) {
        if (proc->hStdin != INVALID_HANDLE_VALUE) CloseHandle(proc->hStdin);
        if (proc->hStdout != INVALID_HANDLE_VALUE) CloseHandle(proc->hStdout);
        if (proc->hStderr != INVALID_HANDLE_VALUE) CloseHandle(proc->hStderr);
        return false;
    }

    proc->hProcess = pi.hProcess;
    proc->hThread = pi.hThread;
    proc->processId = pi.dwProcessId;
    proc->threadId = pi.dwThreadId;
    proc->is_running = true;

    return true;
}

#else  // Unix implementation

static bool spawn_process_unix(ZymVM* vm, ProcessData* proc, ZymValue argsVal, ZymValue optionsMap) {
    StdioMode stdin_mode = STDIO_PIPE;
    StdioMode stdout_mode = STDIO_PIPE;
    StdioMode stderr_mode = STDIO_PIPE;
    bool use_pty = false;

    if (!zym_isNull(optionsMap) && zym_isMap(optionsMap)) {
        ZymValue stdinOpt = zym_mapGet(vm, optionsMap, "stdin");
        if (zym_isString(stdinOpt)) {
            const char* mode = zym_asCString(stdinOpt);
            if (strcmp(mode, "inherit") == 0) stdin_mode = STDIO_INHERIT;
            else if (strcmp(mode, "null") == 0) stdin_mode = STDIO_NULL;
            else if (strcmp(mode, "pty") == 0) { stdin_mode = STDIO_PTY; use_pty = true; }
        }

        ZymValue stdoutOpt = zym_mapGet(vm, optionsMap, "stdout");
        if (zym_isString(stdoutOpt)) {
            const char* mode = zym_asCString(stdoutOpt);
            if (strcmp(mode, "inherit") == 0) stdout_mode = STDIO_INHERIT;
            else if (strcmp(mode, "null") == 0) stdout_mode = STDIO_NULL;
            else if (strcmp(mode, "pty") == 0) { stdout_mode = STDIO_PTY; use_pty = true; }
        }

        ZymValue stderrOpt = zym_mapGet(vm, optionsMap, "stderr");
        if (zym_isString(stderrOpt)) {
            const char* mode = zym_asCString(stderrOpt);
            if (strcmp(mode, "inherit") == 0) stderr_mode = STDIO_INHERIT;
            else if (strcmp(mode, "null") == 0) stderr_mode = STDIO_NULL;
            else if (strcmp(mode, "pty") == 0) { stderr_mode = STDIO_PTY; use_pty = true; }
        }
    }

    int stdin_pipe[2] = {-1, -1};
    int stdout_pipe[2] = {-1, -1};
    int stderr_pipe[2] = {-1, -1};
    int pty_master = -1, pty_slave = -1;

    if (use_pty) {
#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__)
        if (openpty(&pty_master, &pty_slave, NULL, NULL, NULL) < 0) {
            return false;
        }
        proc->pty_master = pty_master;
        proc->use_pty = true;
        set_nonblocking(pty_master);
#else
        return false;  // PTY not supported on this platform
#endif
    } else {
        // Create regular pipes
        if (stdin_mode == STDIO_PIPE && pipe(stdin_pipe) < 0) return false;
        if (stdout_mode == STDIO_PIPE && pipe(stdout_pipe) < 0) {
            if (stdin_pipe[0] >= 0) { close(stdin_pipe[0]); close(stdin_pipe[1]); }
            return false;
        }
        if (stderr_mode == STDIO_PIPE && pipe(stderr_pipe) < 0) {
            if (stdin_pipe[0] >= 0) { close(stdin_pipe[0]); close(stdin_pipe[1]); }
            if (stdout_pipe[0] >= 0) { close(stdout_pipe[0]); close(stdout_pipe[1]); }
            return false;
        }
    }

    int argc = 1;  // Command itself
    if (!zym_isNull(argsVal) && zym_isList(argsVal)) {
        argc += zym_listLength(argsVal);
    }

    char** argv = malloc((argc + 1) * sizeof(char*));
    if (!argv) {
        if (stdin_pipe[0] >= 0) { close(stdin_pipe[0]); close(stdin_pipe[1]); }
        if (stdout_pipe[0] >= 0) { close(stdout_pipe[0]); close(stdout_pipe[1]); }
        if (stderr_pipe[0] >= 0) { close(stderr_pipe[0]); close(stderr_pipe[1]); }
        if (pty_master >= 0) { close(pty_master); close(pty_slave); }
        return false;
    }

    argv[0] = strdup(proc->command);
    int arg_idx = 1;

    if (!zym_isNull(argsVal) && zym_isList(argsVal)) {
        size_t list_len = zym_listLength(argsVal);
        for (size_t i = 0; i < list_len; i++) {
            ZymValue arg = zym_listGet(vm, argsVal, i);
            if (zym_isString(arg)) {
                argv[arg_idx++] = strdup(zym_asCString(arg));
            }
        }
    }
    argv[arg_idx] = NULL;

    pid_t pid = fork();

    if (pid < 0) {
        // Fork failed
        for (int i = 0; argv[i] != NULL; i++) free(argv[i]);
        free(argv);
        if (stdin_pipe[0] >= 0) { close(stdin_pipe[0]); close(stdin_pipe[1]); }
        if (stdout_pipe[0] >= 0) { close(stdout_pipe[0]); close(stdout_pipe[1]); }
        if (stderr_pipe[0] >= 0) { close(stderr_pipe[0]); close(stderr_pipe[1]); }
        if (pty_master >= 0) { close(pty_master); close(pty_slave); }
        return false;
    }

    if (pid == 0) {
        // Child process

        if (use_pty) {
            // PTY mode
            close(pty_master);

            // Create new session
            setsid();

            // Set controlling terminal
            if (ioctl(pty_slave, TIOCSCTTY, 0) < 0) {
                _exit(1);
            }

            // Redirect stdio
            dup2(pty_slave, STDIN_FILENO);
            dup2(pty_slave, STDOUT_FILENO);
            dup2(pty_slave, STDERR_FILENO);

            if (pty_slave > 2) {
                close(pty_slave);
            }
        } else {
            // Regular pipe mode

            // Setup stdin
            if (stdin_mode == STDIO_PIPE) {
                close(stdin_pipe[1]);
                dup2(stdin_pipe[0], STDIN_FILENO);
                close(stdin_pipe[0]);
            } else if (stdin_mode == STDIO_NULL) {
                int null_fd = open("/dev/null", O_RDONLY);
                dup2(null_fd, STDIN_FILENO);
                close(null_fd);
            }

            // Setup stdout
            if (stdout_mode == STDIO_PIPE) {
                close(stdout_pipe[0]);
                dup2(stdout_pipe[1], STDOUT_FILENO);
                close(stdout_pipe[1]);
            } else if (stdout_mode == STDIO_NULL) {
                int null_fd = open("/dev/null", O_WRONLY);
                dup2(null_fd, STDOUT_FILENO);
                close(null_fd);
            }

            // Setup stderr
            if (stderr_mode == STDIO_PIPE) {
                close(stderr_pipe[0]);
                dup2(stderr_pipe[1], STDERR_FILENO);
                close(stderr_pipe[1]);
            } else if (stderr_mode == STDIO_NULL) {
                int null_fd = open("/dev/null", O_WRONLY);
                dup2(null_fd, STDERR_FILENO);
                close(null_fd);
            }
        }

        // Change working directory if specified
        if (proc->cwd) {
            chdir(proc->cwd);
        }

        // Execute
        execvp(argv[0], argv);

        // If we get here, exec failed
        _exit(127);
    }

    // Parent process
    for (int i = 0; argv[i] != NULL; i++) free(argv[i]);
    free(argv);

    if (use_pty) {
        close(pty_slave);
        proc->stdin_fd = pty_master;
        proc->stdout_fd = pty_master;
        proc->stderr_fd = -1;
        proc->stdin_open = true;
        proc->stdout_open = true;
        proc->stderr_open = false;
    } else {
        // Close child-side pipes
        if (stdin_pipe[0] >= 0) close(stdin_pipe[0]);
        if (stdout_pipe[1] >= 0) close(stdout_pipe[1]);
        if (stderr_pipe[1] >= 0) close(stderr_pipe[1]);

        // Store parent-side pipes
        if (stdin_mode == STDIO_PIPE) {
            proc->stdin_fd = stdin_pipe[1];
            proc->stdin_open = true;
            set_nonblocking(proc->stdin_fd);
        }
        if (stdout_mode == STDIO_PIPE) {
            proc->stdout_fd = stdout_pipe[0];
            proc->stdout_open = true;
            set_nonblocking(proc->stdout_fd);
        }
        if (stderr_mode == STDIO_PIPE) {
            proc->stderr_fd = stderr_pipe[0];
            proc->stderr_open = true;
            set_nonblocking(proc->stderr_fd);
        }
    }

    proc->pid = pid;
    proc->is_running = true;

    return true;
}

#endif

ZymValue process_write(ZymVM* vm, ZymValue context, ZymValue dataVal) {
    ProcessData* proc = (ProcessData*)zym_getNativeData(context);

    if (!proc->stdin_open) {
        zym_runtimeError(vm, "Process stdin is not open");
        return ZYM_ERROR;
    }

    if (!zym_isString(dataVal)) {
        zym_runtimeError(vm, "write() requires a string argument");
        return ZYM_ERROR;
    }

    const char* data = zym_asCString(dataVal);
    size_t len = strlen(data);

#ifdef _WIN32
    DWORD written;
    if (!WriteFile(proc->hStdin, data, len, &written, NULL)) {
        zym_runtimeError(vm, "Failed to write to process stdin");
        return ZYM_ERROR;
    }
#else
    ssize_t written = write(proc->stdin_fd, data, len);
    if (written < 0) {
        zym_runtimeError(vm, "Failed to write to process stdin: %s", strerror(errno));
        return ZYM_ERROR;
    }
#endif

    return context;
}

ZymValue process_writeBuffer(ZymVM* vm, ZymValue context, ZymValue bufferVal) {
    ProcessData* proc = (ProcessData*)zym_getNativeData(context);

    if (!proc->stdin_open) {
        zym_runtimeError(vm, "Process stdin is not open");
        return ZYM_ERROR;
    }

    if (!zym_isMap(bufferVal)) {
        zym_runtimeError(vm, "writeBuffer() requires a Buffer argument");
        return ZYM_ERROR;
    }

    ZymValue getLength = zym_mapGet(vm, bufferVal, "getLength");
    if (zym_isNull(getLength)) {
        zym_runtimeError(vm, "Invalid Buffer object");
        return ZYM_ERROR;
    }

    ZymValue bufferContext = zym_getClosureContext(getLength);
    BufferData* buf = (BufferData*)zym_getNativeData(bufferContext);
    if (!buf) {
        zym_runtimeError(vm, "Failed to get buffer data");
        return ZYM_ERROR;
    }

    size_t bytes_to_write = buf->length - buf->position;
    if (bytes_to_write == 0) {
        return zym_newNumber(0);
    }

#ifdef _WIN32
    DWORD written;
    if (!WriteFile(proc->hStdin, buf->data + buf->position, bytes_to_write, &written, NULL)) {
        zym_runtimeError(vm, "Failed to write buffer to process stdin");
        return ZYM_ERROR;
    }
#else
    ssize_t written = write(proc->stdin_fd, buf->data + buf->position, bytes_to_write);
    if (written < 0) {
        zym_runtimeError(vm, "Failed to write buffer to process stdin: %s", strerror(errno));
        return ZYM_ERROR;
    }
#endif

    buf->position += written;
    return zym_newNumber((double)written);
}

ZymValue process_closeStdin(ZymVM* vm, ZymValue context) {
    ProcessData* proc = (ProcessData*)zym_getNativeData(context);

    if (!proc->stdin_open) {
        return context;
    }

#ifdef _WIN32
    CloseHandle(proc->hStdin);
    proc->hStdin = INVALID_HANDLE_VALUE;
#else
    if (!proc->use_pty) {
        close(proc->stdin_fd);
        proc->stdin_fd = -1;
    }
#endif

    proc->stdin_open = false;
    return context;
}

ZymValue process_read(ZymVM* vm, ZymValue context) {
    ProcessData* proc = (ProcessData*)zym_getNativeData(context);

    if (!proc->stdout_open) {
        return zym_newString(vm, "");
    }

    char buffer[4096];

#ifdef _WIN32
    // Check if data is available before reading (non-blocking)
    DWORD bytesAvail = 0;
    if (!PeekNamedPipe(proc->hStdout, NULL, 0, NULL, &bytesAvail, NULL)) {
        return zym_newString(vm, "");
    }

    if (bytesAvail == 0) {
        return zym_newString(vm, "");
    }

    DWORD bytesRead;
    DWORD toRead = (bytesAvail < sizeof(buffer) - 1) ? bytesAvail : sizeof(buffer) - 1;
    if (!ReadFile(proc->hStdout, buffer, toRead, &bytesRead, NULL)) {
        return zym_newString(vm, "");
    }
    buffer[bytesRead] = '\0';
#else
    ssize_t bytesRead = read(proc->stdout_fd, buffer, sizeof(buffer) - 1);
    if (bytesRead < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return zym_newString(vm, "");
        }
        zym_runtimeError(vm, "Failed to read from process stdout: %s", strerror(errno));
        return ZYM_ERROR;
    }
    buffer[bytesRead] = '\0';
#endif

    return zym_newString(vm, buffer);
}

ZymValue process_readErr(ZymVM* vm, ZymValue context) {
    ProcessData* proc = (ProcessData*)zym_getNativeData(context);

    if (!proc->stderr_open) {
        return zym_newString(vm, "");
    }

    char buffer[4096];

#ifdef _WIN32
    // Check if data is available before reading (non-blocking)
    DWORD bytesAvail = 0;
    if (!PeekNamedPipe(proc->hStderr, NULL, 0, NULL, &bytesAvail, NULL)) {
        return zym_newString(vm, "");
    }

    if (bytesAvail == 0) {
        return zym_newString(vm, "");
    }

    DWORD bytesRead;
    DWORD toRead = (bytesAvail < sizeof(buffer) - 1) ? bytesAvail : sizeof(buffer) - 1;
    if (!ReadFile(proc->hStderr, buffer, toRead, &bytesRead, NULL)) {
        return zym_newString(vm, "");
    }
    buffer[bytesRead] = '\0';
#else
    ssize_t bytesRead = read(proc->stderr_fd, buffer, sizeof(buffer) - 1);
    if (bytesRead < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return zym_newString(vm, "");
        }
        zym_runtimeError(vm, "Failed to read from process stderr: %s", strerror(errno));
        return ZYM_ERROR;
    }
    buffer[bytesRead] = '\0';
#endif

    return zym_newString(vm, buffer);
}

ZymValue process_readNonBlock(ZymVM* vm, ZymValue context) {
    ProcessData* proc = (ProcessData*)zym_getNativeData(context);

    if (!proc->stdout_open) {
        return zym_newString(vm, "");
    }

#ifdef _WIN32
    DWORD bytesAvail = 0;
    if (!PeekNamedPipe(proc->hStdout, NULL, 0, NULL, &bytesAvail, NULL)) {
        return zym_newString(vm, "");
    }

    if (bytesAvail == 0) {
        return zym_newString(vm, "");
    }

    char* buffer = malloc(bytesAvail + 1);
    DWORD bytesRead;
    if (!ReadFile(proc->hStdout, buffer, bytesAvail, &bytesRead, NULL)) {
        free(buffer);
        return zym_newString(vm, "");
    }
    buffer[bytesRead] = '\0';
    ZymValue result = zym_newString(vm, buffer);
    free(buffer);
    return result;
#else
    // Use select to check if data is available
    fd_set readfds;
    struct timeval tv = {0, 0};
    FD_ZERO(&readfds);
    FD_SET(proc->stdout_fd, &readfds);

    int ret = select(proc->stdout_fd + 1, &readfds, NULL, NULL, &tv);
    if (ret <= 0) {
        return zym_newString(vm, "");
    }

    char buffer[4096];
    ssize_t bytesRead = read(proc->stdout_fd, buffer, sizeof(buffer) - 1);
    if (bytesRead <= 0) {
        return zym_newString(vm, "");
    }
    buffer[bytesRead] = '\0';
    return zym_newString(vm, buffer);
#endif
}

ZymValue process_readToBuffer(ZymVM* vm, ZymValue context, ZymValue bufferVal) {
    ProcessData* proc = (ProcessData*)zym_getNativeData(context);

    if (!proc->stdout_open) {
        zym_runtimeError(vm, "Process stdout is not open");
        return ZYM_ERROR;
    }

    if (!zym_isMap(bufferVal)) {
        zym_runtimeError(vm, "readToBuffer() requires a Buffer argument");
        return ZYM_ERROR;
    }

    ZymValue getLength = zym_mapGet(vm, bufferVal, "getLength");
    if (zym_isNull(getLength)) {
        zym_runtimeError(vm, "Invalid Buffer object");
        return ZYM_ERROR;
    }

    ZymValue bufferContext = zym_getClosureContext(getLength);
    BufferData* buf = (BufferData*)zym_getNativeData(bufferContext);
    if (!buf) {
        zym_runtimeError(vm, "Failed to get buffer data");
        return ZYM_ERROR;
    }

    size_t available_space = buf->capacity - buf->position;
    if (available_space == 0) {
        zym_runtimeError(vm, "Buffer is full");
        return ZYM_ERROR;
    }

#ifdef _WIN32
    DWORD bytesRead;
    if (!ReadFile(proc->hStdout, buf->data + buf->position, available_space, &bytesRead, NULL)) {
        if (GetLastError() == ERROR_BROKEN_PIPE) {
            return zym_newNumber(0);
        }
        zym_runtimeError(vm, "Failed to read from process stdout");
        return ZYM_ERROR;
    }
#else
    ssize_t bytesRead = read(proc->stdout_fd, buf->data + buf->position, available_space);
    if (bytesRead < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return zym_newNumber(0);
        }
        zym_runtimeError(vm, "Failed to read from process stdout: %s", strerror(errno));
        return ZYM_ERROR;
    }
#endif

    buf->position += bytesRead;
    if (buf->position > buf->length) {
        buf->length = buf->position;
    }

    return zym_newNumber((double)bytesRead);
}

ZymValue process_kill(ZymVM* vm, ZymValue context, ZymValue signalVal) {
    ProcessData* proc = (ProcessData*)zym_getNativeData(context);

    if (!proc->is_running) {
        return context;
    }

#ifdef _WIN32
    // Windows only supports termination
    if (!TerminateProcess(proc->hProcess, 1)) {
        zym_runtimeError(vm, "Failed to terminate process");
        return ZYM_ERROR;
    }
#else
    int signal = SIGTERM;  // Default signal

    if (!zym_isNull(signalVal)) {
        if (zym_isNumber(signalVal)) {
            signal = (int)zym_asNumber(signalVal);
        } else if (zym_isString(signalVal)) {
            const char* signame = zym_asCString(signalVal);
            if (strcmp(signame, "SIGTERM") == 0) signal = SIGTERM;
            else if (strcmp(signame, "SIGKILL") == 0) signal = SIGKILL;
            else if (strcmp(signame, "SIGINT") == 0) signal = SIGINT;
            else if (strcmp(signame, "SIGHUP") == 0) signal = SIGHUP;
            else if (strcmp(signame, "SIGQUIT") == 0) signal = SIGQUIT;
            else if (strcmp(signame, "SIGUSR1") == 0) signal = SIGUSR1;
            else if (strcmp(signame, "SIGUSR2") == 0) signal = SIGUSR2;
            else if (strcmp(signame, "SIGSTOP") == 0) signal = SIGSTOP;
            else if (strcmp(signame, "SIGCONT") == 0) signal = SIGCONT;
            else {
                zym_runtimeError(vm, "Unknown signal: %s", signame);
                return ZYM_ERROR;
            }
        }
    }

    if (kill(proc->pid, signal) < 0) {
        zym_runtimeError(vm, "Failed to send signal: %s", strerror(errno));
        return ZYM_ERROR;
    }
#endif

    return context;
}

ZymValue process_wait(ZymVM* vm, ZymValue context) {
    ProcessData* proc = (ProcessData*)zym_getNativeData(context);

    if (!proc->is_running) {
        return zym_newNumber((double)proc->exit_code);
    }

#ifdef _WIN32
    WaitForSingleObject(proc->hProcess, INFINITE);
    DWORD exitCode;
    GetExitCodeProcess(proc->hProcess, &exitCode);
    proc->exit_code = (int)exitCode;
#else
    int status;
    waitpid(proc->pid, &status, 0);

    if (WIFEXITED(status)) {
        proc->exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        proc->exit_code = 128 + WTERMSIG(status);
    } else {
        proc->exit_code = -1;
    }
#endif

    proc->is_running = false;
    proc->exit_code_valid = true;

    return zym_newNumber((double)proc->exit_code);
}

ZymValue process_poll(ZymVM* vm, ZymValue context) {
    ProcessData* proc = (ProcessData*)zym_getNativeData(context);

    if (!proc->is_running) {
        if (proc->exit_code_valid) {
            return zym_newNumber((double)proc->exit_code);
        }
        return zym_newNull();
    }

#ifdef _WIN32
    DWORD result = WaitForSingleObject(proc->hProcess, 0);
    if (result == WAIT_OBJECT_0) {
        DWORD exitCode;
        GetExitCodeProcess(proc->hProcess, &exitCode);
        proc->exit_code = (int)exitCode;
        proc->is_running = false;
        proc->exit_code_valid = true;
        return zym_newNumber((double)proc->exit_code);
    }
#else
    int status;
    pid_t result = waitpid(proc->pid, &status, WNOHANG);

    if (result > 0) {
        if (WIFEXITED(status)) {
            proc->exit_code = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            proc->exit_code = 128 + WTERMSIG(status);
        } else {
            proc->exit_code = -1;
        }
        proc->is_running = false;
        proc->exit_code_valid = true;
        return zym_newNumber((double)proc->exit_code);
    }
#endif

    return zym_newNull();
}

ZymValue process_isRunning(ZymVM* vm, ZymValue context) {
    ProcessData* proc = (ProcessData*)zym_getNativeData(context);

    // Try to update status
    if (proc->is_running) {
        process_poll(vm, context);
    }

    return zym_newBool(proc->is_running);
}

ZymValue process_getPid(ZymVM* vm, ZymValue context) {
    ProcessData* proc = (ProcessData*)zym_getNativeData(context);

#ifdef _WIN32
    return zym_newNumber((double)proc->processId);
#else
    return zym_newNumber((double)proc->pid);
#endif
}

ZymValue process_getExitCode(ZymVM* vm, ZymValue context) {
    ProcessData* proc = (ProcessData*)zym_getNativeData(context);

    if (!proc->exit_code_valid) {
        return zym_newNull();
    }

    return zym_newNumber((double)proc->exit_code);
}

// Main spawn implementation (3-arg version)
ZymValue nativeProcess_spawn(ZymVM* vm, ZymValue commandVal, ZymValue argsVal, ZymValue optionsMap) {
    if (!zym_isString(commandVal)) {
        zym_runtimeError(vm, "Process.spawn() requires a string command");
        return ZYM_ERROR;
    }

    const char* command = zym_asCString(commandVal);

    ProcessData* proc = calloc(1, sizeof(ProcessData));
    if (!proc) {
        zym_runtimeError(vm, "Out of memory");
        return ZYM_ERROR;
    }

    proc->command = strdup(command);

#ifdef _WIN32
    proc->hProcess = INVALID_HANDLE_VALUE;
    proc->hThread = INVALID_HANDLE_VALUE;
    proc->hStdin = INVALID_HANDLE_VALUE;
    proc->hStdout = INVALID_HANDLE_VALUE;
    proc->hStderr = INVALID_HANDLE_VALUE;
    proc->hConPTY = INVALID_HANDLE_VALUE;
    proc->use_conpty = false;
#else
    proc->stdin_fd = -1;
    proc->stdout_fd = -1;
    proc->stderr_fd = -1;
    proc->pty_master = -1;
    proc->use_pty = false;
#endif

    proc->is_running = false;
    proc->exit_code = 0;
    proc->exit_code_valid = false;
    proc->stdin_open = false;
    proc->stdout_open = false;
    proc->stderr_open = false;

    proc->stdout_buf_size = 4096;
    proc->stderr_buf_size = 4096;
    proc->stdout_buffer = malloc(proc->stdout_buf_size);
    proc->stderr_buffer = malloc(proc->stderr_buf_size);
    proc->stdout_buf_len = 0;
    proc->stderr_buf_len = 0;

    // Parse options
    if (!zym_isNull(optionsMap) && zym_isMap(optionsMap)) {
        ZymValue cwdVal = zym_mapGet(vm, optionsMap, "cwd");
        if (zym_isString(cwdVal)) {
            proc->cwd = strdup(zym_asCString(cwdVal));
        }
    }

    // Spawn process
    bool success;
#ifdef _WIN32
    success = spawn_process_windows(vm, proc, argsVal, optionsMap);
#else
    success = spawn_process_unix(vm, proc, argsVal, optionsMap);
#endif

    if (!success) {
        // Instead of crashing, return an error object
        ZymValue errorObj = zym_newMap(vm);
        zym_pushRoot(vm, errorObj);

        ZymValue errorStr = zym_newString(vm, "error");
        zym_pushRoot(vm, errorStr);
        ZymValue messageStr = zym_newString(vm, "Failed to spawn process");
        zym_pushRoot(vm, messageStr);

        zym_mapSet(vm, errorObj, "error", messageStr);

        zym_popRoot(vm); // messageStr
        zym_popRoot(vm); // errorStr
        zym_popRoot(vm); // errorObj

        free(proc->command);
        free(proc->cwd);
        free(proc->stdout_buffer);
        free(proc->stderr_buffer);
        free(proc);

        return errorObj;
    }

    ZymValue context = zym_createNativeContext(vm, proc, process_cleanup);
    zym_pushRoot(vm, context);

    #define CREATE_METHOD_0(name, func) \
        ZymValue name = zym_createNativeClosure(vm, #func "()", func, context); \
        zym_pushRoot(vm, name);

    #define CREATE_METHOD_1(name, func) \
        ZymValue name = zym_createNativeClosure(vm, #func "(arg)", func, context); \
        zym_pushRoot(vm, name);

    CREATE_METHOD_1(write, process_write);
    CREATE_METHOD_1(writeBuffer, process_writeBuffer);
    CREATE_METHOD_0(closeStdin, process_closeStdin);
    CREATE_METHOD_0(read, process_read);
    CREATE_METHOD_0(readErr, process_readErr);
    CREATE_METHOD_0(readNonBlock, process_readNonBlock);
    CREATE_METHOD_1(readToBuffer, process_readToBuffer);
    CREATE_METHOD_1(kill, process_kill);
    CREATE_METHOD_0(wait, process_wait);
    CREATE_METHOD_0(poll, process_poll);
    CREATE_METHOD_0(isRunning, process_isRunning);
    CREATE_METHOD_0(getPid, process_getPid);
    CREATE_METHOD_0(getExitCode, process_getExitCode);

    #undef CREATE_METHOD_0
    #undef CREATE_METHOD_1

    ZymValue obj = zym_newMap(vm);
    zym_pushRoot(vm, obj);

    zym_mapSet(vm, obj, "write", write);
    zym_mapSet(vm, obj, "writeBuffer", writeBuffer);
    zym_mapSet(vm, obj, "closeStdin", closeStdin);
    zym_mapSet(vm, obj, "read", read);
    zym_mapSet(vm, obj, "readErr", readErr);
    zym_mapSet(vm, obj, "readNonBlock", readNonBlock);
    zym_mapSet(vm, obj, "readToBuffer", readToBuffer);
    zym_mapSet(vm, obj, "kill", kill);
    zym_mapSet(vm, obj, "wait", wait);
    zym_mapSet(vm, obj, "poll", poll);
    zym_mapSet(vm, obj, "isRunning", isRunning);
    zym_mapSet(vm, obj, "getPid", getPid);
    zym_mapSet(vm, obj, "getExitCode", getExitCode);

    // (context + 13 methods + obj = 15)
    for (int i = 0; i < 15; i++) {
        zym_popRoot(vm);
    }

    return obj;
}

ZymValue nativeProcess_spawn_1(ZymVM* vm, ZymValue commandVal) {
    return nativeProcess_spawn(vm, commandVal, zym_newNull(), zym_newNull());
}

ZymValue nativeProcess_spawn_2(ZymVM* vm, ZymValue commandVal, ZymValue argsVal) {
    return nativeProcess_spawn(vm, commandVal, argsVal, zym_newNull());
}

ZymValue nativeProcess_exec(ZymVM* vm, ZymValue commandVal, ZymValue argsVal, ZymValue optionsMap) {
    ZymValue proc = nativeProcess_spawn(vm, commandVal, argsVal, optionsMap);

    if (zym_isMap(proc)) {
        ZymValue errorField = zym_mapGet(vm, proc, "error");
        ZymValue waitField = zym_mapGet(vm, proc, "wait");

        // If has "error" field but no "wait" field, it's an error object
        if (!zym_isNull(errorField) && zym_isNull(waitField)) {
            // This is an error object, return it directly
            return proc;
        }
    }

    zym_pushRoot(vm, proc);

    // Get context from the closures to call methods directly
    ZymValue closeStdinClosure = zym_mapGet(vm, proc, "closeStdin");
    ZymValue readClosure = zym_mapGet(vm, proc, "read");
    ZymValue readErrClosure = zym_mapGet(vm, proc, "readErr");
    ZymValue pollClosure = zym_mapGet(vm, proc, "poll");
    ZymValue getExitCodeClosure = zym_mapGet(vm, proc, "getExitCode");

    ZymValue context = zym_getClosureContext(closeStdinClosure);

    process_closeStdin(vm, context);

    // Accumulate output
    char* stdout_data = malloc(1);
    char* stderr_data = malloc(1);
    stdout_data[0] = '\0';
    stderr_data[0] = '\0';
    size_t stdout_len = 0;
    size_t stderr_len = 0;
    size_t stdout_cap = 1;
    size_t stderr_cap = 1;

    // Poll until process exits
    bool process_exited = false;
    while (true) {
        ZymValue exitCodeVal = process_poll(vm, context);

        // Try to read stdout
        ZymValue stdoutChunk = process_read(vm, context);
        if (zym_isString(stdoutChunk)) {
            const char* chunk = zym_asCString(stdoutChunk);
            size_t chunk_len = strlen(chunk);
            if (chunk_len > 0) {
                if (stdout_len + chunk_len + 1 > stdout_cap) {
                    stdout_cap = (stdout_len + chunk_len + 1) * 2;
                    char* new_data = realloc(stdout_data, stdout_cap);
                    if (!new_data) {
                        // Out of memory - free what we have and return error
                        free(stdout_data);
                        free(stderr_data);
                        zym_popRoot(vm);  // proc
                        zym_runtimeError(vm, "Out of memory while reading process output");
                        return ZYM_ERROR;
                    }
                    stdout_data = new_data;
                }
                memcpy(stdout_data + stdout_len, chunk, chunk_len);
                stdout_len += chunk_len;
                stdout_data[stdout_len] = '\0';
            }
        }

        // Try to read stderr
        ZymValue stderrChunk = process_readErr(vm, context);
        if (zym_isString(stderrChunk)) {
            const char* chunk = zym_asCString(stderrChunk);
            size_t chunk_len = strlen(chunk);
            if (chunk_len > 0) {
                if (stderr_len + chunk_len + 1 > stderr_cap) {
                    stderr_cap = (stderr_len + chunk_len + 1) * 2;
                    char* new_data = realloc(stderr_data, stderr_cap);
                    if (!new_data) {
                        // Out of memory - free what we have and return error
                        free(stdout_data);
                        free(stderr_data);
                        zym_popRoot(vm);  // proc
                        zym_runtimeError(vm, "Out of memory while reading process output");
                        return ZYM_ERROR;
                    }
                    stderr_data = new_data;
                }
                memcpy(stderr_data + stderr_len, chunk, chunk_len);
                stderr_len += chunk_len;
                stderr_data[stderr_len] = '\0';
            }
        }

        // Check if exited
        if (!zym_isNull(exitCodeVal)) {
            // Process exited - do one more read to capture remaining output
            if (process_exited) {
                break;
            }
            process_exited = true;
        }

        // Small delay to avoid busy-waiting (skip if already exited and doing final read)
        if (!process_exited) {
#ifdef _WIN32
            Sleep(10);
#else
            usleep(10000);
#endif
        }
    }

    ZymValue exitCodeVal = process_getExitCode(vm, context);

    ZymValue result = zym_newMap(vm);
    zym_pushRoot(vm, result);

    ZymValue stdoutStr = zym_newString(vm, stdout_data);
    zym_pushRoot(vm, stdoutStr);
    ZymValue stderrStr = zym_newString(vm, stderr_data);
    zym_pushRoot(vm, stderrStr);

    zym_mapSet(vm, result, "stdout", stdoutStr);
    zym_mapSet(vm, result, "stderr", stderrStr);
    zym_mapSet(vm, result, "exitCode", exitCodeVal);

    zym_popRoot(vm);  // stderrStr
    zym_popRoot(vm);  // stdoutStr
    zym_popRoot(vm);  // proc

    free(stdout_data);
    free(stderr_data);

    zym_popRoot(vm);  // result

    return result;
}

ZymValue nativeProcess_exec_1(ZymVM* vm, ZymValue commandVal) {
    return nativeProcess_exec(vm, commandVal, zym_newNull(), zym_newNull());
}

ZymValue nativeProcess_exec_2(ZymVM* vm, ZymValue commandVal, ZymValue argsVal) {
    return nativeProcess_exec(vm, commandVal, argsVal, zym_newNull());
}

ZymValue nativeProcess_getCwd(ZymVM* vm) {
    char buffer[4096];
    if (getcwd(buffer, sizeof(buffer)) == NULL) {
        zym_runtimeError(vm, "Failed to get current working directory");
        return ZYM_ERROR;
    }
    return zym_newString(vm, buffer);
}

ZymValue nativeProcess_setCwd(ZymVM* vm, ZymValue pathVal) {
    if (!zym_isString(pathVal)) {
        zym_runtimeError(vm, "Process.setCwd() requires a string path");
        return ZYM_ERROR;
    }

    const char* path = zym_asCString(pathVal);
    if (chdir(path) != 0) {
        zym_runtimeError(vm, "Failed to change directory: %s", strerror(errno));
        return ZYM_ERROR;
    }

    return zym_newNull();
}

ZymValue nativeProcess_getEnv(ZymVM* vm, ZymValue keyVal) {
    if (!zym_isString(keyVal)) {
        zym_runtimeError(vm, "Process.getEnv() requires a string key");
        return ZYM_ERROR;
    }

    const char* key = zym_asCString(keyVal);
    const char* value = getenv(key);

    if (value == NULL) {
        return zym_newNull();
    }

    return zym_newString(vm, value);
}

ZymValue nativeProcess_setEnv(ZymVM* vm, ZymValue keyVal, ZymValue valueVal) {
    if (!zym_isString(keyVal) || !zym_isString(valueVal)) {
        zym_runtimeError(vm, "Process.setEnv() requires two string arguments");
        return ZYM_ERROR;
    }

    const char* key = zym_asCString(keyVal);
    const char* value = zym_asCString(valueVal);

#ifdef _WIN32
    if (!SetEnvironmentVariableA(key, value)) {
        zym_runtimeError(vm, "Failed to set environment variable");
        return ZYM_ERROR;
    }
#else
    if (setenv(key, value, 1) != 0) {
        zym_runtimeError(vm, "Failed to set environment variable: %s", strerror(errno));
        return ZYM_ERROR;
    }
#endif

    return zym_newNull();
}

ZymValue nativeProcess_getEnvAll(ZymVM* vm) {
    ZymValue envMap = zym_newMap(vm);
    zym_pushRoot(vm, envMap);

#ifdef _WIN32
    LPCH envStrings = GetEnvironmentStringsA();
    if (envStrings) {
        LPCH current = envStrings;
        while (*current) {
            char* equals = strchr(current, '=');
            if (equals && equals != current) {
                size_t keyLen = equals - current;
                char* key = malloc(keyLen + 1);
                memcpy(key, current, keyLen);
                key[keyLen] = '\0';

                const char* value = equals + 1;

                ZymValue valueStr = zym_newString(vm, value);
                zym_pushRoot(vm, valueStr);
                zym_mapSet(vm, envMap, key, valueStr);
                zym_popRoot(vm);

                free(key);
            }
            current += strlen(current) + 1;
        }
        FreeEnvironmentStringsA(envStrings);
    }
#else
    extern char** environ;
    for (char** env = environ; *env != NULL; env++) {
        char* equals = strchr(*env, '=');
        if (equals) {
            size_t keyLen = equals - *env;
            char* key = malloc(keyLen + 1);
            memcpy(key, *env, keyLen);
            key[keyLen] = '\0';

            const char* value = equals + 1;

            // Protect string allocation from GC
            ZymValue valueStr = zym_newString(vm, value);
            zym_pushRoot(vm, valueStr);
            zym_mapSet(vm, envMap, key, valueStr);
            zym_popRoot(vm);

            free(key);
        }
    }
#endif

    zym_popRoot(vm);
    return envMap;
}

ZymValue nativeProcess_getPid(ZymVM* vm) {
#ifdef _WIN32
    return zym_newNumber((double)GetCurrentProcessId());
#else
    return zym_newNumber((double)getpid());
#endif
}

ZymValue nativeProcess_getParentPid(ZymVM* vm) {
#ifdef _WIN32
    // Windows doesn't have a direct getppid equivalent
    return zym_newNull();
#else
    return zym_newNumber((double)getppid());
#endif
}

ZymValue nativeProcess_exit(ZymVM* vm, ZymValue codeVal) {
    int code = 0;

    if (!zym_isNull(codeVal) && zym_isNumber(codeVal)) {
        code = (int)zym_asNumber(codeVal);
    }

    exit(code);
    return zym_newNull();
}

ZymValue nativeProcess_exit_0(ZymVM* vm) {
    exit(0);
    return zym_newNull();
}
