#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <locale.h>
#include "./natives.h"

#ifdef _WIN32
    #include <windows.h>
    #include <conio.h>
#else
    #include <unistd.h>
    #include <termios.h>
    #include <sys/ioctl.h>
    #include <sys/select.h>
#endif

typedef struct {
    FILE* out;                  // Output stream (stdout)
    FILE* in;                   // Input stream (stdin)
    FILE* err;                  // Error stream (stderr)

    // Color state
    int foreground;             // Current foreground color (0-255)
    int background;             // Current background color (0-255)
    bool use_rgb_fg;
    bool use_rgb_bg;
    int fg_r, fg_g, fg_b;       // RGB foreground
    int bg_r, bg_g, bg_b;       // RGB background

    // Style state
    bool bold;
    bool italic;
    bool underline;
    bool reverse;
    bool strikethrough;
    bool dim;

    // Cursor state
    int cursor_x;               // Last known cursor X (cached)
    int cursor_y;               // Last known cursor Y (cached)
    bool cursor_visible;

    // Terminal size
    int width;
    int height;

    // Mode state
    bool raw_mode;              // Raw input mode (no echo, no buffer)
    bool alt_screen;            // Using alternate screen buffer

#ifdef _WIN32
    HANDLE hConsole;
    DWORD originalMode;
    CONSOLE_SCREEN_BUFFER_INFO originalInfo;
#else
    struct termios original_termios;
    bool termios_saved;
#endif
} ConsoleData;

#ifdef _WIN32

static void enable_virtual_terminal(ConsoleData* con) {
    DWORD mode = 0;
    GetConsoleMode(con->hConsole, &mode);
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(con->hConsole, mode);
}

static void get_console_size(ConsoleData* con) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(con->hConsole, &csbi)) {
        con->width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        con->height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    } else {
        con->width = 80;  // Fallback dimensions
        con->height = 24;
    }
}

#else

static void get_console_size(ConsoleData* con) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        con->width = ws.ws_col;
        con->height = ws.ws_row;
    } else {
        con->width = 80;  // Fallback dimensions
        con->height = 24;
    }
}

#endif

void console_cleanup(ZymVM* vm, void* ptr) {
    ConsoleData* con = (ConsoleData*)ptr;

    fprintf(con->out, "\033[0m");

    if (!con->cursor_visible) {
        fprintf(con->out, "\033[?25h");
    }

    if (con->alt_screen) {
        fprintf(con->out, "\033[?1049l");
    }

#ifdef _WIN32
    SetConsoleMode(con->hConsole, con->originalMode);
#else
    if (con->termios_saved) {
        tcsetattr(STDIN_FILENO, TCSANOW, &con->original_termios);
    }
#endif

    fflush(con->out);
    free(con);
}

static const char* get_ansi_fg_code(int color) {
    static const char* codes[] = {
        "30", "31", "32", "33", "34", "35", "36", "37",
        "90", "91", "92", "93", "94", "95", "96", "97"
    };
    if (color >= 0 && color < 16) {
        return codes[color];
    }
    return "39";
}

static const char* get_ansi_bg_code(int color) {
    static const char* codes[] = {
        "40", "41", "42", "43", "44", "45", "46", "47",
        "100", "101", "102", "103", "104", "105", "106", "107"
    };
    if (color >= 0 && color < 16) {
        return codes[color];
    }
    return "49";
}

static void apply_styles(ConsoleData* con) {
    fprintf(con->out, "\033[0m");

    if (con->bold) fprintf(con->out, "\033[1m");
    if (con->dim) fprintf(con->out, "\033[2m");
    if (con->italic) fprintf(con->out, "\033[3m");
    if (con->underline) fprintf(con->out, "\033[4m");
    if (con->reverse) fprintf(con->out, "\033[7m");
    if (con->strikethrough) fprintf(con->out, "\033[9m");

    if (con->use_rgb_fg) {
        fprintf(con->out, "\033[38;2;%d;%d;%dm", con->fg_r, con->fg_g, con->fg_b);
    } else if (con->foreground >= 0) {
        fprintf(con->out, "\033[%sm", get_ansi_fg_code(con->foreground));
    }

    if (con->use_rgb_bg) {
        fprintf(con->out, "\033[48;2;%d;%d;%dm", con->bg_r, con->bg_g, con->bg_b);
    } else if (con->background >= 0) {
        fprintf(con->out, "\033[%sm", get_ansi_bg_code(con->background));
    }
}

