#include "orchestration/LifecycleManager.h"
#include "utils/Logger.h"

#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>
#include <cstring>

#ifdef __linux__
#include <unistd.h>
#endif

LifecycleManager::LifecycleManager(IsolationManager* isolation_mgr)
    : isolation_mgr_(isolation_mgr) {
    Logger::getInstance().info("LifecycleManager initialized");
}

LifecycleManager::~LifecycleManager() {
    // 停止所有健康检查线程
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& kv : health_check_running_) {
            if (kv.second) {
                kv.second->store(false);
            }
        }
    }
    // 等待所有健康检查线程退出
    for (auto& kv : health_check_threads_) {
        if (kv.second.joinable()) {
            kv.second.join();
        }
    }
    // 销毁所有容器
    for (auto& kv : containers_) {
        if (kv.second) {
            kv.second->destroy();
        }
    }
    Logger::getInstance().info("LifecycleManager destroyed");
}

std::string LifecycleManager::generateContainerId() const {
    // 使用 时间戳 + 随机数 生成 64 位 hex 字符串
    // 格式: <unix_ms 高位(8 hex)><随机数(8 hex)> 共 16 字符
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    uint64_t ts = static_cast<uint64_t>(ms);

    std::random_device rd;
    std::mt19937_64 gen(rd());
    uint64_t rnd = gen();

    std::ostringstream oss;
    oss << std::hex << std::setfill('0')
        << std::setw(8) << (ts & 0xFFFFFFFF)
        << std::setw(8) << (rnd & 0xFFFFFFFF);
    return oss.str();
}

Runtime* LifecycleManager::selectRuntime(const ContainerConfig& config) {
    // TODO: 实际项目中应通过运行时工厂根据镜像名/平台选择具体 Runtime
    // 此处简化: 根据 image 前缀判断平台
    // - "ubuntu"/"alpine"/"debian"/"centos" 等 -> LinuxRuntime
    // - "windows" -> WindowsRuntime
    //
    // 当前未注入任何运行时实例, 返回 nullptr
    // 调用方应通过外部注入的 runtime map 处理
    (void)config;
    Logger::getInstance().warn("selectRuntime: no runtime registered, returning nullptr");
    return nullptr;
}

bool LifecycleManager::initializeContainer(Container* container) {
    if (!container) {
        return false;
    }
    if (!isolation_mgr_) {
        Logger::getInstance().warn("initializeContainer: no isolation manager");
        // 不视为错误, 仅跳过隔离初始化
        return true;
    }

    // 创建网络命名空间
    // TODO: 创建用户/PID/挂载等命名空间, 并通过 IsolationManager 设置 cgroup 限制
    int netns_fd = isolation_mgr_->createNamespace(container->getId(), NamespaceType::Network);
    if (netns_fd < 0) {
        Logger::getInstance().warn("initializeContainer: createNamespace(Network) failed for " +
                                    container->getId());
    }

    // 设置资源限制
    const auto& limits = container->getConfig().resources;
    if (limits.memory_limit > 0) {
        isolation_mgr_->setResourceLimit(container->getId(), "memory.max", limits.memory_limit);
    }
    if (limits.cpu_limit > 0) {
        // cgroup v2: cpu.max = "<quota> <period>", 简化为 limit*100000/100000
        uint64_t quota = static_cast<uint64_t>(limits.cpu_limit) * 100000;
        isolation_mgr_->setResourceLimit(container->getId(), "cpu.max", quota);
    }
    return true;
}

void LifecycleManager::cleanupContainer(const std::string& container_id) {
    if (isolation_mgr_) {
        isolation_mgr_->cleanup(container_id);
    }
}

std::string LifecycleManager::createContainer(const ContainerConfig& config) {
    std::string container_id = generateContainerId();

    // 选择运行时
    Runtime* runtime = selectRuntime(config);
    if (!runtime) {
        Logger::getInstance().error("createContainer: no suitable runtime for image " + config.image);
        return "";
    }

    // 通过运行时创建容器
    auto container = runtime->create(config);
    if (!container) {
        Logger::getInstance().error("createContainer: runtime create failed for " + config.name);
        return "";
    }
    // 设置容器 ID (运行时创建时通常会自动生成, 此处统一覆盖以保持唯一性)
    // 注意: Container 构造时已传入 id, 此处不再修改

    // 初始化容器隔离环境
    if (!initializeContainer(container.get())) {
        Logger::getInstance().error("createContainer: initializeContainer failed for " + container_id);
        cleanupContainer(container_id);
        return "";
    }

    std::lock_guard<std::mutex> lock(mutex_);
    containers_[container_id] = std::move(container);
    Logger::getInstance().info("createContainer: " + container_id + " name=" + config.name);
    return container_id;
}

bool LifecycleManager::startContainer(const std::string& container_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = containers_.find(container_id);
    if (it == containers_.end()) {
        Logger::getInstance().error("startContainer: not found: " + container_id);
        return false;
    }
    Container* container = it->second.get();

    // 选择运行时并启动
    Runtime* runtime = selectRuntime(container->getConfig());
    if (!runtime) {
        // 回退到容器自身的 start 方法
        bool ok = container->start();
        if (ok) {
            Logger::getInstance().info("startContainer (builtin): " + container_id);
        }
        return ok;
    }
    bool ok = runtime->start(*container);
    if (ok) {
        Logger::getInstance().info("startContainer: " + container_id);
    }
    return ok;
}

