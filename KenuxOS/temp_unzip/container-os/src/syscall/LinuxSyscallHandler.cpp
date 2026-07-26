#include "syscall/LinuxSyscallHandler.h"
#include "storage/fs/FsAdapter.h"

// 用 extern "C" 包含文件系统的 syscall 头文件
extern "C" {
#include "syscall.h"
}

#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>

namespace containeros {

// 静态成员初始化
FsAdapter* LinuxSyscallHandler::fs_adapter_ = nullptr;

void LinuxSyscallHandler::bindFilesystem(FsAdapter* fs) {
    fs_adapter_ = fs;
    if (fs) {
        fs->activate();
    }
}

bool LinuxSyscallHandler::hasFilesystem() {
    return fs_adapter_ != nullptr;
}

// TODO: 在自定义操作系统上，需要实现底层 syscall 入口
// 当前实现基于 Linux x86_64 系统调用约定
// Linux x86_64 syscall 约定:
//   rax = syscall_number
//   rdi, rsi, rdx, r10, r8, r9 依次传递前 6 个参数
//   返回值: rax，负数表示错误码(-errno)

// x86_64 Linux syscall 封装实现
uint64_t LinuxSyscallHandler::syscall64(uint64_t number, uint64_t arg1, uint64_t arg2,
                                        uint64_t arg3, uint64_t arg4, uint64_t arg5,
                                        uint64_t arg6) {
    uint64_t result;

#if defined(__GNUC__) || defined(__clang__)
    // GCC/Clang 内联汇编实现
    __asm__ volatile (
        "syscall"
        : "=a"(result)
        : "a"(number), "D"(arg1), "S"(arg2), "d"(arg3), "r"(arg4), "r"(arg5), "r"(arg6)
        : "rcx", "r11", "memory"
    );
#else
    // TODO: 其他编译器平台的 syscall 实现
    // 在自定义操作系统上，这里需要调用内核的 syscall 入口
    result = 0;
#endif

    return result;
}

// ============================================
// 文件系统调用实现
// 有 FsAdapter 时转发到容器文件系统, 否则回退到宿主 syscall
// ============================================

int LinuxSyscallHandler::open(const char* pathname, int flags, mode_t mode) {
    if (fs_adapter_) {
        // 转发到容器文件系统的 sys_open
        return sys_open(pathname, flags, static_cast<fs_mode_t>(mode));
    }
    return static_cast<int>(::open(pathname, flags, mode));
}

int LinuxSyscallHandler::close(int fd) {
    if (fs_adapter_) {
        return sys_close(fd);
    }
    return ::close(fd);
}

ssize_t LinuxSyscallHandler::read(int fd, void* buf, size_t count) {
    if (fs_adapter_) {
        return static_cast<ssize_t>(sys_read(fd, buf, count));
    }
    return ::read(fd, buf, count);
}

ssize_t LinuxSyscallHandler::write(int fd, const void* buf, size_t count) {
    if (fs_adapter_) {
        return static_cast<ssize_t>(sys_write(fd, buf, count));
    }
    return ::write(fd, buf, count);
}

off_t LinuxSyscallHandler::lseek(int fd, off_t offset, int whence) {
    if (fs_adapter_) {
        return static_cast<off_t>(sys_lseek(fd, static_cast<fs_off_t>(offset), whence));
    }
    return ::lseek(fd, offset, whence);
}

int LinuxSyscallHandler::stat(const char* pathname, struct stat* buf) {
    if (fs_adapter_) {
        // 容器文件系统使用自己的 stat 结构, 转换到系统的 struct stat
        fs_stat_buf fs_buf;
        int ret = sys_stat(pathname, &fs_buf);
        if (ret != 0) return -1;
        buf->st_mode = fs_buf.st_mode;
        buf->st_nlink = fs_buf.st_nlink;
        buf->st_uid = fs_buf.st_uid;
        buf->st_gid = fs_buf.st_gid;
        buf->st_size = static_cast<off_t>(fs_buf.st_size);
        buf->st_atime = fs_buf.st_atime;
        buf->st_mtime = fs_buf.st_mtime;
        buf->st_ctime = fs_buf.st_ctime;
        return 0;
    }
    return ::stat(pathname, buf);
}

int LinuxSyscallHandler::fstat(int fd, struct stat* buf) {
    if (fs_adapter_) {
        fs_stat_buf fs_buf;
        int ret = sys_fstat(fd, &fs_buf);
        if (ret != 0) return -1;
        buf->st_mode = fs_buf.st_mode;
        buf->st_nlink = fs_buf.st_nlink;
        buf->st_uid = fs_buf.st_uid;
        buf->st_gid = fs_buf.st_gid;
        buf->st_size = static_cast<off_t>(fs_buf.st_size);
        buf->st_atime = fs_buf.st_atime;
        buf->st_mtime = fs_buf.st_mtime;
        buf->st_ctime = fs_buf.st_ctime;
        return 0;
    }
    return ::fstat(fd, buf);
}

int LinuxSyscallHandler::mkdir(const char* pathname, mode_t mode) {
    if (fs_adapter_) {
        return sys_mkdir(pathname, static_cast<fs_mode_t>(mode));
    }
    return ::mkdir(pathname, mode);
}

int LinuxSyscallHandler::rmdir(const char* pathname) {
    if (fs_adapter_) {
        return sys_rmdir(pathname);
    }
    return ::rmdir(pathname);
}

int LinuxSyscallHandler::unlink(const char* pathname) {
    if (fs_adapter_) {
        return sys_unlink(pathname);
    }
    return ::unlink(pathname);
}

int LinuxSyscallHandler::link(const char* oldpath, const char* newpath) {
    if (fs_adapter_) {
        return sys_link(oldpath, newpath);
    }
    return ::link(oldpath, newpath);
}

int LinuxSyscallHandler::chmod(const char* pathname, mode_t mode) {
    if (fs_adapter_) {
        return sys_chmod(pathname, static_cast<fs_mode_t>(mode));
    }
    return ::chmod(pathname, mode);
}

int LinuxSyscallHandler::chown(const char* pathname, uid_t owner, gid_t group) {
    // 文件系统暂不支持 chown, 有 FsAdapter 时返回成功
    if (fs_adapter_) {
        return 0;
    }
    return ::chown(pathname, owner, group);
}

int LinuxSyscallHandler::access(const char* pathname, int mode) {
    if (fs_adapter_) {
        return sys_access(pathname, mode);
    }
    return ::access(pathname, mode);
}

int LinuxSyscallHandler::chdir(const char* path) {
    if (fs_adapter_) {
        return sys_chdir(path);
    }
    return ::chdir(path);
}

char* LinuxSyscallHandler::getcwd(char* buf, size_t size) {
    if (fs_adapter_) {
        return sys_getcwd(buf, size);
    }
    return ::getcwd(buf, size);
}

int LinuxSyscallHandler::truncate(const char* path, off_t length) {
    if (fs_adapter_) {
        return sys_truncate(path, static_cast<fs_off_t>(length));
    }
    return ::truncate(path, length);
}

int LinuxSyscallHandler::ftruncate(int fd, off_t length) {
    if (fs_adapter_) {
        return sys_ftruncate(fd, static_cast<fs_off_t>(length));
    }
    return ::ftruncate(fd, length);
}

// ============================================
// 进程系统调用实现
// ============================================

pid_t LinuxSyscallHandler::fork() {
    uint64_t result = syscall64(SYS_fork);
    if (result > -4096LL) {
        errno = -static_cast<int>(result);
        return -1;
    }
    return static_cast<pid_t>(result);
}

int LinuxSyscallHandler::execve(const char* filename, char* const argv[], char* const envp[]) {
    uint64_t result = syscall64(SYS_execve, reinterpret_cast<uint64_t>(filename),
                                reinterpret_cast<uint64_t>(argv), reinterpret_cast<uint64_t>(envp));
    if (result > -4096LL) {
        errno = -static_cast<int>(result);
        return -1;
    }
    return static_cast<int>(result);
}

pid_t LinuxSyscallHandler::waitpid(pid_t pid, int* wstatus, int options) {
    uint64_t result = syscall64(SYS_waitpid, pid, reinterpret_cast<uint64_t>(wstatus), options);
    if (result > -4096LL) {
        errno = -static_cast<int>(result);
        return -1;
    }
    return static_cast<pid_t>(result);
}

int LinuxSyscallHandler::kill(pid_t pid, int sig) {
    uint64_t result = syscall64(SYS_kill, pid, sig);
    if (result > -4096LL) {
        errno = -static_cast<int>(result);
        return -1;
    }
    return static_cast<int>(result);
}

void LinuxSyscallHandler::exit(int status) {
    // exit syscall 不会返回
    syscall64(SYS_exit, status);
}

pid_t LinuxSyscallHandler::getpid() {
    uint64_t result = syscall64(SYS_getpid);
    return static_cast<pid_t>(result);
}

pid_t LinuxSyscallHandler::getppid() {
    uint64_t result = syscall64(SYS_getppid);
    return static_cast<pid_t>(result);
}

int LinuxSyscallHandler::setuid(uid_t uid) {
    uint64_t result = syscall64(SYS_setuid, uid);
    if (result > -4096LL) {
        errno = -static_cast<int>(result);
        return -1;
    }
    return static_cast<int>(result);
}

int LinuxSyscallHandler::setgid(gid_t gid) {
    uint64_t result = syscall64(SYS_setgid, gid);
    if (result > -4096LL) {
        errno = -static_cast<int>(result);
        return -1;
    }
    return static_cast<int>(result);
}

// ============================================
// 内存系统调用实现
// ============================================

void* LinuxSyscallHandler::mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset) {
    uint64_t result = syscall64(SYS_mmap, reinterpret_cast<uint64_t>(addr), length, prot,
                                flags, fd, offset);
    if (result > -4096LL) {
        errno = -static_cast<int>(result);
        return reinterpret_cast<void*>(-1);
    }
    return reinterpret_cast<void*>(result);
}

