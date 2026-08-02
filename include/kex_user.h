/*
 * kex_user.h - KEX 用户程序 API (对标 KenuxOS kapi_syscall.h)
 *
 * 用户程序 #include 此头即可调用 Kenux 系统调用.
 * 编译为 .kex 时, 这些调用内联为 syscall 机器码.
 *
 * x86_64 syscall 调用约定:
 *   syscall 号: rax
 *   参数:       rdi, rsi, rdx, r10, r8, r9
 *   返回值:     rax
 *   clobber:    rcx, r11
 */
#ifndef KEX_USER_H
#define KEX_USER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * syscall 编号 (与 KenuxOS kapi_syscall.h / Linux x86_64 一致)
 * ======================================================================== */

/* --- 文件 I/O --- */
#define SYS_read                0
#define SYS_write               1
#define SYS_open                2
#define SYS_close               3
#define SYS_stat                4
#define SYS_fstat               5
#define SYS_lstat               6
#define SYS_poll                7
#define SYS_lseek               8
#define SYS_mmap                9
#define SYS_mprotect            10
#define SYS_munmap              11
#define SYS_brk                 12
#define SYS_rt_sigaction        13
#define SYS_rt_sigprocmask      14
#define SYS_rt_sigreturn        15
#define SYS_ioctl               16
#define SYS_pread64             17
#define SYS_pwrite64            18
#define SYS_readv               19
#define SYS_writev              20
#define SYS_access              21
#define SYS_pipe                22
#define SYS_select              23
#define SYS_sched_yield         24
#define SYS_mremap              25
#define SYS_msync               26
#define SYS_mincore             27
#define SYS_madvise             28
#define SYS_shmget              29
#define SYS_shmat               30
#define SYS_shmctl              31
#define SYS_dup                 32
#define SYS_dup2                33
#define SYS_pause               34
#define SYS_nanosleep           35
#define SYS_getitimer           36
#define SYS_alarm               37
#define SYS_setitimer           38
#define SYS_getpid              39
#define SYS_sendfile            40
#define SYS_socket              41
#define SYS_connect             42
#define SYS_accept              43
#define SYS_sendto              44
#define SYS_recvfrom            45
#define SYS_sendmsg             46
#define SYS_recvmsg             47
#define SYS_shutdown            48
#define SYS_bind                49
#define SYS_listen              50
#define SYS_getsockname         51
#define SYS_getpeername         52
#define SYS_socketpair          53
#define SYS_setsockopt          54
#define SYS_getsockopt          55
#define SYS_clone               56
#define SYS_fork                57
#define SYS_vfork               58
#define SYS_execve              59
#define SYS_exit                60
#define SYS_wait4               61
#define SYS_kill                62
#define SYS_uname               63
#define SYS_semget              64
#define SYS_semop               65
#define SYS_semctl              66
#define SYS_shmdt               67
#define SYS_msgget              68
#define SYS_msgsnd              69
#define SYS_msgrcv              70
#define SYS_msgctl              71
#define SYS_fcntl               72
#define SYS_flock               73
#define SYS_fsync               74
#define SYS_fdatasync           75
#define SYS_truncate            76
#define SYS_ftruncate           77
#define SYS_getdents            78
#define SYS_getcwd              79
#define SYS_chdir               80
#define SYS_fchdir              81
#define SYS_rename              82
#define SYS_mkdir               83
#define SYS_rmdir               84
#define SYS_creat               85
#define SYS_link                86
#define SYS_unlink              87
#define SYS_symlink             88
#define SYS_readlink            89
#define SYS_chmod               90
#define SYS_fchmod              91
#define SYS_chown               92
#define SYS_fchown              93
#define SYS_lchown              94
#define SYS_umask               95
#define SYS_gettimeofday        96
#define SYS_getrlimit           97
#define SYS_getrusage           98
#define SYS_sysinfo             99
#define SYS_times               100
#define SYS_ptrace              101
#define SYS_getuid              102
#define SYS_syslog              103
#define SYS_getgid              104
#define SYS_setuid              105
#define SYS_setgid              106
#define SYS_geteuid             107
#define SYS_getegid             108
#define SYS_setpgid             109
#define SYS_getppid             110
#define SYS_getpgrp             111
#define SYS_setsid              112
#define SYS_setreuid            113
#define SYS_setregid            114
#define SYS_getgroups           115
#define SYS_setgroups           116
#define SYS_setresuid           117
#define SYS_getresuid           118
#define SYS_setresgid           119
#define SYS_getresgid           120
#define SYS_getpgid             121
#define SYS_setfsuid            122
#define SYS_setfsgid            123
#define SYS_getsid              124
#define SYS_capget              125
#define SYS_capset              126
#define SYS_rt_sigpending       127
#define SYS_rt_sigtimedwait     128
#define SYS_rt_sigqueueinfo     129
#define SYS_rt_sigsuspend       130
#define SYS_sigaltstack         131
#define SYS_utime               132
#define SYS_mknod               133
#define SYS_uselib              134
#define SYS_personality         135
#define SYS_ustat               136
#define SYS_statfs              137
#define SYS_fstatfs             138
#define SYS_sysfs               139
#define SYS_getpriority         140
#define SYS_setpriority         141
#define SYS_sched_setparam      142
#define SYS_sched_getparam      143
#define SYS_sched_setscheduler  144
#define SYS_sched_getscheduler  145
#define SYS_sched_get_priority_max 146
#define SYS_sched_get_priority_min 147
#define SYS_sched_rr_get_interval 148
#define SYS_mlock               149
#define SYS_munlock             150
#define SYS_mlockall            151
#define SYS_munlockall          152
#define SYS_vhangup             153
#define SYS_modify_ldt          154
#define SYS_pivot_root          155
#define SYS__sysctl             156
#define SYS_prctl               157
#define SYS_arch_prctl          158
#define SYS_adjtimex            159
#define SYS_setrlimit           160
#define SYS_chroot              161
#define SYS_sync                162
#define SYS_acct                163
#define SYS_settimeofday        164
#define SYS_mount               165
#define SYS_umount2             166
#define SYS_swapon              167
#define SYS_swapoff             168
#define SYS_reboot              169
#define SYS_sethostname         170
#define SYS_setdomainname       171
#define SYS_iopl                172
#define SYS_ioperm              173
#define SYS_create_module       174
#define SYS_init_module         175
#define SYS_delete_module       176
#define SYS_get_kernel_syms     177
#define SYS_query_module        178
#define SYS_quotactl            179
#define SYS_nfsservctl          180
#define SYS_getpmsg             181
#define SYS_putpmsg             182
#define SYS_afs_syscall         183
#define SYS_tuxcall             184
#define SYS_security            185
#define SYS_gettid              186
#define SYS_readahead           187
#define SYS_setxattr            188
#define SYS_lsetxattr           189
#define SYS_fsetxattr           190
#define SYS_getxattr            191
#define SYS_lgetxattr           192
#define SYS_fgetxattr           193
#define SYS_listxattr           194
#define SYS_llistxattr          195
#define SYS_flistxattr          196
#define SYS_removexattr         197
#define SYS_lremovexattr        198
#define SYS_fremovexattr        199
#define SYS_tkill               200
#define SYS_time                201
#define SYS_futex               202
#define SYS_sched_setaffinity   203
#define SYS_sched_getaffinity   204
#define SYS_set_thread_area     205
#define SYS_io_setup            206
#define SYS_io_destroy          207
#define SYS_io_getevents        208
#define SYS_io_submit           209
#define SYS_io_cancel           210
#define SYS_get_thread_area     211
#define SYS_lookup_dcookie      212
#define SYS_epoll_create        213
#define SYS_epoll_ctl_old       214
#define SYS_epoll_wait_old      215
#define SYS_remap_file_pages    216
#define SYS_getdents64          217
#define SYS_set_tid_address     218
#define SYS_restart_syscall     219
#define SYS_semtimedop          220
#define SYS_fadvise64           221
#define SYS_timer_create        222
#define SYS_timer_settime       223
#define SYS_timer_gettime       224
#define SYS_timer_getoverrun    225
#define SYS_timer_delete        226
#define SYS_clock_settime       227
#define SYS_clock_gettime       228
#define SYS_clock_getres        229
#define SYS_clock_nanosleep     230
#define SYS_exit_group          231
#define SYS_epoll_wait          232
#define SYS_epoll_ctl           233
#define SYS_tgkill              234
#define SYS_utimes              235
#define SYS_vserver             236
#define SYS_mbind               237
#define SYS_set_mempolicy       238
#define SYS_get_mempolicy       239
#define SYS_mq_open             240
#define SYS_mq_unlink           241
#define SYS_mq_timedsend        242
#define SYS_mq_timedreceive     243
#define SYS_mq_notify           244
#define SYS_mq_getsetattr       245
#define SYS_kexec_load          246
#define SYS_waitid              247
#define SYS_add_key             248
#define SYS_request_key         249
#define SYS_keyctl              250
#define SYS_ioprio_set          251
#define SYS_ioprio_get          252
#define SYS_inotify_init        253
#define SYS_inotify_add_watch   254
#define SYS_inotify_rm_watch    255
#define SYS_migrate_pages       256
#define SYS_openat              257
#define SYS_mkdirat             258
#define SYS_mknodat             259
#define SYS_fchownat            260
#define SYS_futimesat           261
#define SYS_newfstatat          262
#define SYS_unlinkat            263
#define SYS_renameat            264
#define SYS_linkat              265
#define SYS_symlinkat           266
#define SYS_readlinkat          267
#define SYS_fchmodat            268
#define SYS_faccessat           269
#define SYS_pselect6            270
#define SYS_ppoll               271
#define SYS_unshare             272
#define SYS_set_robust_list     273
#define SYS_get_robust_list     274
#define SYS_splice              275
#define SYS_tee                 276
#define SYS_sync_file_range     277
#define SYS_vmsplice            278
#define SYS_move_pages          279
#define SYS_utimensat           280
#define SYS_epoll_pwait         281
#define SYS_signalfd            282
#define SYS_timerfd_create      283
#define SYS_eventfd             284
#define SYS_fallocate           285
#define SYS_timerfd_settime     286
#define SYS_timerfd_gettime     287
#define SYS_accept4             288
#define SYS_signalfd4           289
#define SYS_eventfd2            290
#define SYS_epoll_create1       291
#define SYS_dup3                292
#define SYS_pipe2               293
#define SYS_inotify_init1       294
#define SYS_preadv              295
#define SYS_pwritev             296
#define SYS_rt_tgsigqueueinfo   297
#define SYS_perf_event_open     298
#define SYS_recvmmsg            299
#define SYS_fanotify_init       300
#define SYS_fanotify_mark       301
#define SYS_prlimit64           302
#define SYS_name_to_handle_at   303
#define SYS_open_by_handle_at   304
#define SYS_clock_adjtime       305
#define SYS_syncfs              306
#define SYS_sendmmsg            307
#define SYS_setns               308
#define SYS_getcpu              309
#define SYS_process_vm_readv    310
#define SYS_process_vm_writev   311
#define SYS_kcmp                312
#define SYS_finit_module        313
#define SYS_sched_setattr       314
#define SYS_sched_getattr       315
#define SYS_renameat2           316
#define SYS_seccomp             317
#define SYS_getrandom           318
#define SYS_memfd_create        319
#define SYS_kexec_file_load     320
#define SYS_bpf                 321
#define SYS_execveat            322
#define SYS_userfaultfd         323
#define SYS_membarrier          324
#define SYS_mlock2              325
#define SYS_copy_file_range     326
#define SYS_preadv2             327
#define SYS_pwritev2            328
#define SYS_pkey_mprotect       329
#define SYS_pkey_alloc          330
#define SYS_pkey_free           331
#define SYS_statx               332
#define SYS_io_uring_setup      425
#define SYS_io_uring_enter      426
#define SYS_io_uring_register   427
#define SYS_openat2             437
#define SYS_clone3              435
#define SYS_close_range         436

