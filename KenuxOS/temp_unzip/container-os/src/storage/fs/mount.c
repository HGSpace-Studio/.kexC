#include "fs.h"
#include <string.h>
#include <stdlib.h>

static struct fs_mount* mounts[MAX_MOUNT_POINTS];
static uint32_t mount_count = 0;

int fs_mount(const char* device, const char* name, struct fs_driver* driver, struct fs_mount** mount) {
    if (!device || !name || !driver || !mount) {
        return -1;
    }
    
    if (mount_count >= MAX_MOUNT_POINTS) {
        return -1;
    }
    
    for (uint32_t i = 0; i < mount_count; i++) {
        if (strcmp(mounts[i]->name, name) == 0) {
            return -1;
        }
    }
    
    *mount = (struct fs_mount*)malloc(sizeof(struct fs_mount));
    if (!*mount) {
        return -1;
    }
    
    memset(*mount, 0, sizeof(struct fs_mount));
    
    strncpy((*mount)->name, name, sizeof((*mount)->name) - 1);
    strncpy((*mount)->device, device, sizeof((*mount)->device) - 1);
    (*mount)->driver = driver;
    
    uint8_t buffer[BLOCK_SIZE];
    uint64_t bytes_read = driver->read_block(driver->context, 0, buffer);
    if (bytes_read != BLOCK_SIZE) {
        free(*mount);
        return -1;
    }
    
    memcpy(&(*mount)->superblock, buffer, sizeof(struct fs_superblock));
    
    int ret = fs_validate_superblock(&(*mount)->superblock);
    if (ret != 0) {
        free(*mount);
        return ret;
    }
    
    (*mount)->root_inode = (*mount)->superblock.root_inode;
    (*mount)->mounted = true;
    
    mounts[mount_count++] = *mount;
    fs_add_mount(*mount);
    
    return fs_journal_init(*mount);
}

int fs_unmount(struct fs_mount* mount) {
    if (!mount || !mount->mounted) {
        return -1;
    }
    
    fs_flush_cache(mount);
    fs_write_superblock(mount);
    fs_journal_shutdown(mount);
    
    mount->mounted = false;
    
    fs_remove_mount(mount);
    
    for (uint32_t i = 0; i < mount_count; i++) {
        if (mounts[i] == mount) {
            for (uint32_t j = i; j < mount_count - 1; j++) {
                mounts[j] = mounts[j + 1];
            }
            mount_count--;
            break;
        }
    }
    
    free(mount);
    
    return 0;
}

