#ifndef LIBC_STDIO_H
#define LIBC_STDIO_H

#include <stddef.h>
#include <stdint.h>

#define LIBC_COLOR 0xFF00FFFFU

/* Output text color for command output; resolved to the kernel global OutColor
   (set from /REG/color/op_fg.v). Declared here so both kernel and guest builds
   reference the same symbol. */
extern uint32_t OutColor;

int putchar(int c);
int puts(const char *s);

#ifndef __NCC__
#include <stdarg.h>

int printf(const char *fmt, ...);
int sprintf(char *buf, const char *fmt, ...);
int snprintf(char *buf, size_t size, const char *fmt, ...);
int vprintf(const char *fmt, va_list ap);
int vsprintf(char *buf, const char *fmt, va_list ap);
int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap);

int scanf(const char *fmt, ...);
int sscanf(const char *buf, const char *fmt, ...);
int vscanf(const char *fmt, va_list ap);
int vsscanf(const char *buf, const char *fmt, va_list ap);
#endif

#endif
