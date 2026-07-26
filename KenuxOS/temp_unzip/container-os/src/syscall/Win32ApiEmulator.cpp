#include "syscall/Win32ApiEmulator.h"
#include "syscall/LinuxSyscallHandler.h"

#include "utils/Logger.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <dlfcn.h>
#include <cstring>
#include <cerrno>
#include <climits>

// 类型别名定义，补充头文件中未定义的指针类型
using LPDWORD = DWORD*;
using LPPROCESS_INFORMATION = PROCESS_INFORMATION*;
using LPSTARTUPINFOA = STARTUPINFOA*;

// 私有命名空间，定义内部常量和辅助函数
namespace {

// 无效的文件描述符
constexpr int INVALID_FD = -1;

// 无限等待常量
constexpr DWORD INFINITE = 0xFFFFFFFF;

// Linux open 标志与 Win32 的映射关系
// Win32 访问权限到 Linux O_RDONLY/O_WRONLY/O_RDWR 的转换
int mapDesiredAccessToOpenFlags(DWORD dwDesiredAccess) {
    if (dwDesiredAccess & GENERIC_READ) {
        if (dwDesiredAccess & GENERIC_WRITE) {
            return O_RDWR;
        }
        return O_RDONLY;
    }
    if (dwDesiredAccess & GENERIC_WRITE) {
        return O_WRONLY;
    }
    return O_RDONLY;
}

// Win32 创建方式到 Linux O_CREAT/O_EXCL/O_TRUNC 的转换
int mapCreationDispositionToOpenFlags(DWORD dwCreationDisposition) {
    switch (dwCreationDisposition) {
        case CREATE_NEW:
            return O_CREAT | O_EXCL;
        case CREATE_ALWAYS:
            return O_CREAT | O_TRUNC;
        case OPEN_EXISTING:
        default:
            return 0;
    }
}

// Win32 文件属性到 Linux mode_t 的转换
mode_t mapFileAttributesToMode(DWORD dwFlagsAndAttributes) {
    mode_t mode = 0644;
    // TODO: 自定义 OS 上文件权限模型可能不同，需根据实际情况调整
    if (dwFlagsAndAttributes & FILE_ATTRIBUTE_NORMAL) {
        mode = 0644;
    }
    return mode;
}

// Linux errno 到 Win32 错误码的映射
DWORD mapErrnoToWin32Error(int error) {
    switch (error) {
        case ENOENT:
            return ERROR_FILE_NOT_FOUND;
        case EACCES:
        case EPERM:
            return ERROR_ACCESS_DENIED;
        case EEXIST:
            return 80; // ERROR_FILE_EXISTS
        case EINVAL:
            return 87; // ERROR_INVALID_PARAMETER
        case EMFILE:
            return 48; // ERROR_TOO_MANY_OPEN_FILES
        case ENOMEM:
            return 8;  // ERROR_NOT_ENOUGH_MEMORY
        default:
            return static_cast<DWORD>(error) | 0x80000000;
    }
}

// Win32 内存保护属性到 Linux PROT_* 的转换
int mapProtectToProtFlags(DWORD flProtect) {
    int prot = 0;
    // TODO: 自定义 OS 上内存保护模型可能不同，需根据实际情况调整
    if (flProtect & 0x01) { // PAGE_READONLY
        prot |= PROT_READ;
    }
    if (flProtect & 0x02) { // PAGE_READWRITE
        prot |= PROT_READ | PROT_WRITE;
    }
    if (flProtect & 0x04) { // PAGE_EXECUTE
        prot |= PROT_EXEC;
    }
    if (flProtect & 0x08) { // PAGE_EXECUTE_READ
        prot |= PROT_EXEC | PROT_READ;
    }
    if (flProtect & 0x10) { // PAGE_EXECUTE_READWRITE
        prot |= PROT_EXEC | PROT_READ | PROT_WRITE;
    }
    if (prot == 0) {
        prot = PROT_READ | PROT_WRITE;
    }
    return prot;
}

// Win32 内存分配类型到 Linux MAP_* 的转换
int mapAllocationTypeToMapFlags(DWORD flAllocationType) {
    int flags = MAP_PRIVATE | MAP_ANONYMOUS;
    // TODO: 自定义 OS 上 mmap 标志可能不同，需根据实际情况调整
    if (flAllocationType & 0x1000) { // MEM_RESERVE
        flags |= MAP_NORESERVE;
    }
    if (flAllocationType & 0x2000) { // MEM_COMMIT
        // MAP_ANONYMOUS 已隐含 COMMIT
    }
    return flags;
}

} // namespace

