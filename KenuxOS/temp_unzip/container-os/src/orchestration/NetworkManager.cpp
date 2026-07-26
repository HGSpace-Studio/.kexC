#include "orchestration/NetworkManager.h"
#include "utils/Logger.h"

#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <random>
#include <algorithm>

// Linux 网络相关头文件, 在自定义 OS 上可能不可用, 通过宏保护
#ifdef __linux__
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/if_link.h>
#endif

// 自定义 OS 上可能未定义 CLONE_NEWNET, 提供默认值
#ifndef CLONE_NEWNET
#define CLONE_NEWNET 0x40000000
#endif

// 执行 shell 命令的辅助宏, 失败时记录日志
// 自定义 OS 可能不支持 ip/iptables 命令, 失败时仅记录而不崩溃
static bool execCmd(const std::string& cmd) {
    Logger::getInstance().debug("NetworkManager exec: " + cmd);
    int ret = std::system(cmd.c_str());
    if (ret != 0) {
        Logger::getInstance().warn("NetworkManager cmd failed: " + cmd +
                                    " ret=" + std::to_string(ret));
        return false;
    }
    return true;
}

NetworkManager::NetworkManager() {
    // 初始化 IP 池: 所有 IP 均未分配
    ip_pool_.assign(kIpPoolEnd - kIpPoolStart + 1, false);
}

NetworkManager::~NetworkManager() {
    // 析构时关闭所有命名空间 fd
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& kv : networks_) {
        if (kv.second.netns_fd >= 0) {
            close(kv.second.netns_fd);
            kv.second.netns_fd = -1;
        }
    }
}

bool NetworkManager::initBridge(const std::string& bridge_name,
                                const std::string& subnet,
                                const std::string& gateway) {
    std::lock_guard<std::mutex> lock(mutex_);

    bridge_name_ = bridge_name;
    subnet_ = subnet;
    gateway_ = gateway;

    // 1. 创建网桥设备
    // TODO: 在自定义 OS 上替换为 netlink 调用 (RTM_NEWLINK)
    std::string cmd = "ip link add " + bridge_name + " type bridge";
    if (!execCmd(cmd)) {
        // 网桥可能已存在, 继续后续配置
        Logger::getInstance().warn("Bridge create failed, may already exist: " + bridge_name);
    }

    // 2. 设置网桥 IP (gateway/24)
    // 从 subnet 中提取前缀长度, 简化处理: 默认 24
    cmd = "ip addr add " + gateway_ + "/24 dev " + bridge_name;
    execCmd(cmd);

    // 3. 启用网桥设备
    cmd = "ip link set " + bridge_name + " up";
    execCmd(cmd);

    // 4. 开启转发并添加 MASQUERADE 规则 (使容器可以访问外部)
    execCmd("sysctl -w net.ipv4.ip_forward=1");
    // TODO: 使用 iptables-restore 或 nftables 替换
    execCmd("iptables -t nat -A POSTROUTING -s " + subnet_ + " -j MASQUERADE");

    Logger::getInstance().info("Bridge initialized: " + bridge_name +
                                " subnet=" + subnet + " gateway=" + gateway);
    return true;
}

bool NetworkManager::createVethPair(const std::string& name1, const std::string& name2) {
    // TODO: 使用 netlink RTM_NEWLINK 创建 veth pair 替代 ip 命令
    std::string cmd = "ip link add " + name1 +
                      " type veth peer name " + name2;
    return execCmd(cmd);
}

bool NetworkManager::moveToNetns(const std::string& dev_name, int netns_fd) {
    (void)netns_fd;
    // TODO: 使用 setns + netlink 将设备移动到目标网络命名空间
    // 这里简化为通过 pid (pid 由 /proc/<pid>/ns/net 派生), 此处通过 fd 也支持
    // 注意: ip link set <dev> netns <fd> 需要 ip 命令支持 fd 形式
    std::string cmd = "ip link set " + dev_name +
                      " netns " + std::to_string(netns_fd);
    return execCmd(cmd);
}

bool NetworkManager::setIpAddress(const std::string& dev_name, const std::string& ip) {
    std::string cmd = "ip addr add " + ip + " dev " + dev_name;
    return execCmd(cmd);
}

bool NetworkManager::setDefaultRoute(int netns_fd, const std::string& gateway) {
    (void)netns_fd;
    // TODO: 进入目标命名空间后添加默认路由, 此处仅占位
    // 实际实现需要先 setns(netns_fd, CLONE_NEWNET), 然后 ip route add default via <gateway>
    // 之后 setns 回原命名空间
    std::string cmd = "ip route add default via " + gateway;
    return execCmd(cmd);
}

