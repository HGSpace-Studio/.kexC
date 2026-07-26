#include "isolation/ProcessGroup.h"

#include <unistd.h>     // setpgid(), getpgid()
#include <cerrno>
#include <cstring>

ProcessGroup::ProcessGroup() = default;

ProcessGroup::~ProcessGroup() = default;

bool ProcessGroup::create(const std::string& container_id) {
    // 检查该容器是否已存在进程组
    if (groups_.find(container_id) != groups_.end()) {
        return false;
    }

    // 创建进程组: 以当前进程作为组长
    // setpgid(0, 0) 将当前进程设置为新的进程组组长，PGID 等于当前 PID
    // TODO: 自定义 OS 上 setpgid() 可能尚未实现，需内核侧补充
    if (::setpgid(0, 0) != 0) {
        return false;
    }

    ProcessGroupInfo info;
    // 获取当前进程组 ID 作为容器进程组的 PGID
    // TODO: 自定义 OS 上 getpgid() 可能尚未实现，需内核侧补充
    info.pgid = ::getpgid(0);
    if (info.pgid < 0) {
        return false;
    }

    groups_[container_id] = info;
    return true;
}

bool ProcessGroup::addProcess(const std::string& container_id, pid_t pid) {
    auto it = groups_.find(container_id);
    if (it == groups_.end()) {
        return false;
    }

    // 将指定进程加入该容器的进程组
    // setpgid(pid, pgid) 将进程 pid 的进程组设为 pgid
    // TODO: 自定义 OS 上 setpgid() 可能尚未实现，需内核侧补充
    if (::setpgid(pid, it->second.pgid) != 0) {
        return false;
    }

    // 记录到进程列表 (避免重复记录同一进程)
    for (pid_t existing : it->second.processes) {
        if (existing == pid) {
            return true;
        }
    }
    it->second.processes.push_back(pid);
    return true;
}

bool ProcessGroup::removeProcess(const std::string& container_id, pid_t pid) {
    auto it = groups_.find(container_id);
    if (it == groups_.end()) {
        return false;
    }

    // 仅从本地记录列表中移除 (不改变进程的实际进程组归属)
    auto& procs = it->second.processes;
    for (auto pit = procs.begin(); pit != procs.end(); ++pit) {
        if (*pit == pid) {
            procs.erase(pit);
            return true;
        }
    }
    return false;
}

bool ProcessGroup::signalAll(const std::string& container_id, int signal) {
    auto it = groups_.find(container_id);
    if (it == groups_.end()) {
        return false;
    }

    // 向整个进程组发送信号: killpg(pgid, signal)
    // killpg() 会向进程组中的所有进程发送指定信号
    // TODO: 自定义 OS 上 killpg() 可能尚未实现，需内核侧补充
    if (::killpg(it->second.pgid, signal) != 0) {
        return false;
    }
    return true;
}

bool ProcessGroup::killAll(const std::string& container_id) {
    // 发送 SIGKILL 强制终止容器内所有进程
    return signalAll(container_id, SIGKILL);
}

std::vector<pid_t> ProcessGroup::getProcesses(const std::string& container_id) {
    auto it = groups_.find(container_id);
    if (it == groups_.end()) {
        return {};
    }
    return it->second.processes;
}

void ProcessGroup::cleanup(const std::string& container_id) {
    // 清理: 先尝试向组内所有进程发送 SIGTERM，再移除本地记录
    signalAll(container_id, SIGTERM);
    groups_.erase(container_id);
}
