// =============================================================================
// 文件: CaptureServiceTestWindow.cpp
// 作用: 实现采集服务测试界面、共享控件接入和多分辨率布局。
// =============================================================================
#include "CaptureServiceTestWindow.h"

#include "CaptureImageView.h"

#include "../../Common/include/MeyerQtModuleUtils.h"
#include "../../MyUIComponents/include/UIComponents.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLibrary>
#include <QPushButton>
#include <QSizePolicy>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace
{
    typedef IUIComponents* (*GetUiComponentsFunction)();

    // 动态字符串仅用于显示设备返回值；固定界面文案全部由调用方 tr() 产生。
    QString ValueOrUnknown(const char* text)
    {
        return text != nullptr && text[0] != '\0'
            ? QString::fromUtf8(text)
            : QObject::tr("Unknown");
    }

    // 把按钮对象名集中命名，QSS 可以只依赖稳定对象名而不依赖业务变量名。
    void SetButtonObjectName(QPushButton* button, const char* name)
    {
        if (button != nullptr)
        {
            button->setObjectName(QString::fromLatin1(name));
        }
    }
}

CaptureServiceTestWindow::CaptureServiceTestWindow(
    bool simulator, std::uint32_t simulatedFlags, QWidget* parent)
    : QWidget(parent), m_simulator(simulator), m_simulatedFlags(simulatedFlags),
      m_configured(false),
      m_displayedGroupSequence(0U), m_uiComponentsLibrary(nullptr),
      m_uiComponents(nullptr), m_pollTimer(new QTimer(this)),
      m_colorAdmissionButton(nullptr), m_calibration3DAdmissionButton(nullptr),
      m_scanAdmissionButton(nullptr), m_startButton(nullptr), m_stopButton(nullptr),
      m_lightOnButton(nullptr), m_lightOffButton(nullptr), m_imageSelector(nullptr),
      m_imageView(nullptr), m_eventLog(nullptr), m_connectionValue(nullptr),
      m_usbValue(nullptr), m_seriesValue(nullptr), m_profileValue(nullptr),
      m_deviceIdValue(nullptr), m_deviceIdStatusValue(nullptr), m_modelValue(nullptr),
      m_firmwareValue(nullptr), m_productionValue(nullptr), m_captureStateValue(nullptr),
      m_ledValue(nullptr), m_scanHeadValue(nullptr), m_longPressValue(nullptr),
      m_groupSequenceValue(nullptr), m_packetCountValue(nullptr)
      , m_pipelineRevisionValue(nullptr), m_pipelineUnavailableValue(nullptr)
{
    setObjectName("MeyerScanCaptureServiceTestRoot");
    setWindowTitle(tr("Capture Service Test"));
    resize(1480, 900);
    setMinimumSize(1000, 680);
    LoadUiComponents();
    BuildUi();
    ConnectActions();
    m_pollTimer->setInterval(50);
    connect(m_pollTimer, &QTimer::timeout,
            this, &CaptureServiceTestWindow::PollService);
    m_pollTimer->start();
    MeyerQtModule::ApplyModuleQss(
        this, "MyCaptureService", "capture_service_test.qss", nullptr);

    // 模拟异常标志只传给测试后端；真实 DeviceTransport 会忽略该字段。
    const std::int32_t result = m_controller.Configure(
        m_simulator, m_simulatedFlags);
    m_configured = result == MeyerCaptureServiceResult_Ok;
    if (!m_configured)
    {
        m_eventLog->append(tr("Dependency configuration failed."));
        m_eventLog->append(QString::fromUtf8(m_controller.LastError().c_str()));
    }
    UpdateActionAvailability();
}

CaptureServiceTestWindow::~CaptureServiceTestWindow()
{
    // UIComponents 创建的 QWidget 仍然属于当前窗口。不能在 QWidget 基类
    // 析构前卸载 DLL，否则子控件销毁时可能跳到已经失效的虚函数表；QLibrary
    // 由 Qt 父对象在窗口子控件清理后统一释放，并且启用了 PreventUnloadHint。
    m_uiComponents = nullptr;
    m_uiComponentsLibrary = nullptr;
}