ZymValue console_write(ZymVM* vm, ZymValue context, ZymValue textVal) {
    ConsoleData* con = (ConsoleData*)zym_getNativeData(context);

    if (!zym_isString(textVal)) {
        zym_runtimeError(vm, "write() requires a string argument");
        return ZYM_ERROR;
    }

    const char* text = zym_asCString(textVal);
    fprintf(con->out, "%s", text);

    return context;
}

ZymValue console_writeLine(ZymVM* vm, ZymValue context, ZymValue textVal) {
    ConsoleData* con = (ConsoleData*)zym_getNativeData(context);

    if (!zym_isString(textVal)) {
        zym_runtimeError(vm, "writeLine() requires a string argument");
        return ZYM_ERROR;
    }

    const char* text = zym_asCString(textVal);
    fprintf(con->out, "%s\n", text);

    return context;
}

ZymValue console_writeBuffer(ZymVM* vm, ZymValue context, ZymValue bufferVal) {
    ConsoleData* con = (ConsoleData*)zym_getNativeData(context);

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

    typedef struct {
        uint8_t* data;
        size_t capacity;
        size_t length;
        size_t position;
        ZymValue position_ref;
        bool auto_grow;
        int endianness;
    } BufferData;

    BufferData* buf = (BufferData*)zym_getNativeData(bufferContext);
    if (!buf) {
        zym_runtimeError(vm, "Failed to get buffer data");
        return ZYM_ERROR;
    }

    fwrite(buf->data, 1, buf->length, con->out);

    return context;
}

ZymValue console_flush(ZymVM* vm, ZymValue context) {
    ConsoleData* con = (ConsoleData*)zym_getNativeData(context);
    fflush(con->out);
    return context;
}

ZymValue console_setColor(ZymVM* vm, ZymValue context, ZymValue colorVal) {
    ConsoleData* con = (ConsoleData*)zym_getNativeData(context);

    if (zym_isNumber(colorVal)) {
        int color = (int)zym_asNumber(colorVal);
        if (color < 0 || color > 15) {
            zym_runtimeError(vm, "Color must be 0-15");
            return ZYM_ERROR;
        }
        con->foreground = color;
        con->use_rgb_fg = false;
    } else if (zym_isString(colorVal)) {
        const char* colorName = zym_asCString(colorVal);

        if (strcmp(colorName, "black") == 0) con->foreground = 0;
        else if (strcmp(colorName, "red") == 0) con->foreground = 1;
        else if (strcmp(colorName, "green") == 0) con->foreground = 2;
        else if (strcmp(colorName, "yellow") == 0) con->foreground = 3;
        else if (strcmp(colorName, "blue") == 0) con->foreground = 4;
        else if (strcmp(colorName, "magenta") == 0) con->foreground = 5;
        else if (strcmp(colorName, "cyan") == 0) con->foreground = 6;
        else if (strcmp(colorName, "white") == 0) con->foreground = 7;
        else if (strcmp(colorName, "bright_black") == 0 || strcmp(colorName, "gray") == 0) con->foreground = 8;
        else if (strcmp(colorName, "bright_red") == 0) con->foreground = 9;
        else if (strcmp(colorName, "bright_green") == 0) con->foreground = 10;
        else if (strcmp(colorName, "bright_yellow") == 0) con->foreground = 11;
        else if (strcmp(colorName, "bright_blue") == 0) con->foreground = 12;
        else if (strcmp(colorName, "bright_magenta") == 0) con->foreground = 13;
        else if (strcmp(colorName, "bright_cyan") == 0) con->foreground = 14;
        else if (strcmp(colorName, "bright_white") == 0) con->foreground = 15;
        else {
            zym_runtimeError(vm, "Unknown color name: %s", colorName);
            return ZYM_ERROR;
        }
        con->use_rgb_fg = false;
    } else {
        zym_runtimeError(vm, "setColor() requires a number (0-15) or string");
        return ZYM_ERROR;
    }

    apply_styles(con);
    return context;
}

