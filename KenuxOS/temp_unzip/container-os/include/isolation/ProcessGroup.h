#pragma once

// 进程组管理头文件
// 通过 Unix 进程组 (process group) 机制管理容器内进程
// 便于对容器内所有进程统一发送信号

#include <string>
#include <unordered_map>
#include <vector>

#include <sys/types.h>  // pid_t 类型定义
#include <signal.h>     // killpg(), SIGKILL 等信号定义

// 进程组管理器
// 通过 setpgid()/killpg()/getpgid() 等系统调用管理容器进程组
class ProcessGroup {
public:
    ProcessGroup();
    ~ProcessGroup();

    // 为容器创建进程组
    // container_id: 容器标识
    // 返回: 成功返回 true
    bool create(const std::string& container_id);

    // 添加进程到容器的进程组
    // container_id: 容器标识
    // pid:          进程 ID
    // 返回: 成功返回 true
    bool addProcess(const std::string& container_id, pid_t pid);

    // 从容器进程组中移除进程
    // container_id: 容器标识
    // pid:          进程 ID
    // 返回: 成功返回 true
    bool removeProcess(const std::string& container_id, pid_t pid);

    // 向容器内所有进程发送信号
    // container_id: 容器标识
    // signal:       信号编号 (如 SIGTERM)
    // 返回: 成功返回 true
    bool signalAll(const std::string& container_id, int signal);

    // 终止容器内所有进程 (发送 SIGKILL)
    // container_id: 容器标识
    // 返回: 成功返回 true
    bool killAll(const std::string& container_id);

    // 获取容器进程组中的所有进程
    // container_id: 容器标识
    // 返回: 进程 PID 列表
    std::vector<pid_t> getProcesses(const std::string& container_id);

    // 清理容器进程组相关资源
    // container_id: 容器标识
    void cleanup(const std::string& container_id);

private:
    // 容器进程组信息
    struct ProcessGroupInfo {
        pid_t pgid;                    // 进程组 ID
        std::vector<pid_t> processes;  // 组内进程列表
    };

    // 容器进程组表: container_id -> 进程组信息
    std::unordered_map<std::string, ProcessGroupInfo> groups_;
};