// ===== 构造函数与析构函数 =====

Win32ApiEmulator::Win32ApiEmulator()
    : next_handle_(1), next_module_(1), last_error_(ERROR_SUCCESS) {
    Logger::getInstance().info("Win32ApiEmulator 初始化");
}

Win32ApiEmulator::~Win32ApiEmulator() {
    // 清理所有打开的句柄
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& pair : handle_to_fd_) {
        containeros::LinuxSyscallHandler::close(pair.second);
    }
    handle_to_fd_.clear();

    // 清理所有加载的 DLL
    for (const auto& pair : module_to_dll_) {
        if (pair.second.handle) {
            ::dlclose(pair.second.handle);
        }
    }
    module_to_dll_.clear();

    Logger::getInstance().info("Win32ApiEmulator 销毁");
}

// ===== 文件操作函数实现 =====

HANDLE Win32ApiEmulator::CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess,
                                     DWORD dwShareMode, LPVOID lpSecurityAttributes,
                                     DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes,
                                     HANDLE hTemplateFile) {
    // 参数校验
    if (!lpFileName || *lpFileName == '\0') {
        SetLastError(ERROR_INVALID_PARAMETER);
        return INVALID_HANDLE_VALUE;
    }

    // 转换 Win32 参数到 Linux open 系统调用参数
    int open_flags = mapDesiredAccessToOpenFlags(dwDesiredAccess);
    open_flags |= mapCreationDispositionToOpenFlags(dwCreationDisposition);
    mode_t mode = mapFileAttributesToMode(dwFlagsAndAttributes);

    // 通过 LinuxSyscallHandler 调用 (有 FsAdapter 时走容器文件系统)
    int fd = containeros::LinuxSyscallHandler::open(lpFileName, open_flags, mode);
    if (fd < 0) {
        SetLastError(mapErrnoToWin32Error(errno));
        Logger::getInstance().error("CreateFileA 失败, 文件: " + std::string(lpFileName) +
                                    ", 错误: " + std::string(strerror(errno)));
        return INVALID_HANDLE_VALUE;
    }

    // 生成 HANDLE 并建立映射
    std::lock_guard<std::mutex> lock(mutex_);
    HANDLE handle = generateHandle();
    handle_to_fd_[handle] = fd;

    SetLastError(ERROR_SUCCESS);
    Logger::getInstance().debug("CreateFileA 成功, 文件: " + std::string(lpFileName) +
                                ", fd: " + std::to_string(fd));
    return handle;
}

BOOL Win32ApiEmulator::CloseHandle(HANDLE hObject) {
    if (hObject == INVALID_HANDLE_VALUE || hObject == nullptr) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto it = handle_to_fd_.find(hObject);
    if (it == handle_to_fd_.end()) {
        SetLastError(ERROR_INVALID_HANDLE_VALUE);
        return FALSE;
    }

    int fd = it->second;
    handle_to_fd_.erase(it);

    // 通过 LinuxSyscallHandler 调用 (有 FsAdapter 时走容器文件系统)
    if (containeros::LinuxSyscallHandler::close(fd) < 0) {
        SetLastError(mapErrnoToWin32Error(errno));
        Logger::getInstance().error("CloseHandle 失败, fd: " + std::to_string(fd) +
                                    ", 错误: " + std::string(strerror(errno)));
        return FALSE;
    }

    SetLastError(ERROR_SUCCESS);
    Logger::getInstance().debug("CloseHandle 成功, fd: " + std::to_string(fd));
    return TRUE;
}

