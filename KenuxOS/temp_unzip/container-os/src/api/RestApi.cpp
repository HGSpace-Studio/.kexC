#include "api/RestApi.h"
#include "utils/Logger.h"

// POSIX socket 头文件
#ifdef __linux__
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>
#else
// TODO: 在非 POSIX 平台 (如 Windows / 自定义 OS) 上提供等价 socket 实现
// 当前仅 Linux 完整提供 POSIX socket API (sys/socket.h, netinet/in.h, arpa/inet.h, unistd.h)
// 其他平台需移植以下函数: socket / bind / listen / accept / recv / send / close / setsockopt
// 以及 struct sockaddr_in / INADDR_ANY / htons / SOL_SOCKET / SO_REUSEADDR 等定义
#endif

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cctype>
#include <sstream>
#include <vector>
#include <thread>
#include <atomic>
#include <algorithm>

namespace {

// ====== JSON 工具 (自实现, 不依赖第三方库) ======

// JSON 字符串转义, 保证输出合法 JSON
std::string escapeJsonString(const std::string& s) {
    std::string result;
    result.reserve(s.size() + 2);
    for (char c : s) {
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x",
                                  static_cast<unsigned char>(c));
                    result += buf;
                } else {
                    result += c;
                }
                break;
        }
    }
    return result;
}

// JSON 值结构, 支持对象/数组/字符串/数字/布尔/null
struct JsonValue {
    enum Type { Null, Bool, Number, String, Array, Object };
    Type type = Null;
    bool b = false;
    double num = 0;
    std::string str;
    std::vector<JsonValue> arr;
    std::vector<std::pair<std::string, JsonValue>> obj;

    // 在对象中按键查找, 未找到返回 nullptr
    const JsonValue* find(const std::string& key) const {
        if (type != Object) return nullptr;
        for (const auto& kv : obj) {
            if (kv.first == key) return &kv.second;
        }
        return nullptr;
    }
};

// 极简 JSON 解析器 (递归下降)
// 支持 object / array / string / number / bool / null
class JsonParser {
public:
    explicit JsonParser(const std::string& s) : s_(s), i_(0) {}

    // 解析整个 JSON 文本, 成功返回 true
    bool parse(JsonValue& out) {
        skipWs();
        return parseValue(out);
    }

private:
    const std::string& s_;
    size_t i_;

    void skipWs() {
        while (i_ < s_.size() &&
               (s_[i_] == ' ' || s_[i_] == '\t' || s_[i_] == '\n' || s_[i_] == '\r')) {
            ++i_;
        }
    }

    bool parseValue(JsonValue& out) {
        skipWs();
        if (i_ >= s_.size()) return false;
        char c = s_[i_];
        if (c == '{') return parseObject(out);
        if (c == '[') return parseArray(out);
        if (c == '"') { out.type = JsonValue::String; return parseString(out.str); }
        if (c == 't' || c == 'f') return parseBool(out);
        if (c == 'n') return parseNull(out);
        return parseNumber(out);
    }

    bool parseString(std::string& out) {
        if (i_ >= s_.size() || s_[i_] != '"') return false;
        ++i_;  // 跳过开引号
        out.clear();
        while (i_ < s_.size() && s_[i_] != '"') {
            if (s_[i_] == '\\' && i_ + 1 < s_.size()) {
                char esc = s_[i_ + 1];
                switch (esc) {
                    case '"':  out += '"';  break;
                    case '\\': out += '\\'; break;
                    case '/':  out += '/';  break;
                    case 'n':  out += '\n'; break;
                    case 'r':  out += '\r'; break;
                    case 't':  out += '\t'; break;
                    default:   out += esc;  break;
                }
                i_ += 2;
            } else {
                out += s_[i_];
                ++i_;
            }
        }
        if (i_ >= s_.size()) return false;  // 缺少闭引号
        ++i_;  // 跳过闭引号
        return true;
    }

    bool parseNumber(JsonValue& out) {
        size_t start = i_;
        if (i_ < s_.size() && (s_[i_] == '-' || s_[i_] == '+')) ++i_;
        while (i_ < s_.size() &&
               (std::isdigit(static_cast<unsigned char>(s_[i_])) ||
                s_[i_] == '.' || s_[i_] == 'e' || s_[i_] == 'E' ||
                s_[i_] == '+' || s_[i_] == '-')) {
            ++i_;
        }
        if (i_ == start) return false;
        out.type = JsonValue::Number;
        out.num = std::stod(s_.substr(start, i_ - start));
        return true;
    }