// 通过 QLibrary 使用程序目录中的共享控件 DLL，路径不依赖 currentPath。
void CaptureServiceTestWindow::LoadUiComponents()
{
    const QString path = QDir(QCoreApplication::applicationDirPath())
        .filePath("MeyerScan_UIComponents.dll");
    m_uiComponentsLibrary = new QLibrary(path, this);
    m_uiComponentsLibrary->setLoadHints(QLibrary::PreventUnloadHint);
    if (!m_uiComponentsLibrary->load())
    {
        delete m_uiComponentsLibrary;
        m_uiComponentsLibrary = nullptr;
        return;
    }
    GetUiComponentsFunction getComponents =
        reinterpret_cast<GetUiComponentsFunction>(
            m_uiComponentsLibrary->resolve("GetUIComponents"));
    if (getComponents == nullptr)
    {
        return;
    }
    m_uiComponents = getComponents();
    if (m_uiComponents != nullptr)
    {
        const QByteArray appDir =
            QCoreApplication::applicationDirPath().toUtf8();
        if (!m_uiComponents->Init(appDir.constData()))
        {
            m_uiComponents = nullptr;
        }
    }
}

QPushButton* CaptureServiceTestWindow::CreateButton(
    const QString& text, bool primary, QWidget* parent)
{
    QPushButton* button = nullptr;
    const QByteArray textBytes = text.toUtf8();
    if (m_uiComponents != nullptr)
    {
        button = m_uiComponents->CreateButton(
            primary ? MeyerButtonRolePrimary : MeyerButtonRoleSecondary,
            MeyerButtonContentTextOnly,
            textBytes.constData(), "", parent);
    }
    if (button == nullptr)
    {
        button = new QPushButton(text, parent);
    }
    button->setMinimumHeight(40);
    return button;
}

QComboBox* CaptureServiceTestWindow::CreateComboBox(QWidget* parent)
{
    QComboBox* combo = m_uiComponents != nullptr
        ? m_uiComponents->CreateComboBox(parent)
        : new QComboBox(parent);
    combo->setMinimumHeight(36);
    return combo;
}

