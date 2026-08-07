#include "kex_user.h"

#define KEX_UTS_LEN 65

struct kex_new_utsname {
    char sysname[KEX_UTS_LEN];
    char nodename[KEX_UTS_LEN];
    char release[KEX_UTS_LEN];
    char version[KEX_UTS_LEN];
    char machine[KEX_UTS_LEN];
    char domainname[KEX_UTS_LEN];
};

struct kex_sysinfo {
    long uptime;
    unsigned long loads[3];
    unsigned long totalram;
    unsigned long freeram;
    unsigned long sharedram;
    unsigned long bufferram;
    unsigned long totalswap;
    unsigned long freeswap;
    unsigned short procs;
    unsigned short pad;
    unsigned long totalhigh;
    unsigned long freehigh;
    unsigned int mem_unit;
};

struct kex_info_buf {
    char version[32];
    char name[32];
};

struct kex_version_buf {
    char kernel[64];
    char version[64];
    char build[64];
};

struct kex_uptime_buf {
    long uptime_sec;
    long idle_sec;
};

struct kex_cpu_info_buf {
    unsigned long cpu_id;
    unsigned long freq_mhz;
    unsigned long vendor[4];
    char model_name[96];
};

static void kex_print_str(const char *s) {
    sys_write(1, s, kex_strlen(s));
}

static void kex_print_kv_str(const char *key, const char *val) {
    kex_print_str(key);
    kex_print_str(val);
    kex_putchar('\n');
}

static void kex_print_kv_long(const char *key, long val) {
    kex_print_str(key);
    kex_printlong(val, 1);
}

void _start(void) {
    kex_puts("========================================");
    kex_puts("    Kenux System Information (sysinfo)  ");
    kex_puts("========================================");
    kex_puts("");

    kex_puts("--- kenux_info ---");
    struct kex_info_buf info;
    kex_memset(&info, 0, sizeof(info));
    long ret = kenux_info(&info);
    if (ret == 0) {
        kex_print_kv_str("  version: ", info.version);
        kex_print_kv_str("  name:    ", info.name);
    } else {
        kex_print_kv_long("  failed, ret=", ret);
    }
    kex_puts("");

    kex_puts("--- kenux_get_version ---");
    struct kex_version_buf vbuf;
    kex_memset(&vbuf, 0, sizeof(vbuf));
    ret = kenux_get_version(&vbuf);
    if (ret == 0) {
        kex_print_kv_str("  kernel:  ", vbuf.kernel);
        kex_print_kv_str("  version: ", vbuf.version);
        kex_print_kv_str("  build:   ", vbuf.build);
    } else {
        kex_print_kv_long("  not available, ret=", ret);
    }
    kex_puts("");

    kex_puts("--- kenux_get_uptime ---");
    struct kex_uptime_buf ubuf;
    kex_memset(&ubuf, 0, sizeof(ubuf));
    ret = kenux_get_uptime(&ubuf);
    if (ret == 0) {
        kex_print_kv_long("  uptime (s): ", ubuf.uptime_sec);
        kex_print_kv_long("  idle   (s): ", ubuf.idle_sec);
    } else {
        kex_print_kv_long("  not available, ret=", ret);
    }
    kex_puts("");

    kex_puts("--- kenux_get_cpu_count ---");
    ret = kenux_get_cpu_count();
    if (ret >= 0) {
        kex_print_kv_long("  CPU count: ", ret);
    } else {
        kex_print_kv_long("  not available, ret=", ret);
    }
    kex_puts("");

    kex_puts("--- kenux_get_cpu_info ---");
    long cpu_count = kenux_get_cpu_count();
    if (cpu_count < 0) cpu_count = 1;
    for (long i = 0; i < cpu_count && i < 4; i++) {
        struct kex_cpu_info_buf cbuf;
        kex_memset(&cbuf, 0, sizeof(cbuf));
        ret = kenux_get_cpu_info((int)i, &cbuf);
        if (ret == 0) {
            kex_print_str("  CPU ");
            kex_printlong(i, 0);
            kex_puts(":");
            kex_print_kv_long("    cpu_id:    ", (long)cbuf.cpu_id);
            kex_print_kv_long("    freq_mhz:  ", (long)cbuf.freq_mhz);
            kex_print_kv_str("    model:     ", cbuf.model_name);
        } else {
            kex_print_str("  CPU ");
            kex_printlong(i, 0);
            kex_print_kv_long(" not available, ret=", ret);
        }
    }
    kex_puts("");

    kex_puts("--- sys_uname ---");
    struct kex_new_utsname uname_buf;
    kex_memset(&uname_buf, 0, sizeof(uname_buf));
    ret = sys_uname(&uname_buf);
    if (ret == 0) {
        kex_print_kv_str("  sysname:    ", uname_buf.sysname);
        kex_print_kv_str("  nodename:   ", uname_buf.nodename);
        kex_print_kv_str("  release:    ", uname_buf.release);
        kex_print_kv_str("  version:    ", uname_buf.version);
        kex_print_kv_str("  machine:    ", uname_buf.machine);
        kex_print_kv_str("  domainname: ", uname_buf.domainname);
    } else {
        kex_print_kv_long("  failed, ret=", ret);
    }
    kex_puts("");

    kex_puts("--- sys_sysinfo ---");
    struct kex_sysinfo si;
    kex_memset(&si, 0, sizeof(si));
    ret = sys_sysinfo(&si);
    if (ret == 0) {
        kex_print_kv_long("  uptime (s):    ", si.uptime);
        kex_print_kv_long("  totalram:      ", (long)si.totalram);
        kex_print_kv_long("  freeram:       ", (long)si.freeram);
        kex_print_kv_long("  sharedram:     ", (long)si.sharedram);
        kex_print_kv_long("  bufferram:     ", (long)si.bufferram);
        kex_print_kv_long("  totalswap:     ", (long)si.totalswap);
        kex_print_kv_long("  freeswap:      ", (long)si.freeswap);
        kex_print_kv_long("  procs:         ", (long)si.procs);
        kex_print_kv_long("  totalhigh:     ", (long)si.totalhigh);
        kex_print_kv_long("  freehigh:      ", (long)si.freehigh);
        kex_print_kv_long("  mem_unit:      ", (long)si.mem_unit);
        kex_puts("  load averages:");
        kex_print_str("    1min:  ");
        kex_printlong((long)si.loads[0], 1);
        kex_print_str("    5min:  ");
        kex_printlong((long)si.loads[1], 1);
        kex_print_str("    15min: ");
        kex_printlong((long)si.loads[2], 1);
    } else {
        kex_print_kv_long("  failed, ret=", ret);
    }
    kex_puts("");

    kex_puts("--- kenux_get_loadavg ---");
    long loads[3];
    kex_memset(loads, 0, sizeof(loads));
    ret = kenux_get_loadavg(loads, 3);
    if (ret > 0) {
        kex_print_str("  1min:  ");
        kex_printlong(loads[0], 1);
        kex_print_str("  5min:  ");
        kex_printlong(loads[1], 1);
        kex_print_str("  15min: ");
        kex_printlong(loads[2], 1);
    } else {
        kex_print_kv_long("  failed, ret=", ret);
    }
    kex_puts("");

    kex_puts("========================================");
    kex_puts("  System information dump complete.");
    kex_puts("========================================");

    sys_exit(0);
}
