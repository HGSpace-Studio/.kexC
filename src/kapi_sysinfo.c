#include "kapi_sysinfo.h"
#include "kapi.h"
#include <string.h>

static kapi_utsname_t uts_info;
static char hostname[KAPI_SYS_HOSTNAME_MAX] = "kenux";
static char domainname[KAPI_SYS_DOMAIN_MAX] = "";

int kapi_sysinfo_init(void)
{
    strncpy(uts_info.sysname, "KenuxOS", KAPI_SYS_NAME_MAX - 1);
    strncpy(uts_info.nodename, hostname, KAPI_SYS_HOSTNAME_MAX - 1);
    strncpy(uts_info.release, "6.1.0-kenux", KAPI_SYS_VERSION_MAX - 1);
    strncpy(uts_info.version, KAPI_VERSION_STRING, KAPI_SYS_VERSION_MAX - 1);
    strncpy(uts_info.machine, "x86_64", sizeof(uts_info.machine) - 1);
    strncpy(uts_info.domainname, domainname, KAPI_SYS_DOMAIN_MAX - 1);
    return KAPI_OK;
}

int kapi_uname(kapi_utsname_t* buf)
{
    if (!buf) return KAPI_EINVAL;
    *buf = uts_info;
    return KAPI_OK;
}

const char* kapi_get_sysname(void) { return uts_info.sysname; }
const char* kapi_get_release(void) { return uts_info.release; }
const char* kapi_get_version_str(void) { return uts_info.version; }
const char* kapi_get_machine(void) { return uts_info.machine; }
const char* kapi_get_nodename(void) { return uts_info.nodename; }

int kapi_set_nodename(const char* name)
{
    if (!name) return KAPI_EINVAL;
    strncpy(uts_info.nodename, name, KAPI_SYS_HOSTNAME_MAX - 1);
    strncpy(hostname, name, sizeof(hostname) - 1);
    return KAPI_OK;
}

const char* kapi_get_domainname(void) { return uts_info.domainname; }

int kapi_set_domainname(const char* name)
{
    if (!name) return KAPI_EINVAL;
    strncpy(uts_info.domainname, name, KAPI_SYS_DOMAIN_MAX - 1);
    strncpy(domainname, name, sizeof(domainname) - 1);
    return KAPI_OK;
}

int kapi_sysinfo(kapi_sysinfo_t* info)
{
    if (!info) return KAPI_EINVAL;
    memset(info, 0, sizeof(*info));
    info->uptime = kapi_get_uptime_ms() / 1000;
    info->loads[0] = 0;
    info->loads[1] = 0;
    info->loads[2] = 0;
    info->procs = 1;
    info->mem_unit = 1;
    return KAPI_OK;
}

uint64_t kapi_uptime(void)
{
    return kapi_get_uptime_ms() / 1000;
}

void kapi_uptime_detail(uint64_t* uptime, uint64_t* idle)
{
    if (uptime) *uptime = kapi_get_uptime_ms() / 1000;
    if (idle) *idle = 0;
}

double kapi_load_average(int which)
{
    (void)which;
    return 0.0;
}

int kapi_get_cpu_info(kapi_cpuinfo_t* info)
{
    if (!info) return KAPI_EINVAL;
    memset(info, 0, sizeof(*info));
    info->cpu_count = 1;
    info->online_cpus = 1;
    info->active_cpus = 1;
    info->possible_cpus = 1;
    info->present_cpus = 1;
    info->cores_per_socket = 1;
    info->threads_per_core = 1;
    info->socket_count = 1;
    info->is_64bit = true;
    info->has_fpu = true;
    info->has_sse = true;
    info->has_sse2 = true;
    return KAPI_OK;
}

int kapi_get_cpu_stats(int cpu, kapi_cpu_stats_t* stats)
{
    if (!stats) return KAPI_EINVAL;
    memset(stats, 0, sizeof(*stats));
    stats->cpu_id = cpu;
    stats->idle_time = kapi_get_uptime_ms() * 1000;
    stats->total_time = stats->idle_time;
    stats->idle_percent = 100.0;
    return KAPI_OK;
}

int kapi_get_all_cpu_stats(kapi_cpu_stats_t* stats, int count)
{
    if (!stats || count <= 0) return KAPI_EINVAL;
    return kapi_get_cpu_stats(0, &stats[0]);
}

double kapi_cpu_usage(int cpu)
{
    (void)cpu;
    return 0.0;
}

double kapi_cpu_usage_total(void)
{
    return 0.0;
}

int kapi_set_cpu_affinity(int pid, uint64_t mask)
{
    (void)pid; (void)mask;
    return KAPI_OK;
}

uint64_t kapi_get_cpu_affinity(int pid)
{
    (void)pid;
    return 1;
}

int kapi_set_process_priority(int pid, int priority)
{
    (void)pid; (void)priority;
    return KAPI_OK;
}

int kapi_get_process_priority(int priority)
{
    (void)priority;
    return 0;
}

int kapi_set_process_nice(int pid, int nice)
{
    (void)pid; (void)nice;
    return KAPI_OK;
}

