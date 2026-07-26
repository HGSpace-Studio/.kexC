#include "kapi_vfs.h"
#include "kapi.h"

#include <arch/fs.h>
#include <fs.h>
#include <arch/memory.h>
#include <string.h>
#include <stdio.h>

struct kapi_mount {
    void*    vfs_mount;
    char     dev_name[64];
    char     dir_name[256];
    char     type[32];
    uint64_t flags;
    int      valid;
    int      mounted;
};

struct kapi_dentry {
    void*    vfs_dentry;
    char     name[KAPI_VFS_DCACHE_NAME_LEN];
    uint32_t namelen;
    uint32_t type;
    kapi_inode_t* inode;
    kapi_dentry_t* parent;
    int      valid;
    int      refcount;
};

struct kapi_inode {
    void*    vfs_inode;
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
    int      valid;
    int      refcount;
};

struct kapi_vfs_sb {
    void*    vfs_sb;
    uint64_t block_size;
    uint64_t maxbytes;
    int      valid;
};

#define KAPI_VFS_MAX_MOUNTS   64
#define KAPI_VFS_MAX_DENTRIES 512
#define KAPI_VFS_MAX_INODES   512

static kapi_mount_t  kapi_mount_table[KAPI_VFS_MAX_MOUNTS];
static kapi_dentry_t kapi_dentry_table[KAPI_VFS_MAX_DENTRIES];
static kapi_inode_t  kapi_inode_table[KAPI_VFS_MAX_INODES];
static int kapi_vfs_initialized = 0;

static kapi_mount_t* kapi_alloc_mount(void)
{
    for (int i = 0; i < KAPI_VFS_MAX_MOUNTS; i++) {
        if (!kapi_mount_table[i].valid) {
            memset(&kapi_mount_table[i], 0, sizeof(kapi_mount_t));
            kapi_mount_table[i].valid = 1;
            return &kapi_mount_table[i];
        }
    }
    return NULL;
}

static kapi_dentry_t* kapi_alloc_dentry(void)
{
    for (int i = 0; i < KAPI_VFS_MAX_DENTRIES; i++) {
        if (!kapi_dentry_table[i].valid) {
            memset(&kapi_dentry_table[i], 0, sizeof(kapi_dentry_t));
            kapi_dentry_table[i].valid = 1;
            kapi_dentry_table[i].refcount = 1;
            return &kapi_dentry_table[i];
        }
    }
    return NULL;
}

static kapi_inode_t* kapi_alloc_inode(void)
{
    for (int i = 0; i < KAPI_VFS_MAX_INODES; i++) {
        if (!kapi_inode_table[i].valid) {
            memset(&kapi_inode_table[i], 0, sizeof(kapi_inode_t));
            kapi_inode_table[i].valid = 1;
            kapi_inode_table[i].refcount = 1;
            return &kapi_inode_table[i];
        }
    }
    return NULL;
}

int kapi_vfs_init(void)
{
    if (kapi_vfs_initialized) {
        return KAPI_OK;
    }
    memset(kapi_mount_table, 0, sizeof(kapi_mount_table));
    memset(kapi_dentry_table, 0, sizeof(kapi_dentry_table));
    memset(kapi_inode_table, 0, sizeof(kapi_inode_table));
    kapi_vfs_initialized = 1;
    return KAPI_OK;
}

kapi_mount_t* kapi_vfs_kern_mount(const char* dev_name, const char* dir_name,
                                   const char* type, uint64_t flags, void* data)
{
    if (!dir_name || !type) {
        return NULL;
    }

    kapi_mount_t* mnt = kapi_alloc_mount();
    if (!mnt) {
        return NULL;
    }

    if (dev_name) {
        strncpy(mnt->dev_name, dev_name, sizeof(mnt->dev_name) - 1);
    }
    strncpy(mnt->dir_name, dir_name, sizeof(mnt->dir_name) - 1);
    strncpy(mnt->type, type, sizeof(mnt->type) - 1);
    mnt->flags = flags;

    int ret = vfs_mount(mnt->dev_name, mnt->dir_name, mnt->type);
    if (ret != 0) {
        ret = fs_mount(mnt->type, mnt->dev_name, mnt->dir_name);
    }

    if (ret == 0) {
        mnt->mounted = 1;
    }

    (void)data;
    return mnt;
}

