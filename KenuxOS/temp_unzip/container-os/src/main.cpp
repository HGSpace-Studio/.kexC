// container-os 主程序入口
// 负责命令行参数解析、各管理器初始化, 以及 CLI/GUI/API 三种运行模式的调度
// 版本: container-os v1.0.0

#include <cstdlib>
#include <cstring>
#include <csignal>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <thread>
#include <chrono>

// 核心与管理器头文件
#include "utils/Logger.h"
#include "utils/Config.h"
#include "core/IsolationManager.h"
#include "storage/ImageManager.h"
#include "storage/VolumeManager.h"
#include "orchestration/NetworkManager.h"
#include "orchestration/LifecycleManager.h"
#include "orchestration/Scheduler.h"

// GUI 头文件 (条件编译: Qt 或 TUI)
#include "gui/ContainerManagerWindow.h"

#ifdef HAVE_QT
#include <QApplication>
#endif

// POSIX 网络与信号头文件 (用于 API 模式的 HTTP 服务器和 CLI 模式的输入超时)
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>

// ==================== 全局变量 ====================

// 程序版本号
static constexpr const char* kVersion = "container-os v1.0.0";

// 信号处理标志: 收到 SIGINT/SIGTERM 后置为 true, 各模式检测后优雅退出
static std::atomic<bool> g_should_exit{false};

// ==================== 信号处理 ====================

// 信号处理函数
// 仅设置退出标志, 不在信号上下文中执行复杂操作
static void signalHandler(int sig) {
    (void)sig;
    g_should_exit.store(true);
}

// 注册 SIGINT 和 SIGTERM 信号处理器
static void setupSignalHandlers() {
    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
}

// ==================== 帮助与版本信息 ====================

static void printHelp() {
    std::cout << kVersion << "\n\n";
    std::cout << "用法: container-os [选项]\n\n";
    std::cout << "选项:\n";
    std::cout << "  --cli              启动 CLI 交互模式\n";
    std::cout << "  --gui              启动 GUI 图形界面模式 (默认)\n";
    std::cout << "  --api              启动 API 服务模式\n";
    std::cout << "  --port <n>         API 服务端口 (默认: 8080)\n";
    std::cout << "  --storage <path>   存储路径 (默认: /var/lib/container-os)\n";
    std::cout << "  --log-level <lvl>  日志级别: trace/debug/info/warn/error/fatal\n";
    std::cout << "  --log-file <path>  日志文件路径 (默认: 仅控制台输出)\n";
    std::cout << "  --help             显示帮助信息\n";
    std::cout << "  --version          显示版本信息\n";
}

static void printVersion() {
    std::cout << kVersion << "\n";
}

// ==================== CLI 模式 ====================

