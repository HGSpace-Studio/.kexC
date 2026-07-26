#pragma once

// 资源管理器头文件
// 负责宿主机资源探测、容器资源使用统计、资源预留与回收

#include <string>
#include <unordered_map>
#include <cstdint>
#include <mutex>

#include "core/Container.h"          // ContainerConfig / ResourceLimits
#include "core/IsolationManager.h"    // ResourceUsage

// 宿主机资源信息结构体
struct HostResources {
    int cpu_count = 0;                // 逻辑 CPU 核数
    uint64_t total_memory = 0;        // 总内存 (字节)
    uint64_t available_memory = 0;   // 可用内存 (字节)
    uint64_t disk_available = 0;     // 可用磁盘空间 (字节)
    double cpu_usage_percent = 0.0;  // CPU 使用率 (0-100)
};

// 资源管理器
// 单例语义: 通过构造函数创建即可, 内部状态线程安全
class ResourceManager {
public:
    ResourceManager();
    ~ResourceManager();

    // 获取当前宿主机资源信息 (实时读取 /proc)
    HostResources getHostResources();

    // 获取指定容器的资源使用情况
    // 引用 core 层定义的 ResourceUsage
    ResourceUsage getContainerUsage(const std::string& container_id);

    // 检查是否有足够资源容纳指定配置
    // config:   待部署容器配置
    // resources: 当前宿主机可用资源
    // 返回:      资源充足返回 true
    bool checkResources(const ContainerConfig& config,
                        const HostResources& resources) const;

    // 预留资源 (将容器资源计入已用)
    // container_id: 容器 ID
    // config:       容器配置
    // 返回:         成功预留返回 true, 资源不足返回 false
    bool reserveResources(const std::string& container_id, const ContainerConfig& config);

    // 释放预留资源
    // container_id: 容器 ID
    // 返回:         成功释放返回 true
    bool releaseResources(const std::string& container_id);

private:
    // 读取 /proc/meminfo, 解析总内存与可用内存
    void readMemoryInfo(uint64_t& total, uint64_t& available) const;

    // 读取 /proc/cpuinfo, 统计逻辑 CPU 数
    int readCpuCount() const;

    // 读取 /proc/stat, 计算 CPU 使用率 (与上次采样差值)
    double readCpuUsage() const;

    // 读取磁盘可用空间 (statvfs)
    uint64_t readDiskAvailable() const;

    // 获取容器 cgroup 中的资源使用情况
    ResourceUsage readContainerUsage(const std::string& container_id) const;

    // 已预留资源表: container_id -> ResourceLimits
    std::unordered_map<std::string, ResourceLimits> reserved_resources_;

    // 用于 CPU 使用率计算的上次采样
    mutable uint64_t prev_total_time_ = 0;
    mutable uint64_t prev_idle_time_ = 0;

    mutable std::mutex mutex_;
};
