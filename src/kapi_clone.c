#include "kapi_clone.h"
#include "kapi.h"
#include "kapi_signal.h"
#include "kapi_elf.h"
#include <string.h>

static int next_pid = 1;
static int next_tid = 1;
static int current_pid = 0;

typedef struct {
    int pid;
    int ppid;
    uid_t uid, euid, suid, fsuid;
    gid_t gid, egid, sgid, fsgid;
    int pgid, sid;
    kapi_sigset_t sig_blocked;
    char name[32];
} kapi_task_t;

#define KAPI_TASK_MAX  1024
static kapi_task_t task_table[KAPI_TASK_MAX];
static int task_count = 0;

int kapi_clone_init(void)
{
    memset(task_table, 0, sizeof(task_table));
    next_pid = 1;
    next_tid = 1;
    current_pid = 0;

    task_table[0].pid = 1;
    task_table[0].ppid = 0;
    task_table[0].uid = 0;
    task_table[0].euid = 0;
    task_table[0].suid = 0;
    task_table[0].fsuid = 0;
    task_table[0].gid = 0;
    task_table[0].egid = 0;
    task_table[0].sgid = 0;
    task_table[0].fsgid = 0;
    task_table[0].pgid = 1;
    task_table[0].sid = 1;
    strncpy(task_table[0].name, "init", sizeof(task_table[0].name) - 1);
    current_pid = 1;
    task_count = 1;

    return KAPI_OK;
}

static int alloc_pid(void)
{
    for (int i = 0; i < KAPI_TASK_MAX; i++) {
        if (task_table[i].pid == 0) {
            return i + 1;
        }
    }
    return -1;
}

static kapi_task_t* find_task(int pid)
{
    if (pid <= 0 || pid > KAPI_TASK_MAX) return NULL;
    if (task_table[pid - 1].pid != pid) return NULL;
    return &task_table[pid - 1];
}

static void copy_parent_to_child(int child_idx, int parent_idx)
{
    task_table[child_idx] = task_table[parent_idx];
    task_table[child_idx].ppid = parent_idx + 1;
    task_table[child_idx].pid = child_idx + 1;
}

static uint64_t clone_entry_wrapper(void* arg)
{
    kapi_clone_fn_t fn = (kapi_clone_fn_t)arg;
    return (uint64_t)(fn(NULL));
}

int kapi_clone(uint64_t flags, void* stack, int* parent_tidptr, int* child_tidptr, uint64_t tls)
{
    int idx = alloc_pid();
    if (idx < 0) return -1;

    int parent_idx = current_pid - 1;
    if (parent_idx < 0 || parent_idx >= KAPI_TASK_MAX) return -1;

    memset(&task_table[idx], 0, sizeof(kapi_task_t));
    task_table[idx].pid = idx + 1;
    task_table[idx].ppid = current_pid;

    if (!(flags & KAPI_CLONE_FS)) {
        task_table[idx].uid = task_table[parent_idx].uid;
        task_table[idx].euid = task_table[parent_idx].euid;
        task_table[idx].gid = task_table[parent_idx].gid;
        task_table[idx].egid = task_table[parent_idx].egid;
    }

    if (!(flags & KAPI_CLONE_FILES)) {
        task_table[idx].pgid = task_table[parent_idx].pgid;
        task_table[idx].sid = task_table[parent_idx].sid;
    }

    if (!(flags & KAPI_CLONE_SIGHAND)) {
        task_table[idx].sig_blocked = task_table[parent_idx].sig_blocked;
    }

    strncpy(task_table[idx].name, task_table[parent_idx].name, sizeof(task_table[idx].name) - 1);

    if (parent_tidptr) *parent_tidptr = idx + 1;
    if (child_tidptr) *child_tidptr = idx + 1;
    (void)tls;

    if (stack && !(flags & KAPI_CLONE_VM)) {
        size_t stack_size = 8192 * 16;
        memcpy(stack, stack, stack_size);
    }

    task_count++;
    return idx + 1;
}

int kapi_clone3(const kapi_clone_args_t* args, size_t size)
{
    (void)size;
    if (!args) return KAPI_EINVAL;

    return kapi_clone(args->flags, args->stack_base,
                      args->parent_tidptr, args->child_tidptr,
                      args->tls);
}

int kapi_fork(void)
{
    return kapi_clone(KAPI_SIGCHLD | KAPI_CLONE_PARENT_SETTID |
                      KAPI_CLONE_CHILD_CLEARTID | KAPI_CHILD_SETTID,
                      NULL, NULL, NULL, 0);
}

int kapi_vfork(void)
{
    return kapi_clone(KAPI_VFORK | KAPI_SIGCHLD | KAPI_CLONE_VM |
                      KAPI_CLONE_PARENT_SETTID | KAPI_CLONE_CHILD_CLEARTID,
                      NULL, NULL, NULL, 0);
}

