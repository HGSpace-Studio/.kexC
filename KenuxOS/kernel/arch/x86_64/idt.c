#include <arch/idt.h>
#include <arch/gdt.h>
#include <arch/interrupt.h>
#include <string.h>

void syscall_handler(void);
void page_fault_handler(void);

extern void usermode_syscall_entry(void);
extern void usermode_page_fault_entry(void);

static idt_entry_t idt[IDT_ENTRIES];
static idt_register_t idt_register;

void idt_init(void)
{
    idt_register.limit = (sizeof(idt_entry_t) * IDT_ENTRIES) - 1;
    idt_register.base = (uint64_t)idt;

    memset(idt, 0, sizeof(idt));

    idt_set_gate(0x20, (uint64_t)interrupt_handler, 0x08, 0x8E, 0);
    idt_set_gate(0x80, (uint64_t)usermode_syscall_entry, 0x1B, 0x8E, 0);
    idt_set_gate(0x14, (uint64_t)usermode_page_fault_entry, 0x08, 0x8E, 0);

    idt_flush((uint64_t)&idt_register);
}

void idt_set_gate(uint8_t index, uint64_t handler, uint16_t selector, uint8_t type_attr, uint8_t ist)
{
    idt[index].base_low = handler & 0xFFFF;
    idt[index].base_mid = (handler >> 16) & 0xFFFF;
    idt[index].base_high = (handler >> 32) & 0xFFFFFFFF;
    idt[index].zero = 0;
    idt[index].selector = selector;
    idt[index].ist = ist;
    idt[index].type_attr = type_attr;
}

void idt_flush(uint64_t addr)
{
    __asm__ volatile ("lidt (%0)" :: "r" (addr));
}
