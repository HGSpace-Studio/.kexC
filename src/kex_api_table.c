/*
 * kex_api_table.c - Kenux API 映射表 (对标 KenuxOS kapi_syscall.h)
 *
 * 覆盖:
 *   - 标准 syscall 0-335 (有 inline 包装的 sys_* 函数)
 *   - Kenux 扩展 syscall 451-500 (kenux_*)
 *
 * 加载器流程: 导入表 CRC32 -> kex_api_lookup_crc32 -> syscall_nr ->
 *            生成 stub (mov eax,nr; syscall; ret) -> 回填导入地址
 */
#include "kex_api.h"
#include "kex_crc32.h"
#include <string.h>

/* 标准 syscall (有 Kenux inline 包装) */
static kex_api_entry_t kex_std_apis[] = {
    /* 文件 IO */
    { "sys_read",        0,   0, KEX_API_SYSCALL, 3 },  /* fd, buf, count */
    { "sys_write",       0,   1, KEX_API_SYSCALL, 3 },
    { "sys_open",        0,   2, KEX_API_SYSCALL, 3 },
    { "sys_close",       0,   3, KEX_API_SYSCALL, 1 },
    { "sys_stat",        0,   4, KEX_API_SYSCALL, 2 },
    { "sys_fstat",       0,   5, KEX_API_SYSCALL, 2 },
    { "sys_lstat",       0,   6, KEX_API_SYSCALL, 2 },
    { "sys_poll",        0,   7, KEX_API_SYSCALL, 3 },
    { "sys_lseek",       0,   8, KEX_API_SYSCALL, 3 },
    { "sys_mmap",        0,   9, KEX_API_SYSCALL, 6 },
    { "sys_mprotect",    0,  10, KEX_API_SYSCALL, 3 },
    { "sys_munmap",      0,  11, KEX_API_SYSCALL, 2 },
    { "sys_brk",         0,  12, KEX_API_SYSCALL, 1 },
    { "sys_rt_sigaction",0,  13, KEX_API_SYSCALL, 4 },
    { "sys_rt_sigprocmask",0,14, KEX_API_SYSCALL, 4 },
    { "sys_rt_sigreturn",0,  15, KEX_API_SYSCALL, 0 },
    { "sys_ioctl",       0,  16, KEX_API_SYSCALL, 3 },
    { "sys_pread64",     0,  17, KEX_API_SYSCALL, 4 },
    { "sys_pwrite64",    0,  18, KEX_API_SYSCALL, 4 },
    { "sys_readv",       0,  19, KEX_API_SYSCALL, 3 },
    { "sys_writev",      0,  20, KEX_API_SYSCALL, 3 },
    { "sys_access",      0,  21, KEX_API_SYSCALL, 2 },
    { "sys_pipe",        0,  22, KEX_API_SYSCALL, 1 },
    { "sys_select",      0,  23, KEX_API_SYSCALL, 5 },
    { "sys_sched_yield", 0,  24, KEX_API_SYSCALL, 0 },
    { "sys_mremap",      0,  25, KEX_API_SYSCALL, 5 },
    { "sys_msync",       0,  26, KEX_API_SYSCALL, 3 },
    { "sys_mincore",     0,  27, KEX_API_SYSCALL, 3 },
    { "sys_madvise",     0,  28, KEX_API_SYSCALL, 3 },
    { "sys_shmget",      0,  29, KEX_API_SYSCALL, 3 },
    { "sys_shmat",       0,  30, KEX_API_SYSCALL, 3 },
    { "sys_shmctl",      0,  31, KEX_API_SYSCALL, 3 },
    { "sys_dup",         0,  32, KEX_API_SYSCALL, 1 },
    { "sys_dup2",        0,  33, KEX_API_SYSCALL, 2 },
    { "sys_pause",       0,  34, KEX_API_SYSCALL, 0 },
    { "sys_nanosleep",   0,  35, KEX_API_SYSCALL, 2 },
    { "sys_getitimer",   0,  36, KEX_API_SYSCALL, 2 },
    { "sys_alarm",       0,  37, KEX_API_SYSCALL, 1 },
    { "sys_setitimer",   0,  38, KEX_API_SYSCALL, 3 },
    { "sys_getpid",      0,  39, KEX_API_SYSCALL, 0 },
    { "sys_sendfile",    0,  40, KEX_API_SYSCALL, 4 },
    /* 网络 */
    { "sys_socket",      0,  41, KEX_API_SYSCALL, 3 },
    { "sys_connect",     0,  42, KEX_API_SYSCALL, 3 },
    { "sys_accept",      0,  43, KEX_API_SYSCALL, 3 },
    { "sys_sendto",      0,  44, KEX_API_SYSCALL, 6 },
    { "sys_recvfrom",    0,  45, KEX_API_SYSCALL, 6 },
    { "sys_sendmsg",     0,  46, KEX_API_SYSCALL, 3 },
    { "sys_recvmsg",     0,  47, KEX_API_SYSCALL, 3 },
    { "sys_shutdown",    0,  48, KEX_API_SYSCALL, 2 },
    { "sys_bind",        0,  49, KEX_API_SYSCALL, 3 },
    { "sys_listen",      0,  50, KEX_API_SYSCALL, 2 },
    { "sys_getsockname", 0,  51, KEX_API_SYSCALL, 3 },
    { "sys_getpeername", 0,  52, KEX_API_SYSCALL, 3 },
    { "sys_socketpair",  0,  53, KEX_API_SYSCALL, 4 },
    { "sys_setsockopt",  0,  54, KEX_API_SYSCALL, 5 },
    { "sys_getsockopt",  0,  55, KEX_API_SYSCALL, 5 },
    /* 进程/线程 */
    { "sys_clone",       0,  56, KEX_API_SYSCALL, 5 },
    { "sys_fork",        0,  57, KEX_API_SYSCALL, 0 },
    { "sys_vfork",       0,  58, KEX_API_SYSCALL, 0 },
    { "sys_execve",      0,  59, KEX_API_SYSCALL, 3 },
    { "sys_exit",        0,  60, KEX_API_SYSCALL, 1 },
    { "sys_wait4",       0,  61, KEX_API_SYSCALL, 4 },
    { "sys_kill",        0,  62, KEX_API_SYSCALL, 2 },
    { "sys_uname",       0,  63, KEX_API_SYSCALL, 1 },
    { "sys_semget",      0,  64, KEX_API_SYSCALL, 3 },
    { "sys_semop",       0,  65, KEX_API_SYSCALL, 3 },
    { "sys_semctl",      0,  66, KEX_API_SYSCALL, 4 },
    { "sys_shmdt",       0,  67, KEX_API_SYSCALL, 1 },
    { "sys_msgget",      0,  68, KEX_API_SYSCALL, 2 },
    { "sys_msgsnd",      0,  69, KEX_API_SYSCALL, 4 },
    { "sys_msgrcv",      0,  70, KEX_API_SYSCALL, 5 },
    { "sys_msgctl",      0,  71, KEX_API_SYSCALL, 3 },
    /* 文件控制 */
    { "sys_fcntl",       0,  72, KEX_API_SYSCALL, 3 },
    { "sys_flock",       0,  73, KEX_API_SYSCALL, 2 },
    { "sys_fsync",       0,  74, KEX_API_SYSCALL, 1 },
    { "sys_fdatasync",   0,  75, KEX_API_SYSCALL, 1 },
    { "sys_truncate",    0,  76, KEX_API_SYSCALL, 2 },
    { "sys_ftruncate",   0,  77, KEX_API_SYSCALL, 2 },
    { "sys_getdents",    0,  78, KEX_API_SYSCALL, 3 },
    { "sys_getcwd",      0,  79, KEX_API_SYSCALL, 2 },
    { "sys_chdir",       0,  80, KEX_API_SYSCALL, 1 },
    { "sys_fchdir",      0,  81, KEX_API_SYSCALL, 1 },
    { "sys_rename",      0,  82, KEX_API_SYSCALL, 2 },
    { "sys_mkdir",       0,  83, KEX_API_SYSCALL, 2 },
    { "sys_rmdir",       0,  84, KEX_API_SYSCALL, 1 },
    { "sys_creat",       0,  85, KEX_API_SYSCALL, 2 },
    { "sys_link",        0,  86, KEX_API_SYSCALL, 2 },
    { "sys_unlink",      0,  87, KEX_API_SYSCALL, 1 },
    { "sys_symlink",     0,  88, KEX_API_SYSCALL, 2 },
    { "sys_readlink",    0,  89, KEX_API_SYSCALL, 3 },
    { "sys_chmod",       0,  90, KEX_API_SYSCALL, 2 },
    { "sys_fchmod",      0,  91, KEX_API_SYSCALL, 2 },
    { "sys_chown",       0,  92, KEX_API_SYSCALL, 3 },
    { "sys_fchown",      0,  93, KEX_API_SYSCALL, 3 },
    { "sys_lchown",      0,  94, KEX_API_SYSCALL, 3 },
    { "sys_umask",       0,  95, KEX_API_SYSCALL, 1 },
    { "sys_gettimeofday",0,  96, KEX_API_SYSCALL, 2 },
    { "sys_getrlimit",   0,  97, KEX_API_SYSCALL, 2 },
    { "sys_getrusage",   0,  98, KEX_API_SYSCALL, 2 },
    { "sys_sysinfo",     0,  99, KEX_API_SYSCALL, 1 },
    { "sys_times",       0, 100, KEX_API_SYSCALL, 1 },
    { "sys_ptrace",      0, 101, KEX_API_SYSCALL, 4 },
    { "sys_getuid",      0, 102, KEX_API_SYSCALL, 0 },
    { "sys_syslog",      0, 103, KEX_API_SYSCALL, 3 },
    { "sys_getgid",      0, 104, KEX_API_SYSCALL, 0 },
    { "sys_setuid",      0, 105, KEX_API_SYSCALL, 1 },
    { "sys_setgid",      0, 106, KEX_API_SYSCALL, 1 },
    { "sys_geteuid",     0, 107, KEX_API_SYSCALL, 0 },
    { "sys_getegid",     0, 108, KEX_API_SYSCALL, 0 },
    { "sys_setpgid",     0, 109, KEX_API_SYSCALL, 2 },
    { "sys_getppid",     0, 110, KEX_API_SYSCALL, 0 },
    { "sys_getpgrp",     0, 111, KEX_API_SYSCALL, 0 },
    { "sys_setsid",      0, 112, KEX_API_SYSCALL, 0 },
    { "sys_setreuid",    0, 113, KEX_API_SYSCALL, 2 },
    { "sys_setregid",    0, 114, KEX_API_SYSCALL, 2 },
    { "sys_getgroups",   0, 115, KEX_API_SYSCALL, 2 },
    { "sys_setgroups",   0, 116, KEX_API_SYSCALL, 2 },
    { "sys_setresuid",   0, 117, KEX_API_SYSCALL, 3 },
    { "sys_getresuid",   0, 118, KEX_API_SYSCALL, 3 },
    { "sys_setresgid",   0, 119, KEX_API_SYSCALL, 3 },
    { "sys_getresgid",   0, 120, KEX_API_SYSCALL, 3 },
    { "sys_getpgid",     0, 121, KEX_API_SYSCALL, 1 },
    { "sys_setfsuid",    0, 122, KEX_API_SYSCALL, 1 },
    { "sys_setfsgid",    0, 123, KEX_API_SYSCALL, 1 },
    { "sys_getsid",      0, 124, KEX_API_SYSCALL, 1 },
    { "sys_capget",      0, 125, KEX_API_SYSCALL, 2 },
    { "sys_capset",      0, 126, KEX_API_SYSCALL, 2 },
    { "sys_rt_sigpending",0,127, KEX_API_SYSCALL, 2 },
    { "sys_rt_sigtimedwait",0,128,KEX_API_SYSCALL, 3 },
    { "sys_rt_sigqueueinfo",0,129,KEX_API_SYSCALL, 3 },
    { "sys_rt_sigsuspend",0,130, KEX_API_SYSCALL, 2 },
    { "sys_sigaltstack", 0, 131, KEX_API_SYSCALL, 2 },
    { "sys_utime",       0, 132, KEX_API_SYSCALL, 2 },
    { "sys_mknod",       0, 133, KEX_API_SYSCALL, 3 },
    /* 系统配置 */
    { "sys_personality", 0, 135, KEX_API_SYSCALL, 1 },
    { "sys_ustat",       0, 136, KEX_API_SYSCALL, 2 },
    { "sys_statfs",      0, 137, KEX_API_SYSCALL, 2 },
    { "sys_fstatfs",     0, 138, KEX_API_SYSCALL, 2 },
    { "sys_sysfs",       0, 139, KEX_API_SYSCALL, 3 },
    { "sys_getpriority", 0, 140, KEX_API_SYSCALL, 2 },
    { "sys_setpriority", 0, 141, KEX_API_SYSCALL, 3 },
    { "sys_sched_setparam",    0, 142, KEX_API_SYSCALL, 2 },
    { "sys_sched_getparam",    0, 143, KEX_API_SYSCALL, 2 },
    { "sys_sched_setscheduler",0, 144, KEX_API_SYSCALL, 3 },
    { "sys_sched_getscheduler",0, 145, KEX_API_SYSCALL, 1 },
    { "sys_sched_get_priority_max", 0, 146, KEX_API_SYSCALL, 1 },
    { "sys_sched_get_priority_min", 0, 147, KEX_API_SYSCALL, 1 },
    { "sys_sched_rr_get_interval",  0, 148, KEX_API_SYSCALL, 2 },
    { "sys_mlock",       0, 149, KEX_API_SYSCALL, 2 },
    { "sys_munlock",     0, 150, KEX_API_SYSCALL, 2 },
    { "sys_mlockall",    0, 151, KEX_API_SYSCALL, 1 },
    { "sys_munlockall",  0, 152, KEX_API_SYSCALL, 0 },
    /* 系统/资源 */
    { "sys_prctl",       0, 157, KEX_API_SYSCALL, 5 },
    { "sys_arch_prctl",  0, 158, KEX_API_SYSCALL, 2 },
    { "sys_adjtimex",    0, 159, KEX_API_SYSCALL, 1 },
    { "sys_setrlimit",   0, 160, KEX_API_SYSCALL, 2 },
    { "sys_chroot",      0, 161, KEX_API_SYSCALL, 1 },
    { "sys_sync",        0, 162, KEX_API_SYSCALL, 0 },
    { "sys_acct",        0, 163, KEX_API_SYSCALL, 1 },
    { "sys_settimeofday",0, 164, KEX_API_SYSCALL, 2 },
    { "sys_mount",       0, 165, KEX_API_SYSCALL, 5 },
    { "sys_umount2",     0, 166, KEX_API_SYSCALL, 2 },
    { "sys_swapon",      0, 167, KEX_API_SYSCALL, 2 },
    { "sys_swapoff",     0, 168, KEX_API_SYSCALL, 1 },
    { "sys_reboot",      0, 169, KEX_API_SYSCALL, 4 },
    { "sys_sethostname", 0, 170, KEX_API_SYSCALL, 2 },
    { "sys_setdomainname",0,171, KEX_API_SYSCALL, 2 },
    { "sys_iopl",        0, 172, KEX_API_SYSCALL, 1 },
    { "sys_ioperm",      0, 173, KEX_API_SYSCALL, 3 },
    { "sys_init_module", 0, 175, KEX_API_SYSCALL, 3 },
    { "sys_delete_module",0,176, KEX_API_SYSCALL, 2 },
    { "sys_quotactl",    0, 179, KEX_API_SYSCALL, 4 },
    /* 扩展属性 */
    { "sys_setxattr",    0, 188, KEX_API_SYSCALL, 5 },
    { "sys_lsetxattr",   0, 189, KEX_API_SYSCALL, 5 },
    { "sys_fsetxattr",   0, 190, KEX_API_SYSCALL, 5 },
    { "sys_getxattr",    0, 191, KEX_API_SYSCALL, 4 },
    { "sys_lgetxattr",   0, 192, KEX_API_SYSCALL, 4 },
    { "sys_fgetxattr",   0, 193, KEX_API_SYSCALL, 4 },
    { "sys_listxattr",   0, 194, KEX_API_SYSCALL, 3 },
    { "sys_llistxattr",  0, 195, KEX_API_SYSCALL, 3 },
    { "sys_flistxattr",  0, 196, KEX_API_SYSCALL, 3 },
    { "sys_removexattr", 0, 197, KEX_API_SYSCALL, 2 },
    { "sys_lremovexattr",0, 198, KEX_API_SYSCALL, 2 },
    { "sys_fremovexattr",0, 199, KEX_API_SYSCALL, 2 },
    /* 进程/线程扩展 */
    { "sys_gettid",      0, 186, KEX_API_SYSCALL, 0 },
    { "sys_readahead",   0, 187, KEX_API_SYSCALL, 3 },
    { "sys_tkill",       0, 200, KEX_API_SYSCALL, 2 },
    { "sys_time",        0, 201, KEX_API_SYSCALL, 1 },
    { "sys_futex",       0, 202, KEX_API_SYSCALL, 6 },
    { "sys_sched_setaffinity", 0, 203, KEX_API_SYSCALL, 3 },
    { "sys_sched_getaffinity", 0, 204, KEX_API_SYSCALL, 3 },
    /* IO */
    { "sys_io_setup",    0, 206, KEX_API_SYSCALL, 2 },
    { "sys_io_destroy",  0, 207, KEX_API_SYSCALL, 1 },
    { "sys_io_getevents",0, 208, KEX_API_SYSCALL, 5 },
    { "sys_io_submit",   0, 209, KEX_API_SYSCALL, 3 },
    { "sys_io_cancel",   0, 210, KEX_API_SYSCALL, 3 },
    /* epoll */
    { "sys_epoll_create",0, 213, KEX_API_SYSCALL, 1 },
    { "sys_epoll_ctl_old",0,214, KEX_API_SYSCALL, 4 },
    { "sys_epoll_wait_old",0,215,KEX_API_SYSCALL, 4 },
    { "sys_remap_file_pages",0,216,KEX_API_SYSCALL, 5 },
    { "sys_getdents64",  0, 217, KEX_API_SYSCALL, 3 },
    { "sys_set_tid_address",0,218,KEX_API_SYSCALL, 1 },
    { "sys_restart_syscall",0,219,KEX_API_SYSCALL, 0 },
    { "sys_semtimedop",  0, 220, KEX_API_SYSCALL, 4 },
    { "sys_fadvise64",   0, 221, KEX_API_SYSCALL, 4 },
    /* 定时器 */
    { "sys_timer_create",0, 222, KEX_API_SYSCALL, 3 },
    { "sys_timer_settime",0,223, KEX_API_SYSCALL, 4 },
    { "sys_timer_gettime",0,224, KEX_API_SYSCALL, 2 },
    { "sys_timer_getoverrun",0,225,KEX_API_SYSCALL,1 },
    { "sys_timer_delete",0, 226, KEX_API_SYSCALL, 1 },
    /* 时钟 */
    { "sys_clock_settime",  0, 227, KEX_API_SYSCALL, 2 },
    { "sys_clock_gettime",  0, 228, KEX_API_SYSCALL, 2 },
    { "sys_clock_getres",   0, 229, KEX_API_SYSCALL, 2 },
    { "sys_clock_nanosleep",0, 230, KEX_API_SYSCALL, 4 },
    /* 退出/信号 */
    { "sys_exit_group",  0, 231, KEX_API_SYSCALL, 1 },
    { "sys_epoll_wait",  0, 232, KEX_API_SYSCALL, 4 },
    { "sys_epoll_ctl",   0, 233, KEX_API_SYSCALL, 4 },
    { "sys_tgkill",      0, 234, KEX_API_SYSCALL, 3 },
    { "sys_utimes",      0, 235, KEX_API_SYSCALL, 2 },
    /* 进程 */
    { "sys_waitid",      0, 247, KEX_API_SYSCALL, 5 },
    /* 消息队列 */
    { "sys_mq_open",     0, 240, KEX_API_SYSCALL, 4 },
    { "sys_mq_unlink",   0, 241, KEX_API_SYSCALL, 1 },
    { "sys_mq_timedsend",0, 242, KEX_API_SYSCALL, 5 },
    { "sys_mq_timedreceive",0,243,KEX_API_SYSCALL, 5 },
    { "sys_mq_notify",   0, 244, KEX_API_SYSCALL, 2 },
    { "sys_mq_getsetattr",0,245, KEX_API_SYSCALL, 3 },
    /* kexec */
    { "sys_kexec_load",  0, 246, KEX_API_SYSCALL, 5 },
    /* 密钥 */
    { "sys_add_key",     0, 248, KEX_API_SYSCALL, 5 },
    { "sys_request_key", 0, 249, KEX_API_SYSCALL, 4 },
    { "sys_keyctl",      0, 250, KEX_API_SYSCALL, 5 },
    /* IO 优先级 */
    { "sys_ioprio_set",  0, 251, KEX_API_SYSCALL, 3 },
    { "sys_ioprio_get",  0, 252, KEX_API_SYSCALL, 2 },
    /* inotify */
    { "sys_inotify_init",0, 253, KEX_API_SYSCALL, 0 },
    { "sys_inotify_add_watch",0,254,KEX_API_SYSCALL,3 },
    { "sys_inotify_rm_watch",0,255,KEX_API_SYSCALL,2 },
    /* *at 系列 */
    { "sys_openat",      0, 257, KEX_API_SYSCALL, 4 },
    { "sys_mkdirat",     0, 258, KEX_API_SYSCALL, 3 },
    { "sys_mknodat",     0, 259, KEX_API_SYSCALL, 4 },
    { "sys_fchownat",    0, 260, KEX_API_SYSCALL, 5 },
    { "sys_futimesat",   0, 261, KEX_API_SYSCALL, 3 },
    { "sys_newfstatat",  0, 262, KEX_API_SYSCALL, 4 },
    { "sys_unlinkat",    0, 263, KEX_API_SYSCALL, 3 },
    { "sys_renameat",    0, 264, KEX_API_SYSCALL, 4 },
    { "sys_linkat",      0, 265, KEX_API_SYSCALL, 5 },
    { "sys_symlinkat",   0, 266, KEX_API_SYSCALL, 3 },
    { "sys_readlinkat",  0, 267, KEX_API_SYSCALL, 4 },
    { "sys_fchmodat",    0, 268, KEX_API_SYSCALL, 3 },
    { "sys_faccessat",   0, 269, KEX_API_SYSCALL, 3 },
    { "sys_pselect6",    0, 270, KEX_API_SYSCALL, 6 },
    { "sys_ppoll",       0, 271, KEX_API_SYSCALL, 4 },
    { "sys_unshare",     0, 272, KEX_API_SYSCALL, 1 },
    { "sys_set_robust_list",0,273,KEX_API_SYSCALL, 2 },
    { "sys_get_robust_list",0,274,KEX_API_SYSCALL, 3 },
    { "sys_splice",      0, 275, KEX_API_SYSCALL, 6 },
    { "sys_tee",         0, 276, KEX_API_SYSCALL, 4 },
    { "sys_sync_file_range",0,277,KEX_API_SYSCALL, 4 },
    { "sys_vmsplice",    0, 278, KEX_API_SYSCALL, 4 },
    { "sys_move_pages",  0, 279, KEX_API_SYSCALL, 6 },
    { "sys_utimensat",   0, 280, KEX_API_SYSCALL, 4 },
    { "sys_epoll_pwait", 0, 281, KEX_API_SYSCALL, 5 },
    { "sys_signalfd",    0, 282, KEX_API_SYSCALL, 3 },
    { "sys_timerfd_create",0,283,KEX_API_SYSCALL, 2 },
    { "sys_eventfd",     0, 284, KEX_API_SYSCALL, 1 },
    { "sys_fallocate",   0, 285, KEX_API_SYSCALL, 4 },
    { "sys_timerfd_settime",0,286,KEX_API_SYSCALL, 4 },
    { "sys_timerfd_gettime",0,287,KEX_API_SYSCALL, 2 },
    { "sys_accept4",     0, 288, KEX_API_SYSCALL, 4 },
    { "sys_signalfd4",   0, 289, KEX_API_SYSCALL, 4 },
    { "sys_eventfd2",    0, 290, KEX_API_SYSCALL, 2 },
    { "sys_epoll_create1",0,291,KEX_API_SYSCALL, 1 },
    { "sys_dup3",        0, 292, KEX_API_SYSCALL, 3 },
    { "sys_pipe2",       0, 293, KEX_API_SYSCALL, 2 },
    { "sys_inotify_init1",0,294,KEX_API_SYSCALL, 1 },
    { "sys_preadv",      0, 295, KEX_API_SYSCALL, 5 },
    { "sys_pwritev",     0, 296, KEX_API_SYSCALL, 5 },
    { "sys_rt_tgsigqueueinfo",0,297,KEX_API_SYSCALL,4 },
    { "sys_perf_event_open",0,298,KEX_API_SYSCALL, 5 },
    { "sys_recvmmsg",    0, 299, KEX_API_SYSCALL, 5 },
    { "sys_fanotify_init",0,300,KEX_API_SYSCALL, 2 },
    { "sys_fanotify_mark",0,301,KEX_API_SYSCALL, 5 },
    { "sys_prlimit64",   0, 302, KEX_API_SYSCALL, 4 },
    { "sys_name_to_handle_at",0,303,KEX_API_SYSCALL,3 },
    { "sys_open_by_handle_at",0,304,KEX_API_SYSCALL,3 },
    { "sys_clock_adjtime",0,305,KEX_API_SYSCALL, 2 },
    { "sys_syncfs",      0, 306, KEX_API_SYSCALL, 1 },
    { "sys_sendmmsg",    0, 307, KEX_API_SYSCALL, 4 },
    { "sys_setns",       0, 308, KEX_API_SYSCALL, 2 },
    { "sys_getcpu",      0, 309, KEX_API_SYSCALL, 3 },
    { "sys_process_vm_readv",0,310,KEX_API_SYSCALL, 6 },
    { "sys_process_vm_writev",0,311,KEX_API_SYSCALL, 6 },
    { "sys_kcmp",        0, 312, KEX_API_SYSCALL, 5 },
    { "sys_finit_module",0, 313, KEX_API_SYSCALL, 3 },
    { "sys_sched_setattr",0,314,KEX_API_SYSCALL, 3 },
    { "sys_sched_getattr",0,315,KEX_API_SYSCALL, 4 },
    { "sys_renameat2",   0, 316, KEX_API_SYSCALL, 5 },
    { "sys_seccomp",     0, 317, KEX_API_SYSCALL, 3 },
    { "sys_getrandom",   0, 318, KEX_API_SYSCALL, 3 },
    { "sys_memfd_create",0, 319, KEX_API_SYSCALL, 2 },
    { "sys_kexec_file_load",0,320,KEX_API_SYSCALL, 5 },
    { "sys_bpf",         0, 321, KEX_API_SYSCALL, 3 },
    { "sys_execveat",    0, 322, KEX_API_SYSCALL, 5 },
    { "sys_userfaultfd", 0, 323, KEX_API_SYSCALL, 1 },
    { "sys_membarrier",  0, 324, KEX_API_SYSCALL, 2 },
    { "sys_mlock2",      0, 325, KEX_API_SYSCALL, 3 },
    { "sys_copy_file_range",0,326,KEX_API_SYSCALL, 6 },
    { "sys_preadv2",     0, 327, KEX_API_SYSCALL, 6 },
    { "sys_pwritev2",    0, 328, KEX_API_SYSCALL, 6 },
    { "sys_pkey_mprotect",0,329,KEX_API_SYSCALL, 4 },
    { "sys_pkey_alloc",  0, 330, KEX_API_SYSCALL, 2 },
    { "sys_pkey_free",   0, 331, KEX_API_SYSCALL, 1 },
    { "sys_statx",       0, 332, KEX_API_SYSCALL, 5 },
    /* io_uring */
    { "sys_io_uring_setup", 0, 425, KEX_API_SYSCALL, 2 },
    { "sys_io_uring_enter", 0, 426, KEX_API_SYSCALL, 6 },
    { "sys_io_uring_register",0,427,KEX_API_SYSCALL, 4 },
    /* 新系统调用 */
    { "sys_openat2",     0, 437, KEX_API_SYSCALL, 4 },
    { "sys_clone3",      0, 435, KEX_API_SYSCALL, 2 },
    { "sys_close_range", 0, 436, KEX_API_SYSCALL, 3 },
};