int kapi_vfs_kern_umount(kapi_mount_t* mnt, uint64_t flags)
{
    if (!mnt || !mnt->valid) {
        return KAPI_EINVAL;
    }

    int ret = vfs_umount(mnt->dir_name);
    if (ret == 0) {
        mnt->mounted = 0;
        mnt->valid = 0;
    }

    (void)flags;
    return ret == 0 ? KAPI_OK : KAPI_ERROR;
}

int kapi_vfs_umount_tree(kapi_mount_t* mnt, uint64_t flags)
{
    if (!mnt || !mnt->valid) {
        return KAPI_EINVAL;
    }

    int ret = kapi_vfs_kern_umount(mnt, flags);
    if (ret != KAPI_OK) {
        return ret;
    }

    for (int i = 0; i < KAPI_VFS_MAX_MOUNTS; i++) {
        if (kapi_mount_table[i].valid && kapi_mount_table[i].mounted) {
            if (strncmp(kapi_mount_table[i].dir_name, mnt->dir_name,
                        strlen(mnt->dir_name)) == 0) {
                kapi_vfs_kern_umount(&kapi_mount_table[i], flags);
            }
        }
    }
    return KAPI_OK;
}

int kapi_vfs_mount_info(kapi_mount_t* mnt, kapi_mount_info_t* info)
{
    if (!mnt || !mnt->valid || !info) {
        return KAPI_EINVAL;
    }

    memset(info, 0, sizeof(*info));
    strncpy(info->dev_name, mnt->dev_name, sizeof(info->dev_name) - 1);
    strncpy(info->dir_name, mnt->dir_name, sizeof(info->dir_name) - 1);
    strncpy(info->type, mnt->type, sizeof(info->type) - 1);
    info->flags = mnt->flags;
    info->mounted = mnt->mounted;
    return KAPI_OK;
}

kapi_dentry_t* kapi_vfs_dentry_lookup(kapi_dentry_t* parent, const char* name)
{
    if (!name) {
        return NULL;
    }

    vfs_node_t* vfs_parent = NULL;
    if (parent && parent->vfs_dentry) {
        vfs_parent = (vfs_node_t*)parent->vfs_dentry;
    } else {
        vfs_parent = vfs_root;
    }

    if (!vfs_parent || !vfs_parent->finddir) {
        return NULL;
    }

    vfs_node_t* node = vfs_parent->finddir(vfs_parent, name);
    if (!node) {
        return NULL;
    }

    kapi_dentry_t* dentry = kapi_alloc_dentry();
    if (!dentry) {
        return NULL;
    }

    dentry->vfs_dentry = node;
    strncpy(dentry->name, name, sizeof(dentry->name) - 1);
    dentry->namelen = (uint32_t)strlen(name);
    dentry->type = (uint32_t)node->type;
    dentry->parent = parent;
    return dentry;
}

kapi_dentry_t* kapi_vfs_dentry_create(kapi_dentry_t* parent, const char* name,
                                       uint32_t type, uint32_t mode)
{
    if (!parent || !name) {
        return NULL;
    }

    vfs_node_t* vfs_parent = (vfs_node_t*)parent->vfs_dentry;
    if (!vfs_parent) {
        vfs_parent = vfs_root;
    }

    if (!vfs_parent) {
        return NULL;
    }

    kapi_dentry_t* dentry = kapi_alloc_dentry();
    if (!dentry) {
        return NULL;
    }

    strncpy(dentry->name, name, sizeof(dentry->name) - 1);
    dentry->namelen = (uint32_t)strlen(name);
    dentry->type = type;
    dentry->parent = parent;

    if (vfs_parent->mkdir && (type == KAPI_VFS_DT_DIR)) {
        vfs_parent->mkdir(vfs_parent, name, mode);
    }

    kapi_inode_t* inode = kapi_alloc_inode();
    if (inode) {
        inode->mode = mode;
        inode->uid = 0;
        inode->gid = 0;
        inode->size = 0;
        inode->nlink = 1;
        dentry->inode = inode;
    }

    return dentry;
}

