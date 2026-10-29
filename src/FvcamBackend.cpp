#include "FvcamBackend.h"
#include "FvCamera.h"
#include <QDebug>
#include <cstring>
#include <limits>
#include <vector>

struct FvcamBackend::State {
    bool initialized = false;
    bool grabbing = false;
    void* handle = nullptr;
    std::vector<FV_DEVICE_INFO> devices;
    std::vector<unsigned char> rgb;
    bool mono = false;
    bool bgr = false;
};

FvcamBackend::FvcamBackend() : m_state(std::make_unique<State>()) {}

FvcamBackend::~FvcamBackend()
{
    close();
    if (m_state->initialized) FV_Terminate();
}

int FvcamBackend::enumerate(QList<DeviceInfo>& devices)
{
    auto& s = *m_state;
    devices.clear();
    s.devices.clear();
    if (!s.initialized) {
        int ret = FV_Initialize();
        if (ret != FV_SUCCESS) return ret;
        s.initialized = true;
    }
    // FVCAM 的传输类型不是 bitmask（CSI2 为 3），分别枚举并复制 SDK 数据。
    int supportedTransports = 0;
    for (unsigned int transport : {FV_GIGE_DEVICE, FV_USB_DEVICE}) {
        FV_DEVICE_INFO_LIST* list = nullptr;
        int ret = FV_EnumDevices(transport, &list);
        // 部分发行库只编入 GigE；缺少 USB manager 不应丢弃已枚举的 GigE 设备。
        if (ret == FV_E_UNKNOWN_TYPE) {
            qWarning() << "FVCAM SDK 未提供传输类型:" << transport;
            continue;
        }
        if (ret != FV_SUCCESS) {
            s.devices.clear();
            return ret;
        }
        ++supportedTransports;
        if (list) {
            for (unsigned int i = 0; i < list->nDeviceNum; ++i)
                s.devices.push_back(list->scDeviceInfo[i]);
        }
    }
    if (supportedTransports == 0) return FV_E_UNKNOWN_TYPE;
    for (const auto& raw : s.devices) {
        DeviceInfo info;
        info.indexInSdk = devices.size();
        auto fill = [&info](const auto& data) {
            info.name = QString::fromLocal8Bit(data.chUserDefinedName,
                strnlen(data.chUserDefinedName, sizeof(data.chUserDefinedName)));
            if (info.name.isEmpty())
                info.name = QString::fromLocal8Bit(data.chModelName,
                    strnlen(data.chModelName, sizeof(data.chModelName)));
            info.serialNumber = QString::fromLatin1(data.chSerialNumber,
                strnlen(data.chSerialNumber, sizeof(data.chSerialNumber)));
        };
        if (raw.nTLayerType == FV_GIGE_DEVICE) {
            const auto& data = raw.SpecialInfo.stGigEInfo;
            fill(data);
            info.transportType = "GigE";
            info.ipAddress = QString("%1.%2.%3.%4").arg(data.nCurrentIp >> 24)
                .arg((data.nCurrentIp >> 16) & 255).arg((data.nCurrentIp >> 8) & 255)
                .arg(data.nCurrentIp & 255);
        } else {
            fill(raw.SpecialInfo.stUsb3VInfo);
            info.transportType = "USB3";
        }
        devices.append(info);
    }
    return FV_SUCCESS;
}

int FvcamBackend::open(int index)
{
    auto& s = *m_state;
    if (index < 0 || static_cast<size_t>(index) >= s.devices.size())
        return FV_E_INVALID_DEVICE;
    close();
    int ret = FV_CreateHandle(&s.handle, &s.devices[index]);
    if (ret != FV_SUCCESS) return ret;
    ret = FV_OpenDevice(s.handle);
    if (ret != FV_SUCCESS) {
        FV_DestroyHandle(s.handle);
        s.handle = nullptr;
    }
    return ret;
}

void FvcamBackend::close()
{
    stop();
    if (m_state->handle) {
        FV_CloseDevice(m_state->handle);
        FV_DestroyHandle(m_state->handle);
        m_state->handle = nullptr;
    }
}

bool FvcamBackend::isOpen() const { return m_state->handle != nullptr; }