BOOL Win32ApiEmulator::ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
                                LPDWORD lpNumberOfBytesRead, LPVOID lpOverlapped) {
    if (hFile == INVALID_HANDLE_VALUE || hFile == nullptr || !lpBuffer) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto it = handle_to_fd_.find(hFile);
    if (it == handle_to_fd_.end()) {
        SetLastError(ERROR_INVALID_HANDLE_VALUE);
        return FALSE;
    }

    int fd = it->second;

    // 通过 LinuxSyscallHandler 调用 (有 FsAdapter 时走容器文件系统)
    ssize_t bytes_read = containeros::LinuxSyscallHandler::read(fd, lpBuffer, nNumberOfBytesToRead);
    if (bytes_read < 0) {
        SetLastError(mapErrnoToWin32Error(errno));
        Logger::getInstance().error("ReadFile 失败, fd: " + std::to_string(fd) +
                                    ", 错误: " + std::string(strerror(errno)));
        return FALSE;
    }

    if (lpNumberOfBytesRead) {
        *lpNumberOfBytesRead = static_cast<DWORD>(bytes_read);
    }

    SetLastError(ERROR_SUCCESS);
    Logger::getInstance().debug("ReadFile 成功, fd: " + std::to_string(fd) +
                                ", 读取字节数: " + std::to_string(bytes_read));
    return TRUE;
}

BOOL Win32ApiEmulator::WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
                                 LPDWORD lpNumberOfBytesWritten, LPVOID lpOverlapped) {
    if (hFile == INVALID_HANDLE_VALUE || hFile == nullptr || !lpBuffer) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto it = handle_to_fd_.find(hFile);
    if (it == handle_to_fd_.end()) {
        SetLastError(ERROR_INVALID_HANDLE_VALUE);
        return FALSE;
    }

    int fd = it->second;

    // 通过 LinuxSyscallHandler 调用 (有 FsAdapter 时走容器文件系统)
    ssize_t bytes_written = containeros::LinuxSyscallHandler::write(fd, lpBuffer, nNumberOfBytesToWrite);
    if (bytes_written < 0) {
        SetLastError(mapErrnoToWin32Error(errno));
        Logger::getInstance().error("WriteFile 失败, fd: " + std::to_string(fd) +
                                    ", 错误: " + std::string(strerror(errno)));
        return FALSE;
    }

    if (lpNumberOfBytesWritten) {
        *lpNumberOfBytesWritten = static_cast<DWORD>(bytes_written);
    }

    SetLastError(ERROR_SUCCESS);
    Logger::getInstance().debug("WriteFile 成功, fd: " + std::to_string(fd) +
                                ", 写入字节数: " + std::to_string(bytes_written));
    return TRUE;
}

// ===== 错误码管理 =====

DWORD Win32ApiEmulator::GetLastError() {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_error_;
}

BOOL Win32ApiEmulator::SetLastError(DWORD dwErrCode) {
    std::lock_guard<std::mutex> lock(mutex_);
    last_error_ = dwErrCode;
    return TRUE;
}

// ===== 进程管理函数实现 =====

