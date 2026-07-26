#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <mutex>
#include <memory>

// Windows 基础类型定义
// 这些类型在 Win32 API 中广泛使用，用于模拟 Windows 环境
using HANDLE = void*;
using DWORD = uint32_t;
using BOOL = int;
using LPVOID = void*;
using LPCSTR = const char*;
using LPSTR = char*;
using SIZE_T = size_t;
using UINT = uint32_t;
using FARPROC = void(*)();
using HMODULE = void*;

// Windows 常量定义
// 用于 Win32 API 返回值和参数
constexpr HANDLE INVALID_HANDLE_VALUE = reinterpret_cast<HANDLE>(-1);
constexpr DWORD ERROR_SUCCESS = 0;
constexpr DWORD ERROR_FILE_NOT_FOUND = 2;
constexpr DWORD ERROR_ACCESS_DENIED = 5;
constexpr DWORD GENERIC_READ = 0x80000000;
constexpr DWORD GENERIC_WRITE = 0x40000000;
constexpr DWORD FILE_SHARE_READ = 0x00000001;
constexpr DWORD FILE_SHARE_WRITE = 0x00000002;
constexpr DWORD OPEN_EXISTING = 3;
constexpr DWORD CREATE_NEW = 1;
constexpr DWORD CREATE_ALWAYS = 2;
constexpr DWORD FILE_ATTRIBUTE_NORMAL = 0x00000080;

// Windows 进程信息结构
struct PROCESS_INFORMATION {
    HANDLE hProcess;
    HANDLE hThread;
    DWORD dwProcessId;
    DWORD dwThreadId;
};

// Windows 启动信息结构
struct STARTUPINFOA {
    DWORD cb;
    LPSTR lpReserved;
    LPSTR lpDesktop;
    LPSTR lpTitle;
    DWORD dwX;
    DWORD dwY;
    DWORD dwXSize;
    DWORD dwYSize;
    DWORD dwXCountChars;
    DWORD dwYCountChars;
    DWORD dwFillAttribute;
    DWORD dwFlags;
    WORD wShowWindow;
    WORD cbReserved2;
    LPBYTE lpReserved2;
    HANDLE hStdInput;
    HANDLE hStdOutput;
    HANDLE hStdError;
};

// Win32 API 模拟器
// 在自定义操作系统上模拟 Windows Win32 API，通过调用 Linux syscall 兼容层实现功能
// 提供文件操作、进程管理、内存管理、DLL 加载等核心功能的模拟
class Win32ApiEmulator {
public:
    Win32ApiEmulator();
    ~Win32ApiEmulator();

    // ===== kernel32.dll 核心文件操作函数 =====

