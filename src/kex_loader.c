/*
 * kex_loader.c - .kex/.kxp 文件加载与解析
 */
#include "kex_loader.h"
#include "kex_crc32.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
