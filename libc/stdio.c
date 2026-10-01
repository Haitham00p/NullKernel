/* Copyright (C) 2026 Haitham Al-Nasser
 * This file is part of NullOS.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NullOS. If not, see <https://www.gnu.org/licenses/>.
 */

#include "libc/stdio.h"
#include "libc/string.h"
#include "libc/ctype.h"
#include "kernel/terminal/terminal.h"
#include "drivers/input/keyboard/keyboard.h"

typedef struct
{
    char *buf;
    size_t size;
    size_t len;
    int to_terminal;
} LibcSink;

static void SinkPut(LibcSink *sink, char c)
{
    if (sink->to_terminal)
    {
        TerminalPutChar32(c, OutColor);
        return;
    }
    if (sink->buf != NULL && sink->len + 1U < sink->size)
    {
        sink->buf[sink->len] = c;
    }
    sink->len++;
}

static void SinkStr(LibcSink *sink, const char *s)
{
    while (*s != '\0')
    {
        SinkPut(sink, *s++);
    }
}

static void SinkPad(LibcSink *sink, char c, size_t n)
{
    while (n > 0)
    {
        SinkPut(sink, c);
        n--;
    }
}

static void SinkFinish(LibcSink *sink)
{
    if (!sink->to_terminal && sink->buf != NULL && sink->size > 0)
    {
        size_t end = sink->len < sink->size - 1U ? sink->len : sink->size - 1U;
        sink->buf[end] = '\0';
    }
}

static void SinkNumber(LibcSink *sink, unsigned long long value, int base,
                       int uppercase, int signed_value, int sign, int width,
                       int zero_pad, int left_align)
{
    static const char lower[] = "0123456789abcdef";
    static const char upper[] = "0123456789ABCDEF";
    const char *digits = uppercase ? upper : lower;
    char tmp[68];
    int i = 0;
    size_t pad;

    (void)signed_value;

    do
    {
        tmp[i++] = digits[value % (unsigned long long)base];
        value /= (unsigned long long)base;
    } while (value > 0);

    if (sign < 0)
    {
        tmp[i++] = '-';
    }
    else if (sign > 0)
    {
        tmp[i++] = '+';
    }

    pad = (size_t)i;
    if ((size_t)width > pad)
    {
        pad = (size_t)width - pad;
    }
    else
    {
        pad = 0;
    }

    if (!left_align && !zero_pad)
    {
        SinkPad(sink, ' ', pad);
    }
    if (!left_align && zero_pad)
    {
        SinkPad(sink, '0', pad);
        for (int j = i - 1; j >= 0; j--)
        {
            SinkPut(sink, tmp[j]);
        }
        return;
    }

    for (int j = i - 1; j >= 0; j--)
    {
        SinkPut(sink, tmp[j]);
    }
    if (left_align)
    {
        SinkPad(sink, ' ', pad);
    }
}

