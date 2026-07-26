

#include <arch/memory.h>
#include <string.h>

/* 内核镜像结束符号, 由 linker.ld 导出 */
extern char kernel_end[];
extern char kernel_start[];

static memory_pool_t memory_pool;
/* 关键修复: 原值 0x100000 会覆盖 0x200000 处的内核镜像.
   从 kernel_end 之后对齐到 PAGE_SIZE 开始分配. */
static uint64_t memory_start = 0;
static int memory_pool_inited = 0;

static vma_t kernel_vma;
static void* current_pml4 = NULL;

void memory_init(void)
{
    memset(&memory_pool, 0, sizeof(memory_pool));
    memory_pool.total_size = 0;
    memory_pool.used_size = 0;
    memory_pool.alloc_count = 0;
    memory_pool.free_count = 0;

    for (int i = 0; i < MEMORY_POOL_SIZE; i++) {
        memory_pool.blocks[i].address = 0;
        memory_pool.blocks[i].size = 0;
        memory_pool.blocks[i].used = 0;
        memory_pool.blocks[i].order = 0;
    }

    /* 从内核镜像结束后开始, 对齐到 PAGE_SIZE */
    if (!memory_pool_inited) {
        uint64_t kend = (uint64_t)(uintptr_t)kernel_end;
        memory_start = (kend + PAGE_SIZE - 1) & ~((uint64_t)PAGE_SIZE - 1);
        /* 保留一定安全间隔 */
        memory_start += PAGE_SIZE * 16;
        memory_pool_inited = 1;
    }

    vma_init(&kernel_vma);
}

void* memory_alloc(uint64_t size)
{
    if (size == 0) return NULL;

    uint64_t aligned_size = (size + MEMORY_BLOCK_SIZE - 1) & ~(MEMORY_BLOCK_SIZE - 1);

    for (int i = 0; i < MEMORY_POOL_SIZE; i++) {
        if (!memory_pool.blocks[i].used &&
            memory_pool.blocks[i].size >= aligned_size) {
            memory_pool.blocks[i].used = 1;
            memory_pool.used_size += memory_pool.blocks[i].size;
            memory_pool.alloc_count++;
            return (void*)memory_pool.blocks[i].address;
        }
    }

    if (memory_start + aligned_size > 0x80000000ULL) {
        /* 限制在 2GB 以内, 避免与内核高半区/设备 MMIO 冲突 */
        return NULL;
    }

    for (int i = 0; i < MEMORY_POOL_SIZE; i++) {
        if (!memory_pool.blocks[i].used) {
            memory_pool.blocks[i].address = memory_start;
            memory_pool.blocks[i].size = aligned_size;

            uint64_t s = aligned_size;
            int order = 0;
            while (s > MEMORY_BLOCK_SIZE) { s >>= 1; order++; }
            memory_pool.blocks[i].order = (uint8_t)order;

            memory_pool.blocks[i].used = 1;
            memory_start += aligned_size;
            memory_pool.total_size += aligned_size;
            memory_pool.used_size += aligned_size;
            memory_pool.alloc_count++;
            return (void*)memory_pool.blocks[i].address;
        }
    }

    return NULL;
}

void memory_free(void* ptr)
{
    if (!ptr) return;

    for (int i = 0; i < MEMORY_POOL_SIZE; i++) {
        if (memory_pool.blocks[i].address == (uint64_t)ptr) {
            memory_pool.used_size -= memory_pool.blocks[i].size;
            memory_pool.blocks[i].used = 0;
            memory_pool.free_count++;
            break;
        }
    }
}

uint64_t memory_get_free(void)
{
    return memory_pool.total_size - memory_pool.used_size;
}

uint64_t memory_get_total(void)
{
    return memory_pool.total_size;
}

void* memory_alloc_aligned(uint64_t size, uint64_t alignment)
{
    if (alignment <= MEMORY_BLOCK_SIZE) {
        return memory_alloc(size);
    }

    uint64_t total = size + alignment + MEMORY_BLOCK_SIZE;
    uint8_t* raw = (uint8_t*)memory_alloc(total);
    if (!raw) return NULL;

    uint64_t addr = (uint64_t)raw + MEMORY_BLOCK_SIZE;
    addr = (addr + alignment - 1) & ~(alignment - 1);
    return (void*)addr;
}

