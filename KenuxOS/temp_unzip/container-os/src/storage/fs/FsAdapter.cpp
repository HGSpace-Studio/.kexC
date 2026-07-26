#include "storage/fs/FsAdapter.h"

// 用 extern "C" 包含 C 头文件, 避免 C++ name mangling
extern "C" {
#include "fs.h"
#include "syscall.h"
}

#include <cstring>
#include <cstdlib>

// ==================== 构造与析构 ====================

FsAdapter::FsAdapter(uint64_t disk_blocks)
    : disk_blocks_(disk_blocks)
    , driver_(nullptr)
    , mount_(nullptr) {
    // 分配内存磁盘空间
    disk_.resize(disk_blocks * BLOCK_SIZE, 0);

    // 创建文件系统驱动
    driver_ = (fs_driver*)std::malloc(sizeof(fs_driver));
    driver_->context = this;
    driver_->read_block = memReadBlock;
    driver_->write_block = memWriteBlock;
}

FsAdapter::~FsAdapter() {
    if (mount_) {
        unmount();
    }
    if (driver_) {
        std::free(driver_);
        driver_ = nullptr;
    }
}

// ==================== 内存磁盘驱动回调 ====================

uint64_t FsAdapter::memReadBlock(void* ctx, uint64_t block_num, uint8_t* buffer) {
    FsAdapter* self = static_cast<FsAdapter*>(ctx);
    if (!self || block_num >= self->disk_blocks_) {
        return 0;
    }
    std::memcpy(buffer,
                self->disk_.data() + block_num * BLOCK_SIZE,
                BLOCK_SIZE);
    return BLOCK_SIZE;
}

uint64_t FsAdapter::memWriteBlock(void* ctx, uint64_t block_num, const uint8_t* buffer) {
    FsAdapter* self = static_cast<FsAdapter*>(ctx);
    if (!self || block_num >= self->disk_blocks_) {
        return 0;
    }
    std::memcpy(self->disk_.data() + block_num * BLOCK_SIZE,
                buffer,
                BLOCK_SIZE);
    return BLOCK_SIZE;
}

// ==================== 生命周期管理 ====================

bool FsAdapter::format(const std::string& volume_name) {
    int ret = fs_format(driver_, disk_blocks_, volume_name.c_str());
    return ret == 0;
}

bool FsAdapter::mount(const std::string& device_name, const std::string& mount_name) {
    int ret = fs_mount(device_name.c_str(),
                       mount_name.c_str(),
                       driver_,
                       &mount_);
    if (ret == 0) {
        activate();
    }
    return ret == 0;
}

bool FsAdapter::unmount() {
    if (!mount_) return true;
    int ret = fs_unmount(mount_);
    if (ret == 0) {
        mount_ = nullptr;
    }
    return ret == 0;
}

void FsAdapter::activate() {
    if (mount_) {
        sys_set_mount(mount_);
    }
}

// ==================== 文件操作 ====================

int FsAdapter::open(const std::string& path, uint32_t mode) {
    uint32_t fd = 0;
    int ret = fs_open(mount_, path.c_str(), mode, &fd);
    if (ret != 0) return -1;
    return static_cast<int>(fd);
}

int FsAdapter::close(int fd) {
    return fs_close(static_cast<uint32_t>(fd));
}

int64_t FsAdapter::read(int fd, void* buf, uint64_t count) {
    uint64_t bytes_read = 0;
    int ret = fs_read(static_cast<uint32_t>(fd), buf, count, &bytes_read);
    if (ret != 0) return -1;
    return static_cast<int64_t>(bytes_read);
}

int64_t FsAdapter::write(int fd, const void* buf, uint64_t count) {
    uint64_t bytes_written = 0;
    int ret = fs_write(static_cast<uint32_t>(fd), buf, count, &bytes_written);
    if (ret != 0) return -1;
    return static_cast<int64_t>(bytes_written);
}

int64_t FsAdapter::lseek(int fd, int64_t offset, int whence) {
    int ret = fs_seek(static_cast<uint32_t>(fd), offset, whence);
    if (ret != 0) return -1;
    return 0;
}

int FsAdapter::create(const std::string& path, uint32_t type) {
    uint64_t inode_num = 0;
    int ret = fs_create(mount_, path.c_str(), type, &inode_num);
    if (ret != 0) return -1;
    return static_cast<int>(inode_num);
}

int FsAdapter::mkdir(const std::string& path) {
    return fs_mkdir(mount_, path.c_str());
}

int FsAdapter::remove(const std::string& path) {
    return fs_remove(mount_, path.c_str());
}

int FsAdapter::truncate(int fd, uint64_t size) {
    return fs_truncate(static_cast<uint32_t>(fd), size);
}

int FsAdapter::sync(int fd) {
    return fs_fsync(static_cast<uint32_t>(fd));
}

// ==================== 查询操作 ====================

int FsAdapter::stat(int fd, FileInfo* info) {
    struct fs_stat fs_info;
    int ret = fs_stat(static_cast<uint32_t>(fd), &fs_info);
    if (ret != 0) return -1;

    info->size = fs_info.size;
    info->type = fs_info.type;
    info->permissions = fs_info.permissions;
    info->link_count = fs_info.link_count;
    return 0;
}

int FsAdapter::listDirectory(const std::string& path,
                              std::vector<DirEntry>* entries) {
    uint64_t inode_num = 0;
    int ret = fs_resolve_path(mount_, path.c_str(), &inode_num);
    if (ret != 0) return -1;

    // 每次读取一块目录条目
    fs_dir_entry buf[64];
    uint32_t count = 0;
    ret = fs_read_directory(mount_, inode_num, buf, 64, &count);
    if (ret != 0) return -1;

    entries->clear();
    entries->reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        DirEntry e;
        e.inode = buf[i].inode;
        e.name = buf[i].name;
        e.type = buf[i].type;
        entries->push_back(std::move(e));
    }
    return 0;
}

uint64_t FsAdapter::getTotalBlocks() const {
    return mount_ ? mount_->superblock.block_count : disk_blocks_;
}

uint64_t FsAdapter::getFreeBlocks() const {
    return mount_ ? mount_->superblock.free_blocks : 0;
}
