/*
 * kex.c - KexKit 工具链合并单文件
 *
 * 本文件由以下源文件合并而成:
 *   头文件: kex_compat.h, kex_format.h, kex_crc32.h, kex_api.h, kex_loader.h
 *   源文件: kex_crc32.c, kex_api_table.c, kex_loader.c, kexc.c, kex_interp.c, kex_main.c
 *
 * 合并规则: 系统头文件统一去重置于顶部, 项目头文件内联,
 *           去掉 KEX_STANDALONE 独立 main 块和前向声明.
 *
 * 用法: kex compile <input.c> <output.kex> [--name N] [--kxp]
 *       kex run <program.kex> [-L<libdir>] [args...]
 *       kex version | help
 */

/* ---- 系统头文件 (统一, 去重) ---- */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif


/* ============================================================
 * kex_compat.h - Windows/Linux 平台兼容层 (内联)
 * ============================================================ */
/*
 * kex_compat.h - Windows/Linux 平台兼容层
 *
 * Linux: 直接使用 POSIX API (mmap, mprotect, dirent)
 * Windows: 用 VirtualAlloc 等 API 模拟 POSIX 接口
 */
#ifdef _WIN32
/* ===== Windows ===== */
/* ---- mmap / mprotect / munmap 兼容 ---- */
#define PROT_READ    0x1
#define PROT_WRITE   0x2
#define PROT_EXEC    0x4

#define MAP_PRIVATE    0x02
#define MAP_ANONYMOUS  0x20
#define MAP_FAILED     ((void *)(intptr_t)-1)

static inline DWORD _prot_to_win(int prot) {
    if (prot & PROT_EXEC) {
        if (prot & PROT_WRITE) return PAGE_EXECUTE_READWRITE;
        if (prot & PROT_READ)  return PAGE_EXECUTE_READ;
        return PAGE_NOACCESS;
    }
    if (prot & PROT_WRITE) return PAGE_READWRITE;
    if (prot & PROT_READ)  return PAGE_READONLY;
    return PAGE_NOACCESS;
}

static inline void *mmap(void *addr, size_t length, int prot, int flags,
                         int fd, long long offset) {
    (void)fd; (void)offset; (void)addr; (void)flags;
    void *p = VirtualAlloc(NULL, length, MEM_COMMIT | MEM_RESERVE,
                           _prot_to_win(prot));
    return p ? p : MAP_FAILED;
}

static inline int mprotect(void *addr, size_t len, int prot) {
    DWORD old;
    return VirtualProtect(addr, len, _prot_to_win(prot), &old) ? 0 : -1;
}

static inline int munmap(void *addr, size_t length) {
    (void)length;
    return VirtualFree(addr, 0, MEM_RELEASE) ? 0 : -1;
}

/* ---- 低地址内存分配 (替代 MAP_32BIT, 保证地址 < 4GB) ---- */
static inline void *mmap_low(size_t length, int prot) {
    /* 从 0x00010000 开始向上尝试, 找到第一个可用的低 4GB 地址 */
    for (uintptr_t addr = 0x00010000; addr < 0x100000000ULL; addr += 0x00010000) {
        void *p = VirtualAlloc((void *)addr, length,
                               MEM_COMMIT | MEM_RESERVE, _prot_to_win(prot));
        if (p) return p;
    }
    return MAP_FAILED;
}

/* ---- dirent 兼容 (mingw-w64 自带 dirent.h, 但部分版本可能缺失) ---- */
/* mingw-w64 提供 <dirent.h>, 直接包含即可 */
#else
/* ===== Linux ===== */
/* Linux 上 mmap_low 用 MAP_32BIT */
static inline void *mmap_low(size_t length, int prot) {
    return mmap(NULL, length, prot,
                MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
}
#endif


/* ============================================================
 * kex_format.h - KEX/KXP HGS 文件格式定义 (内联)
 * ============================================================ */
/*
 * kex_format.h - KEX/KXP HGS 文件格式定义
 *
 * 对标 KenuxOS kapi_syscall.h (x86_64, syscall 调用约定:
 *   rdi, rsi, rdx, r10, r8, r9; 返回 rax; syscall 号放 rax)
 *
 * 文件类型:
 *   .kex - KenuxOS 主程序 (可执行)
 *   .kxp - KenuxOS 动态加载库 (共享库)
 *
 * 文件布局 (所有多字节字段小端):
 *
 *   偏移    大小   字段
 *   ------|------|------------------------------------------
 *   0x00   4      魔数 F0 A5 48 47 ("HGS" 格式)
 *   0x04   2      版本号 (0x5300, 保留 'S' 标识)
 *   0x06   2      头部大小 (固定 80 = 0x0050)
 *   0x08   4      程序总大小 (含所有段)
 *   0x0C   4      代码入口偏移 (相对文件头)
 *   0x10   4      代码大小 (从 Kenux--- 之后到 KKKSTOP 之前)
 *   0x14   4      导入表偏移 (相对文件头)
 *   0x18   4      导入表条目数量
 *   0x1C   4      导出表偏移 (相对文件头)
 *   0x20   4      导出表条目数量
 *   0x24   4      图标偏移 (相对文件头)
 *   0x28   4      图标大小 (PNG 字节数)
 *   0x2C   4      栈大小 (默认 1MB)
 *   0x30   4      堆大小 (默认 1MB)
 *   0x34   4      CRC32 校验和 (整个文件, 此字段按 0 计算)
 *   0x38   2      文件类型 (0=KEX 主程序, 1=KXP 动态库)
 *   0x3A   2      标志位 (bit0: 含图标, bit1: 含资源, bit2: PIE)
 *   0x3C   4      重定位表偏移 (0=无重定位; 非0指向重定位区)
 *   0x40   4      .rodata 偏移 (相对文件头; 0=无只读数据段)
 *   0x44   4      .rodata 大小 (字节数)
 *   0x48   4      本地重定位表偏移 (0=无; 段内地址修补表)
 *   0x4C   4      本地重定位条目数
 *   0x50  16      程序名 (UTF-8, 不足补 0)
 *   0x60   8      机器码开始符 "Kenux---"
 *   0x68   N      x86-64 机器码
 *   ...    7      程序结束符 "KKKSTOP"
 *   ...   变长    .rodata 段 (只读字符串/常量数据)
 *   ...   变长    导入表 (每条: CRC32 + 地址占位 + 函数名\0)
 *   ...   变长    导出表 (每条: 函数名\0 + 代码偏移)
 *   ...   变长    重定位表 (4字节条目数 + 每条8字节: 代码偏移+CRC32)
 *   ...   变长    本地重定位表 (每条8字节: patch偏移+目标符号值)
 *   ...   变长    资源区 (图标 PNG / 音频 / 图像 / 字体)
 *
 * 本地重定位表: 用于修补代码中引用 .rodata 的 RIP 相对地址.
 *   call_off 是 lea/mov 指令中 disp32 操作数的文件偏移
 *   sym_value 是 .rodata 段中的偏移 (相对 .rodata 起始)
 * 加载时: target_addr = rodata_base + sym_value;
 *         修补 disp32 = target_addr - (patch_off + 4)
 *
 * 注: 头部已扩展为 80 字节, 以容纳 .rodata 与本地重定位字段.
 *
 * 重定位表用于动态链接: .kex 调用 .kxp 导出函数时, call 指令的
 * 目标地址在加载时由解释器回填. 每条记录: 代码区内 call 指令的偏移
 * (4字节) + 目标符号名 CRC32 (4字节).
 */
#ifdef __cplusplus
extern "C" {
#endif

/* ---- 魔数与版本 ---- */
#define KEX_MAGIC0      0xF0
#define KEX_MAGIC1      0xA5
#define KEX_MAGIC2      0x48
#define KEX_MAGIC3      0x47
#define KEX_MAGIC       0x4748A5F0u   /* 小端读取: F0 A5 48 47 */

#define KEX_VERSION     0x5300u       /* 保留 'S' 标识 */

#define KEX_HEADER_SIZE 80u           /* 核心头部固定 80 字节 (扩展后) */
#define KEX_NAME_LEN    16u           /* 程序名长度 */

/* ---- 代码区标记 ---- */
#define KEX_CODE_START  "Kenux---"    /* 8 字节机器码开始符 */
#define KEX_CODE_END    "KKKSTOP"     /* 7 字节程序结束符 */
#define KEX_CODE_START_LEN 8u
#define KEX_CODE_END_LEN   7u

/* ---- 文件类型 ---- */
#define KEX_FILE_KEX    0u            /* 主程序 (可执行) */
#define KEX_FILE_KXP    1u            /* 动态加载库 */

/* ---- 标志位 ---- */
#define KEX_FLAG_ICON   0x0001u       /* 含图标 */
#define KEX_FLAG_RES    0x0002u       /* 含资源区 */
#define KEX_FLAG_PIE    0x0004u       /* 位置无关 */

/* ---- 默认内存参数 ---- */
#define KEX_DEFAULT_STACK (1u * 1024u * 1024u)  /* 1MB */
#define KEX_DEFAULT_HEAP  (1u * 1024u * 1024u)  /* 1MB */

#pragma pack(push, 1)

/* 核心头部 (80 字节) */
typedef struct {
    uint8_t  magic[4];        /* 0x00: F0 A5 48 47 */
    uint16_t version;         /* 0x04: 0x5300 */
    uint16_t header_size;     /* 0x06: 80 */
    uint32_t total_size;      /* 0x08: 程序总大小 */
    uint32_t entry_offset;    /* 0x0C: 代码入口偏移 (相对文件头) */
    uint32_t code_size;       /* 0x10: 代码字节数 */
    uint32_t import_offset;   /* 0x14: 导入表偏移 */
    uint32_t import_count;    /* 0x18: 导入条目数 */
    uint32_t export_offset;   /* 0x1C: 导出表偏移 */
    uint32_t export_count;    /* 0x20: 导出条目数 */
    uint32_t icon_offset;     /* 0x24: 图标偏移 */
    uint32_t icon_size;       /* 0x28: 图标大小 */
    uint32_t stack_size;      /* 0x2C: 栈大小 */
    uint32_t heap_size;       /* 0x30: 堆大小 */
    uint32_t crc32;           /* 0x34: CRC32 */
    uint16_t file_type;       /* 0x38: 0=KEX 1=KXP */
    uint16_t flags;           /* 0x3A: 标志位 */
    uint32_t reloc_offset;    /* 0x3C: 重定位表偏移 (0=无) */
    uint32_t rodata_offset;   /* 0x40: .rodata 偏移 (0=无) */
    uint32_t rodata_size;     /* 0x44: .rodata 大小 */
    uint32_t local_reloc_off; /* 0x48: 本地重定位表偏移 (0=无) */
    uint32_t local_reloc_cnt; /* 0x4C: 本地重定位条目数 */
} kex_header_t;               /* sizeof = 80 (无填充, #pragma pack(1)) */

/* 程序名区 (紧跟头部, 16 字节, 位于偏移 0x50)
 * 注: 头部 80(0x50) + 程序名 16(0x10) = 0x60 = 机器码开始符偏移 */
typedef struct {
    char name[KEX_NAME_LEN];  /* UTF-8, 不足补 0 */
} kex_name_t;

/* 导入表条目 (变长):
 *   4 字节 API 名称 CRC32 哈希
 *   4 字节 API 地址占位 (编译时为 0, 加载时回填实现地址)
 *   N 字节 函数名字符串 (0 结尾)
 * 注意: 条目变长, 遍历时按字符串结尾推进. */
typedef struct {
    uint32_t api_crc32;       /* API 名称的 CRC32 哈希 */
    uint32_t api_addr;        /* 加载时回填的实现地址 (或 stub 偏移) */
    /* char name[];  紧跟 0 结尾的函数名 */
} kex_import_entry_t;

/* 导出表条目 (变长):
 *   N 字节 函数名字符串 (0 结尾)
 *   4 字节 函数代码偏移 (相对文件头)
 * 用于 .kxp 动态库向其他程序导出符号. */
typedef struct {
    /* char name[];  0 结尾的函数名 */
    /* uint32_t code_offset;  相对文件头 */
} kex_export_entry_t;

/* 重定位表条目 (固定 8 字节):
 *   4 字节 代码区内 call 指令的操作数偏移 (相对文件头)
 *   4 字节 目标符号名的 CRC32
 * 加载时: 根据 CRC32 找到目标函数实际地址, 回填到 call 指令的操作数.
 * x86_64 call rel32 指令: E8 xx xx xx xx, 操作数 = 目标 - (call指令+5)
 */
#pragma pack(push, 1)
typedef struct {
    uint32_t call_off;    /* 代码区内 call rel32 操作数的文件偏移 */
    uint32_t sym_crc32;   /* 目标符号 CRC32 */
} kex_reloc_entry_t;
#pragma pack(pop)

/* 本地重定位条目 (固定 8 字节, 用于段内地址修补):
 *   4 字节 代码区内 disp32 操作数的文件偏移 (RIP 相对寻址)
 *   4 字节 目标符号在 .rodata 段中的偏移 (相对 .rodata 起始)
 * 加载时: 实际地址 = rodata_base + sym_value;
 *         修补: *(int32_t*)(code+patch_off) = 实际地址 - (patch_off+4 - code_base)
 * 用于 lea/mov 指令引用 .rodata 中的字符串/常量.
 */
#pragma pack(push, 1)
typedef struct {
    uint32_t patch_off;   /* 代码区内 disp32 操作数的文件偏移 */
    uint32_t sym_value;   /* 目标符号在 .rodata 中的偏移 */
} kex_local_reloc_t;
#pragma pack(pop)

#pragma pack(pop)

/* 编译期断言: 头部必须 80 字节 */
static inline void kex_header_size_check(void) {
    /* 借助数组大小为负的旧技巧做静态断言 */
    typedef char kex_header_size_must_be_80[
        (sizeof(kex_header_t) == KEX_HEADER_SIZE) ? 1 : -1
    ];
    (void)sizeof(kex_header_size_must_be_80);
}

/* ---- 错误码 ---- */
#define KEX_OK              0
#define KEX_ERR_MAGIC      -1
#define KEX_ERR_VERSION    -2
#define KEX_ERR_SIZE       -3
#define KEX_ERR_CRC        -4
#define KEX_ERR_FORMAT     -5
#define KEX_ERR_NO_CODE    -6
#define KEX_ERR_NO_MEM     -7
#define KEX_ERR_IMPORT     -8
#define KEX_ERR_NOENTRY    -9
#define KEX_ERR_IO        -10
#define KEX_ERR_RELOC     -11   /* 重定位失败 */
#define KEX_ERR_EXPORT    -12   /* 导出表解析失败 */
#define KEX_ERR_RODATA    -13   /* .rodata 段解析失败 */
#define KEX_ERR_LRELOC    -14   /* 本地重定位失败 */

/* ---- 内联工具 ---- */
/* 读取程序名区 (位于 header_size 偏移处) */
static inline const char *kex_get_name(const uint8_t *buf) {
    return (const char *)(buf + KEX_HEADER_SIZE);
}

/* 代码区起始偏移 = 头部(80) + 程序名(16) + 开始符(8) */
#define KEX_CODE_REGION_OFFSET (KEX_HEADER_SIZE + KEX_NAME_LEN + KEX_CODE_START_LEN)

/* 计算代码区实际起始 (入口机器码第一字节) */
static inline uint32_t kex_code_base(const kex_header_t *h) {
    (void)h;
    return KEX_CODE_REGION_OFFSET;
}

#ifdef __cplusplus
}
#endif


/* ============================================================
 * kex_crc32.h - CRC32 工具 (声明, 内联)
 * ============================================================ */
/*
 * kex_crc32.h - CRC32 工具
 *
 * 用途:
 *   1. 计算 API 名称的 CRC32 哈希 (导入表匹配)
 *   2. 计算整个 KEX 文件的 CRC32 校验和 (头部 crc32 字段)
 *
 * 采用 IEEE 802.3 多项式 0xEDB88320 (与 zlib / PNG 一致),
 * 初值 0xFFFFFFFF, 异或输出 0xFFFFFFFF.
 */
#ifdef __cplusplus
extern "C" {
#endif

/* 一次性计算 buffer 的 CRC32 */
uint32_t kex_crc32(const void *buf, size_t len);

/* 增量计算: 先 init, 多次 update, 最后 finalize */
uint32_t kex_crc32_init(void);
uint32_t kex_crc32_update(uint32_t crc, const void *buf, size_t len);
uint32_t kex_crc32_finalize(uint32_t crc);

/* 计算以 0 结尾字符串的 CRC32 (用于 API 名称哈希) */
uint32_t kex_crc32_str(const char *s);

#ifdef __cplusplus
}
#endif


/* ============================================================
 * kex_crc32.c - CRC32 (IEEE 802.3) 实现
 * ============================================================ */
/*
 * kex_crc32.c - CRC32 (IEEE 802.3) 实现
 */
static uint32_t crc_table[256];
static int crc_table_inited = 0;

static void crc_table_init(void) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++) {
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        }
        crc_table[i] = c;
    }
    crc_table_inited = 1;
}

uint32_t kex_crc32_init(void) {
    if (!crc_table_inited) crc_table_init();
    return 0xFFFFFFFFu;
}

