#include "HellschreiberDecoder.h"

#include <QColor>
#include <QPainter>
#include <QtMath>

namespace {

constexpr double kTwoPi = 6.283185307179586476925286766559;

/**
 * @brief Returns black ink intensity on white paper from normalized signal level.
 */
int grayFromLevel(double normalized)
{
    normalized = qBound(0.0, normalized, 1.0);

    /* A gentle gamma keeps weak Hell pixels visible without turning the whole
     * paper black when AGC/noise estimates are still settling.
     */
    const double gamma = qSqrt(normalized);
    return qBound(0, static_cast<int>(qRound(255.0 - (245.0 * gamma))), 255);
}

} // namespace

HellschreiberDecoder::HellschreiberDecoder(QObject *parent)
    : QObject(parent)
{
    reset();
}

QString HellschreiberDecoder::modeName()
{
    return "Feld Hell";
}

QString HellschreiberDecoder::variantName(Variant variant)
{
    return (variant == Variant::Fsk105) ? QString("FSK-105") : QString("Feld Hell");
}

HellschreiberDecoder::Variant HellschreiberDecoder::variantFromKey(const QString &key)
{
    const QString normalized = key.trimmed().toUpper();
    if (normalized == "FSK105" || normalized == "FSK-105" || normalized == "FSK HELL 105") {
        return Variant::Fsk105;
    }
    return Variant::FeldHell;
}

QString HellschreiberDecoder::variantKey(Variant variant)
{
    return (variant == Variant::Fsk105) ? QString("FSK105") : QString("FeldHell");
}

double HellschreiberDecoder::fsk105ShiftHz()
{
    /* fldigi's FSKH105 path uses a narrow two-tone separation around the Hell
     * carrier.  Keep this interoperability default but make it internal: the
     * operator still tunes the center marker, not the individual tones.
     */
    return 55.0;
}

QVector<FrequencyMarker> HellschreiberDecoder::frequencyMarkers(double toneHz,
                                                                double bandwidthHz,
                                                                Variant variant,
                                                                double fskShiftHz)
{
    QVector<FrequencyMarker> markers;

    FrequencyMarker center;
    center.frequencyHz = toneHz;
    center.label = (variant == Variant::Fsk105) ? QString("FSK-105") : QString("Hell");
    center.color = QColor(40, 210, 255);
    markers.append(center);

    if (variant == Variant::Fsk105) {
        const double halfShift = qMax(5.0, fskShiftHz * 0.5);

        FrequencyMarker low;
        low.frequencyHz = qMax(10.0, toneHz - halfShift);
        low.label = "FSK black";
        low.color = QColor(80, 220, 180);
        markers.append(low);

        FrequencyMarker high;
        high.frequencyHz = toneHz + halfShift;
        high.label = "FSK white";
        high.color = QColor(130, 170, 255);
        markers.append(high);

        return markers;
    }

    FrequencyMarker low;
    low.frequencyHz = qMax(10.0, toneHz - (bandwidthHz * 0.5));
    low.label = "Hell BW";
    low.color = QColor(100, 150, 210);
    markers.append(low);

    FrequencyMarker high;
    high.frequencyHz = toneHz + (bandwidthHz * 0.5);
    high.label = "Hell BW";
    high.color = QColor(100, 150, 210);
    markers.append(high);

    return markers;
}

void HellschreiberDecoder::reset()
{
    m_phase = 0.0;
    m_i = 0.0;
    m_q = 0.0;
    m_lowPhase = 0.0;
    m_highPhase = 0.0;
    m_lowI = 0.0;
    m_lowQ = 0.0;
    m_highI = 0.0;
    m_highQ = 0.0;
    m_lowEnvelope = 0.0;
    m_highEnvelope = 0.0;
    m_pixelPhase = 0.0;
    m_envelope = 0.0;
    m_noise = 0.004;
    m_peak = 0.040;
    m_column = 0;
    m_row = 0;
    m_paperRow = 0;
    m_columnsWritten = 0;
    m_paperRowsWritten = 0;
    m_samplesProcessed = 0;
    m_statusCounter = 0;

    resetPaperImage();

    if (m_sampleRate > 0) {
        configureForSampleRate(m_sampleRate);
    }

    emit imageUpdated(m_image);
    emit statusChanged(QString("%1: %2 Hz, %3 col/s, waiting for signal")
                           .arg(variantName(m_variant))
                           .arg(m_toneHz, 0, 'f', 0)
                           .arg(m_columnRate, 0, 'f', 2));
    emitCurrentMarkers();
}

