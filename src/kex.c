#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#define KEX_PATH_SEP '\\'
#define KEX_PATH_SEP_STR "\\"
#define KEX_POPEN_MODE "rb"
#define KEX_MKDIR(d) _mkdir(d)
#else
#include <unistd.h>
#include <sys/wait.h>
#include <dlfcn.h>
#define KEX_PATH_SEP '/'
#define KEX_PATH_SEP_STR "/"
#define KEX_POPEN_MODE "r"
#define KEX_MKDIR(d) mkdir(d, 0755)
#endif

#include "kex.h"

#define KEX_MAGIC0 0xF0
#define KEX_MAGIC1 0xA5
#define KEX_MAGIC2 0x48
#define KEX_MAGIC3 0x47
#define KEX_VERSION 0x5300u
#define KEX_HEADER_SIZE 80u
#define KEX_NAME_LEN 16u
#define KEX_FILE_KEX 0u
#define KEX_FILE_KXP 1u
#define KEX_CODE_START "Kenux---"
#define KEX_CODE_END "KKKSTOP"
#define KEX_DEFAULT_STACK (1u*1024u*1024u)
#define KEX_DEFAULT_HEAP  (1u*1024u*1024u)

#pragma pack(push,1)
typedef struct {
    uint8_t  magic[4];
    uint16_t version;
    uint16_t header_size;
    uint32_t total_size;
    uint32_t entry_offset;
    uint32_t code_size;
    uint32_t import_offset;
    uint32_t import_count;
    uint32_t export_offset;
    uint32_t export_count;
    uint32_t icon_offset;
    uint32_t icon_size;
    uint32_t stack_size;
    uint32_t heap_size;
    uint32_t crc32;
    uint16_t file_type;
    uint16_t flags;
    uint32_t reloc_offset;
    uint32_t rodata_offset;
    uint32_t rodata_size;
    uint32_t local_reloc_off;
    uint32_t local_reloc_cnt;
} kex_header_t;

typedef struct {
    uint32_t call_off;
    uint32_t sym_crc32;
} kex_reloc_entry_t;

typedef struct {
    uint32_t patch_off;
    uint32_t sym_value;
} kex_local_reloc_t;
#pragma pack(pop)

typedef struct {
    uint32_t api_crc32;
    uint32_t api_addr;
} kex_import_entry_t;

typedef struct {
    char name[256];
    uint32_t crc32;
} import_ref_t;

static uint32_t crc32_table[256];
static int crc32_table_ready = 0;

static void crc32_init_table(void) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++)
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        crc32_table[i] = c;
    }
    crc32_table_ready = 1;
}

static uint32_t crc32_compute(const void *buf, size_t len) {
    if (!crc32_table_ready) crc32_init_table();
    const uint8_t *p = (const uint8_t *)buf;
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++)
        c = crc32_table[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

static uint32_t crc32_str(const char *s) {
    return crc32_compute(s, strlen(s));
}

static int detect_lang(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return KEX_LANG_C;
    if (strcmp(ext, ".cpp") == 0 || strcmp(ext, ".cxx") == 0 ||
        strcmp(ext, ".cc") == 0 || strcmp(ext, ".CPP") == 0 ||
        strcmp(ext, ".C") == 0)
        return KEX_LANG_CPP;
    return KEX_LANG_C;
}

static bool file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static char *file_read(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    buf[rd] = '\0';
    fclose(f);
    if (out_len) *out_len = rd;
    return buf;
}

static int file_write(const char *path, const void *data, size_t len) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    size_t wr = fwrite(data, 1, len, f);
    fclose(f);
    return wr == len ? 0 : -1;
}

static int mkdirs(const char *path) {
    char tmp[KEX_PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);
    if (len > 0 && (tmp[len-1] == KEX_PATH_SEP || tmp[len-1] == '/'))
        tmp[len-1] = '\0';
    for (char *p = tmp + 1; *p; p++) {
        if (*p == KEX_PATH_SEP || *p == '/') {
            *p = '\0';
            if (!file_exists(tmp)) KEX_MKDIR(tmp);
            *p = KEX_PATH_SEP;
        }
    }
    if (!file_exists(tmp)) KEX_MKDIR(tmp);
    return 0;
}

static int run_cmd(const char *cmd, bool verbose) {
    if (verbose) fprintf(stderr, "[kex] %s\n", cmd);
    int ret = system(cmd);
#ifdef _WIN32
    return ret;
#else
    if (WIFEXITED(ret)) return WEXITSTATUS(ret);
    return -1;
#endif
}

static void get_temp_dir(char *out, size_t sz) {
#ifdef _WIN32
    const char *tmp = getenv("TEMP");
    if (!tmp) tmp = getenv("TMP");
    if (!tmp) tmp = "C:\\Temp";
    snprintf(out, sz, "%s\\kex_%d", tmp, (int)GetCurrentProcessId());
#else
    const char *tmp = getenv("TMPDIR");
    if (!tmp) tmp = "/tmp";
    snprintf(out, sz, "%s/kex_%d", tmp, (int)getpid());
#endif
}

static void get_basename(char *out, size_t sz, const char *path) {
    const char *p = strrchr(path, KEX_PATH_SEP);
    if (!p) p = strrchr(path, '/');
    if (!p) p = path; else p++;
    const char *dot = strrchr(p, '.');
    if (dot) {
        size_t n = (size_t)(dot - p);
        if (n >= sz) n = sz - 1;
        memcpy(out, p, n);
        out[n] = '\0';
    } else {
        snprintf(out, sz, "%s", p);
    }
}

static void config_defaults(kex_config_t *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->output_format = KEX_FMT_KEX;
    cfg->opt_level = KEX_OPT_O2;
    cfg->debug_info = false;
    cfg->strip = false;
    cfg->pie = true;
    cfg->static_link = false;
    cfg->verbose = false;
    cfg->keep_temps = false;
    cfg->has_graphics = false;
#ifdef _WIN32
    snprintf(cfg->cc, sizeof(cfg->cc), "gcc.exe");
    snprintf(cfg->cxx, sizeof(cfg->cxx), "g++.exe");
    snprintf(cfg->ld, sizeof(cfg->ld), "ld.exe");
    snprintf(cfg->ar, sizeof(cfg->ar), "ar.exe");
    snprintf(cfg->objcopy, sizeof(cfg->objcopy), "objcopy.exe");
#else
    snprintf(cfg->cc, sizeof(cfg->cc), "gcc");
    snprintf(cfg->cxx, sizeof(cfg->cxx), "g++");
    snprintf(cfg->ld, sizeof(cfg->ld), "ld");
    snprintf(cfg->ar, sizeof(cfg->ar), "ar");
    snprintf(cfg->objcopy, sizeof(cfg->objcopy), "objcopy");
#endif
}

static void detect_toolchain(kex_config_t *cfg) {
    char *cc_env = getenv("KEX_CC");
    char *cxx_env = getenv("KEX_CXX");
    char *ld_env = getenv("KEX_LD");
    if (cc_env) snprintf(cfg->cc, sizeof(cfg->cc), "%s", cc_env);
    if (cxx_env) snprintf(cfg->cxx, sizeof(cfg->cxx), "%s", cxx_env);
    if (ld_env) snprintf(cfg->ld, sizeof(cfg->ld), "%s", ld_env);
}

static int scan_imports(const char *src, import_ref_t *imports, size_t *n_imports, size_t max_imports, bool *has_gfx) {
    const char *p = src;
    size_t count = 0;
    *has_gfx = false;
    while (*p && count < max_imports) {
        while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
        if (strncmp(p, "kapi_", 5) == 0 || strncmp(p, "kgr_", 4) == 0) {
            const char *start = p;
            while (*p && (isalnum((unsigned char)*p) || *p == '_')) p++;
            size_t len = (size_t)(p - start);
            if (len > 0 && len < 256) {
                char name[256];
                memcpy(name, start, len);
                name[len] = '\0';
                bool dup = false;
                for (size_t i = 0; i < count; i++) {
                    if (strcmp(imports[i].name, name) == 0) { dup = true; break; }
                }
                if (!dup) {
                    snprintf(imports[count].name, sizeof(imports[count].name), "%s", name);
                    imports[count].crc32 = crc32_str(name);
                    count++;
                }
                if (strncmp(name, "kgr_", 4) == 0 ||
                    strncmp(name, "kapi_graphics", 13) == 0 ||
                    strncmp(name, "kapi_window", 11) == 0 ||
                    strncmp(name, "kapi_render", 11) == 0)
                    *has_gfx = true;
            }
        } else {
            if (*p) p++;
        }
    }
    *n_imports = count;
    return 0;
}

