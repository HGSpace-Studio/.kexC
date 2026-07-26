#pragma once

#include "core/Runtime.h"

#include <memory>
#include <string>
#include <vector>

// 跨平台运行时适配器
// 维护多个运行时实例, 根据二进制文件格式自动选择合适的运行时执行
class CrossPlatformAdapter {
public:
    // 二进制格式枚举
    enum class BinaryFormat {
        ELF,      // Linux 可执行与可链接格式
        PE,       // Windows 可移植可执行格式
        Script,   // 脚本文件 (以 "#!" 开头, 由解释器执行)
        Unknown   // 未知格式
    };

    CrossPlatformAdapter();
    ~CrossPlatformAdapter();

    // 根据二进制文件路径检测格式后, 返回对应 Runtime 指针
    // 未找到匹配运行时返回 nullptr
    Runtime* selectRuntime(const std::string& binary_path);

    // 检测指定路径的二进制格式
    BinaryFormat detectFormat(const std::string& path) const;

    // 获取所有已注册运行时
    const std::vector<std::unique_ptr<Runtime>>& getRuntimes() const;

    // 注册新运行时, 转移所有权
    void registerRuntime(std::unique_ptr<Runtime> runtime);

private:
    // 读取文件前 len 字节作为魔数
    // 成功返回 true, 并将数据写入 magic 缓冲区
    bool readFileMagic(const std::string& path, char* magic, size_t len) const;

    // 已注册的运行时列表
    std::vector<std::unique_ptr<Runtime>> runtimes_;
};
