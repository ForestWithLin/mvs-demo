# 海康相机实时图像查看器 — 设计规格

## 概述

基于 Hikvision MVS SDK 和 Qt6.8.3 开发的桌面应用，用于实时读取相机图像、显示图像基本信息，并提供图像操作功能。

## 环境

- **SDK**: `/opt/MVS/lib/64/libMvCameraControl.so`，头文件 `/opt/MVS/include/`
- **Qt**: `/opt/software/Qt6.8.3/`，CMake 集成于 `/opt/software/Qt6.8.3/lib/cmake/`
- **构建**: CMake
- **平台**: Linux x86_64

## 整体架构

```
MainWindow（组合 + 编排）
  ├── DevicePanel（左侧：设备列表）
  ├── ImageViewer（中央：QGraphicsView 子类，实时画面 + 交互操作）
  ├── CameraParamPanel（右侧：相机参数编辑）
  └── StatusBar（底部：帧率/分辨率/像素格式/状态）

CameraEngine（独立 QThread，封装 MVS SDK 调用）
  └── libMvCameraControl.so（Hikvision MVS SDK）

通信方式：CameraEngine 通过 Qt 信号/槽将帧数据（QImage）异步发送到 UI 线程
```

## 模块设计

### 1. CameraEngine

**职责**：封装所有 Hikvision MVS SDK 调用，运行在独立线程。

**公开接口**：

| 方法 | 说明 |
|------|------|
| `enumDevices()` | 枚举所有可用相机 |
| `connectToDevice(int index)` | 连接指定设备 |
| `disconnectDevice()` | 断开设备 |
| `startGrabbing()` | 开始取流 |
| `stopGrabbing()` | 停止取流 |
| `getParam(const QString& key)` | 异步读取相机参数 |
| `setParam(const QString& key, const QVariant& value)` | 设置相机参数 |
| `captureSnapshot(const QString& filePath)` | 截图保存到文件 |

**信号**：
- `deviceListUpdated(QList<DeviceInfo>)`
- `frameReceived(QImage)`
- `deviceDisconnected()`
- `paramReadFinished(QString key, QVariant value)`
- `errorOccurred(int errCode, QString msg)`

**内部流程**：
1. `MV_CC_Initialize()` → `MV_CC_EnumDevices()` 枚举
2. `MV_CC_CreateHandle()` → `MV_CC_OpenDevice()` 打开
3. 独立线程循环 `MV_CC_GetOneFrameTimeout()` 获取帧
4. `MV_CC_ConvertPixelTypeEx()` 转成 RGB888
5. 构造 `QImage(data, w, h, QImage::Format_RGB888)` → `emit frameReceived()`
6. 参数通过 `MV_CC_GetFloatValue/SetFloatValue` 等接口操作

### 2. ImageViewer

**职责**：图像显示 + 交互操作（继承 QGraphicsView）。

| 操作 | 实现 |
|------|------|
| 缩放 | 鼠标滚轮 → `scale(factor, factor)` |
| 平移 | 鼠标中键/右键拖拽 |
| 旋转 90°/180° | `QGraphicsView::rotate()` |
| 水平/垂直翻转 | `scale(-1, 1)` / `scale(1, -1)` |
| 自适应显示 | `fitInView()` |
| 放大镜 | 局部 QGraphicsEllipseItem 放大渲染 |
| 全屏 | QWidget::showFullScreen()，隐藏工具栏 |

**公共方法**：`setImage(QImage)`, `rotateImage(int)`, `flipHorizontal()`, `flipVertical()`, `fitToWindow()`, `zoomIn()`, `zoomOut()`, `toggleFullscreen()`

### 3. CameraParamPanel

- QTreeWidget 分组的参数列表
- 分组：曝光、图像、颜色、IO、触发
- 每个参数项显示名称和当前值（可编辑）
- 修改后调用 `CameraEngine::setParam()`
- 常用参数：ExposureTime, Gain, BalanceRatio, AcquisitionFrameRate, Width, Height, TriggerMode

### 4. DevicePanel

- 左侧 QTreeWidget 列表
- 显示设备名、IP/序列号、传输类型
- 双击连接，右键断开

### 5. MainWindow

- QMainWindow + QSplitter 三栏布局
- 菜单栏：文件（截图/退出）、视图（工具栏/全屏）、帮助（关于）
- 工具栏：连接/断开、截图、缩放（+/—/自适应）、旋转、翻转、全屏
- 状态栏：帧率 | 分辨率 | 像素格式 | 设备状态

## 数据流

```
相机 → MVS SDK → CameraEngine::grabLoop()
  → MV_CC_GetOneFrameTimeout()
  → MV_CC_ConvertPixelTypeEx() (Bayer→RGB888)
  → QImage → emit frameReceived(QImage)
  → ImageViewer::updateFrame(QImage) [主线程]
  → QGraphicsScene 更新 → 屏幕渲染
```

## 错误处理

- CameraEngine 所有 SDK 调用检查返回值，非 MV_OK 则 emit `errorOccurred()`
- 设备断开时自动停止 Grabbing，emit `deviceDisconnected()`，UI 更新设备状态
- 截图失败弹出 QMessageBox 提示
- 参数读写失败在状态栏短暂显示错误信息

## 文件结构

```
mvs-demo/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── MainWindow.h/cpp
│   ├── CameraEngine.h/cpp
│   ├── ImageViewer.h/cpp
│   ├── DevicePanel.h/cpp
│   ├── CameraParamPanel.h/cpp
│   └── Types.h              (DeviceInfo 等公用数据结构)
└── docs/superpowers/specs/
    └── 2026-04-30-mvs-camera-viewer-design.md
```
