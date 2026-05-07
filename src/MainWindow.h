#ifndef MVSVIEWER_MAIN_WINDOW_H
#define MVSVIEWER_MAIN_WINDOW_H

#include <QMainWindow>
#include <QSplitter>
#include <QStatusBar>
#include <QLabel>
#include <QToolBar>
#include <QMenuBar>
#include <QAction>
#include "Types.h"

class CameraEngine;
class ImageViewer;
class DevicePanel;
class CameraParamPanel;
class VirtualCameraPanel;
class QDoubleSpinBox;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onDevicesUpdated(const QList<DeviceInfo>& devices);
    void onDeviceConnected();
    void onDeviceDisconnected();
    void onFrameReceived(const QImage& frame);
    void onError(int errCode, const QString& msg);
    void onStatusMessage(const QString& msg);
    void onZoomChanged(double factor);

    void onConnectDevice(int index);
    void onDisconnectDevice();
    void onRefreshDevices();
    void onSnapshot();
    void onParamChanged(const QString& key, const QVariant& value);

private:
    void setupUI();
    void setupMenuBar();
    void setupToolBar();
    void setupStatusBar();
    void setupConnections();

    // UI 组件
    QSplitter* m_splitter;
    DevicePanel* m_devicePanel;
    ImageViewer* m_imageViewer;
    CameraParamPanel* m_paramPanel;

    // 状态栏标签
    QLabel* m_statusFps;
    QLabel* m_statusResolution;
    QLabel* m_statusPixelFormat;
    QLabel* m_statusDevice;

    // 工具栏动作
    QAction* m_actConnect;
    QAction* m_actDisconnect;
    QAction* m_actSnapshot;
    QAction* m_actZoomIn;
    QAction* m_actZoomOut;
    QAction* m_actFitWindow;
    QAction* m_actRotateLeft;
    QAction* m_actRotateRight;
    QAction* m_actFlipH;
    QAction* m_actFlipV;
    QAction* m_actFullscreen;

    // 虚拟相机面板
    VirtualCameraPanel* m_virtualCamPanel;

    // 帧率控制
    QDoubleSpinBox* m_fpsSpinBox;

    // 核心引擎
    CameraEngine* m_cameraEngine;
    QThread* m_workerThread;

    // 帧率统计
    int m_frameCount = 0;
    QTimer* m_fpsTimer;
};

#endif // MVSVIEWER_MAIN_WINDOW_H
