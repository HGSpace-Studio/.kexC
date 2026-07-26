#pragma once

// 容器 CLI 头文件
// 提供命令行界面, 支持容器/镜像/网络管理操作
// 不依赖第三方 CLI 库, 使用 printf/cout 输出

#include <string>
#include <vector>

#include "orchestration/LifecycleManager.h"
#include "storage/ImageManager.h"
#include "orchestration/NetworkManager.h"

// 容器命令行界面
// 解析命令行参数并调用对应管理器执行操作
class ContainerCli {
public:
    // 构造函数
    // lifecycle:    容器生命周期管理器
    // image_mgr:    镜像管理器
    // network_mgr:  网络管理器
    ContainerCli(LifecycleManager* lifecycle, ImageManager* image_mgr,
                 NetworkManager* network_mgr);
    ~ContainerCli();

    // 主入口
    // argc, argv: 命令行参数
    // 返回:       0 表示成功, 非 0 表示失败
    int run(int argc, char* argv[]);

private:
    // ====== 容器子命令 ======
    int handleContainer(int argc, char* argv[]);
    int handleImage(int argc, char* argv[]);
    int handleNetwork(int argc, char* argv[]);
    int handleInfo();

    // ====== 输出格式化方法 ======
    // 以表格形式打印容器列表
    // show_all: 是否显示已停止的容器
    void printContainers(bool show_all);
    // 以表格形式打印镜像列表
    void printImages();
    // 以表格形式打印网络列表
    void printNetworks();
    // 打印帮助信息
    void printHelp();

    // ====== 参数解析辅助 ======
    // 解析 KEY=VAL 形式的环境变量
    static bool parseEnv(const std::string& s, std::string& key, std::string& val);
    // 解析 host:container 形式的端口映射
    static bool parsePort(const std::string& s, int& host, int& container);
    // 解析 host:container 形式的卷映射
    static bool parseVolume(const std::string& s, std::string& host, std::string& container);

    LifecycleManager* lifecycle_;    // 容器生命周期管理器 (非所有, 外部注入)
    ImageManager* image_mgr_;        // 镜像管理器 (非所有, 外部注入)
    NetworkManager* network_mgr_;    // 网络管理器 (非所有, 外部注入)
};