static int compile_source(const kex_config_t *cfg, const char *src_path, const char *obj_path, int lang) {
    char cmd[8192];
    const char *compiler = (lang == KEX_LANG_CPP) ? cfg->cxx : cfg->cc;
    int pos = snprintf(cmd, sizeof(cmd), "%s -c", compiler);
    const char *opt_flags[] = {"-O0", "-O1", "-O2", "-O3", "-Os"};
    if (cfg->opt_level >= 0 && cfg->opt_level <= 4)
        pos += snprintf(cmd + pos, sizeof(cmd) - pos, " %s", opt_flags[cfg->opt_level]);
    if (cfg->debug_info) pos += snprintf(cmd + pos, sizeof(cmd) - pos, " -g");
    if (cfg->pie) pos += snprintf(cmd + pos, sizeof(cmd) - pos, " -fPIE");
    pos += snprintf(cmd + pos, sizeof(cmd) - pos, " -fno-stack-protector -nostdlib -nostdinc");
    pos += snprintf(cmd + pos, sizeof(cmd) - pos, " -ffreestanding -fno-builtin");
    if (lang == KEX_LANG_CPP)
        pos += snprintf(cmd + pos, sizeof(cmd) - pos, " -fno-exceptions -fno-rtti");
    for (int i = 0; i < cfg->inc_dir_count; i++)
        pos += snprintf(cmd + pos, sizeof(cmd) - pos, " -I%s", cfg->inc_dirs[i]);
    for (int i = 0; i < cfg->define_count; i++)
        pos += snprintf(cmd + pos, sizeof(cmd) - pos, " -D%s", cfg->defines[i]);
    for (int i = 0; i < cfg->extra_cflag_count; i++)
        pos += snprintf(cmd + pos, sizeof(cmd) - pos, " %s", cfg->extra_cflags[i]);
    pos += snprintf(cmd + pos, sizeof(cmd) - pos, " -o %s %s", obj_path, src_path);
    return run_cmd(cmd, cfg->verbose);
}

static int link_relocatable(const kex_config_t *cfg, const kex_obj_t *objs, int obj_count, const char *out_path) {
    char cmd[8192];
    int pos = snprintf(cmd, sizeof(cmd), "%s -r -o %s", cfg->ld, out_path);
    for (int i = 0; i < obj_count; i++)
        pos += snprintf(cmd + pos, sizeof(cmd) - pos, " %s", objs[i].obj_path);
    return run_cmd(cmd, cfg->verbose);
}

static uint8_t *extract_elf_section(const uint8_t *elf_data, size_t elf_size,
                                     const char *sec_name, size_t *out_size) {
    if (elf_size < 64) return NULL;
    uint8_t ei_class = elf_data[4];
    if (ei_class != 1 && ei_class != 2) return NULL;
    int is64 = (ei_class == 2);

    uint16_t e_shstrndx;
    uint64_t e_shoff;
    uint16_t e_shentsize, e_shnum;
    if (is64) {
        if (elf_size < 64) return NULL;
        e_shoff = *(uint64_t *)(elf_data + 40);
        e_shentsize = *(uint16_t *)(elf_data + 58);
        e_shnum = *(uint16_t *)(elf_data + 60);
        e_shstrndx = *(uint16_t *)(elf_data + 62);
    } else {
        if (elf_size < 52) return NULL;
        e_shoff = *(uint32_t *)(elf_data + 32);
        e_shentsize = *(uint16_t *)(elf_data + 46);
        e_shnum = *(uint16_t *)(elf_data + 48);
        e_shstrndx = *(uint16_t *)(elf_data + 50);
    }

    if (e_shoff == 0 || e_shnum == 0) return NULL;
    if (e_shstrndx >= e_shnum) return NULL;

    uint8_t *shstrtab = NULL;
    size_t shstrtab_size = 0;
    if (is64) {
        uint8_t *sh = (uint8_t *)elf_data + e_shoff + (size_t)e_shstrndx * e_shentsize;
        uint64_t off = *(uint64_t *)(sh + 24);
        shstrtab_size = *(uint64_t *)(sh + 32);
        if (off + shstrtab_size > elf_size) return NULL;
        shstrtab = (uint8_t *)elf_data + off;
    } else {
        uint8_t *sh = (uint8_t *)elf_data + e_shoff + (size_t)e_shstrndx * e_shentsize;
        uint32_t off = *(uint32_t *)(sh + 16);
        shstrtab_size = *(uint32_t *)(sh + 20);
        if (off + shstrtab_size > elf_size) return NULL;
        shstrtab = (uint8_t *)elf_data + off;
    }

    for (uint16_t i = 0; i < e_shnum; i++) {
        uint8_t *sh;
        uint32_t sh_name;
        uint64_t sh_offset, sh_size;
        if (is64) {
            sh = (uint8_t *)elf_data + e_shoff + (size_t)i * e_shentsize;
            sh_name = *(uint32_t *)(sh + 0);
            sh_offset = *(uint64_t *)(sh + 24);
            sh_size = *(uint64_t *)(sh + 32);
        } else {
            sh = (uint8_t *)elf_data + e_shoff + (size_t)i * e_shentsize;
            sh_name = *(uint32_t *)(sh + 0);
            sh_offset = *(uint32_t *)(sh + 16);
            sh_size = *(uint32_t *)(sh + 20);
        }
        if (sh_name >= shstrtab_size) continue;
        const char *name = (const char *)shstrtab + sh_name;
        if (strcmp(name, sec_name) == 0) {
            if (sh_offset + sh_size > elf_size) return NULL;
            uint8_t *sec = (uint8_t *)malloc(sh_size > 0 ? sh_size : 1);
            if (!sec) return NULL;
            if (sh_size > 0) memcpy(sec, elf_data + sh_offset, sh_size);
            *out_size = sh_size;
            return sec;
        }
    }
    return NULL;
}

