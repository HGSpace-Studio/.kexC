#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <chrono>

// 日志级别枚举
// 严重程度由低到高：Trace < Debug < Info < Warn < Error < Fatal
enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Fatal
};

// 日志系统（单例模式）
// 支持 trace/debug/info/warn/error/fatal 六个级别
// 支持控制台与文件输出，线程安全，使用 chrono 记录时间戳
// 不依赖任何第三方库
class Logger {
public:
    // 获取单例实例
    // C++11 起保证局部静态变量初始化的线程安全
    static Logger& getInstance();

    // 禁止拷贝与赋值
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // 设置/获取当前日志级别，低于该级别的日志将被丢弃
    void setLevel(LogLevel level);
    LogLevel getLevel() const;

    // 启用或关闭控制台输出（默认开启）
    void setConsoleOutput(bool enable);

    // 设置文件输出路径，成功返回 true
    // 若之前已打开文件，会先关闭旧文件再打开新文件
    bool setFileOutput(const std::string& filename);

    // 关闭文件输出
    void closeFileOutput();

    // 各级别日志接口
    void trace(const std::string& message);
    void debug(const std::string& message);
    void info(const std::string& message);
    void warn(const std::string& message);
    void error(const std::string& message);
    void fatal(const std::string& message);

    // 通用日志接口，按指定级别写入
    void log(LogLevel level, const std::string& message);

private:
    Logger();
    ~Logger();

    // 实际写入逻辑（调用前已持有锁）
    void writeLog(LogLevel level, const std::string& message);

    // 日志级别转字符串
    static const char* levelToString(LogLevel level);

    // 获取当前时间字符串，格式：YYYY-MM-DD HH:MM:SS.mmm
    static std::string getCurrentTimeString();

    LogLevel level_;
    bool console_output_;
    std::ofstream file_stream_;
    mutable std::mutex mutex_;
};
