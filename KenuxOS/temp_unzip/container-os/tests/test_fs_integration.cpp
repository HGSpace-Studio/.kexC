// 集成测试: 验证容器文件系统 + LinuxSyscallHandler + Win32ApiEmulator 联动
// 测试流程:
//   1. 创建 FsAdapter (内存磁盘), 格式化并挂载
//   2. 绑定到 LinuxSyscallHandler
//   3. 通过 LinuxSyscallHandler 操作文件 (验证转发到容器文件系统)
//   4. 通过 Win32ApiEmulator 操作文件 (验证转译层 -> LinuxSyscallHandler -> 容器文件系统)

#include "storage/fs/FsAdapter.h"
#include "syscall/LinuxSyscallHandler.h"
#include "syscall/Win32ApiEmulator.h"
#include "utils/Logger.h"

#include <cstring>
#include <iostream>
#include <cassert>

using namespace containeros;

// 简单测试框架
static int test_count = 0;
static int test_passed = 0;

#define TEST(name) test_count++; std::cout << "[TEST] " << name << " ... ";
#define PASS() test_passed++; std::cout << "PASS" << std::endl;
#define FAIL(msg) std::cout << "FAIL: " << msg << std::endl;

int main() {
    Logger::getInstance().setLevel(LogLevel::LOG_WARNING);

    // ============================================================
    // 阶段 1: 创建并初始化容器文件系统
    // ============================================================
    std::cout << "\n=== 阶段 1: 初始化容器文件系统 ===" << std::endl;

    FsAdapter fs(4096);  // 4096 块 = 16MB 内存磁盘
    if (!fs.format("TestFS")) {
        std::cerr << "格式化失败!" << std::endl;
        return 1;
    }
    std::cout << "文件系统格式化成功" << std::endl;

    if (!fs.mount("memdisk", "root")) {
        std::cerr << "挂载失败!" << std::endl;
        return 1;
    }
    std::cout << "文件系统挂载成功" << std::endl;

    // 绑定到 LinuxSyscallHandler
    LinuxSyscallHandler::bindFilesystem(&fs);
    assert(LinuxSyscallHandler::hasFilesystem());
    std::cout << "已绑定到 LinuxSyscallHandler" << std::endl;

    // ============================================================
    // 阶段 2: 通过 LinuxSyscallHandler 操作文件
    // ============================================================
    std::cout << "\n=== 阶段 2: Linux syscall 转发测试 ===" << std::endl;

    // 创建目录
    TEST("mkdir /home");
    if (LinuxSyscallHandler::mkdir("/home", 0755) == 0) {
        PASS();
    } else {
        FAIL("mkdir 返回非零");
    }

    TEST("mkdir /home/user");
    if (LinuxSyscallHandler::mkdir("/home/user", 0755) == 0) {
        PASS();
    } else {
        FAIL("mkdir 返回非零");
    }

    // 创建并写入文件
    TEST("open + write 文件");
    int fd = LinuxSyscallHandler::open("/home/user/test.txt", O_RDWR | O_CREAT, 0644);
    if (fd >= 0) {
        const char* content = "Hello from container filesystem!";
        ssize_t written = LinuxSyscallHandler::write(fd, content, strlen(content));
        if (written == (ssize_t)strlen(content)) {
            PASS();
        } else {
            FAIL("写入字节数不匹配");
        }
        LinuxSyscallHandler::close(fd);
    } else {
        FAIL("open 失败");
    }

    // 读取文件
    TEST("open + read 文件");
    fd = LinuxSyscallHandler::open("/home/user/test.txt", O_RDONLY, 0);
    if (fd >= 0) {
        char buf[256] = {0};
        ssize_t bytes = LinuxSyscallHandler::read(fd, buf, sizeof(buf) - 1);
        if (bytes > 0 && strcmp(buf, "Hello from container filesystem!") == 0) {
            PASS();
        } else {
            FAIL("读取内容不匹配");
        }
        LinuxSyscallHandler::close(fd);
    } else {
        FAIL("open 失败");
    }

    // stat 测试
    TEST("stat 文件");
    struct stat st;
    if (LinuxSyscallHandler::stat("/home/user/test.txt", &st) == 0) {
        if (st.st_size == strlen("Hello from container filesystem!")) {
            PASS();
        } else {
            FAIL("文件大小不匹配");
        }
    } else {
        FAIL("stat 失败");
    }

    // ============================================================
    // 阶段 3: 通过 Win32ApiEmulator 操作文件
    // ============================================================
    std::cout << "\n=== 阶段 3: Win32 API 转译层测试 ===" << std::endl;

    auto& win32 = Win32ApiEmulator::getInstance();

    // CreateFileA
    TEST("Win32 CreateFileA");
    HANDLE hFile = win32.CreateFileA(
        "/home/user/win32_test.txt",
        GENERIC_WRITE,
        0, nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        PASS();
    } else {
        FAIL("CreateFileA 失败");
    }

    // WriteFile
    TEST("Win32 WriteFile");
    const char* win_content = "Written via Win32 API!";
    DWORD bytes_written = 0;
    BOOL write_ok = win32.WriteFile(hFile, win_content, strlen(win_content),
                                     &bytes_written, nullptr);
    if (write_ok && bytes_written == strlen(win_content)) {
        PASS();
    } else {
        FAIL("WriteFile 失败");
    }

    // CloseHandle
    TEST("Win32 CloseHandle");
    if (win32.CloseHandle(hFile)) {
        PASS();
    } else {
        FAIL("CloseHandle 失败");
    }

    // 重新打开读取
    TEST("Win32 ReadFile");
    hFile = win32.CreateFileA(
        "/home/user/win32_test.txt",
        GENERIC_READ,
        0, nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        char buf[256] = {0};
        DWORD bytes_read = 0;
        BOOL read_ok = win32.ReadFile(hFile, buf, sizeof(buf) - 1, &bytes_read, nullptr);
        if (read_ok && bytes_read == strlen(win_content) &&
            strcmp(buf, win_content) == 0) {
            PASS();
        } else {
            FAIL("读取内容不匹配");
        }
        win32.CloseHandle(hFile);
    } else {
        FAIL("CreateFileA 读取失败");
    }

    // ============================================================
    // 阶段 4: 交叉验证 (Linux 写, Win32 读)
    // ============================================================
    std::cout << "\n=== 阶段 4: 交叉验证 ===" << std::endl;

    // Linux 写入
    TEST("Linux 写 -> Win32 读");
    fd = LinuxSyscallHandler::open("/home/user/cross.txt", O_RDWR | O_CREAT, 0644);
    if (fd >= 0) {
        const char* cross_content = "Cross-platform content";
        LinuxSyscallHandler::write(fd, cross_content, strlen(cross_content));
        LinuxSyscallHandler::close(fd);

        // Win32 读取
        hFile = win32.CreateFileA("/home/user/cross.txt", GENERIC_READ, 0, nullptr,
                                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile != INVALID_HANDLE_VALUE) {
            char buf[256] = {0};
            DWORD bytes_read = 0;
            win32.ReadFile(hFile, buf, sizeof(buf) - 1, &bytes_read, nullptr);
            if (strcmp(buf, cross_content) == 0) {
                PASS();
            } else {
                FAIL("交叉读取内容不匹配");
            }
            win32.CloseHandle(hFile);
        } else {
            FAIL("Win32 打开失败");
        }
    } else {
        FAIL("Linux open 失败");
    }

    // ============================================================
    // 汇总
    // ============================================================
    std::cout << "\n=== 测试汇总 ===" << std::endl;
    std::cout << "通过: " << test_passed << "/" << test_count << std::endl;

    // 清理
    LinuxSyscallHandler::bindFilesystem(nullptr);

    return (test_passed == test_count) ? 0 : 1;
}
