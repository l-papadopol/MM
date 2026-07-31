#include "CwDiagnosticWidget.h"

#include <QBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMouseEvent>
#include <QOpenGLWidget>
#include <QPainter>
#include <QPolygonF>
#include <QShowEvent>
#include <QSize>
#include <QSplitter>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QWheelEvent>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

constexpr double kHistorySeconds = 60.0;
constexpr double kMinWindowSeconds = 1.5;
constexpr double kMaxWindowSeconds = 30.0;
constexpr int kDiagnosticFrameIntervalMs = 40;   // 25 FPS maximum.
constexpr int kInteractiveFrameIntervalMs = 16;  // Responsive drag/zoom.

float clampf(float value, float low, float high)
{
    return std::max(low, std::min(high, value));
}

int mouseX(const QMouseEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return qRound(event->position().x());
#else
    return event->pos().x();
#endif
}

class CwDiagnosticCanvas final : public QOpenGLWidget
{
public:
    explicit CwDiagnosticCanvas(bool primary, QWidget *parent = nullptr)
        : QOpenGLWidget(parent), m_primary(primary)
    {
        setMinimumHeight(185);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setMouseTracking(true);
        setUpdateBehavior(QOpenGLWidget::NoPartialUpdate);
        setAttribute(Qt::WA_OpaquePaintEvent, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
        setAutoFillBackground(false);

        // Sixty seconds at one diagnostic sample every 5 ms is about 12,000
        // points. Reserving once avoids allocator churn on the GUI thread.
        m_points.reserve(14000);

        // Decoder diagnostics and spectrum frames can arrive independently.
        // Coalesce them into one GL-backed presentation frame instead of
        // forcing one full QWidget paint for every callback.
        m_repaintTimer.setSingleShot(true);
        connect(&m_repaintTimer, &QTimer::timeout, this, [this]() {
            m_repaintQueued = false;
            if (!m_dirty || !isVisible()) return;
            m_dirty = false;
            update();
        });
    }

    void appendSamples(const QVector<CwDiagnosticPoint> &samples)
    {
        if (samples.isEmpty()) return;

        m_points.reserve(std::max(m_points.capacity(), m_points.size() + samples.size()));
        for (const CwDiagnosticPoint &sample : samples) m_points.append(sample);

        const double newest = m_points.constLast().timestampSec;
        const double cutoff = newest - kHistorySeconds;
        const auto firstToKeep = std::lower_bound(
            m_points.begin(), m_points.end(), cutoff,
            [](const CwDiagnosticPoint &point, double timestamp) {
                return point.timestampSec < timestamp;
            });
        if (firstToKeep != m_points.begin()) {
            // One compacting move per batch. The previous remove(0) loop moved
            // almost the complete 60-second vector repeatedly and was a major
            // source of UI stalls.
            m_points.erase(m_points.begin(), firstToKeep);
        }

        if (m_live) m_viewEndSec = newest;
        requestRepaint(false);
    }

    void setSpectrum(const QVector<float> &offsetsHz,
                     const QVector<float> &inputPsdDb,
                     const QVector<float> &filteredPsdDb,
                     const QVector<float> &theoreticalResponseDb,
                     float trackedOffsetHz,
                     float effectiveBandwidthHz,
                     float interfererOffsetHz,
                     float interfererConfidence,
                     float carrierProminenceDb,
                     float carrierPeakWidthHz,
                     float carrierToSecondDb)
    {
        m_offsetsHz = offsetsHz;
        m_inputPsdDb = inputPsdDb;
        m_filteredPsdDb = filteredPsdDb;
        m_theoreticalResponseDb = theoreticalResponseDb;
        m_trackedOffsetHz = trackedOffsetHz;
        m_effectiveBandwidthHz = effectiveBandwidthHz;
        m_interfererOffsetHz = interfererOffsetHz;
        m_interfererConfidence = interfererConfidence;
        m_carrierProminenceDb = carrierProminenceDb;
        m_carrierPeakWidthHz = carrierPeakWidthHz;
        m_carrierToSecondDb = carrierToSecondDb;
        requestRepaint(false);
    }

    void setCaptions(const QString &fftCaption, const QString &paperCaption)
    {
        m_fftCaption = fftCaption;
        m_paperCaption = paperCaption;
        requestRepaint(true);
    }

    void clearHistory()
    {
        m_points.clear();
        m_offsetsHz.clear();
        m_inputPsdDb.clear();
        m_filteredPsdDb.clear();
        m_theoreticalResponseDb.clear();
        m_live = true;
        m_viewEndSec = 0.0;
        requestRepaint(true);
    }

    void goLive()
    {
        m_live = true;
        if (!m_points.isEmpty()) m_viewEndSec = m_points.constLast().timestampSec;
        requestRepaint(true);
    }

    void goOldest()
    {
        if (m_points.isEmpty()) return;
        m_live = false;
        m_viewEndSec = std::min(m_points.constLast().timestampSec,
                                m_points.constFirst().timestampSec + m_windowSeconds);
        requestRepaint(true);
    }

    void step(double fraction)
    {
        if (m_points.isEmpty()) return;
        m_live = false;
        m_viewEndSec += fraction * m_windowSeconds;
        const double newest = m_points.constLast().timestampSec;
        const double oldest = m_points.constFirst().timestampSec;
        m_viewEndSec = std::max(oldest + m_windowSeconds,
                                std::min(newest, m_viewEndSec));
        requestRepaint(true);
    }

    void zoom(double factor)
    {
        m_windowSeconds = std::max(kMinWindowSeconds,
                                   std::min(kMaxWindowSeconds, m_windowSeconds * factor));
        requestRepaint(true);
    }

protected:
    void initializeGL() override
    {
        // QPainter uses the QOpenGLWidget paint engine/FBO. The actual OpenGL
        // backend is selected by Qt (desktop GL, ANGLE or software fallback).
    }

    void paintGL() override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, false);
        painter.setRenderHint(QPainter::TextAntialiasing, true);
        painter.fillRect(rect(), QColor(5, 7, 8));

