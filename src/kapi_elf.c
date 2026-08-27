#include "kapi_elf.h"
#include "kapi.h"
#include <string.h>

static kapi_elf_image_t elf_images[64];
static int elf_image_count = 0;
static kapi_elf_link_map_t global_link_map;
static char dlerror_buf[256];

int kapi_elf_init(void)
{
    memset(elf_images, 0, sizeof(elf_images));
    memset(&global_link_map, 0, sizeof(global_link_map));
    elf_image_count = 0;
    dlerror_buf[0] = '\0';
    return KAPI_OK;
}

int kapi_elf_validate(const kapi_elf64_ehdr_t* ehdr)
{
    if (!ehdr) return KAPI_EINVAL;
    if (ehdr->e_ident[KAPI_EI_MAG0] != KAPI_ELFMAG0) return KAPI_ERROR;
    if (ehdr->e_ident[KAPI_EI_MAG1] != KAPI_ELFMAG1) return KAPI_ERROR;
    if (ehdr->e_ident[KAPI_EI_MAG2] != KAPI_ELFMAG2) return KAPI_ERROR;
    if (ehdr->e_ident[KAPI_EI_MAG3] != KAPI_ELFMAG3) return KAPI_ERROR;
    if (ehdr->e_ident[KAPI_EI_CLASS] != KAPI_ELFCLASS64) return KAPI_ERROR;
    if (ehdr->e_ident[KAPI_EI_DATA] != KAPI_ELFDATA2LSB) return KAPI_ERROR;
    if (ehdr->e_machine != KAPI_EM_X86_64) return KAPI_ERROR;
    if (ehdr->e_type != KAPI_ET_EXEC && ehdr->e_type != KAPI_ET_DYN) return KAPI_ERROR;
    return KAPI_OK;
}

static kapi_elf_image_t* elf_image_alloc(void)
{
    for (int i = 0; i < 64; i++) {
        if (elf_images[i].fd == 0 && elf_images[i].mapped_base == NULL) {
            memset(&elf_images[i], 0, sizeof(kapi_elf_image_t));
            elf_images[i].fd = -1;
            return &elf_images[i];
        }
    }
    return NULL;
}

kapi_elf_image_t* kapi_elf_load(const char* path)
{
    if (!path) return NULL;

    int fd = kapi_open(path, 0);
    if (fd < 0) {
        snprintf(dlerror_buf, sizeof(dlerror_buf), "cannot open %s", path);
        return NULL;
    }

    kapi_elf64_ehdr_t ehdr;
    ssize_t n = kapi_read(fd, &ehdr, sizeof(ehdr));
    if (n != (ssize_t)sizeof(ehdr)) {
        kapi_close(fd);
        snprintf(dlerror_buf, sizeof(dlerror_buf), "cannot read ELF header from %s", path);
        return NULL;
    }

    if (kapi_elf_validate(&ehdr) != KAPI_OK) {
        kapi_close(fd);
        snprintf(dlerror_buf, sizeof(dlerror_buf), "invalid ELF file %s", path);
        return NULL;
    }

    kapi_elf_image_t* image = elf_image_alloc();
    if (!image) {
        kapi_close(fd);
        return NULL;
    }

    image->ehdr = ehdr;
    image->fd = fd;
    strncpy(image->pathname, path, sizeof(image->pathname) - 1);
    image->is_dynamic = (ehdr.e_type == KAPI_ET_DYN) ? 1 : 0;

    if (kapi_elf_load_phdrs(image) != KAPI_OK) {
        kapi_elf_unload(image);
        return NULL;
    }

    if (kapi_elf_load_shdrs(image) != KAPI_OK) {
        kapi_elf_unload(image);
        return NULL;
    }

    if (kapi_elf_load_segments(image) != KAPI_OK) {
        kapi_elf_unload(image);
        return NULL;
    }

    if (kapi_elf_read_interp(image) != KAPI_OK && image->is_interpreted) {
        kapi_elf_unload(image);
        return NULL;
    }

    if (image->is_dynamic) {
        kapi_elf_load_dynamic(image);
    }

    image->entry_addr = image->ehdr.e_entry;
    if (image->is_dynamic && image->base_addr != 0) {
        image->entry_addr += image->base_addr;
    }

    return image;
}