    bool parseBool(JsonValue& out) {
        if (s_.compare(i_, 4, "true") == 0) {
            out.type = JsonValue::Bool;
            out.b = true;
            i_ += 4;
            return true;
        }
        if (s_.compare(i_, 5, "false") == 0) {
            out.type = JsonValue::Bool;
            out.b = false;
            i_ += 5;
            return true;
        }
        return false;
    }

    bool parseNull(JsonValue& out) {
        if (s_.compare(i_, 4, "null") == 0) {
            out.type = JsonValue::Null;
            i_ += 4;
            return true;
        }
        return false;
    }

    bool parseArray(JsonValue& out) {
        ++i_;  // 跳过 '['
        out.type = JsonValue::Array;
        out.arr.clear();
        skipWs();
        if (i_ < s_.size() && s_[i_] == ']') { ++i_; return true; }  // 空数组
        while (true) {
            JsonValue v;
            if (!parseValue(v)) return false;
            out.arr.push_back(std::move(v));
            skipWs();
            if (i_ >= s_.size()) return false;
            if (s_[i_] == ',') { ++i_; continue; }
            if (s_[i_] == ']') { ++i_; return true; }
            return false;
        }
    }

    bool parseObject(JsonValue& out) {
        ++i_;  // 跳过 '{'
        out.type = JsonValue::Object;
        out.obj.clear();
        skipWs();
        if (i_ < s_.size() && s_[i_] == '}') { ++i_; return true; }  // 空对象
        while (true) {
            skipWs();
            std::string key;
            if (!parseString(key)) return false;
            skipWs();
            if (i_ >= s_.size() || s_[i_] != ':') return false;
            ++i_;  // 跳过 ':'
            JsonValue v;
            if (!parseValue(v)) return false;
            out.obj.emplace_back(std::move(key), std::move(v));
            skipWs();
            if (i_ >= s_.size()) return false;
            if (s_[i_] == ',') { ++i_; continue; }
            if (s_[i_] == '}') { ++i_; return true; }
            return false;
        }
    }
};

// 构建错误响应 JSON
std::string makeErrorJson(const std::string& msg) {
    std::ostringstream oss;
    oss << "{\"error\":\"" << escapeJsonString(msg) << "\"}";
    return oss.str();
}

// 构建成功响应 JSON
std::string makeSuccessJson(const std::string& msg) {
    std::ostringstream oss;
    oss << "{\"message\":\"" << escapeJsonString(msg) << "\"}";
    return oss.str();
}

// HTTP 状态码转文本
const char* statusText(int code) {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 500: return "Internal Server Error";
        default:  return "Unknown";
    }
}

// 分割路径, 例如 "/api/containers/abc/logs" -> ["api", "containers", "abc", "logs"]
std::vector<std::string> splitPath(const std::string& path) {
    std::vector<std::string> parts;
    size_t start = 0;
    size_t end = 0;
    while (start < path.size()) {
        // 跳过前导 '/'
        while (start < path.size() && path[start] == '/') ++start;
        if (start >= path.size()) break;
        end = path.find('/', start);
        if (end == std::string::npos) end = path.size();
        parts.push_back(path.substr(start, end - start));
        start = end;
    }
    return parts;
}

// 按 '?' 分割 URL, 返回 path 部分 (不含 query string)
std::string stripQuery(const std::string& url) {
    size_t pos = url.find('?');
    if (pos == std::string::npos) return url;
    return url.substr(0, pos);
}

