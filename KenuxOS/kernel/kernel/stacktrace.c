

#include <arch/types.h>
#include <arch/vga.h>
#include <string.h>

uint64_t kernel_symbols_start[2] = {0, 0};
uint64_t kernel_symbols_end[1] = {0};

typedef struct {
    uint64_t addr;
    const char* name;
} ksym_entry_t;

static const char* __addr_to_symbol(uint64_t addr)
{
    if (!kernel_symbols_start || kernel_symbols_start == kernel_symbols_end) {
        return NULL;
    }

    ksym_entry_t* sym = (ksym_entry_t*)kernel_symbols_start;
    ksym_entry_t* end = (ksym_entry_t*)kernel_symbols_end;
    const char* best = NULL;
    uint64_t best_addr = 0;

    while (sym < end) {
        if (sym->addr <= addr && sym->addr > best_addr && sym->name) {
            best_addr = sym->addr;
            best = sym->name;
        }
        sym++;
    }

    return best;
}

static void __print_frame(uint64_t addr, int frame_no)
{
    const char* sym = __addr_to_symbol(addr);

    char buf[32];
    int i = 0;
    buf[i++] = '0';
    buf[i++] = 'x';
    for (int shift = 60; shift >= 0; shift -= 4) {
        int nibble = (int)((addr >> shift) & 0xF);
        buf[i++] = (nibble < 10) ? ('0' + nibble) : ('a' + nibble - 10);
    }
    buf[i] = '\0';

    vga_puts("  [");
    char num[4];
    int n = 0;
    int f = frame_no;
    do {
        num[n++] = '0' + (f % 10);
        f /= 10;
    } while (f > 0);
    while (n-- > 0) vga_putchar(num[n]);
    vga_puts("] ");
    vga_puts(buf);
    if (sym) {
        vga_puts(" <");
        vga_puts(sym);
        vga_puts(">");
    }
    vga_puts("\n");
}

void stack_trace(void)
{
    uint64_t* rbp;
    __asm__ volatile ("movq %%rbp, %0" : "=r"(rbp));

    vga_puts("Call Trace:\n");

    int frame = 0;
    while (rbp && frame < 32) {
        uint64_t ret_addr = *(rbp + 1);
        if (!ret_addr) break;

        __print_frame(ret_addr, frame);

        uint64_t next_rbp = *rbp;
        if (next_rbp <= (uint64_t)rbp) break;
        rbp = (uint64_t*)next_rbp;
        frame++;
    }
}

void panic(const char* msg)
{

    __asm__ volatile ("cli");

    vga_puts("\n\n");
    vga_puts("Kernel panic - not syncing: ");
    vga_puts(msg ? msg : "unknown");
    vga_puts("\n\n");

    stack_trace();

    vga_puts("\nSystem halted.\n");

    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

void __assert_fail(const char* expr, const char* file, int line)
{
    char buf[256];
    int i = 0;
    const char* p = "Assertion failed: ";
    while (*p && i < 255) buf[i++] = *p++;
    p = expr;
    while (*p && i < 255) buf[i++] = *p++;
    p = " at ";
    while (*p && i < 255) buf[i++] = *p++;
    p = file;
    while (*p && i < 255) buf[i++] = *p++;
    if (i < 255) buf[i++] = ':';

    int l = line;
    char num[8];
    int n = 0;
    do {
        num[n++] = '0' + (l % 10);
        l /= 10;
    } while (l > 0);
    while (n-- > 0 && i < 255) buf[i++] = num[n];
    buf[i] = '\0';

    panic(buf);
}
