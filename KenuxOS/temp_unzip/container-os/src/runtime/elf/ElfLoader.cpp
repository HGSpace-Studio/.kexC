#include "runtime/elf/ElfLoader.h"

#include <fstream>
#include <algorithm>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace elf {

ElfLoader::ElfLoader()
    : m_is64Bit(false)
    , m_machine(EM_NONE)
    , m_type(ET_NONE)
    , m_entryPoint(0)
    , m_ehdr32(nullptr)
    , m_ehdr64(nullptr) {
}

ElfLoader::~ElfLoader() {
}

bool ElfLoader::load(const std::string& path) {
    // 读取文件内容
    if (!readFile(path)) {
        return false;
    }

    // 解析 ELF 头部
    if (!parseHeader()) {
        return false;
    }

    // 加载程序段
    if (!loadSegments()) {
        return false;
    }

    // 解析节区表
    if (!parseSections()) {
        return false;
    }

    // 解析符号表
    if (!parseSymbols()) {
        return false;
    }

    // 解析重定位表
    if (!parseRelocations()) {
        return false;
    }

    // 解析符号
    if (!resolveSymbols()) {
        return false;
    }

    // 执行重定位
    if (!relocate()) {
        return false;
    }

    return true;
}

bool ElfLoader::readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return false;
    }

    std::streampos size = file.tellg();
    m_fileData.resize(static_cast<size_t>(size));
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(m_fileData.data()), size);
    file.close();

    m_path = path;
    return true;
}

bool ElfLoader::parseHeader() {
    if (!verifyMagic()) {
        return false;
    }

    if (!parseElfClass()) {
        return false;
    }

    if (m_is64Bit) {
        m_ehdr64 = reinterpret_cast<const Elf64_Ehdr*>(m_fileData.data());
        m_machine = m_ehdr64->e_machine;
        m_type = m_ehdr64->e_type;
        m_entryPoint = m_ehdr64->e_entry;
    } else {
        m_ehdr32 = reinterpret_cast<const Elf32_Ehdr*>(m_fileData.data());
        m_machine = m_ehdr32->e_machine;
        m_type = m_ehdr32->e_type;
        m_entryPoint = m_ehdr32->e_entry;
    }

    return true;
}

bool ElfLoader::verifyMagic() {
    if (m_fileData.size() < EI_NIDENT) {
        return false;
    }

    for (int i = 0; i < 4; ++i) {
        if (m_fileData[i] != ELF_MAGIC[i]) {
            return false;
        }
    }

    return true;
}

bool ElfLoader::parseElfClass() {
    if (m_fileData.size() < EI_NIDENT) {
        return false;
    }

    uint8_t elfClass = m_fileData[EI_CLASS];
    switch (elfClass) {
        case ELFCLASS32:
            m_is64Bit = false;
            break;
        case ELFCLASS64:
            m_is64Bit = true;
            break;
        default:
            return false;
    }

    return true;
}

bool ElfLoader::loadSegments() {
    m_segments.clear();

    if (m_is64Bit) {
        if (!m_ehdr64) {
            return false;
        }

        uint64_t phoff = m_ehdr64->e_phoff;
        uint16_t phentsize = m_ehdr64->e_phentsize;
        uint16_t phnum = m_ehdr64->e_phnum;

        for (uint16_t i = 0; i < phnum; ++i) {
            uint64_t offset = phoff + static_cast<uint64_t>(i) * phentsize;
            if (offset + sizeof(Elf64_Phdr) > m_fileData.size()) {
                return false;
            }

            const Elf64_Phdr* phdr = reinterpret_cast<const Elf64_Phdr*>(m_fileData.data() + offset);
            if (!loadSegment64(*phdr)) {
                return false;
            }
        }
    } else {
        if (!m_ehdr32) {
            return false;
        }

        uint32_t phoff = m_ehdr32->e_phoff;
        uint16_t phentsize = m_ehdr32->e_phentsize;
        uint16_t phnum = m_ehdr32->e_phnum;

        for (uint16_t i = 0; i < phnum; ++i) {
            uint32_t offset = phoff + static_cast<uint32_t>(i) * phentsize;
            if (offset + sizeof(Elf32_Phdr) > m_fileData.size()) {
                return false;
            }

            const Elf32_Phdr* phdr = reinterpret_cast<const Elf32_Phdr*>(m_fileData.data() + offset);
            if (!loadSegment32(*phdr)) {
                return false;
            }
        }
    }

    return true;
}

