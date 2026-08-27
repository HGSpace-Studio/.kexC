#ifndef KAPI_CLONE_H
#define KAPI_CLONE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_CLONE_VM             0x00000100
#define KAPI_CLONE_FS             0x00000200
#define KAPI_CLONE_FILES          0x00000400
#define KAPI_CLONE_SIGHAND        0x00000800
#define KAPI_CLONE_PIDFD          0x00001000
#define KAPI_CLONE_PTRACE         0x00002000
#define KAPI_CLONE_VFORK          0x00004000
#define KAPI_CLONE_PARENT         0x00008000
#define KAPI_CLONE_THREAD         0x00010000
#define KAPI_CLONE_NEWNS          0x00020000
#define KAPI_CLONE_SYSVSEM        0x00040000
#define KAPI_CLONE_SETTLS         0x00080000
#define KAPI_CLONE_PARENT_SETTID  0x00100000
#define KAPI_CLONE_CHILD_CLEARTID 0x00200000
#define KAPI_CLONE_DETACHED       0x00400000
#define KAPI_CLONE_UNTRACED       0x00800000
#define KAPI_CLONE_CHILD_SETTID   0x01000000
#define KAPI_CLONE_NEWCGROUP      0x02000000
#define KAPI_CLONE_NEWUTS         0x04000000
#define KAPI_CLONE_NEWIPC         0x08000000
#define KAPI_CLONE_NEWPID         0x10000000
#define KAPI_CLONE_NEWNET         0x20000000
#define KAPI_CLONE_IO             0x40000000
#define KAPI_CLONE_NEWTIME        0x80000000

#define KAPI_CSIGNAL              0x000000FF

#define KAPI_CLONE_CLEAR_SIGHAND  0x100000000ULL
#define KAPI_CLONE_INTO_CGROUP    0x200000000ULL

typedef struct {
    uint64_t flags;
    void*    stack_base;
    size_t   stack_size;
    int*     parent_tidptr;
    int*     child_tidptr;
    uint64_t tls;
    int      exit_signal;
} kapi_clone_args_t;

typedef struct {
    int      pid;
    int      tid;
    void*    stack;
    size_t   stack_size;
    uint64_t tls;
    int*     clear_tid;
    int*     set_tid;
} kapi_clone_result_t;

typedef int (*kapi_clone_fn_t)(void* arg);

int kapi_clone(uint64_t flags, void* stack, int* parent_tidptr, int* child_tidptr, uint64_t tls);

int kapi_clone3(const kapi_clone_args_t* args, size_t size);

int kapi_fork(void);

int kapi_vfork(void);

int kapi_daemon(int nochdir, int noclose);

pid_t kapi_getpid(void);
pid_t kapi_gettid(void);
pid_t kapi_getppid(void);
pid_t kapi_getpgid(pid_t pid);
pid_t kapi_getsid(pid_t pid);

int kapi_setpgid(pid_t pid, pid_t pgid);
int kapi_setsid(void);

uid_t kapi_getuid(void);
uid_t kapi_geteuid(void);
gid_t kapi_getgid(void);
gid_t kapi_getegid(void);

int kapi_setuid(uid_t uid);
int kapi_setgid(gid_t gid);
int kapi_seteuid(uid_t euid);
int kapi_setegid(gid_t egid);
int kapi_setreuid(uid_t ruid, uid_t euid);
int kapi_setregid(gid_t rgid, gid_t egid);
int kapi_setresuid(uid_t ruid, uid_t euid, uid_t suid);
int kapi_setresgid(gid_t rgid, gid_t egid, gid_t sgid);
int kapi_getresuid(uid_t* ruid, uid_t* euid, uid_t* suid);
int kapi_getresgid(gid_t* rgid, gid_t* egid, gid_t* sgid);

int kapi_getgroups(int size, gid_t* list);
int kapi_setgroups(int size, const gid_t* list);

int kapi_waitpid(pid_t pid, int* wstatus, int options);
int kapi_wait4(pid_t pid, int* wstatus, int options, void* rusage);
int kapi_waitid(int idtype, pid_t id, kapi_siginfo_t* infop, int options);