void HellschreiberDecoder::processAudioBlock(const AudioBlock &block)
{
    if (block.samples.isEmpty() || block.sampleRate <= 0) {
        return;
    }

    if (block.sampleRate != m_sampleRate) {
        m_sampleRate = block.sampleRate;
        configureForSampleRate(m_sampleRate);
    }

    for (float sample : block.samples) {
        processSample(static_cast<double>(sample));
    }

    maybeEmitStatus();
}

void HellschreiberDecoder::setVariant(Variant variant)
{
    if (m_variant == variant) {
        return;
    }

    m_variant = variant;
    m_i = 0.0;
    m_q = 0.0;
    m_lowI = 0.0;
    m_lowQ = 0.0;
    m_highI = 0.0;
    m_highQ = 0.0;
    m_lowEnvelope = 0.0;
    m_highEnvelope = 0.0;
    m_envelope = 0.0;
    m_noise = 0.004;
    m_peak = 0.040;

    if (m_sampleRate > 0) {
        configureForSampleRate(m_sampleRate);
    }

    emit statusChanged(QString("Hell variant: %1").arg(variantName(m_variant)));
    emitCurrentMarkers();
}

void HellschreiberDecoder::setToneHz(double toneHz)
{
    m_toneHz = qBound(250.0, toneHz, 3500.0);
    if (m_sampleRate > 0) {
        configureForSampleRate(m_sampleRate);
    }
    emitCurrentMarkers();
}

void HellschreiberDecoder::setColumnRate(double columnRate)
{
    m_columnRate = qBound(2.0, columnRate, 80.0);
    if (m_sampleRate > 0) {
        configureForSampleRate(m_sampleRate);
    }
}

void HellschreiberDecoder::setBandwidthHz(double bandwidthHz)
{
    m_bandwidthHz = qBound(40.0, bandwidthHz, 800.0);
    if (m_sampleRate > 0) {
        configureForSampleRate(m_sampleRate);
    }
    emitCurrentMarkers();
}

void HellschreiberDecoder::setFskShiftHz(double shiftHz)
{
    m_fskShiftHz = qBound(20.0, shiftHz, 300.0);
    if (m_sampleRate > 0) {
        configureForSampleRate(m_sampleRate);
    }
    emitCurrentMarkers();
}

void HellschreiberDecoder::setVerticalScale(int scale)
{
    const int bounded = qBound(1, scale, 12);
    if (m_verticalScale == bounded) {
        return;
    }

    /* Paper zoom is pure display scaling.  Do not reset DSP state, current
     * column or old pixels: rebuild the scaled image from the unscaled paper.
     * The same zoom is applied horizontally and vertically so Feld Hell glyphs
     * keep their original paper aspect ratio.
     */
    m_verticalScale = bounded;
    rebuildDisplayImageFromLogical();
    emit imageUpdated(m_image);
}

void HellschreiberDecoder::appendTransmitRaster(const QImage &raster)
{
    if (raster.isNull()) {
        return;
    }

    if (m_logicalImage.isNull() || m_image.isNull()) {
        resetPaperImage();
    }

    const QColor txInk(205, 0, 0);
    const int columns = raster.width();
    const int rows = qMin(kLogicalRasterHeight, raster.height());

    /* Keep transmitted text on the same paper tape as RX, instead of replacing
     * the paper with a separate preview page.  White raster pixels simply
     * advance the paper column; black pixels are drawn in red.
     */
    for (int x = 0; x < columns; ++x) {
        if (m_column >= kPaperWidth) {
            finishPaperRow();
        }

        for (int y = 0; y < rows; ++y) {
            if (raster.constScanLine(y)[x] < 128) {
                setPaperPixel(m_column, m_paperRow, y, txInk);
            }
        }

        ++m_column;
        ++m_columnsWritten;
    }

    emit imageUpdated(m_image);
}

int HellschreiberDecoder::verticalScale() const
{
    return m_verticalScale;
}

HellschreiberDecoder::Variant HellschreiberDecoder::variant() const
{
    return m_variant;
}

