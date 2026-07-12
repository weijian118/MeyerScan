#include "SendUI.h"

#include <QApplication>
#include <QByteArray>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QLineEdit>
#include <QList>
#include <QPushButton>
#include <QStringList>
#include <QTextEdit>
#include <QVector>
#include <QWidget>
#include <cstdio>

namespace {

// 发送页 smoke 的轻量断言函数。
//
// 这里不引入 QtTest，目的是让 VS2015、CMake 和部署目录都能直接运行同一个 EXE。
// 返回 bool 后由 main 分配稳定退出码，自动化脚本可以准确定位失败阶段。
bool Check(bool condition, const char* message) {
    if (condition) {
        std::printf("[PASS] %s\n", message);
        return true;
    }

    std::fprintf(stderr, "[FAIL] %s\n", message);
    return false;
}

// 保存 SendUI 实际上报的动作序列。
// QVector 只存在于测试进程内部，不会跨 DLL 边界；DLL 看到的仍然只是 void*。
struct ActionRecorder {
    QVector<int> actionIds;
};

// SendUI 通过纯 C 回调上报按钮动作。
// 测试记录 actionId 而不执行导出/上传，真实业务处理仍属于 MainExe 和后续服务模块。
void OnAction(void* context, int actionId) {
    auto* recorder = static_cast<ActionRecorder*>(context);
    if (recorder) {
        recorder->actionIds.append(actionId);
    }

    std::printf("[ACTION] %d\n", actionId);
}

}

// SendUITest.exe 入口。
// 默认执行不进入事件循环的 smoke；传入 --show 时才显示窗口供人工检查布局。
int main(int argc, char* argv[]) {
    // SendUI 创建 QWidget，因此必须使用 QApplication，不能替换为 QCoreApplication。
    QApplication app(argc, argv);

    // appDir 固定取测试 EXE 所在目录，不使用可能被第三方启动器改变的 currentPath。
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString logDir = QDir(appDir).filePath("logs");
    // mkpath 可重复调用；目录已存在时同样返回成功，不需要测试前清理历史日志。
    QDir().mkpath(logDir);
    // 公共 DLL 接口使用 UTF-8 const char*；命名 QByteArray 保证两个缓冲区覆盖整个 Init 调用。
    const QByteArray appDirBytes = appDir.toUtf8();
    const QByteArray logDirBytes = logDir.toUtf8();

    // 通过和 MainExe 相同的工厂函数取得 DLL 内单例，测试不直接构造实现类。
    ISendUI* send = GetSendUI();
    if (!Check(send != nullptr, "SendUI factory returned an instance")) {
        return 1;
    }

    if (!Check(send->Init(appDirBytes.constData(), logDirBytes.constData()), "SendUI initialized")) {
        return 2;
    }

    // 回调上下文由测试栈对象持有，其生命周期覆盖 SendUI 的全部点击测试。
    ActionRecorder recorder;
    send->SetActionCallback(&OnAction, &recorder);

    // 先写入有效标准上下文，再传入非法 JSON。
    // 第二次调用必须返回 false 且保留上一份有效上下文，避免内部对象和已显示控件状态分裂。
    const char* validContext =
        "{\"patient\":{\"name\":\"Send Test Patient\"},"
        "\"order\":{\"orderId\":\"SEND-001\",\"doctor\":\"Dr. Send\","
        "\"caseType\":\"restoration\",\"clinic\":\"Smoke Clinic\","
        "\"dataFormat\":\"PLY\",\"note\":\"Smoke note\"}}";
    if (!Check(send->SetSessionContextJson(validContext), "SendUI accepts valid session context")) {
        return 3;
    }
    if (!Check(!send->SetSessionContextJson("{invalid-json"), "SendUI rejects invalid session context")) {
        return 4;
    }

    // 创建真实页面并检查稳定 objectName；测试不依赖当前语言显示文字。
    QWidget* widget = send->CreateWidget();
    if (!Check(widget != nullptr, "SendUI created root QWidget")) {
        return 5;
    }

    if (!Check(widget->objectName() == "MeyerScanSendUIRoot", "SendUI root object name is correct")) {
        return 6;
    }

    // 通过稳定 objectName 定位字段，可同时证明非法 JSON 没有覆盖此前有效上下文。
    QLineEdit* patientEdit = widget->findChild<QLineEdit*>("SendUIPatientNameEdit");
    QComboBox* formatCombo = widget->findChild<QComboBox*>("SendUIDataFormatCombo");
    QTextEdit* noteEdit = widget->findChild<QTextEdit*>("SendUINoteEdit");
    if (!Check(patientEdit && patientEdit->text() == "Send Test Patient",
               "SendUI applies patient context to stable field")) {
        return 7;
    }
    if (!Check(formatCombo && formatCombo->currentText() == "PLY",
               "SendUI applies data format without emitting a user action")) {
        return 8;
    }
    if (!Check(noteEdit && noteEdit->toPlainText() == "Smoke note",
               "SendUI applies note context")) {
        return 9;
    }

    // 上下文程序性写入由 QSignalBlocker 屏蔽，不能伪造 DataFormatChanged 客户操作。
    if (!Check(recorder.actionIds.isEmpty(), "Context fill does not emit user actions")) {
        return 10;
    }

    // click() 同步触发 QPushButton::clicked，验证 UI -> 稳定 actionId -> 宿主回调完整链路。
    QPushButton* exportButton = widget->findChild<QPushButton*>("SendUIExportButton");
    QPushButton* finishButton = widget->findChild<QPushButton*>("SendUIFinishButton");
    if (!Check(exportButton && finishButton, "SendUI exposes stable action object names")) {
        return 11;
    }
    exportButton->click();
    finishButton->click();
    if (!Check(recorder.actionIds.contains(SendUIActionExport)
                   && recorder.actionIds.contains(SendUIActionFinish),
               "SendUI button clicks report stable action ids")) {
        return 12;
    }

    // 改变下拉框模拟真实客户选择，验证新增动作只在非阻断的交互路径上触发。
    formatCombo->setCurrentIndex(formatCombo->findText("STL"));
    if (!Check(recorder.actionIds.contains(SendUIActionDataFormatChanged),
               "SendUI reports data format changes")) {
        return 13;
    }

    // 人工模式进入 Qt 事件循环；自动 smoke 不 show，避免测试期间界面闪现。
    if (QCoreApplication::arguments().contains("--show")) {
        widget->resize(1360, 820);
        widget->show();
        return app.exec();
    }

    // QWidget 由测试宿主创建且没有父对象，因此测试负责 delete；随后 Shutdown 只清弱引用。
    delete widget;
    send->Shutdown();
    std::printf("SendUITest passed.\n");
    return 0;
}