ZymValue console_setBackgroundColor(ZymVM* vm, ZymValue context, ZymValue colorVal) {
    ConsoleData* con = (ConsoleData*)zym_getNativeData(context);

    if (zym_isNumber(colorVal)) {
        int color = (int)zym_asNumber(colorVal);
        if (color < 0 || color > 15) {
            zym_runtimeError(vm, "Color must be 0-15");
            return ZYM_ERROR;
        }
        con->background = color;
        con->use_rgb_bg = false;
    } else if (zym_isString(colorVal)) {
        const char* colorName = zym_asCString(colorVal);

        if (strcmp(colorName, "black") == 0) con->background = 0;
        else if (strcmp(colorName, "red") == 0) con->background = 1;
        else if (strcmp(colorName, "green") == 0) con->background = 2;
        else if (strcmp(colorName, "yellow") == 0) con->background = 3;
        else if (strcmp(colorName, "blue") == 0) con->background = 4;
        else if (strcmp(colorName, "magenta") == 0) con->background = 5;
        else if (strcmp(colorName, "cyan") == 0) con->background = 6;
        else if (strcmp(colorName, "white") == 0) con->background = 7;
        else if (strcmp(colorName, "bright_black") == 0 || strcmp(colorName, "gray") == 0) con->background = 8;
        else if (strcmp(colorName, "bright_red") == 0) con->background = 9;
        else if (strcmp(colorName, "bright_green") == 0) con->background = 10;
        else if (strcmp(colorName, "bright_yellow") == 0) con->background = 11;
        else if (strcmp(colorName, "bright_blue") == 0) con->background = 12;
        else if (strcmp(colorName, "bright_magenta") == 0) con->background = 13;
        else if (strcmp(colorName, "bright_cyan") == 0) con->background = 14;
        else if (strcmp(colorName, "bright_white") == 0) con->background = 15;
        else {
            zym_runtimeError(vm, "Unknown color name: %s", colorName);
            return ZYM_ERROR;
        }
        con->use_rgb_bg = false;
    } else {
        zym_runtimeError(vm, "setBackgroundColor() requires a number (0-15) or string");
        return ZYM_ERROR;
    }

    apply_styles(con);
    return context;
}

ZymValue console_setColorRGB(ZymVM* vm, ZymValue context, ZymValue rVal, ZymValue gVal, ZymValue bVal) {
    ConsoleData* con = (ConsoleData*)zym_getNativeData(context);

    if (!zym_isNumber(rVal) || !zym_isNumber(gVal) || !zym_isNumber(bVal)) {
        zym_runtimeError(vm, "setColorRGB() requires three numbers (r, g, b)");
        return ZYM_ERROR;
    }

    int r = (int)zym_asNumber(rVal);
    int g = (int)zym_asNumber(gVal);
    int b = (int)zym_asNumber(bVal);

    if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
        zym_runtimeError(vm, "RGB values must be 0-255");
        return ZYM_ERROR;
    }

    con->fg_r = r;
    con->fg_g = g;
    con->fg_b = b;
    con->use_rgb_fg = true;

    apply_styles(con);
    return context;
}

ZymValue console_setBackgroundColorRGB(ZymVM* vm, ZymValue context, ZymValue rVal, ZymValue gVal, ZymValue bVal) {
    ConsoleData* con = (ConsoleData*)zym_getNativeData(context);

    if (!zym_isNumber(rVal) || !zym_isNumber(gVal) || !zym_isNumber(bVal)) {
        zym_runtimeError(vm, "setBackgroundColorRGB() requires three numbers (r, g, b)");
        return ZYM_ERROR;
    }

    int r = (int)zym_asNumber(rVal);
    int g = (int)zym_asNumber(gVal);
    int b = (int)zym_asNumber(bVal);

    if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
        zym_runtimeError(vm, "RGB values must be 0-255");
        return ZYM_ERROR;
    }

    con->bg_r = r;
    con->bg_g = g;
    con->bg_b = b;
    con->use_rgb_bg = true;

    apply_styles(con);
    return context;
}

