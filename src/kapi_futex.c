#include "kapi_futex.h"
#include "kapi.h"
#include <string.h>

static kapi_futex_table_t futex_table;

static inline uint32_t futex_hash(uint32_t* uaddr)
{
    uintptr_t key = (uintptr_t)uaddr;
    key = ((key >> 4) ^ (key >> 12) ^ (key >> 20)) & 0xFF;
    return (uint32_t)key;
}

int kapi_futex_init(void)
{
    memset(&futex_table, 0, sizeof(futex_table));
    for (int i = 0; i < KAPI_FUTEX_HASH_SIZE; i++) {
        futex_table.buckets[i].head = NULL;
        futex_table.buckets[i].tail = NULL;
        futex_table.buckets[i].count = 0;
    }
    return KAPI_OK;
}

static kapi_futex_q_t* futex_q_alloc(void)
{
    for (int i = 0; i < 4096; i++) {
        if (!futex_table.pool_used[i]) {
            futex_table.pool_used[i] = 1;
            futex_table.pool_count++;
            return &futex_table.pool[i];
        }
    }
    return NULL;
}

static void futex_q_free(kapi_futex_q_t* q)
{
    if (!q) return;
    int idx = (int)(q - futex_table.pool);
    if (idx >= 0 && idx < 4096) {
        futex_table.pool_used[idx] = 0;
        futex_table.pool_count--;
    }
}

static void bucket_enqueue(kapi_futex_bucket_t* b, kapi_futex_q_t* q)
{
    q->next = NULL;
    q->prev = b->tail;
    if (b->tail) b->tail->next = q;
    else b->head = q;
    b->tail = q;
    b->count++;
}

static kapi_futex_q_t* bucket_dequeue(kapi_futex_bucket_t* b)
{
    if (!b->head) return NULL;
    kapi_futex_q_t* q = b->head;
    b->head = q->next;
    if (b->head) b->head->prev = NULL;
    else b->tail = NULL;
    q->next = NULL;
    q->prev = NULL;
    b->count--;
    return q;
}

static void bucket_remove(kapi_futex_bucket_t* b, kapi_futex_q_t* q)
{
    if (q->prev) q->prev->next = q->next;
    else b->head = q->next;
    if (q->next) q->next->prev = q->prev;
    else b->tail = q->prev;
    q->next = NULL;
    q->prev = NULL;
    b->count--;
}

int kapi_futex_wait(uint32_t* uaddr, uint32_t val, const struct timespec* timeout)
{
    if (!uaddr) return KAPI_EINVAL;

    if (*uaddr != val) return KAPI_EAGAIN;

    uint32_t h = futex_hash(uaddr);
    kapi_futex_bucket_t* b = &futex_table.buckets[h];

    kapi_futex_q_t* q = futex_q_alloc();
    if (!q) return KAPI_ENOMEM;

    q->uaddr = uaddr;
    q->val = val;
    q->bitset = KAPI_FUTEX_BITSET_MATCH_ANY;
    q->pid = kapi_proc_get_pid(kapi_proc_current());
    q->woken = 0;

    bucket_enqueue(b, q);

    uint64_t deadline = 0;
    if (timeout) {
        deadline = kapi_get_uptime_ms() + (uint64_t)timeout->tv_sec * 1000 + (uint64_t)timeout->tv_nsec / 1000000;
    }

    while (!q->woken) {
        if (*uaddr != val) {
            bucket_remove(b, q);
            futex_q_free(q);
            return KAPI_EAGAIN;
        }
        if (timeout && kapi_get_uptime_ms() >= deadline) {
            bucket_remove(b, q);
            futex_q_free(q);
            return KAPI_ETIMEDOUT;
        }
        kapi_proc_yield();
    }

    bucket_remove(b, q);
    futex_q_free(q);
    return KAPI_OK;
}

int kapi_futex_wake(uint32_t* uaddr, uint32_t count)
{
    if (!uaddr) return KAPI_EINVAL;
    if (count == 0) return 0;

    uint32_t h = futex_hash(uaddr);
    kapi_futex_bucket_t* b = &futex_table.buckets[h];
    int woken = 0;

    kapi_futex_q_t* q = b->head;
    while (q && (uint32_t)woken < count) {
        kapi_futex_q_t* next = q->next;
        if (q->uaddr == uaddr) {
            q->woken = 1;
            woken++;
        }
        q = next;
    }

    return woken;
}

