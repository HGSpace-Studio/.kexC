

#ifndef KAPI_MEMORY_H
#define KAPI_MEMORY_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_MEM_DEFAULT     0x00
#define KAPI_MEM_KERNEL      0x01
#define KAPI_MEM_USER        0x02
#define KAPI_MEM_DMA         0x04
#define KAPI_MEM_UNCACHED    0x08
#define KAPI_MEM_WRITEBACK   0x10

#define KAPI_PROT_NONE       0x00
#define KAPI_PROT_READ       0x01
#define KAPI_PROT_WRITE      0x02
#define KAPI_PROT_EXEC       0x04

#define KAPI_MAP_SHARED      0x01
#define KAPI_MAP_PRIVATE     0x02
#define KAPI_MAP_FIXED       0x04
#define KAPI_MAP_ANONYMOUS   0x08

typedef struct {
    uint64_t total;
    uint64_t free;
    uint64_t used;
    uint64_t cached;
    uint64_t total_pages;
    uint64_t free_pages;
} kapi_mem_info_t;

void* kapi_malloc(size_t size);

void kapi_free(void* ptr);

void* kapi_realloc(void* ptr, size_t size);

void* kapi_calloc(size_t count, size_t size);

uint64_t kapi_page_alloc(int count, uint32_t flags);

void kapi_page_free(uint64_t paddr, int count);

uint64_t kapi_mmap(uint64_t vaddr, uint64_t paddr, size_t size, int prot);

int kapi_munmap(uint64_t vaddr, size_t size);

int kapi_mprotect(uint64_t vaddr, size_t size, int prot);

int kapi_mem_get_info(kapi_mem_info_t* info);

uint64_t kapi_mem_get_usage(void);

#ifdef __cplusplus
}
#endif

#endif
