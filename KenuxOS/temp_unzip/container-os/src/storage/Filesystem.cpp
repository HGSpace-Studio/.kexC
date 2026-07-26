#include "storage/Filesystem.h"

#include <sys/mount.h>   // mount(), umount(), umount2(), MS_*, MNT_DETACH
#include <sys/stat.h>    // stat(), mkdir(), S_ISDIR
#include <dirent.h>      // opendir(), readdir(), closedir()
#include <unistd.h>      // rmdir(), unlink()
#include <cerrno>
#include <cstring>       // memset(), strncpy(), memcpy()
#include <cstdio>        // snprintf()
#include <ctime>         // time()
#include <fstream>
#include <algorithm>
#include <iterator>      // istreambuf_iterator

// 文件系统类型枚举转字符串
const char* Filesystem::typeToString(FilesystemType type) {
    switch (type) {
        case FilesystemType::Ext4:     return "ext4";
        case FilesystemType::Overlay:  return "overlay";
        case FilesystemType::Tmpfs:    return "tmpfs";
        case FilesystemType::Bind:     return "none";   // 绑定挂载使用 MS_BIND 标志, 类型填 "none"
        case FilesystemType::Squashfs: return "squashfs";
        default:                       return "none";
    }
}

// 挂载文件系统
bool Filesystem::mount(const std::string& source, const std::string& target,
                      FilesystemType fs_type, unsigned long flags,
                      const std::string& data) {
    // 确保挂载点目录存在
    makeDirectory(target, 0755);

    // 调用系统 mount()
    // TODO: 自定义 OS 需实现 mount() 系统调用及对应文件系统内核支持
    const char* type_str = typeToString(fs_type);
    int ret = ::mount(source.c_str(), target.c_str(), type_str, flags,
                      data.empty() ? nullptr : data.c_str());
    return ret == 0;
}

// 卸载文件系统
bool Filesystem::umount(const std::string& target) {
    // 先尝试普通卸载
    // TODO: 自定义 OS 需实现 umount() / umount2() 系统调用
    if (::umount(target.c_str()) == 0) {
        return true;
    }
    // 失败则尝试懒卸载 (MNT_DETACH: 立即从命名空间分离, 延迟释放资源)
    return ::umount2(target.c_str(), MNT_DETACH) == 0;
}

// 递归创建目录 (类似 mkdir -p)
bool Filesystem::makeDirectory(const std::string& path, mode_t mode) {
    if (path.empty()) {
        return false;
    }

    // 逐级创建路径中的每个目录分量
    std::string current;
    for (size_t i = 0; i < path.size(); ++i) {
        current += path[i];
        // 遇到路径分隔符时创建已积累的子路径 (跳过根目录 "/")
        if (path[i] == '/' && current.size() > 1) {
            ::mkdir(current.c_str(), mode);
            // 忽略中间错误 (目录已存在等), 最终统一检查
        }
    }

    // 创建最终目录
    if (::mkdir(current.c_str(), mode) != 0 && errno != EEXIST) {
        return false;
    }
    return true;
}

// 删除空目录
bool Filesystem::removeDirectory(const std::string& path) {
    return ::rmdir(path.c_str()) == 0;
}

// 复制文件
bool Filesystem::copyFile(const std::string& src, const std::string& dst) {
    std::ifstream in(src, std::ios::binary);
    if (!in.is_open()) {
        return false;
    }
    std::ofstream out(dst, std::ios::binary);
    if (!out.is_open()) {
        return false;
    }
    // 通过流缓冲区一次性写入
    out << in.rdbuf();
    return out.good();
}

// 判断路径是否存在
bool Filesystem::exists(const std::string& path) {
    struct stat st;
    return ::stat(path.c_str(), &st) == 0;
}

// 判断路径是否为目录
bool Filesystem::isDirectory(const std::string& path) {
    struct stat st;
    if (::stat(path.c_str(), &st) != 0) {
        return false;
    }
    return S_ISDIR(st.st_mode);
}

// 获取文件大小
int64_t Filesystem::getFileSize(const std::string& path) {
    struct stat st;
    if (::stat(path.c_str(), &st) != 0) {
        return -1;
    }
    return static_cast<int64_t>(st.st_size);
}