int kapi_daemon(int nochdir, int noclose)
{
    int ret = kapi_fork();
    if (ret < 0) return -1;
    if (ret > 0) _exit(0);

    if (!nochdir) kapi_chdir("/");
    if (!noclose) {
        kapi_close(0); kapi_close(1); kapi_close(2);
    }

    return 0;
}

pid_t kapi_getpid(void)
{
    return current_pid;
}

pid_t kapi_gettid(void)
{
    return current_pid;
}

pid_t kapi_getppid(void)
{
    kapi_task_t* t = find_task(current_pid);
    if (!t) return 0;
    return t->ppid;
}

pid_t kapi_getpgid(pid_t pid)
{
    if (pid == 0) pid = current_pid;
    kapi_task_t* t = find_task(pid);
    if (!t) return -1;
    return t->pgid;
}

pid_t kapi_getsid(pid_t pid)
{
    if (pid == 0) pid = current_pid;
    kapi_task_t* t = find_task(pid);
    if (!t) return -1;
    return t->sid;
}

int kapi_setpgid(pid_t pid, pid_t pgid)
{
    if (pid == 0) pid = current_pid;
    if (pgid == 0) pgid = current_pid;

    kapi_task_t* t = find_task(pid);
    if (!t) return KAPI_EINVAL;
    t->pgid = pgid;
    return KAPI_OK;
}

int kapi_setsid(void)
{
    kapi_task_t* t = find_task(current_pid);
    if (!t) return KAPI_EINVAL;

    for (int i = 0; i < KAPI_TASK_MAX; i++) {
        if (task_table[i].pid && task_table[i].pgid == current_pid) {
            return KAPI_EPERM;
        }
    }

    t->sid = current_pid;
    t->pgid = current_pid;
    return current_pid;
}

uid_t kapi_getuid(void)
{
    kapi_task_t* t = find_task(current_pid);
    return t ? t->uid : 0;
}

uid_t kapi_geteuid(void)
{
    kapi_task_t* t = find_task(current_pid);
    return t ? t->euid : 0;
}

gid_t kapi_getgid(void)
{
    kapi_task_t* t = find_task(current_pid);
    return t ? t->gid : 0;
}

gid_t kapi_getegid(void)
{
    kapi_task_t* t = find_task(current_pid);
    return t ? t->egid : 0;
}

int kapi_setuid(uid_t uid)
{
    kapi_task_t* t = find_task(current_pid);
    if (!t) return KAPI_EINVAL;
    t->uid = uid;
    t->euid = uid;
    t->fsuid = uid;
    return KAPI_OK;
}

int kapi_setgid(gid_t gid)
{
    kapi_task_t* t = find_task(current_pid);
    if (!t) return KAPI_EINVAL;
    t->gid = gid;
    t->egid = gid;
    t->fsgid = gid;
    return KAPI_OK;
}

int kapi_seteuid(uid_t euid)
{
    kapi_task_t* t = find_task(current_pid);
    if (!t) return KAPI_EINVAL;
    t->euid = euid;
    t->fsuid = euid;
    return KAPI_OK;
}

int kapi_setegid(gid_t egid)
{
    kapi_task_t* t = find_task(current_pid);
    if (!t) return KAPI_EINVAL;
    t->egid = egid;
    t->fsgid = egid;
    return KAPI_OK;
}

int kapi_setreuid(uid_t ruid, uid_t euid)
{
    kapi_task_t* t = find_task(current_pid);
    if (!t) return KAPI_EINVAL;
    t->uid = ruid;
    t->euid = euid;
    t->fsuid = euid;
    return KAPI_OK;
}

int kapi_setregid(gid_t rgid, gid_t egid)
{
    kapi_task_t* t = find_task(current_pid);
    if (!t) return KAPI_EINVAL;
    t->gid = rgid;
    t->egid = egid;
    t->fsgid = egid;
    return KAPI_OK;
}

int kapi_setresuid(uid_t ruid, uid_t euid, uid_t suid)
{
    kapi_task_t* t = find_task(current_pid);
    if (!t) return KAPI_EINVAL;
    t->uid = ruid;
    t->euid = euid;
    t->suid = suid;
    t->fsuid = euid;
    return KAPI_OK;
}

int kapi_setresgid(gid_t rgid, gid_t egid, gid_t sgid)
{
    kapi_task_t* t = find_task(current_pid);
    if (!t) return KAPI_EINVAL;
    t->gid = rgid;
    t->egid = egid;
    t->sgid = sgid;
    t->fsgid = egid;
    return KAPI_OK;
}

int kapi_getresuid(uid_t* ruid, uid_t* euid, uid_t* suid)
{
    kapi_task_t* t = find_task(current_pid);
    if (!t || !ruid || !euid || !suid) return KAPI_EINVAL;
    *ruid = t->uid;
    *euid = t->euid;
    *suid = t->suid;
    return KAPI_OK;
}

