# MVSViewer

Qt6 desktop application for displaying and controlling cameras using Hikvision MVS SDK or Fairsion FVCAM SDK.

在左侧设备面板选择 **MVS** 或 **FVCAM**，自动枚举对应 SDK 的设备，然后选择设备连接。
默认使用 MVS；切换 SDK 或刷新设备前需要先断开连接。

FVCAM 接入参考本机 `/opt/FVCAM/samples/C/Grab_Colour`、`Grab_Gray` 和开发指南，分别枚举 GigE / USB3。
传输支持取决于 SDK 发行库：本机库未提供 USB manager，会提示并跳过 USB 枚举，保留 GigE 设备。
预览支持 Mono8、RGB8/BGR8，以及由 SDK 转换的 Bayer 格式；其他格式会明确报错。
高位深灰度相机请先通过厂商工具设置为 Mono8。取流期间不允许修改 FVCAM 的 Width、Height、PixelFormat。
参数按相机实际 GenICam 节点类型访问，不存在或不可访问的节点会在状态栏报错。
虚拟相机面板仍使用 MVS 的虚拟相机机制，不受 SDK 选择影响。

## Features

- **Live camera preview** — Real-time image display from GigE / USB3 / CameraLink cameras
- **Camera parameter control** — Adjust exposure, gain, frame rate, trigger mode, white balance, and more via parameter tree
- **Image operations** — Zoom, pan, rotate, flip, and fit-to-window
- **Snapshot** — Save current frame to PNG/JPG/BMP
- **Frame rate control** — Set target frame rate via toolbar spinbox
- **Multi-device support** — Enumerate and switch between connected cameras

## Dependencies

| Dependency | Version | Path |
|---|---|---|
| Qt | 6.8.3 | `/opt/software/Qt6.8.3/` |
| Hikvision MVS SDK | 4.7.x | `/opt/MVS/` |
| Fairsion FVCAM SDK | 本机安装版本 | `/opt/FVCAM/` |
| CMake | >= 3.20 | |
| C++ compiler | C++17 capable (GCC 9+, Clang 10+) | |

The MVS SDK includes `MvCameraControl.h`, `CameraParams.h`, and `libMvCameraControl.so`.
Install it from the [Hikvision MVS download page](https://www.hikrobotics.com/en/machinevision/productdownload).

## Build

### Normal build (references system dynamic libraries)

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j6
./build/MVSViewer
```

### Portable bundle (self-contained, for distribution)

Packages Qt, MVS and FVCAM SDK `.so` files alongside the executable for use on other Linux machines.
FVCAM 安装路径可通过 CMake 的 `-DFVCAM_ROOT=/path/to/FVCAM` 指定，打包时需设置同名环境变量。

```bash
cmake -B build-portable -S . -DPORTABLE_BUILD=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-portable -j6
./scripts/bundle_portable.sh build-portable
```

Output:
- `build-portable/MVSViewer-Portable/` — self-contained directory
- `build-portable/MVSViewer-Portable.tar.gz` — compressed archive (~23 MB)

On the target machine:

```bash
tar xzf MVSViewer-Portable.tar.gz
cd MVSViewer-Portable
./run.sh
```

> **Note:** The target machine needs X11 runtime libraries (libxcb, libxkbcommon, etc.). These are pre-installed on virtually all Linux desktop distributions.

### Headless grab test

双 SDK 枚举/切换/销毁测试（不连接或修改实际相机）：

```bash
cmake -S . -B build -DBUILD_BACKEND_SMOKE=ON
cmake --build build -j6
ctest --test-dir build --output-on-failure
```

无界面测试清空 `DISPLAY`，避免 MVS 内部渲染库等待失效的 SSH X11 转发。
以下原有抓帧测试仅针对 MVS：

```bash
g++ -std=c++17 test_grab/main.cpp -o test_grab/test_grab \
    -I/opt/MVS/include \
    -I/opt/software/Qt6.8.3/include \
    -I/opt/software/Qt6.8.3/include/QtCore \
    -L/opt/MVS/lib/64 -lMvCameraControl \
    -L/opt/software/Qt6.8.3/lib -lQt6Core
```

## Architecture

```
src/
├── main.cpp                 # Entry point
├── MainWindow.h/.cpp        # Top-level window, toolbar, menus, signal wiring
├── CameraEngine.h/.cpp      # Worker thread: SDK wrapper (enum, connect, grab, params)
├── FvcamBackend.h/.cpp      # FVCAM lifecycle, capture and GenICam parameters; isolated SDK headers
├── ImageViewer.h/.cpp       # Camera image display with zoom/pan/rotate/flip
├── DevicePanel.h/.cpp       # Device list and connect/disconnect controls
├── CameraParamPanel.h/.cpp  # Parameter tree (exposure, gain, frame rate, trigger, etc.)
├── VirtualCameraPanel.h/.cpp
└── Types.h                  # Shared structs (DeviceInfo, CameraParamInfo)
```

### Thread Model

| Thread | Components | Responsibilities |
|---|---|---|
| **Main thread** | MainWindow, ImageViewer, DevicePanel, CameraParamPanel | All UI rendering and interaction |
| **Worker thread** | CameraEngine | Device enumeration, connect/disconnect, frame grabbing (QTimer at ~30 fps), parameter I/O |

Cross-thread communication uses `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` and Qt signals/slots.

### Data Flow

1. App starts → `CameraEngine::enumDevices()` on worker thread
2. User selects device → `connectToDevice()` → `startGrabbing()` → `grabFrame()` every 33ms
3. Each frame: `MV_CC_GetImageBuffer` → RGB888 conversion → `QImage::copy()` → `frameReceived` signal → `ImageViewer::setImage`
4. Parameters: `requestAllParams()` → `getParam()` (probes float → int → enum SDK APIs) → `paramReadFinished` → tree update

## Usage

1. Launch the application
2. Devices are enumerated automatically
3. Select a device and click **Connect** (or toolbar button)
4. Use the toolbar to control frame rate, zoom, rotate, flip
5. Adjust camera parameters in the right-side parameter panel
6. Click **Snapshot** to save the current frame

## License

[MIT](LICENSE)
