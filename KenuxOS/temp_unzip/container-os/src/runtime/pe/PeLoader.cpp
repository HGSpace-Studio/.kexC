#include "PeLoader.h"
#include <fstream>
#include <iostream>

namespace pe {

PeLoader::PeLoader()
    : m_dosHeader(nullptr)
    , m_fileHeader(nullptr)
    , m_optionalHeader32(nullptr)
    , m_optionalHeader64(nullptr)
    , m_sectionHeaders(nullptr)
    , m_is64Bit(false)
    , m_imageBase(0)
    , m_entryPoint(0)
{
}

PeLoader::~PeLoader()
{
}

bool PeLoader::load(const std::string& path)
{
    // 使用 std::ifstream 读取 PE 文件
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "[PE Loader] 无法打开文件: " << path << std::endl;
        return false;
    }

    // 获取文件大小
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    // 读取文件数据
    m_fileData.resize(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(m_fileData.data()), size)) {
        std::cerr << "[PE Loader] 读取文件失败: " << path << std::endl;
        return false;
    }

    // 解析 DOS 头、PE 头、节区表、导入表和导出表
    if (!parseDosHeader()) return false;
    if (!parsePeHeader()) return false;
    if (!parseSections()) return false;
    if (!parseImportTable()) return false;
    if (!parseExportTable()) return false;

    std::cout << "[PE Loader] 文件加载成功: " << path << std::endl;
    return true;
}

bool PeLoader::parseDosHeader()
{
    // 检查文件大小是否足够容纳 DOS 头
    if (m_fileData.size() < sizeof(IMAGE_DOS_HEADER)) {
        std::cerr << "[PE Loader] 文件太小，无法解析 DOS 头" << std::endl;
        return false;
    }

    // 获取 DOS 头指针
    m_dosHeader = reinterpret_cast<const IMAGE_DOS_HEADER*>(m_fileData.data());

    // 验证 DOS 签名 ('M' 'Z')
    if (m_dosHeader->e_magic != 0x5A4D) {
        std::cerr << "[PE Loader] 无效的 DOS 签名" << std::endl;
        return false;
    }

    std::cout << "[PE Loader] DOS 头解析成功，PE 偏移: 0x" << std::hex << m_dosHeader->e_lfanew << std::dec << std::endl;
    return true;
}