int kapi_get_process_nice(int pid)
{
    (void)pid;
    return 0;
}

int kapi_get_disk_stats(const char* device, kapi_disk_stats_t* stats)
{
    if (!stats) return KAPI_EINVAL;
    memset(stats, 0, sizeof(*stats));
    (void)device;
    return KAPI_OK;
}

int kapi_get_net_stats(const char* interface, kapi_net_stats_t* stats)
{
    if (!stats) return KAPI_EINVAL;
    memset(stats, 0, sizeof(*stats));
    (void)interface;
    return KAPI_OK;
}

int kapi_get_proc_stat(int pid, kapi_proc_stat_t* stat)
{
    if (!stat) return KAPI_EINVAL;
    memset(stat, 0, sizeof(*stat));
    stat->pid = pid;
    stat->state_char = 'S';
    return KAPI_OK;
}

int kapi_get_proc_status(int pid, char* status, size_t size)
{
    if (!status || size == 0) return KAPI_EINVAL;
    snprintf(status, size, "Pid:\t%d\n", pid);
    return KAPI_OK;
}

int kapi_get_proc_cmdline(int pid, char* cmdline, size_t size)
{
    if (!cmdline || size == 0) return KAPI_EINVAL;
    cmdline[0] = '\0';
    (void)pid;
    return KAPI_OK;
}

int kapi_get_proc_exe(int pid, char* exe, size_t size)
{
    if (!exe || size == 0) return KAPI_EINVAL;
    exe[0] = '\0';
    (void)pid;
    return KAPI_OK;
}

int kapi_get_proc_cwd(int pid, char* cwd, size_t size)
{
    if (!cwd || size == 0) return KAPI_EINVAL;
    kapi_getcwd(cwd, size);
    (void)pid;
    return KAPI_OK;
}

char** kapi_get_proc_environ(int pid)
{
    (void)pid;
    return NULL;
}

int kapi_get_proc_maps(int pid, void** maps, int max_maps)
{
    (void)pid; (void)maps; (void)max_maps;
    return 0;
}

int kapi_get_proc_fds(int pid, int* fds, int max_fds)
{
    (void)pid; (void)fds; (void)max_fds;
    return 0;
}

int kapi_get_proc_children(int pid, int* children, int max_children)
{
    (void)pid; (void)children; (void)max_children;
    return 0;
}

int kapi_get_system_stats(kapi_system_stats_t* stats)
{
    if (!stats) return KAPI_EINVAL;
    memset(stats, 0, sizeof(*stats));
    stats->boot_time = 0;
    stats->load_avg_1min = 0.0;
    stats->load_avg_5min = 0.0;
    stats->load_avg_15min = 0.0;
    return KAPI_OK;
}

int kapi_get_memory_info(uint64_t* total, uint64_t* free, uint64_t* available,
                         uint64_t* buffers, uint64_t* cached)
{
    if (total) *total = 0;
    if (free) *free = 0;
    if (available) *available = 0;
    if (buffers) *buffers = 0;
    if (cached) *cached = 0;
    return KAPI_OK;
}

int kapi_get_swap_info(uint64_t* total, uint64_t* free)
{
    if (total) *total = 0;
    if (free) *free = 0;
    return KAPI_OK;
}

int kapi_get_process_list(int* pids, int max_pids)
{
    (void)pids; (void)max_pids;
    return 0;
}

int kapi_get_thread_list(int pid, int* tids, int max_tids)
{
    (void)pid; (void)tids; (void)max_tids;
    return 0;
}

int kapi_kill_all(int signum)
{
    (void)signum;
    return KAPI_OK;
}

int kapi_reboot(int cmd)
{
    (void)cmd;
    return KAPI_OK;
}

int kapi_power_off(void) { return KAPI_OK; }
int kapi_halt(void) { return KAPI_OK; }

int kapi_suspend(enum suspend_state state)
{
    (void)state;
    return KAPI_OK;
}

int kapi_get_boot_time(time_t* boot_time)
{
    if (!boot_time) return KAPI_EINVAL;
    *boot_time = 0;
    return KAPI_OK;
}

int kapi_get_timezone(long* tz_minuteswest, int* dst)
{
    if (tz_minuteswest) *tz_minuteswest = 0;
    if (dst) *dst = 0;
    return KAPI_OK;
}

int kapi_set_timezone(long tz_minuteswest, int dst)
{
    (void)tz_minuteswest; (void)dst;
    return KAPI_OK;
}

int kapi_get_hostname(char* name, size_t len)
{
    if (!name || len == 0) return KAPI_EINVAL;
    strncpy(name, hostname, len - 1);
    name[len - 1] = '\0';
    return KAPI_OK;
}

int kapi_set_hostname(const char* name)
{
    if (!name) return KAPI_EINVAL;
    strncpy(hostname, name, sizeof(hostname) - 1);
    hostname[sizeof(hostname) - 1] = '\0';
    strncpy(uts_info.nodename, name, KAPI_SYS_HOSTNAME_MAX - 1);
    return KAPI_OK;
}