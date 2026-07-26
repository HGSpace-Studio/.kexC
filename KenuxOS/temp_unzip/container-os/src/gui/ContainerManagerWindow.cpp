// 容器管理 GUI 窗口实现文件
// 包含 Qt6 图形界面和 TUI 文本界面两种实现, 通过 HAVE_QT 宏切换

#include "gui/ContainerManagerWindow.h"

#include "utils/Logger.h"
#include "orchestration/ResourceManager.h"

#include <vector>
#include <cstdint>
#include <string>
#include <cstdlib>

// ==================== 公共辅助函数 ====================

// 容器状态枚举转中文字符串
static const char* statusToString(ContainerStatus status) {
    switch (status) {
        case ContainerStatus::Created: return "已创建";
        case ContainerStatus::Running: return "运行中";
        case ContainerStatus::Stopped: return "已停止";
        case ContainerStatus::Deleted: return "已删除";
        case ContainerStatus::Error:   return "错误";
        default: return "未知";
    }
}

// 将字节数格式化为可读字符串 (如 "512 MB", "2 GB")
static std::string formatMemory(uint64_t bytes) {
    if (bytes == 0) return "无限制";
    const uint64_t KB = 1024;
    const uint64_t MB = 1024 * KB;
    const uint64_t GB = 1024 * MB;
    if (bytes >= GB) return std::to_string(bytes / GB) + " GB";
    if (bytes >= MB) return std::to_string(bytes / MB) + " MB";
    if (bytes >= KB) return std::to_string(bytes / KB) + " KB";
    return std::to_string(bytes) + " B";
}

#ifdef HAVE_QT
// ==================== Qt6 图形界面版本实现 ====================

#include <QApplication>
#include <QLineEdit>
#include <QDialog>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QTextEdit>
#include <QPushButton>

ContainerManagerWindow::ContainerManagerWindow(LifecycleManager* lifecycle,
                                               ImageManager* image_mgr,
                                               NetworkManager* network_mgr)
    : QMainWindow(nullptr)
    , lifecycle_(lifecycle)
    , image_mgr_(image_mgr)
    , network_mgr_(network_mgr)
    , tabs_(nullptr)
    , container_table_(nullptr)
    , image_table_(nullptr)
    , toolbar_(nullptr)
    , status_bar_(nullptr)
    , status_label_(nullptr)
    , timer_(nullptr)
{
    setupUI();
    setupToolbar();
    setupStatusBar();

    // 首次加载数据
    refreshContainerList();
    refreshImageList();
    updateStatusBar();

    // 启动定时刷新 (每 2 秒)
    timer_ = new QTimer(this);
    connect(timer_, &QTimer::timeout, this, &ContainerManagerWindow::onTimerTimeout);
    timer_->start(2000);

    setWindowTitle("container-os 容器管理器");
    resize(900, 600);

    Logger::getInstance().info("GUI 窗口初始化完成 (Qt6 模式)");
}

ContainerManagerWindow::~ContainerManagerWindow() {
    if (timer_) {
        timer_->stop();
    }
    Logger::getInstance().info("GUI 窗口已销毁");
}

void ContainerManagerWindow::setupUI() {
    // 创建标签页控件
    tabs_ = new QTabWidget(this);
    setCentralWidget(tabs_);

    // ----- 容器列表表格 -----
    container_table_ = new QTableWidget(this);
    container_table_->setColumnCount(6);
    container_table_->setHorizontalHeaderLabels(
        {"ID", "名称", "镜像", "状态", "CPU (核)", "内存"});
    container_table_->horizontalHeader()->setStretchLastSection(true);
    container_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    container_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    container_table_->setColumnWidth(0, 200);
    container_table_->setColumnWidth(1, 120);
    container_table_->setColumnWidth(2, 150);
    container_table_->setColumnWidth(3, 80);
    container_table_->setColumnWidth(4, 80);
    connect(container_table_, &QTableWidget::cellDoubleClicked,
            this, &ContainerManagerWindow::onContainerCellDoubleClicked);

    tabs_->addTab(container_table_, "容器列表");

    // ----- 镜像列表表格 -----
    image_table_ = new QTableWidget(this);
    image_table_->setColumnCount(5);
    image_table_->setHorizontalHeaderLabels(
        {"镜像 ID", "名称", "标签", "OS", "大小"});
    image_table_->horizontalHeader()->setStretchLastSection(true);
    image_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    image_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);

    tabs_->addTab(image_table_, "镜像列表");
}

