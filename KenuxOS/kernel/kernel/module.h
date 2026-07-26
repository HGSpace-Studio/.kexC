

#ifndef KERNEL_MODULE_H
#define KERNEL_MODULE_H

#include <arch/types.h>
#include <arch/spinlock.h>

#define MODULE_NAME_MAX   64
#define MODULE_MAX        32

#define MODULE_STATE_LIVE    0
#define MODULE_STATE_COMING  1
#define MODULE_STATE_GOING   2

typedef struct module {
    char     name[MODULE_NAME_MAX];
    void*    text_section;
    uint64_t text_size;
    void*    data_section;
    uint64_t data_size;
    void*    bss_section;
    uint64_t bss_size;
    void*    symtab;
    uint64_t symtab_size;
    void*    strtab;
    uint64_t strtab_size;
    void*    core_text;
    uint64_t core_text_size;
    int      (*init)(void);
    void     (*exit)(void);
    uint32_t state;
    uint32_t ref_count;
    struct module* next;
} module_t;

void module_init(void);

int module_load(const void* data, uint64_t size, const char* name);

int module_unload(const char* name);

module_t* module_find(const char* name);

int module_get(module_t* mod);
int module_put(module_t* mod);

module_t* module_get_first(void);
module_t* module_get_next(module_t* mod);

void module_export_symbol(const char* name, void* addr);
void* module_lookup_symbol(const char* name);

#endif
