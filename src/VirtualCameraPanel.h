#ifndef MVSVIEWER_VIRTUAL_CAMERA_PANEL_H
#define MVSVIEWER_VIRTUAL_CAMERA_PANEL_H

#include <QWidget>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QImage>
#include <QSize>

class VirtualCameraPanel : public QWidget
{
    Q_OBJECT
public:
    explicit VirtualCameraPanel(QWidget* parent = nullptr);

    void refreshDevices();

signals:
    void statusMessage(const QString& msg);

private slots:
    void onUploadImage();
    void onDeviceSelected(int index);
    void onConvert();

private:
    struct VirtualDevice {
        QString serial;         // 序列号（目录名）
        QString modelName;      // 型号（从 XML 读取）
        QStringList formatDirs; // 支持的格式目录名
        QString path;           // 完整路径
        QSize resolution;       // 从寄存器文件读取的分辨率
    };

    void scanVirtualDevices();
    void scanFormatsForDevice(int index);
    QSize readDeviceResolution(const QString& devicePath) const;

    bool saveAsMono8Bmp(const QImage& image, const QString& filePath);
    bool saveAsRGB24Bmp(const QImage& image, const QString& filePath);
    bool saveAsBayerBmp(const QImage& image, const QString& filePath);

    QPushButton* m_uploadBtn;
    QLabel* m_fileNameLabel;
    QLabel* m_previewLabel;
    QComboBox* m_deviceCombo;
    QComboBox* m_formatCombo;
    QPushButton* m_convertBtn;
    QLabel* m_statusLabel;

    QImage m_originalImage;
    QString m_uploadedFilePath;
    QList<VirtualDevice> m_devices;
};

#endif // MVSVIEWER_VIRTUAL_CAMERA_PANEL_H