void ContainerManagerWindow::setupToolbar() {
    toolbar_ = addToolBar("主工具栏");
    toolbar_->setMovable(false);

    // 创建容器按钮
    QAction* create_action = new QAction("创建容器", this);
    create_action->setToolTip("创建新容器");
    connect(create_action, &QAction::triggered, this, &ContainerManagerWindow::onCreateClicked);
    toolbar_->addAction(create_action);

    toolbar_->addSeparator();

    // 启动按钮
    QAction* start_action = new QAction("启动", this);
    start_action->setToolTip("启动选中的容器");
    connect(start_action, &QAction::triggered, this, &ContainerManagerWindow::onStartClicked);
    toolbar_->addAction(start_action);

    // 停止按钮
    QAction* stop_action = new QAction("停止", this);
    stop_action->setToolTip("停止选中的容器");
    connect(stop_action, &QAction::triggered, this, &ContainerManagerWindow::onStopClicked);
    toolbar_->addAction(stop_action);

    // 删除按钮
    QAction* delete_action = new QAction("删除", this);
    delete_action->setToolTip("删除选中的容器");
    connect(delete_action, &QAction::triggered, this, &ContainerManagerWindow::onDeleteClicked);
    toolbar_->addAction(delete_action);

    toolbar_->addSeparator();

    // 刷新按钮
    QAction* refresh_action = new QAction("刷新", this);
    refresh_action->setToolTip("刷新所有列表");
    connect(refresh_action, &QAction::triggered, this, &ContainerManagerWindow::onRefreshClicked);
    toolbar_->addAction(refresh_action);
}

void ContainerManagerWindow::setupStatusBar() {
    status_bar_ = statusBar();
    status_label_ = new QLabel(this);
    status_bar_->addWidget(status_label_);
}

void ContainerManagerWindow::updateStatusBar() {
    // 统计容器数量
    auto containers = lifecycle_->listContainers();
    int running_count = 0;
    for (auto* c : containers) {
        if (c->getStatus() == ContainerStatus::Running) {
            ++running_count;
        }
    }

    auto images = image_mgr_->listImages();

    // 获取系统资源信息 (通过 ResourceManager)
    ResourceManager rm;
    HostResources host = rm.getHostResources();

    QString text = QString("容器: %1 个 (运行中: %2) | 镜像: %3 个 | "
                           "CPU: %4 核 (使用率: %5%) | 内存可用: %6 MB")
        .arg(containers.size())
        .arg(running_count)
        .arg(images.size())
        .arg(host.cpu_count)
        .arg(static_cast<int>(host.cpu_usage_percent))
        .arg(host.available_memory / (1024 * 1024));

    status_label_->setText(text);
}

void ContainerManagerWindow::refreshContainerList() {
    auto containers = lifecycle_->listContainers();
    container_table_->setRowCount(static_cast<int>(containers.size()));

    int row = 0;
    for (auto* c : containers) {
        const auto& cfg = c->getConfig();
        container_table_->setItem(row, 0,
            new QTableWidgetItem(QString::fromStdString(c->getId())));
        container_table_->setItem(row, 1,
            new QTableWidgetItem(QString::fromStdString(cfg.name)));
        container_table_->setItem(row, 2,
            new QTableWidgetItem(QString::fromStdString(cfg.image)));
        container_table_->setItem(row, 3,
            new QTableWidgetItem(QString::fromUtf8(statusToString(c->getStatus()))));
        container_table_->setItem(row, 4,
            new QTableWidgetItem(QString::number(cfg.resources.cpu_limit)));
        container_table_->setItem(row, 5,
            new QTableWidgetItem(
                QString::fromStdString(formatMemory(cfg.resources.memory_limit))));
        ++row;
    }
}

void ContainerManagerWindow::refreshImageList() {
    auto images = image_mgr_->listImages();
    image_table_->setRowCount(static_cast<int>(images.size()));

    int row = 0;
    for (const auto& img : images) {
        image_table_->setItem(row, 0,
            new QTableWidgetItem(QString::fromStdString(img.id)));
        image_table_->setItem(row, 1,
            new QTableWidgetItem(QString::fromStdString(img.name)));
        image_table_->setItem(row, 2,
            new QTableWidgetItem(QString::fromStdString(img.tag)));
        image_table_->setItem(row, 3,
            new QTableWidgetItem(QString::fromStdString(img.os)));
        image_table_->setItem(row, 4,
            new QTableWidgetItem(
                QString::fromStdString(formatMemory(static_cast<uint64_t>(img.size)))));
        ++row;
    }
}

