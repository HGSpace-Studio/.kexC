

#ifndef KAPI_STRING_H
#define KAPI_STRING_H

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

size_t kapi_strlcpy(char* dst, const char* src, size_t size);

size_t kapi_strscpy(char* dst, const char* src, size_t size);

int kapi_snprintf(char* str, size_t size, const char* format, ...);

int kapi_vsnprintf(char* str, size_t size, const char* format, va_list args);

#ifdef __cplusplus
}
#endif

#endif
