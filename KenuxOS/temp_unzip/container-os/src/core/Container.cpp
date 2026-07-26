#include "core/Container.h"

#include <unistd.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <sched.h>
#include <fcntl.h>
#include <cstring>
#include <cstdio>
#include <sstream>

namespace {
// JSON 字符串转义, 保证 inspect() 输出合法 JSON
std::string escapeJsonString(const std::string& s) {
    std::string result;
    result.reserve(s.size() + 2);
    for (char c : s) {
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x",
                                  static_cast<unsigned char>(c));
                    result += buf;
                } else {
                    result += c;
                }
                break;
        }
    }
    return result;
}
}  // namespace

Container::Container(const std::string& id, const ContainerConfig& config)
    : id_(id), config_(config), status_(ContainerStatus::Created), pid_(-1) {}

Container::~Container() {
    // 析构时若容器仍在运行, 尝试停止以释放资源
    if (status_ == ContainerStatus::Running) {
        stop(0);
    }
}

bool Container::start() {
    if (status_ == ContainerStatus::Running) {
        appendLog("容器已在运行中");
        return true;
    }
    if (status_ != ContainerStatus::Created && status_ != ContainerStatus::Stopped) {
        appendLog("无法从当前状态启动容器");
        return false;
    }

    appendLog("正在启动容器: " + id_);

    // 1. 设置命名空间隔离
    if (!setupNamespace()) {
        status_ = ContainerStatus::Error;
        appendLog("命名空间设置失败");
        return false;
    }
    // 2. 设置 Cgroups 资源限制
    if (!setupCgroups()) {
        status_ = ContainerStatus::Error;
        appendLog("Cgroups 设置失败");
        return false;
    }
    // 3. 设置容器网络
    if (!setupNetwork()) {
        status_ = ContainerStatus::Error;
        appendLog("网络设置失败");
        return false;
    }
    // 4. 设置容器文件系统
    if (!setupFilesystem()) {
        status_ = ContainerStatus::Error;
        appendLog("文件系统设置失败");
        return false;
    }

    // TODO: 在自定义 OS 上实现容器主进程的 fork/exec
    // 当前为骨架实现, 实际应通过 clone/fork 创建子进程并在新命名空间中执行命令
    // pid_ = fork();
    // if (pid_ == 0) {
    //     // 子进程: 设置环境变量并执行命令
    //     execv(config_.command.c_str(), argv);
    //     _exit(127);
    // }

    status_ = ContainerStatus::Running;
    appendLog("容器启动成功, PID: " + std::to_string(pid_));
    return true;
}

bool Container::stop(int timeout_sec) {
    if (status_ != ContainerStatus::Running) {
        appendLog("容器未运行, 无需停止");
        return true;
    }

    appendLog("正在停止容器: " + id_ + ", 超时: " + std::to_string(timeout_sec) + "s");

    // TODO: 在自定义 OS 上实现进程信号停止逻辑
    // 1. 发送 SIGTERM 优雅停止
    // if (pid_ > 0) {
    //     kill(pid_, SIGTERM);
    // }
    // 2. 等待 timeout_sec 秒, 超时后发送 SIGKILL 强制停止
    // for (int i = 0; i < timeout_sec; ++i) {
    //     if (waitpid(pid_, nullptr, WNOHANG) == pid_) break;
    //     sleep(1);
    // }
    // if (pid_ > 0) {
    //     kill(pid_, SIGKILL);
    //     waitpid(pid_, nullptr, 0);
    // }

    cleanup();
    status_ = ContainerStatus::Stopped;
    appendLog("容器已停止");
    return true;
}

bool Container::restart() {
    appendLog("正在重启容器: " + id_);
    if (!stop(10)) {
        return false;
    }
    return start();
}

bool Container::destroy() {
    if (status_ == ContainerStatus::Deleted) {
        return true;
    }
    if (status_ == ContainerStatus::Running) {
        if (!stop(10)) {
            return false;
        }
    }
    cleanup();
    status_ = ContainerStatus::Deleted;
    appendLog("容器已销毁: " + id_);
    return true;
}

std::string Container::getLogs() const {
    return logs_;
}

ContainerStatus Container::getStatus() const {
    return status_;
}

