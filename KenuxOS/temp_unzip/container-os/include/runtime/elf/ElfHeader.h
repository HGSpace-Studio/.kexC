#pragma once

#include <cstdint>
#include <array>

namespace elf {

// ELF 魔数定义
constexpr uint8_t EI_MAG0 = 0x7f;
constexpr uint8_t EI_MAG1 = 'E';
constexpr uint8_t EI_MAG2 = 'L';
constexpr uint8_t EI_MAG3 = 'F';

constexpr std::array<uint8_t, 4> ELF_MAGIC = {EI_MAG0, EI_MAG1, EI_MAG2, EI_MAG3};

// ELF 头部索引
constexpr int EI_CLASS = 4;
constexpr int EI_DATA = 5;
constexpr int EI_VERSION = 6;
constexpr int EI_OSABI = 7;
constexpr int EI_ABIVERSION = 8;
constexpr int EI_PAD = 9;
constexpr int EI_NIDENT = 16;

// ELF 类定义
constexpr uint8_t ELFCLASSNONE = 0;
constexpr uint8_t ELFCLASS32 = 1;
constexpr uint8_t ELFCLASS64 = 2;

// ELF 数据编码
constexpr uint8_t ELFDATANONE = 0;
constexpr uint8_t ELFDATA2LSB = 1;
constexpr uint8_t ELFDATA2MSB = 2;

// ELF 版本
constexpr uint32_t EV_NONE = 0;
constexpr uint32_t EV_CURRENT = 1;

// ELF OS/ABI 定义
constexpr uint8_t ELFOSABI_NONE = 0;
constexpr uint8_t ELFOSABI_SYSV = 0;
constexpr uint8_t ELFOSABI_HPUX = 1;
constexpr uint8_t ELFOSABI_NETBSD = 2;
constexpr uint8_t ELFOSABI_GNU = 3;
constexpr uint8_t ELFOSABI_LINUX = 3;
constexpr uint8_t ELFOSABI_SOLARIS = 6;
constexpr uint8_t ELFOSABI_AIX = 7;
constexpr uint8_t ELFOSABI_IRIX = 8;
constexpr uint8_t ELFOSABI_FREEBSD = 9;
constexpr uint8_t ELFOSABI_TRU64 = 10;
constexpr uint8_t ELFOSABI_MODESTO = 11;
constexpr uint8_t ELFOSABI_OPENBSD = 12;
constexpr uint8_t ELFOSABI_OPENVMS = 13;
constexpr uint8_t ELFOSABI_NSK = 14;
constexpr uint8_t ELFOSABI_AROS = 15;
constexpr uint8_t ELFOSABI_ARM = 97;
constexpr uint8_t ELFOSABI_STANDALONE = 255;

// ELF 类型
constexpr uint16_t ET_NONE = 0;
constexpr uint16_t ET_REL = 1;
constexpr uint16_t ET_EXEC = 2;
constexpr uint16_t ET_DYN = 3;
constexpr uint16_t ET_CORE = 4;

// ELF 机器类型
constexpr uint16_t EM_NONE = 0;
constexpr uint16_t EM_386 = 3;
constexpr uint16_t EM_X86_64 = 62;
constexpr uint16_t EM_ARM = 40;
constexpr uint16_t EM_AARCH64 = 183;
constexpr uint16_t EM_MIPS = 8;
constexpr uint16_t EM_PPC = 20;
constexpr uint16_t EM_PPC64 = 21;
constexpr uint16_t EM_SPARC = 2;

// ELF 段类型
constexpr uint32_t PT_NULL = 0;
constexpr uint32_t PT_LOAD = 1;
constexpr uint32_t PT_DYNAMIC = 2;
constexpr uint32_t PT_INTERP = 3;
constexpr uint32_t PT_NOTE = 4;
constexpr uint32_t PT_SHLIB = 5;
constexpr uint32_t PT_PHDR = 6;
constexpr uint32_t PT_TLS = 7;
constexpr uint32_t PT_LOOS = 0x60000000;
constexpr uint32_t PT_HIOS = 0x6fffffff;
constexpr uint32_t PT_LOPROC = 0x70000000;
constexpr uint32_t PT_HIPROC = 0x7fffffff;

// ELF 段标志
constexpr uint32_t PF_R = 0x4;
constexpr uint32_t PF_W = 0x2;
constexpr uint32_t PF_X = 0x1;
constexpr uint32_t PF_MASKOS = 0x0ff00000;
constexpr uint32_t PF_MASKPROC = 0xf0000000;

// ELF 节区类型
constexpr uint32_t SHT_NULL = 0;
constexpr uint32_t SHT_PROGBITS = 1;
constexpr uint32_t SHT_SYMTAB = 2;
constexpr uint32_t SHT_STRTAB = 3;
constexpr uint32_t SHT_RELA = 4;
constexpr uint32_t SHT_HASH = 5;
constexpr uint32_t SHT_DYNAMIC = 6;
constexpr uint32_t SHT_NOTE = 7;
constexpr uint32_t SHT_NOBITS = 8;
constexpr uint32_t SHT_REL = 9;
constexpr uint32_t SHT_SHLIB = 10;
constexpr uint32_t SHT_DYNSYM = 11;
constexpr uint32_t SHT_LOOS = 0x60000000;
constexpr uint32_t SHT_HIOS = 0x6fffffff;
constexpr uint32_t SHT_LOPROC = 0x70000000;
constexpr uint32_t SHT_HIPROC = 0x7fffffff;

// ELF 节区标志
constexpr uint64_t SHF_WRITE = 0x1;
constexpr uint64_t SHF_ALLOC = 0x2;
constexpr uint64_t SHF_EXECINSTR = 0x4;
constexpr uint64_t SHF_MASKOS = 0x0f000000;
constexpr uint64_t SHF_MASKPROC = 0xf0000000;
constexpr uint64_t SHF_INFO_LINK = 0x40;
constexpr uint64_t SHF_LINK_ORDER = 0x80;
constexpr uint64_t SHF_OS_NONCONFORMING = 0x100;
constexpr uint64_t SHF_GROUP = 0x200;
constexpr uint64_t SHF_TLS = 0x400;

// ELF 符号绑定
constexpr uint8_t STB_LOCAL = 0;
constexpr uint8_t STB_GLOBAL = 1;
constexpr uint8_t STB_WEAK = 2;
constexpr uint8_t STB_LOOS = 10;
constexpr uint8_t STB_HIOS = 12;
constexpr uint8_t STB_LOPROC = 13;
constexpr uint8_t STB_HIPROC = 15;

// ELF 符号类型
constexpr uint8_t STT_NOTYPE = 0;
constexpr uint8_t STT_OBJECT = 1;
constexpr uint8_t STT_FUNC = 2;
constexpr uint8_t STT_SECTION = 3;
constexpr uint8_t STT_FILE = 4;
constexpr uint8_t STT_COMMON = 5;
constexpr uint8_t STT_TLS = 6;
constexpr uint8_t STT_LOOS = 10;
constexpr uint8_t STT_HIOS = 12;
constexpr uint8_t STT_LOPROC = 13;
constexpr uint8_t STT_HIPROC = 15;

// ELF 符号特殊节区索引
constexpr uint16_t SHN_UNDEF = 0;
constexpr uint16_t SHN_LORESERVE = 0xff00;
constexpr uint16_t SHN_LOPROC = 0xff00;
constexpr uint16_t SHN_HIPROC = 0xff1f;
constexpr uint16_t SHN_LOOS = 0xff20;
constexpr uint16_t SHN_HIOS = 0xff3f;
constexpr uint16_t SHN_ABS = 0xfff1;
constexpr uint16_t SHN_COMMON = 0xfff2;
constexpr uint16_t SHN_XINDEX = 0xffff;

// i386 重定位类型
constexpr uint32_t R_386_NONE = 0;
constexpr uint32_t R_386_32 = 1;
constexpr uint32_t R_386_PC32 = 2;
constexpr uint32_t R_386_GOT32 = 3;
constexpr uint32_t R_386_PLT32 = 4;
constexpr uint32_t R_386_COPY = 5;
constexpr uint32_t R_386_GLOB_DAT = 6;
constexpr uint32_t R_386_JMP_SLOT = 7;
constexpr uint32_t R_386_RELATIVE = 8;
constexpr uint32_t R_386_GOTOFF = 9;
constexpr uint32_t R_386_GOTPC = 10;

// x86-64 重定位类型
constexpr uint32_t R_X86_64_NONE = 0;
constexpr uint32_t R_X86_64_64 = 1;
constexpr uint32_t R_X86_64_PC32 = 2;
constexpr uint32_t R_X86_64_GOT32 = 3;
constexpr uint32_t R_X86_64_PLT32 = 4;
constexpr uint32_t R_X86_64_COPY = 5;
constexpr uint32_t R_X86_64_GLOB_DAT = 6;
constexpr uint32_t R_X86_64_JMP_SLOT = 7;
constexpr uint32_t R_X86_64_RELATIVE = 8;
constexpr uint32_t R_X86_64_GOTPCREL = 9;
constexpr uint32_t R_X86_64_32 = 10;
constexpr uint32_t R_X86_64_32S = 11;

// ELF32 头部结构
struct Elf32_Ehdr {
    uint8_t e_ident[EI_NIDENT];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

// ELF64 头部结构
struct Elf64_Ehdr {
    uint8_t e_ident[EI_NIDENT];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

// ELF32 程序段表项
struct Elf32_Phdr {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
};

// ELF64 程序段表项
struct Elf64_Phdr {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
};

// ELF32 节区表项
struct Elf32_Shdr {
    uint32_t sh_name;
    uint32_t sh_type;
    uint32_t sh_flags;
    uint32_t sh_addr;
    uint32_t sh_offset;
    uint32_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint32_t sh_addralign;
    uint32_t sh_entsize;
};

// ELF64 节区表项
struct Elf64_Shdr {
    uint32_t sh_name;
    uint32_t sh_type;
    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;
    uint64_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize;
};

// ELF32 符号表项
struct Elf32_Sym {
    uint32_t st_name;
    uint32_t st_value;
    uint32_t st_size;
    uint8_t st_info;
    uint8_t st_other;
    uint16_t st_shndx;
};

// ELF64 符号表项
struct Elf64_Sym {
    uint32_t st_name;
    uint8_t st_info;
    uint8_t st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
};

// ELF32 重定位表项 (无 addend)
struct Elf32_Rel {
    uint32_t r_offset;
    uint32_t r_info;
};

// ELF32 重定位表项 (有 addend)
struct Elf32_Rela {
    uint32_t r_offset;
    uint32_t r_info;
    int32_t r_addend;
};

// ELF64 重定位表项 (无 addend)
struct Elf64_Rel {
    uint64_t r_offset;
    uint64_t r_info;
};

// ELF64 重定位表项 (有 addend)
struct Elf64_Rela {
    uint64_t r_offset;
    uint64_t r_info;
    int64_t r_addend;
};

// 从符号信息字节中提取绑定
constexpr uint8_t ELF32_ST_BIND(uint8_t info) { return info >> 4; }
constexpr uint8_t ELF64_ST_BIND(uint8_t info) { return info >> 4; }

// 从符号信息字节中提取类型
constexpr uint8_t ELF32_ST_TYPE(uint8_t info) { return info & 0xf; }
constexpr uint8_t ELF64_ST_TYPE(uint8_t info) { return info & 0xf; }

// 组合符号绑定和类型
constexpr uint8_t ELF32_ST_INFO(uint8_t bind, uint8_t type) { return (bind << 4) | (type & 0xf); }
constexpr uint8_t ELF64_ST_INFO(uint8_t bind, uint8_t type) { return (bind << 4) | (type & 0xf); }

// 从重定位信息中提取符号索引
constexpr uint32_t ELF32_R_SYM(uint32_t info) { return info >> 8; }
constexpr uint64_t ELF64_R_SYM(uint64_t info) { return info >> 32; }

// 从重定位信息中提取重定位类型
constexpr uint8_t ELF32_R_TYPE(uint32_t info) { return info & 0xff; }
constexpr uint32_t ELF64_R_TYPE(uint64_t info) { return info & 0xffffffff; }

// 组合重定位符号索引和类型
constexpr uint32_t ELF32_R_INFO(uint32_t sym, uint8_t type) { return (sym << 8) | (type & 0xff); }
constexpr uint64_t ELF64_R_INFO(uint64_t sym, uint32_t type) { return (sym << 32) | (type & 0xffffffff); }

} // namespace elf
