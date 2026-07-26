#include <arch/uart.h>
#include <arch/io.h>
#include <arch/spinlock.h>
#include <arch/idt.h>
#include <arch/interrupt.h>
#include <string.h>

static uart_port_t uart_ports[UART_PORT_MAX];
static spinlock_t uart_locks[UART_PORT_MAX];
static uart_port_enum_t uart_default_port = UART_COM1;
static uint16_t uart_port_bases[UART_PORT_MAX] = {
    UART_COM1_BASE, UART_COM2_BASE, UART_COM3_BASE, UART_COM4_BASE
};
static uint8_t uart_port_irqs[UART_PORT_MAX] = {
    UART_COM1_IRQ, UART_COM2_IRQ, UART_COM3_IRQ, UART_COM4_IRQ
};

static inline uint8_t uart_read_reg(uint16_t base, uint8_t reg)
{
    return inb(base + reg);
}

static inline void uart_write_reg(uint16_t base, uint8_t reg, uint8_t value)
{
    outb(base + reg, value);
}

static int uart_buffer_put(uart_buffer_t* buf, uint8_t data)
{
    if (buf->count >= UART_BUFFER_SIZE) return -1;
    buf->data[buf->head] = data;
    buf->head = (buf->head + 1) % UART_BUFFER_SIZE;
    buf->count++;
    return 0;
}

static int uart_buffer_get(uart_buffer_t* buf, uint8_t* data)
{
    if (buf->count == 0) return -1;
    *data = buf->data[buf->tail];
    buf->tail = (buf->tail + 1) % UART_BUFFER_SIZE;
    buf->count--;
    return 0;
}

int uart_detect(uint16_t base)
{
    uint8_t scratch;

    uart_write_reg(base, UART_REG_FCR, UART_FCR_ENABLE | UART_FCR_RX_CLR | UART_FCR_TX_CLR);

    scratch = uart_read_reg(base, UART_REG_IER);
    uart_write_reg(base, UART_REG_IER, 0x00);
    if (uart_read_reg(base, UART_REG_IER) != 0x00) return 0;

    uart_write_reg(base, UART_REG_IER, 0x0F);
    if (uart_read_reg(base, UART_REG_IER) != 0x0F) return 0;

    uart_write_reg(base, UART_REG_IER, scratch);

    uart_write_reg(base, UART_REG_LCR, UART_LCR_DLAB);

    scratch = uart_read_reg(base, UART_REG_DLL);
    uart_write_reg(base, UART_REG_DLL, 0xAA);
    if (uart_read_reg(base, UART_REG_DLL) != 0xAA) return 0;
    uart_write_reg(base, UART_REG_DLL, scratch);

    scratch = uart_read_reg(base, UART_REG_DLH);
    uart_write_reg(base, UART_REG_DLH, 0x55);
    if (uart_read_reg(base, UART_REG_DLH) != 0x55) return 0;
    uart_write_reg(base, UART_REG_DLH, scratch);

    uart_write_reg(base, UART_REG_LCR, 0x00);

    uint8_t saved_fcr = uart_read_reg(base, UART_REG_FCR);
    uart_write_reg(base, UART_REG_FCR, UART_FCR_ENABLE | UART_FCR_14_BYTE);
    if ((uart_read_reg(base, UART_REG_FCR) & 0xC0) != 0xC0) return 0;
    uart_write_reg(base, UART_REG_FCR, saved_fcr);

    uart_write_reg(base, UART_REG_MCR, UART_MCR_RTS | UART_MCR_OUT2 | UART_MCR_LOOP);
    uint8_t msr = uart_read_reg(base, UART_REG_MSR);
    if ((msr & 0x10) == 0 || (msr & 0xB0) != 0xB0) return 0;

    uart_write_reg(base, UART_REG_MCR, UART_MCR_RTS | UART_MCR_OUT2);
    msr = uart_read_reg(base, UART_REG_MSR);
    if ((msr & 0xB0) != 0x80) return 0;

    scratch = uart_read_reg(base, UART_REG_SCR);
    uart_write_reg(base, UART_REG_SCR, 0x5A);
    if (uart_read_reg(base, UART_REG_SCR) != 0x5A) return 0;
    uart_write_reg(base, UART_REG_SCR, scratch);

    return 1;
}

