// =============================================================================
// 文件: test/main.cpp
// 作用: 启动 CaptureServiceTest 窗口，并提供无硬件 --smoke 回归入口。
// =============================================================================
#include "CaptureServiceTestWindow.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QThread>

#include <cstdio>
#include <string>

namespace
{
    enum SmokeScenario
    {
        SmokeScenario_Normal,
        SmokeScenario_TimeoutOnce,
        SmokeScenario_TimeoutAlways,
        SmokeScenario_Disconnect,
        SmokeScenario_PartialPacket
    };

    // 冒烟模式用事件循环驱动 Qt 定时器和慢处理线程，避免直接 sleep 阻塞 UI。
    int RunSmoke(QApplication& application)
    {
        // processEvents 是 Qt 的静态成员；即使通过 application 对象调用，
        // VS2015 仍会把形参判为未使用，因此显式保留本地事件循环对象语义。
        (void)application;
        CaptureServiceTestWindow window(true, 0U);
        window.hide();
        if (window.RunColorCalibrationPreflight() != MeyerCaptureServiceResult_Ok)
        {
            std::printf("CaptureServiceTest smoke: preflight failed\n");
            return 2;
        }
        if (window.RequestLightForTest(true) != MeyerCaptureServiceResult_Ok ||
            window.StartCaptureForTest() != MeyerCaptureServiceResult_Ok)
        {
            std::printf("CaptureServiceTest smoke: start failed\n");
            return 3;
        }

        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < 5000 && !window.HasProcessedImage())
        {
            application.processEvents(QEventLoop::AllEvents, 25);
            QThread::msleep(10);
        }
        // 采集中排入两个无回包灯光命令，验证组六图边界命令窗口不会让
        // DeviceCmd 被 UI 线程并发调用；模拟器会在后续组边界消费它们。
        window.RequestLightForTest(false);
        window.RequestLightForTest(true);
        application.processEvents(QEventLoop::AllEvents, 50);
        const bool imageReady = window.HasProcessedImage();
        const int stopResult = window.StopCaptureForTest();
        const bool contract = window.SmokeContractPassed();
        std::printf("CaptureServiceTest smoke: image=%d stop=%d contract=%d\n",
                    imageReady ? 1 : 0, stopResult, contract ? 1 : 0);
        return imageReady && stopResult == MeyerCaptureServiceResult_Ok && contract
            ? 0 : 4;
    }

    // 异常 smoke 仍创建完整 Qt 窗口对象，只是隐藏显示；这样既验证
    // CaptureService，也覆盖界面轮询事件、按钮状态和析构释放链路。
    int RunFailureSmoke(QApplication& application, SmokeScenario scenario)
    {
        // 与正常 smoke 一样，application 负责维持 Qt 生命周期，事件泵调用本身
        // 是静态成员；显式引用可避免 VS2015 产生误导性的 C4100。
        (void)application;
        std::uint32_t flags = 0U;
        std::int32_t expectedEvent = MeyerCaptureServiceEvent_None;
        bool expectRecovery = false;
        switch (scenario)
        {
        case SmokeScenario_TimeoutOnce:
            flags = MeyerCaptureServiceSimulatedFlag_StreamTimeoutOnce;
            expectedEvent = MeyerCaptureServiceEvent_ReceiveTimeout;
            expectRecovery = true;
            break;
        case SmokeScenario_TimeoutAlways:
            flags = MeyerCaptureServiceSimulatedFlag_StreamTimeoutAlways;
            expectedEvent = MeyerCaptureServiceEvent_StreamStalled;
            break;
        case SmokeScenario_Disconnect:
            flags = MeyerCaptureServiceSimulatedFlag_DisconnectDuringCapture;
            expectedEvent = MeyerCaptureServiceEvent_DeviceDisconnected;
            break;
        case SmokeScenario_PartialPacket:
            flags = MeyerCaptureServiceSimulatedFlag_PartialStreamPacket;
            expectedEvent = MeyerCaptureServiceEvent_PartialPacket;
            expectRecovery = true;
            break;
        default:
            return RunSmoke(application);
        }

        CaptureServiceTestWindow window(true, flags);
        window.hide();
        if (window.RunColorCalibrationPreflight() != MeyerCaptureServiceResult_Ok ||
            window.StartCaptureForTest() != MeyerCaptureServiceResult_Ok)
        {
            std::printf("CaptureServiceTest failure smoke: setup failed\n");
            return 5;
        }

        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < 5000)
        {
            application.processEvents(QEventLoop::AllEvents, 25);
            const bool terminalState = expectRecovery
                ? window.HasProcessedImage()
                : window.IsFaulted();
            if (terminalState && window.HasSeenEvent(expectedEvent))
            {
                break;
            }
            QThread::msleep(10);
        }

        const bool eventSeen = window.HasSeenEvent(expectedEvent);
        const bool stateMatched = expectRecovery
            ? window.HasProcessedImage() && !window.IsFaulted()
            : window.IsFaulted();
        const int stopResult = window.StopCaptureForTest();
        std::printf(
            "CaptureServiceTest failure smoke: scenario=%d event=%d state=%d stop=%d\n",
            static_cast<int>(scenario), eventSeen ? 1 : 0,
            stateMatched ? 1 : 0, stopResult);
        return eventSeen && stateMatched &&
               stopResult == MeyerCaptureServiceResult_Ok ? 0 : 6;
    }
}

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    bool smoke = false;
    bool simulator = false;
    SmokeScenario smokeScenario = SmokeScenario_Normal;
    std::uint32_t simulatedFlags = 0U;
    for (int index = 1; index < argc; ++index)
    {
        const std::string argument(argv[index] == nullptr ? "" : argv[index]);
        if (argument == "--smoke")
        {
            smoke = true;
        }
        else if (argument == "--smoke-timeout-once")
        {
            smoke = true;
            smokeScenario = SmokeScenario_TimeoutOnce;
        }
        else if (argument == "--smoke-timeout-always")
        {
            smoke = true;
            smokeScenario = SmokeScenario_TimeoutAlways;
        }
        else if (argument == "--smoke-disconnect")
        {
            smoke = true;
            smokeScenario = SmokeScenario_Disconnect;
        }
        else if (argument == "--smoke-partial")
        {
            smoke = true;
            smokeScenario = SmokeScenario_PartialPacket;
        }
        else if (argument == "--simulate")
        {
            simulator = true;
        }
        else if (argument == "--simulate-timeout-once")
        {
            simulator = true;
            simulatedFlags |= MeyerCaptureServiceSimulatedFlag_StreamTimeoutOnce;
        }
        else if (argument == "--simulate-timeout-always")
        {
            simulator = true;
            simulatedFlags |= MeyerCaptureServiceSimulatedFlag_StreamTimeoutAlways;
        }
        else if (argument == "--simulate-disconnect")
        {
            simulator = true;
            simulatedFlags |= MeyerCaptureServiceSimulatedFlag_DisconnectDuringCapture;
        }
        else if (argument == "--simulate-partial")
        {
            simulator = true;
            simulatedFlags |= MeyerCaptureServiceSimulatedFlag_PartialStreamPacket;
        }
    }

    if (smoke)
    {
        return smokeScenario == SmokeScenario_Normal
            ? RunSmoke(application)
            : RunFailureSmoke(application, smokeScenario);
    }

    CaptureServiceTestWindow window(simulator, simulatedFlags);
    window.show();
    return application.exec();
}
