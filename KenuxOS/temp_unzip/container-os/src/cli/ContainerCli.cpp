#include "cli/ContainerCli.h"
#include "utils/Logger.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>

// ====== ContainerCli 实现 ======

ContainerCli::ContainerCli(LifecycleManager* lifecycle, ImageManager* image_mgr,
                           NetworkManager* network_mgr)
    : lifecycle_(lifecycle), image_mgr_(image_mgr), network_mgr_(network_mgr) {
    Logger::getInstance().info("ContainerCli initialized");
}

ContainerCli::~ContainerCli() = default;

int ContainerCli::run(int argc, char* argv[]) {
    if (argc < 2) {
        printHelp();
        return 1;
    }

    std::string cmd = argv[1];

    if (cmd == "container") {
        return handleContainer(argc, argv);
    }
    if (cmd == "image") {
        return handleImage(argc, argv);
    }
    if (cmd == "network") {
        return handleNetwork(argc, argv);
    }
    if (cmd == "info") {
        return handleInfo();
    }
    if (cmd == "help" || cmd == "--help" || cmd == "-h") {
        printHelp();
        return 0;
    }

    std::printf("未知命令: %s\n", cmd.c_str());
    printHelp();
    return 1;
}

// ====== 容器子命令 ======

int ContainerCli::handleContainer(int argc, char* argv[]) {
    if (argc < 3) {
        std::printf("用法: %s container <create|start|stop|restart|rm|ls|logs|inspect> ...\n",
                    argc > 0 ? argv[0] : "container-cli");
        return 1;
    }

    std::string sub = argv[2];

    // container create <image> [options]
    if (sub == "create") {
        if (argc < 4) {
            std::printf("用法: container create <image> [--name <name>] "
                        "[--cpu <n>] [--memory <bytes>] "
                        "[-e KEY=VAL] [-p host:container] [-v host:container]\n");
            return 1;
        }
        if (!lifecycle_) {
            std::printf("错误: LifecycleManager 不可用\n");
            return 1;
        }

        ContainerConfig config;
        config.image = argv[3];

        // 解析可选参数
        for (int i = 4; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--name" && i + 1 < argc) {
                config.name = argv[++i];
            } else if (arg == "--cpu" && i + 1 < argc) {
                config.resources.cpu_limit = std::atoi(argv[++i]);
            } else if (arg == "--memory" && i + 1 < argc) {
                config.resources.memory_limit = std::strtoull(argv[++i], nullptr, 10);
            } else if (arg == "-e" && i + 1 < argc) {
                std::string key, val;
                if (parseEnv(argv[++i], key, val)) {
                    config.environment[key] = val;
                } else {
                    std::printf("警告: 忽略无效环境变量: %s\n", argv[i]);
                }
            } else if (arg == "-p" && i + 1 < argc) {
                int host_port = 0, container_port = 0;
                if (parsePort(argv[++i], host_port, container_port)) {
                    config.port_mappings.emplace_back(host_port, container_port);
                } else {
                    std::printf("警告: 忽略无效端口映射: %s\n", argv[i]);
                }
            } else if (arg == "-v" && i + 1 < argc) {
                std::string host_path, container_path;
                if (parseVolume(argv[++i], host_path, container_path)) {
                    config.volume_mappings.emplace_back(host_path, container_path);
                } else {
                    std::printf("警告: 忽略无效卷映射: %s\n", argv[i]);
                }
            } else {
                std::printf("警告: 未知参数: %s\n", arg.c_str());
            }
        }

        std::string id = lifecycle_->createContainer(config);
        if (id.empty()) {
            std::printf("错误: 创建容器失败\n");
            return 1;
        }
        std::printf("%s\n", id.c_str());
        return 0;
    }

    // container start <id>
    if (sub == "start") {
        if (argc < 4) {
            std::printf("用法: container start <id>\n");
            return 1;
        }
        if (!lifecycle_) {
            std::printf("错误: LifecycleManager 不可用\n");
            return 1;
        }
        std::string id = argv[3];
        if (!lifecycle_->startContainer(id)) {
            std::printf("错误: 启动容器失败: %s\n", id.c_str());
            return 1;
        }
        std::printf("%s\n", id.c_str());
        return 0;
    }

    // container stop <id>
    if (sub == "stop") {
        if (argc < 4) {
            std::printf("用法: container stop <id>\n");
            return 1;
        }
        if (!lifecycle_) {
            std::printf("错误: LifecycleManager 不可用\n");
            return 1;
        }
        std::string id = argv[3];
        if (!lifecycle_->stopContainer(id)) {
            std::printf("错误: 停止容器失败: %s\n", id.c_str());
            return 1;
        }
        std::printf("%s\n", id.c_str());
        return 0;
    }

    // container restart <id>
    if (sub == "restart") {
        if (argc < 4) {
            std::printf("用法: container restart <id>\n");
            return 1;
        }
        if (!lifecycle_) {
            std::printf("错误: LifecycleManager 不可用\n");
            return 1;
        }
        std::string id = argv[3];
        if (!lifecycle_->restartContainer(id)) {
            std::printf("错误: 重启容器失败: %s\n", id.c_str());
            return 1;
        }
        std::printf("%s\n", id.c_str());
        return 0;
    }

    // container rm <id>
    if (sub == "rm") {
        if (argc < 4) {
            std::printf("用法: container rm <id>\n");
            return 1;
        }
        if (!lifecycle_) {
            std::printf("错误: LifecycleManager 不可用\n");
            return 1;
        }
        std::string id = argv[3];
        if (!lifecycle_->destroyContainer(id)) {
            std::printf("错误: 删除容器失败: %s\n", id.c_str());
            return 1;
        }
        std::printf("%s\n", id.c_str());
        return 0;
    }

    // container ls [-a]
    if (sub == "ls") {
        bool show_all = false;
        for (int i = 3; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "-a" || arg == "--all") {
                show_all = true;
            }
        }
        printContainers(show_all);
        return 0;
    }

    // container logs <id>
    if (sub == "logs") {
        if (argc < 4) {
            std::printf("用法: container logs <id>\n");
            return 1;
        }
        if (!lifecycle_) {
            std::printf("错误: LifecycleManager 不可用\n");
            return 1;
        }
        std::string id = argv[3];
        Container* c = lifecycle_->getContainer(id);
        if (!c) {
            std::printf("错误: 容器不存在: %s\n", id.c_str());
            return 1;
        }
        std::printf("%s", c->getLogs().c_str());
        return 0;
    }

    // container inspect <id>
    if (sub == "inspect") {
        if (argc < 4) {
            std::printf("用法: container inspect <id>\n");
            return 1;
        }
        if (!lifecycle_) {
            std::printf("错误: LifecycleManager 不可用\n");
            return 1;
        }
        std::string id = argv[3];
        Container* c = lifecycle_->getContainer(id);
        if (!c) {
            std::printf("错误: 容器不存在: %s\n", id.c_str());
            return 1;
        }
        std::printf("%s\n", c->inspect().c_str());
        return 0;
    }

    std::printf("未知子命令: container %s\n", sub.c_str());
    return 1;
}