// 使用 layout 而不是绝对坐标，窗口缩放和多语言文本变化不会覆盖相邻控件。
void CaptureServiceTestWindow::BuildUi()
{
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(18, 16, 18, 16);
    rootLayout->setSpacing(12);

    auto* titleRow = new QHBoxLayout();
    auto* title = new QLabel(tr("Capture Service Test"), this);
    title->setObjectName("CaptureServiceTitle");
    title->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    titleRow->addWidget(title);
    auto* mode = new QLabel(
        m_simulator ? tr("Simulator") : tr("Real Device"), this);
    mode->setObjectName("CaptureServiceMode");
    titleRow->addWidget(mode);
    rootLayout->addLayout(titleRow);

    auto* admissionRow = new QHBoxLayout();
    auto* admissionLabel = new QLabel(tr("Admission"), this);
    admissionLabel->setObjectName("SectionLabel");
    admissionRow->addWidget(admissionLabel);
    m_colorAdmissionButton = CreateButton(tr("Color Calibration"), true, this);
    m_calibration3DAdmissionButton = CreateButton(tr("3D Calibration"), false, this);
    m_scanAdmissionButton = CreateButton(tr("Scan Reconstruction"), false, this);
    SetButtonObjectName(m_colorAdmissionButton, "ColorAdmissionButton");
    SetButtonObjectName(m_calibration3DAdmissionButton, "ReservedAdmissionButton");
    SetButtonObjectName(m_scanAdmissionButton, "ReservedAdmissionButton");
    m_calibration3DAdmissionButton->setEnabled(false);
    m_scanAdmissionButton->setEnabled(false);
    m_calibration3DAdmissionButton->setToolTip(tr("Reserved for a later phase."));
    m_scanAdmissionButton->setToolTip(tr("Reserved for a later phase."));
    admissionRow->addWidget(m_colorAdmissionButton);
    admissionRow->addWidget(m_calibration3DAdmissionButton);
    admissionRow->addWidget(m_scanAdmissionButton);
    admissionRow->addStretch(1);
    rootLayout->addLayout(admissionRow);

    auto* body = new QHBoxLayout();
    body->setSpacing(12);

    auto* infoPanel = new QFrame(this);
    infoPanel->setObjectName("CaptureInfoPanel");
    auto* infoLayout = new QFormLayout(infoPanel);
    infoLayout->setContentsMargins(14, 14, 14, 14);
    infoLayout->setHorizontalSpacing(12);
    infoLayout->setVerticalSpacing(10);
    auto addInfo = [infoLayout, infoPanel](const QString& label, QLabel*& value) {
        auto* key = new QLabel(label, infoPanel);
        value = new QLabel(QObject::tr("Unknown"), infoPanel);
        value->setObjectName("InfoValue");
        value->setWordWrap(true);
        infoLayout->addRow(key, value);
    };
    addInfo(tr("Connection"), m_connectionValue);
    addInfo(tr("USB"), m_usbValue);
    addInfo(tr("Device Series"), m_seriesValue);
    addInfo(tr("Device Profile"), m_profileValue);
    addInfo(tr("Device ID"), m_deviceIdValue);
    addInfo(tr("ID Status"), m_deviceIdStatusValue);
    addInfo(tr("Product Model"), m_modelValue);
    addInfo(tr("Firmware"), m_firmwareValue);
    addInfo(tr("Production Mode"), m_productionValue);
    infoPanel->setMinimumWidth(280);
    infoPanel->setMaximumWidth(360);
    body->addWidget(infoPanel, 0);

    auto* center = new QFrame(this);
    center->setObjectName("CaptureCenterPanel");
    auto* centerLayout = new QVBoxLayout(center);
    centerLayout->setContentsMargins(14, 14, 14, 14);
    auto* selectorRow = new QHBoxLayout();
    auto* selectorLabel = new QLabel(tr("Display"), center);
    m_imageSelector = CreateComboBox(center);
    m_imageSelector->addItem(tr("0"));
    m_imageSelector->addItem(tr("1"));
    m_imageSelector->addItem(tr("2"));
    m_imageSelector->addItem(tr("3"));
    m_imageSelector->addItem(tr("4"));
    m_imageSelector->addItem(tr("5"));
    m_imageSelector->addItem(tr("6"));
    m_imageSelector->addItem(tr("Color"));
    selectorRow->addWidget(selectorLabel);
    selectorRow->addWidget(m_imageSelector, 1);
    centerLayout->addLayout(selectorRow);
    m_imageView = new CaptureImageView(center);
    m_imageView->SetMessage(tr("No processed image yet."));
    centerLayout->addWidget(m_imageView, 1);
    body->addWidget(center, 1);

    auto* statusPanel = new QFrame(this);
    statusPanel->setObjectName("CaptureStatusPanel");
    auto* statusLayout = new QVBoxLayout(statusPanel);
    statusLayout->setContentsMargins(14, 14, 14, 14);
    auto* statusTitle = new QLabel(tr("Capture Group Status"), statusPanel);
    statusTitle->setObjectName("SectionLabel");
    statusLayout->addWidget(statusTitle);
    auto* statusForm = new QFormLayout();
    auto addStatus = [statusForm, statusPanel](const QString& label, QLabel*& value) {
        auto* key = new QLabel(label, statusPanel);
        value = new QLabel(QObject::tr("Unknown"), statusPanel);
        value->setObjectName("InfoValue");
        statusForm->addRow(key, value);
    };
    addStatus(tr("Capture State"), m_captureStateValue);
    addStatus(tr("LED"), m_ledValue);
    addStatus(tr("Scan Head"), m_scanHeadValue);
    addStatus(tr("Long Press"), m_longPressValue);
    addStatus(tr("Group"), m_groupSequenceValue);
    addStatus(tr("Packets"), m_packetCountValue);
    addStatus(tr("Pipeline Revision"), m_pipelineRevisionValue);
    addStatus(tr("Unavailable Features"), m_pipelineUnavailableValue);
    statusLayout->addLayout(statusForm);
    auto* eventTitle = new QLabel(tr("Events"), statusPanel);
    eventTitle->setObjectName("SectionLabel");
    statusLayout->addWidget(eventTitle);
    m_eventLog = new QTextEdit(statusPanel);
    m_eventLog->setObjectName("CaptureEventLog");
    m_eventLog->setReadOnly(true);
    m_eventLog->setMinimumHeight(180);
    statusLayout->addWidget(m_eventLog, 1);
    statusPanel->setMinimumWidth(300);
    statusPanel->setMaximumWidth(390);
    body->addWidget(statusPanel, 0);
    rootLayout->addLayout(body, 1);

    auto* controls = new QHBoxLayout();
    m_startButton = CreateButton(tr("Start Capture"), true, this);
    m_stopButton = CreateButton(tr("Stop Capture"), false, this);
    m_lightOnButton = CreateButton(tr("Light On"), false, this);
    m_lightOffButton = CreateButton(tr("Light Off"), false, this);
    SetButtonObjectName(m_startButton, "StartCaptureButton");
    SetButtonObjectName(m_stopButton, "StopCaptureButton");
    SetButtonObjectName(m_lightOnButton, "LightButton");
    SetButtonObjectName(m_lightOffButton, "LightButton");
    controls->addWidget(m_startButton);
    controls->addWidget(m_stopButton);
    controls->addSpacing(12);
    controls->addWidget(m_lightOnButton);
    controls->addWidget(m_lightOffButton);
    controls->addStretch(1);
    rootLayout->addLayout(controls);
}

