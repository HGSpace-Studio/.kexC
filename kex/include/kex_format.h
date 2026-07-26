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
#ifndef KEX_FORMAT_H
#define KEX_FORMAT_H

#include <stdint.h>
#include <stddef.h>

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

#endif /* KEX_FORMAT_H */