// 将 ContainerStatus 转为字符串
const char* statusToStr(ContainerStatus s) {
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

// ====== RestApi 实现 ======

RestApi::RestApi(LifecycleManager* lifecycle, ImageManager* image_mgr,
                 NetworkManager* network_mgr, int port)
    : port_(port), running_(false), server_fd_(-1),
      lifecycle_(lifecycle), image_mgr_(image_mgr), network_mgr_(network_mgr) {
    Logger::getInstance().info("RestApi initialized on port " + std::to_string(port_));
}

RestApi::~RestApi() {
    stop();
}

bool RestApi::start() {
    if (running_.load()) {
        return true;  // 已在运行
    }

    // 1. 创建监听 socket
    server_fd_ = static_cast<int>(::socket(AF_INET, SOCK_STREAM, 0));
    if (server_fd_ < 0) {
        Logger::getInstance().error("RestApi::start socket() failed");
        return false;
    }

    // 2. 设置地址复用, 避免 TIME_WAIT 导致 bind 失败
    int opt = 1;
    ::setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR,
                 reinterpret_cast<const char*>(&opt), sizeof(opt));

    // 3. 绑定地址
    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<uint16_t>(port_));
    if (::bind(server_fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        Logger::getInstance().error("RestApi::start bind() failed on port " +
                                     std::to_string(port_));
        ::close(server_fd_);
        server_fd_ = -1;
        return false;
    }

    // 4. 开始监听
    if (::listen(server_fd_, 128) < 0) {
        Logger::getInstance().error("RestApi::start listen() failed");
        ::close(server_fd_);
        server_fd_ = -1;
        return false;
    }

    // 5. 启动主服务线程
    running_.store(true);
    server_thread_ = std::thread(&RestApi::serverLoop, this);
    Logger::getInstance().info("RestApi listening on port " + std::to_string(port_));
    return true;
}

void RestApi::stop() {
    if (!running_.load()) return;
    running_.store(false);

    // 关闭监听 socket, 使 accept() 返回失败从而退出循环
    if (server_fd_ >= 0) {
        ::close(server_fd_);
        server_fd_ = -1;
    }

    // 等待主服务线程退出
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
    Logger::getInstance().info("RestApi stopped");
}

void RestApi::serverLoop() {
    while (running_.load()) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = static_cast<int>(
            ::accept(server_fd_, reinterpret_cast<struct sockaddr*>(&client_addr), &client_len));
        if (client_fd < 0) {
            if (!running_.load()) break;  // 正常关闭
            continue;
        }

        // 为每个连接创建独立线程处理, 实现并发
        // 使用 detach, 线程自行退出; handleClient 内部会关闭 client_fd
        std::thread(&RestApi::handleClient, this, client_fd).detach();
    }
}

void RestApi::handleClient(int client_fd) {
    // 读取请求数据: 先读 header, 再按 Content-Length 读 body
    std::string raw;
    char buf[4096];
    size_t header_end = std::string::npos;

    // 第一阶段: 读取直到遇到 \r\n\r\n (header 结束标记)
    while (header_end == std::string::npos) {
        ssize_t n = ::recv(client_fd, buf, sizeof(buf), 0);
        if (n <= 0) {
            ::close(client_fd);
            return;
        }
        raw.append(buf, static_cast<size_t>(n));
        header_end = raw.find("\r\n\r\n");
        if (header_end != std::string::npos) break;
        // 防止恶意超大 header
        if (raw.size() > 1024 * 1024) {
            ::close(client_fd);
            return;
        }
    }

    // 解析请求行: METHOD PATH HTTP/1.1
    size_t first_space = raw.find(' ');
    size_t second_space = raw.find(' ', first_space + 1);
    if (first_space == std::string::npos || second_space == std::string::npos) {
        ::close(client_fd);
        return;
    }
    std::string method = raw.substr(0, first_space);
    std::string url = raw.substr(first_space + 1, second_space - first_space - 1);
    std::string path = stripQuery(url);

    // 解析 Content-Length
    size_t content_length = 0;
    {
        std::string lower = raw;
        // 大小写不敏感查找 Content-Length
        std::string key = "content-length:";
        for (size_t i = 0; i + key.size() <= lower.size(); ++i) {
            bool match = true;
            for (size_t j = 0; j < key.size(); ++j) {
                if (std::tolower(static_cast<unsigned char>(lower[i + j])) !=
                    key[j]) {
                    match = false;
                    break;
                }
            }
            if (match) {
                size_t val_start = i + key.size();
                while (val_start < lower.size() && lower[val_start] == ' ') ++val_start;
                size_t val_end = lower.find("\r\n", val_start);
                if (val_end != std::string::npos) {
                    content_length = static_cast<size_t>(
                        std::stoul(lower.substr(val_start, val_end - val_start)));
                }
                break;
            }
        }
    }

    // 第二阶段: 读取 body (按 Content-Length)
    std::string body;
    size_t body_start = header_end + 4;  // 跳过 \r\n\r\n
    if (body_start < raw.size()) {
        body = raw.substr(body_start);
    }
    while (body.size() < content_length) {
        ssize_t n = ::recv(client_fd, buf, sizeof(buf), 0);
        if (n <= 0) break;
        body.append(buf, static_cast<size_t>(n));
    }
    if (body.size() > content_length) {
        body.resize(content_length);
    }

    // 分发请求
    std::string response;
    int status_code = 200;
    handleRequest(method, path, body, response, status_code);

    // 构建 HTTP 响应
    std::ostringstream resp;
    resp << "HTTP/1.1 " << status_code << " " << statusText(status_code) << "\r\n";
    resp << "Content-Type: application/json\r\n";
    resp << "Content-Length: " << response.size() << "\r\n";
    resp << "Connection: close\r\n";
    resp << "\r\n";
    resp << response;

    std::string resp_str = resp.str();
    ::send(client_fd, resp_str.data(), static_cast<int>(resp_str.size()), 0);
    ::close(client_fd);
}