kapi_elf_image_t* kapi_elf_load_from_fd(int fd)
{
    if (fd < 0) return NULL;

    kapi_elf64_ehdr_t ehdr;
    ssize_t n = kapi_pread(fd, &ehdr, sizeof(ehdr), 0);
    if (n != (ssize_t)sizeof(ehdr)) return NULL;
    if (kapi_elf_validate(&ehdr) != KAPI_OK) return NULL;

    kapi_elf_image_t* image = elf_image_alloc();
    if (!image) return NULL;

    image->ehdr = ehdr;
    image->fd = fd;
    image->is_dynamic = (ehdr.e_type == KAPI_ET_DYN) ? 1 : 0;

    kapi_elf_load_phdrs(image);
    kapi_elf_load_shdrs(image);
    kapi_elf_load_segments(image);

    if (image->is_dynamic) {
        kapi_elf_load_dynamic(image);
    }

    image->entry_addr = image->ehdr.e_entry;
    if (image->is_dynamic && image->base_addr != 0) {
        image->entry_addr += image->base_addr;
    }

    return image;
}

kapi_elf_image_t* kapi_elf_load_from_memory(const void* data, size_t size)
{
    if (!data || size < sizeof(kapi_elf64_ehdr_t)) return NULL;

    const kapi_elf64_ehdr_t* ehdr = (const kapi_elf64_ehdr_t*)data;
    if (kapi_elf_validate(ehdr) != KAPI_OK) return NULL;

    kapi_elf_image_t* image = elf_image_alloc();
    if (!image) return NULL;

    image->ehdr = *ehdr;
    image->fd = -1;
    image->mapped_base = (void*)data;
    image->mapped_size = size;
    image->is_dynamic = (ehdr->e_type == KAPI_ET_DYN) ? 1 : 0;

    kapi_elf_load_phdrs(image);
    kapi_elf_load_shdrs(image);

    return image;
}

int kapi_elf_load_phdrs(kapi_elf_image_t* image)
{
    if (!image || image->ehdr.e_phnum == 0) return KAPI_EINVAL;

    image->phnum = image->ehdr.e_phnum;
    size_t sz = (size_t)image->phnum * sizeof(kapi_elf64_phdr_t);
    image->phdrs = (kapi_elf64_phdr_t*)kapi_malloc(sz);
    if (!image->phdrs) return KAPI_ENOMEM;

    if (image->fd >= 0) {
        ssize_t n = kapi_pread(image->fd, image->phdrs, sz, image->ehdr.e_phoff);
        if (n != (ssize_t)sz) {
            kapi_free(image->phdrs);
            image->phdrs = NULL;
            return KAPI_ERROR;
        }
    } else if (image->mapped_base) {
        memcpy(image->phdrs, (uint8_t*)image->mapped_base + image->ehdr.e_phoff, sz);
    }

    return KAPI_OK;
}

int kapi_elf_load_shdrs(kapi_elf_image_t* image)
{
    if (!image || image->ehdr.e_shnum == 0) return KAPI_OK;

    image->shnum = image->ehdr.e_shnum;
    size_t sz = (size_t)image->shnum * sizeof(kapi_elf64_shdr_t);
    image->shdrs = (kapi_elf64_shdr_t*)kapi_malloc(sz);
    if (!image->shdrs) return KAPI_ENOMEM;

    if (image->fd >= 0) {
        ssize_t n = kapi_pread(image->fd, image->shdrs, sz, image->ehdr.e_shoff);
        if (n != (ssize_t)sz) {
            kapi_free(image->shdrs);
            image->shdrs = NULL;
            return KAPI_ERROR;
        }
    } else if (image->mapped_base) {
        memcpy(image->shdrs, (uint8_t*)image->mapped_base + image->ehdr.e_shoff, sz);
    }

    return KAPI_OK;
}

