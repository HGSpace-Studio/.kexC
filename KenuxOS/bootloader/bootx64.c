#include "include/efi.h"
#include "include/fbc.h"

#define NULL 0
#define EFI_FILE_MODE_READ 0x00000001
#define COM1_PORT 0x3F8

static inline void outb(UINT16 port, UINT8 val) {
    __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline UINT8 inb(UINT16 port) {
    UINT8 ret;
    __asm__ __volatile__("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void serial_putc(char c) {
    while ((inb(COM1_PORT + 5) & 0x20) == 0);
    outb(COM1_PORT, (UINT8)c);
}

static void serial_puts(const char *s) {
    while (*s) {
        if (*s == '\n') serial_putc('\r');
        serial_putc(*s++);
    }
}

static void serial_puthex64(UINT64 val) {
    serial_puts("0x");
    for (int i = 60; i >= 0; i -= 4) {
        int nib = (int)((val >> i) & 0xF);
        serial_putc(nib < 10 ? '0' + nib : 'A' + nib - 10);
    }
}

static void xmemset(void *dest, int val, UINTN count) {
    char *d = (char *)dest;
    while (count--) *d++ = val;
}

static void xmemcpy(void *dest, const void *src, UINTN count) {
    char *d = (char *)dest;
    const char *s = (const char *)src;
    while (count--) *d++ = *s++;
}

static void *kmalloc(UINTN size, struct EFI_BOOT_SERVICES *BS) {
    void *buf = NULL;
    BS->AllocatePool(EfiLoaderData, size, &buf);
    return buf;
}

static void kfree(void *buf, struct EFI_BOOT_SERVICES *BS) {
    if (buf) BS->FreePool(buf);
}

static void print(struct EFI_SYSTEM_TABLE *ST, CHAR16 *str) {
    ST->ConOut->OutputString(ST->ConOut, str);
}

static void print_hex64(struct EFI_SYSTEM_TABLE *ST, UINT64 val) {
    CHAR16 buf[17];
    for (int i = 15; i >= 0; i--) {
        UINT8 nib = (UINT8)((val >> (4 * i)) & 0xF);
        buf[15 - i] = nib < 10 ? '0' + nib : 'A' + nib - 10;
    }
    buf[16] = 0;
    print(ST, buf);
}

typedef struct {
    UINT16 e_magic;
    UINT16 e_cblp;
    UINT16 e_cp;
    UINT16 e_crlc;
    UINT16 e_cparhdr;
    UINT16 e_minalloc;
    UINT16 e_maxalloc;
    UINT16 e_ss;
    UINT16 e_sp;
    UINT16 e_csum;
    UINT16 e_ip;
    UINT16 e_cs;
    UINT16 e_lfarlc;
    UINT16 e_ovno;
    UINT16 e_res[4];
    UINT16 e_oemid;
    UINT16 e_oeminfo;
    UINT16 e_res2[10];
    UINT32 e_lfanew;
} IMAGE_DOS_HEADER;

typedef struct {
    UINT16 Machine;
    UINT16 NumberOfSections;
    UINT32 TimeDateStamp;
    UINT32 PointerToSymbolTable;
    UINT32 NumberOfSymbols;
    UINT16 SizeOfOptionalHeader;
    UINT16 Characteristics;
} IMAGE_FILE_HEADER;

typedef struct {
    UINT32 VirtualAddress;
    UINT32 Size;
} IMAGE_DATA_DIRECTORY;

typedef struct {
    UINT16 Magic;
    UINT8 MajorLinkerVersion;
    UINT8 MinorLinkerVersion;
    UINT32 SizeOfCode;
    UINT32 SizeOfInitializedData;
    UINT32 SizeOfUninitializedData;
    UINT32 AddressOfEntryPoint;
    UINT32 BaseOfCode;
    UINT64 ImageBase;
    UINT32 SectionAlignment;
    UINT32 FileAlignment;
    UINT16 MajorOperatingSystemVersion;
    UINT16 MinorOperatingSystemVersion;
    UINT16 MajorImageVersion;
    UINT16 MinorImageVersion;
    UINT16 MajorSubsystemVersion;
    UINT16 MinorSubsystemVersion;
    UINT32 Win32VersionValue;
    UINT32 SizeOfImage;
    UINT32 SizeOfHeaders;
    UINT32 CheckSum;
    UINT16 Subsystem;
    UINT16 DllCharacteristics;
    UINT64 SizeOfStackReserve;
    UINT64 SizeOfStackCommit;
    UINT64 SizeOfHeapReserve;
    UINT64 SizeOfHeapCommit;
    UINT32 LoaderFlags;
    UINT32 NumberOfRvaAndSizes;
    IMAGE_DATA_DIRECTORY DataDirectory[16];
} IMAGE_OPTIONAL_HEADER64;

typedef struct {
    UINT8 Name[8];
    UINT32 VirtualSize;
    UINT32 VirtualAddress;
    UINT32 SizeOfRawData;
    UINT32 PointerToRawData;
    UINT32 PointerToRelocations;
    UINT32 PointerToLinenumbers;
    UINT16 NumberOfRelocations;
    UINT16 NumberOfLinenumbers;
    UINT32 Characteristics;
} IMAGE_SECTION_HEADER;

#define IMAGE_FILE_MACHINE_AMD64 0x8664

static UINT64 load_pe64(void *buf, struct EFI_SYSTEM_TABLE *ST) {
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)buf;
    if (dos->e_magic != 0x5a4d) {
        print(ST, L"Not MZ\n");
        return 0;
    }

    UINT32 pe_off = dos->e_lfanew;
    UINT32 *pe_sig = (UINT32 *)((UINT8 *)buf + pe_off);
    if (*pe_sig != 0x00004550) {
        print(ST, L"PE sig bad\n");
        return 0;
    }

    IMAGE_FILE_HEADER *coff = (IMAGE_FILE_HEADER *)((UINT8 *)buf + pe_off + 4);
    if (coff->Machine != IMAGE_FILE_MACHINE_AMD64) {
        print(ST, L"Not AMD64\n");
        return 0;
    }

    IMAGE_OPTIONAL_HEADER64 *opt = (IMAGE_OPTIONAL_HEADER64 *)((UINT8 *)coff + sizeof(IMAGE_FILE_HEADER));
    if (opt->Magic != 0x20b) {
        print(ST, L"Not PE32+\n");
        return 0;
    }

    UINT64 image_base = opt->ImageBase;
    UINT64 entry = image_base + opt->AddressOfEntryPoint;

    print(ST, L"PE32+ OK, base=0x");
    print_hex64(ST, image_base);
    print(ST, L" entry=0x");
    print_hex64(ST, entry);
    print(ST, L"\n");

    struct EFI_BOOT_SERVICES *BS = ST->BootServices;
    UINTN num_pages = (opt->SizeOfImage + 4095) / 4096;
    EFI_PHYSICAL_ADDRESS alloc_addr = image_base;
    EFI_STATUS alloc_status = BS->AllocatePages(AllocateAddress, EfiLoaderData, num_pages, &alloc_addr);
    if (EFI_ERROR(alloc_status)) {
        print(ST, L"Alloc failed\n");
        return 0;
    }

    xmemset((void *)(UINTN)image_base, 0, (UINTN)opt->SizeOfImage);
    xmemcpy((void *)(UINTN)image_base, buf, (UINTN)opt->SizeOfHeaders);

    IMAGE_SECTION_HEADER *sect = (IMAGE_SECTION_HEADER *)((UINT8 *)opt + coff->SizeOfOptionalHeader);
    for (UINTN i = 0; i < coff->NumberOfSections; i++) {
        if (sect[i].VirtualAddress >= opt->SizeOfImage) {
            continue;
        }
        UINT8 *src = (UINT8 *)buf + sect[i].PointerToRawData;
        UINT8 *dst = (UINT8 *)(UINTN)(image_base + sect[i].VirtualAddress);
        UINTN sz = (UINTN)sect[i].SizeOfRawData;
        if (sz > 0) {
            xmemcpy(dst, src, sz);
        }
    }

    print(ST, L"PE loaded\n");
    return entry;
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, struct EFI_SYSTEM_TABLE *SystemTable) {
    serial_puts("[BOOT] efi_main entry\n");

    struct EFI_BOOT_SERVICES *BS = SystemTable->BootServices;
    struct EFI_GRAPHICS_OUTPUT_PROTOCOL *GOP = NULL;
    struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *sfsp = NULL;
    struct EFI_FILE_PROTOCOL *root = NULL;
    struct EFI_FILE_PROTOCOL *file = NULL;
    EFI_STATUS status;

    print(SystemTable, L"\nKenuxK UEFI Bootloader\n");
    print(SystemTable, L"========================\n");
    serial_puts("[BOOT] KenuxK UEFI Bootloader\n");

    EFI_GUID gop_guid = {0x9042a9de, 0x23dc, 0x4a38, {0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a}};
    print(SystemTable, L"Locating GOP...\n");
    serial_puts("[BOOT] Locating GOP...\n");
    status = BS->LocateProtocol(&gop_guid, NULL, (void **)&GOP);
    if (EFI_ERROR(status)) GOP = NULL;
    serial_puts(GOP ? "[BOOT] GOP found\n" : "[BOOT] GOP not found\n");

    EFI_GUID sfsp_guid = {0x964e5b22, 0x6459, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}};
    print(SystemTable, L"Locating SFSP...\n");
    status = BS->LocateProtocol(&sfsp_guid, NULL, (void **)&sfsp);
    if (EFI_ERROR(status)) {
        print(SystemTable, L"SFSP not found\n");
        while (1);
    }

    print(SystemTable, L"Opening root...\n");
    status = sfsp->OpenVolume(sfsp, &root);
    if (EFI_ERROR(status)) {
        print(SystemTable, L"OpenVolume failed\n");
        while (1);
    }

    print(SystemTable, L"Loading kernel...\n");
    status = root->Open(root, &file, L"KENUXK.BIN", EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status)) {
        print(SystemTable, L"Open KENUXK.BIN failed\n");
        while (1);
    }

    EFI_GUID file_info_guid = {0x09576e92, 0x6d3f, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}};
    UINTN info_size = sizeof(EFI_FILE_INFO);
    EFI_FILE_INFO *info = (EFI_FILE_INFO *)kmalloc(info_size, BS);
    file->GetInfo(file, &file_info_guid, &info_size, (void *)info);
    if (info_size > sizeof(EFI_FILE_INFO)) {
        kfree(info, BS);
        info = (EFI_FILE_INFO *)kmalloc(info_size, BS);
        file->GetInfo(file, &file_info_guid, &info_size, (void *)info);
    }
    UINTN kernel_size = (UINTN)info->FileSize;
    kfree(info, BS);

    print(SystemTable, L"Kernel size: ");
    {
        CHAR16 numbuf[20];
        UINTN n = kernel_size;
        int i = 19;
        numbuf[i--] = 0;
        if (n == 0) numbuf[i--] = '0';
        while (n > 0 && i >= 0) {
            numbuf[i--] = '0' + (n % 10);
            n /= 10;
        }
        print(SystemTable, &numbuf[i + 1]);
        print(SystemTable, L" bytes\n");
    }

    UINTN kernel_pages = (kernel_size + 4095) / 4096;
    EFI_PHYSICAL_ADDRESS load_addr = 0x200000;
    status = BS->AllocatePages(AllocateAddress, EfiLoaderData, kernel_pages, &load_addr);
    if (EFI_ERROR(status)) {
        print(SystemTable, L"AllocatePages at 0x200000 failed\n");
        while (1);
    }

    void *kernel_buf = kmalloc(kernel_size, BS);
    if (!kernel_buf) {
        print(SystemTable, L"Malloc failed\n");
        while (1);
    }
    UINTN read_size = kernel_size;
    file->Read(file, &read_size, kernel_buf);
    file->Close(file);

    xmemcpy((void *)(UINTN)0x200000, kernel_buf, kernel_size);
    kfree(kernel_buf, BS);

    print(SystemTable, L"Kernel loaded at 0x200000\n");
    UINT64 entry = 0x200000;

    if (GOP) {
        print(SystemTable, L"Setting 1024x768...\n");
        for (UINT32 i = 0; i < GOP->Mode->MaxMode; i++) {
            UINTN sz = 0;
            EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info = NULL;
            if (GOP->QueryMode(GOP, i, &sz, &info) == EFI_SUCCESS) {
                if (info->HorizontalResolution == 1024 && info->VerticalResolution == 768) {
                    GOP->SetMode(GOP, i);
                    break;
                }
            }
        }
    }

    struct FrameBufferConfig fbc;
    xmemset(&fbc, 0, sizeof(fbc));
    if (GOP) {
        fbc.frame_buffer = (UINT8 *)GOP->Mode->FrameBufferBase;
        fbc.pixels_per_scan_line = GOP->Mode->Info->PixelsPerScanLine;
        fbc.horizontal_resolution = GOP->Mode->Info->HorizontalResolution;
        fbc.vertical_resolution = GOP->Mode->Info->VerticalResolution;
        if (GOP->Mode->Info->PixelFormat == PixelBlueGreenRedReserved8BitPerColor) {
            fbc.pixel_format = kPixelBGRR;
        } else {
            fbc.pixel_format = kPixelRGBR;
        }
    }

    UINTN map_size = 0;
    EFI_MEMORY_DESCRIPTOR *map_buf = NULL;
    UINTN map_key = 0;
    UINTN desc_size = 0;
    UINT32 desc_ver = 0;
    BS->GetMemoryMap(&map_size, NULL, &map_key, &desc_size, &desc_ver);
    map_size += 4096 * 4;
    UINTN map_buf_size = map_size;
    map_buf = (EFI_MEMORY_DESCRIPTOR *)kmalloc(map_buf_size, BS);

    print(SystemTable, L"Exiting boot services...\n");
    serial_puts("[BOOT] Exiting boot services...\n");
    do {
        map_size = map_buf_size;
        BS->GetMemoryMap(&map_size, map_buf, &map_key, &desc_size, &desc_ver);
        status = BS->ExitBootServices(ImageHandle, map_key);
    } while (EFI_ERROR(status));

    serial_puts("[BOOT] Boot services exited, jumping to kernel...\n");

    struct MemoryMapInfo mmi;
    mmi.buffer_size = map_buf_size;
    mmi.map_size = map_size;
    mmi.descriptor_size = desc_size;
    mmi.descriptor_version = desc_ver;
    mmi.buffer = map_buf;

    typedef void (*KernelEntry)(const struct FrameBufferConfig *, const struct MemoryMapInfo *);
    KernelEntry kernel = (KernelEntry)entry;
    kernel(&fbc, &mmi);

    while (1);
    return EFI_SUCCESS;
}