void RestApi::handleRequest(const std::string& method, const std::string& path,
                            const std::string& body, std::string& response,
                            int& status_code) {
    auto parts = splitPath(path);

    // 路由: /api/containers...
    if (parts.size() >= 2 && parts[0] == "api" && parts[1] == "containers") {
        if (parts.size() == 2) {
            // /api/containers
            if (method == "POST") {
                handleCreateContainer(body, response, status_code);
            } else if (method == "GET") {
                handleListContainers(response, status_code);
            } else {
                response = makeErrorJson("Method not allowed");
                status_code = 400;
            }
            return;
        }
        if (parts.size() == 3) {
            // /api/containers/{id}
            std::string id = parts[2];
            if (method == "GET") {
                handleGetContainer(id, response, status_code);
            } else if (method == "DELETE") {
                handleDeleteContainer(id, response, status_code);
            } else {
                response = makeErrorJson("Method not allowed");
                status_code = 400;
            }
            return;
        }
        if (parts.size() == 4) {
            // /api/containers/{id}/{action}
            std::string id = parts[2];
            std::string action = parts[3];
            if (action == "start" && method == "PUT") {
                handleStartContainer(id, response, status_code);
            } else if (action == "stop" && method == "PUT") {
                handleStopContainer(id, response, status_code);
            } else if (action == "logs" && method == "GET") {
                handleContainerLogs(id, response, status_code);
            } else {
                response = makeErrorJson("Method not allowed");
                status_code = 400;
            }
            return;
        }
    }

    // 路由: /api/images...
    if (parts.size() >= 2 && parts[0] == "api" && parts[1] == "images") {
        if (parts.size() == 2) {
            // /api/images
            if (method == "GET") {
                handleListImages(response, status_code);
            } else if (method == "POST") {
                handlePullImage(body, response, status_code);
            } else {
                response = makeErrorJson("Method not allowed");
                status_code = 400;
            }
            return;
        }
        if (parts.size() == 3) {
            // /api/images/{name}
            std::string name = parts[2];
            if (method == "DELETE") {
                handleDeleteImage(name, response, status_code);
            } else {
                response = makeErrorJson("Method not allowed");
                status_code = 400;
            }
            return;
        }
    }

    // 未知路由
    response = makeErrorJson("Not found: " + path);
    status_code = 404;
}

// ====== 容器路由实现 ======

