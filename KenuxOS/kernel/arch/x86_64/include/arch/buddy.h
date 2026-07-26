

#ifndef ARCH_BUDDY_H
#define ARCH_BUDDY_H

#include <arch/types.h>
#include <arch/spinlock.h>

#define BUDDY_MIN_ORDER     0
#define BUDDY_MAX_ORDER     11
#define BUDDY_ORDERS        (BUDDY_MAX_ORDER + 1)

typedef struct page {
    struct page* next;
    struct page* prev;
    uint32_t     order;
    uint32_t     flags;
    uint64_t     pfn;
    struct kmem_cache* slab_cache;  /* SLAB 反向映射: 属于哪个 cache */
} page_t;

#define PAGE_FLAG_USED      0x01
#define PAGE_FLAG_RESERVED  0x02
#define PAGE_FLAG_SLAB      0x04
#define PAGE_FLAG_DMA       0x08

typedef struct {
    page_t* free_list[BUDDY_ORDERS];
    uint64_t  free_count[BUDDY_ORDERS];
    spinlock_t lock;

    page_t* page_array;
    uint64_t page_array_size;
    uint64_t total_pages;
    uint64_t used_pages;
    uint64_t reserved_pages;

    uint64_t base_pfn;
    uint64_t end_pfn;
} buddy_zone_t;

int buddy_init(buddy_zone_t* zone, uint64_t base_addr, uint64_t size_bytes);

page_t* buddy_alloc_pages(buddy_zone_t* zone, uint32_t order);

void buddy_free_pages(buddy_zone_t* zone, page_t* page);

page_t* buddy_alloc_pages_n(buddy_zone_t* zone, uint32_t n);

void* page_to_virt(const page_t* page);
page_t* virt_to_page(buddy_zone_t* zone, void* virt);

page_t* pfn_to_page(buddy_zone_t* zone, uint64_t pfn);
uint64_t page_to_pfn(const page_t* page);

void buddy_get_stats(buddy_zone_t* zone, uint64_t* total, uint64_t* free, uint64_t* used);

buddy_zone_t* buddy_get_main_zone(void);
page_t* alloc_pages(uint32_t order);
void free_pages(page_t* page);
void* alloc_pages_virt(uint32_t order);
void free_pages_virt(void* virt);

#endif
