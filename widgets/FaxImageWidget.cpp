#include "FaxImageWidget.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QWheelEvent>

// -----------------------------------------------------------------------------
// Construction
// -----------------------------------------------------------------------------

FaxImageWidget::FaxImageWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(420, 260);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
}

// -----------------------------------------------------------------------------
// Public getters
// -----------------------------------------------------------------------------

bool FaxImageWidget::fitToWindow() const
{
    return m_fitToWindow;
}

int FaxImageWidget::zoomPercent() const
{
    return m_zoomPercent;
}

// -----------------------------------------------------------------------------
// Public slots
// -----------------------------------------------------------------------------

void FaxImageWidget::setImage(const QImage &image)
{
    m_image = image;
    clampPanOffset();
    update();
}

void FaxImageWidget::clear()
{
    m_image = QImage();
    m_panOffset = QPoint();
    m_txProgressVisible = false;
    m_txProgress = 0.0;
    update();
}

void FaxImageWidget::setFitToWindow()
{
    if (m_fitToWindow && m_panOffset.isNull()) {
        return;
    }

    m_fitToWindow = true;
    m_panOffset = QPoint();
    emit zoomChanged(m_zoomPercent, true);
    update();
}

void FaxImageWidget::setActualSize()
{
    setZoomPercent(100);
}

void FaxImageWidget::zoomIn()
{
    const int nextZoom = qMin(800, m_fitToWindow ? 125 : m_zoomPercent + 25);
    setZoomPercent(nextZoom);
}

void FaxImageWidget::zoomOut()
{
    const int nextZoom = qMax(25, m_fitToWindow ? 75 : m_zoomPercent - 25);
    setZoomPercent(nextZoom);
}

void FaxImageWidget::setZoomPercent(int percent)
{
    const int boundedPercent = qBound(25, percent, 800);

    if (!m_fitToWindow && m_zoomPercent == boundedPercent) {
        return;
    }

    m_fitToWindow = false;
    m_zoomPercent = boundedPercent;
    clampPanOffset();
    emit zoomChanged(m_zoomPercent, false);
    update();
}

// -----------------------------------------------------------------------------
// QWidget events
// -----------------------------------------------------------------------------


void FaxImageWidget::setTransmitProgress(double progress)
{
    m_txProgressVisible = true;
    m_txProgress = qBound(0.0, progress, 1.0);
    update();
}

void FaxImageWidget::clearTransmitProgress()
{
    if (!m_txProgressVisible) {
        return;
    }

    m_txProgressVisible = false;
    m_txProgress = 0.0;
    update();
}

void FaxImageWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.fillRect(rect(), palette().window());

    if (!m_image.isNull()) {
        const QRect targetRect = imageTargetRect();
        painter.fillRect(targetRect, QColor(18, 18, 18));

        painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
        painter.drawImage(targetRect, m_image);

        if (m_txProgressVisible) {
            const int transmittedHeight = static_cast<int>(targetRect.height() * m_txProgress);

            if (transmittedHeight > 0) {
                const QRect sentRect(targetRect.left(),
                                     targetRect.top(),
                                     targetRect.width(),
                                     qMin(transmittedHeight, targetRect.height()));
                painter.fillRect(sentRect, QColor(0, 0, 0, 90));
            }

            const int y = targetRect.top() +
                          qBound(0, transmittedHeight, targetRect.height() - 1);
            painter.setPen(QPen(QColor(0, 255, 120), 2));
            painter.drawLine(targetRect.left(), y, targetRect.right(), y);
        }
    }

    painter.setPen(QColor(130, 150, 160));
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
}

void FaxImageWidget::wheelEvent(QWheelEvent *event)
{
    if (m_image.isNull()) {
        event->ignore();
        return;
    }

    const int delta = event->angleDelta().y();

    if (delta > 0) {
        zoomIn();
    } else if (delta < 0) {
        zoomOut();
    }

    event->accept();
}

void FaxImageWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && !m_fitToWindow && !m_image.isNull()) {
        m_panning = true;
        m_lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void FaxImageWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_panning) {
        const QPoint delta = event->pos() - m_lastMousePos;
        m_lastMousePos = event->pos();
        m_panOffset += delta;
        clampPanOffset();
        update();
        event->accept();
        return;
    }

    QWidget::mouseMoveEvent(event);
}

void FaxImageWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_panning) {
        m_panning = false;
        unsetCursor();
        event->accept();
        return;
    }

    QWidget::mouseReleaseEvent(event);
}

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

QRect FaxImageWidget::imageTargetRect() const
{
    if (m_image.isNull()) {
        return QRect();
    }

    QSize targetSize;

    if (m_fitToWindow) {
        targetSize = m_image.size().scaled(size(), Qt::KeepAspectRatio);
    } else {
        targetSize = QSize(
            qMax(1, (m_image.width() * m_zoomPercent) / 100),
            qMax(1, (m_image.height() * m_zoomPercent) / 100)
            );
    }

    const QPoint topLeft(
        ((width() - targetSize.width()) / 2) + m_panOffset.x(),
        ((height() - targetSize.height()) / 2) + m_panOffset.y()
        );

    return QRect(topLeft, targetSize);
}

void FaxImageWidget::clampPanOffset()
{
    if (m_fitToWindow || m_image.isNull()) {
        m_panOffset = QPoint();
        return;
    }

    const QSize targetSize(
        qMax(1, (m_image.width() * m_zoomPercent) / 100),
        qMax(1, (m_image.height() * m_zoomPercent) / 100)
        );

    const int maxX = qMax(0, (targetSize.width() - width()) / 2);
    const int maxY = qMax(0, (targetSize.height() - height()) / 2);

    m_panOffset.setX(qBound(-maxX, m_panOffset.x(), maxX));
    m_panOffset.setY(qBound(-maxY, m_panOffset.y(), maxY));
}
