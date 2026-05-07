#include <QCoreApplication>
#include <QTimer>
#include <QImage>
#include <QDebug>
#include <QThread>
#include <cstdio>
#include "MvCameraControl.h"

// 最小化测试：只抓帧，不做任何渲染
int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    MV_CC_Initialize();

    // 枚举设备
    MV_CC_DEVICE_INFO_LIST devList;
    memset(&devList, 0, sizeof(devList));
    int ret = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE | MV_CAMERALINK_DEVICE, &devList);
    if (ret != MV_OK || devList.nDeviceNum == 0) {
        printf("No devices found\n");
        return 1;
    }
    printf("Found %u device(s)\n", devList.nDeviceNum);

    // 连接第一个设备
    void* handle = nullptr;
    ret = MV_CC_CreateHandle(&handle, devList.pDeviceInfo[0]);
    if (ret != MV_OK) { printf("CreateHandle failed: %d\n", ret); return 1; }

    ret = MV_CC_OpenDevice(handle, MV_ACCESS_Exclusive, 0);
    if (ret != MV_OK) { printf("OpenDevice failed: %d\n", ret); return 1; }
    printf("Device opened\n");

    // 开始取流
    ret = MV_CC_StartGrabbing(handle);
    if (ret != MV_OK) { printf("StartGrabbing failed: %d\n", ret); return 1; }
    printf("Started grabbing\n");

    // 设置帧率
    MV_CC_SetFloatValue(handle, "AcquisitionFrameRate", 30.0f);

    int frameCount = 0;
    auto* timer = new QTimer(&app);
    QObject::connect(timer, &QTimer::timeout, [handle, &frameCount]() {
        MV_FRAME_OUT frameOut = {0};
        int ret = MV_CC_GetImageBuffer(handle, &frameOut, 100);
        if (ret != MV_OK) {
            if (ret != MV_E_NODATA) printf("GetImageBuffer error: %d\n", ret);
            return;
        }

        MV_FRAME_OUT_INFO_EX& info = frameOut.stFrameInfo;
        unsigned int rgbSize = info.nWidth * info.nHeight * 3;

        if (rgbSize > 0) {
            static std::vector<unsigned char> buffer;
            buffer.resize(rgbSize);

            MV_CC_PIXEL_CONVERT_PARAM_EX conv = {0};
            conv.nWidth = info.nWidth;
            conv.nHeight = info.nHeight;
            conv.enSrcPixelType = info.enPixelType;
            conv.pSrcData = frameOut.pBufAddr;
            conv.nSrcDataLen = info.nFrameLen;
            conv.enDstPixelType = PixelType_Gvsp_RGB8_Packed;
            conv.pDstBuffer = buffer.data();
            conv.nDstBufferSize = rgbSize;

            ret = MV_CC_ConvertPixelTypeEx(handle, &conv);
            if (ret == MV_OK) {
                // 模拟 QImage 创建和复制（与 CameraEngine 完全相同）
                QImage image(buffer.data(), info.nWidth, info.nHeight,
                             info.nWidth * 3, QImage::Format_RGB888);
                QImage copiedImage = image.copy();
                // 模拟 setImage 中的 QImage 赋值
                QImage currentImage = copiedImage;
                Q_UNUSED(currentImage);

                frameCount++;
                if (frameCount % 30 == 0) {
                    printf("Grabbed %d frames (%dx%d)\n",
                           frameCount, info.nWidth, info.nHeight);
                }
            } else {
                printf("ConvertPixelTypeEx failed: %d\n", ret);
            }
        }

        MV_CC_FreeImageBuffer(handle, &frameOut);
    });

    // 运行 15 秒后退出
    timer->start(33);
    QTimer::singleShot(15000, &app, &QCoreApplication::quit);

    int exitCode = app.exec();

    // 清理
    MV_CC_StopGrabbing(handle);
    MV_CC_CloseDevice(handle);
    MV_CC_DestroyHandle(handle);
    printf("Done, total frames: %d\n", frameCount);

    return exitCode;
}