BOOL Win32ApiEmulator::CreateProcessA(LPCSTR lpApplicationName, LPSTR lpCommandLine,
                                      LPVOID lpProcessAttributes, LPVOID lpThreadAttributes,
                                      BOOL bInheritHandles, DWORD dwCreationFlags,
                                      LPVOID lpEnvironment, LPCSTR lpCurrentDirectory,
                                      LPSTARTUPINFOA lpStartupInfo,
                                      LPPROCESS_INFORMATION lpProcessInformation) {
    // 参数校验
    if (!lpApplicationName && !lpCommandLine) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (!lpProcessInformation) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    // 默认使用应用程序名称作为命令
    const char* cmd = lpApplicationName ? lpApplicationName : lpCommandLine;

    // TODO: 自定义 OS 上 fork/exec 可能不可用，需实现进程管理适配层
    // 当前使用标准 Linux fork 系统调用
    pid_t pid = ::fork();
    if (pid < 0) {
        SetLastError(mapErrnoToWin32Error(errno));
        Logger::getInstance().error("CreateProcessA fork 失败, 命令: " + std::string(cmd) +
                                    ", 错误: " + std::string(strerror(errno)));
        return FALSE;
    }

    if (pid == 0) {
        // 子进程: 设置工作目录
        if (lpCurrentDirectory && *lpCurrentDirectory != '\0') {
            ::chdir(lpCurrentDirectory);
        }

        // 执行命令
        // TODO: 实际应解析命令行参数，支持带参数的命令
        char* argv[] = {const_cast<char*>(cmd), nullptr};
        ::execve(cmd, argv, nullptr);

        // execve 失败，退出子进程
        ::_exit(127);
    }

    // 父进程: 填充进程信息
    std::lock_guard<std::mutex> lock(mutex_);
    HANDLE process_handle = generateHandle();
    HANDLE thread_handle = generateHandle();

    // 将进程 PID 存储在句柄映射中（使用负数表示进程句柄）
    handle_to_fd_[process_handle] = -pid;
    handle_to_fd_[thread_handle] = -(pid + 1);

    lpProcessInformation->hProcess = process_handle;
    lpProcessInformation->hThread = thread_handle;
    lpProcessInformation->dwProcessId = static_cast<DWORD>(pid);
    lpProcessInformation->dwThreadId = static_cast<DWORD>(pid);

    SetLastError(ERROR_SUCCESS);
    Logger::getInstance().info("CreateProcessA 成功, 命令: " + std::string(cmd) +
                               ", PID: " + std::to_string(pid));
    return TRUE;
}

BOOL Win32ApiEmulator::TerminateProcess(HANDLE hProcess, UINT uExitCode) {
    if (hProcess == INVALID_HANDLE_VALUE || hProcess == nullptr) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto it = handle_to_fd_.find(hProcess);
    if (it == handle_to_fd_.end()) {
        SetLastError(ERROR_INVALID_HANDLE_VALUE);
        return FALSE;
    }

    int pid = -it->second; // 恢复 PID（存储时使用负数）

    // TODO: 自定义 OS 上 kill 系统调用可能不可用，需实现进程管理适配层
    if (::kill(pid, SIGKILL) < 0) {
        SetLastError(mapErrnoToWin32Error(errno));
        Logger::getInstance().error("TerminateProcess 失败, PID: " + std::to_string(pid) +
                                    ", 错误: " + std::string(strerror(errno)));
        return FALSE;
    }

    // 等待进程退出
    int status = 0;
    ::waitpid(pid, &status, 0);

    // 清理句柄映射
    handle_to_fd_.erase(it);

    SetLastError(ERROR_SUCCESS);
    Logger::getInstance().info("TerminateProcess 成功, PID: " + std::to_string(pid) +
                               ", 退出码: " + std::to_string(uExitCode));
    return TRUE;
}

DWORD Win32ApiEmulator::WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds) {
    if (hHandle == INVALID_HANDLE_VALUE || hHandle == nullptr) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0xFFFFFFFF; // WAIT_FAILED
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto it = handle_to_fd_.find(hHandle);
    if (it == handle_to_fd_.end()) {
        SetLastError(ERROR_INVALID_HANDLE_VALUE);
        return 0xFFFFFFFF; // WAIT_FAILED
    }

    int pid = -it->second; // 恢复 PID

    // TODO: 自定义 OS 上 waitpid 可能不可用，需实现进程管理适配层
    int options = 0;
    if (dwMilliseconds != INFINITE) {
        options |= WNOHANG;
    }

    int status = 0;
    pid_t result = ::waitpid(pid, &status, options);

    if (result < 0) {
        SetLastError(mapErrnoToWin32Error(errno));
        return 0xFFFFFFFF; // WAIT_FAILED
    }

    if (result == 0) {
        // 进程仍在运行
        return 0x00000102; // WAIT_TIMEOUT
    }

    // 进程已退出
    SetLastError(ERROR_SUCCESS);
    return 0x00000000; // WAIT_OBJECT_0
}

