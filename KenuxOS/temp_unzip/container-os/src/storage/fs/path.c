#include "fs.h"
#include <string.h>
#include <stdio.h>

static const char* path_next_component(const char* path, char* component, uint32_t max_len) {
    if (!path || !component || max_len == 0) {
        return NULL;
    }
    
    memset(component, 0, max_len);
    
    while (*path == '/') {
        path++;
    }
    
    if (*path == '\0') {
        return NULL;
    }
    
    uint32_t i = 0;
    while (*path != '\0' && *path != '/' && i < max_len - 1) {
        component[i++] = *path++;
    }
    component[i] = '\0';
    
    while (*path == '/') {
        path++;
    }
    
    return path;
}

int fs_resolve_path(struct fs_mount* mount, const char* path, uint64_t* inode_num) {
    if (!mount || !path || !inode_num) {
        return -1;
    }
    
    uint64_t current_inode = mount->root_inode;
    
    if (path[0] == '/') {
        path++;
    } else {
        current_inode = 1;
    }
    
    char component[MAX_FILENAME + 1];
    
    while (1) {
        const char* next = path_next_component(path, component, sizeof(component));
        
        if (component[0] == '\0') {
            *inode_num = current_inode;
            return 0;
        }
        
        int ret = fs_lookup(mount, current_inode, component, &current_inode);
        if (ret != 0) {
            return ret;
        }
        
        if (next == NULL) {
            *inode_num = current_inode;
            return 0;
        }
        
        path = next;
    }
}

int fs_create(struct fs_mount* mount, const char* path, uint32_t type, uint64_t* inode_num) {
    if (!mount || !path || !inode_num) {
        return -1;
    }
    
    char path_copy[1024];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';
    
    char* last_slash = strrchr(path_copy, '/');
    if (!last_slash) {
        return -1;
    }
    
    char* name = last_slash + 1;
    *last_slash = '\0';
    
    uint64_t parent_inode;
    int ret = fs_resolve_path(mount, path_copy[0] ? path_copy : "/", &parent_inode);
    if (ret != 0) {
        return ret;
    }
    
    uint64_t existing_inode;
    ret = fs_lookup(mount, parent_inode, name, &existing_inode);
    if (ret == 0) {
        *inode_num = existing_inode;
        return 0;
    }
    
    ret = fs_alloc_inode(mount, type, inode_num);
    if (ret != 0) {
        return ret;
    }
    
    ret = fs_create_entry(mount, parent_inode, name, *inode_num, type);
    if (ret != 0) {
        fs_free_inode(mount, *inode_num);
        *inode_num = 0;
        return ret;
    }
    
    struct fs_inode inode;
    ret = fs_read_inode(mount, *inode_num, &inode);
    if (ret != 0) {
        return ret;
    }
    
    if (type == FS_TYPE_DIRECTORY) {
        uint64_t block_num;
        ret = fs_alloc_block(mount, &block_num);
        if (ret != 0) {
            fs_free_inode(mount, *inode_num);
            return ret;
        }
        
        uint8_t buffer[BLOCK_SIZE] = {0};
        
        ((struct fs_dir_entry*)buffer)->inode = *inode_num;
        ((struct fs_dir_entry*)buffer)->name_length = 1;
        ((struct fs_dir_entry*)buffer)->type = FS_TYPE_DIRECTORY;
        ((struct fs_dir_entry*)buffer)->name[0] = '.';
        
        ((struct fs_dir_entry*)(buffer + sizeof(struct fs_dir_entry)))->inode = parent_inode;
        ((struct fs_dir_entry*)(buffer + sizeof(struct fs_dir_entry)))->name_length = 2;
        ((struct fs_dir_entry*)(buffer + sizeof(struct fs_dir_entry)))->type = FS_TYPE_DIRECTORY;
        ((struct fs_dir_entry*)(buffer + sizeof(struct fs_dir_entry)))->name[0] = '.';
        ((struct fs_dir_entry*)(buffer + sizeof(struct fs_dir_entry)))->name[1] = '.';
        
        ret = fs_write_block(mount, block_num, buffer);
        if (ret != 0) {
            fs_free_block(mount, block_num);
            fs_free_inode(mount, *inode_num);
            return ret;
        }
        
        inode.size = BLOCK_SIZE;
        inode.extents[0].start_block = block_num;
        inode.extents[0].length = 1;
        inode.link_count = 2;
        
        ret = fs_write_inode(mount, *inode_num, &inode);
        if (ret != 0) {
            fs_free_block(mount, block_num);
            fs_free_inode(mount, *inode_num);
            return ret;
        }
    }
    
    return 0;
}