int uart_set_baud(uart_port_enum_t port, uint32_t baud)
{
    if (port >= UART_PORT_MAX || !uart_ports[port].initialized) return -1;
    if (baud == 0) return -1;

    uint16_t divisor = (uint16_t)(UART_BAUD_BASE / baud);
    uint16_t base = uart_ports[port].base;

    uint8_t lcr = uart_read_reg(base, UART_REG_LCR);
    uart_write_reg(base, UART_REG_LCR, lcr | UART_LCR_DLAB);
    uart_write_reg(base, UART_REG_DLL, (uint8_t)(divisor & 0xFF));
    uart_write_reg(base, UART_REG_DLH, (uint8_t)((divisor >> 8) & 0xFF));
    uart_write_reg(base, UART_REG_LCR, lcr);

    uart_ports[port].baud = baud;
    return 0;
}

void uart_set_flow_control(uart_port_enum_t port, int enable)
{
    if (port >= UART_PORT_MAX || !uart_ports[port].initialized) return;
    uint16_t base = uart_ports[port].base;
    uint8_t mcr;

    if (enable) {
        mcr = uart_read_reg(base, UART_REG_MCR);
        mcr |= UART_MCR_RTS;
        uart_write_reg(base, UART_REG_MCR, mcr);
        mcr = uart_read_reg(base, UART_REG_MCR);
        mcr &= ~UART_MCR_LOOP;
        uart_write_reg(base, UART_REG_MCR, mcr);
        uart_ports[port].hw_flow_control = 1;
    } else {
        mcr = uart_read_reg(base, UART_REG_MCR);
        mcr &= ~UART_MCR_RTS;
        uart_write_reg(base, UART_REG_MCR, mcr);
        uart_ports[port].hw_flow_control = 0;
    }
}

int uart_init_port(uart_port_enum_t port, uint32_t baud)
{
    if (port >= UART_PORT_MAX) return -1;

    uint16_t base = uart_port_bases[port];
    if (!uart_detect(base)) return -1;

    uart_ports[port].base = base;
    uart_ports[port].irq = uart_port_irqs[port];
    uart_ports[port].baud = baud;
    uart_ports[port].data_bits = 8;
    uart_ports[port].stop_bits = 1;
    uart_ports[port].parity = UART_LCR_NO_PARITY;
    uart_ports[port].fcr_trigger = UART_FCR_14_BYTE;
    uart_ports[port].irq_enabled = 0;
    uart_ports[port].hw_flow_control = 0;

    memset(&uart_ports[port].rx_buffer, 0, sizeof(uart_buffer_t));
    memset(&uart_ports[port].tx_buffer, 0, sizeof(uart_buffer_t));
    spin_init(&uart_locks[port]);

    uart_write_reg(base, UART_REG_IER, 0x00);

    uart_write_reg(base, UART_REG_FCR, UART_FCR_ENABLE | UART_FCR_RX_CLR | UART_FCR_TX_CLR | UART_FCR_14_BYTE);

    uint8_t lcr = UART_LCR_8_BITS | UART_LCR_1_STOP | UART_LCR_NO_PARITY;
    uart_write_reg(base, UART_REG_LCR, lcr);

    uart_set_baud(port, baud);

    uart_write_reg(base, UART_REG_MCR, UART_MCR_OUT2 | UART_MCR_RTS | UART_MCR_DTR);

    (void)uart_read_reg(base, UART_REG_LSR);
    (void)uart_read_reg(base, UART_REG_IIR);
    (void)uart_read_reg(base, UART_REG_MSR);
    (void)uart_read_reg(base, UART_REG_DATA);

    uart_ports[port].initialized = 1;
    return 0;
}

void uart_init(void)
{
    for (int i = 0; i < UART_PORT_MAX; i++) {
        uart_ports[i].initialized = 0;
        spin_init(&uart_locks[i]);
    }

    if (uart_init_port(UART_COM1, 115200) == 0) {
        uart_default_port = UART_COM1;
    } else if (uart_init_port(UART_COM2, 115200) == 0) {
        uart_default_port = UART_COM2;
    }
}

