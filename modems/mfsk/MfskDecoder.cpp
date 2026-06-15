#include "MfskDecoder.h"

#include <QColor>
#include <QtMath>


MfskDecoder::MfskDecoder(QObject *parent)
    : QObject(parent)
{
    m_resampler.configure(kInternalSampleRate);
    configureForCurrentSettings();
    reset();
}

QString MfskDecoder::modeName()
{
    return QStringLiteral("MFSK Text");
}

QString MfskDecoder::variantName(Variant variant)
{
    return variant == Variant::Mfsk32 ? QStringLiteral("MFSK32") : QStringLiteral("MFSK16");
}

MfskDecoder::Variant MfskDecoder::variantFromKey(const QString &key)
{
    const QString k = key.trimmed().toUpper();
    if (k == QStringLiteral("MFSK32")) {
        return Variant::Mfsk32;
    }
    return Variant::Mfsk16;
}

QVector<FrequencyMarker> MfskDecoder::frequencyMarkers(double centerHz, Variant variant)
{
    const int tones = (variant == Variant::Mfsk32) ? 32 : 16;
    const double spacing = (variant == Variant::Mfsk32) ? 31.25 : 15.625;
    const double first = centerHz - (0.5 * static_cast<double>(tones - 1) * spacing);
    const double last = first + spacing * static_cast<double>(tones - 1);

    QVector<FrequencyMarker> markers;
    FrequencyMarker low;
    low.frequencyHz = first;
    low.label = variantName(variant) + QStringLiteral(" low");
    low.color = QColor(255, 190, 90);
    markers.append(low);

    FrequencyMarker center;
    center.frequencyHz = centerHz;
    center.label = variantName(variant);
    center.color = QColor(90, 220, 255);
    markers.append(center);

    FrequencyMarker high;
    high.frequencyHz = last;
    high.label = variantName(variant) + QStringLiteral(" high");
    high.color = QColor(255, 190, 90);
    markers.append(high);
    return markers;
}

void MfskDecoder::reset()
{
    m_resampler.reset();
    m_symbolBuffer.clear();
    m_symbolPhase = 0.0;
    m_effectiveCenterHz = m_centerHz;
    m_afcOffsetHz = 0.0;
    m_toneBank.reset();
    m_rxState = 0;
    m_firstDataTone = 0;
    m_text.clear();
    m_decodedChars = 0;
    m_badFrames = 0;
    m_statusCounter = 0;
    m_lastConfidence = 0.0;
    m_lastToneOffsetHz = 0.0;
    m_symbolsSeen = 0;
    configureForCurrentSettings();

    emit textUpdated(m_text);
    emit statusChanged(QStringLiteral("%1: waiting for framed MFSK symbols")
                           .arg(variantName(m_variant)));
    emit markersChanged(frequencyMarkers(m_centerHz, m_variant));
}

void MfskDecoder::setVariant(Variant variant)
{
    if (m_variant == variant) {
        return;
    }
    m_variant = variant;
    reset();
}

void MfskDecoder::setCenterHz(double centerHz)
{
    m_centerHz = qBound(300.0, centerHz, 3300.0);
    m_afcOffsetHz = 0.0;
    m_effectiveCenterHz = m_centerHz;
    configureForCurrentSettings();
    emit markersChanged(frequencyMarkers(m_centerHz, m_variant));
}

void MfskDecoder::setAfcEnabled(bool enabled)
{
    if (m_afcEnabled == enabled) {
        return;
    }
    m_afcEnabled = enabled;
    if (!m_afcEnabled) {
        m_afcOffsetHz = 0.0;
        m_effectiveCenterHz = m_centerHz;
        configureForCurrentSettings();
        emit markersChanged(frequencyMarkers(m_effectiveCenterHz, m_variant));
    }
}

void MfskDecoder::setAfcRangeHz(double rangeHz)
{
    m_afcRangeHz = qBound(5.0, rangeHz, 200.0);
}

MfskDecoder::Variant MfskDecoder::variant() const
{
    return m_variant;
}

double MfskDecoder::centerHz() const
{
    return m_centerHz;
}

bool MfskDecoder::afcEnabled() const
{
    return m_afcEnabled;
}

double MfskDecoder::afcRangeHz() const
{
    return m_afcRangeHz;
}

QString MfskDecoder::receivedText() const
{
    return m_text;
}

void MfskDecoder::processAudioBlock(const AudioBlock &block)
{
    if (block.samples.isEmpty() || block.sampleRate <= 0) {
        return;
    }

    if (block.sampleRate != m_inputSampleRate) {
        m_inputSampleRate = block.sampleRate;
        m_resampler.reset();
    }

    const QVector<double> internal = m_resampler.process(block.samples, block.sampleRate);
    for (double sample : internal) {
        processInternalSample(sample);
    }

    maybeEmitStatus();
}

int MfskDecoder::toneCount() const
{
    return m_variant == Variant::Mfsk32 ? 32 : 16;
}

double MfskDecoder::symbolRate() const
{
    return m_variant == Variant::Mfsk32 ? 31.25 : 15.625;
}

double MfskDecoder::toneSpacingHz() const
{
    return symbolRate();
}

