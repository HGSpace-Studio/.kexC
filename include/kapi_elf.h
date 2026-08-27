#ifndef KAPI_ELF_H
#define KAPI_ELF_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_EI_NIDENT     16
#define KAPI_EI_MAG0       0
#define KAPI_EI_MAG1       1
#define KAPI_EI_MAG2       2
#define KAPI_EI_MAG3       3
#define KAPI_EI_CLASS      4
#define KAPI_EI_DATA       5
#define KAPI_EI_VERSION    6
#define KAPI_EI_OSABI      7
#define KAPI_EI_ABIVERSION 8

#define KAPI_ELFMAG0       0x7F
#define KAPI_ELFMAG1       0x45
#define KAPI_ELFMAG2       0x4C
#define KAPI_ELFMAG3       0x46

#define KAPI_ELFCLASSNONE  0
#define KAPI_ELFCLASS32    1
#define KAPI_ELFCLASS64    2

#define KAPI_ELFDATA2LSB   1
#define KAPI_ELFDATA2MSB   2

#define KAPI_EV_CURRENT    1

#define KAPI_ET_NONE       0
#define KAPI_ET_REL        1
#define KAPI_ET_EXEC       2
#define KAPI_ET_DYN        3
#define KAPI_ET_CORE       4

#define KAPI_EM_X86_64     62

#define KAPI_SHT_NULL      0
#define KAPI_SHT_PROGBITS  1
#define KAPI_SHT_SYMTAB    2
#define KAPI_SHT_STRTAB    3
#define KAPI_SHT_RELA      4
#define KAPI_SHT_HASH      5
#define KAPI_SHT_DYNAMIC   6
#define KAPI_SHT_NOTE      7
#define KAPI_SHT_NOBITS    8
#define KAPI_SHT_REL       9
#define KAPI_SHT_SHLIB     10
#define KAPI_SHT_DYNSYM    11
#define KAPI_SHT_INIT_ARRAY  14
#define KAPI_SHT_FINI_ARRAY  15
#define KAPI_SHT_GNU_HASH   0x6ffffff6
#define KAPI_SHT_GNU_VERSYM 0x6fffffff
#define KAPI_SHT_GNU_VERDEF 0x6ffffffd
#define KAPI_SHT_GNU_VERNEED 0x6ffffffe

#define KAPI_SHF_WRITE     0x1
#define KAPI_SHF_ALLOC     0x2
#define KAPI_SHF_EXECINSTR 0x4

#define KAPI_STB_LOCAL     0
#define KAPI_STB_GLOBAL    1
#define KAPI_STB_WEAK      2

#define KAPI_STT_NOTYPE    0
#define KAPI_STT_OBJECT    1
#define KAPI_STT_FUNC      2
#define KAPI_STT_SECTION   3
#define KAPI_STT_FILE      4
#define KAPI_STT_COMMON    5
#define KAPI_STT_TLS       6

#define KAPI_R_X86_64_NONE      0
#define KAPI_R_X86_64_64        1
#define KAPI_R_X86_64_PC32      2
#define KAPI_R_X86_64_GOT32     3
#define KAPI_R_X86_64_PLT32     4
#define KAPI_R_X86_64_COPY      5
#define KAPI_R_X86_64_GLOB_DAT  6
#define KAPI_R_X86_64_JMP_SLOT  7
#define KAPI_R_X86_64_RELATIVE  8
#define KAPI_R_X86_64_GOTPCREL  9
#define KAPI_R_X86_64_32       10
#define KAPI_R_X86_64_32S      11
#define KAPI_R_X86_64_16       12
#define KAPI_R_X86_64_PC16     13
#define KAPI_R_X86_64_8        14
#define KAPI_R_X86_64_PC8      15
#define KAPI_R_X86_64_DTPMOD64 16
#define KAPI_R_X86_64_DTPOFF64 17
#define KAPI_R_X86_64_TPOFF64  18
#define KAPI_R_X86_64_TLSGD    19
#define KAPI_R_X86_64_TLSLD    20
#define KAPI_R_X86_64_DTPOFF32 21
#define KAPI_R_X86_64_GOTTPOFF 22
#define KAPI_R_X86_64_TPOFF32  23
#define KAPI_R_X86_64_IRELATIVE 37

