#ifndef LINUX_SYSCALL_HANDLER_H
#define LINUX_SYSCALL_HANDLER_H

#include <cstdint>
#include <cstddef>
#include <sys/types.h>
#include <sys/socket.h>

// 前向声明 FsAdapter, 避免循环包含
class FsAdapter;

#ifdef __cplusplus
extern "C" {
#endif

// Linux x86_64 syscall 号常量
enum LinuxSyscallNumbers {
    SYS_read = 0,
    SYS_write = 1,
    SYS_open = 2,
    SYS_close = 3,
    SYS_stat = 4,
    SYS_fstat = 5,
    SYS_lstat = 6,
    SYS_lseek = 8,
    SYS_mmap = 9,
    SYS_munmap = 11,
    SYS_brk = 12,
    SYS_socket = 41,
    SYS_bind = 49,
    SYS_connect = 50,
    SYS_listen = 51,
    SYS_accept = 52,
    SYS_send = 44,
    SYS_recv = 45,
    SYS_fork = 57,
    SYS_execve = 59,
    SYS_exit = 60,
    SYS_waitpid = 61,
    SYS_chdir = 80,
    SYS_mkdir = 83,
    SYS_rmdir = 84,
    SYS_unlink = 87,
    SYS_link = 88,
    SYS_chmod = 90,
    SYS_chown = 92,
    SYS_kill = 62,
    SYS_getpid = 39,
    SYS_getppid = 40,
    SYS_setuid = 105,
    SYS_setgid = 106,
    SYS_sbrk = 13,
};

// syscall 标志常量
#define O_RDONLY        00000000
#define O_WRONLY        00000001
#define O_RDWR          00000002
#define O_CREAT         00000100
#define O_EXCL          00000200
#define O_NOCTTY        00000400
#define O_TRUNC         00001000
#define O_APPEND        00002000
#define O_NONBLOCK      00004000

#define SEEK_SET        0
#define SEEK_CUR        1
#define SEEK_END        2

#define PROT_NONE       0x00
#define PROT_READ       0x01
#define PROT_WRITE      0x02
#define PROT_EXEC       0x04

#define MAP_SHARED      0x01
#define MAP_PRIVATE     0x02
#define MAP_FIXED       0x10
#define MAP_ANONYMOUS   0x20

#define SOCK_STREAM     1
#define SOCK_DGRAM      2
#define SOCK_RAW        3

#define AF_UNIX         1
#define AF_INET         2
#define AF_INET6        10

#define SOL_SOCKET      1

#define SO_REUSEADDR    2
#define SO_KEEPALIVE    9

#ifdef __cplusplus
}
#endif

namespace containeros {

class LinuxSyscallHandler {
public:
    // 绑定容器文件系统: 绑定后文件相关 syscall 将转发到 FsAdapter
    static void bindFilesystem(FsAdapter* fs);
    static bool hasFilesystem();

    // 文件系统调用 (有 FsAdapter 时转发, 否则回退到宿主 syscall)
    static int open(const char* pathname, int flags, mode_t mode);
    static int close(int fd);
    static ssize_t read(int fd, void* buf, size_t count);
    static ssize_t write(int fd, const void* buf, size_t count);
    static off_t lseek(int fd, off_t offset, int whence);
    static int stat(const char* pathname, struct stat* buf);
    static int fstat(int fd, struct stat* buf);
    static int mkdir(const char* pathname, mode_t mode);
    static int rmdir(const char* pathname);
    static int unlink(const char* pathname);
    static int link(const char* oldpath, const char* newpath);
    static int chmod(const char* pathname, mode_t mode);
    static int chown(const char* pathname, uid_t owner, gid_t group);
    static int access(const char* pathname, int mode);
    static int chdir(const char* path);
    static char* getcwd(char* buf, size_t size);
    static int truncate(const char* path, off_t length);
    static int ftruncate(int fd, off_t length);

    // 进程系统调用 (暂用宿主 syscall)
    static pid_t fork();
    static int execve(const char* filename, char* const argv[], char* const envp[]);
    static pid_t waitpid(pid_t pid, int* wstatus, int options);
    static int kill(pid_t pid, int sig);
    static void exit(int status);
    static pid_t getpid();
    static pid_t getppid();
    static int setuid(uid_t uid);
    static int setgid(gid_t gid);

    // 内存系统调用 (暂用宿主 syscall)
    static void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset);
    static int munmap(void* addr, size_t length);
    static int brk(void* addr);
    static void* sbrk(intptr_t increment);

    // 网络系统调用 (暂用宿主 syscall)
    static int socket(int domain, int type, int protocol);
    static int bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen);
    static int listen(int sockfd, int backlog);
    static int accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen);
    static int connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen);
    static ssize_t send(int sockfd, const void* buf, size_t len, int flags);
    static ssize_t recv(int sockfd, void* buf, size_t len, int flags);

private:
    // 绑定的容器文件系统实例 (nullptr 时回退到宿主 syscall)
    static FsAdapter* fs_adapter_;

    // x86_64 Linux syscall 封装
    static uint64_t syscall64(uint64_t number, uint64_t arg1 = 0, uint64_t arg2 = 0,
                              uint64_t arg3 = 0, uint64_t arg4 = 0, uint64_t arg5 = 0,
                              uint64_t arg6 = 0);
};

} // namespace containeros

#endif // LINUX_SYSCALL_HANDLER_H