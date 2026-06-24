#include "LoginHost.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QTimer>

namespace {
// 测试阶段使用的口扫云登录地址。
// 后续 MainExe 会从配置中心或安装包配置中读取正式地址。
const char* kDefaultLoginUrl = "https://myscan.meyerop.com/login";
}

// 构造登录测试宿主。
// 这里直接连接登录 DLL 的信号，避免测试代码在 main() 中暴露 Qt 信号细节。
LoginHost::LoginHost(QObject* parent)
    : QObject(parent) {
    // 使用 Qt 信号槽连接登录模块返回值。
    // MainExe 集成时也必须接这个信号，否则登录成功后无法进入首页。
    connect(&m_loginWidget,
            &CBLMeyerLoginWidget::loginStatusReturn,
            this,
            &LoginHost::OnLoginStatusReturn);
}

// 显示登录窗口。
// initLoginWidgetAndShow 是登录模块对外给出的主入口，参数必须一次性传完整。
void LoginHost::Start() {
    // 参数通过函数单独构造，便于后续对比 MainExe 里真实集成参数。
    m_loginWidget.initLoginWidgetAndShow(BuildLoginParameters());
}

// 构建登录参数。
// 说明:
//   - applicationDirPath() 固定取 EXE 所在目录，不受第三方启动时 currentPath 影响。
//   - 缩放系数先使用 1.0，后续由 MainExe 根据屏幕和统一 UI 规则计算。
//   - languageType 先使用简体中文索引，登录模块内部会自动加载对应 qm 文件。
UserLoginParameters LoginHost::BuildLoginParameters() const {
    // applicationDirPath() 是测试 EXE 所在目录。
    // license.lic 和登录 DLL 依赖都由 PostBuild 复制到这里。
    const QString appDir = QCoreApplication::applicationDirPath();

    // UserLoginParameters 是外部登录模块定义的结构体。
    // 这里逐项赋值，避免 memset 破坏 QString 成员的构造状态。
    UserLoginParameters params;

    // 测试宿主先固定缩放系数为 1。MainExe 后续会根据统一 UI/DPI 策略计算。
    params.nfaktoW = 1.0;
    params.nfaktoH = 1.0;

    // 离线许可文件统一放在 Resources 目录，避免运行时依赖 D:\wj 开发路径。
    params.dataPath = QDir(appDir).filePath("Resources/license.lic");

    // 语言索引使用登录模块既有枚举，登录模块内部会加载对应 qm。
    params.languageType = G_LANG_SIMPLIFIED_CHINESE;

    // AppPath 必须是口扫主程序所在目录。测试宿主用自身目录模拟正式安装目录。
    params.AppPath = appDir;
    params.loginUrl = QString::fromUtf8(kDefaultLoginUrl);
    params.registerUrl = QString::fromUtf8(kDefaultLoginUrl);

    // version 当前只用于满足登录模块参数要求，正式值后续应由 MainExe/版本模块统一传入。
    params.version = 100;
    return params;
}

// 处理登录模块返回状态。
// 测试宿主不进入首页，所以只要登录流程出现明确结束状态，就延迟到事件循环空闲时退出。
// 使用 QTimer::singleShot(0) 是为了让登录模块先完成当前信号栈内的清理工作。
void LoginHost::OnLoginStatusReturn(const LoginReturnParameters& result) {
    // qDebug 只用于测试宿主人工观察，不作为正式客户操作日志。
    qDebug() << "Login status:" << result.currentStatus;

    // 测试宿主没有首页页面，登录成功/取消后都应退出，避免自动化测试卡住。
    if (result.currentStatus == LOGIN_SUCCESS ||
        result.currentStatus == WRITECLOUDMSG_SUCCESS ||
        result.currentStatus == USER_CANCEL_LOGIN) {
        QTimer::singleShot(0, qApp, SLOT(quit()));
    }
}