#define KAPI_DT_NULL         0
#define KAPI_DT_NEEDED       1
#define KAPI_DT_PLTRELSZ     2
#define KAPI_DT_PLTGOT       3
#define KAPI_DT_HASH         4
#define KAPI_DT_STRTAB       5
#define KAPI_DT_SYMTAB       6
#define KAPI_DT_RELA         7
#define KAPI_DT_RELASZ       8
#define KAPI_DT_RELAENT      9
#define KAPI_DT_STRSZ        10
#define KAPI_DT_SYMENT       11
#define KAPI_DT_INIT         12
#define KAPI_DT_FINI         13
#define KAPI_DT_SONAME       14
#define KAPI_DT_RPATH        15
#define KAPI_DT_SYMBOLIC     16
#define KAPI_DT_REL          17
#define KAPI_DT_RELSZ        18
#define KAPI_DT_RELENT       19
#define KAPI_DT_PLTREL       20
#define KAPI_DT_DEBUG        21
#define KAPI_DT_TEXTREL      22
#define KAPI_DT_JMPREL       23
#define KAPI_DT_BIND_NOW     24
#define KAPI_DT_INIT_ARRAY   25
#define KAPI_DT_FINI_ARRAY   26
#define KAPI_DT_INIT_ARRAYSZ 27
#define KAPI_DT_FINI_ARRAYSZ 28
#define KAPI_DT_RUNPATH      29
#define KAPI_DT_FLAGS        30
#define KAPI_DT_FLAGS_1      0x6ffffffb
#define KAPI_DT_VERDEF       0x6ffffffc
#define KAPI_DT_VERDEFNUM    0x6ffffffd
#define KAPI_DT_VERSYM       0x6fffffff
#define KAPI_DT_VERNEED      0x6ffffffe
#define KAPI_DT_VERNEEDNUM   0x6fffffff
#define KAPI_DT_GNU_HASH     0x6ffffef2

#define KAPI_PF_X            0x1
#define KAPI_PF_W            0x2
#define KAPI_PF_R            0x4

#define KAPI_AT_NULL         0
#define KAPI_AT_IGNORE       1
#define KAPI_AT_EXECFD       2
#define KAPI_AT_PHDR         3
#define KAPI_AT_PHENT        4
#define KAPI_AT_PHNUM        5
#define KAPI_AT_PAGESZ       6
#define KAPI_AT_BASE         7
#define KAPI_AT_FLAGS        8
#define KAPI_AT_ENTRY        9
#define KAPI_AT_UID          10
#define KAPI_AT_EUID         11
#define KAPI_AT_GID          12
#define KAPI_AT_EGID         13
#define KAPI_AT_CLKTCK       17
#define KAPI_AT_PLATFORM     15
#define KAPI_AT_HWCAP        16
#define KAPI_AT_SYSINFO      32
#define KAPI_AT_SYSINFO_EHDR 33
#define KAPI_AT_RANDOM       25
#define KAPI_AT_SECURE       23

typedef uint16_t kapi_elf_half_t;
typedef uint32_t kapi_elf_word_t;
typedef int32_t  kapi_elf_sword_t;
typedef uint64_t kapi_elf_xword_t;
typedef int64_t  kapi_elf_sxword_t;
typedef uint64_t kapi_elf_addr_t;
typedef uint64_t kapi_elf_off_t;

typedef struct {
    unsigned char e_ident[KAPI_EI_NIDENT];
    kapi_elf_half_t e_type;
    kapi_elf_half_t e_machine;
    kapi_elf_word_t e_version;
    kapi_elf_addr_t e_entry;
    kapi_elf_off_t  e_phoff;
    kapi_elf_off_t  e_shoff;
    kapi_elf_word_t e_flags;
    kapi_elf_half_t e_ehsize;
    kapi_elf_half_t e_phentsize;
    kapi_elf_half_t e_phnum;
    kapi_elf_half_t e_shentsize;
    kapi_elf_half_t e_shnum;
    kapi_elf_half_t e_shstrndx;
} kapi_elf64_ehdr_t;

typedef struct {
    kapi_elf_word_t p_type;
    kapi_elf_word_t p_flags;
    kapi_elf_off_t  p_offset;
    kapi_elf_addr_t p_vaddr;
    kapi_elf_addr_t p_paddr;
    kapi_elf_xword_t p_filesz;
    kapi_elf_xword_t p_memsz;
    kapi_elf_xword_t p_align;
} kapi_elf64_phdr_t;

typedef struct {
    kapi_elf_word_t sh_name;
    kapi_elf_word_t sh_type;
    kapi_elf_xword_t sh_flags;
    kapi_elf_addr_t sh_addr;
    kapi_elf_off_t  sh_offset;
    kapi_elf_xword_t sh_size;
    kapi_elf_word_t sh_link;
    kapi_elf_word_t sh_info;
    kapi_elf_xword_t sh_addralign;
    kapi_elf_xword_t sh_entsize;
} kapi_elf64_shdr_t;

typedef struct {
    kapi_elf_word_t st_name;
    unsigned char   st_info;
    unsigned char   st_other;
    kapi_elf_half_t st_shndx;
    kapi_elf_addr_t st_value;
    kapi_elf_xword_t st_size;
} kapi_elf64_sym_t;

typedef struct {
    kapi_elf_addr_t r_offset;
    kapi_elf_xword_t r_info;
} kapi_elf64_rela_t;

typedef struct {
    kapi_elf_xword_t d_tag;
    union {
        kapi_elf_xword_t d_val;
        kapi_elf_addr_t  d_ptr;
    } d_un;
} kapi_elf64_dyn_t;

