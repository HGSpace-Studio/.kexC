# Kenux API 参考文档

Kenux API (Kapi) 是 Kenux 操作系统提供给应用开发者的系统调用接口。所有 Kex 程序通过 Kapi 与内核交互。

> 版本：2.0.0 | 目标架构：x86-64 | 调用约定：syscall (rdi, rsi, rdx, r10, r8, r9 → rax)

---

## 目录

1. [核心基础](#1-核心基础)
2. [内存管理](#2-内存管理)
3. [进程与线程](#3-进程与线程)
4. [文件系统](#4-文件系统)
5. [网络通信](#5-网络通信)
6. [图形渲染](#6-图形渲染)
7. [窗口管理](#7-窗口管理)
8. [同步原语](#8-同步原语)
9. [时间与定时器](#9-时间与定时器)
10. [I/O 操作](#10-io-操作)
11. [数据结构](#11-数据结构)
12. [安全与加密](#12-安全与加密)
13. [高级子系统](#13-高级子系统)

---

## 1. 核心基础

### 头文件

```c
#include <kapi.h>
```

### 通用返回码

| 常量 | 值 | 含义 |
|------|----|------|
| `KAPI_OK` | 0 | 成功 |
| `KAPI_ERROR` | -1 | 一般错误 |
| `KAPI_EINVAL` | -2 | 无效参数 |
| `KAPI_ENOMEM` | -3 | 内存不足 |
| `KAPI_ENOENT` | -4 | 不存在 |
| `KAPI_EACCES` | -5 | 权限不足 |
| `KAPI_EBUSY` | -6 | 资源忙 |
| `KAPI_ENOSYS` | -7 | 未实现 |
| `KAPI_EAGAIN` | -8 | 重试 |

### 字符串操作

```c
#include <kapi_string.h>

size_t kapi_strlcpy(char* dst, const char* src, size_t size);
size_t kapi_strscpy(char* dst, const char* src, size_t size);
int    kapi_snprintf(char* str, size_t size, const char* format, ...);
int    kapi_vsnprintf(char* str, size_t size, const char* format, va_list args);
```

- `kapi_strlcpy` / `kapi_strscpy`：安全字符串拷贝，保证 NUL 结尾，返回源长度
- `kapi_snprintf` / `kapi_vsnprintf`：格式化输出，保证 NUL 结尾

---

## 2. 内存管理

### 头文件

```c
#include <kapi_memory.h>
```

### 内存标志

| 标志 | 含义 |
|------|------|
| `KAPI_MEM_DEFAULT` | 默认分配 |
| `KAPI_MEM_KERNEL` | 内核空间 |
| `KAPI_MEM_USER` | 用户空间 |
| `KAPI_MEM_DMA` | DMA 可访问 |
| `KAPI_MEM_ZEROED` | 零初始化 |
| `KAPI_MEM_UNCACHED` | 不缓存 |
| `KAPI_MEM_WRITEBACK` | 写回策略 |

### 内存保护

| 标志 | 含义 |
|------|------|
| `KAPI_PROT_READ` | 可读 |
| `KAPI_PROT_WRITE` | 可写 |
| `KAPI_PROT_EXEC` | 可执行 |

### 映射标志

| 标志 | 含义 |
|------|------|
| `KAPI_MAP_SHARED` | 共享映射 |
| `KAPI_MAP_PRIVATE` | 私有映射 |
| `KAPI_MAP_ANONYMOUS` | 匿名映射 |
| `KAPI_MAP_FIXED` | 固定地址 |

### 内存信息查询

```c
kapi_mem_info_t info;
// info.total        - 物理内存总量
// info.free         - 空闲内存
// info.used         - 已用内存
// info.cached       - 缓存占用
// info.kernel_total - 内核内存总量
// info.user_total   - 用户内存总量
// info.dma_total    - DMA 内存总量
// info.slab_total   - Slab 分配器总量
```

### 常量

- `KAPI_PAGE_SIZE` = 4096（页大小）
- `KAPI_HUGE_2MB` = 2MB（大页）
- `KAPI_HUGE_1GB` = 1GB（大页）

---

## 3. 进程与线程

### 头文件

```c
#include <kapi_process.h>
#include <kapi_kthread.h>
```

### 进程状态

| 状态 | 含义 |
|------|------|
| `KAPI_PROC_DEAD` | 已终止 |
| `KAPI_PROC_READY` | 就绪 |
| `KAPI_PROC_RUNNING` | 运行中 |
| `KAPI_PROC_WAITING` | 等待 |
| `KAPI_PROC_ZOMBIE` | 僵尸 |
| `KAPI_PROC_CREATED` | 已创建 |
| `KAPI_PROC_START` | 启动中 |

### 进程优先级

| 优先级 | 含义 |
|--------|------|
| `KAPI_PRIO_IDLE` | 空闲 |
| `KAPI_PRIO_LOW` | 低 |
| `KAPI_PRIO_NORMAL` | 普通 |
| `KAPI_PRIO_HIGH` | 高 |
| `KAPI_PRIO_REALTIME` | 实时 |

### 内核线程

```c
kapi_kthread_t kapi_kthread_run(const char* name, int (*func)(void*), void* arg);
void           kapi_kthread_stop(kapi_kthread_t kt);
int            kapi_kthread_join(kapi_kthread_t kt, uint64_t* ret);
int            kapi_kthread_should_stop(void);
void           kapi_kthread_wake(kapi_kthread_t kt);
```

- `kapi_kthread_run`：创建并启动内核线程，`name` 为线程名，`func` 为入口函数，`arg` 为参数
- `kapi_kthread_stop`：请求线程停止
- `kapi_kthread_join`：等待线程结束并获取返回值
- `kapi_kthread_should_stop`：线程内检查是否应停止
- `kapi_kthread_wake`：唤醒线程

---

## 4. 文件系统

### 头文件

```c
#include <kapi_vfs.h>
#include <kapi_fs_ext.h>
```

### 文件操作

```c
int     kapi_open(const char* pathname, int flags);
int     kapi_close(int fd);
ssize_t kapi_read(int fd, void* buf, size_t count);
ssize_t kapi_write(int fd, const void* buf, size_t count);
off_t   kapi_lseek(int fd, off_t offset, int whence);
int     kapi_fsync(int fd);
int     kapi_fstat(int fd, kapi_stat_t* statbuf);
int     kapi_stat(const char* pathname, kapi_stat_t* statbuf);
```

### 打开标志

| 标志 | 含义 |
|------|------|
| `KAPI_FS_RDONLY` | 只读 |
| `KAPI_FS_WRONLY` | 只写 |
| `KAPI_FS_RDWR` | 读写 |
| `KAPI_FS_CREAT` | 不存在则创建 |
| `KAPI_FS_TRUNC` | 截断 |
| `KAPI_FS_APPEND` | 追加 |
| `KAPI_FS_NONBLOCK` | 非阻塞 |
| `KAPI_FS_DIRECTORY` | 目录 |

### 目录操作

```c
kapi_dir_t       kapi_opendir(const char* name);
kapi_dirent_t*   kapi_readdir(kapi_dir_t dir);
int              kapi_closedir(kapi_dir_t dir);
```

### 文件信息

```c
kapi_file_info_t info;
// info.is_file     - 是否为普通文件
// info.is_dir      - 是否为目录
// info.exists      - 是否存在
// info.readable    - 是否可读
// info.writable    - 是否可写
// info.size        - 文件大小
// info.mode        - 权限模式
```

### VFS 挂载

```c
kapi_mount_t* kapi_vfs_kern_mount(const char* dev, const char* dir,
                                   const char* type, uint64_t flags, void* data);
int           kapi_vfs_kern_umount(kapi_mount_t* mnt, uint64_t flags);
```

---

## 5. 网络通信

### 头文件

```c
#include <kapi_socket.h>
```

### Socket 创建与连接

```c
kapi_sockfd_t kapi_socket(int domain, int type, int protocol);
int           kapi_bind(kapi_sockfd_t sockfd, const kapi_sockaddr_t* addr, size_t addrlen);
int           kapi_listen(kapi_sockfd_t sockfd, int backlog);
kapi_sockfd_t kapi_accept(kapi_sockfd_t sockfd, kapi_sockaddr_t* addr, size_t* addrlen);
int           kapi_connect(kapi_sockfd_t sockfd, const kapi_sockaddr_t* addr, size_t addrlen);
```

### 数据收发

```c
int64_t kapi_send(kapi_sockfd_t sockfd, const void* buf, size_t len, int flags);
int64_t kapi_recv(kapi_sockfd_t sockfd, void* buf, size_t len, int flags);
int64_t kapi_sendto(kapi_sockfd_t sockfd, const void* buf, size_t len, int flags,
                    const kapi_sockaddr_t* dest_addr, size_t addrlen);
int64_t kapi_recvfrom(kapi_sockfd_t sockfd, void* buf, size_t len, int flags,
                      kapi_sockaddr_t* src_addr, size_t* addrlen);
```

### 地址族

| 常量 | 含义 |
|------|------|
| `KAPI_AF_UNIX` | Unix 域 |
| `KAPI_AF_INET` | IPv4 |

### Socket 类型

| 常量 | 含义 |
|------|------|
| `KAPI_SOCK_STREAM` | TCP 流 |
| `KAPI_SOCK_DGRAM` | UDP 数据报 |
| `KAPI_SOCK_RAW` | 原始套接字 |

### 地址结构

```c
kapi_sockaddr_in_t addr;
addr.sin_family = KAPI_AF_INET;
addr.sin_port   = htons(8080);
addr.sin_addr   = inet_addr("127.0.0.1");
```

---

## 6. 图形渲染

### 头文件

```c
#include <kapi_gfxrender.h>
```

这是 Kenux 图形应用的核心 API，提供窗口创建、事件处理和 2D 绘图。

### 颜色

```c
kgr_color_t red   = {255, 0, 0, 255};
kgr_color_t green = {0, 255, 0, 255};
kgr_color_t blue  = {0, 0, 255, 255};
kgr_color_t black = {0, 0, 0, 255};
kgr_color_t white = {255, 255, 255, 255};
```

### 像素格式

| 格式 | 含义 |
|------|------|
| `KGR_FORMAT_RGBA8888` | 32位 RGBA |
| `KGR_FORMAT_RGBX8888` | 32位 RGBX |
| `KGR_FORMAT_RGB565` | 16位 RGB |

### 窗口标志

| 标志 | 含义 |
|------|------|
| `KGR_FLAG_VSYNC` | 垂直同步 |
| `KGR_FLAG_DOUBLE_BUF` | 双缓冲 |
| `KGR_FLAG_RESIZABLE` | 可调整大小 |
| `KGR_FLAG_FULLSCREEN` | 全屏 |
| `KGR_FLAG_BORDERLESS` | 无边框 |
| `KGR_FLAG_HIDDEN` | 隐藏 |

### 创建窗口与渲染上下文

```c
kgr_window_desc_t desc = {0};
strcpy(desc.title, "我的应用");
desc.width  = 800;
desc.height = 600;
desc.format = KGR_FORMAT_RGBA8888;
desc.flags  = KGR_FLAG_DOUBLE_BUF | KGR_FLAG_RESIZABLE;
desc.background = (kgr_color_t){0, 0, 0, 255};

kgr_context_t* ctx = kgr_create(&desc);
if (!ctx) { /* 创建失败 */ }
```

### 事件处理

```c
void my_event_handler(kgr_context_t* ctx, const kgr_event_t* ev, void* ud) {
    switch (ev->type) {
    case KGR_EVENT_CLOSE:
        kgr_request_close(ctx);
        break;
    case KGR_EVENT_KEY_DOWN:
        if (ev->data.key.key == 27) /* ESC */
            kgr_request_close(ctx);
        break;
    case KGR_EVENT_MOUSE_MOVE:
        /* ev->data.mouse.x, ev->data.mouse.y */
        break;
    case KGR_EVENT_RESIZE:
        /* ev->data.resize.width, ev->data.resize.height */
        break;
    }
}

kgr_set_event_handler(ctx, my_event_handler, NULL);
```

### 事件类型

| 事件 | 含义 |
|------|------|
| `KGR_EVENT_CLOSE` | 关闭请求 |
| `KGR_EVENT_RESIZE` | 窗口大小改变 |
| `KGR_EVENT_KEY_DOWN` | 按键按下 |
| `KGR_EVENT_KEY_UP` | 按键释放 |
| `KGR_EVENT_MOUSE_MOVE` | 鼠标移动 |
| `KGR_EVENT_MOUSE_DOWN` | 鼠标按下 |
| `KGR_EVENT_MOUSE_UP` | 鼠标释放 |
| `KGR_EVENT_WHEEL` | 滚轮 |
| `KGR_EVENT_PAINT` | 重绘 |

### 绘图操作

```c
kgr_begin_frame(ctx);

kgr_clear(ctx, (kgr_color_t){0, 0, 0, 255});

kgr_draw_line(ctx, 0, 0, 800, 600, (kgr_color_t){255, 0, 0, 255});
kgr_draw_rect(ctx, 100, 100, 200, 150, (kgr_color_t){0, 255, 0, 255});
kgr_fill_rect(ctx, 300, 100, 200, 150, (kgr_color_t){0, 0, 255, 255});
kgr_draw_circle(ctx, 400, 300, 100, (kgr_color_t){255, 255, 0, 255});
kgr_fill_circle(ctx, 400, 300, 80, (kgr_color_t){255, 128, 0, 255});
kgr_fill_round_rect(ctx, 50, 50, 200, 80, 10, (kgr_color_t){64, 64, 64, 255});
kgr_draw_text(ctx, 60, 70, "Hello Kenux!", (kgr_color_t){255, 255, 255, 255});

kgr_end_frame(ctx);
kgr_present(ctx);
```

### 帧缓冲区直接访问

```c
int32_t stride;
uint32_t* fb = kgr_fb(ctx, &stride);
if (fb) {
    fb[y * stride / 4 + x] = 0xFF0000FF; /* RGBA: 红 */
}
```

### 图像绘制

```c
kgr_draw_image(ctx, x, y, w, h, png_data, png_len);
```

### 窗口控制

```c
kgr_set_title(ctx, "新标题");
kgr_resize(ctx, 1024, 768);
kgr_get_size(ctx, &w, &h);
bool closing = kgr_should_close(ctx);
kgr_request_close(ctx);
```

### 销毁

```c
kgr_destroy(ctx);
```

---

## 7. 窗口管理

### 头文件

```c
#include <kapi_window.h>
```

### 窗口类型

| 类型 | 含义 |
|------|------|
| `KAPI_WINDOW_TYPE_NORMAL` | 普通窗口 |
| `KAPI_WINDOW_TYPE_DIALOG` | 对话框 |
| `KAPI_WINDOW_TYPE_TOOLTIP` | 提示框 |
| `KAPI_WINDOW_TYPE_MENU` | 菜单 |
| `KAPI_WINDOW_TYPE_DESKTOP` | 桌面 |

### 窗口标志

| 标志 | 含义 |
|------|------|
| `KAPI_WINDOW_FLAG_VISIBLE` | 可见 |
| `KAPI_WINDOW_FLAG_BORDER` | 有边框 |
| `KAPI_WINDOW_FLAG_RESIZABLE` | 可调整大小 |
| `KAPI_WINDOW_FLAG_FULLSCREEN` | 全屏 |
| `KAPI_WINDOW_FLAG_TOPMOST` | 置顶 |
| `KAPI_WINDOW_FLAG_TRANSPARENT` | 透明 |

### 创建与销毁

```c
kapi_window_t* win = kapi_window_create("标题", x, y, w, h,
                                         KAPI_WINDOW_TYPE_NORMAL,
                                         KAPI_WINDOW_FLAG_VISIBLE | KAPI_WINDOW_FLAG_BORDER);
kapi_window_destroy(win);
```

---

## 8. 同步原语

### 头文件

```c
#include <kapi_mutex.h>
#include <kapi_completion.h>
#include <kapi_wait.h>
#include <kapi_rcu.h>
```

### 互斥锁

```c
kapi_mutex_t mtx = kapi_mutex_create();

kapi_mutex_lock(mtx);
/* 临界区 */
kapi_mutex_unlock(mtx);

int locked = kapi_mutex_trylock(mtx);
if (locked == 0) { /* 获得锁 */ kapi_mutex_unlock(mtx); }

kapi_mutex_destroy(mtx);
```

### 完成量

```c
#include <kapi_completion.h>

kapi_completion_t comp = kapi_completion_create();

/* 等待方 */
kapi_completion_wait(comp);

/* 完成方 */
kapi_completion_complete(comp);

kapi_completion_destroy(comp);
```

---

## 9. 时间与定时器

### 头文件

```c
#include <kapi_time.h>
```

### 核心函数

```c
kapi_ktime_t kapi_ktime_get(void);
int64_t      kapi_ktime_get_ns(void);
uint64_t     kapi_jiffies(void);
uint64_t     kapi_jiffies_to_ms(uint64_t j);
uint64_t     kapi_ms_to_jiffies(uint64_t ms);
```

- `kapi_ktime_get`：获取内核时间（纳秒精度）
- `kapi_ktime_get_ns`：获取当前纳秒时间戳
- `kapi_jiffies`：获取系统滴答数

### 系统信息

```c
kapi_system_info_t info;
// info.kernel_name    - 内核名称
// info.kernel_version - 内核版本
// info.version_major  - 主版本号
// info.version_minor  - 次版本号
```

---

## 10. I/O 操作

### 头文件

```c
#include <kapi_io.h>
```

### 端口 I/O

```c
uint8_t  val8  = kapi_inb(port);
uint16_t val16 = kapi_inw(port);
uint32_t val32 = kapi_inl(port);

kapi_outb(port, val8);
kapi_outw(port, val16);
kapi_outl(port, val32);
```

### 内存映射 I/O

```c
uint8_t  v8  = kapi_read8(addr);
uint16_t v16 = kapi_read16(addr);
uint32_t v32 = kapi_read32(addr);
uint64_t v64 = kapi_read64(addr);

kapi_write8(addr, v8);
kapi_write32(addr, v32);

kapi_iomap_t* map = kapi_ioremap(phys_addr, size, KAPI_IO_MEM_WRITEBACK);
kapi_iounmap(map);
```

---

## 11. 数据结构

### 链表

```c
#include <kapi_list.h>

struct my_node {
    int value;
    kapi_list_node_t node;
};

kapi_list_head_t head;
kapi_list_init(&head);

kapi_list_add(&new_node->node, &head);
kapi_list_del(&node->node);
```

### 红黑树

```c
#include <kapi_rbtree.h>

kapi_rb_root_t root = KAPI_RB_ROOT;
kapi_rb_node_t* node = kapi_rb_first(&root);
```

### 哈希表

```c
#include <kapi_hash.h>

uint32_t h = kapi_hash32(data, len, seed);
```

### FIFO

```c
#include <kapi_kfifo.h>

KAPI_DEFINE_KFIFO(fifo, uint8_t, 1024);
kapi_kfifo_in(&fifo, data, len);
size_t n = kapi_kfifo_out(&fifo, buf, bufsize);
```

### 原子操作

```c
#include <kapi_atomic.h>

kapi_atomic_t counter = KAPI_ATOMIC_INIT(0);
kapi_atomic_inc(&counter);
int val = kapi_atomic_read(&counter);
```

### 位图

```c
#include <kapi_bitmap.h>

kapi_bitmap_t bm[8];
kapi_bitmap_set(bm, bit_index);
int set = kapi_bitmap_test(bm, bit_index);
kapi_bitmap_clear(bm, bit_index);
```

---

## 12. 安全与加密

### 头文件

```c
#include <kapi_security.h>
#include <kapi_crypto_fs.h>
```

- `kapi_security.h`：安全策略、能力模型、访问控制
- `kapi_crypto_fs.h`：加密文件系统支持
- `kapi_full_disk_encryption.h`：全盘加密

---

## 13. 高级子系统

| 头文件 | 子系统 |
|--------|--------|
| `kapi_ebpf.h` | eBPF 程序与映射 |
| `kapi_kprobe.h` | 内核探针 |
| `kapi_ftrace.h` | 函数追踪 |
| `kapi_netlink.h` | Netlink 通信 |
| `kapi_container.h` | 容器化支持 |
| `kapi_namespace.h` | 命名空间 |
| `kapi_lvm.h` | 逻辑卷管理 |
| `kapi_raid.h` | RAID 管理 |
| `kapi_wireless.h` | 无线网络 |
| `kapi_vpn.h` | VPN 隧道 |
| `kapi_cpufreq.h` | CPU 频率调节 |
| `kapi_iommu.h` | IOMMU 管理 |
| `kapi_sandbox.h` | 沙箱隔离 |
| `kapi_profiler.h` | 性能分析 |
| `kapi_memleak.h` | 内存泄漏检测 |
| `kapi_logging.h` | 日志系统 |
| `kapi_trace.h` | 追踪框架 |
| `kapi_kanvasui.h` | KanvasUI 界面框架 |
| `kapi_clipboard.h` | 剪贴板 |
| `kapi_dragdrop.h` | 拖放 |
| `kapi_cursor.h` | 光标管理 |
| `kapi_ime.h` | 输入法 |
| `kapi_inotify.h` | 文件监控 |
| `kapi_device_manager.h` | 设备管理器 |
| `kapi_debugger.h` | 调试器接口 |
| `kapi_virt_ext.h` | 虚拟化扩展 |
| `kapi_net_ext.h` | 网络扩展 |
| `kapi_sync_ext.h` | 同步扩展 |
| `kapi_render_ext.h` | 渲染扩展 |
| `kapi_leonos.h` | LeonOS 兼容层 |
| `kapi_unified_syscall.h` | 统一系统调用 |
| `kapi_unified_log.h` | 统一日志 |
| `kapi_sysinfo.h` | 系统信息 |
| `kapi_signalfd.h` | 信号文件描述符 |
| `kapi_timerfd.h` | 定时器文件描述符 |
| `kapi_eventfd.h` | 事件文件描述符 |
| `kapi_epoll.h` | epoll I/O 多路复用 |
| `kapi_poll.h` | poll I/O 多路复用 |