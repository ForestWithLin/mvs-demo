#ifndef MVSVIEWER_CAMERA_ENGINE_H
#define MVSVIEWER_CAMERA_ENGINE_H

#include <QObject>
#include <QThread>
#include <QImage>
#include <QTimer>
#include <QMutex>
#include <atomic>
#include <vector>
#include "Types.h"

#include "MvCameraControl.h"

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
    void enumDevices();
    void connectToDevice(int index);
    void disconnectDevice();
    void startGrabbing();
    void stopGrabbing();
    void getParam(const QString& key);
    void setParam(const QString& key, const QVariant& value);
    void setTargetFrameRate(double fps);

signals:
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
