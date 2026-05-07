#include "VirtualCameraPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QPixmap>
#include <QMessageBox>
#include <QXmlStreamReader>
#include <QTextStream>
#include <QDateTime>
#include <QRegularExpression>
#include <QPainter>

VirtualCameraPanel::VirtualCameraPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);

    auto* group = new QGroupBox("虚拟相机", this);
    auto* layout = new QVBoxLayout(group);

    // 上传图片
    auto* uploadLayout = new QHBoxLayout();
    m_uploadBtn = new QPushButton("上传图片");
    m_fileNameLabel = new QLabel("未选择文件");
    m_fileNameLabel->setStyleSheet("color: #888;");
    uploadLayout->addWidget(m_uploadBtn);
    uploadLayout->addWidget(m_fileNameLabel, 1);
    layout->addLayout(uploadLayout);

    // 预览区域
    m_previewLabel = new QLabel();
    m_previewLabel->setMinimumSize(200, 150);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setStyleSheet(
        "QLabel { background-color: #2d2d2d; border: 1px solid #555; "
        "border-radius: 4px; color: #888; }");
    m_previewLabel->setText("预览区域");
    layout->addWidget(m_previewLabel);

    // 虚拟设备选择
    layout->addWidget(new QLabel("虚拟设备:"));
    m_deviceCombo = new QComboBox();
    layout->addWidget(m_deviceCombo);

    // 输出格式选择
    layout->addWidget(new QLabel("输出格式:"));
    m_formatCombo = new QComboBox();
    layout->addWidget(m_formatCombo);

    // 转换按钮
    m_convertBtn = new QPushButton("转换并保存为 BMP");
    m_convertBtn->setEnabled(false);
    m_convertBtn->setStyleSheet(
        "QPushButton { background-color: #0078d4; color: white; "
        "padding: 6px; font-weight: bold; border-radius: 4px; }"
        "QPushButton:disabled { background-color: #555; }");
    layout->addWidget(m_convertBtn);

    // 状态信息
    m_statusLabel = new QLabel();
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet("color: #aaa;");
    layout->addWidget(m_statusLabel);

    mainLayout->addWidget(group);

    connect(m_uploadBtn, &QPushButton::clicked, this, &VirtualCameraPanel::onUploadImage);
    connect(m_deviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &VirtualCameraPanel::onDeviceSelected);
    connect(m_convertBtn, &QPushButton::clicked, this, &VirtualCameraPanel::onConvert);

    refreshDevices();
}

void VirtualCameraPanel::refreshDevices()
{
    scanVirtualDevices();
}

void VirtualCameraPanel::scanVirtualDevices()
{
    m_devices.clear();
    m_deviceCombo->clear();

    QDir camerasDir("/var/tmp/VirtualCamera/Cameras");
    if (!camerasDir.exists()) {
        m_deviceCombo->addItem("未找到虚拟设备目录");
        return;
    }

    auto entries = camerasDir.entryInfoList({"Vir*"}, QDir::Dirs | QDir::NoDotAndDotDot);
    if (entries.isEmpty()) {
        m_deviceCombo->addItem("未找到虚拟设备");
        return;
    }

    for (const auto& entry : entries) {
        VirtualDevice dev;
        dev.serial = entry.fileName();
        dev.path = entry.absoluteFilePath();
        dev.resolution = readDeviceResolution(dev.path);

        // 从 XML 读取型号名
        QDir devDir(dev.path);
        auto xmlFiles = devDir.entryList({"*.xml"}, QDir::Files);
        if (!xmlFiles.isEmpty()) {
            QFile xmlFile(devDir.filePath(xmlFiles.first()));
            if (xmlFile.open(QIODevice::ReadOnly)) {
                QXmlStreamReader xml(&xmlFile);
                while (!xml.atEnd() && !xml.hasError()) {
                    if (xml.readNext() == QXmlStreamReader::StartElement
                        && xml.name().toString() == "RegisterDescription") {
                        dev.modelName = xml.attributes().value("ModelName").toString();
                        break;
                    }
                }
            }
        }

        m_devices.append(dev);
    }

    for (const auto& dev : m_devices) {
        QString label = dev.modelName.isEmpty()
            ? dev.serial
            : QString("%1 (%2)").arg(dev.modelName, dev.serial);
        if (dev.resolution.isValid()) {
            label += QString(" [%1x%2]")
                         .arg(dev.resolution.width())
                         .arg(dev.resolution.height());
        }
        m_deviceCombo->addItem(label);
    }

    if (!m_devices.isEmpty()) {
        scanFormatsForDevice(0);
    }
}