// ====== 镜像子命令 ======

int ContainerCli::handleImage(int argc, char* argv[]) {
    if (argc < 3) {
        std::printf("用法: %s image <pull|ls|rm> ...\n",
                    argc > 0 ? argv[0] : "container-cli");
        return 1;
    }

    std::string sub = argv[2];

    // image pull <name:tag>
    if (sub == "pull") {
        if (argc < 4) {
            std::printf("用法: image pull <name:tag>\n");
            return 1;
        }
        if (!image_mgr_) {
            std::printf("错误: ImageManager 不可用\n");
            return 1;
        }
        std::string full = argv[3];
        std::string name = full;
        std::string tag = "latest";
        size_t colon = full.find(':');
        if (colon != std::string::npos) {
            name = full.substr(0, colon);
            tag = full.substr(colon + 1);
        }
        if (!image_mgr_->pullImage(name, tag)) {
            std::printf("错误: 拉取镜像失败: %s:%s\n", name.c_str(), tag.c_str());
            return 1;
        }
        std::printf("%s:%s\n", name.c_str(), tag.c_str());
        return 0;
    }

    // image ls
    if (sub == "ls") {
        printImages();
        return 0;
    }

    // image rm <name:tag>
    if (sub == "rm") {
        if (argc < 4) {
            std::printf("用法: image rm <name:tag>\n");
            return 1;
        }
        if (!image_mgr_) {
            std::printf("错误: ImageManager 不可用\n");
            return 1;
        }
        std::string full = argv[3];
        std::string name = full;
        std::string tag = "latest";
        size_t colon = full.find(':');
        if (colon != std::string::npos) {
            name = full.substr(0, colon);
            tag = full.substr(colon + 1);
        }
        if (!image_mgr_->removeImage(name, tag)) {
            std::printf("错误: 删除镜像失败: %s:%s\n", name.c_str(), tag.c_str());
            return 1;
        }
        std::printf("%s:%s\n", name.c_str(), tag.c_str());
        return 0;
    }

    std::printf("未知子命令: image %s\n", sub.c_str());
    return 1;
}

