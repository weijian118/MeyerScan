#pragma once

#include <QWidget>

#ifdef MEYERSCAN_ORDERCREATEUI_EXPORTS
#  define MEYERSCAN_ORDERCREATEUI_API __declspec(dllexport)
#else
#  define MEYERSCAN_ORDERCREATEUI_API __declspec(dllimport)
#endif

// 建单界面向外抛出的用户动作 ID。
// 外部模块不要直接依赖按钮对象名，而是通过这些稳定 ID 接收用户操作。
enum OrderCreateActionId {
    OrderCreateActionConfirm = 1,
    OrderCreateActionCancel = 2,
    OrderCreateActionPrevious = 3,
    OrderCreateActionNext = 4,
    OrderCreateActionClearAllTeeth = 5,
    OrderCreateActionToothSelectionChanged = 6,
    OrderCreateActionScanProcessChanged = 7,
};

// 建单界面模块公开接口。
// 本模块只负责“建单界面展示和临时 UI 状态”，不负责数据库保存、订单规则加载或扫描流程控制。
class MEYERSCAN_ORDERCREATEUI_API IOrderCreateUI {
public:
    // 虚析构函数保证通过接口指针释放派生类时行为正确。
    virtual ~IOrderCreateUI() = default;

    // 初始化模块路径和日志。
    // appDirUtf8 必须是 MeyerScan.exe 所在目录，不能使用 currentPath 推导。
    // logDirUtf8 是统一日志目录，一般为 appDir/logs。
    virtual bool Init(const char* appDirUtf8, const char* logDirUtf8) = 0;

    // 创建建单界面根 QWidget。
    // 调用方可以把返回值嵌入 OrderScanWorkspaceShell 或其它全屏容器。
    virtual QWidget* CreateWidget(QWidget* parent = nullptr) = 0;

    // 设置用户动作回调。
    // 回调只传稳定动作 ID，不跨 DLL 传递 QWidget、QString 或复杂模型对象。
    virtual void SetActionCallback(void (*callback)(void* context, int actionId), void* context) = 0;

    // 设置建单上下文 JSON。
    // contextJsonUtf8 必须是 UTF-8 文本，字段来自手工建单、第三方拉起或 HIS/Worklist 适配后的统一结构。
    // 本接口只把患者、订单、扫描方案显示到界面，不保存数据库，也不决定后续扫描流程。
    virtual bool SetOrderContextJson(const char* contextJsonUtf8) = 0;

    // 返回模块版本字符串。
    virtual const char* GetModuleVersion() const = 0;

    // 关闭模块并清理缓存状态。
    virtual void Shutdown() = 0;

    // 获取当前建单页面生成的扫描流程 JSON。
    // 返回指针由 OrderCreateUI 模块内部缓存并维护生命周期，调用方必须立即复制使用。
    // JSON 中只包含 POD/字符串/数组，不跨 DLL 传递 QWidget、QString 或复杂对象。
    virtual const char* GetCurrentScanProcessJson() = 0;
};

// C ABI 工厂函数，便于 MainExe 或测试宿主静态/动态获取模块接口。
extern "C" MEYERSCAN_ORDERCREATEUI_API IOrderCreateUI* GetOrderCreateUI();

