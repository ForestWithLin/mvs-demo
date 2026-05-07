# MVSViewer

Qt6 desktop application for displaying and controlling Hikvision machine vision cameras (MVS SDK).

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
| CMake | >= 3.20 | |
| C++ compiler | C++17 capable (GCC 9+, Clang 10+) | |

The MVS SDK includes `MvCameraControl.h`, `CameraParams.h`, and `libMvCameraControl.so`.
Install it from the [Hikvision MVS download page](https://www.hikrobotics.com/en/machinevision/productdownload).

## Build

### Normal build (references system dynamic libraries)

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/MVSViewer
```

### Portable bundle (self-contained, for distribution)

Packages all Qt and MVS SDK `.so` files alongside the executable for use on other Linux machines.

```bash
cmake -B build-portable -S . -DPORTABLE_BUILD=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-portable -j$(nproc)
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