int kapi_elf_load_segments(kapi_elf_image_t* image)
{
    if (!image || !image->phdrs) return KAPI_EINVAL;

    kapi_elf_addr_t base = 0;
    kapi_elf_addr_t min_addr = UINT64_MAX;
    kapi_elf_addr_t max_addr = 0;

    for (int i = 0; i < image->phnum; i++) {
        kapi_elf64_phdr_t* ph = &image->phdrs[i];
        if (ph->p_type != 1) continue;

        kapi_elf_addr_t seg_start = ph->p_vaddr;
        kapi_elf_addr_t seg_end = ph->p_vaddr + ph->p_memsz;
        kapi_elf_addr_t page_start = seg_start & ~(KAPI_PAGE_SIZE - 1);
        kapi_elf_addr_t page_end = (seg_end + KAPI_PAGE_SIZE - 1) & ~(KAPI_PAGE_SIZE - 1);

        if (page_start < min_addr) min_addr = page_start;
        if (page_end > max_addr) max_addr = page_end;
    }

    if (min_addr == UINT64_MAX) return KAPI_ERROR;

    if (image->is_dynamic) {
        size_t total_size = (size_t)(max_addr - min_addr);
        void* mapped = kapi_malloc(total_size);
        if (!mapped) return KAPI_ENOMEM;
        base = (kapi_elf_addr_t)(uintptr_t)mapped;
        image->mapped_base = mapped;
        image->mapped_size = total_size;
        image->base_addr = base - min_addr;
    } else {
        image->base_addr = 0;
    }

    for (int i = 0; i < image->phnum; i++) {
        kapi_elf64_phdr_t* ph = &image->phdrs[i];
        if (ph->p_type != 1) continue;

        kapi_elf_addr_t vaddr = image->base_addr + ph->p_vaddr;
        void* dest = (void*)(uintptr_t)vaddr;

        if (ph->p_filesz > 0) {
            if (image->fd >= 0) {
                kapi_pread(image->fd, dest, (size_t)ph->p_filesz, ph->p_offset);
            } else if (image->mapped_base) {
                memcpy(dest, (uint8_t*)image->mapped_base + ph->p_offset, (size_t)ph->p_filesz);
            }
        }

        if (ph->p_memsz > ph->p_filesz) {
            memset((uint8_t*)dest + ph->p_filesz, 0, (size_t)(ph->p_memsz - ph->p_filesz));
        }

        int prot = 0;
        if (ph->p_flags & KAPI_PF_R) prot |= KAPI_PROT_READ;
        if (ph->p_flags & KAPI_PF_W) prot |= KAPI_PROT_WRITE;
        if (ph->p_flags & KAPI_PF_X) prot |= KAPI_PROT_EXEC;
        kapi_mprotect(vaddr, (size_t)ph->p_memsz, prot);
    }

    return KAPI_OK;
}

int kapi_elf_load_dynamic(kapi_elf_image_t* image)
{
    if (!image || !image->phdrs) return KAPI_EINVAL;

    for (int i = 0; i < image->phnum; i++) {
        if (image->phdrs[i].p_type != 2) continue;

        kapi_elf_addr_t dyn_vaddr = image->base_addr + image->phdrs[i].p_vaddr;
        size_t dyn_size = (size_t)image->phdrs[i].p_filesz;
        image->dynamic = (kapi_elf64_dyn_t*)(uintptr_t)dyn_vaddr;

        kapi_elf_xword_t strtab_off = 0;
        kapi_elf_xword_t symtab_off = 0;
        kapi_elf_xword_t rela_off = 0;
        kapi_elf_xword_t jmprel_off = 0;
        kapi_elf_xword_t relasz = 0;
        kapi_elf_xword_t jmprelsz = 0;
        kapi_elf_xword_t syment = sizeof(kapi_elf64_sym_t);
        kapi_elf_xword_t strsz = 0;

        kapi_elf64_dyn_t* d = image->dynamic;
        while (d->d_tag != KAPI_DT_NULL) {
            switch (d->d_tag) {
            case KAPI_DT_STRTAB:   strtab_off = d->d_un.d_val; break;
            case KAPI_DT_SYMTAB:   symtab_off = d->d_un.d_val; break;
            case KAPI_DT_STRSZ:    strsz = d->d_un.d_val; break;
            case KAPI_DT_SYMENT:   syment = d->d_un.d_val; break;
            case KAPI_DT_RELA:     rela_off = d->d_un.d_val; break;
            case KAPI_DT_RELASZ:   relasz = d->d_un.d_val; break;
            case KAPI_DT_JMPREL:   jmprel_off = d->d_un.d_val; break;
            case KAPI_DT_PLTRELSZ: jmprelsz = d->d_un.d_val; break;
            default: break;
            }
            d++;
        }

        if (strtab_off) {
            image->dynstr = (char*)(uintptr_t)(image->base_addr + strtab_off);
        }
        if (symtab_off) {
            image->dynsym = (kapi_elf64_sym_t*)(uintptr_t)(image->base_addr + symtab_off);
        }
        if (rela_off) {
            image->rela = (kapi_elf64_rela_t*)(uintptr_t)(image->base_addr + rela_off);
            image->relacount = (kapi_elf_word_t)(relasz / sizeof(kapi_elf64_rela_t));
        }
        if (jmprel_off) {
            image->jmprel = (kapi_elf64_rela_t*)(uintptr_t)(image->base_addr + jmprel_off);
            image->jmprelcount = (kapi_elf_word_t)(jmprelsz / sizeof(kapi_elf64_rela_t));
        }

        (void)dyn_size;
        break;
    }

    return KAPI_OK;
}