// 列出目录下的所有条目名称
std::vector<std::string> Filesystem::listDirectory(const std::string& path) {
    std::vector<std::string> entries;
    DIR* dir = ::opendir(path.c_str());
    if (!dir) {
        return entries;
    }
    struct dirent* entry;
    while ((entry = ::readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") {
            continue;
        }
        entries.push_back(name);
    }
    ::closedir(dir);
    return entries;
}

// ====== tar 打包/解包实现 ======
// tar (ustar) 文件头布局 (共 512 字节):
//   偏移 0:    name      (100 字节)
//   偏移 100:  mode      (8 字节, 八进制)
//   偏移 108:  uid       (8 字节, 八进制)
//   偏移 116:  gid       (8 字节, 八进制)
//   偏移 124:  size      (12 字节, 八进制)
//   偏移 136:  mtime     (12 字节, 八进制)
//   偏移 148:  chksum    (8 字节, 八进制)
//   偏移 156:  typeflag  (1 字节, '0'=文件 '5'=目录)
//   偏移 157:  linkname  (100 字节)
//   偏移 257:  magic     (6 字节, "ustar")
//   偏移 263:  version   (2 字节, "00")
//   偏移 345:  prefix    (155 字节, 名字前缀)

// 计算 tar 头校验和
unsigned int Filesystem::calcChecksum(const char* header) {
    // 校验和字段 (偏移 148, 8 字节) 应已填充为空格
    unsigned int sum = 0;
    for (int i = 0; i < 512; ++i) {
        sum += static_cast<unsigned char>(header[i]);
    }
    return sum;
}

// 从 tar 头字段解析八进制数值
int64_t Filesystem::parseOctal(const char* data, int len) {
    int64_t result = 0;
    for (int i = 0; i < len; ++i) {
        if (data[i] >= '0' && data[i] <= '7') {
            result = result * 8 + (data[i] - '0');
        } else {
            // 遇到非八进制字符 (NUL/空格) 即停止
            break;
        }
    }
    return result;
}

// 写入 tar 文件头
void Filesystem::writeTarHeader(std::ofstream& out, const std::string& name,
                                int64_t size, char typeflag, mode_t mode) {
    char header[512];
    std::memset(header, 0, sizeof(header));

    // name (100 字节)
    std::strncpy(header, name.c_str(), 99);

    // mode (8 字节, 七位八进制 + NUL)
    std::snprintf(header + 100, 8, "%07o", static_cast<unsigned>(mode & 0777));

    // uid / gid (8 字节, 此处固定为 0)
    std::snprintf(header + 108, 8, "%07o", 0);
    std::snprintf(header + 116, 8, "%07o", 0);

    // size (12 字节, 十一位八进制 + NUL)
    std::snprintf(header + 124, 12, "%011o", static_cast<unsigned long>(size));

    // mtime (12 字节, 当前时间)
    std::snprintf(header + 136, 12, "%011o",
                  static_cast<unsigned long>(std::time(nullptr)));

    // typeflag (1 字节)
    header[156] = typeflag;

    // magic + version
    std::memcpy(header + 257, "ustar", 5);
    std::memcpy(header + 263, "00", 2);

    // 计算校验和前, 先将校验和字段填充为空格
    std::memset(header + 148, ' ', 8);
    unsigned int chksum = calcChecksum(header);
    // 校验和格式: 六位八进制 + NUL + 空格
    std::snprintf(header + 148, 8, "%06o", chksum);
    header[154] = '\0';
    header[155] = ' ';

    out.write(header, 512);
}

// 递归打包目录
bool Filesystem::createTarballRecursive(const std::string& path,
                                         const std::string& base,
                                         std::ofstream& out) {
    std::vector<std::string> entries = listDirectory(path);
    for (const auto& entry : entries) {
        std::string full_path = path;
        if (full_path.back() != '/') {
            full_path += '/';
        }
        full_path += entry;

        std::string arc_name = base + "/" + entry;

        struct stat st;
        if (::stat(full_path.c_str(), &st) != 0) {
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            // 目录条目: 写入目录头, 递归处理子目录
            writeTarHeader(out, arc_name, 0, '5', st.st_mode);
            createTarballRecursive(full_path, arc_name, out);
        } else if (S_ISREG(st.st_mode)) {
            // 普通文件: 写入文件头 + 内容, 填充到 512 字节边界
            writeTarHeader(out, arc_name, st.st_size, '0', st.st_mode);

            std::ifstream in(full_path, std::ios::binary);
            if (in.is_open()) {
                char buf[4096];
                while (in) {
                    in.read(buf, sizeof(buf));
                    if (in.gcount() > 0) {
                        out.write(buf, in.gcount());
                    }
                }
            }
            // 填充零到 512 字节边界
            int64_t padded = (st.st_size + 511) & ~511LL;
            int64_t padding = padded - st.st_size;
            if (padding > 0) {
                static const char zeros[512] = {0};
                out.write(zeros, padding);
            }
        }
        // 其他类型 (符号链接等) 暂不处理
    }
    return true;
}

