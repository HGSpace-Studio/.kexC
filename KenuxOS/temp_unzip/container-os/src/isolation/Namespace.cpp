#include "isolation/Namespace.h"

#include <cerrno>
#include <cstring>

NamespaceManager::NamespaceManager() = default;

NamespaceManager::~NamespaceManager() {
    // 析构时关闭所有尚未释放的命名空间文件描述符，防止 fd 泄漏
    for (auto& container_pair : namespace_fds_) {
        for (auto& type_fd : container_pair.second) {
            if (type_fd.second >= 0) {
                ::close(type_fd.second);
                type_fd.second = -1;
            }
        }
    }
}

// 将命名空间类型转换为 clone()/unshare()/setns() 使用的标志位
int NamespaceManager::typeToCloneFlag(NamespaceType type) const {
    switch (type) {
        case NamespaceType::PID:     return CLONE_NEWPID;
        case NamespaceType::Network: return CLONE_NEWNET;
        case NamespaceType::Mount:   return CLONE_NEWNS;
        case NamespaceType::UTS:     return CLONE_NEWUTS;
        case NamespaceType::IPC:     return CLONE_NEWIPC;
        case NamespaceType::User:    return CLONE_NEWUSER;
        case NamespaceType::Cgroup:  return CLONE_NEWCGROUP;
    }
    return 0;
}

// 将命名空间类型映射到 /proc/self/ns/ 下的对应文件名
std::string NamespaceManager::typeToProcName(NamespaceType type) const {
    switch (type) {
        case NamespaceType::PID:     return "pid";
        case NamespaceType::Network: return "net";
        case NamespaceType::Mount:   return "mnt";
        case NamespaceType::UTS:     return "uts";
        case NamespaceType::IPC:     return "ipc";
        case NamespaceType::User:    return "user";
        case NamespaceType::Cgroup:  return "cgroup";
    }
    return "";
}

void NamespaceManager::closeAllFds(const std::string& container_id) {
    auto it = namespace_fds_.find(container_id);
    if (it == namespace_fds_.end()) {
        return;
    }
    // 关闭该容器所有命名空间类型的文件描述符
    for (auto& type_fd : it->second) {
        if (type_fd.second >= 0) {
            ::close(type_fd.second);
            type_fd.second = -1;
        }
    }
    it->second.clear();
}

int NamespaceManager::create(const std::string& container_id, NamespaceType type) {
    // 调用 unshare() 创建新的命名空间
    // 注意: unshare() 直接作用于当前进程。在实际容器运行时中，通常应在
    //       fork() 出的子进程中调用 unshare()，避免影响管理进程自身。
    // TODO: 自定义 OS 上 unshare() 可能尚未实现，需内核侧补充实现
    int flag = typeToCloneFlag(type);
    if (flag == 0) {
        return -1;
    }

    if (::unshare(flag) != 0) {
        return -1;
    }

    // 创建成功后，打开 /proc/self/ns/<type> 获取命名空间文件描述符
    // 该 fd 可用于后续 setns() 加入该命名空间
    // TODO: 自定义 OS 上 /proc 文件系统路径可能不同，需适配
    std::string ns_path = "/proc/self/ns/" + typeToProcName(type);
    int fd = ::open(ns_path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return -1;
    }

    // 记录到管理表 (若该容器已有同类型 fd，先关闭旧的)
    auto& type_map = namespace_fds_[container_id];
    auto existed = type_map.find(type);
    if (existed != type_map.end() && existed->second >= 0) {
        ::close(existed->second);
    }
    type_map[type] = fd;
    return fd;
}

bool NamespaceManager::join(int fd, NamespaceType type) {
    if (fd < 0) {
        return false;
    }

    // 调用 setns() 加入指定命名空间
    // 第二个参数为 clone 标志位，用于指定要加入的命名空间类型
    // TODO: 自定义 OS 上 setns() 可能尚未实现，需内核侧补充实现
    int flag = typeToCloneFlag(type);
    if (::setns(fd, flag) != 0) {
        return false;
    }
    return true;
}

void NamespaceManager::destroy(const std::string& container_id) {
    // 关闭该容器的所有 fd 并从管理表中移除
    closeAllFds(container_id);
    namespace_fds_.erase(container_id);
}

int NamespaceManager::getNamespaceFd(const std::string& container_id, NamespaceType type) {
    auto it = namespace_fds_.find(container_id);
    if (it == namespace_fds_.end()) {
        return -1;
    }
    auto type_it = it->second.find(type);
    if (type_it == it->second.end()) {
        return -1;
    }
    return type_it->second;
}
