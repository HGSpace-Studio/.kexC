#include "include/cgroup.h"
#include <arch/types.h>
#include <arch/spinlock.h>
#include <string.h>
#include <slab.h>

cgroup_hierarchy_t cgroup_hierarchies[CGROUP_MAX_HIERARCHIES];
cgroup_subsys_t *cgroup_subsys[CGROUP_MAX_SUBSYS];
cgroup_t *cgroup_root_default = NULL;
u64 cgroup_next_id = 1;

static spinlock_t cgroup_id_lock = SPINLOCK_INIT;
static spinlock_t cgroup_subsys_lock = SPINLOCK_INIT;
static cgroup_subsys_t builtin_subsys_cpu;
static cgroup_subsys_t builtin_subsys_mem;
static cgroup_subsys_t builtin_subsys_blkio;
static cgroup_subsys_t builtin_subsys_dev;

static cgroup_subsys_state_t *cpu_css_alloc(cgroup_t *cgrp)
{
    cgroup_cpu_state_t *state = kzalloc(sizeof(cgroup_cpu_state_t));
    if (!state) return NULL;
    cgroup_subsys_state_t *css = kzalloc(sizeof(cgroup_subsys_state_t));
    if (!css) { kfree(state); return NULL; }
    state->cpu_shares = CPU_SHARES_DEFAULT;
    state->cpu_cfs_period_us = CPU_CFS_PERIOD_US;
    state->cpu_cfs_quota_us = CPU_CFS_QUOTA_US;
    css->subsys_id = CGROUP_SUBSYS_CPU;
    css->cgroup = cgrp;
    css->priv = state;
    spin_init(&css->lock);
    return css;
}

static void cpu_css_free(cgroup_subsys_state_t *css)
{
    if (!css) return;
    if (css->priv) kfree(css->priv);
    kfree(css);
}

static int cpu_attach(cgroup_t *cgrp, cgroup_task_t *task)
{
    (void)cgrp;
    (void)task;
    return 0;
}

static void cpu_detach(cgroup_t *cgrp, cgroup_task_t *task)
{
    (void)cgrp;
    (void)task;
}

static int cpu_fork(cgroup_t *cgrp, cgroup_task_t *parent, cgroup_task_t *child)
{
    (void)cgrp;
    (void)parent;
    (void)child;
    return 0;
}

static void cpu_exit(cgroup_t *cgrp, cgroup_task_t *task)
{
    (void)cgrp;
    (void)task;
}

static cgroup_subsys_state_t *mem_css_alloc(cgroup_t *cgrp)
{
    cgroup_memory_state_t *state = kzalloc(sizeof(cgroup_memory_state_t));
    if (!state) return NULL;
    cgroup_subsys_state_t *css = kzalloc(sizeof(cgroup_subsys_state_t));
    if (!css) { kfree(state); return NULL; }
    state->memory_limit_in_bytes = MEM_LIMIT_IN_BYTES_MAX;
    state->memory_usage_in_bytes = 0;
    state->memory_max_usage_in_bytes = 0;
    state->memory_failcnt = 0;
    state->memory_swap_limit_in_bytes = MEM_LIMIT_IN_BYTES_MAX;
    state->memory_swap_usage_in_bytes = 0;
    state->memory_kmem_limit_in_bytes = MEM_LIMIT_IN_BYTES_MAX;
    state->memory_kmem_usage_in_bytes = 0;
    state->oom_control = 0;
    state->under_oom = 0;
    state->oom_kill_count = 0;
    state->oom_notify_mask = 0;
    state->oom_notify = NULL;
    css->subsys_id = CGROUP_SUBSYS_MEMORY;
    css->cgroup = cgrp;
    css->priv = state;
    spin_init(&css->lock);
    return css;
}

static void mem_css_free(cgroup_subsys_state_t *css)
{
    if (!css) return;
    if (css->priv) kfree(css->priv);
    kfree(css);
}

