

#ifndef KAPI_TIME_H
#define KAPI_TIME_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int64_t kapi_ktime_t;

kapi_ktime_t kapi_ktime_get(void);

int64_t kapi_ktime_get_ns(void);

uint64_t kapi_jiffies(void);

uint64_t kapi_jiffies_to_ms(uint64_t j);

uint64_t kapi_ms_to_jiffies(uint64_t ms);

#ifdef __cplusplus
}
#endif

#endif
