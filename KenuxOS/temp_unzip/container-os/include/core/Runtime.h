#pragma once

#include <memory>
#include <string>
#include "core/Container.h"

// 运行时抽象基类
// 定义容器运行时的统一接口, 具体实现 (LinuxRuntime / WindowsRuntime 等) 负责实际执行
class Runtime {
public:
    virtual ~Runtime();

    // 容器生命周期管理 (纯虚函数, 由具体运行时实现)
    virtual std::unique_ptr<Container> create(const ContainerConfig& config) = 0;
    virtual bool start(Container& container) = 0;
    virtual bool stop(Container& container, int timeout_sec = 10) = 0;
    virtual bool destroy(Container& container) = 0;

    // 运行时能力查询
    // 是否支持指定的操作系统类型 (如 "linux" / "windows")
    virtual bool supportsPlatform(const std::string& os_type) const = 0;
    // 获取运行时名称
    virtual std::string getRuntimeName() const = 0;
    // 检测是否可以执行指定路径的二进制文件
    virtual bool canExecute(const std::string& binary_path) const = 0;
};