int kapi_elf_apply_relocations(kapi_elf_image_t* image)
{
    if (!image || !image->rela) return KAPI_EINVAL;

    for (uint32_t i = 0; i < image->relacount; i++) {
        kapi_elf64_rela_t* r = &image->rela[i];
        uint32_t type = (uint32_t)(r->r_info & 0xFFFFFFFF);
        uint32_t sym_idx = (uint32_t)(r->r_info >> 32);
        kapi_elf_addr_t* target = (kapi_elf_addr_t*)(uintptr_t)(image->base_addr + r->r_offset);
        kapi_elf_sxword_t addend = (kapi_elf_sxword_t)r->r_info;

        switch (type) {
        case KAPI_R_X86_64_64: {
            kapi_elf64_sym_t* sym = &image->dynsym[sym_idx];
            kapi_elf_addr_t sym_val = image->base_addr + sym->st_value;
            *target = sym_val + (kapi_elf_addr_t)addend;
            break;
        }
        case KAPI_R_X86_64_RELATIVE:
            *target = image->base_addr + (kapi_elf_addr_t)addend;
            break;
        case KAPI_R_X86_64_GLOB_DAT:
        case KAPI_R_X86_64_JUMP_SLOT: {
            kapi_elf64_sym_t* sym = &image->dynsym[sym_idx];
            const char* name = image->dynstr + sym->st_name;
            void* addr = kapi_elf_lookup_symbol(image, name);
            if (addr) {
                *target = (kapi_elf_addr_t)(uintptr_t)addr;
            } else if (sym->st_value) {
                *target = image->base_addr + sym->st_value;
            }
            break;
        }
        case KAPI_R_X86_64_PC32: {
            kapi_elf64_sym_t* sym = &image->dynsym[sym_idx];
            kapi_elf_addr_t sym_val = image->base_addr + sym->st_value;
            int32_t* ptarget32 = (int32_t*)target;
            *ptarget32 = (int32_t)(sym_val + addend - (kapi_elf_addr_t)(uintptr_t)target);
            break;
        }
        default:
            break;
        }
    }

    return KAPI_OK;
}

int kapi_elf_resolve_symbol(kapi_elf_image_t* image, const char* name, void** addr)
{
    if (!image || !name || !addr) return KAPI_EINVAL;

    if (image->dynsym && image->dynstr) {
        for (uint32_t i = 0; i < image->dynsymcount; i++) {
            kapi_elf64_sym_t* sym = &image->dynsym[i];
            if (sym->st_name == 0 || sym->st_value == 0) continue;
            const char* sname = image->dynstr + sym->st_name;
            if (strcmp(sname, name) == 0) {
                *addr = (void*)(uintptr_t)(image->base_addr + sym->st_value);
                return KAPI_OK;
            }
        }
    }

    if (image->symtab && image->strtab) {
        for (uint32_t i = 0; i < image->symcount; i++) {
            kapi_elf64_sym_t* sym = &image->symtab[i];
            if (sym->st_name == 0 || sym->st_value == 0) continue;
            const char* sname = image->strtab + sym->st_name;
            if (strcmp(sname, name) == 0) {
                *addr = (void*)(uintptr_t)(image->base_addr + sym->st_value);
                return KAPI_OK;
            }
        }
    }

    return KAPI_ENOENT;
}