/* --- 新增标准 syscall (333-450, 对齐 KenuxOS kapi_syscall.h) --- */
#define SYS_io_pgetevents           333
#define SYS_rseq                    334
#define SYS_kexec_file_load2        335
#define SYS_pidfd_send_signal       424
#define SYS_open_tree               428
#define SYS_move_mount              429
#define SYS_fsopen                  430
#define SYS_fsconfig                431
#define SYS_fsmount                 432
#define SYS_fspick                  433
#define SYS_pidfd_open              434
#define SYS_pidfd_getfd             438
#define SYS_faccessat2              439
#define SYS_process_madvise         440
#define SYS_epoll_pwait2            441
#define SYS_mount_setattr           442
#define SYS_quotactl_fd             443
#define SYS_landlock_create_ruleset 444
#define SYS_landlock_add_rule       445
#define SYS_landlock_restrict_self  446
#define SYS_memfd_secret            447
#define SYS_process_mrelease        448
#define SYS_futex_waitv             449
#define SYS_set_mempolicy_home_node 450

/* --- Kenux 扩展 syscall (451-500) --- */
#define SYS_kenux_info              451
#define SYS_kenux_debug             452
#define SYS_kenux_get_version       453
#define SYS_kenux_get_uptime        454
#define SYS_kenux_get_loadavg       455
#define SYS_kenux_reboot            456
#define SYS_kenux_poweroff          457
#define SYS_kenux_halt              458
#define SYS_kenux_get_cpu_count     459
#define SYS_kenux_get_cpu_info      460
#define SYS_kenux_set_affinity      461
#define SYS_kenux_get_affinity      462
#define SYS_kenux_create_namespace  463
#define SYS_kenux_enter_namespace   464
#define SYS_kenux_get_namespace     465
#define SYS_kenux_vmspace_create    466
#define SYS_kenux_vmspace_destroy   467
#define SYS_kenux_vmspace_switch    468
#define SYS_kenux_iommu_map         469
#define SYS_kenux_iommu_unmap       470
#define SYS_kenux_dma_alloc         471
#define SYS_kenux_dma_free          472
#define SYS_kenux_pci_read          473
#define SYS_kenux_pci_write         474
#define SYS_kenux_pci_enum          475
#define SYS_kenux_acpi_query        476
#define SYS_kenux_smbios_get        477
#define SYS_kenux_fb_get_info       478
#define SYS_kenux_fb_map            479
#define SYS_kenux_fb_unmap          480
#define SYS_kenux_fb_flip           481
#define SYS_kenux_gpu_submit        482
#define SYS_kenux_gpu_wait          483
#define SYS_kenux_net_attach        484
#define SYS_kenux_net_detach        485
#define SYS_kenux_net_ioctl         486
#define SYS_kenux_fs_snapshot       487
#define SYS_kenux_fs_rollback       488
#define SYS_kenux_fs_compress       489
#define SYS_kenux_fs_encrypt        490
#define SYS_kenux_audit_log         491
#define SYS_kenux_audit_config      492
#define SYS_kenux_seccomp_install   493
#define SYS_kenux_seccomp_filter    494
#define SYS_kenux_trace_attach      495
#define SYS_kenux_trace_detach      496
#define SYS_kenux_trace_read        497
#define SYS_kenux_trace_write       498
#define SYS_kenux_kprobe_register   499
#define SYS_kenux_kprobe_unregister 500

/* ========================================================================
 * 内联 syscall 包装 (0-6 参数)
 * 关键: 必须强制内联, 否则 -O2 下编译器可能不内联 static inline,
 * 导致参数通过栈传递, 破坏 syscall 寄存器约定.
 * ======================================================================== */
#define KEX_ALWAYS_INLINE __attribute__((always_inline)) static inline

KEX_ALWAYS_INLINE long kex_syscall0(long nr) {
    long ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(nr) : "rcx","r11","memory");
    return ret;
}
KEX_ALWAYS_INLINE long kex_syscall1(long nr, long a1) {
    long ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(nr), "D"(a1) : "rcx","r11","memory");
    return ret;
}
KEX_ALWAYS_INLINE long kex_syscall2(long nr, long a1, long a2) {
    long ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(nr), "D"(a1), "S"(a2) : "rcx","r11","memory");
    return ret;
}
KEX_ALWAYS_INLINE long kex_syscall3(long nr, long a1, long a2, long a3) {
    long ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(nr), "D"(a1), "S"(a2), "d"(a3) : "rcx","r11","memory");
    return ret;
}
KEX_ALWAYS_INLINE long kex_syscall4(long nr, long a1, long a2, long a3, long a4) {
    long ret;
    register long r10 __asm__("r10") = a4;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(nr), "D"(a1), "S"(a2), "d"(a3), "r"(r10) : "rcx","r11","memory");
    return ret;
}
KEX_ALWAYS_INLINE long kex_syscall5(long nr, long a1, long a2, long a3, long a4, long a5) {
    long ret;
    register long r10 __asm__("r10") = a4;
    register long r8  __asm__("r8")  = a5;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(nr), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8) : "rcx","r11","memory");
    return ret;
}
KEX_ALWAYS_INLINE long kex_syscall6(long nr, long a1, long a2, long a3, long a4, long a5, long a6) {
    long ret;
    register long r10 __asm__("r10") = a4;
    register long r8  __asm__("r8")  = a5;
    register long r9  __asm__("r9")  = a6;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(nr), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8), "r"(r9) : "rcx","r11","memory");
    return ret;
}

