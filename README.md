# Kex - Kenux 编译器

Kex 是 Kenux 专属的 C/C++ 编译工具链，面向 Kenux App 开发者。支持将 C/C++ 源码编译为 Kenux 可执行文件 (.kex)、动态库 (.kxp) 或原生 ELF，内置图形渲染支持，一套代码同时支持 Linux 和 Windows。

## 特性

- 多源文件编译：多个 .c / .cpp 文件一起编译为 kex / kxp / elf
- 三种输出格式：`.kex`（Kenux 可执行）、`.kxp`（Kenux 动态库）、`.elf`（原生 ELF）
- 图形渲染：编译含图形 API 的程序时自动开启窗口支持
- 构建系统：`KenuxBuild` 文件描述项目，类似 CMake / Ninja
- 跨平台：同一套代码在 Linux 和 Windows 上编译运行
- 系统安装：`kex install` 一键安装到 PATH，终端随处可用

## 快速开始

### 编译

```bash
# 安装到系统
sudo kex install

# 编译单个文件为 kex
kex hello.c -o hello.kex

# 编译多个文件为 kex
kex main.c utils.c render.c -o myapp.kex --name myapp

# 编译为 kxp 动态库
kex lib.c --shared -o mylib.kxp

# 编译为原生 ELF
kex tool.c --format elf -o tool
```

### 运行

```bash
# 运行 kex 程序
kex run hello.kex

# 运行带图形窗口的程序
kex run myapp.kex --gfx --gfx-size 1024x768
```

### 构建系统

```bash
# 初始化项目
kex init

# 构建所有目标
kex build

# 构建指定目标
kex build --target myapp

# 清理
kex clean
```

`KenuxBuild` 文件示例：

```
project(myapp)

set(CC gcc)
set(CXX g++)
set(OPT 2)

include_dir(./include)

target(myapp)
  sources(main.c utils.c)
  format(kex)
  output(myapp.kex)

target(mylib)
  sources(lib.c)
  format(kxp)
  output(mylib.kxp)
  shared()

target(native_tool)
  sources(tool.c)
  format(elf)
  output(tool)
```

### 安装 / 卸载

```bash
# 安装到系统 PATH（Linux 默认 /usr/local/bin，Windows 默认 C:\Kenux）
sudo kex install

# 安装到指定目录
kex install --prefix /opt/kenux

# 卸载
sudo kex uninstall
```

## 编译选项

| 选项 | 说明 |
|------|------|
| `-o <路径>` | 输出文件 |
| `--format <格式>` | 输出格式：kex / kxp / elf（默认 kex） |
| `--name <名称>` | 程序名（嵌入 kex 头部） |
| `--shared` | 构建共享库 (.kxp) |
| `-O<级别>` | 优化级别 0/1/2/3/s |
| `-g` | 包含调试信息 |
| `-s` | 剥离符号 |
| `-static` | 静态链接 |
| `-I<目录>` | 头文件目录 |
| `-L<目录>` | 库目录 |
| `-D<定义>` | 预处理器定义 |
| `-l<库>` | 链接库 |
| `--cc <路径>` | 指定 C 编译器 |
| `--cxx <路径>` | 指定 C++ 编译器 |
| `--verbose` | 详细输出 |
| `--keep-temps` | 保留临时文件 |

## KEX 文件格式

Kex 编译器生成的 `.kex` 文件遵循 HGS 格式规范，Kenux 内核可直接加载执行：

- 魔数：`F0 A5 48 47`
- 版本：`0x5300`
- 头部：80 字节固定头部，含代码/数据/导入导出/重定位信息
- 代码区：以 `Kenux---` 开始，`KKKSTOP` 结束的 x86-64 机器码
- 导入表：CRC32 哈希 + 函数名，加载时回填 Kenux API 地址
- 重定位：支持跨模块动态链接和段内 .rodata 引用修补

## 图形渲染

使用 Kenux 图形 API 的程序编译时会自动检测图形依赖，运行时打开独立窗口：

```c
#include <kapi_gfxrender.h>

int main() {
    kgr_window_desc_t desc = {0};
    strcpy(desc.title, "My App");
    desc.width = 800;
    desc.height = 600;
    desc.format = KGR_FORMAT_RGBA8888;
    desc.flags = KGR_FLAG_RESIZABLE;

    kgr_context_t* ctx = kgr_create(&desc);
    while (!ctx->should_close) {
        kgr_begin_frame(ctx);
        kgr_clear(ctx, (kgr_color_t){0, 0, 0, 255});
        kgr_draw_rect(ctx, 100, 100, 200, 150, (kgr_color_t){255, 0, 0, 255});
        kgr_end_frame(ctx);
        kgr_present(ctx);
    }
    kgr_destroy(ctx);
    return 0;
}
```

## 项目结构

```
kexC/
├── include/
│   ├── kex.h              编译器配置定义
│   ├── kex_format.h       KEX/KXP 文件格式定义
│   ├── kex_api.h          Kenux API 映射
│   ├── kex_crc32.h        CRC32 工具
│   └── kapi_gfxrender.h   图形渲染 API
├── src/
│   └── kex.c              编译器主程序
├── bin/                    编译输出
├── Makefile
└── README.md
```

## 编译 Kex 自身

```bash
make            # Linux
make win32      # Windows 交叉编译
make clean      # 清理
```

## 依赖

- GCC 或 Clang（C11 标准）
- Make
- 目标系统需有 C/C++ 编译器（gcc/g++）用于编译用户代码
- Linux 图形程序需要 X11 开发库
- Windows 图形程序需要 GDI32 / User32

## License

Kenux Project