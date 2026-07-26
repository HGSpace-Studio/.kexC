#pragma once

// 文件系统工具类头文件
// 封装挂载/卸载、目录操作、文件复制、tar 打包/解包等底层文件系统操作
// 所有方法均为静态方法, 无需实例化

#include <string>
#include <vector>
#include <cstdint>
#include <fstream>      // std::ofstream (用于 tar 打包)
#include <sys/types.h>  // mode_t

// 文件系统类型枚举
// 对应容器系统所支持的各类文件系统
enum class FilesystemType {
    Ext4,        // ext4: 标准日志文件系统
    Overlay,     // overlay: 联合文件系统 (OverlayFS), 用于容器 rootfs
    Tmpfs,       // tmpfs: 基于内存的临时文件系统
    Bind,        // bind: 绑定挂载, 将一个路径映射到另一个路径
    Squashfs     // squashfs: 只读压缩文件系统, 常用于镜像层存储
};

// 文件系统工具类
// 提供跨平台容器系统所需的底层文件系统操作
// 所有方法均为静态方法, 禁止实例化
class Filesystem {
public:
    // 文件系统类型枚举转字符串 (用于 mount() 的 fs_type 参数)
    // 返回: 文件系统类型名称 (如 "ext4", "overlay" 等)
    static const char* typeToString(FilesystemType type);

    // 挂载文件系统
    // source:  挂载源 (设备路径或目录路径, overlay 时为 "overlay")
    // target:  挂载点路径
    // fs_type: 文件系统类型
    // flags:   挂载标志 (MS_RDONLY / MS_BIND 等, 见 <sys/mount.h>)
    // data:    挂载选项字符串 (如 "lowerdir=...,upperdir=...,workdir=...")
    // 返回:    成功返回 true
    static bool mount(const std::string& source, const std::string& target,
                      FilesystemType fs_type, unsigned long flags,
                      const std::string& data);

    // 卸载文件系统
    // target: 挂载点路径
    // 返回:   成功返回 true (失败时尝试懒卸载)
    static bool umount(const std::string& target);

    // 递归创建目录 (类似 mkdir -p), 目录已存在视为成功
    // path: 目录路径
    // mode: 权限位 (如 0755)
    // 返回: 成功返回 true
    static bool makeDirectory(const std::string& path, mode_t mode);

    // 删除空目录 (仅删除目录本身, 不递归)
    // path: 目录路径
    // 返回: 成功返回 true
    static bool removeDirectory(const std::string& path);

    // 复制文件
    // src: 源文件路径
    // dst: 目标文件路径
    // 返回: 成功返回 true
    static bool copyFile(const std::string& src, const std::string& dst);

    // 判断路径是否存在
    static bool exists(const std::string& path);

    // 判断路径是否为目录
    static bool isDirectory(const std::string& path);

    // 获取文件大小 (字节)
    // 返回: 成功返回文件大小, 失败返回 -1
    static int64_t getFileSize(const std::string& path);

    // 列出目录下的所有条目名称 (不包含 "." 和 "..")
    static std::vector<std::string> listDirectory(const std::string& path);

    // 将源目录打包为 tar 文件
    // src: 源目录路径
    // dst: 目标 tar 文件路径
    // 返回: 成功返回 true
    static bool createTarball(const std::string& src, const std::string& dst);

    // 解压 tar 文件到目标目录
    // src: tar 文件路径
    // dst: 目标目录路径
    // 返回: 成功返回 true
    static bool extractTarball(const std::string& src, const std::string& dst);

private:
    Filesystem() = delete;  // 工具类, 禁止实例化

    // 递归打包目录内部实现
    // path: 当前遍历的目录全路径
    // base: 归档内相对路径前缀
    // out:  输出文件流
    static bool createTarballRecursive(const std::string& path,
                                       const std::string& base,
                                       std::ofstream& out);

    // 写入 tar 文件头 (512 字节)
    // name:      归档内文件名
    // size:      文件内容大小 (字节)
    // typeflag:  类型标志 ('0'=普通文件, '5'=目录)
    // mode:      权限位
    static void writeTarHeader(std::ofstream& out, const std::string& name,
                               int64_t size, char typeflag, mode_t mode);

    // 计算 tar 头校验和 (校验和字段视为 8 个空格时, 全部 512 字节的累加)
    static unsigned int calcChecksum(const char* header);

    // 从 tar 头字段解析八进制数值
    // data: 字段起始指针
    // len:  字段长度
    static int64_t parseOctal(const char* data, int len);
};
