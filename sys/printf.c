#include <types.h>
#include <stat.h>
#include <stdio.h>
#include <stdarg.h>

char putchar_buf[512];

typedef void (*putch_fn)(char, void*);

int
putchar(char c)
{
  write(1, &c, 1);
  return c;
}

void
putc(int fd, char c)
{
  write(fd, &c, 1);
}

static void
printint(putch_fn putch, void *ctx, int xx, int base, int sgn, int width, int zero_pad, int left_justify)
{
    static char digits[] = "0123456789ABCDEF";
    char buf[16];
    int i, neg;
    uint x;

    neg = 0;
    if(sgn && xx < 0) {
        neg = 1;
        x = -xx;
    } else {
        x = xx;
    }

    i = 0;
    do {
        buf[i++] = digits[x % base];
    } while((x /= base) != 0);

    int total_digits = i;
    int total_chars = total_digits + (neg ? 1 : 0);
    int pad = width > total_chars ? width - total_chars : 0;

    if (left_justify) {
        // left justified: content then padding
        if (neg) putch('-', ctx);
        while (--i >= 0) putch(buf[i], ctx);
        for (int j = 0; j < pad; j++) putch(' ', ctx);
    } else if (zero_pad) {
        // zero padded: sign then zeros then digits
        if (neg) putch('-', ctx);
        for (int j = 0; j < pad; j++) putch('0', ctx);
        while (--i >= 0) putch(buf[i], ctx);
    } else {
        // right justified: padding then sign then digits
        for (int j = 0; j < pad; j++) putch(' ', ctx);
        if (neg) putch('-', ctx);
        while (--i >= 0) putch(buf[i], ctx);
    }
}

static void
vprintfmt(putch_fn putch, void *ctx, const char *fmt, va_list ap)
{
    char *s;
    int c, i, state;
    int width, zero_pad, precision, left_justify;
    int has_precision;

    state = 0;
    for(i = 0; fmt[i]; i++) {
        c = fmt[i] & 0xff;
        if(state == 0) {
            if(c == '%') {
                state = '%';
                width = 0;
                zero_pad = 0;
                left_justify = 0;
                precision = -1;
                has_precision = 0;
            } else {
                putch(c, ctx);
            }
        } else if(state == '%') {
            // parse flags
            if (c == '-') {
                left_justify = 1;
                i++;
                c = fmt[i] & 0xff;
            } else if (c == '0') {
                zero_pad = 1;
                i++;
                c = fmt[i] & 0xff;
            }

            // parse width
            while(c >= '0' && c <= '9') {
                width = width * 10 + (c - '0');
                i++;
                c = fmt[i] & 0xff;
            }

            // parse precision
            if(c == '.') {
                has_precision = 1;
                precision = 0;
                i++;
                c = fmt[i] & 0xff;

                while(c >= '0' && c <= '9') {
                    precision = precision * 10 + (c - '0');
                    i++;
                    c = fmt[i] & 0xff;
                }
            }

            if(c == 'l') {
                i++;
                c = fmt[i] & 0xff;
            }

            if(c == 'd') {
                printint(putch, ctx, va_arg(ap, int), 10, 1, width, zero_pad, left_justify);
            } else if(c == 'u') {
                printint(putch, ctx, va_arg(ap, int), 10, 0, width, zero_pad, left_justify);
            } else if(c == 'x' || c == 'p') {
                printint(putch, ctx, va_arg(ap, int), 16, 0, width, zero_pad, left_justify);
            } else if(c == 's') {
                s = va_arg(ap, char*);
                if(s == 0) s = "(null)";

                // calculate string length to print
                int len = 0;
                const char *p = s;
                while (*p != 0) {
                    if (has_precision && len >= precision) break;
                    len++;
                    p++;
                }

                int pad = width > len ? width - len : 0;
                if (left_justify) {
                    // left justified: string then spaces
                    for (int j = 0; j < len; j++) {
                        putch(s[j], ctx);
                    }
                    for (int j = 0; j < pad; j++) putch(' ', ctx);
                } else {
                    // right justified: spaces then string
                    for (int j = 0; j < pad; j++) putch(' ', ctx);
                    for (int j = 0; j < len; j++) {
                        putch(s[j], ctx);
                    }
                }
            } else if(c == 'c') {
		// justify
                int pad = width > 1 ? width - 1 : 0;
                if (left_justify) {
                    putch(va_arg(ap, int), ctx);
                    for (int j = 0; j < pad; j++) putch(' ', ctx);
                } else {
                    for (int j = 0; j < pad; j++) putch(' ', ctx);
                    putch(va_arg(ap, int), ctx);
                }
            } else if(c == '%') {
                putch(c, ctx);
            } else {
                putch('%', ctx);
                putch(c, ctx);
            }
            state = 0;
            width = 0;
            zero_pad = 0;
            left_justify = 0;
        }
    }
}