        const QRectF full = rect().adjusted(7, 5, -7, -6);
        const qreal spectrumHeight = std::max<qreal>(76.0, full.height() * 0.31);
        const QRectF spectrumRect(full.left(), full.top(), full.width(), spectrumHeight);
        const QRectF tapeRect(full.left(), spectrumRect.bottom() + 7.0,
                              full.width(), full.bottom() - spectrumRect.bottom() - 7.0);
        drawSpectrum(painter, spectrumRect);
        drawTape(painter, tapeRect);
    }

    void showEvent(QShowEvent *event) override
    {
        QOpenGLWidget::showEvent(event);
        requestRepaint(true);
    }

    void wheelEvent(QWheelEvent *event) override
    {
        zoom(event->angleDelta().y() > 0 ? 0.82 : 1.22);
        event->accept();
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            m_dragging = true;
            m_dragStartX = mouseX(event);
            m_dragStartEndSec = m_viewEndSec;
            m_live = false;
            event->accept();
            return;
        }
        QOpenGLWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (!m_dragging || width() <= 1) {
            QOpenGLWidget::mouseMoveEvent(event);
            return;
        }
        const double delta = static_cast<double>(mouseX(event) - m_dragStartX) /
                             static_cast<double>(width());
        m_viewEndSec = m_dragStartEndSec - delta * m_windowSeconds;
        if (!m_points.isEmpty()) {
            const double newest = m_points.constLast().timestampSec;
            const double oldest = m_points.constFirst().timestampSec;
            m_viewEndSec = std::max(oldest + m_windowSeconds,
                                    std::min(newest, m_viewEndSec));
        }
        requestRepaint(true);
        event->accept();
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            m_dragging = false;
            event->accept();
            return;
        }
        QOpenGLWidget::mouseReleaseEvent(event);
    }