bool ElfLoader::loadSegment32(const Elf32_Phdr& phdr) {
    if (phdr.p_type == PT_NULL) {
        return true;
    }

    Segment segment;
    segment.type = phdr.p_type;
    segment.vaddr = phdr.p_vaddr;
    segment.paddr = phdr.p_paddr;
    segment.filesz = phdr.p_filesz;
    segment.memsz = phdr.p_memsz;
    segment.flags = phdr.p_flags;
    segment.align = phdr.p_align;

    if (phdr.p_type == PT_LOAD && phdr.p_filesz > 0) {
        if (phdr.p_offset + phdr.p_filesz > m_fileData.size()) {
            return false;
        }
        segment.data.resize(phdr.p_filesz);
        std::memcpy(segment.data.data(), m_fileData.data() + phdr.p_offset, phdr.p_filesz);
    }

    m_segments.push_back(segment);
    return true;
}

bool ElfLoader::loadSegment64(const Elf64_Phdr& phdr) {
    if (phdr.p_type == PT_NULL) {
        return true;
    }

    Segment segment;
    segment.type = phdr.p_type;
    segment.vaddr = phdr.p_vaddr;
    segment.paddr = phdr.p_paddr;
    segment.filesz = phdr.p_filesz;
    segment.memsz = phdr.p_memsz;
    segment.flags = phdr.p_flags;
    segment.align = phdr.p_align;

    if (phdr.p_type == PT_LOAD && phdr.p_filesz > 0) {
        if (phdr.p_offset + phdr.p_filesz > m_fileData.size()) {
            return false;
        }
        segment.data.resize(phdr.p_filesz);
        std::memcpy(segment.data.data(), m_fileData.data() + phdr.p_offset, phdr.p_filesz);
    }

    m_segments.push_back(segment);
    return true;
}

bool ElfLoader::parseSections() {
    m_sections.clear();

    if (m_is64Bit) {
        return parseSections64();
    } else {
        return parseSections32();
    }
}

bool ElfLoader::parseSections32() {
    if (!m_ehdr32) {
        return false;
    }

    uint32_t shoff = m_ehdr32->e_shoff;
    uint16_t shentsize = m_ehdr32->e_shentsize;
    uint16_t shnum = m_ehdr32->e_shnum;
    uint16_t shstrndx = m_ehdr32->e_shstrndx;

    const char* shstrtab = nullptr;
    if (shstrndx != SHN_UNDEF) {
        uint32_t strtabOffset = shoff + static_cast<uint32_t>(shstrndx) * shentsize;
        if (strtabOffset + sizeof(Elf32_Shdr) <= m_fileData.size()) {
            const Elf32_Shdr* strtabShdr = reinterpret_cast<const Elf32_Shdr*>(m_fileData.data() + strtabOffset);
            if (strtabShdr->sh_offset + strtabShdr->sh_size <= m_fileData.size()) {
                shstrtab = reinterpret_cast<const char*>(m_fileData.data() + strtabShdr->sh_offset);
            }
        }
    }

    for (uint16_t i = 0; i < shnum; ++i) {
        uint32_t offset = shoff + static_cast<uint32_t>(i) * shentsize;
        if (offset + sizeof(Elf32_Shdr) > m_fileData.size()) {
            return false;
        }

        const Elf32_Shdr* shdr = reinterpret_cast<const Elf32_Shdr*>(m_fileData.data() + offset);

        Section section;
        if (shstrtab) {
            section.name = shstrtab + shdr->sh_name;
        }
        section.type = shdr->sh_type;
        section.flags = shdr->sh_flags;
        section.addr = shdr->sh_addr;
        section.offset = shdr->sh_offset;
        section.size = shdr->sh_size;
        section.link = shdr->sh_link;
        section.info = shdr->sh_info;
        section.align = shdr->sh_addralign;
        section.entsize = shdr->sh_entsize;

        if (shdr->sh_type != SHT_NOBITS && shdr->sh_size > 0) {
            if (shdr->sh_offset + shdr->sh_size <= m_fileData.size()) {
                section.data.resize(shdr->sh_size);
                std::memcpy(section.data.data(), m_fileData.data() + shdr->sh_offset, shdr->sh_size);
            }
        }

        m_sections.push_back(section);
    }

    return true;
}

