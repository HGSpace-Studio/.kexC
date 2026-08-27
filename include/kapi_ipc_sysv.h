#ifndef KAPI_IPC_SYSV_H
#define KAPI_IPC_SYSV_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_IPC_PRIVATE   0
#define KAPI_IPC_RMID      0
#define KAPI_IPC_SET       1
#define KAPI_IPC_STAT      2
#define KAPI_IPC_INFO      3
#define KAPI_IPC_CREAT     01000
#define KAPI_IPC_EXCL      02000
#define KAPI_IPC_NOWAIT    04000
#define KAPI_IPC_NOPERM   010000

#define KAPI_IPC_64       0x0100

#define KAPI_SHM_RDONLY   010000
#define KAPI_SHM_RND      020000
#define KAPI_SHM_REMAP    040000
#define KAPI_SHM_EXEC     0100000
#define KAPI_SHM_LOCK     11
#define KAPI_SHM_UNLOCK   12
#define KAPI_SHM_STAT     13
#define KAPI_SHM_INFO     14
#define KAPI_SHM_DEST     01000
#define KAPI_SHM_LOCKED   02000
#define KAPI_SHM_HUGETLB  04000
#define KAPI_SHM_NORESERVE 010000

#define KAPI_SEM_UNDO     0x1000
#define KAPI_GETPID        11
#define KAPI_GETVAL        12
#define KAPI_GETALL        13
#define KAPI_GETNCNT       14
#define KAPI_GETZCNT       15
#define KAPI_SETVAL        16
#define KAPI_SETALL        17

#define KAPI_MSG_NOERROR   010000
#define KAPI_MSG_EXCEPT    020000
#define KAPI_MSG_COPY      040000

typedef struct {
    int32_t  key;
    uint32_t uid;
    uint32_t gid;
    uint32_t cuid;
    uint32_t cgid;
    uint16_t mode;
    uint16_t seq;
    int32_t  id;
} kapi_ipc_perm_t;

typedef struct {
    kapi_ipc_perm_t shm_perm;
    size_t          shm_segsz;
    int64_t         shm_atime;
    int64_t         shm_dtime;
    int64_t         shm_ctime;
    int32_t         shm_cpid;
    int32_t         shm_lpid;
    uint32_t        shm_nattch;
    uint32_t        shm_npages;
    void*           shm_addr;
    uint64_t        shm_pages[1024];
} kapi_shmid_ds_t;

typedef struct {
    kapi_ipc_perm_t sem_perm;
    int64_t         sem_otime;
    int64_t         sem_ctime;
    uint16_t        sem_nsems;
    int16_t*        sem_array;
    uint16_t*       sem_pid;
    int16_t*        sem_undo;
} kapi_semid_ds_t;

typedef struct {
    kapi_ipc_perm_t msg_perm;
    int64_t         msg_stime;
    int64_t         msg_rtime;
    int64_t         msg_ctime;
    uint32_t        msg_qnum;
    uint32_t        msg_qbytes;
    uint32_t        msg_lspid;
    uint32_t        msg_lrpid;
} kapi_msqid_ds_t;

typedef struct {
    int32_t  mtype;
    uint8_t  mtext[1];
} kapi_msgbuf_t;

#define KAPI_SHM_MAX  64
#define KAPI_SEM_MAX  64
#define KAPI_MSG_MAX  64
#define KAPI_SEM_VALUE_MAX  32767
#define KAPI_MSG_QUEUE_MAX  64

typedef struct {
    int32_t  mtype;
    size_t   msize;
    void*    data;
} kapi_msg_node_t;

typedef struct {
    kapi_msqid_ds_t ds;
    kapi_msg_node_t messages[KAPI_MSG_QUEUE_MAX];
    int             msg_count;
    int             msg_head;
    int             msg_tail;
    size_t          msg_total_bytes;
} kapi_msg_queue_t;

int kapi_ipc_init(void);

int kapi_shmget(key_t key, size_t size, int shmflg);

void* kapi_shmat(int shmid, const void* shmaddr, int shmflg);

int kapi_shmdt(const void* shmaddr);

int kapi_shmctl(int shmid, int cmd, kapi_shmid_ds_t* buf);

int kapi_semget(key_t key, int nsems, int semflg);

int kapi_semop(int semid, struct sembuf* sops, size_t nsops);

int kapi_semctl(int semid, int semnum, int cmd, ...);

int kapi_semctl_val(int semid, int semnum, int cmd, int val);

int kapi_msgget(key_t key, int msgflg);

int kapi_msgsnd(int msqid, const void* msgp, size_t msgsz, int msgflg);

ssize_t kapi_msgrcv(int msqid, void* msgp, size_t msgsz, long msgtyp, int msgflg);

int kapi_msgctl(int msqid, int cmd, kapi_msqid_ds_t* buf);

int kapi_shm_find_by_key(key_t key);
int kapi_sem_find_by_key(key_t key);
int kapi_msg_find_by_key(key_t key);

#ifdef __cplusplus
}
#endif

#endif