private:
    struct VisibleRange {
        int first = 0;
        int last = 0; // Exclusive.
        double beginSec = 0.0;
        double endSec = 0.0;
    };

    QColor channelColor() const
    {
        return m_primary ? QColor(80, 255, 120) : QColor(70, 165, 255);
    }

    void requestRepaint(bool interactive)
    {
        m_dirty = true;
        if (!isVisible()) return;

        const int delay = interactive ? kInteractiveFrameIntervalMs
                                      : kDiagnosticFrameIntervalMs;
        if (m_repaintQueued) {
            if (interactive && m_repaintTimer.remainingTime() > delay)
                m_repaintTimer.start(delay);
            return;
        }
        m_repaintQueued = true;
        m_repaintTimer.start(delay);
    }

    void drawFrame(QPainter &painter, const QRectF &rect, const QString &caption)
    {
        painter.setPen(QPen(QColor(106, 67, 28), 1.0));
        painter.setBrush(QColor(8, 10, 11));
        painter.drawRect(rect);
        painter.setPen(QColor(255, 179, 71));
        painter.drawText(rect.adjusted(7, 3, -5, -3),
                         Qt::AlignLeft | Qt::AlignTop, caption);
    }

    void drawSpectrum(QPainter &painter, const QRectF &rect)
    {
        const QString caption = QStringLiteral("%1   C %2 dB · W %3 Hz · Δ2 %4 dB")
            .arg(m_fftCaption)
            .arg(m_carrierProminenceDb, 0, 'f', 1)
            .arg(m_carrierPeakWidthHz, 0, 'f', 1)
            .arg(m_carrierToSecondDb, 0, 'f', 1);
        drawFrame(painter, rect, caption);
        const QRectF graph = rect.adjusted(34, 20, -7, -7);
        constexpr float kMinDb = -110.0f;
        constexpr float kMaxDb = 0.0f;

        painter.save();
        painter.setClipRect(rect.adjusted(1, 1, -1, -1));
        painter.setPen(QPen(QColor(35, 42, 44), 1.0));
        for (int i = 0; i <= 6; ++i) {
            const qreal x = graph.left() + graph.width() * i / 6.0;
            painter.drawLine(QPointF(x, graph.top()), QPointF(x, graph.bottom()));
        }
        for (int db = -100; db <= 0; db += 20) {
            const qreal y = graph.bottom() - graph.height() *
                (static_cast<qreal>(db) - kMinDb) / (kMaxDb - kMinDb);
            painter.drawLine(QPointF(graph.left(), y), QPointF(graph.right(), y));
            painter.setPen(QColor(125, 130, 132));
            painter.drawText(QRectF(rect.left() + 2, y - 8, 29, 16),
                             Qt::AlignRight | Qt::AlignVCenter,
                             QString::number(db));
            painter.setPen(QPen(QColor(35, 42, 44), 1.0));
        }

        const auto xForOffset = [&graph](float offset) {
            return graph.left() + graph.width() *
                (clampf(offset, -300.0f, 300.0f) + 300.0f) / 600.0f;
        };
        const auto yForDb = [&graph](float db) {
            const float bounded = clampf(db, kMinDb, kMaxDb);
            return graph.bottom() - graph.height() *
                (bounded - kMinDb) / (kMaxDb - kMinDb);
        };

        const qreal halfBw = 0.5 * std::max(1.0f, m_effectiveBandwidthHz);
        const qreal passbandLeft = xForOffset(m_trackedOffsetHz - halfBw);
        const qreal passbandRight = xForOffset(m_trackedOffsetHz + halfBw);
        painter.fillRect(QRectF(std::min(passbandLeft, passbandRight), graph.top(),
                                std::abs(passbandRight - passbandLeft), graph.height()),
                         QColor(channelColor().red(), channelColor().green(),
                                channelColor().blue(), 20));

        const bool valid = m_offsetsHz.size() > 1 &&
            m_offsetsHz.size() == m_inputPsdDb.size() &&
            m_offsetsHz.size() == m_filteredPsdDb.size() &&
            m_offsetsHz.size() == m_theoreticalResponseDb.size();
        if (valid) {
            QPolygonF inputLine;
            QPolygonF outputLine;
            QPolygonF responseLine;
            inputLine.reserve(m_offsetsHz.size());
            outputLine.reserve(m_offsetsHz.size());
            responseLine.reserve(m_offsetsHz.size());
            for (int i = 0; i < m_offsetsHz.size(); ++i) {
                const qreal x = xForOffset(m_offsetsHz.at(i));
                inputLine.append(QPointF(x, yForDb(m_inputPsdDb.at(i))));
                outputLine.append(QPointF(x, yForDb(m_filteredPsdDb.at(i))));
                // Theoretical response is relative. Anchor it at -8 dB so it
                // remains a clearly separate guide rather than impersonating a
                // measured output spectrum.
                responseLine.append(QPointF(
                    x, yForDb(-8.0f + m_theoreticalResponseDb.at(i))));
            }
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setPen(QPen(QColor(154, 158, 162), 1.0));
            painter.drawPolyline(inputLine);
            painter.setPen(QPen(channelColor(), 1.6));
            painter.drawPolyline(outputLine);
            painter.setPen(QPen(QColor(255, 179, 71, 190), 1.0, Qt::DashLine));
            painter.drawPolyline(responseLine);
            painter.setRenderHint(QPainter::Antialiasing, false);
        }

        painter.setPen(QPen(QColor(255, 191, 70), 1.0, Qt::DashLine));
        painter.drawLine(QPointF(xForOffset(0.0f), graph.top()),
                         QPointF(xForOffset(0.0f), graph.bottom()));
        painter.setPen(QPen(channelColor(), 1.6));
        painter.drawLine(QPointF(xForOffset(m_trackedOffsetHz), graph.top()),
                         QPointF(xForOffset(m_trackedOffsetHz), graph.bottom()));
        if (m_interfererConfidence > 0.15f) {
            painter.setPen(QPen(QColor(255, 82, 70, 200), 1.4, Qt::DashLine));
            painter.drawLine(QPointF(xForOffset(m_interfererOffsetHz), graph.top()),
                             QPointF(xForOffset(m_interfererOffsetHz), graph.bottom()));
        }

        // Compact legend: measured input, measured output, theoretical filter.
        const qreal legendY = graph.top() + 3.0;
        painter.setPen(QPen(QColor(154, 158, 162), 1.4));
        painter.drawLine(QPointF(graph.left() + 5, legendY),
                         QPointF(graph.left() + 25, legendY));
        painter.setPen(QColor(180, 184, 186));
        painter.drawText(QPointF(graph.left() + 29, legendY + 4), QStringLiteral("IN"));
        painter.setPen(QPen(channelColor(), 1.6));
        painter.drawLine(QPointF(graph.left() + 58, legendY),
                         QPointF(graph.left() + 78, legendY));
        painter.setPen(channelColor());
        painter.drawText(QPointF(graph.left() + 82, legendY + 4), QStringLiteral("OUT"));
        painter.setPen(QPen(QColor(255, 179, 71), 1.0, Qt::DashLine));
        painter.drawLine(QPointF(graph.left() + 126, legendY),
                         QPointF(graph.left() + 146, legendY));
        painter.setPen(QColor(255, 179, 71));
        painter.drawText(QPointF(graph.left() + 150, legendY + 4), QStringLiteral("H(f)"));
        painter.restore();
    }

    VisibleRange visibleRange() const
    {
        VisibleRange range;
        if (m_points.isEmpty()) return range;

        range.endSec = m_live ? m_points.constLast().timestampSec : m_viewEndSec;
        range.beginSec = range.endSec - m_windowSeconds;

        const auto first = std::lower_bound(
            m_points.constBegin(), m_points.constEnd(), range.beginSec,
            [](const CwDiagnosticPoint &point, double timestamp) {
                return point.timestampSec < timestamp;
            });
        const auto last = std::upper_bound(
            first, m_points.constEnd(), range.endSec,
            [](double timestamp, const CwDiagnosticPoint &point) {
                return timestamp < point.timestampSec;
            });
        range.first = static_cast<int>(first - m_points.constBegin());
        range.last = static_cast<int>(last - m_points.constBegin());
        return range;
    }

    void drawTape(QPainter &painter, const QRectF &rect)
    {
        drawFrame(painter, rect,
                  QStringLiteral("%1  %2 s")
                      .arg(m_paperCaption)
                      .arg(m_windowSeconds, 0, 'f', 1));
        const QRectF graph = rect.adjusted(7, 20, -7, -7);
        const qreal envelopeBottom = graph.top() + graph.height() * 0.54;
        const qreal squareTop = envelopeBottom + 5.0;
        const qreal squareBottom = graph.top() + graph.height() * 0.78;
        const qreal morseY = graph.bottom() - 8.0;

        painter.setPen(QPen(QColor(38, 44, 46), 1.0));
        for (int i = 0; i <= static_cast<int>(std::ceil(m_windowSeconds)); ++i) {
            const qreal x = graph.right() - graph.width() * i / m_windowSeconds;
            painter.drawLine(QPointF(x, graph.top()), QPointF(x, graph.bottom()));
        }
        painter.drawLine(QPointF(graph.left(), squareTop - 2.0),
                         QPointF(graph.right(), squareTop - 2.0));
        painter.drawLine(QPointF(graph.left(), squareBottom + 2.0),
                         QPointF(graph.right(), squareBottom + 2.0));

        const VisibleRange range = visibleRange();
        const int count = range.last - range.first;
        if (count < 2) return;

        const CwDiagnosticPoint &latest = m_points.at(range.last - 1);
        painter.setPen(QColor(188, 194, 196));
        painter.drawText(rect.adjusted(7, 3, -7, -3),
                         Qt::AlignRight | Qt::AlignTop,
                         QStringLiteral("%1 WPM · ENV %2 dB · C %3 dB · COH %4 dB/%5% · BW %6 · %7")
                             .arg(latest.wpm, 0, 'f', 1)
                             .arg(latest.snrDb, 0, 'f', 1)
                             .arg(latest.carrierProminenceDb, 0, 'f', 1)
                             .arg(latest.coherentSnrDb, 0, 'f', 1)
                             .arg(latest.coherence * 100.0f, 0, 'f', 0)
                             .arg(latest.effectiveBandwidthHz, 0, 'f', 0)
                             .arg(latest.trackingConfirmed
                                  ? QString::fromUtf8("✓") : QString::fromUtf8("…")));

        const auto xForTime = [&](double timestamp) {
            return graph.left() + graph.width() *
                (timestamp - range.beginSec) / m_windowSeconds;
        };

        double scaleTop = 1.0e-9;
        for (int i = range.first; i < range.last; ++i) {
            const CwDiagnosticPoint &point = m_points.at(i);
            scaleTop = std::max(scaleTop,
                                static_cast<double>(std::max(point.signalLevel,
                                                            point.filteredEnvelope)));
        }
        scaleTop *= 1.12;
        const auto yForEnvelope = [&](float value) {
            const double normalized = std::max(0.0,
                std::min(1.0, static_cast<double>(value) / scaleTop));
            return envelopeBottom - normalized *
                (envelopeBottom - graph.top() - 3.0);
        };

        // Never send substantially more vertices to the GL painter than there
        // are horizontal pixels. The complete 5 ms history is retained for
        // segment timing; only the display polylines are decimated.
        const int maxPolylineVertices = std::max(64, qRound(graph.width() * 1.5));
        const int stride = std::max(1, count / maxPolylineVertices);
        QPolygonF acqLine;
        QPolygonF envLine;
        QPolygonF lowLine;
        QPolygonF highLine;
        const int reserved = count / stride + 2;
        acqLine.reserve(reserved);
        envLine.reserve(reserved);
        lowLine.reserve(reserved);
        highLine.reserve(reserved);

        for (int i = range.first; i < range.last; i += stride) {
            const CwDiagnosticPoint &point = m_points.at(i);
            const qreal x = xForTime(point.timestampSec);
            acqLine.append(QPointF(x, yForEnvelope(point.acquisitionEnvelope)));
            envLine.append(QPointF(x, yForEnvelope(point.filteredEnvelope)));
            lowLine.append(QPointF(x, yForEnvelope(point.thresholdLow)));
            highLine.append(QPointF(x, yForEnvelope(point.thresholdHigh)));
        }
        if ((range.last - 1 - range.first) % stride != 0) {
            const CwDiagnosticPoint &point = m_points.at(range.last - 1);
            const qreal x = xForTime(point.timestampSec);
            acqLine.append(QPointF(x, yForEnvelope(point.acquisitionEnvelope)));
            envLine.append(QPointF(x, yForEnvelope(point.filteredEnvelope)));
            lowLine.append(QPointF(x, yForEnvelope(point.thresholdLow)));
            highLine.append(QPointF(x, yForEnvelope(point.thresholdHigh)));
        }

        painter.save();
        painter.setClipRect(graph);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(QPen(QColor(115, 122, 126, 150), 1.0));
        painter.drawPolyline(acqLine);
        painter.setPen(QPen(channelColor(), 1.5));
        painter.drawPolyline(envLine);
        painter.setPen(QPen(QColor(255, 174, 58, 170), 1.0, Qt::DashLine));
        painter.drawPolyline(lowLine);
        painter.setPen(QPen(QColor(255, 92, 66, 190), 1.0, Qt::DashLine));
        painter.drawPolyline(highLine);
        painter.setRenderHint(QPainter::Antialiasing, false);

        const qreal lowY = yForEnvelope(latest.thresholdLow);
        const qreal highY = yForEnvelope(latest.thresholdHigh);
        painter.setPen(QColor(255, 174, 58, 210));
        painter.drawText(QPointF(graph.left() + 2.0, lowY - 2.0),
                         QString::fromUtf8("↓"));
        painter.setPen(QColor(255, 92, 66, 220));
        painter.drawText(QPointF(graph.left() + 2.0, highY - 2.0),
                         QString::fromUtf8("↑"));

        // QSB erasures are not ordinary key-up intervals. The tracker sends
        // the exact historical interval once the carrier returns, so paint the
        // actual OFF run rather than a marker delayed by the diagnostic rate.
        double lastErasureEnd = -1.0;
        for (int i = range.first; i < range.last; ++i) {
            const CwDiagnosticPoint &point = m_points.at(i);
            if (!point.qsbErasure ||
                !(point.qsbErasureEndSec > point.qsbErasureStartSec) ||
                std::abs(point.qsbErasureEndSec - lastErasureEnd) < 1.0e-5) {
                continue;
            }
            const qreal x1 = xForTime(point.qsbErasureStartSec);
            const qreal x2 = std::max(x1 + 1.0,
                                      xForTime(point.qsbErasureEndSec));
            const QRectF erasureRect(x1, squareTop, x2 - x1,
                                     squareBottom - squareTop);
            painter.fillRect(erasureRect, QBrush(QColor(230, 90, 245, 72),
                                                 Qt::BDiagPattern));
            lastErasureEnd = point.qsbErasureEndSec;
        }

        // The native decoder does not inject synthetic edge corrections.

        bool segmentOn = false;
        double segmentStart = 0.0;
        float segmentWpm = 20.0f;
        double segmentProbabilitySum = 0.0;
        int segmentProbabilityCount = 0;
        bool segmentConfirmed = true;
        for (int i = range.first; i < range.last; ++i) {
            const CwDiagnosticPoint &point = m_points.at(i);
            if (point.keyDown && !segmentOn) {
                segmentOn = true;
                segmentStart = point.timestampSec;
                segmentWpm = std::max(5.0f, point.wpm);
                segmentProbabilitySum = point.markProbability;
                segmentProbabilityCount = 1;
                segmentConfirmed = point.trackingConfirmed;
            } else if (segmentOn && point.keyDown) {
                segmentProbabilitySum += point.markProbability;
                ++segmentProbabilityCount;
                segmentConfirmed = segmentConfirmed && point.trackingConfirmed;
            }
            const bool closing = segmentOn &&
                (!point.keyDown || i + 1 == range.last);
            if (!closing) continue;

            const double segmentEnd = point.timestampSec;
            const qreal x1 = xForTime(segmentStart);
            const qreal x2 = std::max(x1 + 1.0, xForTime(segmentEnd));
            const double averageProbability = segmentProbabilityCount > 0
                ? segmentProbabilitySum / static_cast<double>(segmentProbabilityCount) : 0.5;
            const int alpha = segmentConfirmed
                ? qBound(80, qRound(70.0 + 100.0 * averageProbability), 175)
                : 34;
            const QRectF pulseRect(x1, squareTop, x2 - x1,
                                   squareBottom - squareTop);
            painter.fillRect(pulseRect,
                             QColor(channelColor().red(), channelColor().green(),
                                    channelColor().blue(), alpha));
            if (!segmentConfirmed || averageProbability < 0.72) {
                painter.setPen(QPen(channelColor(), 1.0, Qt::DashLine));
                painter.drawRect(pulseRect);
            }
            const double durationMs = 1000.0 *
                std::max(0.0, segmentEnd - segmentStart);
            const double dotMs = 1200.0 / std::max(5.0f, segmentWpm);
            const QString symbol = durationMs < 1.9 * dotMs
                ? QString::fromUtf8("·") : QString::fromUtf8("—");
            painter.setPen(channelColor());
            painter.drawText(QRectF(x1 - 3.0, squareBottom + 3.0,
                                    std::max<qreal>(14.0, x2 - x1 + 6.0),
                                    morseY - squareBottom),
                             Qt::AlignHCenter | Qt::AlignTop, symbol);
            segmentOn = false;
        }
        painter.restore();
    }

