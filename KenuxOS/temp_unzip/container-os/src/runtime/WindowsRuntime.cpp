#include "runtime/WindowsRuntime.h"

#include "core/Container.h"
#include "utils/Logger.h"
#include "runtime/pe/PeLoader.h"
#include "syscall/Win32ApiEmulator.h"

#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <unordered_set>
#include <algorithm>
#include <vector>
#include <string>

WindowsRuntime::WindowsRuntime() 
    : pe_loader_(std::make_unique<PeLoader>()),
      win32_emulator_(std::make_unique<Win32ApiEmulator>()) {
}

WindowsRuntime::~WindowsRuntime() = default;

bool WindowsRuntime::setupWin32ApiEmulation(const std::string& rootfs) {
    Logger::getInstance().info("设置 Win32 API 模拟层: " + rootfs);
    (void)rootfs;
    return true;
}

bool WindowsRuntime::resolveDllDependencies(const std::string& exe_path,
                                            std::vector<std::string>& deps) {
    Logger::getInstance().info("解析 DLL 依赖: " + exe_path);

    if (pe_loader_->load(exe_path)) {
        auto imports = pe_loader_->getImports();
        for (const auto& import : imports) {
            deps.push_back(import.dll_name);
            for (const auto& func : import.functions) {
                Logger::getInstance().info("  导入: " + import.dll_name + "!" + func);
            }
        }
        return true;
    }

    deps.clear();
    return false;
}

bool WindowsRuntime::setupRegistryEmulation(const std::string& rootfs) {
    Logger::getInstance().info("设置注册表模拟: " + rootfs);
    (void)rootfs;
    return true;
}

std::unique_ptr<Container> WindowsRuntime::create(const ContainerConfig& config) {
    Logger::getInstance().info("WindowsRuntime 创建容器: " + config.name);

    if (config.command.empty()) {
        Logger::getInstance().error("启动命令为空");
        return nullptr;
    }

    if (!pe_loader_->load(config.command)) {
        Logger::getInstance().warn("命令非 PE 二进制: " + config.command +
                                   ", 可能是不兼容格式");
    } else {
        Logger::getInstance().info("PE 架构: " + 
            (pe_loader_->is64Bit() ? "x86_64" : "i386"));
        Logger::getInstance().info("PE 子系统: " + 
            (pe_loader_->getSubsystem() == 3 ? "console" : "gui"));
    }

    std::string id = config.name + "-" + std::to_string(::getpid());
    return std::unique_ptr<Container>(new Container(id, config));
}

bool WindowsRuntime::start(Container& container) {
    Logger::getInstance().info("WindowsRuntime 启动容器: " + container.getId());

    const ContainerConfig& config = container.getConfig();

    if (!pe_loader_->load(config.command)) {
        Logger::getInstance().error("PE 文件加载失败: " + config.command);
        return false;
    }

    if (!pe_loader_->parseImportTable()) {
        Logger::getInstance().warn("导入表解析失败");
    }

    if (!pe_loader_->parseExportTable()) {
        Logger::getInstance().warn("导出表解析失败");
    }

    auto imports = pe_loader_->getImports();
    Logger::getInstance().info("导入的 DLL 数量: " + std::to_string(imports.size()));

    std::vector<std::string> deps;
    if (!resolveDllDependencies(config.command, deps)) {
        Logger::getInstance().warn("DLL 依赖解析失败");
    }

    win32_emulator_->SetLastError(ERROR_SUCCESS);

    Logger::getInstance().info("PE 入口点: 0x" + 
        std::to_string(pe_loader_->getEntryPoint()));

    return true;
}

bool WindowsRuntime::stop(Container& container, int timeout_sec) {
    Logger::getInstance().info("WindowsRuntime 停止容器: " + container.getId() +
                               ", 超时: " + std::to_string(timeout_sec) + "s");
    return true;
}

bool WindowsRuntime::destroy(Container& container) {
    Logger::getInstance().info("WindowsRuntime 销毁容器: " + container.getId());

    if (!stop(container, 10)) {
        Logger::getInstance().warn("停止容器失败, 继续清理资源");
    }

    return container.destroy();
}

bool WindowsRuntime::supportsPlatform(const std::string& os_type) const {
    return os_type == "windows";
}

std::string WindowsRuntime::getRuntimeName() const {
    return "windows-runtime";
}

bool WindowsRuntime::canExecute(const std::string& binary_path) const {
    return pe_loader_->load(binary_path) && 
           ::access(binary_path.c_str(), R_OK) == 0;
}