void* kapi_elf_lookup_symbol(kapi_elf_image_t* image, const char* name)
{
    void* addr = NULL;

    if (kapi_elf_resolve_symbol(image, name, &addr) == KAPI_OK) {
        return addr;
    }

    for (int i = 0; i < global_link_map.count; i++) {
        kapi_elf_shared_lib_t* lib = &global_link_map.libs[i];
        if (!lib->symtab || !lib->strtab) continue;
        for (uint32_t j = 0; j < lib->symcount; j++) {
            if (lib->symtab[j].st_name == 0 || lib->symtab[j].st_value == 0) continue;
            const char* sname = lib->strtab + lib->symtab[j].st_name;
            if (strcmp(sname, name) == 0) {
                return (void*)((uint8_t*)lib->base + lib->symtab[j].st_value);
            }
        }
    }

    return NULL;
}

int kapi_elf_resolve_got(kapi_elf_image_t* image)
{
    if (!image || !image->dynamic) return KAPI_EINVAL;
    return kapi_elf_apply_relocations(image);
}

int kapi_elf_resolve_plt(kapi_elf_image_t* image)
{
    if (!image || !image->jmprel) return KAPI_OK;

    for (uint32_t i = 0; i < image->jmprelcount; i++) {
        kapi_elf64_rela_t* r = &image->jmprel[i];
        uint32_t type = (uint32_t)(r->r_info & 0xFFFFFFFF);
        if (type != KAPI_R_X86_64_JUMP_SLOT) continue;

        uint32_t sym_idx = (uint32_t)(r->r_info >> 32);
        kapi_elf64_sym_t* sym = &image->dynsym[sym_idx];
        const char* name = image->dynstr + sym->st_name;
        kapi_elf_addr_t* target = (kapi_elf_addr_t*)(uintptr_t)(image->base_addr + r->r_offset);

        void* addr = kapi_elf_lookup_symbol(image, name);
        if (addr) {
            *target = (kapi_elf_addr_t)(uintptr_t)addr;
        }
    }

    return KAPI_OK;
}

int kapi_elf_load_shared_deps(kapi_elf_image_t* image, kapi_elf_link_map_t* link_map)
{
    if (!image || !image->dynamic || !link_map) return KAPI_EINVAL;

    kapi_elf64_dyn_t* d = image->dynamic;
    while (d->d_tag != KAPI_DT_NULL) {
        if (d->d_tag == KAPI_DT_NEEDED) {
            const char* name = image->dynstr + d->d_un.d_val;
            if (link_map->count < KAPI_ELF_MAX_SHARED) {
                kapi_elf_shared_lib_t* lib = &link_map->libs[link_map->count++];
                strncpy(lib->name, name, sizeof(lib->name) - 1);
                lib->version = 0;
            }
        }
        d++;
    }

    return KAPI_OK;
}

kapi_elf_load_info_t kapi_elf_map_binary(kapi_elf_image_t* image)
{
    kapi_elf_load_info_t info;
    memset(&info, 0, sizeof(info));

    if (!image) return info;

    info.base = image->base_addr;
    info.bias = image->is_dynamic ? image->base_addr : 0;
    info.entry = image->entry_addr;
    info.phdr = image->base_addr + image->ehdr.e_phoff;
    info.phnum = image->ehdr.e_phnum;
    info.phent = image->ehdr.e_phentsize;
    info.pagesz = KAPI_PAGE_SIZE;

    return info;
}

