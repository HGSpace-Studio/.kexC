#include "fs.h"
#include <string.h>

struct journal_transaction {
    uint64_t id;
    uint32_t block_count;
    uint64_t blocks[64];
    uint8_t data[64][BLOCK_SIZE];
    bool committed;
};

static struct journal_transaction current_tx;
static uint64_t next_tx_id = 1;
static bool journal_enabled = false;

int fs_journal_init(struct fs_mount* mount) {
    if (!mount) {
        return -1;
    }
    
    if (mount->superblock.journal_start == 0) {
        journal_enabled = false;
        return 0;
    }
    
    uint8_t buffer[BLOCK_SIZE];
    int ret = fs_read_block(mount, mount->superblock.journal_start, buffer);
    if (ret != 0) {
        return ret;
    }
    
    struct fs_journal_header* header = (struct fs_journal_header*)buffer;
    
    if (header->magic != JOURNAL_MAGIC) {
        memset(header, 0, sizeof(*header));
        header->magic = JOURNAL_MAGIC;
        header->version = 1;
        header->sequence = 0;
        header->start = mount->superblock.journal_start + 1;
        header->end = mount->superblock.journal_start + JOURNAL_BLOCKS;
        header->commit_count = 0;
        
        ret = fs_write_block(mount, mount->superblock.journal_start, buffer);
        if (ret != 0) {
            return ret;
        }
        
        journal_enabled = true;
        return 0;
    }
    
    journal_enabled = true;
    
    return fs_journal_recover(mount);
}

int fs_journal_begin(struct fs_mount* mount, uint64_t* transaction_id) {
    if (!mount || !transaction_id || !journal_enabled) {
        *transaction_id = 0;
        return 0;
    }
    
    if (current_tx.id != 0 && !current_tx.committed) {
        return -1;
    }
    
    memset(&current_tx, 0, sizeof(current_tx));
    current_tx.id = next_tx_id++;
    *transaction_id = current_tx.id;
    
    return 0;
}

int fs_journal_write_block(struct fs_mount* mount, uint64_t transaction_id, uint64_t block_num, const uint8_t* data) {
    if (!mount || !data || !journal_enabled) {
        return 0;
    }
    
    if (current_tx.id != transaction_id) {
        return -1;
    }
    
    if (current_tx.block_count >= 64) {
        return -1;
    }
    
    current_tx.blocks[current_tx.block_count] = block_num;
    memcpy(current_tx.data[current_tx.block_count], data, BLOCK_SIZE);
    current_tx.block_count++;
    
    return 0;
}

int fs_journal_commit(struct fs_mount* mount, uint64_t transaction_id) {
    if (!mount || !journal_enabled) {
        return 0;
    }
    
    if (current_tx.id != transaction_id) {
        return -1;
    }
    
    if (current_tx.block_count == 0) {
        current_tx.committed = true;
        return 0;
    }
    
    uint8_t buffer[BLOCK_SIZE];
    int ret = fs_read_block(mount, mount->superblock.journal_start, buffer);
    if (ret != 0) {
        return ret;
    }
    
    struct fs_journal_header* header = (struct fs_journal_header*)buffer;
    
    uint64_t journal_pos = header->start + (header->sequence % (header->end - header->start));
    
    struct fs_journal_entry entry;
    memset(&entry, 0, sizeof(entry));
    entry.sequence = header->sequence;
    entry.type = 1;
    entry.block_count = current_tx.block_count;
    entry.transaction_id = current_tx.id;
    memcpy(entry.blocks, current_tx.blocks, sizeof(current_tx.blocks));
    
    memcpy(buffer, &entry, sizeof(entry));
    ret = fs_write_block(mount, journal_pos, buffer);
    if (ret != 0) {
        return ret;
    }
    
    journal_pos++;
    if (journal_pos >= header->end) {
        journal_pos = header->start;
    }
    
    for (uint32_t i = 0; i < current_tx.block_count; i++) {
        ret = fs_write_block(mount, journal_pos, current_tx.data[i]);
        if (ret != 0) {
            return ret;
        }
        
        journal_pos++;
        if (journal_pos >= header->end) {
            journal_pos = header->start;
        }
    }
    
    header->sequence++;
    header->commit_count++;
    header->start = journal_pos;
    
    memcpy(buffer, header, sizeof(*header));
    ret = fs_write_block(mount, mount->superblock.journal_start, buffer);
    if (ret != 0) {
        return ret;
    }
    
    for (uint32_t i = 0; i < current_tx.block_count; i++) {
        ret = fs_write_block(mount, current_tx.blocks[i], current_tx.data[i]);
        if (ret != 0) {
            return ret;
        }
    }
    
    current_tx.committed = true;
    
    return 0;
}

int fs_journal_recover(struct fs_mount* mount) {
    if (!mount || !journal_enabled) {
        return 0;
    }
    
    uint8_t buffer[BLOCK_SIZE];
    int ret = fs_read_block(mount, mount->superblock.journal_start, buffer);
    if (ret != 0) {
        return ret;
    }
    
    struct fs_journal_header* header = (struct fs_journal_header*)buffer;
    
    if (header->sequence == 0) {
        return 0;
    }
    
    uint64_t last_seq = header->sequence - 1;
    uint64_t journal_pos = header->start + (last_seq % (header->end - header->start));
    
    ret = fs_read_block(mount, journal_pos, buffer);
    if (ret != 0) {
        return ret;
    }
    
    struct fs_journal_entry* entry = (struct fs_journal_entry*)buffer;
    
    if (entry->sequence != last_seq) {
        return 0;
    }
    
    journal_pos++;
    if (journal_pos >= header->end) {
        journal_pos = header->start;
    }
    
    for (uint32_t i = 0; i < entry->block_count; i++) {
        ret = fs_read_block(mount, journal_pos, buffer);
        if (ret != 0) {
            return ret;
        }
        
        ret = fs_write_block(mount, entry->blocks[i], buffer);
        if (ret != 0) {
            return ret;
        }
        
        journal_pos++;
        if (journal_pos >= header->end) {
            journal_pos = header->start;
        }
    }
    
    header->start = journal_pos;
    
    memcpy(buffer, header, sizeof(*header));
    return fs_write_block(mount, mount->superblock.journal_start, buffer);
}

int fs_journal_shutdown(struct fs_mount* mount) {
    if (!mount || !journal_enabled) {
        return 0;
    }
    
    if (current_tx.id != 0 && !current_tx.committed) {
        fs_journal_commit(mount, current_tx.id);
    }
    
    uint8_t buffer[BLOCK_SIZE];
    int ret = fs_read_block(mount, mount->superblock.journal_start, buffer);
    if (ret != 0) {
        return ret;
    }
    
    struct fs_journal_header* header = (struct fs_journal_header*)buffer;
    header->sequence = 0;
    
    return fs_write_block(mount, mount->superblock.journal_start, buffer);
}