#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <unordered_map>

#include "ElfHeader.h"

namespace elf {

// ELF 程序段结构
struct Segment {
    uint32_t type;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t filesz;
    uint64_t memsz;
    uint32_t flags;
    uint64_t align;
    std::vector<uint8_t> data;
};

// ELF 节区结构
struct Section {
    std::string name;
    uint32_t type;
    uint64_t flags;
    uint64_t addr;
    uint64_t offset;
    uint64_t size;
    uint32_t link;
    uint32_t info;
    uint64_t align;
    uint64_t entsize;
    std::vector<uint8_t> data;
};

// ELF 符号结构
struct Symbol {
    std::string name;
    uint64_t value;
    uint64_t size;
    uint8_t bind;
    uint8_t type;
    uint16_t shndx;
};

// ELF 重定位结构
struct Relocation {
    uint64_t offset;
    uint64_t info;
    int64_t addend;
    uint32_t symIndex;
    uint32_t type;
};

class ElfLoader {
public:
    ElfLoader();
    ~ElfLoader();

    bool load(const std::string& path);
    bool parseHeader();
    bool loadSegments();
    bool parseSections();
    bool parseSymbols();
    bool parseRelocations();
    bool resolveSymbols();
    bool relocate();

    uint64_t getEntryPoint() const;
    const std::vector<Segment>& getSegments() const;
    const std::vector<Section>& getSections() const;
    const std::vector<Symbol>& getSymbols() const;
    const std::vector<Relocation>& getRelocations() const;

    bool is32Bit() const;
    bool is64Bit() const;
    uint16_t getMachine() const;
    uint16_t getType() const;

private:
    std::string m_path;
    std::vector<uint8_t> m_fileData;
    bool m_is64Bit;
    uint16_t m_machine;
    uint16_t m_type;
    uint64_t m_entryPoint;

    std::vector<Segment> m_segments;
    std::vector<Section> m_sections;
    std::vector<Symbol> m_symbols;
    std::vector<Relocation> m_relocations;
    std::unordered_map<std::string, uint64_t> m_symbolTable;

    const Elf32_Ehdr* m_ehdr32;
    const Elf64_Ehdr* m_ehdr64;

    bool readFile(const std::string& path);
    bool verifyMagic();
    bool parseElfClass();
    bool loadSegment32(const Elf32_Phdr& phdr);
    bool loadSegment64(const Elf64_Phdr& phdr);
    bool parseSections32();
    bool parseSections64();
    bool parseSymbols32();
    bool parseSymbols64();
    bool parseRelocations32();
    bool parseRelocations64();
    bool relocate32();
    bool relocate64();
    bool relocateX86_64();
    bool relocateI386();
    uint64_t allocateMemory(uint64_t size, uint32_t flags);
};

} // namespace elf
