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
#include "kex_format.h"
#include "kex_crc32.h"
#include "kex_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
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

/* 独立模式: 编译为 kexc 时提供 main */
#ifdef KEX_STANDALONE
int main(int argc, char **argv) {
    /* argv[0]=kexc, argv[1..]=参数; kexc_main 期望 argv[0]=第一个参数 */
    return kexc_main(argc - 1, argv + 1);
}
#endif