ZymValue console_reset(ZymVM* vm, ZymValue context) {
    ConsoleData* con = (ConsoleData*)zym_getNativeData(context);

    con->foreground = -1;
    con->background = -1;
    con->use_rgb_fg = false;
    con->use_rgb_bg = false;
    con->bold = false;
    con->italic = false;
    con->underline = false;
    con->reverse = false;
    con->strikethrough = false;
    con->dim = false;

    fprintf(con->out, "\033[0m");
    fflush(con->out);

    return context;
}

ZymValue console_setBold(ZymVM* vm, ZymValue context, ZymValue enableVal) {
    ConsoleData* con = (ConsoleData*)zym_getNativeData(context);

    if (!zym_isBool(enableVal)) {
        zym_runtimeError(vm, "setBold() requires a boolean argument");
        return ZYM_ERROR;
    }

    con->bold = zym_asBool(enableVal);
    apply_styles(con);
    return context;
}

ZymValue console_setItalic(ZymVM* vm, ZymValue context, ZymValue enableVal) {
    ConsoleData* con = (ConsoleData*)zym_getNativeData(context);

    if (!zym_isBool(enableVal)) {
        zym_runtimeError(vm, "setItalic() requires a boolean argument");
        return ZYM_ERROR;
    }

    con->italic = zym_asBool(enableVal);
    apply_styles(con);
    return context;
}

ZymValue console_setUnderline(ZymVM* vm, ZymValue context, ZymValue enableVal) {
    ConsoleData* con = (ConsoleData*)zym_getNativeData(context);

    if (!zym_isBool(enableVal)) {
        zym_runtimeError(vm, "setUnderline() requires a boolean argument");
        return ZYM_ERROR;
    }

    con->underline = zym_asBool(enableVal);
    apply_styles(con);
    return context;
}

ZymValue console_setReverse(ZymVM* vm, ZymValue context, ZymValue enableVal) {
    ConsoleData* con = (ConsoleData*)zym_getNativeData(context);

    if (!zym_isBool(enableVal)) {
        zym_runtimeError(vm, "setReverse() requires a boolean argument");
        return ZYM_ERROR;
    }

    con->reverse = zym_asBool(enableVal);
    apply_styles(con);
    return context;
}

ZymValue console_setStrikethrough(ZymVM* vm, ZymValue context, ZymValue enableVal) {
    ConsoleData* con = (ConsoleData*)zym_getNativeData(context);

    if (!zym_isBool(enableVal)) {
        zym_runtimeError(vm, "setStrikethrough() requires a boolean argument");
        return ZYM_ERROR;
    }

    con->strikethrough = zym_asBool(enableVal);
    apply_styles(con);
    return context;
}

ZymValue console_setDim(ZymVM* vm, ZymValue context, ZymValue enableVal) {
    ConsoleData* con = (ConsoleData*)zym_getNativeData(context);

    if (!zym_isBool(enableVal)) {
        zym_runtimeError(vm, "setDim() requires a boolean argument");
        return ZYM_ERROR;
    }

    con->dim = zym_asBool(enableVal);
    apply_styles(con);
    return context;
}

ZymValue console_moveCursor(ZymVM* vm, ZymValue context, ZymValue xVal, ZymValue yVal) {
    ConsoleData* con = (ConsoleData*)zym_getNativeData(context);

    if (!zym_isNumber(xVal) || !zym_isNumber(yVal)) {
        zym_runtimeError(vm, "moveCursor() requires two number arguments (x, y)");
        return ZYM_ERROR;
    }

    int x = (int)zym_asNumber(xVal);
    int y = (int)zym_asNumber(yVal);

    // ANSI uses 1-based indexing
    fprintf(con->out, "\033[%d;%dH", y + 1, x + 1);
    con->cursor_x = x;
    con->cursor_y = y;

    return context;
}

ZymValue console_moveCursorUp(ZymVM* vm, ZymValue context, ZymValue countVal) {
    ConsoleData* con = (ConsoleData*)zym_getNativeData(context);

    int count = 1;
    if (!zym_isNull(countVal)) {
        if (!zym_isNumber(countVal)) {
            zym_runtimeError(vm, "moveCursorUp() requires a number argument");
            return ZYM_ERROR;
        }
        count = (int)zym_asNumber(countVal);
    }

    if (count > 0) {
        fprintf(con->out, "\033[%dA", count);
        con->cursor_y -= count;
        if (con->cursor_y < 0) con->cursor_y = 0;
    }

    return context;
}

