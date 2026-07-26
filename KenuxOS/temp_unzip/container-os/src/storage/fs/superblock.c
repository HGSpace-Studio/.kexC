#include "fs.h"
#include <string.h>

int fs_read_superblock(struct fs_mount* mount) {
    if (!mount) {
        return -1;
    }

    uint8_t buffer[BLOCK_SIZE];
    int ret = fs_read_block(mount, 0, buffer);
    if (ret != 0) {
        return ret;
    }

    memcpy(&mount->superblock, buffer, sizeof(struct fs_superblock));
    return fs_validate_superblock(&mount->superblock);
}

int fs_write_superblock(struct fs_mount* mount) {
    if (!mount) {
        return -1;
    }
    
    mount->superblock.last_write_time = 0;
    
    return fs_write_block(mount, 0, (uint8_t*)&mount->superblock);
}

int fs_validate_superblock(struct fs_superblock* sb) {
    if (!sb) {
        return -1;
    }
    
    if (sb->magic != FS_MAGIC) {
        return -1;
    }
    
    if (sb->block_size != BLOCK_SIZE) {
        return -1;
    }
    
    if (sb->inode_size != INODE_SIZE) {
        return -1;
    }
    
    if (sb->block_count == 0) {
        return -1;
    }
    
    if (sb->block_bitmap_start >= sb->block_count ||
        sb->inode_bitmap_start >= sb->block_count ||
        sb->inode_table_start >= sb->block_count ||
        sb->data_start >= sb->block_count) {
        return -1;
    }
    
    return 0;
}

int fs_calculate_layout(struct fs_superblock* sb, uint64_t total_blocks) {
    if (!sb) {
        return -1;
    }
    
    memset(sb, 0, sizeof(*sb));
    
    sb->magic = FS_MAGIC;
    sb->version = 1;
    sb->block_count = total_blocks;
    sb->block_size = BLOCK_SIZE;
    sb->inode_size = INODE_SIZE;
    
    uint64_t journal_blocks = JOURNAL_BLOCKS;
    if (total_blocks < journal_blocks + 100) {
        journal_blocks = 0;
    }
    
    sb->journal_start = 1;
    
    uint64_t block_bitmap_blocks = (total_blocks + BLOCK_BITS - 1) / BLOCK_BITS;
    sb->block_bitmap_start = sb->journal_start + journal_blocks;
    
    uint64_t inode_count = total_blocks / 8;
    uint64_t inode_bitmap_blocks = (inode_count + BLOCK_BITS - 1) / BLOCK_BITS;
    sb->inode_bitmap_start = sb->block_bitmap_start + block_bitmap_blocks;
    
    uint64_t inode_table_blocks = (inode_count + INODES_PER_BLOCK - 1) / INODES_PER_BLOCK;
    sb->inode_table_start = sb->inode_bitmap_start + inode_bitmap_blocks;
    
    sb->data_start = sb->inode_table_start + inode_table_blocks;
    
    sb->free_blocks = total_blocks - sb->data_start;
    sb->free_inodes = inode_count;
    sb->inode_count = inode_count;
    
    sb->root_inode = 1;
    
    return 0;
}