static int parse_elf_relocs(const uint8_t *elf_data, size_t elf_size,
                            kex_reloc_entry_t **out_relocs, size_t *out_reloc_count,
                            kex_local_reloc_t **out_local, size_t *out_local_count) {
    *out_relocs = NULL;
    *out_reloc_count = 0;
    *out_local = NULL;
    *out_local_count = 0;
    if (elf_size < 64) return 0;
    uint8_t ei_class = elf_data[4];
    if (ei_class != 2) return 0;

    uint64_t e_shoff = *(uint64_t *)(elf_data + 40);
    uint16_t e_shentsize = *(uint16_t *)(elf_data + 58);
    uint16_t e_shnum = *(uint16_t *)(elf_data + 60);
    uint16_t e_shstrndx = *(uint16_t *)(elf_data + 62);
    if (e_shoff == 0 || e_shnum == 0) return 0;

    uint8_t *shstrtab_sh = (uint8_t *)elf_data + e_shoff + (size_t)e_shstrndx * e_shentsize;
    uint64_t strtab_off = *(uint64_t *)(shstrtab_sh + 24);
    size_t strtab_sz = *(uint64_t *)(shstrtab_sh + 32);
    const uint8_t *shstrtab = elf_data + strtab_off;

    uint8_t *symtab = NULL;
    size_t symtab_sz = 0;
    const uint8_t *strtab = NULL;
    size_t strtab_size = 0;

    for (uint16_t i = 0; i < e_shnum; i++) {
        uint8_t *sh = (uint8_t *)elf_data + e_shoff + (size_t)i * e_shentsize;
        uint32_t sh_name = *(uint32_t *)(sh + 0);
        uint32_t sh_type = *(uint32_t *)(sh + 4);
        uint64_t sh_offset = *(uint64_t *)(sh + 24);
        uint64_t sh_size = *(uint64_t *)(sh + 32);
        if (sh_name >= strtab_sz) continue;
        const char *name = (const char *)shstrtab + sh_name;
        if (sh_type == 2 && strcmp(name, ".symtab") == 0) {
            symtab = (uint8_t *)elf_data + sh_offset;
            symtab_sz = sh_size;
            uint32_t link = *(uint32_t *)(sh + 12);
            uint8_t *lsh = (uint8_t *)elf_data + e_shoff + (size_t)link * e_shentsize;
            uint64_t loff = *(uint64_t *)(lsh + 24);
            strtab_size = *(uint64_t *)(lsh + 32);
            strtab = elf_data + loff;
        }
    }
    if (!symtab || !strtab) return 0;

    kex_reloc_entry_t *relocs = NULL;
    size_t reloc_count = 0;
    size_t reloc_cap = 0;
    kex_local_reloc_t *local = NULL;
    size_t local_count = 0;
    size_t local_cap = 0;

    for (uint16_t i = 0; i < e_shnum; i++) {
        uint8_t *sh = (uint8_t *)elf_data + e_shoff + (size_t)i * e_shentsize;
        uint32_t sh_type = *(uint32_t *)(sh + 4);
        if (sh_type != 4) continue;
        uint64_t sh_offset = *(uint64_t *)(sh + 24);
        uint64_t sh_size = *(uint64_t *)(sh + 32);
        uint64_t sh_entsize = *(uint64_t *)(sh + 56);
        if (sh_entsize == 0) sh_entsize = 24;
        size_t n_entries = (size_t)(sh_size / sh_entsize);
        for (size_t j = 0; j < n_entries; j++) {
            const uint8_t *rel = elf_data + sh_offset + j * sh_entsize;
            uint64_t r_offset = *(uint64_t *)(rel + 0);
            uint32_t r_info = *(uint32_t *)(rel + 8);
            uint32_t r_sym = r_info >> 8;
            uint32_t r_type = r_info & 0xFF;
            if (r_type != 4) continue;
            size_t sym_off = (size_t)r_sym * 24;
            if (sym_off + 24 > symtab_sz) continue;
            uint32_t st_name = *(uint32_t *)(symtab + sym_off + 0);
            uint8_t st_info = symtab[sym_off + 4];
            uint8_t st_bind = st_info >> 4;
            if (st_name >= strtab_size) continue;
            const char *sym_name = (const char *)strtab + st_name;
            if (sym_name[0] == '\0') continue;
            if (st_bind == 1) {
                if (reloc_count >= reloc_cap) {
                    reloc_cap = reloc_cap ? reloc_cap * 2 : 64;
                    relocs = (kex_reloc_entry_t *)realloc(relocs, reloc_cap * sizeof(kex_reloc_entry_t));
                }
                relocs[reloc_count].call_off = (uint32_t)r_offset;
                relocs[reloc_count].sym_crc32 = crc32_str(sym_name);
                reloc_count++;
            } else if (st_bind == 0 && strncmp(sym_name, ".rodata", 7) == 0) {
                if (local_count >= local_cap) {
                    local_cap = local_cap ? local_cap * 2 : 64;
                    local = (kex_local_reloc_t *)realloc(local, local_cap * sizeof(kex_local_reloc_t));
                }
                local[local_count].patch_off = (uint32_t)r_offset;
                local[local_count].sym_value = (uint32_t)(*(uint64_t *)(symtab + sym_off + 8));
                local_count++;
            }
        }
    }

    *out_relocs = relocs;
    *out_reloc_count = reloc_count;
    *out_local = local;
    *out_local_count = local_count;
    return 0;
}

static int build_kex_file(const uint8_t *code, size_t code_size,
                          const uint8_t *rodata, size_t rodata_size,
                          const import_ref_t *imports, size_t n_imports,
                          const kex_reloc_entry_t *relocs, size_t n_relocs,
                          const kex_local_reloc_t *local_relocs, size_t n_local_relocs,
                          const char *prog_name, int is_kxp,
                          const char *out_path) {
    size_t name_len = prog_name ? strlen(prog_name) : 0;
    if (name_len > KEX_NAME_LEN) name_len = KEX_NAME_LEN;

    size_t import_area = 0;
    for (size_t i = 0; i < n_imports; i++)
        import_area += 4 + 4 + strlen(imports[i].name) + 1;

    size_t reloc_area = 4 + n_relocs * sizeof(kex_reloc_entry_t);
    size_t local_reloc_area = n_local_relocs * sizeof(kex_local_reloc_t);

    size_t total = KEX_HEADER_SIZE + KEX_NAME_LEN + 8 + code_size + 7;
    if (rodata_size > 0) total += rodata_size;
    size_t import_off_val = total;
    total += import_area;
    size_t export_off_val = total;
    size_t reloc_off_val = 0;
    if (n_relocs > 0) {
        reloc_off_val = total;
        total += reloc_area;
    }
    size_t local_reloc_off_val = 0;
    if (n_local_relocs > 0) {
        local_reloc_off_val = total;
        total += local_reloc_area;
    }

    uint8_t *buf = (uint8_t *)calloc(total, 1);
    if (!buf) return KEX_ERR_IO;

    kex_header_t *h = (kex_header_t *)buf;
    h->magic[0] = KEX_MAGIC0; h->magic[1] = KEX_MAGIC1;
    h->magic[2] = KEX_MAGIC2; h->magic[3] = KEX_MAGIC3;
    h->version = KEX_VERSION;
    h->header_size = KEX_HEADER_SIZE;
    h->total_size = (uint32_t)total;
    h->entry_offset = KEX_HEADER_SIZE + KEX_NAME_LEN;
    h->code_size = (uint32_t)code_size;
    h->import_offset = (uint32_t)import_off_val;
    h->import_count = (uint32_t)n_imports;
    h->export_offset = (uint32_t)export_off_val;
    h->export_count = 0;
    h->icon_offset = 0;
    h->icon_size = 0;
    h->stack_size = KEX_DEFAULT_STACK;
    h->heap_size = KEX_DEFAULT_HEAP;
    h->crc32 = 0;
    h->file_type = (uint16_t)(is_kxp ? KEX_FILE_KXP : KEX_FILE_KEX);
    h->flags = 0;
    if (rodata_size > 0) h->flags |= 0x0004u;
    h->reloc_offset = (uint32_t)reloc_off_val;
    if (rodata_size > 0) {
        h->rodata_offset = (uint32_t)(KEX_HEADER_SIZE + KEX_NAME_LEN + 8 + code_size + 7);
        h->rodata_size = (uint32_t)rodata_size;
    }
    h->local_reloc_off = (uint32_t)local_reloc_off_val;
    h->local_reloc_cnt = (uint32_t)n_local_relocs;

    size_t off = KEX_HEADER_SIZE;
    if (prog_name) {
        size_t cp = name_len < KEX_NAME_LEN ? name_len : KEX_NAME_LEN;
        memcpy(buf + off, prog_name, cp);
    }
    off += KEX_NAME_LEN;

    memcpy(buf + off, KEX_CODE_START, 8);
    off += 8;
    memcpy(buf + off, code, code_size);
    off += code_size;
    memcpy(buf + off, KEX_CODE_END, 7);
    off += 7;

    if (rodata_size > 0 && rodata) {
        memcpy(buf + off, rodata, rodata_size);
        off += rodata_size;
    }

    for (size_t i = 0; i < n_imports; i++) {
        *(uint32_t *)(buf + off) = imports[i].crc32;
        off += 4;
        *(uint32_t *)(buf + off) = 0;
        off += 4;
        size_t nl = strlen(imports[i].name) + 1;
        memcpy(buf + off, imports[i].name, nl);
        off += nl;
    }

    if (n_relocs > 0) {
        *(uint32_t *)(buf + off) = (uint32_t)n_relocs;
        off += 4;
        memcpy(buf + off, relocs, n_relocs * sizeof(kex_reloc_entry_t));
        off += n_relocs * sizeof(kex_reloc_entry_t);
    }

    if (n_local_relocs > 0) {
        memcpy(buf + off, local_relocs, n_local_relocs * sizeof(kex_local_reloc_t));
        off += n_local_relocs * sizeof(kex_local_reloc_t);
    }

    h->crc32 = 0;
    uint32_t crc = crc32_compute(buf, total);
    h->crc32 = crc;

    int ret = file_write(out_path, buf, total);
    free(buf);
    return ret;
}

