

#include "module.h"
#include <arch/elf.h>
#include <arch/memory.h>
#include <string.h>
#include <slab.h>
#include <spinlock.h>

static module_t module_table[MODULE_MAX];
static uint32_t module_count = 0;
static spinlock_t module_lock = SPINLOCK_INIT;

#define EXPORT_MAX 256
typedef struct {
    char    name[64];
    void*   addr;
} export_entry_t;

static export_entry_t exports[EXPORT_MAX];
static uint32_t export_count = 0;

void module_init(void)
{
    memset(module_table, 0, sizeof(module_table));
    module_count = 0;
    spin_init(&module_lock);
    export_count = 0;
}

void module_export_symbol(const char* name, void* addr)
{
    if (!name || export_count >= EXPORT_MAX) return;
    strncpy(exports[export_count].name, name, 63);
    exports[export_count].name[63] = '\0';
    exports[export_count].addr = addr;
    export_count++;
}

void* module_lookup_symbol(const char* name)
{
    if (!name) return NULL;

    for (uint32_t i = 0; i < export_count; i++) {
        if (strcmp(exports[i].name, name) == 0) {
            return exports[i].addr;
        }
    }

    spin_lock(&module_lock);
    for (uint32_t i = 0; i < module_count; i++) {
        module_t* mod = &module_table[i];
        if (mod->name[0] == '\0') continue;

        if (mod->symtab && mod->strtab) {
            Elf64_Sym* syms = (Elf64_Sym*)mod->symtab;
            uint64_t sym_count = mod->symtab_size / sizeof(Elf64_Sym);
            const char* strtab = (const char*)mod->strtab;

            for (uint64_t j = 0; j < sym_count; j++) {
                if (syms[j].st_name == 0) continue;
                const char* sym_name = strtab + syms[j].st_name;
                if (strcmp(sym_name, name) == 0) {
                    if (syms[j].st_shndx == 0) continue;
                    void* addr = (void*)((uint64_t)mod->text_section + syms[j].st_value);
                    spin_unlock(&module_lock);
                    return addr;
                }
            }
        }
    }
    spin_unlock(&module_lock);
    return NULL;
}

static int __apply_relocations(module_t* mod, const uint8_t* data, uint64_t size)
{

    const Elf64_Ehdr* ehdr = (const Elf64_Ehdr*)data;

    const Elf64_Shdr* shdrs = (const Elf64_Shdr*)(data + ehdr->e_shoff);
    const Elf64_Shdr* shstrtab = &shdrs[ehdr->e_shstrndx];
    const char* shstr = (const char*)(data + shstrtab->sh_offset);

    for (uint16_t i = 0; i < ehdr->e_shnum; i++) {
        if (shdrs[i].sh_type != 4) continue;

        const Elf64_Rela* relas = (const Elf64_Rela*)(data + shdrs[i].sh_offset);
        uint64_t rela_count = shdrs[i].sh_size / sizeof(Elf64_Rela);

        for (uint64_t r = 0; r < rela_count; r++) {
            uint64_t offset = relas[r].r_offset;
            uint32_t type = (uint32_t)(relas[r].r_info >> 32);

            int64_t addend = relas[r].r_addend;

            uint8_t* target = NULL;
            if (offset < mod->text_size) {
                target = (uint8_t*)mod->text_section + offset;
            } else if (offset >= mod->text_size && offset < mod->text_size + mod->data_size) {
                target = (uint8_t*)mod->data_section + (offset - mod->text_size);
            }

            if (!target) continue;

            if (type == 1) {
                uint64_t* p = (uint64_t*)target;
                *p = (uint64_t)addend;
            }

            else if (type == 2) {
                uint32_t* p = (uint32_t*)target;
                *p = (uint32_t)((int64_t)addend - (int64_t)target);
            }

            else if (type == 4) {
                uint32_t* p = (uint32_t*)target;
                *p = (uint32_t)((int64_t)addend - (int64_t)target);
            }
        }
    }

    return 0;
}

