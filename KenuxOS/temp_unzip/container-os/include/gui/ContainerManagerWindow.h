#pragma once

// 容器管理 GUI 窗口头文件
// 提供图形界面 (Qt6) 和文本界面 (TUI) 两种实现
// 通过 HAVE_QT 宏进行条件编译:
//   - 定义了 HAVE_QT 时使用 Qt6 的 QMainWindow + QTableWidget
//   - 未定义时回退到基于 cout 的简单 TUI 文本界面

#include <string>
#include <atomic>
#include <thread>
#include <chrono>

#include "orchestration/LifecycleManager.h"
#include "storage/ImageManager.h"
#include "orchestration/NetworkManager.h"

#ifdef HAVE_QT
// ==================== Qt6 图形界面版本 ====================

#include <QMainWindow>
#include <QTableWidget>
#include <QTabWidget>
#include <QToolBar>
#include <QStatusBar>
#include <QTimer>
#include <QAction>
#include <QLabel>

// 容器管理主窗口 (Qt 版本)
// 继承 QMainWindow, 提供容器/镜像双标签页表格、工具栏和状态栏
// 使用 QTimer 定时刷新容器列表与状态栏
class ContainerManagerWindow : public QMainWindow {
    Q_OBJECT
public:
    // 构造函数
    // lifecycle:   容器生命周期管理器 (外部注入, 非所有)
    // image_mgr:   镜像管理器 (外部注入, 非所有)
    // network_mgr: 网络管理器 (外部注入, 非所有)
    ContainerManagerWindow(LifecycleManager* lifecycle,
                           ImageManager* image_mgr,
                           NetworkManager* network_mgr);

    ~ContainerManagerWindow();

    // 刷新容器列表, 从 LifecycleManager 拉取最新数据填充表格
    void refreshContainerList();

    // 刷新镜像列表, 从 ImageManager 拉取最新数据填充表格
    void refreshImageList();

    // 显示创建容器对话框, 收集用户输入并调用 LifecycleManager::createContainer
    void showCreateDialog();

    // 容器双击回调, 显示该容器的详细信息
    // id: 被双击的容器 ID
    void onContainerDoubleClicked(const std::string& id);

private slots:
    void onCreateClicked();        // 工具栏 "创建容器" 按钮触发
    void onStartClicked();         // 工具栏 "启动" 按钮触发
    void onStopClicked();          // 工具栏 "停止" 按钮触发
    void onDeleteClicked();        // 工具栏 "删除" 按钮触发
    void onRefreshClicked();       // 工具栏 "刷新" 按钮触发
    void onTimerTimeout();         // 定时器超时, 自动刷新
    void onContainerCellDoubleClicked(int row, int column);  // 表格单元格双击

private:
    // 构建 UI 界面 (标签页、表格、工具栏、状态栏)
    void setupUI();
    // 构建工具栏按钮
    void setupToolbar();
    // 构建状态栏
    void setupStatusBar();
    // 更新状态栏的容器统计与系统资源信息
    void updateStatusBar();
    // 获取当前选中行的容器 ID (无选中返回空串)
    std::string getSelectedContainerId() const;

    LifecycleManager* lifecycle_;       // 生命周期管理器
    ImageManager* image_mgr_;           // 镜像管理器
    NetworkManager* network_mgr_;       // 网络管理器

    QTabWidget* tabs_;                  // 标签页控件: 容器列表 / 镜像列表
    QTableWidget* container_table_;     // 容器列表表格 (ID, 名称, 镜像, 状态, CPU, 内存)
    QTableWidget* image_table_;         // 镜像列表表格
    QToolBar* toolbar_;                 // 工具栏
    QStatusBar* status_bar_;            // 状态栏
    QLabel* status_label_;              // 状态栏文本标签
    QTimer* timer_;                     // 定时刷新定时器
};

#else
// ==================== TUI 文本界面版本 ====================

#include <iostream>
#include <sstream>

// 容器管理窗口 (TUI 版本)
// 不依赖 Qt 或 ncurses, 仅使用标准 cout 输出
// 通过 select() 实现带超时的输入读取, 达到定时刷新效果
class ContainerManagerWindow {
public:
    // 构造函数
    // lifecycle:   容器生命周期管理器
    // image_mgr:   镜像管理器
    // network_mgr: 网络管理器
    ContainerManagerWindow(LifecycleManager* lifecycle,
                           ImageManager* image_mgr,
                           NetworkManager* network_mgr);

    ~ContainerManagerWindow();

    // 刷新容器列表 (打印到终端)
    void refreshContainerList();

    // 刷新镜像列表 (打印到终端)
    void refreshImageList();

    // 显示创建容器对话框 (通过 cin 读取用户输入)
    void showCreateDialog();

    // 容器双击回调 (TUI 中通过选择序号触发, 显示容器详情)
    void onContainerDoubleClicked(const std::string& id);

    // 主事件循环 (阻塞), 由 main 调用
    // 使用 select() 实现带超时的输入, 每次超时后自动刷新显示
    // 返回退出码 (0 表示正常退出)
    int exec();

private:
    // 打印主界面 (清屏后输出容器列表、镜像列表和系统信息)
    void printDisplay();
    // 打印操作菜单
    void printMenu();
    // 处理用户输入命令
    void handleInput(const std::string& input);
    // 根据序号获取容器 ID (序号从 1 开始, 无效返回空串)
    std::string getContainerIdByIndex(int index);

    LifecycleManager* lifecycle_;       // 生命周期管理器
    ImageManager* image_mgr_;           // 镜像管理器
    NetworkManager* network_mgr_;       // 网络管理器

    std::atomic<bool> running_;         // 主循环运行标志
};

#endif // HAVE_QT
