#include "syscall.h"
#include "fs.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static struct fs_mount* g_current_mount = NULL;
static char g_cwd[1024] = "/";

static int posix_mode_to_fs_perm(fs_mode_t mode) {
    int perm = 0;
    if (mode & (S_IRUSR | S_IRGRP | S_IROTH)) perm |= FS_PERM_READ;
    if (mode & (S_IWUSR | S_IWGRP | S_IWOTH)) perm |= FS_PERM_WRITE;
    if (mode & (S_IXUSR | S_IXGRP | S_IXOTH)) perm |= FS_PERM_EXEC;
    return perm;
}

static fs_mode_t fs_perm_to_posix_mode(uint32_t fs_perm) {
    fs_mode_t mode = 0;
    if (fs_perm & FS_PERM_READ) mode |= S_IRUSR | S_IRGRP | S_IROTH;
    if (fs_perm & FS_PERM_WRITE) mode |= S_IWUSR | S_IWGRP | S_IWOTH;
    if (fs_perm & FS_PERM_EXEC) mode |= S_IXUSR | S_IXGRP | S_IXOTH;
    return mode;
}

static int fs_type_to_posix_mode(uint32_t fs_type) {
    if (fs_type & FS_TYPE_DIRECTORY) return S_IFDIR;
    if (fs_type & FS_TYPE_REGULAR) return S_IFREG;
    if (fs_type & FS_TYPE_SYMLINK) return S_IFLNK;
    return S_IFREG;
}

static int resolve_path_to_inode(const char* pathname, uint64_t* inode_num) {
    if (!g_current_mount || !pathname || !inode_num) return -1;
    
    char full_path[1024];
    if (pathname[0] == '/') {
        strncpy(full_path, pathname, sizeof(full_path) - 1);
    } else {
        snprintf(full_path, sizeof(full_path), "%s/%s", g_cwd, pathname);
    }
    full_path[sizeof(full_path) - 1] = '\0';
    
    return fs_resolve_path(g_current_mount, full_path, inode_num);
}

static int resolve_fd_to_inode(int fd, uint64_t* inode_num) {
    if (!g_current_mount || fd < 0) return -1;
    return fs_resolve_path(g_current_mount, "/", inode_num);
}

void sys_set_mount(struct fs_mount* mount) {
    g_current_mount = mount;
}

struct fs_mount* sys_get_mount(void) {
    return g_current_mount;
}

int sys_open(const char* pathname, int flags, fs_mode_t mode) {
    if (!g_current_mount || !pathname) return -1;
    
    char full_path[1024];
    if (pathname[0] == '/') {
        strncpy(full_path, pathname, sizeof(full_path) - 1);
    } else {
        snprintf(full_path, sizeof(full_path), "%s/%s", g_cwd, pathname);
    }
    full_path[sizeof(full_path) - 1] = '\0';
    
    uint64_t inode_num;
    int ret;
    
    if (flags & O_CREAT) {
        uint32_t fs_type = (flags & O_DIRECTORY) ? FS_TYPE_DIRECTORY : FS_TYPE_REGULAR;
        ret = fs_create(g_current_mount, full_path, fs_type, &inode_num);
        if (ret != 0) return -1;
    } else {
        ret = fs_resolve_path(g_current_mount, full_path, &inode_num);
        if (ret != 0) return -1;
    }
    
    uint32_t fs_mode = 0;
    if (flags & O_RDONLY) fs_mode |= FS_PERM_READ;
    if (flags & O_WRONLY) fs_mode |= FS_PERM_WRITE;
    if (flags & O_RDWR) fs_mode |= FS_PERM_READ | FS_PERM_WRITE;
    
    uint32_t fd;
    ret = fs_open(g_current_mount, full_path, fs_mode, &fd);
    if (ret != 0) return -1;
    
    if (flags & O_TRUNC) {
        fs_truncate(fd, 0);
    }
    
    return fd;
}

int sys_close(int fd) {
    if (!g_current_mount || fd < 0) return -1;
    return fs_close(fd);
}

fs_ssize_t sys_read(int fd, void* buf, size_t count) {
    if (!g_current_mount || fd < 0 || !buf || count == 0) return -1;
    
    uint64_t bytes_read;
    int ret = fs_read(fd, buf, count, &bytes_read);
    if (ret != 0) return -1;
    
    return bytes_read;
}