ZymValue console_moveCursorDown(ZymVM* vm, ZymValue context, ZymValue countVal) {
    ConsoleData* con = (ConsoleData*)zym_getNativeData(context);

    int count = 1;
    if (!zym_isNull(countVal)) {
        if (!zym_isNumber(countVal)) {
            zym_runtimeError(vm, "moveCursorDown() requires a number argument");
            return ZYM_ERROR;
        }
        count = (int)zym_asNumber(countVal);
    }

    if (count > 0) {
        fprintf(con->out, "\033[%dB", count);
        con->cursor_y += count;
    }

    return context;
}

ZymValue console_moveCursorLeft(ZymVM* vm, ZymValue context, ZymValue countVal) {
    ConsoleData* con = (ConsoleData*)zym_getNativeData(context);

    int count = 1;
    if (!zym_isNull(countVal)) {
        if (!zym_isNumber(countVal)) {
            zym_runtimeError(vm, "moveCursorLeft() requires a number argument");
            return ZYM_ERROR;
        }
        count = (int)zym_asNumber(countVal);
    }

    if (count > 0) {
        fprintf(con->out, "\033[%dD", count);
        con->cursor_x -= count;
        if (con->cursor_x < 0) con->cursor_x = 0;
    }

    return context;
}

ZymValue console_moveCursorRight(ZymVM* vm, ZymValue context, ZymValue countVal) {
    ConsoleData* con = (ConsoleData*)zym_getNativeData(context);

    int count = 1;
    if (!zym_isNull(countVal)) {
        if (!zym_isNumber(countVal)) {
            zym_runtimeError(vm, "moveCursorRight() requires a number argument");
            return ZYM_ERROR;
        }
        count = (int)zym_asNumber(countVal);
    }

    if (count > 0) {
        fprintf(con->out, "\033[%dC", count);
        con->cursor_x += count;
    }

    return context;
}

ZymValue console_hideCursor(ZymVM* vm, ZymValue context) {
    ConsoleData* con = (ConsoleData*)zym_getNativeData(context);
    fprintf(con->out, "\033[?25l");
    con->cursor_visible = false;
    return context;
}

ZymValue console_showCursor(ZymVM* vm, ZymValue context) {
    ConsoleData* con = (ConsoleData*)zym_getNativeData(context);
    fprintf(con->out, "\033[?25h");
    con->cursor_visible = true;
    return context;
}

ZymValue console_saveCursorPos(ZymVM* vm, ZymValue context) {
    ConsoleData* con = (ConsoleData*)zym_getNativeData(context);
    fprintf(con->out, "\033[s");
    return context;
}

ZymValue console_restoreCursorPos(ZymVM* vm, ZymValue context) {
    ConsoleData* con = (ConsoleData*)zym_getNativeData(context);
    fprintf(con->out, "\033[u");
    return context;
}

ZymValue console_clear(ZymVM* vm, ZymValue context) {
    ConsoleData* con = (ConsoleData*)zym_getNativeData(context);
    fprintf(con->out, "\033[2J\033[H");
    con->cursor_x = 0;
    con->cursor_y = 0;
    return context;
}

ZymValue console_clearLine(ZymVM* vm, ZymValue context) {
    ConsoleData* con = (ConsoleData*)zym_getNativeData(context);
    fprintf(con->out, "\033[2K");
    return context;
}

ZymValue console_clearToEndOfLine(ZymVM* vm, ZymValue context) {
    ConsoleData* con = (ConsoleData*)zym_getNativeData(context);
    fprintf(con->out, "\033[K");
    return context;
}

ZymValue console_clearToStartOfLine(ZymVM* vm, ZymValue context) {
    ConsoleData* con = (ConsoleData*)zym_getNativeData(context);
    fprintf(con->out, "\033[1K");
    return context;
}

