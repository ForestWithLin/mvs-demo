#ifndef MVSVIEWER_CAMERA_ENGINE_H
#define MVSVIEWER_CAMERA_ENGINE_H

#include <QObject>
#include <QThread>
#include <QImage>
#include <QTimer>
#include <QMutex>
#include <atomic>
#include <vector>
#include <memory>
#include "Types.h"

#include "MvCameraControl.h"

class FvcamBackend;

class CameraEngine : public QObject
{
    Q_OBJECT
public:
    explicit CameraEngine(QObject* parent = nullptr);
    ~CameraEngine() override;

    // 生命周期管理
    void setWorkerThread(QThread* thread);
    QThread* workerThread() const { return m_workerThread; }

public slots:
    // 仅允许未连接时切换，完成后重新枚举；所有 SDK 操作都在工作线程执行。
    void setSdk(CameraSdk sdk);
    void enumDevices();
    void connectToDevice(int index);
    void disconnectDevice();
    void startGrabbing();
    void stopGrabbing();
    void getParam(const QString& key);
    void setParam(const QString& key, const QVariant& value);
    void setTargetFrameRate(double fps);

signals:
    void sdkChanged(CameraSdk sdk);
    void deviceListUpdated(const QList<DeviceInfo>& devices);
    void frameReceived(const QImage& frame);
    void deviceConnected();
    void deviceDisconnected();
    void paramReadFinished(const QString& key, const QVariant& value);
    void paramWriteFinished(const QString& key, bool success);
    void errorOccurred(int errCode, const QString& msg);
    void statusMessage(const QString& msg);

private slots:
    void grabFrame();

private:
    bool isConnected() const;
    CameraSdk m_sdk = CameraSdk::MVS;
    std::unique_ptr<FvcamBackend> m_fvcam;
    bool m_mvsInitialized = false;
    void* m_handle = nullptr;
    QThread* m_workerThread = nullptr;
    QTimer* m_grabTimer = nullptr;

    QList<DeviceInfo> m_deviceList;
    MV_CC_DEVICE_INFO* m_selectedDeviceInfo = nullptr;

    std::vector<unsigned char> m_rgbBuffer;
    std::atomic<bool> m_isGrabbing{false};

    static QString getDeviceName(MV_CC_DEVICE_INFO* pInfo);
    static QString getTransportTypeStr(unsigned int tlayerType);
};

#endif // MVSVIEWER_CAMERA_ENGINE_H
