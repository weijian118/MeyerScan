#pragma once

#include <QWidget>

#ifdef MEYERSCAN_DATAPROCESSUI_EXPORTS
#  define MEYERSCAN_DATAPROCESSUI_API __declspec(dllexport)
#else
#  define MEYERSCAN_DATAPROCESSUI_API __declspec(dllimport)
#endif

// 数据处理阶段向宿主上报的稳定动作编号。
//
// 编号只表达用户意图，截图、编辑、颈缘线、倒凹和测量算法仍由后续业务 DLL 执行。
// 新动作只能追加，不能修改已有数值。
enum DataProcessActionId {
    DataProcessActionPrevious = 1,
    DataProcessActionNext = 2,
    DataProcessActionScreenshot = 3,
    DataProcessActionEdit = 4,
    DataProcessActionMargin = 5,
    DataProcessActionUndercut = 6,
    DataProcessActionColor = 7,
    DataProcessActionMeasure = 8,
};

// 数据处理阶段 UI 的公开接口。
//
// 本模块负责页面、交互入口和 QVTK/OpenGL 显示生命周期，不直接实现后处理算法、
// 数据 IO、病例保存或扫描流程生成，只消费宿主传入的 scanProcess.steps。
class MEYERSCAN_DATAPROCESSUI_API IDataProcessUI {
public:
    // 接口对象由 DLL 内单例持有，调用方不得 delete。
    virtual ~IDataProcessUI() = default;

    // 初始化应用目录和统一日志目录，并立即复制 UTF-8 参数。
    virtual bool Init(const char* appDirUtf8, const char* logDirUtf8) = 0;

    // 创建处理阶段根 QWidget；宿主挂载完成后再显式调用 Activate。
    virtual QWidget* CreateWidget(QWidget* parent = nullptr) = 0;

    // 校验并保存轻量订单/会话 JSON；失败时不覆盖上一份有效上下文。
    virtual bool SetSessionContextJson(const char* contextJsonUtf8) = 0;

    // 注册只传 void* 和稳定 actionId 的纯 C 回调。
    virtual void SetActionCallback(void (*callback)(void* context, int actionId), void* context) = 0;

    // 页面成为当前步骤后调用，刷新当前模型部位的显示状态。
    virtual void Activate() = 0;

    // 离开处理页前释放 QVTK/OpenGL 重资源，不能只隐藏 QWidget。
    virtual void DeactivateAndRelease() = 0;

    // 返回供 versionList 记录的静态代码版本字符串。
    virtual const char* GetModuleVersion() const = 0;

    // 清理模块缓存、回调和重资源，不跨 DLL 删除宿主根 QWidget。
    virtual void Shutdown() = 0;
};

// C ABI 工厂由 MainExe 或 ScanReconstructStudio.exe 动态解析。
extern "C" MEYERSCAN_DATAPROCESSUI_API IDataProcessUI* GetDataProcessUI();
