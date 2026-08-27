#include "kapi_security_ext.h"
#include "kapi.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

static kapi_cap_t current_caps;
static kapi_cap_t ambient_caps;
static int caps_initialized = 0;

static kapi_security_id_t next_secid = 1;

typedef struct {
    kapi_security_id_t sid;
    char ctx[256];
    int valid;
} kapi_secctx_entry_t;

#define KAPI_SECCTX_MAX  256
static kapi_secctx_entry_t secctx_table[KAPI_SECCTX_MAX];

#define KAPI_AUDIT_MAX_CALLBACKS  8
#define KAPI_AUDIT_FILTER_MAX    32

typedef struct {
    int type;
    int result;
    char pattern[128];
    int active;
} kapi_audit_filter_t;

static int audit_enabled = 1;
static kapi_audit_callback_t audit_callbacks[KAPI_AUDIT_MAX_CALLBACKS];
static int audit_callback_count = 0;
static kapi_audit_filter_t audit_filters[KAPI_AUDIT_FILTER_MAX];
static int audit_filter_count = 0;
static int audit_seqno = 0;

int kapi_security_ext_init(void)
{
    memset(&current_caps, 0, sizeof(current_caps));
    memset(&ambient_caps, 0, sizeof(ambient_caps));
    memset(&current_caps.effective, true, sizeof(current_caps.effective));
    memset(&current_caps.permitted, true, sizeof(current_caps.permitted));
    caps_initialized = 1;
    memset(secctx_table, 0, sizeof(secctx_table));
    memset(audit_callbacks, 0, sizeof(audit_callbacks));
    memset(audit_filters, 0, sizeof(audit_filters));
    audit_callback_count = 0;
    audit_filter_count = 0;
    audit_enabled = 1;
    audit_seqno = 0;
    return KAPI_OK;
}

int kapi_cap_get(kapi_cap_t* caps)
{
    if (!caps) return KAPI_EINVAL;
    *caps = current_caps;
    return KAPI_OK;
}

int kapi_cap_set(const kapi_cap_t* caps)
{
    if (!caps) return KAPI_EINVAL;
    current_caps = *caps;
    return KAPI_OK;
}

bool kapi_cap_check(int cap)
{
    if (cap < 0 || cap > KAPI_CAP_LAST_CAP) return false;
    return current_caps.effective[cap];
}

int kapi_cap_raise(int cap)
{
    if (cap < 0 || cap > KAPI_CAP_LAST_CAP) return KAPI_EINVAL;
    current_caps.effective[cap] = true;
    current_caps.permitted[cap] = true;
    return KAPI_OK;
}

int kapi_cap_lower(int cap)
{
    if (cap < 0 || cap > KAPI_CAP_LAST_CAP) return KAPI_EINVAL;
    current_caps.effective[cap] = false;
    return KAPI_OK;
}

int kapi_cap_set_ambient(int cap)
{
    if (cap < 0 || cap > KAPI_CAP_LAST_CAP) return KAPI_EINVAL;
    ambient_caps.effective[cap] = true;
    current_caps.permitted[cap] = true;
    return KAPI_OK;
}

bool kapi_cap_has_ambient(int cap)
{
    if (cap < 0 || cap > KAPI_CAP_LAST_CAP) return false;
    return ambient_caps.effective[cap];
}

int kapi_cap_clear(void)
{
    memset(&current_caps, 0, sizeof(current_caps));
    memset(&ambient_caps, 0, sizeof(ambient_caps));
    return KAPI_OK;
}

