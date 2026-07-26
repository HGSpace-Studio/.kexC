

#include "kapi_string.h"
#include "kapi.h"

#include <string.h>
#include <stdarg.h>

size_t kapi_strlcpy(char* dst, const char* src, size_t size)
{
    if (!dst || !src) {
        return 0;
    }

    size_t src_len = strlen(src);

    if (size) {
        size_t copy_len = src_len < size - 1 ? src_len : size - 1;
        memcpy(dst, src, copy_len);
        dst[copy_len] = '\0';
    }

    return src_len;
}

size_t kapi_strscpy(char* dst, const char* src, size_t size)
{
    if (!dst || !src || !size) {
        return 0;
    }

    size_t i;
    for (i = 0; i < size - 1 && src[i]; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';

    if (src[i]) {
        return 0;
    }

    return i;
}

int kapi_snprintf(char* str, size_t size, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    int ret = kapi_vsnprintf(str, size, format, args);
    va_end(args);
    return ret;
}

int kapi_vsnprintf(char* str, size_t size, const char* format, va_list args)
{
    if (!str || !format || size == 0) {
        return 0;
    }

    int count = 0;
    while (*format && count < (int)size - 1) {
        if (*format == '%') {
            format++;
            if (*format == 's') {
                const char* s = va_arg(args, const char*);
                while (*s && count < (int)size - 1) {
                    str[count++] = *s++;
                }
            } else if (*format == 'd' || *format == 'i') {
                int n = va_arg(args, int);
                if (n < 0 && count < (int)size - 1) {
                    str[count++] = '-';
                    n = -n;
                }
                char buffer[12];
                int i = 0;
                do {
                    buffer[i++] = '0' + (n % 10);
                    n /= 10;
                } while (n > 0 && i < 11);
                while (i > 0 && count < (int)size - 1) {
                    str[count++] = buffer[--i];
                }
            } else if (*format == 'x' || *format == 'X') {
                unsigned int n = va_arg(args, unsigned int);
                char buffer[16];
                int i = 0;
                do {
                    char digit = n % 16;
                    buffer[i++] = (digit < 10) ? ('0' + digit) : ((*format == 'X') ? ('A' + digit - 10) : ('a' + digit - 10));
                    n /= 16;
                } while (n > 0 && i < 15);
                while (i > 0 && count < (int)size - 1) {
                    str[count++] = buffer[--i];
                }
            } else if (*format == 'c') {
                if (count < (int)size - 1) {
                    str[count++] = (char)va_arg(args, int);
                }
            } else if (*format == '%') {
                if (count < (int)size - 1) {
                    str[count++] = '%';
                }
            }
            format++;
        } else {
            str[count++] = *format++;
        }
    }
    str[count] = '\0';
    return count;
}
