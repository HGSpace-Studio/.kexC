#pragma once

// 容器网络管理器头文件
// 负责容器网络模式管理、网桥创建、veth pair 创建、IP 地址分配、NAT/端口映射等

#include <string>
#include <vector>
#include <utility>
#include <unordered_map>
#include <cstdint>
#include <mutex>

// 网络模式枚举
// 对应 Docker 风格的四种容器网络模式
enum class NetworkMode {
    Bridge,     // 桥接模式: 通过虚拟网桥连接容器, 默认模式
    Host,       // 主机模式: 容器直接使用宿主机网络栈
    None,       // 无网络: 仅 loopback 接口
    Container   // 共享模式: 与另一容器共享网络命名空间
};

// 网络配置结构体
// 描述容器网络的配置信息, 由调用方传入 setupNetwork 使用
struct NetworkConfig {
    NetworkMode mode = NetworkMode::Bridge;          // 网络模式
    std::string bridge_name;                          // 网桥名称 (Bridge 模式)
    std::string ip_address;                           // 指定 IP 地址 (空表示自动分配)
    std::string subnet;                               // 子网, 例如 "10.0.0.0/24"
    std::string gateway;                              // 网关地址
    std::vector<std::pair<int, int>> port_mappings;   // 端口映射 (host_port, container_port)
};

// 容器网络信息结构体
// 描述已建立的容器网络状态
struct ContainerNetwork {
    std::string container_id;   // 容器 ID
    std::string veth_host;      // 宿主机侧 veth 设备名
    std::string veth_container; // 容器侧 veth 设备名
    std::string ip_address;     // 分配给容器的 IP
    std::string mac_address;   // 容器侧 MAC 地址
    int netns_fd = -1;         // 网络命名空间文件描述符 (-1 表示未建立)
};

// 网络管理器
// 管理容器网络配置、IP 池分配、veth 设备和 NAT 规则
class NetworkManager {
public:
    NetworkManager();
    ~NetworkManager();

    // 初始化网桥, 创建网桥设备并设置 IP
    // bridge_name: 网桥名称, 例如 "cbr0"
    // subnet:      子网, 例如 "10.0.0.0/24"
    // gateway:     网关地址, 例如 "10.0.0.1"
    // 返回:        成功返回 true
    bool initBridge(const std::string& bridge_name,
                    const std::string& subnet,
                    const std::string& gateway);

    // 为容器配置网络 (创建 veth pair, 分配 IP, 设置 NAT)
    // container_id: 容器 ID
    // config:       网络配置
    // 返回:         成功返回 true
    bool setupNetwork(const std::string& container_id, const NetworkConfig& config);

    // 拆除容器网络 (删除 veth pair, 释放 IP, 移除 NAT)
    // container_id: 容器 ID
    // 返回:         成功返回 true
    bool teardownNetwork(const std::string& container_id);

    // 添加端口映射 (动态添加, 不重建整个网络)
    // container_id:   容器 ID
    // host_port:      宿主机端口
    // container_port: 容器端口
    // 返回:           成功返回 true
    bool addPortMapping(const std::string& container_id,
                        int host_port,
                        int container_port);

    // 移除端口映射
    // container_id:   容器 ID
    // host_port:      宿主机端口
    // 返回:           成功返回 true
    bool removePortMapping(const std::string& container_id, int host_port);

    // 获取容器的网络信息
    // container_id: 容器 ID
    // 返回:          网络信息结构体, 未找到时返回默认结构
    ContainerNetwork getNetworkInfo(const std::string& container_id) const;

    // 列出所有已建立网络的容器 ID
    std::vector<std::string> listNetworks() const;

private:
    // 创建一对 veth 设备
    // name1, name2: 两个对端设备名 (长度需 <= 15)
    // 返回:          成功返回 true
    bool createVethPair(const std::string& name1, const std::string& name2);

    // 将指定网络设备移动到目标网络命名空间
    // dev_name:  设备名
    // netns_fd:  命名空间文件描述符
    // 返回:       成功返回 true
    bool moveToNetns(const std::string& dev_name, int netns_fd);

    // 为指定设备设置 IP 地址
    // dev_name: 设备名
    // ip:       IP/前缀, 例如 "10.0.0.2/24"
    // 返回:      成功返回 true
    bool setIpAddress(const std::string& dev_name, const std::string& ip);

    // 在指定网络命名空间中设置默认路由
    // netns_fd: 命名空间文件描述符
    // gateway:  网关地址
    // 返回:      成功返回 true
    bool setDefaultRoute(int netns_fd, const std::string& gateway);

    // 添加 NAT 规则 (MASQUERADE 或 DNAT)
    // container_id:   容器 ID
    // host_port:      宿主机端口 (0 表示仅添加 SNAT/MASQUERADE)
    // container_port: 容器端口 (0 表示仅 MASQUERADE)
    // 返回:           成功返回 true
    bool addNatRule(const std::string& container_id,
                    int host_port,
                    int container_port);

    // 移除 NAT 规则
    // container_id:   容器 ID
    // host_port:      宿主机端口
    // container_port: 容器端口
    // 返回:           成功返回 true
    bool removeNatRule(const std::string& container_id,
                       int host_port,
                       int container_port);

    // 从 IP 池中分配一个可用 IP
    // 返回: 分配的 IP 字符串, 失败返回空串
    std::string allocateIp();

    // 释放 IP 到池中
    // ip: 待释放的 IP
    void releaseIp(const std::string& ip);

    // 生成 veth 设备名 (基于容器 ID 截断)
    std::string generateVethName(const std::string& container_id, bool is_host) const;

    // 简单 IP 池实现: 管理 10.0.0.2 - 10.0.0.254
    // 使用位图记录已分配的 IP
    static constexpr int kIpPoolStart = 2;    // 起始主机号
    static constexpr int kIpPoolEnd = 254;   // 结束主机号
    std::vector<bool> ip_pool_;              // true 表示已分配

    // 网桥配置缓存
    std::string bridge_name_;
    std::string subnet_;
    std::string gateway_;

    // 容器网络信息表: container_id -> ContainerNetwork
    std::unordered_map<std::string, ContainerNetwork> networks_;

    // 多线程访问保护
    mutable std::mutex mutex_;
};
