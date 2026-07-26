# Kenux Kernel

**最新版本：** 26.7.9K

**目标架构：** x86_64

**编译模式：** freestanding

**启动方式：** Multiboot / EFI

---

## 概述

Kenux Kernel 是一个为 KenuxOS 设计的 x86_64 操作系统内核。提供完整的内核子系统，包括进程管理、内存管理、文件系统、网络协议栈、设备驱动和同步原语。当前版本包含 220+ 个源文件，约 40,000+ 行代码，编译产物 kernel.elf 约 457 KB。

---

## 核心特性

### 进程与调度
- 5 级优先级轮转调度（IDLE / LOW / NORMAL / HIGH / REALTIME）
- CFS 完全公平调度器（红黑树 + vruntime）
- x86_64 上下文切换（保存/恢复寄存器、栈切换）
- kthread 内核线程框架
- 用户态 ELF 加载器 + 动态链接器 ld.so

### 内存管理
- Buddy 物理页分配器（11 级 order，4KB~8MB）
- Slab 对象分配器（kmalloc/kfree，8B~2048B）
- 四级页表映射（PML4 -> PDPT -> PD -> PT，动态分配中间页表）
- VMA 虚拟内存区域管理
- 页缓存（Page Cache，哈希 + LRU）
- Swap 交换机制

### 同步与 IPC
- 自旋锁 / Ticket Lock / 读写锁 / 顺序锁
- 信号量 / 互斥锁 / 条件变量 / 屏障（含超时变体）
- RCU 无锁读取 / SRCU（多核感知）
- System V 共享内存 / 消息队列 / 信号量
- Futex / 命名管道 FIFO / Unix Domain Socket
- IPC 环形缓冲区（自旋锁保护，按源过滤接收）

### 文件系统
- VFS 虚拟文件系统框架（挂载/卸载、路径解析）
- ext2（完整直接块 + 单/双/三间接块支持）
- ext4（JBD2 日志、真实 AHCI 磁盘 IO）
- TMPFS / procfs（真实内核数据） / sysfs / devtmpfs
- ELF 可执行文件加载器
- JBD2 文件系统日志（真实设备读写）
- Page Cache + 目录项缓存

### 网络
- 完整 TCP/IP 协议栈（ARP / IP / ICMP / UDP / TCP）
- TCP 状态机（三次握手、四次挥手、滑动窗口）
- TCP 拥塞控制（Reno / CUBIC / BBR，per-socket 独立状态）
- DHCP 完整状态机（Discover -> Offer -> Request -> ACK）
- DNS 解析（UDP 查询 + 轮询接收，带缓存）
- Netfilter 钩子框架 + 连接跟踪（引用计数）
- BSD Socket API
- Epoll 事件轮询

### 设备驱动
- AHCI SATA（H2D FIS 构建、PRD 设置、DMA 读写）
- NVMe 存储驱动
- xHCI / EHCI USB 主控（端口状态检测）
- USB HID（控制传输、中断传输、报告描述符解析）
- PS/2 键盘 / 鼠标驱动
- VGA 文本模式 + Framebuffer GOP 图形渲染（分辨率设置）
- HDA / AC97 声卡（MMIO 寄存器读写）
- LAPIC / IOAPIC 中断控制器（IOAPIC MADT 解析）
- PCI 设备枚举与配置空间读写
- ACPI 电源管理（RSDP 签名修复、S3/S4/S5）
- VirtIO MMIO 设备探测

### 虚拟化与安全
- KVM / VMX 虚拟化（VMXON 初始化、EPT、CR/MSR/IO 模拟）
- 安全子系统（LSM / SELinux / capabilities / ASLR / seccomp / SMEP/SMAP）
- BPF 字节码过滤框架
- cgroup / namespace 容器化
- GDB 远程调试 Stub（COM1 串口）
- ftrace 函数跟踪

