/* math.c - 数学运算示例
 *
 * 演示: 循环 / 条件 / 递归 / 整数运算
 *       展示 KexKit 对复杂 C 控制流的支持.
 *
 * 编译: ./kexc examples/math.c examples/math.kex --name math
 * 运行: ./kex_interp examples/math.kex
 */
#include "kex_user.h"

/* 递归计算阶乘 */
long factorial(long n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}

/* 迭代计算斐波那契数 */
long fibonacci(long n) {
    if (n <= 0) return 0;
    if (n <= 2) return 1;
    long a = 1, b = 1, c;
    for (long i = 3; i <= n; i++) {
        c = a + b;
        a = b;
        b = c;
    }
    return b;
}

/* 判断素数 */
long is_prime(long n) {
    if (n < 2) return 0;
    if (n < 4) return 1;       /* 2, 3 */
    if (n % 2 == 0) return 0;
    for (long i = 3; i * i <= n; i += 2) {
        if (n % i == 0) return 0;
    }
    return 1;
}

void _start(void) {
    kex_puts("=== KexKit Math Demo ===");

    /* 阶乘 */
    kex_puts("factorial(5) =");
    kex_printlong(factorial(5), 1);       /* 120 */

    kex_puts("factorial(10) =");
    kex_printlong(factorial(10), 1);      /* 3628800 */

    /* 斐波那契 */
    kex_puts("fibonacci(10) =");
    kex_printlong(fibonacci(10), 1);      /* 55 */

    kex_puts("fibonacci(20) =");
    kex_printlong(fibonacci(20), 1);      /* 6765 */

    /* 素数检测 */
    kex_puts("primes below 30:");
    for (long i = 2; i < 30; i++) {
        if (is_prime(i)) {
            kex_printlong(i, 0);
            kex_putchar(' ');
        }
    }
    kex_putchar('\n');

    /* 最大公约数 (辗转相除) */
    kex_puts("gcd(48, 36) =");
    long a = 48, b = 36;
    while (b != 0) {
        long t = a % b;
        a = b;
        b = t;
    }
    kex_printlong(a, 1);                  /* 12 */

    sys_exit(0);
}
