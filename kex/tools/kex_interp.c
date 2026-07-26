/* kex_interp.c - .kex 解释器 / 加载执行器
 *
 * 在主机 (Linux/WSL) 或 KenuxOS 用户态运行 .kex 程序:
 *   1. 加载并校验 .kex (魔数 / CRC32 / 标记)
 *   2. 校验导入表 (所有 API 须可被 Kenux API 表解析)
 *   3. 生成 syscall stub, 回填导入地址占位
 *   4. 加载 .kxp 依赖库, 建立导出符号表
 *   5. 处理重定位表: 回填 call rel32 目标地址
 *   6. mmap 可执行内存 (先 RW 写入, 再 mprotect 为 RX, 满足 W^X)
 *   7. 分配栈, 跳转到入口
 *
 * 用法: ./kex_interp <program.kex> [args...]
 *       ./kex_interp -L<libdir> <program.kex>
 */
#include "kex_loader.h"
#include "kex_api.h"
#include "kex_format.h"
#include "kex_crc32.h"
#include "kex_compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

/* ---- 已加载的 .kxp 库 ---- */
typedef struct {
    kex_program_t prog;     /* 解析后的库 */
    void *code_mem;         /* mmap 的代码区 */
    size_t code_alloc;      /* mmap 大小 */
    void *rodata_mem;       /* mmap 的 .rodata 区 (可为 NULL) */
    size_t rodata_alloc;    /* .rodata mmap 大小 */
    char libname[64];       /* 库名 (不含路径) */
} loaded_kxp_t;

#define MAX_LIBS 32
static loaded_kxp_t g_libs[MAX_LIBS];
static int g_nlibs = 0;
static const char *g_libdir = ".";  /* 默认搜索目录 */

/* ---- 已解析的符号 (全局符号表, 动态扩容) ---- */
#define MAX_SYMS 1024
typedef struct {
    char name[64];
    void *addr;             /* 符号实际运行时地址 */
    uint32_t crc32;
} resolved_sym_t;

static resolved_sym_t g_syms[MAX_SYMS];
static int g_nsyms = 0;

/* 添加已解析符号; 成功返回 0, 表满返回 -1 */
static int add_symbol(const char *name, void *addr) {
    if (g_nsyms >= MAX_SYMS) {
        fprintf(stderr, "符号表已满 (%d)\n", MAX_SYMS);
        return -1;
    }
    /* 去重: 同名符号覆盖旧地址 */
    uint32_t crc = kex_crc32_str(name);
    for (int i = 0; i < g_nsyms; i++) {
        if (g_syms[i].crc32 == crc) {
            g_syms[i].addr = addr;
            return 0;
        }
    }
    strncpy(g_syms[g_nsyms].name, name, 63);
    g_syms[g_nsyms].name[63] = 0;
    g_syms[g_nsyms].addr = addr;
    g_syms[g_nsyms].crc32 = crc;
    g_nsyms++;
    return 0;
}

/* 按 CRC32 查找符号 */
static void *lookup_symbol_crc32(uint32_t crc) {
    for (int i = 0; i < g_nsyms; i++)
        if (g_syms[i].crc32 == crc) return g_syms[i].addr;
    return NULL;
}

/* ---- Windows: syscall 模拟层 ---- */
/* .kex 代码中使用 Linux syscall 指令 (0F 05), 在 Windows 上无法直接执行.
 * 方案: 加载代码后扫描 0F 05 替换为 CD 80 (int 0x80),
 * 用 VEH (Vectored Exception Handler) 拦截 int 0x80 触发的异常,
 * 读取寄存器分发到对应的 Windows API. */
#ifdef _WIN32