double HellschreiberDecoder::toneHz() const
{
    return m_toneHz;
}

double HellschreiberDecoder::columnRate() const
{
    return m_columnRate;
}

double HellschreiberDecoder::bandwidthHz() const
{
    return m_bandwidthHz;
}

double HellschreiberDecoder::fskShiftHz() const
{
    return m_fskShiftHz;
}

QImage HellschreiberDecoder::currentImage() const
{
    return m_image;
}

void HellschreiberDecoder::configureForSampleRate(int sampleRate)
{
    if (sampleRate <= 0) {
        return;
    }

    const double center = qBound(10.0, m_toneHz, sampleRate * 0.45);
    const double halfShift = qMax(5.0, m_fskShiftHz * 0.5);
    const double lowTone = qBound(10.0, center - halfShift, sampleRate * 0.45);
    const double highTone = qBound(10.0, center + halfShift, sampleRate * 0.45);

    m_phaseInc = kTwoPi * center / static_cast<double>(sampleRate);
    m_lowPhaseInc = kTwoPi * lowTone / static_cast<double>(sampleRate);
    m_highPhaseInc = kTwoPi * highTone / static_cast<double>(sampleRate);

    const double rc = 1.0 / (kTwoPi * qBound(20.0, m_bandwidthHz, 1200.0));
    const double dt = 1.0 / static_cast<double>(sampleRate);
    m_lpAlpha = qBound(0.0005, dt / (rc + dt), 0.40);

    const double fskDetectorBandwidth = qBound(12.0, m_fskShiftHz * 0.45, 90.0);
    const double fskRc = 1.0 / (kTwoPi * fskDetectorBandwidth);
    m_fskLpAlpha = qBound(0.0003, dt / (fskRc + dt), 0.30);

    const double pixelRate = m_columnRate * static_cast<double>(kLogicalRasterHeight);
    m_pixelsPerSample = pixelRate / static_cast<double>(sampleRate);
}

void HellschreiberDecoder::processSample(double sample)
{
    sample = qBound(-1.0, sample, 1.0);

    const double normalized = (m_variant == Variant::Fsk105)
                                  ? processFsk105Sample(sample)
                                  : processFeldHellSample(sample);

    m_pixelPhase += m_pixelsPerSample;
    while (m_pixelPhase >= 1.0) {
        m_pixelPhase -= 1.0;
        writePixel(normalized);
    }

    ++m_samplesProcessed;
}

double HellschreiberDecoder::processFeldHellSample(double sample)
{
    const double envelope = processToneDetector(sample,
                                                m_phaseInc,
                                                m_phase,
                                                m_i,
                                                m_q,
                                                m_envelope);

    const double gate = updateSignalGate(envelope);
    const double span = qMax(0.008, m_peak - m_noise);
    const double normalized = qBound(0.0, (envelope - m_noise) / span, 1.0);
    return normalized * gate;
}

double HellschreiberDecoder::processFsk105Sample(double sample)
{
    const double lowEnvelope = processToneDetector(sample,
                                                   m_lowPhaseInc,
                                                   m_lowPhase,
                                                   m_lowI,
                                                   m_lowQ,
                                                   m_lowEnvelope);
    const double highEnvelope = processToneDetector(sample,
                                                    m_highPhaseInc,
                                                    m_highPhase,
                                                    m_highI,
                                                    m_highQ,
                                                    m_highEnvelope);

    const double total = lowEnvelope + highEnvelope;
    const double gate = updateSignalGate(total);

    if (total <= 1.0e-8) {
        return 0.0;
    }

    /* FSK-105 is frequency-shift keyed rather than amplitude keyed.  Match the
     * fldigi convention: the lower tone prints black ink, the upper tone prints
     * white paper.  A soft ratio avoids noisy all-or-nothing bands.
     */
    const double lowRatio = lowEnvelope / qMax(1.0e-8, total);
    const double ink = qBound(0.0, (lowRatio - 0.34) / 0.34, 1.0);
    return ink * gate;
}