int kapi_futex_requeue(uint32_t* uaddr1, uint32_t* uaddr2, int count, int max_count)
{
    if (!uaddr1 || !uaddr2) return KAPI_EINVAL;

    uint32_t h1 = futex_hash(uaddr1);
    kapi_futex_bucket_t* b1 = &futex_table.buckets[h1];
    uint32_t h2 = futex_hash(uaddr2);
    kapi_futex_bucket_t* b2 = &futex_table.buckets[h2];

    int woken = 0;
    int requeued = 0;

    kapi_futex_q_t* q = b1->head;
    while (q) {
        kapi_futex_q_t* next = q->next;
        if (q->uaddr == uaddr1) {
            if (woken < count) {
                q->woken = 1;
                woken++;
            } else if (requeued < max_count) {
                bucket_remove(b1, q);
                q->uaddr = uaddr2;
                bucket_enqueue(b2, q);
                requeued++;
            }
        }
        q = next;
    }

    return woken + requeued;
}

int kapi_futex_cmp_requeue(uint32_t* uaddr1, uint32_t cmpval, uint32_t* uaddr2, int count, int max_count)
{
    if (!uaddr1) return KAPI_EINVAL;
    if (*uaddr1 != cmpval) return KAPI_EAGAIN;
    return kapi_futex_requeue(uaddr1, uaddr2, count, max_count);
}

uint32_t kapi_futex_compute_op(int oparg, uint32_t op, int cmp)
{
    uint32_t oldval = (uint32_t)oparg;
    uint32_t newval;
    int encoded_op = op;

    int op_code = (encoded_op >> 28) & 0xF;
    int op_arg = encoded_op & 0xFFF;
    int cmp_code = (encoded_op >> 24) & 0xF;

    switch (op_code) {
    case KAPI_FUTEX_OP_SET:   newval = (uint32_t)op_arg; break;
    case KAPI_FUTEX_OP_ADD:   newval = oldval + (uint32_t)op_arg; break;
    case KAPI_FUTEX_OP_OR:    newval = oldval | (uint32_t)op_arg; break;
    case KAPI_FUTEX_OP_ANDN:  newval = oldval & ~(uint32_t)op_arg; break;
    case KAPI_FUTEX_OP_XOR:   newval = oldval ^ (uint32_t)op_arg; break;
    default: newval = oldval; break;
    }

    (void)cmp;
    switch (cmp_code) {
    case KAPI_FUTEX_OP_SET_EQ:  return (newval == (uint32_t)cmp) ? 1 : 0;
    case KAPI_FUTEX_OP_SET_NE:  return (newval != (uint32_t)cmp) ? 1 : 0;
    case KAPI_FUTEX_OP_SET_LE:  return (newval <= (uint32_t)cmp) ? 1 : 0;
    case KAPI_FUTEX_OP_SET_GE:  return (newval >= (uint32_t)cmp) ? 1 : 0;
    default: return 0;
    }
}

int kapi_futex_wake_op(uint32_t* uaddr1, uint32_t* uaddr2, int nr_wake, int nr_wake2, int op)
{
    if (!uaddr1 || !uaddr2) return KAPI_EINVAL;

    uint32_t oldval2 = *uaddr2;
    int cmp = (op >> 12) & 0xFFF;
    uint32_t result = kapi_futex_compute_op((int)oldval2, op, cmp);
    if (result) {
        *uaddr2 = oldval2;
    }

    int woken1 = kapi_futex_wake(uaddr1, (uint32_t)nr_wake);
    int woken2 = 0;
    if (result) {
        woken2 = kapi_futex_wake(uaddr2, (uint32_t)nr_wake2);
    }

    return woken1 + woken2;
}

