#include <arch/vga.h>
#include <arch/drivers.h>
#include <fs.h>
#include <arch/ipc.h>
#include <arch/usermode.h>
#include <arch/shell.h>
#include <arch/pci.h>
#include <arch/acpi.h>
#include <arch/smbios.h>
#include <arch/rtc.h>
#include <arch/pit.h>
#include <arch/pic.h>
#include <arch/idt.h>
#include <arch/gdt.h>
#include <arch/hpet.h>
#include <arch/ahci.h>
#include <arch/sata.h>
#include <arch/ehci.h>
#include <arch/xhci.h>
#include <arch/net.h>
#include <arch/sound.h>
#include <arch/ac97.h>
#include <arch/hda.h>
#include <arch/framebuffer.h>
#include <arch/memory.h>
#include <arch/process.h>
#include <arch/syscall.h>
#include <arch/interrupt.h>
#include <arch/boot.h>
#include <arch/gui.h>
#include <container_gui.h>
#include <string.h>
#include <memory.h>
#include <stdio.h>

#include <kapi.h>

extern void init_main(void);

/* Debug: output a character to COM1 serial port */
static inline void serial_putc(char c)
{
    __asm__ volatile ("outb %0, %1" : : "a"(c), "d"((unsigned short)0x3F8));
}

void init_main(void)
{
    while (1) { }
}

uint64_t kenux_uptime(void)
{
    extern uint64_t timer_get_jiffies(void);
    return timer_get_jiffies();
}

