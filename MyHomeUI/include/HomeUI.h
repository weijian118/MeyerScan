#pragma once

#include <QWidget>

#ifdef MEYERSCAN_HOMEUI_EXPORTS
#  define MEYERSCAN_HOMEUI_API __declspec(dllexport)
#else
#  define MEYERSCAN_HOMEUI_API __declspec(dllimport)
#endif

// IHomeUI 是首页 UI 模块的公共接口。
// 模块边界:
//   - 首页只负责展示入口按钮、输出用户点击日志、通过回调通知 MainExe。
//   - 首页不决定权限、不直接跳转到其他模块、不承载建单/浏览/设置的业务逻辑。
//   - 入口显隐和启用态由 MainExe 读取 ConfigCenter/Permission 后传入。
class MEYERSCAN_HOMEUI_API IHomeUI {
public:
    // 虚析构函数用于保持跨 DLL 多态接口安全。
    virtual ~IHomeUI() = default;

    // 初始化首页模块。
    // databaseConfigPath 当前用于连通数据库基础链路，logDir 是统一日志目录。
    virtual bool Init(const char* databaseConfigPath, const char* logDir) = 0;

    // 设置首页入口点击回调。
    // callback 由 MainExe 提供；context 一般传 MainWindow 指针，避免 UI 模块直接依赖 MainExe 类型。
    virtual void SetEntryCallback(void (*callback)(void* context, int entryId), void* context) = 0;

    // 设置某个入口是否可见。
    // 只控制显示，不代表最终动作授权；关键动作仍需要上层或服务层二次校验。
    virtual void SetEntryVisible(int entryId, bool visible) = 0;

    // 设置某个入口是否可点击。
    // visible=false 表示入口不显示；enabled=false 表示入口显示但禁用，便于后续提示原因。
    virtual void SetEntryEnabled(int entryId, bool enabled) = 0;

    // 创建首页 QWidget。
    // 调用方负责把返回的页面挂入 MainExe 的页面容器。
    virtual QWidget* CreateWidget(QWidget* parent = nullptr) = 0;

    // 返回模块版本字符串。
    virtual const char* GetModuleVersion() const = 0;

    // 关闭首页模块并清理缓存状态。
    virtual void Shutdown() = 0;
};

// 首页入口 ID。
// 回调中只传递这些稳定 ID，避免跨 DLL 传递 QAction/QPushButton 指针。
enum HomeEntryId {
    HomeEntryCreate = 1,    // 建单入口
    HomeEntryBrowse = 2,    // 案例管理/浏览入口
    HomeEntryPractice = 3,  // 练习入口
    HomeEntrySettings = 4,  // 设置入口

    // 900 段用于首页顶部工具和窗口动作，避免与四个业务入口 ID 混淆。
    // HomeUI 仍只上报稳定整数，真正的窗口/页面操作由 MainExe 完成。
    HomeActionMinimize = 901,
    HomeActionClose = 902,
    HomeActionCalibration = 903,
    HomeActionCloud = 904,
    HomeActionHelp = 905,
};

// C ABI 工厂函数。
// 后续 MainExe 可直接链接 lib，也可以用 LoadLibrary/GetProcAddress 动态获取。
extern "C" MEYERSCAN_HOMEUI_API IHomeUI* GetHomeUI();