#define KAPI_WNOHANG    0x00000001
#define KAPI_WUNTRACED  0x00000002
#define KAPI_WSTOPPED   KAPI_WUNTRACACED
#define KAPI_WCONTINUED 0x00000008
#define KAPI_WNOWAIT    0x01000000
#define KAPI_WEXITED    0x00000004

#define KAPI_P_ALL       0
#define KAPI_P_PID       1
#define KAPI_P_PGID      2
#define KAPI_P_PIDFD     3

static inline int KAPI_WIFEXITED(int status)   { return ((status) & 0x7f) == 0; }
static inline int KAPI_WEXITSTATUS(int status)  { return ((status) & 0xff00) >> 8; }
static inline int KAPI_WIFSIGNALED(int status)  { return ((signed char)(((status) & 0x7f) + 1) >> 1) > 0; }
static inline int KAPI_WTERMSIG(int status)     { return (status) & 0x7f; }
static inline int KAPI_WIFSTOPPED(int status)   { return ((status) & 0xff) == 0x7f; }
static inline int KAPI_WSTOPSIG(int status)     { return ((status) & 0xff00) >> 8; }
static inline int KAPI_WIFCONTINUED(int status) { return status == 0xffff; }

int kapi_execve(const char* pathname, char* const* argv, char* const* envp);
int kapi_execv(const char* pathname, char* const* argv);
int kapi_execvp(const char* file, char* const* argv);
int kapi_execvpe(const char* file, char* const* argv, char* const* envp);

int kapi_prctl(int option, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5);

#define KAPI_PR_SET_PDEATHSIG  1
#define KAPI_PR_GET_PDEATHSIG  2
#define KAPI_PR_GET_DUMPABLE   3
#define KAPI_PR_SET_DUMPABLE   4
#define KAPI_PR_GET_UNALIGN    5
#define KAPI_PR_SET_UNALIGN    6
#define KAPI_PR_GET_KEEPCAPS   7
#define KAPI_PR_SET_KEEPCAPS   8
#define KAPI_PR_GET_FPEMU      9
#define KAPI_PR_SET_FPEMU     10
#define KAPI_PR_GET_FPEXC     11
#define KAPI_PR_SET_FPEXC     12
#define KAPI_PR_GET_TIMING    13
#define KAPI_PR_SET_TIMING    14
#define KAPI_PR_SET_NAME      15
#define KAPI_PR_GET_NAME      16
#define KAPI_PR_GET_ENDIAN    20
#define KAPI_PR_SET_ENDIAN    21
#define KAPI_PR_GET_SECCOMP   21
#define KAPI_PR_SET_SECCOMP   22
#define KAPI_PR_CAPBSET_READ  23
#define KAPI_PR_CAPBSET_DROP  24
#define KAPI_PR_GET_TSC       25
#define KAPI_PR_SET_TSC       26
#define KAPI_PR_GET_SECUREBITS 27
#define KAPI_PR_SET_SECUREBITS 28
#define KAPI_PR_SET_TIMERSLACK 29
#define KAPI_PR_GET_TIMERSLACK 30
#define KAPI_PR_TASK_PERF_EVENTS_DISABLE 31
#define KAPI_PR_TASK_PERF_EVENTS_ENABLE  32
#define KAPI_PR_MCE_KILL     33
#define KAPI_PR_MCE_KILL_GET 34
#define KAPI_PR_SET_MM       35
#define KAPI_PR_GET_TID_ADDRESS 40
#define KAPI_PR_SET_CHILD_SUBREAPER 36
#define KAPI_PR_GET_CHILD_SUBREAPER 37
#define KAPI_PR_SET_NO_NEW_PRIVS 38
#define KAPI_PR_GET_NO_NEW_PRIVS 39
#define KAPI_PR_SET_VMA      40
#define KAPI_PR_GET_VMA      41
#define KAPI_PR_SET_SPECULATION_CTRL 53
#define KAPI_PR_GET_SPECULATION_CTRL 52

#ifdef __cplusplus
}
#endif

#endif