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
#ifndef KEX_CRC32_H
#define KEX_CRC32_H

#include <stdint.h>
#include <stddef.h>

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

#endif /* KEX_CRC32_H */