int LinuxSyscallHandler::munmap(void* addr, size_t length) {
    uint64_t result = syscall64(SYS_munmap, reinterpret_cast<uint64_t>(addr), length);
    if (result > -4096LL) {
        errno = -static_cast<int>(result);
        return -1;
    }
    return static_cast<int>(result);
}

int LinuxSyscallHandler::brk(void* addr) {
    uint64_t result = syscall64(SYS_brk, reinterpret_cast<uint64_t>(addr));
    if (result > -4096LL) {
        errno = -static_cast<int>(result);
        return -1;
    }
    return 0;
}

void* LinuxSyscallHandler::sbrk(intptr_t increment) {
    uint64_t result = syscall64(SYS_sbrk, increment);
    if (result > -4096LL) {
        errno = -static_cast<int>(result);
        return reinterpret_cast<void*>(-1);
    }
    return reinterpret_cast<void*>(result);
}

// ============================================
// 网络系统调用实现
// ============================================

int LinuxSyscallHandler::socket(int domain, int type, int protocol) {
    uint64_t result = syscall64(SYS_socket, domain, type, protocol);
    if (result > -4096LL) {
        errno = -static_cast<int>(result);
        return -1;
    }
    return static_cast<int>(result);
}

int LinuxSyscallHandler::bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
    uint64_t result = syscall64(SYS_bind, sockfd, reinterpret_cast<uint64_t>(addr), addrlen);
    if (result > -4096LL) {
        errno = -static_cast<int>(result);
        return -1;
    }
    return static_cast<int>(result);
}

