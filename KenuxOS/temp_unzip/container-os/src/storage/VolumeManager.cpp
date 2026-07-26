#include "storage/VolumeManager.h"

#include "storage/Filesystem.h"

#include <sys/mount.h>   // MS_BIND
#include <sys/stat.h>
#include <unistd.h>      // unlink()
#include <cerrno>
#include <fstream>
#include <sstream>

// ====== VolumeManager 实现 ======

// 构造函数: 创建卷存储目录并加载已有卷
VolumeManager::VolumeManager(const std::string& storage_path)
    : storage_path_(storage_path),
      volumes_dir_(storage_path_ + "/volumes/") {
    // 确保卷存储目录存在
    Filesystem::makeDirectory(volumes_dir_, 0755);

    // 扫描已有卷, 重建缓存
    for (const auto& name : Filesystem::listDirectory(volumes_dir_)) {
        std::string dir = volumes_dir_ + name;
        if (!Filesystem::isDirectory(dir)) {
            continue;
        }

        VolumeInfo info;
        info.name = name;
        info.mountpoint = dir + "/data";

        // 从选项文件加载驱动与选项
        info.options = loadOptions(dir);
        auto it = info.options.find("driver");
        info.driver = (it != info.options.end()) ? it->second : "local";

        volumes_[name] = info;
    }
}

// 析构函数
VolumeManager::~VolumeManager() = default;

// 创建数据卷
bool VolumeManager::createVolume(const std::string& name, const std::string& driver,
                                 const std::map<std::string, std::string>& options) {
    // 卷已存在则返回失败
    if (volumes_.find(name) != volumes_.end()) {
        return false;
    }

    std::string dir = volumes_dir_ + name;
    std::string data = dir + "/data";

    // 创建卷数据目录
    if (!Filesystem::makeDirectory(data, 0755)) {
        return false;
    }

    // tmpfs 驱动: 挂载 tmpfs 到数据目录
    if (driver == "tmpfs") {
        // TODO: 自定义 OS 需实现 tmpfs 内核支持
        if (!Filesystem::mount("tmpfs", data, FilesystemType::Tmpfs, 0, "")) {
            return false;
        }
    }

    // 保存选项 (含 driver 字段)
    std::map<std::string, std::string> all_options = options;
    all_options["driver"] = driver;
    saveOptions(dir, all_options);

    // 更新缓存
    VolumeInfo info;
    info.name = name;
    info.driver = driver;
    info.mountpoint = data;
    info.options = options;
    volumes_[name] = info;
    return true;
}

// 删除数据卷
bool VolumeManager::removeVolume(const std::string& name) {
    auto it = volumes_.find(name);
    if (it == volumes_.end()) {
        return false;
    }

    std::string dir = volumes_dir_ + name;

    // tmpfs 驱动: 先卸载 tmpfs
    if (it->second.driver == "tmpfs") {
        Filesystem::umount(it->second.mountpoint);
    }

    // 递归删除卷目录
    removeTree(dir);

    volumes_.erase(it);
    return true;
}

// 列出所有数据卷
std::vector<VolumeInfo> VolumeManager::listVolumes() {
    std::vector<VolumeInfo> result;
    result.reserve(volumes_.size());
    for (const auto& kv : volumes_) {
        result.push_back(kv.second);
    }
    return result;
}

// 获取指定卷的信息
VolumeInfo VolumeManager::getVolumeInfo(const std::string& name) {
    auto it = volumes_.find(name);
    if (it != volumes_.end()) {
        return it->second;
    }
    return VolumeInfo{};  // 不存在时返回默认值
}

// 将卷挂载到容器的指定路径 (绑定挂载)
bool VolumeManager::mountVolume(const std::string& name, const std::string& container_id,
                                const std::string& dest) {
    auto it = volumes_.find(name);
    if (it == volumes_.end()) {
        return false;
    }

    // 确保目标路径存在
    Filesystem::makeDirectory(dest, 0755);

    // 绑定挂载卷数据目录到目标路径
    // TODO: 自定义 OS 需实现 MS_BIND 挂载
    return Filesystem::mount(it->second.mountpoint, dest, FilesystemType::Bind,
                             MS_BIND, "");
}

// 从容器的指定路径卸载卷
bool VolumeManager::unmountVolume(const std::string& container_id, const std::string& dest) {
    // 卸载目标路径上的绑定挂载
    return Filesystem::umount(dest);
}

// 保存卷选项到文件 (每行 "key=value")
bool VolumeManager::saveOptions(const std::string& volume_dir,
                                const std::map<std::string, std::string>& options) {
    std::ofstream ofs(volume_dir + "/options");
    if (!ofs.is_open()) {
        return false;
    }
    for (const auto& kv : options) {
        ofs << kv.first << "=" << kv.second << "\n";
    }
    return ofs.good();
}

// 从文件加载卷选项
std::map<std::string, std::string> VolumeManager::loadOptions(const std::string& volume_dir) {
    std::map<std::string, std::string> options;
    std::ifstream ifs(volume_dir + "/options");
    if (!ifs.is_open()) {
        return options;
    }
    std::string line;
    while (std::getline(ifs, line)) {
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            options[line.substr(0, pos)] = line.substr(pos + 1);
        }
    }
    return options;
}

// 递归删除目录树
bool VolumeManager::removeTree(const std::string& path) {
    for (const auto& entry : Filesystem::listDirectory(path)) {
        std::string child = path + "/" + entry;
        if (Filesystem::isDirectory(child)) {
            removeTree(child);
        } else {
            ::unlink(child.c_str());
        }
    }
    return Filesystem::removeDirectory(path);
}