bool LifecycleManager::stopContainer(const std::string& container_id, int timeout_sec) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = containers_.find(container_id);
    if (it == containers_.end()) {
        Logger::getInstance().error("stopContainer: not found: " + container_id);
        return false;
    }
    Container* container = it->second.get();

    Runtime* runtime = selectRuntime(container->getConfig());
    if (!runtime) {
        bool ok = container->stop(timeout_sec);
        if (ok) {
            Logger::getInstance().info("stopContainer (builtin): " + container_id);
        }
        return ok;
    }
    bool ok = runtime->stop(*container, timeout_sec);
    if (ok) {
        Logger::getInstance().info("stopContainer: " + container_id);
    }
    return ok;
}

bool LifecycleManager::restartContainer(const std::string& container_id) {
    // 复用 stop + start 流程
    if (!stopContainer(container_id, 10)) {
        return false;
    }
    return startContainer(container_id);
}

bool LifecycleManager::destroyContainer(const std::string& container_id) {
    // 先停止健康检查
    stopHealthCheck(container_id);

    std::unique_ptr<Container> container;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = containers_.find(container_id);
        if (it == containers_.end()) {
            Logger::getInstance().error("destroyContainer: not found: " + container_id);
            return false;
        }
        container = std::move(it->second);
        containers_.erase(it);
    }

    // 调用运行时销毁
    if (container) {
        Runtime* runtime = selectRuntime(container->getConfig());
        if (runtime) {
            runtime->destroy(*container);
        } else {
            container->destroy();
        }
    }

    // 清理隔离资源
    cleanupContainer(container_id);
    Logger::getInstance().info("destroyContainer: " + container_id);
    return true;
}

Container* LifecycleManager::getContainer(const std::string& container_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = containers_.find(container_id);
    if (it == containers_.end()) {
        return nullptr;
    }
    return it->second.get();
}

std::vector<Container*> LifecycleManager::listContainers() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Container*> result;
    result.reserve(containers_.size());
    for (const auto& kv : containers_) {
        result.push_back(kv.second.get());
    }
    return result;
}

bool LifecycleManager::startHealthCheck(const std::string& container_id,
                                         HealthCheckCallback callback) {
    // 第一步: 若已有健康检查在运行, 先停止 (在锁内标记并取出线程)
    std::thread old_thread;
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // 容器必须存在
        if (containers_.find(container_id) == containers_.end()) {
            Logger::getInstance().error("startHealthCheck: container not found: " + container_id);
            return false;
        }

        // 标记旧的健康检查停止
        auto run_it = health_check_running_.find(container_id);
        if (run_it != health_check_running_.end() && run_it->second) {
            run_it->second->store(false);
        }
        // 取出旧的线程对象 (在锁外 join, 避免死锁)
        auto thr_it = health_check_threads_.find(container_id);
        if (thr_it != health_check_threads_.end()) {
            old_thread = std::move(thr_it->second);
            health_check_threads_.erase(thr_it);
        }
    }

    // 第二步: 在锁外等待旧线程退出
    if (old_thread.joinable()) {
        old_thread.join();
    }

    // 第三步: 在锁内创建新的健康检查任务
    std::atomic<bool>* flag_ptr = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // 再次校验容器仍存在
        if (containers_.find(container_id) == containers_.end()) {
            Logger::getInstance().error("startHealthCheck: container disappeared: " + container_id);
            return false;
        }

        // 创建新的运行标志
        auto flag = std::make_unique<std::atomic<bool>>(true);
        flag_ptr = flag.get();
        health_check_running_[container_id] = std::move(flag);

        // 启动健康检查线程
        health_check_threads_[container_id] = std::thread(
            [this, container_id, callback, flag_ptr]() {
                Logger::getInstance().info("HealthCheck started for " + container_id);
                // 健康检查间隔 (秒)
                constexpr int kIntervalSec = 5;
                while (flag_ptr->load()) {
                    bool healthy = false;
                    if (callback) {
                        healthy = callback(container_id);
                    }
                    if (!healthy) {
                        Logger::getInstance().warn("HealthCheck unhealthy: " + container_id);
                        // 不健康时执行重启 (此处简化: 仅记录日志)
                        // TODO: 触发容器重启策略
                    }
                    // 间隔等待, 支持被取消
                    for (int i = 0; i < kIntervalSec * 10 && flag_ptr->load(); ++i) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    }
                }
                Logger::getInstance().info("HealthCheck stopped for " + container_id);
            });
    }

    Logger::getInstance().info("startHealthCheck: " + container_id);
    return true;
}

void LifecycleManager::stopHealthCheck(const std::string& container_id) {
    // 第一步: 在锁内标记运行标志为 false, 并取出线程对象
    std::thread thread_to_join;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = health_check_running_.find(container_id);
        if (it != health_check_running_.end() && it->second) {
            it->second->store(false);
        }
        auto thr_it = health_check_threads_.find(container_id);
        if (thr_it != health_check_threads_.end()) {
            thread_to_join = std::move(thr_it->second);
            health_check_threads_.erase(thr_it);
        }
    }

    // 第二步: 在锁外等待线程退出, 避免回调中获取锁导致死锁
    if (thread_to_join.joinable()) {
        thread_to_join.join();
    }

    // 第三步: 清理运行标志
    std::lock_guard<std::mutex> lock(mutex_);
    health_check_running_.erase(container_id);
}