int kapi_elf_run_init(kapi_elf_image_t* image)
{
    if (!image || !image->dynamic) return KAPI_OK;

    kapi_elf64_dyn_t* d = image->dynamic;
    kapi_elf_addr_t init_addr = 0;
    kapi_elf_addr_t init_array_addr = 0;
    kapi_elf_xword_t init_array_sz = 0;

    while (d->d_tag != KAPI_DT_NULL) {
        switch (d->d_tag) {
        case KAPI_DT_INIT:
            init_addr = image->base_addr + d->d_un.d_ptr;
            break;
        case KAPI_DT_INIT_ARRAY:
            init_array_addr = image->base_addr + d->d_un.d_ptr;
            break;
        case KAPI_DT_INIT_ARRAYSZ:
            init_array_sz = d->d_un.d_val;
            break;
        default: break;
        }
        d++;
    }

    if (init_addr) {
        void (*init_fn)(void) = (void(*)(void))(uintptr_t)init_addr;
        init_fn();
    }

    if (init_array_addr && init_array_sz) {
        kapi_elf_addr_t* arr = (kapi_elf_addr_t*)(uintptr_t)init_array_addr;
        size_t count = init_array_sz / sizeof(kapi_elf_addr_t);
        for (size_t i = 0; i < count; i++) {
            if (arr[i]) {
                void (*fn)(void) = (void(*)(void))(uintptr_t)arr[i];
                fn();
            }
        }
    }

    return KAPI_OK;
}

int kapi_elf_run_fini(kapi_elf_image_t* image)
{
    if (!image || !image->dynamic) return KAPI_OK;

    kapi_elf64_dyn_t* d = image->dynamic;
    kapi_elf_addr_t fini_array_addr = 0;
    kapi_elf_xword_t fini_array_sz = 0;

    while (d->d_tag != KAPI_DT_NULL) {
        switch (d->d_tag) {
        case KAPI_DT_FINI_ARRAY:
            fini_array_addr = image->base_addr + d->d_un.d_ptr;
            break;
        case KAPI_DT_FINI_ARRAYSZ:
            fini_array_sz = d->d_un.d_val;
            break;
        default: break;
        }
        d++;
    }

    if (fini_array_addr && fini_array_sz) {
        kapi_elf_addr_t* arr = (kapi_elf_addr_t*)(uintptr_t)fini_array_addr;
        size_t count = fini_array_sz / sizeof(kapi_elf_addr_t);
        for (size_t i = count; i > 0; i--) {
            if (arr[i - 1]) {
                void (*fn)(void) = (void(*)(void))(uintptr_t)arr[i - 1];
                fn();
            }
        }
    }

    return KAPI_OK;
}

int kapi_elf_setup_auxv(kapi_elf_load_info_t* info, int argc, char** argv, char** envp)
{
    if (!info) return KAPI_EINVAL;
    (void)argc; (void)argv; (void)envp;

    int idx = 0;
    info->auxv[idx].a_val = KAPI_AT_ENTRY; info->auxv[idx++].a_val = (long)info->entry;
    info->auxv[idx].a_val = KAPI_AT_PHDR;  info->auxv[idx++].a_val = (long)info->phdr;
    info->auxv[idx].a_val = KAPI_AT_PHNUM; info->auxv[idx++].a_val = (long)info->phnum;
    info->auxv[idx].a_val = KAPI_AT_PHENT; info->auxv[idx++].a_val = (long)info->phent;
    info->auxv[idx].a_val = KAPI_AT_PAGESZ; info->auxv[idx++].a_val = (long)info->pagesz;
    info->auxv[idx].a_val = KAPI_AT_BASE;  info->auxv[idx++].a_val = (long)info->interp_base;
    info->auxv[idx].a_val = KAPI_AT_UID;   info->auxv[idx++].a_val = 0;
    info->auxv[idx].a_val = KAPI_AT_EUID;  info->auxv[idx++].a_val = 0;
    info->auxv[idx].a_val = KAPI_AT_GID;   info->auxv[idx++].a_val = 0;
    info->auxv[idx].a_val = KAPI_AT_EGID;  info->auxv[idx++].a_val = 0;
    info->auxv[idx].a_val = KAPI_AT_CLKTCK; info->auxv[idx++].a_val = 100;
    info->auxv[idx].a_val = KAPI_AT_NULL;  info->auxv[idx++].a_val = 0;
    info->auxv_count = idx;

    return KAPI_OK;
}