static int mem_attach(cgroup_t *cgrp, cgroup_task_t *task)
{
    (void)cgrp;
    (void)task;
    return 0;
}

static void mem_detach(cgroup_t *cgrp, cgroup_task_t *task)
{
    (void)cgrp;
    (void)task;
}

static int mem_fork(cgroup_t *cgrp, cgroup_task_t *parent, cgroup_task_t *child)
{
    (void)cgrp;
    (void)parent;
    (void)child;
    return 0;
}

static void mem_exit(cgroup_t *cgrp, cgroup_task_t *task)
{
    (void)cgrp;
    (void)task;
}

static cgroup_subsys_state_t *blkio_css_alloc(cgroup_t *cgrp)
{
    cgroup_blkio_state_t *state = kzalloc(sizeof(cgroup_blkio_state_t));
    if (!state) return NULL;
    cgroup_subsys_state_t *css = kzalloc(sizeof(cgroup_subsys_state_t));
    if (!css) { kfree(state); return NULL; }
    state->blkio_throttle_read_bps = (u64)(-1);
    state->blkio_throttle_write_bps = (u64)(-1);
    state->blkio_throttle_read_iops = (u64)(-1);
    state->blkio_throttle_write_iops = (u64)(-1);
    state->blkio_weight = BLKIO_WEIGHT_DEFAULT;
    css->subsys_id = CGROUP_SUBSYS_BLKIO;
    css->cgroup = cgrp;
    css->priv = state;
    spin_init(&css->lock);
    return css;
}

static void blkio_css_free(cgroup_subsys_state_t *css)
{
    if (!css) return;
    if (css->priv) kfree(css->priv);
    kfree(css);
}

static int blkio_attach(cgroup_t *cgrp, cgroup_task_t *task)
{
    (void)cgrp;
    (void)task;
    return 0;
}

static void blkio_detach(cgroup_t *cgrp, cgroup_task_t *task)
{
    (void)cgrp;
    (void)task;
}

static int blkio_fork(cgroup_t *cgrp, cgroup_task_t *parent, cgroup_task_t *child)
{
    (void)cgrp;
    (void)parent;
    (void)child;
    return 0;
}

static void blkio_exit(cgroup_t *cgrp, cgroup_task_t *task)
{
    (void)cgrp;
    (void)task;
}

static cgroup_subsys_state_t *dev_css_alloc(cgroup_t *cgrp)
{
    cgroup_devices_state_t *state = kzalloc(sizeof(cgroup_devices_state_t));
    if (!state) return NULL;
    cgroup_subsys_state_t *css = kzalloc(sizeof(cgroup_subsys_state_t));
    if (!css) { kfree(state); return NULL; }
    state->whitelist_head = NULL;
    state->deny_all = 1;
    css->subsys_id = CGROUP_SUBSYS_DEVICES;
    css->cgroup = cgrp;
    css->priv = state;
    spin_init(&css->lock);
    return css;
}

static void dev_css_free(cgroup_subsys_state_t *css)
{
    if (!css) return;
    cgroup_devices_state_t *state = (cgroup_devices_state_t *)css->priv;
    if (state) {
        cgroup_device_whitelist_t *entry = state->whitelist_head;
        while (entry) {
            cgroup_device_whitelist_t *next = entry->next;
            kfree(entry);
            entry = next;
        }
        kfree(state);
    }
    kfree(css);
}

static int dev_attach(cgroup_t *cgrp, cgroup_task_t *task)
{
    (void)cgrp;
    (void)task;
    return 0;
}

static void dev_detach(cgroup_t *cgrp, cgroup_task_t *task)
{
    (void)cgrp;
    (void)task;
}

static int dev_fork(cgroup_t *cgrp, cgroup_task_t *parent, cgroup_task_t *child)
{
    (void)cgrp;
    (void)parent;
    (void)child;
    return 0;
}

static void dev_exit(cgroup_t *cgrp, cgroup_task_t *task)
{
    (void)cgrp;
    (void)task;
}