static const kapi_cap_name_t cap_names[] = {
    { "CAP_CHOWN",            KAPI_CAP_CHOWN },
    { "CAP_DAC_OVERRIDE",     KAPI_CAP_DAC_OVERRIDE },
    { "CAP_DAC_READ_SEARCH",  KAPI_CAP_DAC_READ_SEARCH },
    { "CAP_FOWNER",           KAPI_CAP_FOWNER },
    { "CAP_FSETID",           KAPI_CAP_FSETID },
    { "CAP_KILL",             KAPI_CAP_KILL },
    { "CAP_SETGID",           KAPI_CAP_SETGID },
    { "CAP_SETUID",           KAPI_CAP_SETUID },
    { "CAP_SETPCAP",          KAPI_CAP_SETPCAP },
    { "CAP_LINUX_IMMUTABLE",  KAPI_CAP_LINUX_IMMUTABLE },
    { "CAP_NET_BIND_SERVICE", KAPI_CAP_NET_BIND_SERVICE },
    { "CAP_NET_BROADCAST",    KAPI_CAP_NET_BROADCAST },
    { "CAP_NET_ADMIN",        KAPI_CAP_NET_ADMIN },
    { "CAP_NET_RAW",          KAPI_CAP_NET_RAW },
    { "CAP_IPC_LOCK",         KAPI_CAP_IPC_LOCK },
    { "CAP_IPC_OWNER",        KAPI_CAP_IPC_OWNER },
    { "CAP_SYS_MODULE",       KAPI_CAP_SYS_MODULE },
    { "CAP_SYS_RAWIO",        KAPI_CAP_SYS_RAWIO },
    { "CAP_SYS_CHROOT",       KAPI_CAP_SYS_CHROOT },
    { "CAP_SYS_PTRACE",       KAPI_CAP_SYS_PTRACE },
    { "CAP_SYS_PACCT",        KAPI_CAP_SYS_PACCT },
    { "CAP_SYS_ADMIN",        KAPI_CAP_SYS_ADMIN },
    { "CAP_SYS_BOOT",         KAPI_CAP_SYS_BOOT },
    { "CAP_SYS_NICE",         KAPI_CAP_SYS_NICE },
    { "CAP_SYS_RESOURCE",     KAPI_CAP_SYS_RESOURCE },
    { "CAP_SYS_TIME",         KAPI_CAP_SYS_TIME },
    { "CAP_SYS_TTY_CONFIG",   KAPI_CAP_SYS_TTY_CONFIG },
    { "CAP_MKNOD",            KAPI_CAP_MKNOD },
    { "CAP_LEASE",            KAPI_CAP_LEASE },
    { "CAP_AUDIT_WRITE",      KAPI_AUDIT_WRITE },
    { "CAP_AUDIT_CONTROL",    KAPI_AUDIT_CONTROL },
    { "CAP_SETFCAP",          KAPI_CAP_SETFCAP },
    { "CAP_MAC_OVERRIDE",     KAPI_MAC_OVERRIDE },
    { "CAP_MAC_ADMIN",        KAPI_MAC_ADMIN },
    { "CAP_SYSLOG",           KAPI_SYSLOG },
    { "CAP_WAKE_ALARM",       KAPI_WAKE_ALARM },
    { "CAP_BLOCK_SUSPEND",    KAPI_BLOCK_SUSPEND },
    { "CAP_AUDIT_READ",       KAPI_AUDIT_READ },
    { "CAP_PERFMON",          KAPI_PERFMON },
    { "CAP_BPF",              KAPI_BPF },
    { "CAP_CHECKPOINT_RESTORE", KAPI_CHECKPOINT_RESTORE },
};

const char* kapi_cap_to_name(int cap)
{
    for (size_t i = 0; i < sizeof(cap_names) / sizeof(cap_names[0]); i++) {
        if (cap_names[i].value == cap) return cap_names[i].name;
    }
    return "CAP_UNKNOWN";
}

int kapi_cap_from_name(const char* name)
{
    if (!name) return -1;
    for (size_t i = 0; i < sizeof(cap_names) / sizeof(cap_names[0]); i++) {
        if (strcmp(cap_names[i].name, name) == 0) return cap_names[i].value;
    }
    return -1;
}

