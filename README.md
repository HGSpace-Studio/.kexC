# KexKit

**KexKit** 是 KenuxOS 的用户态程序工具链，包含编译器 (`kexc`)、解释器/加载器 (`kex_interp`)、运行时库 (`libkex.a`) 和统一入口 (`kex`)。它把 C 源码编译为紧凑的 HGS 二进制格式 (`.kex` 主程序 / `.kxp` 动态库)，可在 KenuxOS 或宿主机 (Linux/WSL/Windows) 上加载执行。

```bash
kex compile hello.c hello.kex --name hello   # 编译
kex run hello.kex                             # 运行
```

## 目录结构

```
kex/
├── include/                  # 公共头文件
│   ├── kex_format.h          #   HGS 文件格式定义 (头部/段/重定位)
│   ├── kex_api.h             #   API 映射表接口
│   ├── kex_loader.h          #   加载器接口 (kex_program_t 等)
│   ├── kex_crc32.h           #   CRC32 工具
│   ├── kex_compat.h          #   跨平台兼容层 (mmap/mprotect 等)
│   └── kex_user.h            #   用户程序 API (sys_*/kenux_* 包装)
├── src/                      # 库与编译器源码
│   ├── kex_main.c            #   统一入口 (compile/run/version/help)
│   ├── kexc.c                #   KEX 编译器: C -> .kex/.kxp
│   ├── kex_loader.c          #   文件加载与解析
│   ├── kex_api_table.c       #   Kenux API 映射表 (哈希查找)
│   └── kex_crc32.c           #   CRC32 实现
├── tools/
│   └── kex_interp.c          #   解释器/加载执行器
├── examples/                 # 示例程序
│   ├── hello.c               #   最小示例
│   ├── minimal.c             #   极简示例 (单字符输出)
│   ├── libhello.c            #   .kxp 动态库示例
│   ├── test_kxp.c            #   动态库调用示例
│   ├── fileio.c              #   文件 I/O 示例
│   ├── utils.c               #   便利工具函数示例
│   ├── math.c                #   递归调用示例 (阶乘/斐波那契/素数)
│   └── test_debug.c          #   调试测试示例
├── Makefile
├── LICENSE                   # KenuxOS 开发者许可协议
└── README.md
```

## 构建

依赖: `gcc` (支持 x86_64), `make`, Linux 或 WSL 环境。

```bash
make            # 构建 libkex.a, kexc, kex_interp, kex (统一入口)
make examples   # 编译所有示例
make test       # 编译并运行所有测试
make clean      # 清理产物
```

### Windows 交叉编译

在 WSL/Linux 中交叉编译 Windows 原生 exe:

```bash
make win32      # 构建 kexc.exe, kex_interp.exe, kex.exe
```

### 统一入口 `kex`

`kex` 将编译器和解释器合为单文件，支持子命令:

```bash
kex compile <input.c> <output.kex> [--name NAME] [--kxp]  # 编译
kex c       <input.c> <output.kex> [--name NAME] [--kxp]  # 编译 (简写)
kex run     <program.kex> [-L<libdir>] [args...]          # 运行
kex r       <program.kex> [-L<libdir>] [args...]          # 运行 (简写)
kex version                                               # 版本信息
kex help                                                  # 帮助
```

## 快速开始

### 1. Hello World

```bash
make demo
# 或使用统一入口:
./kex compile examples/hello.c examples/hello.kex --name hello
./kex run examples/hello.kex
# 或分别使用:
./kexc examples/hello.c examples/hello.kex --name hello
./kex_interp examples/hello.kex
```

输出:
```
Hello, KenuxOS!
syscall works!
```

### 2. 动态库 (.kxp)

```bash
make demo-kxp
# 或使用统一入口:
./kex compile examples/libhello.c examples/libhello.kxp --name libhello --kxp
./kex compile examples/test_kxp.c examples/test_kxp.kex --name test_kxp
./kex run examples/test_kxp.kex -Lexamples
# 或分别使用:
./kexc examples/libhello.c examples/libhello.kxp --name libhello --kxp
./kexc examples/test_kxp.c examples/test_kxp.kex --name test_kxp
./kex_interp -Lexamples examples/test_kxp.kex
```

输出:
```
from library!
3+4=7
Hello, Kenux!
```

### 3. 文件 I/O

```bash
make demo-fileio
```

## HGS 文件格式

所有 `.kex` / `.kxp` 文件采用 HGS (HGS Generic Stream) 格式:

| 偏移  | 大小 | 字段                     |
|-------|------|--------------------------|
| 0x00  | 4    | 魔数 `F0 A5 48 47`       |
| 0x04  | 2    | 版本 `0x5300`            |
| 0x06  | 2    | 头部大小 `80`            |
| 0x08  | 4    | 文件总大小               |
| 0x0C  | 4    | 代码入口偏移             |
| 0x10  | 4    | 代码大小                 |
| 0x14  | 4    | 导入表偏移               |
| 0x18  | 4    | 导入条目数               |
| 0x1C  | 4    | 导出表偏移               |
| 0x20  | 4    | 导出条目数               |
| 0x24  | 4    | 图标偏移                 |
| 0x28  | 4    | 图标大小                 |
| 0x2C  | 4    | 栈大小                   |
| 0x30  | 4    | 堆大小                   |
| 0x34  | 4    | CRC32 校验和             |
| 0x38  | 2    | 文件类型 (0=KEX, 1=KXP)  |
| 0x3A  | 2    | 标志位                   |
| 0x3C  | 4    | 重定位表偏移             |
| 0x40  | 4    | .rodata 偏移             |
| 0x44  | 4    | .rodata 大小             |
| 0x48  | 4    | 本地重定位表偏移         |
| 0x4C  | 4    | 本地重定位条目数         |
| 0x50  | 16   | 程序名                   |
| 0x60  | 8    | 机器码开始符 `Kenux---`  |
| 0x68  | N    | x86-64 机器码            |
| ...   | 7    | 结束符 `KKKSTOP`         |
| ...   | 变长 | .rodata / 导入表 / 导出表 / 重定位表 / 资源 |

