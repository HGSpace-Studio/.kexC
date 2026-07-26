#include "runtime/CrossPlatformAdapter.h"

#include "runtime/LinuxRuntime.h"
#include "runtime/WindowsRuntime.h"
#include "utils/Logger.h"

#include <unistd.h>
#include <fcntl.h>
#include <cstring>

CrossPlatformAdapter::CrossPlatformAdapter() {
    // 默认注册 Linux 与 Windows 两种运行时
    registerRuntime(std::unique_ptr<Runtime>(new LinuxRuntime()));
    registerRuntime(std::unique_ptr<Runtime>(new WindowsRuntime()));
    Logger::getInstance().info("CrossPlatformAdapter 已注册 " +
                               std::to_string(runtimes_.size()) + " 个运行时");
}

CrossPlatformAdapter::~CrossPlatformAdapter() = default;

bool CrossPlatformAdapter::readFileMagic(const std::string& path, char* magic,
                                         size_t len) const {
    // 打开文件并读取前 len 字节作为魔数
    if (magic == nullptr || len == 0) {
        return false;
    }

    int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return false;
    }

    ssize_t n = ::read(fd, magic, len);
    ::close(fd);

    // 不足 len 字节也视为成功, 调用方按实际读取量判断
    return n > 0;
}

CrossPlatformAdapter::BinaryFormat CrossPlatformAdapter::detectFormat(
    const std::string& path) const {
    // 读取文件前 4 字节判断格式
    char magic[4] = {0};
    if (!readFileMagic(path, magic, sizeof(magic))) {
        return BinaryFormat::Unknown;
    }

    // ELF: 0x7f 'E' 'L' 'F'
    if (static_cast<unsigned char>(magic[0]) == 0x7f &&
        magic[1] == 'E' && magic[2] == 'L' && magic[3] == 'F') {
        return BinaryFormat::ELF;
    }

    // PE: 'M' 'Z' (DOS 头), 仅需前 2 字节即可判定, 完整判定应包含 PE 签名
    if (magic[0] == 'M' && magic[1] == 'Z') {
        return BinaryFormat::PE;
    }

    // Script: '#!' 开头
    if (magic[0] == '#' && magic[1] == '!') {
        return BinaryFormat::Script;
    }

    return BinaryFormat::Unknown;
}

Runtime* CrossPlatformAdapter::selectRuntime(const std::string& binary_path) {
    // 1. 检测文件格式
    BinaryFormat format = detectFormat(binary_path);

    // 2. 根据格式选择对应运行时
    //    ELF  -> LinuxRuntime
    //    PE   -> WindowsRuntime
    //    Script/Unknown -> 优先选择当前平台运行时
    for (const auto& rt : runtimes_) {
        switch (format) {
            case BinaryFormat::ELF:
                if (rt->supportsPlatform("linux") &&
                    rt->canExecute(binary_path)) {
                    Logger::getInstance().info("选择运行时: " +
                                               rt->getRuntimeName() +
                                               " (ELF 格式)");
                    return rt.get();
                }
                break;
            case BinaryFormat::PE:
                if (rt->supportsPlatform("windows") &&
                    rt->canExecute(binary_path)) {
                    Logger::getInstance().info("选择运行时: " +
                                               rt->getRuntimeName() +
                                               " (PE 格式)");
                    return rt.get();
                }
                break;
            case BinaryFormat::Script:
                // 脚本由解释器执行, 优先使用 Linux 运行时
                if (rt->supportsPlatform("linux")) {
                    Logger::getInstance().info("选择运行时: " +
                                               rt->getRuntimeName() +
                                               " (Script 格式)");
                    return rt.get();
                }
                break;
            default:
                break;
        }
    }

    // 未匹配到合适运行时, 回退到第一个可执行该文件的运行时
    for (const auto& rt : runtimes_) {
        if (rt->canExecute(binary_path)) {
            Logger::getInstance().info("回退选择运行时: " +
                                       rt->getRuntimeName());
            return rt.get();
        }
    }

    Logger::getInstance().warn("未找到可执行 " + binary_path + " 的运行时");
    return nullptr;
}

const std::vector<std::unique_ptr<Runtime>>& CrossPlatformAdapter::getRuntimes() const {
    return runtimes_;
}

void CrossPlatformAdapter::registerRuntime(std::unique_ptr<Runtime> runtime) {
    if (runtime == nullptr) {
        return;
    }
    Logger::getInstance().info("注册运行时: " + runtime->getRuntimeName());
    runtimes_.push_back(std::move(runtime));
}
