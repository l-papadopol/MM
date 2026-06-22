#include "CwDecoder.h"

#include "ggmorse/ggmorse.h"

#include <QByteArray>
#include <QColor>
#include <QtGlobal>
#include <QtMath>
#include <algorithm>
#include <cstring>
#include <cstdint>

namespace {

constexpr double kMinSearchHz = 250.0;
constexpr double kMaxSearchHz = 2000.0;
constexpr double kTiny = 1.0e-12;
constexpr double kTargetCwRms = 0.055;
constexpr double kMaxCwAutoGain = 25.0;
constexpr double kMaxCwPeak = 0.90;

bool finitePositive(double v)
{
    return qIsFinite(v) && v > 0.0;
}

} // namespace

CwDecoder::CwDecoder(QObject *parent)
    : QObject(parent)
{
    reset();
}

CwDecoder::~CwDecoder() = default;

QString CwDecoder::modeName()
{
    return QStringLiteral("CW Morse");
}

QVector<FrequencyMarker> CwDecoder::frequencyMarkers(double toneHz)
{
    FrequencyMarker marker;
    marker.frequencyHz = toneHz;
    marker.label = QStringLiteral("CW");
    marker.color = QColor(140, 255, 120);
    QVector<FrequencyMarker> markers;
    markers.append(marker);
    return markers;
}

void CwDecoder::reset()
{
    m_sampleCounter = 0;
    m_statusCounter = 0;
    m_text.clear();
    m_decodedChars = 0;
    m_ggmorseSmoothedWpm = qBound(5.0, m_wpm, 80.0);
    m_ggmorseLastCost = 999.0;
    m_ggmorseLastThreshold = 0.0;
    m_inputRms = 0.0;
    m_inputPeak = 0.0;
    m_inputGain = 1.0;
    m_trackedToneHz = qBound(kMinSearchHz, m_toneHz, kMaxSearchHz);
    resetGgmorse();

    emit textUpdated(m_text);
    emit statusChanged(QStringLiteral("CW ggmorse: %1 Hz, %2 WPM %3")
                           .arg(m_toneHz, 0, 'f', 0)
                           .arg(m_wpm, 0, 'f', 0)
                           .arg(m_autoWpm ? QStringLiteral("auto") : QStringLiteral("manual")));
    emit speedEstimateChanged(trackedWpm());
    emit markersChanged(frequencyMarkers(m_afcEnabled ? m_trackedToneHz : m_toneHz));
}

void CwDecoder::processAudioBlock(const AudioBlock &block)
{
    if (block.samples.isEmpty() || block.sampleRate <= 0) {
        return;
    }

    if (block.sampleRate != m_sampleRate) {
        m_sampleRate = block.sampleRate;
        resetGgmorse();
    }

    m_sampleCounter += block.samples.size();
    updateBlockLevel(block);
    processGgmorseBlock(block);
    maybeEmitStatus();
}

void CwDecoder::setToneHz(double toneHz)
{
    m_toneHz = qBound(kMinSearchHz, toneHz, kMaxSearchHz);
    if (!m_afcEnabled) {
        m_trackedToneHz = m_toneHz;
    } else {
        const double lo = qMax(kMinSearchHz, m_toneHz - m_afcRangeHz);
        const double hi = qMin(kMaxSearchHz, m_toneHz + m_afcRangeHz);
        m_trackedToneHz = qBound(lo, m_trackedToneHz, hi);
    }
    m_ggmorseConfiguredFreqHz = -9999.0;
    emit markersChanged(frequencyMarkers(m_afcEnabled ? m_trackedToneHz : m_toneHz));
}

void CwDecoder::setWpm(double wpm)
{
    m_wpm = qBound(5.0, wpm, 80.0);
    if (!m_autoWpm) {
        m_ggmorseSmoothedWpm = m_wpm;
    }
    m_ggmorseConfiguredWpm = -999.0;
    emit speedEstimateChanged(trackedWpm());
}

void CwDecoder::setAutoWpm(bool enabled)
{
    if (m_autoWpm == enabled) {
        return;
    }
    m_autoWpm = enabled;
    if (!m_autoWpm) {
        m_ggmorseSmoothedWpm = m_wpm;
    }
    m_ggmorseConfiguredWpm = -999.0;
    emit speedEstimateChanged(trackedWpm());
}

