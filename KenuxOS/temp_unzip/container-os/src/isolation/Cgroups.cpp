#include "isolation/Cgroups.h"

#include <sys/stat.h>   // mkdir(), stat()
#include <unistd.h>     // rmdir()
#include <cerrno>
#include <cstring>
#include <sstream>
#include <fstream>

CgroupsManager::CgroupsManager()
    : cgroup_root_("/sys/fs/cgroup/"),
      version_(CgroupVersion::V2) {
    // 构造时自动检测当前系统的 cgroup 版本
    version_ = detectCgroupVersion();
}

CgroupsManager::~CgroupsManager() {
    // 析构时清理所有残留的容器 cgroup 目录，忽略错误
    for (auto& pair : cgroup_paths_) {
        ::rmdir(pair.second.c_str());
    }
}

CgroupVersion CgroupsManager::detectCgroupVersion() {
    // cgroup v2 通过 /sys/fs/cgroup/cgroup.controllers 文件标识统一层级
    // TODO: 自定义 OS 上 /sys/fs/cgroup 挂载点可能不存在，需内核侧适配
    struct stat st;
    if (::stat("/sys/fs/cgroup/cgroup.controllers", &st) == 0) {
        return CgroupVersion::V2;
    }
    // 检测 v1: 存在子系统目录如 /sys/fs/cgroup/cpu
    if (::stat("/sys/fs/cgroup/cpu", &st) == 0) {
        return CgroupVersion::V1;
    }
    // 默认返回 v2
    return CgroupVersion::V2;
}

std::string CgroupsManager::getCgroupPath(const std::string& container_id) const {
    // v2 路径: /sys/fs/cgroup/<container_id>/
    return cgroup_root_ + container_id + "/";
}

bool CgroupsManager::createCgroup(const std::string& container_id) {
    std::string path = getCgroupPath(container_id);

    // 创建容器 cgroup 目录 (在 cgroup 文件系统上创建目录即创建 cgroup)
    // TODO: 自定义 OS 上 mkdir() 对 cgroup 文件系统的支持需内核实现
    if (::mkdir(path.c_str(), 0755) != 0) {
        if (errno != EEXIST) {
            return false;
        }
    }

    cgroup_paths_[container_id] = path;
    return true;
}

bool CgroupsManager::writeCgroupFile(const std::string& path, const std::string& value) {
    // 使用 fstream 写入 cgroup 控制文件
    std::ofstream ofs(path);
    if (!ofs.is_open()) {
        return false;
    }
    ofs << value;
    return ofs.good();
}

std::string CgroupsManager::readCgroupFile(const std::string& path) const {
    // 读取 cgroup 控制文件内容 (单行)
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        return "";
    }
    std::string content;
    std::getline(ifs, content);
    return content;
}

bool CgroupsManager::setCpuLimit(const std::string& container_id, int cpu_count) {
    auto it = cgroup_paths_.find(container_id);
    if (it == cgroup_paths_.end()) {
        return false;
    }

    if (version_ == CgroupVersion::V2) {
        // v2: cpu.max 文件，格式为 "$QUOTA $PERIOD"
        // 例如 2 个 CPU: "200000 100000" (quota=2*100000, period=100000 => 2 核)
        // TODO: 自定义 OS 需实现 cpu.max 控制文件
        std::string value = std::to_string(cpu_count * 100000) + " 100000";
        return writeCgroupFile(it->second + "cpu.max", value);
    } else {
        // v1: 分别写入 cpu.cfs_quota_us 与 cpu.cfs_period_us
        // TODO: v1 兼容，自定义 OS 暂未支持
        bool ok1 = writeCgroupFile(it->second + "cpu.cfs_quota_us",
                                   std::to_string(cpu_count * 100000));
        bool ok2 = writeCgroupFile(it->second + "cpu.cfs_period_us", "100000");
        return ok1 && ok2;
    }
}

bool CgroupsManager::setMemoryLimit(const std::string& container_id, uint64_t bytes) {
    auto it = cgroup_paths_.find(container_id);
    if (it == cgroup_paths_.end()) {
        return false;
    }

    if (version_ == CgroupVersion::V2) {
        // v2: memory.max 文件
        // TODO: 自定义 OS 需实现 memory.max 控制文件
        return writeCgroupFile(it->second + "memory.max", std::to_string(bytes));
    } else {
        // v1: memory.limit_in_bytes
        // TODO: v1 兼容
        return writeCgroupFile(it->second + "memory.limit_in_bytes", std::to_string(bytes));
    }
}

bool CgroupsManager::setBlkioLimit(const std::string& container_id, uint64_t bps) {
    auto it = cgroup_paths_.find(container_id);
    if (it == cgroup_paths_.end()) {
        return false;
    }

    if (version_ == CgroupVersion::V2) {
        // v2: io.max 文件，格式为 "$MAJ:$MIN rbps=$BPS wbps=$BPS"
        // 此处使用占位设备号 8:0 (典型 SCSI 磁盘主从设备号)
        // TODO: 自定义 OS 需实现 io.max 控制文件，且需传入正确的设备号
        std::string value = "8:0 rbps=" + std::to_string(bps) +
                            " wbps=" + std::to_string(bps);
        return writeCgroupFile(it->second + "io.max", value);
    } else {
        // v1: blkio.throttle.read_bps_device / write_bps_device
        // 格式: "$MAJ:$MIN $BPS"
        // TODO: v1 兼容
        std::string value = "8:0 " + std::to_string(bps);
        bool ok1 = writeCgroupFile(it->second + "blkio.throttle.read_bps_device", value);
        bool ok2 = writeCgroupFile(it->second + "blkio.throttle.write_bps_device", value);
        return ok1 && ok2;
    }
}