bool NetworkManager::addNatRule(const std::string& container_id,
                                 int host_port,
                                 int container_port) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = networks_.find(container_id);
    if (it == networks_.end()) {
        Logger::getInstance().error("addNatRule: container not found: " + container_id);
        return false;
    }
    const std::string& container_ip = it->second.ip_address;

    // 仅添加 MASQUERADE (出向 SNAT)
    if (host_port == 0 || container_port == 0) {
        std::string cmd = "iptables -t nat -A POSTROUTING -s " + container_ip +
                          " -j MASQUERADE";
        return execCmd(cmd);
    }

    // DNAT: 宿主机 host_port 流量转发到 container_ip:container_port
    // TODO: 使用 nftables 替换 iptables
    std::string dnat = "iptables -t nat -A PREROUTING -p tcp --dport " +
                       std::to_string(host_port) +
                       " -j DNAT --to-destination " + container_ip + ":" +
                       std::to_string(container_port);
    return execCmd(dnat);
}

bool NetworkManager::removeNatRule(const std::string& container_id,
                                    int host_port,
                                    int container_port) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = networks_.find(container_id);
    if (it == networks_.end()) {
        return false;
    }
    const std::string& container_ip = it->second.ip_address;

    if (host_port == 0 || container_port == 0) {
        std::string cmd = "iptables -t nat -D POSTROUTING -s " + container_ip +
                          " -j MASQUERADE";
        return execCmd(cmd);
    }

    std::string dnat = "iptables -t nat -D PREROUTING -p tcp --dport " +
                       std::to_string(host_port) +
                       " -j DNAT --to-destination " + container_ip + ":" +
                       std::to_string(container_port);
    return execCmd(dnat);
}

std::string NetworkManager::allocateIp() {
    // 在 IP 池中查找第一个未分配的 IP
    for (int i = kIpPoolStart; i <= kIpPoolEnd; ++i) {
        int idx = i - kIpPoolStart;
        if (!ip_pool_[idx]) {
            ip_pool_[idx] = true;
            // 固定使用 10.0.0.x 网段
            return "10.0.0." + std::to_string(i);
        }
    }
    Logger::getInstance().error("IP pool exhausted (10.0.0.2-254)");
    return "";
}

void NetworkManager::releaseIp(const std::string& ip) {
    // 解析最后一段数字
    auto pos = ip.rfind('.');
    if (pos == std::string::npos) return;
    int host = std::atoi(ip.substr(pos + 1).c_str());
    if (host < kIpPoolStart || host > kIpPoolEnd) return;
    ip_pool_[host - kIpPoolStart] = false;
}

std::string NetworkManager::generateVethName(const std::string& container_id,
                                              bool is_host) const {
    // veth 设备名最多 15 字符
    // host 侧: v + 容器 ID 前 7 位; container 侧: c + 容器 ID 前 7 位
    std::string prefix = is_host ? "v" : "c";
    std::string id_part = container_id.substr(0, std::min<size_t>(container_id.size(), 7));
    // 过滤非法字符 (只保留字母数字), 避免命名空间 ID 含横杠
    std::string filtered;
    for (char c : id_part) {
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            filtered += c;
        }
    }
    return prefix + filtered;
}