private:
    bool m_primary = true;
    QString m_fftCaption = QStringLiteral("Measured PSD ±300 Hz");
    QString m_paperCaption = QStringLiteral("Virtual paper");
    QVector<CwDiagnosticPoint> m_points;
    QVector<float> m_offsetsHz;
    QVector<float> m_inputPsdDb;
    QVector<float> m_filteredPsdDb;
    QVector<float> m_theoreticalResponseDb;
    float m_trackedOffsetHz = 0.0f;
    float m_effectiveBandwidthHz = 120.0f;
    float m_interfererOffsetHz = 0.0f;
    float m_interfererConfidence = 0.0f;
    float m_carrierProminenceDb = -99.0f;
    float m_carrierPeakWidthHz = 0.0f;
    float m_carrierToSecondDb = 0.0f;
    bool m_live = true;
    double m_viewEndSec = 0.0;
    double m_windowSeconds = 4.0;
    bool m_dragging = false;
    int m_dragStartX = 0;
    double m_dragStartEndSec = 0.0;
    QTimer m_repaintTimer;
    bool m_repaintQueued = false;
    bool m_dirty = true;
};

} // namespace

class CwDiagnosticWidget::ChannelPane final : public QWidget
{
public:
    ChannelPane(bool primary, QWidget *parent = nullptr)
        : QWidget(parent), m_primary(primary)
    {
        QVBoxLayout *outer = new QVBoxLayout(this);
        outer->setContentsMargins(4, 4, 4, 4);
        outer->setSpacing(4);

        QHBoxLayout *tools = new QHBoxLayout();
        tools->setContentsMargins(0, 0, 0, 0);
        tools->setSpacing(3);
        m_title = new QLabel(this);
        m_title->setStyleSheet(primary
            ? QStringLiteral("font-weight:700;color:#50ff78;")
            : QStringLiteral("font-weight:700;color:#4aa8ff;"));
        tools->addWidget(m_title);
        tools->addStretch(1);

        // Use real Qt media/navigation icons instead of Unicode glyphs.
        // The global UI font can legitimately lack those glyphs on Windows or
        // on some Linux themes, in which case Qt elides them to "...".  Native
        // standard icons are font-independent and remain readable everywhere.
        m_farLeft = makeIconButton(QStringLiteral("media-skip-backward"),
                                   QStyle::SP_MediaSkipBackward);
        m_left = makeIconButton(QStringLiteral("media-seek-backward"),
                                QStyle::SP_MediaSeekBackward);
        m_live = makeTextButton(QStringLiteral("LIVE"));
        m_right = makeIconButton(QStringLiteral("media-seek-forward"),
                                 QStyle::SP_MediaSeekForward);
        m_farRight = makeIconButton(QStringLiteral("media-skip-forward"),
                                    QStyle::SP_MediaSkipForward);
        m_zoomOut = makeIconButton(QStringLiteral("zoom-out"),
                                   QStyle::SP_ArrowDown);
        m_zoomIn = makeIconButton(QStringLiteral("zoom-in"),
                                  QStyle::SP_ArrowUp);
        m_clear = makeIconButton(QStringLiteral("edit-clear"),
                                 QStyle::SP_DialogResetButton);
        for (QToolButton *button : {m_farLeft, m_left, m_live, m_right,
                                    m_farRight, m_zoomOut, m_zoomIn, m_clear})
            tools->addWidget(button);
        outer->addLayout(tools);

        m_canvas = new CwDiagnosticCanvas(primary, this);
        outer->addWidget(m_canvas, 1);

        connect(m_farLeft, &QToolButton::clicked, this,
                [this]() { m_canvas->goOldest(); });
        connect(m_left, &QToolButton::clicked, this,
                [this]() { m_canvas->step(-0.25); });
        connect(m_live, &QToolButton::clicked, this,
                [this]() { m_canvas->goLive(); });
        connect(m_right, &QToolButton::clicked, this,
                [this]() { m_canvas->step(0.25); });
        connect(m_farRight, &QToolButton::clicked, this,
                [this]() { m_canvas->goLive(); });
        connect(m_zoomOut, &QToolButton::clicked, this,
                [this]() { m_canvas->zoom(1.25); });
        connect(m_zoomIn, &QToolButton::clicked, this,
                [this]() { m_canvas->zoom(0.80); });
        connect(m_clear, &QToolButton::clicked, this,
                [this]() { m_canvas->clearHistory(); });
    }