int kapi_elf_read_interp(kapi_elf_image_t* image)
{
    if (!image || !image->phdrs) return KAPI_OK;

    for (int i = 0; i < image->phnum; i++) {
        if (image->phdrs[i].p_type != 3) continue;

        kapi_elf_addr_t interp_vaddr = image->phdrs[i].p_vaddr;
        size_t interp_len = (size_t)image->phdrs[i].p_filesz;
        if (interp_len >= sizeof(image->interp_path)) interp_len = sizeof(image->interp_path) - 1;

        const char* interp_str = (const char*)(uintptr_t)(image->base_addr + interp_vaddr);
        memcpy(image->interp_path, interp_str, interp_len);
        image->interp_path[interp_len] = '\0';
        image->is_interpreted = 1;
        image->interp_addr = interp_vaddr;
        break;
    }

    return KAPI_OK;
}

int kapi_elf_load_interp(kapi_elf_image_t* image)
{
    if (!image || !image->is_interpreted) return KAPI_OK;
    return KAPI_OK;
}

int kapi_elf_unload(kapi_elf_image_t* image)
{
    if (!image) return KAPI_EINVAL;

    if (image->phdrs) { kapi_free(image->phdrs); image->phdrs = NULL; }
    if (image->shdrs) { kapi_free(image->shdrs); image->shdrs = NULL; }
    if (image->mapped_base && image->is_dynamic) { kapi_free(image->mapped_base); image->mapped_base = NULL; }
    if (image->fd >= 0) { kapi_close(image->fd); image->fd = -1; }

    image->symtab = NULL;
    image->dynsym = NULL;
    image->strtab = NULL;
    image->dynstr = NULL;
    image->dynamic = NULL;
    image->rela = NULL;
    image->jmprel = NULL;

    return KAPI_OK;
}

int kapi_elf_dlopen(const char* path, int mode)
{
    (void)mode;
    if (!path) return -1;

    kapi_elf_image_t* image = kapi_elf_load(path);
    if (!image) return -1;

    if (image->is_dynamic) {
        kapi_elf_apply_relocations(image);
        kapi_elf_resolve_plt(image);
        kapi_elf_run_init(image);
    }

    for (int i = 0; i < 64; i++) {
        if (&elf_images[i] == image) return i + 1;
    }

    return -1;
}

int kapi_elf_dlclose(int handle)
{
    if (handle < 1 || handle > 64) return KAPI_EINVAL;
    kapi_elf_image_t* image = &elf_images[handle - 1];
    return kapi_elf_unload(image);
}

void* kapi_elf_dlsym(int handle, const char* symbol)
{
    if (!symbol) return NULL;

    if (handle == 0) {
        for (int i = 0; i < 64; i++) {
            if (elf_images[i].fd != 0 || elf_images[i].mapped_base != NULL) {
                void* addr = kapi_elf_lookup_symbol(&elf_images[i], symbol);
                if (addr) return addr;
            }
        }
        return NULL;
    }

    if (handle < 1 || handle > 64) return NULL;
    kapi_elf_image_t* image = &elf_images[handle - 1];
    return kapi_elf_lookup_symbol(image, symbol);
}

int kapi_elf_dlinfo(int handle, int request, void* info)
{
    (void)handle; (void)request; (void)info;
    return KAPI_OK;
}

char* kapi_elf_dlerror(void)
{
    return dlerror_buf;
}

int kapi_elf_get_load_info(kapi_elf_image_t* image, kapi_elf_load_info_t* info)
{
    if (!image || !info) return KAPI_EINVAL;
    *info = kapi_elf_map_binary(image);
    return KAPI_OK;
}