uint32_t kex_crc32_update(uint32_t crc, const void *buf, size_t len) {
    const uint8_t *p = (const uint8_t *)buf;
    if (!crc_table_inited) crc_table_init();
    for (size_t i = 0; i < len; i++) {
        crc = crc_table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc;
}

uint32_t kex_crc32_finalize(uint32_t crc) {
    return crc ^ 0xFFFFFFFFu;
}

uint32_t kex_crc32(const void *buf, size_t len) {
    uint32_t crc = kex_crc32_init();
    crc = kex_crc32_update(crc, buf, len);
    return kex_crc32_finalize(crc);
}

uint32_t kex_crc32_str(const char *s) {
    uint32_t crc = kex_crc32_init();
    while (*s) {
        crc = crc_table[(crc ^ (uint8_t)*s) & 0xFF] ^ (crc >> 8);
        s++;
    }
    return kex_crc32_finalize(crc);
}


/* ============================================================
 * kex_api.h - Kenux API 映射 (声明, 内联)
 * ============================================================ */
/*
 * kex_api.h - Kenux API 映射 (对标 KenuxOS kapi_syscall.h)
 *
 * 作用:
 *   .kex 导入表里记录的是 API 名称的 CRC32 哈希. 加载器/解释器需要根据
 *   CRC32 反查到对应的 syscall 编号, 再生成 syscall stub 回填到导入表.
 *
 *   本头定义 API 条目结构与查询接口, 实现在 kex_api_table.c.
 *
 * KenuxOS x86_64 syscall 调用约定 (来自 kapi_syscall.h):
 *   - syscall 号: rax
 *   - 参数:       rdi, rsi, rdx, r10, r8, r9
 *   - 返回值:     rax
 *   - clobber:    rcx, r11
 */
#ifdef __cplusplus
extern "C" {
#endif

/* API 实现类型 */
#define KEX_API_SYSCALL   0   /* 直接 syscall (生成 stub: mov rax,nr; syscall; ret) */
#define KEX_API_DIRECT    1   /* 直接地址 (内核导出函数, 运行时提供) */
#define KEX_API_KENUX     2   /* Kenux 扩展 syscall (451-500) */

/* API 条目 */
typedef struct {
    const char *name;       /* API 名称, 如 "sys_write" */
    uint32_t    crc32;      /* 名称的 CRC32 (运行时填充, 表里为 0 时按需算) */
    uint16_t    syscall_nr; /* syscall 编号 */
    uint8_t     impl_type;  /* KEX_API_* */
    uint8_t     nargs;      /* 参数个数 (0-6) */
} kex_api_entry_t;

/* 按名称查找 API 条目; 找不到返回 NULL */
const kex_api_entry_t *kex_api_lookup_name(const char *name);

/* 按 CRC32 查找 API 条目; 找不到返回 NULL (加载器用) */
const kex_api_entry_t *kex_api_lookup_crc32(uint32_t crc32);

/* 获取标准 syscall 表 (sys_* 系列), 用于编译器/调试;
 * 若需获取包含 Kenux 扩展的完整表, 请用 kex_api_table_all() */
const kex_api_entry_t *kex_api_table(size_t *count);

/* 获取完整 API 表 (标准 + Kenux 扩展), 返回指向各条目的指针数组.
 * *count 接收条目总数; 返回值是指向 const kex_api_entry_t* 的数组 */
const kex_api_entry_t * const *kex_api_table_all(size_t *count);

/* 计算给定名称的 CRC32 (与导入表哈希一致) */
uint32_t kex_api_crc32(const char *name);

#ifdef __cplusplus
}
#endif


/* ============================================================
 * kex_api_table.c - Kenux API 映射表 (实现)
 * ============================================================ */
/*
 * kex_api_table.c - Kenux API 映射表 (对标 KenuxOS kapi_syscall.h)
 *
 * 覆盖:
 *   - 标准 syscall 0-450 (含新增 333-335, 424, 428-450)
 *   - Kenux 扩展 syscall 451-500 (kenux_*)
 *
 * 加载器流程: 导入表 CRC32 -> kex_api_lookup_crc32 -> syscall_nr ->
 *            生成 stub (mov eax,nr; syscall; ret) -> 回填导入地址
 */
/* 标准 syscall (有 Kenux inline 包装) */
static kex_api_entry_t kex_std_apis[] = {
    /* 文件 IO */
    { "sys_read",        0,   0, KEX_API_SYSCALL, 3 },  /* fd, buf, count */
    { "sys_write",       0,   1, KEX_API_SYSCALL, 3 },
    { "sys_open",        0,   2, KEX_API_SYSCALL, 3 },
    { "sys_close",       0,   3, KEX_API_SYSCALL, 1 },
    { "sys_stat",        0,   4, KEX_API_SYSCALL, 2 },
    { "sys_fstat",       0,   5, KEX_API_SYSCALL, 2 },
    { "sys_lstat",       0,   6, KEX_API_SYSCALL, 2 },
    { "sys_poll",        0,   7, KEX_API_SYSCALL, 3 },
    { "sys_lseek",       0,   8, KEX_API_SYSCALL, 3 },
    { "sys_mmap",        0,   9, KEX_API_SYSCALL, 6 },
    { "sys_mprotect",    0,  10, KEX_API_SYSCALL, 3 },
    { "sys_munmap",      0,  11, KEX_API_SYSCALL, 2 },
    { "sys_brk",         0,  12, KEX_API_SYSCALL, 1 },
    { "sys_rt_sigaction",0,  13, KEX_API_SYSCALL, 4 },
    { "sys_rt_sigprocmask",0,14, KEX_API_SYSCALL, 4 },
    { "sys_rt_sigreturn",0,  15, KEX_API_SYSCALL, 0 },
    { "sys_ioctl",       0,  16, KEX_API_SYSCALL, 3 },
    { "sys_pread64",     0,  17, KEX_API_SYSCALL, 4 },
    { "sys_pwrite64",    0,  18, KEX_API_SYSCALL, 4 },
    { "sys_readv",       0,  19, KEX_API_SYSCALL, 3 },
    { "sys_writev",      0,  20, KEX_API_SYSCALL, 3 },
    { "sys_access",      0,  21, KEX_API_SYSCALL, 2 },
    { "sys_pipe",        0,  22, KEX_API_SYSCALL, 1 },
    { "sys_select",      0,  23, KEX_API_SYSCALL, 5 },
    { "sys_sched_yield", 0,  24, KEX_API_SYSCALL, 0 },
    { "sys_mremap",      0,  25, KEX_API_SYSCALL, 5 },
    { "sys_msync",       0,  26, KEX_API_SYSCALL, 3 },
    { "sys_mincore",     0,  27, KEX_API_SYSCALL, 3 },
    { "sys_madvise",     0,  28, KEX_API_SYSCALL, 3 },
    { "sys_shmget",      0,  29, KEX_API_SYSCALL, 3 },
    { "sys_shmat",       0,  30, KEX_API_SYSCALL, 3 },
    { "sys_shmctl",      0,  31, KEX_API_SYSCALL, 3 },
    { "sys_dup",         0,  32, KEX_API_SYSCALL, 1 },
    { "sys_dup2",        0,  33, KEX_API_SYSCALL, 2 },
    { "sys_pause",       0,  34, KEX_API_SYSCALL, 0 },
    { "sys_nanosleep",   0,  35, KEX_API_SYSCALL, 2 },
    { "sys_getitimer",   0,  36, KEX_API_SYSCALL, 2 },
    { "sys_alarm",       0,  37, KEX_API_SYSCALL, 1 },
    { "sys_setitimer",   0,  38, KEX_API_SYSCALL, 3 },
    { "sys_getpid",      0,  39, KEX_API_SYSCALL, 0 },
    { "sys_sendfile",    0,  40, KEX_API_SYSCALL, 4 },
    /* 网络 */
    { "sys_socket",      0,  41, KEX_API_SYSCALL, 3 },
    { "sys_connect",     0,  42, KEX_API_SYSCALL, 3 },
    { "sys_accept",      0,  43, KEX_API_SYSCALL, 3 },
    { "sys_sendto",      0,  44, KEX_API_SYSCALL, 6 },
    { "sys_recvfrom",    0,  45, KEX_API_SYSCALL, 6 },
    { "sys_sendmsg",     0,  46, KEX_API_SYSCALL, 3 },
    { "sys_recvmsg",     0,  47, KEX_API_SYSCALL, 3 },
    { "sys_shutdown",    0,  48, KEX_API_SYSCALL, 2 },
    { "sys_bind",        0,  49, KEX_API_SYSCALL, 3 },
    { "sys_listen",      0,  50, KEX_API_SYSCALL, 2 },
    { "sys_getsockname", 0,  51, KEX_API_SYSCALL, 3 },
    { "sys_getpeername", 0,  52, KEX_API_SYSCALL, 3 },
    { "sys_socketpair",  0,  53, KEX_API_SYSCALL, 4 },
    { "sys_setsockopt",  0,  54, KEX_API_SYSCALL, 5 },
    { "sys_getsockopt",  0,  55, KEX_API_SYSCALL, 5 },
    /* 进程/线程 */
    { "sys_clone",       0,  56, KEX_API_SYSCALL, 5 },
    { "sys_fork",        0,  57, KEX_API_SYSCALL, 0 },
    { "sys_vfork",       0,  58, KEX_API_SYSCALL, 0 },
    { "sys_execve",      0,  59, KEX_API_SYSCALL, 3 },
    { "sys_exit",        0,  60, KEX_API_SYSCALL, 1 },
    { "sys_wait4",       0,  61, KEX_API_SYSCALL, 4 },
    { "sys_kill",        0,  62, KEX_API_SYSCALL, 2 },
    { "sys_uname",       0,  63, KEX_API_SYSCALL, 1 },
    { "sys_semget",      0,  64, KEX_API_SYSCALL, 3 },
    { "sys_semop",       0,  65, KEX_API_SYSCALL, 3 },
    { "sys_semctl",      0,  66, KEX_API_SYSCALL, 4 },
    { "sys_shmdt",       0,  67, KEX_API_SYSCALL, 1 },
    { "sys_msgget",      0,  68, KEX_API_SYSCALL, 2 },
    { "sys_msgsnd",      0,  69, KEX_API_SYSCALL, 4 },
    { "sys_msgrcv",      0,  70, KEX_API_SYSCALL, 5 },
    { "sys_msgctl",      0,  71, KEX_API_SYSCALL, 3 },
    /* 文件控制 */
    { "sys_fcntl",       0,  72, KEX_API_SYSCALL, 3 },
    { "sys_flock",       0,  73, KEX_API_SYSCALL, 2 },
    { "sys_fsync",       0,  74, KEX_API_SYSCALL, 1 },
    { "sys_fdatasync",   0,  75, KEX_API_SYSCALL, 1 },
    { "sys_truncate",    0,  76, KEX_API_SYSCALL, 2 },
    { "sys_ftruncate",   0,  77, KEX_API_SYSCALL, 2 },
    { "sys_getdents",    0,  78, KEX_API_SYSCALL, 3 },
    { "sys_getcwd",      0,  79, KEX_API_SYSCALL, 2 },
    { "sys_chdir",       0,  80, KEX_API_SYSCALL, 1 },
    { "sys_fchdir",      0,  81, KEX_API_SYSCALL, 1 },
    { "sys_rename",      0,  82, KEX_API_SYSCALL, 2 },
    { "sys_mkdir",       0,  83, KEX_API_SYSCALL, 2 },
    { "sys_rmdir",       0,  84, KEX_API_SYSCALL, 1 },
    { "sys_creat",       0,  85, KEX_API_SYSCALL, 2 },
    { "sys_link",        0,  86, KEX_API_SYSCALL, 2 },
    { "sys_unlink",      0,  87, KEX_API_SYSCALL, 1 },
    { "sys_symlink",     0,  88, KEX_API_SYSCALL, 2 },
    { "sys_readlink",    0,  89, KEX_API_SYSCALL, 3 },
    { "sys_chmod",       0,  90, KEX_API_SYSCALL, 2 },
    { "sys_fchmod",      0,  91, KEX_API_SYSCALL, 2 },
    { "sys_chown",       0,  92, KEX_API_SYSCALL, 3 },
    { "sys_fchown",      0,  93, KEX_API_SYSCALL, 3 },
    { "sys_lchown",      0,  94, KEX_API_SYSCALL, 3 },
    { "sys_umask",       0,  95, KEX_API_SYSCALL, 1 },
    { "sys_gettimeofday",0,  96, KEX_API_SYSCALL, 2 },
    { "sys_getrlimit",   0,  97, KEX_API_SYSCALL, 2 },
    { "sys_getrusage",   0,  98, KEX_API_SYSCALL, 2 },
    { "sys_sysinfo",     0,  99, KEX_API_SYSCALL, 1 },
    { "sys_times",       0, 100, KEX_API_SYSCALL, 1 },
    { "sys_ptrace",      0, 101, KEX_API_SYSCALL, 4 },
    { "sys_getuid",      0, 102, KEX_API_SYSCALL, 0 },
    { "sys_syslog",      0, 103, KEX_API_SYSCALL, 3 },
    { "sys_getgid",      0, 104, KEX_API_SYSCALL, 0 },
    { "sys_setuid",      0, 105, KEX_API_SYSCALL, 1 },
    { "sys_setgid",      0, 106, KEX_API_SYSCALL, 1 },
    { "sys_geteuid",     0, 107, KEX_API_SYSCALL, 0 },
    { "sys_getegid",     0, 108, KEX_API_SYSCALL, 0 },
    { "sys_setpgid",     0, 109, KEX_API_SYSCALL, 2 },
    { "sys_getppid",     0, 110, KEX_API_SYSCALL, 0 },
    { "sys_getpgrp",     0, 111, KEX_API_SYSCALL, 0 },
    { "sys_setsid",      0, 112, KEX_API_SYSCALL, 0 },
    { "sys_setreuid",    0, 113, KEX_API_SYSCALL, 2 },
    { "sys_setregid",    0, 114, KEX_API_SYSCALL, 2 },
    { "sys_getgroups",   0, 115, KEX_API_SYSCALL, 2 },
    { "sys_setgroups",   0, 116, KEX_API_SYSCALL, 2 },
    { "sys_setresuid",   0, 117, KEX_API_SYSCALL, 3 },
    { "sys_getresuid",   0, 118, KEX_API_SYSCALL, 3 },
    { "sys_setresgid",   0, 119, KEX_API_SYSCALL, 3 },
    { "sys_getresgid",   0, 120, KEX_API_SYSCALL, 3 },
    { "sys_getpgid",     0, 121, KEX_API_SYSCALL, 1 },
    { "sys_setfsuid",    0, 122, KEX_API_SYSCALL, 1 },
    { "sys_setfsgid",    0, 123, KEX_API_SYSCALL, 1 },
    { "sys_getsid",      0, 124, KEX_API_SYSCALL, 1 },
    { "sys_capget",      0, 125, KEX_API_SYSCALL, 2 },
    { "sys_capset",      0, 126, KEX_API_SYSCALL, 2 },
    { "sys_rt_sigpending",0,127, KEX_API_SYSCALL, 2 },
    { "sys_rt_sigtimedwait",0,128,KEX_API_SYSCALL, 3 },
    { "sys_rt_sigqueueinfo",0,129,KEX_API_SYSCALL, 3 },
    { "sys_rt_sigsuspend",0,130, KEX_API_SYSCALL, 2 },
    { "sys_sigaltstack", 0, 131, KEX_API_SYSCALL, 2 },
    { "sys_utime",       0, 132, KEX_API_SYSCALL, 2 },
    { "sys_mknod",       0, 133, KEX_API_SYSCALL, 3 },
    /* 系统配置 */
    { "sys_personality", 0, 135, KEX_API_SYSCALL, 1 },
    { "sys_ustat",       0, 136, KEX_API_SYSCALL, 2 },
    { "sys_statfs",      0, 137, KEX_API_SYSCALL, 2 },
    { "sys_fstatfs",     0, 138, KEX_API_SYSCALL, 2 },
    { "sys_sysfs",       0, 139, KEX_API_SYSCALL, 3 },
    { "sys_getpriority", 0, 140, KEX_API_SYSCALL, 2 },
    { "sys_setpriority", 0, 141, KEX_API_SYSCALL, 3 },
    { "sys_sched_setparam",    0, 142, KEX_API_SYSCALL, 2 },
    { "sys_sched_getparam",    0, 143, KEX_API_SYSCALL, 2 },
    { "sys_sched_setscheduler",0, 144, KEX_API_SYSCALL, 3 },
    { "sys_sched_getscheduler",0, 145, KEX_API_SYSCALL, 1 },
    { "sys_sched_get_priority_max", 0, 146, KEX_API_SYSCALL, 1 },
    { "sys_sched_get_priority_min", 0, 147, KEX_API_SYSCALL, 1 },
    { "sys_sched_rr_get_interval",  0, 148, KEX_API_SYSCALL, 2 },
    { "sys_mlock",       0, 149, KEX_API_SYSCALL, 2 },
    { "sys_munlock",     0, 150, KEX_API_SYSCALL, 2 },
    { "sys_mlockall",    0, 151, KEX_API_SYSCALL, 1 },
    { "sys_munlockall",  0, 152, KEX_API_SYSCALL, 0 },
    /* 系统/资源 */
    { "sys_prctl",       0, 157, KEX_API_SYSCALL, 5 },
    { "sys_arch_prctl",  0, 158, KEX_API_SYSCALL, 2 },
    { "sys_adjtimex",    0, 159, KEX_API_SYSCALL, 1 },
    { "sys_setrlimit",   0, 160, KEX_API_SYSCALL, 2 },
    { "sys_chroot",      0, 161, KEX_API_SYSCALL, 1 },
    { "sys_sync",        0, 162, KEX_API_SYSCALL, 0 },
    { "sys_acct",        0, 163, KEX_API_SYSCALL, 1 },
    { "sys_settimeofday",0, 164, KEX_API_SYSCALL, 2 },
    { "sys_mount",       0, 165, KEX_API_SYSCALL, 5 },
    { "sys_umount2",     0, 166, KEX_API_SYSCALL, 2 },
    { "sys_swapon",      0, 167, KEX_API_SYSCALL, 2 },
    { "sys_swapoff",     0, 168, KEX_API_SYSCALL, 1 },
    { "sys_reboot",      0, 169, KEX_API_SYSCALL, 4 },
    { "sys_sethostname", 0, 170, KEX_API_SYSCALL, 2 },
    { "sys_setdomainname",0,171, KEX_API_SYSCALL, 2 },
    { "sys_iopl",        0, 172, KEX_API_SYSCALL, 1 },
    { "sys_ioperm",      0, 173, KEX_API_SYSCALL, 3 },
    { "sys_init_module", 0, 175, KEX_API_SYSCALL, 3 },
    { "sys_delete_module",0,176, KEX_API_SYSCALL, 2 },
    { "sys_quotactl",    0, 179, KEX_API_SYSCALL, 4 },
    /* 扩展属性 */
    { "sys_setxattr",    0, 188, KEX_API_SYSCALL, 5 },
    { "sys_lsetxattr",   0, 189, KEX_API_SYSCALL, 5 },
    { "sys_fsetxattr",   0, 190, KEX_API_SYSCALL, 5 },
    { "sys_getxattr",    0, 191, KEX_API_SYSCALL, 4 },
    { "sys_lgetxattr",   0, 192, KEX_API_SYSCALL, 4 },
    { "sys_fgetxattr",   0, 193, KEX_API_SYSCALL, 4 },
    { "sys_listxattr",   0, 194, KEX_API_SYSCALL, 3 },
    { "sys_llistxattr",  0, 195, KEX_API_SYSCALL, 3 },
    { "sys_flistxattr",  0, 196, KEX_API_SYSCALL, 3 },
    { "sys_removexattr", 0, 197, KEX_API_SYSCALL, 2 },
    { "sys_lremovexattr",0, 198, KEX_API_SYSCALL, 2 },
    { "sys_fremovexattr",0, 199, KEX_API_SYSCALL, 2 },
    /* 进程/线程扩展 */
    { "sys_gettid",      0, 186, KEX_API_SYSCALL, 0 },
    { "sys_readahead",   0, 187, KEX_API_SYSCALL, 3 },
    { "sys_tkill",       0, 200, KEX_API_SYSCALL, 2 },
    { "sys_time",        0, 201, KEX_API_SYSCALL, 1 },
    { "sys_futex",       0, 202, KEX_API_SYSCALL, 6 },
    { "sys_sched_setaffinity", 0, 203, KEX_API_SYSCALL, 3 },
    { "sys_sched_getaffinity", 0, 204, KEX_API_SYSCALL, 3 },
    /* IO */
    { "sys_io_setup",    0, 206, KEX_API_SYSCALL, 2 },
    { "sys_io_destroy",  0, 207, KEX_API_SYSCALL, 1 },
    { "sys_io_getevents",0, 208, KEX_API_SYSCALL, 5 },
    { "sys_io_submit",   0, 209, KEX_API_SYSCALL, 3 },
    { "sys_io_cancel",   0, 210, KEX_API_SYSCALL, 3 },
    /* epoll */
    { "sys_epoll_create",0, 213, KEX_API_SYSCALL, 1 },
    { "sys_epoll_ctl_old",0,214, KEX_API_SYSCALL, 4 },
    { "sys_epoll_wait_old",0,215,KEX_API_SYSCALL, 4 },
    { "sys_remap_file_pages",0,216,KEX_API_SYSCALL, 5 },
    { "sys_getdents64",  0, 217, KEX_API_SYSCALL, 3 },
    { "sys_set_tid_address",0,218,KEX_API_SYSCALL, 1 },
    { "sys_restart_syscall",0,219,KEX_API_SYSCALL, 0 },
    { "sys_semtimedop",  0, 220, KEX_API_SYSCALL, 4 },
    { "sys_fadvise64",   0, 221, KEX_API_SYSCALL, 4 },
    /* 定时器 */
    { "sys_timer_create",0, 222, KEX_API_SYSCALL, 3 },
    { "sys_timer_settime",0,223, KEX_API_SYSCALL, 4 },
    { "sys_timer_gettime",0,224, KEX_API_SYSCALL, 2 },
    { "sys_timer_getoverrun",0,225,KEX_API_SYSCALL,1 },
    { "sys_timer_delete",0, 226, KEX_API_SYSCALL, 1 },
    /* 时钟 */
    { "sys_clock_settime",  0, 227, KEX_API_SYSCALL, 2 },
    { "sys_clock_gettime",  0, 228, KEX_API_SYSCALL, 2 },
    { "sys_clock_getres",   0, 229, KEX_API_SYSCALL, 2 },
    { "sys_clock_nanosleep",0, 230, KEX_API_SYSCALL, 4 },
    /* 退出/信号 */
    { "sys_exit_group",  0, 231, KEX_API_SYSCALL, 1 },
    { "sys_epoll_wait",  0, 232, KEX_API_SYSCALL, 4 },
    { "sys_epoll_ctl",   0, 233, KEX_API_SYSCALL, 4 },
    { "sys_tgkill",      0, 234, KEX_API_SYSCALL, 3 },
    { "sys_utimes",      0, 235, KEX_API_SYSCALL, 2 },
    /* 进程 */
    { "sys_waitid",      0, 247, KEX_API_SYSCALL, 5 },
    /* 消息队列 */
    { "sys_mq_open",     0, 240, KEX_API_SYSCALL, 4 },
    { "sys_mq_unlink",   0, 241, KEX_API_SYSCALL, 1 },
    { "sys_mq_timedsend",0, 242, KEX_API_SYSCALL, 5 },
    { "sys_mq_timedreceive",0,243,KEX_API_SYSCALL, 5 },
    { "sys_mq_notify",   0, 244, KEX_API_SYSCALL, 2 },
    { "sys_mq_getsetattr",0,245, KEX_API_SYSCALL, 3 },
    /* kexec */
    { "sys_kexec_load",  0, 246, KEX_API_SYSCALL, 5 },
    /* 密钥 */
    { "sys_add_key",     0, 248, KEX_API_SYSCALL, 5 },
    { "sys_request_key", 0, 249, KEX_API_SYSCALL, 4 },
    { "sys_keyctl",      0, 250, KEX_API_SYSCALL, 5 },
    /* IO 优先级 */
    { "sys_ioprio_set",  0, 251, KEX_API_SYSCALL, 3 },
    { "sys_ioprio_get",  0, 252, KEX_API_SYSCALL, 2 },
    /* inotify */
    { "sys_inotify_init",0, 253, KEX_API_SYSCALL, 0 },
    { "sys_inotify_add_watch",0,254,KEX_API_SYSCALL,3 },
    { "sys_inotify_rm_watch",0,255,KEX_API_SYSCALL,2 },
    /* *at 系列 */
    { "sys_openat",      0, 257, KEX_API_SYSCALL, 4 },
    { "sys_mkdirat",     0, 258, KEX_API_SYSCALL, 3 },
    { "sys_mknodat",     0, 259, KEX_API_SYSCALL, 4 },
    { "sys_fchownat",    0, 260, KEX_API_SYSCALL, 5 },
    { "sys_futimesat",   0, 261, KEX_API_SYSCALL, 3 },
    { "sys_newfstatat",  0, 262, KEX_API_SYSCALL, 4 },
    { "sys_unlinkat",    0, 263, KEX_API_SYSCALL, 3 },
    { "sys_renameat",    0, 264, KEX_API_SYSCALL, 4 },
    { "sys_linkat",      0, 265, KEX_API_SYSCALL, 5 },
    { "sys_symlinkat",   0, 266, KEX_API_SYSCALL, 3 },
    { "sys_readlinkat",  0, 267, KEX_API_SYSCALL, 4 },
    { "sys_fchmodat",    0, 268, KEX_API_SYSCALL, 3 },
    { "sys_faccessat",   0, 269, KEX_API_SYSCALL, 3 },
    { "sys_pselect6",    0, 270, KEX_API_SYSCALL, 6 },
    { "sys_ppoll",       0, 271, KEX_API_SYSCALL, 4 },
    { "sys_unshare",     0, 272, KEX_API_SYSCALL, 1 },
    { "sys_set_robust_list",0,273,KEX_API_SYSCALL, 2 },
    { "sys_get_robust_list",0,274,KEX_API_SYSCALL, 3 },
    { "sys_splice",      0, 275, KEX_API_SYSCALL, 6 },
    { "sys_tee",         0, 276, KEX_API_SYSCALL, 4 },
    { "sys_sync_file_range",0,277,KEX_API_SYSCALL, 4 },
    { "sys_vmsplice",    0, 278, KEX_API_SYSCALL, 4 },
    { "sys_move_pages",  0, 279, KEX_API_SYSCALL, 6 },
    { "sys_utimensat",   0, 280, KEX_API_SYSCALL, 4 },
    { "sys_epoll_pwait", 0, 281, KEX_API_SYSCALL, 5 },
    { "sys_signalfd",    0, 282, KEX_API_SYSCALL, 3 },
    { "sys_timerfd_create",0,283,KEX_API_SYSCALL, 2 },
    { "sys_eventfd",     0, 284, KEX_API_SYSCALL, 1 },
    { "sys_fallocate",   0, 285, KEX_API_SYSCALL, 4 },
    { "sys_timerfd_settime",0,286,KEX_API_SYSCALL, 4 },
    { "sys_timerfd_gettime",0,287,KEX_API_SYSCALL, 2 },
    { "sys_accept4",     0, 288, KEX_API_SYSCALL, 4 },
    { "sys_signalfd4",   0, 289, KEX_API_SYSCALL, 4 },
    { "sys_eventfd2",    0, 290, KEX_API_SYSCALL, 2 },
    { "sys_epoll_create1",0,291,KEX_API_SYSCALL, 1 },
    { "sys_dup3",        0, 292, KEX_API_SYSCALL, 3 },
    { "sys_pipe2",       0, 293, KEX_API_SYSCALL, 2 },
    { "sys_inotify_init1",0,294,KEX_API_SYSCALL, 1 },
    { "sys_preadv",      0, 295, KEX_API_SYSCALL, 5 },
    { "sys_pwritev",     0, 296, KEX_API_SYSCALL, 5 },
    { "sys_rt_tgsigqueueinfo",0,297,KEX_API_SYSCALL,4 },
    { "sys_perf_event_open",0,298,KEX_API_SYSCALL, 5 },
    { "sys_recvmmsg",    0, 299, KEX_API_SYSCALL, 5 },
    { "sys_fanotify_init",0,300,KEX_API_SYSCALL, 2 },
    { "sys_fanotify_mark",0,301,KEX_API_SYSCALL, 5 },
    { "sys_prlimit64",   0, 302, KEX_API_SYSCALL, 4 },
    { "sys_name_to_handle_at",0,303,KEX_API_SYSCALL,3 },
    { "sys_open_by_handle_at",0,304,KEX_API_SYSCALL,3 },
    { "sys_clock_adjtime",0,305,KEX_API_SYSCALL, 2 },
    { "sys_syncfs",      0, 306, KEX_API_SYSCALL, 1 },
    { "sys_sendmmsg",    0, 307, KEX_API_SYSCALL, 4 },
    { "sys_setns",       0, 308, KEX_API_SYSCALL, 2 },
    { "sys_getcpu",      0, 309, KEX_API_SYSCALL, 3 },
    { "sys_process_vm_readv",0,310,KEX_API_SYSCALL, 6 },
    { "sys_process_vm_writev",0,311,KEX_API_SYSCALL, 6 },
    { "sys_kcmp",        0, 312, KEX_API_SYSCALL, 5 },
    { "sys_finit_module",0, 313, KEX_API_SYSCALL, 3 },
    { "sys_sched_setattr",0,314,KEX_API_SYSCALL, 3 },
    { "sys_sched_getattr",0,315,KEX_API_SYSCALL, 4 },
    { "sys_renameat2",   0, 316, KEX_API_SYSCALL, 5 },
    { "sys_seccomp",     0, 317, KEX_API_SYSCALL, 3 },
    { "sys_getrandom",   0, 318, KEX_API_SYSCALL, 3 },
    { "sys_memfd_create",0, 319, KEX_API_SYSCALL, 2 },
    { "sys_kexec_file_load",0,320,KEX_API_SYSCALL, 5 },
    { "sys_bpf",         0, 321, KEX_API_SYSCALL, 3 },
    { "sys_execveat",    0, 322, KEX_API_SYSCALL, 5 },
    { "sys_userfaultfd", 0, 323, KEX_API_SYSCALL, 1 },
    { "sys_membarrier",  0, 324, KEX_API_SYSCALL, 2 },
    { "sys_mlock2",      0, 325, KEX_API_SYSCALL, 3 },
    { "sys_copy_file_range",0,326,KEX_API_SYSCALL, 6 },
    { "sys_preadv2",     0, 327, KEX_API_SYSCALL, 6 },
    { "sys_pwritev2",    0, 328, KEX_API_SYSCALL, 6 },
    { "sys_pkey_mprotect",0,329,KEX_API_SYSCALL, 4 },
    { "sys_pkey_alloc",  0, 330, KEX_API_SYSCALL, 2 },
    { "sys_pkey_free",   0, 331, KEX_API_SYSCALL, 1 },
    { "sys_statx",       0, 332, KEX_API_SYSCALL, 5 },
    /* io_uring */
    { "sys_io_uring_setup", 0, 425, KEX_API_SYSCALL, 2 },
    { "sys_io_uring_enter", 0, 426, KEX_API_SYSCALL, 6 },
    { "sys_io_uring_register",0,427,KEX_API_SYSCALL, 4 },
    /* 新系统调用 */
    { "sys_openat2",     0, 437, KEX_API_SYSCALL, 4 },
    { "sys_clone3",      0, 435, KEX_API_SYSCALL, 2 },
    { "sys_close_range", 0, 436, KEX_API_SYSCALL, 3 },
    /* 新增 syscall 333-335 */
    { "sys_io_pgetevents",  0, 333, KEX_API_SYSCALL, 6 },
    { "sys_rseq",           0, 334, KEX_API_SYSCALL, 4 },
    { "sys_kexec_file_load2",0,335, KEX_API_SYSCALL, 5 },
    /* pidfd 系列 424-434 */
    { "sys_pidfd_send_signal", 0, 424, KEX_API_SYSCALL, 4 },
    { "sys_open_tree",      0, 428, KEX_API_SYSCALL, 3 },
    { "sys_move_mount",     0, 429, KEX_API_SYSCALL, 5 },
    { "sys_fsopen",         0, 430, KEX_API_SYSCALL, 2 },
    { "sys_fsconfig",       0, 431, KEX_API_SYSCALL, 5 },
    { "sys_fsmount",        0, 432, KEX_API_SYSCALL, 3 },
    { "sys_fspick",         0, 433, KEX_API_SYSCALL, 3 },
    { "sys_pidfd_open",     0, 434, KEX_API_SYSCALL, 2 },
    /* 新增 syscall 438-450 */
    { "sys_pidfd_getfd",    0, 438, KEX_API_SYSCALL, 3 },
    { "sys_faccessat2",     0, 439, KEX_API_SYSCALL, 4 },
    { "sys_process_madvise",0, 440, KEX_API_SYSCALL, 5 },
    { "sys_epoll_pwait2",   0, 441, KEX_API_SYSCALL, 5 },
    { "sys_mount_setattr",  0, 442, KEX_API_SYSCALL, 5 },
    { "sys_quotactl_fd",    0, 443, KEX_API_SYSCALL, 4 },
    { "sys_landlock_create_ruleset", 0, 444, KEX_API_SYSCALL, 3 },
    { "sys_landlock_add_rule",      0, 445, KEX_API_SYSCALL, 4 },
    { "sys_landlock_restrict_self", 0, 446, KEX_API_SYSCALL, 2 },
    { "sys_memfd_secret",   0, 447, KEX_API_SYSCALL, 1 },
    { "sys_process_mrelease",0,448, KEX_API_SYSCALL, 2 },
    { "sys_futex_waitv",    0, 449, KEX_API_SYSCALL, 3 },
    { "sys_set_mempolicy_home_node",0,450,KEX_API_SYSCALL, 4 },
};

/* Kenux 扩展 syscall 451-500 */
static kex_api_entry_t kex_kenux_apis[] = {
    { "kenux_info",              0, 451, KEX_API_KENUX, 1 },
    { "kenux_debug",             0, 452, KEX_API_KENUX, 1 },
    { "kenux_get_version",       0, 453, KEX_API_KENUX, 1 },
    { "kenux_get_uptime",        0, 454, KEX_API_KENUX, 1 },
    { "kenux_get_loadavg",       0, 455, KEX_API_KENUX, 2 },
    { "kenux_reboot",            0, 456, KEX_API_KENUX, 0 },
    { "kenux_poweroff",          0, 457, KEX_API_KENUX, 0 },
    { "kenux_halt",              0, 458, KEX_API_KENUX, 0 },
    { "kenux_get_cpu_count",     0, 459, KEX_API_KENUX, 0 },
    { "kenux_get_cpu_info",      0, 460, KEX_API_KENUX, 2 },
    { "kenux_set_affinity",      0, 461, KEX_API_KENUX, 2 },
    { "kenux_get_affinity",      0, 462, KEX_API_KENUX, 2 },
    { "kenux_create_namespace",  0, 463, KEX_API_KENUX, 1 },
    { "kenux_enter_namespace",   0, 464, KEX_API_KENUX, 1 },
    { "kenux_get_namespace",     0, 465, KEX_API_KENUX, 2 },
    { "kenux_vmspace_create",    0, 466, KEX_API_KENUX, 0 },
    { "kenux_vmspace_destroy",   0, 467, KEX_API_KENUX, 1 },
    { "kenux_vmspace_switch",    0, 468, KEX_API_KENUX, 1 },
    { "kenux_iommu_map",         0, 469, KEX_API_KENUX, 4 },
    { "kenux_iommu_unmap",       0, 470, KEX_API_KENUX, 2 },
    { "kenux_dma_alloc",         0, 471, KEX_API_KENUX, 2 },
    { "kenux_dma_free",          0, 472, KEX_API_KENUX, 2 },
    { "kenux_pci_read",          0, 473, KEX_API_KENUX, 4 },
    { "kenux_pci_write",         0, 474, KEX_API_KENUX, 4 },
    { "kenux_pci_enum",          0, 475, KEX_API_KENUX, 2 },
    { "kenux_acpi_query",        0, 476, KEX_API_KENUX, 2 },
    { "kenux_smbios_get",        0, 477, KEX_API_KENUX, 2 },
    { "kenux_fb_get_info",       0, 478, KEX_API_KENUX, 1 },
    { "kenux_fb_map",            0, 479, KEX_API_KENUX, 1 },
    { "kenux_fb_unmap",          0, 480, KEX_API_KENUX, 1 },
    { "kenux_fb_flip",           0, 481, KEX_API_KENUX, 1 },
    { "kenux_gpu_submit",        0, 482, KEX_API_KENUX, 3 },
    { "kenux_gpu_wait",          0, 483, KEX_API_KENUX, 1 },
    { "kenux_net_attach",        0, 484, KEX_API_KENUX, 2 },
    { "kenux_net_detach",        0, 485, KEX_API_KENUX, 1 },
    { "kenux_net_ioctl",         0, 486, KEX_API_KENUX, 3 },
    { "kenux_fs_snapshot",       0, 487, KEX_API_KENUX, 2 },
    { "kenux_fs_rollback",       0, 488, KEX_API_KENUX, 2 },
    { "kenux_fs_compress",       0, 489, KEX_API_KENUX, 3 },
    { "kenux_fs_encrypt",        0, 490, KEX_API_KENUX, 3 },
    { "kenux_audit_log",         0, 491, KEX_API_KENUX, 3 },
    { "kenux_audit_config",      0, 492, KEX_API_KENUX, 2 },
    { "kenux_seccomp_install",   0, 493, KEX_API_KENUX, 2 },
    { "kenux_seccomp_filter",    0, 494, KEX_API_KENUX, 3 },
    { "kenux_trace_attach",      0, 495, KEX_API_KENUX, 2 },
    { "kenux_trace_detach",      0, 496, KEX_API_KENUX, 1 },
    { "kenux_trace_read",        0, 497, KEX_API_KENUX, 3 },
    { "kenux_trace_write",       0, 498, KEX_API_KENUX, 3 },
    { "kenux_kprobe_register",   0, 499, KEX_API_KENUX, 2 },
    { "kenux_kprobe_unregister", 0, 500, KEX_API_KENUX, 1 },
};

/* 哈希表 (CRC32 -> API 条目), 用于 O(1) 查找 */
#define KEX_API_HASH_SIZE 1024
static kex_api_entry_t *g_hash_table[KEX_API_HASH_SIZE];
static int table_inited = 0;

/* 合并视图: 指向 std_apis 和 kenux_apis 的指针数组, 供 kex_api_table() 使用 */
static const kex_api_entry_t *g_all_apis[
    sizeof(kex_std_apis) / sizeof(kex_std_apis[0]) +
    sizeof(kex_kenux_apis) / sizeof(kex_kenux_apis[0])
];
static size_t g_all_apis_count = 0;

/* 计算 CRC32 在哈希表中的槽位 (简单取模) */
static size_t hash_slot(uint32_t crc32) {
    /* 将 32 位 CRC 均匀分布到哈希表 */
    return (crc32 ^ (crc32 >> 16)) % KEX_API_HASH_SIZE;
}

static void ensure_init(void) {
    if (table_inited) return;

    /* 填充 CRC32 字段 */
    size_t n1 = sizeof(kex_std_apis) / sizeof(kex_std_apis[0]);
    size_t n2 = sizeof(kex_kenux_apis) / sizeof(kex_kenux_apis[0]);
    for (size_t i = 0; i < n1; i++)
        ((kex_api_entry_t *)kex_std_apis)[i].crc32 = kex_crc32_str(kex_std_apis[i].name);
    for (size_t i = 0; i < n2; i++)
        ((kex_api_entry_t *)kex_kenux_apis)[i].crc32 = kex_crc32_str(kex_kenux_apis[i].name);

    /* 构建哈希表 (开放寻址法, 条目数远小于哈希表大小) */
    memset(g_hash_table, 0, sizeof(g_hash_table));
    g_all_apis_count = 0;
    for (size_t i = 0; i < n1; i++) {
        size_t slot = hash_slot(kex_std_apis[i].crc32);
        while (g_hash_table[slot] != NULL) {
            slot = (slot + 1) % KEX_API_HASH_SIZE;
        }
        g_hash_table[slot] = &((kex_api_entry_t *)kex_std_apis)[i];
        g_all_apis[g_all_apis_count++] = &kex_std_apis[i];
    }
    for (size_t i = 0; i < n2; i++) {
        size_t slot = hash_slot(kex_kenux_apis[i].crc32);
        while (g_hash_table[slot] != NULL) {
            slot = (slot + 1) % KEX_API_HASH_SIZE;
        }
        g_hash_table[slot] = &((kex_api_entry_t *)kex_kenux_apis)[i];
        g_all_apis[g_all_apis_count++] = &kex_kenux_apis[i];
    }

    table_inited = 1;
}

const kex_api_entry_t *kex_api_lookup_name(const char *name) {
    ensure_init();
    size_t n1 = sizeof(kex_std_apis) / sizeof(kex_std_apis[0]);
    size_t n2 = sizeof(kex_kenux_apis) / sizeof(kex_kenux_apis[0]);
    for (size_t i = 0; i < n1; i++)
        if (strcmp(kex_std_apis[i].name, name) == 0) return &kex_std_apis[i];
    for (size_t i = 0; i < n2; i++)
        if (strcmp(kex_kenux_apis[i].name, name) == 0) return &kex_kenux_apis[i];
    return NULL;
}

/* 按 CRC32 查找 (哈希表加速, O(1) 平均) */
const kex_api_entry_t *kex_api_lookup_crc32(uint32_t crc32) {
    ensure_init();
    size_t slot = hash_slot(crc32);
    /* 开放寻址: 最多探测 KEX_API_HASH_SIZE 次 */
    for (size_t i = 0; i < KEX_API_HASH_SIZE; i++) {
        kex_api_entry_t *e = g_hash_table[slot];
        if (e == NULL) return NULL;  /* 空槽 = 未找到 */
        if (e->crc32 == crc32) return e;
        slot = (slot + 1) % KEX_API_HASH_SIZE;
    }
    return NULL;
}

const kex_api_entry_t *kex_api_table(size_t *count) {
    ensure_init();
    /* 返回标准表; Kenux 表需用 kex_api_table_all() 获取 */
    if (count) *count = sizeof(kex_std_apis) / sizeof(kex_std_apis[0]);
    return kex_std_apis;
}

const kex_api_entry_t * const *kex_api_table_all(size_t *count) {
    ensure_init();
    if (count) *count = g_all_apis_count;
    return g_all_apis;
}

uint32_t kex_api_crc32(const char *name) {
    return kex_crc32_str(name);
}


/* ============================================================
 * kex_loader.h - .kex/.kxp 文件加载与解析 (声明, 内联)
 * ============================================================ */
/*
 * kex_loader.h - .kex/.kxp 文件加载与解析
 */
#ifdef __cplusplus
extern "C" {
#endif

/* 解析后的导入条目 */
typedef struct {
    uint32_t  crc32;        /* API 名称 CRC32 */
    uint32_t *addr_slot;    /* 指向文件缓冲里的地址占位 (回填用) */
    uint32_t  slot_file_off;/* 地址占位在文件中的偏移 (回填用) */
    const char *name;       /* 函数名 (指向文件缓冲) */
} kex_import_t;

/* 解析后的导出条目 */
typedef struct {
    const char *name;       /* 函数名 */
    uint32_t code_offset;   /* 相对文件头 */
} kex_export_t;

/* 解析后的重定位条目 */
typedef struct {
    uint32_t call_off;      /* call rel32 操作数的文件偏移 */
    uint32_t sym_crc32;     /* 目标符号 CRC32 */
} kex_reloc_t;

/* 解析后的本地重定位条目 (段内 .rodata 引用) */
typedef struct {
    uint32_t patch_off;     /* 代码区内 disp32 操作数的文件偏移 */
    uint32_t sym_value;     /* 目标符号在 .rodata 中的偏移 */
} kex_local_reloc_entry_t;

/* 解析后的程序 */
typedef struct {
    kex_header_t header;
    char name[KEX_NAME_LEN + 1];

    const uint8_t *code;        /* 代码数据 (指向 raw) */
    uint32_t code_size;
    uint32_t entry_file_offset; /* 入口相对文件头偏移 */

    const uint8_t *rodata;      /* .rodata 段数据 (指向 raw) */
    uint32_t rodata_size;

    kex_import_t *imports;
    uint32_t import_count;
    kex_export_t *exports;
    uint32_t export_count;
    kex_reloc_t  *relocs;       /* 重定位表 */
    uint32_t reloc_count;
    kex_local_reloc_entry_t *local_relocs;  /* 本地重定位表 */
    uint32_t local_reloc_count;

    const uint8_t *icon;        /* 图标数据 (指向 raw) */
    uint32_t icon_size;

    uint8_t *raw;               /* 原始文件缓冲 (需 free) */
    size_t   raw_size;
} kex_program_t;

/* 从文件加载并解析; 成功返回 KEX_OK */
int kex_load_file(const char *path, kex_program_t *prog);

/* 从内存缓冲解析 (不接管 buf 所有权); 成功返回 KEX_OK */
int kex_load_mem(const uint8_t *buf, size_t size, kex_program_t *prog);

/* 校验 CRC32 (整个文件, crc32 字段按 0 计算) */
int kex_verify_crc32(const kex_program_t *prog);

/* 释放 (仅 free 内部 raw 与表) */
void kex_free(kex_program_t *prog);

/* 打印程序信息 (调试) */
void kex_dump(const kex_program_t *prog);

#ifdef __cplusplus
}
#endif


/* ============================================================
 * kex_loader.c - .kex/.kxp 文件加载与解析 (实现)
 * ============================================================ */
/*
 * kex_loader.c - .kex/.kxp 文件加载与解析
 */
int kex_load_mem(const uint8_t *buf, size_t size, kex_program_t *prog) {
    if (!buf || !prog) return KEX_ERR_FORMAT;
    if (size < KEX_HEADER_SIZE + KEX_NAME_LEN + KEX_CODE_START_LEN) return KEX_ERR_SIZE;

    memset(prog, 0, sizeof(*prog));

    const kex_header_t *h = (const kex_header_t *)buf;

    /* 魔数 */
    if (h->magic[0] != KEX_MAGIC0 || h->magic[1] != KEX_MAGIC1 ||
        h->magic[2] != KEX_MAGIC2 || h->magic[3] != KEX_MAGIC3)
        return KEX_ERR_MAGIC;

    if (h->version != KEX_VERSION) return KEX_ERR_VERSION;
    if (h->header_size != KEX_HEADER_SIZE) return KEX_ERR_FORMAT;
    if (h->total_size > size) return KEX_ERR_SIZE;

    prog->header = *h;
    memcpy(prog->name, buf + KEX_HEADER_SIZE, KEX_NAME_LEN);
    prog->name[KEX_NAME_LEN] = '\0';

    /* 校验开始符 "Kenux---" */
    if (memcmp(buf + KEX_HEADER_SIZE + KEX_NAME_LEN,
               KEX_CODE_START, KEX_CODE_START_LEN) != 0)
        return KEX_ERR_FORMAT;

    /* 代码区: 紧跟开始符之后, 长度 = code_size, 尾部应有 KKKSTOP */
    uint32_t code_off = KEX_CODE_REGION_OFFSET;
    if (code_off + h->code_size + KEX_CODE_END_LEN > h->total_size)
        return KEX_ERR_FORMAT;

    /* 校验结束符 "KKKSTOP" */
    if (memcmp(buf + code_off + h->code_size,
               KEX_CODE_END, KEX_CODE_END_LEN) != 0)
        return KEX_ERR_FORMAT;

    prog->code = buf + code_off;
    prog->code_size = h->code_size;
    prog->entry_file_offset = h->entry_offset;

    /* .rodata 段 */
    if (h->rodata_size > 0) {
        if (h->rodata_offset == 0 ||
            (uint64_t)h->rodata_offset + h->rodata_size > h->total_size)
            return KEX_ERR_RODATA;
        prog->rodata = buf + h->rodata_offset;
        prog->rodata_size = h->rodata_size;
    }

    /* 导入表 */
    if (h->import_count > 0) {
        if (h->import_offset == 0 ||
            h->import_offset + 8 > h->total_size)   /* 至少能读一条头部 */
            return KEX_ERR_FORMAT;
        prog->imports = (kex_import_t *)calloc(h->import_count, sizeof(kex_import_t));
        if (!prog->imports) return KEX_ERR_NO_MEM;
        prog->import_count = h->import_count;

        uint32_t off = h->import_offset;
        for (uint32_t i = 0; i < h->import_count; i++) {
            if (off + 8 > h->total_size) return KEX_ERR_FORMAT;
            uint32_t crc = *(const uint32_t *)(buf + off);
            uint32_t slot = off + 4;                 /* 地址占位偏移 */
            const char *nm = (const char *)(buf + off + 8);
            /* 找字符串结尾 */
            const char *end = (const char *)memchr(nm, 0, h->total_size - (off + 8));
            if (!end) return KEX_ERR_FORMAT;
            prog->imports[i].crc32 = crc;
            prog->imports[i].addr_slot = (uint32_t *)(prog->raw ? prog->raw : (uint8_t*)buf) + (slot/4);
            /* 注意: addr_slot 仅在可写缓冲里有效; 解析阶段用偏移记录 */
            prog->imports[i].slot_file_off = slot;
            prog->imports[i].name = nm;
            off = (uint32_t)((end + 1) - (const char *)buf);
        }
    }

    /* 导出表 */
    if (h->export_count > 0) {
        if (h->export_offset == 0 ||
            h->export_offset + 5 > h->total_size)
            return KEX_ERR_FORMAT;
        prog->exports = (kex_export_t *)calloc(h->export_count, sizeof(kex_export_t));
        if (!prog->exports) return KEX_ERR_NO_MEM;
        prog->export_count = h->export_count;

        uint32_t off = h->export_offset;
        for (uint32_t i = 0; i < h->export_count; i++) {
            const char *nm = (const char *)(buf + off);
            const char *end = (const char *)memchr(nm, 0, h->total_size - off);
            if (!end) return KEX_ERR_FORMAT;
            uint32_t nm_end = (uint32_t)((end + 1) - (const char *)buf);
            if (nm_end + 4 > h->total_size) return KEX_ERR_FORMAT;
            prog->exports[i].name = nm;
            prog->exports[i].code_offset = *(const uint32_t *)(buf + nm_end);
            off = nm_end + 4;
        }
    }

    /* 图标 */
    if (h->icon_size > 0 && h->icon_offset != 0) {
        if (h->icon_offset + h->icon_size > h->total_size) return KEX_ERR_FORMAT;
        prog->icon = buf + h->icon_offset;
        prog->icon_size = h->icon_size;
    }

    /* 重定位表 */
    if (h->reloc_offset != 0) {
        if (h->reloc_offset + 4 > h->total_size) return KEX_ERR_FORMAT;
        uint32_t rcount = *(const uint32_t *)(buf + h->reloc_offset);
        if (rcount > 0) {
            if (h->reloc_offset + 4 + rcount * 8 > h->total_size)
                return KEX_ERR_FORMAT;
            prog->relocs = (kex_reloc_t *)calloc(rcount, sizeof(kex_reloc_t));
            if (!prog->relocs) return KEX_ERR_NO_MEM;
            prog->reloc_count = rcount;
            for (uint32_t i = 0; i < rcount; i++) {
                const kex_reloc_entry_t *re =
                    (const kex_reloc_entry_t *)(buf + h->reloc_offset + 4 + i * 8);
                prog->relocs[i].call_off  = re->call_off;
                prog->relocs[i].sym_crc32 = re->sym_crc32;
            }
        }
    }

    /* 本地重定位表 (.rodata 段内引用) */
    if (h->local_reloc_cnt > 0 && h->local_reloc_off != 0) {
        if ((uint64_t)h->local_reloc_off + (uint64_t)h->local_reloc_cnt * 8 > h->total_size)
            return KEX_ERR_LRELOC;
        prog->local_relocs = (kex_local_reloc_entry_t *)
            calloc(h->local_reloc_cnt, sizeof(kex_local_reloc_entry_t));
        if (!prog->local_relocs) return KEX_ERR_NO_MEM;
        prog->local_reloc_count = h->local_reloc_cnt;
        for (uint32_t i = 0; i < h->local_reloc_cnt; i++) {
            const kex_local_reloc_t *lr =
                (const kex_local_reloc_t *)(buf + h->local_reloc_off + i * 8);
            prog->local_relocs[i].patch_off = lr->patch_off;
            prog->local_relocs[i].sym_value = lr->sym_value;
        }
    }

    return KEX_OK;
}

int kex_load_file(const char *path, kex_program_t *prog) {
    if (!path || !prog) return KEX_ERR_FORMAT;
    FILE *fp = fopen(path, "rb");
    if (!fp) return KEX_ERR_IO;
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (sz <= 0) { fclose(fp); return KEX_ERR_SIZE; }

    uint8_t *buf = (uint8_t *)malloc((size_t)sz);
    if (!buf) { fclose(fp); return KEX_ERR_NO_MEM; }
    if (fread(buf, 1, (size_t)sz, fp) != (size_t)sz) {
        fclose(fp); free(buf); return KEX_ERR_IO;
    }
    fclose(fp);

    int ret = kex_load_mem(buf, (size_t)sz, prog);
    if (ret != KEX_OK) { free(buf); return ret; }

    /* 接管缓冲, 修正 addr_slot 指向可写缓冲 */
    prog->raw = buf;
    prog->raw_size = (size_t)sz;
    for (uint32_t i = 0; i < prog->import_count; i++) {
        prog->imports[i].addr_slot =
            (uint32_t *)(prog->raw + prog->imports[i].slot_file_off);
    }
    /* 重新指向 raw 内的 name/code/rodata/export (因为之前指向的是参数 buf) */
    prog->code = prog->raw + KEX_CODE_REGION_OFFSET;
    if (prog->rodata_size > 0)
        prog->rodata = prog->raw + prog->header.rodata_offset;
    if (prog->import_count) {
        /* imports[].name 已是相对 buf 的指针, buf == raw 时仍有效 */
    }
    return KEX_OK;
}

int kex_verify_crc32(const kex_program_t *prog) {
    if (!prog || !prog->raw) return KEX_ERR_FORMAT;
    /* 把 crc32 字段处置 0 计算 */
    uint8_t *tmp = (uint8_t *)malloc(prog->raw_size);
    if (!tmp) return KEX_ERR_NO_MEM;
    memcpy(tmp, prog->raw, prog->raw_size);
    uint32_t field_off = offsetof(kex_header_t, crc32);
    memset(tmp + field_off, 0, 4);
    uint32_t calc = kex_crc32(tmp, prog->raw_size);
    free(tmp);
    return (calc == prog->header.crc32) ? KEX_OK : KEX_ERR_CRC;
}

void kex_free(kex_program_t *prog) {
    if (!prog) return;
    if (prog->imports) free(prog->imports);
    if (prog->exports) free(prog->exports);
    if (prog->relocs)  free(prog->relocs);
    if (prog->local_relocs) free(prog->local_relocs);
    if (prog->raw) free(prog->raw);
    memset(prog, 0, sizeof(*prog));
}

void kex_dump(const kex_program_t *prog) {
    if (!prog) return;
    printf("=== KEX Program ===\n");
    printf("  name:        %s\n", prog->name);
    printf("  type:        %s\n", prog->header.file_type == KEX_FILE_KXP ? "KXP" : "KEX");
    printf("  version:     0x%04x\n", prog->header.version);
    printf("  total_size:  %u\n", prog->header.total_size);
    printf("  entry_off:   0x%x\n", prog->header.entry_offset);
    printf("  code_size:   %u\n", prog->header.code_size);
    printf("  stack:       %u KB\n", prog->header.stack_size / 1024);
    printf("  heap:        %u KB\n", prog->header.heap_size / 1024);
    printf("  crc32:       0x%08x\n", prog->header.crc32);
    printf("  imports:     %u\n", prog->import_count);
    for (uint32_t i = 0; i < prog->import_count; i++)
        printf("    [%u] crc=0x%08x name=%s\n", i, prog->imports[i].crc32, prog->imports[i].name);
    printf("  exports:     %u\n", prog->export_count);
    for (uint32_t i = 0; i < prog->export_count; i++)
        printf("    [%u] %s @ 0x%x\n", i, prog->exports[i].name, prog->exports[i].code_offset);
    if (prog->reloc_count) {
        printf("  relocs:      %u\n", prog->reloc_count);
        for (uint32_t i = 0; i < prog->reloc_count; i++)
            printf("    [%u] call_off=0x%x sym_crc=0x%08x\n",
                   i, prog->relocs[i].call_off, prog->relocs[i].sym_crc32);
    }
    if (prog->rodata_size) {
        printf("  rodata:      %u bytes @ off 0x%x\n",
               prog->rodata_size, prog->header.rodata_offset);
    }
    if (prog->local_reloc_count) {
        printf("  local_relocs:%u\n", prog->local_reloc_count);
        for (uint32_t i = 0; i < prog->local_reloc_count; i++)
            printf("    [%u] patch_off=0x%x sym_value=0x%x\n",
                   i, prog->local_relocs[i].patch_off, prog->local_relocs[i].sym_value);
    }
    if (prog->icon_size)
        printf("  icon:        %u bytes PNG\n", prog->icon_size);
    printf("===================\n");
}


/* ============================================================
 * kexc.c - KEX 编译器 (合并, 去掉 KEX_STANDALONE)
 * ============================================================ */
/*
 * kexc.c - KEX 编译器: C 源码 -> .kex (HGS 格式)
 *
 * 流程:
 *   1. gcc 把 C 源码编译为 freestanding .o
 *   2. 从 ELF .o 提取 .text 机器码
 *   3. 扫描源码, 收集引用的 Kenux API (sys_xxx 与 kenux_xxx), 生成导入表
 *   4. 组装 .kex: 头部 + 程序名 + "Kenux---" + 代码 + "KKKSTOP" + 导入表
 *   5. 计算 CRC32 回填
 *
 * 用法: kexc <input.c> <output.kex> [--name NAME] [--kxp]
 */
#ifdef _WIN32
static int use_color = 0;
#else
static int use_color = 1;
#endif

#define ANSI_RESET   "\x1b[0m"
#define ANSI_BOLD    "\x1b[1m"
#define ANSI_CYAN    "\x1b[36m"
#define ANSI_GREEN   "\x1b[32m"
#define ANSI_RED     "\x1b[31m"
#define ANSI_YELLOW  "\x1b[33m"
#define ANSI_BLUE    "\x1b[34m"

static void banner(void) {
    printf("\n");
    if (use_color) printf(ANSI_CYAN ANSI_BOLD);
    printf("+------------------------------------------------------------+\n");
    printf("|          KexKit  KEX Compiler (kexc)  v1.0                 |\n");
    printf("|          HGS format / KenuxOS API                          |\n");
    printf("+------------------------------------------------------------+\n");
    if (use_color) printf(ANSI_RESET);
    printf("\n");
}

/* ---- 从 ELF .o 读取 .text 段 ---- */
/* ELF64 Section Header 布局:
 *   0x00  sh_name      (4B)
 *   0x04  sh_type      (4B)
 *   0x08  sh_flags     (8B)
 *   0x10  sh_addr      (8B)
 *   0x18  sh_offset    (8B)  <- 文件偏移
 *   0x20  sh_size      (8B)
 *   0x28  sh_link      (4B)
 *   0x2C  sh_info      (4B)
 *   0x30  sh_addralign (8B)
 *   0x38  sh_entsize   (8B)
 */
#define SH_NAME_OFF     0
#define SH_TYPE_OFF     4
#define SH_OFFSET_OFF   24
#define SH_SIZE_OFF     32
#define SH_LINK_OFF     40
#define SH_INFO_OFF     44
#define SH_ENTSIZE_OFF  56
#define SH_ADDR_OFF     16

/* ELF64 Symbol 布局 (24 字节):
 *   0x00  st_name  (4B)
 *   0x04  st_info  (1B)
 *   0x05  st_other (1B)
 *   0x06  st_shndx (2B)
 *   0x08  st_value (8B)
 *   0x10  st_size  (8B)
 */
#define SYM_NAME_OFF   0
#define SYM_INFO_OFF   4
#define SYM_SHNDX_OFF  6
#define SYM_VALUE_OFF  8
#define SYM_SIZE       24

/* ELF64 Rela 布局 (24 字节):
 *   0x00  r_offset (8B)
 *   0x08  r_info   (8B)  -> sym = r_info >> 32, type = r_info & 0xffffffff
 *   0x10  r_addend (8B)
 */
#define RELA_OFFSET_OFF 0
#define RELA_INFO_OFF   8
#define RELA_ADDEND_OFF 16
#define RELA_SIZE       24

/* R_X86_64_PC32 = 2, R_X86_64_PLT32 = 4, R_X86_64_NONE = 0,
 * R_X86_64_32 = 10, R_X86_64_32S = 11 */
#define R_X86_64_NONE   0
#define R_X86_64_PC32   2
#define R_X86_64_PLT32  4
#define R_X86_64_32    10
#define R_X86_64_32S   11

/* 读取整个文件到内存 */
static uint8_t *read_file_all(const char *path, size_t *out_size) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (sz <= 0) { fclose(fp); return NULL; }
    uint8_t *buf = (uint8_t *)malloc(sz);
    if (!buf) { fclose(fp); return NULL; }
    if (fread(buf, 1, sz, fp) != (size_t)sz) { free(buf); fclose(fp); return NULL; }
    fclose(fp);
    *out_size = (size_t)sz;
    return buf;
}

