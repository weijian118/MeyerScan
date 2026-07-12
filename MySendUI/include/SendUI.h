#pragma once

#include <QWidget>

#ifdef MEYERSCAN_SENDUI_EXPORTS
#  define MEYERSCAN_SENDUI_API __declspec(dllexport)
#else
#  define MEYERSCAN_SENDUI_API __declspec(dllimport)
#endif

// SendUI 向宿主上报的稳定动作编号。
//
// 动作编号只表达用户意图，不代表导出、压缩、邮件或上传已经执行成功。
// 新动作只能追加到枚举末尾，不能修改已有数值，否则旧版 MainExe 会误解回调含义。
enum SendUIActionId {
    SendUIActionPrevious = 1,
    SendUIActionExport = 2,
    SendUIActionCompress = 3,
    SendUIActionEmailSend = 4,
    SendUIActionUpload = 5,
    SendUIActionFinish = 6,
    SendUIActionDataFormatChanged = 7,
};

// 发送页面的公开接口。
//
// 模块边界：
// 1. 只显示会话上下文、收集轻量输入并上报动作；
// 2. 不直接访问数据库、文件打包、邮件服务或云端网络；
// 3. 跨 DLL 只传 UTF-8 字节、稳定整数和 QWidget 指针，不传 QJsonObject 所有权。
class MEYERSCAN_SENDUI_API ISendUI {
public:
    // 接口对象由 DLL 内部单例持有，调用方不能 delete，因此这里只保留虚析构保证多态定义完整。
    virtual ~ISendUI() = default;

    // 初始化应用目录和统一日志目录。
    // 两个参数都是调用期间有效的 UTF-8 字符串，模块内部必须立即复制，不能长期保存原始指针。
    virtual bool Init(const char* appDirUtf8, const char* logDirUtf8) = 0;

    // 创建将被 OrderScanWorkspaceShell 挂载的根 QWidget。
    // 返回对象交给 Qt 父子树管理；离开工作台前宿主应先调用 Shutdown 清理模块弱引用。
    virtual QWidget* CreateWidget(QWidget* parent = nullptr) = 0;

    // 接收 UTF-8 JSON 会话上下文并忽略未知字段。
    // 非法 JSON 返回 false，并保留上一份有效上下文，避免界面和内部状态不一致。
    virtual bool SetSessionContextJson(const char* contextJsonUtf8) = 0;

    // 注册纯 C 回调，上报 SendUIActionId。
    // callback/context 由宿主拥有，Shutdown 后模块不再保存或调用它们。
    virtual void SetActionCallback(void (*callback)(void* context, int actionId), void* context) = 0;

    // 返回供 versionList 记录的静态代码版本字符串，调用方不得释放或修改。
    virtual const char* GetModuleVersion() const = 0;

    // 清空控件弱引用、上下文和回调，不负责 delete 宿主持有的根 QWidget。
    virtual void Shutdown() = 0;
};

// C ABI 工厂函数由 MainExe 通过 QLibrary::resolve 获取，避免主程序链接 SendUI import lib。
extern "C" MEYERSCAN_SENDUI_API ISendUI* GetSendUI();
