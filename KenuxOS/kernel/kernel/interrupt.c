#include <interrupt.h>
#include <string.h>
#include <arch/io.h>
#include <arch/apic.h>

static void* interrupt_handlers[256];

void interrupt_init(void)
{
    memset(interrupt_handlers, 0, sizeof(interrupt_handlers));
}

void interrupt_register(uint8_t irq, void* handler)
{
    if (irq < 256) {
        interrupt_handlers[irq] = handler;
    }
}

void interrupt_dispatch(void)
{
    uint8_t irq = 0xFF;

    outb(0x20, 0x0B);
    uint8_t isr1 = inb(0x20);
    if (isr1) {
        irq = 0;
        while ((isr1 & 1) == 0) {
            isr1 >>= 1;
            irq++;
        }
    } else {
        outb(0xA0, 0x0B);
        uint8_t isr2 = inb(0xA0);
        if (isr2) {
            irq = 8;
            while ((isr2 & 1) == 0) {
                isr2 >>= 1;
                irq++;
            }
        }
    }

    if (irq != 0xFF) {
        void (*handler)(void) = (void (*)(void))interrupt_handlers[irq];
        if (handler) {
            handler();
        }
    }

    if (irq == 0 || irq == 0xFF) {
        extern void timer_tick(void);
        timer_tick();
    }

    if (irq >= 8) {
        outb(0xA0, 0x20);
    }
    outb(0x20, 0x20);

    lapic_eoi();
}