double MfskDecoder::firstToneHz() const
{
    return m_effectiveCenterHz - (0.5 * static_cast<double>(toneCount() - 1) * toneSpacingHz());
}

void MfskDecoder::configureForCurrentSettings()
{
    m_symbolSamples = kInternalSampleRate / symbolRate();
    m_symbolBuffer.reserve(static_cast<int>(qCeil(m_symbolSamples)) + 8);
    m_toneBank.configure(kInternalSampleRate, firstToneHz(), toneSpacingHz(), toneCount(),
                         static_cast<int>(qRound(m_symbolSamples)));
}

void MfskDecoder::processInternalSample(double sample)
{
    m_symbolBuffer.append(qBound(-1.0, sample, 1.0));
    m_symbolPhase += 1.0;

    if (m_symbolPhase >= m_symbolSamples) {
        processSymbol(m_symbolBuffer);
        m_symbolBuffer.clear();
        m_symbolPhase -= m_symbolSamples;
        ++m_symbolsSeen;
    }
}

int MfskDecoder::detectTone(const QVector<double> &symbol, double *confidenceOut, double *offsetHzOut)
{
    const GoertzelToneBank::Result result = m_toneBank.analyse(symbol, m_afcEnabled);
    if (confidenceOut != nullptr) {
        *confidenceOut = result.confidence;
    }
    if (offsetHzOut != nullptr) {
        *offsetHzOut = result.offsetHz;
    }
    return result.bestIndex;
}

void MfskDecoder::processSymbol(const QVector<double> &symbol)
{
    double confidence = 0.0;
    double offsetHz = 0.0;
    const int tone = detectTone(symbol, &confidence, &offsetHz);
    m_lastConfidence = confidence;
    m_lastToneOffsetHz = offsetHz;
    if (tone < 0) {
        return;
    }

    if (confidence < 1.20) {
        ++m_badFrames;
        m_rxState = 0;
        return;
    }

    updateAfcFromSymbol(offsetHz, confidence);
    handleTone(tone, confidence);
}


void MfskDecoder::updateAfcFromSymbol(double offsetHz, double confidence)
{
    if (!m_afcEnabled || confidence < 1.45) {
        return;
    }

    const double pull = qBound(-toneSpacingHz() * 0.20, offsetHz, toneSpacingHz() * 0.20);
    const double next = qBound(-m_afcRangeHz, (0.985 * m_afcOffsetHz) + (0.015 * (m_afcOffsetHz + pull)), m_afcRangeHz);
    if (qAbs(next - m_afcOffsetHz) < 0.002) {
        return;
    }

    m_afcOffsetHz = next;
    m_effectiveCenterHz = qBound(300.0, m_centerHz + m_afcOffsetHz, 3300.0);
    configureForCurrentSettings();
}

void MfskDecoder::handleTone(int toneIndex, double confidence)
{
    Q_UNUSED(confidence)
    const int tones = toneCount();
    const int startTone = tones - 1;

    if (m_rxState == 0) {
        if (toneIndex == startTone) {
            m_rxState = 1;
            m_firstDataTone = 0;
        }
        return;
    }

    if (m_rxState == 1) {
        m_firstDataTone = toneIndex;
        m_rxState = 2;
        return;
    }

    int code = 0;
    if (m_variant == Variant::Mfsk32) {
        code = ((m_firstDataTone & 0x1F) << 3) | (toneIndex & 0x07);
    } else {
        code = ((m_firstDataTone & 0x0F) << 4) | (toneIndex & 0x0F);
    }

    finishCharacter(code);
    m_rxState = 0;
}

void MfskDecoder::finishCharacter(int code)
{
    if (code == '\r') {
        return;
    }

    if (code == '\n' || code == '\t' || (code >= 32 && code <= 126)) {
        const QString decoded(QChar(static_cast<ushort>(code)));
        m_text.append(decoded);
        if (m_text.size() > 20000) {
            m_text.remove(0, m_text.size() - 20000);
        }
        ++m_decodedChars;
        emit characterReceived(decoded);
        emit textUpdated(m_text);
        return;
    }

    ++m_badFrames;
}

void MfskDecoder::maybeEmitStatus()
{
    ++m_statusCounter;
    if (m_statusCounter < 8) {
        return;
    }
    m_statusCounter = 0;

    emit statusChanged(QStringLiteral("%1: center %2 Hz%3, %4 tones, spacing %5 Hz, confidence %6, chars %7, bad %8")
                           .arg(variantName(m_variant))
                           .arg(m_effectiveCenterHz, 0, 'f', 1)
                           .arg(m_afcEnabled ? (QStringLiteral(" (AFC ") + (m_afcOffsetHz >= 0.0 ? QStringLiteral("+") : QString()) + QString::number(m_afcOffsetHz, 'f', 1) + QStringLiteral(" Hz)")) : QString())
                           .arg(toneCount())
                           .arg(toneSpacingHz(), 0, 'f', 3)
                           .arg(m_lastConfidence, 0, 'f', 2)
                           .arg(m_decodedChars)
                           .arg(m_badFrames));
    emit markersChanged(frequencyMarkers(m_effectiveCenterHz, m_variant));
}
