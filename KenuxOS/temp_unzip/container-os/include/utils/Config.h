#pragma once

#include <string>
#include <unordered_map>
#include <mutex>

// 配置管理（单例模式）
// 支持加载 JSON 风格配置文件或 key=value 格式配置文件
// 提供 get/getInt/getBool/getDouble 等访问方法，均支持默认值
// 线程安全，无第三方依赖
class Config {
public:
    // 获取单例实例
    static Config& getInstance();

    // 禁止拷贝与赋值
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;

    // 从文件加载配置，成功返回 true
    // 自动识别格式：以 '{' 开头视为 JSON，否则按 key=value 解析
    // 加载会替换原有配置
    bool loadFromFile(const std::string& filename);

    // 从字符串加载配置
    bool loadFromString(const std::string& content);

    // 清空所有配置项
    void clear();

    // 设置配置项
    void set(const std::string& key, const std::string& value);

    // 读取配置项（带默认值）
    std::string get(const std::string& key, const std::string& default_value = "") const;
    int getInt(const std::string& key, int default_value = 0) const;
    bool getBool(const std::string& key, bool default_value = false) const;
    double getDouble(const std::string& key, double default_value = 0.0) const;

    // 判断键是否存在
    bool has(const std::string& key) const;

private:
    Config();
    ~Config();

    // 简单 JSON 扁平对象解析（仅支持字符串/数字/布尔/null）
    bool parseSimpleJson(const std::string& content);

    // key=value 格式解析
    bool parseKeyValue(const std::string& content);

    // 去除字符串首尾空白
    static std::string trim(const std::string& s);

    // 去除字符串两端双引号
    static std::string stripQuotes(const std::string& s);

    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::string> values_;
};