int LinuxSyscallHandler::listen(int sockfd, int backlog) {
    uint64_t result = syscall64(SYS_listen, sockfd, backlog);
    if (result > -4096LL) {
        errno = -static_cast<int>(result);
        return -1;
    }
    return static_cast<int>(result);
}

int LinuxSyscallHandler::accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen) {
    uint64_t result = syscall64(SYS_accept, sockfd, reinterpret_cast<uint64_t>(addr), reinterpret_cast<uint64_t>(addrlen));
    if (result > -4096LL) {
        errno = -static_cast<int>(result);
        return -1;
    }
    return static_cast<int>(result);
}

int LinuxSyscallHandler::connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
    uint64_t result = syscall64(SYS_connect, sockfd, reinterpret_cast<uint64_t>(addr), addrlen);
    if (result > -4096LL) {
        errno = -static_cast<int>(result);
        return -1;
    }
    return static_cast<int>(result);
}

ssize_t LinuxSyscallHandler::send(int sockfd, const void* buf, size_t len, int flags) {
    uint64_t result = syscall64(SYS_send, sockfd, reinterpret_cast<uint64_t>(buf), len, flags);
    if (result > -4096LL) {
        errno = -static_cast<int>(result);
        return -1;
    }
    return static_cast<ssize_t>(result);
}

ssize_t LinuxSyscallHandler::recv(int sockfd, void* buf, size_t len, int flags) {
    uint64_t result = syscall64(SYS_recv, sockfd, reinterpret_cast<uint64_t>(buf), len, flags);
    if (result > -4096LL) {
        errno = -static_cast<int>(result);
        return -1;
    }
    return static_cast<ssize_t>(result);
}

} // namespace containeros