QSize VirtualCameraPanel::readDeviceResolution(const QString& devicePath) const
{
    // 从寄存器数据文件读取 Width/Height
    // 格式: "<hex_addr> <value>" 每行一对
    // Width 寄存器地址: 0x30360
    // Height 寄存器地址: 0x303a0
    QDir dir(devicePath);
    auto regFiles = dir.entryList({"*_regfile.dat"}, QDir::Files);
    if (regFiles.isEmpty()) return {};

    QFile file(dir.filePath(regFiles.first()));
    if (!file.open(QIODevice::ReadOnly)) return {};

    int w = 0, h = 0;
    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        // 格式: "HEX_ADDR VALUE"
        int spaceIdx = line.indexOf(' ');
        if (spaceIdx < 0) continue;

        QString addrStr = line.left(spaceIdx).trimmed();
        QString valStr = line.mid(spaceIdx + 1).trimmed();

        bool addrOk = false, valOk = false;
        uint addr = addrStr.toUInt(&addrOk, 16);
        int val = valStr.toInt(&valOk);

        if (!addrOk || !valOk) continue;

        if (addr == 0x30360) w = val;
        if (addr == 0x303a0) h = val;
    }

    if (w > 0 && h > 0) return QSize(w, h);
    return {};
}

void VirtualCameraPanel::scanFormatsForDevice(int index)
{
    m_formatCombo->clear();

    if (index < 0 || index >= m_devices.size()) return;

    auto& device = m_devices[index];
    QDir devDir(device.path);
    auto formatDirs = devDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);

    // 排除非格式目录
    device.formatDirs.clear();
    for (const auto& dir : formatDirs) {
        const QString& name = dir.fileName();
        // 跳过隐藏目录
        if (name.startsWith('.')) continue;
        device.formatDirs.append(name);
    }

    device.formatDirs.sort();

    for (const auto& fmt : device.formatDirs) {
        m_formatCombo->addItem(fmt);
    }

    if (m_formatCombo->count() == 0) {
        m_formatCombo->addItem("无可用格式");
    }
}

void VirtualCameraPanel::onDeviceSelected(int index)
{
    if (index >= 0 && index < m_devices.size()) {
        scanFormatsForDevice(index);
    }
    m_convertBtn->setEnabled(!m_originalImage.isNull()
                             && !m_devices.isEmpty()
                             && m_formatCombo->count() > 0
                             && m_formatCombo->currentText() != "无可用格式");
}

void VirtualCameraPanel::onUploadImage()
{
    QString filePath = QFileDialog::getOpenFileName(
        this, "选择图片", QString(),
        "图片文件 (*.png *.jpg *.jpeg *.bmp);;所有文件 (*)");

    if (filePath.isEmpty()) return;

    QImageReader reader(filePath);
    m_originalImage = reader.read();
    if (m_originalImage.isNull()) {
        QMessageBox::warning(this, "加载失败", "无法加载图片: " + reader.errorString());
        return;
    }

    m_uploadedFilePath = filePath;
    QFileInfo fi(filePath);
    m_fileNameLabel->setText(fi.fileName());
    m_fileNameLabel->setStyleSheet("color: #fff;");

    QPixmap thumb = QPixmap::fromImage(m_originalImage)
                        .scaled(m_previewLabel->size(), Qt::KeepAspectRatio,
                                Qt::SmoothTransformation);
    m_previewLabel->setPixmap(thumb);
    m_previewLabel->setText(QString());

    m_statusLabel->setText(QString("已加载: %1x%2")
                               .arg(m_originalImage.width())
                               .arg(m_originalImage.height()));

    m_convertBtn->setEnabled(!m_devices.isEmpty()
                             && m_formatCombo->count() > 0
                             && m_formatCombo->currentText() != "无可用格式");
}

