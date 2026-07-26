#include "fs.h"
#include <string.h>

static struct fs_file open_files[MAX_OPEN_FILES];
static struct fs_mount* file_mounts[MAX_OPEN_FILES];
static uint32_t next_fd = 1;

int fs_open(struct fs_mount* mount, const char* path, uint32_t mode, uint32_t* fd) {
    if (!mount || !path || !fd) {
        return -1;
    }
    
    uint64_t inode_num;
    int ret = fs_resolve_path(mount, path, &inode_num);
    if (ret != 0) {
        return ret;
    }
    
    for (uint32_t i = 1; i < MAX_OPEN_FILES; i++) {
        if (!open_files[i].inode_num) {
            *fd = i;
            open_files[i].inode_num = inode_num;
            open_files[i].offset = 0;
            open_files[i].mode = mode;
            open_files[i].is_directory = false;
            open_files[i].dir_pos = 0;
            file_mounts[i] = mount;
            
            struct fs_inode inode;
            ret = fs_read_inode(mount, inode_num, &inode);
            if (ret != 0) {
                open_files[i].inode_num = 0;
                file_mounts[i] = NULL;
                return ret;
            }
            
            open_files[i].is_directory = !!(inode.type & FS_TYPE_DIRECTORY);
            
            return 0;
        }
    }
    
    return -1;
}

int fs_close(uint32_t fd) {
    if (fd >= MAX_OPEN_FILES || !open_files[fd].inode_num) {
        return -1;
    }
    
    fs_fsync(fd);
    
    open_files[fd].inode_num = 0;
    open_files[fd].offset = 0;
    open_files[fd].mode = 0;
    open_files[fd].is_directory = false;
    open_files[fd].dir_pos = 0;
    file_mounts[fd] = NULL;
    
    return 0;
}

int fs_read(uint32_t fd, void* buffer, uint64_t size, uint64_t* bytes_read) {
    if (fd >= MAX_OPEN_FILES || !open_files[fd].inode_num || !buffer || !bytes_read) {
        return -1;
    }
    
    if (open_files[fd].is_directory) {
        return -1;
    }
    
    struct fs_mount* mount = file_mounts[fd];
    struct fs_inode inode;
    int ret = fs_read_inode(mount, open_files[fd].inode_num, &inode);
    if (ret != 0) {
        return ret;
    }
    
    if (!(inode.type & FS_TYPE_REGULAR)) {
        return -1;
    }
    
    *bytes_read = 0;
    uint8_t* buf = (uint8_t*)buffer;
    uint64_t offset = open_files[fd].offset;
    
    while (size > 0 && offset < inode.size) {
        uint64_t block_num;
        uint64_t offset_in_block;
        
        ret = fs_extent_find(mount, &inode, offset, &block_num, &offset_in_block);
        if (ret != 0) {
            break;
        }
        
        uint64_t to_read = BLOCK_SIZE - offset_in_block;
        if (to_read > size) {
            to_read = size;
        }
        if (to_read > inode.size - offset) {
            to_read = inode.size - offset;
        }
        
        uint8_t block_buffer[BLOCK_SIZE];
        ret = fs_read_block(mount, block_num, block_buffer);
        if (ret != 0) {
            break;
        }
        
        memcpy(buf, block_buffer + offset_in_block, to_read);
        
        buf += to_read;
        offset += to_read;
        *bytes_read += to_read;
        size -= to_read;
    }
    
    open_files[fd].offset = offset;
    
    return 0;
}