static void init_builtin_subsystems(void)
{
    memset(&builtin_subsys_cpu, 0, sizeof(builtin_subsys_cpu));
    strncpy(builtin_subsys_cpu.name, "cpu", CGROUP_SUBSYS_NAME_MAX);
    builtin_subsys_cpu.id = CGROUP_SUBSYS_CPU;
    builtin_subsys_cpu.css_alloc = cpu_css_alloc;
    builtin_subsys_cpu.css_free = cpu_css_free;
    builtin_subsys_cpu.attach = cpu_attach;
    builtin_subsys_cpu.detach = cpu_detach;
    builtin_subsys_cpu.fork = cpu_fork;
    builtin_subsys_cpu.exit = cpu_exit;
    builtin_subsys_cpu.enabled = 1;

    memset(&builtin_subsys_mem, 0, sizeof(builtin_subsys_mem));
    strncpy(builtin_subsys_mem.name, "memory", CGROUP_SUBSYS_NAME_MAX);
    builtin_subsys_mem.id = CGROUP_SUBSYS_MEMORY;
    builtin_subsys_mem.css_alloc = mem_css_alloc;
    builtin_subsys_mem.css_free = mem_css_free;
    builtin_subsys_mem.attach = mem_attach;
    builtin_subsys_mem.detach = mem_detach;
    builtin_subsys_mem.fork = mem_fork;
    builtin_subsys_mem.exit = mem_exit;
    builtin_subsys_mem.enabled = 1;

    memset(&builtin_subsys_blkio, 0, sizeof(builtin_subsys_blkio));
    strncpy(builtin_subsys_blkio.name, "blkio", CGROUP_SUBSYS_NAME_MAX);
    builtin_subsys_blkio.id = CGROUP_SUBSYS_BLKIO;
    builtin_subsys_blkio.css_alloc = blkio_css_alloc;
    builtin_subsys_blkio.css_free = blkio_css_free;
    builtin_subsys_blkio.attach = blkio_attach;
    builtin_subsys_blkio.detach = blkio_detach;
    builtin_subsys_blkio.fork = blkio_fork;
    builtin_subsys_blkio.exit = blkio_exit;
    builtin_subsys_blkio.enabled = 1;

    memset(&builtin_subsys_dev, 0, sizeof(builtin_subsys_dev));
    strncpy(builtin_subsys_dev.name, "devices", CGROUP_SUBSYS_NAME_MAX);
    builtin_subsys_dev.id = CGROUP_SUBSYS_DEVICES;
    builtin_subsys_dev.css_alloc = dev_css_alloc;
    builtin_subsys_dev.css_free = dev_css_free;
    builtin_subsys_dev.attach = dev_attach;
    builtin_subsys_dev.detach = dev_detach;
    builtin_subsys_dev.fork = dev_fork;
    builtin_subsys_dev.exit = dev_exit;
    builtin_subsys_dev.enabled = 1;

    cgroup_subsys[CGROUP_SUBSYS_CPU] = &builtin_subsys_cpu;
    cgroup_subsys[CGROUP_SUBSYS_MEMORY] = &builtin_subsys_mem;
    cgroup_subsys[CGROUP_SUBSYS_BLKIO] = &builtin_subsys_blkio;
    cgroup_subsys[CGROUP_SUBSYS_DEVICES] = &builtin_subsys_dev;
}

static void alloc_css_for_subsys(cgroup_t *cgrp, int subsys_id)
{
    cgroup_subsys_t *ss = cgroup_subsys[subsys_id];
    if (!ss || !ss->enabled || !ss->css_alloc) return;
    if (cgrp->subsys_state[subsys_id]) return;
    cgrp->subsys_state[subsys_id] = ss->css_alloc(cgrp);
}

