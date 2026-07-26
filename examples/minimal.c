/* minimal.kex - 最小测试: 直接内联 syscall, 不依赖栈数据 */
#include "kex_user.h"

void _start(void) {
    /* 直接用 syscall 写一个字符, 不依赖任何内存数据 */
    long nr = 1;  /* SYS_write */
    long fd = 1;  /* stdout */
    
    /* 把 'A' 放到 rax 寄存器, 再写到栈上一个字节, 然后传地址 */
    char c = 'A';
    sys_write(fd, &c, 1);
    
    char c2 = '\n';
    sys_write(fd, &c2, 1);
    
    sys_exit(0);
}