static int build_elf_executable(const kex_config_t *cfg, const kex_obj_t *objs, int obj_count,
                                const char *out_path) {
    char cmd[8192];
    int pos = snprintf(cmd, sizeof(cmd), "%s", cfg->cc);
    for (int i = 0; i < obj_count; i++)
        pos += snprintf(cmd + pos, sizeof(cmd) - pos, " %s", objs[i].obj_path);
    pos += snprintf(cmd + pos, sizeof(cmd) - pos, " -o %s", out_path);
    if (cfg->static_link) pos += snprintf(cmd + pos, sizeof(cmd) - pos, " -static");
    if (cfg->strip) pos += snprintf(cmd + pos, sizeof(cmd) - pos, " -s");
    if (cfg->pie) pos += snprintf(cmd + pos, sizeof(cmd) - pos, " -pie");
    for (int i = 0; i < cfg->lib_dir_count; i++)
        pos += snprintf(cmd + pos, sizeof(cmd) - pos, " -L%s", cfg->lib_dirs[i]);
    for (int i = 0; i < cfg->lib_count; i++)
        pos += snprintf(cmd + pos, sizeof(cmd) - pos, " -l%s", cfg->libs[i]);
    for (int i = 0; i < cfg->extra_ldflag_count; i++)
        pos += snprintf(cmd + pos, sizeof(cmd) - pos, " %s", cfg->extra_ldflags[i]);
    pos += snprintf(cmd + pos, sizeof(cmd) - pos, " -nostdlib -lgcc");
    return run_cmd(cmd, cfg->verbose);
}

static int do_compile(kex_config_t *cfg) {
    if (cfg->source_count == 0) {
        fprintf(stderr, "kex: 没有输入文件\n");
        return KEX_ERR_NOINPUT;
    }

    char tmpdir[KEX_PATH_MAX];
    get_temp_dir(tmpdir, sizeof(tmpdir));
    mkdirs(tmpdir);

    kex_obj_t objs[KEX_MAX_SOURCES];
    int obj_count = 0;
    bool any_gfx = false;

    for (int i = 0; i < cfg->source_count; i++) {
        char basename[256];
        get_basename(basename, sizeof(basename), cfg->sources[i]);
        char obj_path[KEX_PATH_MAX];
        snprintf(obj_path, sizeof(obj_path), "%s" KEX_PATH_SEP_STR "%s.o", tmpdir, basename);

        int lang = cfg->source_langs[i];
        if (lang == KEX_LANG_C) lang = detect_lang(cfg->sources[i]);
        cfg->source_langs[i] = lang;

        if (cfg->verbose) fprintf(stderr, "[kex] 编译 %s -> %s\n", cfg->sources[i], obj_path);

        int ret = compile_source(cfg, cfg->sources[i], obj_path, lang);
        if (ret != 0) {
            fprintf(stderr, "kex: 编译失败: %s\n", cfg->sources[i]);
            return KEX_ERR_COMPILE;
        }

        snprintf(objs[obj_count].obj_path, sizeof(objs[obj_count].obj_path), "%s", obj_path);
        objs[obj_count].lang = lang;
        objs[obj_count].compiled = true;
        obj_count++;

        size_t src_len = 0;
        char *src_code = file_read(cfg->sources[i], &src_len);
        if (src_code) {
            import_ref_t file_imports[512];
            size_t file_n_imports = 0;
            bool file_has_gfx = false;
            scan_imports(src_code, file_imports, &file_n_imports, 512, &file_has_gfx);
            if (file_has_gfx) any_gfx = true;
            free(src_code);
        }
    }

    cfg->has_graphics = any_gfx;

    if (cfg->output_format == KEX_FMT_ELF) {
        int ret = build_elf_executable(cfg, objs, obj_count, cfg->output);
        if (ret != 0) {
            fprintf(stderr, "kex: ELF链接失败\n");
            return KEX_ERR_LINK;
        }
        if (cfg->verbose) fprintf(stderr, "[kex] 输出: %s (ELF)\n", cfg->output);
        return KEX_OK;
    }

    char merged_obj[KEX_PATH_MAX];
    if (obj_count == 1) {
        snprintf(merged_obj, sizeof(merged_obj), "%s", objs[0].obj_path);
    } else {
        snprintf(merged_obj, sizeof(merged_obj), "%s" KEX_PATH_SEP_STR "merged.o", tmpdir);
        int ret = link_relocatable(cfg, objs, obj_count, merged_obj);
        if (ret != 0) {
            fprintf(stderr, "kex: 部分链接失败\n");
            return KEX_ERR_LINK;
        }
    }

    size_t elf_len = 0;
    uint8_t *elf_data = (uint8_t *)file_read(merged_obj, &elf_len);
    if (!elf_data || elf_len == 0) {
        fprintf(stderr, "kex: 无法读取合并目标文件\n");
        return KEX_ERR_IO;
    }

    size_t code_size = 0;
    uint8_t *code = extract_elf_section(elf_data, elf_len, ".text", &code_size);
    if (!code || code_size == 0) {
        free(elf_data);
        fprintf(stderr, "kex: 无.text段\n");
        return KEX_ERR_FORMAT;
    }

    size_t rodata_size = 0;
    uint8_t *rodata = extract_elf_section(elf_data, elf_len, ".rodata", &rodata_size);

    kex_reloc_entry_t *relocs = NULL;
    size_t n_relocs = 0;
    kex_local_reloc_t *local_relocs = NULL;
    size_t n_local_relocs = 0;
    parse_elf_relocs(elf_data, elf_len, &relocs, &n_relocs, &local_relocs, &n_local_relocs);

    import_ref_t all_imports[1024];
    size_t all_import_count = 0;
    for (int i = 0; i < cfg->source_count; i++) {
        size_t src_len = 0;
        char *src = file_read(cfg->sources[i], &src_len);
        if (!src) continue;
        import_ref_t file_imports[512];
        size_t file_n = 0;
        bool dummy_gfx = false;
        scan_imports(src, file_imports, &file_n, 512, &dummy_gfx);
        for (size_t j = 0; j < file_n; j++) {
            bool dup = false;
            for (size_t k = 0; k < all_import_count; k++) {
                if (strcmp(all_imports[k].name, file_imports[j].name) == 0) { dup = true; break; }
            }
            if (!dup && all_import_count < 1024) {
                all_imports[all_import_count++] = file_imports[j];
            }
        }
        free(src);
    }

    int is_kxp = (cfg->output_format == KEX_FMT_KXP) ? 1 : 0;
    const char *prog_name = cfg->name[0] ? cfg->name : NULL;

    int ret = build_kex_file(code, code_size, rodata, rodata_size,
                             all_imports, all_import_count,
                             relocs, n_relocs, local_relocs, n_local_relocs,
                             prog_name, is_kxp, cfg->output);

    free(code);
    if (rodata) free(rodata);
    if (relocs) free(relocs);
    if (local_relocs) free(local_relocs);
    free(elf_data);

    if (ret != 0) {
        fprintf(stderr, "kex: kex构建失败\n");
        return KEX_ERR_FORMAT;
    }

    const char *fmt_str = is_kxp ? "KXP" : "KEX";
    if (cfg->verbose) fprintf(stderr, "[kex] 输出: %s (%s, %zu字节代码, %zu导入)\n",
                              cfg->output, fmt_str, code_size, all_import_count);

    if (!cfg->keep_temps) {
        for (int i = 0; i < obj_count; i++) remove(objs[i].obj_path);
        if (obj_count > 1) remove(merged_obj);
        rmdir(tmpdir);
    }

    return KEX_OK;
}