int kapi_getresgid(gid_t* rgid, gid_t* egid, gid_t* sgid)
{
    kapi_task_t* t = find_task(current_pid);
    if (!t || !rgid || !egid || !sgid) return KAPI_EINVAL;
    *rgid = t->gid;
    *egid = t->egid;
    *sgid = t->sgid;
    return KAPI_OK;
}

int kapi_getgroups(int size, gid_t* list)
{
    (void)size; (void)list;
    return 0;
}

int kapi_setgroups(int size, const gid_t* list)
{
    (void)size; (void)list;
    return KAPI_OK;
}

int kapi_waitpid(pid_t pid, int* wstatus, int options)
{
    (void)options;

    if (pid > 0) {
        for (;;) {
            kapi_task_t* t = find_task(pid);
            if (!t) {
                if (wstatus) *wstatus = 0;
                return pid;
            }
            kapi_proc_yield();
        }
    } else if (pid == -1) {
        for (;;) {
            for (int i = 0; i < KAPI_TASK_MAX; i++) {
                if (task_table[i].pid && task_table[i].ppid == current_pid &&
                    task_table[i].state == 3) {
                    int found = task_table[i].pid;
                    if (wstatus) *wstatus = 0;
                    return found;
                }
            }
            kapi_proc_yield();
        }
    } else if (pid == 0) {
        kapi_task_t* me = find_task(current_pid);
        if (!me) return KAPI_ECHILD;
        for (;;) {
            for (int i = 0; i < KAPI_TASK_MAX; i++) {
                if (task_table[i].pid && task_table[i].ppid == current_pid &&
                    task_table[i].pgid == me->pgid &&
                    task_table[i].state == 3) {
                    int found = task_table[i].pid;
                    if (wstatus) *wstatus = 0;
                    return found;
                }
            }
            kapi_proc_yield();
        }
    }

    return KAPI_ECHILD;
}

int kapi_wait4(pid_t pid, int* wstatus, int options, void* rusage)
{
    (void)rusage;
    return kapi_waitpid(pid, wstatus, options);
}

int kapi_waitid(int idtype, pid_t id, kapi_siginfo_t* infop, int options)
{
    (void)idtype; (void)id; (void)infop; (void)options;
    return KAPI_OK;
}

int kapi_execve(const char* pathname, char* const* argv, char* const* envp)
{
    if (!pathname) return KAPI_EINVAL;

    kapi_elf_image_t* image = kapi_elf_load(pathname);
    if (!image) return KAPI_ERROR;

    if (image->is_dynamic) {
        kapi_elf_apply_relocations(image);
        kapi_elf_resolve_plt(image);
        kapi_elf_run_init(image);
    }

    kapi_elf_load_info_t info = kapi_elf_map_binary(image);
    kapi_elf_setup_auxv(&info, argv ? 0 : 0, argv, envp);

    void (*entry)(void) = (void(*)(void))(uintptr_t)info.entry;
    entry();

    kapi_elf_unload(image);
    return KAPI_OK;
}

int kapi_execv(const char* pathname, char* const* argv)
{
    return kapi_execve(pathname, argv, NULL);
}

int kapi_execvp(const char* file, char* const* argv)
{
    if (!file) return KAPI_EINVAL;
    return kapi_execve(file, argv, NULL);
}

int kapi_execvpe(const char* file, char* const* argv, char* const* envp)
{
    if (!file) return KAPI_EINVAL;
    return kapi_execve(file, argv, envp);
}

int kapi_prctl(int option, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5)
{
    kapi_task_t* t = find_task(current_pid);
    if (!t) return KAPI_EINVAL;

    switch (option) {
    case KAPI_PR_SET_NAME:
        if ((const char*)arg2) {
            strncpy(t->name, (const char*)arg2, sizeof(t->name) - 1);
            t->name[sizeof(t->name) - 1] = '\0';
        }
        return KAPI_OK;

    case KAPI_PR_GET_NAME:
        if ((char*)arg2) {
            strncpy((char*)arg2, t->name, 15);
            ((char*)arg2)[15] = '\0';
        }
        return KAPI_OK;

    case KAPI_PR_GET_PDEATHSIG:
        return 0;

    case KAPI_PR_SET_PDEATHSIG:
        return KAPI_OK;

    case KAPI_PR_SET_DUMPABLE:
    case KAPI_PR_GET_DUMPABLE:
        return KAPI_OK;

    case KAPI_PR_SET_NO_NEW_PRIVS:
        return KAPI_OK;

    case KAPI_PR_GET_NO_NEW_PRIVS:
        return 0;

    default:
        break;
    }

    (void)arg3; (void)arg4; (void)arg5;
    return KAPI_ENOSYS;
}