kapi_security_id_t kapi_secctx_to_secid(const char* secctx, size_t seclen)
{
    if (!secctx) return 0;
    for (int i = 0; i < KAPI_SECCTX_MAX; i++) {
        if (secctx_table[i].valid && strncmp(secctx_table[i].ctx, secctx, seclen) == 0) {
            return secctx_table[i].sid;
        }
    }
    kapi_security_id_t sid = next_secid++;
    for (int i = 0; i < KAPI_SECCTX_MAX; i++) {
        if (!secctx_table[i].valid) {
            secctx_table[i].sid = sid;
            size_t copy_len = seclen < sizeof(secctx_table[i].ctx) - 1 ? seclen : sizeof(secctx_table[i].ctx) - 1;
            memcpy(secctx_table[i].ctx, secctx, copy_len);
            secctx_table[i].ctx[copy_len] = '\0';
            secctx_table[i].valid = 1;
            return sid;
        }
    }
    return sid;
}

const char* kapi_secid_to_secctx(kapi_security_id_t secid, size_t* seclen)
{
    for (int i = 0; i < KAPI_SECCTX_MAX; i++) {
        if (secctx_table[i].valid && secctx_table[i].sid == secid) {
            if (seclen) *seclen = strlen(secctx_table[i].ctx);
            return secctx_table[i].ctx;
        }
    }
    if (seclen) *seclen = 0;
    return NULL;
}

int kapi_secid_to_ctx(kapi_security_id_t secid, char** ctx)
{
    if (!ctx) return KAPI_EINVAL;
    for (int i = 0; i < KAPI_SECCTX_MAX; i++) {
        if (secctx_table[i].valid && secctx_table[i].sid == secid) {
            size_t len = strlen(secctx_table[i].ctx) + 1;
            *ctx = (char*)kapi_malloc(len);
            if (!*ctx) return KAPI_ENOMEM;
            memcpy(*ctx, secctx_table[i].ctx, len);
            return KAPI_OK;
        }
    }
    return KAPI_ENOENT;
}

void kapi_release_secctx(char* secctx, size_t seclen)
{
    (void)seclen;
    if (secctx) kapi_free(secctx);
}

int kapi_secmark_relabel_packet(const char* secctx)
{
    (void)secctx;
    return KAPI_OK;
}

const char* kapi_secmark_get_connlabel(uint32_t conn_label)
{
    (void)conn_label;
    return NULL;
}

int kapi_secmark_set_connlabel(uint32_t conn_label, const char* label)
{
    (void)conn_label; (void)label;
    return KAPI_OK;
}

int kapi_security_compute_av(kapi_security_id_t ssid,
                             kapi_security_id_t tsid,
                             kapi_security_class_t tclass,
                             kapi_access_vector_t requested,
                             kapi_av_decision_t* avd)
{
    if (!avd) return KAPI_EINVAL;
    memset(avd, 0, sizeof(*avd));
    avd->source_sid = ssid;
    avd->target_sid = tsid;
    avd->tclass = tclass;
    avd->requested = requested;
    avd->decided = requested;
    avd->result = 0;
    return KAPI_OK;
}

int kapi_security_transition_sid(kapi_security_id_t ssid,
                                kapi_security_id_t tsid,
                                kapi_security_class_t tclass,
                                kapi_security_id_t* newsid)
{
    if (!newsid) return KAPI_EINVAL;
    *newsid = tsid;
    (void)ssid; (void)tclass;
    return KAPI_OK;
}

int kapi_security_member_sid(kapi_security_id_t ssid,
                             kapi_security_id_t tsid,
                             kapi_security_id_t* out_sid)
{
    if (!out_sid) return KAPI_EINVAL;
    *out_sid = tsid;
    (void)ssid;
    return KAPI_OK;
}

int kapi_security_sid_to_context(kapi_security_id_t sid,
                                 char** context)
{
    if (!context) return KAPI_EINVAL;
    for (int i = 0; i < KAPI_SECCTX_MAX; i++) {
        if (secctx_table[i].valid && secctx_table[i].sid == sid) {
            size_t len = strlen(secctx_table[i].ctx) + 1;
            *context = (char*)kapi_malloc(len);
            if (!*context) return KAPI_ENOMEM;
            memcpy(*context, secctx_table[i].ctx, len);
            return KAPI_OK;
        }
    }
    return KAPI_ENOENT;
}