static int LibcVsnprintf(LibcSink *sink, const char *fmt, va_list ap)
{
    while (*fmt != '\0')
    {
        if (*fmt != '%')
        {
            SinkPut(sink, *fmt++);
            continue;
        }
        fmt++;

        int left_align = 0;
        int zero_pad = 0;
        int width = 0;
        int long_l = 0;
        int longlong_ll = 0;
        int size_z = 0;

        while (*fmt == '-' || *fmt == '0' || *fmt == '+' || *fmt == ' ' ||
               *fmt == '#')
        {
            if (*fmt == '-')
            {
                left_align = 1;
            }
            else if (*fmt == '0')
            {
                zero_pad = 1;
            }
            fmt++;
        }

        while (isdigit((int)*fmt))
        {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        while (*fmt == 'l')
        {
            if (long_l)
            {
                longlong_ll = 1;
            }
            long_l = 1;
            fmt++;
        }
        if (*fmt == 'z')
        {
            size_z = 1;
            fmt++;
        }
        else if (*fmt == 'h')
        {
            fmt++;
            if (*fmt == 'h')
            {
                fmt++;
            }
        }

        switch (*fmt)
        {
            case 'd':
            case 'i':
            {
                long long v;
                int sign = 0;

                if (longlong_ll)
                {
                    v = va_arg(ap, long long);
                }
                else if (long_l)
                {
                    v = (long long)va_arg(ap, long);
                }
                else if (size_z)
                {
                    v = (long long)va_arg(ap, size_t);
                }
                else
                {
                    v = (long long)va_arg(ap, int);
                }
                if (v < 0)
                {
                    sign = -1;
                    v = -v;
                }
                SinkNumber(sink, (unsigned long long)v, 10, 0, 1, sign,
                           width, zero_pad, left_align);
                break;
            }
            case 'u':
            case 'x':
            case 'X':
            case 'o':
            {
                unsigned long long v;

                if (longlong_ll)
                {
                    v = va_arg(ap, unsigned long long);
                }
                else if (long_l)
                {
                    v = (unsigned long long)va_arg(ap, unsigned long);
                }
                else if (size_z)
                {
                    v = (unsigned long long)va_arg(ap, size_t);
                }
                else
                {
                    v = (unsigned long long)va_arg(ap, unsigned int);
                }
                SinkNumber(sink, v, *fmt == 'o' ? 8 : 16,
                           *fmt == 'X', 0, 0, width, zero_pad, left_align);
                break;
            }
            case 'c':
            {
                char c = (char)va_arg(ap, int);
                int padlen = width > 0 ? width - 1 : 0;

                if (!left_align)
                {
                    SinkPad(sink, ' ', (size_t)padlen);
                }
                SinkPut(sink, c);
                if (left_align)
                {
                    SinkPad(sink, ' ', (size_t)padlen);
                }
                break;
            }
            case 's':
            {
                const char *s = va_arg(ap, const char *);
                size_t len;

                if (s == NULL)
                {
                    s = "(null)";
                }
                len = strlen(s);
                if ((size_t)width > len && !left_align)
                {
                    SinkPad(sink, ' ', (size_t)width - len);
                }
                SinkStr(sink, s);
                if ((size_t)width > len && left_align)
                {
                    SinkPad(sink, ' ', (size_t)width - len);
                }
                break;
            }
            case 'p':
            {
                unsigned long long v = (unsigned long long)va_arg(ap, void *);

                SinkStr(sink, "0x");
                SinkNumber(sink, v, 16, 0, 0, 0, 0, 0, 0);
                break;
            }
            case '%':
                SinkPut(sink, '%');
                break;
            case '\0':
                return -1;
            default:
                SinkPut(sink, '%');
                SinkPut(sink, *fmt);
                break;
        }
        if (*fmt != '\0')
        {
            fmt++;
        }
    }
    return (int)sink->len;
}

int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
    LibcSink sink;

    sink.buf = buf;
    sink.size = size;
    sink.len = 0;
    sink.to_terminal = 0;
    LibcVsnprintf(&sink, fmt, ap);
    SinkFinish(&sink);
    return (int)sink.len;
}

int vsprintf(char *buf, const char *fmt, va_list ap)
{
    return vsnprintf(buf, (size_t)-1, fmt, ap);
}

int snprintf(char *buf, size_t size, const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return n;
}

int sprintf(char *buf, const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsprintf(buf, fmt, ap);
    va_end(ap);
    return n;
}

int vprintf(const char *fmt, va_list ap)
{
    LibcSink sink;

    sink.buf = NULL;
    sink.size = 0;
    sink.len = 0;
    sink.to_terminal = 1;
    LibcVsnprintf(&sink, fmt, ap);
    return (int)sink.len;
}

int printf(const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vprintf(fmt, ap);
    va_end(ap);
    return n;
}

int putchar(int c)
{
    TerminalPutChar32((char)c, OutColor);
    return c;
}

int puts(const char *s)
{
    while (*s)
    {
        TerminalPutChar32(*s++, OutColor);
    }
    TerminalPutChar32('\n', OutColor);
    return 0;
}

/* ------------------------------------------------------------------ */
/* scanf family                                                       */
/* ------------------------------------------------------------------ */

typedef struct
{
    const char *buf;
    size_t size;
    size_t pos;
    int from_keyboard;
    int have_pushback;
    char pushback_char;
} LibcSource;

static char SrcGet(LibcSource *src)
{
    if (src->have_pushback)
    {
        src->have_pushback = 0;
        return src->pushback_char;
    }
    if (src->from_keyboard)
    {
        return KbdReadChar();
    }
    if (src->pos >= src->size)
    {
        return (char)-1;
    }
    return src->buf[src->pos++];
}

static void SrcUnget(LibcSource *src, char c)
{
    src->have_pushback = 1;
    src->pushback_char = c;
}

static int SrcIsSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
           c == '\v' || c == '\f';
}

