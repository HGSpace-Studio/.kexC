

#ifndef KAPI_PROCESS_H
#define KAPI_PROCESS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_PROC_DEAD       0
#define KAPI_PROC_READY      1
#define KAPI_PROC_RUNNING    2
#define KAPI_PROC_WAITING    3
#define KAPI_PROC_ZOMBIE     4

#define KAPI_PRIO_IDLE       0
#define KAPI_PRIO_LOW        1
#define KAPI_PRIO_NORMAL     2
#define KAPI_PRIO_HIGH       3
#define KAPI_PRIO_REALTIME   4

typedef struct kapi_process* kapi_proc_t;

typedef struct kapi_thread* kapi_thread_t;

typedef struct {
    int       pid;
    int       ppid;
    int       state;
    int       priority;
    char      name[32];
    uint64_t  cpu_time;
    uint64_t  mem_usage;
} kapi_proc_info_t;

kapi_proc_t kapi_proc_create(const char* name, void (*entry)(void*), void* arg, int prio);

void kapi_proc_exit(int status);

int kapi_proc_kill(kapi_proc_t proc, int force);

int kapi_proc_kill_by_pid(int pid, int force);

kapi_proc_t kapi_proc_current(void);

int kapi_proc_get_pid(kapi_proc_t proc);

int kapi_proc_get_info(kapi_proc_t proc, kapi_proc_info_t* info);

void kapi_proc_sleep(uint64_t ms);

void kapi_proc_yield(void);

int kapi_proc_wait(int pid, int* status);

kapi_thread_t kapi_thread_create(void (*entry)(void*), void* arg);

void kapi_thread_exit(void);

int kapi_ipc_send(int pid, const void* msg, size_t size);

int kapi_ipc_recv(void* msg, size_t size, uint64_t timeout_ms);

int kapi_proc_count(void);

int kapi_proc_list(kapi_proc_info_t* infos, int count);

#ifdef __cplusplus
}
#endif

#endif
