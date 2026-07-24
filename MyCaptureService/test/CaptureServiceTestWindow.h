// =============================================================================
// 文件: CaptureServiceTestWindow.h
// 作用: 声明 CaptureService 的 Qt 人工联调和模拟测试窗口。
// =============================================================================
#pragma once

#include "CaptureServiceTestController.h"

#include <QWidget>

class CaptureImageView;
class QComboBox;
class QFrame;
class QLabel;
class QLibrary;
class QPushButton;
class QTextEdit;
class QTimer;
class IUIComponents;

class CaptureServiceTestWindow : public QWidget
{
public:
    // simulator 控制真实设备或模拟后端；simulatedFlags 只在模拟后端中生效，
    // 用于稳定复现超时、断连和部分包等硬件现场异常。
    explicit CaptureServiceTestWindow(bool simulator,
                                      std::uint32_t simulatedFlags = 0U,
                                      QWidget* parent = nullptr);
    ~CaptureServiceTestWindow() override;

    // 暴露给 --smoke 的控制器动作和状态检查。
    std::int32_t RunColorCalibrationPreflight();
    std::int32_t StartCaptureForTest();
    std::int32_t StopCaptureForTest();
    std::int32_t RequestLightForTest(bool on);
    bool HasProcessedImage() const;
    bool SmokeContractPassed() const;
    // 异常 smoke 通过这两个只读查询验证 UI 已消费事件且服务进入预期状态。
    bool HasSeenEvent(std::int32_t eventType) const;
    bool IsFaulted() const;

private:
    // 构建布局并连接 Qt 信号。
    void BuildUi();
    void ConnectActions();
    // 优先使用共享 UI DLL 创建控件，失败时退回 Qt 原生控件。
    QPushButton* CreateButton(const QString& text, bool primary, QWidget* parent);
    QComboBox* CreateComboBox(QWidget* parent);
    void LoadUiComponents();
    // 轮询 Service 事件、设备快照、组状态和图像。
    void PollService();
    void RefreshDeviceInfo();
    void RefreshCaptureState();
    void RefreshImage();
    void AppendEvent(const MeyerCaptureServiceEvent& eventInfo);
    QString EventText(std::int32_t eventType) const;
    QString ScanHeadText(std::int32_t scanHeadType) const;
    QString BooleanText(std::int32_t value) const;
    void UpdateActionAvailability();

    bool m_simulator;
    std::uint32_t m_simulatedFlags;
    bool m_configured;
    std::uint64_t m_displayedGroupSequence;
    std::vector<std::int32_t> m_seenEventTypes;
    CaptureServiceTestController m_controller;
    QLibrary* m_uiComponentsLibrary;
    IUIComponents* m_uiComponents;
    QTimer* m_pollTimer;

    QPushButton* m_colorAdmissionButton;
    QPushButton* m_calibration3DAdmissionButton;
    QPushButton* m_scanAdmissionButton;
    QPushButton* m_startButton;
    QPushButton* m_stopButton;
    QPushButton* m_lightOnButton;
    QPushButton* m_lightOffButton;
    QComboBox* m_imageSelector;
    CaptureImageView* m_imageView;
    QTextEdit* m_eventLog;
    QLabel* m_connectionValue;
    QLabel* m_usbValue;
    QLabel* m_seriesValue;
    QLabel* m_profileValue;
    QLabel* m_deviceIdValue;
    QLabel* m_deviceIdStatusValue;
    QLabel* m_modelValue;
    QLabel* m_firmwareValue;
    QLabel* m_productionValue;
    QLabel* m_captureStateValue;
    QLabel* m_ledValue;
    QLabel* m_scanHeadValue;
    QLabel* m_longPressValue;
    QLabel* m_groupSequenceValue;
    QLabel* m_packetCountValue;
    QLabel* m_pipelineRevisionValue;
    QLabel* m_pipelineUnavailableValue;
};
