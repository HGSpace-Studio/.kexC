#ifndef KAPI_FUTEX_H
#define KAPI_FUTEX_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_FUTEX_WAIT            0
#define KAPI_FUTEX_WAKE            1
#define KAPI_FUTEX_FD              2
#define KAPI_FUTEX_REQUEUE         3
#define KAPI_FUTEX_CMP_REQUEUE     4
#define KAPI_FUTEX_WAKE_OP         5
#define KAPI_FUTEX_LOCK_PI         6
#define KAPI_FUTEX_UNLOCK_PI       7
#define KAPI_FUTEX_TRYLOCK_PI      8
#define KAPI_FUTEX_WAIT_BITSET     9
#define KAPI_FUTEX_WAKE_BITSET    10
#define KAPI_FUTEX_WAIT_REQUEUE_PI  11
#define KAPI_FUTEX_CMP_REQUEUE_PI  12
#define KAPI_FUTEX_LOCK_PI2       13

#define KAPI_FUTEX_PRIVATE_FLAG   128
#define KAPI_FUTEX_CLOCK_REALTIME 256

#define KAPI_FUTEX_BITSET_MATCH_ANY  0xFFFFFFFF

#define KAPI_FUTEX_OP_SET          0
#define KAPI_FUTEX_OP_ADD          1
#define KAPI_FUTEX_OP_OR           2
#define KAPI_FUTEX_OP_ANDN         3
#define KAPI_FUTEX_OP_XOR          4
#define KAPI_FUTEX_OP_SET_EQ       0
#define KAPI_FUTEX_OP_SET_NE       1
#define KAPI_FUTEX_OP_SET_LE       2
#define KAPI_FUTEX_OP_SET_GE       3
#define KAPI_FUTEX_OP_ADD_EQ       4
#define KAPI_FUTEX_OP_ADD_NE       5
#define KAPI_FUTEX_OP_ADD_LE       6
#define KAPI_FUTEX_OP_ADD_GE       7
#define KAPI_FUTEX_OP_OR_EQ        8
#define KAPI_FUTEX_OP_OR_NE        9
#define KAPI_FUTEX_OP_OR_LE       10
#define KAPI_FUTEX_OP_OR_GE       11
#define KAPI_FUTEX_OP_ANDN_EQ     12
#define KAPI_FUTEX_OP_ANDN_NE     13
#define KAPI_FUTEX_OP_ANDN_LE     14
#define KAPI_FUTEX_OP_ANDN_GE     15
#define KAPI_FUTEX_OP_XOR_EQ     16
#define KAPI_FUTEX_OP_XOR_NE     17
#define KAPI_FUTEX_OP_XOR_LE     18
#define KAPI_FUTEX_OP_XOR_GE     19

typedef struct kapi_futex_q kapi_futex_q_t;

struct kapi_futex_q {
    uint32_t*        uaddr;
    uint32_t         val;
    uint32_t         bitset;
    int              pid;
    int              woken;
    kapi_futex_q_t*  next;
    kapi_futex_q_t*  prev;
};

typedef struct {
    kapi_futex_q_t* head;
    kapi_futex_q_t* tail;
    int             count;
} kapi_futex_bucket_t;

#define KAPI_FUTEX_HASH_SIZE   256

typedef struct {
    kapi_futex_bucket_t buckets[KAPI_FUTEX_HASH_SIZE];
    kapi_futex_q_t      pool[4096];
    int                 pool_used[4096];
    int                 pool_count;
} kapi_futex_table_t;

int kapi_futex_init(void);

int kapi_futex_syscall(uint32_t* uaddr, int op, uint32_t val,
                       const struct timespec* timeout,
                       uint32_t* uaddr2, uint32_t val2, uint32_t val3);

int kapi_futex_wait(uint32_t* uaddr, uint32_t val, const struct timespec* timeout);

int kapi_futex_wake(uint32_t* uaddr, uint32_t count);

int kapi_futex_requeue(uint32_t* uaddr1, uint32_t* uaddr2, int count, int max_count);

int kapi_futex_cmp_requeue(uint32_t* uaddr1, uint32_t cmpval, uint32_t* uaddr2, int count, int max_count);

int kapi_futex_wake_op(uint32_t* uaddr1, uint32_t* uaddr2, int nr_wake, int nr_wake2, int op);

int kapi_futex_wait_bitset(uint32_t* uaddr, uint32_t val, const struct timespec* timeout, uint32_t bitset);

int kapi_futex_wake_bitset(uint32_t* uaddr, uint32_t count, uint32_t bitset);

int kapi_futex_lock_pi(uint32_t* uaddr, const struct timespec* timeout);

int kapi_futex_unlock_pi(uint32_t* uaddr);

int kapi_futex_trylock_pi(uint32_t* uaddr);

uint32_t kapi_futex_compute_op(int oparg, uint32_t op, int cmp);

#ifdef __cplusplus
}
#endif

#endif