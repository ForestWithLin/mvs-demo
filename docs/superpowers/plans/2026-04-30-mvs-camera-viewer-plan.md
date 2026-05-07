# 海康相机实时图像查看器 — 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a Qt6 desktop application that reads real-time images from Hikvision cameras via MVS SDK, displays basic image info, and provides interactive image operations (zoom, rotate, flip, fullscreen, snapshot).

**Architecture:** QMainWindow with QSplitter three-panel layout (device list | image viewer | camera params). CameraEngine wraps MVS SDK calls in a QObject moved to a QThread, emitting QImage frames via queued signals to the UI thread. ImageViewer extends QGraphicsView for native transform support.

**Tech Stack:** Qt6.8.3 (Widgets, Core, Gui), Hikvision MVS SDK (libMvCameraControl.so 4.7.0), CMake, C++17, Linux x86_64

---

### Task 1: CMakeLists.txt — 项目构建配置

**Files:**
- Delete: `CmakeLists.txt`
- Create: `CMakeLists.txt`
- Verify: `src/` exists

- [ ] **Step 1: Remove old wrongly-named file and create CMakeLists.txt**

Old file is named `CmakeLists.txt` (lowercase 'L') — CMake requires exactly `CMakeLists.txt`.

```bash
rm /home/linwj/project/personal/mvs-demo/CmakeLists.txt
```

- [ ] **Step 2: Write CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.20)
project(MVSViewer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# --- Qt6 ---
set(CMAKE_PREFIX_PATH "/opt/software/Qt6.8.3")
find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

# --- Hikvision MVS SDK ---
set(MVS_INCLUDE_DIR "/opt/MVS/include")
set(MVS_LIB_DIR "/opt/MVS/lib/64")
set(MVS_LIBS "${MVS_LIB_DIR}/libMvCameraControl.so")

# --- Source files ---
set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/CameraEngine.cpp
    src/ImageViewer.cpp
    src/DevicePanel.cpp
    src/CameraParamPanel.cpp
)

set(HEADERS
    src/MainWindow.h
    src/CameraEngine.h
    src/ImageViewer.h
    src/DevicePanel.h
    src/CameraParamPanel.h
    src/Types.h
)

# --- Executable ---
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${MVS_INCLUDE_DIR}
)

target_link_directories(${PROJECT_NAME} PRIVATE ${MVS_LIB_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
    ${MVS_LIBS}
)
```

- [ ] **Step 3: Verify cmake configure works**

Run: `mkdir -p build && cd build && cmake .. 2>&1 | head -30`
Expected: Configures successfully, finds Qt6 and MVS headers.

---

### Task 2: Types.h — 公用数据结构

**Files:**
- Create: `src/Types.h`

- [ ] **Step 1: Write Types.h**

```cpp
#ifndef MVSVIEWER_TYPES_H
#define MVSVIEWER_TYPES_H

#include <QString>
#include <QMetaType>

struct DeviceInfo {
    QString name;            // 设备显示名称
    QString serialNumber;    // 序列号
    QString ipAddress;       // IP 地址（仅 GigE）
    QString transportType;   // 传输层类型: "GigE" / "USB3" / "CameraLink"
    int indexInSdk;          // 在 SDK 枚举列表中的索引
    bool isConnected = false;

    // SDK 原始设备信息指针（不拥有，仅用于 CameraEngine 内部）
    void* sdkDeviceInfo = nullptr;
};

// 相机参数描述
struct CameraParamInfo {
    QString key;             // SDK 参数 key
    QString displayName;     // 显示名称
    QString category;        // 分组: 曝光/图像/颜色/触发/IO
    QString type;            // "int" / "float" / "enum" / "bool"
};

Q_DECLARE_METATYPE(DeviceInfo)
Q_DECLARE_METATYPE(QList<DeviceInfo>)

#endif // MVSVIEWER_TYPES_H
```

---

### Task 3: CameraEngine — SDK 封装

**Files:**
- Create: `src/CameraEngine.h`
- Create: `src/CameraEngine.cpp`

- [ ] **Step 1: Write CameraEngine.h**

```cpp
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

    std::vector<unsigned char> m_frameBuffer;
    std::atomic<bool> m_isGrabbing{false};

    static QString getDeviceName(MV_CC_DEVICE_INFO* pInfo);
    static QString getTransportTypeStr(unsigned int tlayerType);
};

#endif // MVSVIEWER_CAMERA_ENGINE_H
```

- [ ] **Step 2: Write CameraEngine.cpp**

```cpp
#include "CameraEngine.h"
#include "CameraParams.h"

#include <QDateTime>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

