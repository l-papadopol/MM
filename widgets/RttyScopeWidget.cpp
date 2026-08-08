#include "RttyScopeWidget.h"

#include <QPainter>
#include <QPaintEvent>
#include <QPainterPath>
#include <QRadialGradient>
#include <QtMath>

RttyScopeWidget::RttyScopeWidget(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(230, 230);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    m_animTimer.setInterval(50);
    connect(&m_animTimer, &QTimer::timeout,
            this, &RttyScopeWidget::advanceAnimation);
    m_animTimer.start();
}

void RttyScopeWidget::setTuningMetrics(double markLevel,
                                       double spaceLevel,
                                       double snrLike,
                                       bool locked)
{
    m_markLevel = (0.82 * m_markLevel) + (0.18 * qMax(0.0, markLevel));
    m_spaceLevel = (0.82 * m_spaceLevel) + (0.18 * qMax(0.0, spaceLevel));
    m_snrLike = (0.85 * m_snrLike) + (0.15 * qMax(0.0, snrLike));
    m_locked = locked;

    if (!locked && m_framesSinceTraceUpdate > 8) {
        m_tracePoints.clear();
    }

    update();
}

void RttyScopeWidget::setTrace(const QVector<QPointF> &tracePoints,
                               double snrLike,
                               bool locked)
{
    m_snrLike = (0.75 * m_snrLike) + (0.25 * qMax(0.0, snrLike));
    m_locked = locked;

    /*
     * Draw only real incoming points.  The old widget ignored tracePoints and
     * painted an ideal crossed ellipse, which looked nice but was not a useful
     * tuning instrument because it appeared even without a usable signal.
     */
    if (locked && snrLike >= 1.5 && !tracePoints.isEmpty()) {
        m_tracePoints = tracePoints;
        const int maxPoints = 420;
        if (m_tracePoints.size() > maxPoints) {
            m_tracePoints.remove(0, m_tracePoints.size() - maxPoints);
        }
        m_framesSinceTraceUpdate = 0;
    } else {
        m_tracePoints.clear();
        m_framesSinceTraceUpdate = 1000;
    }

    update();
}

void RttyScopeWidget::advanceAnimation()
{
    if (m_framesSinceTraceUpdate < 1000) {
        ++m_framesSinceTraceUpdate;
    }

    if (m_framesSinceTraceUpdate > 24) {
        m_tracePoints.clear();
        m_locked = false;
    }

    update();
}

void RttyScopeWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), palette().window());

    const int side = qMin(width(), height());
    const QRectF outerRect((width() - side) * 0.5 + 3.0,
                           (height() - side) * 0.5 + 3.0,
                           side - 6.0,
                           side - 6.0);
    const QPointF center = outerRect.center();
    const double radius = outerRect.width() * 0.5;

    QRadialGradient bezelGradient(center, radius);
    bezelGradient.setColorAt(0.0, QColor(52, 52, 52));
    bezelGradient.setColorAt(0.72, QColor(24, 24, 24));
    bezelGradient.setColorAt(1.0, QColor(8, 8, 8));
    painter.setPen(QPen(QColor(12, 12, 12), 2.0));
    painter.setBrush(bezelGradient);
    painter.drawEllipse(outerRect);

    const QRectF screenRect = outerRect.adjusted(radius * 0.12,
                                                 radius * 0.12,
                                                 -radius * 0.12,
                                                 -radius * 0.12);
    const double screenRadius = screenRect.width() * 0.5;

    QPainterPath screenPath;
    screenPath.addEllipse(screenRect);

    painter.save();
    painter.setClipPath(screenPath);

    QRadialGradient screenGradient(screenRect.center(), screenRadius);
    screenGradient.setColorAt(0.0, QColor(4, 12, 6));
    screenGradient.setColorAt(0.75, QColor(1, 6, 2));
    screenGradient.setColorAt(1.0, QColor(0, 0, 0));
    painter.fillRect(screenRect, screenGradient);

    painter.setPen(QPen(QColor(95, 105, 95, 70), 1.0));
    painter.drawEllipse(screenRect.adjusted(screenRadius * 0.25,
                                            screenRadius * 0.25,
                                            -screenRadius * 0.25,
                                            -screenRadius * 0.25));
    painter.drawEllipse(screenRect.adjusted(screenRadius * 0.50,
                                            screenRadius * 0.50,
                                            -screenRadius * 0.50,
                                            -screenRadius * 0.50));
    painter.drawLine(QPointF(center.x(), screenRect.top() + screenRadius * 0.08),
                     QPointF(center.x(), screenRect.bottom() - screenRadius * 0.08));
    painter.drawLine(QPointF(screenRect.left() + screenRadius * 0.08, center.y()),
                     QPointF(screenRect.right() - screenRadius * 0.08, center.y()));

    if (!m_tracePoints.isEmpty()) {
        const double scale = screenRadius * 0.82;
        const double snrNorm = qBound(0.0, m_snrLike / 18.0, 1.0);
        const double freshness = qBound(0.0, 1.0 - (static_cast<double>(m_framesSinceTraceUpdate) / 24.0), 1.0);
        const double globalIntensity = qBound(0.0, (0.25 + 0.75 * snrNorm) * freshness, 1.0);
        const int count = m_tracePoints.size();

        /*
         * Draw phosphor dots, not a connected polyline.  Connecting decimated
         * Mark/Space samples creates diagonal strokes between unrelated FSK
         * states; those strokes look like decoding errors even though they are
         * only a display artefact.
         */
        painter.setPen(Qt::NoPen);

        for (int i = 0; i < count; ++i) {
            const QPointF &point = m_tracePoints.at(i);
            const double xNorm = qBound(-1.0, point.x(), 1.0);
            const double yNorm = qBound(-1.0, point.y(), 1.0);
            const QPointF mapped(center.x() + xNorm * scale,
                                 center.y() - yNorm * scale);
            const double age = static_cast<double>(i + 1) / static_cast<double>(qMax(1, count));
            const double dotIntensity = qBound(0.0, globalIntensity * (0.18 + 0.82 * age), 1.0);

            painter.setBrush(QColor(30, 255, 90, static_cast<int>(34 * dotIntensity)));
            painter.drawEllipse(mapped, 4.8, 4.8);

            painter.setBrush(QColor(80, 255, 130, static_cast<int>(110 * dotIntensity)));
            painter.drawEllipse(mapped, 2.0, 2.0);

            painter.setBrush(QColor(220, 255, 225, static_cast<int>(210 * dotIntensity)));
            painter.drawEllipse(mapped, 0.8, 0.8);
        }
    }

    painter.restore();

    painter.setPen(QPen(QColor(18, 18, 18), 2.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(screenRect);
}