    // 创建或打开文件
    // 参数:
    //   lpFileName: 文件路径
    //   dwDesiredAccess: 访问权限 (GENERIC_READ/GENERIC_WRITE)
    //   dwShareMode: 共享模式 (FILE_SHARE_READ/FILE_SHARE_WRITE)
    //   lpSecurityAttributes: 安全属性 (未使用)
    //   dwCreationDisposition: 创建方式 (OPEN_EXISTING/CREATE_NEW/CREATE_ALWAYS)
    //   dwFlagsAndAttributes: 文件标志和属性
    //   hTemplateFile: 模板文件句柄 (未使用)
    // 返回: 文件句柄，失败返回 INVALID_HANDLE_VALUE
    HANDLE CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                       LPVOID lpSecurityAttributes, DWORD dwCreationDisposition,
                       DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);

    // 关闭句柄
    // 参数: hObject: 要关闭的句柄
    // 返回: 成功返回 TRUE，失败返回 FALSE
    BOOL CloseHandle(HANDLE hObject);

    // 读取文件
    // 参数:
    //   hFile: 文件句柄
    //   lpBuffer: 接收数据的缓冲区
    //   nNumberOfBytesToRead: 请求读取的字节数
    //   lpNumberOfBytesRead: 实际读取的字节数
    //   lpOverlapped: 重叠操作结构 (未使用)
    // 返回: 成功返回 TRUE，失败返回 FALSE
    BOOL ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
                  LPDWORD lpNumberOfBytesRead, LPVOID lpOverlapped);

    // 写入文件
    // 参数:
    //   hFile: 文件句柄
    //   lpBuffer: 要写入的数据缓冲区
    //   nNumberOfBytesToWrite: 请求写入的字节数
    //   lpNumberOfBytesWritten: 实际写入的字节数
    //   lpOverlapped: 重叠操作结构 (未使用)
    // 返回: 成功返回 TRUE，失败返回 FALSE
    BOOL WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
                   LPDWORD lpNumberOfBytesWritten, LPVOID lpOverlapped);

    // 获取最后错误码
    // 返回: 最近一次 API 调用的错误码
    DWORD GetLastError();

    // 设置最后错误码
    // 参数: dwErrCode: 要设置的错误码
    // 返回: 成功返回 TRUE
    BOOL SetLastError(DWORD dwErrCode);

    // ===== 进程管理函数 =====

    // 创建进程
    // 参数:
    //   lpApplicationName: 应用程序名称
    //   lpCommandLine: 命令行参数
    //   lpProcessAttributes: 进程安全属性 (未使用)
    //   lpThreadAttributes: 线程安全属性 (未使用)
    //   bInheritHandles: 是否继承句柄
    //   dwCreationFlags: 创建标志
    //   lpEnvironment: 环境变量 (未使用)
    //   lpCurrentDirectory: 当前工作目录
    //   lpStartupInfo: 启动信息
    //   lpProcessInformation: 进程信息输出
    // 返回: 成功返回 TRUE，失败返回 FALSE
    BOOL CreateProcessA(LPCSTR lpApplicationName, LPSTR lpCommandLine,
                        LPVOID lpProcessAttributes, LPVOID lpThreadAttributes,
                        BOOL bInheritHandles, DWORD dwCreationFlags,
                        LPVOID lpEnvironment, LPCSTR lpCurrentDirectory,
                        LPSTARTUPINFOA lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation);

    // 终止进程
    // 参数:
    //   hProcess: 进程句柄
    //   uExitCode: 退出码
    // 返回: 成功返回 TRUE，失败返回 FALSE
    BOOL TerminateProcess(HANDLE hProcess, UINT uExitCode);

    // 等待对象
    // 参数:
    //   hHandle: 对象句柄
    //   dwMilliseconds: 等待时间（毫秒），INFINITE 表示无限等待
    // 返回: 等待结果
    DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds);

    // 退出进程
    // 参数: uExitCode: 进程退出码
    void ExitProcess(UINT uExitCode);

    // ===== 内存管理函数 =====

    // 虚拟内存分配
    // 参数:
    //   lpAddress: 请求的内存地址（NULL 表示自动分配）
    //   dwSize: 分配大小
    //   flAllocationType: 分配类型
    //   flProtect: 保护属性
    // 返回: 分配的内存地址，失败返回 NULL
    LPVOID VirtualAlloc(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect);

    // 虚拟内存释放
    // 参数:
    //   lpAddress: 要释放的内存地址
    //   dwSize: 释放大小（0 表示释放整个区域）
    //   dwFreeType: 释放类型
    // 返回: 成功返回 TRUE，失败返回 FALSE
    BOOL VirtualFree(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType);

    // 堆内存分配
    // 参数:
    //   hHeap: 堆句柄（NULL 使用进程默认堆）
    //   dwFlags: 分配标志
    //   dwBytes: 分配大小
    // 返回: 分配的内存地址，失败返回 NULL
    LPVOID HeapAlloc(HANDLE hHeap, DWORD dwFlags, SIZE_T dwBytes);

    // 堆内存释放
    // 参数:
    //   hHeap: 堆句柄
    //   dwFlags: 释放标志
    //   lpMem: 要释放的内存地址
    // 返回: 成功返回 TRUE，失败返回 FALSE
    BOOL HeapFree(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem);

    // ===== DLL 加载函数 =====

    // 加载 DLL 模块
    // 参数: lpLibFileName: DLL 文件路径
    // 返回: 模块句柄，失败返回 NULL
    HMODULE LoadLibraryA(LPCSTR lpLibFileName);

    // 获取 DLL 导出函数地址
    // 参数:
    //   hModule: 模块句柄
    //   lpProcName: 函数名称
    // 返回: 函数地址，失败返回 NULL
    FARPROC GetProcAddress(HMODULE hModule, LPCSTR lpProcName);

    // 释放 DLL 模块
    // 参数: hLibModule: 模块句柄
    // 返回: 成功返回 TRUE，失败返回 FALSE
    BOOL FreeLibrary(HMODULE hLibModule);

private:
    // 将 Win32 访问权限转换为 Linux open 标志
    int convertAccessMode(DWORD dwDesiredAccess) const;

    // 将 Win32 创建方式转换为 Linux open 标志
    int convertCreationDisposition(DWORD dwCreationDisposition) const;

    // 将 Win32 文件属性转换为 Linux open 标志
    int convertFileAttributes(DWORD dwFlagsAndAttributes) const;

    // 生成唯一的 HANDLE 值
    HANDLE generateHandle();

    // HANDLE 到 Linux 文件描述符的映射表
    std::unordered_map<HANDLE, int> handle_to_fd_;

    // HMODULE 到 DLL 信息的映射表
    struct DllInfo {
        std::string path;
        void* handle;
    };
    std::unordered_map<HMODULE, DllInfo> module_to_dll_;

    // 下一个可用的 HANDLE 值
    uint64_t next_handle_;

    // 下一个可用的 HMODULE 值
    uint64_t next_module_;

    // 最后错误码
    DWORD last_error_;

    // 线程互斥锁，保护映射表和错误状态
    mutable std::mutex mutex_;
};