    void appendSamples(const QVector<CwDiagnosticPoint> &samples)
    {
        m_canvas->appendSamples(samples);
    }

    void setSpectrum(const QVector<float> &offsetsHz,
                     const QVector<float> &inputPsdDb,
                     const QVector<float> &filteredPsdDb,
                     const QVector<float> &theoreticalResponseDb,
                     float trackedOffsetHz,
                     float effectiveBandwidthHz,
                     float interfererOffsetHz,
                     float interfererConfidence,
                     float carrierProminenceDb,
                     float carrierPeakWidthHz,
                     float carrierToSecondDb)
    {
        m_canvas->setSpectrum(offsetsHz, inputPsdDb, filteredPsdDb,
                              theoreticalResponseDb, trackedOffsetHz,
                              effectiveBandwidthHz, interfererOffsetHz,
                              interfererConfidence, carrierProminenceDb,
                              carrierPeakWidthHz, carrierToSecondDb);
    }

    void clearHistory() { m_canvas->clearHistory(); }

    void setTexts(const QString &title, const QString &liveButtonText,
                  const QString &fftCaption, const QString &paperCaption,
                  const QString &oldestTip, const QString &backTip,
                  const QString &liveTip, const QString &forwardTip,
                  const QString &newestTip, const QString &zoomOutTip,
                  const QString &zoomInTip, const QString &clearTip)
    {
        m_title->setText(title);
        m_live->setText(liveButtonText);
        m_canvas->setCaptions(fftCaption, paperCaption);
        m_farLeft->setToolTip(oldestTip);
        m_left->setToolTip(backTip);
        m_live->setToolTip(liveTip);
        m_right->setToolTip(forwardTip);
        m_farRight->setToolTip(newestTip);
        m_zoomOut->setToolTip(zoomOutTip);
        m_zoomIn->setToolTip(zoomInTip);
        m_clear->setToolTip(clearTip);
        m_farLeft->setAccessibleName(oldestTip);
        m_left->setAccessibleName(backTip);
        m_live->setAccessibleName(liveTip);
        m_right->setAccessibleName(forwardTip);
        m_farRight->setAccessibleName(newestTip);
        m_zoomOut->setAccessibleName(zoomOutTip);
        m_zoomIn->setAccessibleName(zoomInTip);
        m_clear->setAccessibleName(clearTip);
    }

private:
    QToolButton *makeTextButton(const QString &text)
    {
        QToolButton *button = new QToolButton(this);
        button->setText(text);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setAutoRaise(false);
        button->setMinimumWidth(48);
        button->setMinimumHeight(26);
        return button;
    }