int fs_format(struct fs_driver* driver, uint64_t block_count, const char* volume_name) {
    if (!driver || block_count < 100) {
        return -1;
    }
    
    struct fs_superblock sb;
    int ret = fs_calculate_layout(&sb, block_count);
    if (ret != 0) {
        return ret;
    }
    
    if (volume_name) {
        strncpy(sb.volume_name, volume_name, sizeof(sb.volume_name) - 1);
    }
    
    sb.created_time = 0;
    sb.last_mount_time = 0;
    sb.last_write_time = 0;
    sb.state = 1;
    sb.features = 0;
    
    uint8_t buffer[BLOCK_SIZE];
    memset(buffer, 0, BLOCK_SIZE);
    memcpy(buffer, &sb, sizeof(sb));
    
    uint64_t bytes_written = driver->write_block(driver->context, 0, buffer);
    if (bytes_written != BLOCK_SIZE) {
        return -1;
    }
    
    uint64_t journal_blocks = JOURNAL_BLOCKS;
    if (block_count < journal_blocks + 100) {
        journal_blocks = 0;
    }
    
    for (uint64_t i = 1; i < sb.block_bitmap_start; i++) {
        memset(buffer, 0, BLOCK_SIZE);
        
        if (i >= sb.journal_start && i < sb.journal_start + journal_blocks && i == sb.journal_start) {
            struct fs_journal_header* header = (struct fs_journal_header*)buffer;
            header->magic = JOURNAL_MAGIC;
            header->version = 1;
            header->sequence = 0;
            header->start = sb.journal_start + 1;
            header->end = sb.journal_start + journal_blocks;
            header->commit_count = 0;
        }
        
        bytes_written = driver->write_block(driver->context, i, buffer);
        if (bytes_written != BLOCK_SIZE) {
            return -1;
        }
    }
    
    uint64_t block_bitmap_blocks = (block_count + BLOCK_BITS - 1) / BLOCK_BITS;
    uint64_t total_data_blocks = block_count - sb.data_start;
    for (uint64_t i = 0; i < block_bitmap_blocks; i++) {
        memset(buffer, 0, BLOCK_SIZE);
        
        /* 块位图使用数据块号（相对于 data_start），位 N 对应数据块 N */
        /* 所有数据块默认空闲（0），只需标记超出范围的位为已使用 */
        uint64_t last_bit = i * BLOCK_BITS + BLOCK_BITS - 1;
        if (last_bit >= total_data_blocks) {
            uint64_t bits_to_set = last_bit - total_data_blocks + 1;
            for (uint64_t b = 0; b < bits_to_set; b++) {
                uint64_t bit_pos = BLOCK_BITS - 1 - b;
                buffer[bit_pos / 8] |= (1 << (bit_pos % 8));
            }
        }
        
        bytes_written = driver->write_block(driver->context, sb.block_bitmap_start + i, buffer);
        if (bytes_written != BLOCK_SIZE) {
            return -1;
        }
    }
    
    uint64_t inode_bitmap_blocks = (sb.inode_count + BLOCK_BITS - 1) / BLOCK_BITS;
    for (uint64_t i = 0; i < inode_bitmap_blocks; i++) {
        memset(buffer, 0, BLOCK_SIZE);
        
        uint64_t last_bit = i * BLOCK_BITS + BLOCK_BITS - 1;
        if (last_bit >= sb.inode_count) {
            uint64_t bits_to_set = (last_bit - sb.inode_count + 1) % 8;
            if (bits_to_set > 0) {
                buffer[BLOCK_SIZE - 1] = (0xFF << bits_to_set);
            }
        }
        
        bytes_written = driver->write_block(driver->context, sb.inode_bitmap_start + i, buffer);
        if (bytes_written != BLOCK_SIZE) {
            return -1;
        }
    }
    
    for (uint64_t i = 0; i < (sb.inode_count + INODES_PER_BLOCK - 1) / INODES_PER_BLOCK; i++) {
        memset(buffer, 0, BLOCK_SIZE);
        bytes_written = driver->write_block(driver->context, sb.inode_table_start + i, buffer);
        if (bytes_written != BLOCK_SIZE) {
            return -1;
        }
    }
    
    uint64_t root_block = sb.data_start;
    
    uint64_t data_block_num = root_block - sb.data_start;
    uint64_t bitmap_block = data_block_num / BLOCK_BITS;
    uint64_t bitmap_byte = (data_block_num % BLOCK_BITS) / 8;
    uint32_t bitmap_bit = (data_block_num % BLOCK_BITS) % 8;
    
    ret = driver->read_block(driver->context, sb.block_bitmap_start + bitmap_block, buffer);
    if (ret != BLOCK_SIZE) {
        return -1;
    }
    
    buffer[bitmap_byte] |= (1 << bitmap_bit);
    
    bytes_written = driver->write_block(driver->context, sb.block_bitmap_start + bitmap_block, buffer);
    if (bytes_written != BLOCK_SIZE) {
        return -1;
    }
    
    sb.free_blocks--;
    
    memset(buffer, 0, BLOCK_SIZE);
    ((struct fs_dir_entry*)buffer)->inode = 1;
    ((struct fs_dir_entry*)buffer)->name_length = 1;
    ((struct fs_dir_entry*)buffer)->type = FS_TYPE_DIRECTORY;
    ((struct fs_dir_entry*)buffer)->name[0] = '.';
    
    ((struct fs_dir_entry*)(buffer + sizeof(struct fs_dir_entry)))->inode = 1;
    ((struct fs_dir_entry*)(buffer + sizeof(struct fs_dir_entry)))->name_length = 2;
    ((struct fs_dir_entry*)(buffer + sizeof(struct fs_dir_entry)))->type = FS_TYPE_DIRECTORY;
    ((struct fs_dir_entry*)(buffer + sizeof(struct fs_dir_entry)))->name[0] = '.';
    ((struct fs_dir_entry*)(buffer + sizeof(struct fs_dir_entry)))->name[1] = '.';
    
    bytes_written = driver->write_block(driver->context, root_block, buffer);
    if (bytes_written != BLOCK_SIZE) {
        return -1;
    }
    
    struct fs_inode root_inode;
    memset(&root_inode, 0, sizeof(root_inode));
    root_inode.magic = FS_MAGIC;
    root_inode.type = FS_TYPE_DIRECTORY;
    root_inode.permissions = FS_PERM_READ | FS_PERM_WRITE | FS_PERM_EXEC;
    root_inode.uid = 0;
    root_inode.gid = 0;
    root_inode.size = BLOCK_SIZE;
    root_inode.atime = 0;
    root_inode.mtime = 0;
    root_inode.ctime = 0;
    root_inode.link_count = 2;
    root_inode.generation = 0;
    root_inode.extents[0].start_block = root_block;
    root_inode.extents[0].length = 1;
    
    uint64_t root_inode_block = sb.inode_table_start + get_inode_block(1);
    uint64_t root_inode_offset = get_inode_offset(1);
    
    ret = driver->read_block(driver->context, root_inode_block, buffer);
    if (ret != BLOCK_SIZE) {
        return -1;
    }
    
    memcpy(buffer + root_inode_offset, &root_inode, INODE_SIZE);
    
    bytes_written = driver->write_block(driver->context, root_inode_block, buffer);
    if (bytes_written != BLOCK_SIZE) {
        return -1;
    }
    
    uint64_t inode_bitmap_byte = 0;
    uint32_t inode_bitmap_bit = 0;
    
    ret = driver->read_block(driver->context, sb.inode_bitmap_start + inode_bitmap_byte / BLOCK_SIZE, buffer);
    if (ret != BLOCK_SIZE) {
        return -1;
    }
    
    buffer[inode_bitmap_byte % BLOCK_SIZE] |= (1 << inode_bitmap_bit);
    
    bytes_written = driver->write_block(driver->context, sb.inode_bitmap_start + inode_bitmap_byte / BLOCK_SIZE, buffer);
    if (bytes_written != BLOCK_SIZE) {
        return -1;
    }
    
    memset(buffer, 0, BLOCK_SIZE);
    memcpy(buffer, &sb, sizeof(sb));
    bytes_written = driver->write_block(driver->context, 0, buffer);
    if (bytes_written != BLOCK_SIZE) {
        return -1;
    }
    
    return 0;
}