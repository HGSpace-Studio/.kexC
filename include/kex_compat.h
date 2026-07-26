/*
 * kex_compat.h - Windows/Linux 平台兼容层
 *
 * Linux: 直接使用 POSIX API (mmap, mprotect, dirent)
 * Windows: 用 VirtualAlloc 等 API 模拟 POSIX 接口
 */
#ifndef KEX_COMPAT_H
#define KEX_COMPAT_H

#ifdef _WIN32
/* ===== Windows ===== */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>

/* ---- mmap / mprotect / munmap 兼容 ---- */
#define PROT_READ    0x1
#define PROT_WRITE   0x2
#define PROT_EXEC    0x4

#define MAP_PRIVATE    0x02
#define MAP_ANONYMOUS  0x20
#define MAP_FAILED     ((void *)(intptr_t)-1)

static inline DWORD _prot_to_win(int prot) {
    if (prot & PROT_EXEC) {
        if (prot & PROT_WRITE) return PAGE_EXECUTE_READWRITE;
        if (prot & PROT_READ)  return PAGE_EXECUTE_READ;
        return PAGE_NOACCESS;
    }
    if (prot & PROT_WRITE) return PAGE_READWRITE;
    if (prot & PROT_READ)  return PAGE_READONLY;
    return PAGE_NOACCESS;
}

static inline void *mmap(void *addr, size_t length, int prot, int flags,
                         int fd, long long offset) {
    (void)fd; (void)offset; (void)addr; (void)flags;
    void *p = VirtualAlloc(NULL, length, MEM_COMMIT | MEM_RESERVE,
                           _prot_to_win(prot));
    return p ? p : MAP_FAILED;
}

static inline int mprotect(void *addr, size_t len, int prot) {
    DWORD old;
    return VirtualProtect(addr, len, _prot_to_win(prot), &old) ? 0 : -1;
}

static inline int munmap(void *addr, size_t length) {
    (void)length;
    return VirtualFree(addr, 0, MEM_RELEASE) ? 0 : -1;
}

/* ---- 低地址内存分配 (替代 MAP_32BIT, 保证地址 < 4GB) ---- */
static inline void *mmap_low(size_t length, int prot) {
    /* 从 0x00010000 开始向上尝试, 找到第一个可用的低 4GB 地址 */
    for (uintptr_t addr = 0x00010000; addr < 0x100000000ULL; addr += 0x00010000) {
        void *p = VirtualAlloc((void *)addr, length,
                               MEM_COMMIT | MEM_RESERVE, _prot_to_win(prot));
        if (p) return p;
    }
    return MAP_FAILED;
}

/* ---- dirent 兼容 (mingw-w64 自带 dirent.h, 但部分版本可能缺失) ---- */
/* mingw-w64 提供 <dirent.h>, 直接包含即可 */
#include <dirent.h>

#else
/* ===== Linux ===== */
#include <sys/mman.h>
#include <unistd.h>
#include <dirent.h>
#include <stdint.h>

/* Linux 上 mmap_low 用 MAP_32BIT */
static inline void *mmap_low(size_t length, int prot) {
    return mmap(NULL, length, prot,
                MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
}
#endif

#endif /* KEX_COMPAT_H */