fs_ssize_t sys_write(int fd, const void* buf, size_t count) {
    if (!g_current_mount || fd < 0 || !buf || count == 0) return -1;
    
    uint64_t bytes_written;
    int ret = fs_write(fd, buf, count, &bytes_written);
    if (ret != 0) return -1;
    
    return bytes_written;
}

fs_off_t sys_lseek(int fd, fs_off_t offset, int whence) {
    if (!g_current_mount || fd < 0) return -1;
    
    int ret = fs_seek(fd, offset, whence);
    if (ret != 0) return -1;
    
    return 0;
}

int sys_fstat(int fd, struct fs_stat_buf* buf) {
    if (!g_current_mount || fd < 0 || !buf) return -1;
    
    struct fs_stat fs_stat_buf;
    int ret = fs_stat(fd, &fs_stat_buf);
    if (ret != 0) return -1;
    
    buf->st_mode = fs_type_to_posix_mode(fs_stat_buf.type) | fs_perm_to_posix_mode(fs_stat_buf.permissions);
    buf->st_nlink = fs_stat_buf.link_count;
    buf->st_uid = 0;
    buf->st_gid = 0;
    buf->st_size = fs_stat_buf.size;
    buf->st_atime = fs_stat_buf.atime;
    buf->st_mtime = fs_stat_buf.mtime;
    buf->st_ctime = fs_stat_buf.ctime;
    
    return 0;
}

int sys_stat(const char* pathname, struct fs_stat_buf* buf) {
    if (!g_current_mount || !pathname || !buf) return -1;
    
    uint64_t inode_num;
    int ret = resolve_path_to_inode(pathname, &inode_num);
    if (ret != 0) return -1;
    
    struct fs_inode inode;
    ret = fs_read_inode(g_current_mount, inode_num, &inode);
    if (ret != 0) return -1;
    
    buf->st_mode = fs_type_to_posix_mode(inode.type) | fs_perm_to_posix_mode(inode.permissions);
    buf->st_nlink = inode.link_count;
    buf->st_uid = inode.uid;
    buf->st_gid = inode.gid;
    buf->st_size = inode.size;
    buf->st_atime = inode.atime;
    buf->st_mtime = inode.mtime;
    buf->st_ctime = inode.ctime;
    
    return 0;
}

int sys_mkdir(const char* pathname, fs_mode_t mode) {
    (void)mode;
    if (!g_current_mount || !pathname) return -1;
    
    char full_path[1024];
    if (pathname[0] == '/') {
        strncpy(full_path, pathname, sizeof(full_path) - 1);
    } else {
        snprintf(full_path, sizeof(full_path), "%s/%s", g_cwd, pathname);
    }
    full_path[sizeof(full_path) - 1] = '\0';
    
    return fs_mkdir(g_current_mount, full_path);
}

int sys_mkdirat(int dirfd, const char* pathname, fs_mode_t mode) {
    (void)dirfd;
    return sys_mkdir(pathname, mode);
}

int sys_rmdir(const char* pathname) {
    if (!g_current_mount || !pathname) return -1;
    
    char full_path[1024];
    if (pathname[0] == '/') {
        strncpy(full_path, pathname, sizeof(full_path) - 1);
    } else {
        snprintf(full_path, sizeof(full_path), "%s/%s", g_cwd, pathname);
    }
    full_path[sizeof(full_path) - 1] = '\0';
    
    return fs_remove(g_current_mount, full_path);
}

int sys_unlink(const char* pathname) {
    if (!g_current_mount || !pathname) return -1;
    
    char full_path[1024];
    if (pathname[0] == '/') {
        strncpy(full_path, pathname, sizeof(full_path) - 1);
    } else {
        snprintf(full_path, sizeof(full_path), "%s/%s", g_cwd, pathname);
    }
    full_path[sizeof(full_path) - 1] = '\0';
    
    return fs_remove(g_current_mount, full_path);
}

int sys_unlinkat(int dirfd, const char* pathname, int flags) {
    (void)dirfd;
    (void)flags;
    return sys_unlink(pathname);
}

int sys_link(const char* oldpath, const char* newpath) {
    (void)oldpath;
    (void)newpath;
    return -1;
}

