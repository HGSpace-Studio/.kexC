#ifndef FS_H
#define FS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define FS_MAGIC 0x54524145

#define BLOCK_SIZE 4096
#define BLOCK_BITS (BLOCK_SIZE * 8)

#define INODE_SIZE 256
#define INODES_PER_BLOCK (BLOCK_SIZE / INODE_SIZE)

#define DIRECT_EXTENTS 8
#define INDIRECT_EXTENTS 1
#define DOUBLE_INDIRECT_EXTENTS 1
#define TOTAL_EXTENTS (DIRECT_EXTENTS + INDIRECT_EXTENTS + DOUBLE_INDIRECT_EXTENTS)

#define MAX_FILENAME 255
#define DIR_ENTRIES_PER_BLOCK (BLOCK_SIZE / sizeof(struct fs_dir_entry))

#define MAX_OPEN_FILES 1024
#define MAX_MOUNT_POINTS 16

#define CACHE_SIZE 1024
#define INODE_CACHE_SIZE 128

#define JOURNAL_MAGIC 0x4A4E5452
#define JOURNAL_BLOCKS 1024

#define FS_PERM_READ 0x01
#define FS_PERM_WRITE 0x02
#define FS_PERM_EXEC 0x04

#define FS_TYPE_REGULAR 0x01
#define FS_TYPE_DIRECTORY 0x02
#define FS_TYPE_SYMLINK 0x04
#define FS_TYPE_CHAR_DEVICE 0x08
#define FS_TYPE_BLOCK_DEVICE 0x10

#ifndef SEEK_SET
#define SEEK_SET 0
#endif
#ifndef SEEK_CUR
#define SEEK_CUR 1
#endif
#ifndef SEEK_END
#define SEEK_END 2
#endif

struct fs_extent {
    uint64_t start_block;
    uint64_t length;
};

struct fs_inode {
    uint32_t magic;
    uint32_t type;
    uint32_t permissions;
    uint32_t uid;
    uint32_t gid;
    uint64_t size;
    uint64_t atime;
    uint64_t mtime;
    uint64_t ctime;
    uint32_t link_count;
    uint32_t generation;
    struct fs_extent extents[TOTAL_EXTENTS];
    uint64_t indirect_block;
    uint64_t double_indirect_block;
    uint32_t padding[8];
};

struct fs_superblock {
    uint32_t magic;
    uint32_t version;
    uint64_t block_count;
    uint64_t inode_count;
    uint64_t free_blocks;
    uint64_t free_inodes;
    uint64_t block_bitmap_start;
    uint64_t inode_bitmap_start;
    uint64_t inode_table_start;
    uint64_t data_start;
    uint64_t journal_start;
    uint64_t root_inode;
    uint64_t volume_id;
    uint64_t created_time;
    uint64_t last_mount_time;
    uint64_t last_write_time;
    uint32_t block_size;
    uint32_t inode_size;
    uint32_t features;
    uint32_t state;
    char volume_name[64];
};

struct fs_dir_entry {
    uint64_t inode;
    uint16_t name_length;
    uint16_t type;
    char name[MAX_FILENAME + 1];
};

struct fs_journal_header {
    uint32_t magic;
    uint32_t version;
    uint64_t sequence;
    uint64_t start;
    uint64_t end;
    uint64_t commit_count;
};

struct fs_journal_entry {
    uint64_t sequence;
    uint32_t type;
    uint32_t block_count;
    uint64_t blocks[64];
    uint64_t transaction_id;
};

struct fs_block_cache {
    uint64_t block_num;
    bool dirty;
    bool valid;
    uint32_t ref_count;
    uint64_t last_access;
    uint8_t data[BLOCK_SIZE];
};

struct fs_inode_cache {
    uint64_t inode_num;
    bool dirty;
    bool valid;
    uint32_t ref_count;
    uint64_t last_access;
    struct fs_inode inode;
};

struct fs_file {
    uint64_t inode_num;
    uint64_t offset;
    uint32_t mode;
    bool is_directory;
    uint32_t dir_pos;
};

struct fs_mount {
    char name[32];
    char device[64];
    uint64_t root_inode;
    struct fs_superblock superblock;
    struct fs_driver* driver;
    struct fs_block_cache block_cache[CACHE_SIZE];
    struct fs_inode_cache inode_cache[INODE_CACHE_SIZE];
    uint32_t cache_hits;
    uint32_t cache_misses;
    bool mounted;
    bool read_only;
};

typedef uint64_t (*fs_read_block_fn)(void* ctx, uint64_t block_num, uint8_t* buffer);
typedef uint64_t (*fs_write_block_fn)(void* ctx, uint64_t block_num, const uint8_t* buffer);

struct fs_driver {
    void* context;
    fs_read_block_fn read_block;
    fs_write_block_fn write_block;
};

struct fs_stat {
    uint64_t size;
    uint32_t type;
    uint32_t permissions;
    uint64_t atime;
    uint64_t mtime;
    uint64_t ctime;
    uint32_t link_count;
};

