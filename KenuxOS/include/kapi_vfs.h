#ifndef KAPI_VFS_H
#define KAPI_VFS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct kapi_mount    kapi_mount_t;
typedef struct kapi_dentry   kapi_dentry_t;
typedef struct kapi_inode    kapi_inode_t;
typedef struct kapi_vfs_sb   kapi_vfs_sb_t;

#define KAPI_VFS_MS_RDONLY      (1ULL << 0)
#define KAPI_VFS_MS_NOSUID      (1ULL << 1)
#define KAPI_VFS_MS_NODEV       (1ULL << 2)
#define KAPI_VFS_MS_NOEXEC      (1ULL << 3)
#define KAPI_VFS_MS_SYNCHRONOUS (1ULL << 4)
#define KAPI_VFS_MS_REMOUNT     (1ULL << 5)
#define KAPI_VFS_MS_BIND        (1ULL << 6)
#define KAPI_VFS_MS_MOVE        (1ULL << 7)
#define KAPI_VFS_MS_NOATIME     (1ULL << 8)
#define KAPI_VFS_MS_NODIRATIME  (1ULL << 9)

#define KAPI_VFS_DT_UNKNOWN      0
#define KAPI_VFS_DT_FIFO         1
#define KAPI_VFS_DT_CHR          2
#define KAPI_VFS_DT_DIR          4
#define KAPI_VFS_DT_BLK          6
#define KAPI_VFS_DT_REG          8
#define KAPI_VFS_DT_LNK         10
#define KAPI_VFS_DT_SOCK        12
#define KAPI_VFS_DT_WHT         14

#define KAPI_VFS_S_IFMT   0170000
#define KAPI_VFS_S_IFSOCK  0140000
#define KAPI_VFS_S_IFLNK   0120000
#define KAPI_VFS_S_IFREG   0100000
#define KAPI_VFS_S_IFBLK   0060000
#define KAPI_VFS_S_IFDIR   0040000
#define KAPI_VFS_S_IFCHR   0020000
#define KAPI_VFS_S_IFIFO   0010000

#define KAPI_VFS_DCACHE_NAME_LEN 256

typedef struct {
    char     name[KAPI_VFS_DCACHE_NAME_LEN];
    uint64_t ino;
    uint32_t type;
    uint32_t namelen;
    uint64_t parent_ino;
} kapi_dentry_info_t;

typedef struct {
    uint64_t ino;
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint64_t size;
    uint64_t blocks;
    uint32_t blksize;
    uint32_t nlink;
    uint64_t atime;
    uint64_t mtime;
    uint64_t ctime;
    uint64_t atime_nsec;
    uint64_t mtime_nsec;
    uint64_t ctime_nsec;
} kapi_inode_info_t;

typedef struct {
    char     dev_name[64];
    char     dir_name[256];
    char     type[32];
    uint64_t flags;
    uint64_t block_size;
    uint64_t maxbytes;
    int      mounted;
} kapi_mount_info_t;

kapi_mount_t* kapi_vfs_kern_mount(const char* dev_name, const char* dir_name,
                                   const char* type, uint64_t flags, void* data);

int kapi_vfs_kern_umount(kapi_mount_t* mnt, uint64_t flags);

int kapi_vfs_umount_tree(kapi_mount_t* mnt, uint64_t flags);

int kapi_vfs_mount_info(kapi_mount_t* mnt, kapi_mount_info_t* info);

kapi_dentry_t* kapi_vfs_dentry_lookup(kapi_dentry_t* parent, const char* name);

kapi_dentry_t* kapi_vfs_dentry_create(kapi_dentry_t* parent, const char* name,
                                       uint32_t type, uint32_t mode);

int kapi_vfs_dentry_delete(kapi_dentry_t* dentry);

int kapi_vfs_dentry_get_info(kapi_dentry_t* dentry, kapi_dentry_info_t* info);

kapi_inode_t* kapi_vfs_dentry_get_inode(kapi_dentry_t* dentry);

void kapi_vfs_dentry_put(kapi_dentry_t* dentry);

kapi_inode_t* kapi_vfs_inode_alloc(kapi_vfs_sb_t* sb, uint32_t mode);

int kapi_vfs_inode_free(kapi_inode_t* inode);

int kapi_vfs_inode_get_info(kapi_inode_t* inode, kapi_inode_info_t* info);

int kapi_vfs_inode_set_attr(kapi_inode_t* inode, uint32_t mode, uint32_t uid,
                             uint32_t gid, uint64_t size);

int kapi_vfs_sync(void);

int kapi_vfs_init(void);

#ifdef __cplusplus
}
#endif

#endif