/* 实现 Linux syscall 到 Windows API 的映射 */
static long kex_do_syscall(uint64_t nr, uint64_t a1, uint64_t a2, uint64_t a3,
                           uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a4; (void)a5; (void)a6;
    switch (nr) {
    case 0: { /* sys_read(fd, buf, count) */
        if (a1 == 0) { /* stdin */
            char *buf = (char *)a2;
            DWORD got = 0;
            HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
            ReadFile(h, buf, (DWORD)a3, &got, NULL);
            return (long)got;
        }
        return (long)_read((int)a1, (void *)a2, (unsigned)a3);
    }
    case 1: { /* sys_write(fd, buf, count) */
        if (a1 == 1 || a1 == 2) {
            DWORD written = 0;
            HANDLE h = (a1 == 1) ? GetStdHandle(STD_OUTPUT_HANDLE)
                                  : GetStdHandle(STD_ERROR_HANDLE);
            WriteFile(h, (void *)a2, (DWORD)a3, &written, NULL);
            return (long)written;
        }
        return (long)_write((int)a1, (void *)a2, (unsigned)a3);
    }
    case 2: { /* sys_open(path, flags, mode) */
        return (long)_open((const char *)a1, (int)a2, (int)a3);
    }
    case 3: /* sys_close(fd) */
        return (long)_close((int)a1);
    case 8: { /* sys_lseek(fd, offset, whence) */
        return (long)_lseek((int)a1, (long)a2, (int)a3);
    }
    case 39: /* sys_getpid */
        return (long)GetCurrentProcessId();
    case 60: /* sys_exit */
        ExitProcess((UINT)a1);
        return 0;
    case 87: /* sys_unlink */
        return DeleteFileA((const char *)a1) ? 0 : -1;
    case 102: /* sys_getuid */
        return 0;
    case 104: /* sys_getgid */
        return 0;
    case 107: /* sys_geteuid */
        return 0;
    case 108: /* sys_getegid */
        return 0;
    case 231: /* exit_group */
        ExitProcess((UINT)a1);
        return 0;
    default:
        fprintf(stderr, "[VEH] 未实现的 syscall %llu\n", (unsigned long long)nr);
        return -1;
    }
}

