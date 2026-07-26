#pragma once

// 镜像管理器头文件
// 负责容器镜像的拉取、加载、存储与 rootfs 挂载
// 通过 OverlayFS 将多个镜像层联合挂载为容器根文件系统

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

// 镜像信息结构体
// 描述一个容器镜像的元数据
struct ImageInfo {
    std::string id;             // 镜像唯一标识 (哈希)
    std::string name;           // 镜像名称 (如 "ubuntu")
    std::string tag;            // 镜像标签 (如 "latest")
    std::string os;             // 目标操作系统 (如 "linux")
    std::string arch;           // 目标架构 (如 "amd64")
    std::string binary_format;  // 二进制格式 (如 "elf")
    int64_t size = 0;           // 镜像总大小 (字节)
    std::string config;         // 镜像配置 (JSON 字符串)
};

// 镜像层结构体
// 描述镜像的单个层 (layer)
struct ImageLayer {
    std::string digest;         // 层摘要 (如 "sha256:abcdef...")
    std::string path;           // 层在磁盘上的解压路径 (OverlayFS lowerdir)
    int64_t size = 0;           // 层大小 (字节)
    bool cached = false;        // 是否已缓存到本地
};

// 镜像管理器
// 负责镜像的拉取、加载、删除、查询, 以及容器 rootfs 的挂载/卸载
// 镜像存储路径: storage_path_/images/
// 容器 rootfs 路径: storage_path_/containers/<container_id>/rootfs
class ImageManager {
public:
    // 构造函数
    // storage_path: 存储根路径, 镜像与容器数据均存放于此
    explicit ImageManager(const std::string& storage_path);
    ~ImageManager();

    // 拉取镜像 (模拟下载)
    // name: 镜像名称
    // tag:  镜像标签
    // 返回: 成功返回 true (已存在视为成功)
    bool pullImage(const std::string& name, const std::string& tag);

    // 从 tar 文件加载镜像 (兼容 Docker 镜像格式)
    // tar_path: tar 文件路径
    // 返回:     成功返回 true
    bool loadImage(const std::string& tar_path);

    // 删除镜像
    // name: 镜像名称
    // tag:  镜像标签
    // 返回: 成功返回 true
    bool removeImage(const std::string& name, const std::string& tag);

    // 列出所有本地镜像
    std::vector<ImageInfo> listImages();

    // 获取指定镜像的信息
    // name: 镜像名称
    // tag:  镜像标签
    // 返回: ImageInfo (不存在时返回默认值)
    ImageInfo getImageInfo(const std::string& name, const std::string& tag);

    // 获取镜像的所有层
    // image_id: 镜像标识
    // 返回:      层列表 (从底层到顶层)
    std::vector<ImageLayer> getLayers(const std::string& image_id);

    // 挂载镜像为容器 rootfs (通过 OverlayFS 联合挂载所有层)
    // image_id:     镜像标识
    // container_id: 容器标识
    // 返回:          挂载点路径 (失败返回空字符串)
    std::string mountRootfs(const std::string& image_id,
                            const std::string& container_id);

    // 卸载容器 rootfs
    // container_id: 容器标识
    // 返回:          成功返回 true
    bool unmountRootfs(const std::string& container_id);

private:
    // OverlayFS 挂载
    // layers:      底层目录列表 (从上到下排列)
    // upper_dir:   可写层目录
    // work_dir:    OverlayFS 工作目录
    // mount_point: 挂载点
    // 返回:         成功返回 true
    bool mountOverlay(const std::vector<std::string>& layers,
                      const std::string& upper_dir,
                      const std::string& work_dir,
                      const std::string& mount_point);

    // 解析 manifest.json, 填充镜像信息
    // path: manifest.json 文件路径
    // info: 输出参数, 填充后的镜像信息
    // 返回: 成功返回 true
    bool parseManifest(const std::string& path, ImageInfo& info);

    // 解压镜像层到目标路径
    // layer_path: 层 tar 文件路径
    // dest_path:  解压目标目录
    // 返回:        成功返回 true
    bool extractLayer(const std::string& layer_path,
                      const std::string& dest_path);

    // 存储根路径
    std::string storage_path_;

    // 镜像信息缓存: image_id -> ImageInfo
    std::unordered_map<std::string, ImageInfo> images_;

    // 镜像层缓存: image_id -> 层列表
    std::unordered_map<std::string, std::vector<ImageLayer>> layers_;
};