// 将源目录打包为 tar 文件
bool Filesystem::createTarball(const std::string& src, const std::string& dst) {
    if (!isDirectory(src)) {
        return false;
    }

    std::ofstream out(dst, std::ios::binary);
    if (!out.is_open()) {
        return false;
    }

    // 提取源目录的 basename 作为归档内的根目录名
    std::string base = src;
    while (base.size() > 1 && base.back() == '/') {
        base.pop_back();
    }
    size_t pos = base.find_last_of('/');
    if (pos != std::string::npos) {
        base = base.substr(pos + 1);
    }
    if (base.empty()) {
        base = "root";
    }

    // 写入根目录条目
    struct stat st;
    if (::stat(src.c_str(), &st) == 0) {
        writeTarHeader(out, base, 0, '5', st.st_mode);
    }

    // 递归打包内容
    createTarballRecursive(src, base, out);

    // 写入归档结束标记 (两个 512 字节的零块)
    static const char zeros[1024] = {0};
    out.write(zeros, 1024);

    return out.good();
}

// 解压 tar 文件到目标目录
bool Filesystem::extractTarball(const std::string& src, const std::string& dst) {
    std::ifstream in(src, std::ios::binary);
    if (!in.is_open()) {
        return false;
    }

    if (!makeDirectory(dst, 0755)) {
        return false;
    }

    char header[512];
    while (in.read(header, 512)) {
        // 检查是否为空块 (归档结束标记)
        bool all_zero = true;
        for (int i = 0; i < 512; ++i) {
            if (header[i] != 0) {
                all_zero = false;
                break;
            }
        }
        if (all_zero) {
            break;
        }

        // 解析文件名: name (100 字节) + prefix (155 字节)
        std::string name(header, 100);
        size_t nul = name.find('\0');
        if (nul != std::string::npos) {
            name = name.substr(0, nul);
        }
        std::string prefix(header + 345, 155);
        nul = prefix.find('\0');
        if (nul != std::string::npos) {
            prefix = prefix.substr(0, nul);
        }
        if (!prefix.empty()) {
            name = prefix + "/" + name;
        }
        if (name.empty()) {
            break;
        }

        // 解析大小、类型、权限
        int64_t size = parseOctal(header + 124, 12);
        char typeflag = header[156];
        mode_t mode = static_cast<mode_t>(parseOctal(header + 100, 8));
        if (mode == 0) {
            mode = 0755;
        }

        // 构建目标完整路径
        std::string full_path = dst;
        if (full_path.back() != '/') {
            full_path += '/';
        }
        full_path += name;

        if (typeflag == '5') {
            // 目录条目
            makeDirectory(full_path, mode);
        } else if (typeflag == '0' || typeflag == '\0') {
            // 普通文件: 先创建父目录
            size_t slash = full_path.find_last_of('/');
            if (slash != std::string::npos) {
                makeDirectory(full_path.substr(0, slash), 0755);
            }
            // 写入文件内容
            std::ofstream out_file(full_path, std::ios::binary);
            int64_t remaining = size;
            char buf[4096];
            while (remaining > 0 && in) {
                int64_t to_read = (remaining < static_cast<int64_t>(sizeof(buf)))
                                  ? remaining
                                  : static_cast<int64_t>(sizeof(buf));
                in.read(buf, to_read);
                int64_t got = in.gcount();
                if (got <= 0) {
                    break;
                }
                out_file.write(buf, got);
                remaining -= got;
            }
            out_file.close();
            // 跳过填充到 512 字节边界
            int64_t padded = (size + 511) & ~511LL;
            int64_t padding = padded - size;
            if (padding > 0) {
                in.ignore(padding);
            }
        } else {
            // 其他类型 (符号链接等): 跳过内容块
            int64_t padded = (size + 511) & ~511LL;
            in.ignore(padded);
        }
    }
    return true;
}
