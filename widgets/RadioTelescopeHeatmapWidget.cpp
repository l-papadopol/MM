#include "RadioTelescopeHeatmapWidget.h"

#include <QPainter>
#include <QPaintEvent>
#include <QLinearGradient>
#include <QPolygonF>
#include <QtMath>
#include <algorithm>

namespace {

static double normalizeAzimuth(double az)
{
    double v = std::fmod(az, 360.0);
    if (v < 0.0) v += 360.0;
    return v;
}

static double contrastNormalizedNoise(double noiseDb, double minDb, double maxDb, bool logContrast)
{
    const double span = qMax(0.05, maxDb - minDb);
    const double linear = qBound(0.0, (noiseDb - minDb) / span, 1.0);
    if (!logContrast) {
        return linear;
    }
    // After a completed scan, stretch the measured range with a logarithmic
    // response so even small RFI/noise deltas become visible color changes.
    constexpr double k = 18.0;
    return qLn(1.0 + k * linear) / qLn(1.0 + k);
}

static QColor colorForNoise(double noiseDb, double minDb, double maxDb, bool logContrast)
{
    const double t = contrastNormalizedNoise(noiseDb, minDb, maxDb, logContrast);
    const int red = static_cast<int>(qRound(255.0 * qPow(t, 0.82)));
    const int green = static_cast<int>(qRound(195.0 * qPow(qMax(0.0, 1.0 - qAbs(t - 0.58) / 0.58), 1.05)));
    const int blue = static_cast<int>(qRound(48.0 * (1.0 - t)));
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

void RadioTelescopeHeatmapWidget::setScanActive(bool active)
{
    m_scanActive = active;
    update();
}

void RadioTelescopeHeatmapWidget::clear()
{
    m_tiles.clear();
    m_overlayText.clear();
    m_hasCurrentTarget = false;
    m_scanActive = false;
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

    QVector<double> measuredValues;
    measuredValues.reserve(m_tiles.size());
    for (int i = 0; i < m_tiles.size(); ++i) {
        if (!m_tiles[i].valid) continue;
        measuredValues.append(tileDisplayNoiseDb(i));
    }

    double minDb = -100.0;
    double maxDb = -40.0;
    bool hasMeasurements = !measuredValues.isEmpty();
    if (hasMeasurements) {
        std::sort(measuredValues.begin(), measuredValues.end());
        if (!m_scanActive && measuredValues.size() >= 8) {
            const int lo = qBound(0, static_cast<int>(qFloor((measuredValues.size() - 1) * 0.05)), measuredValues.size() - 1);
            const int hi = qBound(0, static_cast<int>(qCeil((measuredValues.size() - 1) * 0.95)), measuredValues.size() - 1);
            minDb = measuredValues[lo];
            maxDb = measuredValues[hi];
        } else {
            minDb = measuredValues.first();
            maxDb = measuredValues.last();
        }
        const double span = maxDb - minDb;
        if (span < 0.5) {
            const double mid = 0.5 * (minDb + maxDb);
            minDb = mid - 0.25;
            maxDb = mid + 0.25;
        } else if (m_scanActive && span < 3.0) {
            const double mid = 0.5 * (minDb + maxDb);
            minDb = mid - 1.5;
            maxDb = mid + 1.5;
        }
    }
    const bool useLogContrast = hasMeasurements && !m_scanActive;

    auto displayValueForTile = [this](int tileIndex, bool *hasColorOut, bool *measuredOut) -> double {
        if (hasColorOut != nullptr) *hasColorOut = false;
        if (measuredOut != nullptr) *measuredOut = false;
        if (tileIndex < 0 || tileIndex >= m_tiles.size()) {
            return -120.0;
        }

        const TileSample &base = m_tiles[tileIndex];
        if (base.valid) {
            if (hasColorOut != nullptr) *hasColorOut = true;
            if (measuredOut != nullptr) *measuredOut = true;
            return tileDisplayNoiseDb(tileIndex);
        }

        if (!m_interpolateGradient) {
            return -120.0;
        }

        double weightedSum = 0.0;
        double weightTotal = 0.0;
        const double radiusDeg = qMax(2.0, base.beamWidthDeg * 1.45);
        for (const TileSample &tile : m_tiles) {
            if (!tile.valid) continue;
            double da = qAbs(normalizeAzimuth(tile.azimuthDeg) - normalizeAzimuth(base.azimuthDeg));
            da = qMin(da, 360.0 - da);
            const double de = qAbs(tile.elevationDeg - base.elevationDeg);
            const double d = qSqrt(da * da + de * de);
            if (d > radiusDeg) continue;
            const double w = 1.0 / qMax(0.25, d * d);
            weightedSum += tile.noiseDb * w;
            weightTotal += w;
        }

        if (weightTotal <= 0.0) {
            return -120.0;
        }
        if (hasColorOut != nullptr) *hasColorOut = true;
        if (measuredOut != nullptr) *measuredOut = false;
        return weightedSum / weightTotal;
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
    const double nominalElStep = qMax(2.0, beam * 0.8660254);
    const double verticalHexRadius = outerRadius * (nominalElStep / 90.0) / 1.5;
    const double ringMidRadius = (innerRingRadius + outerRadius) * 0.5;

    auto tileCenter = [center, outerRadius, innerRingRadius, this](const TileSample &tile) -> QPointF {
        if (tile.hasSkyPosition) {
            return QPointF(center.x() + tile.skyX * outerRadius,
                           center.y() + tile.skyY * outerRadius);
        }
        if (m_useElevationAxis) {
            const double r = outerRadius * (1.0 - qBound(0.0, tile.elevationDeg, 90.0) / 90.0);
            return polarPoint(center, r, normalizeAzimuth(tile.azimuthDeg));
        }
        return polarPoint(center, (innerRingRadius + outerRadius) * 0.5, normalizeAzimuth(tile.azimuthDeg));
    };

    auto tileHexRadius = [outerRadius, ringMidRadius, verticalHexRadius, this](const TileSample &tile) -> double {
        const double beamDeg = qMax(2.0, tile.beamWidthDeg);
        if (tile.hasSkyPosition && m_useElevationAxis) {
            // Same pointy-top honeycomb geometry used by the scan planner:
            // horizontal center spacing = beam/90 of the sky radius,
            // hex circumradius = spacing / sqrt(3).
            const double radius = outerRadius * (beamDeg / (90.0 * qSqrt(3.0)));
            return qBound(4.5, radius * 0.995, outerRadius * 0.22);
        }
        if (tile.hasSkyPosition && !m_useElevationAxis) {
            const double chord = 2.0 * ringMidRadius * qSin(qDegreesToRadians(beamDeg * 0.5));
            return qBound(6.5, (chord / qSqrt(3.0)) * 0.98, outerRadius * 0.22);
        }
        if (m_useElevationAxis) {
            const double r = outerRadius * (1.0 - qBound(0.0, tile.elevationDeg, 90.0) / 90.0);
            const double chord = 2.0 * qMax(1.0, r) * qSin(qDegreesToRadians(beamDeg * 0.5));
            const double azRadius = chord / qSqrt(3.0);
            const double radius = qMax(verticalHexRadius, azRadius) * 0.98;
            return qBound(5.5, radius, outerRadius * 0.16);
        }
        const double chord = 2.0 * ringMidRadius * qSin(qDegreesToRadians(beamDeg * 0.5));
        return qBound(6.5, (chord / qSqrt(3.0)) * 0.98, outerRadius * 0.22);
    };

    // Draw the scan plan itself, not an independent decorative honeycomb.
    // This guarantees one visible hexagon for every commanded tile, so a measured
    // tile can never disappear because a screen-only cell was nearer to an
    // unmeasured point.
    for (int pass = 0; pass < 2; ++pass) {
        for (int i = 0; i < m_tiles.size(); ++i) {
            const TileSample &tile = m_tiles[i];
            const bool measuredPass = tile.valid;
            if ((pass == 0 && measuredPass) || (pass == 1 && !measuredPass)) {
                continue;
            }

            const QPointF c = tileCenter(tile);
            const double r = tileHexRadius(tile);
            bool hasColor = false;
            bool measured = false;
            const double noise = displayValueForTile(i, &hasColor, &measured);
            QColor fill = hasColor ? colorForNoise(noise, minDb, maxDb, useLogContrast) : QColor(0, 0, 0, 10);
            fill.setAlpha(measured ? 206 : (hasColor ? 74 : 14));
            const QColor line = measured ? QColor(170, 105, 32, 190)
                                         : QColor(190, 125, 38, 95);
            drawHexAt(c, r, fill, line, measured ? 1.05 : 0.72);
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
    for (int step = 0; step <= 8; ++step) {
        const double u = static_cast<double>(step) / 8.0;
        const double db = minDb + (maxDb - minDb) * u;
        grad.setColorAt(u, colorForNoise(db, minDb, maxDb, useLogContrast));
    }
    p.setBrush(grad);
    p.setPen(QPen(QColor(180, 120, 40, 180), 1.0));
    p.drawRect(legendRect);
    p.setPen(QColor(255, 210, 120));
    p.drawText(QRectF(legendRect.left(), legendRect.top() - 16.0, legendRect.width(), 14.0),
               Qt::AlignCenter,
               QStringLiteral("Noise %1 .. %2 dBFS%3")
                   .arg(QString::number(minDb, 'f', 1), QString::number(maxDb, 'f', 1),
                        useLogContrast ? QStringLiteral(" log") : QString()));
}
