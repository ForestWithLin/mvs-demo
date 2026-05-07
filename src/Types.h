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
