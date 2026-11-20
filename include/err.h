/* err.h —— 统一的错误信息写入助手（不分配内存、不 exit）。 */
#ifndef NAV_ERR_H
#define NAV_ERR_H

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#if defined(__GNUC__)
#define NAV_PRINTF(fmt_idx, arg_idx) __attribute__((format(printf, fmt_idx, arg_idx)))
#else
#define NAV_PRINTF(fmt_idx, arg_idx)
#endif

static inline void nav_err_set(char *err, size_t cap, const char *fmt, ...) NAV_PRINTF(3, 4);

static inline void nav_err_set(char *err, size_t cap, const char *fmt, ...)
{
    va_list ap;
    if (err == NULL || cap == 0) {
        return;
    }
    va_start(ap, fmt);
    vsnprintf(err, cap, fmt, ap);
    va_end(ap);
}

/* 清空错误缓冲区（便于连续调用时区分是否出错）。 */
static inline void nav_err_clear(char *err, size_t cap)
{
    if (err != NULL && cap > 0) {
        err[0] = '\0';
    }
}

#endif /* NAV_ERR_H */