ZymValue console_scrollUp(ZymVM* vm, ZymValue context, ZymValue countVal) {
    ConsoleData* con = (ConsoleData*)zym_getNativeData(context);

    int count = 1;
    if (!zym_isNull(countVal)) {
        if (!zym_isNumber(countVal)) {
            zym_runtimeError(vm, "scrollUp() requires a number argument");
            return ZYM_ERROR;
        }
        count = (int)zym_asNumber(countVal);
    }

    if (count > 0) {
        fprintf(con->out, "\033[%dS", count);
    }

    return context;
}

ZymValue console_scrollDown(ZymVM* vm, ZymValue context, ZymValue countVal) {
    ConsoleData* con = (ConsoleData*)zym_getNativeData(context);

    int count = 1;
    if (!zym_isNull(countVal)) {
        if (!zym_isNumber(countVal)) {
            zym_runtimeError(vm, "scrollDown() requires a number argument");
            return ZYM_ERROR;
        }
        count = (int)zym_asNumber(countVal);
    }

    if (count > 0) {
        fprintf(con->out, "\033[%dT", count);
    }

    return context;
}

ZymValue console_useAltScreen(ZymVM* vm, ZymValue context) {
    ConsoleData* con = (ConsoleData*)zym_getNativeData(context);
    fprintf(con->out, "\033[?1049h");
    con->alt_screen = true;
    return context;
}

ZymValue console_useMainScreen(ZymVM* vm, ZymValue context) {
    ConsoleData* con = (ConsoleData*)zym_getNativeData(context);
    fprintf(con->out, "\033[?1049l");
    con->alt_screen = false;
    return context;
}

ZymValue console_readLine(ZymVM* vm, ZymValue context) {
    ConsoleData* con = (ConsoleData*)zym_getNativeData(context);
    char buffer[4096];

    if (fgets(buffer, sizeof(buffer), con->in) == NULL) {
        return zym_newNull();
    }

    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
        len--;
    }
    if (len > 0 && buffer[len - 1] == '\r') {
        buffer[len - 1] = '\0';
    }

    return zym_newString(vm, buffer);
}

ZymValue console_readChar(ZymVM* vm, ZymValue context) {
    ConsoleData* con = (ConsoleData*)zym_getNativeData(context);

#ifdef _WIN32
    int ch = _getch();
    if (ch == EOF) {
        return zym_newNull();
    }
    char str[2] = {(char)ch, '\0'};
    return zym_newString(vm, str);
#else
    int ch = getchar();
    if (ch == EOF) {
        return zym_newNull();
    }
    char str[2] = {(char)ch, '\0'};
    return zym_newString(vm, str);
#endif
}

ZymValue console_hasInput(ZymVM* vm, ZymValue context) {
#ifdef _WIN32
    return zym_newBool(_kbhit() != 0);
#else
    struct timeval tv = {0, 0};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return zym_newBool(select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0);
#endif
}

ZymValue console_setRawMode(ZymVM* vm, ZymValue context, ZymValue enableVal) {
    ConsoleData* con = (ConsoleData*)zym_getNativeData(context);

    if (!zym_isBool(enableVal)) {
        zym_runtimeError(vm, "setRawMode() requires a boolean argument");
        return ZYM_ERROR;
    }

    bool enable = zym_asBool(enableVal);

#ifdef _WIN32
    DWORD mode;
    if (enable) {
        GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &con->originalMode);
        mode = con->originalMode & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT);
        SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), mode);
    } else {
        SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), con->originalMode);
    }
#else
    if (enable) {
        struct termios raw;
        if (!con->termios_saved) {
            tcgetattr(STDIN_FILENO, &con->original_termios);
            con->termios_saved = true;
        }
        raw = con->original_termios;
        raw.c_lflag &= ~(ECHO | ICANON);
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    } else {
        if (con->termios_saved) {
            tcsetattr(STDIN_FILENO, TCSANOW, &con->original_termios);
        }
    }
#endif

    con->raw_mode = enable;
    return context;
}

ZymValue console_getWidth(ZymVM* vm, ZymValue context) {
    ConsoleData* con = (ConsoleData*)zym_getNativeData(context);
    get_console_size(con);
    return zym_newNumber((double)con->width);
}