// CLI 交互模式
// 提供简单的命令行交互界面, 支持容器和镜像管理操作
static int runCliMode(LifecycleManager* lifecycle,
                      ImageManager* image_mgr,
                      NetworkManager* network_mgr) {
    Logger::getInstance().info("启动 CLI 交互模式");

    std::cout << kVersion << " - CLI 模式\n";
    std::cout << "输入 'help' 查看可用命令, 'exit' 退出\n\n";

    std::string line;
    while (!g_should_exit.load()) {
        std::cout << "container-os> " << std::flush;

        // 设置 stdin 超时检测 (每 1 秒检查一次退出标志)
        fd_set fds;
        struct timeval tv;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int ret = select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv);
        if (ret <= 0) {
            // 超时或错误, 继续循环检查退出标志
            continue;
        }

        if (!std::getline(std::cin, line)) {
            break;  // EOF
        }

        // 去除首尾空白
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        line = line.substr(start);

        // 解析命令和参数
        std::string cmd = line;
        std::string arg;
        size_t space = line.find(' ');
        if (space != std::string::npos) {
            cmd = line.substr(0, space);
            arg = line.substr(space + 1);
            // 去除参数首尾空白
            size_t s = arg.find_first_not_of(" \t");
            if (s != std::string::npos) arg = arg.substr(s);
        }

        if (cmd == "exit" || cmd == "quit") {
            break;
        } else if (cmd == "help") {
            std::cout << "可用命令:\n";
            std::cout << "  ps                        列出所有容器\n";
            std::cout << "  images                    列出所有镜像\n";
            std::cout << "  run <name> <image> [cmd]  创建并启动容器\n";
            std::cout << "  create <name> <image>     创建容器 (不启动)\n";
            std::cout << "  start <id>                启动容器\n";
            std::cout << "  stop <id>                 停止容器\n";
            std::cout << "  rm <id>                   删除容器\n";
            std::cout << "  inspect <id>              查看容器详情\n";
            std::cout << "  pull <name> [tag]         拉取镜像\n";
            std::cout << "  logs <id>                 查看容器日志\n";
            std::cout << "  exit                      退出\n";
        } else if (cmd == "ps") {
            auto containers = lifecycle->listContainers();
            if (containers.empty()) {
                std::cout << "暂无容器\n";
            } else {
                for (auto* c : containers) {
                    const auto& cfg = c->getConfig();
                    std::cout << "  " << c->getId().substr(0, 12) << "  "
                              << cfg.name << "  "
                              << cfg.image << "  ";
                    switch (c->getStatus()) {
                        case ContainerStatus::Created: std::cout << "Created"; break;
                        case ContainerStatus::Running: std::cout << "Running"; break;
                        case ContainerStatus::Stopped: std::cout << "Stopped"; break;
                        case ContainerStatus::Deleted: std::cout << "Deleted"; break;
                        case ContainerStatus::Error:   std::cout << "Error"; break;
                    }
                    std::cout << "\n";
                }
            }
        } else if (cmd == "images") {
            auto images = image_mgr->listImages();
            if (images.empty()) {
                std::cout << "暂无镜像\n";
            } else {
                for (const auto& img : images) {
                    std::cout << "  " << img.name << ":" << img.tag
                              << "  (OS: " << img.os << ")"
                              << "  " << (img.size / 1024 / 1024) << " MB\n";
                }
            }
        } else if (cmd == "create" || cmd == "run") {
            // 解析参数: create <name> <image> [command]
            std::string name, image, command;
            size_t pos1 = arg.find(' ');
            if (pos1 == std::string::npos) {
                std::cout << "用法: " << cmd << " <name> <image> [command]\n";
                continue;
            }
            name = arg.substr(0, pos1);
            std::string rest = arg.substr(pos1 + 1);
            size_t pos2 = rest.find(' ');
            if (pos2 == std::string::npos) {
                image = rest;
            } else {
                image = rest.substr(0, pos2);
                command = rest.substr(pos2 + 1);
            }

            ContainerConfig config;
            config.name = name;
            config.image = image;
            config.command = command;

            std::string id = lifecycle->createContainer(config);
            if (id.empty()) {
                std::cout << "容器创建失败\n";
                continue;
            }
            std::cout << "容器已创建: " << id << "\n";

            if (cmd == "run") {
                if (lifecycle->startContainer(id)) {
                    std::cout << "容器已启动\n";
                } else {
                    std::cout << "容器启动失败\n";
                }
            }
        } else if (cmd == "start") {
            if (arg.empty()) {
                std::cout << "用法: start <id>\n";
            } else if (lifecycle->startContainer(arg)) {
                std::cout << "容器已启动\n";
            } else {
                std::cout << "容器启动失败\n";
            }
        } else if (cmd == "stop") {
            if (arg.empty()) {
                std::cout << "用法: stop <id>\n";
            } else if (lifecycle->stopContainer(arg)) {
                std::cout << "容器已停止\n";
            } else {
                std::cout << "容器停止失败\n";
            }
        } else if (cmd == "rm") {
            if (arg.empty()) {
                std::cout << "用法: rm <id>\n";
            } else if (lifecycle->destroyContainer(arg)) {
                std::cout << "容器已删除\n";
            } else {
                std::cout << "容器删除失败\n";
            }
        } else if (cmd == "inspect") {
            if (arg.empty()) {
                std::cout << "用法: inspect <id>\n";
            } else {
                Container* c = lifecycle->getContainer(arg);
                if (c) {
                    std::cout << c->inspect() << "\n";
                } else {
                    std::cout << "容器不存在\n";
                }
            }
        } else if (cmd == "pull") {
            std::string name, tag = "latest";
            size_t pos = arg.find(' ');
            if (pos == std::string::npos) {
                name = arg;
            } else {
                name = arg.substr(0, pos);
                tag = arg.substr(pos + 1);
            }
            if (name.empty()) {
                std::cout << "用法: pull <name> [tag]\n";
            } else if (image_mgr->pullImage(name, tag)) {
                std::cout << "镜像拉取成功: " << name << ":" << tag << "\n";
            } else {
                std::cout << "镜像拉取失败\n";
            }
        } else if (cmd == "logs") {
            if (arg.empty()) {
                std::cout << "用法: logs <id>\n";
            } else {
                Container* c = lifecycle->getContainer(arg);
                if (c) {
                    std::cout << c->getLogs() << "\n";
                } else {
                    std::cout << "容器不存在\n";
                }
            }
        } else {
            std::cout << "未知命令: " << cmd << " (输入 'help' 查看帮助)\n";
        }
    }

    // 清理网络 (如果有容器使用了网络)
    (void)network_mgr;

    Logger::getInstance().info("CLI 模式已退出");
    return 0;
}

