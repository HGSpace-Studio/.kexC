#include "syscall/SyscallDispatcher.h"
#include "syscall/LinuxSyscallHandler.h"
#include "syscall/Win32ApiEmulator.h"
#include "utils/Logger.h"
#include <cstring>

namespace containeros {

namespace {

std::string getLinuxSyscallName(uint64_t number) {
    switch (number) {
        case SYS_read: return "read";
        case SYS_write: return "write";
        case SYS_open: return "open";
        case SYS_close: return "close";
        case SYS_stat: return "stat";
        case SYS_fstat: return "fstat";
        case SYS_lstat: return "lstat";
        case SYS_lseek: return "lseek";
        case SYS_mmap: return "mmap";
        case SYS_munmap: return "munmap";
        case SYS_brk: return "brk";
        case SYS_socket: return "socket";
        case SYS_bind: return "bind";
        case SYS_connect: return "connect";
        case SYS_listen: return "listen";
        case SYS_accept: return "accept";
        case SYS_send: return "send";
        case SYS_recv: return "recv";
        case SYS_fork: return "fork";
        case SYS_execve: return "execve";
        case SYS_exit: return "exit";
        case SYS_waitpid: return "waitpid";
        case SYS_chdir: return "chdir";
        case SYS_mkdir: return "mkdir";
        case SYS_rmdir: return "rmdir";
        case SYS_unlink: return "unlink";
        case SYS_link: return "link";
        case SYS_chmod: return "chmod";
        case SYS_chown: return "chown";
        case SYS_kill: return "kill";
        case SYS_getpid: return "getpid";
        case SYS_getppid: return "getppid";
        case SYS_setuid: return "setuid";
        case SYS_setgid: return "setgid";
        case SYS_sbrk: return "sbrk";
        default: return "unknown_syscall(" + std::to_string(number) + ")";
    }
}

} // namespace

SyscallDispatcher::SyscallDispatcher(PlatformType platform)
    : platform_(platform), tracing_enabled_(false), linux_handler_(nullptr), windows_emulator_(nullptr) {
    Logger::getInstance().info("SyscallDispatcher 初始化, 平台: " + 
        (platform == PlatformType::Linux ? "Linux" : platform == PlatformType::Windows ? "Windows" : "Unknown"));
}

SyscallDispatcher::~SyscallDispatcher() {
    Logger::getInstance().info("SyscallDispatcher 销毁");
}

int64_t SyscallDispatcher::dispatch(uint64_t syscall_number, uint64_t arg1, uint64_t arg2,
                                    uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    std::lock_guard<std::mutex> lock(mutex_);

    bool intercepted = false;
    auto it = interceptors_.find(syscall_number);
    if (it != interceptors_.end()) {
        int64_t result = it->second(syscall_number, arg1, arg2, arg3, arg4, arg5, arg6, intercepted);
        if (intercepted) {
            if (tracing_enabled_) {
                logSyscallTrace(syscall_number, arg1, arg2, arg3, arg4, arg5, arg6, result);
            }
            return result;
        }
    }

    int64_t result = 0;
    switch (platform_) {
        case PlatformType::Linux:
            result = dispatchLinux(syscall_number, arg1, arg2, arg3, arg4, arg5, arg6);
            break;
        case PlatformType::Windows:
            result = dispatchWindows(syscall_number, arg1, arg2, arg3, arg4, arg5, arg6);
            break;
        case PlatformType::Unknown:
        default:
            Logger::getInstance().error("SyscallDispatcher: 未知平台类型, syscall_number: " + std::to_string(syscall_number));
            result = -1;
            break;
    }

    if (tracing_enabled_) {
        logSyscallTrace(syscall_number, arg1, arg2, arg3, arg4, arg5, arg6, result);
    }

    return result;
}

int64_t SyscallDispatcher::dispatchLinux(uint64_t syscall_number, uint64_t arg1, uint64_t arg2,
                                         uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    // TODO: 在自定义操作系统上，需要实现底层 syscall 分发逻辑
    // 当前实现基于 LinuxSyscallHandler 的静态方法调用

    switch (syscall_number) {
        case SYS_read:
            return LinuxSyscallHandler::read(static_cast<int>(arg1), reinterpret_cast<void*>(arg2), static_cast<size_t>(arg3));
        case SYS_write:
            return LinuxSyscallHandler::write(static_cast<int>(arg1), reinterpret_cast<const void*>(arg2), static_cast<size_t>(arg3));
        case SYS_open:
            return LinuxSyscallHandler::open(reinterpret_cast<const char*>(arg1), static_cast<int>(arg2), static_cast<mode_t>(arg3));
        case SYS_close:
            return LinuxSyscallHandler::close(static_cast<int>(arg1));
        case SYS_stat:
            return LinuxSyscallHandler::stat(reinterpret_cast<const char*>(arg1), reinterpret_cast<struct stat*>(arg2));
        case SYS_lseek:
            return LinuxSyscallHandler::lseek(static_cast<int>(arg1), static_cast<off_t>(arg2), static_cast<int>(arg3));
        case SYS_mmap:
            return reinterpret_cast<int64_t>(LinuxSyscallHandler::mmap(reinterpret_cast<void*>(arg1), static_cast<size_t>(arg2),
                static_cast<int>(arg3), static_cast<int>(arg4), static_cast<int>(arg5), static_cast<off_t>(arg6)));
        case SYS_munmap:
            return LinuxSyscallHandler::munmap(reinterpret_cast<void*>(arg1), static_cast<size_t>(arg2));
        case SYS_brk:
            return LinuxSyscallHandler::brk(reinterpret_cast<void*>(arg1));
        case SYS_socket:
            return LinuxSyscallHandler::socket(static_cast<int>(arg1), static_cast<int>(arg2), static_cast<int>(arg3));
        case SYS_bind:
            return LinuxSyscallHandler::bind(static_cast<int>(arg1), reinterpret_cast<const struct sockaddr*>(arg2), static_cast<socklen_t>(arg3));
        case SYS_connect:
            return LinuxSyscallHandler::connect(static_cast<int>(arg1), reinterpret_cast<const struct sockaddr*>(arg2), static_cast<socklen_t>(arg3));
        case SYS_listen:
            return LinuxSyscallHandler::listen(static_cast<int>(arg1), static_cast<int>(arg2));
        case SYS_accept:
            return LinuxSyscallHandler::accept(static_cast<int>(arg1), reinterpret_cast<struct sockaddr*>(arg2), reinterpret_cast<socklen_t*>(arg3));
        case SYS_send:
            return LinuxSyscallHandler::send(static_cast<int>(arg1), reinterpret_cast<const void*>(arg2), static_cast<size_t>(arg3), static_cast<int>(arg4));
        case SYS_recv:
            return LinuxSyscallHandler::recv(static_cast<int>(arg1), reinterpret_cast<void*>(arg2), static_cast<size_t>(arg3), static_cast<int>(arg4));
        case SYS_fork:
            return LinuxSyscallHandler::fork();
        case SYS_execve:
            return LinuxSyscallHandler::execve(reinterpret_cast<const char*>(arg1), reinterpret_cast<char* const*>(arg2), reinterpret_cast<char* const*>(arg3));
        case SYS_exit:
            LinuxSyscallHandler::exit(static_cast<int>(arg1));
            return 0;
        case SYS_waitpid:
            return LinuxSyscallHandler::waitpid(static_cast<pid_t>(arg1), reinterpret_cast<int*>(arg2), static_cast<int>(arg3));
        case SYS_chdir:
            return LinuxSyscallHandler::chdir(reinterpret_cast<const char*>(arg1));
        case SYS_mkdir:
            return LinuxSyscallHandler::mkdir(reinterpret_cast<const char*>(arg1), static_cast<mode_t>(arg2));
        case SYS_rmdir:
            return LinuxSyscallHandler::rmdir(reinterpret_cast<const char*>(arg1));
        case SYS_unlink:
            return LinuxSyscallHandler::unlink(reinterpret_cast<const char*>(arg1));
        case SYS_link:
            return LinuxSyscallHandler::link(reinterpret_cast<const char*>(arg1), reinterpret_cast<const char*>(arg2));
        case SYS_chmod:
            return LinuxSyscallHandler::chmod(reinterpret_cast<const char*>(arg1), static_cast<mode_t>(arg2));
        case SYS_chown:
            return LinuxSyscallHandler::chown(reinterpret_cast<const char*>(arg1), static_cast<uid_t>(arg2), static_cast<gid_t>(arg3));
        case SYS_kill:
            return LinuxSyscallHandler::kill(static_cast<pid_t>(arg1), static_cast<int>(arg2));
        case SYS_getpid:
            return LinuxSyscallHandler::getpid();
        case SYS_getppid:
            return LinuxSyscallHandler::getppid();
        case SYS_setuid:
            return LinuxSyscallHandler::setuid(static_cast<uid_t>(arg1));
        case SYS_setgid:
            return LinuxSyscallHandler::setgid(static_cast<gid_t>(arg1));
        case SYS_sbrk:
            return reinterpret_cast<int64_t>(LinuxSyscallHandler::sbrk(static_cast<intptr_t>(arg1)));
        default:
            Logger::getInstance().warn("SyscallDispatcher: 未支持的 Linux syscall, 编号: " + std::to_string(syscall_number));
            // TODO: 在自定义操作系统上，需要实现通用的 syscall 转发机制
            return -1;
    }
}

int64_t SyscallDispatcher::dispatchWindows(uint64_t syscall_number, uint64_t arg1, uint64_t arg2,
                                           uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    // TODO: 在自定义操作系统上，Windows syscall 号与 Linux 不同，需要实现映射
    // 当前实现通过 Win32ApiEmulator 模拟 Win32 API 调用
    // 这里将常见的 syscall 号映射到对应的 Win32 API

    Win32ApiEmulator* emulator = reinterpret_cast<Win32ApiEmulator*>(windows_emulator_);
    if (!emulator) {
        Logger::getInstance().error("SyscallDispatcher: Windows 模拟器未初始化");
        return -1;
    }

    // Windows syscall 号到 Win32 API 的映射
    // TODO: 自定义 OS 上需要实现完整的 Windows NT syscall 号映射
    switch (syscall_number) {
        case 0x00:
            return reinterpret_cast<int64_t>(emulator->CreateFileA(
                reinterpret_cast<LPCSTR>(arg1), static_cast<DWORD>(arg2),
                static_cast<DWORD>(arg3), reinterpret_cast<LPVOID>(arg4),
                static_cast<DWORD>(arg5), static_cast<DWORD>(arg6),
                reinterpret_cast<HANDLE>(0)));
        case 0x01:
            return static_cast<int64_t>(emulator->CloseHandle(reinterpret_cast<HANDLE>(arg1)));
        case 0x02:
            return static_cast<int64_t>(emulator->ReadFile(
                reinterpret_cast<HANDLE>(arg1), reinterpret_cast<LPVOID>(arg2),
                static_cast<DWORD>(arg3), reinterpret_cast<LPDWORD>(arg4),
                reinterpret_cast<LPVOID>(arg5)));
        case 0x03:
            return static_cast<int64_t>(emulator->WriteFile(
                reinterpret_cast<HANDLE>(arg1), reinterpret_cast<LPCVOID>(arg2),
                static_cast<DWORD>(arg3), reinterpret_cast<LPDWORD>(arg4),
                reinterpret_cast<LPVOID>(arg5)));
        case 0x04:
            return reinterpret_cast<int64_t>(emulator->VirtualAlloc(
                reinterpret_cast<LPVOID>(arg1), static_cast<SIZE_T>(arg2),
                static_cast<DWORD>(arg3), static_cast<DWORD>(arg4)));
        case 0x05:
            return static_cast<int64_t>(emulator->VirtualFree(
                reinterpret_cast<LPVOID>(arg1), static_cast<SIZE_T>(arg2),
                static_cast<DWORD>(arg3)));
        case 0x06:
            return reinterpret_cast<int64_t>(emulator->CreateProcessA(
                reinterpret_cast<LPCSTR>(arg1), reinterpret_cast<LPSTR>(arg2),
                reinterpret_cast<LPVOID>(arg3), reinterpret_cast<LPVOID>(arg4),
                static_cast<BOOL>(arg5), static_cast<DWORD>(arg6),
                nullptr, nullptr, nullptr, nullptr));
        default:
            Logger::getInstance().warn("SyscallDispatcher: 未支持的 Windows syscall, 编号: " + std::to_string(syscall_number));
            return -1;
    }
}

void SyscallDispatcher::logSyscallTrace(uint64_t syscall_number, uint64_t arg1, uint64_t arg2,
                                        uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6,
                                        int64_t result) {
    std::string syscall_name = getSyscallName(syscall_number);
    std::string trace_msg = "SYSCALL TRACE: " + syscall_name +
        "(" + std::to_string(arg1) + ", " + std::to_string(arg2) + ", " +
        std::to_string(arg3) + ", " + std::to_string(arg4) + ", " +
        std::to_string(arg5) + ", " + std::to_string(arg6) + ") -> " +
        std::to_string(result);
    Logger::getInstance().trace(trace_msg);
}

void SyscallDispatcher::setPlatform(PlatformType platform) {
    std::lock_guard<std::mutex> lock(mutex_);
    platform_ = platform;
    Logger::getInstance().info("SyscallDispatcher 平台切换: " +
        (platform == PlatformType::Linux ? "Linux" : platform == PlatformType::Windows ? "Windows" : "Unknown"));
}

PlatformType SyscallDispatcher::getPlatform() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return platform_;
}