static int read_elf_text(const char *obj_path, uint8_t **data, size_t *size) {
    size_t fsize = 0;
    uint8_t *fbuf = read_file_all(obj_path, &fsize);
    if (!fbuf) return -1;

    if (fsize < 64 || memcmp(fbuf, "\x7F" "ELF", 4) != 0) {
        free(fbuf); return -1;
    }

    uint64_t shoff = *(uint64_t *)(fbuf + 40);
    uint16_t shentsize = *(uint16_t *)(fbuf + 58);
    uint16_t shnum = *(uint16_t *)(fbuf + 60);
    uint16_t shstrndx = *(uint16_t *)(fbuf + 62);
    if (shentsize == 0) shentsize = 64;

    /* 读 shstrtab section header 拿 sh_offset */
    if (shoff + (uint64_t)shstrndx * shentsize + 64 > fsize) {
        free(fbuf); return -1;
    }
    uint8_t *shstr_sh = fbuf + shoff + (uint64_t)shstrndx * shentsize;
    uint64_t shstrtab_off = *(uint64_t *)(shstr_sh + SH_OFFSET_OFF);

    for (int i = 0; i < shnum; i++) {
        uint8_t *sh = fbuf + shoff + (uint64_t)i * shentsize;
        uint32_t name_off = *(uint32_t *)(sh + SH_NAME_OFF);
        uint64_t off = *(uint64_t *)(sh + SH_OFFSET_OFF);
        uint64_t sz = *(uint64_t *)(sh + SH_SIZE_OFF);

        const char *name = (const char *)(fbuf + shstrtab_off + name_off);
        if (strcmp(name, ".text") == 0) {
            if (off + sz > fsize) { free(fbuf); return -1; }
            *data = (uint8_t *)malloc(sz);
            if (!*data) { free(fbuf); return -1; }
            memcpy(*data, fbuf + off, sz);
            *size = sz;
            free(fbuf);
            return 0;
        }
    }
    free(fbuf);
    return -1;
}

