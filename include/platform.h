/* platform.h —— 平台相关：UTF-8 控制台、UTF-8 路径文件、按显示宽度对齐。 */
#ifndef PLATFORM_H
#define PLATFORM_H

#include <stddef.h>
#include <stdio.h>

/* 把 Windows 控制台切到 UTF-8 代码页；非 Windows 平台为空操作。 */
void platform_init_console(void);

/* 以 UTF-8 路径打开文件（Windows 下走 _wfopen，支持中文路径）。 */
FILE *platform_fopen(const char *path, const char *mode);

/* 读取一行输入并去掉行尾 CR/LF；成功返回长度，EOF 返回 -1。 */
int platform_read_line(char *buf, size_t cap);

/* UTF-8 字符串的终端显示宽度（CJK 记 2 列）。 */
int nav_display_width(const char *s);

/* 打印字符串并补空格到指定显示宽度；超宽时不截断，只打印原串。 */
void nav_print_padded(const char *s, int width);

/* 取可执行文件所在目录（UTF-8），成功返回 0。 */
int platform_exe_dir(char *buf, size_t cap);

/* 创建单层目录；已存在视为成功。 */
int platform_mkdir(const char *path);

/* 判断路径是否为已存在的普通文件。 */
int platform_file_exists(const char *path);

/* Windows 下 main 的 argv 是按 ANSI 代码页转换的，中文参数会乱码。
 * 用 GetCommandLineW + CommandLineToArgvW 重新取参并转成 UTF-8。
 * 失败返回 NULL（调用方继续使用原 argv）。 */
char **platform_utf8_args(int *argc_out);

/* 高精度计时（毫秒）。Windows 用 QueryPerformanceCounter，其它平台退回 clock()。 */
double platform_now_ms(void);

#endif /* PLATFORM_H */
