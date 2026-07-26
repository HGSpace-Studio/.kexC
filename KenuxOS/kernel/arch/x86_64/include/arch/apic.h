

#ifndef ARCH_APIC_H
#define ARCH_APIC_H

#include <arch/types.h>

#define LAPIC_BASE_DEFAULT   0xFEE00000ULL

#define LAPIC_ID             0x020
#define LAPIC_VER            0x030

#define LAPIC_TPR            0x080

#define LAPIC_EOI            0x0B0

#define LAPIC_SVR            0x0F0

#define LAPIC_ESR            0x280

#define LAPIC_ICR0           0x300
#define LAPIC_ICR1           0x310

#define LAPIC_LVT_TIMER      0x320
#define LAPIC_LVT_THERMAL    0x330
#define LAPIC_LVT_PERF       0x340
#define LAPIC_LVT_LINT0      0x350
#define LAPIC_LVT_LINT1      0x360
#define LAPIC_LVT_ERROR      0x370

#define LAPIC_TIMER_INIT     0x380
#define LAPIC_TIMER_CURR     0x390
#define LAPIC_TIMER_DIV      0x3E0

#define LAPIC_SVR_ENABLE     0x100

#define LAPIC_TIMER_ONESHOT  0x00000
#define LAPIC_TIMER_PERIODIC 0x20000
#define LAPIC_TIMER_TSCDEAD  0x40000

#define LAPIC_TIMER_MASKED   0x10000

#define LAPIC_ICR_ASSERT     (1u << 14)
#define LAPIC_ICR_EDGE       (0u << 15)
#define LAPIC_ICR_FIXED      0x00000
#define LAPIC_ICR_BUSY       (1u << 12)

void lapic_init(uint64_t base_addr);

uint32_t lapic_read(uint32_t reg);

void lapic_write(uint32_t reg, uint32_t value);

void lapic_eoi(void);

uint32_t lapic_id(void);

void lapic_send_ipi(uint32_t target_apic_id, uint32_t vector);

void lapic_timer_calibrate(void);

void lapic_setup_timer(uint32_t vector, uint32_t ms);

void lapic_lvt_mask(uint32_t reg);

void lapic_lvt_unmask(uint32_t reg);

uint32_t lapic_get_version(void);

#endif
