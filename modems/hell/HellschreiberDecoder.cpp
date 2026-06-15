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

    m_image = QImage(kPaperWidth, kPaperHeight, QImage::Format_RGB32);
    m_image.fill(Qt::white);

    QPainter painter(&m_image);
    painter.setPen(QColor(225, 225, 225));
    for (int y = kRasterHeight; y < kPaperHeight; y += (kRasterHeight + kLineGap)) {
        painter.drawLine(0, y + (kLineGap / 2), kPaperWidth - 1, y + (kLineGap / 2));
    }

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
    if (m_image.isNull()) {
        return;
    }

    const int gray = grayFromLevel(level);
    const int x = qBound(0, m_column, kPaperWidth - 1);
    const int y = qBound(0, currentPaperTop() + (m_row * kVerticalScale), kPaperHeight - 1);

    /* Enlarge the visual paper row only.  The modem timing remains based on
     * kLogicalRasterHeight, so 17.5 columns/s is not slowed down.
     */
    for (int dy = 0; dy < kVerticalScale; ++dy) {
        const int yy = y + dy;
        if (yy >= 0 && yy < kPaperHeight) {
            m_image.setPixelColor(x, yy, QColor(gray, gray, gray));
        }
    }

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
        const int shift = kRasterHeight + kLineGap;
        QPainter painter(&m_image);
        painter.drawImage(QPoint(0, -shift), m_image);
        painter.fillRect(0, kPaperHeight - shift, kPaperWidth, shift, Qt::white);
        painter.setPen(QColor(225, 225, 225));
        painter.drawLine(0,
                         kPaperHeight - (kLineGap / 2),
                         kPaperWidth - 1,
                         kPaperHeight - (kLineGap / 2));
        m_paperRow = kVisibleRows - 1;
    }
}

int HellschreiberDecoder::currentPaperTop() const
{
    return m_paperRow * (kRasterHeight + kLineGap);
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