void RestApi::handleCreateContainer(const std::string& body, std::string& response,
                                    int& status_code) {
    if (!lifecycle_) {
        response = makeErrorJson("LifecycleManager not available");
        status_code = 500;
        return;
    }

    // 解析 JSON 请求体
    JsonParser parser(body);
    JsonValue root;
    if (!parser.parse(root) || root.type != JsonValue::Object) {
        response = makeErrorJson("Invalid JSON body");
        status_code = 400;
        return;
    }

    ContainerConfig config;
    const JsonValue* j_name = root.find("name");
    if (j_name && j_name->type == JsonValue::String) config.name = j_name->str;

    const JsonValue* j_image = root.find("image");
    if (j_image && j_image->type == JsonValue::String) config.image = j_image->str;
    else {
        response = makeErrorJson("Missing 'image' field");
        status_code = 400;
        return;
    }

    const JsonValue* j_cmd = root.find("command");
    if (j_cmd && j_cmd->type == JsonValue::String) config.command = j_cmd->str;

    // CPU 限制
    const JsonValue* j_cpu = root.find("cpu_limit");
    if (j_cpu && j_cpu->type == JsonValue::Number) {
        config.resources.cpu_limit = static_cast<int>(j_cpu->num);
    }

    // 内存限制
    const JsonValue* j_mem = root.find("memory_limit");
    if (j_mem && j_mem->type == JsonValue::Number) {
        config.resources.memory_limit = static_cast<uint64_t>(j_mem->num);
    }

    // 环境变量
    const JsonValue* j_env = root.find("environment");
    if (j_env && j_env->type == JsonValue::Object) {
        for (const auto& kv : j_env->obj) {
            if (kv.second.type == JsonValue::String) {
                config.environment[kv.first] = kv.second.str;
            }
        }
    }

    // 端口映射
    const JsonValue* j_ports = root.find("port_mappings");
    if (j_ports && j_ports->type == JsonValue::Array) {
        for (const auto& item : j_ports->arr) {
            if (item.type != JsonValue::Object) continue;
            const JsonValue* h = item.find("host");
            const JsonValue* c = item.find("container");
            if (h && c && h->type == JsonValue::Number && c->type == JsonValue::Number) {
                config.port_mappings.emplace_back(
                    static_cast<int>(h->num), static_cast<int>(c->num));
            }
        }
    }

    // 卷映射
    const JsonValue* j_vols = root.find("volume_mappings");
    if (j_vols && j_vols->type == JsonValue::Array) {
        for (const auto& item : j_vols->arr) {
            if (item.type != JsonValue::Object) continue;
            const JsonValue* h = item.find("host");
            const JsonValue* c = item.find("container");
            if (h && c && h->type == JsonValue::String && c->type == JsonValue::String) {
                config.volume_mappings.emplace_back(h->str, c->str);
            }
        }
    }

    std::string container_id = lifecycle_->createContainer(config);
    if (container_id.empty()) {
        response = makeErrorJson("Failed to create container");
        status_code = 500;
        return;
    }

    std::ostringstream oss;
    oss << "{\"id\":\"" << escapeJsonString(container_id) << "\"}";
    response = oss.str();
    status_code = 201;
}

void RestApi::handleListContainers(std::string& response, int& status_code) {
    if (!lifecycle_) {
        response = makeErrorJson("LifecycleManager not available");
        status_code = 500;
        return;
    }

    auto containers = lifecycle_->listContainers();
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < containers.size(); ++i) {
        if (i > 0) oss << ",";
        Container* c = containers[i];
        if (!c) continue;
        oss << "{\"id\":\"" << escapeJsonString(c->getId()) << "\",";
        oss << "\"name\":\"" << escapeJsonString(c->getConfig().name) << "\",";
        oss << "\"image\":\"" << escapeJsonString(c->getConfig().image) << "\",";
        oss << "\"status\":\"" << statusToStr(c->getStatus()) << "\"}";
    }
    oss << "]";
    response = oss.str();
    status_code = 200;
}

void RestApi::handleGetContainer(const std::string& id, std::string& response,
                                 int& status_code) {
    if (!lifecycle_) {
        response = makeErrorJson("LifecycleManager not available");
        status_code = 500;
        return;
    }

    Container* c = lifecycle_->getContainer(id);
    if (!c) {
        response = makeErrorJson("Container not found: " + id);
        status_code = 404;
        return;
    }

    // 直接复用 Container::inspect() 返回的 JSON
    response = c->inspect();
    status_code = 200;
}

void RestApi::handleStartContainer(const std::string& id, std::string& response,
                                   int& status_code) {
    if (!lifecycle_) {
        response = makeErrorJson("LifecycleManager not available");
        status_code = 500;
        return;
    }

    if (!lifecycle_->startContainer(id)) {
        response = makeErrorJson("Failed to start container: " + id);
        status_code = 500;
        return;
    }
    response = makeSuccessJson("Container started");
    status_code = 200;
}

void RestApi::handleStopContainer(const std::string& id, std::string& response,
                                  int& status_code) {
    if (!lifecycle_) {
        response = makeErrorJson("LifecycleManager not available");
        status_code = 500;
        return;
    }

    if (!lifecycle_->stopContainer(id)) {
        response = makeErrorJson("Failed to stop container: " + id);
        status_code = 500;
        return;
    }
    response = makeSuccessJson("Container stopped");
    status_code = 200;
}

