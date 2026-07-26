/*
 * kex_loader.h - .kex/.kxp 文件加载与解析
 */
#ifndef KEX_LOADER_H
#define KEX_LOADER_H

#include "kex_format.h"
#include <stdint.h>
#include <stddef.h>

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

#endif /* KEX_LOADER_H */