void VirtualCameraPanel::onConvert()
{
    if (m_originalImage.isNull()) return;

    int devIndex = m_deviceCombo->currentIndex();
    int fmtIndex = m_formatCombo->currentIndex();
    if (devIndex < 0 || devIndex >= m_devices.size()) return;

    const auto& device = m_devices[devIndex];
    if (fmtIndex < 0 || fmtIndex >= device.formatDirs.size()) return;

    const QString& formatDir = device.formatDirs[fmtIndex];

    // 从寄存器文件获取目标分辨率
    QSize targetSize = device.resolution;
    if (!targetSize.isValid()) {
        QMessageBox::warning(this, "转换失败",
                             "无法从寄存器文件读取该虚拟设备的分辨率。\n"
                             "请检查设备目录下的 *_regfile.dat 文件。");
        m_statusLabel->setText("转换失败: 无法获取分辨率");
        return;
    }

    // 缩放图片至目标分辨率
    QImage processed = m_originalImage;
    if (processed.size() != targetSize) {
        processed = processed.scaled(targetSize, Qt::KeepAspectRatio,
                                     Qt::SmoothTransformation);
        if (processed.size() != targetSize) {
            QImage canvas(targetSize, QImage::Format_RGB888);
            canvas.fill(Qt::black);
            QPainter painter(&canvas);
            painter.drawImage(QPoint((targetSize.width() - processed.width()) / 2,
                                     (targetSize.height() - processed.height()) / 2),
                              processed);
            painter.end();
            processed = canvas;
        }
    }

    // 目标路径
    QString targetDir = device.path + "/" + formatDir;
    QDir().mkpath(targetDir);

    // 生成文件名
    QString baseName = QFileInfo(m_uploadedFilePath).completeBaseName();
    if (baseName.isEmpty()) baseName = "Image";
    baseName.replace(QRegularExpression("[^a-zA-Z0-9_\\-]"), "_");
    baseName += "_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString outputPath = targetDir + "/" + baseName + ".bmp";

    bool success = false;
    if (formatDir == "Mono8") {
        success = saveAsMono8Bmp(processed, outputPath);
    } else if (formatDir == "RGB24") {
        success = saveAsRGB24Bmp(processed, outputPath);
    } else if (formatDir == "Bayer8" || formatDir == "Raw") {
        success = saveAsBayerBmp(processed, outputPath);
    } else {
        success = saveAsMono8Bmp(processed, outputPath);
    }

    if (success) {
        m_statusLabel->setText(
            QString("转换成功!\n分辨率: %1x%2\n保存至: %3")
                .arg(targetSize.width()).arg(targetSize.height())
                .arg(outputPath));
        emit statusMessage(
            QString("虚拟相机图片已生成: %1x%2 → %3")
                .arg(targetSize.width()).arg(targetSize.height())
                .arg(outputPath));
    } else {
        QMessageBox::warning(this, "转换失败", "图片保存失败，请重试。");
        m_statusLabel->setText("转换失败");
    }
}

