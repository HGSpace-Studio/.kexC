#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <utility>
#include <cstdint>
#include <sys/types.h>  // pid_t

// 容器状态枚举
enum class ContainerStatus {
    Created,   // 已创建
    Running,   // 运行中
    Stopped,   // 已停止
    Deleted,   // 已删除
    Error      // 错误状态
};

// 资源限制配置子结构
struct ResourceLimits {
    int cpu_limit = 0;                // CPU 限制 (核数)
    int cpu_reservation = 0;          // CPU 预留 (核数)
    uint64_t memory_limit = 0;        // 内存限制 (字节)
    uint64_t memory_reservation = 0;  // 内存预留 (字节)
};

// 容器配置结构体
struct ContainerConfig {
    std::string name;                                                 // 容器名称
    std::string image;                                                // 镜像名称
    std::string command;                                              // 启动命令
    std::unordered_map<std::string, std::string> environment;         // 环境变量
    std::vector<std::pair<int, int>> port_mappings;                    // 端口映射 (host:container)
    std::vector<std::pair<std::string, std::string>> volume_mappings; // 卷映射 (host:container)
    ResourceLimits resources;                                         // 资源限制
};

// 容器类: 封装单个容器的生命周期与隔离环境管理
class Container {
public:
    Container(const std::string& id, const ContainerConfig& config);
    ~Container();

    // 启动容器
    bool start();
    // 停止容器, timeout_sec 为优雅停止的超时时间 (秒)
    bool stop(int timeout_sec = 10);
    // 重启容器
    bool restart();
    // 销毁容器
    bool destroy();

    // 获取容器日志
    std::string getLogs() const;
    // 获取容器状态
    ContainerStatus getStatus() const;
    // 获取容器详情, 返回 JSON 字符串
    std::string inspect() const;

    const std::string& getId() const { return id_; }
    const ContainerConfig& getConfig() const { return config_; }

private:
    std::string id_;                  // 容器唯一标识
    ContainerConfig config_;          // 容器配置
    ContainerStatus status_;          // 当前状态
    pid_t pid_;                       // 容器主进程 PID
    std::string logs_;               // 日志缓冲区

    // 隔离环境设置
    bool setupNamespace();            // 设置命名空间隔离
    bool setupCgroups();              // 设置 Cgroups 资源限制
    bool setupNetwork();              // 设置容器网络
    bool setupFilesystem();           // 设置容器文件系统
    void cleanup();                   // 清理容器资源

    // 内部辅助方法
    void appendLog(const std::string& msg);
    std::string statusToString(ContainerStatus status) const;
};
