#ifndef KAPI_SIGNAL_H
#define KAPI_SIGNAL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_SIGHUP       1
#define KAPI_SIGINT       2
#define KAPI_SIGQUIT      3
#define KAPI_SIGILL       4
#define KAPI_SIGTRAP      5
#define KAPI_SIGABRT      6
#define KAPI_SIGIOT       6
#define KAPI_SIGBUS       7
#define KAPI_SIGFPE       8
#define KAPI_SIGKILL      9
#define KAPI_SIGUSR1      10
#define KAPI_SIGSEGV      11
#define KAPI_SIGUSR2      12
#define KAPI_SIGPIPE      13
#define KAPI_SIGALRM      14
#define KAPI_SIGTERM      15
#define KAPI_SIGSTKFLT    16
#define KAPI_SIGCHLD      17
#define KAPI_SIGCONT      18
#define KAPI_SIGSTOP      19
#define KAPI_SIGTSTP      20
#define KAPI_SIGTTIN      21
#define KAPI_SIGTTOU      22
#define KAPI_SIGURG       23
#define KAPI_SIGXCPU      24
#define KAPI_SIGXFSZ      25
#define KAPI_SIGVTALRM    26
#define KAPI_SIGPROF      27
#define KAPI_SIGWINCH     28
#define KAPI_SIGIO        29
#define KAPI_SIGPWR       30
#define KAPI_SIGSYS       31
#define KAPI_SIGRTMIN     32
#define KAPI_SIGRTMAX     64

#define KAPI_SIG_BLOCK    0
#define KAPI_SIG_UNBLOCK  1
#define KAPI_SIG_SETMASK  2

#define KAPI_SA_NOCLDSTOP  0x00000001
#define KAPI_SA_NOCLDWAIT  0x00000002
#define KAPI_SA_SIGINFO    0x00000004
#define KAPI_SA_ONSTACK    0x08000000
#define KAPI_SA_RESTART    0x10000000
#define KAPI_SA_NODEFER    0x40000000
#define KAPI_SA_RESETHAND  0x80000000

#define KAPI_SI_USER       0
#define KAPI_SI_KERNEL     0x80
#define KAPI_SI_QUEUE      -1
#define KAPI_SI_TIMER      -2
#define KAPI_SI_MESGQ      -3
#define KAPI_SI_ASYNCIO    -4
#define KAPI_SI_SIGIO      -5
#define KAPI_SI_TKILL      -6
#define KAPI_SI_DETHREAD   -7
#define KAPI_SI_ASYNCNL    -60

#define KAPI_ILL_ILLOPC    1
#define KAPI_ILL_ILLOPN    2
#define KAPI_ILL_ILLADR    3
#define KAPI_ILL_ILLTRP    4
#define KAPI_ILL_PRVOPC    5
#define KAPI_ILL_PRVREG    6
#define KAPI_ILL_COPROC    7
#define KAPI_ILL_BADSTK    8

#define KAPI_FPE_INTDIV    1
#define KAPI_FPE_INTOVF    2
#define KAPI_FPE_FLTDIV    3
#define KAPI_FPE_FLTOVF    4
#define KAPI_FPE_FLTUND    5
#define KAPI_FPE_FLTRES    6
#define KAPI_FPE_FLTINV    7
#define KAPI_FPE_FLTSUB    8

#define KAPI_SEGV_MAPERR   1
#define KAPI_SEGV_ACCERR   2
#define KAPI_SEGV_BNDERR   3
#define KAPI_SEGV_PKUERR   4

#define KAPI_BUS_ADRALN    1
#define KAPI_BUS_ADRERR    2
#define KAPI_BUS_OBJERR    3
#define KAPI_BUS_MCEERR_AR 4
#define KAPI_BUS_MCEERR_AO 5

#define KAPI_CLD_EXITED    1
#define KAPI_CLD_KILLED    2
#define KAPI_CLD_DUMPED    3
#define KAPI_CLD_TRAPPED   4
#define KAPI_CLD_STOPPED   5
#define KAPI_CLD_CONTINUED 6

#define KAPI_TRAP_BRKPT    1
#define KAPI_TRAP_TRACE    2

#define KAPI_POLL_IN       1
#define KAPI_POLL_OUT      2
#define KAPI_POLL_MSG      3
#define KAPI_POLL_ERR      4
#define KAPI_POLL_PRI      5
#define KAPI_POLL_HUP      6

typedef struct {
    uint64_t sig[1];
} kapi_sigset_t;

#define KAPI_SIGSET_WORDS  1

static inline void kapi_sigemptyset(kapi_sigset_t* set)
{
    set->sig[0] = 0;
}

