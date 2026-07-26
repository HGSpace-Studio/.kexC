/* hello.kex - KexKit 第一个示例程序
 *
 * 演示: .rodata 字符串字面量 + sys_write / sys_exit.
 * kexc 会把字符串放入 .rodata 段, 解释器加载时自动重定位.
 */
#include "kex_user.h"

void _start(void) {
    /* .rodata 字符串字面量, 编译器放入只读段, 加载时自动重定位 */
    const char *msg = "Hello, KenuxOS!\n";
    const char *msg2 = "syscall works!\n";

    sys_write(1, msg, 16);
    sys_write(1, msg2, 15);
    sys_exit(0);
}
