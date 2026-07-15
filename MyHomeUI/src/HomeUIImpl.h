#pragma once

#include "HomeUI.h"
#include "Logger.h"
#include "UIComponents.h"
#include <QCoreApplication>
#include <QLibrary>
#include <QString>

using GetLoggerFunc = ILogger* (*)();
using GetUIComponentsFunc = IUIComponents* (*)();

// HomeUIImpl 是首页模块的唯一实现类。
// 首页只负责展示入口和上报入口 ID，不保存订单、不判断流程、不直接启动扫描。
class HomeUIImpl : public IHomeUI {
    Q_DECLARE_TR_FUNCTIONS(HomeUI)

public:
    // 返回进程内单例，保证首页模块状态集中管理。
    static HomeUIImpl& Instance();

    // 初始化首页模块需要的日志和共享 UI。
    bool Init(const char* appDirUtf8, const char* logDir) override;

    // 注册入口点击回调，由 MainExe 统一决定页面切换和后续流程。
    void SetEntryCallback(void (*callback)(void* context, int entryId), void* context) override;

    // 设置某个入口是否显示，最终结果由 MainExe 合并配置和权限后传入。
    void SetEntryVisible(int entryId, bool visible) override;

    // 设置某个入口是否可点击，最终结果由 MainExe 合并权限后传入。
    void SetEntryEnabled(int entryId, bool enabled) override;

    // 创建首页 QWidget；调用方负责把它放入主窗口容器并管理生命周期。
    QWidget* CreateWidget(QWidget* parent = nullptr) override;

    // 返回模块版本字符串，用于版本清单和现场排查。
    const char* GetModuleVersion() const override;

    // 释放本模块持有的轻量引用；不关闭进程级 Logger 单例。
    void Shutdown() override;

    // 返回最后一次初始化状态文本，供界面状态栏和测试输出使用。
    QString LastStatus() const { return m_lastStatus; }

private:
    // MeyerScan.exe 所在目录，用于按绝对路径加载模块依赖。
    QString m_appDir;

    HomeUIImpl() = default;
    ~HomeUIImpl() = default;
    HomeUIImpl(const HomeUIImpl&) = delete;
    HomeUIImpl& operator=(const HomeUIImpl&) = delete;

    // 写结构化日志；日志对象未初始化时静默返回，避免 UI 因日志失败崩溃。
    void WriteLog(LogLevel level, const char* operation, const QString& content);

    // 动态加载 Logger，并把 ILogger 指针缓存到成员变量。
    void LoadLogger(const char* logDir);

    // 动态加载 UIComponents，并把 IUIComponents 指针缓存到成员变量。
    void LoadUIComponents();

    // 入口按钮点击后的统一上报函数。
    void NotifyEntryClicked(int entryId);

    // 判断入口当前是否显示，非法 ID 默认显示，避免误伤新增入口。
    bool IsEntryVisible(int entryId) const;

    // 判断入口当前是否可点击，非法 ID 默认可用，避免误伤新增入口。
    bool IsEntryEnabled(int entryId) const;

private:
    // Logger DLL 句柄使用 PreventUnloadHint，避免进程退出时卸载顺序问题。
    QLibrary m_loggerLibrary;

    // UIComponents DLL 句柄；首页只借用共享控件工厂，不拥有其生命周期。
    QLibrary m_uiComponentsLibrary;

    // 缓存后的日志接口指针；模块内部后续写日志都复用该变量。
    ILogger* m_logger = nullptr;

    // 缓存后的共享 UI 接口；不可用时首页降级为本地 QPushButton。
    IUIComponents* m_uiComponents = nullptr;

    // 最近一次状态文本，显示在首页底部，也便于测试宿主检查。
    QString m_lastStatus = "Not initialized";

    // MainExe 注册的入口回调和上下文指针。
    void (*m_entryCallback)(void* context, int entryId) = nullptr;
    void* m_entryCallbackContext = nullptr;

    // 入口显隐数组；下标按 HomeEntry 枚举使用，0 号位保留。
    bool m_entryVisible[5] = { true, true, true, true, true };

    // 入口启用数组；下标按 HomeEntry 枚举使用，0 号位保留。
    bool m_entryEnabled[5] = { true, true, true, true, true };
};