int cgroup_init(void)
{
    memset(cgroup_hierarchies, 0, sizeof(cgroup_hierarchies));
    memset(cgroup_subsys, 0, sizeof(cgroup_subsys));
    cgroup_root_default = NULL;
    cgroup_next_id = 1;
    spin_init(&cgroup_id_lock);
    spin_init(&cgroup_subsys_lock);

    init_builtin_subsystems();

    cgroup_hierarchies[0].subsys_bitmask = (1ULL << CGROUP_SUBSYS_CPU)
                                         | (1ULL << CGROUP_SUBSYS_MEMORY)
                                         | (1ULL << CGROUP_SUBSYS_BLKIO)
                                         | (1ULL << CGROUP_SUBSYS_DEVICES);
    cgroup_hierarchies[0].hierarchy_id = 0;
    strncpy(cgroup_hierarchies[0].name, "unified", CGROUP_NAME_MAX);
    spin_init(&cgroup_hierarchies[0].lock);
    cgroup_hierarchies[0].active = 1;

    cgroup_root_default = cgroup_create(NULL, "/");
    if (cgroup_root_default) {
        cgroup_root_default->hierarchy = &cgroup_hierarchies[0];
        cgroup_hierarchies[0].root_cgroup = cgroup_root_default;
    }

    return 0;
}

static u64 alloc_cgroup_id(void)
{
    u64 id;
    spin_lock(&cgroup_id_lock);
    id = cgroup_next_id++;
    spin_unlock(&cgroup_id_lock);
    return id;
}

cgroup_t *cgroup_create(cgroup_t *parent, const char *name)
{
    if (!name || name[0] == '\0') return NULL;

    cgroup_t *cgrp = kzalloc(sizeof(cgroup_t));
    if (!cgrp) return NULL;

    cgrp->id = alloc_cgroup_id();
    strncpy(cgrp->name, name, CGROUP_NAME_MAX - 1);
    cgrp->name[CGROUP_NAME_MAX - 1] = '\0';
    cgrp->parent = parent;
    cgrp->children = NULL;
    cgrp->sibling_next = NULL;
    cgrp->sibling_prev = NULL;
    spin_init(&cgrp->lock);
    cgrp->flags = 0;
    cgrp->tasks_head = NULL;
    cgrp->tasks_tail = NULL;
    cgrp->nr_tasks = 0;

    for (int i = 0; i < CGROUP_MAX_SUBSYS; i++) {
        cgrp->subsys_state[i] = NULL;
    }

    if (parent) {
        if (parent->depth >= CGROUP_MAX_DEPTH) {
            kfree(cgrp);
            return NULL;
        }
        cgrp->depth = parent->depth + 1;
        cgrp->hierarchy = parent->hierarchy;

        spin_lock(&parent->lock);
        cgrp->sibling_next = parent->children;
        if (parent->children) {
            parent->children->sibling_prev = cgrp;
        }
        parent->children = cgrp;
        spin_unlock(&parent->lock);

        for (int i = 0; i < CGROUP_MAX_SUBSYS; i++) {
            alloc_css_for_subsys(cgrp, i);
        }
    } else {
        cgrp->depth = 0;
        for (int i = 0; i < CGROUP_MAX_SUBSYS; i++) {
            alloc_css_for_subsys(cgrp, i);
        }
    }

    return cgrp;
}

int cgroup_mkdir(cgroup_t *parent, const char *name)
{
    if (!parent || !name || name[0] == '\0') return -1;
    if (parent->depth >= CGROUP_MAX_DEPTH) return -1;

    spin_lock(&parent->lock);
    cgroup_t *child = parent->children;
    while (child) {
        if (strncmp(child->name, name, CGROUP_NAME_MAX) == 0) {
            spin_unlock(&parent->lock);
            return -1;
        }
        child = child->sibling_next;
    }
    spin_unlock(&parent->lock);

    cgroup_t *cgrp = cgroup_create(parent, name);
    return cgrp ? 0 : -1;
}

static void cgroup_remove_tasks(cgroup_t *cgrp)
{
    cgroup_task_t *task = cgrp->tasks_head;
    while (task) {
        cgroup_task_t *next = task->next;
        kfree(task);
        task = next;
    }
    cgrp->tasks_head = NULL;
    cgrp->tasks_tail = NULL;
    cgrp->nr_tasks = 0;
}

