#pragma once

// REST API 头文件
// 提供 HTTP RESTful 接口, 允许通过 HTTP 调用容器/镜像/网络管理功能
// 自实现简单的 HTTP 服务器, 不依赖第三方库
// 使用 POSIX socket API: socket / bind / listen / accept

#include <string>
#include <thread>
#include <atomic>

#include "orchestration/LifecycleManager.h"
#include "storage/ImageManager.h"
#include "orchestration/NetworkManager.h"

// REST API 服务
// 在后台线程中运行 HTTP 服务器, 多线程处理客户端请求
class RestApi {
public:
    // 构造函数
    // lifecycle:    容器生命周期管理器
    // image_mgr:    镜像管理器
    // network_mgr:  网络管理器
    // port:         监听端口, 默认 8080
    RestApi(LifecycleManager* lifecycle, ImageManager* image_mgr,
            NetworkManager* network_mgr, int port = 8080);
    ~RestApi();

    // 启动 HTTP 服务 (非阻塞, 在后台线程中运行)
    // 返回: 成功返回 true
    bool start();

    // 停止 HTTP 服务
    void stop();

private:
    // 主服务循环: accept 客户端连接并分发到工作线程
    void serverLoop();

    // 处理单个客户端连接 (在工作线程中执行)
    void handleClient(int client_fd);

    // 统一请求分发
    // method:      HTTP 方法 (GET/POST/PUT/DELETE)
    // path:        请求路径 (不含 query string)
    // body:        请求体
    // response:    输出, 响应体 (JSON 字符串)
    // status_code: 输出, HTTP 状态码
    void handleRequest(const std::string& method, const std::string& path,
                       const std::string& body, std::string& response, int& status_code);

    // ====== 容器相关路由 ======
    // POST /api/containers
    void handleCreateContainer(const std::string& body, std::string& response, int& status_code);
    // GET /api/containers
    void handleListContainers(std::string& response, int& status_code);
    // GET /api/containers/{id}
    void handleGetContainer(const std::string& id, std::string& response, int& status_code);
    // PUT /api/containers/{id}/start
    void handleStartContainer(const std::string& id, std::string& response, int& status_code);
    // PUT /api/containers/{id}/stop
    void handleStopContainer(const std::string& id, std::string& response, int& status_code);
    // DELETE /api/containers/{id}
    void handleDeleteContainer(const std::string& id, std::string& response, int& status_code);
    // GET /api/containers/{id}/logs
    void handleContainerLogs(const std::string& id, std::string& response, int& status_code);

    // ====== 镜像相关路由 ======
    // GET /api/images
    void handleListImages(std::string& response, int& status_code);
    // POST /api/images
    void handlePullImage(const std::string& body, std::string& response, int& status_code);
    // DELETE /api/images/{name}
    void handleDeleteImage(const std::string& name, std::string& response, int& status_code);

    int port_;                       // 监听端口
    std::atomic<bool> running_;      // 服务运行标志
    std::thread server_thread_;      // 主服务线程
    int server_fd_;                  // 监听 socket 文件描述符

    LifecycleManager* lifecycle_;    // 容器生命周期管理器 (非所有, 外部注入)
    ImageManager* image_mgr_;        // 镜像管理器 (非所有, 外部注入)
    NetworkManager* network_mgr_;    // 网络管理器 (非所有, 外部注入)
};