/* ---- 从 ELF .o 读取 .rodata 段 ---- */
/* 返回 .rodata 数据 (malloc), 同时返回 .rodata 段的 shndx 供重定位使用 */
static int read_elf_rodata(const char *obj_path, uint8_t **data, size_t *size,
                           uint16_t *rodata_shndx) {
    *data = NULL; *size = 0;
    if (rodata_shndx) *rodata_shndx = 0;

    size_t fsize = 0;
    uint8_t *fbuf = read_file_all(obj_path, &fsize);
    if (!fbuf) return -1;

    if (fsize < 64 || memcmp(fbuf, "\x7F" "ELF", 4) != 0) {
        free(fbuf); return -1;
    }

    uint64_t shoff = *(uint64_t *)(fbuf + 40);
    uint16_t shentsize = *(uint16_t *)(fbuf + 58);
    uint16_t shnum = *(uint16_t *)(fbuf + 60);
    uint16_t shstrndx = *(uint16_t *)(fbuf + 62);
    if (shentsize == 0) shentsize = 64;

    if (shoff + (uint64_t)shstrndx * shentsize + 64 > fsize) {
        free(fbuf); return -1;
    }
    uint8_t *shstr_sh = fbuf + shoff + (uint64_t)shstrndx * shentsize;
    uint64_t shstrtab_off = *(uint64_t *)(shstr_sh + SH_OFFSET_OFF);

    for (int i = 0; i < shnum; i++) {
        uint8_t *sh = fbuf + shoff + (uint64_t)i * shentsize;
        uint32_t name_off = *(uint32_t *)(sh + SH_NAME_OFF);
        uint64_t off = *(uint64_t *)(sh + SH_OFFSET_OFF);
        uint64_t sz = *(uint64_t *)(sh + SH_SIZE_OFF);

        const char *name = (const char *)(fbuf + shstrtab_off + name_off);
        if (strcmp(name, ".rodata") == 0) {
            if (sz == 0) { free(fbuf); return 0; }  /* 空 .rodata */
            if (off + sz > fsize) { free(fbuf); return -1; }
            *data = (uint8_t *)malloc(sz);
            if (!*data) { free(fbuf); return -1; }
            memcpy(*data, fbuf + off, sz);
            *size = sz;
            if (rodata_shndx) *rodata_shndx = (uint16_t)i;
            free(fbuf);
            return 0;
        }
    }
    /* 无 .rodata 段是正常的 */
    free(fbuf);
    return 0;
}

