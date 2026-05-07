#include "ImageViewer.h"
#include <QWheelEvent>
#include <QMouseEvent>
#include <QScrollBar>
#include <QtMath>

ImageViewer::ImageViewer(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(false);
    setMinimumSize(200, 200);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void ImageViewer::setImage(const QImage& image)
{
    if (image.isNull()) return;

    m_currentImage = image;

    // 仅当图像尺寸变化时才重算 fitToWindow
    static int lastW = 0, lastH = 0;
    if (m_fitToWindowEnabled && (image.width() != lastW || image.height() != lastH)) {
        fitToWindow();
        lastW = image.width();
        lastH = image.height();
    }

    update(); // 触发重绘
}

void ImageViewer::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.setRenderHint(QPainter::Antialiasing);

    // 填充背景
    painter.fillRect(rect(), QColor(45, 45, 45));

    if (m_currentImage.isNull()) {
        painter.setPen(QColor(160, 160, 160));
        painter.drawText(rect(), Qt::AlignCenter, "无图像");
        return;
    }

    painter.save();

    // 平移到视口中心
    painter.translate(rect().center());

    // 应用缩放
    painter.scale(m_zoomFactor, m_zoomFactor);

    // 应用旋转
    painter.rotate(m_rotation);

    // 应用翻转
    if (m_flipH) painter.scale(-1, 1);
    if (m_flipV) painter.scale(1, -1);

    // 应用平移偏移
    painter.translate(m_offset);

    // 绘制图像（居中）
    QRectF imgRect = imageRect();
    painter.drawImage(imgRect, m_currentImage);

    painter.restore();
}

void ImageViewer::zoomIn()
{
    m_fitToWindowEnabled = false;
    m_zoomFactor *= 1.25;
    emit zoomChanged(m_zoomFactor);
    update();
}

void ImageViewer::zoomOut()
{
    m_fitToWindowEnabled = false;
    m_zoomFactor *= 0.8;
    emit zoomChanged(m_zoomFactor);
    update();
}

void ImageViewer::fitToWindow()
{
    if (m_currentImage.isNull()) return;

    m_fitToWindowEnabled = true;
    m_rotation = 0.0;
    m_flipH = false;
    m_flipV = false;
    m_offset = QPointF(0, 0);

    // 计算合适的缩放比例
    double scaleX = static_cast<double>(width()) / m_currentImage.width();
    double scaleY = static_cast<double>(height()) / m_currentImage.height();
    m_zoomFactor = qMin(scaleX, scaleY) * 0.9; // 留 10% 边距

    emit zoomChanged(m_zoomFactor);
    update();
}

void ImageViewer::rotateImage(int degrees)
{
    m_fitToWindowEnabled = false;
    m_rotation += degrees;
    update();
}

void ImageViewer::flipHorizontal()
{
    m_fitToWindowEnabled = false;
    m_flipH = !m_flipH;
    update();
}

void ImageViewer::flipVertical()
{
    m_fitToWindowEnabled = false;
    m_flipV = !m_flipV;
    update();
}

void ImageViewer::toggleFullscreen()
{
    if (m_isFullscreen) {
        if (parentWidget()) parentWidget()->showNormal();
    } else {
        if (parentWidget()) parentWidget()->showFullScreen();
    }
    m_isFullscreen = !m_isFullscreen;
    emit fullscreenChanged(m_isFullscreen);
}

void ImageViewer::resetView()
{
    fitToWindow();
}

void ImageViewer::wheelEvent(QWheelEvent* event)
{
    m_fitToWindowEnabled = false;
    double factor = (event->angleDelta().y() > 0) ? 1.15 : 0.87;
    m_zoomFactor *= factor;
    emit zoomChanged(m_zoomFactor);
    update();
    event->accept();
}

void ImageViewer::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton) {
        m_isPanning = true;
        m_lastPanPoint = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void ImageViewer::mouseMoveEvent(QMouseEvent* event)
{
    if (m_isPanning) {
        QPointF delta = event->position() - QPointF(m_lastPanPoint);
        m_lastPanPoint = event->pos();
        // 将屏幕像素偏移转换为图像坐标偏移
        m_offset += QPointF(delta.x() / m_zoomFactor, delta.y() / m_zoomFactor);
        update();
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void ImageViewer::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton) {
        m_isPanning = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void ImageViewer::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (m_fitToWindowEnabled && !m_currentImage.isNull()) {
        fitToWindow();
    }
}

QRectF ImageViewer::imageRect() const
{
    if (m_currentImage.isNull()) return QRectF();
    double hw = m_currentImage.width() / 2.0;
    double hh = m_currentImage.height() / 2.0;
    return QRectF(-hw, -hh, m_currentImage.width(), m_currentImage.height());
}