int kapi_security_context_to_sid(const char* context,
                                 kapi_security_id_t* out_sid)
{
    if (!context || !out_sid) return KAPI_EINVAL;
    size_t ctxlen = strlen(context);
    for (int i = 0; i < KAPI_SECCTX_MAX; i++) {
        if (secctx_table[i].valid &&
            strncmp(secctx_table[i].ctx, context, ctxlen) == 0 &&
            secctx_table[i].ctx[ctxlen] == '\0') {
            *out_sid = secctx_table[i].sid;
            return KAPI_OK;
        }
    }
    kapi_security_id_t sid = next_secid++;
    for (int i = 0; i < KAPI_SECCTX_MAX; i++) {
        if (!secctx_table[i].valid) {
            secctx_table[i].sid = sid;
            size_t copy_len = ctxlen < sizeof(secctx_table[i].ctx) - 1
                              ? ctxlen : sizeof(secctx_table[i].ctx) - 1;
            memcpy(secctx_table[i].ctx, context, copy_len);
            secctx_table[i].ctx[copy_len] = '\0';
            secctx_table[i].valid = 1;
            *out_sid = sid;
            return KAPI_OK;
        }
    }
    return KAPI_ENOMEM;
}

int kapi_security_port_sid(uint16_t port, uint8_t protocol,
                           kapi_security_id_t* out_sid)
{
    if (!out_sid) return KAPI_EINVAL;
    (void)port; (void)protocol;
    *out_sid = 0;
    return KAPI_OK;
}

int kapi_security_netif_sid(const char* name,
                            kapi_security_id_t* ifsid)
{
    if (!name || !ifsid) return KAPI_EINVAL;
    *ifsid = 0;
    return KAPI_OK;
}

int kapi_security_node_sid(uint16_t addr_family, void* addr,
                           kapi_security_id_t* out_sid)
{
    if (!addr || !out_sid) return KAPI_EINVAL;
    (void)addr_family;
    *out_sid = 0;
    return KAPI_OK;
}

int kapi_security_fs_use(const char* path,
                         kapi_security_class_t* behavior,
                         kapi_security_id_t* out_sid)
{
    if (!path || !behavior || !out_sid) return KAPI_EINVAL;
    *behavior = KAPI_SECCLASS_FILE;
    *out_sid = 0;
    return KAPI_OK;
}

int kapi_security_getprocattr(int field, char** value)
{
    if (!value) return KAPI_EINVAL;
    (void)field;
    *value = NULL;
    return KAPI_ENOENT;
}

int kapi_security_setprocattr(const char* name, void* value, size_t size)
{
    if (!name || !value) return KAPI_EINVAL;
    (void)size;
    return KAPI_OK;
}

int kapi_security_create(const char* filename, mode_t mode)
{
    if (!filename) return KAPI_EINVAL;
    (void)mode;
    return KAPI_OK;
}

int kapi_security_access(const char* filename, int mask)
{
    if (!filename) return KAPI_EINVAL;
    (void)mask;
    return KAPI_OK;
}

int kapi_security_link(const char* oldname, const char* newname)
{
    if (!oldname || !newname) return KAPI_EINVAL;
    return KAPI_OK;
}

int kapi_security_unlink(const char* filename)
{
    if (!filename) return KAPI_EINVAL;
    return KAPI_OK;
}

int kapi_security_rename(const char* oldname, const char* newname)
{
    if (!oldname || !newname) return KAPI_EINVAL;
    return KAPI_OK;
}

int kapi_security_mkdir(const char* dirname, mode_t mode)
{
    if (!dirname) return KAPI_EINVAL;
    (void)mode;
    return KAPI_OK;
}