bool ElfLoader::parseSections64() {
    if (!m_ehdr64) {
        return false;
    }

    uint64_t shoff = m_ehdr64->e_shoff;
    uint16_t shentsize = m_ehdr64->e_shentsize;
    uint16_t shnum = m_ehdr64->e_shnum;
    uint16_t shstrndx = m_ehdr64->e_shstrndx;

    const char* shstrtab = nullptr;
    if (shstrndx != SHN_UNDEF) {
        uint64_t strtabOffset = shoff + static_cast<uint64_t>(shstrndx) * shentsize;
        if (strtabOffset + sizeof(Elf64_Shdr) <= m_fileData.size()) {
            const Elf64_Shdr* strtabShdr = reinterpret_cast<const Elf64_Shdr*>(m_fileData.data() + strtabOffset);
            if (strtabShdr->sh_offset + strtabShdr->sh_size <= m_fileData.size()) {
                shstrtab = reinterpret_cast<const char*>(m_fileData.data() + strtabShdr->sh_offset);
            }
        }
    }

    for (uint16_t i = 0; i < shnum; ++i) {
        uint64_t offset = shoff + static_cast<uint64_t>(i) * shentsize;
        if (offset + sizeof(Elf64_Shdr) > m_fileData.size()) {
            return false;
        }

        const Elf64_Shdr* shdr = reinterpret_cast<const Elf64_Shdr*>(m_fileData.data() + offset);

        Section section;
        if (shstrtab) {
            section.name = shstrtab + shdr->sh_name;
        }
        section.type = shdr->sh_type;
        section.flags = shdr->sh_flags;
        section.addr = shdr->sh_addr;
        section.offset = shdr->sh_offset;
        section.size = shdr->sh_size;
        section.link = shdr->sh_link;
        section.info = shdr->sh_info;
        section.align = shdr->sh_addralign;
        section.entsize = shdr->sh_entsize;

        if (shdr->sh_type != SHT_NOBITS && shdr->sh_size > 0) {
            if (shdr->sh_offset + shdr->sh_size <= m_fileData.size()) {
                section.data.resize(shdr->sh_size);
                std::memcpy(section.data.data(), m_fileData.data() + shdr->sh_offset, shdr->sh_size);
            }
        }

        m_sections.push_back(section);
    }

    return true;
}

bool ElfLoader::parseSymbols() {
    m_symbols.clear();
    m_symbolTable.clear();

    if (m_is64Bit) {
        return parseSymbols64();
    } else {
        return parseSymbols32();
    }
}

bool ElfLoader::parseSymbols32() {
    const Section* strtab = nullptr;

    for (const auto& section : m_sections) {
        if (section.type == SHT_STRTAB && !section.name.empty()) {
            strtab = &section;
            break;
        }
    }

    if (!strtab) {
        return true;
    }

    for (const auto& section : m_sections) {
        if (section.type != SHT_SYMTAB && section.type != SHT_DYNSYM) {
            continue;
        }

        size_t numSymbols = section.size / sizeof(Elf32_Sym);
        const Elf32_Sym* symtab = reinterpret_cast<const Elf32_Sym*>(section.data.data());

        for (size_t i = 0; i < numSymbols; ++i) {
            const Elf32_Sym& sym = symtab[i];

            Symbol symbol;
            if (sym.st_name != 0 && sym.st_name < strtab->size) {
                symbol.name = reinterpret_cast<const char*>(strtab->data.data() + sym.st_name);
            }
            symbol.value = sym.st_value;
            symbol.size = sym.st_size;
            symbol.bind = ELF32_ST_BIND(sym.st_info);
            symbol.type = ELF32_ST_TYPE(sym.st_info);
            symbol.shndx = sym.st_shndx;

            m_symbols.push_back(symbol);

            if (!symbol.name.empty() && symbol.bind == STB_GLOBAL) {
                m_symbolTable[symbol.name] = symbol.value;
            }
        }
    }

    return true;
}

bool ElfLoader::parseSymbols64() {
    const Section* strtab = nullptr;

    for (const auto& section : m_sections) {
        if (section.type == SHT_STRTAB && !section.name.empty()) {
            strtab = &section;
            break;
        }
    }

    if (!strtab) {
        return true;
    }

    for (const auto& section : m_sections) {
        if (section.type != SHT_SYMTAB && section.type != SHT_DYNSYM) {
            continue;
        }

        size_t numSymbols = section.size / sizeof(Elf64_Sym);
        const Elf64_Sym* symtab = reinterpret_cast<const Elf64_Sym*>(section.data.data());

        for (size_t i = 0; i < numSymbols; ++i) {
            const Elf64_Sym& sym = symtab[i];

            Symbol symbol;
            if (sym.st_name != 0 && sym.st_name < strtab->size) {
                symbol.name = reinterpret_cast<const char*>(strtab->data.data() + sym.st_name);
            }
            symbol.value = sym.st_value;
            symbol.size = sym.st_size;
            symbol.bind = ELF64_ST_BIND(sym.st_info);
            symbol.type = ELF64_ST_TYPE(sym.st_info);
            symbol.shndx = sym.st_shndx;

            m_symbols.push_back(symbol);

            if (!symbol.name.empty() && symbol.bind == STB_GLOBAL) {
                m_symbolTable[symbol.name] = symbol.value;
            }
        }
    }

    return true;
}