int sys_symlink(const char* target, const char* linkpath) {
    (void)target;
    (void)linkpath;
    return -1;
}

int sys_readlink(const char* pathname, char* buf, size_t bufsiz) {
    (void)pathname;
    (void)buf;
    (void)bufsiz;
    return -1;
}

int sys_chmod(const char* pathname, fs_mode_t mode) {
    if (!g_current_mount || !pathname) return -1;
    
    uint64_t inode_num;
    int ret = resolve_path_to_inode(pathname, &inode_num);
    if (ret != 0) return -1;
    
    struct fs_inode inode;
    ret = fs_read_inode(g_current_mount, inode_num, &inode);
    if (ret != 0) return -1;
    
    inode.permissions = posix_mode_to_fs_perm(mode);
    
    return fs_write_inode(g_current_mount, inode_num, &inode);
}

int sys_fchmod(int fd, fs_mode_t mode) {
    if (!g_current_mount || fd < 0) return -1;
    
    struct fs_stat fs_stat_buf;
    int ret = fs_stat(fd, &fs_stat_buf);
    if (ret != 0) return -1;
    
    uint64_t inode_num;
    ret = fs_resolve_path(g_current_mount, "/", &inode_num);
    if (ret != 0) return -1;
    
    struct fs_inode inode;
    ret = fs_read_inode(g_current_mount, inode_num, &inode);
    if (ret != 0) return -1;
    
    inode.permissions = posix_mode_to_fs_perm(mode);
    
    return fs_write_inode(g_current_mount, inode_num, &inode);
}

int sys_truncate(const char* pathname, fs_off_t length) {
    if (!g_current_mount || !pathname) return -1;
    
    char full_path[1024];
    if (pathname[0] == '/') {
        strncpy(full_path, pathname, sizeof(full_path) - 1);
    } else {
        snprintf(full_path, sizeof(full_path), "%s/%s", g_cwd, pathname);
    }
    full_path[sizeof(full_path) - 1] = '\0';
    
    uint32_t fd;
    int ret = fs_open(g_current_mount, full_path, FS_PERM_WRITE, &fd);
    if (ret != 0) return -1;
    
    ret = fs_truncate(fd, length);
    fs_close(fd);
    
    return ret;
}

int sys_ftruncate(int fd, fs_off_t length) {
    if (!g_current_mount || fd < 0) return -1;
    return fs_truncate(fd, length);
}

int sys_access(const char* pathname, int mode) {
    if (!g_current_mount || !pathname) return -1;
    
    uint64_t inode_num;
    int ret = resolve_path_to_inode(pathname, &inode_num);
    if (ret != 0) return -1;
    
    struct fs_inode inode;
    ret = fs_read_inode(g_current_mount, inode_num, &inode);
    if (ret != 0) return -1;
    
    if ((mode & R_OK) && !(inode.permissions & FS_PERM_READ)) return -1;
    if ((mode & W_OK) && !(inode.permissions & FS_PERM_WRITE)) return -1;
    if ((mode & X_OK) && !(inode.permissions & FS_PERM_EXEC)) return -1;
    
    return 0;
}

int sys_chdir(const char* path) {
    if (!g_current_mount || !path) return -1;
    
    char full_path[1024];
    if (path[0] == '/') {
        strncpy(full_path, path, sizeof(full_path) - 1);
    } else {
        snprintf(full_path, sizeof(full_path), "%s/%s", g_cwd, path);
    }
    full_path[sizeof(full_path) - 1] = '\0';
    
    uint64_t inode_num;
    int ret = fs_resolve_path(g_current_mount, full_path, &inode_num);
    if (ret != 0) return -1;
    
    struct fs_inode inode;
    ret = fs_read_inode(g_current_mount, inode_num, &inode);
    if (ret != 0) return -1;
    
    if (!(inode.type & FS_TYPE_DIRECTORY)) return -1;
    
    strncpy(g_cwd, full_path, sizeof(g_cwd) - 1);
    g_cwd[sizeof(g_cwd) - 1] = '\0';
    
    return 0;
}

int sys_fchdir(int fd) {
    (void)fd;
    return -1;
}

char* sys_getcwd(char* buf, size_t size) {
    if (!buf || size == 0) return NULL;
    
    strncpy(buf, g_cwd, size - 1);
    buf[size - 1] = '\0';
    
    return buf;
}