/* Kenux 扩展 syscall 451-500 */
static kex_api_entry_t kex_kenux_apis[] = {
    { "kenux_info",              0, 451, KEX_API_KENUX, 1 },
    { "kenux_debug",             0, 452, KEX_API_KENUX, 1 },
    { "kenux_get_version",       0, 453, KEX_API_KENUX, 1 },
    { "kenux_get_uptime",        0, 454, KEX_API_KENUX, 1 },
    { "kenux_get_loadavg",       0, 455, KEX_API_KENUX, 2 },
    { "kenux_reboot",            0, 456, KEX_API_KENUX, 0 },
    { "kenux_poweroff",          0, 457, KEX_API_KENUX, 0 },
    { "kenux_halt",              0, 458, KEX_API_KENUX, 0 },
    { "kenux_get_cpu_count",     0, 459, KEX_API_KENUX, 0 },
    { "kenux_get_cpu_info",      0, 460, KEX_API_KENUX, 2 },
    { "kenux_set_affinity",      0, 461, KEX_API_KENUX, 2 },
    { "kenux_get_affinity",      0, 462, KEX_API_KENUX, 2 },
    { "kenux_create_namespace",  0, 463, KEX_API_KENUX, 1 },
    { "kenux_enter_namespace",   0, 464, KEX_API_KENUX, 1 },
    { "kenux_get_namespace",     0, 465, KEX_API_KENUX, 2 },
    { "kenux_vmspace_create",    0, 466, KEX_API_KENUX, 0 },
    { "kenux_vmspace_destroy",   0, 467, KEX_API_KENUX, 1 },
    { "kenux_vmspace_switch",    0, 468, KEX_API_KENUX, 1 },
    { "kenux_iommu_map",         0, 469, KEX_API_KENUX, 4 },
    { "kenux_iommu_unmap",       0, 470, KEX_API_KENUX, 2 },
    { "kenux_dma_alloc",         0, 471, KEX_API_KENUX, 2 },
    { "kenux_dma_free",          0, 472, KEX_API_KENUX, 2 },
    { "kenux_pci_read",          0, 473, KEX_API_KENUX, 4 },
    { "kenux_pci_write",         0, 474, KEX_API_KENUX, 4 },
    { "kenux_pci_enum",          0, 475, KEX_API_KENUX, 2 },
    { "kenux_acpi_query",        0, 476, KEX_API_KENUX, 2 },
    { "kenux_smbios_get",        0, 477, KEX_API_KENUX, 2 },
    { "kenux_fb_get_info",       0, 478, KEX_API_KENUX, 1 },
    { "kenux_fb_map",            0, 479, KEX_API_KENUX, 1 },
    { "kenux_fb_unmap",          0, 480, KEX_API_KENUX, 1 },
    { "kenux_fb_flip",           0, 481, KEX_API_KENUX, 1 },
    { "kenux_gpu_submit",        0, 482, KEX_API_KENUX, 3 },
    { "kenux_gpu_wait",          0, 483, KEX_API_KENUX, 1 },
    { "kenux_net_attach",        0, 484, KEX_API_KENUX, 2 },
    { "kenux_net_detach",        0, 485, KEX_API_KENUX, 1 },
    { "kenux_net_ioctl",         0, 486, KEX_API_KENUX, 3 },
    { "kenux_fs_snapshot",       0, 487, KEX_API_KENUX, 2 },
    { "kenux_fs_rollback",       0, 488, KEX_API_KENUX, 2 },
    { "kenux_fs_compress",       0, 489, KEX_API_KENUX, 3 },
    { "kenux_fs_encrypt",        0, 490, KEX_API_KENUX, 3 },
    { "kenux_audit_log",         0, 491, KEX_API_KENUX, 3 },
    { "kenux_audit_config",      0, 492, KEX_API_KENUX, 2 },
    { "kenux_seccomp_install",   0, 493, KEX_API_KENUX, 2 },
    { "kenux_seccomp_filter",    0, 494, KEX_API_KENUX, 3 },
    { "kenux_trace_attach",      0, 495, KEX_API_KENUX, 2 },
    { "kenux_trace_detach",      0, 496, KEX_API_KENUX, 1 },
    { "kenux_trace_read",        0, 497, KEX_API_KENUX, 3 },
    { "kenux_trace_write",       0, 498, KEX_API_KENUX, 3 },
    { "kenux_kprobe_register",   0, 499, KEX_API_KENUX, 2 },
    { "kenux_kprobe_unregister", 0, 500, KEX_API_KENUX, 1 },
};

