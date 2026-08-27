#include "kapi_signal.h"
#include "kapi.h"
#include <string.h>

#define KAPI_SIGNAL_MAX  64

static kapi_sigaction_t sig_actions[KAPI_SIGNAL_MAX + 1];
static kapi_sigset_t sig_blocked;
static kapi_sigset_t sig_pending_global;
static kapi_stack_t sig_altstack;

int kapi_signal_init(void)
{
    memset(sig_actions, 0, sizeof(sig_actions));
    kapi_sigemptyset(&sig_blocked);
    kapi_sigaddset(&sig_blocked, KAPI_SIGKILL);
    kapi_sigaddset(&sig_blocked, KAPI_SIGSTOP);
    kapi_sigemptyset(&sig_pending_global);
    memset(&sig_altstack, 0, sizeof(sig_altstack));
    return KAPI_OK;
}

int kapi_sigaction(int signum, const kapi_sigaction_t* act, kapi_sigaction_t* oldact)
{
    if (signum < 1 || signum > KAPI_SIGNAL_MAX) return KAPI_EINVAL;
    if (signum == KAPI_SIGKILL || signum == KAPI_SIGSTOP) return KAPI_EINVAL;

    if (oldact) {
        *oldact = sig_actions[signum];
    }

    if (act) {
        sig_actions[signum] = *act;
        if (act->sa_flags & KAPI_SA_RESETHAND) {
            sig_actions[signum]._sa_handler.sa_handler = NULL;
        }
    }

    return KAPI_OK;
}

kapi_sighandler_t kapi_signal(int signum, kapi_sighandler_t handler)
{
    if (signum < 1 || signum > KAPI_SIGNAL_MAX) return NULL;
    if (signum == KAPI_SIGKILL || signum == KAPI_SIGSTOP) return NULL;

    kapi_sighandler_t old = sig_actions[signum]._sa_handler.sa_handler;

    kapi_sigaction_t act;
    memset(&act, 0, sizeof(act));
    act._sa_handler.sa_handler = handler;
    kapi_sigemptyset(&act.sa_mask);
    act.sa_flags = KAPI_SA_RESTART;

    kapi_sigaction(signum, &act, NULL);

    return old;
}

int kapi_sigprocmask(int how, const kapi_sigset_t* set, kapi_sigset_t* oldset)
{
    if (oldset) {
        *oldset = sig_blocked;
    }

    if (set) {
        kapi_sigset_t newset = *set;
        kapi_sigaddset(&newset, KAPI_SIGKILL);
        kapi_sigaddset(&newset, KAPI_SIGSTOP);

        switch (how) {
        case KAPI_SIG_BLOCK:
            sig_blocked.sig[0] |= newset.sig[0];
            break;
        case KAPI_SIG_UNBLOCK:
            sig_blocked.sig[0] &= ~newset.sig[0];
            break;
        case KAPI_SIG_SETMASK:
            sig_blocked = newset;
            break;
        default:
            return KAPI_EINVAL;
        }
    }

    return KAPI_OK;
}

int kapi_sigpending(kapi_sigset_t* set)
{
    if (!set) return KAPI_EINVAL;
    set->sig[0] = sig_pending_global.sig[0] & ~sig_blocked.sig[0];
    return KAPI_OK;
}

int kapi_sigsuspend(const kapi_sigset_t* mask)
{
    if (!mask) return KAPI_EINVAL;

    kapi_sigset_t old_blocked = sig_blocked;
    kapi_sigset_t new_mask = *mask;
    kapi_sigaddset(&new_mask, KAPI_SIGKILL);
    kapi_sigaddset(&new_mask, KAPI_SIGSTOP);
    sig_blocked = new_mask;

    kapi_proc_yield();

    sig_blocked = old_blocked;
    return KAPI_ERROR;
}

int kapi_sigwait(const kapi_sigset_t* set, int* sig)
{
    if (!set || !sig) return KAPI_EINVAL;

    for (;;) {
        for (int i = 1; i <= KAPI_SIGNAL_MAX; i++) {
            if (kapi_sigismember(set, i) && kapi_sigismember(&sig_pending_global, i)) {
                kapi_sigdelset(&sig_pending_global, i);
                *sig = i;
                return KAPI_OK;
            }
        }
        kapi_proc_yield();
    }
}

int kapi_sigwaitinfo(const kapi_sigset_t* set, kapi_siginfo_t* info)
{
    int sig;
    int ret = kapi_sigwait(set, &sig);
    if (ret != KAPI_OK) return ret;

    if (info) {
        memset(info, 0, sizeof(*info));
        info->si_signo = sig;
        info->si_code = KAPI_SI_USER;
    }

    return sig;
}

int kapi_sigtimedwait(const kapi_sigset_t* set, kapi_siginfo_t* info, const struct timespec* timeout)
{
    if (!set) return KAPI_EINVAL;

    uint64_t deadline = 0;
    if (timeout) {
        deadline = kapi_get_uptime_ms() + (uint64_t)timeout->tv_sec * 1000 + (uint64_t)timeout->tv_nsec / 1000000;
    }

    for (;;) {
        for (int i = 1; i <= KAPI_SIGNAL_MAX; i++) {
            if (kapi_sigismember(set, i) && kapi_sigismember(&sig_pending_global, i)) {
                kapi_sigdelset(&sig_pending_global, i);
                if (info) {
                    memset(info, 0, sizeof(*info));
                    info->si_signo = i;
                    info->si_code = KAPI_SI_USER;
                }
                return i;
            }
        }

        if (timeout && kapi_get_uptime_ms() >= deadline) {
            return KAPI_EAGAIN;
        }

        kapi_proc_yield();
    }
}