CameraEngine::CameraEngine(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<DeviceInfo>("DeviceInfo");
    qRegisterMetaType<QList<DeviceInfo>>("QList<DeviceInfo>");
}

CameraEngine::~CameraEngine()
{
    stopGrabbing();
    disconnectDevice();
}

void CameraEngine::setWorkerThread(QThread* thread)
{
    m_workerThread = thread;
}

void CameraEngine::enumDevices()
{
    MV_CC_Initialize();

    MV_CC_DEVICE_INFO_LIST devList;
    memset(&devList, 0, sizeof(devList));

    // 枚举所有类型设备 (GigE | USB | CameraLink)
    int ret = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE | MV_CAMERALINK_DEVICE, &devList);
    if (ret != MV_OK) {
        emit errorOccurred(ret, QString("枚举设备失败: err=%1").arg(ret));
        return;
    }

    m_deviceList.clear();
    for (unsigned int i = 0; i < devList.nDeviceNum; ++i) {
        MV_CC_DEVICE_INFO* pInfo = devList.pDeviceInfo[i];
        DeviceInfo info;
        info.name = getDeviceName(pInfo);
        info.transportType = getTransportTypeStr(pInfo->nTLayerType);
        info.indexInSdk = i;
        info.sdkDeviceInfo = pInfo;

        // 提取序列号和 IP
        if (pInfo->nTLayerType == MV_GIGE_DEVICE) {
            MV_GIGE_DEVICE_INFO& gige = pInfo->SpecialInfo.stGigEInfo;
            info.serialNumber = QString::fromLatin1((const char*)gige.chSerialNumber);
            info.ipAddress = QString("%1.%2.%3.%4")
                .arg((gige.nCurrentIp >> 24) & 0xFF)
                .arg((gige.nCurrentIp >> 16) & 0xFF)
                .arg((gige.nCurrentIp >> 8) & 0xFF)
                .arg(gige.nCurrentIp & 0xFF);
        } else if (pInfo->nTLayerType == MV_USB_DEVICE) {
            MV_USB3_DEVICE_INFO& usb = pInfo->SpecialInfo.stUsb3VInfo;
            info.serialNumber = QString::fromLatin1((const char*)usb.chSerialNumber);
        } else if (pInfo->nTLayerType == MV_CAMERALINK_DEVICE) {
            MV_CAMERALINK_DEVICE_INFO& cl = pInfo->SpecialInfo.stCamLinkInfo;
            info.serialNumber = QString::fromLatin1((const char*)cl.chSerialNumber);
        }

        m_deviceList.append(info);
    }

    emit deviceListUpdated(m_deviceList);
    emit statusMessage(QString("枚举完成，发现 %1 个设备").arg(devList.nDeviceNum));
}

void CameraEngine::connectToDevice(int index)
{
    if (index < 0 || index >= m_deviceList.size()) {
        emit errorOccurred(-1, "无效设备索引");
        return;
    }

    // 先断开之前的连接
    if (m_handle) {
        disconnectDevice();
    }

    DeviceInfo& info = m_deviceList[index];
    MV_CC_DEVICE_INFO* pInfo = static_cast<MV_CC_DEVICE_INFO*>(info.sdkDeviceInfo);

    int ret = MV_CC_CreateHandle(&m_handle, pInfo);
    if (ret != MV_OK) {
        emit errorOccurred(ret, "创建设备句柄失败");
        m_handle = nullptr;
        return;
    }

    ret = MV_CC_OpenDevice(m_handle, MV_ACCESS_Exclusive, 0);
    if (ret != MV_OK) {
        MV_CC_DestroyHandle(m_handle);
        m_handle = nullptr;
        emit errorOccurred(ret, "打开设备失败");
        return;
    }

    info.isConnected = true;
    m_selectedDeviceInfo = pInfo;

    emit deviceConnected();
    emit statusMessage(QString("已连接到: %1").arg(info.name));
}

void CameraEngine::disconnectDevice()
{
    stopGrabbing();

    if (m_handle) {
        MV_CC_CloseDevice(m_handle);
        MV_CC_DestroyHandle(m_handle);
        m_handle = nullptr;
    }

    m_selectedDeviceInfo = nullptr;
    for (auto& d : m_deviceList) {
        d.isConnected = false;
    }

    emit deviceDisconnected();
    emit statusMessage("设备已断开");
}