void kernel_main(struct FrameBufferConfig *fbc, struct MemoryMapInfo *mmi)
{
    serial_putc('V');
    framebuffer_init(fbc);

    serial_putc('G');
    gdt_init();
    serial_putc('I');
    idt_init();
    serial_putc('P');
    pic_init();

    serial_putc('M');
    memory_init();

    serial_putc('A');
    acpi_init();
    serial_putc('S');
    smbios_init();

    serial_putc('C');
    pci_init();

    serial_putc('H');
    hpet_init();
    serial_putc('T');
    pit_init();
    serial_putc('R');
    rtc_init();

    serial_putc('O');
    process_init();
    serial_putc('Y');
    syscall_init();
    serial_putc('N');
    interrupt_init();

    serial_putc('F');
    fs_init();
    serial_putc('L');
    ipc_init();

    serial_putc('K');
    vga_print("Initializing KAPI layer...\n");
    if (kapi_init() == KAPI_OK) {
        vga_print("KAPI layer initialized successfully\n");
    } else {
        vga_print("KAPI layer initialization failed!\n");
    }
    serial_putc('U');

    usermode_init();
    serial_putc('E');
    shell_init();
    serial_putc('D');

    drivers_init();
    serial_putc('R');

    drivers_register("PCI", pci_init);
    drivers_register("ACPI", acpi_init);
    drivers_register("SMBIOS", smbios_init);
    drivers_register("RTC", rtc_init);
    drivers_register("PIT", pit_init);
    drivers_register("PIC", pic_init);
    drivers_register("IDT", idt_init);
    drivers_register("GDT", gdt_init);
    drivers_register("HPET", hpet_init);
    drivers_register("AHCI", ahci_init);
    drivers_register("SATA", sata_init);
    drivers_register("EHCI", ehci_init);
    drivers_register("XHCI", xhci_init);
    drivers_register("NETWORK", network_init);
    drivers_register("SOUND", sound_init);
    drivers_register("AC97", ac97_init);
    drivers_register("HDA", hda_init);
    drivers_register("USERMODE", usermode_init);
    drivers_register("SHELL", shell_init);
    serial_putc('B');

    vga_print("========================================\n");
    vga_print("  Kenux Kernel 26.7.9K\n");
    vga_print("  Build: 2026-07-07\n");
    vga_print("  Arch: x86_64\n");
    vga_print("  Scheduler: 5-level priority RR\n");
    vga_print("  VFS: tree-based virtual filesystem\n");
    vga_print("  Syscalls: 1000+ (Linux compatible)\n");
    vga_print("  Build: Ninja\n");
    vga_print("========================================\n");
    vga_print("Memory total: ");
    char buffer[64];
    sprintf(buffer, "%lu KB\n", memory_get_total() / 1024);
    vga_print(buffer);
    sprintf(buffer, "Memory free: %lu KB\n", memory_get_free() / 1024);
    vga_print(buffer);

    vga_print("PCI devices found\n");
    vga_print("ACPI tables found\n");
    vga_print("SMBIOS tables found\n");

    vga_print("Starting drivers\n");
    serial_putc('X');
    drivers_start();
    serial_putc('Y');

    vga_print("Creating processes\n");
    serial_putc('Z');
    process_create("idle", (void*)0x100000, 0);
    serial_putc('1');

    vga_print("Creating init process\n");
    serial_putc('2');
    uint64_t pid;
    usermode_create_process("init", (void*)init_main, &pid);
    serial_putc('3');

    vga_print("Starting kernel\n");
    serial_putc('4');

    /* 启动仿 Windows GUI */
    desktop_init();
    serial_putc('W');

    /* 创建欢迎窗口 (位于中央) */
    static struct window welcome_win;
    window_init(&welcome_win, 312, 278, 400, 200, "Welcome to KenuxK");
    desktop_add_window(&welcome_win);
    serial_putc('a');

    /* 创建 OK 按钮 (在窗口内容区内) */
    static struct widget ok_btn;
    widget_init(&ok_btn, WIDGET_BUTTON, 572, 430, 80, 24);
    widget_set_text(&ok_btn, "OK");
    ok_btn.bg_color = GUI_COLOR_BTN_FACE;
    ok_btn.fg_color = GUI_COLOR_TEXT;
    welcome_win.base.children[welcome_win.base.child_count++] = &ok_btn;
    serial_putc('b');

    /* 创建系统信息窗口 (位于左上角) */
    static struct window info_win;
    window_init(&info_win, 20, 20, 350, 160, "System Information");
    info_win.state = WIN_STATE_INACTIVE;
    desktop_add_window(&info_win);
    serial_putc('c');

    /* 启动容器管理器 GUI */
    container_gui_show();
    serial_putc('X');

    /* 绘制桌面和窗口 */
    serial_putc('d'); /* desktop_run 之前 */
    desktop_run();
    serial_putc('Q');

    /* 在欢迎窗口内容区绘制文字（必须在 desktop_run 之后） */
    gfx_draw_text(332, 320, "KenuxK Operating System",
                  GUI_COLOR_TEXT, GUI_COLOR_WINDOW_BG);
    gfx_draw_text(332, 340, "UEFI Boot + GOP Framebuffer",
                  GUI_COLOR_TEXT, GUI_COLOR_WINDOW_BG);
    gfx_draw_text(332, 360, "GUI: Windows Classic Theme",
                  GUI_COLOR_TEXT, GUI_COLOR_WINDOW_BG);

    /* 在系统信息窗口内容区绘制文字 */
    gfx_draw_text(40, 70, "OS:    KenuxK 26.7.9K",
                  GUI_COLOR_TEXT, GUI_COLOR_WINDOW_BG);
    gfx_draw_text(40, 90, "Arch:  x86_64",
                  GUI_COLOR_TEXT, GUI_COLOR_WINDOW_BG);
    gfx_draw_text(40, 110, "Boot:  UEFI (OVMF)",
                  GUI_COLOR_TEXT, GUI_COLOR_WINDOW_BG);
    gfx_draw_text(40, 130, "Video: GOP 1024x768x32",
                  GUI_COLOR_TEXT, GUI_COLOR_WINDOW_BG);
    serial_putc('T');

    shell_run();
    serial_putc('5');

    while (1) {
    }
}