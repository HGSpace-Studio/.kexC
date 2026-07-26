#pragma once

#include "PeHeader.h"
#include <string>
#include <vector>
#include <memory>

namespace pe {

struct Section {
    std::string name;
    uint32_t virtual_size;
    uint32_t virtual_address;
    uint32_t raw_size;
    uint32_t raw_offset;
    uint32_t characteristics;
    std::vector<uint8_t> data;
};

struct ImportEntry {
    std::string dll_name;
    std::vector<std::string> functions;
};

struct ExportEntry {
    std::string name;
    uint32_t ordinal;
    uint64_t address;
};

class PeLoader {
public:
    PeLoader();
    ~PeLoader();

    bool load(const std::string& path);
    bool parseDosHeader();
    bool parsePeHeader();
    bool parseSections();
    bool parseImportTable();
    bool parseExportTable();
    bool loadDll(const std::string& dll_name);
    bool resolveImports();

    uint64_t getEntryPoint() const;
    std::vector<Section> getSections() const;
    std::vector<ImportEntry> getImports() const;
    std::vector<ExportEntry> getExports() const;
    bool is32Bit() const;
    bool is64Bit() const;
    uint16_t getMachine() const;
    uint16_t getSubsystem() const;
    uint64_t getImageBase() const;

private:
    std::vector<uint8_t> m_fileData;
    const IMAGE_DOS_HEADER* m_dosHeader;
    const IMAGE_FILE_HEADER* m_fileHeader;
    const IMAGE_OPTIONAL_HEADER32* m_optionalHeader32;
    const IMAGE_OPTIONAL_HEADER64* m_optionalHeader64;
    const IMAGE_SECTION_HEADER* m_sectionHeaders;

    std::vector<Section> m_sections;
    std::vector<ImportEntry> m_imports;
    std::vector<ExportEntry> m_exports;

    bool m_is64Bit;
    uint64_t m_imageBase;
    uint64_t m_entryPoint;

    uint64_t rvaToOffset(uint32_t rva) const;
    const uint8_t* getPtrFromRva(uint32_t rva) const;
    std::string readString(uint32_t rva) const;
};

} // namespace pe
