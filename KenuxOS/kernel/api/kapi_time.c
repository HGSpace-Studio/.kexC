

#include "kapi_time.h"
#include "kapi.h"

#include <arch/hpet.h>
#include <timer.h>

kapi_ktime_t kapi_ktime_get(void)
{
    return (kapi_ktime_t)hpet_get_ticks();
}

int64_t kapi_ktime_get_ns(void)
{
    return (int64_t)hpet_get_ticks();
}

uint64_t kapi_jiffies(void)
{
    return timer_get_jiffies();
}

uint64_t kapi_jiffies_to_ms(uint64_t j)
{
    return timer_jiffies_to_ms(j);
}

uint64_t kapi_ms_to_jiffies(uint64_t ms)
{
    return timer_ms_to_jiffies(ms);
}