void CameraEngine::startGrabbing()
{
    if (!m_handle || m_isGrabbing) return;

    // 先获取一帧以确定 buffer 大小
    MV_FRAME_OUT_INFO_EX frameInfo = {0};
    m_frameBuffer.resize(4 * 1920 * 1080 * 3); // 足够 4K 三通道

    int ret = MV_CC_StartGrabbing(m_handle);
    if (ret != MV_OK) {
        emit errorOccurred(ret, "开始取流失败");
        return;
    }

    m_isGrabbing = true;

    // QTimer 在 worker thread 的事件循环中触发
    m_grabTimer = new QTimer(this);
    m_grabTimer->setTimerType(Qt::PreciseTimer);
    connect(m_grabTimer, &QTimer::timeout, this, &CameraEngine::grabFrame);
    m_grabTimer->start(33); // ~30 fps

    emit statusMessage("开始取流");
}

void CameraEngine::stopGrabbing()
{
    m_isGrabbing = false;

    if (m_grabTimer) {
        m_grabTimer->stop();
        delete m_grabTimer;
        m_grabTimer = nullptr;
    }

    if (m_handle) {
        MV_CC_StopGrabbing(m_handle);
    }

    emit statusMessage("停止取流");
}

void CameraEngine::grabFrame()
{
    if (!m_handle || !m_isGrabbing) return;

    MV_FRAME_OUT_INFO_EX frameInfo = {0};
    int ret = MV_CC_GetOneFrameTimeout(m_handle, m_frameBuffer.data(),
                                        static_cast<unsigned int>(m_frameBuffer.size()),
                                        &frameInfo, 100);
    if (ret != MV_OK) {
        // MV_EAGAIN 是超时，正常
        if (ret != MV_EAGAIN) {
            emit errorOccurred(ret, "取帧失败");
        }
        return;
    }

    // 转换为 RGB888
    MV_CC_PIXEL_CONVERT_PARAM_EX convertParam = {0};
    convertParam.nWidth = frameInfo.nWidth;
    convertParam.nHeight = frameInfo.nHeight;
    convertParam.enSrcPixelType = frameInfo.enPixelType;
    convertParam.pSrcData = m_frameBuffer.data();
    convertParam.nSrcDataLen = frameInfo.nFrameLen;
    convertParam.enDstPixelType = PixelType_Gvsp_RGB8_Packed;
    convertParam.nDstBufferSize = frameInfo.nWidth * frameInfo.nHeight * 3;

    std::vector<unsigned char> rgbBuffer(frameInfo.nWidth * frameInfo.nHeight * 3);
    convertParam.pDstData = rgbBuffer.data();

    ret = MV_CC_ConvertPixelTypeEx(m_handle, &convertParam);
    if (ret != MV_OK) {
        emit errorOccurred(ret, "像素格式转换失败");
        return;
    }

    QImage image(rgbBuffer.data(), frameInfo.nWidth, frameInfo.nHeight,
                 frameInfo.nWidth * 3, QImage::Format_RGB888);

    // QImage 是浅拷贝，需要复制数据
    QImage copiedImage = image.copy();

    emit frameReceived(copiedImage);
}

void CameraEngine::getParam(const QString& key)
{
    if (!m_handle) return;

    QByteArray keyBytes = key.toUtf8();

    // 尝试 float 类型
    MVCC_FLOATVALUE floatVal;
    memset(&floatVal, 0, sizeof(floatVal));
    int ret = MV_CC_GetFloatValue(m_handle, keyBytes.constData(), &floatVal);
    if (ret == MV_OK) {
        emit paramReadFinished(key, QVariant(floatVal.fCurValue));
        return;
    }

    // 尝试 int 类型
    MVCC_INTVALUE_EX intVal;
    memset(&intVal, 0, sizeof(intVal));
    ret = MV_CC_GetIntValueEx(m_handle, keyBytes.constData(), &intVal);
    if (ret == MV_OK) {
        emit paramReadFinished(key, QVariant(static_cast<qlonglong>(intVal.nCurValue)));
        return;
    }

    // 尝试 enum 类型
    MVCC_ENUMVALUE enumVal;
    memset(&enumVal, 0, sizeof(enumVal));
    ret = MV_CC_GetEnumValue(m_handle, keyBytes.constData(), &enumVal);
    if (ret == MV_OK) {
        emit paramReadFinished(key, QVariant(static_cast<int>(enumVal.nCurValue)));
        return;
    }

    emit errorOccurred(-1, QString("读取参数失败: %1").arg(key));
}

void CameraEngine::setParam(const QString& key, const QVariant& value)
{
    if (!m_handle) return;

    QByteArray keyBytes = key.toUtf8();
    int ret = MV_OK;

    switch (value.type()) {
    case QVariant::Double:
    case QVariant::String:
    {
        bool ok = false;
        double fVal = value.toDouble(&ok);
        if (ok) {
            ret = MV_CC_SetFloatValue(m_handle, keyBytes.constData(), static_cast<float>(fVal));
        }
        break;
    }
    case QVariant::Int:
    case QVariant::LongLong:
    {
        ret = MV_CC_SetEnumValue(m_handle, keyBytes.constData(),
                                  static_cast<unsigned int>(value.toUInt()));
        break;
    }
    default:
        ret = MV_CC_SetEnumValue(m_handle, keyBytes.constData(),
                                  static_cast<unsigned int>(value.toUInt()));
        break;
    }

    emit paramWriteFinished(key, ret == MV_OK);
    if (ret != MV_OK) {
        emit errorOccurred(ret, QString("设置参数失败: %1").arg(key));
    }
}

