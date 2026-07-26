#include "fs.h"
#include <string.h>

int fs_extent_find(struct fs_mount* mount, struct fs_inode* inode, uint64_t offset, uint64_t* block_num, uint64_t* offset_in_block) {
    if (!mount || !inode || !block_num || !offset_in_block) {
        return -1;
    }
    
    uint64_t block_index = offset / BLOCK_SIZE;
    *offset_in_block = offset % BLOCK_SIZE;
    
    for (uint32_t i = 0; i < DIRECT_EXTENTS; i++) {
        if (inode->extents[i].length == 0) {
            continue;
        }
        
        if (block_index < inode->extents[i].length) {
            *block_num = inode->extents[i].start_block + block_index;
            return 0;
        }
        
        block_index -= inode->extents[i].length;
    }
    
    if (inode->indirect_block != 0) {
        uint8_t buffer[BLOCK_SIZE];
        int ret = fs_read_block(mount, inode->indirect_block, buffer);
        if (ret != 0) {
            return ret;
        }
        
        uint64_t* indirect_entries = (uint64_t*)buffer;
        uint64_t entries_per_block = BLOCK_SIZE / sizeof(uint64_t);
        
        for (uint64_t i = 0; i < entries_per_block; i++) {
            if (indirect_entries[i] == 0) {
                continue;
            }
            
            if (block_index == 0) {
                *block_num = indirect_entries[i];
                return 0;
            }
            
            block_index--;
        }
    }
    
    if (inode->double_indirect_block != 0) {
        uint8_t buffer[BLOCK_SIZE];
        int ret = fs_read_block(mount, inode->double_indirect_block, buffer);
        if (ret != 0) {
            return ret;
        }
        
        uint64_t* indirect_entries = (uint64_t*)buffer;
        uint64_t entries_per_block = BLOCK_SIZE / sizeof(uint64_t);
        
        for (uint64_t i = 0; i < entries_per_block; i++) {
            if (indirect_entries[i] == 0) {
                continue;
            }
            
            uint8_t inner_buffer[BLOCK_SIZE];
            ret = fs_read_block(mount, indirect_entries[i], inner_buffer);
            if (ret != 0) {
                continue;
            }
            
            uint64_t* inner_entries = (uint64_t*)inner_buffer;
            
            for (uint64_t j = 0; j < entries_per_block; j++) {
                if (inner_entries[j] == 0) {
                    continue;
                }
                
                if (block_index == 0) {
                    *block_num = inner_entries[j];
                    return 0;
                }
                
                block_index--;
            }
        }
    }
    
    return -1;
}

