#include "kapi.h"

/* Debug: output a character to COM1 */
#define SERIAL_DBG(c) do { __asm__ volatile ("outb %0, %1" : : "a"((char)(c)), "d"((unsigned short)0x3F8)); } while (0)

extern int kapi_process_init(void);
extern int kapi_memory_init(void);
extern int kapi_fs_init(void);
extern int kapi_device_init(void);
extern int kapi_syscall_init(void);
extern int kapi_crc_init(void);
extern int kapi_irq_init(void);
extern int kapi_io_init(void);
extern int kapi_pci_init(void);
extern int kapi_dma_init(void);
extern int kapi_security_init(void);
extern int kapi_skbuff_init(void);
extern int kapi_netdevice_init(void);
extern int kapi_socket_init(void);
extern int kapi_netlink_init(void);
extern int kapi_vfs_init(void);
extern int kapi_seq_file_init(void);
extern int kapi_debugfs_init(void);
extern int kapi_mempool_init(void);
extern int kapi_ftrace_init(void);

int kapi_init(void)
{
    int ret;

    SERIAL_DBG('1');
    ret = kapi_process_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }
    SERIAL_DBG('2');
    ret = kapi_memory_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }
    SERIAL_DBG('3');
    ret = kapi_fs_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }
    SERIAL_DBG('4');
    ret = kapi_device_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }
    SERIAL_DBG('5');
    ret = kapi_syscall_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }

    SERIAL_DBG('6');
    ret = kapi_crc_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }
    SERIAL_DBG('7');
    ret = kapi_irq_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }
    SERIAL_DBG('8');
    ret = kapi_io_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }
    SERIAL_DBG('9');
    ret = kapi_pci_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }
    SERIAL_DBG('A');
    ret = kapi_dma_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }
    SERIAL_DBG('B');
    ret = kapi_security_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }

    SERIAL_DBG('C');
    ret = kapi_skbuff_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }
    SERIAL_DBG('D');
    ret = kapi_netdevice_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }
    SERIAL_DBG('E');
    ret = kapi_socket_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }
    SERIAL_DBG('F');
    ret = kapi_netlink_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }

    SERIAL_DBG('G');
    ret = kapi_vfs_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }
    SERIAL_DBG('H');
    ret = kapi_seq_file_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }
    SERIAL_DBG('I');
    ret = kapi_debugfs_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }
    SERIAL_DBG('J');
    ret = kapi_mempool_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }
    SERIAL_DBG('K');
    ret = kapi_ftrace_init();
    if (ret != KAPI_OK) { SERIAL_DBG('!'); return ret; }

    SERIAL_DBG('Z');
    return KAPI_OK;
}
