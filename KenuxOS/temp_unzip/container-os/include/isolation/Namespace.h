#pragma once

// Linux 命名空间隔离管理头文件
// 通过 unshare()/setns() 等系统调用实现容器的 PID/网络/挂载等隔离

#include <string>
#include <unordered_map>

// Linux 系统调用相关头文件
#include <sched.h>      // unshare(), setns(), CLONE_NEW* 标志
#include <unistd.h>     // close(), getpgid() 等
#include <fcntl.h>      // open(), O_RDONLY 等标志
#include <sys/stat.h>   // 文件状态信息

// 命名空间类型枚举
// 对应 Linux 的各类命名空间，用于实现不同维度的容器隔离
enum class NamespaceType {
    PID,        // 进程隔离: 容器内进程独立编号
    Network,    // 网络隔离: 独立的网络栈、网卡、路由表
    Mount,      // 挂载点隔离: 独立的文件系统视图
    UTS,        // 主机名隔离: 独立的 hostname/domainname
    IPC,        // 进程间通信隔离: 独立的 IPC 资源 (消息队列、共享内存等)
    User,       // 用户隔离: 独立的 UID/GID 映射
    Cgroup      // Cgroup 隔离: 独立的 cgroup 视图
};

// 命名空间管理器
// 负责创建、加入、销毁 Linux 命名空间，实现容器间的隔离
class NamespaceManager {
public:
    NamespaceManager();
    ~NamespaceManager();

    // 创建指定类型的命名空间
    // container_id: 容器标识
    // type:         命名空间类型
    // 返回:         命名空间文件描述符 (>=0)，失败返回 -1
    int create(const std::string& container_id, NamespaceType type);

    // 加入指定类型的命名空间
    // fd:   命名空间文件描述符
    // type: 命名空间类型
    // 返回: 成功返回 true
    bool join(int fd, NamespaceType type);

    // 销毁容器对应的所有命名空间 (关闭所有 fd)
    // container_id: 容器标识
    void destroy(const std::string& container_id);

    // 获取容器指定类型命名空间的文件描述符
    // container_id: 容器标识
    // type:         命名空间类型
    // 返回:         文件描述符 (>=0)，不存在或失败返回 -1
    int getNamespaceFd(const std::string& container_id, NamespaceType type);

private:
    // 容器命名空间描述符表: container_id -> (NamespaceType -> fd)
    std::unordered_map<std::string, std::unordered_map<NamespaceType, int>> namespace_fds_;

    // 将命名空间类型转换为 unshare()/setns() 使用的 clone 标志位
    int typeToCloneFlag(NamespaceType type) const;

    // 将命名空间类型转换为 /proc/self/ns/ 下的文件名
    std::string typeToProcName(NamespaceType type) const;

    // 关闭并清理单个容器的所有文件描述符
    void closeAllFds(const std::string& container_id);
};