// ==================== GUI 模式 ====================

// GUI 图形界面模式
// 有 Qt 时启动 QApplication + ContainerManagerWindow
// 无 Qt 时启动 TUI 版本的 ContainerManagerWindow::exec()
static int runGuiMode(int argc, char* argv[],
                      LifecycleManager* lifecycle,
                      ImageManager* image_mgr,
                      NetworkManager* network_mgr) {
    Logger::getInstance().info("启动 GUI 模式");

#ifdef HAVE_QT
    QApplication app(argc, argv);
    ContainerManagerWindow window(lifecycle, image_mgr, network_mgr);
    window.show();
    return app.exec();
#else
    ContainerManagerWindow window(lifecycle, image_mgr, network_mgr);
    return window.exec();
#endif
}

// ==================== API 模式 ====================

// API 服务模式
// 实现一个极简的 HTTP 服务器, 提供 RESTful 风格的容器管理 API
// 不依赖第三方 HTTP 库, 使用 POSIX socket 自实现

// HTTP 响应: 将 body 包装为 HTTP 响应报文并发送
static void sendHttpResponse(int client_fd, int status_code,
                             const std::string& content_type,
                             const std::string& body) {
    const char* status_text = "OK";
    switch (status_code) {
        case 200: status_text = "OK"; break;
        case 201: status_text = "Created"; break;
        case 400: status_text = "Bad Request"; break;
        case 404: status_text = "Not Found"; break;
        case 500: status_text = "Internal Server Error"; break;
    }

    std::ostringstream response;
    response << "HTTP/1.1 " << status_code << " " << status_text << "\r\n"
             << "Content-Type: " << content_type << "\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Connection: close\r\n"
             << "\r\n"
             << body;

    std::string resp_str = response.str();
    send(client_fd, resp_str.c_str(), resp_str.size(), 0);
}

// 手动构建 JSON 字符串 (不依赖 nlohmann_json)
static std::string jsonEscape(const std::string& s) {
    std::string result;
    for (char c : s) {
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:   result += c; break;
        }
    }
    return result;
}