int kapi_elf_build_id(kapi_elf_image_t* image, uint8_t* out, size_t* out_len)
{
    if (!image || !out || !out_len) return KAPI_EINVAL;

    for (int i = 0; i < image->shnum; i++) {
        if (image->shdrs[i].sh_type != KAPI_SHT_NOTE) continue;

        kapi_elf_off_t offset = image->shdrs[i].sh_offset;
        size_t size = (size_t)image->shdrs[i].sh_size;

        uint8_t* notes = (uint8_t*)kapi_malloc(size);
        if (!notes) return KAPI_ENOMEM;

        if (image->fd >= 0) {
            kapi_pread(image->fd, notes, size, offset);
        } else if (image->mapped_base) {
            memcpy(notes, (uint8_t*)image->mapped_base + offset, size);
        }

        size_t pos = 0;
        while (pos + 12 <= size) {
            uint32_t namesz = *(uint32_t*)(notes + pos);
            uint32_t descsz = *(uint32_t*)(notes + pos + 4);
            uint32_t type = *(uint32_t*)(notes + pos + 8);
            size_t aligned_namesz = (namesz + 3) & ~3u;
            size_t aligned_descsz = (descsz + 3) & ~3u;

            if (type == 3 && namesz == 4 && pos + 12 + aligned_namesz + descsz <= size) {
                if (memcmp(notes + pos + 12, "GNU", 4) == 0) {
                    size_t copy = descsz < *out_len ? descsz : *out_len;
                    memcpy(out, notes + pos + 12 + aligned_namesz, copy);
                    *out_len = descsz;
                    kapi_free(notes);
                    return KAPI_OK;
                }
            }

            pos += 12 + aligned_namesz + aligned_descsz;
        }

        kapi_free(notes);
    }

    *out_len = 0;
    return KAPI_ENOENT;
}

uint32_t kapi_elf_gnu_hash(const char* name)
{
    uint32_t h = 5381;
    while (*name) {
        h = (h << 5) + h + (unsigned char)*name;
        name++;
    }
    return h;
}

kapi_elf64_sym_t* kapi_elf_gnu_hash_lookup(kapi_elf_image_t* image, const char* name)
{
    if (!image || !name || !image->dynsym || !image->dynstr) return NULL;

    for (int i = 0; i < (int)image->shnum; i++) {
        if (image->shdrs[i].sh_type == KAPI_SHT_GNU_HASH) {
            kapi_elf_off_t off = image->shdrs[i].sh_offset;
            uint32_t* hash_table = (uint32_t*)(uintptr_t)(image->base_addr + off);
            uint32_t nbuckets = hash_table[0];
            uint32_t symndx = hash_table[1];
            uint32_t maskwords = hash_table[2];
            (void)maskwords;
            uint32_t shift2 = hash_table[3];
            (void)shift2;
            uint32_t* buckets = hash_table + 4 + maskwords;
            uint32_t* chains = buckets + nbuckets;

            uint32_t h = kapi_elf_gnu_hash(name);
            uint32_t bucket = h % nbuckets;
            uint32_t sym_idx = buckets[bucket];

            if (sym_idx == 0) return NULL;

            while (sym_idx >= symndx) {
                kapi_elf64_sym_t* sym = &image->dynsym[sym_idx];
                const char* sname = image->dynstr + sym->st_name;
                if (strcmp(sname, name) == 0 && sym->st_value != 0) {
                    return sym;
                }
                if ((chains[sym_idx - symndx] & ~1u) == 0) break;
                sym_idx++;
            }

            return NULL;
        }
    }

    return NULL;
}

kapi_elf64_sym_t* kapi_elf_sysv_hash_lookup(kapi_elf_image_t* image, const char* name)
{
    if (!image || !name || !image->dynsym || !image->dynstr) return NULL;

    for (int i = 0; i < (int)image->shnum; i++) {
        if (image->shdrs[i].sh_type == KAPI_SHT_HASH) {
            uint32_t* hash_table = (uint32_t*)(uintptr_t)(image->base_addr + image->shdrs[i].sh_offset);
            uint32_t nbucket = hash_table[0];
            uint32_t nchain = hash_table[1];
            (void)nchain;
            uint32_t* buckets = hash_table + 2;
            uint32_t* chains = buckets + nbucket;

            uint32_t h = 0;
            for (const char* p = name; *p; p++) {
                h = (h << 4) + (unsigned char)*p;
                h ^= (h >> 24) & 0xf0;
            }

            uint32_t idx = buckets[h % nbucket];
            while (idx != 0) {
                kapi_elf64_sym_t* sym = &image->dynsym[idx];
                const char* sname = image->dynstr + sym->st_name;
                if (strcmp(sname, name) == 0 && sym->st_value != 0) {
                    return sym;
                }
                idx = chains[idx];
            }

            return NULL;
        }
    }

    return NULL;
}