/* ========================================================================
 * 常用 API 包装 (sys_ 命名, 与 Kenux API 表一致)
 * ======================================================================== */

/* --- 文件 I/O --- */
KEX_ALWAYS_INLINE long sys_read(int fd, void *buf, size_t count) {
    return kex_syscall3(SYS_read, (long)fd, (long)buf, (long)count);
}
KEX_ALWAYS_INLINE long sys_write(int fd, const void *buf, size_t count) {
    return kex_syscall3(SYS_write, (long)fd, (long)buf, (long)count);
}
KEX_ALWAYS_INLINE long sys_open(const char *path, int flags, int mode) {
    return kex_syscall3(SYS_open, (long)path, (long)flags, (long)mode);
}
KEX_ALWAYS_INLINE long sys_openat(int dirfd, const char *path, int flags, int mode) {
    return kex_syscall4(SYS_openat, (long)dirfd, (long)path, (long)flags, (long)mode);
}
KEX_ALWAYS_INLINE long sys_close(int fd) {
    return kex_syscall1(SYS_close, (long)fd);
}
KEX_ALWAYS_INLINE long sys_close_range(unsigned fd, unsigned max_fd, unsigned flags) {
    return kex_syscall3(SYS_close_range, (long)fd, (long)max_fd, (long)flags);
}
KEX_ALWAYS_INLINE long sys_lseek(int fd, long offset, int whence) {
    return kex_syscall3(SYS_lseek, (long)fd, offset, (long)whence);
}
KEX_ALWAYS_INLINE long sys_stat(const char *path, void *statbuf) {
    return kex_syscall2(SYS_stat, (long)path, (long)statbuf);
}
KEX_ALWAYS_INLINE long sys_fstat(int fd, void *statbuf) {
    return kex_syscall2(SYS_fstat, (long)fd, (long)statbuf);
}
KEX_ALWAYS_INLINE long sys_lstat(const char *path, void *statbuf) {
    return kex_syscall2(SYS_lstat, (long)path, (long)statbuf);
}
KEX_ALWAYS_INLINE long sys_pread64(int fd, void *buf, size_t count, long offset) {
    return kex_syscall4(SYS_pread64, (long)fd, (long)buf, (long)count, offset);
}
KEX_ALWAYS_INLINE long sys_pwrite64(int fd, const void *buf, size_t count, long offset) {
    return kex_syscall4(SYS_pwrite64, (long)fd, (long)buf, (long)count, offset);
}
KEX_ALWAYS_INLINE long sys_readv(int fd, const void *iov, int iovcnt) {
    return kex_syscall3(SYS_readv, (long)fd, (long)iov, (long)iovcnt);
}
KEX_ALWAYS_INLINE long sys_writev(int fd, const void *iov, int iovcnt) {
    return kex_syscall3(SYS_writev, (long)fd, (long)iov, (long)iovcnt);
}
KEX_ALWAYS_INLINE long sys_access(const char *path, int mode) {
    return kex_syscall2(SYS_access, (long)path, (long)mode);
}
KEX_ALWAYS_INLINE long sys_dup(int oldfd) {
    return kex_syscall1(SYS_dup, (long)oldfd);
}
KEX_ALWAYS_INLINE long sys_dup2(int oldfd, int newfd) {
    return kex_syscall2(SYS_dup2, (long)oldfd, (long)newfd);
}
KEX_ALWAYS_INLINE long sys_dup3(int oldfd, int newfd, int flags) {
    return kex_syscall3(SYS_dup3, (long)oldfd, (long)newfd, (long)flags);
}
KEX_ALWAYS_INLINE long sys_pipe(int pipefd[2]) {
    return kex_syscall1(SYS_pipe, (long)pipefd);
}
KEX_ALWAYS_INLINE long sys_pipe2(int pipefd[2], int flags) {
    return kex_syscall2(SYS_pipe2, (long)pipefd, (long)flags);
}
KEX_ALWAYS_INLINE long sys_fcntl(int fd, int cmd, long arg) {
    return kex_syscall3(SYS_fcntl, (long)fd, (long)cmd, arg);
}
KEX_ALWAYS_INLINE long sys_ioctl(int fd, unsigned long cmd, unsigned long arg) {
    return kex_syscall3(SYS_ioctl, (long)fd, (long)cmd, (long)arg);
}
KEX_ALWAYS_INLINE long sys_fsync(int fd) {
    return kex_syscall1(SYS_fsync, (long)fd);
}
KEX_ALWAYS_INLINE long sys_fdatasync(int fd) {
    return kex_syscall1(SYS_fdatasync, (long)fd);
}
KEX_ALWAYS_INLINE long sys_truncate(const char *path, long length) {
    return kex_syscall2(SYS_truncate, (long)path, length);
}
KEX_ALWAYS_INLINE long sys_ftruncate(int fd, long length) {
    return kex_syscall2(SYS_ftruncate, (long)fd, length);
}
KEX_ALWAYS_INLINE long sys_fallocate(int fd, int mode, long offset, long len) {
    return kex_syscall4(SYS_fallocate, (long)fd, (long)mode, offset, len);
}
KEX_ALWAYS_INLINE long sys_getdents(int fd, void *dirp, unsigned count) {
    return kex_syscall3(SYS_getdents, (long)fd, (long)dirp, (long)count);
}
KEX_ALWAYS_INLINE long sys_getdents64(int fd, void *dirp, unsigned count) {
    return kex_syscall3(SYS_getdents64, (long)fd, (long)dirp, (long)count);
}
KEX_ALWAYS_INLINE long sys_getcwd(char *buf, size_t size) {
    return kex_syscall2(SYS_getcwd, (long)buf, (long)size);
}
KEX_ALWAYS_INLINE long sys_chdir(const char *path) {
    return kex_syscall1(SYS_chdir, (long)path);
}
KEX_ALWAYS_INLINE long sys_fchdir(int fd) {
    return kex_syscall1(SYS_fchdir, (long)fd);
}
KEX_ALWAYS_INLINE long sys_rename(const char *oldpath, const char *newpath) {
    return kex_syscall2(SYS_rename, (long)oldpath, (long)newpath);
}
KEX_ALWAYS_INLINE long sys_renameat2(int olddfd, const char *oldpath,
                                     int newdfd, const char *newpath, unsigned flags) {
    return kex_syscall5(SYS_renameat2, (long)olddfd, (long)oldpath, (long)newdfd, (long)newpath, (long)flags);
}
KEX_ALWAYS_INLINE long sys_mkdir(const char *path, int mode) {
    return kex_syscall2(SYS_mkdir, (long)path, (long)mode);
}
KEX_ALWAYS_INLINE long sys_mkdirat(int dirfd, const char *path, int mode) {
    return kex_syscall3(SYS_mkdirat, (long)dirfd, (long)path, (long)mode);
}
KEX_ALWAYS_INLINE long sys_rmdir(const char *path) {
    return kex_syscall1(SYS_rmdir, (long)path);
}
KEX_ALWAYS_INLINE long sys_creat(const char *path, int mode) {
    return kex_syscall2(SYS_creat, (long)path, (long)mode);
}
KEX_ALWAYS_INLINE long sys_link(const char *oldpath, const char *newpath) {
    return kex_syscall2(SYS_link, (long)oldpath, (long)newpath);
}
KEX_ALWAYS_INLINE long sys_unlink(const char *path) {
    return kex_syscall1(SYS_unlink, (long)path);
}
KEX_ALWAYS_INLINE long sys_unlinkat(int dirfd, const char *path, int flags) {
    return kex_syscall3(SYS_unlinkat, (long)dirfd, (long)path, (long)flags);
}
KEX_ALWAYS_INLINE long sys_symlink(const char *target, const char *linkpath) {
    return kex_syscall2(SYS_symlink, (long)target, (long)linkpath);
}
KEX_ALWAYS_INLINE long sys_readlink(const char *path, char *buf, size_t bufsize) {
    return kex_syscall3(SYS_readlink, (long)path, (long)buf, (long)bufsize);
}
KEX_ALWAYS_INLINE long sys_chmod(const char *path, int mode) {
    return kex_syscall2(SYS_chmod, (long)path, (long)mode);
}
KEX_ALWAYS_INLINE long sys_fchmod(int fd, int mode) {
    return kex_syscall2(SYS_fchmod, (long)fd, (long)mode);
}
KEX_ALWAYS_INLINE long sys_chown(const char *path, int uid, int gid) {
    return kex_syscall3(SYS_chown, (long)path, (long)uid, (long)gid);
}
KEX_ALWAYS_INLINE long sys_fchown(int fd, int uid, int gid) {
    return kex_syscall3(SYS_fchown, (long)fd, (long)uid, (long)gid);
}
KEX_ALWAYS_INLINE long sys_umask(int mode) {
    return kex_syscall1(SYS_umask, (long)mode);
}
KEX_ALWAYS_INLINE long sys_sendfile(int out_fd, int in_fd, long *offset, size_t count) {
    return kex_syscall4(SYS_sendfile, (long)out_fd, (long)in_fd, (long)offset, (long)count);
}
KEX_ALWAYS_INLINE long sys_memfd_create(const char *name, unsigned flags) {
    return kex_syscall2(SYS_memfd_create, (long)name, (long)flags);
}
KEX_ALWAYS_INLINE long sys_getrandom(void *buf, size_t buflen, unsigned flags) {
    return kex_syscall3(SYS_getrandom, (long)buf, (long)buflen, (long)flags);
}