int kapi_security_rmdir(const char* dirname)
{
    if (!dirname) return KAPI_EINVAL;
    return KAPI_OK;
}

int kapi_security_mknod(const char* filename, mode_t mode, dev_t dev)
{
    if (!filename) return KAPI_EINVAL;
    (void)mode; (void)dev;
    return KAPI_OK;
}

#ifdef KAL_KERNEL

int kapi_security_chmod(struct dentry* dentry, struct vfsmnt* mnt, mode_t mode)
{
    if (!dentry) return KAPI_EINVAL;
    (void)mnt; (void)mode;
    return KAPI_OK;
}

int kapi_security_chown(struct dentry* dentry, struct vfsmnt* mnt,
                         uid_t user, gid_t group)
{
    if (!dentry) return KAPI_EINVAL;
    (void)mnt; (void)user; (void)group;
    return KAPI_OK;
}

int kapi_security_truncate(struct dentry* dentry, loff_t length)
{
    if (!dentry) return KAPI_EINVAL;
    (void)length;
    return KAPI_OK;
}

int kapi_security_getattr(struct vfsmnt* mnt, struct dentry* dentry)
{
    if (!dentry) return KAPI_EINVAL;
    (void)mnt;
    return KAPI_OK;
}

int kapi_security_setattr(struct dentry* dentry, struct iattr* iattr)
{
    if (!dentry) return KAPI_EINVAL;
    (void)iattr;
    return KAPI_OK;
}

int kapi_security_setxattr(struct dentry* dentry, const char* name,
                            const void* value, size_t size, int flags)
{
    if (!dentry || !name) return KAPI_EINVAL;
    (void)value; (void)size; (void)flags;
    return KAPI_OK;
}

int kapi_security_removexattr(struct dentry* dentry, const char* name)
{
    if (!dentry || !name) return KAPI_EINVAL;
    return KAPI_OK;
}

int kapi_security_getxattr(struct dentry* dentry, const char* name,
                            void* value, size_t size)
{
    if (!dentry || !name) return KAPI_EINVAL;
    (void)value; (void)size;
    return KAPI_OK;
}

int kapi_security_listxattr(struct dentry* dentry, char* list, size_t list_size)
{
    if (!dentry) return KAPI_EINVAL;
    (void)list; (void)list_size;
    return KAPI_OK;
}

int kapi_security_socket_create(int family, int type, int protocol, int kern)
{
    (void)family; (void)type; (void)protocol; (void)kern;
    return KAPI_OK;
}

int kapi_security_socket_post_create(struct socket* sock, int family,
                                      int type, int protocol, int kern)
{
    if (!sock) return KAPI_EINVAL;
    (void)family; (void)type; (void)protocol; (void)kern;
    return KAPI_OK;
}

int kapi_security_socket_bind(struct socket* sock,
                               struct sockaddr* address, int addrlen)
{
    if (!sock || !address) return KAPI_EINVAL;
    (void)addrlen;
    return KAPI_OK;
}

int kapi_security_socket_connect(struct socket* sock,
                                  struct sockaddr* address, int addrlen)
{
    if (!sock || !address) return KAPI_EINVAL;
    (void)addrlen;
    return KAPI_OK;
}

int kapi_security_socket_listen(struct socket* sock, int backlog)
{
    if (!sock) return KAPI_EINVAL;
    (void)backlog;
    return KAPI_OK;
}

int kapi_security_socket_accept(struct socket* sock, struct socket* newsock)
{
    if (!sock || !newsock) return KAPI_EINVAL;
    return KAPI_OK;
}

int kapi_security_socket_sendmsg(struct socket* sock, struct msghdr* msg,
                                  int size)
{
    if (!sock || !msg) return KAPI_EINVAL;
    (void)size;
    return KAPI_OK;
}

int kapi_security_socket_recvmsg(struct socket* sock, struct msghdr* msg,
                                  int size, int flags)
{
    if (!sock || !msg) return KAPI_EINVAL;
    (void)size; (void)flags;
    return KAPI_OK;
}