void CwDecoder::setBandwidthHz(double bandwidthHz)
{
    m_bandwidthHz = qBound(30.0, bandwidthHz, 500.0);
    m_ggmorseConfiguredMinHz = -1.0;
    m_ggmorseConfiguredMaxHz = -1.0;
}

void CwDecoder::setAfcEnabled(bool enabled)
{
    if (m_afcEnabled == enabled) {
        return;
    }
    m_afcEnabled = enabled;
    if (!m_afcEnabled) {
        m_trackedToneHz = m_toneHz;
    }
    m_ggmorseConfiguredFreqHz = -9999.0;
    emit markersChanged(frequencyMarkers(m_afcEnabled ? m_trackedToneHz : m_toneHz));
}

void CwDecoder::setAfcRangeHz(double rangeHz)
{
    m_afcRangeHz = qBound(5.0, rangeHz, 100.0);
    if (m_afcEnabled) {
        const double lo = qMax(kMinSearchHz, m_toneHz - m_afcRangeHz);
        const double hi = qMin(kMaxSearchHz, m_toneHz + m_afcRangeHz);
        m_trackedToneHz = qBound(lo, m_trackedToneHz, hi);
        m_ggmorseConfiguredMinHz = -1.0;
        m_ggmorseConfiguredMaxHz = -1.0;
        emit markersChanged(frequencyMarkers(m_trackedToneHz));
    }
}

double CwDecoder::toneHz() const
{
    return m_afcEnabled ? m_trackedToneHz : m_toneHz;
}

double CwDecoder::wpm() const
{
    return m_wpm;
}

bool CwDecoder::autoWpm() const
{
    return m_autoWpm;
}

double CwDecoder::trackedWpm() const
{
    return qBound(5.0, m_autoWpm ? m_ggmorseSmoothedWpm : m_wpm, 80.0);
}

double CwDecoder::bandwidthHz() const
{
    return m_bandwidthHz;
}

QString CwDecoder::receivedText() const
{
    return m_text;
}

void CwDecoder::resetGgmorse()
{
    m_ggmorse.reset();
    m_ggmorseFifo.clear();
    m_ggmorseSampleRate = 0;
    m_ggmorseConfiguredFreqHz = -9999.0;
    m_ggmorseConfiguredToneHz = -1.0;
    m_ggmorseConfiguredWpm = -999.0;
    m_ggmorseConfiguredMinHz = -1.0;
    m_ggmorseConfiguredMaxHz = -1.0;
}

void CwDecoder::configureGgmorseForBlock(const AudioBlock &block)
{
    if (block.sampleRate <= 0) {
        return;
    }

    if (!m_ggmorse || m_ggmorseSampleRate != block.sampleRate) {
        GGMorse::Parameters params = GGMorse::getDefaultParameters();
        params.sampleRateInp = static_cast<float>(block.sampleRate);
        params.sampleRateOut = static_cast<float>(block.sampleRate);
        params.samplesPerFrame = GGMorse::kDefaultSamplesPerFrame;
        params.sampleFormatInp = GGMORSE_SAMPLE_FORMAT_F32;
        params.sampleFormatOut = GGMORSE_SAMPLE_FORMAT_F32;
        m_ggmorse.reset(new GGMorse(params));
        m_ggmorseSampleRate = block.sampleRate;
        m_ggmorseFifo.clear();
        m_ggmorseConfiguredFreqHz = -9999.0;
        m_ggmorseConfiguredToneHz = -1.0;
        m_ggmorseConfiguredWpm = -999.0;
        m_ggmorseConfiguredMinHz = -1.0;
        m_ggmorseConfiguredMaxHz = -1.0;
    }

    if (!m_ggmorse) {
        return;
    }

    const double selectedTone = qBound(kMinSearchHz, m_toneHz, kMaxSearchHz);
    const double decodeFreq = m_afcEnabled ? -1.0 : selectedTone;
    const double rangeHalf = m_afcEnabled ? qMax(40.0, m_afcRangeHz) : qMax(80.0, m_bandwidthHz);
    const double minHz = qMax(kMinSearchHz, selectedTone - rangeHalf);
    const double maxHz = qMin(kMaxSearchHz, selectedTone + rangeHalf);
    const double selectedWpm = m_autoWpm ? -1.0 : qBound(5.0, m_wpm, 80.0);

    if (qAbs(m_ggmorseConfiguredFreqHz - decodeFreq) > 0.5 ||
        qAbs(m_ggmorseConfiguredToneHz - selectedTone) > 0.5 ||
        qAbs(m_ggmorseConfiguredWpm - selectedWpm) > 0.5 ||
        qAbs(m_ggmorseConfiguredMinHz - minHz) > 0.5 ||
        qAbs(m_ggmorseConfiguredMaxHz - maxHz) > 0.5) {
        GGMorse::ParametersDecode dec = GGMorse::getDefaultParametersDecode();
        dec.frequency_hz = static_cast<float>(decodeFreq);
        dec.speed_wpm = static_cast<float>(selectedWpm);
        dec.frequencyRangeMin_hz = static_cast<float>(minHz);
        dec.frequencyRangeMax_hz = static_cast<float>(maxHz);
        dec.applyFilterHighPass = true;
        dec.applyFilterLowPass = true;
        m_ggmorse->setParametersDecode(dec);
        m_ggmorseConfiguredFreqHz = decodeFreq;
        m_ggmorseConfiguredToneHz = selectedTone;
        m_ggmorseConfiguredWpm = selectedWpm;
        m_ggmorseConfiguredMinHz = minHz;
        m_ggmorseConfiguredMaxHz = maxHz;
    }
}