bool NetworkManager::setupNetwork(const std::string& container_id,
                                  const NetworkConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 已存在网络信息, 先清理
    if (networks_.find(container_id) != networks_.end()) {
        Logger::getInstance().warn("setupNetwork: container already has network, will skip: " + container_id);
        return false;
    }

    ContainerNetwork info;
    info.container_id = container_id;

    // Host 模式: 直接使用宿主机网络栈, 不创建 veth
    if (config.mode == NetworkMode::Host) {
        info.veth_host = "";
        info.veth_container = "";
        info.ip_address = gateway_;  // 复用宿主机 IP
        info.mac_address = "";
        info.netns_fd = -1;
        networks_[container_id] = info;
        Logger::getInstance().info("setupNetwork Host mode: " + container_id);
        return true;
    }

    // None 模式: 仅创建空命名空间
    if (config.mode == NetworkMode::None) {
        // TODO: 创建空网络命名空间
        info.netns_fd = -1;
        networks_[container_id] = info;
        Logger::getInstance().info("setupNetwork None mode: " + container_id);
        return true;
    }

    // Container 模式: 共享另一容器的网络命名空间
    // TODO: 实现 Container 模式 (引用目标容器 netns_fd)
    if (config.mode == NetworkMode::Container) {
        Logger::getInstance().warn("Container network mode not yet implemented");
        info.netns_fd = -1;
        networks_[container_id] = info;
        return false;
    }

    // Bridge 模式: 创建 veth pair, 分配 IP, 桥接到网桥
    info.veth_host = generateVethName(container_id, true);
    info.veth_container = generateVethName(container_id, false);

    // 1. 创建 veth pair
    if (!createVethPair(info.veth_host, info.veth_container)) {
        Logger::getInstance().error("createVethPair failed for " + container_id);
        return false;
    }

    // 2. 分配 IP
    if (config.ip_address.empty()) {
        info.ip_address = allocateIp();
        if (info.ip_address.empty()) {
            // 分配失败, 回滚 veth
            execCmd("ip link del " + info.veth_host);
            return false;
        }
    } else {
        info.ip_address = config.ip_address;
        // 标记 IP 为已分配
        releaseIp(config.ip_address);  // 先释放以确保不会被 allocateIp 二次分配
        auto pos = config.ip_address.rfind('.');
        if (pos != std::string::npos) {
            int host = std::atoi(config.ip_address.substr(pos + 1).c_str());
            if (host >= kIpPoolStart && host <= kIpPoolEnd) {
                ip_pool_[host - kIpPoolStart] = true;
            }
        }
    }

    // 3. 创建/进入容器网络命名空间
    // TODO: 使用 unshare(CLONE_NEWNET) 创建命名空间, 通过 /proc/self/ns/net 获取 fd
    // 这里简化处理: 不实际创建 netns, veth 留在宿主机上
    info.netns_fd = -1;

    // 4. 将 veth_container 一端移入容器命名空间 (此处跳过)
    // if (info.netns_fd >= 0) {
    //     moveToNetns(info.veth_container, info.netns_fd);
    // }

    // 5. 设置容器侧 veth 的 IP (简化: 直接在宿主机侧设置)
    if (!setIpAddress(info.veth_container, info.ip_address + "/24")) {
        Logger::getInstance().warn("setIpAddress failed for " + info.veth_container);
    }

    // 6. 启用两端 veth 设备
    execCmd("ip link set " + info.veth_host + " up");
    execCmd("ip link set " + info.veth_container + " up");

    // 7. 将 host 端 veth 加入网桥
    if (!bridge_name_.empty()) {
        execCmd("ip link set " + info.veth_host + " master " + bridge_name_);
    }

    // 8. 生成随机 MAC (此处仅占位, 实际 MAC 由内核自动分配)
    info.mac_address = "02:42:" + info.ip_address;  // 简化: 基于 IP

    // 9. 设置默认路由 (在容器命名空间内)
    if (info.netns_fd >= 0 && !gateway_.empty()) {
        setDefaultRoute(info.netns_fd, gateway_);
    }

    // 10. 添加 MASQUERADE
    addNatRule(container_id, 0, 0);

    // 11. 处理端口映射
    for (const auto& pm : config.port_mappings) {
        addNatRule(container_id, pm.first, pm.second);
    }

    networks_[container_id] = info;
    Logger::getInstance().info("setupNetwork Bridge mode: " + container_id +
                                " ip=" + info.ip_address);
    return true;
}

bool NetworkManager::teardownNetwork(const std::string& container_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = networks_.find(container_id);
    if (it == networks_.end()) {
        return false;
    }

    ContainerNetwork& info = it->second;

    // 1. 移除所有 NAT 规则 (MASQUERADE)
    removeNatRule(container_id, 0, 0);

    // 2. 删除 host 侧 veth (会自动删除 container 侧对端)
    if (!info.veth_host.empty()) {
        execCmd("ip link del " + info.veth_host);
    }

    // 3. 关闭命名空间 fd
    if (info.netns_fd >= 0) {
        close(info.netns_fd);
        info.netns_fd = -1;
    }

    // 4. 释放 IP
    if (!info.ip_address.empty()) {
        releaseIp(info.ip_address);
    }

    networks_.erase(it);
    Logger::getInstance().info("teardownNetwork: " + container_id);
    return true;
}

bool NetworkManager::addPortMapping(const std::string& container_id,
                                     int host_port,
                                     int container_port) {
    // 动态添加 DNAT 规则
    return addNatRule(container_id, host_port, container_port);
}

bool NetworkManager::removePortMapping(const std::string& container_id, int host_port) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = networks_.find(container_id);
    if (it == networks_.end()) {
        return false;
    }
    const std::string& container_ip = it->second.ip_address;
    // 简化: 仅按 host_port 删除 DNAT 规则 (假设容器端口与添加时一致)
    std::string dnat = "iptables -t nat -D PREROUTING -p tcp --dport " +
                       std::to_string(host_port) +
                       " -j DNAT --to-destination " + container_ip;
    return execCmd(dnat);
}

ContainerNetwork NetworkManager::getNetworkInfo(const std::string& container_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = networks_.find(container_id);
    if (it != networks_.end()) {
        return it->second;
    }
    // 未找到返回默认结构
    return ContainerNetwork{};
}

std::vector<std::string> NetworkManager::listNetworks() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;
    result.reserve(networks_.size());
    for (const auto& kv : networks_) {
        result.push_back(kv.first);
    }
    return result;
}
