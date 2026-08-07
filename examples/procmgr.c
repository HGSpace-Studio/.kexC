#include "kex_user.h"

struct kex_cpu_info_buf {
    unsigned long cpu_id;
    unsigned long freq_mhz;
    unsigned long vendor[4];
    char model_name[96];
};

static void print_str(const char *s) {
    sys_write(1, s, kex_strlen(s));
}

static void print_kv(const char *key, long val) {
    print_str(key);
    kex_printlong(val, 1);
}

void _start(void) {
    kex_puts("========================================");
    kex_puts("   Kenux Process Manager (procmgr)     ");
    kex_puts("========================================");
    kex_puts("");

    long pid = sys_getpid();
    long ppid = sys_getppid();
    long tid = sys_gettid();
    long uid = sys_getuid();
    long gid = sys_getgid();
    long euid = sys_geteuid();
    long egid = sys_getegid();

    kex_puts("--- Current Process ---");
    print_kv("  PID:  ", pid);
    print_kv("  PPID: ", ppid);
    print_kv("  TID:  ", tid);
    print_kv("  UID:  ", uid);
    print_kv("  GID:  ", gid);
    print_kv("  EUID: ", euid);
    print_kv("  EGID: ", egid);
    kex_puts("");

    kex_puts("--- Process Group / Session ---");
    long pgrp = sys_getpgrp();
    long sid = sys_setsid();
    print_kv("  PGRP: ", pgrp);
    if (sid >= 0) {
        print_kv("  SID:  ", sid);
    } else {
        kex_puts("  SID:  (already session leader)");
    }
    kex_puts("");

    kex_puts("--- CPU Affinity ---");
    long cpu_count = kenux_get_cpu_count();
    print_kv("  CPU count: ", cpu_count);

    if (cpu_count > 0) {
        unsigned char mask[16];
        kex_memset(mask, 0, sizeof(mask));
        long ret = kenux_get_affinity((int)pid, mask);
        if (ret == 0) {
            kex_puts("  Affinity mask:");
            for (long i = 0; i < cpu_count && i < 64; i++) {
                int byte_idx = (int)(i / 8);
                int bit_idx = (int)(i % 8);
                if (mask[byte_idx] & (1 << bit_idx)) {
                    print_str("    CPU ");
                    kex_printlong(i, 1);
                }
            }
        } else {
            print_kv("  get_affinity failed: ", ret);
        }
    }
    kex_puts("");

    kex_puts("--- Fork Demo ---");
    long child = sys_fork();
    if (child == 0) {
        long my_pid = sys_getpid();
        long my_ppid = sys_getppid();
        kex_puts("  [Child] process started");
        print_kv("  [Child] PID:  ", my_pid);
        print_kv("  [Child] PPID: ", my_ppid);

        for (volatile int i = 0; i < 1000000; i++) { }
        kex_puts("  [Child] work done, exiting");
        sys_exit(0);
    } else if (child > 0) {
        kex_puts("  [Parent] forked child");
        print_kv("  [Parent] child PID: ", child);

        int status;
        long waited = sys_wait4((int)child, &status, 0, 0);
        print_kv("  [Parent] wait4 returned: ", waited);
        print_kv("  [Parent] child status:   ", (long)status);
    } else {
        print_kv("  fork failed: ", child);
    }
    kex_puts("");

    kex_puts("--- Scheduling ---");
    long prio = sys_getpriority(0, (int)pid);
    print_kv("  Current priority: ", prio);

    long ret = sys_setpriority(0, (int)pid, 5);
    if (ret == 0) {
        long new_prio = sys_getpriority(0, (int)pid);
        print_kv("  New priority:     ", new_prio);
    } else {
        print_kv("  setpriority failed: ", ret);
    }

    kex_puts("  Yielding CPU...");
    sys_sched_yield();
    kex_puts("  Yield complete.");
    kex_puts("");

    kex_puts("--- Resource Limits ---");
    struct {
        unsigned long cur;
        unsigned long max;
    } rlim;
    kex_memset(&rlim, 0, sizeof(rlim));

    ret = sys_getrlimit(7, &rlim);
    if (ret == 0) {
        kex_puts("  RLIMIT_NOFILE:");
        print_kv("    soft: ", (long)rlim.cur);
        print_kv("    hard: ", (long)rlim.max);
    }

    ret = sys_getrlimit(9, &rlim);
    if (ret == 0) {
        kex_puts("  RLIMIT_AS:");
        print_kv("    soft: ", (long)rlim.cur);
        print_kv("    hard: ", (long)rlim.max);
    }
    kex_puts("");

    kex_puts("--- CPU Info ---");
    for (long i = 0; i < cpu_count && i < 2; i++) {
        struct kex_cpu_info_buf cbuf;
        kex_memset(&cbuf, 0, sizeof(cbuf));
        ret = kenux_get_cpu_info((int)i, &cbuf);
        if (ret == 0) {
            print_str("  CPU ");
            kex_printlong(i, 0);
            kex_puts(":");
            print_kv("    cpu_id:   ", (long)cbuf.cpu_id);
            print_kv("    freq_mhz: ", (long)cbuf.freq_mhz);
            print_str("    model:    ");
            print_str(cbuf.model_name);
            kex_putchar('\n');
        }
    }
    kex_puts("");

    kex_puts("========================================");
    kex_puts("  Process management dump complete.");
    kex_puts("========================================");

    sys_exit(0);
}
