/* test_debug.c - 调试用最小测试 */
#include "kex_user.h"

void _start(void) {
    /* 测试1: 栈上字符串 */
    char buf[4];
    buf[0] = 'A'; buf[1] = 'B'; buf[2] = 'C'; buf[3] = '\n';
    sys_write(1, buf, 4);

    /* 测试2: .rodata 字符串 */
    kex_puts("TEST_RODATA");

    /* 测试3: 两个 .rodata 字符串 */
    sys_write(1, "first\n", 6);
    sys_write(1, "second\n", 7);

    sys_exit(0);
}