static inline void kapi_sigfillset(kapi_sigset_t* set)
{
    set->sig[0] = ~0ULL;
}

static inline int kapi_sigaddset(kapi_sigset_t* set, int signum)
{
    if (signum < 1 || signum > 64) return -1;
    set->sig[0] |= (1ULL << (signum - 1));
    return 0;
}

static inline int kapi_sigdelset(kapi_sigset_t* set, int signum)
{
    if (signum < 1 || signum > 64) return -1;
    set->sig[0] &= ~(1ULL << (signum - 1));
    return 0;
}

static inline int kapi_sigismember(const kapi_sigset_t* set, int signum)
{
    if (signum < 1 || signum > 64) return 0;
    return (set->sig[0] & (1ULL << (signum - 1))) != 0;
}

typedef struct {
    int32_t si_signo;
    int32_t si_errno;
    int32_t si_code;
    int32_t si_trapno;
    int32_t si_pid;
    int32_t si_uid;
    int32_t si_status;
    void*   si_addr;
    int32_t si_band;
    union {
        struct {
            int32_t _timerid;
            int32_t _overrun;
        } _timer;
        struct {
            int32_t _mqid;
        } _mq;
        struct {
            void* _addr;
            int32_t _addr_lsb;
        } _sigfault;
        struct {
            int64_t _utime;
            int64_t _stime;
        } _sigchld;
    } _sifields;
} kapi_siginfo_t;

typedef void (*kapi_sighandler_t)(int);
typedef void (*kapi_sigaction_handler_t)(int, kapi_siginfo_t*, void*);

struct kapi_sigaction {
    union {
        kapi_sighandler_t sa_handler;
        kapi_sigaction_handler_t sa_sigaction;
    } _sa_handler;
    kapi_sigset_t sa_mask;
    uint32_t      sa_flags;
    void*         sa_restorer;
};

typedef struct kapi_sigaction kapi_sigaction_t;

typedef struct {
    void*    ss_sp;
    uint32_t ss_flags;
    size_t   ss_size;
} kapi_stack_t;

#define KAPI_SS_ONSTACK   1
#define KAPI_SS_DISABLE   2

#define KAPI_MINSIGSTKSZ 2048
#define KAPI_SIGSTKSZ     8192

typedef struct {
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t rdi, rsi, rbp, rbx, rdx, rax, rcx, rsp;
    uint64_t rip;
    uint64_t eflags;
    uint16_t cs, ss, ds, es, fs, gs;
    void*    fpstate;
    uint64_t __reserved1[8];
} kapi_ucontext_t;

struct kapi_sigframe {
    kapi_siginfo_t  info;
    kapi_ucontext_t uc;
    void*           retcode;
};

int kapi_signal_init(void);

int kapi_sigaction(int signum, const kapi_sigaction_t* act, kapi_sigaction_t* oldact);

kapi_sighandler_t kapi_signal(int signum, kapi_sighandler_t handler);

int kapi_sigprocmask(int how, const kapi_sigset_t* set, kapi_sigset_t* oldset);

int kapi_sigpending(kapi_sigset_t* set);

int kapi_sigsuspend(const kapi_sigset_t* mask);

int kapi_sigwait(const kapi_sigset_t* set, int* sig);

int kapi_sigwaitinfo(const kapi_sigset_t* set, kapi_siginfo_t* info);

int kapi_sigtimedwait(const kapi_sigset_t* set, kapi_siginfo_t* info, const struct timespec* timeout);

int kapi_kill(int pid, int sig);

int kapi_tkill(int tid, int sig);

int kapi_tgkill(int tgid, int tid, int sig);

int kapi_raise(int sig);

int kapi_sigaltstack(const kapi_stack_t* ss, kapi_stack_t* old_ss);

int kapi_siginterrupt(int sig, int flag);

int kapi_sigismember_ext(const kapi_sigset_t* set, int signum);

int kapi_sigisemptyset(const kapi_sigset_t* set);

int kapi_sigandset(kapi_sigset_t* dest, const kapi_sigset_t* left, const kapi_sigset_t* right);

int kapi_sigorset(kapi_sigset_t* dest, const kapi_sigset_t* left, const kapi_sigset_t* right);

int kapi_signotset(kapi_sigset_t* dest, const kapi_sigset_t* src);

int kapi_sigqueue(int pid, int sig, const union sigval value);

int kapi_pause(void);

uint64_t kapi_signal_frame_size(void);

int kapi_setup_sigframe(void* frame, int signum, kapi_siginfo_t* info, kapi_ucontext_t* uc);

int kapi_restore_sigframe(void* frame, kapi_ucontext_t* uc);

#ifdef __cplusplus
}
#endif

#endif