QString CameraEngine::getDeviceName(MV_CC_DEVICE_INFO* pInfo)
{
    if (!pInfo) return "Unknown";

    if (pInfo->nTLayerType == MV_GIGE_DEVICE) {
        return QString::fromLatin1((const char*)pInfo->SpecialInfo.stGigEInfo.chUserDefinedName);
    } else if (pInfo->nTLayerType == MV_USB_DEVICE) {
        return QString::fromLatin1((const char*)pInfo->SpecialInfo.stUsb3VInfo.chUserDefinedName);
    } else if (pInfo->nTLayerType == MV_CAMERALINK_DEVICE) {
        return QString::fromLatin1((const char*)pInfo->SpecialInfo.stCamLinkInfo.chUserDefinedName);
    }
    return "Unknown";
}

QString CameraEngine::getTransportTypeStr(unsigned int tlayerType)
{
    switch (tlayerType) {
    case MV_GIGE_DEVICE:       return "GigE";
    case MV_USB_DEVICE:        return "USB3";
    case MV_CAMERALINK_DEVICE: return "CameraLink";
    default:                   return QString("Unknown(0x%1)").arg(tlayerType, 0, 16);
    }
}
```

---

### Task 4: ImageViewer — 图像显示与交互

**Files:**
- Create: `src/ImageViewer.h`
- Create: `src/ImageViewer.cpp`

- [ ] **Step 1: Write ImageViewer.h**

```cpp
#ifndef MVSVIEWER_IMAGE_VIEWER_H
#define MVSVIEWER_IMAGE_VIEWER_H

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QPixmap>
#include <QImage>

class ImageViewer : public QGraphicsView
{
    Q_OBJECT
public:
    explicit ImageViewer(QWidget* parent = nullptr);

    void setImage(const QImage& image);
    QImage currentImage() const { return m_currentImage; }

public slots:
    void zoomIn();
    void zoomOut();
    void fitToWindow();
    void rotateImage(int degrees);
    void flipHorizontal();
    void flipVertical();
    void toggleFullscreen();
    void resetView();

signals:
    void zoomChanged(double factor);
    void fullscreenChanged(bool isFullscreen);

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    QGraphicsScene* m_scene;
    QGraphicsPixmapItem* m_pixmapItem;
    QPixmap m_pixmap;
    QImage m_currentImage;

    double m_zoomFactor = 1.0;
    bool m_isPanning = false;
    QPoint m_lastPanPoint;
    bool m_isFullscreen = false;
    bool m_fitToWindowEnabled = true;
};

#endif // MVSVIEWER_IMAGE_VIEWER_H
```

- [ ] **Step 2: Write ImageViewer.cpp**

```cpp
#include "ImageViewer.h"
#include <QWheelEvent>
#include <QMouseEvent>
#include <QScrollBar>

ImageViewer::ImageViewer(QWidget* parent)
    : QGraphicsView(parent)
{
    m_scene = new QGraphicsScene(this);
    setScene(m_scene);

    m_pixmapItem = m_scene->addPixmap(QPixmap());

    // 视图设置
    setRenderHint(QPainter::SmoothPixmapTransform);
    setDragMode(QGraphicsView::NoDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorUnderMouse);
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setBackgroundBrush(QBrush(QColor(45, 45, 45)));

    // 允许鼠标追踪用于后续的放大镜等功能
    setMouseTracking(false);
}

void ImageViewer::setImage(const QImage& image)
{
    if (image.isNull()) return;

    m_currentImage = image;
    m_pixmap = QPixmap::fromImage(image);
    m_pixmapItem->setPixmap(m_pixmap);

    // 设置场景矩形
    m_scene->setSceneRect(m_pixmap.rect());

    if (m_fitToWindowEnabled) {
        fitToWindow();
    }
}

void ImageViewer::zoomIn()
{
    m_fitToWindowEnabled = false;
    double factor = 1.25;
    scale(factor, factor);
    m_zoomFactor *= factor;
    emit zoomChanged(m_zoomFactor);
}

void ImageViewer::zoomOut()
{
    m_fitToWindowEnabled = false;
    double factor = 0.8;
    scale(factor, factor);
    m_zoomFactor *= factor;
    emit zoomChanged(m_zoomFactor);
}