// 处理单个 HTTP 请求
static void handleHttpRequest(int client_fd,
                              LifecycleManager* lifecycle,
                              ImageManager* image_mgr) {
    char buffer[4096];
    ssize_t bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) {
        close(client_fd);
        return;
    }
    buffer[bytes] = '\0';

    // 解析 HTTP 请求行: METHOD PATH HTTP/1.1
    std::string request(buffer);
    size_t first_space = request.find(' ');
    size_t second_space = request.find(' ', first_space + 1);
    if (first_space == std::string::npos || second_space == std::string::npos) {
        sendHttpResponse(client_fd, 400, "application/json", "{\"error\":\"Bad Request\"}");
        close(client_fd);
        return;
    }

    std::string method = request.substr(0, first_space);
    std::string path = request.substr(first_space + 1, second_space - first_space - 1);

    Logger::getInstance().debug("API 请求: " + method + " " + path);

    // 路由匹配
    if (method == "GET" && path == "/containers") {
        // 列出所有容器
        auto containers = lifecycle->listContainers();
        std::ostringstream json;
        json << "[";
        for (size_t i = 0; i < containers.size(); ++i) {
            auto* c = containers[i];
            const auto& cfg = c->getConfig();
            if (i > 0) json << ",";
            json << "{\"id\":\"" << jsonEscape(c->getId()) << "\","
                 << "\"name\":\"" << jsonEscape(cfg.name) << "\","
                 << "\"image\":\"" << jsonEscape(cfg.image) << "\","
                 << "\"status\":\"" << jsonEscape(c->inspect()) << "\"}";
        }
        json << "]";
        sendHttpResponse(client_fd, 200, "application/json", json.str());
    } else if (method == "GET" && path == "/images") {
        // 列出所有镜像
        auto images = image_mgr->listImages();
        std::ostringstream json;
        json << "[";
        for (size_t i = 0; i < images.size(); ++i) {
            const auto& img = images[i];
            if (i > 0) json << ",";
            json << "{\"id\":\"" << jsonEscape(img.id) << "\","
                 << "\"name\":\"" << jsonEscape(img.name) << "\","
                 << "\"tag\":\"" << jsonEscape(img.tag) << "\","
                 << "\"os\":\"" << jsonEscape(img.os) << "\","
                 << "\"size\":" << img.size << "}";
        }
        json << "]";
        sendHttpResponse(client_fd, 200, "application/json", json.str());
    } else if (method == "POST" && path.find("/containers/") == 0) {
        // 容器操作: /containers/<id>/start, /containers/<id>/stop
        std::string sub = path.substr(std::string("/containers/").length());
        size_t slash = sub.find('/');
        if (slash == std::string::npos) {
            sendHttpResponse(client_fd, 400, "application/json", "{\"error\":\"Bad Request\"}");
        } else {
            std::string id = sub.substr(0, slash);
            std::string action = sub.substr(slash + 1);

            bool ok = false;
            if (action == "start") {
                ok = lifecycle->startContainer(id);
            } else if (action == "stop") {
                ok = lifecycle->stopContainer(id);
            } else if (action == "restart") {
                ok = lifecycle->restartContainer(id);
            } else {
                sendHttpResponse(client_fd, 404, "application/json", "{\"error\":\"Unknown action\"}");
                close(client_fd);
                return;
            }

            if (ok) {
                sendHttpResponse(client_fd, 200, "application/json", "{\"status\":\"ok\"}");
            } else {
                sendHttpResponse(client_fd, 500, "application/json", "{\"error\":\"Operation failed\"}");
            }
        }
    } else if (method == "DELETE" && path.find("/containers/") == 0) {
        // 删除容器: DELETE /containers/<id>
        std::string id = path.substr(std::string("/containers/").length());
        if (lifecycle->destroyContainer(id)) {
            sendHttpResponse(client_fd, 200, "application/json", "{\"status\":\"deleted\"}");
        } else {
            sendHttpResponse(client_fd, 500, "application/json", "{\"error\":\"Delete failed\"}");
        }
    } else if (method == "POST" && path == "/containers/create") {
        // 创建容器: POST /containers/create
        // 简单解析 body 中的 name 和 image
        std::string body;
        size_t body_start = request.find("\r\n\r\n");
        if (body_start != std::string::npos) {
            body = request.substr(body_start + 4);
        }

        ContainerConfig config;
        // 极简解析: 查找 "name":"xxx" 和 "image":"xxx"
        auto extract = [&body](const std::string& key) -> std::string {
            std::string pattern = "\"" + key + "\":\"";
            size_t pos = body.find(pattern);
            if (pos == std::string::npos) return "";
            pos += pattern.length();
            size_t end = body.find('"', pos);
            if (end == std::string::npos) return "";
            return body.substr(pos, end - pos);
        };
        config.name = extract("name");
        config.image = extract("image");
        config.command = extract("command");

        std::string id = lifecycle->createContainer(config);
        if (!id.empty()) {
            std::string json = "{\"id\":\"" + jsonEscape(id) + "\",\"status\":\"created\"}";
            sendHttpResponse(client_fd, 201, "application/json", json);
        } else {
            sendHttpResponse(client_fd, 500, "application/json", "{\"error\":\"Create failed\"}");
        }
    } else if (method == "GET" && path == "/health") {
        // 健康检查
        sendHttpResponse(client_fd, 200, "application/json", "{\"status\":\"healthy\"}");
    } else {
        // 未匹配路由
        sendHttpResponse(client_fd, 404, "application/json", "{\"error\":\"Not Found\"}");
    }

    close(client_fd);
}