int FvcamBackend::start()
{
    auto& s = *m_state;
    QVariant width, height, pixel;
    int ret = read("Width", width);
    if (ret != FV_SUCCESS) return ret;
    ret = read("Height", height);
    if (ret != FV_SUCCESS) return ret;
    ret = read("PixelFormat", pixel);
    if (ret != FV_SUCCESS) return ret;
    const qint64 w = width.toLongLong(), h = height.toLongLong();
    if (w <= 0 || h <= 0 || w > 65535 || h > 65535 ||
        w * h * 3 > std::numeric_limits<unsigned int>::max())
        return FV_E_PARAMETER_RANGE;
    s.bgr = pixel.toLongLong() == PixelType_Gvsp_BGR8_Packed;
    switch (pixel.toLongLong()) {
    case PixelType_Gvsp_Mono8:
        s.mono = true;
        break;
    case PixelType_Gvsp_Mono1p: case PixelType_Gvsp_Mono2p: case PixelType_Gvsp_Mono4p:
    case PixelType_Gvsp_Mono8_Signed: case PixelType_Gvsp_Mono10:
    case PixelType_Gvsp_Mono10_Packed: case PixelType_Gvsp_Mono12:
    case PixelType_Gvsp_Mono12_Packed: case PixelType_Gvsp_Mono14: case PixelType_Gvsp_Mono16:
        return FV_E_INVALID_PIX_FMT;
    case PixelType_Gvsp_RGB8_Packed: case PixelType_Gvsp_BGR8_Packed:
    case PixelType_Gvsp_BayerGR8: case PixelType_Gvsp_BayerRG8:
    case PixelType_Gvsp_BayerGB8: case PixelType_Gvsp_BayerBG8:
    case PixelType_Gvsp_BayerGR10: case PixelType_Gvsp_BayerRG10:
    case PixelType_Gvsp_BayerGB10: case PixelType_Gvsp_BayerBG10:
    case PixelType_Gvsp_BayerGR12: case PixelType_Gvsp_BayerRG12:
    case PixelType_Gvsp_BayerGB12: case PixelType_Gvsp_BayerBG12:
    case PixelType_Gvsp_BayerGR10_Packed: case PixelType_Gvsp_BayerRG10_Packed:
    case PixelType_Gvsp_BayerGB10_Packed: case PixelType_Gvsp_BayerBG10_Packed:
    case PixelType_Gvsp_BayerGR12_Packed: case PixelType_Gvsp_BayerRG12_Packed:
    case PixelType_Gvsp_BayerGB12_Packed: case PixelType_Gvsp_BayerBG12_Packed:
    case PixelType_Gvsp_BayerGR16: case PixelType_Gvsp_BayerRG16:
    case PixelType_Gvsp_BayerGB16: case PixelType_Gvsp_BayerBG16:
        s.mono = false;
        s.rgb.resize(static_cast<size_t>(w * h * 3));
        break;
    default:
        return FV_E_INVALID_PIX_FMT;
    }
    ret = FV_StartGrabbing(s.handle);
    s.grabbing = ret == FV_SUCCESS;
    return ret;
}

int FvcamBackend::stop()
{
    if (!m_state->grabbing) return FV_SUCCESS;
    m_state->grabbing = false;
    return FV_StopGrabbing(m_state->handle);
}

int FvcamBackend::grab(QImage& image)
{
    auto& s = *m_state;
    image = QImage();
    FV_FRAME_OUT_INFO info{};
    unsigned char* data = s.rgb.data();
    int ret = s.mono ? FV_GetImageBuffer(s.handle, &data, &info, 100)
        : FV_GetImageForRGB(s.handle, data, static_cast<unsigned int>(s.rgb.size()), &info, 100);
    if (ret == FV_E_TIMEOUT) return FV_SUCCESS;
    if (ret != FV_SUCCESS) return ret;
    const size_t bytes = size_t(info.nWidth) * info.nHeight * (s.mono ? 1 : 3);
    if (!data || bytes == 0 || (s.mono ? info.nFrameLen < bytes : s.rgb.size() < bytes))
        return FV_E_PARAMETER_RANGE;
    // 内部灰度缓存在下次取帧时失效；RGB 缓存也会复用，发送前必须复制。
    QImage view(data, info.nWidth, info.nHeight, info.nWidth * (s.mono ? 1 : 3),
                s.mono ? QImage::Format_Grayscale8 :
                s.bgr ? QImage::Format_BGR888 : QImage::Format_RGB888);
    image = (s.mono || s.bgr) ? view.convertToFormat(QImage::Format_RGB888) : view.copy();
    return FV_SUCCESS;
}

int FvcamBackend::read(const QString& key, QVariant& value)
{
    auto* handle = m_state->handle;
    const auto name = key.toUtf8();
    const auto access = FV_GetAccessMode(handle, name.constData());
    if (access != FV_AM_RO && access != FV_AM_RW) return FV_E_PARAMETER_RANGE;
    char type[256]{};
    int ret = FV_GetInterfaceTypeName(handle, name.constData(), type);
    if (ret != FV_SUCCESS) return ret;
    // Getter 直接返回数值而非错误码，必须先检查节点访问权限与类型。
    if (std::strcmp(type, "IFloat") == 0)
        value = double(FV_GetFloatValue(handle, name.constData()));
    else if (std::strcmp(type, "IInteger") == 0)
        value = qlonglong(FV_GetIntValue(handle, name.constData()));
    else if (std::strcmp(type, "IEnumeration") == 0)
        value = qlonglong(FV_GetEnumIntValue(handle, name.constData()));
    else if (std::strcmp(type, "IBoolean") == 0)
        value = FV_GetBoolValue(handle, name.constData());
    else return FV_E_UNKNOWN_TYPE;
    return FV_SUCCESS;
}

int FvcamBackend::write(const QString& key, const QVariant& value)
{
    auto& s = *m_state;
    // 取流期间固定帧缓存尺寸和转换路径；修改格式/ROI 需先停止取流。
    if (s.grabbing && (key == "Width" || key == "Height" || key == "PixelFormat"))
        return FV_E_STREAMING;
    const auto name = key.toUtf8();
    const auto access = FV_GetAccessMode(s.handle, name.constData());
    if (access != FV_AM_WO && access != FV_AM_RW) return FV_E_PARAMETER_RANGE;
    char type[256]{};
    int ret = FV_GetInterfaceTypeName(s.handle, name.constData(), type);
    if (ret != FV_SUCCESS) return ret;
    if (std::strcmp(type, "IFloat") == 0)
        return FV_SetFloatValue(s.handle, name.constData(), value.toFloat());
    if (std::strcmp(type, "IInteger") == 0)
        return FV_SetIntValue(s.handle, name.constData(), value.toLongLong());
    if (std::strcmp(type, "IEnumeration") == 0)
        return FV_SetEnumIntValue(s.handle, name.constData(), value.toLongLong());
    if (std::strcmp(type, "IBoolean") == 0)
        return FV_SetBoolValue(s.handle, name.constData(), value.toBool());
    return FV_E_UNKNOWN_TYPE;
}
