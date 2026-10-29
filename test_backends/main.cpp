#include "CameraEngine.h"
#include <QCoreApplication>
#include <QDebug>
#include <atomic>

// 使用本机 SDK 验证枚举、反复切换和工作线程销毁；不连接或修改实际相机。
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QThread worker;
    auto* engine = new CameraEngine;
    engine->moveToThread(&worker);
    std::atomic<bool> destroyed{false};
    QObject::connect(&worker, &QThread::finished, engine, &QObject::deleteLater);
    QObject::connect(engine, &QObject::destroyed, [&]() { destroyed = true; });
    int errors = 0, lists = 0, changes = 0;
    QObject::connect(engine, &CameraEngine::errorOccurred, engine,
        [&](int code, const QString& message) { ++errors; qWarning() << code << message; });
    QObject::connect(engine, &CameraEngine::deviceListUpdated, engine,
        [&](const QList<DeviceInfo>& devices) { ++lists; qInfo() << "devices:" << devices.size(); });
    QObject::connect(engine, &CameraEngine::sdkChanged, engine,
        [&](CameraSdk) { ++changes; });
    worker.start();
    bool passed = false;
    QMetaObject::invokeMethod(engine, [&]() {
        qInfo() << "Enumerating MVS";
        engine->enumDevices();
        qInfo() << "Switching to FVCAM";
        engine->setSdk(CameraSdk::FVCAM);
        qInfo() << "Switching to MVS";
        engine->setSdk(CameraSdk::MVS);
        qInfo() << "Switching to FVCAM again";
        engine->setSdk(CameraSdk::FVCAM);
        engine->enumDevices();
        passed = errors == 0 && lists == 5 && changes == 3;
        engine->connectToDevice(-1);
        passed = passed && errors == 1;
        engine->disconnectDevice();
    }, Qt::BlockingQueuedConnection);
    worker.quit();
    worker.wait();
    if (!passed || !destroyed) return 1;
    qInfo() << "SDK switch/lifecycle smoke test passed";
    return 0;
}