static void cgroup_destroy_recursive(cgroup_t *cgrp)
{
    if (!cgrp) return;
    cgroup_t *child = cgrp->children;
    while (child) {
        cgroup_t *next = child->sibling_next;
        cgroup_destroy_recursive(child);
        child = next;
    }

    for (int i = 0; i < CGROUP_MAX_SUBSYS; i++) {
        if (cgrp->subsys_state[i]) {
            cgroup_subsys_t *ss = cgroup_subsys[i];
            if (ss && ss->css_free) {
                ss->css_free(cgrp->subsys_state[i]);
            }
            cgrp->subsys_state[i] = NULL;
        }
    }

    cgroup_remove_tasks(cgrp);

    if (cgrp->parent) {
        spin_lock(&cgrp->parent->lock);
        if (cgrp->sibling_prev) {
            cgrp->sibling_prev->sibling_next = cgrp->sibling_next;
        } else {
            cgrp->parent->children = cgrp->sibling_next;
        }
        if (cgrp->sibling_next) {
            cgrp->sibling_next->sibling_prev = cgrp->sibling_prev;
        }
        spin_unlock(&cgrp->parent->lock);
    }

    kfree(cgrp);
}

void cgroup_destroy(cgroup_t *cgrp)
{
    if (!cgrp) return;
    if (cgrp == cgroup_root_default) return;

    spin_lock(&cgrp->lock);
    if (cgrp->nr_tasks > 0) {
        spin_unlock(&cgrp->lock);
        return;
    }
    if (cgrp->children) {
        spin_unlock(&cgrp->lock);
        return;
    }
    spin_unlock(&cgrp->lock);

    cgroup_destroy_recursive(cgrp);
}

static cgroup_task_t *find_task_in_cgroup(cgroup_t *cgrp, u64 pid)
{
    cgroup_task_t *t = cgrp->tasks_head;
    while (t) {
        if (t->pid == pid) return t;
        t = t->next;
    }
    return NULL;
}

int cgroup_attach_task(cgroup_t *cgrp, u64 pid)
{
    if (!cgrp || pid == 0) return -1;

    spin_lock(&cgrp->lock);
    if (cgrp->nr_tasks >= CGROUP_MAX_TASKS) {
        spin_unlock(&cgrp->lock);
        return -1;
    }

    if (find_task_in_cgroup(cgrp, pid)) {
        spin_unlock(&cgrp->lock);
        return 0;
    }

    cgroup_task_t *task = kzalloc(sizeof(cgroup_task_t));
    if (!task) {
        spin_unlock(&cgrp->lock);
        return -1;
    }

    task->pid = pid;
    task->next = NULL;
    task->prev = cgrp->tasks_tail;

    if (cgrp->tasks_tail) {
        cgrp->tasks_tail->next = task;
    } else {
        cgrp->tasks_head = task;
    }
    cgrp->tasks_tail = task;
    cgrp->nr_tasks++;

    for (int i = 0; i < CGROUP_MAX_SUBSYS; i++) {
        cgroup_subsys_t *ss = cgroup_subsys[i];
        if (ss && ss->enabled && ss->attach) {
            ss->attach(cgrp, task);
        }
    }

    spin_unlock(&cgrp->lock);
    return 0;
}

int cgroup_detach_task(cgroup_t *cgrp, u64 pid)
{
    if (!cgrp) return -1;

    spin_lock(&cgrp->lock);
    cgroup_task_t *task = find_task_in_cgroup(cgrp, pid);
    if (!task) {
        spin_unlock(&cgrp->lock);
        return -1;
    }

    for (int i = 0; i < CGROUP_MAX_SUBSYS; i++) {
        cgroup_subsys_t *ss = cgroup_subsys[i];
        if (ss && ss->enabled && ss->detach) {
            ss->detach(cgrp, task);
        }
    }

    if (task->prev) {
        task->prev->next = task->next;
    } else {
        cgrp->tasks_head = task->next;
    }
    if (task->next) {
        task->next->prev = task->prev;
    } else {
        cgrp->tasks_tail = task->prev;
    }
    cgrp->nr_tasks--;
    kfree(task);

    spin_unlock(&cgrp->lock);
    return 0;
}

