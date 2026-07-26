#ifndef KAPI_H
#define KAPI_H

#define KAPI_VERSION_MAJOR 1
#define KAPI_VERSION_MINOR 0
#define KAPI_VERSION_PATCH 0

#include <stdint.h>
#include <stddef.h>

#define KAPI_OK          0
#define KAPI_ERROR      -1
#define KAPI_EINVAL     -2
#define KAPI_ENOMEM     -3
#define KAPI_ENOENT     -4
#define KAPI_EACCES     -5
#define KAPI_EBUSY      -6
#define KAPI_ENOSYS     -7
#define KAPI_EAGAIN     -8

#include "kapi_list.h"
#include "kapi_atomic.h"
#include "kapi_bitmap.h"
#include "kapi_kfifo.h"
#include "kapi_idr.h"
#include "kapi_rbtree.h"
#include "kapi_hash.h"
#include "kapi_sort.h"
#include "kapi_crc.h"

#include "kapi_mutex.h"
#include "kapi_completion.h"
#include "kapi_wait.h"
#include "kapi_rcu.h"

#include "kapi_string.h"
#include "kapi_time.h"
#include "kapi_random.h"

#include "kapi_irq.h"
#include "kapi_kthread.h"
#include "kapi_notifier.h"
#include "kapi_sched.h"
#include "kapi_smp.h"
#include "kapi_cpumask.h"
#include "kapi_module.h"
#include "kapi_params.h"

#include "kapi_io.h"
#include "kapi_pci.h"
#include "kapi_dma.h"
#include "kapi_cdev.h"
#include "kapi_blkdev.h"
#include "kapi_security.h"

#include "kapi_skbuff.h"
#include "kapi_netdevice.h"
#include "kapi_socket.h"
#include "kapi_netlink.h"

#include "kapi_vfs.h"
#include "kapi_seq_file.h"
#include "kapi_debugfs.h"
#include "kapi_kobject.h"
#include "kapi_mempool.h"
#include "kapi_ftrace.h"

static inline void kapi_get_version(int* major, int* minor, int* patch)
{
    if (major) *major = KAPI_VERSION_MAJOR;
    if (minor) *minor = KAPI_VERSION_MINOR;
    if (patch) *patch = KAPI_VERSION_PATCH;
}

int kapi_init(void);

#endif