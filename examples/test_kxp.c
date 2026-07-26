/* test_kxp.c - 测试 .kxp 动态库加载
 *
 * 调用 libhello.kxp 导出的 lib_hello() / lib_add() / lib_greet() 函数.
 *
 * 编译: ./kexc examples/test_kxp.c examples/test_kxp.kex --name test_kxp
 * 运行: ./kex_interp -Lexamples examples/test_kxp.kex
 */
#include "kex_user.h"

/* 声明外部库函数 (由 .kxp 提供) */
void lib_hello(void);
long lib_add(long a, long b);
void lib_greet(const char *name);

void _start(void) {
    /* 调用库函数: 打印消息 */
    lib_hello();

    /* 调用库函数: 计算并打印 3 + 4 = 7 */
    long result = lib_add(3, 4);

    /* 打印 "3+4=" 前缀 (.rodata 字符串) */
    const char *prefix = "3+4=";
    sys_write(1, prefix, 4);

    /* 把数字转成字符输出 (个位数) */
    char c = '0' + (char)(result % 10);
    char nl = '\n';
    sys_write(1, &c, 1);
    sys_write(1, &nl, 1);

    /* 调用带参数的库函数 */
    lib_greet("Kenux");

    sys_exit(0);
}