int fs_remove(struct fs_mount* mount, const char* path) {
    if (!mount || !path) {
        return -1;
    }
    
    char path_copy[1024];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';
    
    char* last_slash = strrchr(path_copy, '/');
    if (!last_slash) {
        return -1;
    }
    
    char* name = last_slash + 1;
    *last_slash = '\0';
    
    uint64_t parent_inode;
    int ret = fs_resolve_path(mount, path_copy[0] ? path_copy : "/", &parent_inode);
    if (ret != 0) {
        return ret;
    }
    
    uint64_t target_inode;
    ret = fs_lookup(mount, parent_inode, name, &target_inode);
    if (ret != 0) {
        return ret;
    }
    
    struct fs_inode inode;
    ret = fs_read_inode(mount, target_inode, &inode);
    if (ret != 0) {
        return ret;
    }
    
    if (inode.type & FS_TYPE_DIRECTORY) {
        /* 目录非空检查：遍历目录项，除了 "." 和 ".." 外不能有其他条目 */
        uint64_t num_blocks = (inode.size + BLOCK_SIZE - 1) / BLOCK_SIZE;
        for (uint64_t i = 0; i < num_blocks; i++) {
            uint64_t block_num;
            uint64_t offset_in_block;
            ret = fs_extent_find(mount, &inode, i * BLOCK_SIZE, &block_num, &offset_in_block);
            if (ret != 0 || offset_in_block != 0) {
                continue;
            }
            
            uint8_t buffer[BLOCK_SIZE];
            ret = fs_read_block(mount, block_num, buffer);
            if (ret != 0) {
                return ret;
            }
            
            struct fs_dir_entry* entries = (struct fs_dir_entry*)buffer;
            for (uint32_t j = 0; j < DIR_ENTRIES_PER_BLOCK; j++) {
                if (entries[j].inode == 0) {
                    continue;
                }
                if (strcmp(entries[j].name, ".") == 0 || strcmp(entries[j].name, "..") == 0) {
                    continue;
                }
                /* 目录中有其他条目，非空 */
                return -1;
            }
        }
    }
    
    ret = fs_remove_entry(mount, parent_inode, name);
    if (ret != 0) {
        return ret;
    }
    
    if (inode.type & FS_TYPE_DIRECTORY) {
        struct fs_inode parent_dir;
        ret = fs_read_inode(mount, parent_inode, &parent_dir);
        if (ret == 0) {
            parent_dir.link_count--;
            fs_write_inode(mount, parent_inode, &parent_dir);
        }
    }
    
    inode.link_count--;
    /* 必须先把更新后的 link_count 写回磁盘，否则 fs_free_inode
     * 重新读取时会看到旧的 link_count 并拒绝释放。 */
    ret = fs_write_inode(mount, target_inode, &inode);
    if (ret != 0) {
        return ret;
    }
    
    if (inode.link_count == 0) {
        return fs_free_inode(mount, target_inode);
    }
    
    return 0;
}

int fs_mkdir(struct fs_mount* mount, const char* path) {
    if (!mount || !path) {
        return -1;
    }
    
    uint64_t inode_num;
    int ret = fs_create(mount, path, FS_TYPE_DIRECTORY, &inode_num);
    if (ret != 0) {
        return ret;
    }
    
    return 0;
}