void uart_init_default(void)
{
    uart_init();
}

void uart_putc(uart_port_enum_t port, char c)
{
    if (port >= UART_PORT_MAX || !uart_ports[port].initialized) return;
    uint16_t base = uart_ports[port].base;

    if (uart_ports[port].hw_flow_control) {
        uint32_t timeout = UART_DETECT_TIMEOUT;
        while (!(uart_read_reg(base, UART_REG_MSR) & UART_MSR_CTS) && --timeout) {
            __asm__ volatile("pause");
        }
    }

    uint32_t timeout = UART_DETECT_TIMEOUT;
    while (!(uart_read_reg(base, UART_REG_LSR) & UART_LSR_THRE) && --timeout) {
        __asm__ volatile("pause");
    }

    uart_write_reg(base, UART_REG_DATA, (uint8_t)c);

    timeout = UART_DETECT_TIMEOUT;
    while (!(uart_read_reg(base, UART_REG_LSR) & UART_LSR_TEMT) && --timeout) {
        __asm__ volatile("pause");
    }
}

char uart_getc(uart_port_enum_t port)
{
    if (port >= UART_PORT_MAX || !uart_ports[port].initialized) return 0;

    while (1) {
        if (uart_buffer_get(&uart_ports[port].rx_buffer, (uint8_t*)&port) == 0) {
            break;
        }
        uint16_t base = uart_ports[port].base;
        if (uart_read_reg(base, UART_REG_LSR) & UART_LSR_DR) {
            uint8_t ch = uart_read_reg(base, UART_REG_DATA);
            return (char)ch;
        }
        __asm__ volatile("hlt");
    }
    return 0;
}

int uart_getc_timeout(uart_port_enum_t port, uint32_t timeout_us)
{
    if (port >= UART_PORT_MAX || !uart_ports[port].initialized) return -1;

    while (timeout_us--) {
        uint8_t ch;
        if (uart_buffer_get(&uart_ports[port].rx_buffer, &ch) == 0) {
            return (int)ch;
        }
        uint16_t base = uart_ports[port].base;
        if (uart_read_reg(base, UART_REG_LSR) & UART_LSR_DR) {
            uint8_t c = uart_read_reg(base, UART_REG_DATA);
            return (int)c;
        }
        for (volatile int d = 0; d < 10; d++);
    }
    return -1;
}

void uart_puts(uart_port_enum_t port, const char* s)
{
    if (!s) return;
    while (*s) {
        uart_putc(port, *s++);
    }
}

int uart_rx_available(uart_port_enum_t port)
{
    if (port >= UART_PORT_MAX || !uart_ports[port].initialized) return 0;
    spin_lock(&uart_locks[port]);
    int count = (int)uart_ports[port].rx_buffer.count;
    spin_unlock(&uart_locks[port]);
    return count;
}

void uart_flush(uart_port_enum_t port)
{
    if (port >= UART_PORT_MAX || !uart_ports[port].initialized) return;
    spin_lock(&uart_locks[port]);
    uart_ports[port].rx_buffer.head = 0;
    uart_ports[port].rx_buffer.tail = 0;
    uart_ports[port].rx_buffer.count = 0;
    uart_ports[port].tx_buffer.head = 0;
    uart_ports[port].tx_buffer.tail = 0;
    uart_ports[port].tx_buffer.count = 0;
    spin_unlock(&uart_locks[port]);
}

