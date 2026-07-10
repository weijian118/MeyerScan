#include "ScanReconstructStudio.h"

#include "ScanReconstructStudioWindow.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QPointer>
#include <QString>

namespace {
namespace ModuleInfo {
const char* Name = "MeyerScan_ScanReconstructStudio";
const char* Version = "MeyerScan_ScanReconstructStudio v0.1.2 (2026-07-10)";
}

class ScanReconstructStudioModule : public IScanReconstructStudio {
public:
    // 返回 DLL 内部单例。
    // 该模块只承载一个嵌入窗口，避免多个扫描重建壳同时抢占 VTK/OpenGL 资源。
    static ScanReconstructStudioModule& Instance() {
        static ScanReconstructStudioModule instance;
        return instance;
    }

    // 初始化运行路径。
    // 调用方必须优先传 MeyerScan.exe 所在目录；为空时才回退到当前 EXE 目录。
    bool Init(const char* appDirUtf8, const char* logDirUtf8) override {
        m_appDir = QString::fromUtf8(appDirUtf8 ? appDirUtf8 : "");
        m_logDir = QString::fromUtf8(logDirUtf8 ? logDirUtf8 : "");
        if (m_appDir.isEmpty()) {
            m_appDir = QCoreApplication::applicationDirPath();
        }
        if (m_logDir.isEmpty()) {
            m_logDir = QDir(m_appDir).filePath("logs");
        }
        QDir().mkpath(m_logDir);
        return true;
    }

    // 创建可嵌入窗口。
    // 每次创建前先释放旧窗口，保证同一进程内不会同时存在两个扫描重建壳。
    QWidget* CreateWidget(const char* contextJsonUtf8, QWidget* parent = nullptr) override {
        ReleaseWindow();
        const QByteArray contextJson(contextJsonUtf8 && contextJsonUtf8[0]
                                         ? contextJsonUtf8
                                         : "{\"source\":\"embedded\",\"sessionId\":\"scan-reconstruct\"}");
        m_window = new ScanReconstructStudioWindow(m_appDir, m_logDir, contextJson, parent);
        if (!m_window->Initialize()) {
            delete m_window.data();
            m_window = nullptr;
            return nullptr;
        }
        return m_window;
    }

    // 执行 DLL 形态烟测。
    // 使用栈上窗口可以验证加载/切换/释放，不污染模块单例中的 m_window。
    bool RunSmoke(const char* contextJsonUtf8) override {
        const QByteArray contextJson(contextJsonUtf8 && contextJsonUtf8[0]
                                         ? contextJsonUtf8
                                         : "{\"source\":\"dll-smoke\",\"sessionId\":\"scan-reconstruct-smoke\"}");
        ScanReconstructStudioWindow window(m_appDir, m_logDir, contextJson);
        return window.RunSmoke();
    }

    // 返回代码版本。
    // 版本清单通过 GetMeyerModuleVersion() 读取同一个字符串。
    const char* GetModuleVersion() const override {
        return ModuleInfo::Version;
    }

    // 释放模块当前持有的窗口。
    void Shutdown() override {
        ReleaseWindow();
    }

private:
    ScanReconstructStudioModule() = default;
    ~ScanReconstructStudioModule() override {
        ReleaseWindow();
    }
    ScanReconstructStudioModule(const ScanReconstructStudioModule&) = delete;
    ScanReconstructStudioModule& operator=(const ScanReconstructStudioModule&) = delete;

    // 释放当前窗口。
    // m_window 使用 QPointer：如果窗口已经被父 QWidget 的析构释放，QPointer 会自动变空，
    // 这里就不会再次 delete，避免嵌入 MainExe 时发生二次释放。
    void ReleaseWindow() {
        if (!m_window) {
            return;
        }
        m_window->close();
        delete m_window.data();
        m_window = nullptr;
    }

private:
    QString m_appDir;
    QString m_logDir;
    QPointer<ScanReconstructStudioWindow> m_window;
};
} // namespace

extern "C" MEYERSCAN_SCANRECONSTRUCTSTUDIO_API IScanReconstructStudio* GetScanReconstructStudio() {
    return &ScanReconstructStudioModule::Instance();
}

extern "C" MEYERSCAN_SCANRECONSTRUCTSTUDIO_API const char* GetMeyerModuleVersion() {
    return ModuleInfo::Version;
}