void CaptureServiceTestWindow::ConnectActions()
{
    connect(m_colorAdmissionButton, &QPushButton::clicked, this, [this]() {
        RunColorCalibrationPreflight();
    });
    connect(m_startButton, &QPushButton::clicked, this, [this]() {
        StartCaptureForTest();
    });
    connect(m_stopButton, &QPushButton::clicked, this, [this]() {
        StopCaptureForTest();
    });
    connect(m_lightOnButton, &QPushButton::clicked, this, [this]() {
        RequestLightForTest(true);
    });
    connect(m_lightOffButton, &QPushButton::clicked, this, [this]() {
        RequestLightForTest(false);
    });
    connect(m_imageSelector,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, [this](int) { RefreshImage(); });
}

std::int32_t CaptureServiceTestWindow::RunColorCalibrationPreflight()
{
    const std::int32_t result = m_controller.PrepareColorCalibration();
    RefreshDeviceInfo();
    RefreshCaptureState();
    UpdateActionAvailability();
    return result;
}

std::int32_t CaptureServiceTestWindow::StartCaptureForTest()
{
    const std::int32_t result = m_controller.StartCapture();
    RefreshCaptureState();
    UpdateActionAvailability();
    return result;
}

std::int32_t CaptureServiceTestWindow::StopCaptureForTest()
{
    const std::int32_t result = m_controller.StopCapture();
    RefreshCaptureState();
    UpdateActionAvailability();
    return result;
}

std::int32_t CaptureServiceTestWindow::RequestLightForTest(bool on)
{
    return m_controller.RequestLight(on);
}

void CaptureServiceTestWindow::PollService()
{
    MeyerCaptureServiceEvent eventInfo = {};
    while (m_controller.PollEvent(eventInfo))
    {
        AppendEvent(eventInfo);
    }
    RefreshDeviceInfo();
    RefreshCaptureState();
    MeyerCaptureServiceStateSnapshot state = {};
    if (m_controller.ReadState(state) &&
        state.latestDataAvailable != 0 &&
        state.latestGroupSequence != m_displayedGroupSequence)
    {
        m_displayedGroupSequence = state.latestGroupSequence;
        RefreshImage();
    }
    UpdateActionAvailability();
}

void CaptureServiceTestWindow::RefreshDeviceInfo()
{
    MeyerCaptureServiceDeviceInfo info = {};
    if (!m_controller.ReadDeviceInfo(info))
    {
        return;
    }
    m_connectionValue->setText(info.deviceConnected != 0
        ? tr("Connected") : tr("Not connected"));
    m_usbValue->setText(info.deviceConnected == 0
        ? tr("Unknown") : (info.usb2 != 0 ? tr("USB 2.0") : tr("USB 3.0")));
    m_seriesValue->setText(ValueOrUnknown(info.deviceSeriesUtf8));
    m_profileValue->setText(ValueOrUnknown(info.deviceProfileUtf8));
    m_deviceIdValue->setText(ValueOrUnknown(info.reportedDeviceIdUtf8));
    m_deviceIdStatusValue->setText(ValueOrUnknown(info.deviceIdStatusUtf8));
    m_modelValue->setText(ValueOrUnknown(info.productModelUtf8));
    const QString mainVersion = ValueOrUnknown(info.mainFirmwareVersionUtf8);
    const QString projectionVersion = ValueOrUnknown(info.projectionFirmwareVersionUtf8);
    m_firmwareValue->setText(projectionVersion == tr("Unknown")
        ? mainVersion : mainVersion + tr(" / Projection ") + projectionVersion);
    m_productionValue->setText(BooleanText(info.productionMode));
}