    QToolButton *makeIconButton(const QString &themeName,
                                QStyle::StandardPixmap fallback)
    {
        QToolButton *button = new QToolButton(this);
        QIcon icon = QIcon::fromTheme(themeName);
        if (icon.isNull()) {
            icon = style()->standardIcon(fallback, nullptr, button);
        }
        button->setIcon(icon);
        button->setText(QString());
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        button->setIconSize(QSize(18, 18));
        button->setAutoRaise(false);
        button->setMinimumSize(34, 26);
        button->setMaximumHeight(28);
        return button;
    }

    bool m_primary = true;
    QLabel *m_title = nullptr;
    CwDiagnosticCanvas *m_canvas = nullptr;
    QToolButton *m_farLeft = nullptr;
    QToolButton *m_left = nullptr;
    QToolButton *m_live = nullptr;
    QToolButton *m_right = nullptr;
    QToolButton *m_farRight = nullptr;
    QToolButton *m_zoomOut = nullptr;
    QToolButton *m_zoomIn = nullptr;
    QToolButton *m_clear = nullptr;
};

CwDiagnosticWidget::CwDiagnosticWidget(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(5, 5, 5, 5);
    layout->setSpacing(4);

    QSplitter *splitter = new QSplitter(Qt::Vertical, this);
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(5);
    m_channelA = new ChannelPane(true, splitter);
    m_channelB = new ChannelPane(false, splitter);
    splitter->addWidget(m_channelA);
    splitter->addWidget(m_channelB);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    layout->addWidget(splitter, 1);
    retranslateUi();
}