ZymValue console_getHeight(ZymVM* vm, ZymValue context) {
    ConsoleData* con = (ConsoleData*)zym_getNativeData(context);
    get_console_size(con);
    return zym_newNumber((double)con->height);
}

ZymValue nativeConsole_create(ZymVM* vm) {
    // Allocate console data
    ConsoleData* con = calloc(1, sizeof(ConsoleData));
    if (!con) {
        zym_runtimeError(vm, "Out of memory");
        return ZYM_ERROR;
    }

    con->out = stdout;
    con->in = stdin;
    con->err = stderr;

    con->foreground = -1;
    con->background = -1;
    con->use_rgb_fg = false;
    con->use_rgb_bg = false;
    con->bold = false;
    con->italic = false;
    con->underline = false;
    con->reverse = false;
    con->strikethrough = false;
    con->dim = false;
    con->cursor_visible = true;
    con->cursor_x = 0;
    con->cursor_y = 0;
    con->raw_mode = false;
    con->alt_screen = false;

#ifdef _WIN32
    con->hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleMode(con->hConsole, &con->originalMode);
    GetConsoleScreenBufferInfo(con->hConsole, &con->originalInfo);
    enable_virtual_terminal(con);

    // Enable UTF-8 output for box drawing and Unicode characters
    SetConsoleOutputCP(CP_UTF8);
#else
    con->termios_saved = false;

    // Enable UTF-8 on Unix systems
    setlocale(LC_ALL, "");
#endif

    get_console_size(con);

    ZymValue context = zym_createNativeContext(vm, con, console_cleanup);
    zym_pushRoot(vm, context);

    #define CREATE_METHOD_0(name, func) \
        ZymValue name = zym_createNativeClosure(vm, #func "()", func, context); \
        zym_pushRoot(vm, name);

    #define CREATE_METHOD_1(name, func) \
        ZymValue name = zym_createNativeClosure(vm, #func "(arg)", func, context); \
        zym_pushRoot(vm, name);

    #define CREATE_METHOD_2(name, func) \
        ZymValue name = zym_createNativeClosure(vm, #func "(arg1, arg2)", func, context); \
        zym_pushRoot(vm, name);

    #define CREATE_METHOD_3(name, func) \
        ZymValue name = zym_createNativeClosure(vm, #func "(arg1, arg2, arg3)", func, context); \
        zym_pushRoot(vm, name);

    CREATE_METHOD_1(write, console_write);
    CREATE_METHOD_1(writeLine, console_writeLine);
    CREATE_METHOD_1(writeBuffer, console_writeBuffer);
    CREATE_METHOD_0(flush, console_flush);

    CREATE_METHOD_1(setColor, console_setColor);
    CREATE_METHOD_1(setBackgroundColor, console_setBackgroundColor);
    CREATE_METHOD_3(setColorRGB, console_setColorRGB);
    CREATE_METHOD_3(setBackgroundColorRGB, console_setBackgroundColorRGB);
    CREATE_METHOD_0(reset, console_reset);

    CREATE_METHOD_1(setBold, console_setBold);
    CREATE_METHOD_1(setItalic, console_setItalic);
    CREATE_METHOD_1(setUnderline, console_setUnderline);
    CREATE_METHOD_1(setReverse, console_setReverse);
    CREATE_METHOD_1(setStrikethrough, console_setStrikethrough);
    CREATE_METHOD_1(setDim, console_setDim);

    CREATE_METHOD_2(moveCursor, console_moveCursor);
    CREATE_METHOD_1(moveCursorUp, console_moveCursorUp);
    CREATE_METHOD_1(moveCursorDown, console_moveCursorDown);
    CREATE_METHOD_1(moveCursorLeft, console_moveCursorLeft);
    CREATE_METHOD_1(moveCursorRight, console_moveCursorRight);
    CREATE_METHOD_0(hideCursor, console_hideCursor);
    CREATE_METHOD_0(showCursor, console_showCursor);
    CREATE_METHOD_0(saveCursorPos, console_saveCursorPos);
    CREATE_METHOD_0(restoreCursorPos, console_restoreCursorPos);

    CREATE_METHOD_0(clear, console_clear);
    CREATE_METHOD_0(clearLine, console_clearLine);
    CREATE_METHOD_0(clearToEndOfLine, console_clearToEndOfLine);
    CREATE_METHOD_0(clearToStartOfLine, console_clearToStartOfLine);
    CREATE_METHOD_1(scrollUp, console_scrollUp);
    CREATE_METHOD_1(scrollDown, console_scrollDown);
    CREATE_METHOD_0(useAltScreen, console_useAltScreen);
    CREATE_METHOD_0(useMainScreen, console_useMainScreen);

    CREATE_METHOD_0(readLine, console_readLine);
    CREATE_METHOD_0(readChar, console_readChar);
    CREATE_METHOD_0(hasInput, console_hasInput);
    CREATE_METHOD_1(setRawMode, console_setRawMode);

    CREATE_METHOD_0(getWidth, console_getWidth);
    CREATE_METHOD_0(getHeight, console_getHeight);

    #undef CREATE_METHOD_0
    #undef CREATE_METHOD_1
    #undef CREATE_METHOD_2
    #undef CREATE_METHOD_3

    ZymValue obj = zym_newMap(vm);
    zym_pushRoot(vm, obj);

    zym_mapSet(vm, obj, "write", write);
    zym_mapSet(vm, obj, "writeLine", writeLine);
    zym_mapSet(vm, obj, "writeBuffer", writeBuffer);
    zym_mapSet(vm, obj, "flush", flush);

    zym_mapSet(vm, obj, "setColor", setColor);
    zym_mapSet(vm, obj, "setBackgroundColor", setBackgroundColor);
    zym_mapSet(vm, obj, "setColorRGB", setColorRGB);
    zym_mapSet(vm, obj, "setBackgroundColorRGB", setBackgroundColorRGB);
    zym_mapSet(vm, obj, "reset", reset);

    zym_mapSet(vm, obj, "setBold", setBold);
    zym_mapSet(vm, obj, "setItalic", setItalic);
    zym_mapSet(vm, obj, "setUnderline", setUnderline);
    zym_mapSet(vm, obj, "setReverse", setReverse);
    zym_mapSet(vm, obj, "setStrikethrough", setStrikethrough);
    zym_mapSet(vm, obj, "setDim", setDim);

    zym_mapSet(vm, obj, "moveCursor", moveCursor);
    zym_mapSet(vm, obj, "moveCursorUp", moveCursorUp);
    zym_mapSet(vm, obj, "moveCursorDown", moveCursorDown);
    zym_mapSet(vm, obj, "moveCursorLeft", moveCursorLeft);
    zym_mapSet(vm, obj, "moveCursorRight", moveCursorRight);
    zym_mapSet(vm, obj, "hideCursor", hideCursor);
    zym_mapSet(vm, obj, "showCursor", showCursor);
    zym_mapSet(vm, obj, "saveCursorPos", saveCursorPos);
    zym_mapSet(vm, obj, "restoreCursorPos", restoreCursorPos);

    zym_mapSet(vm, obj, "clear", clear);
    zym_mapSet(vm, obj, "clearLine", clearLine);
    zym_mapSet(vm, obj, "clearToEndOfLine", clearToEndOfLine);
    zym_mapSet(vm, obj, "clearToStartOfLine", clearToStartOfLine);
    zym_mapSet(vm, obj, "scrollUp", scrollUp);
    zym_mapSet(vm, obj, "scrollDown", scrollDown);
    zym_mapSet(vm, obj, "useAltScreen", useAltScreen);
    zym_mapSet(vm, obj, "useMainScreen", useMainScreen);

    zym_mapSet(vm, obj, "readLine", readLine);
    zym_mapSet(vm, obj, "readChar", readChar);
    zym_mapSet(vm, obj, "hasInput", hasInput);
    zym_mapSet(vm, obj, "setRawMode", setRawMode);

    zym_mapSet(vm, obj, "getWidth", getWidth);
    zym_mapSet(vm, obj, "getHeight", getHeight);

    // (context + 39 methods + obj = 41)
    for (int i = 0; i < 41; i++) {
        zym_popRoot(vm);
    }

    return obj;
}