/* ---- 从 ELF .o 读取导出符号 (全局函数) ---- */
/* ELF 符号表 st_info: 高4位=bind, 低4位=type */
#define ELF_ST_BIND(info)  ((info) >> 4)
#define ELF_ST_TYPE(info)  ((info) & 0xf)

#define STB_GLOBAL  1
#define STB_WEAK    2
#define STT_FUNC    2
#define SHN_UNDEF   0

typedef struct {
    char name[64];
    uint64_t value;     /* 符号在 .text 段中的偏移 */
} export_sym_t;

static int read_elf_exports(const char *obj_path, export_sym_t **out, size_t *out_n) {
    *out = NULL; *out_n = 0;
    size_t fsize = 0;
    uint8_t *fbuf = read_file_all(obj_path, &fsize);
    if (!fbuf) return -1;

    uint64_t shoff = *(uint64_t *)(fbuf + 40);
    uint16_t shentsize = *(uint16_t *)(fbuf + 58);
    uint16_t shnum = *(uint16_t *)(fbuf + 60);
    uint16_t shstrndx = *(uint16_t *)(fbuf + 62);
    if (shentsize == 0) shentsize = 64;

    if (shoff + (uint64_t)shstrndx * shentsize + 64 > fsize) { free(fbuf); return -1; }
    uint8_t *shstr_sh = fbuf + shoff + (uint64_t)shstrndx * shentsize;
    uint64_t shstrtab_off = *(uint64_t *)(shstr_sh + SH_OFFSET_OFF);

    /* 找 .text section 的 shndx */
    uint16_t text_shndx = 0;
    for (int i = 0; i < shnum; i++) {
        uint8_t *sh = fbuf + shoff + (uint64_t)i * shentsize;
        uint32_t name_off = *(uint32_t *)(sh + SH_NAME_OFF);
        const char *name = (const char *)(fbuf + shstrtab_off + name_off);
        if (strcmp(name, ".text") == 0) { text_shndx = (uint16_t)i; break; }
    }

    /* 找符号表 (.symtab) */
    for (int i = 0; i < shnum; i++) {
        uint8_t *sh = fbuf + shoff + (uint64_t)i * shentsize;
        uint32_t sh_type = *(uint32_t *)(sh + SH_TYPE_OFF);
        if (sh_type != 2) continue;  /* SHT_SYMTAB = 2 */

        uint64_t sym_off = *(uint64_t *)(sh + SH_OFFSET_OFF);
        uint64_t sym_sz = *(uint64_t *)(sh + SH_SIZE_OFF);
        uint32_t sym_link = *(uint32_t *)(sh + SH_LINK_OFF);  /* strtab index */
        uint64_t sym_entsize = *(uint64_t *)(sh + SH_ENTSIZE_OFF);
        if (sym_entsize == 0) sym_entsize = SYM_SIZE;

        /* 找 strtab */
        if (sym_link >= shnum) continue;
        uint8_t *str_sh = fbuf + shoff + (uint64_t)sym_link * shentsize;
        uint64_t strtab_off = *(uint64_t *)(str_sh + SH_OFFSET_OFF);

        size_t nsyms = sym_sz / sym_entsize;
        export_sym_t *list = (export_sym_t *)calloc(nsyms, sizeof(export_sym_t));
        if (!list) { free(fbuf); return -1; }
        size_t count = 0;

        for (size_t j = 0; j < nsyms; j++) {
            uint8_t *sym = fbuf + sym_off + j * sym_entsize;
            uint32_t st_name = *(uint32_t *)(sym + SYM_NAME_OFF);
            uint8_t  st_info = *(sym + SYM_INFO_OFF);
            uint16_t st_shndx = *(uint16_t *)(sym + SYM_SHNDX_OFF);
            uint64_t st_value = *(uint64_t *)(sym + SYM_VALUE_OFF);

            /* 只要全局/弱符号, 类型为 FUNC, 定义在 .text 中 */
            int bind = ELF_ST_BIND(st_info);
            int type = ELF_ST_TYPE(st_info);
            if ((bind == STB_GLOBAL || bind == STB_WEAK) &&
                type == STT_FUNC && st_shndx == text_shndx && st_name != 0) {
                const char *nm = (const char *)(fbuf + strtab_off + st_name);
                strncpy(list[count].name, nm, 63);
                list[count].name[63] = 0;
                list[count].value = st_value;
                count++;
            }
        }
        if (count > 0) {
            *out = list;
            *out_n = count;
        } else {
            free(list);
        }
        free(fbuf);
        return 0;
    }
    free(fbuf);
    return 0;
}

/* ---- 从 ELF .o 读取重定位 (跨文件 call) ---- */
typedef struct {
    uint64_t r_offset;  /* .text 内偏移 */
    uint32_t sym_idx;   /* 符号表索引 */
    char sym_name[64];  /* 解析后的符号名 */
} reloc_ref_t;

static int read_elf_relocs(const char *obj_path, uint16_t rodata_shndx,
                           uint16_t text_shndx,
                           reloc_ref_t **out, size_t *out_n) {
    *out = NULL; *out_n = 0;
    size_t fsize = 0;
    uint8_t *fbuf = read_file_all(obj_path, &fsize);
    if (!fbuf) return -1;

    uint64_t shoff = *(uint64_t *)(fbuf + 40);
    uint16_t shentsize = *(uint16_t *)(fbuf + 58);
    uint16_t shnum = *(uint16_t *)(fbuf + 60);
    if (shentsize == 0) shentsize = 64;

    /* 先找符号表, 缓存 symtab+strtab 信息 */
    uint8_t *symtab_ptr = NULL;
    uint64_t symtab_entsz = 24;
    const char *strtab_ptr = NULL;

    for (int i = 0; i < shnum; i++) {
        uint8_t *sh = fbuf + shoff + (uint64_t)i * shentsize;
        uint32_t sh_type = *(uint32_t *)(sh + SH_TYPE_OFF);
        if (sh_type == 2) {  /* SHT_SYMTAB */
            symtab_ptr = fbuf + *(uint64_t *)(sh + SH_OFFSET_OFF);
            symtab_entsz = *(uint64_t *)(sh + SH_ENTSIZE_OFF);
            if (symtab_entsz == 0) symtab_entsz = SYM_SIZE;
            uint32_t link = *(uint32_t *)(sh + SH_LINK_OFF);
            if (link < shnum) {
                uint8_t *str_sh = fbuf + shoff + (uint64_t)link * shentsize;
                strtab_ptr = (const char *)(fbuf + *(uint64_t *)(str_sh + SH_OFFSET_OFF));
            }
            break;
        }
    }
    if (!symtab_ptr || !strtab_ptr) { free(fbuf); return 0; }

    /* 遍历 .rela.text 段 */
    reloc_ref_t *list = NULL;
    size_t count = 0, cap = 0;

    for (int i = 0; i < shnum; i++) {
        uint8_t *sh = fbuf + shoff + (uint64_t)i * shentsize;
        uint32_t sh_type = *(uint32_t *)(sh + SH_TYPE_OFF);
        if (sh_type != 4) continue;  /* SHT_RELA = 4 */

        /* 只处理 .rela.text (sh_info 指向 .text 段), 跳过 .rela.eh_frame 等 */
        uint32_t sh_info = *(uint32_t *)(sh + SH_INFO_OFF);
        if (text_shndx != 0 && sh_info != text_shndx) continue;

        uint64_t rela_off = *(uint64_t *)(sh + SH_OFFSET_OFF);
        uint64_t rela_sz = *(uint64_t *)(sh + SH_SIZE_OFF);
        uint64_t rela_entsz = *(uint64_t *)(sh + SH_ENTSIZE_OFF);
        if (rela_entsz == 0) rela_entsz = RELA_SIZE;
        size_t n = rela_sz / rela_entsz;

        for (size_t j = 0; j < n; j++) {
            uint8_t *r = fbuf + rela_off + j * rela_entsz;
            uint64_t r_offset = *(uint64_t *)(r + RELA_OFFSET_OFF);
            uint64_t r_info = *(uint64_t *)(r + RELA_INFO_OFF);
            uint32_t sym_idx = (uint32_t)(r_info >> 32);
            uint32_t type = (uint32_t)(r_info & 0xffffffff);

            /* 只处理 PC32/PLT32 (call/jmp rel32) */
            if (type != R_X86_64_PC32 && type != R_X86_64_PLT32)
                continue;

            if (sym_idx == 0) continue;
            uint8_t *sym = symtab_ptr + (uint64_t)sym_idx * symtab_entsz;
            uint32_t st_name = *(uint32_t *)(sym + SYM_NAME_OFF);
            const char *nm = strtab_ptr + st_name;
            if (!nm[0]) continue;

            uint16_t st_shndx = *(uint16_t *)(sym + SYM_SHNDX_OFF);
            if (st_shndx == SHN_UNDEF) {
                /* undefined 符号: 如果是 sys_/kenux_ API 则跳过 (走导入表)
                 * 否则是库函数调用, 需要生成重定位 */
                if (kex_api_lookup_name(nm) != NULL) continue;
            }
            /* .rodata 引用由本地重定位处理, 此处跳过 */
            if (rodata_shndx != 0 && st_shndx == rodata_shndx) continue;
            /* .text 内部定义的符号: 本地函数调用, 不生成跨文件重定位
             * (这些由 apply_text_relocs 在编译时直接修补到代码缓冲区) */
            if (text_shndx != 0 && st_shndx == text_shndx) continue;

            if (count == cap) {
                cap = cap ? cap * 2 : 8;
                list = (reloc_ref_t *)realloc(list, cap * sizeof(*list));
            }
            list[count].r_offset = r_offset;
            list[count].sym_idx = sym_idx;
            strncpy(list[count].sym_name, nm, 63);
            list[count].sym_name[63] = 0;
            count++;
        }
    }

    *out = list;
    *out_n = count;
    free(fbuf);
    return 0;
}

/* ---- 从 ELF .o 读取 .text 内部重定位 (本地函数调用) ---- */
/* 这些是 .text 中调用同文件 .text 内定义函数的 R_X86_64_PC32/PLT32 重定位.
 * 编译时直接修补到代码缓冲区 (call rel32 的目标在同一段内). */
typedef struct {
    uint64_t r_offset;  /* .text 内 call 操作数偏移 */
    uint64_t sym_value; /* 目标函数在 .text 中的偏移 */
    int64_t  addend;    /* r_addend */
} text_reloc_ref_t;

static int read_elf_text_relocs(const char *obj_path, uint16_t text_shndx,
                                text_reloc_ref_t **out, size_t *out_n) {
    *out = NULL; *out_n = 0;
    if (text_shndx == 0) return 0;

    size_t fsize = 0;
    uint8_t *fbuf = read_file_all(obj_path, &fsize);
    if (!fbuf) return -1;

    uint64_t shoff = *(uint64_t *)(fbuf + 40);
    uint16_t shentsize = *(uint16_t *)(fbuf + 58);
    uint16_t shnum = *(uint16_t *)(fbuf + 60);
    if (shentsize == 0) shentsize = 64;

    /* 找符号表 */
    uint8_t *symtab_ptr = NULL;
    uint64_t symtab_entsz = 24;
    for (int i = 0; i < shnum; i++) {
        uint8_t *sh = fbuf + shoff + (uint64_t)i * shentsize;
        uint32_t sh_type = *(uint32_t *)(sh + SH_TYPE_OFF);
        if (sh_type == 2) {
            symtab_ptr = fbuf + *(uint64_t *)(sh + SH_OFFSET_OFF);
            symtab_entsz = *(uint64_t *)(sh + SH_ENTSIZE_OFF);
            if (symtab_entsz == 0) symtab_entsz = SYM_SIZE;
            break;
        }
    }
    if (!symtab_ptr) { free(fbuf); return 0; }

    text_reloc_ref_t *list = NULL;
    size_t count = 0, cap = 0;

    for (int i = 0; i < shnum; i++) {
        uint8_t *sh = fbuf + shoff + (uint64_t)i * shentsize;
        uint32_t sh_type = *(uint32_t *)(sh + SH_TYPE_OFF);
        if (sh_type != 4) continue;

        /* 只处理 .rela.text (sh_info 指向 .text 段), 跳过 .rela.eh_frame 等 */
        uint32_t sh_info = *(uint32_t *)(sh + SH_INFO_OFF);
        if (sh_info != text_shndx) continue;

        uint64_t rela_off = *(uint64_t *)(sh + SH_OFFSET_OFF);
        uint64_t rela_sz = *(uint64_t *)(sh + SH_SIZE_OFF);
        uint64_t rela_entsz = *(uint64_t *)(sh + SH_ENTSIZE_OFF);
        if (rela_entsz == 0) rela_entsz = RELA_SIZE;
        size_t n = rela_sz / rela_entsz;

        for (size_t j = 0; j < n; j++) {
            uint8_t *r = fbuf + rela_off + j * rela_entsz;
            uint64_t r_offset = *(uint64_t *)(r + RELA_OFFSET_OFF);
            uint64_t r_info = *(uint64_t *)(r + RELA_INFO_OFF);
            int64_t  r_addend = *(int64_t *)(r + RELA_ADDEND_OFF);
            uint32_t sym_idx = (uint32_t)(r_info >> 32);
            uint32_t type = (uint32_t)(r_info & 0xffffffff);

            if (type != R_X86_64_PC32 && type != R_X86_64_PLT32) continue;
            if (sym_idx == 0) continue;

            uint8_t *sym = symtab_ptr + (uint64_t)sym_idx * symtab_entsz;
            uint16_t st_shndx = *(uint16_t *)(sym + SYM_SHNDX_OFF);
            uint64_t st_value = *(uint64_t *)(sym + SYM_VALUE_OFF);

            /* 只要 .text 内部定义的符号 */
            if (st_shndx != text_shndx) continue;

            if (count == cap) {
                cap = cap ? cap * 2 : 8;
                list = (text_reloc_ref_t *)realloc(list, cap * sizeof(*list));
            }
            list[count].r_offset = r_offset;
            list[count].sym_value = st_value;
            list[count].addend = r_addend;
            count++;
        }
    }

    *out = list;
    *out_n = count;
    free(fbuf);
    return 0;
}

/* ---- 应用 .text 内部重定位 (本地函数调用) ---- */
/* 编译时直接修补 call rel32 操作数, 目标在同一段内.
 * code: 代码缓冲区, code_size: 代码大小 */
static int apply_text_relocs(uint8_t *code, size_t code_size,
                             const text_reloc_ref_t *relocs, size_t n) {
    for (size_t i = 0; i < n; i++) {
        uint64_t call_off = relocs[i].r_offset;     /* 操作数在 .text 中的偏移 */
        uint64_t target = relocs[i].sym_value;       /* 目标在 .text 中的偏移 */
        int64_t addend = relocs[i].addend;

        /* 边界检查: call_off + 4 不能超过 code_size */
        if (call_off + 4 > code_size) {
            fprintf(stderr, "  .text 重定位 [%zu] 越界: off=0x%lx\n", i, call_off);
            continue;
        }

        /* R_X86_64_PC32/PLT32 标准公式: S + A - P
         *   S = target (目标函数在 .text 中的偏移)
         *   A = addend (通常为 -4, 已含下一指令偏移修正)
         *   P = call_off (被修补的操作数偏移, 即 r_offset)
         * call 指令: E8 dd dd dd dd, CPU 执行时跳转 = RIP + 修补值
         *   RIP = call_off + 4 (下一指令), 验证: RIP + (S+A-P) = S+A+4 = S (当A=-4) */
        int32_t rel = (int32_t)((int64_t)(target + addend) - (int64_t)call_off);
        *(int32_t *)(code + call_off) = rel;
    }
    return 0;
}

/* ---- 从 ELF .o 读取本地重定位 (.rodata 段引用) ---- */
/* 这些是 .text 中引用 .rodata 段符号的 R_X86_64_PC32/PLT32/32S 重定位.
 * 生成: patch_off (代码区内 disp32 偏移) + sym_value (.rodata 内偏移).
 * 加载时由解释器根据 .rodata 实际加载地址修补.
 */
typedef struct {
    uint64_t r_offset;  /* .text 内偏移 (disp32 操作数位置) */
    uint64_t sym_value; /* 符号在 .rodata 中的偏移 */
    uint32_t type;      /* 重定位类型 */
    int64_t  addend;    /* r_addend */
} local_reloc_ref_t;

static int read_elf_local_relocs(const char *obj_path, uint16_t rodata_shndx,
                                 uint16_t text_shndx,
                                 local_reloc_ref_t **out, size_t *out_n) {
    *out = NULL; *out_n = 0;
    if (rodata_shndx == 0) return 0;  /* 无 .rodata 段 */

    size_t fsize = 0;
    uint8_t *fbuf = read_file_all(obj_path, &fsize);
    if (!fbuf) return -1;

    uint64_t shoff = *(uint64_t *)(fbuf + 40);
    uint16_t shentsize = *(uint16_t *)(fbuf + 58);
    uint16_t shnum = *(uint16_t *)(fbuf + 60);
    if (shentsize == 0) shentsize = 64;

    /* 找符号表, 缓存 symtab+strtab 信息 */
    uint8_t *symtab_ptr = NULL;
    uint64_t symtab_entsz = 24;
    const char *strtab_ptr = NULL;

    for (int i = 0; i < shnum; i++) {
        uint8_t *sh = fbuf + shoff + (uint64_t)i * shentsize;
        uint32_t sh_type = *(uint32_t *)(sh + SH_TYPE_OFF);
        if (sh_type == 2) {  /* SHT_SYMTAB */
            symtab_ptr = fbuf + *(uint64_t *)(sh + SH_OFFSET_OFF);
            symtab_entsz = *(uint64_t *)(sh + SH_ENTSIZE_OFF);
            if (symtab_entsz == 0) symtab_entsz = SYM_SIZE;
            uint32_t link = *(uint32_t *)(sh + SH_LINK_OFF);
            if (link < shnum) {
                uint8_t *str_sh = fbuf + shoff + (uint64_t)link * shentsize;
                strtab_ptr = (const char *)(fbuf + *(uint64_t *)(str_sh + SH_OFFSET_OFF));
            }
            break;
        }
    }
    if (!symtab_ptr || !strtab_ptr) { free(fbuf); return 0; }

    local_reloc_ref_t *list = NULL;
    size_t count = 0, cap = 0;

    for (int i = 0; i < shnum; i++) {
        uint8_t *sh = fbuf + shoff + (uint64_t)i * shentsize;
        uint32_t sh_type = *(uint32_t *)(sh + SH_TYPE_OFF);
        if (sh_type != 4) continue;  /* SHT_RELA = 4 */

        /* 只处理 .rela.text (sh_info 指向 .text 段), 跳过 .rela.eh_frame 等 */
        uint32_t sh_info = *(uint32_t *)(sh + SH_INFO_OFF);
        if (text_shndx != 0 && sh_info != text_shndx) continue;

        uint64_t rela_off = *(uint64_t *)(sh + SH_OFFSET_OFF);
        uint64_t rela_sz = *(uint64_t *)(sh + SH_SIZE_OFF);
        uint64_t rela_entsz = *(uint64_t *)(sh + SH_ENTSIZE_OFF);
        if (rela_entsz == 0) rela_entsz = RELA_SIZE;
        size_t n = rela_sz / rela_entsz;

        for (size_t j = 0; j < n; j++) {
            uint8_t *r = fbuf + rela_off + j * rela_entsz;
            uint64_t r_offset = *(uint64_t *)(r + RELA_OFFSET_OFF);
            uint64_t r_info = *(uint64_t *)(r + RELA_INFO_OFF);
            int64_t  r_addend = *(int64_t *)(r + RELA_ADDEND_OFF);
            uint32_t sym_idx = (uint32_t)(r_info >> 32);
            uint32_t type = (uint32_t)(r_info & 0xffffffff);

            /* 处理 PC32/PLT32 (RIP相对) 和 32S (绝对32位) */
            if (type != R_X86_64_PC32 && type != R_X86_64_PLT32 &&
                type != R_X86_64_32S && type != R_X86_64_32)
                continue;

            if (sym_idx == 0) continue;
            uint8_t *sym = symtab_ptr + (uint64_t)sym_idx * symtab_entsz;
            uint16_t st_shndx = *(uint16_t *)(sym + SYM_SHNDX_OFF);
            uint64_t st_value = *(uint64_t *)(sym + SYM_VALUE_OFF);

            /* 只要引用 .rodata 段的符号 */
            if (st_shndx != rodata_shndx) continue;

            if (count == cap) {
                cap = cap ? cap * 2 : 8;
                list = (local_reloc_ref_t *)realloc(list, cap * sizeof(*list));
            }
            list[count].r_offset = r_offset;
            /* 计算符号在 .rodata 中的实际偏移:
             *   R_X86_64_PC32/PLT32 (RIP相对): addend 已含 -4 (下一指令偏移),
             *     实际偏移 = st_value + addend + 4
             *   R_X86_64_32S/32 (绝对32位): 实际偏移 = st_value + addend
             * st_value 对于段符号 (.rodata) 通常为 0, 偏移编码在 addend 中. */
            if (type == R_X86_64_PC32 || type == R_X86_64_PLT32) {
                list[count].sym_value = st_value + (uint64_t)(r_addend + 4);
            } else {
                list[count].sym_value = st_value + (uint64_t)r_addend;
            }
            list[count].type = type;
            list[count].addend = r_addend;
            count++;
        }
    }

    *out = list;
    *out_n = count;
    free(fbuf);
    return 0;
}

