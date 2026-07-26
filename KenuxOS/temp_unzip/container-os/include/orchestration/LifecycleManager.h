#pragma once

// 容器生命周期管理器头文件
// 负责容器的创建、启动、停止、重启、销毁, 以及健康检查

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>

#include "core/Container.h"
#include "core/Runtime.h"
#include "core/IsolationManager.h"

// 健康检查回调签名: 返回 true 表示健康
// 参数为 container_id
using HealthCheckCallback = std::function<bool(const std::string& container_id)>;

// 容器生命周期管理器
// 协调 Runtime / IsolationManager 完成容器从创建到销毁的全过程
class LifecycleManager {
public:
    // 构造函数, 注入隔离管理器
    explicit LifecycleManager(IsolationManager* isolation_mgr);
    ~LifecycleManager();

    // 创建容器, 返回 container_id (失败返回空串)
    std::string createContainer(const ContainerConfig& config);

    // 启动容器
    bool startContainer(const std::string& container_id);

    // 停止容器, timeout_sec 为优雅停止超时
    bool stopContainer(const std::string& container_id, int timeout_sec = 10);

    // 重启容器
    bool restartContainer(const std::string& container_id);

    // 销毁容器
    bool destroyContainer(const std::string& container_id);

    // 获取容器对象指针 (不转移所有权)
    Container* getContainer(const std::string& container_id);

    // 列出所有容器
    std::vector<Container*> listContainers();

    // 启动健康检查
    // container_id: 容器 ID
    // callback:    健康检查回调, 返回 false 表示不健康
    // 返回:        启动成功返回 true
    bool startHealthCheck(const std::string& container_id, HealthCheckCallback callback);

    // 停止健康检查
    void stopHealthCheck(const std::string& container_id);

private:
    // 根据镜像名称选择运行时
    // 例如: "ubuntu:*" / "alpine:*" -> LinuxRuntime
    //       "windows:*" -> WindowsRuntime
    // 此处仅返回名称, 由具体运行时工厂注入
    Runtime* selectRuntime(const ContainerConfig& config);

    // 容器创建后的初始化 (命名空间、cgroup 等)
    bool initializeContainer(Container* container);

    // 容器销毁后的清理
    void cleanupContainer(const std::string& container_id);

    // 生成唯一的 container_id
    std::string generateContainerId() const;

    IsolationManager* isolation_mgr_;  // 隔离管理器 (非所有, 外部注入)

    // 容器表: container_id -> Container
    std::unordered_map<std::string, std::unique_ptr<Container>> containers_;

    // 健康检查运行标志: container_id -> 是否运行
    // 使用 unique_ptr 包装 atomic<bool>, 因为 atomic 不可拷贝/移动
    std::unordered_map<std::string, std::unique_ptr<std::atomic<bool>>> health_check_running_;

    // 健康检查线程表: container_id -> 线程对象
    std::unordered_map<std::string, std::thread> health_check_threads_;

    mutable std::mutex mutex_;
};
