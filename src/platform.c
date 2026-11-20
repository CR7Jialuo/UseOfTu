/* platform.c —— Windows 控制台 UTF-8、UTF-8 路径文件打开、显示宽度对齐。 */
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#endif

void platform_init_console(void)
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

#ifdef _WIN32
static int utf8_to_wide(const char *s, wchar_t *out, int cap)
{
    int n;
    if (s == NULL || out == NULL || cap <= 0) {
        return -1;
    }
    n = MultiByteToWideChar(CP_UTF8, 0, s, -1, out, cap);
    return (n > 0) ? n : -1;
}
#endif

FILE *platform_fopen(const char *path, const char *mode)
{
#ifdef _WIN32
    wchar_t wpath[1024];
    wchar_t wmode[16];
    if (utf8_to_wide(path, wpath, (int)(sizeof(wpath) / sizeof(wpath[0]))) < 0) {
        return NULL;
    }
    if (utf8_to_wide(mode, wmode, (int)(sizeof(wmode) / sizeof(wmode[0]))) < 0) {
        return NULL;
    }
    return _wfopen(wpath, wmode);
#else
    return fopen(path, mode);
#endif
}

int platform_read_line(char *buf, size_t cap)
{
    size_t len;

    if (buf == NULL || cap == 0) {
        return -1;
    }
    if (fgets(buf, (int)cap, stdin) == NULL) {
        return -1;
    }
    len = strlen(buf);
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
        buf[--len] = '\0';
    }
    return (int)len;
}

/* 解码一个 UTF-8 码点，返回消耗字节数；非法序列按 1 字节处理。 */
static int utf8_decode(const unsigned char *s, unsigned int *cp)
{
    if (s[0] < 0x80) {
        *cp = s[0];
        return 1;
    }
    if ((s[0] & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
        *cp = ((unsigned int)(s[0] & 0x1F) << 6) | (unsigned int)(s[1] & 0x3F);
        return 2;
    }
    if ((s[0] & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
        *cp = ((unsigned int)(s[0] & 0x0F) << 12) |
              ((unsigned int)(s[1] & 0x3F) << 6) | (unsigned int)(s[2] & 0x3F);
        return 3;
    }
    if ((s[0] & 0xF8) == 0xF0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80 &&
        (s[3] & 0xC0) == 0x80) {
        *cp = ((unsigned int)(s[0] & 0x07) << 18) |
              ((unsigned int)(s[1] & 0x3F) << 12) |
              ((unsigned int)(s[2] & 0x3F) << 6) | (unsigned int)(s[3] & 0x3F);
        return 4;
    }
    *cp = s[0];
    return 1;
}

static int cp_display_width(unsigned int cp)
{
    if (cp < 0x20) {
        return 0;
    }
    if (cp < 0x7F) {
        return 1;
    }
    if ((cp >= 0x1100 && cp <= 0x115F) || (cp >= 0x2E80 && cp <= 0xA4CF) ||
        (cp >= 0xAC00 && cp <= 0xD7A3) || (cp >= 0xF900 && cp <= 0xFAFF) ||
        (cp >= 0xFE30 && cp <= 0xFE4F) || (cp >= 0xFF00 && cp <= 0xFF60) ||
        (cp >= 0xFFE0 && cp <= 0xFFE6) || (cp >= 0x20000 && cp <= 0x3FFFD)) {
        return 2;
    }
    return 1;
}

int nav_display_width(const char *s)
{
    const unsigned char *p = (const unsigned char *)s;
    int width = 0;

    if (s == NULL) {
        return 0;
    }
    while (*p != '\0') {
        unsigned int cp = 0;
        int used = utf8_decode(p, &cp);
        width += cp_display_width(cp);
        p += used;
    }
    return width;
}

void nav_print_padded(const char *s, int width)
{
    int w;

    if (s == NULL) {
        s = "";
    }
    fputs(s, stdout);
    w = nav_display_width(s);
    while (w < width) {
        fputc(' ', stdout);
        ++w;
    }
}

int platform_exe_dir(char *buf, size_t cap)
{
#ifdef _WIN32
    wchar_t wpath[1024];
    DWORD n;
    int i, len;

    if (buf == NULL || cap == 0) {
        return -1;
    }
    n = GetModuleFileNameW(NULL, wpath, (DWORD)(sizeof(wpath) / sizeof(wpath[0])));
    if (n == 0 || n >= sizeof(wpath) / sizeof(wpath[0])) {
        return -1;
    }
    for (i = (int)n - 1; i >= 0; --i) {
        if (wpath[i] == L'\\' || wpath[i] == L'/') {
            wpath[i] = L'\0';
            break;
        }
    }
    len = WideCharToMultiByte(CP_UTF8, 0, wpath, -1, buf, (int)cap, NULL, NULL);
    return (len > 0) ? 0 : -1;
#else
    (void)buf;
    (void)cap;
    return -1;
#endif
}

int platform_mkdir(const char *path)
{
#ifdef _WIN32
    wchar_t wpath[1024];
    if (utf8_to_wide(path, wpath, (int)(sizeof(wpath) / sizeof(wpath[0]))) < 0) {
        return -1;
    }
    if (_wmkdir(wpath) == 0) {
        return 0;
    }
    return (GetLastError() == ERROR_ALREADY_EXISTS) ? 0 : -1;
#else
    (void)path;
    return -1;
#endif
}

int platform_file_exists(const char *path)
{
    FILE *fp;
    if (path == NULL) {
        return 0;
    }
    fp = platform_fopen(path, "rb");
    if (fp == NULL) {
        return 0;
    }
    fclose(fp);
    return 1;
}

#define ARG_MAX_COUNT 64

double platform_now_ms(void)
{
#ifdef _WIN32
    static LARGE_INTEGER freq;
    static int ready = 0;
    LARGE_INTEGER now;
    if (!ready) {
        QueryPerformanceFrequency(&freq);
        ready = 1;
    }
    QueryPerformanceCounter(&now);
    return (double)now.QuadPart * 1000.0 / (double)freq.QuadPart;
#else
    return (double)clock() * 1000.0 / (double)CLOCKS_PER_SEC;
#endif
}

char **platform_utf8_args(int *argc_out)
{
#ifdef _WIN32
    static char *args[ARG_MAX_COUNT];
    LPWSTR *wargv;
    int wargc = 0;
    int i;

    if (argc_out == NULL) {
        return NULL;
    }
    wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    if (wargv == NULL || wargc <= 0 || wargc > ARG_MAX_COUNT) {
        if (wargv != NULL) {
            LocalFree(wargv);
        }
        return NULL;
    }
    for (i = 0; i < wargc; ++i) {
        int need = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, NULL, 0, NULL, NULL);
        if (need <= 0) {
            int k;
            for (k = 0; k < i; ++k) {
                free(args[k]);
            }
            LocalFree(wargv);
            return NULL;
        }
        args[i] = (char *)malloc((size_t)need);
        if (args[i] == NULL) {
            int k;
            for (k = 0; k < i; ++k) {
                free(args[k]);
            }
            LocalFree(wargv);
            return NULL;
        }
        WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, args[i], need, NULL, NULL);
    }
    LocalFree(wargv);
    *argc_out = wargc;
    return args;
#else
    (void)argc_out;
    return NULL;
#endif
}