void RestApi::handleDeleteContainer(const std::string& id, std::string& response,
                                    int& status_code) {
    if (!lifecycle_) {
        response = makeErrorJson("LifecycleManager not available");
        status_code = 500;
        return;
    }

    if (!lifecycle_->destroyContainer(id)) {
        response = makeErrorJson("Failed to delete container: " + id);
        status_code = 404;
        return;
    }
    response = makeSuccessJson("Container deleted");
    status_code = 200;
}

void RestApi::handleContainerLogs(const std::string& id, std::string& response,
                                  int& status_code) {
    if (!lifecycle_) {
        response = makeErrorJson("LifecycleManager not available");
        status_code = 500;
        return;
    }

    Container* c = lifecycle_->getContainer(id);
    if (!c) {
        response = makeErrorJson("Container not found: " + id);
        status_code = 404;
        return;
    }

    std::ostringstream oss;
    oss << "{\"logs\":\"" << escapeJsonString(c->getLogs()) << "\"}";
    response = oss.str();
    status_code = 200;
}

// ====== 镜像路由实现 ======

void RestApi::handleListImages(std::string& response, int& status_code) {
    if (!image_mgr_) {
        response = makeErrorJson("ImageManager not available");
        status_code = 500;
        return;
    }

    auto images = image_mgr_->listImages();
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < images.size(); ++i) {
        if (i > 0) oss << ",";
        const ImageInfo& img = images[i];
        oss << "{\"id\":\"" << escapeJsonString(img.id) << "\",";
        oss << "\"name\":\"" << escapeJsonString(img.name) << "\",";
        oss << "\"tag\":\"" << escapeJsonString(img.tag) << "\",";
        oss << "\"os\":\"" << escapeJsonString(img.os) << "\",";
        oss << "\"arch\":\"" << escapeJsonString(img.arch) << "\",";
        oss << "\"size\":" << img.size << "}";
    }
    oss << "]";
    response = oss.str();
    status_code = 200;
}

void RestApi::handlePullImage(const std::string& body, std::string& response,
                              int& status_code) {
    if (!image_mgr_) {
        response = makeErrorJson("ImageManager not available");
        status_code = 500;
        return;
    }

    // 解析 JSON 请求体: {"name": "ubuntu", "tag": "latest"} 或 {"name": "ubuntu:latest"}
    JsonParser parser(body);
    JsonValue root;
    if (!parser.parse(root) || root.type != JsonValue::Object) {
        response = makeErrorJson("Invalid JSON body");
        status_code = 400;
        return;
    }

    std::string name;
    std::string tag = "latest";

    const JsonValue* j_name = root.find("name");
    if (!j_name || j_name->type != JsonValue::String) {
        response = makeErrorJson("Missing 'name' field");
        status_code = 400;
        return;
    }
    name = j_name->str;

    // 支持 name:tag 形式
    size_t colon = name.find(':');
    if (colon != std::string::npos) {
        tag = name.substr(colon + 1);
        name = name.substr(0, colon);
    }

    const JsonValue* j_tag = root.find("tag");
    if (j_tag && j_tag->type == JsonValue::String) {
        tag = j_tag->str;
    }

    if (!image_mgr_->pullImage(name, tag)) {
        response = makeErrorJson("Failed to pull image: " + name + ":" + tag);
        status_code = 500;
        return;
    }

    std::ostringstream oss;
    oss << "{\"message\":\"Image pulled\","
        << "\"name\":\"" << escapeJsonString(name) << "\","
        << "\"tag\":\"" << escapeJsonString(tag) << "\"}";
    response = oss.str();
    status_code = 201;
}

void RestApi::handleDeleteImage(const std::string& name, std::string& response,
                                int& status_code) {
    if (!image_mgr_) {
        response = makeErrorJson("ImageManager not available");
        status_code = 500;
        return;
    }

    // name 可能是 "ubuntu:latest" 或 "ubuntu"
    std::string img_name = name;
    std::string tag = "latest";
    size_t colon = img_name.find(':');
    if (colon != std::string::npos) {
        tag = img_name.substr(colon + 1);
        img_name = img_name.substr(0, colon);
    }

    if (!image_mgr_->removeImage(img_name, tag)) {
        response = makeErrorJson("Failed to delete image: " + img_name + ":" + tag);
        status_code = 404;
        return;
    }

    std::ostringstream oss;
    oss << "{\"message\":\"Image deleted\","
        << "\"name\":\"" << escapeJsonString(img_name) << "\","
        << "\"tag\":\"" << escapeJsonString(tag) << "\"}";
    response = oss.str();
    status_code = 200;
}
