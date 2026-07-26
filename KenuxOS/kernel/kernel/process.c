#include <arch/process.h>
#include <memory.h>
#include <string.h>
#include <slab.h>

process_t processes[PROCESS_MAX];
int process_count = 0;
uint64_t current_process = 0;

static void __process_trampoline(void)
{
    process_t* p = &processes[current_process];
    if (p->entry) {
        void (*fn)(void*) = (void (*)(void*))p->entry;
        fn(p->entry_arg);
    }
    process_exit(0);
}

void process_init(void)
{
    for (int i = 0; i < PROCESS_MAX; i++) {
        processes[i].state = PROCESS_UNUSED;
    }
    process_count = 0;
    current_process = 0;
}

uint64_t process_create(const char* name, void* entry, uint64_t priority)
{
    if (process_count >= PROCESS_MAX) {
        return 0;
    }

    process_t* proc = &processes[process_count];
    proc->id = process_count;
    proc->priority = priority;
    proc->state = PROCESS_READY;
    proc->entry = entry;
    proc->stack = NULL;
    memset(&proc->context, 0, sizeof(proc->context));
    if (name) {
        strncpy(proc->name, name, sizeof(proc->name) - 1);
        proc->name[sizeof(proc->name) - 1] = '\0';
    }

    process_count++;
    return proc->id;
}

void process_exit(uint64_t code)
{
    if (current_process < PROCESS_MAX) {
        processes[current_process].state = PROCESS_DEAD;
    }
}

void process_yield(void)
{
    if (process_count <= 1) return;

    uint64_t prev = current_process;
    uint64_t next = prev;

    for (uint64_t i = 1; i < PROCESS_MAX; i++) {
        uint64_t idx = (prev + i) % PROCESS_MAX;
        if (processes[idx].state == PROCESS_READY) {
            next = idx;
            break;
        }
    }

    if (next != prev) {
        processes[prev].state = PROCESS_READY;
        processes[next].state = PROCESS_RUNNING;
        current_process = next;
        process_switch(&processes[prev].context, &processes[next].context);
        processes[current_process].state = PROCESS_RUNNING;
    }

    processes[current_process].sched_count++;
}

__attribute__((naked)) void process_switch(process_context_t* old, process_context_t* new)
{
    __asm__ volatile (
        "pushq %rbp\n\t"
        "pushq %rbx\n\t"
        "pushq %r12\n\t"
        "pushq %r13\n\t"
        "pushq %r14\n\t"
        "pushq %r15\n\t"
        "movq %rsp, 144(%rdi)\n\t"
        "movq 144(%rsi), %rsp\n\t"
        "popq %r15\n\t"
        "popq %r14\n\t"
        "popq %r13\n\t"
        "popq %r12\n\t"
        "popq %rbx\n\t"
        "popq %rbp\n\t"
        "ret\n\t"
    );
}

uint64_t process_get_current_id(void)
{
    return current_process;
}

process_t* process_get(uint64_t pid)
{
    if (pid < PROCESS_MAX && processes[pid].state != PROCESS_UNUSED) {
        return &processes[pid];
    }
    return NULL;
}

void process_wakeup(uint64_t pid)
{
    if (pid < PROCESS_MAX && processes[pid].state == PROCESS_WAITING) {
        processes[pid].state = PROCESS_READY;
    }
}

void enqueue_process(uint64_t pid)
{
    if (pid < PROCESS_MAX && processes[pid].state != PROCESS_UNUSED && processes[pid].state != PROCESS_DEAD) {
        processes[pid].state = PROCESS_READY;
    }
}

uint64_t process_create_ex(const char* name, void* entry, void* arg,
                           uint64_t priority, uint32_t flags, uint64_t parent)
{
    uint64_t pid = (uint64_t)-1;
    for (uint64_t i = 0; i < PROCESS_MAX; i++) {
        if (processes[i].state == PROCESS_UNUSED) {
            pid = i;
            break;
        }
    }
    if (pid == (uint64_t)-1) {
        return (uint64_t)-1;
    }

    process_t* proc = &processes[pid];
    memset(proc, 0, sizeof(process_t));
    proc->id = pid;
    proc->priority = priority;
    proc->state = PROCESS_READY;
    proc->entry = entry;
    proc->entry_arg = arg;
    proc->flags = flags;
    proc->parent_id = parent;

    if (name) {
        strncpy(proc->name, name, sizeof(proc->name) - 1);
        proc->name[sizeof(proc->name) - 1] = '\0';
    }

    proc->stack = kzalloc(PROCESS_STACK_SIZE);
    if (!proc->stack) {
        proc->state = PROCESS_UNUSED;
        return (uint64_t)-1;
    }

    uint64_t* sp = (uint64_t*)((uint8_t*)proc->stack + PROCESS_STACK_SIZE);
    sp--;
    *sp = (uint64_t)__process_trampoline;
    sp -= 6;
    proc->context.rsp = (uint64_t)sp;

    if (pid >= (uint64_t)process_count) {
        process_count = (int)(pid + 1);
    }
    return pid;
}

int thread_create(void* entry, void* arg)
{
    uint64_t tid = process_create_ex("thread", entry, arg,
                                      PRIORITY_NORMAL, 0, current_process);
    if (tid == (uint64_t)-1) {
        return -1;
    }
    return (int)tid;
}

void thread_yield(void)
{
    process_yield();
}

static scheduler_stats_t g_scheduler_stats = {0};

scheduler_stats_t* scheduler_get_stats(void)
{
    return &g_scheduler_stats;
}
