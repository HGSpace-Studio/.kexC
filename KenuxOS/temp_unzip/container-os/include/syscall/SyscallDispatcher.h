#pragma once

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <string>
#include <mutex>

namespace containeros {

enum class PlatformType {
    Linux,
    Windows,
    Unknown
};

enum class SyscallType {
    File,
    Process,
    Memory,
    Network,
    System
};

class SyscallDispatcher {
public:
    using SyscallInterceptor = std::function<int64_t(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, bool&)>;

    explicit SyscallDispatcher(PlatformType platform = PlatformType::Unknown);
    ~SyscallDispatcher();

    int64_t dispatch(uint64_t syscall_number, uint64_t arg1 = 0, uint64_t arg2 = 0,
                     uint64_t arg3 = 0, uint64_t arg4 = 0, uint64_t arg5 = 0,
                     uint64_t arg6 = 0);

    void setPlatform(PlatformType platform);
    PlatformType getPlatform() const;

    void setSyscallHandler(void* handler);
    void setWindowsEmulator(void* emulator);

    void intercept(uint64_t syscall_number, const SyscallInterceptor& callback);
    void removeInterceptor(uint64_t syscall_number);
    bool isIntercepted(uint64_t syscall_number) const;

    void enableTracing(bool enable);

    std::string getSyscallName(uint64_t syscall_number) const;

private:
    int64_t dispatchLinux(uint64_t syscall_number, uint64_t arg1, uint64_t arg2,
                          uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6);

    int64_t dispatchWindows(uint64_t syscall_number, uint64_t arg1, uint64_t arg2,
                            uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6);

    void logSyscallTrace(uint64_t syscall_number, uint64_t arg1, uint64_t arg2,
                         uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6,
                         int64_t result);

    PlatformType platform_;
    std::unordered_map<uint64_t, SyscallInterceptor> interceptors_;
    bool tracing_enabled_;
    void* linux_handler_;
    void* windows_emulator_;
    mutable std::mutex mutex_;
};

} // namespace containeros
