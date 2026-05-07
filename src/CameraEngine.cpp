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
            MV_CamL_DEV_INFO& cl = pInfo->SpecialInfo.stCamLInfo;
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

void CameraEngine::setTargetFrameRate(double fps)
{
    if (fps <= 0) return;

    // 调整取流定时器间隔实现软件限帧
    if (m_grabTimer && m_isGrabbing) {
        int interval = qMax(1, static_cast<int>(1000.0 / fps));
        m_grabTimer->setInterval(interval);
    }

    // 同时尝试设置相机端帧率（部分相机支持）
    if (m_handle) {
        MV_CC_SetFloatValue(m_handle, "AcquisitionFrameRate", static_cast<float>(fps));
    }

    emit statusMessage(QString("目标帧率: %1 fps").arg(fps, 0, 'f', 1));
}

void CameraEngine::grabFrame()
{
    if (!m_handle || !m_isGrabbing) return;

    // MV_CC_GetImageBuffer 由 SDK 内部管理缓冲区，无溢出风险
    MV_FRAME_OUT frameOut = {0};
    int ret = MV_CC_GetImageBuffer(m_handle, &frameOut, 100);
    if (ret != MV_OK) {
        if (ret != MV_E_NODATA) {
            emit errorOccurred(ret, "取帧失败");
        }
        return;
    }

    MV_FRAME_OUT_INFO_EX& info = frameOut.stFrameInfo;
    unsigned int rgbSize = info.nWidth * info.nHeight * 3;
    if (rgbSize == 0) {
        MV_CC_FreeImageBuffer(m_handle, &frameOut);
        return;
    }

    m_rgbBuffer.resize(rgbSize);

    // 转换为 RGB888
    MV_CC_PIXEL_CONVERT_PARAM_EX convertParam = {0};
    convertParam.nWidth = info.nWidth;
    convertParam.nHeight = info.nHeight;
    convertParam.enSrcPixelType = info.enPixelType;
    convertParam.pSrcData = frameOut.pBufAddr;
    convertParam.nSrcDataLen = info.nFrameLen;
    convertParam.enDstPixelType = PixelType_Gvsp_RGB8_Packed;
    convertParam.pDstBuffer = m_rgbBuffer.data();
    convertParam.nDstBufferSize = rgbSize;

    ret = MV_CC_ConvertPixelTypeEx(m_handle, &convertParam);
    if (ret != MV_OK) {
        MV_CC_FreeImageBuffer(m_handle, &frameOut);
        emit errorOccurred(ret, "像素格式转换失败");
        return;
    }

    QImage image(m_rgbBuffer.data(), info.nWidth, info.nHeight,
                 info.nWidth * 3, QImage::Format_RGB888);

    // QImage 是浅拷贝，需要复制数据
    QImage copiedImage = image.copy();
    emit frameReceived(copiedImage);

    MV_CC_FreeImageBuffer(m_handle, &frameOut);
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

    // 根据 QVariant 实际类型选择 SDK API，避免将 int/enum 误传入 SetFloatValue
    auto typeId = value.metaType().id();
    if (typeId == QMetaType::Double || typeId == QMetaType::Float) {
        ret = MV_CC_SetFloatValue(m_handle, keyBytes.constData(), static_cast<float>(value.toDouble()));
    } else {
        // int / enum / uint：先尝试 SetEnumValue，回退到 SetIntValueEx
        ret = MV_CC_SetEnumValue(m_handle, keyBytes.constData(), value.toUInt());
        if (ret != MV_OK) {
            ret = MV_CC_SetIntValueEx(m_handle, keyBytes.constData(), value.toLongLong());
        }
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
        return QString::fromLatin1((const char*)pInfo->SpecialInfo.stCamLInfo.chFamilyName);
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
