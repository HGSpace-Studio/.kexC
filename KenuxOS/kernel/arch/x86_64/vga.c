#include <arch/vga.h>
#include <arch/io.h>
#include <arch/keyboard.h>

/* Debug: output a character to COM1 */
#define SERIAL_DBG(c) do { __asm__ volatile ("outb %0, %1" : : "a"((char)(c)), "d"((unsigned short)0x3F8)); } while (0)

uint16_t* vga_buffer = (uint16_t*)0xB8000;
static uint8_t vga_row = 0;
static uint8_t vga_col = 0;
static uint8_t vga_color = 0x07;

void vga_init(void)
{
    SERIAL_DBG('a');
    /* Verify vga_buffer pointer value (output as hex) */
    {
        uint64_t v = (uint64_t)vga_buffer;
        SERIAL_DBG('0');
        SERIAL_DBG('x');
        for (int i = 60; i >= 0; i -= 4) {
            char h = (char)((v >> i) & 0xF);
            SERIAL_DBG(h < 10 ? '0' + h : 'A' + h - 10);
        }
        SERIAL_DBG(' ');
    }
    vga_clear();
    SERIAL_DBG('b');
}

void vga_clear(void)
{
    SERIAL_DBG('c');
    /* Test: write single byte to vga_buffer */
    volatile uint16_t* p = vga_buffer;
    *p = (uint16_t)((vga_color << 8) | 'X');
    SERIAL_DBG('1');
    /* Test: write at offset 0 */
    vga_buffer[0] = (uint16_t)((vga_color << 8) | ' ');
    SERIAL_DBG('2');
    for (uint8_t y = 0; y < VGA_HEIGHT; y++) {
        for (uint8_t x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[y * VGA_WIDTH + x] = (uint16_t)((vga_color << 8) | ' ');
        }
    }
    SERIAL_DBG('d');
}

void vga_putc(char c)
{
    if (c == '\n') {
        vga_row++;
        vga_col = 0;
    } else if (c == '\r') {
        vga_col = 0;
    } else if (c >= 32) {
        vga_buffer[vga_row * VGA_WIDTH + vga_col] = (uint16_t)((vga_color << 8) | c);
        vga_col++;

        if (vga_col >= VGA_WIDTH) {
            vga_row++;
            vga_col = 0;
        }
    }

    if (vga_row >= VGA_HEIGHT) {
        vga_scroll();
        vga_row = VGA_HEIGHT - 1;
    }
}

void vga_print(const char* str)
{
    while (*str) {
        vga_putc(*str++);
    }
}

/* vga_puts 与 vga_print 功能相同 */
void vga_puts(const char* str)
{
    vga_print(str);
}

/* vga_putchar 与 vga_putc 功能相同 */
void vga_putchar(char c)
{
    vga_putc(c);
}

void vga_setcolor(uint8_t fg, uint8_t bg)
{
    vga_color = (bg << 4) | fg;
}

void vga_scroll(void)
{
    for (uint8_t y = 1; y < VGA_HEIGHT; y++) {
        for (uint8_t x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[(y - 1) * VGA_WIDTH + x] = vga_buffer[y * VGA_WIDTH + x];
        }
    }

    for (uint8_t x = 0; x < VGA_WIDTH; x++) {
        vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = (uint16_t)((vga_color << 8) | ' ');
    }
}

char vga_getchar(void)
{
    key_event_t ev = keyboard_read();
    return ev.ascii;
}

void vga_gets(char* buffer, int size)
{
    int i = 0;
    while (i < size - 1) {
        char c = vga_getchar();
        if (c == '\n' || c == '\r') {
            buffer[i] = '\0';
            break;
        } else if (c >= 32) {
            buffer[i++] = c;
            vga_putc(c);
        } else if (c == '\b' && i > 0) {
            i--;
            vga_putc('\b');
        }
    }
}