### 段说明

- **代码段**: 入口符号 `_start` 所在的 `.text` 机器码，由 `Kenux---` 和 `KKKSTOP` 包围。
- **.rodata 段**: 只读数据 (字符串字面量、常量)。加载时映射到独立内存区并设为只读，代码段通过 RIP 相对寻址引用，由本地重定位表修补 disp32。
- **导入表**: 程序依赖的 Kenux API (syscall)。每条记录 API 名称的 CRC32 + 地址占位 + 函数名。加载时生成 syscall stub 回填地址占位。
- **导出表** (仅 .kxp): 库向外提供的函数符号。每条记录函数名 + 代码偏移。
- **重定位表**: 跨文件 `call rel32` 的目标地址修补。每条记录 call 指令操作数偏移 + 目标符号 CRC32。

## API 体系

KexKit 覆盖完整的 x86_64 syscall 集 (0-450, 含新增 333-335/424/428-450) 加上 Kenux 扩展 (451-500):

### 标准 syscall (sys_ 前缀)

文件 I/O、内存管理、进程线程、网络、信号、时间、文件系统、电源、扩展属性、io_uring、seccomp/eBPF、pidfd、landlock、mount API、futex_waitv 等共 330+ 个系统调用包装。

### Kenux 扩展 (kenux_ 前缀, 451-500)

| 编号范围 | 类别         | 示例                                        |
|----------|--------------|---------------------------------------------|
| 451-455  | 系统信息     | `kenux_info`, `kenux_get_version`           |
| 456-458  | 电源控制     | `kenux_reboot`, `kenux_poweroff`, `kenux_halt` |
| 459-462  | CPU/亲和性   | `kenux_get_cpu_count`, `kenux_set_affinity` |
| 463-465  | 命名空间     | `kenux_create_namespace`                    |
| 466-468  | 虚拟内存空间 | `kenux_vmspace_create/destroy/switch`       |
| 469-472  | IOMMU/DMA    | `kenux_iommu_map`, `kenux_dma_alloc`        |
| 473-475  | PCI          | `kenux_pci_read/write/enum`                 |
| 476-477  | ACPI/SMBIOS  | `kenux_acpi_query`, `kenux_smbios_get`      |
| 478-481  | 帧缓冲       | `kenux_fb_get_info/map/flip`                |
| 482-483  | GPU          | `kenux_gpu_submit/wait`                     |
| 484-486  | 网络         | `kenux_net_attach/detach/ioctl`             |
| 487-490  | 文件系统扩展 | `kenux_fs_snapshot/rollback/compress/encrypt`|
| 491-492  | 审计         | `kenux_audit_log/config`                    |
| 493-494  | seccomp 扩展 | `kenux_seccomp_install/filter`              |
| 495-498  | trace        | `kenux_trace_attach/detach/read/write`      |
| 499-500  | kprobe       | `kenux_kprobe_register/unregister`          |

### 查找性能

API 表使用 1024 槽哈希表 (开放寻址法)，平均 O(1) 查找。`kex_api_lookup_crc32()` 用于加载时按导入表 CRC32 反查 syscall 号。

## 编写 KEX 程序

```c
#include "kex_user.h"

void _start(void) {
    const char *msg = "Hello!\n";
    sys_write(1, msg, 7);
    sys_exit(0);
}
```

约定:
- 入口符号必须是 `void _start(void)`，不接收 argc/argv。
- 所有系统调用通过 `kex_user.h` 中的 `sys_*` / `kenux_*` 内联包装调用，编译后直接内联为 syscall 机器码。
- 字符串字面量自动放入 `.rodata` 段，无需手动管理。
- 不依赖 libc; 使用 `kex_strlen` / `kex_memcpy` / `kex_printlong` 等便利函数。

### 编译为 .kxp 动态库

```c
#include "kex_user.h"

/* 全局函数即导出符号 */
long lib_add(long a, long b) {
    return a + b;
}
```

```bash
./kexc lib.c lib.kxp --name mylib --kxp
```

调用方声明外部函数即可，`kexc` 自动生成跨文件重定位表，`kex_interp` 加载时回填目标地址。

## 安全特性

- **CRC32 校验**: 加载时强制校验整个文件 CRC32，防止损坏或篡改。
- **W^X 内存保护**: 代码段先 RW 写入再 mprotect 为 RX，.rodata 设为只读。
- **边界检查**: 所有偏移和大小在加载时校验，防止越界访问。
- **Stub 低地址分配**: syscall stub 用 `MAP_32BIT` 分配在低 4GB，兼容 32 位地址占位。

## 许可

本项目采用 **KenuxOS 开发者许可协议** (见 [LICENSE](LICENSE))。

- 仅授权用于为 KenuxOS 操作系统开发应用程序
- 禁止商业用途、抄袭源码、搬运至其他平台
- 详见 `LICENSE` 文件