/* ---- 扫描源码, 收集导入 API ---- */
typedef struct {
    const kex_api_entry_t *entry;
} import_ref_t;

static int is_ident_char(int c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '_';
}

/* kex_ 便利函数隐含的 syscall 依赖 */
static const char *kex_util_deps[] = {
    "kex_puts",      "sys_write",
    "kex_putchar",   "sys_write",
    "kex_printlong", "sys_write",
    "kex_printhex",  "sys_write",
    "kex_strlen",    NULL,
    "kex_strcmp",    NULL,
    "kex_memcpy",    NULL,
    "kex_memset",    NULL,
    NULL
};

/* 添加一个导入 (去重) */
static int add_import(import_ref_t **list, size_t *count, size_t *cap,
                      const kex_api_entry_t *e) {
    for (size_t i = 0; i < *count; i++)
        if ((*list)[i].entry == e) return 0;
    if (*count == *cap) {
        *cap = *cap ? *cap * 2 : 8;
        *list = realloc(*list, *cap * sizeof(**list));
    }
    (*list)[(*count)++].entry = e;
    return 1;
}

static int collect_imports(const char *src, import_ref_t **out, size_t *out_n) {
    import_ref_t *list = NULL;
    size_t count = 0, cap = 0;

    const char *p = src;
    while (*p) {
        /* 识别 sys_ 或 kenux_ 前缀 */
        if ((strncmp(p, "sys_", 4) == 0) || (strncmp(p, "kenux_", 6) == 0)) {
            /* 确认前一个字符非标识符 (避免匹配 substring) */
            if (p == src || !is_ident_char((unsigned char)p[-1])) {
                char ident[64] = {0};
                size_t n = 0;
                while (is_ident_char((unsigned char)p[n]) && n < 63) {
                    ident[n] = p[n]; n++;
                }
                const kex_api_entry_t *e = kex_api_lookup_name(ident);
                if (e) {
                    add_import(&list, &count, &cap, e);
                }
                p += n;
                continue;
            }
        }
        /* 识别 kex_ 便利函数, 自动添加其隐含的 syscall 依赖 */
        if (strncmp(p, "kex_", 4) == 0) {
            if (p == src || !is_ident_char((unsigned char)p[-1])) {
                char ident[64] = {0};
                size_t n = 0;
                while (is_ident_char((unsigned char)p[n]) && n < 63) {
                    ident[n] = p[n]; n++;
                }
                /* 查找便利函数的依赖 */
                for (int d = 0; kex_util_deps[d]; d += 2) {
                    if (strcmp(ident, kex_util_deps[d]) == 0 && kex_util_deps[d+1]) {
                        const kex_api_entry_t *e = kex_api_lookup_name(kex_util_deps[d+1]);
                        if (e) add_import(&list, &count, &cap, e);
                    }
                }
                p += n;
                continue;
            }
        }
        p++;
    }
    *out = list;
    *out_n = count;
    return 0;
}

/* ---- 组装 .kex/.kxp ---- */
static int build_kex(const char *obj_path, const char *out_path,
                     const char *prog_name, int is_kxp,
                     import_ref_t *imports, size_t n_imports) {
    uint8_t *code = NULL; size_t code_size = 0;
    if (read_elf_text(obj_path, &code, &code_size) != 0) {
        fprintf(stderr, ANSI_RED "✗ 无法从 %s 读取 .text 段\n" ANSI_RESET, obj_path);
        return 1;
    }

    /* 读取 .rodata 段 */
    uint8_t *rodata = NULL; size_t rodata_size = 0;
    uint16_t rodata_shndx = 0;
    read_elf_rodata(obj_path, &rodata, &rodata_size, &rodata_shndx);

    /* 查找 .text 段的 shndx (用于区分本地函数调用与跨文件调用) */
    uint16_t text_shndx = 0;
    {
        size_t fsize = 0;
        uint8_t *fbuf = read_file_all(obj_path, &fsize);
        if (fbuf) {
            if (fsize >= 64 && memcmp(fbuf, "\x7F" "ELF", 4) == 0) {
                uint64_t shoff = *(uint64_t *)(fbuf + 40);
                uint16_t shentsize = *(uint16_t *)(fbuf + 58);
                uint16_t shnum = *(uint16_t *)(fbuf + 60);
                uint16_t shstrndx = *(uint16_t *)(fbuf + 62);
                if (shentsize == 0) shentsize = 64;
                if (shoff + (uint64_t)shstrndx * shentsize + 64 <= fsize) {
                    uint8_t *shstr_sh = fbuf + shoff + (uint64_t)shstrndx * shentsize;
                    uint64_t shstrtab_off = *(uint64_t *)(shstr_sh + SH_OFFSET_OFF);
                    for (int i = 0; i < shnum; i++) {
                        uint8_t *sh = fbuf + shoff + (uint64_t)i * shentsize;
                        uint32_t name_off = *(uint32_t *)(sh + SH_NAME_OFF);
                        const char *name = (const char *)(fbuf + shstrtab_off + name_off);
                        if (strcmp(name, ".text") == 0) { text_shndx = (uint16_t)i; break; }
                    }
                }
            }
            free(fbuf);
        }
    }

    /* 读取并应用 .text 内部重定位 (本地函数调用, 编译时直接修补) */
    text_reloc_ref_t *text_relocs = NULL; size_t n_text_relocs = 0;
    if (text_shndx != 0) {
        read_elf_text_relocs(obj_path, text_shndx, &text_relocs, &n_text_relocs);
        if (n_text_relocs > 0) {
            apply_text_relocs(code, code_size, text_relocs, n_text_relocs);
            free(text_relocs);
        }
    }

    /* 读取导出符号 (.kxp 用) */
    export_sym_t *exports = NULL; size_t n_exports = 0;
    if (is_kxp) {
        read_elf_exports(obj_path, &exports, &n_exports);
    }

    /* 读取跨文件重定位 (call 到 .kxp 库函数) */
    reloc_ref_t *relocs = NULL; size_t n_relocs = 0;
    read_elf_relocs(obj_path, rodata_shndx, text_shndx, &relocs, &n_relocs);

    /* 读取本地重定位 (.text 引用 .rodata) */
    local_reloc_ref_t *local_relocs = NULL; size_t n_local_relocs = 0;
    if (rodata_shndx != 0) {
        read_elf_local_relocs(obj_path, rodata_shndx, text_shndx,
                              &local_relocs, &n_local_relocs);
    }

    /* 计算各段偏移 */
    uint32_t code_off = KEX_CODE_REGION_OFFSET;
    uint32_t code_end_off = code_off + (uint32_t)code_size;
    uint32_t end_marker_off = code_end_off;                     /* KKKSTOP */

    /* .rodata 段紧跟 KKKSTOP 之后 */
    uint32_t rodata_off = (rodata_size > 0) ? (end_marker_off + KEX_CODE_END_LEN) : 0;
    uint32_t after_rodata = end_marker_off + KEX_CODE_END_LEN +
                            (uint32_t)rodata_size;
    uint32_t import_off = after_rodata;

    /* 导入表字节数 */
    uint32_t import_bytes = 0;
    for (size_t i = 0; i < n_imports; i++) {
        import_bytes += 8 + (uint32_t)strlen(imports[i].entry->name) + 1;
    }
    uint32_t export_off = import_off + import_bytes;

    /* 导出表字节数 (每条: 名字\0 + 4字节偏移) */
    uint32_t export_bytes = 0;
    for (size_t i = 0; i < n_exports; i++) {
        export_bytes += (uint32_t)strlen(exports[i].name) + 1 + 4;
    }
    uint32_t reloc_off = export_off + export_bytes;

    /* 跨文件重定位表字节数 (4字节条目数 + 每条8字节) */
    uint32_t reloc_bytes = n_relocs > 0 ? (4 + (uint32_t)n_relocs * 8) : 0;
    uint32_t local_reloc_off = reloc_off + reloc_bytes;

    /* 本地重定位表字节数 (每条8字节, 无条目数前缀, 由 header 计数) */
    uint32_t local_reloc_bytes = n_local_relocs > 0 ? (uint32_t)n_local_relocs * 8 : 0;
    uint32_t total = local_reloc_off + local_reloc_bytes;

    /* 分配文件缓冲 */
    uint8_t *buf = (uint8_t *)calloc(total, 1);
    if (!buf) {
        free(code); free(rodata); free(exports); free(relocs); free(local_relocs);
        return 1;
    }

    /* 头部 */
    kex_header_t *h = (kex_header_t *)buf;
    h->magic[0] = KEX_MAGIC0; h->magic[1] = KEX_MAGIC1;
    h->magic[2] = KEX_MAGIC2; h->magic[3] = KEX_MAGIC3;
    h->version = KEX_VERSION;
    h->header_size = KEX_HEADER_SIZE;
    h->total_size = total;
    h->entry_offset = code_off;                 /* 入口=代码区起始 */
    h->code_size = (uint32_t)code_size;
    h->import_offset = (n_imports > 0) ? import_off : 0;
    h->import_count = (uint32_t)n_imports;
    h->export_offset = (n_exports > 0) ? export_off : 0;
    h->export_count = (uint32_t)n_exports;
    h->icon_offset = 0;
    h->icon_size = 0;
    h->stack_size = KEX_DEFAULT_STACK;
    h->heap_size = KEX_DEFAULT_HEAP;
    h->crc32 = 0;                               /* 后填 */
    h->file_type = is_kxp ? KEX_FILE_KXP : KEX_FILE_KEX;
    h->flags = 0;
    h->reloc_offset = (n_relocs > 0) ? reloc_off : 0;
    h->rodata_offset = rodata_off;
    h->rodata_size = (uint32_t)rodata_size;
    h->local_reloc_off = (n_local_relocs > 0) ? local_reloc_off : 0;
    h->local_reloc_cnt = (uint32_t)n_local_relocs;

    /* 程序名 */
    char *name_field = (char *)(buf + KEX_HEADER_SIZE);
    if (prog_name) {
        strncpy(name_field, prog_name, KEX_NAME_LEN - 1);
    } else {
        strncpy(name_field, "kex_program", KEX_NAME_LEN - 1);
    }

    /* 开始符 */
    memcpy(buf + KEX_HEADER_SIZE + KEX_NAME_LEN, KEX_CODE_START, KEX_CODE_START_LEN);

    /* 代码 */
    memcpy(buf + code_off, code, code_size);
    free(code);

    /* 结束符 */
    memcpy(buf + end_marker_off, KEX_CODE_END, KEX_CODE_END_LEN);

    /* .rodata 段 */
    if (rodata_size > 0) {
        memcpy(buf + rodata_off, rodata, rodata_size);
        free(rodata);
    }

    /* 导入表 */
    uint32_t off = import_off;
    for (size_t i = 0; i < n_imports; i++) {
        const kex_api_entry_t *e = imports[i].entry;
        *(uint32_t *)(buf + off) = e->crc32;            /* CRC32 */
        *(uint32_t *)(buf + off + 4) = e->syscall_nr;   /* 占位: 暂存 syscall nr */
        strcpy((char *)(buf + off + 8), e->name);
        off += 8 + (uint32_t)strlen(e->name) + 1;
    }

    /* 导出表 (每条: 名字\0 + 4字节代码偏移) */
    off = export_off;
    for (size_t i = 0; i < n_exports; i++) {
        uint32_t nm_len = (uint32_t)strlen(exports[i].name) + 1;
        memcpy(buf + off, exports[i].name, nm_len);
        off += nm_len;
        /* 导出偏移 = 代码区偏移 + 符号在 .text 中的偏移 */
        uint32_t fn_off = code_off + (uint32_t)exports[i].value;
        *(uint32_t *)(buf + off) = fn_off;
        off += 4;
    }
    free(exports);

    /* 跨文件重定位表 (4字节条目数 + 每条8字节) */
    if (n_relocs > 0) {
        off = reloc_off;
        *(uint32_t *)(buf + off) = (uint32_t)n_relocs;
        off += 4;
        for (size_t i = 0; i < n_relocs; i++) {
            /* r_offset 是 call rel32 操作数在 .text 中的偏移
             * 文件偏移 = 代码区偏移 + r_offset (无需 +1, r_offset 已指向操作数) */
            uint32_t call_op_off = code_off + (uint32_t)relocs[i].r_offset;
            uint32_t sym_crc = kex_crc32_str(relocs[i].sym_name);
            *(uint32_t *)(buf + off) = call_op_off;
            *(uint32_t *)(buf + off + 4) = sym_crc;
            off += 8;
        }
        free(relocs);
    }

    /* 本地重定位表 (每条8字节: patch_off + sym_value, 无条目数前缀) */
    if (n_local_relocs > 0) {
        off = local_reloc_off;
        for (size_t i = 0; i < n_local_relocs; i++) {
            /* r_offset 是 disp32 操作数在 .text 中的偏移
             * 文件偏移 = 代码区偏移 + r_offset */
            uint32_t patch_off = code_off + (uint32_t)local_relocs[i].r_offset;
            uint32_t sym_val = (uint32_t)local_relocs[i].sym_value;
            *(uint32_t *)(buf + off) = patch_off;
            *(uint32_t *)(buf + off + 4) = sym_val;
            off += 8;
        }
        free(local_relocs);
    }

    /* CRC32 (字段处置 0) */
    uint32_t crc = kex_crc32(buf, total);
    h->crc32 = crc;

    /* 写文件 */
    FILE *fp = fopen(out_path, "wb");
    if (!fp) { free(buf); fprintf(stderr, ANSI_RED "✗ 无法写 %s\n" ANSI_RESET, out_path); return 1; }
    fwrite(buf, 1, total, fp);
    fclose(fp);
    free(buf);

    printf(ANSI_GREEN "  ✓ " ANSI_RESET "生成 %s (%u 字节)\n", out_path, total);
    printf(ANSI_BLUE "    " ANSI_RESET "入口=0x%x  代码=%zuB  rodata=%zuB  导入=%zu  导出=%zu  跨文件重定位=%zu  本地重定位=%zu  CRC=0x%08x\n",
           code_off, code_size, rodata_size, n_imports, n_exports, n_relocs, n_local_relocs, crc);
    return 0;
}

static void usage(const char *p) {
    banner();
    printf("用法:\n");
    printf("  %s compile <input.c> <output.kex> [--name NAME] [--kxp]\n", p);
    printf("  %s -h | --help\n", p);
    printf("\n选项:\n");
    printf("  --name NAME   设置程序名 (默认 kex_program)\n");
    printf("  --kxp         编译为动态库 (.kxp)\n");
}

/* kexc_main: 供统一入口调用, argc/argv 已去掉子命令 */
int kexc_main(int argc, char **argv) {
    banner();
    if (argc < 2) { usage("kex compile"); return 1; }

    const char *in = NULL, *out = NULL, *name = NULL;
    int is_kxp = 0;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage("kex compile"); return 0;
        } else if (strcmp(argv[i], "--name") == 0 && i + 1 < argc) {
            name = argv[++i];
        } else if (strcmp(argv[i], "--kxp") == 0) {
            is_kxp = 1;
        } else if (!in) {
            in = argv[i];
        } else if (!out) {
            out = argv[i];
        }
    }
    if (!in || !out) { usage("kex compile"); return 1; }

    /* 读取源码 (用于扫描导入) */
    FILE *fp = fopen(in, "rb");
    if (!fp) { fprintf(stderr, ANSI_RED "✗ 无法打开 %s\n" ANSI_RESET, in); return 1; }
    fseek(fp, 0, SEEK_END); long sz = ftell(fp); fseek(fp, 0, SEEK_SET);
    char *src = (char *)malloc(sz + 1);
    fread(src, 1, sz, fp); src[sz] = 0; fclose(fp);

    printf(ANSI_BLUE "▶ " ANSI_RESET "编译源码: %s\n", in);

    /* 扫描导入 */
    import_ref_t *imports = NULL; size_t n_imports = 0;
    collect_imports(src, &imports, &n_imports);
    printf(ANSI_YELLOW "  [1/3] " ANSI_RESET "扫描导入: %zu 个 API\n", n_imports);
    for (size_t i = 0; i < n_imports; i++)
        printf("        %-24s -> syscall %u\n", imports[i].entry->name, imports[i].entry->syscall_nr);

    /* gcc 编译为 .o */
    char obj[1024];
    snprintf(obj, sizeof(obj), "%s.kexc.o", in);

    /* include 路径: 优先环境变量 KEX_INCLUDE, 其次 ./include */
    const char *env_inc = getenv("KEX_INCLUDE");

    char cmd[4096];
    /* -fpic: 让 gcc 用 lea RIP 相对寻址引用 .rodata (R_X86_64_PC32),
     * 而非 mov 绝对地址 (R_X86_64_32S). 后者在运行时 .rodata 加载到
     * 高地址时会溢出 32 位. RIP 相对寻址可被 apply_local_relocs 正确修补. */

#ifdef _WIN32
    /* Windows: kexc.exe 通过 WSL 调用 gcc 生成 ELF 格式 .o
     * (kexc 只能解析 ELF, 不能解析 PE/COFF).
     * 用 wslpath 把 Windows 路径转为 WSL 路径. */
    {
        char abs_inc[1024], abs_in[1024], abs_obj[1024];
        const char *inc_dir = env_inc ? env_inc : "include";
        _fullpath(abs_inc, inc_dir, sizeof(abs_inc));
        _fullpath(abs_in, in, sizeof(abs_in));
        _fullpath(abs_obj, obj, sizeof(abs_obj));

        snprintf(cmd, sizeof(cmd),
            "wsl bash -c \""
            "gcc -m64 -ffreestanding -fpic -fno-stack-protector "
            "-nostdlib -nostartfiles -nodefaultlibs "
            "-I$(wslpath -u '%s') "
            "-c $(wslpath -u '%s') "
            "-o $(wslpath -u '%s') 2>&1"
            "\"",
            abs_inc, abs_in, abs_obj);
    }
#else
    char inc_flag[256];
    if (env_inc) snprintf(inc_flag, sizeof(inc_flag), "-I%s", env_inc);
    else snprintf(inc_flag, sizeof(inc_flag), "-Iinclude");

    snprintf(cmd, sizeof(cmd),
             "gcc -m64 -ffreestanding -fpic -fno-stack-protector "
             "-nostdlib -nostartfiles -nodefaultlibs %s -c %s -o %s 2>&1",
             inc_flag, in, obj);
#endif
    printf(ANSI_YELLOW "  [2/3] " ANSI_RESET "gcc 编译对象文件...\n");
    int gr = system(cmd);
    if (gr != 0) {
        fprintf(stderr, ANSI_RED "✗ gcc 编译失败\n" ANSI_RESET);
        free(src); free(imports); return 1;
    }

    /* 组装 .kex */
    printf(ANSI_YELLOW "  [3/3] " ANSI_RESET "组装 HGS 格式...\n");
    int ret = build_kex(obj, out, name, is_kxp, imports, n_imports);
    remove(obj);

    free(src);
    free(imports);

    if (ret == 0) {
        printf("\n" ANSI_GREEN ANSI_BOLD "✓ 编译完成\n" ANSI_RESET "\n");
    }
    return ret;
}


/* ============================================================
 * kex_interp.c - .kex 解释器 / 加载执行器 (合并, 去掉 KEX_STANDALONE)
 * ============================================================ */