void* memory_zalloc(uint64_t size)
{
    void* ptr = memory_alloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

void* memory_realloc(void* ptr, uint64_t old_size, uint64_t new_size)
{
    if (!ptr) return memory_alloc(new_size);
    if (new_size == 0) { memory_free(ptr); return NULL; }

    void* new_ptr = memory_alloc(new_size);
    if (!new_ptr) return NULL;

    uint64_t copy_size = old_size < new_size ? old_size : new_size;
    memcpy(new_ptr, ptr, copy_size);
    memory_free(ptr);
    return new_ptr;
}

uint64_t memory_alloc_physical(uint64_t count)
{
    uint64_t size = count * PAGE_SIZE;
    void* ptr = memory_alloc(size);
    if (!ptr) return 0;
    return (uint64_t)(uintptr_t)ptr;
}

void memory_free_physical(uint64_t addr, uint64_t count)
{
    (void)count;
    memory_free((void*)(uintptr_t)addr);
}

void memory_get_stats(uint64_t* total, uint64_t* free, uint64_t* ac, uint64_t* fc)
{
    if (total) *total = memory_pool.total_size;
    if (free) *free = memory_pool.total_size - memory_pool.used_size;
    if (ac) *ac = memory_pool.alloc_count;
    if (fc) *fc = memory_pool.free_count;
}

void vma_init(vma_t* vma)
{
    if (!vma) return;
    memset(vma, 0, sizeof(vma_t));
    vma->heap_start = KERNEL_VMA + 0x100000;
    vma->heap_end = vma->heap_start;
    vma->stack_top = KERNEL_VMA_END;
}

int vma_map(vma_t* vma, uint64_t start, uint64_t end, uint64_t flags, const char* name)
{
    if (!vma || vma->count >= VMA_MAX_REGIONS) return -1;
    if (start >= end) return -1;

    vma_region_t* r = &vma->regions[vma->count];
    r->start = start;
    r->end = end;
    r->flags = flags;
    r->type = 0;
    if (name) {
        strncpy(r->name, name, sizeof(r->name) - 1);
        r->name[sizeof(r->name) - 1] = '\0';
    } else {
        r->name[0] = '\0';
    }
    vma->count++;
    return 0;
}

int vma_unmap(vma_t* vma, uint64_t start, uint64_t end)
{
    if (!vma) return -1;

    for (int i = 0; i < vma->count; i++) {
        if (vma->regions[i].start == start && vma->regions[i].end == end) {

            if (i < vma->count - 1) {
                vma->regions[i] = vma->regions[vma->count - 1];
            }
            vma->count--;
            return 0;
        }
    }
    return -1;
}

vma_region_t* vma_find(vma_t* vma, uint64_t addr)
{
    if (!vma) return NULL;
    for (int i = 0; i < vma->count; i++) {
        if (addr >= vma->regions[i].start && addr < vma->regions[i].end) {
            return &vma->regions[i];
        }
    }
    return NULL;
}

uint64_t vma_alloc_range(vma_t* vma, uint64_t size, uint64_t align)
{
    if (!vma || vma->count >= VMA_MAX_REGIONS) return 0;

    uint64_t addr = vma->heap_end;
    if (align > PAGE_SIZE) {
        addr = (addr + align - 1) & ~(align - 1);
    }

    vma->heap_end = addr + size;
    return addr;
}

static uint64_t* __pmap_get_or_alloc_table(uint64_t* entry)
{
    if (*entry & PAGE_PRESENT) {
        return (uint64_t*)PTE_ADDR(*entry);
    }
    void* table = memory_alloc_aligned(PAGE_SIZE, PAGE_SIZE);
    if (!table) return NULL;
    memset(table, 0, PAGE_SIZE);
    *entry = PTE_FLAGS((uint64_t)(uintptr_t)table, PAGE_PRESENT | PAGE_WRITABLE);
    return (uint64_t*)table;
}

void pmap_init(void)
{
    current_pml4 = pmap_get();
}

void* pmap_create(void)
{
    void* pml4 = memory_alloc_aligned(PAGE_SIZE, PAGE_SIZE);
    if (pml4) {
        memset(pml4, 0, PAGE_SIZE);
        if (current_pml4) {
            memcpy(pml4, current_pml4, PAGE_SIZE);
        }
    }
    return pml4;
}

void pmap_switch(void* pml4)
{
    if (!pml4) return;
    current_pml4 = pml4;

    __asm__ volatile (
        "mov %0, %%cr3\n"
        :
        : "r"(pml4)
        : "memory"
    );
}

void* pmap_get(void)
{
    void* cr3;
    __asm__ volatile (
        "mov %%cr3, %0\n"
        : "=r"(cr3)
    );
    return cr3;
}

int pmap_map_page(void* pml4, uint64_t vaddr, uint64_t paddr, uint64_t flags)
{
    if (!pml4) return -1;
    uint64_t* pml4e = (uint64_t*)pml4;
    uint64_t idx_pml4 = (vaddr >> 39) & 0x1FF;
    uint64_t idx_pdpt = (vaddr >> 30) & 0x1FF;
    uint64_t idx_pd   = (vaddr >> 21) & 0x1FF;
    uint64_t idx_pt   = (vaddr >> 12) & 0x1FF;

    uint64_t* pdpt = __pmap_get_or_alloc_table(&pml4e[idx_pml4]);
    if (!pdpt) return -1;
    uint64_t* pd = __pmap_get_or_alloc_table(&pdpt[idx_pdpt]);
    if (!pd) return -1;
    uint64_t* pt = __pmap_get_or_alloc_table(&pd[idx_pd]);
    if (!pt) return -1;

    pt[idx_pt] = PTE_FLAGS(paddr, flags | PAGE_PRESENT);
    __asm__ volatile ("invlpg (%0)" :: "r"(vaddr) : "memory");
    return 0;
}

void pmap_unmap_page(void* pml4, uint64_t vaddr)
{
    if (!pml4) return;
    uint64_t* pml4e = (uint64_t*)pml4;
    uint64_t idx_pml4 = (vaddr >> 39) & 0x1FF;
    if (!(pml4e[idx_pml4] & PAGE_PRESENT)) return;
    uint64_t* pdpt = (uint64_t*)PTE_ADDR(pml4e[idx_pml4]);
    uint64_t idx_pdpt = (vaddr >> 30) & 0x1FF;
    if (!(pdpt[idx_pdpt] & PAGE_PRESENT)) return;
    uint64_t* pd = (uint64_t*)PTE_ADDR(pdpt[idx_pdpt]);
    uint64_t idx_pd = (vaddr >> 21) & 0x1FF;
    if (!(pd[idx_pd] & PAGE_PRESENT)) return;
    uint64_t* pt = (uint64_t*)PTE_ADDR(pd[idx_pd]);
    uint64_t idx_pt = (vaddr >> 12) & 0x1FF;
    pt[idx_pt] = 0;
    __asm__ volatile ("invlpg (%0)" :: "r"(vaddr) : "memory");
}

uint64_t pmap_get_physical(void* pml4, uint64_t vaddr)
{
    if (!pml4) return 0;
    uint64_t* pml4e = (uint64_t*)pml4;
    uint64_t idx_pml4 = (vaddr >> 39) & 0x1FF;
    if (!(pml4e[idx_pml4] & PAGE_PRESENT)) return 0;
    uint64_t* pdpt = (uint64_t*)PTE_ADDR(pml4e[idx_pml4]);
    uint64_t idx_pdpt = (vaddr >> 30) & 0x1FF;
    if (!(pdpt[idx_pdpt] & PAGE_PRESENT)) return 0;
    uint64_t* pd = (uint64_t*)PTE_ADDR(pdpt[idx_pdpt]);
    uint64_t idx_pd = (vaddr >> 21) & 0x1FF;
    if (!(pd[idx_pd] & PAGE_PRESENT)) return 0;
    if (pd[idx_pd] & PAGE_SIZE_2M) {
        return PTE_ADDR(pd[idx_pd]) + (vaddr & 0x1FFFFF);
    }
    uint64_t* pt = (uint64_t*)PTE_ADDR(pd[idx_pd]);
    uint64_t idx_pt = (vaddr >> 12) & 0x1FF;
    if (!(pt[idx_pt] & PAGE_PRESENT)) return 0;
    return PTE_ADDR(pt[idx_pt]) + (vaddr & 0xFFF);
}