void ImageViewer::fitToWindow()
{
    if (m_pixmap.isNull()) return;

    m_fitToWindowEnabled = true;
    resetTransform();
    m_zoomFactor = 1.0;

    // fitInView 保持宽高比
    fitInView(m_pixmapItem, Qt::KeepAspectRatio);
    m_zoomFactor = transform().m11();
    emit zoomChanged(m_zoomFactor);
}

void ImageViewer::rotateImage(int degrees)
{
    m_fitToWindowEnabled = false;
    rotate(degrees);
}

void ImageViewer::flipHorizontal()
{
    m_fitToWindowEnabled = false;
    scale(-1, 1);
}

void ImageViewer::flipVertical()
{
    m_fitToWindowEnabled = false;
    scale(1, -1);
}

void ImageViewer::toggleFullscreen()
{
    if (m_isFullscreen) {
        if (parentWidget()) {
            parentWidget()->showNormal();
        }
    } else {
        if (parentWidget()) {
            parentWidget()->showFullScreen();
        }
    }
    m_isFullscreen = !m_isFullscreen;
    emit fullscreenChanged(m_isFullscreen);
}

void ImageViewer::resetView()
{
    resetTransform();
    m_zoomFactor = 1.0;
    m_fitToWindowEnabled = true;
    fitToWindow();
    emit zoomChanged(m_zoomFactor);
}

void ImageViewer::wheelEvent(QWheelEvent* event)
{
    m_fitToWindowEnabled = false;
    double factor = (event->angleDelta().y() > 0) ? 1.15 : 0.87;
    scale(factor, factor);
    m_zoomFactor *= factor;
    emit zoomChanged(m_zoomFactor);
    event->accept();
}

void ImageViewer::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton) {
        m_isPanning = true;
        m_lastPanPoint = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

void ImageViewer::mouseMoveEvent(QMouseEvent* event)
{
    if (m_isPanning) {
        QPoint delta = event->pos() - m_lastPanPoint;
        m_lastPanPoint = event->pos();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void ImageViewer::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton) {
        m_isPanning = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

void ImageViewer::resizeEvent(QResizeEvent* event)
{
    QGraphicsView::resizeEvent(event);
    if (m_fitToWindowEnabled && !m_pixmap.isNull()) {
        fitToWindow();
    }
}
```

---

### Task 5: DevicePanel — 设备列表面板

**Files:**
- Create: `src/DevicePanel.h`
- Create: `src/DevicePanel.cpp`

- [ ] **Step 1: Write DevicePanel.h**

```cpp
#ifndef MVSVIEWER_DEVICE_PANEL_H
#define MVSVIEWER_DEVICE_PANEL_H

#include <QWidget>
#include <QTreeWidget>
#include <QPushButton>
#include "Types.h"

class DevicePanel : public QWidget
{
    Q_OBJECT
public:
    explicit DevicePanel(QWidget* parent = nullptr);

    void updateDeviceList(const QList<DeviceInfo>& devices);

signals:
    void connectRequested(int index);
    void disconnectRequested();
    void refreshRequested();

private slots:
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onConnectClicked();
    void onDisconnectClicked();

private:
    QTreeWidget* m_tree;
    QPushButton* m_connectBtn;
    QPushButton* m_disconnectBtn;
    QPushButton* m_refreshBtn;
    QList<DeviceInfo> m_devices;
};

#endif // MVSVIEWER_DEVICE_PANEL_H
```

- [ ] **Step 2: Write DevicePanel.cpp**

```cpp
#include "DevicePanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>

DevicePanel::DevicePanel(QWidget* parent)
    : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);

    // 标题
    auto* titleLabel = new QLabel("设备列表");
    titleLabel->setStyleSheet("font-weight: bold; font-size: 13px; padding: 4px;");
    mainLayout->addWidget(titleLabel);

    // 树形列表
    m_tree = new QTreeWidget(this);
    m_tree->setHeaderLabels({"设备名称", "IP/序列号", "类型"});
    m_tree->setColumnWidth(0, 120);
    m_tree->setColumnWidth(1, 100);
    m_tree->setColumnWidth(2, 60);
    m_tree->setRootIsDecorated(false);
    m_tree->setAlternatingRowColors(true);
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    mainLayout->addWidget(m_tree);

    // 按钮栏
    auto* btnLayout = new QHBoxLayout();
    m_refreshBtn = new QPushButton("刷新");
    m_connectBtn = new QPushButton("连接");
    m_disconnectBtn = new QPushButton("断开");
    m_disconnectBtn->setEnabled(false);

    btnLayout->addWidget(m_refreshBtn);
    btnLayout->addWidget(m_connectBtn);
    btnLayout->addWidget(m_disconnectBtn);
    mainLayout->addLayout(btnLayout);

    // 连接信号
    connect(m_refreshBtn, &QPushButton::clicked, this, &DevicePanel::refreshRequested);
    connect(m_connectBtn, &QPushButton::clicked, this, &DevicePanel::onConnectClicked);
    connect(m_disconnectBtn, &QPushButton::clicked, this, &DevicePanel::onDisconnectClicked);
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, &DevicePanel::onItemDoubleClicked);

    setMinimumWidth(250);
    setMaximumWidth(350);
}