#ifdef _WIN32
static int run_kex_with_window_win32(const char *kex_path, int width, int height) {
    HINSTANCE hInst = GetModuleHandle(NULL);
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = hInst;
    wc.lpszClassName = "KenuxGfxWindow";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowA("KenuxGfxWindow", "Kenux App",
                              WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                              width, height, NULL, NULL, hInst, NULL);
    if (!hwnd) return -1;
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    HDC hdc = GetDC(hwnd);
    RECT rc;
    GetClientRect(hwnd, &rc);
    HBITMAP hbm = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    HDC memdc = CreateCompatibleDC(hdc);
    SelectObject(memdc, hbm);
    size_t kex_len = 0;
    uint8_t *kex_data = (uint8_t *)file_read(kex_path, &kex_len);
    if (kex_data) {
        kex_header_t *hdr = (kex_header_t *)kex_data;
        if (hdr->magic[0] == KEX_MAGIC0 && hdr->code_size > 0) {
            uint8_t *code = kex_data + hdr->entry_offset + 8;
            void (*fn)(void*) = (void(*)(void*))(void*)code;
            fn(NULL);
        }
        free(kex_data);
    }
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    DeleteDC(memdc);
    DeleteObject(hbm);
    ReleaseDC(hwnd, hdc);
    DestroyWindow(hwnd);
    return 0;
}
#else
static int run_kex_with_window_x11(const char *kex_path, int width, int height) {
    void *xlib = dlopen("libX11.so", RTLD_LAZY);
    if (!xlib) xlib = dlopen("libX11.so.6", RTLD_LAZY);
    if (!xlib) {
        fprintf(stderr, "kex: 无法加载X11库\n");
        return -1;
    }

    typedef void* (*XOpenDisplay_fn)(const char*);
    typedef int (*XCloseDisplay_fn)(void*);
    typedef unsigned long (*XCreateSimpleWindow_fn)(void*, unsigned long, int, int, unsigned int, unsigned int, unsigned int, unsigned long, unsigned long);
    typedef int (*XMapWindow_fn)(void*, unsigned long);
    typedef int (*XSelectInput_fn)(void*, unsigned long, long);
    typedef int (*XNextEvent_fn)(void*, void*);
    typedef int (*XDestroyWindow_fn)(void*, unsigned long);
    typedef int (*XSync_fn)(void*, int);

    XOpenDisplay_fn xf_open = (XOpenDisplay_fn)dlsym(xlib, "XOpenDisplay");
    XCloseDisplay_fn xf_close = (XCloseDisplay_fn)dlsym(xlib, "XCloseDisplay");
    XCreateSimpleWindow_fn xf_create = (XCreateSimpleWindow_fn)dlsym(xlib, "XCreateSimpleWindow");
    XMapWindow_fn xf_map = (XMapWindow_fn)dlsym(xlib, "XMapWindow");
    XSelectInput_fn xf_select = (XSelectInput_fn)dlsym(xlib, "XSelectInput");
    XNextEvent_fn xf_next = (XNextEvent_fn)dlsym(xlib, "XNextEvent");
    XDestroyWindow_fn xf_destroy = (XDestroyWindow_fn)dlsym(xlib, "XDestroyWindow");
    XSync_fn xf_sync = (XSync_fn)dlsym(xlib, "XSync");

    if (!xf_open || !xf_create || !xf_map) {
        dlclose(xlib);
        return -1;
    }

    void *dpy = xf_open(NULL);
    if (!dpy) { dlclose(xlib); return -1; }

    unsigned long root = 0;
    unsigned long win = xf_create(dpy, root, 0, 0, width, height, 0, 0, 0);
    if (!win) { xf_close(dpy); dlclose(xlib); return -1; }

    if (xf_select) xf_select(dpy, win, 1L << 0 | 1L << 1 | 1L << 2);
    xf_map(dpy, win);
    if (xf_sync) xf_sync(dpy, 0);

    size_t kex_len = 0;
    uint8_t *kex_data = (uint8_t *)file_read(kex_path, &kex_len);
    if (kex_data) {
        kex_header_t *hdr = (kex_header_t *)kex_data;
        if (hdr->magic[0] == KEX_MAGIC0 && hdr->code_size > 0) {
            uint8_t *code = kex_data + hdr->entry_offset + 8;
            void (*fn)(void*) = (void(*)(void*))(void*)code;
            fn(NULL);
        }
        free(kex_data);
    }

    if (xf_next) {
        char ev[64];
        xf_next(dpy, ev);
    }

    if (xf_destroy) xf_destroy(dpy, win);
    xf_close(dpy);
    dlclose(xlib);
    return 0;
}
#endif

static int do_run(const char *kex_path, bool force_gfx, int gfx_w, int gfx_h) {
    size_t kex_len = 0;
    uint8_t *kex_data = (uint8_t *)file_read(kex_path, &kex_len);
    if (!kex_data) {
        fprintf(stderr, "kex: 无法读取 %s\n", kex_path);
        return KEX_ERR_IO;
    }

    if (kex_len < sizeof(kex_header_t)) {
        free(kex_data);
        fprintf(stderr, "kex: 无效的kex文件\n");
        return KEX_ERR_FORMAT;
    }

    kex_header_t *hdr = (kex_header_t *)kex_data;
    if (hdr->magic[0] != KEX_MAGIC0 || hdr->magic[1] != KEX_MAGIC1 ||
        hdr->magic[2] != KEX_MAGIC2 || hdr->magic[3] != KEX_MAGIC3) {
        free(kex_data);
        fprintf(stderr, "kex: 非kex文件\n");
        return KEX_ERR_FORMAT;
    }

    bool needs_gfx = force_gfx;
    if (!needs_gfx) {
        uint8_t *import_start = kex_data + hdr->import_offset;
        for (uint32_t i = 0; i < hdr->import_count; i++) {
            import_start += 8;
            const char *iname = (const char *)import_start;
            size_t inl = strlen(iname) + 1;
            import_start += inl;
            if (strncmp(iname, "kgr_", 4) == 0 ||
                strncmp(iname, "kapi_graphics", 13) == 0 ||
                strncmp(iname, "kapi_window", 11) == 0 ||
                strncmp(iname, "kapi_render", 11) == 0) {
                needs_gfx = true;
                break;
            }
        }
    }

    free(kex_data);

    if (needs_gfx) {
        int w = gfx_w > 0 ? gfx_w : 800;
        int h = gfx_h > 0 ? gfx_h : 600;
        fprintf(stderr, "[kex] 打开图形窗口 %dx%d 用于 %s\n", w, h, kex_path);
#ifdef _WIN32
        return run_kex_with_window_win32(kex_path, w, h);
#else
        return run_kex_with_window_x11(kex_path, w, h);
#endif
    }

    char cmd[KEX_PATH_MAX + 32];
    snprintf(cmd, sizeof(cmd), "%s", kex_path);
    return run_cmd(cmd, false);
}

#define KBUILD_MAX_TARGETS 64
#define KBUILD_MAX_DEPS   128

typedef struct {
    char name[128];
    char sources[KEX_MAX_SOURCES][KEX_PATH_MAX];
    int source_count;
    char output[KEX_PATH_MAX];
    int format;
    int is_shared;
    char depends[KBUILD_MAX_DEPS][128];
    int depend_count;
    char inc_dirs[KEX_MAX_INC_DIRS][KEX_PATH_MAX];
    int inc_dir_count;
    char defines[KEX_MAX_DEFS][256];
    int define_count;
    int opt_level;
    bool debug_info;
    bool strip;
} kbuild_target_t;

typedef struct {
    char cc[KEX_PATH_MAX];
    char cxx[KEX_PATH_MAX];
    char inc_dirs[KEX_MAX_INC_DIRS][KEX_PATH_MAX];
    int inc_dir_count;
    char lib_dirs[KEX_MAX_LIB_DIRS][KEX_PATH_MAX];
    int lib_dir_count;
    char defines[KEX_MAX_DEFS][256];
    int define_count;
    int opt_level;
    bool debug_info;
    bool verbose;
    kbuild_target_t targets[KBUILD_MAX_TARGETS];
    int target_count;
} kbuild_config_t;