// ====== 网络子命令 ======

int ContainerCli::handleNetwork(int argc, char* argv[]) {
    if (argc < 3) {
        std::printf("用法: %s network <ls|create> ...\n",
                    argc > 0 ? argv[0] : "container-cli");
        return 1;
    }

    std::string sub = argv[2];

    // network ls
    if (sub == "ls") {
        printNetworks();
        return 0;
    }

    // network create <name> --subnet <cidr>
    if (sub == "create") {
        if (argc < 4) {
            std::printf("用法: network create <name> --subnet <cidr>\n");
            return 1;
        }
        if (!network_mgr_) {
            std::printf("错误: NetworkManager 不可用\n");
            return 1;
        }
        std::string bridge_name = argv[3];
        std::string subnet;
        for (int i = 4; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--subnet" && i + 1 < argc) {
                subnet = argv[++i];
            }
        }
        if (subnet.empty()) {
            std::printf("错误: 缺少 --subnet 参数\n");
            return 1;
        }

        // 从 "10.0.0.0/24" 推导网关 "10.0.0.1"
        // 简化实现: 取 IP 最后一段替换为 1 (适用于 /24 子网)
        std::string gateway;
        {
            size_t slash = subnet.find('/');
            if (slash != std::string::npos) {
                std::string ip = subnet.substr(0, slash);
                size_t dot = ip.rfind('.');
                if (dot != std::string::npos) {
                    gateway = ip.substr(0, dot + 1) + "1";
                }
            }
        }
        if (gateway.empty()) {
            std::printf("错误: 无法从子网推导网关: %s\n", subnet.c_str());
            return 1;
        }

        if (!network_mgr_->initBridge(bridge_name, subnet, gateway)) {
            std::printf("错误: 创建网络失败: %s\n", bridge_name.c_str());
            return 1;
        }
        std::printf("%s\n", bridge_name.c_str());
        return 0;
    }

    std::printf("未知子命令: network %s\n", sub.c_str());
    return 1;
}

// ====== 系统信息 ======