static int SrcDigitValue(char c)
{
    if (c >= '0' && c <= '9')
    {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f')
    {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F')
    {
        return c - 'A' + 10;
    }
    return -1;
}

/* Reads an integer in the given base.  Returns 1 when at least one digit
   was consumed; *Neg receives the sign and *Value the magnitude.
   MaxWidth caps the digit count (0 = unlimited). */
static int SrcParseNumber(LibcSource *src, int base, int *Neg,
                          unsigned long long *Value, int MaxWidth)
{
    char c;
    int negative = 0;
    unsigned long long v = 0;
    int digits = 0;

    c = SrcGet(src);
    while (SrcIsSpace(c))
    {
        c = SrcGet(src);
    }

    if (c == '+' || c == '-')
    {
        negative = (c == '-');
        c = SrcGet(src);
    }

    while (digits < MaxWidth || MaxWidth == 0)
    {
        int d = SrcDigitValue(c);

        if (d < 0 || d >= base)
        {
            break;
        }
        v = (v * (unsigned long long)base) + (unsigned long long)d;
        digits++;
        c = SrcGet(src);
    }

    if (c != (char)-1)
    {
        SrcUnget(src, c);
    }

    *Neg = negative;
    *Value = v;
    return digits > 0;
}

/* Peek at the next token to resolve the base for %i.  Returns the base
   and leaves the input positioned at the first digit (or its sign). */
static int SrcScanBase(LibcSource *src)
{
    char c = SrcGet(src);

    while (SrcIsSpace(c))
    {
        c = SrcGet(src);
    }

    if (c == '0')
    {
        char n = SrcGet(src);

        if (n == 'x' || n == 'X')
        {
            return 16;
        }
        if (n != (char)-1)
        {
            SrcUnget(src, n);
        }
        SrcUnget(src, c);
        return 8;
    }

    if (c == '+' || c == '-')
    {
        SrcUnget(src, c);
    }
    else if (c != (char)-1)
    {
        SrcUnget(src, c);
    }
    return 10;
}

static void StoreSigned(va_list ap, int Neg, unsigned long long V,
                        int H, int HH, int L, int LL, int Z)
{
    long long s = Neg ? -(long long)V : (long long)V;

    if (LL)
    {
        long long *p = va_arg(ap, long long *);
        if (p != NULL) *p = s;
    }
    else if (L)
    {
        long *p = va_arg(ap, long *);
        if (p != NULL) *p = (long)s;
    }
    else if (Z)
    {
        size_t *p = va_arg(ap, size_t *);
        if (p != NULL) *p = (size_t)s;
    }
    else if (HH)
    {
        signed char *p = va_arg(ap, signed char *);
        if (p != NULL) *p = (signed char)s;
    }
    else if (H)
    {
        short *p = va_arg(ap, short *);
        if (p != NULL) *p = (short)s;
    }
    else
    {
        int *p = va_arg(ap, int *);
        if (p != NULL) *p = (int)s;
    }
}

static void StoreUnsigned(va_list ap, unsigned long long v,
                          int H, int HH, int L, int LL, int Z)
{
    if (LL)
    {
        unsigned long long *p = va_arg(ap, unsigned long long *);
        if (p != NULL) *p = v;
    }
    else if (L)
    {
        unsigned long *p = va_arg(ap, unsigned long *);
        if (p != NULL) *p = (unsigned long)v;
    }
    else if (Z)
    {
        size_t *p = va_arg(ap, size_t *);
        if (p != NULL) *p = (size_t)v;
    }
    else if (HH)
    {
        unsigned char *p = va_arg(ap, unsigned char *);
        if (p != NULL) *p = (unsigned char)v;
    }
    else if (H)
    {
        unsigned short *p = va_arg(ap, unsigned short *);
        if (p != NULL) *p = (unsigned short)v;
    }
    else
    {
        unsigned int *p = va_arg(ap, unsigned int *);
        if (p != NULL) *p = (unsigned int)v;
    }
}

static int LibcVsscan(LibcSource *src, const char *fmt, va_list ap)
{
    int assigned = 0;

    while (*fmt != '\0')
    {
        char c;

        if (SrcIsSpace(*fmt))
        {
            fmt++;
            while (SrcIsSpace(*fmt))
            {
                fmt++;
            }
            c = SrcGet(src);
            while (SrcIsSpace(c))
            {
                c = SrcGet(src);
            }
            if (c != (char)-1)
            {
                SrcUnget(src, c);
            }
            continue;
        }

        if (*fmt != '%')
        {
            c = SrcGet(src);
            if (c == (char)-1 || c != *fmt)
            {
                break;
            }
            fmt++;
            continue;
        }

        fmt++;
        if (*fmt == '%')
        {
            c = SrcGet(src);
            if (c == (char)-1 || c != '%')
            {
                break;
            }
            fmt++;
            continue;
        }

        {
            int suppress = 0;
            int width = 0;
            int long_l = 0;
            int longlong_ll = 0;
            int size_z = 0;
            int short_h = 0;
            int shortshort_hh = 0;
            char conv;

            if (*fmt == '*')
            {
                suppress = 1;
                fmt++;
            }

            while (isdigit((int)*fmt))
            {
                width = width * 10 + (*fmt - '0');
                fmt++;
            }

            while (*fmt == 'l')
            {
                if (long_l)
                {
                    longlong_ll = 1;
                }
                long_l = 1;
                fmt++;
            }
            if (*fmt == 'z')
            {
                size_z = 1;
                fmt++;
            }
            else if (*fmt == 'h')
            {
                short_h = 1;
                fmt++;
                if (*fmt == 'h')
                {
                    shortshort_hh = 1;
                    fmt++;
                }
            }

            conv = *fmt++;
            if (conv == '\0')
            {
                break;
            }

            switch (conv)
            {
                case 'd':
                case 'i':
                case 'u':
                case 'o':
                case 'x':
                case 'X':
                {
                    int base;
                    int Neg;
                    unsigned long long v;

                    if (conv == 'x' || conv == 'X')
                    {
                        base = 16;
                    }
                    else if (conv == 'o')
                    {
                        base = 8;
                    }
                    else if (conv == 'i')
                    {
                        base = SrcScanBase(src);
                    }
                    else
                    {
                        base = 10;
                    }
                    if (!SrcParseNumber(src, base, &Neg, &v, width))
                    {
                        return assigned;
                    }
                    if (!suppress)
                    {
                        if (conv == 'd' || conv == 'i')
                        {
                            StoreSigned(ap, Neg, v, short_h, shortshort_hh,
                                        long_l, longlong_ll, size_z);
                        }
                        else
                        {
                            StoreUnsigned(ap, v, short_h, shortshort_hh,
                                          long_l, longlong_ll, size_z);
                        }
                    }
                    assigned++;
                    break;
                }
                case 'c':
                {
                    int n = width > 0 ? width : 1;

                    while (n > 0)
                    {
                        c = SrcGet(src);
                        if (c == (char)-1)
                        {
                            return assigned;
                        }
                        if (!suppress)
                        {
                            char *out = va_arg(ap, char *);
                            if (out != NULL)
                            {
                                *out = c;
                            }
                        }
                        n--;
                    }
                    assigned++;
                    break;
                }
                case 's':
                {
                    char *out = NULL;
                    int n = 0;

                    c = SrcGet(src);
                    while (SrcIsSpace(c))
                    {
                        c = SrcGet(src);
                    }
                    if (c == (char)-1)
                    {
                        return assigned;
                    }
                    if (!suppress)
                    {
                        out = va_arg(ap, char *);
                        if (out == NULL)
                        {
                            return assigned;
                        }
                    }
                    while (c != (char)-1 && !SrcIsSpace(c) &&
                           (width == 0 || n < width))
                    {
                        if (out != NULL)
                        {
                            *out++ = c;
                        }
                        n++;
                        c = SrcGet(src);
                    }
                    if (c != (char)-1)
                    {
                        SrcUnget(src, c);
                    }
                    if (out != NULL)
                    {
                        *out = '\0';
                    }
                    assigned++;
                    break;
                }
                default:
                    return assigned;
            }
        }
    }
    return assigned;
}

int vsscanf(const char *buf, const char *fmt, va_list ap)
{
    LibcSource src;

    src.buf = buf;
    src.size = strlen(buf);
    src.pos = 0;
    src.from_keyboard = 0;
    src.have_pushback = 0;
    src.pushback_char = 0;
    return LibcVsscan(&src, fmt, ap);
}

int sscanf(const char *buf, const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsscanf(buf, fmt, ap);
    va_end(ap);
    return n;
}

int vscanf(const char *fmt, va_list ap)
{
    LibcSource src;

    src.buf = NULL;
    src.size = 0;
    src.pos = 0;
    src.from_keyboard = 1;
    src.have_pushback = 0;
    src.pushback_char = 0;
    return LibcVsscan(&src, fmt, ap);
}

int scanf(const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vscanf(fmt, ap);
    va_end(ap);
    return n;
}
