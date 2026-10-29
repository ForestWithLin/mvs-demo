#pragma once

#include "Types.h"
#include <QImage>
#include <QVariant>
#include <memory>

// 仅由 CameraEngine 在工作线程调用；隔离两套 SDK 中同名的像素格式枚举。
// 拥有 SDK 初始化、设备句柄及帧缓存，返回 0 表示成功。
class FvcamBackend
{
public:
    FvcamBackend();
    ~FvcamBackend();
    int enumerate(QList<DeviceInfo>& devices);
    int open(int index);
    void close();
    bool isOpen() const;
    int start();
    int stop();
    // 超时返回成功及空图像；非空图像拥有独立数据，可跨线程发送。
    int grab(QImage& image);
    int read(const QString& key, QVariant& value);
    int write(const QString& key, const QVariant& value);

private:
    struct State;
    std::unique_ptr<State> m_state;
};
