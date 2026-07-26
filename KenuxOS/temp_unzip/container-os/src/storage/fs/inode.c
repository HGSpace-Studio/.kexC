#include "fs.h"
#include <string.h>

int fs_read_inode(struct fs_mount* mount, uint64_t inode_num, struct fs_inode* inode) {
    if (!mount || !inode || inode_num == 0) {
        return -1;
    }
    
    if (inode_num > mount->superblock.inode_count) {
        return -1;
    }
    
    uint64_t block_num = mount->superblock.inode_table_start + get_inode_block(inode_num);
    uint64_t offset = get_inode_offset(inode_num);
    
    uint8_t buffer[BLOCK_SIZE];
    int ret = fs_read_block(mount, block_num, buffer);
    if (ret != 0) {
        return ret;
    }
    
    memcpy(inode, buffer + offset, INODE_SIZE);
    
    if (inode->magic != FS_MAGIC) {
        return -1;
    }
    
    return 0;
}

int fs_write_inode(struct fs_mount* mount, uint64_t inode_num, const struct fs_inode* inode) {
    if (!mount || !inode || inode_num == 0) {
        return -1;
    }
    
    if (inode_num > mount->superblock.inode_count) {
        return -1;
    }
    
    for (uint32_t i = 0; i < INODE_CACHE_SIZE; i++) {
        if (mount->inode_cache[i].valid && mount->inode_cache[i].inode_num == inode_num) {
            memcpy(&mount->inode_cache[i].inode, inode, sizeof(struct fs_inode));
            mount->inode_cache[i].dirty = true;
            break;
        }
    }
    
    uint64_t block_num = mount->superblock.inode_table_start + get_inode_block(inode_num);
    uint64_t offset = get_inode_offset(inode_num);
    
    uint8_t buffer[BLOCK_SIZE];
    int ret = fs_read_block(mount, block_num, buffer);
    if (ret != 0) {
        return ret;
    }
    
    memcpy(buffer + offset, inode, INODE_SIZE);
    
    return fs_write_block(mount, block_num, buffer);
}

int fs_alloc_inode(struct fs_mount* mount, uint32_t type, uint64_t* inode_num) {
    if (!mount || !inode_num) {
        return -1;
    }
    
    if (mount->superblock.free_inodes == 0) {
        return -1;
    }
    
    uint64_t bitmap_blocks = (mount->superblock.inode_count + BLOCK_BITS - 1) / BLOCK_BITS;
    uint8_t buffer[BLOCK_SIZE];
    
    for (uint64_t i = 0; i < bitmap_blocks; i++) {
        uint64_t block_num = mount->superblock.inode_bitmap_start + i;
        int ret = fs_read_block(mount, block_num, buffer);
        if (ret != 0) {
            return ret;
        }
        
        for (uint32_t j = 0; j < BLOCK_SIZE; j++) {
            if (buffer[j] != 0xFF) {
                for (uint32_t k = 0; k < 8; k++) {
                    if (!(buffer[j] & (1 << k))) {
                        *inode_num = i * BLOCK_BITS + j * 8 + k + 1;
                        
                        buffer[j] |= (1 << k);
                        ret = fs_write_block(mount, block_num, buffer);
                        if (ret != 0) {
                            return ret;
                        }
                        
                        mount->superblock.free_inodes--;
                        
                        struct fs_inode inode;
                        memset(&inode, 0, sizeof(inode));
                        inode.magic = FS_MAGIC;
                        inode.type = type;
                        inode.permissions = FS_PERM_READ | FS_PERM_WRITE;
                        inode.link_count = type == FS_TYPE_DIRECTORY ? 2 : 1;
                        inode.generation = 0;
                        
                        return fs_write_inode(mount, *inode_num, &inode);
                    }
                }
            }
        }
    }
    
    return -1;
}