bool ElfLoader::parseRelocations() {
    m_relocations.clear();

    if (m_is64Bit) {
        return parseRelocations64();
    } else {
        return parseRelocations32();
    }
}

bool ElfLoader::parseRelocations32() {
    for (const auto& section : m_sections) {
        if (section.type != SHT_REL && section.type != SHT_RELA) {
            continue;
        }

        if (section.type == SHT_RELA) {
            size_t numRelocs = section.size / sizeof(Elf32_Rela);
            const Elf32_Rela* relocs = reinterpret_cast<const Elf32_Rela*>(section.data.data());

            for (size_t i = 0; i < numRelocs; ++i) {
                const Elf32_Rela& rel = relocs[i];

                Relocation relocation;
                relocation.offset = rel.r_offset;
                relocation.info = rel.r_info;
                relocation.addend = rel.r_addend;
                relocation.symIndex = ELF32_R_SYM(rel.r_info);
                relocation.type = ELF32_R_TYPE(rel.r_info);

                m_relocations.push_back(relocation);
            }
        } else {
            size_t numRelocs = section.size / sizeof(Elf32_Rel);
            const Elf32_Rel* relocs = reinterpret_cast<const Elf32_Rel*>(section.data.data());

            for (size_t i = 0; i < numRelocs; ++i) {
                const Elf32_Rel& rel = relocs[i];

                Relocation relocation;
                relocation.offset = rel.r_offset;
                relocation.info = rel.r_info;
                relocation.addend = 0;
                relocation.symIndex = ELF32_R_SYM(rel.r_info);
                relocation.type = ELF32_R_TYPE(rel.r_info);

                m_relocations.push_back(relocation);
            }
        }
    }

    return true;
}

bool ElfLoader::parseRelocations64() {
    for (const auto& section : m_sections) {
        if (section.type != SHT_REL && section.type != SHT_RELA) {
            continue;
        }

        if (section.type == SHT_RELA) {
            size_t numRelocs = section.size / sizeof(Elf64_Rela);
            const Elf64_Rela* relocs = reinterpret_cast<const Elf64_Rela*>(section.data.data());

            for (size_t i = 0; i < numRelocs; ++i) {
                const Elf64_Rela& rel = relocs[i];

                Relocation relocation;
                relocation.offset = rel.r_offset;
                relocation.info = rel.r_info;
                relocation.addend = rel.r_addend;
                relocation.symIndex = ELF64_R_SYM(rel.r_info);
                relocation.type = ELF64_R_TYPE(rel.r_info);

                m_relocations.push_back(relocation);
            }
        } else {
            size_t numRelocs = section.size / sizeof(Elf64_Rel);
            const Elf64_Rel* relocs = reinterpret_cast<const Elf64_Rel*>(section.data.data());

            for (size_t i = 0; i < numRelocs; ++i) {
                const Elf64_Rel& rel = relocs[i];

                Relocation relocation;
                relocation.offset = rel.r_offset;
                relocation.info = rel.r_info;
                relocation.addend = 0;
                relocation.symIndex = ELF64_R_SYM(rel.r_info);
                relocation.type = ELF64_R_TYPE(rel.r_info);

                m_relocations.push_back(relocation);
            }
        }
    }

    return true;
}

bool ElfLoader::resolveSymbols() {
    return true;
}

bool ElfLoader::relocate() {
    if (m_is64Bit) {
        return relocate64();
    } else {
        return relocate32();
    }
}

bool ElfLoader::relocate32() {
    switch (m_machine) {
        case EM_386:
            return relocateI386();
        default:
            break;
    }

    return true;
}

bool ElfLoader::relocate64() {
    switch (m_machine) {
        case EM_X86_64:
            return relocateX86_64();
        default:
            break;
    }

    return true;
}

