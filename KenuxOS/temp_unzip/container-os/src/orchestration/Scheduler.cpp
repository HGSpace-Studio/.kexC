#include "orchestration/Scheduler.h"
#include "utils/Logger.h"

#include <chrono>

Scheduler::Scheduler() {
    // 初始时刷新一次宿主机资源
    updateHostResources();
}

Scheduler::~Scheduler() {
    stop();
}

void Scheduler::start() {
    if (running_.load()) {
        return;
    }
    running_.store(true);
    scheduler_thread_ = std::thread(&Scheduler::schedulerLoop, this);
    Logger::getInstance().info("Scheduler started");
}

void Scheduler::stop() {
    if (!running_.load()) {
        return;
    }
    running_.store(false);
    queue_cv_.notify_all();
    if (scheduler_thread_.joinable()) {
        scheduler_thread_.join();
    }
    Logger::getInstance().info("Scheduler stopped");
}

void Scheduler::submit(const SchedulingRequest& request) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        queue_.push(request);
    }
    queue_cv_.notify_one();
    Logger::getInstance().info("Scheduler submit: container=" + request.container_id +
                                " image=" + request.config.image);
}

HostResources Scheduler::getHostResources() {
    // 实时获取最新宿主机资源
    HostResources fresh = resource_mgr_.getHostResources();
    {
        std::lock_guard<std::mutex> lock(resource_mutex_);
        host_resources_ = fresh;
    }
    return fresh;
}

void Scheduler::setPolicy(Policy policy) {
    policy_ = policy;
    Logger::getInstance().info("Scheduler policy set: " + std::to_string(static_cast<int>(policy)));
}

void Scheduler::schedulerLoop() {
    Logger::getInstance().info("Scheduler loop running");
    while (running_.load()) {
        SchedulingRequest req;
        bool has_request = false;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            // 等待请求或停止信号
            queue_cv_.wait_for(lock, std::chrono::seconds(1),
                                [this]() { return !queue_.empty() || !running_.load(); });
            if (!running_.load()) {
                break;
            }
            if (!queue_.empty()) {
                req = queue_.front();
                queue_.pop();
                has_request = true;
            }
        }

        if (!has_request) {
            // 空闲时定期刷新资源
            updateHostResources();
            continue;
        }

        // 刷新宿主机资源
        updateHostResources();
        HostResources current;
        {
            std::lock_guard<std::mutex> lock(resource_mutex_);
            current = host_resources_;
        }

        // 检查资源是否充足
        if (!checkResources(req.config, current)) {
            Logger::getInstance().warn("Scheduler: insufficient resources for " + req.container_id +
                                        ", will retry later");
            // 资源不足: 放回队列等待 (此处简化为丢弃, 实际可延迟重试)
            // TODO: 实现延迟重试或拒绝策略
            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                queue_.push(req);
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        // 选择运行时
        std::string runtime_name = selectRuntime(req);
        Logger::getInstance().info("Scheduler: selected runtime " + runtime_name +
                                    " for " + req.container_id);

        // 预留资源
        if (!resource_mgr_.reserveResources(req.container_id, req.config)) {
            Logger::getInstance().warn("Scheduler: reserveResources failed for " + req.container_id);
            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                queue_.push(req);
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        // TODO: 调用 LifecycleManager 创建并启动容器
        // 此处仅完成资源决策, 实际部署由上层编排器执行
        Logger::getInstance().info("Scheduler: scheduled " + req.container_id +
                                    " with runtime " + runtime_name);

        // 更新策略计数器
        rr_counter_.fetch_add(1);
    }
    Logger::getInstance().info("Scheduler loop exited");
}

bool Scheduler::checkResources(const ContainerConfig& config,
                                const HostResources& resources) const {
    // 委托给 ResourceManager
    return resource_mgr_.checkResources(config, resources);
}

std::string Scheduler::selectRuntime(const SchedulingRequest& request) const {
    // 优先使用请求中指定的运行时
    if (!request.preferred_runtime.empty()) {
        return request.preferred_runtime;
    }

    // 根据策略选择 (单机场景下, BestFit/FirstFit/RoundRobin 含义弱化)
    // TODO: 多运行时注册后, 根据镜像类型选择
    switch (policy_) {
        case Policy::RoundRobin: {
            // 轮询: 在已注册运行时中循环选择
            // 此处无运行时列表, 仅返回默认
            uint64_t idx = rr_counter_.load() % 2;
            return (idx == 0) ? "linux" : "windows";
        }
        case Policy::BestFit:
            // 最佳适配: 选择资源占用最匹配的运行时
            // 单机场景下退化为默认
            return "linux";
        case Policy::FirstFit:
        default:
            // 首次适配: 选择第一个可用的运行时
            return "linux";
    }
}

void Scheduler::updateHostResources() {
    HostResources fresh = resource_mgr_.getHostResources();
    std::lock_guard<std::mutex> lock(resource_mutex_);
    host_resources_ = fresh;
}
