#ifndef ARCH_X86_64_HPET_H
#define ARCH_X86_64_HPET_H

#include <arch/types.h>

void hpet_init(void);
uint64_t hpet_get_ticks(void);
uint64_t hpet_get_frequency(void);
void hpet_sleep(uint64_t ms);
void hpet_set_periodic(uint32_t hz);

#endif