/* --- 内存管理 --- */
KEX_ALWAYS_INLINE long sys_mmap(void *addr, size_t length, int prot, int flags, int fd, long offset) {
    return kex_syscall6(SYS_mmap, (long)addr, (long)length, (long)prot, (long)flags, (long)fd, offset);
}
KEX_ALWAYS_INLINE long sys_munmap(void *addr, size_t length) {
    return kex_syscall2(SYS_munmap, (long)addr, (long)length);
}
KEX_ALWAYS_INLINE long sys_mprotect(void *addr, size_t len, int prot) {
    return kex_syscall3(SYS_mprotect, (long)addr, (long)len, (long)prot);
}
KEX_ALWAYS_INLINE long sys_brk(void *addr) {
    return kex_syscall1(SYS_brk, (long)addr);
}
KEX_ALWAYS_INLINE long sys_mremap(void *old_addr, size_t old_size, size_t new_size, int flags, void *new_addr) {
    return kex_syscall5(SYS_mremap, (long)old_addr, (long)old_size, (long)new_size, (long)flags, (long)new_addr);
}
KEX_ALWAYS_INLINE long sys_msync(void *addr, size_t length, int flags) {
    return kex_syscall3(SYS_msync, (long)addr, (long)length, (long)flags);
}
KEX_ALWAYS_INLINE long sys_madvise(void *addr, size_t length, int advice) {
    return kex_syscall3(SYS_madvise, (long)addr, (long)length, (long)advice);
}
KEX_ALWAYS_INLINE long sys_mlock(const void *addr, size_t len) {
    return kex_syscall2(SYS_mlock, (long)addr, (long)len);
}
KEX_ALWAYS_INLINE long sys_munlock(const void *addr, size_t len) {
    return kex_syscall2(SYS_munlock, (long)addr, (long)len);
}

/* --- 进程/线程 --- */
KEX_ALWAYS_INLINE long sys_fork(void) {
    return kex_syscall0(SYS_fork);
}
KEX_ALWAYS_INLINE long sys_vfork(void) {
    return kex_syscall0(SYS_vfork);
}
KEX_ALWAYS_INLINE long sys_clone(unsigned long flags, void *stack, int *ptid, int *ctid, unsigned long tls) {
    return kex_syscall5(SYS_clone, (long)flags, (long)stack, (long)ptid, (long)ctid, (long)tls);
}
KEX_ALWAYS_INLINE long sys_clone3(void *args, size_t size) {
    return kex_syscall2(SYS_clone3, (long)args, (long)size);
}
KEX_ALWAYS_INLINE long sys_execve(const char *path, char *const argv[], char *const envp[]) {
    return kex_syscall3(SYS_execve, (long)path, (long)argv, (long)envp);
}
KEX_ALWAYS_INLINE long sys_execveat(int dirfd, const char *path, char *const argv[], char *const envp[], int flags) {
    return kex_syscall5(SYS_execveat, (long)dirfd, (long)path, (long)argv, (long)envp, (long)flags);
}
KEX_ALWAYS_INLINE long sys_exit(int code) {
    return kex_syscall1(SYS_exit, (long)code);
}
KEX_ALWAYS_INLINE long sys_exit_group(int code) {
    return kex_syscall1(SYS_exit_group, (long)code);
}
KEX_ALWAYS_INLINE long sys_wait4(int pid, int *stat_addr, int options, void *ru) {
    return kex_syscall4(SYS_wait4, (long)pid, (long)stat_addr, (long)options, (long)ru);
}
KEX_ALWAYS_INLINE long sys_waitid(int idtype, int id, void *infop, int options, void *ru) {
    return kex_syscall5(SYS_waitid, (long)idtype, (long)id, (long)infop, (long)options, (long)ru);
}
KEX_ALWAYS_INLINE long sys_kill(int pid, int sig) {
    return kex_syscall2(SYS_kill, (long)pid, (long)sig);
}
KEX_ALWAYS_INLINE long sys_tkill(int tid, int sig) {
    return kex_syscall2(SYS_tkill, (long)tid, (long)sig);
}
KEX_ALWAYS_INLINE long sys_tgkill(int tgid, int tid, int sig) {
    return kex_syscall3(SYS_tgkill, (long)tgid, (long)tid, (long)sig);
}
KEX_ALWAYS_INLINE long sys_getpid(void) { return kex_syscall0(SYS_getpid); }
KEX_ALWAYS_INLINE long sys_gettid(void) { return kex_syscall0(SYS_gettid); }
KEX_ALWAYS_INLINE long sys_getppid(void) { return kex_syscall0(SYS_getppid); }
KEX_ALWAYS_INLINE long sys_getpgid(int pid) { return kex_syscall1(SYS_getpgid, (long)pid); }
KEX_ALWAYS_INLINE long sys_setpgid(int pid, int pgid) {
    return kex_syscall2(SYS_setpgid, (long)pid, (long)pgid);
}
KEX_ALWAYS_INLINE long sys_getpgrp(void) { return kex_syscall0(SYS_getpgrp); }
KEX_ALWAYS_INLINE long sys_setsid(void) { return kex_syscall0(SYS_setsid); }
KEX_ALWAYS_INLINE long sys_getsid(int pid) { return kex_syscall1(SYS_getsid, (long)pid); }
KEX_ALWAYS_INLINE long sys_sched_yield(void) { return kex_syscall0(SYS_sched_yield); }

/* --- 身份/权限 --- */
KEX_ALWAYS_INLINE long sys_getuid(void) { return kex_syscall0(SYS_getuid); }
KEX_ALWAYS_INLINE long sys_getgid(void) { return kex_syscall0(SYS_getgid); }
KEX_ALWAYS_INLINE long sys_geteuid(void) { return kex_syscall0(SYS_geteuid); }
KEX_ALWAYS_INLINE long sys_getegid(void) { return kex_syscall0(SYS_getegid); }
KEX_ALWAYS_INLINE long sys_setuid(int uid) { return kex_syscall1(SYS_setuid, (long)uid); }
KEX_ALWAYS_INLINE long sys_setgid(int gid) { return kex_syscall1(SYS_setgid, (long)gid); }
KEX_ALWAYS_INLINE long sys_getgroups(int size, int *list) {
    return kex_syscall2(SYS_getgroups, (long)size, (long)list);
}
KEX_ALWAYS_INLINE long sys_setgroups(int size, int *list) {
    return kex_syscall2(SYS_setgroups, (long)size, (long)list);
}