int cgroup_path(cgroup_t *cgrp, char *buf, u64 buflen)
{
    if (!cgrp || !buf || buflen == 0) return -1;

    char components[CGROUP_MAX_DEPTH][CGROUP_NAME_MAX];
    int depth = 0;

    cgroup_t *cur = cgrp;
    while (cur && depth < CGROUP_MAX_DEPTH) {
        strncpy(components[depth], cur->name, CGROUP_NAME_MAX - 1);
        components[depth][CGROUP_NAME_MAX - 1] = '\0';
        depth++;
        cur = cur->parent;
    }

    u64 pos = 0;
    for (int i = depth - 1; i >= 0; i--) {
        if (pos < buflen) buf[pos++] = '/';
        u64 slen = strlen(components[i]);
        if (pos + slen > buflen) slen = buflen - pos;
        memcpy(buf + pos, components[i], slen);
        pos += slen;
    }

    if (pos == 0) {
        if (buflen >= 2) {
            buf[0] = '/';
            buf[1] = '\0';
            return 1;
        }
        return -1;
    }

    if (pos >= buflen) pos = buflen - 1;
    buf[pos] = '\0';
    return (int)pos;
}

u64 cgroup_task_count(cgroup_t *cgrp)
{
    if (!cgrp) return 0;
    u64 count;
    spin_lock(&cgrp->lock);
    count = cgrp->nr_tasks;
    spin_unlock(&cgrp->lock);
    return count;
}

int cgroup_register_subsys(cgroup_subsys_t *ss)
{
    if (!ss) return -1;

    spin_lock(&cgroup_subsys_lock);
    if (ss->id < 0 || ss->id >= CGROUP_MAX_SUBSYS) {
        spin_unlock(&cgroup_subsys_lock);
        return -1;
    }
    if (cgroup_subsys[ss->id]) {
        spin_unlock(&cgroup_subsys_lock);
        return -1;
    }
    ss->enabled = 1;
    cgroup_subsys[ss->id] = ss;
    spin_unlock(&cgroup_subsys_lock);
    return 0;
}

cgroup_subsys_t *cgroup_get_subsys(int id)
{
    if (id < 0 || id >= CGROUP_MAX_SUBSYS) return NULL;
    return cgroup_subsys[id];
}

cgroup_cpu_state_t *cgroup_cpu_state(cgroup_t *cgrp)
{
    if (!cgrp || !cgrp->subsys_state[CGROUP_SUBSYS_CPU]) return NULL;
    return (cgroup_cpu_state_t *)cgrp->subsys_state[CGROUP_SUBSYS_CPU]->priv;
}

cgroup_memory_state_t *cgroup_mem_state(cgroup_t *cgrp)
{
    if (!cgrp || !cgrp->subsys_state[CGROUP_SUBSYS_MEMORY]) return NULL;
    return (cgroup_memory_state_t *)cgrp->subsys_state[CGROUP_SUBSYS_MEMORY]->priv;
}

cgroup_blkio_state_t *cgroup_blkio_state(cgroup_t *cgrp)
{
    if (!cgrp || !cgrp->subsys_state[CGROUP_SUBSYS_BLKIO]) return NULL;
    return (cgroup_blkio_state_t *)cgrp->subsys_state[CGROUP_SUBSYS_BLKIO]->priv;
}

cgroup_devices_state_t *cgroup_dev_state(cgroup_t *cgrp)
{
    if (!cgrp || !cgrp->subsys_state[CGROUP_SUBSYS_DEVICES]) return NULL;
    return (cgroup_devices_state_t *)cgrp->subsys_state[CGROUP_SUBSYS_DEVICES]->priv;
}

