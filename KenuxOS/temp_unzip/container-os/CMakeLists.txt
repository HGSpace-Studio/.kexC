# CMake 构建配置文件
# container-os 跨平台容器系统
# C++17, 支持可选的 Qt6 / nlohmann_json / spdlog 依赖

cmake_minimum_required(VERSION 3.16)
project(container-os VERSION 1.0.0 LANGUAGES CXX C)

# ==================== C++ 标准 ====================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ==================== 编译选项 ====================

# 警告与优化选项 (适用于 GCC/Clang)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -O2)
endif()

# 默认 Release 构建类型
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ==================== 依赖查找 ====================

# Threads (必须依赖)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Qt6 (可选依赖, 用于 GUI 图形界面)
find_package(Qt6 COMPONENTS Widgets QUIET)

# nlohmann_json (可选依赖, 用于 JSON 解析)
find_package(nlohmann_json QUIET)

# spdlog (可选依赖, 用于日志)
find_package(spdlog QUIET)

# 输出依赖检测结果
message(STATUS "========== 依赖检测结果 ==========")
message(STATUS "Threads:        已找到 (必须)")
if(Qt6_FOUND)
    message(STATUS "Qt6:            已找到 (启用 GUI 图形界面)")
else()
    message(STATUS "Qt6:            未找到 (使用 TUI 文本界面)")
endif()
if(nlohmann_json_FOUND)
    message(STATUS "nlohmann_json:  已找到")
else()
    message(STATUS "nlohmann_json:  未找到 (使用内置 JSON 解析)")
endif()
if(spdlog_FOUND)
    message(STATUS "spdlog:         已找到")
else()
    message(STATUS "spdlog:         未找到 (使用内置 Logger)")
endif()
message(STATUS "==================================")

# ==================== 全局包含路径 ====================

# 所有目标共享 include/ 目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# ==================== 辅助函数 ====================

# 从 src/<name>/ 下的 .cpp 和 .c 创建库
# 如果目录下没有源文件, 创建 INTERFACE 库 (无编译产物, 仅传递依赖)
# 参数:
#   name: 库名称
#   DEPS: 依赖的其他库 (PUBLIC 链接)
function(add_container_lib name)
    cmake_parse_arguments(ARG "" "" "DEPS" ${ARGN})
    file(GLOB_RECURSE cpp_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/${name}/*.cpp")
    file(GLOB_RECURSE c_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/${name}/*.c")
    set(sources ${cpp_sources} ${c_sources})
    if(sources)
        add_library(${name} ${sources})
        if(ARG_DEPS)
            target_link_libraries(${name} PUBLIC ${ARG_DEPS})
        endif()
    else()
        # 无源文件时创建 INTERFACE 库, 仅传递依赖关系
        add_library(${name} INTERFACE)
        if(ARG_DEPS)
            target_link_libraries(${name} INTERFACE ${ARG_DEPS})
        endif()
    endif()
endfunction()

# ==================== 库目标定义 ====================

# 依赖关系说明:
#   utils        -> (无)
#   storage      -> utils, core (Filesystem, ImageManager, VolumeManager, FsAdapter + 容器文件系统 C 源码)
#   syscall      -> utils, storage (LinuxSyscallHandler 转发到 FsAdapter, Win32ApiEmulator, SyscallDispatcher)
#   core         -> utils (Container, IsolationManager, Runtime)
#   isolation    -> core, utils (Cgroups, Namespace, ProcessGroup)
#   runtime      -> core, utils, syscall (ELF/PE 加载器, Linux/Windows 运行时)
#   orchestration -> core, isolation, runtime, storage, utils
#   api          -> orchestration, storage
#   cli          -> orchestration, storage
#   gui          -> orchestration, storage (如有 Qt6 则额外链接 Qt6)

# utils: 日志、配置、验证工具 (Logger, Config, Validator)
add_container_lib(utils)

# core: 容器核心类 (Container, IsolationManager, Runtime)
add_container_lib(core DEPS utils)

# storage: 存储层 (Filesystem, ImageManager, VolumeManager, FsAdapter + 容器专用文件系统)
add_container_lib(storage DEPS utils core)

# syscall: 系统调用兼容层 (LinuxSyscallHandler 转发到 FsAdapter, Win32ApiEmulator, SyscallDispatcher)
add_container_lib(syscall DEPS utils storage)

# isolation: 隔离层 (Cgroups, Namespace, ProcessGroup)
add_container_lib(isolation DEPS core utils)

# runtime: 运行时 (ELF/PE 加载器, CrossPlatformAdapter, LinuxRuntime, WindowsRuntime)
add_container_lib(runtime DEPS core utils syscall)

# orchestration: 编排层 (LifecycleManager, NetworkManager, ResourceManager, Scheduler)
add_container_lib(orchestration DEPS core isolation runtime storage utils)

# api: RESTful API 层 (当前无源文件, 仅传递依赖)
add_container_lib(api DEPS orchestration storage)

# cli: 命令行接口层 (当前无源文件, 仅传递依赖)
add_container_lib(cli DEPS orchestration storage)

# gui: GUI 层 (ContainerManagerWindow, 需要特殊处理 Qt6)
file(GLOB GUI_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/gui/*.cpp")
if(GUI_SOURCES)
    add_library(gui ${GUI_SOURCES})
    target_link_libraries(gui PUBLIC orchestration storage)
    if(Qt6_FOUND)
        # 启用 Qt 元对象编译器 (处理 Q_OBJECT 宏)
        set_target_properties(gui PROPERTIES AUTOMOC ON)
        # 链接 Qt6 Widgets 和 Core
        target_link_libraries(gui PUBLIC Qt6::Widgets Qt6::Core)
        # 定义 HAVE_QT 宏 (PUBLIC 使依赖 gui 的目标也能看到)
        target_compile_definitions(gui PUBLIC HAVE_QT)
    endif()
else()
    # 无 GUI 源文件时创建 INTERFACE 库
    add_library(gui INTERFACE)
    target_link_libraries(gui INTERFACE orchestration storage)
    if(Qt6_FOUND)
        target_compile_definitions(gui INTERFACE HAVE_QT)
    endif()
endif()

# ==================== 可选依赖宏定义 ====================

# nlohmann_json: 定义 HAVE_JSON 宏
if(nlohmann_json_FOUND)
    target_compile_definitions(utils PUBLIC HAVE_JSON)
    target_link_libraries(utils PRIVATE nlohmann_json::nlohmann_json)
endif()

# spdlog: 定义 HAVE_SPDLOG 宏
if(spdlog_FOUND)
    target_compile_definitions(utils PUBLIC HAVE_SPDLOG)
    target_link_libraries(utils PRIVATE spdlog::spdlog)
endif()

# ==================== 主可执行文件 ====================

add_executable(container-os src/main.cpp)

# 链接所有需要的库
target_link_libraries(container-os PRIVATE
    gui               # GUI 层 (含 Qt6 或 TUI)
    cli               # CLI 层
    api               # API 层
    orchestration     # 编排层
    storage           # 存储层
    runtime           # 运行时层
    isolation         # 隔离层
    core              # 核心层
    syscall           # Win32 API 模拟层
    utils             # 工具层
    Threads::Threads  # 线程库
)

# ==================== 安装目标 ====================

install(TARGETS container-os DESTINATION bin)

# ==================== 测试目标 ====================

enable_testing()

# 文件系统集成测试
add_executable(test_fs_integration
    tests/test_fs_integration.cpp
)
target_link_libraries(test_fs_integration PRIVATE
    storage
    syscall
    utils
    Threads::Threads
)
add_test(NAME FsIntegrationTest COMMAND test_fs_integration)
