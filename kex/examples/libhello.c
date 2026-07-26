/* libhello.c - 示例 .kxp 动态库
 *
 * 导出三个函数: lib_hello / lib_add / lib_greet
 *   lib_hello: 打印一条消息 (使用 .rodata 字符串)
 *   lib_add:   返回两数之和
 *   lib_greet: 打印带名字的问候
 *
 * 编译: ./kexc examples/libhello.c examples/libhello.kxp --name libhello --kxp
 * 运行: 见 test_kxp.c
 */
#include "kex_user.h"

/* 导出函数: 打印 "from library!" */
void lib_hello(void) {
    const char *msg = "from library!\n";
    sys_write(1, msg, 14);
}

/* 导出函数: 返回 a + b */
long lib_add(long a, long b) {
    return a + b;
}

/* 导出函数: 打印带名字的问候, 演示 .rodata 字符串拼接 */
void lib_greet(const char *name) {
    const char *prefix = "Hello, ";
    const char *suffix = "!\n";
    sys_write(1, prefix, 7);
    sys_write(1, name, kex_strlen(name));
    sys_write(1, suffix, 2);
}