int kapi_security_socket_getsockname(struct socket* sock)
{
    if (!sock) return KAPI_EINVAL;
    return KAPI_OK;
}

int kapi_security_socket_getpeername(struct socket* sock)
{
    if (!sock) return KAPI_EINVAL;
    return KAPI_OK;
}

int kapi_security_socket_setsockopt(struct socket* sock, int level,
                                     int optname)
{
    if (!sock) return KAPI_EINVAL;
    (void)level; (void)optname;
    return KAPI_OK;
}

int kapi_security_socket_getsockopt(struct socket* sock, int level,
                                     int optname)
{
    if (!sock) return KAPI_EINVAL;
    (void)level; (void)optname;
    return KAPI_OK;
}

int kapi_security_socket_shutdown(struct socket* sock, int how)
{
    if (!sock) return KAPI_EINVAL;
    (void)how;
    return KAPI_OK;
}

int kapi_task_create(unsigned long clone_flags)
{
    (void)clone_flags;
    return KAPI_OK;
}

int kapi_task_alloc_security(struct task_struct* p)
{
    if (!p) return KAPI_EINVAL;
    return KAPI_OK;
}

void kapi_task_free_security(struct task_struct* p)
{
    (void)p;
}

int kapi_task_setuid(uid_t id0, uid_t id1, uid_t id2, uid_t id3,
                      struct cred* new)
{
    if (!new) return KAPI_EINVAL;
    (void)id0; (void)id1; (void)id2; (void)id3;
    return KAPI_OK;
}

int kapi_task_setgid(gid_t id0, gid_t id1, gid_t id2, gid_t id3,
                      struct cred* new)
{
    if (!new) return KAPI_EINVAL;
    (void)id0; (void)id1; (void)id2; (void)id3;
    return KAPI_OK;
}

int kapi_task_setpgid(struct task_struct* p, pid_t pgid)
{
    if (!p) return KAPI_EINVAL;
    (void)pgid;
    return KAPI_OK;
}

int kapi_task_getpgid(struct task_struct* p)
{
    if (!p) return KAPI_EINVAL;
    return KAPI_OK;
}

int kapi_task_getsid(struct task_struct* p)
{
    if (!p) return KAPI_EINVAL;
    return KAPI_OK;
}

void kapi_task_getsecid(struct task_struct* p, u32* secid)
{
    if (!p || !secid) return;
    *secid = 0;
}

int kapi_task_setnice(struct task_struct* p, int nice)
{
    if (!p) return KAPI_EINVAL;
    (void)nice;
    return KAPI_OK;
}

int kapi_task_setscheduler(struct task_struct* p, int policy,
                            struct sched_param* lp)
{
    if (!p) return KAPI_EINVAL;
    (void)policy; (void)lp;
    return KAPI_OK;
}

int kapi_task_getscheduler(struct task_struct* p)
{
    if (!p) return KAPI_EINVAL;
    return KAPI_OK;
}

int kapi_task_movememory(struct task_struct* p)
{
    if (!p) return KAPI_EINVAL;
    return KAPI_OK;
}

int kapi_task_kill(struct task_struct* p, struct siginfo* info,
                    int sig, u32 secid)
{
    if (!p) return KAPI_EINVAL;
    (void)info; (void)sig; (void)secid;
    return KAPI_OK;
}

int kapi_task_wait(struct task_struct* p)
{
    if (!p) return KAPI_EINVAL;
    return KAPI_OK;
}