int fs_free_inode(struct fs_mount* mount, uint64_t inode_num) {
    if (!mount || inode_num == 0) {
        return -1;
    }
    
    if (inode_num > mount->superblock.inode_count) {
        return -1;
    }
    
    struct fs_inode inode;
    int ret = fs_read_inode(mount, inode_num, &inode);
    if (ret != 0) {
        return ret;
    }
    
    if (inode.link_count > 0) {
        return -1;
    }
    
    if (inode.size > 0) {
        fs_extent_free(mount, &inode);
    }
    
    uint64_t bitmap_block = (inode_num - 1) / BLOCK_BITS;
    uint64_t bitmap_byte = ((inode_num - 1) % BLOCK_BITS) / 8;
    uint32_t bitmap_bit = ((inode_num - 1) % BLOCK_BITS) % 8;
    
    uint8_t buffer[BLOCK_SIZE];
    ret = fs_read_block(mount, mount->superblock.inode_bitmap_start + bitmap_block, buffer);
    if (ret != 0) {
        return ret;
    }
    
    buffer[bitmap_byte] &= ~(1 << bitmap_bit);
    
    ret = fs_write_block(mount, mount->superblock.inode_bitmap_start + bitmap_block, buffer);
    if (ret != 0) {
        return ret;
    }
    
    mount->superblock.free_inodes++;
    
    memset(&inode, 0, sizeof(inode));
    return fs_write_inode(mount, inode_num, &inode);
}

int fs_get_inode(struct fs_mount* mount, uint64_t inode_num, struct fs_inode_cache** cache) {
    if (!mount || !cache || inode_num == 0) {
        return -1;
    }
    
    for (uint32_t i = 0; i < INODE_CACHE_SIZE; i++) {
        if (mount->inode_cache[i].valid && mount->inode_cache[i].inode_num == inode_num) {
            mount->inode_cache[i].ref_count++;
            mount->inode_cache[i].last_access = 0;
            *cache = &mount->inode_cache[i];
            mount->cache_hits++;
            return 0;
        }
    }
    
    mount->cache_misses++;
    
    uint32_t lru_idx = 0;
    uint64_t oldest = mount->inode_cache[0].last_access;
    
    for (uint32_t i = 1; i < INODE_CACHE_SIZE; i++) {
        if (!mount->inode_cache[i].valid) {
            lru_idx = i;
            break;
        }
        if (mount->inode_cache[i].last_access > oldest) {
            oldest = mount->inode_cache[i].last_access;
            lru_idx = i;
        }
    }
    
    if (mount->inode_cache[lru_idx].dirty) {
        fs_write_inode(mount, mount->inode_cache[lru_idx].inode_num, &mount->inode_cache[lru_idx].inode);
    }
    
    int ret = fs_read_inode(mount, inode_num, &mount->inode_cache[lru_idx].inode);
    if (ret != 0) {
        return ret;
    }
    
    mount->inode_cache[lru_idx].inode_num = inode_num;
    mount->inode_cache[lru_idx].valid = true;
    mount->inode_cache[lru_idx].dirty = false;
    mount->inode_cache[lru_idx].ref_count = 1;
    mount->inode_cache[lru_idx].last_access = 0;
    
    *cache = &mount->inode_cache[lru_idx];
    
    return 0;
}

int fs_put_inode(struct fs_mount* mount, uint64_t inode_num) {
    if (!mount || inode_num == 0) {
        return -1;
    }
    
    for (uint32_t i = 0; i < INODE_CACHE_SIZE; i++) {
        if (mount->inode_cache[i].valid && mount->inode_cache[i].inode_num == inode_num) {
            mount->inode_cache[i].ref_count--;
            if (mount->inode_cache[i].ref_count == 0) {
                mount->inode_cache[i].last_access++;
            }
            return 0;
        }
    }
    
    return -1;
}

int fs_sync_inode(struct fs_mount* mount, uint64_t inode_num) {
    if (!mount || inode_num == 0) {
        return -1;
    }
    
    for (uint32_t i = 0; i < INODE_CACHE_SIZE; i++) {
        if (mount->inode_cache[i].valid && mount->inode_cache[i].inode_num == inode_num && mount->inode_cache[i].dirty) {
            int ret = fs_write_inode(mount, inode_num, &mount->inode_cache[i].inode);
            if (ret != 0) {
                return ret;
            }
            mount->inode_cache[i].dirty = false;
            return 0;
        }
    }
    
    return 0;
}