/* 哈希表 (CRC32 -> API 条目), 用于 O(1) 查找 */
#define KEX_API_HASH_SIZE 1024
static kex_api_entry_t *g_hash_table[KEX_API_HASH_SIZE];
static int table_inited = 0;

/* 合并视图: 指向 std_apis 和 kenux_apis 的指针数组, 供 kex_api_table() 使用 */
static const kex_api_entry_t *g_all_apis[
    sizeof(kex_std_apis) / sizeof(kex_std_apis[0]) +
    sizeof(kex_kenux_apis) / sizeof(kex_kenux_apis[0])
];
static size_t g_all_apis_count = 0;

/* 计算 CRC32 在哈希表中的槽位 (简单取模) */
static size_t hash_slot(uint32_t crc32) {
    /* 将 32 位 CRC 均匀分布到哈希表 */
    return (crc32 ^ (crc32 >> 16)) % KEX_API_HASH_SIZE;
}

static void ensure_init(void) {
    if (table_inited) return;

    /* 填充 CRC32 字段 */
    size_t n1 = sizeof(kex_std_apis) / sizeof(kex_std_apis[0]);
    size_t n2 = sizeof(kex_kenux_apis) / sizeof(kex_kenux_apis[0]);
    for (size_t i = 0; i < n1; i++)
        ((kex_api_entry_t *)kex_std_apis)[i].crc32 = kex_crc32_str(kex_std_apis[i].name);
    for (size_t i = 0; i < n2; i++)
        ((kex_api_entry_t *)kex_kenux_apis)[i].crc32 = kex_crc32_str(kex_kenux_apis[i].name);

    /* 构建哈希表 (开放寻址法, 条目数远小于哈希表大小) */
    memset(g_hash_table, 0, sizeof(g_hash_table));
    g_all_apis_count = 0;
    for (size_t i = 0; i < n1; i++) {
        size_t slot = hash_slot(kex_std_apis[i].crc32);
        while (g_hash_table[slot] != NULL) {
            slot = (slot + 1) % KEX_API_HASH_SIZE;
        }
        g_hash_table[slot] = &((kex_api_entry_t *)kex_std_apis)[i];
        g_all_apis[g_all_apis_count++] = &kex_std_apis[i];
    }
    for (size_t i = 0; i < n2; i++) {
        size_t slot = hash_slot(kex_kenux_apis[i].crc32);
        while (g_hash_table[slot] != NULL) {
            slot = (slot + 1) % KEX_API_HASH_SIZE;
        }
        g_hash_table[slot] = &((kex_api_entry_t *)kex_kenux_apis)[i];
        g_all_apis[g_all_apis_count++] = &kex_kenux_apis[i];
    }

    table_inited = 1;
}

