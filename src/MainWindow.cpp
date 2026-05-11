#include "MainWindow.h"
#include "CameraEngine.h"
#include "ImageViewer.h"
#include "DevicePanel.h"
#include "CameraParamPanel.h"
#include "VirtualCameraPanel.h"
#include "Types.h"

#include <QApplication>
#include <QMessageBox>
#include <QFileDialog>
#include <QThread>
#include <QTimer>
#include <QDateTime>
#include <QDoubleSpinBox>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("MVS 相机查看器");
    resize(1280, 800);

    setupUI();
    setupMenuBar();
    setupToolBar();
    setupStatusBar();

    // 创建相机引擎和工作者线程
    m_cameraEngine = new CameraEngine();
    m_workerThread = new QThread(this);
    m_cameraEngine->setWorkerThread(m_workerThread);
    m_cameraEngine->moveToThread(m_workerThread);
    m_workerThread->start();

    setupConnections();

    // 帧率统计定时器
    m_fpsTimer = new QTimer(this);
    connect(m_fpsTimer, &QTimer::timeout, this, [this]() {
        m_statusFps->setText(QString("帧率: %1 fps").arg(m_frameCount));
        m_frameCount = 0;
    });
    m_fpsTimer->start(1000);

    // 启动时自动枚举设备
    QMetaObject::invokeMethod(m_cameraEngine, "enumDevices", Qt::QueuedConnection);
}

MainWindow::~MainWindow()
{
    m_workerThread->quit();
    m_workerThread->wait();
    delete m_cameraEngine;
}

void MainWindow::setupUI()
{
    m_splitter = new QSplitter(Qt::Horizontal, this);
    setCentralWidget(m_splitter);

    m_devicePanel = new DevicePanel(this);
    m_imageViewer = new ImageViewer(this);
    m_paramPanel = new CameraParamPanel(this);
    m_virtualCamPanel = new VirtualCameraPanel(this);

    m_splitter->addWidget(m_devicePanel);
    m_splitter->addWidget(m_imageViewer);
    m_splitter->addWidget(m_paramPanel);
    m_splitter->addWidget(m_virtualCamPanel);
    m_splitter->setStretchFactor(0, 0);  // 设备面板不伸缩
    m_splitter->setStretchFactor(1, 1);  // 图像区域伸缩
    m_splitter->setStretchFactor(2, 0);  // 参数面板不伸缩
    m_splitter->setStretchFactor(3, 0);  // 虚拟相机面板不伸缩
    m_splitter->setSizes({250, 530, 250, 250});
}

void MainWindow::setupMenuBar()
{
    // 文件菜单
    QMenu* fileMenu = menuBar()->addMenu("文件(&F)");
    fileMenu->addAction("截图(&S)", QKeySequence("Ctrl+S"), this, &MainWindow::onSnapshot);
    fileMenu->addSeparator();
    fileMenu->addAction("退出(&Q)", QKeySequence("Ctrl+Q"), qApp, &QApplication::quit);

    // 视图菜单
    QMenu* viewMenu = menuBar()->addMenu("视图(&V)");
    viewMenu->addAction("全屏(&F)", QKeySequence("F11"), m_imageViewer, &ImageViewer::toggleFullscreen);

    // 帮助菜单
    QMenu* helpMenu = menuBar()->addMenu("帮助(&H)");
    helpMenu->addAction("关于(&A)", this, [this]() {
        QMessageBox::about(this, "关于 MVS 相机查看器",
                           "MVS 相机查看器 v1.0\n"
                           "基于 Hikvision MVS SDK + Qt6.8.3\n"
                           "支持实时图像显示、相机参数调节、图像操作等功能。");
    });
}

