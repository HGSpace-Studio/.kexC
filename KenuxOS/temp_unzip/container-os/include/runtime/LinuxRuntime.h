#pragma once

#include "core/Runtime.h"
#include "runtime/elf/ElfLoader.h"
#include "syscall/SyscallDispatcher.h"

#include <string>
#include <sys/types.h>

class LinuxRuntime : public Runtime {
public:
    LinuxRuntime();
    ~LinuxRuntime() override;

    std::unique_ptr<Container> create(const ContainerConfig& config) override;
    bool start(Container& container) override;
    bool stop(Container& container, int timeout_sec = 10) override;
    bool destroy(Container& container) override;

    bool supportsPlatform(const std::string& os_type) const override;
    std::string getRuntimeName() const override;
    bool canExecute(const std::string& binary_path) const override;

private:
    bool loadSharedLibraries(const std::string& rootfs);
    bool setupLinuxNamespaces(pid_t child_pid, const ContainerConfig& config);
    bool setupSyscallInterceptor(pid_t pid);

    std::unique_ptr<ElfLoader> elf_loader_;
    std::unique_ptr<SyscallDispatcher> syscall_dispatcher_;
};