void uart_irq_handler(uart_port_enum_t port)
{
    if (port >= UART_PORT_MAX || !uart_ports[port].initialized) return;

    uint16_t base = uart_ports[port].base;

    while ((uart_read_reg(base, UART_REG_IIR) & UART_IIR_NO_INT) == 0) {
        uint8_t iir = uart_read_reg(base, UART_REG_IIR);
        uint8_t id = iir & UART_IIR_ID_MASK;

        if (id == UART_IIR_RDAI || id == UART_IIR_CTOI) {
            while (uart_read_reg(base, UART_REG_LSR) & UART_LSR_DR) {
                uint8_t ch = uart_read_reg(base, UART_REG_DATA);
                uart_read_reg(base, UART_REG_LSR);
                spin_lock(&uart_locks[port]);
                uart_buffer_put(&uart_ports[port].rx_buffer, ch);
                spin_unlock(&uart_locks[port]);
            }
        } else if (id == UART_IIR_THRI) {
            spin_lock(&uart_locks[port]);
            while (uart_ports[port].tx_buffer.count > 0) {
                if (!(uart_read_reg(base, UART_REG_LSR) & UART_LSR_THRE)) break;
                uint8_t ch;
                uart_buffer_get(&uart_ports[port].tx_buffer, &ch);
                uart_write_reg(base, UART_REG_DATA, ch);
            }
            if (uart_ports[port].tx_buffer.count == 0) {
                uint8_t ier = uart_read_reg(base, UART_REG_IER);
                uart_write_reg(base, UART_REG_IER, ier & ~UART_IER_THRI);
            }
            spin_unlock(&uart_locks[port]);
        } else if (id == UART_IIR_RLSI) {
            (void)uart_read_reg(base, UART_REG_LSR);
            (void)uart_read_reg(base, UART_REG_DATA);
        } else if (id == UART_IIR_MSI) {
            (void)uart_read_reg(base, UART_REG_MSR);
            if (uart_ports[port].hw_flow_control) {
                uint8_t msr = uart_read_reg(base, UART_REG_MSR);
                uint8_t mcr = uart_read_reg(base, UART_REG_MCR);
                if (msr & UART_MSR_CTS) {
                    mcr |= UART_MCR_RTS;
                } else {
                    mcr &= ~UART_MCR_RTS;
                }
                uart_write_reg(base, UART_REG_MCR, mcr);
            }
        } else {
            break;
        }
    }
}

static void uart_com1_irq_stub(void)
{
    uart_irq_handler(UART_COM1);
}

static void uart_com2_irq_stub(void)
{
    uart_irq_handler(UART_COM2);
}

static void uart_com3_irq_stub(void)
{
    uart_irq_handler(UART_COM3);
}

static void uart_com4_irq_stub(void)
{
    uart_irq_handler(UART_COM4);
}

static void (*uart_irq_stubs[UART_PORT_MAX])(void) = {
    uart_com1_irq_stub,
    uart_com2_irq_stub,
    uart_com3_irq_stub,
    uart_com4_irq_stub
};

int uart_irq_register(uart_port_enum_t port)
{
    if (port >= UART_PORT_MAX || !uart_ports[port].initialized) return -1;

    uint8_t irq = uart_ports[port].irq;
    interrupt_register(irq, (void*)uart_irq_stubs[port]);

    uint16_t base = uart_ports[port].base;
    uint8_t ier = uart_read_reg(base, UART_REG_IER);
    ier |= UART_IER_RDAI | UART_IER_RLSI | UART_IER_MSI;
    uart_write_reg(base, UART_REG_IER, ier);

    uart_ports[port].irq_enabled = 1;
    return 0;
}

void uart_enable_interrupts(uart_port_enum_t port)
{
    if (port >= UART_PORT_MAX || !uart_ports[port].initialized) return;
    uint16_t base = uart_ports[port].base;
    uint8_t ier = uart_read_reg(base, UART_REG_IER);
    ier |= UART_IER_RDAI | UART_IER_THRI | UART_IER_RLSI | UART_IER_MSI;
    uart_write_reg(base, UART_REG_IER, ier);
    uart_ports[port].irq_enabled = 1;
}

void uart_disable_interrupts(uart_port_enum_t port)
{
    if (port >= UART_PORT_MAX || !uart_ports[port].initialized) return;
    uint16_t base = uart_ports[port].base;
    uart_write_reg(base, UART_REG_IER, 0x00);
    uart_ports[port].irq_enabled = 0;
}

void uart_putc_default(char c)
{
    uart_putc(uart_default_port, c);
}

void uart_puts_default(const char* str)
{
    uart_puts(uart_default_port, str);
}

char uart_getc_default(void)
{
    return uart_getc(uart_default_port);
}

int uart_poll_default(char* c)
{
    return uart_getc_timeout(uart_default_port, 0);
}

void uart_irq_handler_default(void)
{
    uart_irq_handler(uart_default_port);
}
