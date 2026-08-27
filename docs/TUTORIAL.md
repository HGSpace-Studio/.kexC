# 从零开发 Kenux 程序教程

本教程从 Linux 环境安装开始，逐步带你开发出完整的 Kenux 应用程序。学完本教程，你将能够独立开发 N 套 Kenux 程序。

---

## 目录

1. [环境准备与安装](#1-环境准备与安装)
2. [Kenux 程序模型](#2-kenux-程序模型)
3. [第一个程序：Hello Kenux](#3-第一个程序hello-kenux)
4. [编译与运行](#4-编译与运行)
5. [系统调用 API](#5-系统调用-api)
6. [文件 I/O 程序](#6-文件-io-程序)
7. [数学与算法程序](#7-数学与算法程序)
8. [进程管理程序](#8-进程管理程序)
9. [网络通信程序](#9-网络通信程序)
10. [动态库开发（.kxp）](#10-动态库开发kxp)
11. [图形界面程序](#11-图形界面程序)
12. [多文件项目与构建系统](#12-多文件项目与构建系统)
13. [编译为 ELF 原生可执行](#13-编译为-elf-原生可执行)
14. [系统信息工具实战](#14-系统信息工具实战)
15. [完整项目：文件浏览器](#15-完整项目文件浏览器)
16. [调试与排错](#16-调试与排错)
17. [最佳实践](#17-最佳实践)

---

## 1. 环境准备与安装

### 1.1 系统要求

| 项目 | 要求 |
|------|------|
| 操作系统 | Linux (x86-64)，推荐 Ubuntu 22.04+ / Fedora 38+ / Arch |
| 编译器 | GCC 12+ 或 Clang 15+ |
| 构建工具 | GNU Make 4.0+ |
| 图形支持 | X11 开发库 (libx11-dev，仅图形程序需要) |

### 1.2 安装依赖

```bash
# Ubuntu / Debian
sudo apt update
sudo apt install build-essential gcc g++ make

# 图形程序额外需要
sudo apt install libx11-dev

# Fedora / RHEL
sudo dnf install gcc gcc-c++ make
sudo dnf install libX11-devel

# Arch Linux
sudo pacman -S gcc make
sudo pacman -S libx11
```

### 1.3 获取 Kex 编译器

从源码编译安装 Kex：

```bash
# 克隆仓库
git clone <kexC-repo-url>
cd kexC

# 编译
make

# 安装到系统 PATH
sudo make install
# 或者使用 kex 自带的安装命令
./bin/kex install
```

### 1.4 验证安装

```bash
kex
# 应输出:
# kex - Kenux 编译器 v1.0.0
# 用法:
#   kex [选项] <源文件...>
#   kex build [选项]
#   kex run <kex文件> [选项]
#   ...
```

### 1.5 卸载

```bash
sudo kex uninstall
```

---

## 2. Kenux 程序模型

### 2.1 核心概念

Kenux 程序与普通 C 程序的关键区别：

| 特性 | 普通 C 程序 | Kenux 程序 |
|------|------------|------------|
| 入口函数 | `main()` | `_start()` |
| 标准库 | libc | kex_user.h (内联 syscall) |
| 输出格式 | ELF | .kex (HGS 格式) |
| 系统调用 | libc 包装 | 直接 x86-64 syscall |
| 动态库 | .so | .kxp |
| 退出方式 | `return` | `sys_exit()` |

### 2.2 syscall 调用约定

Kenux 遵循 Linux x86-64 syscall 约定：

- **syscall 号**：放入 `rax` 寄存器
- **参数**：依次放入 `rdi`, `rsi`, `rdx`, `r10`, `r8`, `r9`
- **返回值**：存入 `rax`
- **clobber**：`rcx` 和 `r11` 被破坏

### 2.3 KEX 文件格式

.kex 文件内部结构：

```
┌──────────────────────┐
│  头部 (80 字节)       │  魔数 F0 A5 48 47, 版本, 段偏移...
├──────────────────────┤
│  程序名 (16 字节)     │
├──────────────────────┤
│  "Kenux---" (8字节)  │  代码区开始标记
├──────────────────────┤
│  x86-64 机器码       │  你的程序代码
├──────────────────────┤
│  "KKKSTOP" (7字节)   │  代码区结束标记
├──────────────────────┤
│  .rodata 段          │  字符串常量、只读数据
├──────────────────────┤
│  导入表              │  程序依赖的 API 函数
├──────────────────────┤
│  导出表              │  导出给其他程序的函数
├──────────────────────┤
│  重定位表            │  动态链接地址修补
├──────────────────────┤
│  本地重定位表        │  .rodata 引用修补
└──────────────────────┘
```

### 2.4 头文件体系

```c
#include "kex_user.h"       // 核心：syscall 包装、辅助函数
#include <kapi_gfxrender.h> // 图形渲染 API
#include <kapi_window.h>    // 窗口管理 API
#include <kapi.h>           // Kapi 统一头文件
```

---

## 3. 第一个程序：Hello Kenux

创建文件 `hello.c`：

```c
#include "kex_user.h"

void _start(void) {
    const char *msg = "Hello, KenuxOS!\n";
    sys_write(1, msg, 16);
    sys_exit(0);
}
```

**逐行解释**：

1. `#include "kex_user.h"` — 引入 Kenux 用户 API，提供所有 syscall 包装和辅助函数
2. `void _start(void)` — Kenux 程序入口，替代标准 C 的 `main()`
3. `const char *msg = "Hello, KenuxOS!\n"` — 字符串常量，编译器放入 .rodata 段
4. `sys_write(1, msg, 16)` — 系统调用：向文件描述符 1 (stdout) 写入 16 字节
5. `sys_exit(0)` — 系统调用：以返回码 0 退出进程

### 关键要点

- **必须使用 `_start`** 而非 `main`，因为 Kenux 程序不链接 libc
- **必须调用 `sys_exit`** 退出，否则程序执行完入口函数后行为未定义
- **字符串长度需自行计算**，没有 `strlen` 除非使用 `kex_strlen` 辅助函数

---

## 4. 编译与运行

### 4.1 编译为 .kex

```bash
kex hello.c -o hello.kex --name hello
```

参数说明：
- `hello.c` — 源文件
- `-o hello.kex` — 输出文件名
- `--name hello` — 嵌入 kex 头部的程序名（可选，最多 16 字符）

### 4.2 运行 .kex 程序

```bash
kex run hello.kex
```

Kex 加载器会：
1. 读取 .kex 文件，校验魔数和 CRC32
2. 将代码段映射到可执行内存 (mmap RWX)
3. 解析导入表，为每个 API 生成 syscall stub
4. 应用本地重定位（.rodata 引用修补）
5. 应用外部重定位（.kxp 动态链接）
6. 跳转到入口点执行

### 4.3 编译为 .kxp 动态库

```bash
kex libhello.c --shared -o libhello.kxp --name libhello
```

### 4.4 编译为 ELF

```bash
kex hello.c --format elf -o hello
./hello
```

### 4.5 查看详细编译过程

```bash
kex hello.c -o hello.kex --verbose
```

---

## 5. 系统调用 API

### 5.1 文件 I/O

| 函数 | syscall 号 | 说明 |
|------|-----------|------|
| `sys_read(fd, buf, count)` | 0 | 读取文件 |
| `sys_write(fd, buf, count)` | 1 | 写入文件 |
| `sys_open(path, flags, mode)` | 2 | 打开文件 |
| `sys_close(fd)` | 3 | 关闭文件 |
| `sys_lseek(fd, offset, whence)` | 8 | 移动文件指针 |
| `sys_mmap(addr, len, prot, flags, fd, off)` | 9 | 内存映射 |
| `sys_munmap(addr, len)` | 11 | 解除映射 |
| `sys_getcwd(buf, size)` | 79 | 获取当前目录 |
| `sys_chdir(path)` | 80 | 切换目录 |
| `sys_mkdir(path, mode)` | 83 | 创建目录 |
| `sys_unlink(path)` | 87 | 删除文件 |

### 5.2 进程管理

| 函数 | syscall 号 | 说明 |
|------|-----------|------|
| `sys_fork()` | 57 | 创建子进程 |
| `sys_execve(path, argv, envp)` | 59 | 执行程序 |
| `sys_exit(code)` | 60 | 退出进程 |
| `sys_wait4(pid, status, opts, ru)` | 61 | 等待子进程 |
| `sys_getpid()` | 39 | 获取进程 ID |
| `sys_getppid()` | 110 | 获取父进程 ID |
| `sys_getuid()` | 102 | 获取用户 ID |
| `sys_getgid()` | 104 | 获取组 ID |

### 5.3 网络

| 函数 | syscall 号 | 说明 |
|------|-----------|------|
| `sys_socket(domain, type, protocol)` | 41 | 创建套接字 |
| `sys_connect(sockfd, addr, addrlen)` | 42 | 连接 |
| `sys_bind(sockfd, addr, addrlen)` | 49 | 绑定地址 |
| `sys_listen(sockfd, backlog)` | 50 | 监听 |
| `sys_accept(sockfd, addr, addrlen)` | 43 | 接受连接 |
| `sys_sendto(sockfd, buf, len, flags, addr, addrlen)` | 44 | 发送数据 |
| `sys_recvfrom(sockfd, buf, len, flags, addr, addrlen)` | 45 | 接收数据 |
| `sys_close(fd)` | 3 | 关闭套接字 |

### 5.4 Kenux 扩展 syscall

| 函数 | syscall 号 | 说明 |
|------|-----------|------|
| `kenux_info(buf)` | 451 | 获取系统信息 |
| `kenux_get_version(buf)` | 453 | 获取内核版本 |
| `kenux_get_uptime(buf)` | 454 | 获取运行时间 |
| `kenux_get_loadavg(loads, n)` | 455 | 获取负载均值 |
| `kenux_get_cpu_count()` | 459 | 获取 CPU 数量 |
| `kenux_get_cpu_info(id, buf)` | 460 | 获取 CPU 信息 |

### 5.5 辅助函数

kex_user.h 提供的辅助函数（非 syscall，纯计算实现）：

```c
kex_strlen(s)              // 字符串长度
kex_puts(s)                // 输出字符串 + 换行
kex_putchar(c)             // 输出单个字符
kex_printlong(val, newline) // 输出长整数
kex_memset(dst, val, n)    // 内存填充
kex_memcpy(dst, src, n)    // 内存拷贝
kex_memcmp(a, b, n)        // 内存比较
```

---

## 6. 文件 I/O 程序

创建 `fileio.c`：

```c
#include "kex_user.h"

#define O_WRONLY_CREAT_TRUNC 0x241
#define O_RDONLY_FLAG        0
#define FILE_MODE            0644

void _start(void) {
    const char *path = "/tmp/kex_test.txt";
    const char *write_msg = "Kenux file I/O works!\n";

    long fd = sys_open(path, O_WRONLY_CREAT_TRUNC, FILE_MODE);
    if (fd < 0) {
        const char *err = "open(write) failed\n";
        sys_write(2, err, 19);
        sys_exit(1);
    }
    sys_write((int)fd, write_msg, 22);
    sys_close((int)fd);

    fd = sys_open(path, O_RDONLY_FLAG, 0);
    if (fd < 0) {
        const char *err = "open(read) failed\n";
        sys_write(2, err, 18);
        sys_exit(1);
    }

    char buf[64];
    long n = sys_read((int)fd, buf, sizeof(buf));
    sys_close((int)fd);

    if (n > 0) {
        sys_write(1, buf, (size_t)n);
    }

    sys_unlink(path);
    sys_exit(0);
}
```

编译运行：

```bash
kex fileio.c -o fileio.kex --name fileio
kex run fileio.kex
# 输出: Kenux file I/O works!
```

### 要点

- 文件标志使用 Linux 数值常量（`O_WRONLY|O_CREAT|O_TRUNC = 0x241`）
- 所有 syscall 返回 `long`，负值表示错误
- 文件描述符需强转 `int` 传给后续 syscall

---

## 7. 数学与算法程序

创建 `math.c`：

```c
#include "kex_user.h"

long factorial(long n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}

long fibonacci(long n) {
    if (n <= 0) return 0;
    if (n <= 2) return 1;
    long a = 1, b = 1, c;
    for (long i = 3; i <= n; i++) {
        c = a + b;
        a = b;
        b = c;
    }
    return b;
}

long is_prime(long n) {
    if (n < 2) return 0;
    if (n < 4) return 1;
    if (n % 2 == 0) return 0;
    for (long i = 3; i * i <= n; i += 2) {
        if (n % i == 0) return 0;
    }
    return 1;
}

void _start(void) {
    kex_puts("=== Math Demo ===");

    kex_puts("factorial(10) =");
    kex_printlong(factorial(10), 1);

    kex_puts("fibonacci(20) =");
    kex_printlong(fibonacci(20), 1);

    kex_puts("primes below 30:");
    for (long i = 2; i < 30; i++) {
        if (is_prime(i)) {
            kex_printlong(i, 0);
            kex_putchar(' ');
        }
    }
    kex_putchar('\n');

    sys_exit(0);
}
```

```bash
kex math.c -o math.kex --name math
kex run math.kex
```

### 要点

- Kenux 程序完全支持递归、循环、条件等 C 控制流
- 没有浮点 printf，用 `kex_printlong` 输出整数
- 算法逻辑与标准 C 完全一致

---

## 8. 进程管理程序

创建 `procmgr.c`：

```c
#include "kex_user.h"

static void print_kv(const char *key, long val) {
    sys_write(1, key, kex_strlen(key));
    kex_printlong(val, 1);
}

void _start(void) {
    kex_puts("=== Process Manager ===");

    print_kv("  PID:  ", sys_getpid());
    print_kv("  PPID: ", sys_getppid());
    print_kv("  UID:  ", sys_getuid());
    print_kv("  GID:  ", sys_getgid());
    kex_puts("");

    kex_puts("--- Fork Demo ---");
    long child = sys_fork();
    if (child == 0) {
        kex_puts("  [Child] running");
        for (volatile int i = 0; i < 1000000; i++) {}
        kex_puts("  [Child] exiting");
        sys_exit(0);
    } else if (child > 0) {
        kex_puts("  [Parent] waiting for child");
        int status;
        sys_wait4((int)child, &status, 0, 0);
        kex_puts("  [Parent] child exited");
    } else {
        kex_puts("  fork failed");
    }

    sys_exit(0);
}
```

```bash
kex procmgr.c -o procmgr.kex --name procmgr
kex run procmgr.kex
```

### 要点

- `sys_fork()` 返回 0 表示子进程，>0 表示父进程（返回子 PID），<0 表示失败
- 子进程必须 `sys_exit()` 退出，否则会继续执行父进程代码
- `sys_wait4()` 等待子进程结束

---

## 9. 网络通信程序

创建 `netstat.c`：

```c
#include "kex_user.h"

struct kex_sockaddr_in {
    unsigned short sin_family;
    unsigned short sin_port;
    unsigned int   sin_addr;
    char           sin_zero[8];
};

static void print_kv(const char *key, long val) {
    sys_write(1, key, kex_strlen(key));
    kex_printlong(val, 1);
}

void _start(void) {
    kex_puts("=== Network Status ===");

    long tcp_sock = sys_socket(2, 1, 0);
    print_kv("  TCP socket: ", tcp_sock);
    if (tcp_sock >= 0) sys_close((int)tcp_sock);

    long udp_sock = sys_socket(2, 2, 0);
    print_kv("  UDP socket: ", udp_sock);
    if (udp_sock >= 0) sys_close((int)udp_sock);

    long unix_sock = sys_socket(1, 1, 0);
    print_kv("  UNIX socket: ", unix_sock);
    if (unix_sock >= 0) sys_close((int)unix_sock);

    sys_exit(0);
}
```

```bash
kex netstat.c -o netstat.kex --name netstat
kex run netstat.kex
```

### 要点

- `AF_INET=2`, `SOCK_STREAM=1`, `SOCK_DGRAM=2`, `AF_UNIX=1`
- 端口号需网络字节序（大端），手动转换：`((port & 0xFF) << 8) | ((port >> 8) & 0xFF)`
- 套接字也是文件描述符，用 `sys_close` 关闭

---

## 10. 动态库开发（.kxp）

### 10.1 创建动态库

创建 `libhello.c`：

```c
#include "kex_user.h"

void lib_hello(void) {
    const char *msg = "from library!\n";
    sys_write(1, msg, 14);
}

long lib_add(long a, long b) {
    return a + b;
}

void lib_greet(const char *name) {
    const char *prefix = "Hello, ";
    const char *suffix = "!\n";
    sys_write(1, prefix, 7);
    sys_write(1, name, kex_strlen(name));
    sys_write(1, suffix, 2);
}
```

编译为 .kxp：

```bash
kex libhello.c --shared -o libhello.kxp --name libhello
```

### 10.2 使用动态库

创建 `test_kxp.c`：

```c
#include "kex_user.h"

void lib_hello(void);
long lib_add(long a, long b);
void lib_greet(const char *name);

void _start(void) {
    lib_hello();

    long result = lib_add(3, 4);
    const char *prefix = "3+4=";
    sys_write(1, prefix, 4);
    char c = '0' + (char)(result % 10);
    sys_write(1, &c, 1);
    kex_putchar('\n');

    lib_greet("Kenux");
    sys_exit(0);
}
```

编译运行：

```bash
kex test_kxp.c -o test_kxp.kex --name test_kxp
kex run test_kxp.kex -Lexamples
```

### 要点

- .kxp 导出的函数在 .kex 中声明为外部函数即可调用
- 加载器根据重定位表自动解析 .kxp 中的符号地址
- .kxp 支持跨模块链接，类似 Linux 的 .so

---

## 11. 图形界面程序

### 11.1 基本图形窗口

创建 `gfx_demo.c`：

```c
#include <kapi_gfxrender.h>

int main() {
    kgr_window_desc_t desc = {0};
    strcpy(desc.title, "My First Kenux App");
    desc.width = 800;
    desc.height = 600;
    desc.format = KGR_FORMAT_RGBA8888;
    desc.flags = KGR_FLAG_RESIZABLE;

    kgr_context_t *ctx = kgr_create(&desc);
    if (!ctx) return 1;

    while (!ctx->should_close) {
        kgr_begin_frame(ctx);
        kgr_clear(ctx, (kgr_color_t){0, 0, 0, 255});
        kgr_draw_rect(ctx, 100, 100, 200, 150, (kgr_color_t){255, 0, 0, 255});
        kgr_draw_line(ctx, 0, 0, 800, 600, (kgr_color_t){0, 255, 0, 255});
        kgr_end_frame(ctx);
        kgr_present(ctx);
    }

    kgr_destroy(ctx);
    return 0;
}
```

### 11.2 编译与运行

```bash
kex gfx_demo.c -o gfx_demo.kex --name gfx_demo
kex run gfx_demo.kex --gfx --gfx-size 800x600
```

Kex 自动检测源码中的 `kgr_` / `kapi_graphics` / `kapi_window` 依赖，运行时自动打开 X11 窗口。

### 11.3 图形 API 速查

| 函数 | 说明 |
|------|------|
| `kgr_create(&desc)` | 创建图形上下文和窗口 |
| `kgr_destroy(ctx)` | 销毁上下文 |
| `kgr_begin_frame(ctx)` | 开始绘制帧 |
| `kgr_end_frame(ctx)` | 结束绘制帧 |
| `kgr_present(ctx)` | 提交帧到屏幕 |
| `kgr_clear(ctx, color)` | 清屏 |
| `kgr_draw_rect(ctx, x, y, w, h, color)` | 绘制矩形 |
| `kgr_draw_line(ctx, x0, y0, x1, y1, color)` | 绘制线段 |
| `kgr_draw_circle(ctx, cx, cy, r, color)` | 绘制圆 |
| `kgr_draw_text(ctx, x, y, text, color)` | 绘制文本 |

---

## 12. 多文件项目与构建系统

### 12.1 多文件编译

```bash
kex main.c utils.c render.c -o myapp.kex --name myapp
```

Kex 自动检测每个文件的语言（.c → C，.cpp → C++），分别编译后合并。

### 12.2 使用 KenuxBuild 构建文件

创建 `KenuxBuild` 文件：

```
project(myapp)

set(CC gcc)
set(CXX g++)
set(OPT 2)

include_dir(./include)
define(DEBUG)

target(myapp)
  sources(main.c utils.c render.c)
  format(kex)
  output(myapp.kex)

target(mylib)
  sources(libcore.c libutil.c)
  format(kxp)
  output(libmylib.kxp)
  shared()

target(native_tool)
  sources(tool.c)
  format(elf)
  output(kenux-tool)
```

### 12.3 构建命令

```bash
# 初始化项目（生成 KenuxBuild 模板）
kex init

# 构建所有目标
kex build

# 构建指定目标
kex build --target myapp

# 详细输出
kex build --verbose

# 指定构建文件
kex build -f MyBuild

# 清理
kex clean
```

### 12.4 KenuxBuild 语法参考

| 指令 | 说明 | 示例 |
|------|------|------|
| `project(name)` | 项目名 | `project(myapp)` |
| `set(KEY VALUE)` | 设置变量 | `set(CC gcc)` |
| `include_dir(dir)` | 添加头文件目录 | `include_dir(./include)` |
| `define(MACRO)` | 添加预处理器定义 | `define(DEBUG)` |
| `target(name)` | 定义构建目标 | `target(myapp)` |
| `sources(...)` | 目标源文件 | `sources(main.c util.c)` |
| `format(fmt)` | 输出格式 | `format(kex)` / `format(kxp)` / `format(elf)` |
| `output(path)` | 输出文件 | `output(myapp.kex)` |
| `shared()` | 构建为共享库 | `shared()` |
| `depends(...)` | 目标依赖 | `depends(mylib)` |

---

## 13. 编译为 ELF 原生可执行

Kex 支持将同一套代码编译为原生 ELF，直接在 Linux 上运行：

```bash
kex hello.c --format elf -o hello
./hello
```

### 何时使用 ELF 输出

- 需要直接在 Linux 上运行，不经过 Kenux 加载器
- 需要使用 Linux 调试工具（gdb, strace, perf）
- 性能基准测试对比

### 限制

- ELF 输出不包含 KEX 头部信息（程序名、图标等）
- Kenux 扩展 syscall (451-500) 在 Linux 上可能返回 ENOSYS
- 不支持 .kxp 动态链接

---

## 14. 系统信息工具实战

综合运用多种 API，创建 `sysinfo.c`：

```c
#include "kex_user.h"

#define KEX_UTS_LEN 65

struct kex_new_utsname {
    char sysname[KEX_UTS_LEN];
    char nodename[KEX_UTS_LEN];
    char release[KEX_UTS_LEN];
    char version[KEX_UTS_LEN];
    char machine[KEX_UTS_LEN];
    char domainname[KEX_UTS_LEN];
};

struct kex_sysinfo {
    long uptime;
    unsigned long loads[3];
    unsigned long totalram;
    unsigned long freeram;
    unsigned long sharedram;
    unsigned long bufferram;
    unsigned long totalswap;
    unsigned long freeswap;
    unsigned short procs;
    unsigned short pad;
    unsigned long totalhigh;
    unsigned long freehigh;
    unsigned int mem_unit;
};

struct kex_info_buf {
    char version[32];
    char name[32];
};

static void print_kv_str(const char *key, const char *val) {
    sys_write(1, key, kex_strlen(key));
    sys_write(1, val, kex_strlen(val));
    kex_putchar('\n');
}

static void print_kv_long(const char *key, long val) {
    sys_write(1, key, kex_strlen(key));
    kex_printlong(val, 1);
}

void _start(void) {
    kex_puts("========================================");
    kex_puts("    Kenux System Information            ");
    kex_puts("========================================");
    kex_puts("");

    kex_puts("--- Kenux Info ---");
    struct kex_info_buf info;
    kex_memset(&info, 0, sizeof(info));
    long ret = kenux_info(&info);
    if (ret == 0) {
        print_kv_str("  version: ", info.version);
        print_kv_str("  name:    ", info.name);
    } else {
        print_kv_long("  failed, ret=", ret);
    }
    kex_puts("");

    kex_puts("--- sys_uname ---");
    struct kex_new_utsname ub;
    kex_memset(&ub, 0, sizeof(ub));
    ret = sys_uname(&ub);
    if (ret == 0) {
        print_kv_str("  sysname:  ", ub.sysname);
        print_kv_str("  release:  ", ub.release);
        print_kv_str("  machine:  ", ub.machine);
    }
    kex_puts("");

    kex_puts("--- sys_sysinfo ---");
    struct kex_sysinfo si;
    kex_memset(&si, 0, sizeof(si));
    ret = sys_sysinfo(&si);
    if (ret == 0) {
        print_kv_long("  uptime:    ", si.uptime);
        print_kv_long("  totalram:  ", (long)si.totalram);
        print_kv_long("  freeram:   ", (long)si.freeram);
        print_kv_long("  procs:     ", (long)si.procs);
    }
    kex_puts("");

    kex_puts("--- CPU ---");
    long cpu_count = kenux_get_cpu_count();
    print_kv_long("  CPU count: ", cpu_count);

    sys_exit(0);
}
```

```bash
kex sysinfo.c -o sysinfo.kex --name sysinfo
kex run sysinfo.kex
```

---

## 15. 完整项目：文件浏览器

这是一个完整的多功能文件浏览器程序，综合运用文件 I/O、目录遍历、格式化输出：

```c
#include "kex_user.h"

struct kex_stat_buf {
    unsigned long st_dev;
    unsigned long st_ino;
    unsigned long st_nlink;
    int st_mode;
    int st_uid;
    int st_gid;
    unsigned long st_rdev;
    unsigned long st_size;
    unsigned long st_blksize;
    unsigned long st_blocks;
    unsigned long st_atime;
    unsigned long st_mtime;
    unsigned long st_ctime;
};

struct kex_dirent {
    unsigned long d_ino;
    unsigned long d_off;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[256];
};

static void print_str(const char *s) {
    sys_write(1, s, kex_strlen(s));
}

static void print_mode(int mode) {
    char buf[11];
    buf[0] = (mode & 0x40000) ? 'd' : '-';
    buf[1] = (mode & 0x100) ? 'r' : '-';
    buf[2] = (mode & 0x80)  ? 'w' : '-';
    buf[3] = (mode & 0x40)  ? 'x' : '-';
    buf[4] = (mode & 0x40)  ? 'r' : '-';
    buf[5] = (mode & 0x20)  ? 'w' : '-';
    buf[6] = (mode & 0x10)  ? 'x' : '-';
    buf[7] = (mode & 0x4)   ? 'r' : '-';
    buf[8] = (mode & 0x2)   ? 'w' : '-';
    buf[9] = (mode & 0x1)   ? 'x' : '-';
    buf[10] = '\0';
    print_str(buf);
}

static void print_size(unsigned long sz) {
    if (sz < 1024) {
        kex_printlong((long)sz, 0);
        print_str(" B");
    } else if (sz < 1024 * 1024) {
        kex_printlong((long)(sz / 1024), 0);
        print_str(" KB");
    } else {
        kex_printlong((long)(sz / (1024 * 1024)), 0);
        print_str(" MB");
    }
}

void _start(void) {
    kex_puts("=== Kenux File Browser ===");
    kex_puts("");

    char cwd[512];
    kex_memset(cwd, 0, sizeof(cwd));
    long ret = sys_getcwd(cwd, sizeof(cwd));
    if (ret > 0) {
        print_str("  Directory: ");
        print_str(cwd);
        kex_putchar('\n');
    }

    int fd = (int)sys_open(cwd, 0, 0);
    if (fd < 0) {
        kex_puts("  Cannot open directory");
        sys_exit(1);
    }

    char dirbuf[4096];
    long nread = sys_getdents64(fd, dirbuf, sizeof(dirbuf));
    sys_close(fd);

    if (nread <= 0) {
        kex_puts("  Empty or cannot read");
        sys_exit(0);
    }

    kex_puts("  Mode       Size       Name");
    kex_puts("  ----       ----       ----");

    long pos = 0;
    while (pos < nread) {
        struct kex_dirent *d = (struct kex_dirent *)(dirbuf + pos);
        if (d->d_reclen == 0) break;

        if (d->d_name[0] != '.') {
            struct kex_stat_buf st;
            kex_memset(&st, 0, sizeof(st));
            sys_stat(d->d_name, &st);

            print_str("  ");
            print_mode(st.st_mode);
            print_str("  ");
            print_size(st.st_size);
            print_str("  ");
            print_str(d->d_name);
            kex_putchar('\n');
        }

        pos += d->d_reclen;
    }

    sys_exit(0);
}
```

```bash
kex filebrowser.c -o filebrowser.kex --name filebrowser
kex run filebrowser.kex
```

---

## 16. 调试与排错

### 16.1 常见错误

| 错误信息 | 原因 | 解决方法 |
|---------|------|---------|
| `kex: 无法读取 xxx` | 文件不存在 | 检查路径 |
| `kex: 无效的kex文件` | 文件太小 | 重新编译 |
| `kex: 非kex文件` | 格式不对 | 确认输出 .kex 而非 .elf |
| `kex: 代码段超出文件范围` | 文件损坏 | 重新编译 |
| `kex: 无法分配可执行内存` | mmap 失败 | 检查系统内存限制 |
| `Segmentation fault` | 程序逻辑错误 | 用 ELF 格式 + gdb 调试 |

### 16.2 调试流程

```bash
# 1. 编译为 ELF 并带调试信息
kex myapp.c --format elf -g -o myapp_debug

# 2. 用 gdb 调试
gdb ./myapp_debug
(gdb) break _start
(gdb) run

# 3. 用 strace 跟踪 syscall
strace ./myapp_debug

# 4. 确认逻辑正确后，编译为 .kex
kex myapp.c -o myapp.kex --name myapp
kex run myapp.kex
```

### 16.3 保留中间文件

```bash
kex myapp.c -o myapp.kex --keep-temps --verbose
# 查看生成的 .o 文件和合并后的目标文件
```

---

## 17. 最佳实践

### 17.1 代码组织

```
my_project/
├── KenuxBuild          构建配置
├── include/
│   └── myapp.h         项目头文件
├── src/
│   ├── main.c          入口
│   ├── core.c          核心逻辑
│   └── util.c          工具函数
└── assets/
    └── icon.png        程序图标
```

### 17.2 编码规范

1. **入口函数**：始终使用 `void _start(void)`，末尾调用 `sys_exit(0)`
2. **错误处理**：检查每个 syscall 返回值，负值即错误
3. **字符串常量**：依赖 .rodata 段自动重定位，无需手动管理
4. **内存操作**：使用 `kex_memset` / `kex_memcpy` 而非自己写循环
5. **输出**：用 `kex_puts` / `kex_printlong` 替代 printf
6. **无注释**：Kenux 程序代码追求简洁，不写批注

### 17.3 性能优化

```bash
# 使用 -O2 优化
kex myapp.c -O2 -o myapp.kex

# 发布版本剥离符号
kex myapp.c -O2 -s -o myapp.kex

# 最小体积
kex myapp.c -Os -s -o myapp.kex
```

### 17.4 跨平台注意

- Linux 和 Windows 的文件标志值不同，使用条件编译
- 网络字节序转换需手动实现（没有 arpa/inet.h）
- 图形程序 Linux 用 X11，Windows 用 Win32，kex 自动选择

### 17.5 完整开发流程

```
编写代码 → kex 编译为 .kex → kex run 测试 → 编译为 ELF + gdb 调试 → 修复 → 重新编译 .kex → 发布
```

---

## 附录：快速命令参考

```bash
# 编译
kex source.c -o output.kex                    # 编译为 kex
kex source.c --format kxp --shared -o lib.kxp # 编译为 kxp
kex source.c --format elf -o tool             # 编译为 elf
kex a.c b.c c.c -o app.kex --name myapp      # 多文件编译

# 运行
kex run app.kex                               # 运行 kex
kex run app.kex --gfx                         # 强制图形窗口
kex run app.kex --gfx --gfx-size 1024x768     # 指定窗口尺寸

# 构建
kex init                                      # 初始化项目
kex build                                     # 构建所有
kex build --target myapp                      # 构建指定目标
kex clean                                     # 清理

# 安装
kex install                                   # 安装到 PATH
kex install --prefix /opt/kenux               # 安装到指定目录
kex uninstall                                 # 卸载
```

---

> 下一步：查阅 [API 参考文档](API.md) 了解所有 Kapi 接口的详细定义。