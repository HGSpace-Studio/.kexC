#include "core/IsolationManager.h"

#include <unistd.h>
#include <sched.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <cstring>
#include <string>

// 自定义 OS 上可能未定义以下 clone 标志, 此处提供默认值以保证可编译
#ifndef CLONE_NEWPID
#define CLONE_NEWPID 0x20000000
#endif
#ifndef CLONE_NEWNET
#define CLONE_NEWNET 0x40000000
#endif
#ifndef CLONE_NEWNS
#define CLONE_NEWNS 0x00020000
#endif
#ifndef CLONE_NEWUTS
#define CLONE_NEWUTS 0x04000000
#endif
#ifndef CLONE_NEWIPC
#define CLONE_NEWIPC 0x08000000
#endif
#ifndef CLONE_NEWUSER
#define CLONE_NEWUSER 0x10000000
#endif
#ifndef CLONE_NEWCGROUP
#define CLONE_NEWCGROUP 0x02000000
#endif

IsolationManager::IsolationManager() {}

IsolationManager::~IsolationManager() {
    // 析构时关闭所有尚未释放的命名空间文件描述符
    for (auto& kv : isolations_) {
        for (auto& fd_kv : kv.second.namespace_fds) {
            if (fd_kv.second >= 0) {
                close(fd_kv.second);
                fd_kv.second = -1;
            }
        }
    }
}

int IsolationManager::createNamespace(const std::string& container_id,
                                       NamespaceType type) {
    int fd = unshareNamespace(type);
    if (fd < 0) {
        return -1;
    }
    isolations_[container_id].namespace_fds[type] = fd;
    return fd;
}

bool IsolationManager::joinNamespace(int ns_fd, NamespaceType type) {
    return setNs(ns_fd, type);
}

bool IsolationManager::setResourceLimit(const std::string& container_id,
                                         const std::string& resource_type,
                                         uint64_t limit) {
    auto it = isolations_.find(container_id);
    if (it == isolations_.end()) {
        // 容器尚未创建 cgroup, 先创建
        if (!createCgroup(container_id)) {
            return false;
        }
        it = isolations_.find(container_id);
    }
    std::string cgroup_file = it->second.cgroup_path + "/" + resource_type;
    return writeCgroupFile(cgroup_file, std::to_string(limit));
}

ResourceUsage IsolationManager::getResourceUsage(
        const std::string& container_id) const {
    ResourceUsage usage;
    auto it = isolations_.find(container_id);
    if (it == isolations_.end()) {
        return usage;
    }
    const std::string& path = it->second.cgroup_path;

    // TODO: 在自定义 OS 上读取 cgroup v2 统计文件
    // 以下为标准 cgroup v2 文件, 实际格式解析需适配自定义 OS
    std::string mem_current = readCgroupFile(path + "/memory.current");
    std::string mem_peak = readCgroupFile(path + "/memory.peak");
    std::string cpu_stat = readCgroupFile(path + "/cpu.stat");
    std::string io_stat = readCgroupFile(path + "/io.stat");

    if (!mem_current.empty()) {
        try {
            usage.memory_usage = std::stoull(mem_current);
        } catch (...) {
            usage.memory_usage = 0;
        }
    }
    if (!mem_peak.empty()) {
        try {
            usage.memory_max_usage = std::stoull(mem_peak);
        } catch (...) {
            usage.memory_max_usage = 0;
        }
    }
    // TODO: 解析 cpu.stat 中的 usage_usec 字段并转换为纳秒
    // TODO: 解析 io.stat 中的 rbytes/wbytes 字段
    (void)cpu_stat;
    (void)io_stat;
    return usage;
}

void IsolationManager::cleanup(const std::string& container_id) {
    auto it = isolations_.find(container_id);
    if (it == isolations_.end()) {
        return;
    }
    // 关闭该容器所有命名空间文件描述符
    for (auto& fd_kv : it->second.namespace_fds) {
        if (fd_kv.second >= 0) {
            close(fd_kv.second);
            fd_kv.second = -1;
        }
    }
    // TODO: 在自定义 OS 上删除 cgroup 目录
    // rmdir(it->second.cgroup_path.c_str());
    isolations_.erase(it);
}

bool IsolationManager::createCgroup(const std::string& container_id) {
    // TODO: 在自定义 OS 上实现 cgroup v2 目录创建
    std::string path = "/sys/fs/cgroup/" + container_id;
    isolations_[container_id].cgroup_path = path;
    // if (mkdir(path.c_str(), 0755) != 0 && errno != EEXIST) {
    //     return false;
    // }
    return true;
}

bool IsolationManager::writeCgroupFile(const std::string& path,
                                         const std::string& value) {
    // TODO: 在自定义 OS 上实现 cgroup 控制文件写入
    // int fd = open(path.c_str(), O_WRONLY | O_TRUNC);
    // if (fd < 0) {
    //     return false;
    // }
    // ssize_t ret = write(fd, value.c_str(), value.size());
    // close(fd);
    // return ret == static_cast<ssize_t>(value.size());
    (void)path;
    (void)value;
    return true;
}

std::string IsolationManager::readCgroupFile(const std::string& path) const {
    // TODO: 在自定义 OS 上实现 cgroup 控制文件读取
    // int fd = open(path.c_str(), O_RDONLY);
    // if (fd < 0) {
    //     return "";
    // }
    // char buf[256];
    // ssize_t n = read(fd, buf, sizeof(buf) - 1);
    // close(fd);
    // if (n <= 0) {
    //     return "";
    // }
    // buf[n] = '\0';
    // return std::string(buf);
    (void)path;
    return "";
}

int IsolationManager::unshareNamespace(NamespaceType type) {
    // TODO: 在自定义 OS 上实现 unshare 系统调用
    int flags = namespaceTypeToFlag(type);
    // if (unshare(flags) != 0) {
    //     return -1;
    // }
    // 打开 /proc/self/ns/<type> 获取命名空间文件描述符并返回
    // std::string ns_path = "/proc/self/ns/" + namespaceTypeToString(type);
    // int fd = open(ns_path.c_str(), O_RDONLY);
    // return fd;
    (void)flags;
    return -1;
}

bool IsolationManager::setNs(int fd, NamespaceType type) {
    // TODO: 在自定义 OS 上实现 setns 系统调用
    // int flags = namespaceTypeToFlag(type);
    // return setns(fd, flags) == 0;
    (void)fd;
    (void)type;
    return false;
}

int IsolationManager::namespaceTypeToFlag(NamespaceType type) const {
    switch (type) {
        case NamespaceType::PID:     return CLONE_NEWPID;
        case NamespaceType::Network: return CLONE_NEWNET;
        case NamespaceType::Mount:   return CLONE_NEWNS;
        case NamespaceType::UTS:     return CLONE_NEWUTS;
        case NamespaceType::IPC:     return CLONE_NEWIPC;
        case NamespaceType::User:    return CLONE_NEWUSER;
        case NamespaceType::Cgroup:  return CLONE_NEWCGROUP;
        default:                     return 0;
    }
}

std::string IsolationManager::namespaceTypeToString(NamespaceType type) const {
    switch (type) {
        case NamespaceType::PID:     return "pid";
        case NamespaceType::Network: return "net";
        case NamespaceType::Mount:   return "mnt";
        case NamespaceType::UTS:     return "uts";
        case NamespaceType::IPC:     return "ipc";
        case NamespaceType::User:    return "user";
        case NamespaceType::Cgroup:  return "cgroup";
        default:                     return "unknown";
    }
}