typedef struct {
    kapi_elf_sxword_t a_val;
} kapi_elf64_auxv_t;

typedef struct kapi_elf_image kapi_elf_image_t;

struct kapi_elf_image {
    kapi_elf64_ehdr_t  ehdr;
    kapi_elf64_phdr_t* phdrs;
    kapi_elf64_shdr_t* shdrs;
    kapi_elf64_sym_t*  symtab;
    kapi_elf64_sym_t*  dynsym;
    char*              strtab;
    char*              dynstr;
    kapi_elf64_dyn_t*  dynamic;
    kapi_elf64_rela_t* rela;
    kapi_elf64_rela_t* jmprel;
    kapi_elf_half_t    phnum;
    kapi_elf_half_t    shnum;
    kapi_elf_word_t    symcount;
    kapi_elf_word_t    dynsymcount;
    kapi_elf_word_t    relacount;
    kapi_elf_word_t    jmprelcount;
    kapi_elf_addr_t    base_addr;
    kapi_elf_addr_t    entry_addr;
    kapi_elf_addr_t    interp_addr;
    void*              mapped_base;
    size_t             mapped_size;
    int                fd;
    int                is_dynamic;
    int                is_interpreted;
    char               pathname[256];
    char               interp_path[256];
    void**             got;
    void*              tls_template;
    size_t             tls_template_size;
    size_t             tls_align;
};

typedef struct {
    kapi_elf_addr_t  base;
    kapi_elf_addr_t  bias;
    kapi_elf_addr_t  entry;
    kapi_elf_addr_t  phdr;
    kapi_elf_half_t  phnum;
    kapi_elf_half_t  phent;
    kapi_elf_xword_t pagesz;
    void*            interp_base;
    kapi_elf_addr_t  interp_entry;
    kapi_elf64_auxv_t auxv[32];
    int              auxv_count;
} kapi_elf_load_info_t;

typedef struct {
    char  name[256];
    void* base;
    size_t size;
    kapi_elf64_sym_t* symtab;
    char* strtab;
    kapi_elf_word_t symcount;
    kapi_elf_half_t version;
} kapi_elf_shared_lib_t;

#define KAPI_ELF_MAX_SHARED     64

typedef struct {
    kapi_elf_shared_lib_t libs[KAPI_ELF_MAX_SHARED];
    int count;
} kapi_elf_link_map_t;

int kapi_elf_init(void);

kapi_elf_image_t* kapi_elf_load(const char* path);

kapi_elf_image_t* kapi_elf_load_from_fd(int fd);

kapi_elf_image_t* kapi_elf_load_from_memory(const void* data, size_t size);

int kapi_elf_unload(kapi_elf_image_t* image);

int kapi_elf_validate(const kapi_elf64_ehdr_t* ehdr);

int kapi_elf_load_phdrs(kapi_elf_image_t* image);

int kapi_elf_load_shdrs(kapi_elf_image_t* image);

int kapi_elf_load_segments(kapi_elf_image_t* image);

int kapi_elf_apply_relocations(kapi_elf_image_t* image);

int kapi_elf_resolve_symbol(kapi_elf_image_t* image, const char* name, void** addr);

void* kapi_elf_lookup_symbol(kapi_elf_image_t* image, const char* name);

int kapi_elf_resolve_got(kapi_elf_image_t* image);

int kapi_elf_resolve_plt(kapi_elf_image_t* image);

int kapi_elf_load_dynamic(kapi_elf_image_t* image);

int kapi_elf_load_shared_deps(kapi_elf_image_t* image, kapi_elf_link_map_t* link_map);

kapi_elf_load_info_t kapi_elf_map_binary(kapi_elf_image_t* image);

int kapi_elf_run_init(kapi_elf_image_t* image);

int kapi_elf_run_fini(kapi_elf_image_t* image);

int kapi_elf_setup_auxv(kapi_elf_load_info_t* info, int argc, char** argv, char** envp);

int kapi_elf_dlopen(const char* path, int mode);

int kapi_elf_dlclose(int handle);

void* kapi_elf_dlsym(int handle, const char* symbol);

int kapi_elf_dlinfo(int handle, int request, void* info);

char* kapi_elf_dlerror(void);

int kapi_elf_get_load_info(kapi_elf_image_t* image, kapi_elf_load_info_t* info);

int kapi_elf_read_interp(kapi_elf_image_t* image);

int kapi_elf_load_interp(kapi_elf_image_t* image);

int kapi_elf_build_id(kapi_elf_image_t* image, uint8_t* out, size_t* out_len);

uint32_t kapi_elf_gnu_hash(const char* name);

kapi_elf64_sym_t* kapi_elf_gnu_hash_lookup(kapi_elf_image_t* image, const char* name);

kapi_elf64_sym_t* kapi_elf_sysv_hash_lookup(kapi_elf_image_t* image, const char* name);

#ifdef __cplusplus
}
#endif

#endif