void Win32ApiEmulator::ExitProcess(UINT uExitCode) {
    Logger::getInstance().info("ExitProcess, 退出码: " + std::to_string(uExitCode));
    // TODO: 自定义 OS 上 _exit 可能不可用，需实现进程退出适配层
    ::_exit(uExitCode);
}

// ===== 内存管理函数实现 =====

LPVOID Win32ApiEmulator::VirtualAlloc(LPVOID lpAddress, SIZE_T dwSize,
                                      DWORD flAllocationType, DWORD flProtect) {
    if (dwSize == 0) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return nullptr;
    }

    int prot = mapProtectToProtFlags(flProtect);
    int flags = mapAllocationTypeToMapFlags(flAllocationType);

    // TODO: 自定义 OS 上 mmap 系统调用可能不可用，需实现内存管理适配层
    void* addr = ::mmap(lpAddress, dwSize, prot, flags, -1, 0);
    if (addr == MAP_FAILED) {
        SetLastError(mapErrnoToWin32Error(errno));
        Logger::getInstance().error("VirtualAlloc 失败, 大小: " + std::to_string(dwSize) +
                                    ", 错误: " + std::string(strerror(errno)));
        return nullptr;
    }

    SetLastError(ERROR_SUCCESS);
    Logger::getInstance().debug("VirtualAlloc 成功, 地址: " +
                                std::to_string(reinterpret_cast<uintptr_t>(addr)) +
                                ", 大小: " + std::to_string(dwSize));
    return addr;
}

BOOL Win32ApiEmulator::VirtualFree(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType) {
    if (!lpAddress) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    // TODO: 自定义 OS 上 munmap 系统调用可能不可用，需实现内存管理适配层
    // Linux 中 munmap 的 length 参数可以忽略，直接释放整个映射区域
    if (::munmap(lpAddress, dwSize == 0 ? getpagesize() : dwSize) < 0) {
        SetLastError(mapErrnoToWin32Error(errno));
        Logger::getInstance().error("VirtualFree 失败, 地址: " +
                                    std::to_string(reinterpret_cast<uintptr_t>(lpAddress)) +
                                    ", 错误: " + std::string(strerror(errno)));
        return FALSE;
    }

    SetLastError(ERROR_SUCCESS);
    Logger::getInstance().debug("VirtualFree 成功, 地址: " +
                                std::to_string(reinterpret_cast<uintptr_t>(lpAddress)));
    return TRUE;
}

LPVOID Win32ApiEmulator::HeapAlloc(HANDLE hHeap, DWORD dwFlags, SIZE_T dwBytes) {
    if (dwBytes == 0) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return nullptr;
    }

    // TODO: 自定义 OS 上 malloc 可能不可用，需实现堆内存管理适配层
    void* ptr = ::malloc(dwBytes);
    if (!ptr) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        Logger::getInstance().error("HeapAlloc 失败, 大小: " + std::to_string(dwBytes) +
                                    ", 错误: 内存不足");
        return nullptr;
    }

    // 如果设置了 HEAP_ZERO_MEMORY 标志，清零内存
    if (dwFlags & 0x00000008) { // HEAP_ZERO_MEMORY
        ::memset(ptr, 0, dwBytes);
    }

    SetLastError(ERROR_SUCCESS);
    Logger::getInstance().debug("HeapAlloc 成功, 地址: " +
                                std::to_string(reinterpret_cast<uintptr_t>(ptr)) +
                                ", 大小: " + std::to_string(dwBytes));
    return ptr;
}

BOOL Win32ApiEmulator::HeapFree(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem) {
    if (!lpMem) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    // TODO: 自定义 OS 上 free 可能不可用，需实现堆内存管理适配层
    ::free(lpMem);

    SetLastError(ERROR_SUCCESS);
    Logger::getInstance().debug("HeapFree 成功, 地址: " +
                                std::to_string(reinterpret_cast<uintptr_t>(lpMem)));
    return TRUE;
}

// ===== DLL 加载函数实现 =====