bool PeLoader::parsePeHeader()
{
    if (!m_dosHeader) {
        std::cerr << "[PE Loader] DOS 头未解析" << std::endl;
        return false;
    }

    // 获取 PE 签名位置
    uint32_t peOffset = m_dosHeader->e_lfanew;
    if (peOffset + sizeof(uint32_t) > m_fileData.size()) {
        std::cerr << "[PE Loader] PE 签名位置无效" << std::endl;
        return false;
    }

    // 验证 PE 签名 (0x50 0x45 0x00 0x00)
    uint32_t* peSignature = reinterpret_cast<uint32_t*>(m_fileData.data() + peOffset);
    if (*peSignature != PE_SIGNATURE) {
        std::cerr << "[PE Loader] 无效的 PE 签名" << std::endl;
        return false;
    }

    // 获取 COFF 文件头
    m_fileHeader = reinterpret_cast<const IMAGE_FILE_HEADER*>(m_fileData.data() + peOffset + sizeof(uint32_t));
    if (peOffset + sizeof(uint32_t) + sizeof(IMAGE_FILE_HEADER) > m_fileData.size()) {
        std::cerr << "[PE Loader] COFF 文件头位置无效" << std::endl;
        return false;
    }

    // 获取可选头
    uint32_t optionalHeaderOffset = peOffset + sizeof(uint32_t) + sizeof(IMAGE_FILE_HEADER);
    if (optionalHeaderOffset + 2 > m_fileData.size()) {
        std::cerr << "[PE Loader] 可选头位置无效" << std::endl;
        return false;
    }

    uint16_t magic = *reinterpret_cast<const uint16_t*>(m_fileData.data() + optionalHeaderOffset);

    // 根据 Magic 判断 PE32 还是 PE32+
    if (magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        m_is64Bit = false;
        if (optionalHeaderOffset + sizeof(IMAGE_OPTIONAL_HEADER32) > m_fileData.size()) {
            std::cerr << "[PE Loader] PE32 可选头位置无效" << std::endl;
            return false;
        }
        m_optionalHeader32 = reinterpret_cast<const IMAGE_OPTIONAL_HEADER32*>(
            m_fileData.data() + optionalHeaderOffset);
        m_imageBase = m_optionalHeader32->ImageBase;
        m_entryPoint = m_imageBase + m_optionalHeader32->AddressOfEntryPoint;
    } else if (magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        m_is64Bit = true;
        if (optionalHeaderOffset + sizeof(IMAGE_OPTIONAL_HEADER64) > m_fileData.size()) {
            std::cerr << "[PE Loader] PE32+ 可选头位置无效" << std::endl;
            return false;
        }
        m_optionalHeader64 = reinterpret_cast<const IMAGE_OPTIONAL_HEADER64*>(
            m_fileData.data() + optionalHeaderOffset);
        m_imageBase = m_optionalHeader64->ImageBase;
        m_entryPoint = m_imageBase + m_optionalHeader64->AddressOfEntryPoint;
    } else {
        std::cerr << "[PE Loader] 未知的 PE 类型 Magic: 0x" << std::hex << magic << std::dec << std::endl;
        return false;
    }

    // 获取节区表头
    uint32_t sectionTableOffset = optionalHeaderOffset + m_fileHeader->SizeOfOptionalHeader;
    if (sectionTableOffset + sizeof(IMAGE_SECTION_HEADER) * m_fileHeader->NumberOfSections > m_fileData.size()) {
        std::cerr << "[PE Loader] 节区表位置无效" << std::endl;
        return false;
    }
    m_sectionHeaders = reinterpret_cast<const IMAGE_SECTION_HEADER*>(
        m_fileData.data() + sectionTableOffset);

    std::cout << "[PE Loader] PE 头解析成功，机器类型: 0x" << std::hex << m_fileHeader->Machine
              << ", 位数: " << (m_is64Bit ? "64" : "32") << ", 入口点: 0x" << m_entryPoint << std::dec << std::endl;
    return true;
}

bool PeLoader::parseSections()
{
    if (!m_fileHeader || !m_sectionHeaders) {
        std::cerr << "[PE Loader] PE 头未解析" << std::endl;
        return false;
    }

    m_sections.clear();

    // 遍历所有节区
    for (uint16_t i = 0; i < m_fileHeader->NumberOfSections; ++i) {
        const IMAGE_SECTION_HEADER& sectionHeader = m_sectionHeaders[i];

        Section section;
        section.name = std::string(reinterpret_cast<const char*>(sectionHeader.Name), 8);
        section.name.erase(section.name.find_first_of('\0'));
        section.virtual_size = sectionHeader.VirtualSize;
        section.virtual_address = sectionHeader.VirtualAddress;
        section.raw_size = sectionHeader.SizeOfRawData;
        section.raw_offset = sectionHeader.PointerToRawData;
        section.characteristics = sectionHeader.Characteristics;

        // 读取节区数据
        if (sectionHeader.PointerToRawData + sectionHeader.SizeOfRawData <= m_fileData.size()) {
            section.data.resize(sectionHeader.SizeOfRawData);
            std::copy(m_fileData.begin() + sectionHeader.PointerToRawData,
                      m_fileData.begin() + sectionHeader.PointerToRawData + sectionHeader.SizeOfRawData,
                      section.data.begin());
        }

        m_sections.push_back(section);
    }

    std::cout << "[PE Loader] 节区解析成功，共 " << m_sections.size() << " 个节区" << std::endl;
    return true;
}

