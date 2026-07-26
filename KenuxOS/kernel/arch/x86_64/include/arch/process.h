#ifndef ARCH_X86_64_PROCESS_H
#define ARCH_X86_64_PROCESS_H

#include <arch/types.h>

#define PROCESS_MAX            256
#define PROCESS_STACK_SIZE     16384
#define PROCESS_NAME_MAX       32
#define THREAD_NAME_MAX        32

#define PROCESS_TERMINATED     0
#define PROCESS_READY          1
#define PROCESS_RUNNING        2
#define PROCESS_SLEEPING       3
#define PROCESS_WAITING        4
#define PROCESS_ZOMBIE         5
#define PROCESS_STOPPED        6
#define PROCESS_DEAD           7
#define PROCESS_UNUSED         8

#define PRIORITY_IDLE          0
#define PRIORITY_LOW           1
#define PRIORITY_NORMAL        2
#define PRIORITY_HIGH          3
#define PRIORITY_REALTIME      4
#define PRIORITY_COUNT         5

#define PROC_FLAG_KTHREAD      0x0001
#define PROC_FLAG_USER         0x0002
#define PROC_FLAG_FIXED        0x0004
#define PROC_FLAG_EXITING      0x0008

/* 兼容性宏 */
#define PROCESS_FLAG_KTHREAD   PROC_FLAG_KTHREAD
#define PROCESS_FLAG_FIXED     PROC_FLAG_FIXED

#define THREAD_MAX_PER_PROC    16
#define THREAD_MAX             1024

typedef struct {
    uint64_t r15, r14, r13, r12;
    uint64_t r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi;
    uint64_t rdx, rcx, rbx, rax;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;

    uint64_t cr3;
    uint64_t gs_base;
    uint64_t fs_base;
} process_context_t;

typedef struct {
    uint64_t          id;
    uint64_t          parent_id;
    uint64_t          state;
    uint64_t          priority;
    uint64_t          flags;
    char              name[PROCESS_NAME_MAX];

    process_context_t context;
    void*             stack;
    void*             stack_bottom;
    void*             entry;
    void*             entry_arg;

    uint64_t cpu_time_ms;
    uint64_t sched_count;
    uint64_t mem_usage;
    uint64_t rss;

    void*    stack_top;
    void*    stack_base;

    uint64_t pending_signals;
    uint64_t signal_mask;
    void*    signal_data;

    void*    wait_channel;
    uint64_t wakeup_time;

    int      fd_table[16];
    int      fd_count;

    char     cwd[256];

    void*    kthread_data;  /* 内核线程私有数据 */
    void*    cred;          /* 进程凭证 */
} process_t;

typedef struct {
    uint64_t total_switches;
    uint64_t idle_switches;
    uint64_t stolen_time_ms;
} scheduler_stats_t;

void process_init(void);
uint64_t process_create(const char* name, void* entry, uint64_t priority);
void process_switch(process_context_t* old, process_context_t* new);
void process_yield(void);
void process_start(void);
void process_exit(uint64_t code);

uint64_t process_create_ex(const char* name, void* entry, void* arg,
                           uint64_t priority, uint32_t flags, uint64_t parent);
int      process_kill(uint64_t pid, int signal);
int      process_wait(uint64_t pid, int* status, uint64_t timeout_ms);
void     process_sleep(uint64_t ms);
void     process_wakeup(uint64_t pid);
uint64_t process_current_id(void);
process_t* process_get(uint64_t pid);
uint64_t process_find_by_name(const char* name);
int      process_set_priority(uint64_t pid, uint64_t priority);
void     process_dump(const char* prefix);
scheduler_stats_t* scheduler_get_stats(void);

void scheduler_tick(void);

#endif
