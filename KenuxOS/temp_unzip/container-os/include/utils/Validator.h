#pragma once

#include <string>
#include <cstdint>

// 输入验证工具类
// 提供容器名称、端口号、IP 地址、镜像名称、内存大小、文件路径等校验
// 所有方法均为静态方法，纯工具类禁止实例化
class Validator {
public:
    // 验证容器名称：仅允许字母、数字和横杠
    // 长度 1~63，不能以横杠开头或结尾
    static bool validateContainerName(const std::string& name);

    // 验证端口号：有效范围 1~65535
    static bool validatePort(int port);

    // 验证端口号（字符串形式，必须全部为数字）
    static bool validatePort(const std::string& port_str);

    // 验证 IPv4 地址格式，如 192.168.1.1
    // 四段 0~255 的数字，拒绝前导零
    static bool validateIpAddress(const std::string& ip);

    // 验证镜像名称格式：name:tag，如 ubuntu:20.04、myrepo/app:v1.0
    // name 可含小写字母、数字、横杠、点、斜杠、冒号（用于 registry:port）
    // tag 可含字母、数字、点、横杠、下划线，且不含斜杠
    static bool validateImageName(const std::string& image);

    // 验证内存大小字符串，如 "2G"、"512M"、"1024K"、"1T"、"512B"
    // 单位不区分大小写；格式非法返回 false
    static bool validateMemorySize(const std::string& mem_str);

    // 验证内存大小字符串并解析为字节数
    // 解析成功返回 true 并通过 out_bytes 输出字节数
    static bool validateMemorySize(const std::string& mem_str, uint64_t& out_bytes);

    // 验证文件路径基本合法性（跨平台）：
    // 非空、长度不超过 4096、不含控制字符、不含跨平台禁止字符 < > " | ? *
    static bool validateFilePath(const std::string& path);

private:
    Validator() = delete; // 纯静态工具类，禁止实例化
};