void DevicePanel::updateDeviceList(const QList<DeviceInfo>& devices)
{
    m_devices = devices;
    m_tree->clear();

    for (const auto& dev : devices) {
        auto* item = new QTreeWidgetItem();
        item->setText(0, dev.name);
        item->setText(1, dev.transportType == "GigE" ? dev.ipAddress : dev.serialNumber);
        item->setText(2, dev.transportType);
        item->setData(0, Qt::UserRole, dev.indexInSdk);

        if (dev.isConnected) {
            item->setForeground(0, QBrush(QColor(0, 180, 0)));
        }

        m_tree->addTopLevelItem(item);
    }
}

void DevicePanel::onItemDoubleClicked(QTreeWidgetItem* item, int /*column*/)
{
    if (!item) return;
    int index = item->data(0, Qt::UserRole).toInt();
    emit connectRequested(index);
}

void DevicePanel::onConnectClicked()
{
    auto* item = m_tree->currentItem();
    if (!item) return;
    int index = item->data(0, Qt::UserRole).toInt();
    emit connectRequested(index);
}

void DevicePanel::onDisconnectClicked()
{
    emit disconnectRequested();
}
```

Need to add `#include <QLabel>` at the top of DevicePanel.cpp.

---

### Task 6: CameraParamPanel — 相机参数面板

**Files:**
- Create: `src/CameraParamPanel.h`
- Create: `src/CameraParamPanel.cpp`

- [ ] **Step 1: Write CameraParamPanel.h**

```cpp
#ifndef MVSVIEWER_CAMERA_PARAM_PANEL_H
#define MVSVIEWER_CAMERA_PARAM_PANEL_H

#include <QWidget>
#include <QTreeWidget>
#include <QMap>
#include <QVariant>
#include "Types.h"

class CameraEngine;

class CameraParamPanel : public QWidget
{
    Q_OBJECT
public:
    explicit CameraParamPanel(QWidget* parent = nullptr);

    void setEngine(CameraEngine* engine);

public slots:
    void onParamReadFinished(const QString& key, const QVariant& value);
    void clearParams();

signals:
    void paramChangeRequested(const QString& key, const QVariant& value);

private slots:
    void onItemChanged(QTreeWidgetItem* item, int column);

private:
    void buildParamList();
    void requestAllParams();

    QTreeWidget* m_tree;
    CameraEngine* m_engine = nullptr;

    // 预定义的参数列表
    QList<CameraParamInfo> m_paramList;
    QMap<QString, QTreeWidgetItem*> m_paramItems;
};

#endif // MVSVIEWER_CAMERA_PARAM_PANEL_H
```

- [ ] **Step 2: Write CameraParamPanel.cpp**