// ============================================
// extern "C" 接口，方便汇编和其他语言调用
// ============================================

extern "C" {

// 文件系统调用
int sys_open(const char* pathname, int flags, mode_t mode) {
    return containeros::LinuxSyscallHandler::open(pathname, flags, mode);
}

int sys_close(int fd) {
    return containeros::LinuxSyscallHandler::close(fd);
}

ssize_t sys_read(int fd, void* buf, size_t count) {
    return containeros::LinuxSyscallHandler::read(fd, buf, count);
}

ssize_t sys_write(int fd, const void* buf, size_t count) {
    return containeros::LinuxSyscallHandler::write(fd, buf, count);
}

off_t sys_lseek(int fd, off_t offset, int whence) {
    return containeros::LinuxSyscallHandler::lseek(fd, offset, whence);
}

int sys_stat(const char* pathname, struct stat* buf) {
    return containeros::LinuxSyscallHandler::stat(pathname, buf);
}

int sys_mkdir(const char* pathname, mode_t mode) {
    return containeros::LinuxSyscallHandler::mkdir(pathname, mode);
}

int sys_rmdir(const char* pathname) {
    return containeros::LinuxSyscallHandler::rmdir(pathname);
}

int sys_unlink(const char* pathname) {
    return containeros::LinuxSyscallHandler::unlink(pathname);
}

int sys_link(const char* oldpath, const char* newpath) {
    return containeros::LinuxSyscallHandler::link(oldpath, newpath);
}

int sys_chmod(const char* pathname, mode_t mode) {
    return containeros::LinuxSyscallHandler::chmod(pathname, mode);
}

int sys_chown(const char* pathname, uid_t owner, gid_t group) {
    return containeros::LinuxSyscallHandler::chown(pathname, owner, group);
}

// 进程系统调用
pid_t sys_fork() {
    return containeros::LinuxSyscallHandler::fork();
}

int sys_execve(const char* filename, char* const argv[], char* const envp[]) {
    return containeros::LinuxSyscallHandler::execve(filename, argv, envp);
}

pid_t sys_waitpid(pid_t pid, int* wstatus, int options) {
    return containeros::LinuxSyscallHandler::waitpid(pid, wstatus, options);
}

int sys_kill(pid_t pid, int sig) {
    return containeros::LinuxSyscallHandler::kill(pid, sig);
}

void sys_exit(int status) {
    containeros::LinuxSyscallHandler::exit(status);
}

pid_t sys_getpid() {
    return containeros::LinuxSyscallHandler::getpid();
}

pid_t sys_getppid() {
    return containeros::LinuxSyscallHandler::getppid();
}

int sys_setuid(uid_t uid) {
    return containeros::LinuxSyscallHandler::setuid(uid);
}

int sys_setgid(gid_t gid) {
    return containeros::LinuxSyscallHandler::setgid(gid);
}

// 内存系统调用
void* sys_mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset) {
    return containeros::LinuxSyscallHandler::mmap(addr, length, prot, flags, fd, offset);
}

int sys_munmap(void* addr, size_t length) {
    return containeros::LinuxSyscallHandler::munmap(addr, length);
}

int sys_brk(void* addr) {
    return containeros::LinuxSyscallHandler::brk(addr);
}

void* sys_sbrk(intptr_t increment) {
    return containeros::LinuxSyscallHandler::sbrk(increment);
}

// 网络系统调用
int sys_socket(int domain, int type, int protocol) {
    return containeros::LinuxSyscallHandler::socket(domain, type, protocol);
}

int sys_bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
    return containeros::LinuxSyscallHandler::bind(sockfd, addr, addrlen);
}

int sys_listen(int sockfd, int backlog) {
    return containeros::LinuxSyscallHandler::listen(sockfd, backlog);
}

int sys_accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen) {
    return containeros::LinuxSyscallHandler::accept(sockfd, addr, addrlen);
}

int sys_connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
    return containeros::LinuxSyscallHandler::connect(sockfd, addr, addrlen);
}

ssize_t sys_send(int sockfd, const void* buf, size_t len, int flags) {
    return containeros::LinuxSyscallHandler::send(sockfd, buf, len, flags);
}

ssize_t sys_recv(int sockfd, void* buf, size_t len, int flags) {
    return containeros::LinuxSyscallHandler::recv(sockfd, buf, len, flags);
}

} // extern "C"