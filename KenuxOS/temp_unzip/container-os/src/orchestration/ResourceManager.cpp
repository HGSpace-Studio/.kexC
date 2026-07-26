#include "orchestration/ResourceManager.h"
#include "utils/Logger.h"

#include <fstream>
#include <sstream>
#include <cstring>
#include <cstdlib>

// 磁盘空间查询
#ifdef __linux__
#include <sys/statvfs.h>
#endif

ResourceManager::ResourceManager() {}

ResourceManager::~ResourceManager() {}

void ResourceManager::readMemoryInfo(uint64_t& total, uint64_t& available) const {
    total = 0;
    available = 0;
    std::ifstream file("/proc/meminfo");
    if (!file.is_open()) {
        // TODO: 自定义 OS 上 /proc 可能不可用, 后续提供替代实现
        Logger::getInstance().warn("ResourceManager: /proc/meminfo not available");
        return;
    }
    std::string line;
    while (std::getline(file, line)) {
        // 解析 "MemTotal:       16384000 kB" 形式
        std::istringstream iss(line);
        std::string key;
        uint64_t value = 0;
        std::string unit;
        iss >> key >> value >> unit;
        if (key == "MemTotal:") {
            total = value * 1024;  // kB -> bytes
        } else if (key == "MemAvailable:") {
            available = value * 1024;
        }
    }
    // 兼容旧内核: 若无 MemAvailable, 用 MemFree + Buffers + Cached 估算
    if (available == 0) {
        // TODO: 重新解析 MemFree/Buffers/Cached
        available = total / 2;  // 占位估算
    }
}

int ResourceManager::readCpuCount() const {
    // 优先使用 sysconf
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    if (n > 0) {
        return static_cast<int>(n);
    }

    // 回退: 统计 /proc/cpuinfo 中 "processor" 行
    std::ifstream file("/proc/cpuinfo");
    if (!file.is_open()) {
        Logger::getInstance().warn("ResourceManager: /proc/cpuinfo not available");
        return 1;
    }
    int count = 0;
    std::string line;
    while (std::getline(file, line)) {
        if (line.compare(0, 9, "processor") == 0) {
            ++count;
        }
    }
    return count > 0 ? count : 1;
}

double ResourceManager::readCpuUsage() const {
    // 解析 /proc/stat 第一行: cpu user nice system idle iowait irq softirq steal ...
    std::ifstream file("/proc/stat");
    if (!file.is_open()) {
        // TODO: 自定义 OS 上 /proc 可能不可用
        return 0.0;
    }
    std::string line;
    if (!std::getline(file, line)) {
        return 0.0;
    }
    std::istringstream iss(line);
    std::string cpu_label;
    iss >> cpu_label;  // "cpu"
    uint64_t user, nice, system, idle, iowait, irq, softirq, steal;
    iss >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;

    uint64_t total_time = user + nice + system + idle + iowait + irq + softirq + steal;
    uint64_t idle_time = idle + iowait;

    // 与上次采样差值计算
    uint64_t prev_total = prev_total_time_;
    uint64_t prev_idle = prev_idle_time_;
    prev_total_time_ = total_time;
    prev_idle_time_ = idle_time;

    if (total_time == prev_total) {
        return 0.0;
    }
    uint64_t total_diff = total_time - prev_total;
    uint64_t idle_diff = idle_time - prev_idle;
    if (total_diff == 0) {
        return 0.0;
    }
    double usage = (1.0 - static_cast<double>(idle_diff) / total_diff) * 100.0;
    if (usage < 0.0) usage = 0.0;
    if (usage > 100.0) usage = 100.0;
    return usage;
}

uint64_t ResourceManager::readDiskAvailable() const {
#ifdef __linux__
    struct statvfs stat;
    if (statvfs("/", &stat) == 0) {
        return static_cast<uint64_t>(stat.f_bavail) * stat.f_frsize;
    }
    // TODO: 自定义 OS 上 statvfs 不可用时的替代实现
    Logger::getInstance().warn("ResourceManager: statvfs failed");
    return 0;
#else
    // TODO: 自定义 OS 上提供磁盘空间查询
    return 0;
#endif
}

ResourceUsage ResourceManager::readContainerUsage(const std::string& container_id) const {
    ResourceUsage usage;
    // TODO: 通过 IsolationManager / cgroup 读取容器实际使用
    // 路径示例: /sys/fs/cgroup/<container_id>/memory.current / cpu.stat
    // 此处简化为返回 0, 实际使用由调用方通过 IsolationManager 获取
    (void)container_id;
    return usage;
}

HostResources ResourceManager::getHostResources() {
    std::lock_guard<std::mutex> lock(mutex_);

    HostResources res;
    res.cpu_count = readCpuCount();
    readMemoryInfo(res.total_memory, res.available_memory);
    res.disk_available = readDiskAvailable();
    res.cpu_usage_percent = readCpuUsage();
    return res;
}

ResourceUsage ResourceManager::getContainerUsage(const std::string& container_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    return readContainerUsage(container_id);
}

bool ResourceManager::checkResources(const ContainerConfig& config,
                                       const HostResources& resources) const {
    std::lock_guard<std::mutex> lock(mutex_);

    // CPU 检查: 限制核数不能超过宿主机核数
    if (config.resources.cpu_limit > 0 &&
        config.resources.cpu_limit > resources.cpu_count) {
        Logger::getInstance().warn("checkResources: cpu_limit exceeds host cpu_count");
        return false;
    }

    // 内存检查: 限制不能超过可用内存
    if (config.resources.memory_limit > 0 &&
        config.resources.memory_limit > resources.available_memory) {
        Logger::getInstance().warn("checkResources: memory_limit exceeds available memory");
        return false;
    }

    // 累计已预留资源, 判断是否仍有余量
    uint64_t reserved_memory = 0;
    int reserved_cpu = 0;
    for (const auto& kv : reserved_resources_) {
        reserved_memory += kv.second.memory_limit;
        reserved_cpu += kv.second.cpu_limit;
    }

    // 检查加上本次预留后是否超额
    uint64_t new_reserved_mem = reserved_memory + config.resources.memory_limit;
    int new_reserved_cpu = reserved_cpu + config.resources.cpu_limit;

    if (new_reserved_mem > resources.total_memory) {
        Logger::getInstance().warn("checkResources: total reserved memory exceeds total");
        return false;
    }
    if (new_reserved_cpu > resources.cpu_count) {
        Logger::getInstance().warn("checkResources: total reserved cpu exceeds cpu_count");
        return false;
    }
    return true;
}

bool ResourceManager::reserveResources(const std::string& container_id,
                                       const ContainerConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 检查是否已存在预留
    if (reserved_resources_.find(container_id) != reserved_resources_.end()) {
        Logger::getInstance().warn("reserveResources: already reserved: " + container_id);
        return false;
    }
    // 直接写入预留表 (资源充足性检查应由调用方通过 checkResources 提前完成)
    reserved_resources_[container_id] = config.resources;
    Logger::getInstance().info("reserveResources: " + container_id +
                                " mem=" + std::to_string(config.resources.memory_limit) +
                                " cpu=" + std::to_string(config.resources.cpu_limit));
    return true;
}

bool ResourceManager::releaseResources(const std::string& container_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = reserved_resources_.find(container_id);
    if (it == reserved_resources_.end()) {
        Logger::getInstance().warn("releaseResources: not reserved: " + container_id);
        return false;
    }
    reserved_resources_.erase(it);
    Logger::getInstance().info("releaseResources: " + container_id);
    return true;
}