int ContainerCli::handleInfo() {
    std::printf("=== 容器系统信息 ===\n");

    // 容器数量
    if (lifecycle_) {
        auto containers = lifecycle_->listContainers();
        int running = 0;
        for (const auto& c : containers) {
            if (c && c->getStatus() == ContainerStatus::Running) ++running;
        }
        std::printf("容器总数: %zu\n", containers.size());
        std::printf("运行中:   %d\n", running);
    } else {
        std::printf("容器管理器: 不可用\n");
    }

    // 镜像数量
    if (image_mgr_) {
        auto images = image_mgr_->listImages();
        std::printf("镜像数量: %zu\n", images.size());
    } else {
        std::printf("镜像管理器: 不可用\n");
    }

    // 网络数量
    if (network_mgr_) {
        auto nets = network_mgr_->listNetworks();
        std::printf("网络数量: %zu\n", nets.size());
    } else {
        std::printf("网络管理器: 不可用\n");
    }

    return 0;
}

// ====== 输出格式化方法 ======

// 容器状态枚举转字符串
namespace {
const char* containerStatusStr(ContainerStatus s) {
    switch (s) {
        case ContainerStatus::Created: return "created";
        case ContainerStatus::Running: return "running";
        case ContainerStatus::Stopped: return "stopped";
        case ContainerStatus::Deleted: return "deleted";
        case ContainerStatus::Error:   return "error";
        default:                       return "unknown";
    }
}
}  // namespace

void ContainerCli::printContainers(bool show_all) {
    if (!lifecycle_) {
        std::printf("错误: LifecycleManager 不可用\n");
        return;
    }

    auto containers = lifecycle_->listContainers();

    // 表头
    std::printf("%-16s %-20s %-20s %-12s\n",
                "CONTAINER ID", "IMAGE", "NAME", "STATUS");
    std::printf("%-16s %-20s %-20s %-12s\n",
                "------------", "-----", "----", "------");

    for (const auto& c : containers) {
        if (!c) continue;
        ContainerStatus status = c->getStatus();
        // 不显示已删除的容器; 非全量模式下不显示非运行容器
        if (status == ContainerStatus::Deleted) continue;
        if (!show_all && status != ContainerStatus::Running) continue;

        // 截断容器 ID 为前 12 位 (Docker 风格)
        std::string id = c->getId();
        if (id.size() > 12) id = id.substr(0, 12);

        // 镜像名截断显示
        std::string image = c->getConfig().image;
        if (image.size() > 20) image = image.substr(0, 17) + "...";

        // 容器名截断显示
        std::string name = c->getConfig().name;
        if (name.empty()) name = "-";
        if (name.size() > 20) name = name.substr(0, 17) + "...";

        std::printf("%-16s %-20s %-20s %-12s\n",
                    id.c_str(), image.c_str(), name.c_str(),
                    containerStatusStr(status));
    }
}

void ContainerCli::printImages() {
    if (!image_mgr_) {
        std::printf("错误: ImageManager 不可用\n");
        return;
    }

    auto images = image_mgr_->listImages();

    // 表头
    std::printf("%-16s %-20s %-12s %-12s\n",
                "IMAGE ID", "NAME", "TAG", "SIZE");
    std::printf("%-16s %-20s %-12s %-12s\n",
                "--------", "----", "---", "----");

    for (const auto& img : images) {
        // 截断镜像 ID 为前 12 位
        std::string id = img.id;
        if (id.size() > 12) id = id.substr(0, 12);

        // 镜像名截断显示
        std::string name = img.name;
        if (name.size() > 20) name = name.substr(0, 17) + "...";

        // 标签截断显示
        std::string tag = img.tag;
        if (tag.size() > 12) tag = tag.substr(0, 9) + "...";

        // 大小转换为人类可读格式
        char size_buf[32];
        if (img.size >= 1024 * 1024 * 1024) {
            std::snprintf(size_buf, sizeof(size_buf), "%.1fGB",
                          static_cast<double>(img.size) / (1024.0 * 1024.0 * 1024.0));
        } else if (img.size >= 1024 * 1024) {
            std::snprintf(size_buf, sizeof(size_buf), "%.1fMB",
                          static_cast<double>(img.size) / (1024.0 * 1024.0));
        } else if (img.size >= 1024) {
            std::snprintf(size_buf, sizeof(size_buf), "%.1fKB",
                          static_cast<double>(img.size) / 1024.0);
        } else {
            std::snprintf(size_buf, sizeof(size_buf), "%lldB",
                          static_cast<long long>(img.size));
        }

        std::printf("%-16s %-20s %-12s %-12s\n",
                    id.c_str(), name.c_str(), tag.c_str(), size_buf);
    }
}