void CwDiagnosticWidget::appendSamples(int rank,
                                       const QVector<CwDiagnosticPoint> &samples)
{
    (rank <= 0 ? m_channelA : m_channelB)->appendSamples(samples);
}

void CwDiagnosticWidget::setSpectrum(int rank,
                                     const QVector<float> &offsetsHz,
                                     const QVector<float> &inputPsdDb,
                                     const QVector<float> &filteredPsdDb,
                                     const QVector<float> &theoreticalResponseDb,
                                     float trackedOffsetHz,
                                     float effectiveBandwidthHz,
                                     float interfererOffsetHz,
                                     float interfererConfidence,
                                     float carrierProminenceDb,
                                     float carrierPeakWidthHz,
                                     float carrierToSecondDb)
{
    (rank <= 0 ? m_channelA : m_channelB)->setSpectrum(
        offsetsHz, inputPsdDb, filteredPsdDb, theoreticalResponseDb,
        trackedOffsetHz, effectiveBandwidthHz, interfererOffsetHz,
        interfererConfidence, carrierProminenceDb, carrierPeakWidthHz,
        carrierToSecondDb);
}

void CwDiagnosticWidget::clearChannel(int rank)
{
    (rank <= 0 ? m_channelA : m_channelB)->clearHistory();
}