// 启动 API HTTP 服务器
static int runApiMode(LifecycleManager* lifecycle,
                      ImageManager* image_mgr,
                      int port) {
    Logger::getInstance().info("启动 API 服务模式, 端口: " + std::to_string(port));

    // 创建监听 socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        Logger::getInstance().error("创建 socket 失败");
        std::cerr << "错误: 无法创建 socket\n";
        return 1;
    }

    // 设置地址复用, 避免 TIME_WAIT 状态导致绑定失败
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 绑定地址和端口
    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<uint16_t>(port));

    if (bind(server_fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        Logger::getInstance().error("绑定端口失败: " + std::to_string(port));
        std::cerr << "错误: 无法绑定端口 " << port << "\n";
        close(server_fd);
        return 1;
    }

    // 开始监听
    if (listen(server_fd, 10) < 0) {
        Logger::getInstance().error("listen 失败");
        std::cerr << "错误: listen 失败\n";
        close(server_fd);
        return 1;
    }

    std::cout << kVersion << " - API 服务已启动\n";
    std::cout << "监听端口: " << port << "\n";
    std::cout << "可用端点:\n";
    std::cout << "  GET    /health              健康检查\n";
    std::cout << "  GET    /containers          列出所有容器\n";
    std::cout << "  POST   /containers/create   创建容器\n";
    std::cout << "  POST   /containers/<id>/start   启动容器\n";
    std::cout << "  POST   /containers/<id>/stop    停止容器\n";
    std::cout << "  DELETE /containers/<id>      删除容器\n";
    std::cout << "  GET    /images               列出所有镜像\n\n";

    // 接受连接循环
    while (!g_should_exit.load()) {
        // 使用 select 设置超时, 以便定期检查退出标志
        fd_set read_fds;
        struct timeval tv;
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int ret = select(server_fd + 1, &read_fds, nullptr, nullptr, &tv);
        if (ret <= 0) {
            continue;  // 超时或错误, 检查退出标志后继续
        }

        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd,
                               reinterpret_cast<struct sockaddr*>(&client_addr),
                               &client_len);
        if (client_fd < 0) {
            if (!g_should_exit.load()) {
                Logger::getInstance().warn("accept 失败");
            }
            continue;
        }

        // 在新线程中处理请求, 避免阻塞主循环
        std::thread([client_fd, lifecycle, image_mgr]() {
            handleHttpRequest(client_fd, lifecycle, image_mgr);
        }).detach();
    }

    // 清理
    close(server_fd);
    Logger::getInstance().info("API 服务已停止");
    std::cout << "API 服务已停止\n";
    return 0;
}

// ==================== 主函数 ====================