int cgroup_cpu_set_shares(cgroup_t *cgrp, u64 shares)
{
    cgroup_cpu_state_t *state = cgroup_cpu_state(cgrp);
    if (!state) return -1;
    spin_lock(&cgrp->subsys_state[CGROUP_SUBSYS_CPU]->lock);
    state->cpu_shares = shares;
    spin_unlock(&cgrp->subsys_state[CGROUP_SUBSYS_CPU]->lock);
    return 0;
}

int cgroup_cpu_set_cfs_quota(cgroup_t *cgrp, s64 quota_us)
{
    cgroup_cpu_state_t *state = cgroup_cpu_state(cgrp);
    if (!state) return -1;
    spin_lock(&cgrp->subsys_state[CGROUP_SUBSYS_CPU]->lock);
    state->cpu_cfs_quota_us = quota_us;
    spin_unlock(&cgrp->subsys_state[CGROUP_SUBSYS_CPU]->lock);
    return 0;
}

int cgroup_cpu_set_cfs_period(cgroup_t *cgrp, u64 period_us)
{
    cgroup_cpu_state_t *state = cgroup_cpu_state(cgrp);
    if (!state) return -1;
    spin_lock(&cgrp->subsys_state[CGROUP_SUBSYS_CPU]->lock);
    state->cpu_cfs_period_us = period_us;
    spin_unlock(&cgrp->subsys_state[CGROUP_SUBSYS_CPU]->lock);
    return 0;
}

int cgroup_mem_set_limit(cgroup_t *cgrp, u64 limit_bytes)
{
    cgroup_memory_state_t *state = cgroup_mem_state(cgrp);
    if (!state) return -1;
    spin_lock(&cgrp->subsys_state[CGROUP_SUBSYS_MEMORY]->lock);
    state->memory_limit_in_bytes = limit_bytes;
    spin_unlock(&cgrp->subsys_state[CGROUP_SUBSYS_MEMORY]->lock);
    return 0;
}

int cgroup_mem_check_oom(cgroup_t *cgrp)
{
    cgroup_memory_state_t *state = cgroup_mem_state(cgrp);
    if (!state) return 0;
    spin_lock(&cgrp->subsys_state[CGROUP_SUBSYS_MEMORY]->lock);
    int oom = 0;
    if (state->memory_limit_in_bytes != MEM_LIMIT_IN_BYTES_MAX &&
        state->memory_usage_in_bytes >= state->memory_limit_in_bytes) {
        state->memory_failcnt++;
        state->oom_kill_count++;
        state->under_oom = 1;
        oom = 1;
    } else {
        state->under_oom = 0;
    }
    spin_unlock(&cgrp->subsys_state[CGROUP_SUBSYS_MEMORY]->lock);
    return oom;
}

void cgroup_mem_account(cgroup_t *cgrp, u64 delta_bytes)
{
    if (!cgrp) return;
    cgroup_t *p = cgrp;
    while (p) {
        cgroup_memory_state_t *state = cgroup_mem_state(p);
        if (state) {
            spin_lock(&p->subsys_state[CGROUP_SUBSYS_MEMORY]->lock);
            state->memory_usage_in_bytes += delta_bytes;
            if (state->memory_usage_in_bytes > state->memory_max_usage_in_bytes) {
                state->memory_max_usage_in_bytes = state->memory_usage_in_bytes;
            }
            spin_unlock(&p->subsys_state[CGROUP_SUBSYS_MEMORY]->lock);
        }
        p = p->parent;
    }
}

void cgroup_mem_unaccount(cgroup_t *cgrp, u64 delta_bytes)
{
    if (!cgrp) return;
    cgroup_t *p = cgrp;
    while (p) {
        cgroup_memory_state_t *state = cgroup_mem_state(p);
        if (state) {
            spin_lock(&p->subsys_state[CGROUP_SUBSYS_MEMORY]->lock);
            if (state->memory_usage_in_bytes >= delta_bytes) {
                state->memory_usage_in_bytes -= delta_bytes;
            } else {
                state->memory_usage_in_bytes = 0;
            }
            spin_unlock(&p->subsys_state[CGROUP_SUBSYS_MEMORY]->lock);
        }
        p = p->parent;
    }
}