/* --- 网络 --- */
KEX_ALWAYS_INLINE long sys_socket(int domain, int type, int protocol) {
    return kex_syscall3(SYS_socket, (long)domain, (long)type, (long)protocol);
}
KEX_ALWAYS_INLINE long sys_socketpair(int domain, int type, int protocol, int sv[2]) {
    return kex_syscall4(SYS_socketpair, (long)domain, (long)type, (long)protocol, (long)sv);
}
KEX_ALWAYS_INLINE long sys_connect(int sockfd, const void *addr, int addrlen) {
    return kex_syscall3(SYS_connect, (long)sockfd, (long)addr, (long)addrlen);
}
KEX_ALWAYS_INLINE long sys_accept(int sockfd, void *addr, int *addrlen) {
    return kex_syscall3(SYS_accept, (long)sockfd, (long)addr, (long)addrlen);
}
KEX_ALWAYS_INLINE long sys_accept4(int sockfd, void *addr, int *addrlen, int flags) {
    return kex_syscall4(SYS_accept4, (long)sockfd, (long)addr, (long)addrlen, (long)flags);
}
KEX_ALWAYS_INLINE long sys_bind(int sockfd, const void *addr, int addrlen) {
    return kex_syscall3(SYS_bind, (long)sockfd, (long)addr, (long)addrlen);
}
KEX_ALWAYS_INLINE long sys_listen(int sockfd, int backlog) {
    return kex_syscall2(SYS_listen, (long)sockfd, (long)backlog);
}
KEX_ALWAYS_INLINE long sys_shutdown(int sockfd, int how) {
    return kex_syscall2(SYS_shutdown, (long)sockfd, (long)how);
}
KEX_ALWAYS_INLINE long sys_getsockname(int sockfd, void *addr, int *addrlen) {
    return kex_syscall3(SYS_getsockname, (long)sockfd, (long)addr, (long)addrlen);
}
KEX_ALWAYS_INLINE long sys_getpeername(int sockfd, void *addr, int *addrlen) {
    return kex_syscall3(SYS_getpeername, (long)sockfd, (long)addr, (long)addrlen);
}
KEX_ALWAYS_INLINE long sys_sendto(int sockfd, const void *buf, size_t len, int flags,
                                  const void *dest_addr, int addrlen) {
    return kex_syscall6(SYS_sendto, (long)sockfd, (long)buf, (long)len, (long)flags, (long)dest_addr, (long)addrlen);
}
KEX_ALWAYS_INLINE long sys_recvfrom(int sockfd, void *buf, size_t len, int flags,
                                    void *src_addr, int *addrlen) {
    return kex_syscall6(SYS_recvfrom, (long)sockfd, (long)buf, (long)len, (long)flags, (long)src_addr, (long)addrlen);
}
KEX_ALWAYS_INLINE long sys_sendmsg(int sockfd, const void *msg, int flags) {
    return kex_syscall3(SYS_sendmsg, (long)sockfd, (long)msg, (long)flags);
}
KEX_ALWAYS_INLINE long sys_recvmsg(int sockfd, void *msg, int flags) {
    return kex_syscall3(SYS_recvmsg, (long)sockfd, (long)msg, (long)flags);
}
KEX_ALWAYS_INLINE long sys_setsockopt(int sockfd, int level, int optname,
                                      const void *optval, int optlen) {
    return kex_syscall5(SYS_setsockopt, (long)sockfd, (long)level, (long)optname, (long)optval, (long)optlen);
}
KEX_ALWAYS_INLINE long sys_getsockopt(int sockfd, int level, int optname,
                                      void *optval, int *optlen) {
    return kex_syscall5(SYS_getsockopt, (long)sockfd, (long)level, (long)optname, (long)optval, (long)optlen);
}

/* --- 事件/同步 --- */
KEX_ALWAYS_INLINE long sys_poll(void *fds, unsigned nfds, int timeout) {
    return kex_syscall3(SYS_poll, (long)fds, (long)nfds, (long)timeout);
}
KEX_ALWAYS_INLINE long sys_ppoll(void *fds, unsigned nfds, const void *tmo, const void *smask) {
    return kex_syscall4(SYS_ppoll, (long)fds, (long)nfds, (long)tmo, (long)smask);
}
KEX_ALWAYS_INLINE long sys_select(int nfds, void *rfds, void *wfds, void *efds, const void *timeout) {
    return kex_syscall5(SYS_select, (long)nfds, (long)rfds, (long)wfds, (long)efds, (long)timeout);
}
KEX_ALWAYS_INLINE long sys_pselect6(int nfds, void *rfds, void *wfds, void *efds,
                                    const void *timeout, const void *sig) {
    return kex_syscall6(SYS_pselect6, (long)nfds, (long)rfds, (long)wfds, (long)efds, (long)timeout, (long)sig);
}
KEX_ALWAYS_INLINE long sys_epoll_create(int size) {
    return kex_syscall1(SYS_epoll_create, (long)size);
}
KEX_ALWAYS_INLINE long sys_epoll_create1(int flags) {
    return kex_syscall1(SYS_epoll_create1, (long)flags);
}
KEX_ALWAYS_INLINE long sys_epoll_ctl(int epfd, int op, int fd, void *event) {
    return kex_syscall4(SYS_epoll_ctl, (long)epfd, (long)op, (long)fd, (long)event);
}
KEX_ALWAYS_INLINE long sys_epoll_wait(int epfd, void *events, int maxevents, int timeout) {
    return kex_syscall4(SYS_epoll_wait, (long)epfd, (long)events, (long)maxevents, (long)timeout);
}
KEX_ALWAYS_INLINE long sys_epoll_pwait(int epfd, void *events, int maxevents, int timeout, const void *sigmask) {
    return kex_syscall5(SYS_epoll_pwait, (long)epfd, (long)events, (long)maxevents, (long)timeout, (long)sigmask);
}
KEX_ALWAYS_INLINE long sys_futex(int *uaddr, int op, int val, const void *timeout, int *uaddr2, int val3) {
    return kex_syscall6(SYS_futex, (long)uaddr, (long)op, (long)val, (long)timeout, (long)uaddr2, (long)val3);
}
KEX_ALWAYS_INLINE long sys_eventfd(unsigned initval) {
    return kex_syscall1(SYS_eventfd, (long)initval);
}
KEX_ALWAYS_INLINE long sys_eventfd2(unsigned initval, int flags) {
    return kex_syscall2(SYS_eventfd2, (long)initval, (long)flags);
}
KEX_ALWAYS_INLINE long sys_timerfd_create(int clockid, int flags) {
    return kex_syscall2(SYS_timerfd_create, (long)clockid, (long)flags);
}
KEX_ALWAYS_INLINE long sys_timerfd_settime(int fd, int flags, const void *new_val, void *old_val) {
    return kex_syscall4(SYS_timerfd_settime, (long)fd, (long)flags, (long)new_val, (long)old_val);
}
KEX_ALWAYS_INLINE long sys_signalfd(int fd, const void *mask, int sizemask) {
    return kex_syscall3(SYS_signalfd, (long)fd, (long)mask, (long)sizemask);
}
KEX_ALWAYS_INLINE long sys_signalfd4(int fd, const void *mask, int sizemask, int flags) {
    return kex_syscall4(SYS_signalfd4, (long)fd, (long)mask, (long)sizemask, (long)flags);
}
KEX_ALWAYS_INLINE long sys_inotify_init(void) {
    return kex_syscall0(SYS_inotify_init);
}
KEX_ALWAYS_INLINE long sys_inotify_init1(int flags) {
    return kex_syscall1(SYS_inotify_init1, (long)flags);
}
KEX_ALWAYS_INLINE long sys_inotify_add_watch(int fd, const char *path, uint32_t mask) {
    return kex_syscall3(SYS_inotify_add_watch, (long)fd, (long)path, (long)mask);
}
KEX_ALWAYS_INLINE long sys_inotify_rm_watch(int fd, int wd) {
    return kex_syscall2(SYS_inotify_rm_watch, (long)fd, (long)wd);
}

/* --- 时间 --- */
KEX_ALWAYS_INLINE long sys_nanosleep(long sec, long nsec) {
    struct { long tv_sec; long tv_nsec; } ts = { sec, nsec };
    return kex_syscall2(SYS_nanosleep, (long)&ts, 0);
}
KEX_ALWAYS_INLINE long sys_clock_gettime(int clk, void *tp) {
    return kex_syscall2(SYS_clock_gettime, (long)clk, (long)tp);
}
KEX_ALWAYS_INLINE long sys_clock_settime(int clk, const void *tp) {
    return kex_syscall2(SYS_clock_settime, (long)clk, (long)tp);
}
KEX_ALWAYS_INLINE long sys_clock_getres(int clk, void *res) {
    return kex_syscall2(SYS_clock_getres, (long)clk, (long)res);
}
KEX_ALWAYS_INLINE long sys_clock_nanosleep(int clk, int flags, const void *req, void *rem) {
    return kex_syscall4(SYS_clock_nanosleep, (long)clk, (long)flags, (long)req, (long)rem);
}
KEX_ALWAYS_INLINE long sys_gettimeofday(void *tv, void *tz) {
    return kex_syscall2(SYS_gettimeofday, (long)tv, (long)tz);
}
KEX_ALWAYS_INLINE long sys_settimeofday(const void *tv, const void *tz) {
    return kex_syscall2(SYS_settimeofday, (long)tv, (long)tz);
}
KEX_ALWAYS_INLINE long sys_time(long *tloc) {
    return kex_syscall1(SYS_time, (long)tloc);
}
KEX_ALWAYS_INLINE long sys_alarm(unsigned seconds) {
    return kex_syscall1(SYS_alarm, (long)seconds);
}