std::string Container::inspect() const {
    std::ostringstream oss;
    oss << "{";
    oss << "\"id\":\"" << escapeJsonString(id_) << "\",";
    oss << "\"name\":\"" << escapeJsonString(config_.name) << "\",";
    oss << "\"image\":\"" << escapeJsonString(config_.image) << "\",";
    oss << "\"command\":\"" << escapeJsonString(config_.command) << "\",";
    oss << "\"status\":\"" << statusToString(status_) << "\",";
    oss << "\"pid\":" << static_cast<long long>(pid_) << ",";

    // 环境变量
    oss << "\"environment\":{";
    bool first = true;
    for (const auto& kv : config_.environment) {
        if (!first) oss << ",";
        oss << "\"" << escapeJsonString(kv.first) << "\":\""
            << escapeJsonString(kv.second) << "\"";
        first = false;
    }
    oss << "},";

    // 端口映射
    oss << "\"port_mappings\":[";
    for (size_t i = 0; i < config_.port_mappings.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "{\"host\":" << config_.port_mappings[i].first
            << ",\"container\":" << config_.port_mappings[i].second << "}";
    }
    oss << "],";

    // 卷映射
    oss << "\"volume_mappings\":[";
    for (size_t i = 0; i < config_.volume_mappings.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "{\"host\":\"" << escapeJsonString(config_.volume_mappings[i].first)
            << "\",\"container\":\""
            << escapeJsonString(config_.volume_mappings[i].second) << "\"}";
    }
    oss << "],";

    // 资源限制
    oss << "\"resources\":{";
    oss << "\"cpu_limit\":" << config_.resources.cpu_limit << ",";
    oss << "\"cpu_reservation\":" << config_.resources.cpu_reservation << ",";
    oss << "\"memory_limit\":" << config_.resources.memory_limit << ",";
    oss << "\"memory_reservation\":" << config_.resources.memory_reservation;
    oss << "}";

    oss << "}";
    return oss.str();
}

bool Container::setupNamespace() {
    appendLog("设置命名空间隔离...");
    // TODO: 在自定义 OS 上实现 unshare 创建命名空间
    // int flags = CLONE_NEWPID | CLONE_NEWNET | CLONE_NEWNS
    //            | CLONE_NEWUTS | CLONE_NEWIPC | CLONE_NEWUSER;
    // if (unshare(flags) != 0) {
    //     appendLog("unshare 失败: " + std::string(strerror(errno)));
    //     return false;
    // }
    return true;
}

bool Container::setupCgroups() {
    appendLog("设置 Cgroups 资源限制...");
    // TODO: 在自定义 OS 上实现 cgroup v2 创建与资源限制写入
    // std::string cgroup_path = "/sys/fs/cgroup/" + id_;
    // mkdir(cgroup_path.c_str(), 0755);
    // if (config_.resources.cpu_limit > 0) {
    //     writeCgroupFile(cgroup_path + "/cpu.max",
    //                     std::to_string(config_.resources.cpu_limit * 100000));
    // }
    // if (config_.resources.memory_limit > 0) {
    //     writeCgroupFile(cgroup_path + "/memory.max",
    //                     std::to_string(config_.resources.memory_limit));
    // }
    return true;
}

bool Container::setupNetwork() {
    appendLog("设置容器网络...");
    // TODO: 在自定义 OS 上实现 veth pair 创建与端口映射
    // 1. 创建 veth pair (veth_host / veth_container)
    // 2. 将 veth_container 移入容器网络命名空间
    // 3. 配置容器 IP 地址与默认路由
    // 4. 通过 iptables/NAT 设置端口映射
    return true;
}

bool Container::setupFilesystem() {
    appendLog("设置容器文件系统...");
    // TODO: 在自定义 OS 上实现 OverlayFS 挂载构建容器 rootfs
    // std::string options = "lowerdir=" + lower + ",upperdir=" + upper
    //                     + ",workdir=" + work;
    // if (mount("overlay", mount_point.c_str(), "overlay", 0, options.c_str()) != 0) {
    //     appendLog("overlay 挂载失败: " + std::string(strerror(errno)));
    //     return false;
    // }
    // 挂载卷映射 (bind mount)
    // for (const auto& vm : config_.volume_mappings) {
    //     mount(vm.first.c_str(), vm.second.c_str(), nullptr, MS_BIND, nullptr);
    // }
    return true;
}

void Container::cleanup() {
    appendLog("清理容器资源...");
    // TODO: 在自定义 OS 上卸载文件系统、删除 cgroup、清理网络设备
    // 1. 卸载 overlay 与 bind 挂载
    // 2. 删除 cgroup 目录
    // 3. 删除 veth pair 与 NAT 规则
    pid_ = -1;
}

void Container::appendLog(const std::string& msg) {
    logs_.append(msg);
    logs_.append("\n");
}

std::string Container::statusToString(ContainerStatus status) const {
    switch (status) {
        case ContainerStatus::Created:  return "created";
        case ContainerStatus::Running:  return "running";
        case ContainerStatus::Stopped:  return "stopped";
        case ContainerStatus::Deleted:  return "deleted";
        case ContainerStatus::Error:    return "error";
        default:                        return "unknown";
    }
}
