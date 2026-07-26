#include "fs.h"
#include <string.h>
#include <stdio.h>

bool fs_is_block_free(struct fs_mount* mount, uint64_t block_num) {
    if (!mount) {
        return false;
    }
    
    if (block_num >= mount->superblock.block_count) {
        return false;
    }
    
    if (block_num < mount->superblock.data_start) {
        return false;
    }
    
    uint64_t data_block_num = block_num - mount->superblock.data_start;
    uint64_t bitmap_block = data_block_num / BLOCK_BITS;
    uint64_t bitmap_byte = (data_block_num % BLOCK_BITS) / 8;
    uint32_t bitmap_bit = (data_block_num % BLOCK_BITS) % 8;
    
    uint8_t buffer[BLOCK_SIZE];
    int ret = fs_read_block(mount, mount->superblock.block_bitmap_start + bitmap_block, buffer);
    if (ret != 0) {
        return false;
    }
    
    return !(buffer[bitmap_byte] & (1 << bitmap_bit));
}

int fs_mark_block_used(struct fs_mount* mount, uint64_t block_num) {
    if (!mount) {
        return -1;
    }
    
    if (block_num >= mount->superblock.block_count) {
        return -1;
    }
    
    if (block_num < mount->superblock.data_start) {
        return -1;
    }
    
    uint64_t data_block_num = block_num - mount->superblock.data_start;
    uint64_t bitmap_block = data_block_num / BLOCK_BITS;
    uint64_t bitmap_byte = (data_block_num % BLOCK_BITS) / 8;
    uint32_t bitmap_bit = (data_block_num % BLOCK_BITS) % 8;
    
    uint8_t buffer[BLOCK_SIZE];
    int ret = fs_read_block(mount, mount->superblock.block_bitmap_start + bitmap_block, buffer);
    if (ret != 0) {
        return ret;
    }
    
    if (buffer[bitmap_byte] & (1 << bitmap_bit)) {
        return -1;
    }
    
    buffer[bitmap_byte] |= (1 << bitmap_bit);
    
    ret = fs_write_block(mount, mount->superblock.block_bitmap_start + bitmap_block, buffer);
    if (ret != 0) {
        return ret;
    }
    
    mount->superblock.free_blocks--;
    
    return 0;
}

int fs_mark_block_free(struct fs_mount* mount, uint64_t block_num) {
    if (!mount) {
        return -1;
    }
    
    if (block_num >= mount->superblock.block_count) {
        return -1;
    }
    
    if (block_num < mount->superblock.data_start) {
        return -1;
    }
    
    uint64_t data_block_num = block_num - mount->superblock.data_start;
    uint64_t bitmap_block = data_block_num / BLOCK_BITS;
    uint64_t bitmap_byte = (data_block_num % BLOCK_BITS) / 8;
    uint32_t bitmap_bit = (data_block_num % BLOCK_BITS) % 8;
    
    uint8_t buffer[BLOCK_SIZE];
    int ret = fs_read_block(mount, mount->superblock.block_bitmap_start + bitmap_block, buffer);
    if (ret != 0) {
        return ret;
    }
    
    if (!(buffer[bitmap_byte] & (1 << bitmap_bit))) {
        return -1;
    }
    
    buffer[bitmap_byte] &= ~(1 << bitmap_bit);
    
    ret = fs_write_block(mount, mount->superblock.block_bitmap_start + bitmap_block, buffer);
    if (ret != 0) {
        return ret;
    }
    
    mount->superblock.free_blocks++;
    
    return 0;
}

int fs_alloc_block(struct fs_mount* mount, uint64_t* block_num) {
    if (!mount || !block_num) {
        return -1;
    }
    
    if (mount->superblock.free_blocks == 0) {
        return -1;
    }
    
    uint64_t total_data_blocks = mount->superblock.block_count - mount->superblock.data_start;
    uint64_t bitmap_blocks = (total_data_blocks + BLOCK_BITS - 1) / BLOCK_BITS;
    uint8_t buffer[BLOCK_SIZE];
    
    for (uint64_t i = 0; i < bitmap_blocks; i++) {
        uint64_t bnum = mount->superblock.block_bitmap_start + i;
        int ret = fs_read_block(mount, bnum, buffer);
        if (ret != 0) {
            return ret;
        }
        
        for (uint32_t j = 0; j < BLOCK_SIZE; j++) {
            if (buffer[j] != 0xFF) {
                for (uint32_t k = 0; k < 8; k++) {
                    if (!(buffer[j] & (1 << k))) {
                        *block_num = mount->superblock.data_start + i * BLOCK_BITS + j * 8 + k;
                        
                        buffer[j] |= (1 << k);
                        ret = fs_write_block(mount, bnum, buffer);
                        if (ret != 0) {
                            return ret;
                        }
                        
                        mount->superblock.free_blocks--;
                        
                        return 0;
                    }
                }
            }
        }
    }
    
    return -1;
}

int fs_free_block(struct fs_mount* mount, uint64_t block_num) {
    if (!mount) {
        return -1;
    }
    
    return fs_mark_block_free(mount, block_num);
}