void CaptureServiceTestWindow::RefreshCaptureState()
{
    MeyerCaptureServiceStateSnapshot state = {};
    if (!m_controller.ReadState(state))
    {
        return;
    }
    switch (state.state)
    {
    case MeyerCaptureServiceState_Running:
        m_captureStateValue->setText(tr("Running")); break;
    case MeyerCaptureServiceState_Stopping:
        m_captureStateValue->setText(tr("Stopping")); break;
    case MeyerCaptureServiceState_Faulted:
        m_captureStateValue->setText(tr("Faulted")); break;
    case MeyerCaptureServiceState_Ready:
        m_captureStateValue->setText(tr("Ready")); break;
    case MeyerCaptureServiceState_Configured:
        m_captureStateValue->setText(tr("Configured")); break;
    default:
        m_captureStateValue->setText(tr("Created")); break;
    }
    m_ledValue->setText(BooleanText(state.latestLedOn));
    m_longPressValue->setText(BooleanText(state.latestLongPressed));
    m_scanHeadValue->setText(ScanHeadText(state.latestScanHeadType));
    m_groupSequenceValue->setText(QString::number(
        static_cast<qulonglong>(state.latestGroupSequence)));
    m_packetCountValue->setText(QString::number(
        static_cast<qulonglong>(state.totalPackets)));
    m_pipelineRevisionValue->setText(QString::number(
        state.latestPipelineOptionsRevision));
    m_pipelineUnavailableValue->setText(QString("0x%1").arg(
        static_cast<qulonglong>(state.latestPipelineUnavailableFeatures),
        0, 16));
}

void CaptureServiceTestWindow::RefreshImage()
{
    if (m_imageSelector == nullptr || m_imageView == nullptr)
    {
        return;
    }
    const int index = m_imageSelector->currentIndex();
    std::vector<unsigned char> bytes;
    MeyerCaptureServiceStateSnapshot state = {};
    if (!m_controller.ReadState(state) || state.latestDataAvailable == 0)
    {
        m_imageView->SetMessage(tr("No processed image yet."));
        return;
    }
    const int width = 1024;
    const int height = 455;
    if (index == 7)
    {
        if (m_controller.CopyRgb888(bytes) != MeyerCaptureServiceResult_Ok ||
            bytes.size() != static_cast<std::size_t>(width * height * 3))
        {
            m_imageView->SetMessage(tr("Color image is unavailable."));
            return;
        }
        const QImage image(&bytes[0], width, height, width * 3,
                           QImage::Format_RGB888);
        m_imageView->SetImage(image);
        return;
    }
    if (index == 6)
    {
        m_imageView->SetMessage(tr("Image index 6 is reserved."));
        return;
    }
    if (m_controller.CopyPlane(index, bytes) != MeyerCaptureServiceResult_Ok ||
        bytes.size() != static_cast<std::size_t>(width * height))
    {
        m_imageView->SetMessage(tr("Selected image is unavailable."));
        return;
    }
    const QImage image(&bytes[0], width, height, width,
                       QImage::Format_Indexed8);
    QImage display = image.copy();
    QVector<QRgb> colors(256);
    for (int color = 0; color < colors.size(); ++color)
    {
        colors[color] = qRgb(color, color, color);
    }
    display.setColorTable(colors);
    m_imageView->SetImage(display);
}

void CaptureServiceTestWindow::AppendEvent(
    const MeyerCaptureServiceEvent& eventInfo)
{
    // 保存结构化类型供无硬件回归断言。界面文本仍由 tr() 映射，不能依赖
    // 下层 DLL 的英文诊断字符串来判断业务分支。
    m_seenEventTypes.push_back(eventInfo.type);
    const QString text = EventText(eventInfo.type);
    const QString line = QString("[%1] %2 #%3")
        .arg(eventInfo.severity == MeyerCaptureServiceEventSeverity_Error
                 ? tr("Error")
                 : (eventInfo.severity == MeyerCaptureServiceEventSeverity_Warning
                        ? tr("Warning") : tr("Info")))
        .arg(text)
        .arg(static_cast<qulonglong>(eventInfo.groupSequence));
    m_eventLog->append(line);
}

