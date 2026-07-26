#include "runtime/LinuxRuntime.h"

#include "core/Container.h"
#include "utils/Logger.h"
#include "runtime/elf/ElfLoader.h"
#include "syscall/SyscallDispatcher.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <sys/mount.h>
#include <sched.h>
#include <signal.h>
#include <cstring>
#include <cstdio>
#include <cerrno>
#include <fstream>
#include <vector>
#include <string>

LinuxRuntime::LinuxRuntime() 
    : elf_loader_(std::make_unique<ElfLoader>()),
      syscall_dispatcher_(std::make_unique<SyscallDispatcher>(PlatformType::Linux)) {
}

LinuxRuntime::~LinuxRuntime() = default;

bool LinuxRuntime::loadSharedLibraries(const std::string& rootfs) {
    Logger::getInstance().info("加载 rootfs 动态库: " + rootfs);
    (void)rootfs;
    return true;
}

bool LinuxRuntime::setupLinuxNamespaces(pid_t child_pid, const ContainerConfig& config) {
    Logger::getInstance().info("设置 Linux 命名空间, 子进程 PID: " +
                               std::to_string(child_pid));

    int flags = CLONE_NEWPID | CLONE_NEWNET | CLONE_NEWNS |
                CLONE_NEWUTS | CLONE_NEWIPC;
    (void)flags;

    if (!config.name.empty()) {
        // TODO: 进入子进程 UTS 命名空间后调用 sethostname()
    }

    (void)child_pid;
    return true;
}

bool LinuxRuntime::setupSyscallInterceptor(pid_t pid) {
    Logger::getInstance().info("设置 syscall 拦截, PID: " + std::to_string(pid));
    syscall_dispatcher_->enableTracing(true);
    (void)pid;
    return true;
}

std::unique_ptr<Container> LinuxRuntime::create(const ContainerConfig& config) {
    Logger::getInstance().info("LinuxRuntime 创建容器: " + config.name);

    if (config.command.empty()) {
        Logger::getInstance().error("启动命令为空");
        return nullptr;
    }

    if (!elf_loader_->load(config.command)) {
        Logger::getInstance().warn("命令非 ELF 二进制: " + config.command +
                                   ", 可能是脚本或不兼容格式");
    } else {
        Logger::getInstance().info("ELF 架构: " + 
            (elf_loader_->is64Bit() ? "x86_64" : "i386"));
    }

    std::string id = config.name + "-" + std::to_string(::getpid());
    return std::unique_ptr<Container>(new Container(id, config));
}

bool LinuxRuntime::start(Container& container) {
    Logger::getInstance().info("LinuxRuntime 启动容器: " + container.getId());

    const ContainerConfig& config = container.getConfig();

    if (elf_loader_->load(config.command)) {
        uint64_t entry_point = elf_loader_->getEntryPoint();
        Logger::getInstance().info("ELF 入口点: 0x" + 
            std::to_string(entry_point));
        
        if (!elf_loader_->loadSegments()) {
            Logger::getInstance().warn("ELF 段加载失败");
        }
        if (!elf_loader_->resolveSymbols()) {
            Logger::getInstance().warn("符号解析失败");
        }
        if (!elf_loader_->relocate()) {
            Logger::getInstance().warn("重定位失败");
        }
    }

    pid_t pid = ::fork();
    if (pid < 0) {
        Logger::getInstance().error("fork 失败: " + std::string(::strerror(errno)));
        return false;
    }

    if (pid == 0) {
        int ns_flags = CLONE_NEWPID | CLONE_NEWNET | CLONE_NEWNS |
                       CLONE_NEWUTS | CLONE_NEWIPC;
        // ::unshare(ns_flags);

        // 设置环境变量
        // for (const auto& kv : config.environment) {
        //     ::setenv(kv.first.c_str(), kv.second.c_str(), 1);
        // }

        ::_exit(127);
    }

    if (!setupLinuxNamespaces(pid, config)) {
        Logger::getInstance().warn("命名空间设置失败, 容器可能仍可运行");
    }
    if (!setupSyscallInterceptor(pid)) {
        Logger::getInstance().warn("syscall 拦截设置失败");
    }

    int status = 0;
    ::waitpid(pid, &status, 0);

    return true;
}

bool LinuxRuntime::stop(Container& container, int timeout_sec) {
    Logger::getInstance().info("LinuxRuntime 停止容器: " + container.getId() +
                               ", 超时: " + std::to_string(timeout_sec) + "s");
    return true;
}

bool LinuxRuntime::destroy(Container& container) {
    Logger::getInstance().info("LinuxRuntime 销毁容器: " + container.getId());

    if (!stop(container, 10)) {
        Logger::getInstance().warn("停止容器失败, 继续清理资源");
    }

    return container.destroy();
}

bool LinuxRuntime::supportsPlatform(const std::string& os_type) const {
    return os_type == "linux";
}

std::string LinuxRuntime::getRuntimeName() const {
    return "linux-runtime";
}

bool LinuxRuntime::canExecute(const std::string& binary_path) const {
    return elf_loader_->load(binary_path) && 
           ::access(binary_path.c_str(), X_OK) == 0;
}