static char *trim(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    char *e = s + strlen(s) - 1;
    while (e > s && isspace((unsigned char)*e)) *e-- = '\0';
    return s;
}

static int parse_build_file(const char *path, kbuild_config_t *bcfg) {
    size_t len = 0;
    char *data = file_read(path, &len);
    if (!data) return -1;

    memset(bcfg, 0, sizeof(*bcfg));
#ifdef _WIN32
    snprintf(bcfg->cc, sizeof(bcfg->cc), "gcc.exe");
    snprintf(bcfg->cxx, sizeof(bcfg->cxx), "g++.exe");
#else
    snprintf(bcfg->cc, sizeof(bcfg->cc), "gcc");
    snprintf(bcfg->cxx, sizeof(bcfg->cxx), "g++");
#endif
    bcfg->opt_level = KEX_OPT_O2;

    kbuild_target_t *cur_target = NULL;
    char *line = data;
    while (line && *line) {
        char *nl = strchr(line, '\n');
        if (nl) *nl++ = '\0';
        char *t = trim(line);
        line = nl;

        if (t[0] == '#' || t[0] == '\0') continue;

        if (strncmp(t, "project(", 8) == 0) {
            continue;
        } else if (strncmp(t, "set(CC ", 7) == 0) {
            char *v = t + 7;
            char *end = strchr(v, ')');
            if (end) { *end = '\0'; snprintf(bcfg->cc, sizeof(bcfg->cc), "%s", trim(v)); }
        } else if (strncmp(t, "set(CXX ", 8) == 0) {
            char *v = t + 8;
            char *end = strchr(v, ')');
            if (end) { *end = '\0'; snprintf(bcfg->cxx, sizeof(bcfg->cxx), "%s", trim(v)); }
        } else if (strncmp(t, "set(OPT ", 8) == 0) {
            char *v = t + 8;
            char *end = strchr(v, ')');
            if (end) { *end = '\0'; bcfg->opt_level = atoi(trim(v)); }
        } else if (strncmp(t, "include_dir(", 12) == 0) {
            char *v = t + 12;
            char *end = strchr(v, ')');
            if (end) {
                *end = '\0';
                if (bcfg->inc_dir_count < KEX_MAX_INC_DIRS)
                    snprintf(bcfg->inc_dirs[bcfg->inc_dir_count++], KEX_PATH_MAX, "%s", trim(v));
            }
        } else if (strncmp(t, "define(", 7) == 0) {
            char *v = t + 7;
            char *end = strchr(v, ')');
            if (end) {
                *end = '\0';
                if (bcfg->define_count < KEX_MAX_DEFS)
                    snprintf(bcfg->defines[bcfg->define_count++], 256, "%s", trim(v));
            }
        } else if (strncmp(t, "target(", 7) == 0) {
            if (bcfg->target_count < KBUILD_MAX_TARGETS) {
                cur_target = &bcfg->targets[bcfg->target_count++];
                char *v = t + 7;
                char *end = strchr(v, ')');
                if (end) *end = '\0';
                snprintf(cur_target->name, sizeof(cur_target->name), "%s", trim(v));
                cur_target->format = KEX_FMT_KEX;
                cur_target->opt_level = bcfg->opt_level;
            }
        } else if (cur_target) {
            if (strncmp(t, "sources(", 8) == 0) {
                char *v = t + 8;
                char *end = strchr(v, ')');
                if (end) *end = '\0';
                char *tok = strtok(v, " \t");
                while (tok && cur_target->source_count < KEX_MAX_SOURCES) {
                    snprintf(cur_target->sources[cur_target->source_count++], KEX_PATH_MAX, "%s", tok);
                    tok = strtok(NULL, " \t");
                }
            } else if (strncmp(t, "output(", 7) == 0) {
                char *v = t + 7;
                char *end = strchr(v, ')');
                if (end) { *end = '\0'; snprintf(cur_target->output, KEX_PATH_MAX, "%s", trim(v)); }
            } else if (strncmp(t, "format(", 7) == 0) {
                char *v = t + 7;
                char *end = strchr(v, ')');
                if (end) {
                    *end = '\0';
                    v = trim(v);
                    if (strcmp(v, "kex") == 0) cur_target->format = KEX_FMT_KEX;
                    else if (strcmp(v, "kxp") == 0) cur_target->format = KEX_FMT_KXP;
                    else if (strcmp(v, "elf") == 0) cur_target->format = KEX_FMT_ELF;
                }
            } else if (strncmp(t, "shared(", 7) == 0) {
                cur_target->is_shared = 1;
            } else if (strncmp(t, "depends(", 8) == 0) {
                char *v = t + 8;
                char *end = strchr(v, ')');
                if (end) *end = '\0';
                char *tok = strtok(v, " \t");
                while (tok && cur_target->depend_count < KBUILD_MAX_DEPS) {
                    snprintf(cur_target->depends[cur_target->depend_count++], 128, "%s", tok);
                    tok = strtok(NULL, " \t");
                }
            } else if (strncmp(t, "include_dir(", 12) == 0) {
                char *v = t + 12;
                char *end = strchr(v, ')');
                if (end) {
                    *end = '\0';
                    if (cur_target->inc_dir_count < KEX_MAX_INC_DIRS)
                        snprintf(cur_target->inc_dirs[cur_target->inc_dir_count++], KEX_PATH_MAX, "%s", trim(v));
                }
            } else if (strncmp(t, "define(", 7) == 0) {
                char *v = t + 7;
                char *end = strchr(v, ')');
                if (end) {
                    *end = '\0';
                    if (cur_target->define_count < KEX_MAX_DEFS)
                        snprintf(cur_target->defines[cur_target->define_count++], 256, "%s", trim(v));
                }
            } else if (strncmp(t, "opt(", 4) == 0) {
                char *v = t + 4;
                char *end = strchr(v, ')');
                if (end) { *end = '\0'; cur_target->opt_level = atoi(trim(v)); }
            } else if (strncmp(t, "debug(", 6) == 0) {
                cur_target->debug_info = true;
            } else if (strncmp(t, "strip(", 6) == 0) {
                cur_target->strip = true;
            }
        }
    }

    free(data);
    return 0;
}

static int build_target(kbuild_config_t *bcfg, kbuild_target_t *tgt) {
    kex_config_t cfg;
    config_defaults(&cfg);
    snprintf(cfg.cc, sizeof(cfg.cc), "%s", bcfg->cc);
    snprintf(cfg.cxx, sizeof(cfg.cxx), "%s", bcfg->cxx);
    cfg.verbose = bcfg->verbose;
    cfg.opt_level = tgt->opt_level;
    cfg.debug_info = tgt->debug_info;
    cfg.strip = tgt->strip;
    cfg.output_format = tgt->format;
    cfg.is_shared = tgt->is_shared;

    for (int i = 0; i < bcfg->inc_dir_count; i++)
        snprintf(cfg.inc_dirs[cfg.inc_dir_count++], KEX_PATH_MAX, "%s", bcfg->inc_dirs[i]);
    for (int i = 0; i < tgt->inc_dir_count; i++)
        snprintf(cfg.inc_dirs[cfg.inc_dir_count++], KEX_PATH_MAX, "%s", tgt->inc_dirs[i]);
    for (int i = 0; i < bcfg->define_count; i++)
        snprintf(cfg.defines[cfg.define_count++], 256, "%s", bcfg->defines[i]);
    for (int i = 0; i < tgt->define_count; i++)
        snprintf(cfg.defines[cfg.define_count++], 256, "%s", tgt->defines[i]);
    for (int i = 0; i < bcfg->lib_dir_count; i++)
        snprintf(cfg.lib_dirs[cfg.lib_dir_count++], KEX_PATH_MAX, "%s", bcfg->lib_dirs[i]);

    for (int i = 0; i < tgt->source_count; i++) {
        snprintf(cfg.sources[cfg.source_count], KEX_PATH_MAX, "%s", tgt->sources[i]);
        cfg.source_langs[cfg.source_count] = detect_lang(tgt->sources[i]);
        cfg.source_count++;
    }

    if (tgt->output[0]) {
        snprintf(cfg.output, sizeof(cfg.output), "%s", tgt->output);
    } else {
        const char *ext = (tgt->format == KEX_FMT_ELF) ? ".elf" :
                          (tgt->format == KEX_FMT_KXP) ? ".kxp" : ".kex";
        snprintf(cfg.output, sizeof(cfg.output), "%s%s", tgt->name, ext);
    }

    snprintf(cfg.name, sizeof(cfg.name), "%s", tgt->name);

    return do_compile(&cfg);
}