int main(int argc, char* argv[]) {
    // ----- 默认参数 -----
    std::string mode = "gui";        // 运行模式: cli / gui / api
    int api_port = 8080;             // API 端口
    std::string storage_path = "/var/lib/container-os";  // 存储路径
    std::string log_level = "info";  // 日志级别
    std::string log_file;            // 日志文件 (空表示仅控制台)

    // ----- 解析命令行参数 -----
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            printHelp();
            return 0;
        } else if (arg == "--version" || arg == "-v") {
            printVersion();
            return 0;
        } else if (arg == "--cli") {
            mode = "cli";
        } else if (arg == "--gui") {
            mode = "gui";
        } else if (arg == "--api") {
            mode = "api";
        } else if (arg == "--port") {
            if (i + 1 < argc) {
                api_port = std::atoi(argv[++i]);
                if (api_port <= 0 || api_port > 65535) {
                    std::cerr << "错误: 无效的端口号 " << api_port << "\n";
                    return 1;
                }
            }
        } else if (arg == "--storage") {
            if (i + 1 < argc) {
                storage_path = argv[++i];
            }
        } else if (arg == "--log-level") {
            if (i + 1 < argc) {
                log_level = argv[++i];
            }
        } else if (arg == "--log-file") {
            if (i + 1 < argc) {
                log_file = argv[++i];
            }
        } else {
            std::cerr << "警告: 未知参数 " << arg << " (使用 --help 查看帮助)\n";
        }
    }

    // ----- 初始化日志系统 -----
    Logger& logger = Logger::getInstance();
    if (log_level == "trace") {
        logger.setLevel(LogLevel::Trace);
    } else if (log_level == "debug") {
        logger.setLevel(LogLevel::Debug);
    } else if (log_level == "info") {
        logger.setLevel(LogLevel::Info);
    } else if (log_level == "warn") {
        logger.setLevel(LogLevel::Warn);
    } else if (log_level == "error") {
        logger.setLevel(LogLevel::Error);
    } else if (log_level == "fatal") {
        logger.setLevel(LogLevel::Fatal);
    }
    if (!log_file.empty()) {
        logger.setFileOutput(log_file);
    }
    logger.info(std::string("启动 ") + kVersion);

    // ----- 注册信号处理 -----
    setupSignalHandlers();
    logger.info("信号处理器已注册 (SIGINT/SIGTERM)");

    // ----- 加载配置文件 -----
    Config& config = Config::getInstance();
    // 尝试加载默认配置文件 (存在则加载, 不存在则跳过)
    std::string config_path = storage_path + "/config.conf";
    if (config.loadFromFile(config_path)) {
        logger.info("配置文件已加载: " + config_path);
        // 配置文件中的值可覆盖命令行参数 (此处简化, 仅读取)
        if (config.has("port") && api_port == 8080) {
            api_port = config.getInt("port", 8080);
        }
    } else {
        logger.info("未找到配置文件, 使用默认配置");
    }

    // ----- 初始化各管理器 -----
    logger.info("初始化隔离管理器...");
    auto isolation_mgr = std::make_unique<IsolationManager>();

    logger.info("初始化镜像管理器 (存储路径: " + storage_path + ")...");
    auto image_mgr = std::make_unique<ImageManager>(storage_path);

    logger.info("初始化卷管理器...");
    auto volume_mgr = std::make_unique<VolumeManager>(storage_path);

    logger.info("初始化网络管理器...");
    auto network_mgr = std::make_unique<NetworkManager>();
    // 初始化默认网桥
    network_mgr->initBridge("cbr0", "10.0.0.0/24", "10.0.0.1");

    logger.info("初始化生命周期管理器...");
    auto lifecycle = std::make_unique<LifecycleManager>(isolation_mgr.get());

    logger.info("初始化调度器...");
    auto scheduler = std::make_unique<Scheduler>();
    scheduler->start();

    // ----- 根据模式启动对应服务 -----
    int exit_code = 0;

    if (mode == "cli") {
        exit_code = runCliMode(lifecycle.get(), image_mgr.get(), network_mgr.get());
    } else if (mode == "gui") {
        exit_code = runGuiMode(argc, argv, lifecycle.get(), image_mgr.get(), network_mgr.get());
    } else if (mode == "api") {
        exit_code = runApiMode(lifecycle.get(), image_mgr.get(), api_port);
    } else {
        std::cerr << "错误: 未知模式 " << mode << "\n";
        exit_code = 1;
    }

    // ----- 优雅退出 -----
    logger.info("正在停止服务...");
    g_should_exit.store(true);

    scheduler->stop();
    logger.info("调度器已停止");

    // 各管理器由 unique_ptr 自动析构
    // 析构顺序: scheduler -> lifecycle -> network_mgr -> volume_mgr -> image_mgr -> isolation_mgr
    scheduler.reset();
    lifecycle.reset();
    network_mgr.reset();
    volume_mgr.reset();
    image_mgr.reset();
    isolation_mgr.reset();

    logger.info(std::string(kVersion) + " 已退出");
    return exit_code;
}