int kapi_task_prctl(int option, unsigned long arg2, unsigned long arg3,
                     unsigned long arg4, unsigned long arg5)
{
    (void)option; (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    return KAPI_ENOSYS;
}

#endif /* KAL_KERNEL */

int kapi_audit_init(void)
{
    memset(audit_callbacks, 0, sizeof(audit_callbacks));
    memset(audit_filters, 0, sizeof(audit_filters));
    audit_callback_count = 0;
    audit_filter_count = 0;
    audit_enabled = 1;
    audit_seqno = 0;
    return KAPI_OK;
}

static int kapi_audit_match_filter(int type, int result, const char* message)
{
    for (int i = 0; i < KAPI_AUDIT_FILTER_MAX; i++) {
        if (!audit_filters[i].active) continue;
        if (audit_filters[i].type != type) continue;
        if (audit_filters[i].result != 0 && audit_filters[i].result != result) continue;
        if (audit_filters[i].pattern[0] != '\0') {
            if (!message || strstr(message, audit_filters[i].pattern) == NULL)
                continue;
        }
        return 1;
    }
    return 0;
}

void kapi_audit_log(int type, const char* fmt, ...)
{
    if (!audit_enabled) return;

    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    int result = 0;
    if (kapi_audit_match_filter(type, result, buf)) return;

    for (int i = 0; i < audit_callback_count; i++) {
        if (audit_callbacks[i]) {
            audit_callbacks[i](type, buf, result);
        }
    }
}

void kapi_audit_syscall_entry(int arch, int major,
                               unsigned long a0, unsigned long a1,
                               unsigned long a2, unsigned long a3)
{
    if (!audit_enabled) return;
    audit_seqno++;
    kapi_audit_log(1, "syscall entry arch=%d major=%d a0=%lu a1=%lu a2=%lu a3=%lu seqno=%d",
                   arch, major, a0, a1, a2, a3, audit_seqno);
}

void kapi_audit_syscall_exit(int success, long return_code)
{
    if (!audit_enabled) return;
    kapi_audit_log(2, "syscall exit success=%d ret=%ld seqno=%d",
                   success, return_code, audit_seqno);
}

int kapi_register_audit_callback(kapi_audit_callback_t callback)
{
    if (!callback) return KAPI_EINVAL;
    if (audit_callback_count >= KAPI_AUDIT_MAX_CALLBACKS) return KAPI_ENOMEM;
    audit_callbacks[audit_callback_count++] = callback;
    return KAPI_OK;
}

int kapi_unregister_audit_callback(kapi_audit_callback_t callback)
{
    if (!callback) return KAPI_EINVAL;
    for (int i = 0; i < audit_callback_count; i++) {
        if (audit_callbacks[i] == callback) {
            audit_callbacks[i] = audit_callbacks[audit_callback_count - 1];
            audit_callbacks[audit_callback_count - 1] = NULL;
            audit_callback_count--;
            return KAPI_OK;
        }
    }
    return KAPI_ENOENT;
}

int kapi_enable_audit(bool enable)
{
    audit_enabled = enable ? 1 : 0;
    return KAPI_OK;
}

bool kapi_is_audit_enabled(void)
{
    return audit_enabled != 0;
}

int kapi_set_audit_filter(int type, int result, const char* pattern)
{
    if (audit_filter_count >= KAPI_AUDIT_FILTER_MAX) return KAPI_ENOMEM;
    for (int i = 0; i < KAPI_AUDIT_FILTER_MAX; i++) {
        if (!audit_filters[i].active) {
            audit_filters[i].type = type;
            audit_filters[i].result = result;
            if (pattern) {
                size_t plen = strlen(pattern);
                size_t copy_len = plen < sizeof(audit_filters[i].pattern) - 1
                                  ? plen : sizeof(audit_filters[i].pattern) - 1;
                memcpy(audit_filters[i].pattern, pattern, copy_len);
                audit_filters[i].pattern[copy_len] = '\0';
            } else {
                audit_filters[i].pattern[0] = '\0';
            }
            audit_filters[i].active = 1;
            audit_filter_count++;
            return KAPI_OK;
        }
    }
    return KAPI_ENOMEM;
}

int kapi_clear_audit_filters(void)
{
    memset(audit_filters, 0, sizeof(audit_filters));
    audit_filter_count = 0;
    return KAPI_OK;
}