static int do_build(const char *build_file, bool verbose, const char *target_name) {
    kbuild_config_t bcfg;
    if (parse_build_file(build_file, &bcfg) != 0) {
        fprintf(stderr, "kex: 无法解析构建文件: %s\n", build_file);
        return KEX_ERR_FORMAT;
    }
    bcfg.verbose = verbose;

    if (bcfg.target_count == 0) {
        fprintf(stderr, "kex: 未定义构建目标\n");
        return KEX_ERR_ARG;
    }

    for (int i = 0; i < bcfg.target_count; i++) {
        kbuild_target_t *tgt = &bcfg.targets[i];
        if (target_name && strcmp(tgt->name, target_name) != 0) continue;

        fprintf(stderr, "[kex] 构建目标: %s\n", tgt->name);
        int ret = build_target(&bcfg, tgt);
        if (ret != 0) {
            fprintf(stderr, "kex: 目标 %s 构建失败\n", tgt->name);
            return ret;
        }
    }

    return KEX_OK;
}

static int do_clean(const char *build_file, bool verbose) {
    kbuild_config_t bcfg;
    if (parse_build_file(build_file, &bcfg) != 0) return 0;

    for (int i = 0; i < bcfg.target_count; i++) {
        kbuild_target_t *tgt = &bcfg.targets[i];
        const char *ext = (tgt->format == KEX_FMT_ELF) ? ".elf" :
                          (tgt->format == KEX_FMT_KXP) ? ".kxp" : ".kex";
        char out[KEX_PATH_MAX];
        if (tgt->output[0])
            snprintf(out, sizeof(out), "%s", tgt->output);
        else
            snprintf(out, sizeof(out), "%s%s", tgt->name, ext);
        if (file_exists(out)) {
            if (verbose) fprintf(stderr, "[kex] 删除 %s\n", out);
            remove(out);
        }
    }
    return 0;
}

static void print_usage(void) {
    fprintf(stderr,
        "kex - Kenux 编译器 v%d.%d.%d\n"
        "用法:\n"
        "  kex [选项] <源文件...>\n"
        "  kex build [选项]\n"
        "  kex run <kex文件> [选项]\n"
        "  kex clean\n"
        "  kex init\n"
        "\n"
        "编译选项:\n"
        "  -o <路径>        输出文件\n"
        "  --format <格式>  输出格式: kex, kxp, elf (默认: kex)\n"
        "  --name <名称>    程序名 (嵌入kex头部)\n"
        "  --shared         构建共享库 (.kxp)\n"
        "  -O<级别>         优化级别 (0,1,2,3,s)\n"
        "  -g               包含调试信息\n"
        "  -s               剥离符号\n"
        "  -static          静态链接\n"
        "  -I<目录>         添加头文件目录\n"
        "  -L<目录>         添加库目录\n"
        "  -D<定义>         添加预处理器定义\n"
        "  -l<库>           链接库\n"
        "  --cc <路径>      C 编译器\n"
        "  --cxx <路径>     C++ 编译器\n"
        "  --verbose        详细输出\n"
        "  --keep-temps     保留临时文件\n"
        "\n"
        "运行选项:\n"
        "  --gfx            强制图形窗口\n"
        "  --gfx-size WxH   图形窗口尺寸\n"
        "\n"
        "构建选项:\n"
        "  -f <文件>        构建文件 (默认: KenuxBuild)\n"
        "  --target <名称>  构建指定目标\n"
        "\n"
        "安装选项:\n"
        "  install          安装kex到系统PATH\n"
        "  install --prefix <路径>  安装到指定目录\n"
        "  uninstall        从系统PATH卸载kex\n",
        KEX_VERSION_MAJOR, KEX_VERSION_MINOR, KEX_VERSION_PATCH);
}

static int do_install(const char *prefix) {
    char self_path[KEX_PATH_MAX] = {0};
    char dest[KEX_PATH_MAX] = {0};

#ifdef _WIN32
    DWORD len = GetModuleFileNameA(NULL, self_path, KEX_PATH_MAX);
    if (len == 0) {
        fprintf(stderr, "kex: 无法获取当前可执行文件路径\n");
        return KEX_ERR_IO;
    }
    const char *install_dir = prefix ? prefix : "C:\\Kenux";
    snprintf(dest, sizeof(dest), "%s\\kex.exe", install_dir);
#else
    ssize_t len = readlink("/proc/self/exe", self_path, sizeof(self_path) - 1);
    if (len <= 0) {
        fprintf(stderr, "kex: 无法获取当前可执行文件路径\n");
        return KEX_ERR_IO;
    }
    self_path[len] = '\0';
    const char *install_dir = prefix ? prefix : "/usr/local/bin";
    snprintf(dest, sizeof(dest), "%s/kex", install_dir);
#endif

    struct stat st;
    if (stat(install_dir, &st) != 0) {
#ifdef _WIN32
        char mkdir_cmd[KEX_PATH_MAX];
        snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir \"%s\"", install_dir);
        system(mkdir_cmd);
#else
        mkdirs((char *)install_dir);
#endif
    }

    FILE *src = fopen(self_path, "rb");
    if (!src) {
        fprintf(stderr, "kex: 无法打开自身: %s\n", self_path);
        return KEX_ERR_IO;
    }
    FILE *dst = fopen(dest, "wb");
    if (!dst) {
        fprintf(stderr, "kex: 无法写入目标: %s (可能需要sudo)\n", dest);
        fclose(src);
        return KEX_ERR_IO;
    }

    char buf[65536];
    size_t total = 0;
    while (1) {
        size_t n = fread(buf, 1, sizeof(buf), src);
        if (n == 0) break;
        if (fwrite(buf, 1, n, dst) != n) {
            fprintf(stderr, "kex: 写入失败\n");
            fclose(src); fclose(dst);
            return KEX_ERR_IO;
        }
        total += n;
    }
    fclose(src);
    fclose(dst);

#ifndef _WIN32
    chmod(dest, 0755);
#endif

    printf("kex: 已安装到 %s (%zu 字节)\n", dest, total);

#ifdef _WIN32
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment",
                       0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        char path_val[4096];
        DWORD path_len = sizeof(path_val);
        if (RegQueryValueExA(hKey, "Path", NULL, NULL, (LPBYTE)path_val, &path_len) == ERROR_SUCCESS) {
            if (strstr(path_val, install_dir) == NULL) {
                char new_path[8192];
                snprintf(new_path, sizeof(new_path), "%s;%s", path_val, install_dir);
                RegSetValueExA(hKey, "Path", 0, REG_EXPAND_SZ, (LPBYTE)new_path, (DWORD)strlen(new_path)+1);
                printf("kex: 已添加到系统PATH (需重新打开终端)\n");
            }
            RegCloseKey(hKey);
        }
    }
#endif

    return KEX_OK;
}

static int do_uninstall(void) {
    int removed = 0;

#ifdef _WIN32
    const char *paths[] = {
        "C:\\Kenux\\kex.exe",
        "C:\\Program Files\\Kenux\\kex.exe",
        NULL
    };
    for (int i = 0; paths[i]; i++) {
        if (DeleteFileA(paths[i])) {
            printf("kex: 已删除 %s\n", paths[i]);
            removed++;
        }
    }
#else
    const char *paths[] = {
        "/usr/local/bin/kex",
        "/usr/bin/kex",
        NULL
    };
    for (int i = 0; paths[i]; i++) {
        if (unlink(paths[i]) == 0) {
            printf("kex: 已删除 %s\n", paths[i]);
            removed++;
        }
    }
#endif

    if (removed == 0) {
        fprintf(stderr, "kex: 未找到已安装的kex\n");
        return KEX_ERR_IO;
    }

    printf("kex: 卸载完成\n");
    return KEX_OK;
}