static inline uint64_t get_inode_block(uint64_t inode_num) {
    return (inode_num - 1) / INODES_PER_BLOCK;
}

static inline uint64_t get_inode_offset(uint64_t inode_num) {
    return ((inode_num - 1) % INODES_PER_BLOCK) * INODE_SIZE;
}

// Superblock operations
int fs_read_superblock(struct fs_mount* mount);
int fs_write_superblock(struct fs_mount* mount);
int fs_validate_superblock(struct fs_superblock* sb);
int fs_calculate_layout(struct fs_superblock* sb, uint64_t total_blocks);

// Block allocator
int fs_alloc_block(struct fs_mount* mount, uint64_t* block_num);
int fs_free_block(struct fs_mount* mount, uint64_t block_num);
int fs_mark_block_used(struct fs_mount* mount, uint64_t block_num);
int fs_mark_block_free(struct fs_mount* mount, uint64_t block_num);
bool fs_is_block_free(struct fs_mount* mount, uint64_t block_num);

// Inode operations
int fs_alloc_inode(struct fs_mount* mount, uint32_t type, uint64_t* inode_num);
int fs_free_inode(struct fs_mount* mount, uint64_t inode_num);
int fs_read_inode(struct fs_mount* mount, uint64_t inode_num, struct fs_inode* inode);
int fs_write_inode(struct fs_mount* mount, uint64_t inode_num, const struct fs_inode* inode);
int fs_get_inode(struct fs_mount* mount, uint64_t inode_num, struct fs_inode_cache** cache);
int fs_put_inode(struct fs_mount* mount, uint64_t inode_num);
int fs_sync_inode(struct fs_mount* mount, uint64_t inode_num);

// Directory operations
int fs_lookup(struct fs_mount* mount, uint64_t dir_inode, const char* name, uint64_t* inode_num);
int fs_create_entry(struct fs_mount* mount, uint64_t dir_inode, const char* name, uint64_t inode_num, uint32_t type);
int fs_remove_entry(struct fs_mount* mount, uint64_t dir_inode, const char* name);
int fs_read_directory(struct fs_mount* mount, uint64_t dir_inode, struct fs_dir_entry* entries, uint32_t max_entries, uint32_t* count);
int fs_find_empty_dir_slot(struct fs_mount* mount, uint64_t dir_inode, uint64_t* block_num, uint32_t* entry_idx);

// File operations
int fs_open(struct fs_mount* mount, const char* path, uint32_t mode, uint32_t* fd);
int fs_close(uint32_t fd);
int fs_read(uint32_t fd, void* buffer, uint64_t size, uint64_t* bytes_read);
int fs_write(uint32_t fd, const void* buffer, uint64_t size, uint64_t* bytes_written);
int fs_seek(uint32_t fd, int64_t offset, int whence);
int fs_truncate(uint32_t fd, uint64_t size);
int fs_stat(uint32_t fd, struct fs_stat* stat);
int fs_fsync(uint32_t fd);

// Path operations
int fs_resolve_path(struct fs_mount* mount, const char* path, uint64_t* inode_num);
int fs_create(struct fs_mount* mount, const char* path, uint32_t type, uint64_t* inode_num);
int fs_remove(struct fs_mount* mount, const char* path);
int fs_mkdir(struct fs_mount* mount, const char* path);

// Cache operations
int fs_read_block(struct fs_mount* mount, uint64_t block_num, uint8_t* buffer);
int fs_write_block(struct fs_mount* mount, uint64_t block_num, const uint8_t* buffer);
int fs_flush_cache(struct fs_mount* mount);
int fs_invalidate_cache(struct fs_mount* mount, uint64_t block_num);
int fs_add_mount(struct fs_mount* mount);
int fs_remove_mount(struct fs_mount* mount);
void fs_tick(void);

// Journal operations
int fs_journal_init(struct fs_mount* mount);
int fs_journal_begin(struct fs_mount* mount, uint64_t* transaction_id);
int fs_journal_write_block(struct fs_mount* mount, uint64_t transaction_id, uint64_t block_num, const uint8_t* data);
int fs_journal_commit(struct fs_mount* mount, uint64_t transaction_id);
int fs_journal_recover(struct fs_mount* mount);
int fs_journal_shutdown(struct fs_mount* mount);

// Mount operations
int fs_mount(const char* device, const char* name, struct fs_driver* driver, struct fs_mount** mount);
int fs_unmount(struct fs_mount* mount);
int fs_format(struct fs_driver* driver, uint64_t block_count, const char* volume_name);

// Extent operations
int fs_extent_alloc(struct fs_mount* mount, struct fs_inode* inode, uint64_t start_offset, uint64_t length);
int fs_extent_free(struct fs_mount* mount, struct fs_inode* inode);
int fs_extent_find(struct fs_mount* mount, struct fs_inode* inode, uint64_t offset, uint64_t* block_num, uint64_t* offset_in_block);

#endif