bool VirtualCameraPanel::saveAsMono8Bmp(const QImage& image, const QString& filePath)
{
    QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
    int w = gray.width();
    int h = gray.height();
    int rowSize = ((w * 8 + 31) / 32) * 4;
    int pixelDataSize = rowSize * h;
    int paletteSize = 256 * 4;
    int dataOffset = 14 + 40 + paletteSize;
    int fileSize = dataOffset + pixelDataSize;

    QByteArray buf;
    buf.resize(fileSize);
    buf.fill(0);

    // BITMAPFILEHEADER
    buf[0] = 'B';
    buf[1] = 'M';
    qToLittleEndian<uint32_t>(fileSize,  (uchar*)buf.data() + 2);
    qToLittleEndian<uint32_t>(dataOffset, (uchar*)buf.data() + 10);

    // BITMAPINFOHEADER
    qToLittleEndian<uint32_t>(40,    (uchar*)buf.data() + 14);
    qToLittleEndian<int32_t>(w,     (uchar*)buf.data() + 18);
    qToLittleEndian<int32_t>(h,     (uchar*)buf.data() + 22);
    qToLittleEndian<uint16_t>(1,    (uchar*)buf.data() + 26);
    qToLittleEndian<uint16_t>(8,    (uchar*)buf.data() + 28);
    qToLittleEndian<uint32_t>(0,    (uchar*)buf.data() + 30);
    qToLittleEndian<uint32_t>(pixelDataSize, (uchar*)buf.data() + 34);
    qToLittleEndian<int32_t>(2835,  (uchar*)buf.data() + 38);
    qToLittleEndian<int32_t>(2835,  (uchar*)buf.data() + 42);
    qToLittleEndian<uint32_t>(256,  (uchar*)buf.data() + 46);
    qToLittleEndian<uint32_t>(256,  (uchar*)buf.data() + 50);

    // 灰度调色板
    for (int i = 0; i < 256; i++) {
        buf[54 + i * 4 + 0] = i;
        buf[54 + i * 4 + 1] = i;
        buf[54 + i * 4 + 2] = i;
    }

    // 像素数据 (bottom-up)
    int srcRowSize = gray.bytesPerLine();
    const uchar* src = gray.bits();
    for (int y = 0; y < h; y++) {
        memcpy(buf.data() + dataOffset + y * rowSize,
               src + (h - 1 - y) * srcRowSize, w);
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) return false;
    return file.write(buf) == buf.size();
}

bool VirtualCameraPanel::saveAsRGB24Bmp(const QImage& image, const QString& filePath)
{
    QImage rgb = image.convertToFormat(QImage::Format_RGB888);
    int w = rgb.width();
    int h = rgb.height();
    int rowSize = ((w * 24 + 31) / 32) * 4;
    int pixelDataSize = rowSize * h;
    int dataOffset = 14 + 40;
    int fileSize = dataOffset + pixelDataSize;

    QByteArray buf;
    buf.resize(fileSize);
    buf.fill(0);

    buf[0] = 'B';
    buf[1] = 'M';
    qToLittleEndian<uint32_t>(fileSize,  (uchar*)buf.data() + 2);
    qToLittleEndian<uint32_t>(dataOffset, (uchar*)buf.data() + 10);

    qToLittleEndian<uint32_t>(40,   (uchar*)buf.data() + 14);
    qToLittleEndian<int32_t>(w,    (uchar*)buf.data() + 18);
    qToLittleEndian<int32_t>(h,    (uchar*)buf.data() + 22);
    qToLittleEndian<uint16_t>(1,   (uchar*)buf.data() + 26);
    qToLittleEndian<uint16_t>(24,  (uchar*)buf.data() + 28);
    qToLittleEndian<uint32_t>(0,   (uchar*)buf.data() + 30);
    qToLittleEndian<uint32_t>(pixelDataSize, (uchar*)buf.data() + 34);
    qToLittleEndian<int32_t>(2835, (uchar*)buf.data() + 38);
    qToLittleEndian<int32_t>(2835, (uchar*)buf.data() + 42);
    qToLittleEndian<uint32_t>(0,   (uchar*)buf.data() + 46);
    qToLittleEndian<uint32_t>(0,   (uchar*)buf.data() + 50);

    // BGR bottom-up
    int srcRowSize = rgb.bytesPerLine();
    const uchar* src = rgb.bits();
    for (int y = 0; y < h; y++) {
        const uchar* srcRow = src + (h - 1 - y) * srcRowSize;
        uchar* dstRow = (uchar*)buf.data() + dataOffset + y * rowSize;
        for (int x = 0; x < w; x++) {
            dstRow[x * 3 + 0] = srcRow[x * 3 + 2];
            dstRow[x * 3 + 1] = srcRow[x * 3 + 1];
            dstRow[x * 3 + 2] = srcRow[x * 3 + 0];
        }
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) return false;
    return file.write(buf) == buf.size();
}

bool VirtualCameraPanel::saveAsBayerBmp(const QImage& image, const QString& filePath)
{
    return saveAsMono8Bmp(image, filePath);
}