/* --- 信号 --- */
KEX_ALWAYS_INLINE long sys_rt_sigaction(int signum, const void *act, void *oldact, size_t sigsetsize) {
    return kex_syscall4(SYS_rt_sigaction, (long)signum, (long)act, (long)oldact, (long)sigsetsize);
}
KEX_ALWAYS_INLINE long sys_rt_sigprocmask(int how, const void *set, void *oldset, size_t sigsetsize) {
    return kex_syscall4(SYS_rt_sigprocmask, (long)how, (long)set, (long)oldset, (long)sigsetsize);
}
KEX_ALWAYS_INLINE long sys_rt_sigpending(void *set, size_t sigsetsize) {
    return kex_syscall2(SYS_rt_sigpending, (long)set, (long)sigsetsize);
}
KEX_ALWAYS_INLINE long sys_rt_sigsuspend(const void *mask, size_t sigsetsize) {
    return kex_syscall2(SYS_rt_sigsuspend, (long)mask, (long)sigsetsize);
}
KEX_ALWAYS_INLINE long sys_sigaltstack(const void *ss, void *oss) {
    return kex_syscall2(SYS_sigaltstack, (long)ss, (long)oss);
}

/* --- 系统/资源 --- */
KEX_ALWAYS_INLINE long sys_uname(void *buf) {
    return kex_syscall1(SYS_uname, (long)buf);
}
KEX_ALWAYS_INLINE long sys_sysinfo(void *info) {
    return kex_syscall1(SYS_sysinfo, (long)info);
}
KEX_ALWAYS_INLINE long sys_getrlimit(int resource, void *rlim) {
    return kex_syscall2(SYS_getrlimit, (long)resource, (long)rlim);
}
KEX_ALWAYS_INLINE long sys_setrlimit(int resource, const void *rlim) {
    return kex_syscall2(SYS_setrlimit, (long)resource, (long)rlim);
}
KEX_ALWAYS_INLINE long sys_prlimit64(int pid, int resource, const void *new_rlim, void *old_rlim) {
    return kex_syscall4(SYS_prlimit64, (long)pid, (long)resource, (long)new_rlim, (long)old_rlim);
}
KEX_ALWAYS_INLINE long sys_getrusage(int who, void *ru) {
    return kex_syscall2(SYS_getrusage, (long)who, (long)ru);
}
KEX_ALWAYS_INLINE long sys_times(void *buf) {
    return kex_syscall1(SYS_times, (long)buf);
}
KEX_ALWAYS_INLINE long sys_syslog(int type, char *buf, int len) {
    return kex_syscall3(SYS_syslog, (long)type, (long)buf, (long)len);
}
KEX_ALWAYS_INLINE long sys_ptrace(int request, int pid, void *addr, void *data) {
    return kex_syscall4(SYS_ptrace, (long)request, (long)pid, (long)addr, (long)data);
}
KEX_ALWAYS_INLINE long sys_prctl(int option, long arg2, long arg3, long arg4, long arg5) {
    return kex_syscall5(SYS_prctl, (long)option, arg2, arg3, arg4, arg5);
}
KEX_ALWAYS_INLINE long sys_arch_prctl(int code, unsigned long addr) {
    return kex_syscall2(SYS_arch_prctl, (long)code, (long)addr);
}
KEX_ALWAYS_INLINE long sys_capget(void *hdrp, void *datap) {
    return kex_syscall2(SYS_capget, (long)hdrp, (long)datap);
}
KEX_ALWAYS_INLINE long sys_capset(void *hdrp, const void *datap) {
    return kex_syscall2(SYS_capset, (long)hdrp, (long)datap);
}

/* --- 文件系统/挂载 --- */
KEX_ALWAYS_INLINE long sys_mount(const char *source, const char *target, const char *filesystemtype,
                                 unsigned long mountflags, const void *data) {
    return kex_syscall5(SYS_mount, (long)source, (long)target, (long)filesystemtype, (long)mountflags, (long)data);
}
KEX_ALWAYS_INLINE long sys_umount2(const char *target, int flags) {
    return kex_syscall2(SYS_umount2, (long)target, (long)flags);
}
KEX_ALWAYS_INLINE long sys_chroot(const char *path) {
    return kex_syscall1(SYS_chroot, (long)path);
}
KEX_ALWAYS_INLINE long sys_sync(void) { return kex_syscall0(SYS_sync); }
KEX_ALWAYS_INLINE long sys_syncfs(int fd) {
    return kex_syscall1(SYS_syncfs, (long)fd);
}
KEX_ALWAYS_INLINE long sys_statfs(const char *path, void *buf) {
    return kex_syscall2(SYS_statfs, (long)path, (long)buf);
}
KEX_ALWAYS_INLINE long sys_fstatfs(int fd, void *buf) {
    return kex_syscall2(SYS_fstatfs, (long)fd, (long)buf);
}
KEX_ALWAYS_INLINE long sys_statx(int dirfd, const char *pathname, int flags, unsigned mask, void *statxbuf) {
    return kex_syscall5(SYS_statx, (long)dirfd, (long)pathname, (long)flags, (long)mask, (long)statxbuf);
}

/* --- 电源/重启 --- */
KEX_ALWAYS_INLINE long sys_reboot(int magic1, int magic2, int cmd, void *arg) {
    return kex_syscall4(SYS_reboot, (long)magic1, (long)magic2, (long)cmd, (long)arg);
}

/* --- 扩展属性 --- */
KEX_ALWAYS_INLINE long sys_setxattr(const char *path, const char *name, const void *value, size_t size, int flags) {
    return kex_syscall5(SYS_setxattr, (long)path, (long)name, (long)value, (long)size, (long)flags);
}
KEX_ALWAYS_INLINE long sys_getxattr(const char *path, const char *name, void *value, size_t size) {
    return kex_syscall4(SYS_getxattr, (long)path, (long)name, (long)value, (long)size);
}
KEX_ALWAYS_INLINE long sys_listxattr(const char *path, char *list, size_t size) {
    return kex_syscall3(SYS_listxattr, (long)path, (long)list, (long)size);
}
KEX_ALWAYS_INLINE long sys_removexattr(const char *path, const char *name) {
    return kex_syscall2(SYS_removexattr, (long)path, (long)name);
}

/* --- io_uring --- */
KEX_ALWAYS_INLINE long sys_io_uring_setup(unsigned entries, void *params) {
    return kex_syscall2(SYS_io_uring_setup, (long)entries, (long)params);
}
KEX_ALWAYS_INLINE long sys_io_uring_enter(int fd, unsigned to_submit, unsigned min_complete, unsigned flags, void *sig) {
    return kex_syscall5(SYS_io_uring_enter, (long)fd, (long)to_submit, (long)min_complete, (long)flags, (long)sig);
}
KEX_ALWAYS_INLINE long sys_io_uring_register(int fd, unsigned opcode, void *arg, unsigned nr_args) {
    return kex_syscall4(SYS_io_uring_register, (long)fd, (long)opcode, (long)arg, (long)nr_args);
}

/* --- seccomp/eBPF --- */
KEX_ALWAYS_INLINE long sys_seccomp(unsigned op, unsigned flags, const void *args) {
    return kex_syscall3(SYS_seccomp, (long)op, (long)flags, (long)args);
}
KEX_ALWAYS_INLINE long sys_bpf(int cmd, void *attr, unsigned int size) {
    return kex_syscall3(SYS_bpf, (long)cmd, (long)attr, (long)size);
}