int fs_write(uint32_t fd, const void* buffer, uint64_t size, uint64_t* bytes_written) {
    if (fd >= MAX_OPEN_FILES || !open_files[fd].inode_num || !buffer || !bytes_written) {
        return -1;
    }
    
    if (open_files[fd].is_directory) {
        return -1;
    }
    
    struct fs_mount* mount = file_mounts[fd];
    struct fs_inode inode;
    int ret = fs_read_inode(mount, open_files[fd].inode_num, &inode);
    if (ret != 0) {
        return ret;
    }
    
    if (!(inode.type & FS_TYPE_REGULAR)) {
        return -1;
    }
    
    *bytes_written = 0;
    const uint8_t* buf = (const uint8_t*)buffer;
    uint64_t offset = open_files[fd].offset;
    
    while (size > 0) {
        uint64_t block_num;
        uint64_t offset_in_block;
        
        ret = fs_extent_find(mount, &inode, offset, &block_num, &offset_in_block);
        if (ret != 0) {
            ret = fs_extent_alloc(mount, &inode, offset, BLOCK_SIZE);
            if (ret != 0) {
                break;
            }
            
            ret = fs_extent_find(mount, &inode, offset, &block_num, &offset_in_block);
            if (ret != 0) {
                break;
            }
        }
        
        uint64_t to_write = BLOCK_SIZE - offset_in_block;
        if (to_write > size) {
            to_write = size;
        }
        
        uint8_t block_buffer[BLOCK_SIZE];
        ret = fs_read_block(mount, block_num, block_buffer);
        if (ret != 0) {
            break;
        }
        
        memcpy(block_buffer + offset_in_block, buf, to_write);
        
        ret = fs_write_block(mount, block_num, block_buffer);
        if (ret != 0) {
            break;
        }
        
        buf += to_write;
        offset += to_write;
        *bytes_written += to_write;
        size -= to_write;
    }
    
    if (offset > inode.size) {
        inode.size = offset;
        fs_write_inode(mount, open_files[fd].inode_num, &inode);
    }
    
    open_files[fd].offset = offset;
    
    return 0;
}

int fs_seek(uint32_t fd, int64_t offset, int whence) {
    if (fd >= MAX_OPEN_FILES || !open_files[fd].inode_num) {
        return -1;
    }
    
    struct fs_mount* mount = file_mounts[fd];
    struct fs_inode inode;
    int ret = fs_read_inode(mount, open_files[fd].inode_num, &inode);
    if (ret != 0) {
        return ret;
    }
    
    switch (whence) {
        case SEEK_SET:
            open_files[fd].offset = offset;
            break;
        case SEEK_CUR:
            open_files[fd].offset += offset;
            break;
        case SEEK_END:
            open_files[fd].offset = inode.size + offset;
            break;
        default:
            return -1;
    }
    
    if (open_files[fd].offset > inode.size && !(open_files[fd].mode & FS_PERM_WRITE)) {
        return -1;
    }
    
    return 0;
}

int fs_truncate(uint32_t fd, uint64_t size) {
    if (fd >= MAX_OPEN_FILES || !open_files[fd].inode_num) {
        return -1;
    }
    
    struct fs_mount* mount = file_mounts[fd];
    struct fs_inode inode;
    int ret = fs_read_inode(mount, open_files[fd].inode_num, &inode);
    if (ret != 0) {
        return ret;
    }
    
    if (size >= inode.size) {
        return 0;
    }
    
    ret = fs_extent_free(mount, &inode);
    if (ret != 0) {
        return ret;
    }
    
    inode.size = size;
    
    if (size > 0) {
        ret = fs_extent_alloc(mount, &inode, 0, size);
        if (ret != 0) {
            return ret;
        }
    }
    
    return fs_write_inode(mount, open_files[fd].inode_num, &inode);
}

int fs_stat(uint32_t fd, struct fs_stat* stat) {
    if (fd >= MAX_OPEN_FILES || !open_files[fd].inode_num || !stat) {
        return -1;
    }
    
    struct fs_mount* mount = file_mounts[fd];
    struct fs_inode inode;
    int ret = fs_read_inode(mount, open_files[fd].inode_num, &inode);
    if (ret != 0) {
        return ret;
    }
    
    stat->size = inode.size;
    stat->type = inode.type;
    stat->permissions = inode.permissions;
    stat->atime = inode.atime;
    stat->mtime = inode.mtime;
    stat->ctime = inode.ctime;
    stat->link_count = inode.link_count;
    
    return 0;
}

int fs_fsync(uint32_t fd) {
    if (fd >= MAX_OPEN_FILES || !open_files[fd].inode_num) {
        return -1;
    }
    
    struct fs_mount* mount = file_mounts[fd];
    
    fs_flush_cache(mount);
    fs_write_superblock(mount);
    
    return 0;
}