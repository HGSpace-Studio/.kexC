#pragma once

#include "core/Runtime.h"
#include "runtime/pe/PeLoader.h"
#include "syscall/Win32ApiEmulator.h"

#include <string>
#include <vector>

class WindowsRuntime : public Runtime {
public:
    WindowsRuntime();
    ~WindowsRuntime() override;

    std::unique_ptr<Container> create(const ContainerConfig& config) override;
    bool start(Container& container) override;
    bool stop(Container& container, int timeout_sec = 10) override;
    bool destroy(Container& container) override;

    bool supportsPlatform(const std::string& os_type) const override;
    std::string getRuntimeName() const override;
    bool canExecute(const std::string& binary_path) const override;

private:
    bool setupWin32ApiEmulation(const std::string& rootfs);
    bool resolveDllDependencies(const std::string& exe_path,
                                std::vector<std::string>& deps);
    bool setupRegistryEmulation(const std::string& rootfs);

    std::unique_ptr<PeLoader> pe_loader_;
    std::unique_ptr<Win32ApiEmulator> win32_emulator_;
};