bool ElfLoader::relocateI386() {
    for (const auto& reloc : m_relocations) {
        uint8_t type = static_cast<uint8_t>(reloc.type);
        uint32_t offset = static_cast<uint32_t>(reloc.offset);
        uint32_t symIndex = static_cast<uint32_t>(reloc.symIndex);
        int32_t addend = static_cast<int32_t>(reloc.addend);

        uint32_t* target = nullptr;
        for (auto& segment : m_segments) {
            if (segment.type == PT_LOAD && offset >= segment.vaddr && 
                offset < segment.vaddr + segment.memsz) {
                uint32_t segOffset = offset - static_cast<uint32_t>(segment.vaddr);
                if (segOffset + sizeof(uint32_t) <= segment.data.size()) {
                    target = reinterpret_cast<uint32_t*>(segment.data.data() + segOffset);
                }
                break;
            }
        }

        if (!target) {
            continue;
        }

        uint32_t symValue = 0;
        if (symIndex < m_symbols.size()) {
            symValue = static_cast<uint32_t>(m_symbols[symIndex].value);
        }

        switch (type) {
            case R_386_NONE:
                break;
            case R_386_32:
                *target = symValue + addend;
                break;
            case R_386_PC32:
                *target = symValue + addend - offset;
                break;
            case R_386_RELATIVE:
                // TODO: 自定义 OS 需提供加载基址
                *target = addend;
                break;
            default:
                break;
        }
    }

    return true;
}

bool ElfLoader::relocateX86_64() {
    for (const auto& reloc : m_relocations) {
        uint32_t type = reloc.type;
        uint64_t offset = reloc.offset;
        uint64_t symIndex = reloc.symIndex;
        int64_t addend = reloc.addend;

        uint64_t* target = nullptr;
        for (auto& segment : m_segments) {
            if (segment.type == PT_LOAD && offset >= segment.vaddr && 
                offset < segment.vaddr + segment.memsz) {
                uint64_t segOffset = offset - segment.vaddr;
                if (segOffset + sizeof(uint64_t) <= segment.data.size()) {
                    target = reinterpret_cast<uint64_t*>(segment.data.data() + segOffset);
                }
                break;
            }
        }

        if (!target) {
            continue;
        }

        uint64_t symValue = 0;
        if (symIndex < m_symbols.size()) {
            symValue = m_symbols[symIndex].value;
        }

        switch (type) {
            case R_X86_64_NONE:
                break;
            case R_X86_64_64:
                *target = symValue + addend;
                break;
            case R_X86_64_PC32:
                *target = static_cast<uint32_t>(symValue + addend - offset);
                break;
            case R_X86_64_RELATIVE:
                // TODO: 自定义 OS 需提供加载基址
                *target = addend;
                break;
            case R_X86_64_32:
                *target = static_cast<uint32_t>(symValue + addend);
                break;
            case R_X86_64_32S:
                *target = static_cast<int32_t>(symValue + addend);
                break;
            default:
                break;
        }
    }

    return true;
}

uint64_t ElfLoader::allocateMemory(uint64_t size, uint32_t flags) {
#ifdef _WIN32
    DWORD protect = 0;
    if (flags & PF_R) protect |= PAGE_READONLY;
    if (flags & PF_W) protect |= PAGE_READWRITE;
    if (flags & PF_X) protect |= PAGE_EXECUTE;

    void* addr = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, protect);
    return reinterpret_cast<uint64_t>(addr);
#else
    int prot = 0;
    if (flags & PF_R) prot |= PROT_READ;
    if (flags & PF_W) prot |= PROT_WRITE;
    if (flags & PF_X) prot |= PROT_EXEC;

    void* addr = mmap(nullptr, size, prot, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return reinterpret_cast<uint64_t>(addr);
#endif
}

uint64_t ElfLoader::getEntryPoint() const {
    return m_entryPoint;
}

const std::vector<Segment>& ElfLoader::getSegments() const {
    return m_segments;
}

const std::vector<Section>& ElfLoader::getSections() const {
    return m_sections;
}

const std::vector<Symbol>& ElfLoader::getSymbols() const {
    return m_symbols;
}

const std::vector<Relocation>& ElfLoader::getRelocations() const {
    return m_relocations;
}

bool ElfLoader::is32Bit() const {
    return !m_is64Bit;
}

bool ElfLoader::is64Bit() const {
    return m_is64Bit;
}

uint16_t ElfLoader::getMachine() const {
    return m_machine;
}

uint16_t ElfLoader::getType() const {
    return m_type;
}

} // namespace elf