double HellschreiberDecoder::processToneDetector(double sample,
                                                  double phaseInc,
                                                  double &phase,
                                                  double &iState,
                                                  double &qState,
                                                  double &envelopeState)
{
    const double c = qCos(phase);
    const double s = qSin(phase);
    phase += phaseInc;
    if (phase >= kTwoPi) {
        phase -= kTwoPi;
    }

    const double mixedI = sample * c;
    const double mixedQ = sample * -s;
    const double alpha = (m_variant == Variant::Fsk105) ? m_fskLpAlpha : m_lpAlpha;
    iState += alpha * (mixedI - iState);
    qState += alpha * (mixedQ - qState);

    const double instantEnvelope = 2.0 * qSqrt((iState * iState) + (qState * qState));
    envelopeState += 0.10 * (instantEnvelope - envelopeState);
    return envelopeState;
}

double HellschreiberDecoder::updateSignalGate(double observedLevel)
{
    if (observedLevel > m_peak) {
        m_peak = (0.985 * m_peak) + (0.015 * observedLevel);
    } else {
        m_peak *= 0.99995;
    }

    if (observedLevel < m_peak * 0.55) {
        m_noise = (0.995 * m_noise) + (0.005 * observedLevel);
    } else {
        m_noise *= 1.00001;
    }

    const double span = qMax(0.008, m_peak - m_noise);
    return qBound(0.0, (observedLevel - m_noise) / span, 1.0);
}

void HellschreiberDecoder::writePixel(double level)
{
    if (m_logicalImage.isNull() || m_image.isNull()) {
        return;
    }

    if (m_column >= kPaperWidth) {
        finishPaperRow();
    }

    const int gray = grayFromLevel(level);
    const int x = qBound(0, m_column, kPaperWidth - 1);
    /* Keep the proven Feld Hell RX orientation used by the stable 0.5.33
     * receiver.  The scan order is flipped into the visible 14-row paper cell.
     */
    const int visualRow = (kLogicalRasterHeight - 1) - m_row;
    setPaperPixel(x, m_paperRow, visualRow, QColor(gray, gray, gray));

    ++m_row;
    if (m_row >= kLogicalRasterHeight) {
        finishColumn();
    }
}

void HellschreiberDecoder::finishColumn()
{
    m_row = 0;
    ++m_column;
    ++m_columnsWritten;

    if (m_column >= kPaperWidth) {
        finishPaperRow();
    }

    if ((m_columnsWritten % 3) == 0) {
        emit imageUpdated(m_image);
    }
}

void HellschreiberDecoder::finishPaperRow()
{
    m_column = 0;
    ++m_paperRowsWritten;
    ++m_paperRow;

    if (m_paperRow >= kVisibleRows) {
        const int shift = kLogicalRasterHeight + kLineGap;
        const int height = logicalPaperHeight();
        QPainter logicalPainter(&m_logicalImage);
        logicalPainter.drawImage(QPoint(0, -shift), m_logicalImage);
        logicalPainter.fillRect(0, height - shift, kPaperWidth, shift, Qt::white);
        logicalPainter.end();
        drawLogicalSeparators();
        m_paperRow = kVisibleRows - 1;
        rebuildDisplayImageFromLogical();
    }
}

int HellschreiberDecoder::paperZoom() const
{
    return qMax(1, m_verticalScale);
}

int HellschreiberDecoder::paperWidth() const
{
    return kPaperWidth * paperZoom();
}

int HellschreiberDecoder::paperHeight() const
{
    return logicalPaperHeight() * paperZoom();
}

int HellschreiberDecoder::displayYForLogicalRow(int paperRow, int logicalRow) const
{
    const int boundedPaperRow = qBound(0, paperRow, kVisibleRows - 1);
    const int boundedLogicalRow = qBound(0, logicalRow, kLogicalRasterHeight - 1);
    const int logicalY = (boundedPaperRow * (kLogicalRasterHeight + kLineGap)) + boundedLogicalRow;
    return logicalY * paperZoom();
}

int HellschreiberDecoder::currentLogicalPaperTop() const
{
    return m_paperRow * (kLogicalRasterHeight + kLineGap);
}

int HellschreiberDecoder::logicalPaperHeight() const
{
    return (kLogicalRasterHeight + kLineGap) * kVisibleRows;
}