### Kenux API 双层架构
- **K API**（46 个模块）— 内核内部 API，对标 Linux include/linux/
  - 数据结构：list、rbtree、kfifo、idr、bitmap、hash
  - 同步原语：mutex、completion、wait、rcu、atomic
  - 内核基础：kthread、module、notifier、irq、params
  - 调度与 SMP：sched、smp、cpumask
  - 工具库：string、time、random、sort、crc
  - IO 与设备：io、pci、dma、cdev、blkdev
  - 网络栈：skbuff、netdevice、socket、netlink
  - 文件系统：vfs、seq_file、debugfs、kobject
  - 内存与跟踪：mempool、ftrace、security
- **KA API**（应用层）— 用户态 POSIX 系统调用封装
  - 进程管理、内存管理、文件系统、设备管理、系统调用

---

## 目录结构

```
Kenux Kernel/
├── changelogs/              # 更新日志（每个版本独立文件）
├── include/                 # K API + KA API 头文件（46 个）
│   ├── kapi.h               # 统一头文件（包含所有 K API）
│   ├── kapi_list.h          # 双向链表
│   ├── kapi_rbtree.h        # 红黑树
│   ├── kapi_atomic.h        # 原子操作
│   ├── kapi_mutex.h         # 互斥锁
│   ├── kapi_io.h            # IO 端口/MMIO
│   ├── kapi_pci.h           # PCI 设备
│   ├── kapi_socket.h        # Socket 核心
│   ├── kapi_security.h      # LSM 安全
│   ├── kapi_process.h       # 进程管理（KA API）
│   ├── kapi_memory.h        # 内存管理（KA API）
│   └── ...                  # 其他 K API / KA API 模块
├── kernel/
│   ├── api/                 # K API + KA API 实现层（45 个 .c）
│   │   ├── kapi_init.c      # 统一初始化入口
│   │   └── kapi_*.c         # 各模块实现
│   ├── arch/x86_64/         # 架构相关代码
│   │   ├── boot/            # 启动代码（boot.S、linker.ld）
│   │   ├── include/arch/    # 架构头文件
│   │   └── *.c              # 架构实现（驱动、内存、中断等）
│   ├── include/             # 内核内部头文件
│   ├── kernel/              # 核心子系统
│   │   ├── kernel.c         # 内核入口
│   │   ├── process.c        # 进程管理
│   │   ├── memory.c         # 内存管理
│   │   ├── fs.c             # 文件系统
│   │   ├── ipc.c            # 进程间通信
│   │   ├── syscall.c        # 系统调用
│   │   └── ...              # 其他子系统
│   └── lib/libc/            # 内核 C 库
├── CHANGELOG.md             # 更新日志索引
├── Makefile                 # Make 构建配置
├── build.ninja              # Ninja 构建配置
├── kernel.elf               # 编译产物
└── README.md                # 本文件
```

---

## 构建

### 依赖

- GCC 16+（x86_64 交叉编译器或 native）
- NASM 3.01+
- GNU Make 或 Ninja 1.10+

### Make 构建

```bash
make        # 构建内核
make clean  # 清理构建产物
```

### Ninja 构建（推荐，并行编译更快）

```bash
ninja -f build.ninja          # 构建内核
ninja -f build.ninja -t clean # 清理
```

### 输出

- `kernel.elf` — 内核可执行文件（约 457 KB）

---

## 版本历史

详见 `changelogs/` 目录：

| 版本 | 日期 | 主题 |
|------|------|------|
| 26.7.9K | 2026-07-09 | 骨架全面补全 + K API 46 模块 + 双层 API 架构 |
| 26.7.7.4K | 2026-07-07 | 驱动与 IPC 全面补全 |
| 26.7.7.3K | 2026-07-07 | 内核核心子系统深化 |
| 26.7.7.2K | 2026-07-07 | 大规模架构升级 |
| 26.7.7K | 2026-07-07 | 调度器 + 虚拟内存 + VFS + Ninja |
| 26.7.7K (Initial) | 2026-07-07 | KAPI 接口层 + 系统调用 |

---

## 许可证

Kenux Kernel 是独立项目，所有代码由 HGSpace 团队原创实现。
未经允许禁止私自公开内部版本。