/* VEH: 拦截 int 0x80 (CD 80) 异常, 分发到 kex_do_syscall */
static LONG WINAPI kex_veh_handler(PEXCEPTION_POINTERS ep) {
    PEXCEPTION_RECORD er = ep->ExceptionRecord;
    CONTEXT *ctx = ep->ContextRecord;

    /* int 0x80 在 Windows x64 触发 #GP (0xC000001D 或 0xC0000096) */
    if (er->ExceptionCode == 0xC000001D /* STATUS_ILLEGAL_INSTRUCTION */ ||
        er->ExceptionCode == 0xC0000096 /* STATUS_PRIVILEGED_INSTRUCTION */ ||
        er->ExceptionCode == 0xC0000005 /* STATUS_ACCESS_VIOLATION (某些情况) */) {
        uint8_t *rip = (uint8_t *)ctx->Rip;
        if (rip[0] == 0xCD && rip[1] == 0x80) {
            long ret = kex_do_syscall(ctx->Rax, ctx->Rdi, ctx->Rsi, ctx->Rdx,
                                      ctx->R10, ctx->R8, ctx->R9);
            ctx->Rax = (DWORD64)(int64_t)ret;
            ctx->Rip += 2; /* 跳过 CD 80 */
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

/* 扫描代码中的 syscall (0F 05) 替换为 int 0x80 (CD 80) */
static void patch_syscalls(uint8_t *code, size_t size) {
    size_t patched = 0;
    for (size_t i = 0; i + 1 < size; i++) {
        if (code[i] == 0x0F && code[i + 1] == 0x05) {
            code[i] = 0xCD;     /* int 0x80 */
            code[i + 1] = 0x80;
            patched++;
        }
    }
    if (patched > 0)
        printf("  [Win] 替换 %zu 条 syscall -> int 0x80\n", patched);
}

static void g_veh_init(void) {
    AddVectoredExceptionHandler(1, kex_veh_handler);
}

#define KEX_SYSCALL_OP0  0xCD
#define KEX_SYSCALL_OP1  0x80

#else /* Linux */
#define KEX_SYSCALL_OP0  0x0F
#define KEX_SYSCALL_OP1  0x05
static void g_veh_init(void) {}
static void patch_syscalls(uint8_t *code, size_t size) { (void)code; (void)size; }
#endif

/* ---- 生成 syscall stub ---- */
/* 生成: mov eax, <nr>; <syscall>; ret  (共 8 字节)
 * Linux:  syscall = 0F 05
 * Windows: syscall = CD 80 (int 0x80, 由 VEH 拦截)
 * stub 分配在低地址区域 (< 4GB), 保证地址可被 32 位占位容纳 */
#define MAX_STUBS 512
static void *g_stub_page = NULL;
static size_t g_stub_offset = 0;

static void *make_syscall_stub(uint16_t nr) {
    if (!g_stub_page) {
        g_stub_page = mmap_low(4096, PROT_READ | PROT_WRITE | PROT_EXEC);
        if (g_stub_page == MAP_FAILED) {
            g_stub_page = NULL;
            return NULL;
        }
    }
    if (g_stub_offset + 8 > 4096) {
        fprintf(stderr, "stub 页已满\n");
        return NULL;
    }
    uint8_t *s = (uint8_t *)g_stub_page + g_stub_offset;
    g_stub_offset += 8;
    /* stub: B8 xx xx 00 00  CD 80  C3  = mov eax,nr; int 0x80/syscall; ret */
    s[0] = 0xB8;                /* mov eax, imm32 */
    *(uint32_t *)(s + 1) = nr;
    s[5] = KEX_SYSCALL_OP0; s[6] = KEX_SYSCALL_OP1; /* syscall / int 0x80 */
    s[7] = 0xC3;                /* ret */
    return s;
}

/* ---- 边界检查工具 ---- */
/* 校验文件偏移是否在合法范围内 */
static int check_bounds(uint32_t off, uint32_t size, uint32_t total) {
    return (off < total && (uint64_t)off + size <= total);
}

/* 校验 call_off 是否落在代码区内 */
static int check_code_bounds(uint32_t call_off, uint32_t code_off, uint32_t code_size) {
    /* call_off 是操作数偏移, 前面有 1 字节 E8, 后面有 4 字节操作数 */
    if (call_off < code_off + 1) return 0;  /* 不可能: E8 至少在 code_off */
    if (call_off + 4 > code_off + code_size) return 0;
    return 1;
}

/* ---- 加载 .rodata 到独立内存区 (RW) ---- */
/* 返回 mmap 的基址, 失败返回 NULL */
static void *load_rodata(const uint8_t *rodata, uint32_t rodata_size, size_t *out_alloc) {
    if (rodata_size == 0) { *out_alloc = 0; return NULL; }
    size_t alloc = (rodata_size + 4095) & ~4095u;
    void *mem = mmap(NULL, alloc, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) { *out_alloc = 0; return NULL; }
    memcpy(mem, rodata, rodata_size);
    *out_alloc = alloc;
    return mem;
}

/* ---- 应用本地重定位 (修补 .text 中引用 .rodata 的 disp32) ---- */
/* code_base: 代码 mmap 基址, rodata_base: rodata mmap 基址,
 * code_file_off: 代码区文件偏移 (用于换算 patch_off) */
static int apply_local_relocs(void *code_base, void *rodata_base,
                              const kex_local_reloc_entry_t *relocs, uint32_t count,
                              uint32_t code_file_off, uint32_t code_size) {
    for (uint32_t i = 0; i < count; i++) {
        uint32_t patch_off = relocs[i].patch_off;   /* 文件偏移 */
        uint32_t sym_value = relocs[i].sym_value;   /* .rodata 内偏移 */

        /* 边界检查: patch_off 必须在代码区内, 且后面有 4 字节 */
        if (patch_off < code_file_off + 1 ||
            patch_off + 4 > code_file_off + code_size) {
            fprintf(stderr, "  本地重定位 [%u] 越界: patch_off=0x%x\n", i, patch_off);
            continue;
        }

        /* 计算 disp32 操作数在 code_mem 中的偏移 */
        uint32_t off_in_code = patch_off - code_file_off;
        /* RIP 相对: 目标地址 = rodata_base + sym_value
         *          修补值 = 目标地址 - (下一条指令地址)
         *          下一条指令地址 = code_base + off_in_code + 4 */
        uintptr_t target = (uintptr_t)rodata_base + sym_value;
        uintptr_t next_instr = (uintptr_t)code_base + off_in_code + 4;
        int32_t disp = (int32_t)(target - next_instr);

        *(int32_t *)((uint8_t *)code_base + off_in_code) = disp;
    }
    return 0;
}

/* ---- 加载 .kxp 库 ---- */
static int load_kxp(const char *path);

/* 从库目录搜索并加载 .kxp */
__attribute__((unused))
static int load_kxp_by_name(const char *libname) {
    for (int i = 0; i < g_nlibs; i++)
        if (strcmp(g_libs[i].libname, libname) == 0) return 0;
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.kxp", g_libdir, libname);
    return load_kxp(path);
}

static int load_kxp(const char *path) {
    if (g_nlibs >= MAX_LIBS) {
        fprintf(stderr, "已加载库数量达到上限 (%d)\n", MAX_LIBS);
        return -1;
    }

    loaded_kxp_t *lib = &g_libs[g_nlibs];
    int ret = kex_load_file(path, &lib->prog);
    if (ret != KEX_OK) {
        fprintf(stderr, "加载 .kxp 失败: %s (错误 %d)\n", path, ret);
        return -1;
    }

    /* CRC32 校验 (库也必须校验) */
    if (kex_verify_crc32(&lib->prog) != KEX_OK) {
        fprintf(stderr, "错误: .kxp %s CRC32 校验失败, 文件可能损坏\n", path);
        kex_free(&lib->prog);
        return -1;
    }

    /* 从路径提取库名 */
    const char *p = strrchr(path, '/');
    p = p ? p + 1 : path;
    strncpy(lib->libname, p, 63);
    lib->libname[63] = 0;
    char *dot = strrchr(lib->libname, '.');
    if (dot) *dot = 0;

    printf("  加载库: %s (导出=%u)\n", lib->libname, lib->prog.export_count);

    /* 分配可执行内存: 先 RW 写入, 再改 RX (W^X) */
    size_t alloc = (lib->prog.code_size + 4095) & ~4095u;
    lib->code_mem = mmap(NULL, alloc, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (lib->code_mem == MAP_FAILED) {
        fprintf(stderr, "mmap 库代码区失败\n");
        kex_free(&lib->prog);
        return -1;
    }
    lib->code_alloc = alloc;
    memcpy(lib->code_mem, lib->prog.code, lib->prog.code_size);

#ifdef _WIN32
    /* Windows: 库代码也需要 patch syscall */
    patch_syscalls((uint8_t *)lib->code_mem, lib->prog.code_size);
#endif

    /* 加载 .rodata 段到独立内存区 */
    if (lib->prog.rodata_size > 0) {
        lib->rodata_mem = load_rodata(lib->prog.rodata, lib->prog.rodata_size,
                                       &lib->rodata_alloc);
        if (!lib->rodata_mem) {
            fprintf(stderr, "  库 %s .rodata 加载失败\n", lib->libname);
            munmap(lib->code_mem, lib->code_alloc);
            kex_free(&lib->prog);
            return -1;
        }
        printf("    .rodata: %u bytes @ %p\n", lib->prog.rodata_size, lib->rodata_mem);
    }

    /* 应用本地重定位 (修补 .text 中引用 .rodata 的 disp32) */
    if (lib->prog.local_reloc_count > 0 && lib->rodata_mem) {
        apply_local_relocs(lib->code_mem, lib->rodata_mem,
                           lib->prog.local_relocs, lib->prog.local_reloc_count,
                           KEX_CODE_REGION_OFFSET, lib->prog.code_size);
    }

    /* 处理库的导入表: 回填 syscall stub */
    for (uint32_t i = 0; i < lib->prog.import_count; i++) {
        const kex_api_entry_t *e = kex_api_lookup_crc32(lib->prog.imports[i].crc32);
        if (!e) {
            fprintf(stderr, "  库 %s 未解析导入: %s\n",
                    lib->libname, lib->prog.imports[i].name);
            if (lib->rodata_mem) munmap(lib->rodata_mem, lib->rodata_alloc);
            munmap(lib->code_mem, lib->code_alloc);
            kex_free(&lib->prog);
            return -1;
        }
        void *stub = make_syscall_stub(e->syscall_nr);
        if (!stub) {
            fprintf(stderr, "  生成 stub 失败\n");
            if (lib->rodata_mem) munmap(lib->rodata_mem, lib->rodata_alloc);
            munmap(lib->code_mem, lib->code_alloc);
            kex_free(&lib->prog);
            return -1;
        }
        uint32_t slot_off = lib->prog.imports[i].slot_file_off;
        if (!check_bounds(slot_off, 4, lib->prog.raw_size)) {
            fprintf(stderr, "  导入槽位越界: off=0x%x\n", slot_off);
            continue;
        }
        *(uint32_t *)(lib->prog.raw + slot_off) = (uint32_t)(uintptr_t)stub;
    }

    /* 预计算库导出符号的 CRC32 (避免重定位时重复计算) */
    for (uint32_t i = 0; i < lib->prog.export_count; i++) {
        uint32_t off_in_code = lib->prog.exports[i].code_offset - KEX_CODE_REGION_OFFSET;
        void *addr = (uint8_t *)lib->code_mem + off_in_code;
        if (add_symbol(lib->prog.exports[i].name, addr) != 0) {
            if (lib->rodata_mem) munmap(lib->rodata_mem, lib->rodata_alloc);
            munmap(lib->code_mem, lib->code_alloc);
            kex_free(&lib->prog);
            return -1;
        }
        printf("    导出: %s @ %p\n", lib->prog.exports[i].name, addr);
    }

    /* 处理库的跨文件重定位表 */
    for (uint32_t i = 0; i < lib->prog.reloc_count; i++) {
        uint32_t call_off = lib->prog.relocs[i].call_off;
        uint32_t sym_crc  = lib->prog.relocs[i].sym_crc32;

        if (!check_code_bounds(call_off, KEX_CODE_REGION_OFFSET, lib->prog.code_size)) {
            fprintf(stderr, "  库 %s 重定位 [%u] 越界: call_off=0x%x\n",
                    lib->libname, i, call_off);
            continue;
        }

        void *target = lookup_symbol_crc32(sym_crc);
        if (!target) {
            fprintf(stderr, "  库 %s 重定位 [%u] 失败: crc=0x%08x (符号未找到)\n",
                    lib->libname, i, sym_crc);
            continue;
        }

        uint32_t off_in_code = call_off - KEX_CODE_REGION_OFFSET;
        uint8_t *call_addr = (uint8_t *)lib->code_mem + off_in_code - 1;
        int32_t rel = (int32_t)((uintptr_t)target - (uintptr_t)(call_addr + 5));
        *(int32_t *)((uint8_t *)lib->code_mem + off_in_code) = rel;
    }

    /* 代码区改为只读+可执行 (W^X) */
    if (mprotect(lib->code_mem, alloc, PROT_READ | PROT_EXEC) != 0) {
        fprintf(stderr, "  警告: 库 %s mprotect RX 失败\n", lib->libname);
    }
    /* .rodata 改为只读 */
    if (lib->rodata_mem && lib->rodata_alloc > 0) {
        if (mprotect(lib->rodata_mem, lib->rodata_alloc, PROT_READ) != 0) {
            fprintf(stderr, "  警告: 库 %s .rodata mprotect R 失败\n", lib->libname);
        }
    }

    g_nlibs++;
    return 0;
}

/* ---- 设置栈顶并跳转到入口 ---- */
/* 关键: 必须在内联汇编中显式 movq 设置 rsp, 不能依赖 register __asm__("rsp")
 * 因为编译器可能在 prologue 中 push rbp / sub rsp 破坏栈顶设置.
 * 传入参数: %0 = entry, %1 = stack_top
 * 在 asm 中: movq %1, %%rsp; xorq rbp; callq *%0 */
static void run_entry(void *entry, void *stack_top) {
    void (*fn)(void) = (void (*)(void))entry;
#ifdef _WIN32
    /* Windows: .kex 代码里的 syscall 已被替换为 int 0x80 (VEH 处理),
     * run_entry 末尾用 ExitProcess 而非 syscall exit_group */
    __asm__ volatile (
        "movq %1, %%rsp\n\t"
        "xorq %%rbp, %%rbp\n\t"
        "callq *%0\n\t"
        :
        : "r"(fn), "r"(stack_top)
        : "rcx", "r11", "memory"
    );
    ExitProcess(0);
#else
    __asm__ volatile (
        "movq %1, %%rsp\n\t"
        "xorq %%rbp, %%rbp\n\t"
        "callq *%0\n\t"
        "movl $231, %%eax\n\t"   /* exit_group(0) */
        "xorl %%edi, %%edi\n\t"
        "syscall\n\t"
        :
        : "r"(fn), "r"(stack_top)
        : "rcx", "r11", "memory"
    );
#endif
}

/* kex_interp_main: 供统一入口调用, argc/argv 已去掉子命令 */
int kex_interp_main(int argc, char **argv) {
    const char *prog_path = NULL;

#ifdef _WIN32
    /* Windows: 设置 stdout 为二进制模式, 避免 \n 被转为 \r\n */
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
    /* 初始化 VEH (Vectored Exception Handler) 用于拦截 int 0x80 */
    g_veh_init();
#endif

    /* 解析参数 (argv 从参数开始, 无子命令) */
    for (int i = 0; i < argc; i++) {
        if (strncmp(argv[i], "-L", 2) == 0 && argv[i][2]) {
            g_libdir = argv[i] + 2;
        } else if (!prog_path) {
            prog_path = argv[i];
        }
    }
    if (!prog_path) {
        fprintf(stderr, "用法: kex run [-L<libdir>] <program.kex> [args...]\n");
        return 1;
    }

    /* 加载主程序 */
    kex_program_t prog;
    int ret = kex_load_file(prog_path, &prog);
    if (ret != KEX_OK) {
        fprintf(stderr, "加载失败: %d\n", ret);
        return 1;
    }

    /* CRC32 校验 (强制: 失败则拒绝执行) */
    if (kex_verify_crc32(&prog) != KEX_OK) {
        fprintf(stderr, "错误: CRC32 校验失败, 文件可能损坏或被篡改, 拒绝执行\n");
        kex_free(&prog);
        return 1;
    }

    kex_dump(&prog);

    /* 校验导入表并回填 syscall stub */
    printf("\n--- 导入表处理 ---\n");
    for (uint32_t i = 0; i < prog.import_count; i++) {
        const kex_api_entry_t *e = kex_api_lookup_crc32(prog.imports[i].crc32);
        if (!e) {
            printf("  导入 [%u] %-24s -> 待解析 (库符号)\n", i, prog.imports[i].name);
            continue;
        }
        printf("  导入 [%u] %-24s -> syscall %u\n", i, e->name, e->syscall_nr);
        void *stub = make_syscall_stub(e->syscall_nr);
        if (!stub) {
            fprintf(stderr, "生成 stub 失败\n");
            kex_free(&prog);
            return 1;
        }
        uint32_t slot_off = prog.imports[i].slot_file_off;
        if (!check_bounds(slot_off, 4, prog.raw_size)) {
            fprintf(stderr, "导入槽位越界: off=0x%x\n", slot_off);
            continue;
        }
        *(uint32_t *)(prog.raw + slot_off) = (uint32_t)(uintptr_t)stub;
    }

    /* 分配可执行内存: 先 RW 写入, 之后改 RX (W^X) */
    size_t alloc_size = (prog.code_size + 4095) & ~4095u;
    void *code_mem = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (code_mem == MAP_FAILED) {
        fprintf(stderr, "mmap 代码区失败\n");
        kex_free(&prog);
        return 1;
    }
    memcpy(code_mem, prog.code, prog.code_size);

#ifdef _WIN32
    /* Windows: 扫描 .kex 代码中的 syscall (0F 05), 替换为 int 0x80 (CD 80),
     * 由 VEH 拦截并分发到 Windows API */
    patch_syscalls((uint8_t *)code_mem, prog.code_size);
#endif

    /* 加载 .rodata 段到独立内存区 */
    void *rodata_mem = NULL;
    size_t rodata_alloc = 0;
    if (prog.rodata_size > 0) {
        rodata_mem = load_rodata(prog.rodata, prog.rodata_size, &rodata_alloc);
        if (!rodata_mem) {
            fprintf(stderr, ".rodata 加载失败\n");
            munmap(code_mem, alloc_size);
            kex_free(&prog);
            return 1;
        }
        printf("\n--- .rodata 段 ---\n");
        printf("  大小: %u bytes @ %p\n", prog.rodata_size, rodata_mem);
    }

    /* 应用本地重定位 (修补 .text 中引用 .rodata 的 disp32) */
    if (prog.local_reloc_count > 0 && rodata_mem) {
        printf("\n--- 本地重定位处理 ---\n");
        printf("  共 %u 条\n", prog.local_reloc_count);
        apply_local_relocs(code_mem, rodata_mem,
                           prog.local_relocs, prog.local_reloc_count,
                           KEX_CODE_REGION_OFFSET, prog.code_size);
    }

    /* 处理跨文件重定位表: 回填 call rel32 目标地址 */
    if (prog.reloc_count > 0) {
        printf("\n--- 跨文件重定位处理 ---\n");

        /* 加载库目录下所有 .kxp */
        DIR *dir = opendir(g_libdir);
        if (dir) {
            struct dirent *ent;
            while ((ent = readdir(dir)) != NULL) {
                size_t nlen = strlen(ent->d_name);
                if (nlen < 5 || strcmp(ent->d_name + nlen - 4, ".kxp") != 0)
                    continue;
                char path[512];
                snprintf(path, sizeof(path), "%s/%s", g_libdir, ent->d_name);
                load_kxp(path);
            }
            closedir(dir);
        }

        for (uint32_t i = 0; i < prog.reloc_count; i++) {
            uint32_t call_off = prog.relocs[i].call_off;
            uint32_t sym_crc  = prog.relocs[i].sym_crc32;

            /* 边界检查 */
            if (!check_code_bounds(call_off, KEX_CODE_REGION_OFFSET, prog.code_size)) {
                fprintf(stderr, "  重定位 [%u] 越界: call_off=0x%x (代码区=0x%x+0x%x)\n",
                        i, call_off, KEX_CODE_REGION_OFFSET, prog.code_size);
                continue;
            }

            void *target = lookup_symbol_crc32(sym_crc);
            if (!target) {
                fprintf(stderr, "  重定位 [%u] 失败: crc=0x%08x (符号未找到)\n", i, sym_crc);
                continue;
            }

            uint32_t off_in_code = call_off - KEX_CODE_REGION_OFFSET;
            uint8_t *call_addr = (uint8_t *)code_mem + off_in_code - 1;
            int32_t rel = (int32_t)((uintptr_t)target - (uintptr_t)(call_addr + 5));
            *(int32_t *)((uint8_t *)code_mem + off_in_code) = rel;
            printf("  重定位 [%u] call_off=0x%x -> %p\n", i, call_off, target);
        }
    }

    /* 代码区改为只读+可执行 (W^X) */
    if (mprotect(code_mem, alloc_size, PROT_READ | PROT_EXEC) != 0) {
        fprintf(stderr, "警告: mprotect RX 失败\n");
    }
    /* .rodata 改为只读 */
    if (rodata_mem && rodata_alloc > 0) {
        if (mprotect(rodata_mem, rodata_alloc, PROT_READ) != 0) {
            fprintf(stderr, "警告: .rodata mprotect R 失败\n");
        }
    }

    /* 分配栈 (RW) */
    size_t stack_alloc = (prog.header.stack_size + 4095) & ~4095u;
    if (stack_alloc == 0) stack_alloc = KEX_DEFAULT_STACK;
    void *stack = mmap(NULL, stack_alloc,
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (stack == MAP_FAILED) {
        fprintf(stderr, "mmap 栈失败\n");
        if (rodata_mem) munmap(rodata_mem, rodata_alloc);
        munmap(code_mem, alloc_size);
        kex_free(&prog);
        return 1;
    }
    /* 栈顶 16 字节对齐 (SysV ABI: call 前 rsp%16==0) */
    void *stack_top = (void *)(((uintptr_t)stack + stack_alloc) & ~15UL);

    /* 入口地址校验 */
    uint32_t code_region = KEX_CODE_REGION_OFFSET;
    if (prog.entry_file_offset < code_region ||
        prog.entry_file_offset - code_region >= prog.code_size) {
        fprintf(stderr, "错误: 入口偏移 0x%x 越界 (代码区 0x%x-0x%x)\n",
                prog.entry_file_offset, code_region, code_region + prog.code_size);
        if (rodata_mem) munmap(rodata_mem, rodata_alloc);
        munmap(code_mem, alloc_size);
        munmap(stack, stack_alloc);
        kex_free(&prog);
        return 1;
    }
    uint32_t entry_in_code = prog.entry_file_offset - code_region;
    void *entry = (uint8_t *)code_mem + entry_in_code;

    printf("\n>>> 执行 %s (入口=%p, 栈=%zuKB)\n\n",
           prog.name, entry, stack_alloc / 1024);
    fflush(stdout);  /* 关键: 执行前 flush, 避免被 exit_group 跳过 */

    run_entry(entry, stack_top);

    /* 不会到达 (run_entry 末尾调 exit_group) */
    if (rodata_mem) munmap(rodata_mem, rodata_alloc);
    munmap(code_mem, alloc_size);
    munmap(stack, stack_alloc);
    for (int i = 0; i < g_nlibs; i++) {
        if (g_libs[i].rodata_mem) munmap(g_libs[i].rodata_mem, g_libs[i].rodata_alloc);
        munmap(g_libs[i].code_mem, g_libs[i].code_alloc);
        kex_free(&g_libs[i].prog);
    }
    if (g_stub_page) munmap(g_stub_page, 4096);
    kex_free(&prog);
    return 0;
}

/* 独立模式: 编译为 kex_interp 时提供 main */
#ifdef KEX_STANDALONE
int main(int argc, char **argv) {
    /* argv[0]=kex_interp, argv[1..]=参数 */
    return kex_interp_main(argc - 1, argv + 1);
}
#endif
