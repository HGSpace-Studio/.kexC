#include "utils/Validator.h"

#include <cctype>
#include <string>

bool Validator::validateContainerName(const std::string& name) {
    // 容器名称：仅允许字母、数字和横杠
    // 长度 1~63，不能以横杠开头或结尾
    if (name.empty() || name.size() > 63) {
        return false;
    }
    if (name.front() == '-' || name.back() == '-') {
        return false;
    }
    for (char c : name) {
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '-')) {
            return false;
        }
    }
    return true;
}

bool Validator::validatePort(int port) {
    // 端口号有效范围 1~65535
    return port >= 1 && port <= 65535;
}

bool Validator::validatePort(const std::string& port_str) {
    if (port_str.empty()) {
        return false;
    }
    // 必须全部为数字
    for (char c : port_str) {
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    try {
        // 使用无符号长整型避免溢出后误判
        unsigned long val = std::stoul(port_str);
        return val >= 1 && val <= 65535;
    } catch (...) {
        return false;
    }
}

bool Validator::validateIpAddress(const std::string& ip) {
    // 验证 IPv4 地址：四段 0~255 的数字，以 '.' 分隔
    // 拒绝前导零（"0" 合法，"01" 非法）
    if (ip.empty()) {
        return false;
    }

    int segment_count = 0;
    size_t start = 0;
    for (size_t i = 0; i <= ip.size(); ++i) {
        if (i == ip.size() || ip[i] == '.') {
            std::string seg = ip.substr(start, i - start);
            start = i + 1;
            ++segment_count;

            if (seg.empty()) {
                return false; // 空段，如 "1..2" 或 ".1.2.3.4"
            }
            for (char c : seg) {
                if (!std::isdigit(static_cast<unsigned char>(c))) {
                    return false;
                }
            }
            // 拒绝前导零
            if (seg.size() > 1 && seg[0] == '0') {
                return false;
            }
            // 限制长度避免溢出
            if (seg.size() > 3) {
                return false;
            }
            int val = std::stoi(seg);
            if (val > 255) {
                return false;
            }

            if (i == ip.size()) {
                break; // 已处理最后一段
            }
        }
    }

    return segment_count == 4;
}

bool Validator::validateImageName(const std::string& image) {
    // 镜像名称格式：name:tag
    // 以最后一个冒号分隔 name 与 tag
    // name 可含小写字母、数字、横杠、点、斜杠、冒号（用于 registry:port）
    // tag 可含字母、数字、点、横杠、下划线，且不含斜杠
    size_t colon = image.rfind(':');
    if (colon == std::string::npos || colon == 0) {
        return false;
    }
    std::string name = image.substr(0, colon);
    std::string tag = image.substr(colon + 1);
    if (name.empty() || tag.empty()) {
        return false;
    }
    // tag 不能包含斜杠（否则该冒号属于 registry 端口，而非 tag 分隔符）
    if (tag.find('/') != std::string::npos) {
        return false;
    }
    for (char c : name) {
        if (!(std::islower(static_cast<unsigned char>(c)) ||
              std::isdigit(static_cast<unsigned char>(c)) ||
              c == '-' || c == '.' || c == '/' || c == ':')) {
            return false;
        }
    }
    for (char c : tag) {
        if (!(std::isalnum(static_cast<unsigned char>(c)) ||
              c == '.' || c == '-' || c == '_')) {
            return false;
        }
    }
    return true;
}

bool Validator::validateMemorySize(const std::string& mem_str) {
    uint64_t bytes = 0;
    return validateMemorySize(mem_str, bytes);
}

bool Validator::validateMemorySize(const std::string& mem_str, uint64_t& out_bytes) {
    // 内存大小格式：<数字><单位>
    // 单位：B(字节)、K(1024B)、M、G、T，不区分大小写
    // 例如 "512M"、"2G"、"1024K"、"1T"、"512B"
    if (mem_str.empty()) {
        return false;
    }

    // 分离数字部分与单位部分
    size_t idx = 0;
    while (idx < mem_str.size() &&
           (std::isdigit(static_cast<unsigned char>(mem_str[idx])) || mem_str[idx] == '.')) {
        ++idx;
    }
    if (idx == 0) {
        return false; // 缺少数字
    }

    std::string num_part = mem_str.substr(0, idx);
    std::string unit_part = mem_str.substr(idx);

    // 单位转大写并去除空白
    std::string unit;
    for (char c : unit_part) {
        if (c == ' ' || c == '\t') {
            continue;
        }
        unit.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }
    if (unit.empty()) {
        return false; // 缺少单位
    }

    // 解析数字（允许整数与小数）
    double num = 0.0;
    try {
        size_t pos = 0;
        num = std::stod(num_part, &pos);
        std::string rest = num_part.substr(pos);
        bool rest_ok = true;
        for (char c : rest) {
            if (c != ' ' && c != '\t') {
                rest_ok = false;
                break;
            }
        }
        if (!rest_ok) {
            return false;
        }
    } catch (...) {
        return false;
    }
    if (num < 0) {
        return false;
    }

    // 单位换算（二进制 1024 进制）
    uint64_t multiplier = 1;
    if (unit == "B") {
        multiplier = 1ULL;
    } else if (unit == "K") {
        multiplier = 1024ULL;
    } else if (unit == "M") {
        multiplier = 1024ULL * 1024ULL;
    } else if (unit == "G") {
        multiplier = 1024ULL * 1024ULL * 1024ULL;
    } else if (unit == "T") {
        multiplier = 1024ULL * 1024ULL * 1024ULL * 1024ULL;
    } else {
        return false; // 未知单位
    }

    out_bytes = static_cast<uint64_t>(num * static_cast<double>(multiplier));
    return true;
}

bool Validator::validateFilePath(const std::string& path) {
    // 验证文件路径基本合法性（跨平台）：
    // - 非空
    // - 长度不超过 4096
    // - 不含控制字符（含空字节与 DEL）
    // - 不含在 Windows 下非法且跨平台无意义的字符：< > " | ? *
    if (path.empty() || path.size() > 4096) {
        return false;
    }
    for (char c : path) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (uc < 32 || uc == 127) {
            return false; // 控制字符
        }
        if (c == '<' || c == '>' || c == '"' || c == '|' || c == '?' || c == '*') {
            return false; // 跨平台禁止字符
        }
    }
    return true;
}