int kapi_kill(int pid, int sig)
{
    if (sig < 0 || sig > KAPI_SIGNAL_MAX) return KAPI_EINVAL;
    if (sig == 0) return KAPI_OK;

    if (pid > 0) {
        kapi_sigaddset(&sig_pending_global, sig);
        return KAPI_OK;
    }

    if (pid == 0) {
        kapi_sigaddset(&sig_pending_global, sig);
        return KAPI_OK;
    }

    if (pid == -1) {
        kapi_sigaddset(&sig_pending_global, sig);
        return KAPI_OK;
    }

    kapi_sigaddset(&sig_pending_global, sig);
    return KAPI_OK;
}

int kapi_tkill(int tid, int sig)
{
    if (sig < 0 || sig > KAPI_SIGNAL_MAX) return KAPI_EINVAL;
    (void)tid;
    kapi_sigaddset(&sig_pending_global, sig);
    return KAPI_OK;
}

int kapi_tgkill(int tgid, int tid, int sig)
{
    if (sig < 0 || sig > KAPI_SIGNAL_MAX) return KAPI_EINVAL;
    (void)tgid; (void)tid;
    kapi_sigaddset(&sig_pending_global, sig);
    return KAPI_OK;
}

int kapi_raise(int sig)
{
    return kapi_kill(kapi_proc_get_pid(kapi_proc_current()), sig);
}

int kapi_sigaltstack(const kapi_stack_t* ss, kapi_stack_t* old_ss)
{
    if (old_ss) {
        *old_ss = sig_altstack;
    }

    if (ss) {
        if (ss->ss_size < KAPI_MINSIGSTKSZ) return KAPI_EINVAL;
        if (sig_altstack.ss_flags & KAPI_SS_ONSTACK) return KAPI_EPERM;
        sig_altstack = *ss;
        sig_altstack.ss_flags = 0;
    }

    return KAPI_OK;
}

int kapi_siginterrupt(int sig, int flag)
{
    if (sig < 1 || sig > KAPI_SIGNAL_MAX) return KAPI_EINVAL;

    if (flag) {
        sig_actions[sig].sa_flags &= ~KAPI_SA_RESTART;
    } else {
        sig_actions[sig].sa_flags |= KAPI_SA_RESTART;
    }

    return KAPI_OK;
}

int kapi_sigismember_ext(const kapi_sigset_t* set, int signum)
{
    return kapi_sigismember(set, signum);
}

int kapi_sigisemptyset(const kapi_sigset_t* set)
{
    if (!set) return 1;
    return set->sig[0] == 0;
}

int kapi_sigandset(kapi_sigset_t* dest, const kapi_sigset_t* left, const kapi_sigset_t* right)
{
    if (!dest || !left || !right) return KAPI_EINVAL;
    dest->sig[0] = left->sig[0] & right->sig[0];
    return KAPI_OK;
}

int kapi_sigorset(kapi_sigset_t* dest, const kapi_sigset_t* left, const kapi_sigset_t* right)
{
    if (!dest || !left || !right) return KAPI_EINVAL;
    dest->sig[0] = left->sig[0] | right->sig[0];
    return KAPI_OK;
}

int kapi_signotset(kapi_sigset_t* dest, const kapi_sigset_t* src)
{
    if (!dest || !src) return KAPI_EINVAL;
    dest->sig[0] = ~src->sig[0];
    return KAPI_OK;
}

int kapi_sigqueue(int pid, int sig, const union sigval value)
{
    (void)value;
    return kapi_kill(pid, sig);
}

int kapi_pause(void)
{
    kapi_proc_yield();
    return KAPI_ERROR;
}

uint64_t kapi_signal_frame_size(void)
{
    return sizeof(struct kapi_sigframe);
}

int kapi_setup_sigframe(void* frame, int signum, kapi_siginfo_t* info, kapi_ucontext_t* uc)
{
    if (!frame) return KAPI_EINVAL;

    struct kapi_sigframe* sf = (struct kapi_sigframe*)frame;
    memset(sf, 0, sizeof(*sf));

    sf->info.si_signo = signum;
    if (info) {
        sf->info = *info;
    } else {
        sf->info.si_signo = signum;
        sf->info.si_code = KAPI_SI_USER;
        sf->info.si_pid = kapi_proc_get_pid(kapi_proc_current());
    }

    if (uc) {
        sf->uc = *uc;
    }

    return KAPI_OK;
}

int kapi_restore_sigframe(void* frame, kapi_ucontext_t* uc)
{
    if (!frame || !uc) return KAPI_EINVAL;
    struct kapi_sigframe* sf = (struct kapi_sigframe*)frame;
    *uc = sf->uc;
    return KAPI_OK;
}