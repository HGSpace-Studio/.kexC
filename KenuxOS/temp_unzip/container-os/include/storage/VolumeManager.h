#pragma once

// 卷管理器头文件
// 负责容器数据卷的创建、删除、查询与挂载/卸载
// 数据卷提供容器间持久化存储的共享机制

#include <string>
#include <vector>
#include <unordered_map>
#include <map>

// 卷信息结构体
// 描述一个数据卷的元数据
struct VolumeInfo {
    std::string name;                                   // 卷名称
    std::string driver;                                 // 卷驱动 ("local" 或 "tmpfs")
    std::string mountpoint;                             // 卷挂载点路径 (宿主机侧)
    std::map<std::string, std::string> options;         // 卷选项 (键值对)
};

// 卷管理器
// 负责数据卷的生命周期管理与挂载/卸载
// 卷存储路径: storage_path_/volumes/<name>/
class VolumeManager {
public:
    // 构造函数
    // storage_path: 存储根路径
    explicit VolumeManager(const std::string& storage_path);
    ~VolumeManager();

    // 创建数据卷
    // name:    卷名称
    // driver:  卷驱动 ("local" 或 "tmpfs")
    // options: 卷选项 (键值对)
    // 返回:    成功返回 true (已存在返回 false)
    bool createVolume(const std::string& name, const std::string& driver,
                      const std::map<std::string, std::string>& options);

    // 删除数据卷
    // name: 卷名称
    // 返回: 成功返回 true
    bool removeVolume(const std::string& name);

    // 列出所有数据卷
    std::vector<VolumeInfo> listVolumes();

    // 获取指定卷的信息
    // name: 卷名称
    // 返回: VolumeInfo (不存在时返回默认值)
    VolumeInfo getVolumeInfo(const std::string& name);

    // 将卷挂载到容器的指定路径 (绑定挂载)
    // name:         卷名称
    // container_id: 容器标识
    // dest:         容器内目标路径
    // 返回:          成功返回 true
    bool mountVolume(const std::string& name, const std::string& container_id,
                     const std::string& dest);

    // 从容器的指定路径卸载卷
    // container_id: 容器标识
    // dest:         容器内目标路径
    // 返回:          成功返回 true
    bool unmountVolume(const std::string& container_id, const std::string& dest);

private:
    // 存储根路径
    std::string storage_path_;

    // 卷存储目录
    std::string volumes_dir_;

    // 卷信息缓存: name -> VolumeInfo
    std::unordered_map<std::string, VolumeInfo> volumes_;

    // 保存卷选项到文件
    // volume_dir: 卷目录路径
    // options:    选项键值对
    bool saveOptions(const std::string& volume_dir,
                     const std::map<std::string, std::string>& options);

    // 从文件加载卷选项
    // volume_dir: 卷目录路径
    // 返回:        选项键值对
    std::map<std::string, std::string> loadOptions(const std::string& volume_dir);

    // 递归删除目录树
    bool removeTree(const std::string& path);
};