void HellschreiberDecoder::setPaperPixel(int column, int paperRow, int logicalRow, const QColor &color)
{
    const int x = qBound(0, column, kPaperWidth - 1);
    const int boundedPaperRow = qBound(0, paperRow, kVisibleRows - 1);
    const int boundedLogicalRow = qBound(0, logicalRow, kLogicalRasterHeight - 1);
    const int logicalY = (boundedPaperRow * (kLogicalRasterHeight + kLineGap)) + boundedLogicalRow;

    if (!m_logicalImage.isNull() && logicalY >= 0 && logicalY < m_logicalImage.height()) {
        m_logicalImage.setPixelColor(x, logicalY, color);
    }

    if (m_image.isNull()) {
        return;
    }

    const int zoom = paperZoom();
    const int displayX = x * zoom;
    const int displayY = displayYForLogicalRow(boundedPaperRow, boundedLogicalRow);
    for (int dx = 0; dx < zoom; ++dx) {
        const int xx = displayX + dx;
        if (xx < 0 || xx >= m_image.width()) {
            continue;
        }
        for (int dy = 0; dy < zoom; ++dy) {
            const int yy = displayY + dy;
            if (yy >= 0 && yy < m_image.height()) {
                m_image.setPixelColor(xx, yy, color);
            }
        }
    }
}

void HellschreiberDecoder::rebuildDisplayImageFromLogical()
{
    const int zoom = paperZoom();
    m_image = QImage(paperWidth(), paperHeight(), QImage::Format_RGB32);
    m_image.fill(Qt::white);

    if (m_logicalImage.isNull()) {
        return;
    }

    for (int sourceY = 0; sourceY < m_logicalImage.height(); ++sourceY) {
        const int destY = sourceY * zoom;
        for (int x = 0; x < kPaperWidth; ++x) {
            const QColor color = m_logicalImage.pixelColor(x, sourceY);
            const int destX = x * zoom;
            for (int dx = 0; dx < zoom; ++dx) {
                const int xx = destX + dx;
                if (xx < 0 || xx >= m_image.width()) {
                    continue;
                }
                for (int dy = 0; dy < zoom; ++dy) {
                    const int yy = destY + dy;
                    if (yy >= 0 && yy < m_image.height()) {
                        m_image.setPixelColor(xx, yy, color);
                    }
                }
            }
        }
    }
}

void HellschreiberDecoder::drawLogicalSeparators()
{
    if (m_logicalImage.isNull()) {
        return;
    }

    QPainter painter(&m_logicalImage);
    painter.setPen(QColor(225, 225, 225));
    for (int row = 1; row < kVisibleRows; ++row) {
        const int y = (row * (kLogicalRasterHeight + kLineGap)) - (kLineGap / 2);
        if (y >= 0 && y < m_logicalImage.height()) {
            painter.drawLine(0, y, kPaperWidth - 1, y);
        }
    }
}

void HellschreiberDecoder::resetPaperImage()
{
    m_logicalImage = QImage(kPaperWidth, logicalPaperHeight(), QImage::Format_RGB32);
    m_logicalImage.fill(Qt::white);
    drawLogicalSeparators();
    rebuildDisplayImageFromLogical();
}

void HellschreiberDecoder::maybeEmitStatus()
{
    ++m_statusCounter;
    if ((m_statusCounter % 10) != 0) {
        return;
    }

    const double snrLike = qMax(1.0, m_peak / qMax(0.001, m_noise));
    if (m_variant == Variant::Fsk105) {
        emit statusChanged(QString("FSK-105 RX: center %1 Hz, shift %2 Hz, %3 col/s, row %4, column %5, S/N %6")
                               .arg(m_toneHz, 0, 'f', 0)
                               .arg(m_fskShiftHz, 0, 'f', 0)
                               .arg(m_columnRate, 0, 'f', 2)
                               .arg(m_paperRowsWritten + 1)
                               .arg(m_column)
                               .arg(snrLike, 0, 'f', 1));
    } else {
        emit statusChanged(QString("Feld Hell RX: %1 Hz, %2 col/s, paper row %3, column %4, S/N %5")
                               .arg(m_toneHz, 0, 'f', 0)
                               .arg(m_columnRate, 0, 'f', 2)
                               .arg(m_paperRowsWritten + 1)
                               .arg(m_column)
                               .arg(snrLike, 0, 'f', 1));
    }
}

void HellschreiberDecoder::emitCurrentMarkers()
{
    emit markersChanged(frequencyMarkers(m_toneHz, m_bandwidthHz, m_variant, m_fskShiftHz));
}
