/*
 * kex_main.c - KexKit 统一入口
 *
 * 子命令:
 *   kex compile <input.c> <output.kex> [--name N] [--kxp]   编译
 *   kex run <program.kex> [-L<libdir>] [args...]            运行
 *   kex version                                              版本信息
 *   kex help                                                 帮助
 */
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

/* 由 kexc.c 和 kex_interp.c 提供 */
int kexc_main(int argc, char **argv);
int kex_interp_main(int argc, char **argv);

#define KEX_VERSION_STR "1.0"

static void print_banner(void) {
    printf("\n");
    printf("+------------------------------------------------------------+\n");
    printf("|          KexKit - Unified Tool  v%s                        |\n", KEX_VERSION_STR);
    printf("|          compile / run / HGS format                        |\n");
    printf("+------------------------------------------------------------+\n");
    printf("\n");
}

static void print_help(void) {
    print_banner();
    printf("用法: kex <command> [options]\n\n");
    printf("命令:\n");
    printf("  compile <input.c> <output.kex> [--name NAME] [--kxp]\n");
    printf("          编译 C 源码为 .kex (HGS 格式)\n");
    printf("  run <program.kex> [-L<libdir>] [args...]\n");
    printf("          运行 .kex 程序\n");
    printf("  version 显示版本信息\n");
    printf("  help    显示此帮助\n");
    printf("\n");
}

static void print_version(void) {
    printf("KexKit v%s (HGS format)\n", KEX_VERSION_STR);
    printf("  compiler:  kexc\n");
    printf("  runtime:   kex_interp\n");
#ifdef _WIN32
    printf("  platform:  Windows (x86_64)\n");
#else
    printf("  platform:  Linux (x86_64)\n");
#endif
    printf("\n");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_help();
        return 1;
    }

    /* 子命令分发 */
    const char *cmd = argv[1];

    if (strcmp(cmd, "compile") == 0 || strcmp(cmd, "c") == 0) {
        /* kex compile <args...> → kexc_main(argc-2, argv+2) */
        return kexc_main(argc - 2, argv + 2);
    }

    if (strcmp(cmd, "run") == 0 || strcmp(cmd, "r") == 0) {
        /* kex run <args...> → kex_interp_main(argc-2, argv+2) */
        return kex_interp_main(argc - 2, argv + 2);
    }

    if (strcmp(cmd, "version") == 0 || strcmp(cmd, "-v") == 0 ||
        strcmp(cmd, "--version") == 0) {
        print_version();
        return 0;
    }

    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "-h") == 0 ||
        strcmp(cmd, "--help") == 0) {
        print_help();
        return 0;
    }

    /* 未知命令 */
    fprintf(stderr, "未知命令: %s\n", cmd);
    fprintf(stderr, "运行 'kex help' 查看用法\n");
    return 1;
}