QString CaptureServiceTestWindow::EventText(std::int32_t eventType) const
{
    switch (eventType)
    {
    case MeyerCaptureServiceEvent_ModuleLoaded: return tr("Module loaded");
    case MeyerCaptureServiceEvent_PreflightStarted: return tr("Preflight started");
    case MeyerCaptureServiceEvent_PreflightReady: return tr("Preflight ready");
    case MeyerCaptureServiceEvent_PreflightRejected: return tr("Preflight rejected");
    case MeyerCaptureServiceEvent_CaptureStarted: return tr("Capture started");
    case MeyerCaptureServiceEvent_CaptureStopped: return tr("Capture stopped");
    case MeyerCaptureServiceEvent_DeviceDisconnected: return tr("Device disconnected");
    case MeyerCaptureServiceEvent_ReceiveTimeout: return tr("Receive timeout");
    case MeyerCaptureServiceEvent_StreamStalled: return tr("Stream stalled");
    case MeyerCaptureServiceEvent_PartialPacket: return tr("Partial packet");
    case MeyerCaptureServiceEvent_InvalidPacket: return tr("Invalid packet");
    case MeyerCaptureServiceEvent_SequenceReset: return tr("Image sequence reset");
    case MeyerCaptureServiceEvent_ProcessingError: return tr("Processing error");
    case MeyerCaptureServiceEvent_PostProcessDropped: return tr("Post-process group dropped");
    case MeyerCaptureServiceEvent_GroupReady: return tr("Decrypted group ready");
    case MeyerCaptureServiceEvent_GroupProcessed: return tr("Group processed");
    case MeyerCaptureServiceEvent_LightCommandQueued: return tr("Light command queued");
    case MeyerCaptureServiceEvent_LightCommandSent: return tr("Light command sent");
    case MeyerCaptureServiceEvent_CommandFailed: return tr("Command failed");
    case MeyerCaptureServiceEvent_AutoExposureSkipped: return tr("Auto exposure skipped");
    case MeyerCaptureServiceEvent_AutoExposureReserved: return tr("Auto exposure reserved");
    case MeyerCaptureServiceEvent_StateChanged: return tr("State changed");
    case MeyerCaptureServiceEvent_ImagePipelineReady:
        return tr("Image pipeline outputs ready");
    case MeyerCaptureServiceEvent_ImagePipelineFeatureUnavailable:
        return tr("Image pipeline feature unavailable");
    default: return tr("Capture service event");
    }
}

QString CaptureServiceTestWindow::ScanHeadText(std::int32_t scanHeadType) const
{
    switch (scanHeadType)
    {
    case MeyerCaptureScanHead_Large: return tr("Large");
    case MeyerCaptureScanHead_Small: return tr("Small");
    case MeyerCaptureScanHead_NotInserted: return tr("Not inserted");
    default: return tr("Unknown");
    }
}

QString CaptureServiceTestWindow::BooleanText(std::int32_t value) const
{
    return value != 0 ? tr("Yes") : tr("No");
}

void CaptureServiceTestWindow::UpdateActionAvailability()
{
    MeyerCaptureServiceStateSnapshot state = {};
    const bool hasState = m_controller.ReadState(state);
    const bool ready = hasState &&
        (state.state == MeyerCaptureServiceState_Ready ||
         state.state == MeyerCaptureServiceState_Running);
    const bool running = hasState && state.state == MeyerCaptureServiceState_Running;
    m_colorAdmissionButton->setEnabled(m_configured && !running);
    m_startButton->setEnabled(ready && !running);
    m_stopButton->setEnabled(running);
    m_lightOnButton->setEnabled(ready);
    m_lightOffButton->setEnabled(ready);
}

bool CaptureServiceTestWindow::HasProcessedImage() const
{
    MeyerCaptureServiceStateSnapshot state = {};
    return m_controller.ReadState(state) && state.latestDataAvailable != 0;
}

bool CaptureServiceTestWindow::SmokeContractPassed() const
{
    MeyerCaptureServiceDeviceInfo info = {};
    MeyerCaptureServiceStateSnapshot state = {};
    MeyerCapturePipelineOutputInfo outputInfo = {};
    return HasProcessedImage() && m_controller.ReadDeviceInfo(info) &&
           m_controller.ReadState(state) && info.deviceConnected != 0 &&
           info.deviceProfile != 0 && state.latestGroupSequence != 0U &&
           m_controller.ReadPipelineOutputInfo(
               MeyerCapturePipelineOutput_DisplayRgb888, outputInfo) &&
           outputInfo.groupSequence == state.latestGroupSequence;
}

bool CaptureServiceTestWindow::HasSeenEvent(std::int32_t eventType) const
{
    return std::find(m_seenEventTypes.begin(), m_seenEventTypes.end(), eventType) !=
           m_seenEventTypes.end();
}

bool CaptureServiceTestWindow::IsFaulted() const
{
    MeyerCaptureServiceStateSnapshot state = {};
    return m_controller.ReadState(state) &&
           state.state == MeyerCaptureServiceState_Faulted;
}