void MainWindow::setupToolBar()
{
    QToolBar* toolbar = addToolBar("主工具栏");
    toolbar->setMovable(false);

    m_actConnect = toolbar->addAction("连接");
    m_actDisconnect = toolbar->addAction("断开");
    toolbar->addSeparator();
    m_actSnapshot = toolbar->addAction("截图");
    toolbar->addSeparator();
    m_actZoomIn = toolbar->addAction("放大");
    m_actZoomOut = toolbar->addAction("缩小");
    m_actFitWindow = toolbar->addAction("自适应");
    toolbar->addSeparator();
    m_actRotateLeft = toolbar->addAction("左旋");
    m_actRotateRight = toolbar->addAction("右旋");
    m_actFlipH = toolbar->addAction("水平翻转");
    m_actFlipV = toolbar->addAction("垂直翻转");
    toolbar->addSeparator();
    m_actFullscreen = toolbar->addAction("全屏");
    toolbar->addSeparator();

    // 帧率控制
    auto* fpsLabel = new QLabel("帧率:");
    toolbar->addWidget(fpsLabel);
    m_fpsSpinBox = new QDoubleSpinBox();
    m_fpsSpinBox->setRange(0.1, 9999.0);
    m_fpsSpinBox->setDecimals(2);
    m_fpsSpinBox->setSuffix(" fps");
    m_fpsSpinBox->setEnabled(false);
    m_fpsSpinBox->setFixedWidth(120);
    toolbar->addWidget(m_fpsSpinBox);

    toolbar->addSeparator();
    auto* actVirtualCam = toolbar->addAction("虚拟相机");
    actVirtualCam->setCheckable(true);
    actVirtualCam->setChecked(true);
    connect(actVirtualCam, &QAction::toggled, m_virtualCamPanel, &QWidget::setVisible);

    m_actDisconnect->setEnabled(false);
}

void MainWindow::setupStatusBar()
{
    m_statusFps = new QLabel("帧率: -- fps");
    m_statusResolution = new QLabel("分辨率: --");
    m_statusPixelFormat = new QLabel("像素格式: --");
    m_statusDevice = new QLabel("设备: 未连接");

    statusBar()->addWidget(m_statusFps);
    statusBar()->addWidget(m_statusResolution);
    statusBar()->addWidget(m_statusPixelFormat);
    statusBar()->addPermanentWidget(m_statusDevice);
}

void MainWindow::setupConnections()
{
    // CameraEngine -> MainWindow
    connect(m_cameraEngine, &CameraEngine::deviceListUpdated,
            this, &MainWindow::onDevicesUpdated);
    connect(m_cameraEngine, &CameraEngine::deviceConnected,
            this, &MainWindow::onDeviceConnected);
    connect(m_cameraEngine, &CameraEngine::deviceDisconnected,
            this, &MainWindow::onDeviceDisconnected);
    connect(m_cameraEngine, &CameraEngine::frameReceived,
            this, &MainWindow::onFrameReceived);
    connect(m_cameraEngine, &CameraEngine::errorOccurred,
            this, &MainWindow::onError);
    connect(m_cameraEngine, &CameraEngine::statusMessage,
            this, &MainWindow::onStatusMessage);
    connect(m_cameraEngine, &CameraEngine::paramReadFinished,
            m_paramPanel, &CameraParamPanel::onParamReadFinished);
    connect(m_paramPanel, &CameraParamPanel::paramReadRequested,
            m_cameraEngine, &CameraEngine::getParam);

    // 工具栏帧率控件：调整取流速率
    connect(m_fpsSpinBox, &QDoubleSpinBox::valueChanged,
            this, [this](double fps) {
        if (!m_fpsSpinBox->isEnabled()) return;
        QMetaObject::invokeMethod(m_cameraEngine, "setTargetFrameRate",
                                  Qt::QueuedConnection,
                                  Q_ARG(double, fps));
    });

    // MainWindow -> CameraEngine (invoke via queued connection)
    connect(m_devicePanel, &DevicePanel::refreshRequested,
            this, &MainWindow::onRefreshDevices);
    connect(m_devicePanel, &DevicePanel::connectRequested,
            this, &MainWindow::onConnectDevice);
    connect(m_devicePanel, &DevicePanel::disconnectRequested,
            this, &MainWindow::onDisconnectDevice);

    // ImageViewer signals
    connect(m_imageViewer, &ImageViewer::zoomChanged,
            this, &MainWindow::onZoomChanged);

    // Toolbar actions
    connect(m_actZoomIn, &QAction::triggered, m_imageViewer, &ImageViewer::zoomIn);
    connect(m_actZoomOut, &QAction::triggered, m_imageViewer, &ImageViewer::zoomOut);
    connect(m_actFitWindow, &QAction::triggered, m_imageViewer, &ImageViewer::fitToWindow);
    connect(m_actConnect, &QAction::triggered, m_devicePanel, &DevicePanel::onConnectClicked);
    connect(m_actDisconnect, &QAction::triggered, this, &MainWindow::onDisconnectDevice);
    connect(m_actSnapshot, &QAction::triggered, this, &MainWindow::onSnapshot);
    connect(m_actRotateLeft, &QAction::triggered, this, [this]() { m_imageViewer->rotateImage(-90); });
    connect(m_actRotateRight, &QAction::triggered, this, [this]() { m_imageViewer->rotateImage(90); });
    connect(m_actFlipH, &QAction::triggered, m_imageViewer, &ImageViewer::flipHorizontal);
    connect(m_actFlipV, &QAction::triggered, m_imageViewer, &ImageViewer::flipVertical);
    connect(m_actFullscreen, &QAction::triggered, m_imageViewer, &ImageViewer::toggleFullscreen);

    // Param panel
    connect(m_paramPanel, &CameraParamPanel::paramChangeRequested,
            this, &MainWindow::onParamChanged);

    // Virtual camera panel
    connect(m_virtualCamPanel, &VirtualCameraPanel::statusMessage,
            this, &MainWindow::onStatusMessage);
}