int module_load(const void* data, uint64_t size, const char* name)
{
    if (!data || !name || size < sizeof(Elf64_Ehdr)) return -1;

    const Elf64_Ehdr* ehdr = (const Elf64_Ehdr*)data;
    if (ehdr->e_type != ET_REL) return -1;
    if (ehdr->e_machine != EM_X86_64) return -1;

    spin_lock(&module_lock);
    if (module_count >= MODULE_MAX) {
        spin_unlock(&module_lock);
        return -1;
    }

    module_t* mod = &module_table[module_count];
    memset(mod, 0, sizeof(module_t));
    strncpy(mod->name, name, MODULE_NAME_MAX - 1);

    const Elf64_Shdr* shdrs = (const Elf64_Shdr*)(data + ehdr->e_shoff);
    const Elf64_Shdr* shstrtab = &shdrs[ehdr->e_shstrndx];
    const char* shstr = (const char*)(data + shstrtab->sh_offset);

    for (uint16_t i = 0; i < ehdr->e_shnum; i++) {
        const char* sec_name = shstr + shdrs[i].sh_name;

        if (shdrs[i].sh_type == 1 && strcmp(sec_name, ".text") == 0) {
            mod->text_size = shdrs[i].sh_size;
            mod->text_section = kzalloc(shdrs[i].sh_size);
            if (!mod->text_section) goto fail;
            memcpy(mod->text_section, data + shdrs[i].sh_offset, shdrs[i].sh_size);
        } else if (shdrs[i].sh_type == 1 && strcmp(sec_name, ".data") == 0) {
            mod->data_size = shdrs[i].sh_size;
            mod->data_section = kzalloc(shdrs[i].sh_size);
            if (!mod->data_section) goto fail;
            memcpy(mod->data_section, data + shdrs[i].sh_offset, shdrs[i].sh_size);
        } else if (shdrs[i].sh_type == 8) {
            mod->bss_size = shdrs[i].sh_size;
            mod->bss_section = kzalloc(shdrs[i].sh_size);
            if (!mod->bss_section) goto fail;
        } else if (shdrs[i].sh_type == 2) {
            mod->symtab = kzalloc(shdrs[i].sh_size);
            if (mod->symtab) memcpy(mod->symtab, data + shdrs[i].sh_offset, shdrs[i].sh_size);
            mod->symtab_size = shdrs[i].sh_size;
        } else if (shdrs[i].sh_type == 3) {
            mod->strtab = kzalloc(shdrs[i].sh_size);
            if (mod->strtab) memcpy(mod->strtab, data + shdrs[i].sh_offset, shdrs[i].sh_size);
            mod->strtab_size = shdrs[i].sh_size;
        }
    }

    if (mod->symtab && mod->strtab) {
        Elf64_Sym* syms = (Elf64_Sym*)mod->symtab;
        uint64_t sym_count = mod->symtab_size / sizeof(Elf64_Sym);
        const char* strtab = (const char*)mod->strtab;

        for (uint64_t i = 0; i < sym_count; i++) {
            const char* sym_name = strtab + syms[i].st_name;

            if (strcmp(sym_name, "init_module") == 0) {
                mod->init = (int (*)(void))(mod->text_section + syms[i].st_value);
            } else if (strcmp(sym_name, "cleanup_module") == 0) {
                mod->exit = (void (*)(void))(mod->text_section + syms[i].st_value);
            }
        }
    }

    mod->state = MODULE_STATE_LIVE;
    mod->ref_count = 1;
    module_count++;

    spin_unlock(&module_lock);

    if (mod->init) {
        mod->init();
    }

    return 0;

fail:
    if (mod->text_section) kfree(mod->text_section);
    if (mod->data_section) kfree(mod->data_section);
    if (mod->bss_section) kfree(mod->bss_section);
    if (mod->symtab) kfree(mod->symtab);
    if (mod->strtab) kfree(mod->strtab);
    memset(mod, 0, sizeof(module_t));
    spin_unlock(&module_lock);
    return -1;
}

int module_unload(const char* name)
{
    if (!name) return -1;

    spin_lock(&module_lock);
    for (uint32_t i = 0; i < module_count; i++) {
        if (strcmp(module_table[i].name, name) == 0) {
            module_t* mod = &module_table[i];

            if (mod->ref_count > 1) {
                spin_unlock(&module_lock);
                return -1;
            }

            mod->state = MODULE_STATE_GOING;

            if (mod->exit) {
                spin_unlock(&module_lock);
                mod->exit();
                spin_lock(&module_lock);
            }

            kfree(mod->text_section);
            kfree(mod->data_section);
            kfree(mod->bss_section);
            kfree(mod->symtab);
            kfree(mod->strtab);

            for (uint32_t j = i; j < module_count - 1; j++) {
                module_table[j] = module_table[j + 1];
            }
            module_count--;
            memset(&module_table[module_count], 0, sizeof(module_t));

            spin_unlock(&module_lock);
            return 0;
        }
    }
    spin_unlock(&module_lock);
    return -1;
}

module_t* module_find(const char* name)
{
    if (!name) return NULL;
    for (uint32_t i = 0; i < module_count; i++) {
        if (strcmp(module_table[i].name, name) == 0) {
            return &module_table[i];
        }
    }
    return NULL;
}

int module_get(module_t* mod)
{
    if (!mod) return -1;
    mod->ref_count++;
    return 0;
}

int module_put(module_t* mod)
{
    if (!mod || mod->ref_count == 0) return -1;
    mod->ref_count--;
    return 0;
}

module_t* module_get_first(void)
{
    return module_count > 0 ? &module_table[0] : NULL;
}

module_t* module_get_next(module_t* mod)
{
    if (!mod) return NULL;
    uint32_t idx = mod - module_table;
    return (idx + 1 < module_count) ? &module_table[idx + 1] : NULL;
}
