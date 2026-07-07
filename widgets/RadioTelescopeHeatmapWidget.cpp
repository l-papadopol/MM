#include "RadioTelescopeHeatmapWidget.h"

#include <QPainter>
#include <QPaintEvent>
#include <QtMath>
#include <algorithm>

namespace {

static double normalizeAzimuth(double az)
{
    double v = std::fmod(az, 360.0);
    if (v < 0.0) v += 360.0;
    return v;
}

static QColor colorForNoise(double noiseDb, double minDb, double maxDb)
{
    const double span = qMax(1.0, maxDb - minDb);
    const double t = qBound(0.0, (noiseDb - minDb) / span, 1.0);
    const int red = static_cast<int>(qRound(255.0 * qPow(t, 0.85)));
    const int green = static_cast<int>(qRound(180.0 * qPow(qMax(0.0, 1.0 - qAbs(t - 0.55) / 0.55), 1.2)));
    const int blue = static_cast<int>(qRound(40.0 * (1.0 - t)));
    return QColor(red, green, blue);
}

static QPointF polarPoint(const QPointF &center, double radius, double azimuthDeg)
{
    const double angleRad = qDegreesToRadians(azimuthDeg - 90.0);
    return QPointF(center.x() + radius * qCos(angleRad),
                   center.y() + radius * qSin(angleRad));
}

}

RadioTelescopeHeatmapWidget::RadioTelescopeHeatmapWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(360);
    setMinimumWidth(360);
    setAutoFillBackground(false);
}

void RadioTelescopeHeatmapWidget::setTiles(const QVector<TileSample> &tiles, bool useElevationAxis, bool interpolateGradient)
{
    m_tiles = tiles;
    m_useElevationAxis = useElevationAxis;
    m_interpolateGradient = interpolateGradient;
    update();
}

void RadioTelescopeHeatmapWidget::setCurrentTarget(double azimuthDeg, double elevationDeg, bool active)
{
    m_currentAzimuthDeg = azimuthDeg;
    m_currentElevationDeg = elevationDeg;
    m_hasCurrentTarget = active;
    update();
}

void RadioTelescopeHeatmapWidget::setOverlayText(const QString &text)
{
    m_overlayText = text;
    update();
}

void RadioTelescopeHeatmapWidget::clear()
{
    m_tiles.clear();
    m_overlayText.clear();
    m_hasCurrentTarget = false;
    update();
}

double RadioTelescopeHeatmapWidget::tileDisplayNoiseDb(int index) const
{
    if (index < 0 || index >= m_tiles.size() || !m_tiles[index].valid) {
        return -120.0;
    }
    if (!m_interpolateGradient) {
        return m_tiles[index].noiseDb;
    }

    const TileSample &base = m_tiles[index];
    double weightedSum = 0.0;
    double weightTotal = 0.0;
    const double radiusDeg = qMax(2.0, base.beamWidthDeg * 1.35);

    for (const TileSample &tile : m_tiles) {
        if (!tile.valid) continue;
        double da = qAbs(normalizeAzimuth(tile.azimuthDeg) - normalizeAzimuth(base.azimuthDeg));
        da = qMin(da, 360.0 - da);
        const double de = qAbs(tile.elevationDeg - base.elevationDeg);
        const double d = qSqrt(da * da + de * de);
        if (d > radiusDeg) continue;
        const double w = qExp(-(d * d) / qMax(4.0, radiusDeg * radiusDeg * 0.6));
        weightedSum += tile.noiseDb * w;
        weightTotal += w;
    }
    return weightTotal > 0.0 ? (weightedSum / weightTotal) : base.noiseDb;
}

void RadioTelescopeHeatmapWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), QColor(0, 0, 0));

    const QRectF panel = rect().adjusted(8, 8, -8, -8);
    const QPointF center = panel.center();
    const double outerRadius = qMin(panel.width(), panel.height()) * 0.45;
    const double innerRingRadius = outerRadius * 0.58;

    p.setPen(QPen(QColor(180, 120, 40, 180), 1.2));
    if (m_useElevationAxis) {
        for (int el = 0; el <= 90; el += 30) {
            const double r = outerRadius * (1.0 - (static_cast<double>(el) / 90.0));
            p.drawEllipse(center, r, r);
            const QPointF labelPos(center.x() + 6.0, center.y() - r + 14.0);
            p.setPen(QColor(220, 180, 90));
            p.drawText(labelPos, QString::number(el) + QChar(0x00B0));
            p.setPen(QPen(QColor(180, 120, 40, 180), 1.2));
        }
    } else {
        p.drawEllipse(center, outerRadius, outerRadius);
        p.drawEllipse(center, innerRingRadius, innerRingRadius);
    }

    for (int az = 0; az < 360; az += 30) {
        const QPointF p0 = polarPoint(center, m_useElevationAxis ? 0.0 : innerRingRadius, static_cast<double>(az));
        const QPointF p1 = polarPoint(center, outerRadius, static_cast<double>(az));
        p.setPen(QPen(QColor(120, 90, 40, 140), 1.0, Qt::DashLine));
        p.drawLine(p0, p1);
        const QPointF textPos = polarPoint(center, outerRadius + 13.0, static_cast<double>(az));
        p.setPen(QColor(220, 180, 90));
        QRectF tr(textPos.x() - 18.0, textPos.y() - 10.0, 36.0, 20.0);
        p.drawText(tr, Qt::AlignCenter, QString::number(az) + QChar(0x00B0));
    }

    double minDb = 0.0;
    double maxDb = 0.0;
    bool first = true;
    for (int i = 0; i < m_tiles.size(); ++i) {
        if (!m_tiles[i].valid) continue;
        const double value = tileDisplayNoiseDb(i);
        if (first) {
            minDb = maxDb = value;
            first = false;
        } else {
            minDb = qMin(minDb, value);
            maxDb = qMax(maxDb, value);
        }
    }
    if (first) {
        minDb = -100.0;
        maxDb = -40.0;
    }
    if (qAbs(maxDb - minDb) < 6.0) {
        minDb -= 3.0;
        maxDb += 3.0;
    }

    auto sampleForDisplayPoint = [this, minDb](double azDeg, double elDeg, bool *validOut) -> double {
        double bestDist = 1.0e9;
        int bestIndex = -1;
        for (int i = 0; i < m_tiles.size(); ++i) {
            const TileSample &tile = m_tiles[i];
            double da = qAbs(normalizeAzimuth(tile.azimuthDeg) - normalizeAzimuth(azDeg));
            da = qMin(da, 360.0 - da);
            const double de = qAbs(tile.elevationDeg - elDeg);
            const double d = qSqrt(da * da + de * de);
            if (d < bestDist) {
                bestDist = d;
                bestIndex = i;
            }
        }
        const bool ok = bestIndex >= 0 && bestIndex < m_tiles.size() && m_tiles[bestIndex].valid;
        if (validOut != nullptr) *validOut = ok;
        return ok ? tileDisplayNoiseDb(bestIndex) : minDb;
    };

    auto drawHexAt = [&p](const QPointF &c, double radius, const QColor &fill, const QColor &line, double lineWidth) {
        QPolygonF hex;
        for (int k = 0; k < 6; ++k) {
            const double ang = qDegreesToRadians(60.0 * k + 30.0);
            hex << QPointF(c.x() + radius * qCos(ang), c.y() + radius * qSin(ang));
        }
        p.setPen(QPen(line, lineWidth));
        p.setBrush(fill);
        p.drawPolygon(hex);
    };

    const double beam = qMax(2.0, m_tiles.isEmpty() ? 15.0 : m_tiles.first().beamWidthDeg);
    const double hexRadius = m_useElevationAxis
        ? qMax(5.5, outerRadius * (beam / 90.0) * 0.58)
        : qMax(6.5, ((outerRadius + innerRingRadius) * 0.5) * qSin(qDegreesToRadians(beam * 0.5)) / 0.92);
    const double dx = qSqrt(3.0) * hexRadius;
    const double dy = 1.5 * hexRadius;

    const int rowMin = static_cast<int>(qFloor((-outerRadius - hexRadius) / dy)) - 1;
    const int rowMax = static_cast<int>(qCeil((outerRadius + hexRadius) / dy)) + 1;
    const int colMin = static_cast<int>(qFloor((-outerRadius - hexRadius) / dx)) - 1;
    const int colMax = static_cast<int>(qCeil((outerRadius + hexRadius) / dx)) + 1;

    for (int row = rowMin; row <= rowMax; ++row) {
        const double y = center.y() + row * dy;
        const double xOffset = (row & 1) ? dx * 0.5 : 0.0;
        for (int col = colMin; col <= colMax; ++col) {
            const double x = center.x() + col * dx + xOffset;
            const QPointF c(x, y);
            const double rx = x - center.x();
            const double ry = y - center.y();
            const double r = qSqrt(rx * rx + ry * ry);
            if (m_useElevationAxis) {
                if (r > outerRadius + hexRadius * 0.25) continue;
            } else {
                if (r < innerRingRadius - hexRadius * 0.55 || r > outerRadius + hexRadius * 0.55) continue;
            }

            const double az = normalizeAzimuth(qRadiansToDegrees(qAtan2(ry, rx)) + 90.0);
            const double el = m_useElevationAxis ? qBound(0.0, 90.0 * (1.0 - (r / qMax(1.0, outerRadius))), 90.0) : 0.0;
            bool valid = false;
            const double noise = sampleForDisplayPoint(az, el, &valid);
            QColor fill = valid ? colorForNoise(noise, minDb, maxDb) : QColor(0, 0, 0, 18);
            fill.setAlpha(valid ? 198 : 20);
            const QColor line = valid ? QColor(120, 75, 25, 175) : QColor(190, 125, 38, 85);
            drawHexAt(c, hexRadius, fill, line, valid ? 0.85 : 0.65);
        }
    }

    if (m_hasCurrentTarget) {
        QPointF target;
        if (m_useElevationAxis) {
            const double r = outerRadius * (1.0 - qBound(0.0, m_currentElevationDeg, 90.0) / 90.0);
            target = polarPoint(center, r, normalizeAzimuth(m_currentAzimuthDeg));
        } else {
            target = polarPoint(center, (innerRingRadius + outerRadius) * 0.5, normalizeAzimuth(m_currentAzimuthDeg));
        }
        p.setPen(QPen(QColor(255, 255, 255), 2.0));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(target, 7.0, 7.0);
        p.drawLine(target + QPointF(-10, 0), target + QPointF(10, 0));
        p.drawLine(target + QPointF(0, -10), target + QPointF(0, 10));
    }

    if (!m_overlayText.trimmed().isEmpty()) {
        QRectF overlayRect = panel.adjusted(8, 8, -8, -panel.height() + 70);
        p.setPen(QPen(QColor(180, 120, 40, 180), 1.0));
        p.setBrush(QColor(0, 0, 0, 180));
        p.drawRoundedRect(overlayRect, 6.0, 6.0);
        p.setPen(QColor(255, 210, 120));
        p.drawText(overlayRect.adjusted(8, 6, -8, -6), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, m_overlayText);
    }

    QRectF legendRect(panel.right() - 150.0, panel.bottom() - 28.0, 142.0, 18.0);
    QLinearGradient grad(legendRect.topLeft(), legendRect.topRight());
    grad.setColorAt(0.0, colorForNoise(minDb, minDb, maxDb));
    grad.setColorAt(1.0, colorForNoise(maxDb, minDb, maxDb));
    p.setBrush(grad);
    p.setPen(QPen(QColor(180, 120, 40, 180), 1.0));
    p.drawRect(legendRect);
    p.setPen(QColor(255, 210, 120));
    p.drawText(QRectF(legendRect.left(), legendRect.top() - 16.0, legendRect.width(), 14.0),
               Qt::AlignCenter,
               QStringLiteral("Noise %1 .. %2 dBFS")
                   .arg(QString::number(minDb, 'f', 1), QString::number(maxDb, 'f', 1)));
}
