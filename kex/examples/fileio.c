/* fileio.c - 文件 I/O 示例
 *
 * 演示: sys_open / sys_write / sys_read / sys_close
 *       创建文件 -> 写入 -> 读取 -> 输出 -> 关闭
 *
 * 编译: ./kexc examples/fileio.c examples/fileio.kex --name fileio
 * 运行: ./kex_interp examples/fileio.kex
 */
#include "kex_user.h"

/* O_WRONLY | O_CREAT | O_TRUNC = 0x241, O_RDONLY = 0 */
#define O_WRONLY_CREAT_TRUNC 0x241
#define O_RDONLY_FLAG        0
#define FILE_MODE            0644

void _start(void) {
    const char *path = "/tmp/kex_test.txt";
    const char *write_msg = "KexKit file I/O works!\n";

    /* 1. 创建并写文件 */
    long fd = sys_open(path, O_WRONLY_CREAT_TRUNC, FILE_MODE);
    if (fd < 0) {
        const char *err = "open(write) failed\n";
        sys_write(2, err, 19);
        sys_exit(1);
    }
    sys_write((int)fd, write_msg, 22);
    sys_close((int)fd);

    /* 2. 重新打开并读回 */
    fd = sys_open(path, O_RDONLY_FLAG, 0);
    if (fd < 0) {
        const char *err = "open(read) failed\n";
        sys_write(2, err, 18);
        sys_exit(1);
    }

    char buf[64];
    long n = sys_read((int)fd, buf, sizeof(buf));
    sys_close((int)fd);

    /* 3. 输出到 stdout */
    if (n > 0) {
        sys_write(1, buf, (size_t)n);
    }

    /* 4. 删除测试文件 */
    sys_unlink(path);

    sys_exit(0);
}
