#include "fs.h"
#include <string.h>

int fs_lookup(struct fs_mount* mount, uint64_t dir_inode, const char* name, uint64_t* inode_num) {
    if (!mount || !name || !inode_num || dir_inode == 0) {
        return -1;
    }
    
    if (strcmp(name, ".") == 0) {
        *inode_num = dir_inode;
        return 0;
    }
    
    if (strcmp(name, "..") == 0) {
        struct fs_inode dir_inode_data;
        int ret = fs_read_inode(mount, dir_inode, &dir_inode_data);
        if (ret != 0) {
            return ret;
        }
        
        if (!(dir_inode_data.type & FS_TYPE_DIRECTORY)) {
            return -1;
        }
        
        uint8_t buffer[BLOCK_SIZE];
        ret = fs_read_block(mount, dir_inode_data.extents[0].start_block, buffer);
        if (ret != 0) {
            return ret;
        }
        
        struct fs_dir_entry* entry = (struct fs_dir_entry*)buffer;
        if (entry[1].inode != 0) {
            *inode_num = entry[1].inode;
            return 0;
        }
        
        *inode_num = dir_inode;
        return 0;
    }
    
    struct fs_inode inode;
    int ret = fs_read_inode(mount, dir_inode, &inode);
    if (ret != 0) {
        return ret;
    }
    
    if (!(inode.type & FS_TYPE_DIRECTORY)) {
        return -1;
    }
    
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
            
            if (strcmp(entries[j].name, name) == 0) {
                *inode_num = entries[j].inode;
                return 0;
            }
        }
    }
    
    return -1;
}

int fs_create_entry(struct fs_mount* mount, uint64_t dir_inode, const char* name, uint64_t inode_num, uint32_t type) {
    if (!mount || !name || dir_inode == 0 || inode_num == 0) {
        return -1;
    }
    
    if (strlen(name) > MAX_FILENAME) {
        return -1;
    }
    
    struct fs_inode dir_inode_data;
    int ret = fs_read_inode(mount, dir_inode, &dir_inode_data);
    if (ret != 0) {
        return ret;
    }
    
    if (!(dir_inode_data.type & FS_TYPE_DIRECTORY)) {
        return -1;
    }
    
    uint64_t found_block_num;
    uint32_t found_entry_idx;
    
    ret = fs_find_empty_dir_slot(mount, dir_inode, &found_block_num, &found_entry_idx);
    if (ret != 0) {
        return ret;
    }
    
    uint8_t buffer[BLOCK_SIZE];
    ret = fs_read_block(mount, found_block_num, buffer);
    if (ret != 0) {
        return ret;
    }
    
    uint64_t offset = found_entry_idx * sizeof(struct fs_dir_entry);
    struct fs_dir_entry* entry = (struct fs_dir_entry*)(buffer + offset);
    entry->inode = inode_num;
    entry->name_length = strlen(name);
    entry->type = type;
    memset(entry->name, 0, MAX_FILENAME + 1);
    memcpy(entry->name, name, entry->name_length);
    
    ret = fs_write_block(mount, found_block_num, buffer);
    if (ret != 0) {
        return ret;
    }
    
    if (offset + sizeof(struct fs_dir_entry) > dir_inode_data.size) {
        dir_inode_data.size = offset + sizeof(struct fs_dir_entry);
        return fs_write_inode(mount, dir_inode, &dir_inode_data);
    }
    
    return 0;
}

