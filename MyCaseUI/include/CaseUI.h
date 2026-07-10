#pragma once

#include <QWidget>

#ifdef MEYERSCAN_CASEUI_EXPORTS
#  define MEYERSCAN_CASEUI_API __declspec(dllexport)
#else
#  define MEYERSCAN_CASEUI_API __declspec(dllimport)
#endif

// ICaseUI 是案例管理 UI 模块的公共接口。
// 模块边界:
//   - 负责患者/订单列表界面、按钮和用户操作通知。
//   - 不直接实现数据库业务规则；后续正式读写患者/订单应通过 CaseOrderService。
//   - 不决定页面切换，点击“返回首页”等动作只通过回调通知 MainExe。
class MEYERSCAN_CASEUI_API ICaseUI {
public:
    // 虚析构函数用于保持跨 DLL 多态接口安全。
    virtual ~ICaseUI() = default;

    // 初始化案例管理 UI。
    // databaseConfigPath 仅向 RuntimeDataCenter 透传，用于读取患者/订单只读快照；
    // CaseUI 自身不连接 Database、不做数据库健康检查、不执行业务 SQL。
    virtual bool Init(const char* databaseConfigPath, const char* logDir) = 0;

    // 设置案例管理动作回调。
    // callback 由 MainExe 提供，UI 只负责把 actionId 通知出去。
    virtual void SetActionCallback(void (*callback)(void* context, int actionId), void* context) = 0;

    // 设置某个动作入口是否可见。
    // 用于权限/客户定制控制界面入口，不能替代业务层权限校验。
    virtual void SetActionVisible(int actionId, bool visible) = 0;

    // 设置某个动作入口是否可点击。
    // enabled=false 表示按钮保留但禁用，正式执行入口仍需要服务层/流程层复核。
    virtual void SetActionEnabled(int actionId, bool enabled) = 0;

    // 创建案例管理 QWidget。
    // 返回后由 MainExe 页面容器接管显示和销毁。
    virtual QWidget* CreateWidget(QWidget* parent = nullptr) = 0;

    // 返回模块版本字符串。
    virtual const char* GetModuleVersion() const = 0;

    // 关闭案例管理模块，释放模块缓存引用。
    virtual void Shutdown() = 0;
};

// 案例管理动作 ID。
// 数字分组:
//   1~99   页面切换和通用动作
//   100段  患者相关动作
//   200段  订单相关动作
enum CaseActionId {
    CaseActionBackHome = 1,       // 返回首页
    CaseActionSwitchTab = 2,      // 切换患者/订单页签
    CaseActionImportPatient = 101,// 导入患者
    CaseActionExportPatient = 102,// 导出患者
    CaseActionDeletePatient = 103,// 删除患者
    CaseActionNewPatient = 104,   // 新建患者
    CaseActionSearchPatient = 105,// 搜索患者
    CaseActionImportOrder = 201,  // 导入订单
    CaseActionExportOrder = 202,  // 导出订单
    CaseActionOpenOrder = 203,    // 打开订单
    CaseActionDeleteOrder = 204,  // 删除订单
    CaseActionSearchOrder = 205,  // 搜索订单
    CaseActionOpenSettings = 301, // 打开设置
    CaseActionMinimize = 302,     // 最小化 MeyerScan 主窗口
    CaseActionClose = 303,        // 关闭浏览页并返回首页
    CaseActionCloud = 304,        // 打开浏览页云端入口
    CaseActionScreenshot = 305,   // 请求宿主执行浏览页截图
};

// C ABI 工厂函数，便于 MainExe 直接或动态获取案例管理接口。
extern "C" MEYERSCAN_CASEUI_API ICaseUI* GetCaseUI();
