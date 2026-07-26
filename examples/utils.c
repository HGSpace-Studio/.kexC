/* utils.c - 便利工具函数示例
 *
 * 演示: kex_puts / kex_printlong / kex_printhex / kex_strlen
 *       展示 KexKit 用户库的便利输出函数.
 *
 * 编译: ./kexc examples/utils.c examples/utils.kex --name utils
 * 运行: ./kex_interp examples/utils.kex
 */
#include "kex_user.h"

void _start(void) {
    const char *title = "=== KexKit Utility Functions ===\n";
    sys_write(1, title, kex_strlen(title));

    /* 字符串长度 */
    const char *s = "hello";
    kex_puts("kex_strlen(\"hello\") =");
    kex_printlong((long)kex_strlen(s), 1);

    /* 十进制输出 */
    kex_puts("decimal 42:");
    kex_printlong(42, 1);

    kex_puts("decimal -123:");
    kex_printlong(-123, 1);

    /* 十六进制输出 */
    kex_puts("hex 0xdeadbeef:");
    kex_printhex(0xDEADBEEFu, 1);

    kex_puts("hex 0x0:");
    kex_printhex(0, 1);

    /* 内存操作 */
    char buf[16];
    kex_memset(buf, 'A', 5);
    buf[5] = '\n';
    sys_write(1, buf, 6);

    /* 进程信息 */
    kex_puts("PID:");
    kex_printlong(sys_getpid(), 1);

    kex_puts("UID:");
    kex_printlong(sys_getuid(), 1);

    sys_exit(0);
}