/* --- 新增 syscall (333-450) 内联包装 --- */
KEX_ALWAYS_INLINE long sys_io_pgetevents(void *ctx, long min_nr, long nr,
                                         void *events, const void *timeout, const void *usig) {
    return kex_syscall6(SYS_io_pgetevents, (long)ctx, min_nr, nr, (long)events, (long)timeout, (long)usig);
}
KEX_ALWAYS_INLINE long sys_rseq(void *rseq, uint32_t rseq_len, int flags, uint32_t sig) {
    return kex_syscall4(SYS_rseq, (long)rseq, (long)rseq_len, (long)flags, (long)sig);
}
KEX_ALWAYS_INLINE long sys_kexec_file_load2(int kernel_fd, int initrd_fd,
                                             unsigned long cmdline_len, const char *cmdline, unsigned long flags) {
    return kex_syscall5(SYS_kexec_file_load2, (long)kernel_fd, (long)initrd_fd, (long)cmdline_len, (long)cmdline, (long)flags);
}
KEX_ALWAYS_INLINE long sys_pidfd_send_signal(int pidfd, int sig, const void *info, unsigned flags) {
    return kex_syscall4(SYS_pidfd_send_signal, (long)pidfd, (long)sig, (long)info, (long)flags);
}
KEX_ALWAYS_INLINE long sys_open_tree(int dfd, const char *path, unsigned flags) {
    return kex_syscall3(SYS_open_tree, (long)dfd, (long)path, (long)flags);
}
KEX_ALWAYS_INLINE long sys_move_mount(int from_dfd, const char *from_path,
                                      int to_dfd, const char *to_path, unsigned flags) {
    return kex_syscall5(SYS_move_mount, (long)from_dfd, (long)from_path, (long)to_dfd, (long)to_path, (long)flags);
}
KEX_ALWAYS_INLINE long sys_fsopen(const char *fs_name, unsigned flags) {
    return kex_syscall2(SYS_fsopen, (long)fs_name, (long)flags);
}
KEX_ALWAYS_INLINE long sys_fsconfig(int fs_fd, unsigned cmd, const char *key, const void *value, int aux) {
    return kex_syscall5(SYS_fsconfig, (long)fs_fd, (long)cmd, (long)key, (long)value, (long)aux);
}
KEX_ALWAYS_INLINE long sys_fsmount(int fs_fd, unsigned flags, unsigned attr_flags) {
    return kex_syscall3(SYS_fsmount, (long)fs_fd, (long)flags, (long)attr_flags);
}
KEX_ALWAYS_INLINE long sys_fspick(int dfd, const char *path, unsigned flags) {
    return kex_syscall3(SYS_fspick, (long)dfd, (long)path, (long)flags);
}
KEX_ALWAYS_INLINE long sys_pidfd_open(int pid, unsigned flags) {
    return kex_syscall2(SYS_pidfd_open, (long)pid, (long)flags);
}
KEX_ALWAYS_INLINE long sys_pidfd_getfd(int pidfd, int targetfd, unsigned flags) {
    return kex_syscall3(SYS_pidfd_getfd, (long)pidfd, (long)targetfd, (long)flags);
}
KEX_ALWAYS_INLINE long sys_faccessat2(int dfd, const char *path, int mode, int flags) {
    return kex_syscall4(SYS_faccessat2, (long)dfd, (long)path, (long)mode, (long)flags);
}
KEX_ALWAYS_INLINE long sys_process_madvise(int pidfd, const void *iovec, size_t vlen, int advice, unsigned flags) {
    return kex_syscall5(SYS_process_madvise, (long)pidfd, (long)iovec, (long)vlen, (long)advice, (long)flags);
}
KEX_ALWAYS_INLINE long sys_epoll_pwait2(int epfd, void *events, int maxevents, const void *timeout, const void *sigmask) {
    return kex_syscall5(SYS_epoll_pwait2, (long)epfd, (long)events, (long)maxevents, (long)timeout, (long)sigmask);
}
KEX_ALWAYS_INLINE long sys_mount_setattr(int dfd, const char *path, unsigned attr_flags, const void *uattr, size_t usize) {
    return kex_syscall5(SYS_mount_setattr, (long)dfd, (long)path, (long)attr_flags, (long)uattr, (long)usize);
}
KEX_ALWAYS_INLINE long sys_quotactl_fd(unsigned fd, unsigned cmd, int id, void *addr) {
    return kex_syscall4(SYS_quotactl_fd, (long)fd, (long)cmd, (long)id, (long)addr);
}
KEX_ALWAYS_INLINE long sys_landlock_create_ruleset(const void *attr, size_t size, unsigned flags) {
    return kex_syscall3(SYS_landlock_create_ruleset, (long)attr, (long)size, (long)flags);
}
KEX_ALWAYS_INLINE long sys_landlock_add_rule(int ruleset_fd, int rule_type, const void *rule_attr, unsigned flags) {
    return kex_syscall4(SYS_landlock_add_rule, (long)ruleset_fd, (long)rule_type, (long)rule_attr, (long)flags);
}
KEX_ALWAYS_INLINE long sys_landlock_restrict_self(int ruleset_fd, unsigned flags) {
    return kex_syscall2(SYS_landlock_restrict_self, (long)ruleset_fd, (long)flags);
}
KEX_ALWAYS_INLINE long sys_memfd_secret(unsigned flags) {
    return kex_syscall1(SYS_memfd_secret, (long)flags);
}
KEX_ALWAYS_INLINE long sys_process_mrelease(int pidfd, unsigned flags) {
    return kex_syscall2(SYS_process_mrelease, (long)pidfd, (long)flags);
}
KEX_ALWAYS_INLINE long sys_futex_waitv(void *waiters, unsigned nr_futexes, unsigned flags) {
    return kex_syscall3(SYS_futex_waitv, (long)waiters, (long)nr_futexes, (long)flags);
}
KEX_ALWAYS_INLINE long sys_set_mempolicy_home_node(unsigned long start, unsigned long end, unsigned long home_node, unsigned long flags) {
    return kex_syscall4(SYS_set_mempolicy_home_node, (long)start, (long)end, (long)home_node, (long)flags);
}

/* ========================================================================
 * Kenux 扩展 syscall 包装 (kenux_ 命名)
 * ======================================================================== */
KEX_ALWAYS_INLINE long kenux_info(void *buf) {
    return kex_syscall1(SYS_kenux_info, (long)buf);
}
KEX_ALWAYS_INLINE long kenux_debug(const char *msg) {
    return kex_syscall1(SYS_kenux_debug, (long)msg);
}
KEX_ALWAYS_INLINE long kenux_get_version(void *buf) {
    return kex_syscall1(SYS_kenux_get_version, (long)buf);
}
KEX_ALWAYS_INLINE long kenux_get_uptime(void *buf) {
    return kex_syscall1(SYS_kenux_get_uptime, (long)buf);
}
KEX_ALWAYS_INLINE long kenux_get_loadavg(void *buf, int nelem) {
    return kex_syscall2(SYS_kenux_get_loadavg, (long)buf, (long)nelem);
}
KEX_ALWAYS_INLINE long kenux_reboot(void) {
    return kex_syscall0(SYS_kenux_reboot);
}
KEX_ALWAYS_INLINE long kenux_poweroff(void) {
    return kex_syscall0(SYS_kenux_poweroff);
}
KEX_ALWAYS_INLINE long kenux_halt(void) {
    return kex_syscall0(SYS_kenux_halt);
}
KEX_ALWAYS_INLINE long kenux_get_cpu_count(void) {
    return kex_syscall0(SYS_kenux_get_cpu_count);
}
KEX_ALWAYS_INLINE long kenux_get_cpu_info(int cpu, void *buf) {
    return kex_syscall2(SYS_kenux_get_cpu_info, (long)cpu, (long)buf);
}
KEX_ALWAYS_INLINE long kenux_set_affinity(int pid, const void *mask) {
    return kex_syscall2(SYS_kenux_set_affinity, (long)pid, (long)mask);
}
KEX_ALWAYS_INLINE long kenux_get_affinity(int pid, void *mask) {
    return kex_syscall2(SYS_kenux_get_affinity, (long)pid, (long)mask);
}
KEX_ALWAYS_INLINE long kenux_create_namespace(int type) {
    return kex_syscall1(SYS_kenux_create_namespace, (long)type);
}
KEX_ALWAYS_INLINE long kenux_enter_namespace(int fd) {
    return kex_syscall1(SYS_kenux_enter_namespace, (long)fd);
}
KEX_ALWAYS_INLINE long kenux_get_namespace(int pid, void *buf) {
    return kex_syscall2(SYS_kenux_get_namespace, (long)pid, (long)buf);
}
KEX_ALWAYS_INLINE long kenux_vmspace_create(void) {
    return kex_syscall0(SYS_kenux_vmspace_create);
}
KEX_ALWAYS_INLINE long kenux_vmspace_destroy(int id) {
    return kex_syscall1(SYS_kenux_vmspace_destroy, (long)id);
}
KEX_ALWAYS_INLINE long kenux_vmspace_switch(int id) {
    return kex_syscall1(SYS_kenux_vmspace_switch, (long)id);
}
KEX_ALWAYS_INLINE long kenux_fb_get_info(void *buf) {
    return kex_syscall1(SYS_kenux_fb_get_info, (long)buf);
}
KEX_ALWAYS_INLINE long kenux_fb_map(int fd) {
    return kex_syscall1(SYS_kenux_fb_map, (long)fd);
}
KEX_ALWAYS_INLINE long kenux_fb_unmap(int fd) {
    return kex_syscall1(SYS_kenux_fb_unmap, (long)fd);
}
KEX_ALWAYS_INLINE long kenux_fb_flip(int bufidx) {
    return kex_syscall1(SYS_kenux_fb_flip, (long)bufidx);
}
KEX_ALWAYS_INLINE long kenux_net_attach(const char *name, int flags) {
    return kex_syscall2(SYS_kenux_net_attach, (long)name, (long)flags);
}
KEX_ALWAYS_INLINE long kenux_net_detach(int fd) {
    return kex_syscall1(SYS_kenux_net_detach, (long)fd);
}
KEX_ALWAYS_INLINE long kenux_net_ioctl(int fd, int cmd, void *arg) {
    return kex_syscall3(SYS_kenux_net_ioctl, (long)fd, (long)cmd, (long)arg);
}
KEX_ALWAYS_INLINE long kenux_audit_log(int type, const void *data, int len) {
    return kex_syscall3(SYS_kenux_audit_log, (long)type, (long)data, (long)len);
}
KEX_ALWAYS_INLINE long kenux_audit_config(int op, const void *cfg) {
    return kex_syscall2(SYS_kenux_audit_config, (long)op, (long)cfg);
}