int fs_extent_alloc(struct fs_mount* mount, struct fs_inode* inode, uint64_t start_offset, uint64_t length) {
    if (!mount || !inode || length == 0) {
        return -1;
    }
    
    if (start_offset % BLOCK_SIZE != 0) {
        return -1;
    }
    
    uint64_t num_blocks = (length + BLOCK_SIZE - 1) / BLOCK_SIZE;
    uint64_t start_block_index = start_offset / BLOCK_SIZE;
    uint64_t current_block_index = start_block_index;
    uint64_t remaining = num_blocks;
    
    while (remaining > 0) {
        uint64_t extent_index = current_block_index;
        uint64_t offset_in_extent = 0;
        bool in_direct = false;
        bool in_indirect = false;
        bool in_double_indirect = false;
        uint64_t indirect_index = 0;
        uint64_t double_indirect_index = 0;
        
        if (extent_index < DIRECT_EXTENTS) {
            in_direct = true;
        } else {
            extent_index -= DIRECT_EXTENTS;
            if (extent_index < (BLOCK_SIZE / sizeof(uint64_t))) {
                in_indirect = true;
                indirect_index = extent_index;
            } else {
                extent_index -= (BLOCK_SIZE / sizeof(uint64_t));
                in_double_indirect = true;
                double_indirect_index = extent_index / (BLOCK_SIZE / sizeof(uint64_t));
                indirect_index = extent_index % (BLOCK_SIZE / sizeof(uint64_t));
            }
        }
        
        if (in_direct) {
            if (inode->extents[current_block_index].length == 0) {
                uint64_t block_num;
                int ret = fs_alloc_block(mount, &block_num);
                if (ret != 0) {
                    return ret;
                }
                
                inode->extents[current_block_index].start_block = block_num;
                inode->extents[current_block_index].length = 1;
                remaining--;
            }
            current_block_index++;
        } else if (in_indirect) {
            if (inode->indirect_block == 0) {
                uint64_t block_num;
                int ret = fs_alloc_block(mount, &block_num);
                if (ret != 0) {
                    return ret;
                }
                
                uint8_t zero_buffer[BLOCK_SIZE] = {0};
                ret = fs_write_block(mount, block_num, zero_buffer);
                if (ret != 0) {
                    fs_free_block(mount, block_num);
                    return ret;
                }
                
                inode->indirect_block = block_num;
            }
            
            uint8_t buffer[BLOCK_SIZE];
            int ret = fs_read_block(mount, inode->indirect_block, buffer);
            if (ret != 0) {
                return ret;
            }
            
            uint64_t* entries = (uint64_t*)buffer;
            
            if (entries[indirect_index] == 0) {
                uint64_t block_num;
                ret = fs_alloc_block(mount, &block_num);
                if (ret != 0) {
                    return ret;
                }
                
                entries[indirect_index] = block_num;
                remaining--;
            }
            
            ret = fs_write_block(mount, inode->indirect_block, buffer);
            if (ret != 0) {
                return ret;
            }
            
            current_block_index++;
        } else if (in_double_indirect) {
            if (inode->double_indirect_block == 0) {
                uint64_t block_num;
                int ret = fs_alloc_block(mount, &block_num);
                if (ret != 0) {
                    return ret;
                }
                
                uint8_t zero_buffer[BLOCK_SIZE] = {0};
                ret = fs_write_block(mount, block_num, zero_buffer);
                if (ret != 0) {
                    fs_free_block(mount, block_num);
                    return ret;
                }
                
                inode->double_indirect_block = block_num;
            }
            
            uint8_t outer_buffer[BLOCK_SIZE];
            int ret = fs_read_block(mount, inode->double_indirect_block, outer_buffer);
            if (ret != 0) {
                return ret;
            }
            
            uint64_t* outer_entries = (uint64_t*)outer_buffer;
            
            if (outer_entries[double_indirect_index] == 0) {
                uint64_t block_num;
                ret = fs_alloc_block(mount, &block_num);
                if (ret != 0) {
                    return ret;
                }
                
                uint8_t zero_buffer[BLOCK_SIZE] = {0};
                ret = fs_write_block(mount, block_num, zero_buffer);
                if (ret != 0) {
                    fs_free_block(mount, block_num);
                    return ret;
                }
                
                outer_entries[double_indirect_index] = block_num;
                
                ret = fs_write_block(mount, inode->double_indirect_block, outer_buffer);
                if (ret != 0) {
                    return ret;
                }
            }
            
            uint8_t inner_buffer[BLOCK_SIZE];
            ret = fs_read_block(mount, outer_entries[double_indirect_index], inner_buffer);
            if (ret != 0) {
                return ret;
            }
            
            uint64_t* inner_entries = (uint64_t*)inner_buffer;
            
            if (inner_entries[indirect_index] == 0) {
                uint64_t block_num;
                ret = fs_alloc_block(mount, &block_num);
                if (ret != 0) {
                    return ret;
                }
                
                inner_entries[indirect_index] = block_num;
                remaining--;
            }
            
            ret = fs_write_block(mount, outer_entries[double_indirect_index], inner_buffer);
            if (ret != 0) {
                return ret;
            }
            
            current_block_index++;
        }
    }
    
    return 0;
}

int fs_extent_free(struct fs_mount* mount, struct fs_inode* inode) {
    if (!mount || !inode) {
        return -1;
    }
    
    for (uint32_t i = 0; i < TOTAL_EXTENTS; i++) {
        if (inode->extents[i].length > 0) {
            for (uint64_t j = 0; j < inode->extents[i].length; j++) {
                fs_free_block(mount, inode->extents[i].start_block + j);
            }
            inode->extents[i].start_block = 0;
            inode->extents[i].length = 0;
        }
    }
    
    if (inode->indirect_block != 0) {
        uint8_t buffer[BLOCK_SIZE];
        int ret = fs_read_block(mount, inode->indirect_block, buffer);
        if (ret == 0) {
            uint64_t* entries = (uint64_t*)buffer;
            uint64_t entries_per_block = BLOCK_SIZE / sizeof(uint64_t);
            
            for (uint64_t i = 0; i < entries_per_block; i++) {
                if (entries[i] != 0) {
                    fs_free_block(mount, entries[i]);
                }
            }
        }
        
        fs_free_block(mount, inode->indirect_block);
        inode->indirect_block = 0;
    }
    
    if (inode->double_indirect_block != 0) {
        uint8_t buffer[BLOCK_SIZE];
        int ret = fs_read_block(mount, inode->double_indirect_block, buffer);
        if (ret == 0) {
            uint64_t* entries = (uint64_t*)buffer;
            uint64_t entries_per_block = BLOCK_SIZE / sizeof(uint64_t);
            
            for (uint64_t i = 0; i < entries_per_block; i++) {
                if (entries[i] != 0) {
                    uint8_t inner_buffer[BLOCK_SIZE];
                    int inner_ret = fs_read_block(mount, entries[i], inner_buffer);
                    if (inner_ret == 0) {
                        uint64_t* inner_entries = (uint64_t*)inner_buffer;
                        
                        for (uint64_t j = 0; j < entries_per_block; j++) {
                            if (inner_entries[j] != 0) {
                                fs_free_block(mount, inner_entries[j]);
                            }
                        }
                    }
                    
                    fs_free_block(mount, entries[i]);
                }
            }
        }
        
        fs_free_block(mount, inode->double_indirect_block);
        inode->double_indirect_block = 0;
    }
    
    return 0;
}