void CwDiagnosticWidget::setTextTranslator(
    std::function<QString(const QString &, const QString &)> translator)
{
    m_translator = std::move(translator);
    retranslateUi();
}

void CwDiagnosticWidget::retranslateUi()
{
    const auto tr = [this](const QString &key, const QString &fallback) {
        return m_translator ? m_translator(key, fallback) : fallback;
    };
    const QString oldest = tr(QStringLiteral("cw_diag_oldest"),
                              QStringLiteral("Jump to the oldest paper"));
    const QString back = tr(QStringLiteral("cw_diag_back"),
                            QStringLiteral("Move the paper backward"));
    const QString live = tr(QStringLiteral("cw_diag_live"),
                            QStringLiteral("Return to the live edge"));
    const QString forward = tr(QStringLiteral("cw_diag_forward"),
                               QStringLiteral("Move the paper forward"));
    const QString newest = tr(QStringLiteral("cw_diag_newest"),
                              QStringLiteral("Jump to the live edge"));
    const QString zoomOut = tr(QStringLiteral("cw_diag_zoom_out"),
                               QStringLiteral("Show more time"));
    const QString zoomIn = tr(QStringLiteral("cw_diag_zoom_in"),
                              QStringLiteral("Magnify dits and dahs"));
    const QString clear = tr(QStringLiteral("cw_diag_clear"),
                             QStringLiteral("Clear this diagnostic paper"));
    const QString liveButton = tr(QStringLiteral("cw_diag_live_button"),
                                  QStringLiteral("LIVE"));
    const QString fftCaption = tr(QStringLiteral("cw_diag_fft_caption"),
                                  QStringLiteral("Measured PSD ±300 Hz"));
    const QString paperCaption = tr(QStringLiteral("cw_diag_paper_caption"),
                                    QStringLiteral("Virtual paper"));
    m_channelA->setTexts(tr(QStringLiteral("cw_diag_rx_a"),
                            QStringLiteral("RX A · green")),
                         liveButton, fftCaption, paperCaption,
                         oldest, back, live, forward, newest,
                         zoomOut, zoomIn, clear);
    m_channelB->setTexts(tr(QStringLiteral("cw_diag_rx_b"),
                            QStringLiteral("RX B · blue")),
                         liveButton, fftCaption, paperCaption,
                         oldest, back, live, forward, newest,
                         zoomOut, zoomIn, clear);
}
