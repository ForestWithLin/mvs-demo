cmake_minimum_required(VERSION 3.20)
project(MVSViewer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")

# --- 便携式打包选项 ---
option(PORTABLE_BUILD "构建便携式包：将依赖库打包到执行文件目录" OFF)
if(PORTABLE_BUILD)
    # 设置 RPATH 为 $ORIGIN/lib，运行时自动在 lib/ 下找依赖库
    set(CMAKE_BUILD_RPATH "$ORIGIN/lib")
    set(CMAKE_INSTALL_RPATH "$ORIGIN/lib")
    set(CMAKE_BUILD_RPATH_USE_ORIGIN ON)
    message(STATUS "便携式构建模式: RPATH 已设为 \$ORIGIN/lib")
endif()

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
    src/VirtualCameraPanel.cpp
)

set(HEADERS
    src/MainWindow.h
    src/CameraEngine.h
    src/ImageViewer.h
    src/DevicePanel.h
    src/CameraParamPanel.h
    src/Types.h
    src/VirtualCameraPanel.h
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
