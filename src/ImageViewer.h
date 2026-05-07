#ifndef MVSVIEWER_IMAGE_VIEWER_H
#define MVSVIEWER_IMAGE_VIEWER_H

#include <QWidget>
#include <QImage>
#include <QPainter>

class ImageViewer : public QWidget
{
    Q_OBJECT
public:
    explicit ImageViewer(QWidget* parent = nullptr);

    void setImage(const QImage& image);
    QImage currentImage() const { return m_currentImage; }

public slots:
    void zoomIn();
    void zoomOut();
    void fitToWindow();
    void rotateImage(int degrees);
    void flipHorizontal();
    void flipVertical();
    void toggleFullscreen();
    void resetView();

signals:
    void zoomChanged(double factor);
    void fullscreenChanged(bool isFullscreen);

protected:
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    QRectF imageRect() const;
    void updateDisplayImage();

    QImage m_currentImage;       // 原始帧（来自相机）
    QImage m_displayImage;       // 经过旋转/翻转后的图像

    double m_zoomFactor = 1.0;
    double m_rotation = 0.0;
    bool m_flipH = false;
    bool m_flipV = false;
    bool m_isPanning = false;
    QPoint m_lastPanPoint;
    QPointF m_offset;
    bool m_isFullscreen = false;
    bool m_fitToWindowEnabled = true;
};

#endif // MVSVIEWER_IMAGE_VIEWER_H