void CwDecoder::updateBlockLevel(const AudioBlock &block)
{
    double sum2 = 0.0;
    double peak = 0.0;
    for (float s : block.samples) {
        const double v = static_cast<double>(s);
        sum2 += v * v;
        peak = qMax(peak, qAbs(v));
    }
    const double rms = qSqrt(sum2 / qMax(1, block.samples.size()));
    m_inputRms = rms;
    m_inputPeak = peak;

    double gain = 1.0;
    if (finitePositive(rms) && rms < kTargetCwRms) {
        gain = qBound(1.0, kTargetCwRms / rms, kMaxCwAutoGain);
    }
    if (finitePositive(peak) && peak * gain > kMaxCwPeak) {
        gain = qMax(1.0, kMaxCwPeak / peak);
    }
    m_inputGain = gain;
}

float CwDecoder::leveledSample(float sample) const
{
    const double v = qBound(-1.0, static_cast<double>(sample) * m_inputGain, 1.0);
    return static_cast<float>(v);
}

QString CwDecoder::sanitizeGgmorseText(const QByteArray &bytes) const
{
    QString out;
    out.reserve(bytes.size());
    for (char ch : bytes) {
        const uchar c = static_cast<uchar>(ch);
        if (c == '\0' || c == '\a') {
            continue;
        }
        if (c == '\r' || c == '\n' || c == '\t') {
            if (!out.endsWith(QLatin1Char(' '))) {
                out.append(QLatin1Char(' '));
            }
            continue;
        }
        if (c == '?') {
            continue;
        }
        if (c >= 32 && c < 127) {
            out.append(QChar::fromLatin1(static_cast<char>(c)));
        }
    }
    return out;
}

void CwDecoder::appendDecodedText(const QString &text)
{
    if (text.isEmpty()) {
        return;
    }

    QString cleaned;
    cleaned.reserve(text.size());
    for (QChar ch : text) {
        if (ch == QLatin1Char(' ')) {
            if (cleaned.endsWith(QLatin1Char(' '))) {
                continue;
            }
            if (m_text.endsWith(QLatin1Char(' ')) || (m_text.isEmpty() && cleaned.isEmpty())) {
                continue;
            }
        }
        cleaned.append(ch);
    }
    if (cleaned.isEmpty()) {
        return;
    }

    m_text.append(cleaned);
    if (m_text.size() > 20000) {
        m_text.remove(0, m_text.size() - 20000);
    }
    m_decodedChars += cleaned.size();
    emit characterReceived(cleaned);
    emit textUpdated(m_text);
}

