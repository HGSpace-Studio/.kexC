#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include <stddef.h>

#ifndef __BAREMETAL__
#ifdef __cplusplus
// C++ 模式下不包含 <sys/types.h>, 使用自定义类型避免冲突
#else
#include <sys/types.h>
#endif
#endif

/* Forward declaration so struct fs_mount* in prototypes is the same type
 * as the one defined in fs.h, avoiding "conflicting types" errors. */
struct fs_mount;

#ifndef O_RDONLY
#define O_RDONLY    0x0000
#endif
#ifndef O_WRONLY
#define O_WRONLY    0x0001
#endif
#ifndef O_RDWR
#define O_RDWR      0x0002
#endif
#ifndef O_CREAT
#define O_CREAT     0x0040
#endif
#ifndef O_TRUNC
#define O_TRUNC     0x0200
#endif
#ifndef O_APPEND
#define O_APPEND    0x0400
#endif
#ifndef O_DIRECTORY
#define O_DIRECTORY 0x00200000
#endif

#ifndef S_IFREG
#define S_IFREG     0x8000
#endif
#ifndef S_IFDIR
#define S_IFDIR     0x4000
#endif
#ifndef S_IFLNK
#define S_IFLNK     0xa000
#endif

#ifndef S_IRUSR
#define S_IRUSR     0x0100
#endif
#ifndef S_IWUSR
#define S_IWUSR     0x0080
#endif
#ifndef S_IXUSR
#define S_IXUSR     0x0040
#endif
#ifndef S_IRGRP
#define S_IRGRP     0x0020
#endif
#ifndef S_IWGRP
#define S_IWGRP     0x0010
#endif
#ifndef S_IXGRP
#define S_IXGRP     0x0008
#endif
#ifndef S_IROTH
#define S_IROTH     0x0004
#endif
#ifndef S_IWOTH
#define S_IWOTH     0x0002
#endif
#ifndef S_IXOTH
#define S_IXOTH     0x0001
#endif

#ifndef SEEK_SET
#define SEEK_SET    0
#endif
#ifndef SEEK_CUR
#define SEEK_CUR    1
#endif
#ifndef SEEK_END
#define SEEK_END    2
#endif

#ifndef AT_FDCWD
#define AT_FDCWD    -100
#endif

#ifndef R_OK
#define R_OK        4
#endif
#ifndef W_OK
#define W_OK        2
#endif
#ifndef X_OK
#define X_OK        1
#endif
#ifndef F_OK
#define F_OK        0
#endif

struct fs_stat_buf {
    uint32_t st_mode;
    uint32_t st_nlink;
    uint32_t st_uid;
    uint32_t st_gid;
    uint64_t st_size;
    uint64_t st_atime;
    uint64_t st_mtime;
    uint64_t st_ctime;
};

struct fs_dirent_buf {
    uint64_t d_ino;
    char d_name[256];
};

#ifdef __cplusplus
// C++ 模式: 自定义类型别名, 避免与系统头文件冲突
using fs_mode_t = uint32_t;
using fs_off_t = int64_t;
using fs_ssize_t = int64_t;
#else
typedef uint32_t fs_mode_t;
typedef int64_t fs_off_t;
typedef int64_t fs_ssize_t;
#endif

int sys_open(const char* pathname, int flags, fs_mode_t mode);
int sys_close(int fd);
fs_ssize_t sys_read(int fd, void* buf, size_t count);
fs_ssize_t sys_write(int fd, const void* buf, size_t count);
fs_off_t sys_lseek(int fd, fs_off_t offset, int whence);
int sys_fstat(int fd, struct fs_stat_buf* buf);
int sys_stat(const char* pathname, struct fs_stat_buf* buf);
int sys_mkdir(const char* pathname, fs_mode_t mode);
int sys_mkdirat(int dirfd, const char* pathname, fs_mode_t mode);
int sys_rmdir(const char* pathname);
int sys_unlink(const char* pathname);
int sys_unlinkat(int dirfd, const char* pathname, int flags);
int sys_link(const char* oldpath, const char* newpath);
int sys_symlink(const char* target, const char* linkpath);
int sys_readlink(const char* pathname, char* buf, size_t bufsiz);
int sys_chmod(const char* pathname, fs_mode_t mode);
int sys_fchmod(int fd, fs_mode_t mode);
int sys_truncate(const char* pathname, fs_off_t length);
int sys_ftruncate(int fd, fs_off_t length);
int sys_access(const char* pathname, int mode);
int sys_chdir(const char* path);
int sys_fchdir(int fd);
char* sys_getcwd(char* buf, size_t size);

void sys_set_mount(struct fs_mount* mount);
struct fs_mount* sys_get_mount(void);

#endif