bool PeLoader::parseImportTable()
{
    if (!m_fileHeader) {
        std::cerr << "[PE Loader] PE 头未解析" << std::endl;
        return false;
    }

    m_imports.clear();

    // 获取导入表数据目录
    const IMAGE_DATA_DIRECTORY* importDir = nullptr;
    if (m_is64Bit && m_optionalHeader64) {
        importDir = &m_optionalHeader64->DataDirectory[1];
    } else if (!m_is64Bit && m_optionalHeader32) {
        importDir = &m_optionalHeader32->DataDirectory[1];
    }

    if (!importDir || importDir->VirtualAddress == 0 || importDir->Size == 0) {
        std::cout << "[PE Loader] 没有导入表" << std::endl;
        return true;
    }

    // 将 RVA 转换为文件偏移
    uint64_t importTableOffset = rvaToOffset(importDir->VirtualAddress);
    if (importTableOffset == 0) {
        std::cerr << "[PE Loader] 无法将导入表 RVA 转换为文件偏移" << std::endl;
        return false;
    }

    // 遍历导入描述符
    const IMAGE_IMPORT_DESCRIPTOR* importDescriptor =
        reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(m_fileData.data() + importTableOffset);

    while (importDescriptor->Name != 0) {
        ImportEntry entry;

        // 读取 DLL 名称
        uint64_t nameOffset = rvaToOffset(importDescriptor->Name);
        if (nameOffset != 0 && nameOffset < m_fileData.size()) {
            entry.dll_name = readString(nameOffset);
        }

        // 读取导入的函数列表
        uint64_t iatOffset = rvaToOffset(importDescriptor->OriginalFirstThunk);
        if (iatOffset != 0 && iatOffset < m_fileData.size()) {
            const uint32_t* thunk = reinterpret_cast<const uint32_t*>(m_fileData.data() + iatOffset);

            while (*thunk != 0) {
                // 检查是否是按名称导入（最高位为 1 表示按序号导入）
                if ((*thunk & 0x80000000) == 0) {
                    uint64_t importByNameOffset = rvaToOffset(*thunk);
                    if (importByNameOffset != 0 && importByNameOffset < m_fileData.size()) {
                        const IMAGE_IMPORT_BY_NAME* importByName =
                            reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(m_fileData.data() + importByNameOffset);
                        entry.functions.push_back(std::string(reinterpret_cast<const char*>(importByName->Name)));
                    }
                } else {
                    // 按序号导入，序号 = *thunk & 0xFFFF
                    entry.functions.push_back("#" + std::to_string(*thunk & 0xFFFF));
                }

                ++thunk;
            }
        }

        m_imports.push_back(entry);
        ++importDescriptor;
    }

    std::cout << "[PE Loader] 导入表解析成功，共 " << m_imports.size() << " 个 DLL" << std::endl;
    return true;
}

bool PeLoader::parseExportTable()
{
    if (!m_fileHeader) {
        std::cerr << "[PE Loader] PE 头未解析" << std::endl;
        return false;
    }

    m_exports.clear();

    // 获取导出表数据目录
    const IMAGE_DATA_DIRECTORY* exportDir = nullptr;
    if (m_is64Bit && m_optionalHeader64) {
        exportDir = &m_optionalHeader64->DataDirectory[0];
    } else if (!m_is64Bit && m_optionalHeader32) {
        exportDir = &m_optionalHeader32->DataDirectory[0];
    }

    if (!exportDir || exportDir->VirtualAddress == 0 || exportDir->Size == 0) {
        std::cout << "[PE Loader] 没有导出表" << std::endl;
        return true;
    }

    // 将 RVA 转换为文件偏移
    uint64_t exportTableOffset = rvaToOffset(exportDir->VirtualAddress);
    if (exportTableOffset == 0) {
        std::cerr << "[PE Loader] 无法将导出表 RVA 转换为文件偏移" << std::endl;
        return false;
    }

    // 获取导出目录
    const IMAGE_EXPORT_DIRECTORY* exportDirStruct =
        reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(m_fileData.data() + exportTableOffset);

    // 读取导出函数
    uint64_t functionsOffset = rvaToOffset(exportDirStruct->AddressOfFunctions);
    uint64_t namesOffset = rvaToOffset(exportDirStruct->AddressOfNames);
    uint64_t ordinalsOffset = rvaToOffset(exportDirStruct->AddressOfNameOrdinals);

    if (functionsOffset == 0 || namesOffset == 0 || ordinalsOffset == 0) {
        std::cerr << "[PE Loader] 导出表数据无效" << std::endl;
        return false;
    }

    const uint32_t* functions = reinterpret_cast<const uint32_t*>(m_fileData.data() + functionsOffset);
    const uint32_t* names = reinterpret_cast<const uint32_t*>(m_fileData.data() + namesOffset);
    const uint16_t* ordinals = reinterpret_cast<const uint16_t*>(m_fileData.data() + ordinalsOffset);

    for (uint32_t i = 0; i < exportDirStruct->NumberOfNames; ++i) {
        ExportEntry entry;

        uint64_t nameRva = names[i];
        uint64_t nameOffset = rvaToOffset(nameRva);
        if (nameOffset != 0 && nameOffset < m_fileData.size()) {
            entry.name = readString(nameOffset);
        }

        entry.ordinal = ordinals[i] + exportDirStruct->Base;

        uint64_t funcRva = functions[ordinals[i]];
        entry.address = m_imageBase + funcRva;

        m_exports.push_back(entry);
    }

    std::cout << "[PE Loader] 导出表解析成功，共 " << m_exports.size() << " 个导出函数" << std::endl;
    return true;
}