void MainWindow::onDevicesUpdated(const QList<DeviceInfo>& devices)
{
    m_devicePanel->updateDeviceList(devices);
}

void MainWindow::onDeviceConnected()
{
    m_actConnect->setEnabled(false);
    m_actDisconnect->setEnabled(true);
    m_devicePanel->setConnectedState(true);
    m_statusDevice->setText("设备: 已连接");

    // 打开设备后自动开始取流
    QMetaObject::invokeMethod(m_cameraEngine, "startGrabbing", Qt::QueuedConnection);

    // 设置帧率控件初始值（默认 30fps），先设值再启用以避免触发写入
    m_fpsSpinBox->setValue(30.0);
    m_fpsSpinBox->setEnabled(true);

    // 读取当前帧率（用于参数树）
    QMetaObject::invokeMethod(m_cameraEngine, "getParam",
                              Qt::QueuedConnection,
                              Q_ARG(QString, "AcquisitionFrameRate"));

    // 延迟请求所有参数（等待设备稳定）
    QTimer::singleShot(500, m_paramPanel, &CameraParamPanel::requestAllParams);
}

void MainWindow::onDeviceDisconnected()
{
    m_actConnect->setEnabled(true);
    m_actDisconnect->setEnabled(false);
    m_devicePanel->setConnectedState(false);
    m_statusDevice->setText("设备: 未连接");
    m_statusResolution->setText("分辨率: --");
    m_statusPixelFormat->setText("像素格式: --");
    m_paramPanel->clearParams();
    m_fpsSpinBox->setEnabled(false);
}

void MainWindow::onFrameReceived(const QImage& frame)
{
    m_imageViewer->setImage(frame);
    m_frameCount++;

    m_statusResolution->setText(QString("分辨率: %1x%2").arg(frame.width()).arg(frame.height()));
    m_statusPixelFormat->setText("像素格式: RGB24");
}

void MainWindow::onError(int /*errCode*/, const QString& msg)
{
    statusBar()->showMessage("错误: " + msg, 3000);
}

void MainWindow::onStatusMessage(const QString& msg)
{
    statusBar()->showMessage(msg, 3000);
}

void MainWindow::onZoomChanged(double factor)
{
    statusBar()->showMessage(QString("缩放: %1%").arg(static_cast<int>(factor * 100)), 2000);
}

void MainWindow::onConnectDevice(int index)
{
    QMetaObject::invokeMethod(m_cameraEngine, "connectToDevice",
                              Qt::QueuedConnection,
                              Q_ARG(int, index));
}

void MainWindow::onDisconnectDevice()
{
    QMetaObject::invokeMethod(m_cameraEngine, "disconnectDevice",
                              Qt::QueuedConnection);
}

void MainWindow::onRefreshDevices()
{
    QMetaObject::invokeMethod(m_cameraEngine, "enumDevices", Qt::QueuedConnection);
    m_virtualCamPanel->refreshDevices();
}

void MainWindow::onSnapshot()
{
    QImage img = m_imageViewer->currentImage();
    if (img.isNull()) {
        QMessageBox::warning(this, "截图失败", "当前没有图像可保存");
        return;
    }

    QString defaultName = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".png";
    QString filePath = QFileDialog::getSaveFileName(this, "保存截图", defaultName,
                                                     "图片文件 (*.png *.jpg *.bmp)");
    if (filePath.isEmpty()) return;

    if (img.save(filePath)) {
        onStatusMessage(QString("截图已保存: %1").arg(filePath));
    } else {
        QMessageBox::warning(this, "截图失败", "保存图像失败");
    }
}

void MainWindow::onParamChanged(const QString& key, const QVariant& value)
{
    QMetaObject::invokeMethod(m_cameraEngine, "setParam",
                              Qt::QueuedConnection,
                              Q_ARG(QString, key),
                              Q_ARG(QVariant, value));
}