void CwDecoder::emitGgmorseOutput()
{
    if (!m_ggmorse) {
        return;
    }

    GGMorse::TxRx rx;
    while (m_ggmorse->takeRxData(rx) > 0) {
        QByteArray bytes;
        bytes.reserve(static_cast<int>(rx.size()));
        for (std::uint8_t b : rx) {
            bytes.append(static_cast<char>(b));
        }
        appendDecodedText(sanitizeGgmorseText(bytes));
    }

    const GGMorse::Statistics &st = m_ggmorse->getStatistics();
    m_ggmorseLastCost = st.costFunction;
    m_ggmorseLastThreshold = st.signalThreshold;

    if (m_autoWpm && st.estimatedSpeed_wpm >= 5.0f && st.estimatedSpeed_wpm <= 80.0f && st.costFunction < 3.0f) {
        const double candidate = static_cast<double>(st.estimatedSpeed_wpm);
        m_ggmorseSmoothedWpm = (m_ggmorseSmoothedWpm <= 0.0)
            ? candidate
            : ((0.94 * m_ggmorseSmoothedWpm) + (0.06 * candidate));
        emit speedEstimateChanged(trackedWpm());
    }

    maybeUpdateAfcFromGgmorse();
}

void CwDecoder::maybeUpdateAfcFromGgmorse()
{
    if (!m_afcEnabled || !m_ggmorse) {
        return;
    }

    const GGMorse::Statistics &st = m_ggmorse->getStatistics();
    const double pitch = static_cast<double>(st.estimatedPitch_Hz);
    const double lo = qMax(kMinSearchHz, m_toneHz - m_afcRangeHz);
    const double hi = qMin(kMaxSearchHz, m_toneHz + m_afcRangeHz);
    if (!finitePositive(pitch) || pitch < lo || pitch > hi || st.costFunction > 3.5f) {
        return;
    }

    const double old = m_trackedToneHz;
    m_trackedToneHz = qBound(lo, (0.90 * m_trackedToneHz) + (0.10 * pitch), hi);
    if (qAbs(old - m_trackedToneHz) >= 1.0) {
        emit markersChanged(frequencyMarkers(m_trackedToneHz));
    }
}

void CwDecoder::processGgmorseBlock(const AudioBlock &block)
{
    configureGgmorseForBlock(block);
    if (!m_ggmorse || block.samples.isEmpty()) {
        return;
    }

    m_ggmorseFifo.reserve(m_ggmorseFifo.size() + block.samples.size());
    for (float s : block.samples) {
        m_ggmorseFifo.append(leveledSample(s));
    }

    const int maxBuffered = qMax(4096, block.sampleRate * 2);
    if (m_ggmorseFifo.size() > maxBuffered) {
        m_ggmorseFifo.erase(m_ggmorseFifo.begin(), m_ggmorseFifo.begin() + (m_ggmorseFifo.size() - maxBuffered));
    }

    auto inputCb = [this](void *data, std::uint32_t nMaxBytes) -> std::uint32_t {
        const int samplesNeeded = static_cast<int>(nMaxBytes / sizeof(float));
        if (samplesNeeded <= 0 || m_ggmorseFifo.size() < samplesNeeded) {
            return 0;
        }
        std::memcpy(data, m_ggmorseFifo.constData(), static_cast<size_t>(samplesNeeded) * sizeof(float));
        m_ggmorseFifo.erase(m_ggmorseFifo.begin(), m_ggmorseFifo.begin() + samplesNeeded);
        return static_cast<std::uint32_t>(samplesNeeded * sizeof(float));
    };

    m_ggmorse->decode(inputCb);
    emitGgmorseOutput();
}

void CwDecoder::maybeEmitStatus()
{
    ++m_statusCounter;
    if (m_statusCounter < 16) {
        return;
    }
    m_statusCounter = 0;

    emit statusChanged(QStringLiteral("CW ggmorse: %1 Hz, %2 WPM %3, gain %4x, rms %5, peak %6, threshold %7, cost %8, chars %9")
                           .arg(m_afcEnabled ? m_trackedToneHz : m_toneHz, 0, 'f', 0)
                           .arg(trackedWpm(), 0, 'f', 1)
                           .arg(m_autoWpm ? QStringLiteral("auto") : QStringLiteral("manual"))
                           .arg(m_inputGain, 0, 'f', 1)
                           .arg(m_inputRms, 0, 'g', 3)
                           .arg(m_inputPeak, 0, 'g', 3)
                           .arg(m_ggmorseLastThreshold, 0, 'g', 3)
                           .arg(m_ggmorseLastCost, 0, 'f', 2)
                           .arg(m_decodedChars));
    emit markersChanged(frequencyMarkers(m_afcEnabled ? m_trackedToneHz : m_toneHz));
}