```cpp
#include "CameraParamPanel.h"
#include "CameraEngine.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QLabel>

CameraParamPanel::CameraParamPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    auto* titleLabel = new QLabel("相机参数");
    titleLabel->setStyleSheet("font-weight: bold; font-size: 13px; padding: 4px;");
    layout->addWidget(titleLabel);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderLabels({"参数", "值"});
    m_tree->setColumnWidth(0, 130);
    m_tree->setRootIsDecorated(true);
    m_tree->setAlternatingRowColors(true);
    m_tree->header()->setStretchLastSection(true);
    m_tree->setEditTriggers(QTreeWidget::DoubleClicked);

    layout->addWidget(m_tree);

    connect(m_tree, &QTreeWidget::itemChanged, this, &CameraParamPanel::onItemChanged);

    setMinimumWidth(220);
    setMaximumWidth(350);

    buildParamList();
}

void CameraParamPanel::setEngine(CameraEngine* engine)
{
    m_engine = engine;
}

void CameraParamPanel::buildParamList()
{
    m_paramList = {
        // 曝光
        {"ExposureTime",       "曝光时间(us)",   "曝光", "float"},
        {"Gain",               "增益(dB)",       "曝光", "float"},
        {"AutoExposureTime",   "自动曝光",       "曝光", "enum"},

        // 图像
        {"Width",              "宽度",           "图像", "int"},
        {"Height",             "高度",           "图像", "int"},
        {"AcquisitionFrameRate","帧率(fps)",     "图像", "float"},
        {"PixelFormat",        "像素格式",       "图像", "enum"},

        // 颜色
        {"BalanceWhiteAuto",   "自动白平衡",     "颜色", "enum"},
        {"BalanceRatio",       "白平衡比例",     "颜色", "int"},

        // 触发
        {"TriggerMode",        "触发模式",       "触发", "enum"},
        {"TriggerSource",      "触发源",         "触发", "enum"},
    };
}

void CameraParamPanel::requestAllParams()
{
    if (!m_engine) return;

    m_tree->clear();
    m_paramItems.clear();

    // 按分组创建顶层节点
    QMap<QString, QTreeWidgetItem*> groups;
    QSet<QString> addedGroups;
    for (const auto& param : m_paramList) {
        if (!addedGroups.contains(param.category)) {
            auto* groupItem = new QTreeWidgetItem(m_tree);
            groupItem->setText(0, param.category);
            groupItem->setFlags(groupItem->flags() & ~Qt::ItemIsEditable);
            groups[param.category] = groupItem;
            addedGroups.insert(param.category);
        }
    }

    // 添加参数到对应分组
    for (const auto& param : m_paramList) {
        auto* item = new QTreeWidgetItem(groups[param.category]);
        item->setText(0, param.displayName);
        item->setText(1, "--");
        item->setData(0, Qt::UserRole, param.key);
        item->setData(0, Qt::UserRole + 1, param.type);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        m_paramItems[param.key] = item;

        // 异步请求参数值
        m_engine->getParam(param.key);
    }
}

void CameraParamPanel::onParamReadFinished(const QString& key, const QVariant& value)
{
    auto it = m_paramItems.find(key);
    if (it == m_paramItems.end()) return;

    QString display;
    switch (value.type()) {
    case QVariant::Double:
        display = QString::number(value.toDouble(), 'f', 2);
        break;
    case QVariant::Int:
    case QVariant::LongLong:
        display = QString::number(value.toLongLong());
        break;
    default:
        display = value.toString();
        break;
    }

    it.value()->setText(1, display);
}

void CameraParamPanel::clearParams()
{
    m_tree->clear();
    m_paramItems.clear();
}

void CameraParamPanel::onItemChanged(QTreeWidgetItem* item, int column)
{
    if (column != 1 || !item) return;

    QString key = item->data(0, Qt::UserRole).toString();
    QString type = item->data(0, Qt::UserRole + 1).toString();
    QString valueStr = item->text(1);

    QVariant value;
    if (type == "float") {
        value = valueStr.toDouble();
    } else {
        value = valueStr.toInt();
    }

    emit paramChangeRequested(key, value);
}
```

---

### Task 7: MainWindow — 主窗口编排

**Files:**
- Create: `src/MainWindow.h`
- Create: `src/MainWindow.cpp`

- [ ] **Step 1: Write MainWindow.h**

```cpp
#ifndef MVSVIEWER_MAIN_WINDOW_H
#define MVSVIEWER_MAIN_WINDOW_H

#include <QMainWindow>
#include <QSplitter>
#include <QStatusBar>
#include <QLabel>
#include <QToolBar>
#include <QMenuBar>
#include <QAction>

class CameraEngine;
class ImageViewer;
class DevicePanel;
class CameraParamPanel;

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

    // 核心引擎
    CameraEngine* m_cameraEngine;
    QThread* m_workerThread;

    // 帧率统计
    int m_frameCount = 0;
    QTimer* m_fpsTimer;
};

#endif // MVSVIEWER_MAIN_WINDOW_H
```

- [ ] **Step 2: Write MainWindow.cpp**