void SyscallDispatcher::setSyscallHandler(void* handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    linux_handler_ = handler;
    Logger::getInstance().info("SyscallDispatcher: Linux syscall 处理器已设置");
}

void SyscallDispatcher::setWindowsEmulator(void* emulator) {
    std::lock_guard<std::mutex> lock(mutex_);
    windows_emulator_ = emulator;
    Logger::getInstance().info("SyscallDispatcher: Windows 模拟器已设置");
}

void SyscallDispatcher::intercept(uint64_t syscall_number, const SyscallInterceptor& callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    interceptors_[syscall_number] = callback;
    Logger::getInstance().info("SyscallDispatcher: 已拦截 syscall " + std::to_string(syscall_number));
}

void SyscallDispatcher::removeInterceptor(uint64_t syscall_number) {
    std::lock_guard<std::mutex> lock(mutex_);
    interceptors_.erase(syscall_number);
    Logger::getInstance().info("SyscallDispatcher: 已移除 syscall " + std::to_string(syscall_number) + " 的拦截器");
}

bool SyscallDispatcher::isIntercepted(uint64_t syscall_number) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return interceptors_.find(syscall_number) != interceptors_.end();
}

void SyscallDispatcher::enableTracing(bool enable) {
    std::lock_guard<std::mutex> lock(mutex_);
    tracing_enabled_ = enable;
    Logger::getInstance().info("SyscallDispatcher: syscall 追踪 " + (enable ? "已启用" : "已禁用"));
}

std::string SyscallDispatcher::getSyscallName(uint64_t syscall_number) const {
    std::lock_guard<std::mutex> lock(mutex_);
    switch (platform_) {
        case PlatformType::Linux:
            return getLinuxSyscallName(syscall_number);
        case PlatformType::Windows:
            // TODO: 在自定义操作系统上，需要实现 Windows syscall 名称映射
            return "win32_syscall(" + std::to_string(syscall_number) + ")";
        case PlatformType::Unknown:
        default:
            return "unknown_syscall(" + std::to_string(syscall_number) + ")";
    }
}

} // namespace containeros