static void
putch_fd(char ch, void *ctx)
{
    int *fd = (int*)ctx;
    putc(*fd, ch);
}

static void
putch_buf(char ch, void *ctx)
{
    char **buf = (char**)ctx;
    *(*buf)++ = ch;
}

void
vprintf(int fd, const char *fmt, va_list ap)
{
    vprintfmt(putch_fd, &fd, fmt, ap);
}

void
vcprintf(const char *fmt, va_list ap)
{
  vprintf(1, fmt, ap);  // FD 1 = console output
}

void
printf(const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  vprintf(1, fmt, ap);
  va_end(ap);
}

void
fprintf(int fd, const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  vprintf(fd, fmt, ap);
  va_end(ap);
}

int
vsprintf(char *buf, const char *fmt, va_list ap)
{
    char *start = buf;
    vprintfmt(putch_buf, &buf, fmt, ap);
    *buf = '\0';
    return buf - start;
}

int
sprintf(char *buf, const char *fmt, ...)
{
    va_list ap;
    int rc;

    va_start(ap, fmt);
    rc = vsprintf(buf, fmt, ap);
    va_end(ap);
    return rc;
}

int
puts(const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  vprintf(1, fmt, ap);
  va_end(ap);
  return 0;
}

int vsscanf(const char *s, const char *fmt, va_list ap) {
  int count = 0;
  const char *p = s;

  while (*fmt) {
    if (*fmt == ' ' || *fmt == '\t' || *fmt == '\n' || *fmt == '\r') {
      while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
      fmt++;
      continue;
    }

    if (*fmt != '%') {
      if (*p == '\0') break;
      if (*p != *fmt) break;
      p++;
      fmt++;
      continue;
    }

    fmt++;  // Skip '%'
    if (*fmt == '\0') break;

    switch (*fmt) {
      case 'd': {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
          p++;
        if (*p == '\0') goto end;

        int sign = 1;
        if (*p == '-') {
          sign = -1;
          p++;
        } else if (*p == '+') {
          p++;
        }

        if (*p < '0' || *p > '9') goto end;

        int num = 0;
        while (*p >= '0' && *p <= '9') {
          num = num * 10 + (*p - '0');
          p++;
        }
        num *= sign;

        int *ip = va_arg(ap, int*);
        *ip = num;
        count++;
        fmt++;
        break;
      }

      case 'x': {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
          p++;
        if (*p == '\0') goto end;

        int num = 0;
        int found = 0;
        while (1) {
          if (*p >= '0' && *p <= '9') {
            num = num * 16 + (*p - '0');
            found = 1;
            p++;
          } else if (*p >= 'a' && *p <= 'f') {
            num = num * 16 + (*p - 'a' + 10);
            found = 1;
            p++;
          } else if (*p >= 'A' && *p <= 'F') {
            num = num * 16 + (*p - 'A' + 10);
            found = 1;
            p++;
          } else {
            break;
          }
        }

        if (!found) goto end;
        int *ip = va_arg(ap, int*);
        *ip = num;
        count++;
        fmt++;
        break;
      }

      case 'c': {
        if (*p == '\0') goto end;
        char *cp = va_arg(ap, char*);
        *cp = *p++;
        count++;
        fmt++;
        break;
      }

      case 's': {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
          p++;
        if (*p == '\0') goto end;

        char *sp = va_arg(ap, char*);
        while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
          *sp++ = *p++;
        }
        *sp = '\0';
        count++;
        fmt++;
        break;
      }

      case '%': {
        if (*p != '%') goto end;
        p++;
        fmt++;
        break;
      }

      default:
        fmt++;  // Skip unsupported specifier
        break;
    }
  }

end:
  return count;
}

int
sscanf(const char *s, const char *fmt, ...)
{
  va_list ap;
  int rc;

  va_start(ap, fmt);
  rc = vsscanf(s, fmt, ap);
  va_end(ap);

  return rc;
}