bool PeLoader::loadDll(const std::string& dll_name)
{
    // TODO: 在自定义操作系统上实现 DLL 加载
    // 当前实现仅打印日志，实际需要：
    // 1. 在系统路径中查找 DLL 文件
    // 2. 加载 DLL 到内存
    // 3. 解析 DLL 的导出表
    // 4. 返回 DLL 的基地址和导出函数地址映射

    std::cout << "[PE Loader] 加载 DLL: " << dll_name << " (TODO: 自定义 OS DLL 加载实现)" << std::endl;
    return true;
}

bool PeLoader::resolveImports()
{
    // TODO: 在自定义操作系统上实现导入符号解析
    // 当前实现仅打印日志，实际需要：
    // 1. 遍历所有导入的 DLL
    // 2. 调用 loadDll 加载每个 DLL
    // 3. 在 DLL 的导出表中查找对应的函数
    // 4. 将函数地址写入 IAT（导入地址表）

    std::cout << "[PE Loader] 解析导入符号 (TODO: 自定义 OS 导入解析实现)" << std::endl;

    for (const auto& import : m_imports) {
        std::cout << "  DLL: " << import.dll_name << std::endl;
        for (const auto& func : import.functions) {
            std::cout << "    函数: " << func << std::endl;
        }
    }

    return true;
}

uint64_t PeLoader::getEntryPoint() const
{
    return m_entryPoint;
}

std::vector<Section> PeLoader::getSections() const
{
    return m_sections;
}

std::vector<ImportEntry> PeLoader::getImports() const
{
    return m_imports;
}

std::vector<ExportEntry> PeLoader::getExports() const
{
    return m_exports;
}

bool PeLoader::is32Bit() const
{
    return !m_is64Bit;
}

bool PeLoader::is64Bit() const
{
    return m_is64Bit;
}

uint16_t PeLoader::getMachine() const
{
    return m_fileHeader ? m_fileHeader->Machine : 0;
}

uint16_t PeLoader::getSubsystem() const
{
    if (m_is64Bit && m_optionalHeader64) {
        return m_optionalHeader64->Subsystem;
    } else if (!m_is64Bit && m_optionalHeader32) {
        return m_optionalHeader32->Subsystem;
    }
    return 0;
}

uint64_t PeLoader::getImageBase() const
{
    return m_imageBase;
}

uint64_t PeLoader::rvaToOffset(uint32_t rva) const
{
    if (!m_sectionHeaders || !m_fileHeader) {
        return 0;
    }

    // 遍历节区，找到包含该 RVA 的节区
    for (uint16_t i = 0; i < m_fileHeader->NumberOfSections; ++i) {
        const IMAGE_SECTION_HEADER& section = m_sectionHeaders[i];

        if (rva >= section.VirtualAddress &&
            rva < section.VirtualAddress + section.VirtualSize) {
            // 计算文件偏移 = RVA - 节区虚拟地址 + 节区文件偏移
            return rva - section.VirtualAddress + section.PointerToRawData;
        }
    }

    return 0;
}

const uint8_t* PeLoader::getPtrFromRva(uint32_t rva) const
{
    uint64_t offset = rvaToOffset(rva);
    if (offset == 0 || offset >= m_fileData.size()) {
        return nullptr;
    }
    return m_fileData.data() + offset;
}

std::string PeLoader::readString(uint32_t rva) const
{
    uint64_t offset = rvaToOffset(rva);
    if (offset == 0 || offset >= m_fileData.size()) {
        return "";
    }

    const char* str = reinterpret_cast<const char*>(m_fileData.data() + offset);
    return std::string(str);
}

} // namespace pe
