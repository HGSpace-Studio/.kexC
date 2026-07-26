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
#ifndef KEX_API_H
#define KEX_API_H

#include <stdint.h>
#include <stddef.h>

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

#endif /* KEX_API_H */
