

#include <arch/buddy.h>
#include <arch/memory.h>
#include <string.h>

static buddy_zone_t main_zone;
static int buddy_initialized = 0;

static void __list_del(page_t* page)
{
    if (page->prev) page->prev->next = page->next;
    if (page->next) page->next->prev = page->prev;
    page->next = page->prev = NULL;
}

static void __list_add(buddy_zone_t* zone, uint32_t order, page_t* page)
{
    page->next = zone->free_list[order];
    page->prev = NULL;
    if (zone->free_list[order]) {
        zone->free_list[order]->prev = page;
    }
    zone->free_list[order] = page;
    zone->free_count[order]++;
}

static page_t* __list_pop(buddy_zone_t* zone, uint32_t order)
{
    page_t* page = zone->free_list[order];
    if (!page) return NULL;
    zone->free_list[order] = page->next;
    if (page->next) page->next->prev = NULL;
    page->next = page->prev = NULL;
    zone->free_count[order]--;
    return page;
}

static uint64_t __buddy_pfn(uint64_t pfn, uint32_t order)
{
    return pfn ^ (1ULL << order);
}

static int __in_freelist(buddy_zone_t* zone, uint32_t order, page_t* page)
{
    page_t* p = zone->free_list[order];
    while (p) {
        if (p == page) return 1;
        p = p->next;
    }
    return 0;
}

int buddy_init(buddy_zone_t* zone, uint64_t base_addr, uint64_t size_bytes)
{
    memset(zone, 0, sizeof(buddy_zone_t));
    spin_init(&zone->lock);

    zone->base_pfn = base_addr / PAGE_SIZE;
    zone->end_pfn = zone->base_pfn + (size_bytes / PAGE_SIZE);
    zone->total_pages = zone->end_pfn - zone->base_pfn;

    uint64_t array_pages = (zone->total_pages * sizeof(page_t) + PAGE_SIZE - 1) / PAGE_SIZE;
    zone->page_array = (page_t*)base_addr;
    zone->page_array_size = zone->total_pages;

    memset(zone->page_array, 0, array_pages * PAGE_SIZE);

    for (uint64_t i = 0; i < zone->total_pages; i++) {
        page_t* p = &zone->page_array[i];
        p->pfn = zone->base_pfn + i;
        p->order = 0;
        p->flags = 0;
        p->next = p->prev = NULL;
        p->slab_cache = NULL;
    }

    /* 修复: page_array 是保留元数据, 不算 used_pages(已分配给用户).
       原代码把 used_pages=array_pages 会让统计偏高. */
    zone->reserved_pages = array_pages;
    zone->used_pages = 0;

    uint64_t free_start = array_pages;
    uint64_t free_pages = zone->total_pages - array_pages;

    /* 标记 page_array 占用的页为 RESERVED, 避免被分配 */
    for (uint64_t i = 0; i < array_pages && i < zone->total_pages; i++) {
        zone->page_array[i].flags |= PAGE_FLAG_RESERVED;
    }

    while (free_pages > 0) {
        uint32_t order = BUDDY_MAX_ORDER;
        while ((1ULL << order) > free_pages && order > BUDDY_MIN_ORDER) {
            order--;
        }

        page_t* page = &zone->page_array[free_start];
        page->order = order;
        __list_add(zone, order, page);

        free_start += (1ULL << order);
        free_pages -= (1ULL << order);
    }

    buddy_initialized = 1;
    return 0;
}

page_t* buddy_alloc_pages(buddy_zone_t* zone, uint32_t order)
{
    if (order > BUDDY_MAX_ORDER) return NULL;

    spin_lock(&zone->lock);

    uint32_t current_order = order;
    while (current_order <= BUDDY_MAX_ORDER) {
        if (zone->free_list[current_order]) {
            break;
        }
        current_order++;
    }

    if (current_order > BUDDY_MAX_ORDER) {
        spin_unlock(&zone->lock);
        return NULL;
    }

    page_t* page = __list_pop(zone, current_order);
    /* 跳过保留页(不应出现在 free_list, 但防御性检查) */
    while (page && (page->flags & PAGE_FLAG_RESERVED)) {
        page = __list_pop(zone, current_order);
    }
    if (!page) {
        spin_unlock(&zone->lock);
        return NULL;
    }
    page->flags |= PAGE_FLAG_USED;

    while (current_order > order) {
        current_order--;
        uint64_t buddy_pfn_val = __buddy_pfn(page->pfn, current_order);
        page_t* buddy = pfn_to_page(zone, buddy_pfn_val);
        if (buddy) {
            buddy->order = current_order;
            buddy->flags = 0;
            __list_add(zone, current_order, buddy);
        }
    }

    page->order = order;
    zone->used_pages += (1ULL << order);
    spin_unlock(&zone->lock);
    return page;
}