void ContainerCli::printNetworks() {
    if (!network_mgr_) {
        std::printf("错误: NetworkManager 不可用\n");
        return;
    }

    auto net_ids = network_mgr_->listNetworks();

    // 表头
    std::printf("%-16s %-16s %-16s\n",
                "CONTAINER ID", "VETH HOST", "IP ADDRESS");
    std::printf("%-16s %-16s %-16s\n",
                "------------", "---------", "----------");

    for (const auto& cid : net_ids) {
        ContainerNetwork net = network_mgr_->getNetworkInfo(cid);

        // 截断容器 ID 为前 12 位
        std::string id = cid;
        if (id.size() > 12) id = id.substr(0, 12);

        std::printf("%-16s %-16s %-16s\n",
                    id.c_str(), net.veth_host.c_str(), net.ip_address.c_str());
    }
}

void ContainerCli::printHelp() {
    std::printf("容器系统命令行工具\n\n");
    std::printf("用法:\n");
    std::printf("  %s <command> [subcommand] [options]\n\n", "container-cli");
    std::printf("命令:\n");
    std::printf("  container   管理容器\n");
    std::printf("  image       管理镜像\n");
    std::printf("  network     管理网络\n");
    std::printf("  info        显示系统信息\n");
    std::printf("  help        显示帮助信息\n\n");

    std::printf("容器命令:\n");
    std::printf("  container create <image> [--name <name>] [--cpu <n>] "
                "[--memory <bytes>] [-e KEY=VAL] [-p host:container] [-v host:container]\n");
    std::printf("  container start <id>\n");
    std::printf("  container stop <id>\n");
    std::printf("  container restart <id>\n");
    std::printf("  container rm <id>\n");
    std::printf("  container ls [-a]\n");
    std::printf("  container logs <id>\n");
    std::printf("  container inspect <id>\n\n");

    std::printf("镜像命令:\n");
    std::printf("  image pull <name:tag>\n");
    std::printf("  image ls\n");
    std::printf("  image rm <name:tag>\n\n");

    std::printf("网络命令:\n");
    std::printf("  network ls\n");
    std::printf("  network create <name> --subnet <cidr>\n\n");

    std::printf("示例:\n");
    std::printf("  container-cli container create ubuntu:latest --name web -p 8080:80 -e PORT=80\n");
    std::printf("  container-cli container start abc123\n");
    std::printf("  container-cli container ls -a\n");
    std::printf("  container-cli image pull alpine:latest\n");
    std::printf("  container-cli network create cbr0 --subnet 10.0.0.0/24\n");
}

// ====== 参数解析辅助方法 ======

bool ContainerCli::parseEnv(const std::string& s, std::string& key, std::string& val) {
    size_t pos = s.find('=');
    if (pos == std::string::npos || pos == 0) return false;
    key = s.substr(0, pos);
    val = s.substr(pos + 1);
    return true;
}

bool ContainerCli::parsePort(const std::string& s, int& host, int& container) {
    size_t pos = s.find(':');
    if (pos == std::string::npos) return false;
    try {
        host = std::stoi(s.substr(0, pos));
        container = std::stoi(s.substr(pos + 1));
    } catch (...) {
        return false;
    }
    return host > 0 && container > 0;
}

bool ContainerCli::parseVolume(const std::string& s, std::string& host, std::string& container) {
    size_t pos = s.find(':');
    if (pos == std::string::npos || pos == 0 || pos == s.size() - 1) return false;
    host = s.substr(0, pos);
    container = s.substr(pos + 1);
    return true;
}
