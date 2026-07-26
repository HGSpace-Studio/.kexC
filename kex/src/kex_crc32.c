/*
 * kex_crc32.c - CRC32 (IEEE 802.3) 实现
 */
#include "kex_crc32.h"

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