void buddy_free_pages(buddy_zone_t* zone, page_t* page)
{
    if (!page || !(page->flags & PAGE_FLAG_USED)) return;

    spin_lock(&zone->lock);

    uint32_t order = page->order;
    page->flags &= ~PAGE_FLAG_USED;
    zone->used_pages -= (1ULL << order);

    for (;;) {
        if (order >= BUDDY_MAX_ORDER) break;

        uint64_t buddy_pfn_val = __buddy_pfn(page->pfn, order);
        page_t* buddy = pfn_to_page(zone, buddy_pfn_val);

        if (buddy->pfn < zone->base_pfn || buddy->pfn >= zone->end_pfn) break;
        if (buddy->flags & PAGE_FLAG_USED) break;
        if (buddy->order != order) break;
        if (!__in_freelist(zone, order, buddy)) break;

        __list_del(buddy);
        zone->free_count[order]--;

        if (buddy->pfn < page->pfn) {
            page = buddy;
        }
        order++;
    }

    page->order = order;
    __list_add(zone, order, page);

    spin_unlock(&zone->lock);
}

page_t* buddy_alloc_pages_n(buddy_zone_t* zone, uint32_t n)
{
    if (n == 0) return NULL;
    if (n > (1ULL << BUDDY_MAX_ORDER)) return NULL;

    uint32_t order = 0;
    while ((1U << order) < n && order < BUDDY_MAX_ORDER) {
        order++;
    }
    return buddy_alloc_pages(zone, order);
}

void* page_to_virt(const page_t* page)
{
    return (void*)(page->pfn * PAGE_SIZE);
}

page_t* virt_to_page(buddy_zone_t* zone, void* virt)
{
    uint64_t pfn = (uint64_t)virt / PAGE_SIZE;
    if (pfn < zone->base_pfn || pfn >= zone->end_pfn) return NULL;
    return &zone->page_array[pfn - zone->base_pfn];
}

page_t* pfn_to_page(buddy_zone_t* zone, uint64_t pfn)
{
    if (pfn < zone->base_pfn || pfn >= zone->end_pfn) return NULL;
    return &zone->page_array[pfn - zone->base_pfn];
}

uint64_t page_to_pfn(const page_t* page)
{
    return page->pfn;
}

void buddy_get_stats(buddy_zone_t* zone, uint64_t* total, uint64_t* free, uint64_t* used)
{
    if (total) *total = zone->total_pages;
    if (used)  *used  = zone->used_pages;
    if (free) {
        uint64_t f = 0;
        for (int i = 0; i <= BUDDY_MAX_ORDER; i++) {
            f += zone->free_count[i] * (1ULL << i);
        }
        *free = f;
    }
}

page_t* alloc_pages(uint32_t order)
{
    return buddy_alloc_pages(&main_zone, order);
}

void free_pages(page_t* page)
{
    buddy_free_pages(&main_zone, page);
}

void* alloc_pages_virt(uint32_t order)
{
    page_t* p = alloc_pages(order);
    return p ? page_to_virt(p) : NULL;
}

void free_pages_virt(void* virt)
{
    page_t* p = virt_to_page(&main_zone, virt);
    if (p) free_pages(p);
}

int buddy_global_init(uint64_t base_addr, uint64_t size_bytes)
{
    return buddy_init(&main_zone, base_addr, size_bytes);
}

buddy_zone_t* buddy_get_main_zone(void)
{
    return &main_zone;
}