/* kex_interp.c - .kex 解释器 / 加载执行器
 *
 * 在主机 (Linux/WSL) 或 KenuxOS 用户态运行 .kex 程序:
 *   1. 加载并校验 .kex (魔数 / CRC32 / 标记)
 *   2. 校验导入表 (所有 API 须可被 Kenux API 表解析)
 *   3. 生成 syscall stub, 回填导入地址占位
 *   4. 加载 .kxp 依赖库, 建立导出符号表
 *   5. 处理重定位表: 回填 call rel32 目标地址
 *   6. mmap 可执行内存 (先 RW 写入, 再 mprotect 为 RX, 满足 W^X)
 *   7. 分配栈, 跳转到入口
 *
 * 用法: ./kex_interp <program.kex> [args...]
 *       ./kex_interp -L<libdir> <program.kex>
 */

/* ---- 已加载的 .kxp 库 ---- */
typedef struct {
    kex_program_t prog;     /* 解析后的库 */
    void *code_mem;         /* mmap 的代码区 */
    size_t code_alloc;      /* mmap 大小 */
    void *rodata_mem;       /* mmap 的 .rodata 区 (可为 NULL) */
    size_t rodata_alloc;    /* .rodata mmap 大小 */
    char libname[64];       /* 库名 (不含路径) */
} loaded_kxp_t;

#define MAX_LIBS 32
static loaded_kxp_t g_libs[MAX_LIBS];
static int g_nlibs = 0;
static const char *g_libdir = ".";  /* 默认搜索目录 */

/* ---- 已解析的符号 (全局符号表, 动态扩容) ---- */
#define MAX_SYMS 1024
typedef struct {
    char name[64];
    void *addr;             /* 符号实际运行时地址 */
    uint32_t crc32;
} resolved_sym_t;

static resolved_sym_t g_syms[MAX_SYMS];
static int g_nsyms = 0;

/* 添加已解析符号; 成功返回 0, 表满返回 -1 */
static int add_symbol(const char *name, void *addr) {
    if (g_nsyms >= MAX_SYMS) {
        fprintf(stderr, "符号表已满 (%d)\n", MAX_SYMS);
        return -1;
    }
    /* 去重: 同名符号覆盖旧地址 */
    uint32_t crc = kex_crc32_str(name);
    for (int i = 0; i < g_nsyms; i++) {
        if (g_syms[i].crc32 == crc) {
            g_syms[i].addr = addr;
            return 0;
        }
    }
    strncpy(g_syms[g_nsyms].name, name, 63);
    g_syms[g_nsyms].name[63] = 0;
    g_syms[g_nsyms].addr = addr;
    g_syms[g_nsyms].crc32 = crc;
    g_nsyms++;
    return 0;
}

/* 按 CRC32 查找符号 */
static void *lookup_symbol_crc32(uint32_t crc) {
    for (int i = 0; i < g_nsyms; i++)
        if (g_syms[i].crc32 == crc) return g_syms[i].addr;
    return NULL;
}

/* ---- Windows: syscall 模拟层 ---- */
/* .kex 代码中使用 Linux syscall 指令 (0F 05), 在 Windows 上无法直接执行.
 * 方案: 加载代码后扫描 0F 05 替换为 CD 80 (int 0x80),
 * 用 VEH (Vectored Exception Handler) 拦截 int 0x80 触发的异常,
 * 读取寄存器分发到对应的 Windows API. */
#ifdef _WIN32

/* 实现 Linux syscall 到 Windows API 的映射 */
static long kex_do_syscall(uint64_t nr, uint64_t a1, uint64_t a2, uint64_t a3,
                           uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a4; (void)a5; (void)a6;
    switch (nr) {
    case 0: { /* sys_read(fd, buf, count) */
        if (a1 == 0) { /* stdin */
            char *buf = (char *)a2;
            DWORD got = 0;
            HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
            ReadFile(h, buf, (DWORD)a3, &got, NULL);
            return (long)got;
        }
        return (long)_read((int)a1, (void *)a2, (unsigned)a3);
    }
    case 1: { /* sys_write(fd, buf, count) */
        if (a1 == 1 || a1 == 2) {
            DWORD written = 0;
            HANDLE h = (a1 == 1) ? GetStdHandle(STD_OUTPUT_HANDLE)
                                  : GetStdHandle(STD_ERROR_HANDLE);
            WriteFile(h, (void *)a2, (DWORD)a3, &written, NULL);
            return (long)written;
        }
        return (long)_write((int)a1, (void *)a2, (unsigned)a3);
    }
    case 2: { /* sys_open(path, flags, mode) */
        return (long)_open((const char *)a1, (int)a2, (int)a3);
    }
    case 3: /* sys_close(fd) */
        return (long)_close((int)a1);
    case 8: { /* sys_lseek(fd, offset, whence) */
        return (long)_lseek((int)a1, (long)a2, (int)a3);
    }
    case 39: /* sys_getpid */
        return (long)GetCurrentProcessId();
    case 60: /* sys_exit */
        ExitProcess((UINT)a1);
        return 0;
    case 87: /* sys_unlink */
        return DeleteFileA((const char *)a1) ? 0 : -1;
    case 102: /* sys_getuid */
        return 0;
    case 104: /* sys_getgid */
        return 0;
    case 107: /* sys_geteuid */
        return 0;
    case 108: /* sys_getegid */
        return 0;
    case 231: /* exit_group */
        ExitProcess((UINT)a1);
        return 0;
    default:
        fprintf(stderr, "[VEH] 未实现的 syscall %llu\n", (unsigned long long)nr);
        return -1;
    }
}

/* VEH: 拦截 int 0x80 (CD 80) 异常, 分发到 kex_do_syscall.
 * 防递归: kex_do_syscall 内部的 API 调用 (如 WriteFile) 可能触发
 * 新异常, 导致 VEH 递归调用栈溢出. 用 g_in_syscall 标志阻止. */
static BOOL g_in_syscall = FALSE;