HMODULE Win32ApiEmulator::LoadLibraryA(LPCSTR lpLibFileName) {
    if (!lpLibFileName || *lpLibFileName == '\0') {
        SetLastError(ERROR_INVALID_PARAMETER);
        return nullptr;
    }

    std::string lib_path(lpLibFileName);

    // TODO: 自定义 OS 上 dlopen 可能不可用，需实现动态链接器适配层
    // 尝试加载 DLL（.dll 或 .so 文件）
    void* handle = ::dlopen(lpLibFileName, RTLD_LAZY | RTLD_GLOBAL);

    // 如果直接加载失败，尝试添加 .so 后缀
    if (!handle) {
        std::string so_path = lib_path;
        if (so_path.find(".so") == std::string::npos &&
            so_path.find(".dll") == std::string::npos) {
            so_path += ".so";
            handle = ::dlopen(so_path.c_str(), RTLD_LAZY | RTLD_GLOBAL);
        }
    }

    if (!handle) {
        SetLastError(ERROR_FILE_NOT_FOUND);
        Logger::getInstance().error("LoadLibraryA 失败, 文件: " + lib_path +
                                    ", 错误: " + std::string(dlerror()));
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    HMODULE module = reinterpret_cast<HMODULE>(next_module_++);
    module_to_dll_[module] = {lib_path, handle};

    SetLastError(ERROR_SUCCESS);
    Logger::getInstance().info("LoadLibraryA 成功, 文件: " + lib_path);
    return module;
}

FARPROC Win32ApiEmulator::GetProcAddress(HMODULE hModule, LPCSTR lpProcName) {
    if (!hModule || !lpProcName || *lpProcName == '\0') {
        SetLastError(ERROR_INVALID_PARAMETER);
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto it = module_to_dll_.find(hModule);
    if (it == module_to_dll_.end()) {
        SetLastError(ERROR_INVALID_HANDLE_VALUE);
        return nullptr;
    }

    // TODO: 自定义 OS 上 dlsym 可能不可用，需实现符号解析适配层
    void* proc = ::dlsym(it->second.handle, lpProcName);
    if (!proc) {
        SetLastError(127); // ERROR_PROC_NOT_FOUND
        Logger::getInstance().error("GetProcAddress 失败, 模块: " + it->second.path +
                                    ", 函数: " + std::string(lpProcName) +
                                    ", 错误: " + std::string(dlerror()));
        return nullptr;
    }

    SetLastError(ERROR_SUCCESS);
    Logger::getInstance().debug("GetProcAddress 成功, 函数: " + std::string(lpProcName));
    return reinterpret_cast<FARPROC>(proc);
}

BOOL Win32ApiEmulator::FreeLibrary(HMODULE hLibModule) {
    if (!hLibModule) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto it = module_to_dll_.find(hLibModule);
    if (it == module_to_dll_.end()) {
        SetLastError(ERROR_INVALID_HANDLE_VALUE);
        return FALSE;
    }

    // TODO: 自定义 OS 上 dlclose 可能不可用，需实现动态链接器适配层
    if (::dlclose(it->second.handle) < 0) {
        SetLastError(mapErrnoToWin32Error(errno));
        Logger::getInstance().error("FreeLibrary 失败, 模块: " + it->second.path +
                                    ", 错误: " + std::string(dlerror()));
        return FALSE;
    }

    module_to_dll_.erase(it);

    SetLastError(ERROR_SUCCESS);
    Logger::getInstance().info("FreeLibrary 成功");
    return TRUE;
}

// ===== 私有辅助函数实现 =====

HANDLE Win32ApiEmulator::generateHandle() {
    return reinterpret_cast<HANDLE>(next_handle_++);
}

int Win32ApiEmulator::convertAccessMode(DWORD dwDesiredAccess) const {
    return mapDesiredAccessToOpenFlags(dwDesiredAccess);
}

int Win32ApiEmulator::convertCreationDisposition(DWORD dwCreationDisposition) const {
    return mapCreationDispositionToOpenFlags(dwCreationDisposition);
}

int Win32ApiEmulator::convertFileAttributes(DWORD dwFlagsAndAttributes) const {
    return static_cast<int>(mapFileAttributesToMode(dwFlagsAndAttributes));
}
