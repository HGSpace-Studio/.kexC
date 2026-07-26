#ifndef ARCH_X86_64_INTERRUPT_H
#define ARCH_X86_64_INTERRUPT_H

#include <arch/types.h>

void interrupt_init(void);
void interrupt_register(uint8_t irq, void* handler);
void interrupt_handler(uint64_t irq, uint64_t error_code);

#endif
