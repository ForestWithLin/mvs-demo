# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
# Configure (debug)
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build -j$(nproc)

# Run
./build/MVSViewer

# Build & run headless grab test
g++ -std=c++17 test_grab/main.cpp -o test_grab/test_grab \
    -I/opt/MVS/include -I/opt/software/Qt6.8.3/include \
    -L/opt/MVS/lib/64 -lMvCameraControl \
    -L/opt/software/Qt6.8.3/lib -lQt6Core -I/opt/software/Qt6.8.3/include/QtCore

# Clean build
rm -rf build && mkdir build && cmake -B build && cmake --build build -j$(nproc)
```

## Dependencies

- **Qt 6.8.3** at `/opt/software/Qt6.8.3/`
- **Hikvision MVS SDK** at `/opt/MVS/` (includes `MvCameraControl.h`, `CameraParams.h`, `libMvCameraControl.so`)

## Project Architecture

Qt6 desktop application for displaying and controlling Hikvision machine vision cameras.

### Thread Model

- **Main thread**: All UI (MainWindow, ImageViewer, DevicePanel, CameraParamPanel)
- **Worker thread**: CameraEngine (enumeration, connect/disconnect, frame grabbing, parameter read/write)
- Cross-thread communication via `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` and Qt signals/slots

### Key Components

| Component | File(s) | Role |
|---|---|---|
| **MainWindow** | `src/MainWindow.h/.cpp` | Top-level window; owns splitter layout, toolbar, menus, status bar, and all UI-to-engine signal wiring |
| **CameraEngine** | `src/CameraEngine.h/.cpp` | Runs on worker QThread; wraps Hikvision MVS SDK (enum devices, open/close, grab frames with QTimer polling at ~30fps, get/set params via float/int/enum type probing) |
| **ImageViewer** | `src/ImageViewer.h/.cpp` | Custom QWidget for rendering camera frames with zoom, pan (middle/right button), rotate, flip, fit-to-window |
| **DevicePanel** | `src/DevicePanel.h/.cpp` | Left panel with device list (QTreeWidget) and connect/disconnect/refresh buttons |
| **CameraParamPanel** | `src/CameraParamPanel.h/.cpp` | Right panel with categorized camera parameter tree (exposure, gain, frame rate, trigger, etc.) |
| **Types** | `src/Types.h` | Shared data structs: `DeviceInfo`, `CameraParamInfo` |

### Data Flow

1. App starts → `CameraEngine::enumDevices()` invoked on worker thread
2. User selects device → `connectToDevice(index)` → `startGrabbing()` → `grabFrame()` fires every 33ms via QTimer
3. Each frame: `MV_CC_GetImageBuffer` → convert to RGB888 → `QImage::copy()` → emit `frameReceived` → `ImageViewer::setImage`
4. Parameters requested via `getParam()` (tries float → int → enum SDK APIs), displayed in `CameraParamPanel`

### Image Operations

All image transforms (zoom, rotate, flip, pan) are applied in `ImageViewer::paintEvent()` via QPainter transformations — no pixel data is modified.

### test_grab

`test_grab/main.cpp` is a standalone headless grab test (QCoreApplication, no GUI) that validates frame grabbing and pixel conversion logic against the CameraEngine implementation.
