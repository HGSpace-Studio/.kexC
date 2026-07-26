#pragma once

// 容器调度器头文件
// 接收容器部署请求, 根据策略 (FirstFit/BestFit/RoundRobin) 决策是否部署

#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>

#include "core/Container.h"
#include "orchestration/ResourceManager.h"

// 调度策略枚举
enum class Policy {
    FirstFit,    // 首次适配: 选择第一个资源充足的策略
    BestFit,     // 最佳适配: 选择最贴近需求的策略 (此处单机简化)
    RoundRobin   // 轮询: 按顺序轮流 (单机场景意义不大, 保留接口)
};

// 调度请求结构体
struct SchedulingRequest {
    std::string container_id;        // 容器 ID (可选, 空则由调度器生成)
    ContainerConfig config;          // 容器配置
    std::string preferred_runtime;   // 首选运行时名称
};

// 调度器
// 单机版本: 仅在本地决策资源是否充足, 多机扩展时通过子类扩展
class Scheduler {
public:
    Scheduler();
    ~Scheduler();

    // 启动调度线程
    void start();

    // 停止调度线程
    void stop();

    // 提交调度请求到队列
    void submit(const SchedulingRequest& request);

    // 获取当前宿主机资源 (实时)
    HostResources getHostResources();

    // 设置调度策略
    void setPolicy(Policy policy);

private:
    // 调度循环: 不断从队列取请求并处理
    void schedulerLoop();

    // 检查资源是否充足 (委托给 ResourceManager)
    bool checkResources(const ContainerConfig& config, const HostResources& resources) const;

    // 根据请求选择运行时名称
    std::string selectRuntime(const SchedulingRequest& request) const;

    // 更新宿主机资源缓存
    void updateHostResources();

    // 请求队列
    std::queue<SchedulingRequest> queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;

    // 调度线程控制
    std::atomic<bool> running_{false};
    std::thread scheduler_thread_;

    // 调度策略
    Policy policy_ = Policy::FirstFit;

    // RoundRobin 计数器 (策略使用)
    std::atomic<uint64_t> rr_counter_{0};

    // 资源管理器
    ResourceManager resource_mgr_;

    // 宿主机资源缓存 (定期刷新)
    HostResources host_resources_;
    mutable std::mutex resource_mutex_;
};