static int do_init(void) {
    const char *path = "KenuxBuild";
    if (file_exists(path)) {
        fprintf(stderr, "kex: %s 已存在\n", path);
        return KEX_ERR_IO;
    }
    const char *tpl =
        "project(myapp)\n"
        "\n"
        "set(CC gcc)\n"
        "set(CXX g++)\n"
        "set(OPT 2)\n"
        "\n"
        "include_dir(./include)\n"
        "\n"
        "target(myapp)\n"
        "  sources(main.c utils.c)\n"
        "  format(kex)\n"
        "  output(myapp.kex)\n"
        "\n"
        "target(mylib)\n"
        "  sources(lib.c)\n"
        "  format(kxp)\n"
        "  output(mylib.kxp)\n"
        "  shared()\n"
        "\n"
        "target(native_tool)\n"
        "  sources(tool.c)\n"
        "  format(elf)\n"
        "  output(tool)\n";
    return file_write(path, tpl, strlen(tpl));
}

int main(int argc, char **argv) {
    if (argc < 2) { print_usage(); return 1; }

    crc32_init_table();

    if (strcmp(argv[1], "build") == 0) {
        const char *build_file = "KenuxBuild";
        bool verbose = false;
        const char *target_name = NULL;
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "-f") == 0 && i+1 < argc) build_file = argv[++i];
            else if (strcmp(argv[i], "--verbose") == 0) verbose = true;
            else if (strcmp(argv[i], "--target") == 0 && i+1 < argc) target_name = argv[++i];
        }
        return do_build(build_file, verbose, target_name);
    }

    if (strcmp(argv[1], "run") == 0) {
        if (argc < 3) { fprintf(stderr, "kex run: 需要kex文件\n"); return 1; }
        bool force_gfx = false;
        int gfx_w = 800, gfx_h = 600;
        for (int i = 3; i < argc; i++) {
            if (strcmp(argv[i], "--gfx") == 0) force_gfx = true;
            else if (strcmp(argv[i], "--gfx-size") == 0 && i+1 < argc) {
                i++;
                sscanf(argv[i], "%dx%d", &gfx_w, &gfx_h);
            }
        }
        return do_run(argv[2], force_gfx, gfx_w, gfx_h);
    }

    if (strcmp(argv[1], "clean") == 0) {
        const char *build_file = "KenuxBuild";
        bool verbose = false;
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "-f") == 0 && i+1 < argc) build_file = argv[++i];
            else if (strcmp(argv[i], "--verbose") == 0) verbose = true;
        }
        return do_clean(build_file, verbose);
    }

    if (strcmp(argv[1], "init") == 0) {
        return do_init();
    }

    if (strcmp(argv[1], "install") == 0) {
        const char *prefix = NULL;
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--prefix") == 0 && i+1 < argc) prefix = argv[++i];
        }
        return do_install(prefix);
    }

    if (strcmp(argv[1], "uninstall") == 0) {
        return do_uninstall();
    }

    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        print_usage();
        return 0;
    }

    kex_config_t cfg;
    config_defaults(&cfg);
    detect_toolchain(&cfg);

    int i = 1;
    while (i < argc) {
        if (strcmp(argv[i], "-o") == 0 && i+1 < argc) {
            snprintf(cfg.output, sizeof(cfg.output), "%s", argv[++i]);
        } else if (strcmp(argv[i], "--format") == 0 && i+1 < argc) {
            i++;
            if (strcmp(argv[i], "kex") == 0) cfg.output_format = KEX_FMT_KEX;
            else if (strcmp(argv[i], "kxp") == 0) cfg.output_format = KEX_FMT_KXP;
            else if (strcmp(argv[i], "elf") == 0) cfg.output_format = KEX_FMT_ELF;
            else { fprintf(stderr, "kex: 未知格式: %s\n", argv[i]); return 1; }
        } else if (strcmp(argv[i], "--name") == 0 && i+1 < argc) {
            snprintf(cfg.name, sizeof(cfg.name), "%s", argv[++i]);
        } else if (strcmp(argv[i], "--shared") == 0) {
            cfg.is_shared = 1;
            if (cfg.output_format == KEX_FMT_KEX) cfg.output_format = KEX_FMT_KXP;
        } else if (strncmp(argv[i], "-O", 2) == 0 && argv[i][2]) {
            char c = argv[i][2];
            if (c == '0') cfg.opt_level = KEX_OPT_O0;
            else if (c == '1') cfg.opt_level = KEX_OPT_O1;
            else if (c == '2') cfg.opt_level = KEX_OPT_O2;
            else if (c == '3') cfg.opt_level = KEX_OPT_O3;
            else if (c == 's' || c == 'S') cfg.opt_level = KEX_OPT_OS;
        } else if (strcmp(argv[i], "-g") == 0) {
            cfg.debug_info = true;
        } else if (strcmp(argv[i], "-s") == 0) {
            cfg.strip = true;
        } else if (strcmp(argv[i], "-static") == 0) {
            cfg.static_link = true;
        } else if (strncmp(argv[i], "-I", 2) == 0) {
            if (cfg.inc_dir_count < KEX_MAX_INC_DIRS)
                snprintf(cfg.inc_dirs[cfg.inc_dir_count++], KEX_PATH_MAX, "%s", argv[i]+2);
        } else if (strncmp(argv[i], "-L", 2) == 0) {
            if (cfg.lib_dir_count < KEX_MAX_LIB_DIRS)
                snprintf(cfg.lib_dirs[cfg.lib_dir_count++], KEX_PATH_MAX, "%s", argv[i]+2);
        } else if (strncmp(argv[i], "-D", 2) == 0) {
            if (cfg.define_count < KEX_MAX_DEFS)
                snprintf(cfg.defines[cfg.define_count++], 256, "%s", argv[i]+2);
        } else if (strncmp(argv[i], "-l", 2) == 0) {
            if (cfg.lib_count < KEX_MAX_FLAGS)
                snprintf(cfg.libs[cfg.lib_count++], 256, "%s", argv[i]+2);
        } else if (strcmp(argv[i], "--cc") == 0 && i+1 < argc) {
            snprintf(cfg.cc, sizeof(cfg.cc), "%s", argv[++i]);
        } else if (strcmp(argv[i], "--cxx") == 0 && i+1 < argc) {
            snprintf(cfg.cxx, sizeof(cfg.cxx), "%s", argv[++i]);
        } else if (strcmp(argv[i], "--verbose") == 0) {
            cfg.verbose = true;
        } else if (strcmp(argv[i], "--keep-temps") == 0) {
            cfg.keep_temps = true;
        } else if (argv[i][0] != '-') {
            if (cfg.source_count < KEX_MAX_SOURCES) {
                snprintf(cfg.sources[cfg.source_count], KEX_PATH_MAX, "%s", argv[i]);
                cfg.source_langs[cfg.source_count] = detect_lang(argv[i]);
                cfg.source_count++;
            }
        }
        i++;
    }

    if (cfg.source_count == 0) {
        fprintf(stderr, "kex: 没有输入文件\n");
        print_usage();
        return 1;
    }

    if (cfg.output[0] == '\0') {
        char basename[256];
        get_basename(basename, sizeof(basename), cfg.sources[0]);
        const char *ext = (cfg.output_format == KEX_FMT_ELF) ? ".elf" :
                          (cfg.output_format == KEX_FMT_KXP) ? ".kxp" : ".kex";
        snprintf(cfg.output, sizeof(cfg.output), "%s%s", basename, ext);
    }

    if (cfg.name[0] == '\0') {
        get_basename(cfg.name, sizeof(cfg.name), cfg.sources[0]);
    }

    return do_compile(&cfg);
}