/* --- IOMMU / DMA --- */
KEX_ALWAYS_INLINE long kenux_iommu_map(int domain, unsigned long iova,
                                       unsigned long phys, size_t size) {
    return kex_syscall4(SYS_kenux_iommu_map, (long)domain, (long)iova,
                        (long)phys, (long)size);
}
KEX_ALWAYS_INLINE long kenux_iommu_unmap(int domain, unsigned long iova) {
    return kex_syscall2(SYS_kenux_iommu_unmap, (long)domain, (long)iova);
}
KEX_ALWAYS_INLINE long kenux_dma_alloc(size_t size, unsigned flags) {
    return kex_syscall2(SYS_kenux_dma_alloc, (long)size, (long)flags);
}
KEX_ALWAYS_INLINE long kenux_dma_free(void *ptr, size_t size) {
    return kex_syscall2(SYS_kenux_dma_free, (long)ptr, (long)size);
}

/* --- PCI --- */
KEX_ALWAYS_INLINE long kenux_pci_read(int seg, int bus, int dev, int fn) {
    return kex_syscall4(SYS_kenux_pci_read, (long)seg, (long)bus, (long)dev, (long)fn);
}
KEX_ALWAYS_INLINE long kenux_pci_write(int seg, int bus, int dev, int fn) {
    return kex_syscall4(SYS_kenux_pci_write, (long)seg, (long)bus, (long)dev, (long)fn);
}
KEX_ALWAYS_INLINE long kenux_pci_enum(int index, void *buf) {
    return kex_syscall2(SYS_kenux_pci_enum, (long)index, (long)buf);
}

/* --- ACPI / SMBIOS --- */
KEX_ALWAYS_INLINE long kenux_acpi_query(int type, void *buf) {
    return kex_syscall2(SYS_kenux_acpi_query, (long)type, (long)buf);
}
KEX_ALWAYS_INLINE long kenux_smbios_get(int type, void *buf) {
    return kex_syscall2(SYS_kenux_smbios_get, (long)type, (long)buf);
}

/* --- GPU --- */
KEX_ALWAYS_INLINE long kenux_gpu_submit(int ctx, const void *cmd, int count) {
    return kex_syscall3(SYS_kenux_gpu_submit, (long)ctx, (long)cmd, (long)count);
}
KEX_ALWAYS_INLINE long kenux_gpu_wait(int ctx) {
    return kex_syscall1(SYS_kenux_gpu_wait, (long)ctx);
}

/* --- 文件系统扩展 --- */
KEX_ALWAYS_INLINE long kenux_fs_snapshot(const char *path, void *buf) {
    return kex_syscall2(SYS_kenux_fs_snapshot, (long)path, (long)buf);
}
KEX_ALWAYS_INLINE long kenux_fs_rollback(const char *path, void *buf) {
    return kex_syscall2(SYS_kenux_fs_rollback, (long)path, (long)buf);
}
KEX_ALWAYS_INLINE long kenux_fs_compress(const void *in, void *out, size_t len) {
    return kex_syscall3(SYS_kenux_fs_compress, (long)in, (long)out, (long)len);
}
KEX_ALWAYS_INLINE long kenux_fs_encrypt(const void *in, void *out, size_t len) {
    return kex_syscall3(SYS_kenux_fs_encrypt, (long)in, (long)out, (long)len);
}

/* --- seccomp 扩展 --- */
KEX_ALWAYS_INLINE long kenux_seccomp_install(int op, const void *prog) {
    return kex_syscall2(SYS_kenux_seccomp_install, (long)op, (long)prog);
}
KEX_ALWAYS_INLINE long kenux_seccomp_filter(int nr, const void *filter, void *buf) {
    return kex_syscall3(SYS_kenux_seccomp_filter, (long)nr, (long)filter, (long)buf);
}

/* --- trace --- */
KEX_ALWAYS_INLINE long kenux_trace_attach(int pid, const void *opts) {
    return kex_syscall2(SYS_kenux_trace_attach, (long)pid, (long)opts);
}
KEX_ALWAYS_INLINE long kenux_trace_detach(int pid) {
    return kex_syscall1(SYS_kenux_trace_detach, (long)pid);
}
KEX_ALWAYS_INLINE long kenux_trace_read(int pid, void *buf, size_t len) {
    return kex_syscall3(SYS_kenux_trace_read, (long)pid, (long)buf, (long)len);
}
KEX_ALWAYS_INLINE long kenux_trace_write(int pid, const void *buf, size_t len) {
    return kex_syscall3(SYS_kenux_trace_write, (long)pid, (long)buf, (long)len);
}

/* --- kprobe --- */
KEX_ALWAYS_INLINE long kenux_kprobe_register(const char *name, const void *opts) {
    return kex_syscall2(SYS_kenux_kprobe_register, (long)name, (long)opts);
}
KEX_ALWAYS_INLINE long kenux_kprobe_unregister(int id) {
    return kex_syscall1(SYS_kenux_kprobe_unregister, (long)id);
}

/* ========================================================================
 * 便利工具函数
 * ======================================================================== */

/* 计算字符串长度 */
KEX_ALWAYS_INLINE size_t kex_strlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

/* 字符串比较: 0=相等 */
KEX_ALWAYS_INLINE int kex_strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

/* 内存拷贝 */
KEX_ALWAYS_INLINE void kex_memcpy(void *dst, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
}

/* 内存设置 */
KEX_ALWAYS_INLINE void kex_memset(void *dst, int c, size_t n) {
    unsigned char *d = (unsigned char *)dst;
    while (n--) *d++ = (unsigned char)c;
}

/* 打印字符串 (stdout) */
KEX_ALWAYS_INLINE void kex_puts(const char *s) {
    sys_write(1, s, kex_strlen(s));
    char nl = '\n';
    sys_write(1, &nl, 1);
}

/* 打印单个字符 (stdout) */
KEX_ALWAYS_INLINE void kex_putchar(char c) {
    sys_write(1, &c, 1);
}

/* 打印整数 (十进制, 带可选换行) */
KEX_ALWAYS_INLINE void kex_printlong(long val, int newline) {
    char buf[24];
    int pos = sizeof(buf) - 1;
    buf[pos] = '\0';
    int negative = 0;
    unsigned long uval;
    if (val < 0) { negative = 1; uval = (unsigned long)(-(val + 1)) + 1; }
    else uval = (unsigned long)val;
    if (uval == 0) buf[--pos] = '0';
    while (uval > 0) {
        buf[--pos] = '0' + (char)(uval % 10);
        uval /= 10;
    }
    if (negative) buf[--pos] = '-';
    sys_write(1, &buf[pos], sizeof(buf) - 1 - pos);
    if (newline) { char nl = '\n'; sys_write(1, &nl, 1); }
}

/* 打印十六进制 (前缀 0x, 带可选换行) */
KEX_ALWAYS_INLINE void kex_printhex(unsigned long val, int newline) {
    char buf[19];
    int pos = sizeof(buf) - 1;
    buf[pos] = '\0';
    const char *hex = "0123456789abcdef";
    if (val == 0) buf[--pos] = '0';
    while (val > 0) {
        buf[--pos] = hex[val & 0xf];
        val >>= 4;
    }
    buf[--pos] = 'x';
    buf[--pos] = '0';
    sys_write(1, &buf[pos], sizeof(buf) - 1 - pos);
    if (newline) { char nl = '\n'; sys_write(1, &nl, 1); }
}

/* ========================================================================
 * 程序入口原型
 * 用户实现 _start, 也可用 KEX_MAIN 宏声明带 argc/argv 的入口.
 * ======================================================================== */
void _start(void);

#ifdef __cplusplus
}
#endif

#endif /* KEX_USER_H */