int kapi_vfs_dentry_delete(kapi_dentry_t* dentry)
{
    if (!dentry || !dentry->valid) {
        return KAPI_EINVAL;
    }

    vfs_node_t* parent = NULL;
    if (dentry->parent && dentry->parent->vfs_dentry) {
        parent = (vfs_node_t*)dentry->parent->vfs_dentry;
    } else {
        parent = vfs_root;
    }

    if (parent && parent->unlink) {
        parent->unlink(parent, dentry->name);
    }

    if (dentry->inode) {
        dentry->inode->valid = 0;
        dentry->inode = NULL;
    }
    dentry->valid = 0;
    return KAPI_OK;
}

int kapi_vfs_dentry_get_info(kapi_dentry_t* dentry, kapi_dentry_info_t* info)
{
    if (!dentry || !dentry->valid || !info) {
        return KAPI_EINVAL;
    }

    memset(info, 0, sizeof(*info));
    strncpy(info->name, dentry->name, sizeof(info->name) - 1);
    info->namelen = dentry->namelen;
    info->type = dentry->type;
    if (dentry->inode) {
        info->ino = dentry->inode->ino;
    }
    if (dentry->parent && dentry->parent->inode) {
        info->parent_ino = dentry->parent->inode->ino;
    }
    return KAPI_OK;
}

kapi_inode_t* kapi_vfs_dentry_get_inode(kapi_dentry_t* dentry)
{
    if (!dentry || !dentry->valid) {
        return NULL;
    }
    return dentry->inode;
}

void kapi_vfs_dentry_put(kapi_dentry_t* dentry)
{
    if (!dentry) {
        return;
    }
    dentry->refcount--;
    if (dentry->refcount <= 0) {
        dentry->valid = 0;
    }
}

kapi_inode_t* kapi_vfs_inode_alloc(kapi_vfs_sb_t* sb, uint32_t mode)
{
    kapi_inode_t* inode = kapi_alloc_inode();
    if (!inode) {
        return NULL;
    }

    static uint64_t next_ino = 1;
    inode->ino = next_ino++;
    inode->mode = mode;
    inode->nlink = 1;
    if (sb) {
        inode->blksize = (uint32_t)sb->block_size;
    }

    return inode;
}

int kapi_vfs_inode_free(kapi_inode_t* inode)
{
    if (!inode || !inode->valid) {
        return KAPI_EINVAL;
    }

    inode->valid = 0;
    return KAPI_OK;
}

int kapi_vfs_inode_get_info(kapi_inode_t* inode, kapi_inode_info_t* info)
{
    if (!inode || !inode->valid || !info) {
        return KAPI_EINVAL;
    }

    memset(info, 0, sizeof(*info));
    info->ino = inode->ino;
    info->mode = inode->mode;
    info->uid = inode->uid;
    info->gid = inode->gid;
    info->size = inode->size;
    info->blocks = inode->blocks;
    info->blksize = inode->blksize;
    info->nlink = inode->nlink;
    info->atime = inode->atime;
    info->mtime = inode->mtime;
    info->ctime = inode->ctime;
    return KAPI_OK;
}

int kapi_vfs_inode_set_attr(kapi_inode_t* inode, uint32_t mode, uint32_t uid,
                             uint32_t gid, uint64_t size)
{
    if (!inode || !inode->valid) {
        return KAPI_EINVAL;
    }

    inode->mode = mode;
    inode->uid = uid;
    inode->gid = gid;
    inode->size = size;
    return KAPI_OK;
}

int kapi_vfs_sync(void)
{
    return KAPI_OK;
}