static LONG WINAPI kex_veh_handler(PEXCEPTION_POINTERS ep) {
    PEXCEPTION_RECORD er = ep->ExceptionRecord;
    CONTEXT *ctx = ep->ContextRecord;

    /* 防递归: 如果已经在处理 syscall, 不再拦截 */
    if (g_in_syscall)
        return EXCEPTION_CONTINUE_SEARCH;

    /* int 0x80 在 Windows x64 触发 #GP (0xC000001D 或 0xC0000096) */
    if (er->ExceptionCode == 0xC000001D /* STATUS_ILLEGAL_INSTRUCTION */ ||
        er->ExceptionCode == 0xC0000096 /* STATUS_PRIVILEGED_INSTRUCTION */ ||
        er->ExceptionCode == 0xC0000005 /* STATUS_ACCESS_VIOLATION (某些情况) */) {
        uint8_t *rip = (uint8_t *)ctx->Rip;
        if (rip[0] == 0xCD && rip[1] == 0x80) {
            g_in_syscall = TRUE;
            long ret = kex_do_syscall(ctx->Rax, ctx->Rdi, ctx->Rsi, ctx->Rdx,
                                      ctx->R10, ctx->R8, ctx->R9);
            g_in_syscall = FALSE;
            ctx->Rax = (DWORD64)(int64_t)ret;
            ctx->Rip += 2; /* 跳过 CD 80 */
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

/* 扫描代码中的 syscall (0F 05) 替换为 int 0x80 (CD 80) */
static void patch_syscalls(uint8_t *code, size_t size) {
    size_t patched = 0;
    for (size_t i = 0; i + 1 < size; i++) {
        if (code[i] == 0x0F && code[i + 1] == 0x05) {
            code[i] = 0xCD;     /* int 0x80 */
            code[i + 1] = 0x80;
            patched++;
        }
    }
    if (patched > 0)
        printf("  [Win] 替换 %zu 条 syscall -> int 0x80\n", patched);
}

static int g_veh_done = 0;
static void g_veh_init(void) {
    if (g_veh_done) return;          /* GUI 模式下多次 run 只注册一次 */
    AddVectoredExceptionHandler(1, kex_veh_handler);
    g_veh_done = 1;
}

#define KEX_SYSCALL_OP0  0xCD
#define KEX_SYSCALL_OP1  0x80

#else /* Linux */
#define KEX_SYSCALL_OP0  0x0F
#define KEX_SYSCALL_OP1  0x05
static void g_veh_init(void) {}
static void patch_syscalls(uint8_t *code, size_t size) { (void)code; (void)size; }
#endif

/* ---- 生成 syscall stub ---- */
/* 生成: mov eax, <nr>; <syscall>; ret  (共 8 字节)
 * Linux:  syscall = 0F 05
 * Windows: syscall = CD 80 (int 0x80, 由 VEH 拦截)
 * stub 分配在低地址区域 (< 4GB), 保证地址可被 32 位占位容纳 */
#define MAX_STUBS 512
static void *g_stub_page = NULL;
static size_t g_stub_offset = 0;

static void *make_syscall_stub(uint16_t nr) {
    if (!g_stub_page) {
        g_stub_page = mmap_low(4096, PROT_READ | PROT_WRITE | PROT_EXEC);
        if (g_stub_page == MAP_FAILED) {
            g_stub_page = NULL;
            return NULL;
        }
    }
    if (g_stub_offset + 8 > 4096) {
        fprintf(stderr, "stub 页已满\n");
        return NULL;
    }
    uint8_t *s = (uint8_t *)g_stub_page + g_stub_offset;
    g_stub_offset += 8;
    /* stub: B8 xx xx 00 00  CD 80  C3  = mov eax,nr; int 0x80/syscall; ret */
    s[0] = 0xB8;                /* mov eax, imm32 */
    *(uint32_t *)(s + 1) = nr;
    s[5] = KEX_SYSCALL_OP0; s[6] = KEX_SYSCALL_OP1; /* syscall / int 0x80 */
    s[7] = 0xC3;                /* ret */
    return s;
}

/* ---- 边界检查工具 ---- */
/* 校验文件偏移是否在合法范围内 */
static int check_bounds(uint32_t off, uint32_t size, uint32_t total) {
    return (off < total && (uint64_t)off + size <= total);
}

/* 校验 call_off 是否落在代码区内 */
static int check_code_bounds(uint32_t call_off, uint32_t code_off, uint32_t code_size) {
    /* call_off 是操作数偏移, 前面有 1 字节 E8, 后面有 4 字节操作数 */
    if (call_off < code_off + 1) return 0;  /* 不可能: E8 至少在 code_off */
    if (call_off + 4 > code_off + code_size) return 0;
    return 1;
}

/* ---- 加载 .rodata 到独立内存区 (RW) ---- */
/* 返回 mmap 的基址, 失败返回 NULL */
static void *load_rodata(const uint8_t *rodata, uint32_t rodata_size, size_t *out_alloc) {
    if (rodata_size == 0) { *out_alloc = 0; return NULL; }
    size_t alloc = (rodata_size + 4095) & ~4095u;
    void *mem = mmap(NULL, alloc, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) { *out_alloc = 0; return NULL; }
    memcpy(mem, rodata, rodata_size);
    *out_alloc = alloc;
    return mem;
}

/* ---- 应用本地重定位 (修补 .text 中引用 .rodata 的 disp32) ---- */
/* code_base: 代码 mmap 基址, rodata_base: rodata mmap 基址,
 * code_file_off: 代码区文件偏移 (用于换算 patch_off) */
static int apply_local_relocs(void *code_base, void *rodata_base,
                              const kex_local_reloc_entry_t *relocs, uint32_t count,
                              uint32_t code_file_off, uint32_t code_size) {
    for (uint32_t i = 0; i < count; i++) {
        uint32_t patch_off = relocs[i].patch_off;   /* 文件偏移 */
        uint32_t sym_value = relocs[i].sym_value;   /* .rodata 内偏移 */

        /* 边界检查: patch_off 必须在代码区内, 且后面有 4 字节 */
        if (patch_off < code_file_off + 1 ||
            patch_off + 4 > code_file_off + code_size) {
            fprintf(stderr, "  本地重定位 [%u] 越界: patch_off=0x%x\n", i, patch_off);
            continue;
        }

        /* 计算 disp32 操作数在 code_mem 中的偏移 */
        uint32_t off_in_code = patch_off - code_file_off;
        /* RIP 相对: 目标地址 = rodata_base + sym_value
         *          修补值 = 目标地址 - (下一条指令地址)
         *          下一条指令地址 = code_base + off_in_code + 4 */
        uintptr_t target = (uintptr_t)rodata_base + sym_value;
        uintptr_t next_instr = (uintptr_t)code_base + off_in_code + 4;
        int32_t disp = (int32_t)(target - next_instr);

        *(int32_t *)((uint8_t *)code_base + off_in_code) = disp;
    }
    return 0;
}

/* ---- 加载 .kxp 库 ---- */
static int load_kxp(const char *path);

/* 从库目录搜索并加载 .kxp */
__attribute__((unused))
static int load_kxp_by_name(const char *libname) {
    for (int i = 0; i < g_nlibs; i++)
        if (strcmp(g_libs[i].libname, libname) == 0) return 0;
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.kxp", g_libdir, libname);
    return load_kxp(path);
}

static int load_kxp(const char *path) {
    if (g_nlibs >= MAX_LIBS) {
        fprintf(stderr, "已加载库数量达到上限 (%d)\n", MAX_LIBS);
        return -1;
    }

    loaded_kxp_t *lib = &g_libs[g_nlibs];
    int ret = kex_load_file(path, &lib->prog);
    if (ret != KEX_OK) {
        fprintf(stderr, "加载 .kxp 失败: %s (错误 %d)\n", path, ret);
        return -1;
    }

    /* CRC32 校验 (库也必须校验) */
    if (kex_verify_crc32(&lib->prog) != KEX_OK) {
        fprintf(stderr, "错误: .kxp %s CRC32 校验失败, 文件可能损坏\n", path);
        kex_free(&lib->prog);
        return -1;
    }

    /* 从路径提取库名 */
    const char *p = strrchr(path, '/');
    p = p ? p + 1 : path;
    strncpy(lib->libname, p, 63);
    lib->libname[63] = 0;
    char *dot = strrchr(lib->libname, '.');
    if (dot) *dot = 0;

    printf("  加载库: %s (导出=%u)\n", lib->libname, lib->prog.export_count);

    /* 分配可执行内存: 先 RW 写入, 再改 RX (W^X) */
    size_t alloc = (lib->prog.code_size + 4095) & ~4095u;
    lib->code_mem = mmap(NULL, alloc, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (lib->code_mem == MAP_FAILED) {
        fprintf(stderr, "mmap 库代码区失败\n");
        kex_free(&lib->prog);
        return -1;
    }
    lib->code_alloc = alloc;
    memcpy(lib->code_mem, lib->prog.code, lib->prog.code_size);

#ifdef _WIN32
    /* Windows: 库代码也需要 patch syscall */
    patch_syscalls((uint8_t *)lib->code_mem, lib->prog.code_size);
#endif

    /* 加载 .rodata 段到独立内存区 */
    if (lib->prog.rodata_size > 0) {
        lib->rodata_mem = load_rodata(lib->prog.rodata, lib->prog.rodata_size,
                                       &lib->rodata_alloc);
        if (!lib->rodata_mem) {
            fprintf(stderr, "  库 %s .rodata 加载失败\n", lib->libname);
            munmap(lib->code_mem, lib->code_alloc);
            kex_free(&lib->prog);
            return -1;
        }
        printf("    .rodata: %u bytes @ %p\n", lib->prog.rodata_size, lib->rodata_mem);
    }

    /* 应用本地重定位 (修补 .text 中引用 .rodata 的 disp32) */
    if (lib->prog.local_reloc_count > 0 && lib->rodata_mem) {
        apply_local_relocs(lib->code_mem, lib->rodata_mem,
                           lib->prog.local_relocs, lib->prog.local_reloc_count,
                           KEX_CODE_REGION_OFFSET, lib->prog.code_size);
    }

    /* 处理库的导入表: 回填 syscall stub */
    for (uint32_t i = 0; i < lib->prog.import_count; i++) {
        const kex_api_entry_t *e = kex_api_lookup_crc32(lib->prog.imports[i].crc32);
        if (!e) {
            fprintf(stderr, "  库 %s 未解析导入: %s\n",
                    lib->libname, lib->prog.imports[i].name);
            if (lib->rodata_mem) munmap(lib->rodata_mem, lib->rodata_alloc);
            munmap(lib->code_mem, lib->code_alloc);
            kex_free(&lib->prog);
            return -1;
        }
        void *stub = make_syscall_stub(e->syscall_nr);
        if (!stub) {
            fprintf(stderr, "  生成 stub 失败\n");
            if (lib->rodata_mem) munmap(lib->rodata_mem, lib->rodata_alloc);
            munmap(lib->code_mem, lib->code_alloc);
            kex_free(&lib->prog);
            return -1;
        }
        uint32_t slot_off = lib->prog.imports[i].slot_file_off;
        if (!check_bounds(slot_off, 4, lib->prog.raw_size)) {
            fprintf(stderr, "  导入槽位越界: off=0x%x\n", slot_off);
            continue;
        }
        *(uint32_t *)(lib->prog.raw + slot_off) = (uint32_t)(uintptr_t)stub;
    }

    /* 预计算库导出符号的 CRC32 (避免重定位时重复计算) */
    for (uint32_t i = 0; i < lib->prog.export_count; i++) {
        uint32_t off_in_code = lib->prog.exports[i].code_offset - KEX_CODE_REGION_OFFSET;
        void *addr = (uint8_t *)lib->code_mem + off_in_code;
        if (add_symbol(lib->prog.exports[i].name, addr) != 0) {
            if (lib->rodata_mem) munmap(lib->rodata_mem, lib->rodata_alloc);
            munmap(lib->code_mem, lib->code_alloc);
            kex_free(&lib->prog);
            return -1;
        }
        printf("    导出: %s @ %p\n", lib->prog.exports[i].name, addr);
    }

    /* 处理库的跨文件重定位表 */
    for (uint32_t i = 0; i < lib->prog.reloc_count; i++) {
        uint32_t call_off = lib->prog.relocs[i].call_off;
        uint32_t sym_crc  = lib->prog.relocs[i].sym_crc32;

        if (!check_code_bounds(call_off, KEX_CODE_REGION_OFFSET, lib->prog.code_size)) {
            fprintf(stderr, "  库 %s 重定位 [%u] 越界: call_off=0x%x\n",
                    lib->libname, i, call_off);
            continue;
        }

        void *target = lookup_symbol_crc32(sym_crc);
        if (!target) {
            fprintf(stderr, "  库 %s 重定位 [%u] 失败: crc=0x%08x (符号未找到)\n",
                    lib->libname, i, sym_crc);
            continue;
        }

        uint32_t off_in_code = call_off - KEX_CODE_REGION_OFFSET;
        uint8_t *call_addr = (uint8_t *)lib->code_mem + off_in_code - 1;
        int32_t rel = (int32_t)((uintptr_t)target - (uintptr_t)(call_addr + 5));
        *(int32_t *)((uint8_t *)lib->code_mem + off_in_code) = rel;
    }

    /* 代码区改为只读+可执行 (W^X) */
    if (mprotect(lib->code_mem, alloc, PROT_READ | PROT_EXEC) != 0) {
        fprintf(stderr, "  警告: 库 %s mprotect RX 失败\n", lib->libname);
    }
    /* .rodata 改为只读 */
    if (lib->rodata_mem && lib->rodata_alloc > 0) {
        if (mprotect(lib->rodata_mem, lib->rodata_alloc, PROT_READ) != 0) {
            fprintf(stderr, "  警告: 库 %s .rodata mprotect R 失败\n", lib->libname);
        }
    }

    g_nlibs++;
    return 0;
}

/* ---- 设置栈顶并跳转到入口 ---- */
/* 关键: 必须在内联汇编中显式 movq 设置 rsp, 不能依赖 register __asm__("rsp")
 * 因为编译器可能在 prologue 中 push rbp / sub rsp 破坏栈顶设置.
 * 传入参数: %0 = entry, %1 = stack_top
 * 在 asm 中: movq %1, %%rsp; xorq rbp; callq *%0 */
static void run_entry(void *entry, void *stack_top) {
    void (*fn)(void) = (void (*)(void))entry;
#ifdef _WIN32
    /* Windows: .kex 代码里的 syscall 已被替换为 int 0x80 (VEH 处理).
     * 用户代码正常返回后调 ExitProcess. */
    __asm__ volatile (
        "movq %1, %%rsp\n\t"
        "xorq %%rbp, %%rbp\n\t"
        "callq *%0\n\t"
        :
        : "r"(fn), "r"(stack_top)
        : "rcx", "r11", "memory"
    );
    ExitProcess(0);
#else
    __asm__ volatile (
        "movq %1, %%rsp\n\t"
        "xorq %%rbp, %%rbp\n\t"
        "callq *%0\n\t"
        "movl $231, %%eax\n\t"   /* exit_group(0) */
        "xorl %%edi, %%edi\n\t"
        "syscall\n\t"
        :
        : "r"(fn), "r"(stack_top)
        : "rcx", "r11", "memory"
    );
#endif
}

/* kex_interp_main: 供统一入口调用, argc/argv 已去掉子命令 */
int kex_interp_main(int argc, char **argv) {
    const char *prog_path = NULL;

#ifdef _WIN32
    /* Windows: 设置 stdout 为二进制模式, 避免 \n 被转为 \r\n */
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
    /* 初始化 VEH (Vectored Exception Handler) 用于拦截 int 0x80 */
    g_veh_init();
#endif

    /* 解析参数 (argv 从参数开始, 无子命令) */
    for (int i = 0; i < argc; i++) {
        if (strncmp(argv[i], "-L", 2) == 0 && argv[i][2]) {
            g_libdir = argv[i] + 2;
        } else if (!prog_path) {
            prog_path = argv[i];
        }
    }
    if (!prog_path) {
        fprintf(stderr, "用法: kex run [-L<libdir>] <program.kex> [args...]\n");
        return 1;
    }

    /* 加载主程序 */
    kex_program_t prog;
    int ret = kex_load_file(prog_path, &prog);
    if (ret != KEX_OK) {
        fprintf(stderr, "加载失败: %d\n", ret);
        return 1;
    }

    /* CRC32 校验 (强制: 失败则拒绝执行) */
    if (kex_verify_crc32(&prog) != KEX_OK) {
        fprintf(stderr, "错误: CRC32 校验失败, 文件可能损坏或被篡改, 拒绝执行\n");
        kex_free(&prog);
        return 1;
    }

    kex_dump(&prog);

    /* 校验导入表并回填 syscall stub */
    printf("\n--- 导入表处理 ---\n");
    for (uint32_t i = 0; i < prog.import_count; i++) {
        const kex_api_entry_t *e = kex_api_lookup_crc32(prog.imports[i].crc32);
        if (!e) {
            printf("  导入 [%u] %-24s -> 待解析 (库符号)\n", i, prog.imports[i].name);
            continue;
        }
        printf("  导入 [%u] %-24s -> syscall %u\n", i, e->name, e->syscall_nr);
        void *stub = make_syscall_stub(e->syscall_nr);
        if (!stub) {
            fprintf(stderr, "生成 stub 失败\n");
            kex_free(&prog);
            return 1;
        }
        uint32_t slot_off = prog.imports[i].slot_file_off;
        if (!check_bounds(slot_off, 4, prog.raw_size)) {
            fprintf(stderr, "导入槽位越界: off=0x%x\n", slot_off);
            continue;
        }
        *(uint32_t *)(prog.raw + slot_off) = (uint32_t)(uintptr_t)stub;
    }

    /* 分配可执行内存: 先 RW 写入, 之后改 RX (W^X) */
    size_t alloc_size = (prog.code_size + 4095) & ~4095u;
    void *code_mem = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (code_mem == MAP_FAILED) {
        fprintf(stderr, "mmap 代码区失败\n");
        kex_free(&prog);
        return 1;
    }
    memcpy(code_mem, prog.code, prog.code_size);

#ifdef _WIN32
    /* Windows: 扫描 .kex 代码中的 syscall (0F 05), 替换为 int 0x80 (CD 80),
     * 由 VEH 拦截并分发到 Windows API */
    patch_syscalls((uint8_t *)code_mem, prog.code_size);
#endif

    /* 加载 .rodata 段到独立内存区 */
    void *rodata_mem = NULL;
    size_t rodata_alloc = 0;
    if (prog.rodata_size > 0) {
        rodata_mem = load_rodata(prog.rodata, prog.rodata_size, &rodata_alloc);
        if (!rodata_mem) {
            fprintf(stderr, ".rodata 加载失败\n");
            munmap(code_mem, alloc_size);
            kex_free(&prog);
            return 1;
        }
        printf("\n--- .rodata 段 ---\n");
        printf("  大小: %u bytes @ %p\n", prog.rodata_size, rodata_mem);
    }

    /* 应用本地重定位 (修补 .text 中引用 .rodata 的 disp32) */
    if (prog.local_reloc_count > 0 && rodata_mem) {
        printf("\n--- 本地重定位处理 ---\n");
        printf("  共 %u 条\n", prog.local_reloc_count);
        apply_local_relocs(code_mem, rodata_mem,
                           prog.local_relocs, prog.local_reloc_count,
                           KEX_CODE_REGION_OFFSET, prog.code_size);
    }

    /* 处理跨文件重定位表: 回填 call rel32 目标地址 */
    if (prog.reloc_count > 0) {
        printf("\n--- 跨文件重定位处理 ---\n");

        /* 加载库目录下所有 .kxp */
        DIR *dir = opendir(g_libdir);
        if (dir) {
            struct dirent *ent;
            while ((ent = readdir(dir)) != NULL) {
                size_t nlen = strlen(ent->d_name);
                if (nlen < 5 || strcmp(ent->d_name + nlen - 4, ".kxp") != 0)
                    continue;
                char path[512];
                snprintf(path, sizeof(path), "%s/%s", g_libdir, ent->d_name);
                load_kxp(path);
            }
            closedir(dir);
        }

        for (uint32_t i = 0; i < prog.reloc_count; i++) {
            uint32_t call_off = prog.relocs[i].call_off;
            uint32_t sym_crc  = prog.relocs[i].sym_crc32;

            /* 边界检查 */
            if (!check_code_bounds(call_off, KEX_CODE_REGION_OFFSET, prog.code_size)) {
                fprintf(stderr, "  重定位 [%u] 越界: call_off=0x%x (代码区=0x%x+0x%x)\n",
                        i, call_off, KEX_CODE_REGION_OFFSET, prog.code_size);
                continue;
            }

            void *target = lookup_symbol_crc32(sym_crc);
            if (!target) {
                fprintf(stderr, "  重定位 [%u] 失败: crc=0x%08x (符号未找到)\n", i, sym_crc);
                continue;
            }

            uint32_t off_in_code = call_off - KEX_CODE_REGION_OFFSET;
            uint8_t *call_addr = (uint8_t *)code_mem + off_in_code - 1;
            int32_t rel = (int32_t)((uintptr_t)target - (uintptr_t)(call_addr + 5));
            *(int32_t *)((uint8_t *)code_mem + off_in_code) = rel;
            printf("  重定位 [%u] call_off=0x%x -> %p\n", i, call_off, target);
        }
    }

    /* 代码区改为只读+可执行 (W^X) */
    if (mprotect(code_mem, alloc_size, PROT_READ | PROT_EXEC) != 0) {
        fprintf(stderr, "警告: mprotect RX 失败\n");
    }
    /* .rodata 改为只读 */
    if (rodata_mem && rodata_alloc > 0) {
        if (mprotect(rodata_mem, rodata_alloc, PROT_READ) != 0) {
            fprintf(stderr, "警告: .rodata mprotect R 失败\n");
        }
    }

    /* 分配栈 (RW) */
    size_t stack_alloc = (prog.header.stack_size + 4095) & ~4095u;
    if (stack_alloc == 0) stack_alloc = KEX_DEFAULT_STACK;
    void *stack = mmap(NULL, stack_alloc,
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (stack == MAP_FAILED) {
        fprintf(stderr, "mmap 栈失败\n");
        if (rodata_mem) munmap(rodata_mem, rodata_alloc);
        munmap(code_mem, alloc_size);
        kex_free(&prog);
        return 1;
    }
    /* 栈顶 16 字节对齐 (SysV ABI: call 前 rsp%16==0) */
    void *stack_top = (void *)(((uintptr_t)stack + stack_alloc) & ~15UL);

    /* 入口地址校验 */
    uint32_t code_region = KEX_CODE_REGION_OFFSET;
    if (prog.entry_file_offset < code_region ||
        prog.entry_file_offset - code_region >= prog.code_size) {
        fprintf(stderr, "错误: 入口偏移 0x%x 越界 (代码区 0x%x-0x%x)\n",
                prog.entry_file_offset, code_region, code_region + prog.code_size);
        if (rodata_mem) munmap(rodata_mem, rodata_alloc);
        munmap(code_mem, alloc_size);
        munmap(stack, stack_alloc);
        kex_free(&prog);
        return 1;
    }
    uint32_t entry_in_code = prog.entry_file_offset - code_region;
    void *entry = (uint8_t *)code_mem + entry_in_code;

    printf("\n>>> 执行 %s (入口=%p, 栈=%zuKB)\n\n",
           prog.name, entry, stack_alloc / 1024);
    fflush(stdout);  /* 关键: 执行前 flush, 避免被 exit_group 跳过 */

    run_entry(entry, stack_top);

    /* 不会到达 (run_entry 末尾调 exit_group) */
    if (rodata_mem) munmap(rodata_mem, rodata_alloc);
    munmap(code_mem, alloc_size);
    munmap(stack, stack_alloc);
    for (int i = 0; i < g_nlibs; i++) {
        if (g_libs[i].rodata_mem) munmap(g_libs[i].rodata_mem, g_libs[i].rodata_alloc);
        munmap(g_libs[i].code_mem, g_libs[i].code_alloc);
        kex_free(&g_libs[i].prog);
    }
    if (g_stub_page) munmap(g_stub_page, 4096);
    kex_free(&prog);
    return 0;
}


/* ============================================================
 * kex_main.c - KexKit 统一入口 (合并, 去掉前向声明)
 * ============================================================ */
/*
 * kex_main.c - KexKit 统一入口
 *
 * 子命令:
 *   kex compile <input.c> <output.kex> [--name N] [--kxp]   编译
 *   kex run <program.kex> [-L<libdir>] [args...]            运行
 *   kex install  [路径]                                      安装到系统路径
 *   kex version                                              版本信息
 *   kex help                                                 帮助
 */

#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif

#define KEX_VERSION_STR "1.0"

/* 默认安装路径 */
#ifdef _WIN32
/* Windows: 安装到 %LOCALAPPDATA%\KexKit (用户目录, 无需管理员权限,
 * 与 Python/Node 等工具的安装方式一致) */
#define KEX_INSTALL_DIR  "KexKit"
#define KEX_INSTALL_PATH "KexKit\\kex.exe"
#else
#define KEX_INSTALL_DIR "/usr/local/bin"
#define KEX_INSTALL_PATH "/usr/local/bin/kex"
#endif

static void print_banner(void) {
    printf("\n");
    printf("+------------------------------------------------------------+\n");
    printf("|          KexKit - Unified Tool  v%s                        |\n", KEX_VERSION_STR);
    printf("|          compile / run / HGS format                        |\n");
    printf("+------------------------------------------------------------+\n");
    printf("\n");
}

static void print_help(void) {
    print_banner();
    printf("用法: kex <command> [options]\n\n");
    printf("命令:\n");
    printf("  compile <input.c> <output.kex> [--name NAME] [--kxp]\n");
    printf("          编译 C 源码为 .kex (HGS 格式)\n");
    printf("  run <program.kex> [-L<libdir>] [args...]\n");
    printf("          运行 .kex 程序\n");
    printf("  install [路径]  安装 kex (默认: %%LOCALAPPDATA%%\\%s)\n", KEX_INSTALL_PATH);
    printf("          Windows 自动添加 PATH, 安装后新开终端即可用 kex\n");
    printf("  version 显示版本信息\n");
    printf("  help    显示此帮助\n");
    printf("\n");
}

/* ---- kex install: 自安装到系统路径 ---- */
static int kex_install_self(const char *argv0, const char *target) {
    /* 读取自身可执行文件路径 */
    char self_path[4096];

#ifdef _WIN32
    DWORD len = GetModuleFileNameA(NULL, self_path, sizeof(self_path));
    if (len == 0) {
        fprintf(stderr, "错误: 无法获取自身路径\n");
        return 1;
    }

    /* Windows: 默认安装到 %LOCALAPPDATA%\KexKit\kex.exe
     * (用户目录, 无需管理员权限, 与 Python/Node 等工具一致) */
    char default_dest[MAX_PATH];
    if (!target) {
        char appdata[MAX_PATH];
        DWORD alen = GetEnvironmentVariableA("LOCALAPPDATA", appdata, sizeof(appdata));
        if (alen == 0 || alen >= sizeof(appdata)) {
            fprintf(stderr, "错误: 无法获取 LOCALAPPDATA\n");
            return 1;
        }
        snprintf(default_dest, sizeof(default_dest), "%s\\%s", appdata, KEX_INSTALL_PATH);
        target = default_dest;
    }
#else
    ssize_t len = readlink("/proc/self/exe", self_path, sizeof(self_path) - 1);
    if (len < 0) {
        fprintf(stderr, "错误: 无法获取自身路径\n");
        return 1;
    }
    self_path[len] = '\0';
#endif

    const char *dest = target ? target : KEX_INSTALL_PATH;
    printf("安装 KexKit 到: %s\n", dest);

#ifdef _WIN32
    /* Windows: 用 CopyFileA 复制 (比 fopen/fread/fwrite 更可靠,
     * 能正确处理文件锁定和权限) */
    {
        /* 提取目标目录并创建 */
        char dir_path[1024];
        strncpy(dir_path, dest, sizeof(dir_path) - 1);
        dir_path[sizeof(dir_path) - 1] = 0;
        char *last_slash = strrchr(dir_path, '\\');
        if (last_slash) {
            *last_slash = 0;
            CreateDirectoryA(dir_path, NULL);
        }

        /* 删除旧文件 (如果存在且被锁定, CopyFileA 会失败) */
        DeleteFileA(dest);

        if (CopyFileA(self_path, dest, FALSE)) {
            /* 读取文件大小用于显示 */
            WIN32_FILE_ATTRIBUTE_DATA fa;
            size_t total = 0;
            if (GetFileAttributesExA(dest, GetFileExInfoStandard, &fa)) {
                total = (size_t)fa.nFileSizeLow;
            }
            printf("\xe2\x9c\x93 安装完成 (%zu 字节)\n", total);
        } else {
            DWORD err = GetLastError();
            fprintf(stderr, "错误: 复制失败 (错误码 %lu)\n", err);
            if (err == 5)   fprintf(stderr, "  → 拒绝访问 (文件可能被占用, 请先关闭正在运行的 kex)\n");
            if (err == 3)   fprintf(stderr, "  → 找不到路径\n");
            return 1;
        }
    }
#else
    /* Linux: 用 fopen 复制 */
    FILE *src_fp = fopen(self_path, "rb");
    if (!src_fp) {
        fprintf(stderr, "错误: 无法打开自身 (%s)\n", self_path);
        return 1;
    }

    /* 确保目标目录存在 */
    mkdir("/usr/local", 0755);
    mkdir("/usr/local/bin", 0755);

    FILE *dst_fp = fopen(dest, "wb");
    if (!dst_fp) {
        fprintf(stderr, "错误: 无法写入 %s (需要 root 权限? 请用 sudo)\n", dest);
        fclose(src_fp);
        return 1;
    }

    char buf[65536];
    size_t total = 0;
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), src_fp)) > 0) {
        if (fwrite(buf, 1, n, dst_fp) != n) {
            fprintf(stderr, "错误: 写入失败\n");
            fclose(src_fp);
            fclose(dst_fp);
            return 1;
        }
        total += n;
    }
    fclose(src_fp);
    fclose(dst_fp);
    chmod(dest, 0755);
    printf("✓ 安装完成 (%zu 字节)\n", total);
#endif

#ifdef _WIN32
    /* Windows: 自动添加安装目录到用户 PATH 环境变量 */
    {
        /* 提取安装目录 */
        char install_dir[1024];
        strncpy(install_dir, dest, sizeof(install_dir) - 1);
        install_dir[sizeof(install_dir) - 1] = 0;
        char *last_slash = strrchr(install_dir, '\\');
        if (last_slash) *last_slash = 0;

        /* 读取当前用户 PATH (HKCU\Environment\Path) */
        HKEY hKey;
        LONG rc = RegOpenKeyExA(HKEY_CURRENT_USER, "Environment", 0,
                                KEY_READ | KEY_WRITE, &hKey);
        if (rc == ERROR_SUCCESS) {
            char old_path[32768];
            DWORD path_len = sizeof(old_path);
            DWORD path_type = 0;
            old_path[0] = 0;

            RegQueryValueExA(hKey, "Path", NULL, &path_type,
                            (LPBYTE)old_path, &path_len);
            old_path[(path_len < sizeof(old_path)) ? path_len : sizeof(old_path) - 1] = 0;

            /* 检查是否已在 PATH 中 */
            BOOL already_in_path = FALSE;
            char *p = old_path;
            while (p && *p) {
                char *semi = strchr(p, ';');
                size_t seg_len = semi ? (size_t)(semi - p) : strlen(p);
                if (seg_len > 0) {
                    if (_strnicmp(p, install_dir, seg_len) == 0 &&
                        strlen(install_dir) == seg_len) {
                        already_in_path = TRUE;
                        break;
                    }
                }
                p = semi ? semi + 1 : NULL;
            }

            if (already_in_path) {
                printf("\xe2\x9c\x93 安装目录已在 PATH 中: %s\n", install_dir);
            } else {
                /* 追加到 PATH */
                char new_path[34048];
                if (old_path[0]) {
                    /* 确保 old_path 以分号结尾 */
                    size_t olen = strlen(old_path);
                    if (olen > 0 && old_path[olen - 1] != ';')
                        snprintf(new_path, sizeof(new_path), "%s;%s", old_path, install_dir);
                    else
                        snprintf(new_path, sizeof(new_path), "%s%s", old_path, install_dir);
                } else {
                    strncpy(new_path, install_dir, sizeof(new_path) - 1);
                    new_path[sizeof(new_path) - 1] = 0;
                }

                /* 使用 REG_EXPAND_SZ 如果原来是 expand_sz, 否则用 REG_SZ */
                DWORD new_type = (path_type == REG_EXPAND_SZ) ? REG_EXPAND_SZ : REG_SZ;
                rc = RegSetValueExA(hKey, "Path", 0, new_type,
                                   (const BYTE *)new_path,
                                   (DWORD)(strlen(new_path) + 1));
                if (rc == ERROR_SUCCESS) {
                    printf("\xe2\x9c\x93 已添加到用户 PATH: %s\n", install_dir);
                } else {
                    printf("  警告: 写入 PATH 失败 (错误码 %ld)\n", rc);
                }
            }
            RegCloseKey(hKey);

            /* 广播 WM_SETTINGCHANGE 通知其他程序环境变量已更新 */
            DWORD_PTR result;
            SendMessageTimeoutA(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
                               (LPARAM)"Environment", SMTO_ABORTIFHUNG,
                               100, &result);
        } else {
            printf("  警告: 无法打开注册表 (错误码 %ld)\n", rc);
        }

        printf("\n  \xe2\x9c\x93 现在可以在 cmd / PowerShell 中直接使用:\n");
        printf("    kex compile / kex run / kex version\n");
        printf("  \xe2\x84\xb9 新开的终端窗口会自动生效\n");
    }
#else
    printf("  现在可以在任意目录直接使用: kex compile / kex run / kex version\n");
#endif

    return 0;
}

static void print_version(void) {
    printf("KexKit v%s (HGS format)\n", KEX_VERSION_STR);
    printf("  compiler:  kexc\n");
    printf("  runtime:   kex_interp\n");
#ifdef _WIN32
    printf("  platform:  Windows (x86_64)\n");
#else
    printf("  platform:  Linux (x86_64)\n");
#endif
    printf("\n");
}

#ifndef KEX_NO_MAIN
int main(int argc, char **argv) {
#ifdef _WIN32
    /* Windows: 设置控制台为 UTF-8, 解决中文乱码 */
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    if (argc < 2) {
        print_help();
        return 1;
    }

    /* 子命令分发 */
    const char *cmd = argv[1];

    if (strcmp(cmd, "compile") == 0 || strcmp(cmd, "c") == 0) {
        /* kex compile <args...> → kexc_main(argc-2, argv+2) */
        return kexc_main(argc - 2, argv + 2);
    }

    if (strcmp(cmd, "run") == 0 || strcmp(cmd, "r") == 0) {
        /* kex run <args...> → kex_interp_main(argc-2, argv+2) */
        return kex_interp_main(argc - 2, argv + 2);
    }

    if (strcmp(cmd, "install") == 0 || strcmp(cmd, "i") == 0) {
        /* kex install [路径] — 自安装到系统路径 */
        const char *target = (argc >= 3) ? argv[2] : NULL;
        return kex_install_self(argv[0], target);
    }

    if (strcmp(cmd, "version") == 0 || strcmp(cmd, "-v") == 0 ||
        strcmp(cmd, "--version") == 0) {
        print_version();
        return 0;
    }

    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "-h") == 0 ||
        strcmp(cmd, "--help") == 0) {
        print_help();
        return 0;
    }

    /* 未知命令 */
    fprintf(stderr, "未知命令: %s\n", cmd);
    fprintf(stderr, "运行 'kex help' 查看用法\n");
    return 1;
}
#endif /* KEX_NO_MAIN */
