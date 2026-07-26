#pragma once

// FsAdapter: C++ 封装层, 将 C 实现的裸金属文件系统接入容器架构
// 提供内存磁盘驱动 + 文件系统生命周期管理 + POSIX syscall 桥接
// 每个容器实例持有一个 FsAdapter, 实现文件系统隔离

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <memory>

// 前向声明, 避免 C 头文件的宏污染
struct fs_mount;
struct fs_driver;

// 容器文件系统适配器
class FsAdapter {
public:
    // 构造: 指定磁盘大小 (块数, 每块 4KB)
    explicit FsAdapter(uint64_t disk_blocks = 4096);
    ~FsAdapter();

    FsAdapter(const FsAdapter&) = delete;
    FsAdapter& operator=(const FsAdapter&) = delete;

    // ---- 生命周期管理 ----

    // 格式化磁盘
    bool format(const std::string& volume_name = "ContainerFS");

    // 挂载文件系统, 设为当前活跃实例 (供 sys_* 调用使用)
    bool mount(const std::string& device_name = "memdisk",
               const std::string& mount_name = "root");

    // 卸载文件系统
    bool unmount();

    // 设为当前活跃 mount (后续 sys_* 调用将操作此 mount)
    void activate();

    // ---- 文件操作 (直接转发到 fs_ API, 不经过全局 sys_) ----

    // 打开文件, 返回 fd (>=0 成功, <0 失败)
    int open(const std::string& path, uint32_t mode);

    // 关闭文件
    int close(int fd);

    // 读文件, 返回实际读取字节数, -1 失败
    int64_t read(int fd, void* buf, uint64_t count);

    // 写文件, 返回实际写入字节数, -1 失败
    int64_t write(int fd, const void* buf, uint64_t count);

    // 移动文件偏移, 返回新偏移, -1 失败
    int64_t lseek(int fd, int64_t offset, int whence);

    // 创建文件或目录
    int create(const std::string& path, uint32_t type);

    // 创建目录
    int mkdir(const std::string& path);

    // 删除文件或空目录
    int remove(const std::string& path);

    // 截断文件
    int truncate(int fd, uint64_t size);

    // 同步到磁盘
    int sync(int fd);

    // ---- 查询操作 ----

    // 文件信息
    struct FileInfo {
        uint64_t size;
        uint32_t type;
        uint32_t permissions;
        uint32_t link_count;
    };

    int stat(int fd, FileInfo* info);

    // 列出目录条目
    struct DirEntry {
        uint64_t inode;
        std::string name;
        uint16_t type;
    };

    int listDirectory(const std::string& path, std::vector<DirEntry>* entries);

    // 获取磁盘使用情况
    uint64_t getTotalBlocks() const;
    uint64_t getFreeBlocks() const;

    // 获取底层 mount 指针 (供 syscall 层使用)
    fs_mount* getMount() { return mount_; }

private:
    // 内存磁盘数据
    std::vector<uint8_t> disk_;
    uint64_t disk_blocks_;

    // 文件系统驱动
    fs_driver* driver_;

    // 挂载实例
    fs_mount* mount_;

    // 静态回调: 内存磁盘读块
    static uint64_t memReadBlock(void* ctx, uint64_t block_num, uint8_t* buffer);
    static uint64_t memWriteBlock(void* ctx, uint64_t block_num, const uint8_t* buffer);
};
