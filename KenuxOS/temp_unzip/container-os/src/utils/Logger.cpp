#include "utils/Logger.h"

#include <iostream>
#include <ctime>
#include <sstream>
#include <iomanip>

// 获取单例实例
Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

Logger::Logger()
    : level_(LogLevel::Info), console_output_(true) {}

Logger::~Logger() {
    if (file_stream_.is_open()) {
        file_stream_.close();
    }
}

void Logger::setLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    level_ = level;
}

LogLevel Logger::getLevel() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return level_;
}

void Logger::setConsoleOutput(bool enable) {
    std::lock_guard<std::mutex> lock(mutex_);
    console_output_ = enable;
}

bool Logger::setFileOutput(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);
    // 关闭已打开的旧文件
    if (file_stream_.is_open()) {
        file_stream_.close();
    }
    file_stream_.open(filename, std::ios::out | std::ios::app);
    return file_stream_.is_open();
}

void Logger::closeFileOutput() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_stream_.is_open()) {
        file_stream_.close();
    }
}

void Logger::trace(const std::string& message) { log(LogLevel::Trace, message); }
void Logger::debug(const std::string& message) { log(LogLevel::Debug, message); }
void Logger::info(const std::string& message)  { log(LogLevel::Info, message); }
void Logger::warn(const std::string& message)  { log(LogLevel::Warn, message); }
void Logger::error(const std::string& message) { log(LogLevel::Error, message); }
void Logger::fatal(const std::string& message) { log(LogLevel::Fatal, message); }

void Logger::log(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    // 低于当前级别的日志直接丢弃
    if (level < level_) {
        return;
    }
    writeLog(level, message);
}

void Logger::writeLog(LogLevel level, const std::string& message) {
    // 组装日志行：[时间] [级别] 消息
    std::string line;
    line.reserve(message.size() + 32);
    line += '[';
    line += getCurrentTimeString();
    line += "] [";
    line += levelToString(level);
    line += "] ";
    line += message;

    // 控制台输出：warn 及以上输出到 stderr，其余到 stdout
    if (console_output_) {
        if (level >= LogLevel::Warn) {
            std::cerr << line << std::endl;
        } else {
            std::cout << line << std::endl;
        }
    }

    // 文件输出
    if (file_stream_.is_open()) {
        file_stream_ << line << '\n';
        file_stream_.flush();
    }
}

const char* Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Fatal: return "FATAL";
    }
    return "UNKNOWN";
}

std::string Logger::getCurrentTimeString() {
    using namespace std::chrono;

    // 获取当前时间点
    system_clock::time_point now = system_clock::now();

    // 转换为 time_t 以获取秒级时间
    std::time_t now_time = system_clock::to_time_t(now);

    // 线程安全地转换为本地时间
    // Windows 使用 localtime_s，POSIX 使用 localtime_r
    std::tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &now_time);
#else
    localtime_r(&now_time, &tm_buf);
#endif

    // 计算毫秒部分
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count();

    return oss.str();
}
