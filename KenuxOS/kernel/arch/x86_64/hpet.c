#include <arch/types.h>

typedef struct {
    volatile u64 capabilities;
    volatile u64 reserved1;
    volatile u64 config;
    volatile u64 reserved2;
    volatile u64 interrupt_status;
    volatile u64 reserved3;
    volatile u64 reserved4;
    volatile u64 reserved5;
    volatile u64 main_counter;
    volatile u64 reserved6;
    struct {
        volatile u64 config;
        volatile u64 cmp;
        volatile u64 reserved;
    } timers[32];
} hpet_t;

#include <arch/hpet.h>
#include <arch/memory.h>
#include <string.h>

static hpet_t* hpet = NULL;
static u64 hpet_frequency = 0;

void hpet_init(void)
{
    hpet = (hpet_t*)0xFED00000;

    if (hpet->main_counter == 0) {
        hpet->config = 0;
        hpet->main_counter = 0;
        hpet->config = 1;
    }

    hpet_frequency = hpet->capabilities & 0x1FFFFFFF;
}

u64 hpet_get_ticks(void)
{
    return hpet->main_counter;
}

u64 hpet_get_frequency(void)
{
    return hpet_frequency;
}

void hpet_sleep(u64 ms)
{
    u64 ticks = hpet_frequency * ms / 1000;
    u64 start = hpet_get_ticks();
    while (hpet_get_ticks() - start < ticks) {
    }
}

void hpet_set_periodic(u32 hz)
{
    u64 period = hpet_frequency / hz;
    hpet->timers[0].config = 0;
    hpet->timers[0].cmp = hpet->main_counter + period;
    hpet->timers[0].config = 0x4;
}
