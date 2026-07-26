#pragma once

#include <string>
#include <unordered_map>
#include <cstdint>

// 命名空间类型枚举 (统一定义于 isolation/Namespace.h，此处引入以复用)
#include "isolation/Namespace.h"

// 资源使用情况
struct ResourceUsage {
    uint64_t cpu_time_ns = 0;       // CPU 时间 (纳秒)
    uint64_t memory_usage = 0;      // 内存使用 (字节)
    uint64_t memory_max_usage = 0;  // 最大内存使用 (字节)
    uint64_t io_read_bytes = 0;     // I/O 读取字节
    uint64_t io_write_bytes = 0;    // I/O 写入字节
};

// 隔离管理器: 统一管理容器的命名空间与 Cgroup 资源限制
class IsolationManager {
public:
    IsolationManager();
    ~IsolationManager();

    // 创建命名空间, 返回文件描述符 (失败返回 -1)
    int createNamespace(const std::string& container_id, NamespaceType type);

    // 加入命名空间
    bool joinNamespace(int ns_fd, NamespaceType type);

    // 设置资源限制
    // resource_type 例如 "cpu.max" / "memory.max"
    bool setResourceLimit(const std::string& container_id,
                          const std::string& resource_type,
                          uint64_t limit);

    // 获取资源使用情况
    ResourceUsage getResourceUsage(const std::string& container_id) const;

    // 清理容器所有隔离资源
    void cleanup(const std::string& container_id);

private:
    // 容器隔离信息内部结构
    struct ContainerIsolation {
        std::unordered_map<NamespaceType, int> namespace_fds;  // 命名空间文件描述符
        std::string cgroup_path;                                // cgroup 路径
    };

    std::unordered_map<std::string, ContainerIsolation> isolations_;

    // Cgroup 操作
    bool createCgroup(const std::string& container_id);
    bool writeCgroupFile(const std::string& path, const std::string& value);
    std::string readCgroupFile(const std::string& path) const;

    // Namespace 操作
    int unshareNamespace(NamespaceType type);
    bool setNs(int fd, NamespaceType type);

    // 辅助方法
    int namespaceTypeToFlag(NamespaceType type) const;
    std::string namespaceTypeToString(NamespaceType type) const;
};
