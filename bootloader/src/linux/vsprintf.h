#ifndef _LINUX_VSPRINTF_H
#define _LINUX_VSPRINTF_H

#include <stdarg.h>
#include <stddef.h>

int vsnprintf(char *buf, size_t size, const char *fmt_str, va_list args);
int vscnprintf(char *buf, size_t size, const char *fmt, va_list args);
int snprintf(char *buf, size_t size, const char *fmt, ...);
int scnprintf(char *buf, size_t size, const char *fmt, ...);
int vsprintf(char *buf, const char *fmt, va_list args);
int sprintf(char *buf, const char *fmt, ...);

#endif /* _LINUX_VSPRINTF_H */