#include "fs.h"
#include <string.h>

static struct fs_mount* mount_list[MAX_MOUNT_POINTS];
static uint32_t mount_count = 0;

int fs_read_block(struct fs_mount* mount, uint64_t block_num, uint8_t* buffer) {
    if (!mount || !buffer) {
        return -1;
    }
    
    if (!mount->driver || !mount->driver->read_block) {
        return -1;
    }
    
    uint64_t bytes_read = mount->driver->read_block(mount->driver->context, block_num, buffer);
    if (bytes_read != BLOCK_SIZE) {
        return -1;
    }
    
    return 0;
}

int fs_write_block(struct fs_mount* mount, uint64_t block_num, const uint8_t* buffer) {
    if (!mount || !buffer) {
        return -1;
    }
    
    if (!mount->driver || !mount->driver->write_block) {
        return -1;
    }
    
    uint64_t bytes_written = mount->driver->write_block(mount->driver->context, block_num, buffer);
    if (bytes_written != BLOCK_SIZE) {
        return -1;
    }
    
    return 0;
}

int fs_flush_cache(struct fs_mount* mount) {
    (void)mount;
    return 0;
}

int fs_invalidate_cache(struct fs_mount* mount, uint64_t block_num) {
    (void)mount;
    (void)block_num;
    return 0;
}

int fs_add_mount(struct fs_mount* mount) {
    if (!mount || mount_count >= MAX_MOUNT_POINTS) {
        return -1;
    }
    
    mount_list[mount_count++] = mount;
    
    return 0;
}

int fs_remove_mount(struct fs_mount* mount) {
    if (!mount) {
        return -1;
    }
    
    for (uint32_t i = 0; i < mount_count; i++) {
        if (mount_list[i] == mount) {
            for (uint32_t j = i; j < mount_count - 1; j++) {
                mount_list[j] = mount_list[j + 1];
            }
            mount_count--;
            return 0;
        }
    }
    
    return -1;
}

void fs_tick(void) {
    for (uint32_t m = 0; m < mount_count; m++) {
        struct fs_mount* mount = mount_list[m];
        
        for (uint32_t i = 0; i < CACHE_SIZE; i++) {
            if (mount->block_cache[i].valid && mount->block_cache[i].ref_count == 0) {
                mount->block_cache[i].last_access++;
            }
        }
        
        for (uint32_t i = 0; i < INODE_CACHE_SIZE; i++) {
            if (mount->inode_cache[i].valid && mount->inode_cache[i].ref_count == 0) {
                mount->inode_cache[i].last_access++;
            }
        }
    }
}