const kex_api_entry_t *kex_api_lookup_name(const char *name) {
    ensure_init();
    size_t n1 = sizeof(kex_std_apis) / sizeof(kex_std_apis[0]);
    size_t n2 = sizeof(kex_kenux_apis) / sizeof(kex_kenux_apis[0]);
    for (size_t i = 0; i < n1; i++)
        if (strcmp(kex_std_apis[i].name, name) == 0) return &kex_std_apis[i];
    for (size_t i = 0; i < n2; i++)
        if (strcmp(kex_kenux_apis[i].name, name) == 0) return &kex_kenux_apis[i];
    return NULL;
}

/* 按 CRC32 查找 (哈希表加速, O(1) 平均) */
const kex_api_entry_t *kex_api_lookup_crc32(uint32_t crc32) {
    ensure_init();
    size_t slot = hash_slot(crc32);
    /* 开放寻址: 最多探测 KEX_API_HASH_SIZE 次 */
    for (size_t i = 0; i < KEX_API_HASH_SIZE; i++) {
        kex_api_entry_t *e = g_hash_table[slot];
        if (e == NULL) return NULL;  /* 空槽 = 未找到 */
        if (e->crc32 == crc32) return e;
        slot = (slot + 1) % KEX_API_HASH_SIZE;
    }
    return NULL;
}

const kex_api_entry_t *kex_api_table(size_t *count) {
    ensure_init();
    /* 返回标准表; Kenux 表需用 kex_api_table_all() 获取 */
    if (count) *count = sizeof(kex_std_apis) / sizeof(kex_std_apis[0]);
    return kex_std_apis;
}

const kex_api_entry_t * const *kex_api_table_all(size_t *count) {
    ensure_init();
    if (count) *count = g_all_apis_count;
    return g_all_apis;
}

uint32_t kex_api_crc32(const char *name) {
    return kex_crc32_str(name);
}