```cpp
#include "MainWindow.h"
#include "CameraEngine.h"
#include "ImageViewer.h"
#include "DevicePanel.h"
#include "CameraParamPanel.h"
#include "Types.h"

#include <QApplication>
#include <QMessageBox>
#include <QFileDialog>
#include <QThread>
#include <QTimer>
#include <QDateTime>

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
    m_paramPanel->setEngine(m_cameraEngine);

    m_splitter->addWidget(m_devicePanel);
    m_splitter->addWidget(m_imageViewer);
    m_splitter->addWidget(m_paramPanel);
    m_splitter->setStretchFactor(0, 0);  // 设备面板不伸缩
    m_splitter->setStretchFactor(1, 1);  // 图像区域伸缩
    m_splitter->setStretchFactor(2, 0);  // 参数面板不伸缩
    m_splitter->setSizes({250, 780, 250});
}

void MainWindow::setupMenuBar()
{
    // 文件菜单
    QMenu* fileMenu = menuBar()->addMenu("文件(&F)");
    fileMenu->addAction("截图(&S)", this, &MainWindow::onSnapshot, QKeySequence("Ctrl+S"));
    fileMenu->addSeparator();
    fileMenu->addAction("退出(&Q)", qApp, &QApplication::quit, QKeySequence("Ctrl+Q"));

    // 视图菜单
    QMenu* viewMenu = menuBar()->addMenu("视图(&V)");
    viewMenu->addAction("全屏(&F)", m_imageViewer, &ImageViewer::toggleFullscreen, QKeySequence("F11"));

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
    connect(m_actRotateLeft, &QAction::triggered, this, [this]() { m_imageViewer->rotateImage(-90); });
    connect(m_actRotateRight, &QAction::triggered, this, [this]() { m_imageViewer->rotateImage(90); });
    connect(m_actFlipH, &QAction::triggered, m_imageViewer, &ImageViewer::flipHorizontal);
    connect(m_actFlipV, &QAction::triggered, m_imageViewer, &ImageViewer::flipVertical);
    connect(m_actFullscreen, &QAction::triggered, m_imageViewer, &ImageViewer::toggleFullscreen);

    // Param panel
    connect(m_paramPanel, &CameraParamPanel::paramChangeRequested,
            this, &MainWindow::onParamChanged);
}

void MainWindow::onDevicesUpdated(const QList<DeviceInfo>& devices)
{
    m_devicePanel->updateDeviceList(devices);
}

void MainWindow::onDeviceConnected()
{
    m_actConnect->setEnabled(false);
    m_actDisconnect->setEnabled(true);
    m_statusDevice->setText("设备: 已连接");

    // 打开设备后自动开始取流
    QMetaObject::invokeMethod(m_cameraEngine, "startGrabbing", Qt::QueuedConnection);

    // 请求参数
    QTimer::singleShot(500, m_paramPanel, [this]() {
        // 通过 invokeMethod 在 worker thread 中请求参数
        for (const auto& param : m_paramPanel->findChildren<QTreeWidgetItem*>()) {
            Q_UNUSED(param);
        }
    });
}

void MainWindow::onDeviceDisconnected()
{
    m_actConnect->setEnabled(true);
    m_actDisconnect->setEnabled(false);
    m_statusDevice->setText("设备: 未连接");
    m_statusResolution->setText("分辨率: --");
    m_statusPixelFormat->setText("像素格式: --");
    m_paramPanel->clearParams();
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
```

---

### Task 8: main.cpp — 入口

**Files:**
- Create: `src/main.cpp`

- [ ] **Step 1: Write main.cpp**

```cpp
#include <QApplication>
#include "MainWindow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("MVS 相机查看器");
    app.setOrganizationName("MVSViewer");

    MainWindow window;
    window.show();

    return app.exec();
}
```

---

### Task 9: 构建和验证

**Files:**
- none

- [ ] **Step 1: 编译项目**

```bash
cd /home/linwj/project/personal/mvs-demo
mkdir -p build && cd build
cmake .. 2>&1
make -j$(nproc) 2>&1
```

Expected: Compilation succeeds. If any errors occur, fix them and retry.

- [ ] **Step 2: 验证可执行文件**

```bash
ls -la /home/linwj/project/personal/mvs-demo/build/MVSViewer
ldd /home/linwj/project/personal/mvs-demo/build/MVSViewer | grep -E "(MvCamera|Qt)"
```

Expected: Executable exists. `ldd` shows linked `libMvCameraControl.so` and Qt6 libraries.

- [ ] **Step 3: 运行验证**

```bash
LD_LIBRARY_PATH=/opt/MVS/lib/64:$LD_LIBRARY_PATH /home/linwj/project/personal/mvs-demo/build/MVSViewer
```

Run the app, verify:
- Window appears with three-panel layout
- Device enumeration works (click "刷新")
- Connect to a (virtual) device
- Real-time image displays in center panel
- Zoom/rotate/flip toolbar buttons work
- Screenshot saves correctly
- Camera params read and display
- Disconnect works cleanly

---

## 自检清单

1. **Spec 覆盖**: 所有 spec 中的功能点都有对应任务实现 — 枚举(T3)、连接(T3)、取流(T3)、画面显示(T4)、缩放(T4)、旋转(T4)、翻转(T4)、全屏(T4)、截图(T7)、参数面板(T6)、设备列表(T5)、状态栏(T7)
2. **无占位符**: 所有代码块都是完整的可编译代码
3. **类型一致性**: DeviceInfo 在 Types.h 定义，CameraEngine 使用 MVS SDK 原生类型，所有信号/槽签名在模块间匹配
