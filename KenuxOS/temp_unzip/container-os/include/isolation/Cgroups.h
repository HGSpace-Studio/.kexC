#pragma once

// Cgroups 资源限制管理头文件
// 通过 cgroup 机制对容器进行 CPU/内存/IO/PID 等资源限制与使用量统计

#include <string>
#include <unordered_map>
#include <fstream>
#include <cstdint>

// 引用 core 层定义的 ResourceUsage 结构体 (定义于 core/IsolationManager.h)
#include "core/IsolationManager.h"

// Cgroup 版本枚举
enum class CgroupVersion {
    V1,   // Cgroup v1: 传统多层级结构 (每个控制器独立挂载)
    V2     // Cgroup v2: 统一层级结构 (单一挂载点)
};

// Cgroups 资源管理器
// 负责通过 cgroup 机制对容器进行资源限制与使用量统计
class CgroupsManager {
public:
    CgroupsManager();
    ~CgroupsManager();

    // 创建容器的 cgroup 目录
    // container_id: 容器标识
    // 返回: 成功返回 true
    bool createCgroup(const std::string& container_id);

    // 设置 CPU 限制
    // container_id: 容器标识
    // cpu_count:    可用 CPU 核数 (例如 2 表示 2 个核)
    // 返回: 成功返回 true
    bool setCpuLimit(const std::string& container_id, int cpu_count);

    // 设置内存上限
    // container_id: 容器标识
    // bytes:        内存上限 (字节数)
    // 返回: 成功返回 true
    bool setMemoryLimit(const std::string& container_id, uint64_t bytes);

    // 设置块设备 I/O 带宽限制
    // container_id: 容器标识
    // bps:          每秒字节数 (bytes per second)
    // 返回: 成功返回 true
    bool setBlkioLimit(const std::string& container_id, uint64_t bps);

    // 设置容器最大进程数限制
    // container_id: 容器标识
    // max_pids:     最大进程数
    // 返回: 成功返回 true
    bool getPidsLimit(const std::string& container_id, int max_pids);

    // 获取容器资源使用情况
    // container_id: 容器标识
    // 返回: ResourceUsage 结构体 (引用 core 层定义)
    ResourceUsage getResourceUsage(const std::string& container_id);

    // 移除容器的 cgroup
    // container_id: 容器标识
    // 返回: 成功返回 true
    bool removeCgroup(const std::string& container_id);

    // 检测当前系统使用的 cgroup 版本
    // 返回: CgroupVersion::V1 或 V2
    CgroupVersion detectCgroupVersion();

private:
    // cgroup 根路径 (v2: /sys/fs/cgroup/)
    std::string cgroup_root_;

    // 已检测到的 cgroup 版本
    CgroupVersion version_;

    // 容器 cgroup 路径缓存: container_id -> 完整路径
    std::unordered_map<std::string, std::string> cgroup_paths_;

    // 获取容器 cgroup 的完整路径
    // v2 路径: /sys/fs/cgroup/<container_id>/
    std::string getCgroupPath(const std::string& container_id) const;

    // 向 cgroup 控制文件写入值
    bool writeCgroupFile(const std::string& path, const std::string& value);

    // 读取 cgroup 控制文件内容 (单行)
    std::string readCgroupFile(const std::string& path) const;
};