int kapi_futex_wait_bitset(uint32_t* uaddr, uint32_t val, const struct timespec* timeout, uint32_t bitset)
{
    if (!uaddr || bitset == 0) return KAPI_EINVAL;
    if (*uaddr != val) return KAPI_EAGAIN;

    uint32_t h = futex_hash(uaddr);
    kapi_futex_bucket_t* b = &futex_table.buckets[h];

    kapi_futex_q_t* q = futex_q_alloc();
    if (!q) return KAPI_ENOMEM;

    q->uaddr = uaddr;
    q->val = val;
    q->bitset = bitset;
    q->pid = kapi_proc_get_pid(kapi_proc_current());
    q->woken = 0;

    bucket_enqueue(b, q);

    uint64_t deadline = 0;
    if (timeout) {
        deadline = kapi_get_uptime_ms() + (uint64_t)timeout->tv_sec * 1000 + (uint64_t)timeout->tv_nsec / 1000000;
    }

    while (!q->woken) {
        if (*uaddr != val) {
            bucket_remove(b, q);
            futex_q_free(q);
            return KAPI_EAGAIN;
        }
        if (timeout && kapi_get_uptime_ms() >= deadline) {
            bucket_remove(b, q);
            futex_q_free(q);
            return KAPI_ETIMEDOUT;
        }
        kapi_proc_yield();
    }

    bucket_remove(b, q);
    futex_q_free(q);
    return KAPI_OK;
}

int kapi_futex_wake_bitset(uint32_t* uaddr, uint32_t count, uint32_t bitset)
{
    if (!uaddr || bitset == 0) return KAPI_EINVAL;
    if (count == 0) return 0;

    uint32_t h = futex_hash(uaddr);
    kapi_futex_bucket_t* b = &futex_table.buckets[h];
    int woken = 0;

    kapi_futex_q_t* q = b->head;
    while (q && (uint32_t)woken < count) {
        kapi_futex_q_t* next = q->next;
        if (q->uaddr == uaddr && (q->bitset & bitset)) {
            q->woken = 1;
            woken++;
        }
        q = next;
    }

    return woken;
}

int kapi_futex_lock_pi(uint32_t* uaddr, const struct timespec* timeout)
{
    if (!uaddr) return KAPI_EINVAL;

    uint32_t zero = 0;
    if (__atomic_compare_exchange_n(uaddr, &zero, (uint32_t)kapi_proc_get_pid(kapi_proc_current()),
                                    false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
        return KAPI_OK;
    }

    return kapi_futex_wait(uaddr, *uaddr, timeout);
}

int kapi_futex_unlock_pi(uint32_t* uaddr)
{
    if (!uaddr) return KAPI_EINVAL;

    uint32_t zero = 0;
    __atomic_store_n(uaddr, zero, __ATOMIC_SEQ_CST);
    return kapi_futex_wake(uaddr, 1);
}

int kapi_futex_trylock_pi(uint32_t* uaddr)
{
    if (!uaddr) return KAPI_EINVAL;

    uint32_t zero = 0;
    if (__atomic_compare_exchange_n(uaddr, &zero, (uint32_t)kapi_proc_get_pid(kapi_proc_current()),
                                    false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
        return KAPI_OK;
    }

    return KAPI_EBUSY;
}

int kapi_futex_syscall(uint32_t* uaddr, int op, uint32_t val,
                       const struct timespec* timeout,
                       uint32_t* uaddr2, uint32_t val2, uint32_t val3)
{
    int cmd = op & ~KAPI_FUTEX_PRIVATE_FLAG & ~KAPI_FUTEX_CLOCK_REALTIME;

    switch (cmd) {
    case KAPI_FUTEX_WAIT:
        return kapi_futex_wait(uaddr, val, timeout);
    case KAPI_FUTEX_WAKE:
        return kapi_futex_wake(uaddr, val);
    case KAPI_FUTEX_REQUEUE:
        return kapi_futex_requeue(uaddr, uaddr2, (int)val, (int)val2);
    case KAPI_FUTEX_CMP_REQUEUE:
        return kapi_futex_cmp_requeue(uaddr, val, uaddr2, (int)val2, (int)val3);
    case KAPI_FUTEX_WAKE_OP:
        return kapi_futex_wake_op(uaddr, uaddr2, (int)val, (int)val2, (int)val3);
    case KAPI_FUTEX_WAIT_BITSET:
        return kapi_futex_wait_bitset(uaddr, val, timeout, val3);
    case KAPI_FUTEX_WAKE_BITSET:
        return kapi_futex_wake_bitset(uaddr, val, val3);
    case KAPI_FUTEX_LOCK_PI:
        return kapi_futex_lock_pi(uaddr, timeout);
    case KAPI_FUTEX_UNLOCK_PI:
        return kapi_futex_unlock_pi(uaddr);
    case KAPI_FUTEX_TRYLOCK_PI:
        return kapi_futex_trylock_pi(uaddr);
    default:
        return KAPI_ENOSYS;
    }
}