bool CgroupsManager::getPidsLimit(const std::string& container_id, int max_pids) {
    auto it = cgroup_paths_.find(container_id);
    if (it == cgroup_paths_.end()) {
        return false;
    }

    // 写入最大进程数限制
    if (version_ == CgroupVersion::V2) {
        // v2: pids.max 文件
        // TODO: 自定义 OS 需实现 pids.max 控制文件
        return writeCgroupFile(it->second + "pids.max", std::to_string(max_pids));
    } else {
        // v1: pids 子系统下的 pids.max
        // TODO: v1 兼容
        return writeCgroupFile(it->second + "pids.max", std::to_string(max_pids));
    }
}

ResourceUsage CgroupsManager::getResourceUsage(const std::string& container_id) {
    // 初始化为全 0
    ResourceUsage usage = {0, 0, 0, 0, 0};

    auto it = cgroup_paths_.find(container_id);
    if (it == cgroup_paths_.end()) {
        return usage;
    }

    if (version_ == CgroupVersion::V2) {
        // v2: 读取 cpu.stat 中的 usage_usec (微秒)，转换为纳秒
        // 格式: "usage_usec <value>"
        // TODO: 自定义 OS 需实现 cpu.stat 文件
        std::ifstream cpu_ifs(it->second + "cpu.stat");
        if (cpu_ifs.is_open()) {
            std::string line;
            while (std::getline(cpu_ifs, line)) {
                if (line.find("usage_usec") == 0) {
                    uint64_t usec = 0;
                    std::istringstream iss(line.substr(std::string("usage_usec").length()));
                    iss >> usec;
                    usage.cpu_time_ns = usec * 1000;  // 微秒转纳秒
                    break;
                }
            }
        }

        // 读取 memory.current (当前内存使用量)
        // TODO: 自定义 OS 需实现 memory.current 文件
        std::string mem_current = readCgroupFile(it->second + "memory.current");
        if (!mem_current.empty()) {
            usage.memory_usage = std::stoull(mem_current);
        }

        // 读取 memory.peak (历史最大内存使用量)
        // TODO: 自定义 OS 需实现 memory.peak 文件
        std::string mem_peak = readCgroupFile(it->second + "memory.peak");
        if (!mem_peak.empty()) {
            usage.memory_max_usage = std::stoull(mem_peak);
        }

        // 读取 io.stat (I/O 统计，累加所有设备)
        // 格式: "$MAJ:$MIN rbytes=<n> wbytes=<n> rios=<n> wios=<n>"
        // TODO: 自定义 OS 需实现 io.stat 文件
        std::ifstream io_ifs(it->second + "io.stat");
        if (io_ifs.is_open()) {
            std::string line;
            while (std::getline(io_ifs, line)) {
                const std::string rkey = "rbytes=";
                const std::string wkey = "wbytes=";
                size_t rpos = line.find(rkey);
                size_t wpos = line.find(wkey);
                if (rpos != std::string::npos) {
                    std::istringstream iss(line.substr(rpos + rkey.length()));
                    uint64_t rbytes = 0;
                    iss >> rbytes;
                    usage.io_read_bytes += rbytes;
                }
                if (wpos != std::string::npos) {
                    std::istringstream iss(line.substr(wpos + wkey.length()));
                    uint64_t wbytes = 0;
                    iss >> wbytes;
                    usage.io_write_bytes += wbytes;
                }
            }
        }
    } else {
        // v1: 读取各子系统独立的统计文件
        // TODO: v1 兼容，自定义 OS 暂未支持
        std::string cpuacct = readCgroupFile(it->second + "cpuacct.usage");
        if (!cpuacct.empty()) {
            usage.cpu_time_ns = std::stoull(cpuacct);
        }
        std::string mem_usage = readCgroupFile(it->second + "memory.usage_in_bytes");
        if (!mem_usage.empty()) {
            usage.memory_usage = std::stoull(mem_usage);
        }
        std::string mem_max = readCgroupFile(it->second + "memory.max_usage_in_bytes");
        if (!mem_max.empty()) {
            usage.memory_max_usage = std::stoull(mem_max);
        }
    }

    return usage;
}

bool CgroupsManager::removeCgroup(const std::string& container_id) {
    auto it = cgroup_paths_.find(container_id);
    if (it == cgroup_paths_.end()) {
        return false;
    }

    // 移除 cgroup 目录 (cgroup 目录必须无子 cgroup 且无进程才能删除)
    // TODO: 自定义 OS 上 rmdir() 对 cgroup 文件系统的支持需内核实现
    if (::rmdir(it->second.c_str()) != 0) {
        return false;
    }

    cgroup_paths_.erase(it);
    return true;
}