int cgroup_blkio_set_read_bps(cgroup_t *cgrp, u64 bps)
{
    cgroup_blkio_state_t *state = cgroup_blkio_state(cgrp);
    if (!state) return -1;
    spin_lock(&cgrp->subsys_state[CGROUP_SUBSYS_BLKIO]->lock);
    state->blkio_throttle_read_bps = bps;
    spin_unlock(&cgrp->subsys_state[CGROUP_SUBSYS_BLKIO]->lock);
    return 0;
}

int cgroup_blkio_set_write_bps(cgroup_t *cgrp, u64 bps)
{
    cgroup_blkio_state_t *state = cgroup_blkio_state(cgrp);
    if (!state) return -1;
    spin_lock(&cgrp->subsys_state[CGROUP_SUBSYS_BLKIO]->lock);
    state->blkio_throttle_write_bps = bps;
    spin_unlock(&cgrp->subsys_state[CGROUP_SUBSYS_BLKIO]->lock);
    return 0;
}

int cgroup_blkio_set_weight(cgroup_t *cgrp, u32 weight)
{
    cgroup_blkio_state_t *state = cgroup_blkio_state(cgrp);
    if (!state) return -1;
    if (weight > BLKIO_WEIGHT_MAX) return -1;
    spin_lock(&cgrp->subsys_state[CGROUP_SUBSYS_BLKIO]->lock);
    state->blkio_weight = weight;
    spin_unlock(&cgrp->subsys_state[CGROUP_SUBSYS_BLKIO]->lock);
    return 0;
}

int cgroup_dev_allow(cgroup_t *cgrp, u8 type, u32 major, u32 minor, u8 access)
{
    cgroup_devices_state_t *state = cgroup_dev_state(cgrp);
    if (!state) return -1;
    cgroup_device_whitelist_t *entry = kzalloc(sizeof(cgroup_device_whitelist_t));
    if (!entry) return -1;
    entry->access = access;
    entry->type = type;
    entry->major = major;
    entry->minor = minor;
    spin_lock(&cgrp->subsys_state[CGROUP_SUBSYS_DEVICES]->lock);
    entry->next = state->whitelist_head;
    state->whitelist_head = entry;
    spin_unlock(&cgrp->subsys_state[CGROUP_SUBSYS_DEVICES]->lock);
    return 0;
}

int cgroup_dev_deny(cgroup_t *cgrp, u8 type, u32 major, u32 minor, u8 access)
{
    (void)cgrp;
    (void)type;
    (void)major;
    (void)minor;
    (void)access;
    return 0;
}

int cgroup_dev_check(cgroup_t *cgrp, u8 type, u32 major, u32 minor, u8 access)
{
    cgroup_devices_state_t *state = cgroup_dev_state(cgrp);
    if (!state) return -1;
    if (state->deny_all) return -1;
    spin_lock(&cgrp->subsys_state[CGROUP_SUBSYS_DEVICES]->lock);
    cgroup_device_whitelist_t *entry = state->whitelist_head;
    int result = -1;
    while (entry) {
        if (entry->type == type) {
            if ((entry->major == DEV_MAJOR_MAX || entry->major == major) &&
                (entry->minor == DEV_MINOR_MAX || entry->minor == minor)) {
                if (entry->access & access) {
                    result = 0;
                    break;
                }
            }
        }
        entry = entry->next;
    }
    spin_unlock(&cgrp->subsys_state[CGROUP_SUBSYS_DEVICES]->lock);
    return result;
}

cgroup_t *cgroup_from_root(cgroup_hierarchy_t *hierarchy)
{
    if (!hierarchy) return NULL;
    return hierarchy->root_cgroup;
}
