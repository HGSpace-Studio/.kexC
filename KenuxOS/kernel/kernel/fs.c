#include "fs.h"
#include <arch/fs.h>
#include <memory.h>
#include <string.h>
#include <slab.h>

static filesystem_t filesystems[FS_MAX];
static int fs_count = 0;

vfs_node_t* vfs_root = NULL;

static mount_point_t mount_points[FS_MAX_MOUNTS];
static int mount_count = 0;

void fs_init(void)
{
    memset(filesystems, 0, sizeof(filesystems));
    fs_count = 0;
    vfs_root = NULL;
}

int fs_register(filesystem_t* fs)
{
    if (fs_count >= FS_MAX) {
        return -1;
    }

    filesystems[fs_count] = *fs;
    fs_count++;
    return 0;
}

int fs_mount(const char* fs_name, const char* device, const char* mount_point)
{
    for (int i = 0; i < fs_count; i++) {
        if (strcmp(filesystems[i].name, fs_name) == 0) {
            return filesystems[i].mount(device, mount_point);
        }
    }
    return -1;
}

vfs_node_t* vfs_find_path(const char* path);

vfs_node_t* vfs_create_node(const char* name, uint64_t type)
{
    vfs_node_t* node = (vfs_node_t*)kzalloc(sizeof(vfs_node_t));
    if (!node) return NULL;
    if (name) {
        strncpy(node->name, name, sizeof(node->name) - 1);
        node->name[sizeof(node->name) - 1] = '\0';
    }
    node->type = type;
    node->inode = 0;
    node->size = 0;
    node->mode = 0755;
    return node;
}

void vfs_add_child(vfs_node_t* parent, vfs_node_t* child)
{
    if (!parent || !child) return;
    child->parent = parent;
    child->next = parent->children;
    parent->children = child;
}

static open_file_t open_files[FS_MAX_OPEN_FDS];
static int next_fd = 0;

int vfs_open(const char* path, int flags, int mode)
{
    (void)flags; (void)mode;
    vfs_node_t* node = vfs_find_path(path);
    if (!node) return -1;
    if (next_fd >= FS_MAX_OPEN_FDS) return -1;
    int fd = next_fd++;
    open_files[fd].node = node;
    open_files[fd].offset = 0;
    open_files[fd].flags = flags;
    open_files[fd].ref_count = 1;
    if (node->open) node->open(node, flags);
    return fd;
}

int vfs_close(int fd)
{
    if (fd < 0 || fd >= FS_MAX_OPEN_FDS) return -1;
    if (!open_files[fd].node) return -1;
    if (open_files[fd].node->close) open_files[fd].node->close(open_files[fd].node);
    open_files[fd].node = NULL;
    open_files[fd].offset = 0;
    open_files[fd].ref_count = 0;
    return 0;
}

int vfs_read(int fd, void* buf, uint64_t count)
{
    if (fd < 0 || fd >= FS_MAX_OPEN_FDS) return -1;
    open_file_t* f = &open_files[fd];
    if (!f->node || !f->node->read) return -1;
    int ret = f->node->read(f->node, f->offset, buf, count);
    if (ret > 0) f->offset += ret;
    return ret;
}

int vfs_write(int fd, const void* buf, uint64_t count)
{
    if (fd < 0 || fd >= FS_MAX_OPEN_FDS) return -1;
    open_file_t* f = &open_files[fd];
    if (!f->node || !f->node->write) return -1;
    int ret = f->node->write(f->node, f->offset, buf, count);
    if (ret > 0) f->offset += ret;
    return ret;
}

int vfs_lseek(int fd, int64_t offset, int whence)
{
    (void)whence;
    if (fd < 0 || fd >= FS_MAX_OPEN_FDS) return -1;
    open_file_t* f = &open_files[fd];
    if (!f->node) return -1;
    if (offset >= 0) f->offset = (uint64_t)offset;
    return (int)f->offset;
}

int fs_open(const char* name)
{
    return vfs_open(name, FS_O_RDONLY, 0);
}

int fs_close(int fd)
{
    return vfs_close(fd);
}

int fs_read(int fd, void* buffer, uint64_t size)
{
    return vfs_read(fd, buffer, size);
}

int fs_write(int fd, const void* buffer, uint64_t size)
{
    return vfs_write(fd, buffer, size);
}

int fs_seek(int fd, uint64_t offset)
{
    return vfs_lseek(fd, (int64_t)offset, 0);
}

int vfs_mount(const char* source, const char* target, const char* fstype)
{
    if (!fstype || !target) return -1;
    if (mount_count >= FS_MAX_MOUNTS) return -1;

    vfs_node_t* target_node = vfs_find_path(target);
    if (!target_node) return -1;

    for (int i = 0; i < fs_count; i++) {
        if (strcmp(filesystems[i].name, fstype) == 0) {
            int ret = filesystems[i].mount(source, target);
            if (ret == 0) {
                strncpy(mount_points[mount_count].mountpoint, target, 255);
                mount_points[mount_count].mountpoint[255] = '\0';
                mount_points[mount_count].root = target_node;
                mount_points[mount_count].mounted = 1;
                strncpy(mount_points[mount_count].fstype, fstype, 31);
                mount_points[mount_count].fstype[31] = '\0';
                mount_count++;
            }
            return ret;
        }
    }
    return -1;
}

int vfs_umount(const char* target)
{
    if (!target) return -1;
    for (int i = 0; i < mount_count; i++) {
        if (strcmp(mount_points[i].mountpoint, target) == 0) {
            for (int j = 0; j < fs_count; j++) {
                if (strcmp(filesystems[j].name, mount_points[i].fstype) == 0) {
                    int ret = filesystems[j].unmount(target);
                    if (ret == 0) {
                        mount_points[i].mounted = 0;
                        if (i < mount_count - 1) {
                            mount_points[i] = mount_points[mount_count - 1];
                        }
                        mount_count--;
                    }
                    return ret;
                }
            }
            return -1;
        }
    }
    return -1;
}

vfs_node_t* vfs_find_path(const char* path)
{
    if (!path || !vfs_root) return NULL;
    if (path[0] != '/') return NULL;
    if (strcmp(path, "/") == 0) return vfs_root;

    char temp[256];
    strncpy(temp, path, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    vfs_node_t* node = vfs_root;
    char* p = temp + 1;
    while (*p) {
        char* token = p;
        while (*p && *p != '/') p++;
        if (*p == '/') *p++ = '\0';
        if (!node->finddir) return NULL;
        node = node->finddir(node, token);
        if (!node) return NULL;
        while (*p == '/') p++;
    }
    return node;
}