int fs_remove_entry(struct fs_mount* mount, uint64_t dir_inode, const char* name) {
    if (!mount || !name || dir_inode == 0) {
        return -1;
    }
    
    struct fs_inode dir_inode_data;
    int ret = fs_read_inode(mount, dir_inode, &dir_inode_data);
    if (ret != 0) {
        return ret;
    }
    
    if (!(dir_inode_data.type & FS_TYPE_DIRECTORY)) {
        return -1;
    }
    
    uint64_t num_blocks = (dir_inode_data.size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    
    for (uint64_t i = 0; i < num_blocks; i++) {
        uint64_t block_num;
        uint64_t offset_in_block;
        
        ret = fs_extent_find(mount, &dir_inode_data, i * BLOCK_SIZE, &block_num, &offset_in_block);
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
            
            if (strcmp(entries[j].name, name) == 0) {
                memset(&entries[j], 0, sizeof(struct fs_dir_entry));
                
                ret = fs_write_block(mount, block_num, buffer);
                if (ret != 0) {
                    return ret;
                }
                
                return 0;
            }
        }
    }
    
    return -1;
}

int fs_read_directory(struct fs_mount* mount, uint64_t dir_inode, struct fs_dir_entry* entries, uint32_t max_entries, uint32_t* count) {
    if (!mount || !entries || !count || dir_inode == 0) {
        return -1;
    }
    
    struct fs_inode inode;
    int ret = fs_read_inode(mount, dir_inode, &inode);
    if (ret != 0) {
        return ret;
    }
    
    if (!(inode.type & FS_TYPE_DIRECTORY)) {
        return -1;
    }
    
    *count = 0;
    
    uint64_t num_blocks = (inode.size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    
    for (uint64_t i = 0; i < num_blocks && *count < max_entries; i++) {
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
        
        struct fs_dir_entry* dir_entries = (struct fs_dir_entry*)buffer;
        
        for (uint32_t j = 0; j < DIR_ENTRIES_PER_BLOCK && *count < max_entries; j++) {
            if (dir_entries[j].inode != 0) {
                entries[*count] = dir_entries[j];
                (*count)++;
            }
        }
    }
    
    return 0;
}

int fs_find_empty_dir_slot(struct fs_mount* mount, uint64_t dir_inode, uint64_t* block_num, uint32_t* entry_idx) {
    if (!mount || !block_num || !entry_idx || dir_inode == 0) {
        return -1;
    }
    
    struct fs_inode inode;
    int ret = fs_read_inode(mount, dir_inode, &inode);
    if (ret != 0) {
        return ret;
    }
    
    uint64_t num_blocks = (inode.size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    
    for (uint64_t i = 0; i < num_blocks; i++) {
        uint64_t bnum;
        uint64_t offset_in_block;
        
        ret = fs_extent_find(mount, &inode, i * BLOCK_SIZE, &bnum, &offset_in_block);
        if (ret != 0 || offset_in_block != 0) {
            continue;
        }
        
        uint8_t buffer[BLOCK_SIZE];
        ret = fs_read_block(mount, bnum, buffer);
        if (ret != 0) {
            return ret;
        }
        
        struct fs_dir_entry* entries = (struct fs_dir_entry*)buffer;
        
        for (uint32_t j = 0; j < DIR_ENTRIES_PER_BLOCK; j++) {
            if (entries[j].inode == 0) {
                *block_num = bnum;
                *entry_idx = j;
                return 0;
            }
        }
    }
    
    uint64_t aligned_offset = ((inode.size + BLOCK_SIZE - 1) / BLOCK_SIZE) * BLOCK_SIZE;
    ret = fs_extent_alloc(mount, &inode, aligned_offset, BLOCK_SIZE);
    if (ret != 0) {
        return ret;
    }
    
    uint64_t offset_in_block;
    ret = fs_extent_find(mount, &inode, inode.size, block_num, &offset_in_block);
    if (ret != 0) {
        return ret;
    }
    
    uint8_t zero_buffer[BLOCK_SIZE] = {0};
    ret = fs_write_block(mount, *block_num, zero_buffer);
    if (ret != 0) {
        return ret;
    }
    
    inode.size += BLOCK_SIZE;
    ret = fs_write_inode(mount, dir_inode, &inode);
    if (ret != 0) {
        return ret;
    }
    
    *entry_idx = 0;
    return 0;
}