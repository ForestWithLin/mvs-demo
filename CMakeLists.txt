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

# --- Fairsion FVCAM SDK ---
set(FVCAM_ROOT "/opt/FVCAM" CACHE PATH "FVCAM SDK 安装目录")
find_path(FVCAM_INCLUDE_DIR FvCamera.h PATHS "${FVCAM_ROOT}/include" REQUIRED NO_DEFAULT_PATH)
find_library(FVCAM_LIBRARY FvCamera PATHS "${FVCAM_ROOT}/lib" REQUIRED NO_DEFAULT_PATH)

# --- Source files ---
set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/CameraEngine.cpp
    src/FvcamBackend.cpp
    src/ImageViewer.cpp
    src/DevicePanel.cpp
    src/CameraParamPanel.cpp
    src/VirtualCameraPanel.cpp
)

set(HEADERS
    src/MainWindow.h
    src/CameraEngine.h
    src/FvcamBackend.h
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
    ${FVCAM_INCLUDE_DIR}
)

target_link_directories(${PROJECT_NAME} PRIVATE ${MVS_LIB_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
    ${MVS_LIBS}
    ${FVCAM_LIBRARY}
)

option(BUILD_BACKEND_SMOKE "构建不连接相机的双 SDK 冒烟测试" OFF)
if(BUILD_BACKEND_SMOKE)
    enable_testing()
    add_executable(backend_smoke test_backends/main.cpp src/CameraEngine.cpp src/FvcamBackend.cpp)
    target_include_directories(backend_smoke PRIVATE src ${MVS_INCLUDE_DIR} ${FVCAM_INCLUDE_DIR})
    target_link_libraries(backend_smoke PRIVATE Qt6::Core Qt6::Gui ${MVS_LIBS} ${FVCAM_LIBRARY})
    add_test(NAME backend_smoke COMMAND backend_smoke)
    # SDK 会尝试打开 X11；无界面测试不能继承失效的 SSH DISPLAY。
    set_tests_properties(backend_smoke PROPERTIES TIMEOUT 30 ENVIRONMENT "DISPLAY=")
endif()