void ContainerManagerWindow::showCreateDialog() {
    QDialog dialog(this);
    dialog.setWindowTitle("创建容器");
    dialog.setMinimumWidth(350);

    QFormLayout form(&dialog);

    auto* name_edit = new QLineEdit(&dialog);
    name_edit->setPlaceholderText("例如: my-container");
    auto* image_edit = new QLineEdit(&dialog);
    image_edit->setPlaceholderText("例如: ubuntu:latest");
    auto* command_edit = new QLineEdit(&dialog);
    command_edit->setPlaceholderText("例如: /bin/bash");
    auto* cpu_edit = new QLineEdit("2", &dialog);
    auto* memory_edit = new QLineEdit("512", &dialog);

    form.addRow("容器名称:", name_edit);
    form.addRow("镜像名称:", image_edit);
    form.addRow("启动命令:", command_edit);
    form.addRow("CPU 限制 (核):", cpu_edit);
    form.addRow("内存限制 (MB):", memory_edit);

    auto* button_box = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
    form.addRow(button_box);

    connect(button_box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(button_box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        ContainerConfig config;
        config.name = name_edit->text().toStdString();
        config.image = image_edit->text().toStdString();
        config.command = command_edit->text().toStdString();
        config.resources.cpu_limit = cpu_edit->text().toInt();
        config.resources.memory_limit =
            memory_edit->text().toULongLong() * 1024 * 1024;

        std::string id = lifecycle_->createContainer(config);
        if (!id.empty()) {
            QMessageBox::information(this, "成功",
                QString("容器创建成功\nID: %1").arg(QString::fromStdString(id)));
            refreshContainerList();
            updateStatusBar();
            Logger::getInstance().info("容器创建成功: " + id);
        } else {
            QMessageBox::warning(this, "失败", "容器创建失败, 请检查日志");
            Logger::getInstance().error("容器创建失败");
        }
    }
}

void ContainerManagerWindow::onContainerDoubleClicked(const std::string& id) {
    Container* c = lifecycle_->getContainer(id);
    if (!c) {
        QMessageBox::warning(this, "提示", "容器不存在");
        return;
    }

    // 使用容器的 inspect() 获取详情 JSON
    QString details = QString::fromStdString(c->inspect());

    QDialog detail_dialog(this);
    detail_dialog.setWindowTitle(QString("容器详情 - %1").arg(
        QString::fromStdString(c->getId())));
    detail_dialog.setMinimumSize(500, 400);

    auto* layout = new QVBoxLayout(&detail_dialog);
    auto* text_edit = new QTextEdit(&detail_dialog);
    text_edit->setReadOnly(true);
    text_edit->setPlainText(details);
    layout->addWidget(text_edit);

    auto* close_btn = new QPushButton("关闭", &detail_dialog);
    connect(close_btn, &QPushButton::clicked, &detail_dialog, &QDialog::accept);
    layout->addWidget(close_btn);

    detail_dialog.exec();
}

std::string ContainerManagerWindow::getSelectedContainerId() const {
    int row = container_table_->currentRow();
    if (row < 0) return "";
    QTableWidgetItem* item = container_table_->item(row, 0);
    if (!item) return "";
    return item->text().toStdString();
}

// ----- 工具栏按钮槽函数 -----

void ContainerManagerWindow::onCreateClicked() {
    showCreateDialog();
}

void ContainerManagerWindow::onStartClicked() {
    std::string id = getSelectedContainerId();
    if (id.empty()) {
        QMessageBox::warning(this, "提示", "请先选中一个容器");
        return;
    }
    if (lifecycle_->startContainer(id)) {
        Logger::getInstance().info("容器已启动: " + id);
    } else {
        QMessageBox::warning(this, "失败", "容器启动失败");
        Logger::getInstance().error("容器启动失败: " + id);
    }
    refreshContainerList();
    updateStatusBar();
}

void ContainerManagerWindow::onStopClicked() {
    std::string id = getSelectedContainerId();
    if (id.empty()) {
        QMessageBox::warning(this, "提示", "请先选中一个容器");
        return;
    }
    if (lifecycle_->stopContainer(id)) {
        Logger::getInstance().info("容器已停止: " + id);
    } else {
        QMessageBox::warning(this, "失败", "容器停止失败");
        Logger::getInstance().error("容器停止失败: " + id);
    }
    refreshContainerList();
    updateStatusBar();
}

void ContainerManagerWindow::onDeleteClicked() {
    std::string id = getSelectedContainerId();
    if (id.empty()) {
        QMessageBox::warning(this, "提示", "请先选中一个容器");
        return;
    }
    auto reply = QMessageBox::question(this, "确认删除",
        QString("确定要删除容器 %1 吗?").arg(QString::fromStdString(id)),
        QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    if (lifecycle_->destroyContainer(id)) {
        Logger::getInstance().info("容器已删除: " + id);
    } else {
        QMessageBox::warning(this, "失败", "容器删除失败");
        Logger::getInstance().error("容器删除失败: " + id);
    }
    refreshContainerList();
    updateStatusBar();
}

void ContainerManagerWindow::onRefreshClicked() {
    refreshContainerList();
    refreshImageList();
    updateStatusBar();
}

void ContainerManagerWindow::onTimerTimeout() {
    refreshContainerList();
    refreshImageList();
    updateStatusBar();
}

void ContainerManagerWindow::onContainerCellDoubleClicked(int row, int /*column*/) {
    QTableWidgetItem* item = container_table_->item(row, 0);
    if (!item) return;
    std::string id = item->text().toStdString();
    onContainerDoubleClicked(id);
}

#else
// ==================== TUI 文本界面版本实现 ====================

#include <iomanip>
#include <cstring>

// POSIX 头文件, 用于 select() 实现带超时的输入
#include <sys/select.h>
#include <unistd.h>

ContainerManagerWindow::ContainerManagerWindow(LifecycleManager* lifecycle,
                                               ImageManager* image_mgr,
                                               NetworkManager* network_mgr)
    : lifecycle_(lifecycle)
    , image_mgr_(image_mgr)
    , network_mgr_(network_mgr)
    , running_(false)
{
    Logger::getInstance().info("TUI 窗口初始化完成 (非 Qt 模式)");
}

ContainerManagerWindow::~ContainerManagerWindow() {
    running_ = false;
    Logger::getInstance().info("TUI 窗口已销毁");
}

void ContainerManagerWindow::refreshContainerList() {
    auto containers = lifecycle_->listContainers();

    // 打印分隔线和表头
    std::cout << "┌──────┬──────────────────────────┬────────────────┬"
                 "──────────────┬────────┬──────────────┐\n";
    std::cout << "│ 序号 │ ID                       │ 名称           │"
                 " 镜像         │ 状态   │ CPU/内存     │\n";
    std::cout << "├──────┼──────────────────────────┼────────────────┼"
                 "──────────────┼────────┼──────────────┤\n";

    if (containers.empty()) {
        std::cout << "│      (暂无容器)                                                       │\n";
    }

    int index = 1;
    for (auto* c : containers) {
        const auto& cfg = c->getConfig();
        // 截断 ID 以适应列宽
        std::string id = c->getId();
        if (id.length() > 24) id = id.substr(0, 24);

        std::string name = cfg.name;
        if (name.length() > 14) name = name.substr(0, 14);

        std::string image = cfg.image;
        if (image.length() > 12) image = image.substr(0, 12);

        std::string status = statusToString(c->getStatus());
        std::string res = std::to_string(cfg.resources.cpu_limit) + "核/" +
                          formatMemory(cfg.resources.memory_limit);
        if (res.length() > 12) res = res.substr(0, 12);

        std::cout << "│ " << std::setw(4) << std::right << index << " │ "
                  << std::left << std::setw(24) << id << " │ "
                  << std::setw(14) << name << " │ "
                  << std::setw(12) << image << " │ "
                  << std::setw(6) << status << " │ "
                  << std::setw(12) << res << " │\n";
        ++index;
    }

    std::cout << "└──────┴──────────────────────────┴────────────────┴"
                 "──────────────┴────────┴──────────────┘\n";
}

void ContainerManagerWindow::refreshImageList() {
    auto images = image_mgr_->listImages();

    std::cout << "\n  --- 镜像列表 (" << images.size() << " 个) ---\n";
    if (images.empty()) {
        std::cout << "  (暂无镜像)\n";
        return;
    }

    for (const auto& img : images) {
        std::cout << "  " << img.name << ":" << img.tag
                  << "  [OS: " << img.os << "/"
                  << img.arch << "]  "
                  << formatMemory(static_cast<uint64_t>(img.size))
                  << "  (ID: " << img.id.substr(0, 12) << ")\n";
    }
}

void ContainerManagerWindow::showCreateDialog() {
    std::cout << "\n===== 创建新容器 =====\n";

    std::string name, image, command;
    int cpu = 0;
    uint64_t memory_mb = 0;

    std::cout << "容器名称: ";
    std::getline(std::cin, name);

    std::cout << "镜像名称 (如 ubuntu:latest): ";
    std::getline(std::cin, image);

    std::cout << "启动命令 (如 /bin/bash): ";
    std::getline(std::cin, command);

    std::cout << "CPU 限制 (核, 0=不限制): ";
    std::string cpu_str;
    std::getline(std::cin, cpu_str);
    cpu = std::atoi(cpu_str.c_str());

    std::cout << "内存限制 (MB, 0=不限制): ";
    std::string mem_str;
    std::getline(std::cin, mem_str);
    memory_mb = std::strtoull(mem_str.c_str(), nullptr, 10);

    ContainerConfig config;
    config.name = name;
    config.image = image;
    config.command = command;
    config.resources.cpu_limit = cpu;
    config.resources.memory_limit = memory_mb * 1024 * 1024;

    std::string id = lifecycle_->createContainer(config);
    if (!id.empty()) {
        std::cout << "容器创建成功! ID: " << id << "\n";
        Logger::getInstance().info("容器创建成功: " + id);
    } else {
        std::cout << "容器创建失败!\n";
        Logger::getInstance().error("容器创建失败");
    }

    std::cout << "按回车继续...";
    std::cin.ignore(1, '\n');
}

void ContainerManagerWindow::onContainerDoubleClicked(const std::string& id) {
    Container* c = lifecycle_->getContainer(id);
    if (!c) {
        std::cout << "容器不存在: " << id << "\n";
        return;
    }

    std::cout << "\n===== 容器详情 =====\n";
    std::cout << c->inspect() << "\n";
    std::cout << "==============================\n";
    std::cout << "按回车继续...";
    std::cin.ignore(1, '\n');
}

std::string ContainerManagerWindow::getContainerIdByIndex(int index) {
    auto containers = lifecycle_->listContainers();
    if (index < 1 || index > static_cast<int>(containers.size())) {
        return "";
    }
    return containers[index - 1]->getId();
}

void ContainerManagerWindow::printDisplay() {
    // 清屏并将光标移到左上角 (ANSI 转义序列)
    std::cout << "\033[2J\033[H";

    std::cout << "========================================\n";
    std::cout << "     container-os 容器管理器 v1.0.0\n";
    std::cout << "========================================\n\n";

    // 打印系统资源信息
    ResourceManager rm;
    HostResources host = rm.getHostResources();
    std::cout << "系统资源: CPU " << host.cpu_count << " 核 ("
              << static_cast<int>(host.cpu_usage_percent) << "%) | "
              << "内存可用: " << (host.available_memory / (1024 * 1024)) << " MB | "
              << "磁盘可用: " << (host.disk_available / (1024 * 1024)) << " MB\n\n";

    refreshContainerList();
    std::cout << "\n";
    refreshImageList();
    std::cout << "\n";
}

void ContainerManagerWindow::printMenu() {
    std::cout << "┌──────────────────────────────────┐\n";
    std::cout << "│ c - 创建容器                     │\n";
    std::cout << "│ s - 启动容器 (输入序号, 如 s 1)  │\n";
    std::cout << "│ t - 停止容器 (输入序号, 如 t 1)  │\n";
    std::cout << "│ d - 删除容器 (输入序号, 如 d 1)  │\n";
    std::cout << "│ i - 查看容器详情 (输入序号)      │\n";
    std::cout << "│ p - 拉取镜像                     │\n";
    std::cout << "│ r - 手动刷新                     │\n";
    std::cout << "│ q - 退出                         │\n";
    std::cout << "└──────────────────────────────────┘\n";
    std::cout << "请输入选项: " << std::flush;
}

void ContainerManagerWindow::handleInput(const std::string& input) {
    if (input.empty()) return;

    char cmd = input[0];
    // 解析可能的参数 (序号)
    int arg = 0;
    if (input.length() > 1) {
        // 跳过空格, 解析数字
        size_t pos = 1;
        while (pos < input.length() && input[pos] == ' ') ++pos;
        if (pos < input.length()) {
            arg = std::atoi(input.c_str() + pos);
        }
    }

    switch (cmd) {
        case 'c':
        case 'C':
            showCreateDialog();
            break;
        case 's':
        case 'S': {
            if (arg <= 0) {
                std::cout << "用法: s <序号>\n";
                break;
            }
            std::string id = getContainerIdByIndex(arg);
            if (id.empty()) {
                std::cout << "无效的容器序号: " << arg << "\n";
            } else if (lifecycle_->startContainer(id)) {
                std::cout << "容器已启动: " << id << "\n";
                Logger::getInstance().info("容器已启动: " + id);
            } else {
                std::cout << "容器启动失败: " << id << "\n";
                Logger::getInstance().error("容器启动失败: " + id);
            }
            break;
        }
        case 't':
        case 'T': {
            if (arg <= 0) {
                std::cout << "用法: t <序号>\n";
                break;
            }
            std::string id = getContainerIdByIndex(arg);
            if (id.empty()) {
                std::cout << "无效的容器序号: " << arg << "\n";
            } else if (lifecycle_->stopContainer(id)) {
                std::cout << "容器已停止: " << id << "\n";
                Logger::getInstance().info("容器已停止: " + id);
            } else {
                std::cout << "容器停止失败: " << id << "\n";
                Logger::getInstance().error("容器停止失败: " + id);
            }
            break;
        }
        case 'd':
        case 'D': {
            if (arg <= 0) {
                std::cout << "用法: d <序号>\n";
                break;
            }
            std::string id = getContainerIdByIndex(arg);
            if (id.empty()) {
                std::cout << "无效的容器序号: " << arg << "\n";
            } else if (lifecycle_->destroyContainer(id)) {
                std::cout << "容器已删除: " << id << "\n";
                Logger::getInstance().info("容器已删除: " + id);
            } else {
                std::cout << "容器删除失败: " << id << "\n";
                Logger::getInstance().error("容器删除失败: " + id);
            }
            break;
        }
        case 'i':
        case 'I': {
            if (arg <= 0) {
                std::cout << "用法: i <序号>\n";
                break;
            }
            std::string id = getContainerIdByIndex(arg);
            if (id.empty()) {
                std::cout << "无效的容器序号: " << arg << "\n";
            } else {
                onContainerDoubleClicked(id);
            }
            break;
        }
        case 'p':
        case 'P': {
            std::string name, tag;
            std::cout << "镜像名称: ";
            std::getline(std::cin, name);
            std::cout << "镜像标签 (默认 latest): ";
            std::getline(std::cin, tag);
            if (tag.empty()) tag = "latest";
            if (image_mgr_->pullImage(name, tag)) {
                std::cout << "镜像拉取成功: " << name << ":" << tag << "\n";
                Logger::getInstance().info("镜像拉取成功: " + name + ":" + tag);
            } else {
                std::cout << "镜像拉取失败\n";
                Logger::getInstance().error("镜像拉取失败: " + name + ":" + tag);
            }
            break;
        }
        case 'r':
        case 'R':
            std::cout << "已手动刷新\n";
            break;
        case 'q':
        case 'Q':
            running_ = false;
            std::cout << "正在退出...\n";
            break;
        default:
            std::cout << "未知命令: " << cmd << "\n";
            break;
    }
}

int ContainerManagerWindow::exec() {
    running_ = true;

    while (running_) {
        // 打印主界面
        printDisplay();
        printMenu();

        // 使用 select() 实现带超时的输入读取
        // 超时后自动循环刷新, 实现定时刷新效果
        fd_set fds;
        struct timeval tv;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        tv.tv_sec = 3;   // 3 秒超时
        tv.tv_usec = 0;

        int ret = select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv);

        if (ret > 0 && FD_ISSET(STDIN_FILENO, &fds)) {
            // 有输入可读
            std::string line;
            if (std::getline(std::cin, line)) {
                handleInput(line);
            } else {
                // EOF (如 Ctrl+D)
                running_ = false;
            }
        }
        // ret == 0: 超时, 自动循环刷新
        // ret < 0: 错误, 也继续循环
    }

    std::cout << "\n容器管理器已退出.\n";
    return 0;
}

#endif // HAVE_QT
