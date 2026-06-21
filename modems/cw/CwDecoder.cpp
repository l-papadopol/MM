#include "CwDecoder.h"

#include "ggmorse/ggmorse.h"

#include <QByteArray>
#include <QColor>
#include <QRegularExpression>
#include <QMetaType>
#include <QtMath>
#include <algorithm>
#include <cstring>
#include <utility>

namespace {

constexpr double kTwoPi = 6.283185307179586476925286766559;
constexpr double kMinSearchHz = 250.0;
constexpr double kMaxSearchHz = 2000.0;
constexpr double kAfcStepHz = 5.0;
constexpr double kGateSeconds = 0.004;      // Same order as fldigi DEC_RATIO gate.
constexpr double kAfcFrameSeconds = 0.024;  // Slow local AFC frame, not the detector.
constexpr double kDcBlockR = 0.995;
constexpr double kTiny = 1.0e-12;

struct MorseSymbol
{
    const char *pattern;
    const char *text;
};

const MorseSymbol *morseSymbols()
{
    static const MorseSymbol table[] = {
        {".-", "A"}, {"-...", "B"}, {"-.-.", "C"}, {"-..", "D"}, {".", "E"},
        {"..-.", "F"}, {"--.", "G"}, {"....", "H"}, {"..", "I"}, {".---", "J"},
        {"-.-", "K"}, {".-..", "L"}, {"--", "M"}, {"-.", "N"}, {"---", "O"},
        {".--.", "P"}, {"--.-", "Q"}, {".-.", "R"}, {"...", "S"}, {"-", "T"},
        {"..-", "U"}, {"...-", "V"}, {".--", "W"}, {"-..-", "X"}, {"-.--", "Y"},
        {"--..", "Z"},
        {".----", "1"}, {"..---", "2"}, {"...--", "3"}, {"....-", "4"}, {".....", "5"},
        {"-....", "6"}, {"--...", "7"}, {"---..", "8"}, {"----.", "9"}, {"-----", "0"},
        {".-.-.-", "."}, {"--..--", ","}, {"..--..", "?"}, {".----.", "'"},
        {"-.-.--", "!"}, {"-..-.", "/"}, {"-.--.", "("}, {"-.--.-", ")"},
        {".-...", "&"}, {"---...", ":"}, {"-.-.-.", ";"}, {"-...-", "="},
        {".-.-.", "+"}, {"-....-", "-"}, {"..--.-", "_"}, {".-..-.", "\""},
        {"...-..-", "$"}, {".--.-.", "@"},
        {"...-.-", "<SK>"}, {".-.-.", "<AR>"}, {"-...-", "<BT>"},
        {nullptr, nullptr}
    };
    return table;
}

QHash<QString, QString> morseDecodeTable()
{
    QHash<QString, QString> table;
    for (const MorseSymbol *s = morseSymbols(); s->pattern; ++s) {
        if (!table.contains(QString::fromLatin1(s->pattern))) {
            table.insert(QString::fromLatin1(s->pattern), QString::fromLatin1(s->text));
        }
    }
    return table;
}

int bitCount4(quint8 v)
{
    v &= 0x0f;
    int n = 0;
    for (int i = 0; i < 4; ++i) {
        if (v & (1u << i)) {
            ++n;
        }
    }
    return n;
}

bool isFinitePositive(double v)
{
    return qIsFinite(v) && v > 0.0;
}

double medianOf(QVector<double> values)
{
    if (values.isEmpty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    const int n = values.size();
    if ((n % 2) != 0) {
        return values.at(n / 2);
    }
    return 0.5 * (values.at((n / 2) - 1) + values.at(n / 2));
}

QString normalizeRadioCwSpacing(QString text)
{
    if (text.isEmpty()) {
        return text;
    }

    static const QRegularExpression reDe(
        QStringLiteral("(^|\\s)DE([A-Z]{1,3}\\d[A-Z0-9/]{1,12})"));
    static const QRegularExpression reCq(
        QStringLiteral("(^|\\s)CQ([A-Z]{1,3}\\d[A-Z0-9/]{1,12})"));
    static const QRegularExpression reKnownProsig(
        QStringLiteral("(^|\\s)(K|KN|AR|SK)([A-Z]{1,3}\\d[A-Z0-9/]{1,12})"));
    static const QRegularExpression reSplitCallTail(
        QStringLiteral("\\b([A-Z]{1,3}\\d[A-Z0-9/]{1,8})\\s+([A-Z]{1,2})(?=\\s|$)"));
    static const QRegularExpression reDoubleSpaces(QStringLiteral(" {2,}"));

    text.replace(reDe, QStringLiteral("\\1DE \\2"));
    text.replace(reCq, QStringLiteral("\\1CQ \\2"));
    text.replace(reKnownProsig, QStringLiteral("\\1\\2 \\3"));
    text.replace(reSplitCallTail, QStringLiteral("\\1\\2"));
    text.replace(reDoubleSpaces, QStringLiteral(" "));
    return text;
}

} // namespace

CwDecoder::CwDecoder(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<QVector<float>>("QVector<float>");
    reset();
}

CwDecoder::~CwDecoder() = default;

QString CwDecoder::modeName()
{
    return "CW Morse";
}

QVector<FrequencyMarker> CwDecoder::frequencyMarkers(double toneHz)
{
    FrequencyMarker marker;
    marker.frequencyHz = toneHz;
    marker.label = "CW";
    marker.color = QColor(140, 255, 120);
    QVector<FrequencyMarker> markers;
    markers.append(marker);
    return markers;
}

void CwDecoder::reset()
{
    m_phase = 0.0;
    m_lastInput = 0.0;
    m_dcState = 0.0;
    m_i1 = 0.0;
    m_q1 = 0.0;
    m_i2 = 0.0;
    m_q2 = 0.0;
    m_env = 0.0;
    m_gateSamples = 0;
    m_gateAccumulator = 0.0;
    m_gateValue = 0.0;
    m_toneHistory = 0;
    m_keyDown = false;
    m_haveElement = false;
    m_wordSpaceSent = true;
    m_sampleCounter = 0;
    m_keyDownStart = 0;
    m_keyUpStart = 0;
    m_currentPattern.clear();
    m_currentDurations.clear();
    m_recentElements.clear();
    m_recentGaps.clear();
    m_mindEnvelopeHistory.clear();
    m_text.clear();
    m_dotSeconds = 1.2 / qMax(5.0, m_wpm);
    m_trackingDotSeconds = m_dotSeconds;
    m_lastElementSeconds = 0.0;
    m_lastEmittedTrackedWpm = 0.0;
    m_lastFuzzyPattern.clear();
    m_lastFuzzyScore = 0.0;
    m_mindAssistUsed = 0;
    m_mindAssistDisagreed = 0;
    m_mindNativeChars = 0;
    m_mindGgmorseSuppressedChars = 0;
    m_decodedChars = 0;
    m_badPatterns = 0;
    m_statusCounter = 0;
    m_ggmorseFifo.clear();
    m_ggmorseSmoothedWpm = qBound(5.0, m_wpm, 80.0);
    m_ggmorseLastCost = 999.0;
    m_lastGgmorseOutputSample = -1;
    m_trackedToneHz = qBound(kMinSearchHz, m_toneHz, kMaxSearchHz);
    m_lastAfcBestHz = m_trackedToneHz;
    m_lastAfcMarginDb = 0.0;

    resetSignalTracking();

    if (m_sampleRate > 0) {
        configureForSampleRate(m_sampleRate);
    }

    emit textUpdated(m_text);
    emit statusChanged(QString("CW fldigi-like: %1 Hz, %2 WPM %3, waiting for keyed tone")
                           .arg(m_trackedToneHz, 0, 'f', 0)
                           .arg(m_wpm, 0, 'f', 0)
                           .arg(m_autoWpm ? "auto" : "manual"));
    emit speedEstimateChanged(trackedWpm());
    emit markersChanged(frequencyMarkers(m_afcEnabled ? m_trackedToneHz : m_toneHz));
}

void CwDecoder::resetSignalTracking()
{
    m_noiseLevel = 1.0e-4;
    m_signalLevel = 8.0e-4;
    m_openThreshold = 5.0e-4;
    m_closeThreshold = 3.0e-4;
    m_marginDb = 0.0;
}

void CwDecoder::processAudioBlock(const AudioBlock &block)
{
    if (block.samples.isEmpty() || block.sampleRate <= 0) {
        return;
    }

    if (block.sampleRate != m_sampleRate) {
        m_sampleRate = block.sampleRate;
        configureForSampleRate(m_sampleRate);
    }

    for (float input : block.samples) {
        processSample(static_cast<double>(input));
    }

    if (m_useGgmorsePrimary) {
        processGgmorseBlock(block);
    }

    querySilenceDecoder();
    maybeEmitStatus();
}

void CwDecoder::setToneHz(double toneHz)
{
    m_toneHz = qBound(250.0, toneHz, 2500.0);
    if (!m_afcEnabled) {
        m_trackedToneHz = qBound(kMinSearchHz, m_toneHz, kMaxSearchHz);
        m_phase = 0.0;
    } else {
        const double lo = qMax(kMinSearchHz, m_toneHz - m_afcRangeHz);
        const double hi = qMin(kMaxSearchHz, m_toneHz + m_afcRangeHz);
        m_trackedToneHz = qBound(lo, m_trackedToneHz, hi);
    }
    m_ggmorseConfiguredToneHz = -1.0;
    emit markersChanged(frequencyMarkers(m_afcEnabled ? m_trackedToneHz : m_toneHz));
}

void CwDecoder::setWpm(double wpm)
{
    m_wpm = qBound(5.0, wpm, 80.0);
    m_dotSeconds = 1.2 / m_wpm;
    if (!m_autoWpm) {
        m_trackingDotSeconds = m_dotSeconds;
    }
    m_ggmorseConfiguredWpm = -999.0;
    rebuildDetectorConstants();
}

void CwDecoder::setAutoWpm(bool enabled)
{
    if (m_autoWpm == enabled) {
        return;
    }
    m_autoWpm = enabled;
    if (!m_autoWpm) {
        m_trackingDotSeconds = m_dotSeconds;
    }
    m_ggmorseConfiguredWpm = -999.0;
    rebuildDetectorConstants();
    emit speedEstimateChanged(trackedWpm());
}

void CwDecoder::setBandwidthHz(double bandwidthHz)
{
    m_bandwidthHz = qBound(30.0, bandwidthHz, 500.0);
    rebuildDetectorConstants();
}

void CwDecoder::setAfcEnabled(bool enabled)
{
    if (m_afcEnabled == enabled) {
        return;
    }
    m_afcEnabled = enabled;
    if (!m_afcEnabled) {
        m_trackedToneHz = qBound(kMinSearchHz, m_toneHz, kMaxSearchHz);
        m_phase = 0.0;
    }
    m_ggmorseConfiguredToneHz = -1.0;
    emit markersChanged(frequencyMarkers(m_afcEnabled ? m_trackedToneHz : m_toneHz));
}

void CwDecoder::setAfcRangeHz(double rangeHz)
{
    m_afcRangeHz = qBound(5.0, rangeHz, 100.0);
    if (m_afcEnabled) {
        const double lo = qMax(kMinSearchHz, m_toneHz - m_afcRangeHz);
        const double hi = qMin(kMaxSearchHz, m_toneHz + m_afcRangeHz);
        m_trackedToneHz = qBound(lo, m_trackedToneHz, hi);
        m_ggmorseConfiguredToneHz = -1.0;
        emit markersChanged(frequencyMarkers(m_trackedToneHz));
    }
}

void CwDecoder::setMindEventAssistEnabled(bool enabled)
{
    if (m_mindEventAssistEnabled == enabled) {
        return;
    }
    m_mindEventAssistEnabled = enabled;
    if (!enabled) {
        m_mindAssistUsed = 0;
        m_mindAssistDisagreed = 0;
        m_mindNativeChars = 0;
        m_mindGgmorseSuppressedChars = 0;
    }
}

void CwDecoder::setMindEventClassifier(std::function<bool(const QVector<float> &, QVector<float> *, double *)> classifier)
{
    m_mindEventClassifier = std::move(classifier);
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
    return 1.2 / qMax(0.010, m_trackingDotSeconds);
}

double CwDecoder::bandwidthHz() const
{
    return m_bandwidthHz;
}

QString CwDecoder::receivedText() const
{
    return m_text;
}

void CwDecoder::configureForSampleRate(int sampleRate)
{
    if (sampleRate <= 0) {
        return;
    }

    m_gateSamplesNeeded = qMax(16, qRound(sampleRate * kGateSeconds));
    m_afcFrameSamplesNeeded = qMax(128, qRound(sampleRate * kAfcFrameSeconds));
    m_afcFrameSamples.clear();
    m_afcFrameSamples.reserve(m_afcFrameSamplesNeeded);

    m_searchFrequencies.clear();
    for (double f = kMinSearchHz; f <= kMaxSearchHz + 0.1; f += 25.0) {
        if (f < sampleRate * 0.45) {
            m_searchFrequencies.append(f);
        }
    }

    rebuildDetectorConstants();
}

void CwDecoder::rebuildDetectorConstants()
{
    if (m_sampleRate <= 0) {
        return;
    }

    // fldigi's matched filter defaults to roughly 5*WPM/1.2 Hz.  Respect the
    // UI bandwidth but never let the baseband envelope become so wide that it
    // tracks random passband peaks.
    const double fldigiMatchedWidth = 5.0 * qMax(5.0, trackedWpm()) / 1.2;
    const double effectiveWidth = qBound(30.0, qMin(m_bandwidthHz, qMax(45.0, fldigiMatchedWidth)), 500.0);
    const double lowPassCutoff = qBound(18.0, effectiveWidth * 0.55, 280.0);
    m_lpfAlpha = 1.0 - qExp(-kTwoPi * lowPassCutoff / static_cast<double>(m_sampleRate));
    m_lpfAlpha = qBound(0.001, m_lpfAlpha, 0.35);

    // Smooth the detected envelope over a small fraction of a dit.  This rejects
    // QRN spikes but still preserves the edges needed for 30+ WPM CW.
    const double envTau = qBound(0.0035, 0.16 * m_trackingDotSeconds, 0.0180);
    m_envAlpha = 1.0 - qExp(-1.0 / (envTau * static_cast<double>(m_sampleRate)));
    m_envAlpha = qBound(0.001, m_envAlpha, 0.50);
}

void CwDecoder::processSample(double sample)
{
    if (m_sampleRate <= 0) {
        return;
    }

    sample = qBound(-1.0, sample, 1.0);
    ++m_sampleCounter;

    // DC blocker.  Sound-card interfaces and laptop microphones often have a
    // little offset; letting it enter the mixer makes the low-frequency envelope
    // gate drift.
    const double dcBlocked = sample - m_lastInput + (kDcBlockR * m_dcState);
    m_lastInput = sample;
    m_dcState = dcBlocked;

    const double oscI = qCos(m_phase);
    const double oscQ = -qSin(m_phase);
    const double mixI = dcBlocked * oscI;
    const double mixQ = dcBlocked * oscQ;

    m_phase += kTwoPi * qBound(kMinSearchHz, m_trackedToneHz, kMaxSearchHz) /
               static_cast<double>(m_sampleRate);
    while (m_phase >= kTwoPi) {
        m_phase -= kTwoPi;
    }

    // Two cascaded one-pole low-pass filters.  This is cheap enough for old PCs
    // and gives a CW baseband response close to the narrow fldigi CW path.
    m_i1 += m_lpfAlpha * (mixI - m_i1);
    m_q1 += m_lpfAlpha * (mixQ - m_q1);
    m_i2 += m_lpfAlpha * (m_i1 - m_i2);
    m_q2 += m_lpfAlpha * (m_q1 - m_q2);

    const double mag = qSqrt((m_i2 * m_i2) + (m_q2 * m_q2));
    // Human fist recovery: use a faster attack and a gentler release.  Fast
    // attacks preserve short dits; gentler release avoids QRN/QSB punching
    // holes inside one element without smearing long word gaps too much.
    const double envAlpha = (mag > m_env)
        ? qMin(0.75, m_envAlpha * 1.65)
        : qMax(0.0005, m_envAlpha * 0.72);
    m_env += envAlpha * (mag - m_env);

    // MIND CW event profile: keep a compact envelope history around the
    // selected tone.  It is emitted only when an event is classified by the
    // classical CW timing state machine; MIND learns from this teacher signal
    // and does not generate final text.
    m_mindEnvelopeHistory.append(m_env);
    while (m_mindEnvelopeHistory.size() > 512) {
        m_mindEnvelopeHistory.removeFirst();
    }

    m_gateAccumulator += m_env;
    ++m_gateSamples;
    if (m_gateSamples >= m_gateSamplesNeeded) {
        updateDetectorGate();
        m_gateAccumulator = 0.0;
        m_gateSamples = 0;
    }

    if (m_afcEnabled) {
        m_afcFrameSamples.append(dcBlocked);
        if (m_afcFrameSamples.size() >= m_afcFrameSamplesNeeded) {
            processAfcFrame();
            m_afcFrameSamples.clear();
        }
    }
}

void CwDecoder::resetGgmorse()
{
    m_ggmorse.reset();
    m_ggmorseFifo.clear();
    m_ggmorseSampleRate = 0;
    m_ggmorseConfiguredToneHz = -1.0;
    m_ggmorseConfiguredWpm = -999.0;
    m_ggmorseLastCost = 999.0;
}

void CwDecoder::configureGgmorseForBlock(const AudioBlock &block)
{
    if (block.sampleRate <= 0) {
        return;
    }

    const double selectedTone = qBound(kMinSearchHz, m_afcEnabled ? m_trackedToneHz : m_toneHz, kMaxSearchHz);
    const double selectedWpm = m_autoWpm ? -1.0 : qBound(5.0, m_wpm, 80.0);

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
        m_ggmorseConfiguredToneHz = -1.0;
        m_ggmorseConfiguredWpm = -999.0;
    }

    if (!m_ggmorse) {
        return;
    }

    if (qAbs(m_ggmorseConfiguredToneHz - selectedTone) > 0.5 ||
        qAbs(m_ggmorseConfiguredWpm - selectedWpm) > 0.5) {
        GGMorse::ParametersDecode dec = GGMorse::getDefaultParametersDecode();
        // Fixed marker-frequency decode: the operator's green marker remains
        // authoritative.  ggmorse can estimate speed, but it must not retune the
        // UI behind the operator's back.
        dec.frequency_hz = static_cast<float>(selectedTone);
        dec.speed_wpm = static_cast<float>(selectedWpm);
        dec.frequencyRangeMin_hz = static_cast<float>(qMax(kMinSearchHz, selectedTone - qMax(80.0, m_bandwidthHz)));
        dec.frequencyRangeMax_hz = static_cast<float>(qMin(kMaxSearchHz, selectedTone + qMax(80.0, m_bandwidthHz)));
        // Let ggmorse isolate the selected note before its windowed peak and
        // spacing estimator runs.  With the filters disabled, adjacent CW/QRM
        // energy can fill the silences and glue words together.
        dec.applyFilterHighPass = true;
        dec.applyFilterLowPass = true;
        m_ggmorse->setParametersDecode(dec);
        m_ggmorseConfiguredToneHz = selectedTone;
        m_ggmorseConfiguredWpm = selectedWpm;
    }
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
            // ggmorse emits '?' for ambiguous characters.  Do not fill the CW
            // terminal with low-confidence punctuation while the operator is
            // trying to copy a callsign.  The native status line still reports
            // bad/fuzzy patterns for diagnostics.
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

    // Normalize only very common CW glue cases such as DEIW6DRH or CQIZ6NNH.
    // This restores radio-token spacing; it does not invent callsigns or
    // characters, and the raw decoder stream remains otherwise unchanged.
    const QString normalized = normalizeRadioCwSpacing(text);

    QString cleaned;
    cleaned.reserve(normalized.size());
    for (QChar ch : normalized) {
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
    m_text = normalizeRadioCwSpacing(m_text);
    if (m_text.size() > 20000) {
        m_text.remove(0, m_text.size() - 20000);
    }
    m_decodedChars += cleaned.size();
    m_lastGgmorseOutputSample = m_sampleCounter;
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
        const QString decoded = sanitizeGgmorseText(bytes);
        if (mindHeavyHumanFistAssistActive()) {
            // In CW Active, MIND is deliberately used as a heavy human-fist
            // event/timing helper.  ggmorse remains available for speed/cost
            // estimation, but its final text stream is suppressed so the
            // MIND-biased native event decoder can handle irregular spacing.
            m_mindGgmorseSuppressedChars += decoded.size();
        } else {
            appendDecodedText(decoded);
        }
    }

    const GGMorse::Statistics &st = m_ggmorse->getStatistics();
    m_ggmorseLastCost = st.costFunction;
    if (m_autoWpm && st.estimatedSpeed_wpm >= 5.0f && st.estimatedSpeed_wpm <= 60.0f && st.costFunction < 2.2f) {
        const double candidate = static_cast<double>(st.estimatedSpeed_wpm);
        if (m_ggmorseSmoothedWpm <= 0.0) {
            m_ggmorseSmoothedWpm = candidate;
        } else {
            // Very slow UI/timing smoothing.  The user reported that the WPM box
            // jumped constantly; this keeps copy spacing stable while still
            // adapting over a few characters.
            m_ggmorseSmoothedWpm = (0.94 * m_ggmorseSmoothedWpm) + (0.06 * candidate);
        }
        const double oldDot = m_trackingDotSeconds;
        m_trackingDotSeconds = 1.2 / qBound(5.0, m_ggmorseSmoothedWpm, 60.0);
        if (qAbs(oldDot - m_trackingDotSeconds) > 0.0015) {
            rebuildDetectorConstants();
        }
        const double displayWpm = trackedWpm();
        if (qAbs(displayWpm - m_lastEmittedTrackedWpm) >= 0.8) {
            m_lastEmittedTrackedWpm = displayWpm;
            emit speedEstimateChanged(displayWpm);
        }
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
        m_ggmorseFifo.append(s);
    }

    // Bound latency/memory if capture is stopped or if ggmorse temporarily asks
    // for more data than the current block provides.
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

void CwDecoder::updateDetectorGate()
{
    if (m_gateSamples <= 0) {
        return;
    }

    const double value = m_gateAccumulator / static_cast<double>(m_gateSamples);
    m_gateValue = value;

    // Noise floor: fast down, very slow up.  Signal level: fast up, slow down.
    // This follows QSB without allowing one strong dash to permanently raise
    // the squelch threshold.
    if (!m_keyDown) {
        if (value < m_noiseLevel) {
            m_noiseLevel = (0.72 * m_noiseLevel) + (0.28 * value);
        } else {
            m_noiseLevel = (0.997 * m_noiseLevel) + (0.003 * value);
        }
    } else {
        m_noiseLevel *= 0.9995;
    }

    if (value > m_signalLevel) {
        m_signalLevel = (0.78 * m_signalLevel) + (0.22 * value);
    } else {
        const double floorSignal = m_noiseLevel * 4.0;
        m_signalLevel = (0.996 * m_signalLevel) + (0.004 * qMax(value, floorSignal));
    }

    if (m_signalLevel < m_noiseLevel * 3.0) {
        m_signalLevel = m_noiseLevel * 3.0;
    }

    const double span = qMax(m_signalLevel - m_noiseLevel, m_noiseLevel * 1.5);
    m_openThreshold = m_noiseLevel + (0.58 * span);
    m_closeThreshold = m_noiseLevel + (0.36 * span);
    if (m_closeThreshold > m_openThreshold * 0.92) {
        m_closeThreshold = m_openThreshold * 0.92;
    }

    m_marginDb = 20.0 * qLn((value + kTiny) / (m_noiseLevel + kTiny)) / qLn(10.0);

    bool rawTone = m_keyDown ? (value >= m_closeThreshold) : (value >= m_openThreshold);
    // A little absolute margin avoids opening on tiny random fluctuations when
    // the passband is empty.  While already keyed, allow weaker frames to survive
    // QSB dips.
    if (m_marginDb < (m_keyDown ? 2.5 : 5.5)) {
        rawTone = false;
    }

    m_toneHistory = static_cast<quint8>(((m_toneHistory << 1) | (rawTone ? 1 : 0)) & 0x0f);
    const int activeFrames = bitCount4(m_toneHistory);
    const bool debouncedTone = m_keyDown ? (activeFrames >= 1) : (activeFrames >= 3);
    updateKeyState(debouncedTone);
}

void CwDecoder::processAfcFrame()
{
    if (!m_afcEnabled || m_afcFrameSamples.isEmpty() || m_sampleRate <= 0) {
        return;
    }

    QVector<double> windowed;
    windowed.resize(m_afcFrameSamples.size());
    double mean = 0.0;
    for (double v : m_afcFrameSamples) {
        mean += v;
    }
    mean /= static_cast<double>(m_afcFrameSamples.size());

    const int n = windowed.size();
    for (int i = 0; i < n; ++i) {
        const double a = (n > 1) ? (static_cast<double>(i) / static_cast<double>(n - 1)) : 0.0;
        const double blackman = 0.42 - 0.5 * qCos(kTwoPi * a) + 0.08 * qCos(2.0 * kTwoPi * a);
        windowed[i] = (m_afcFrameSamples.at(i) - mean) * blackman;
    }

    const double lo = qMax(kMinSearchHz, m_toneHz - m_afcRangeHz);
    const double hi = qMin(kMaxSearchHz, m_toneHz + m_afcRangeHz);
    double bestPower = 0.0;
    double bestFreq = m_trackedToneHz;
    double sumPower = 0.0;
    int bins = 0;
    for (double f = lo; f <= hi + 0.1; f += kAfcStepHz) {
        if (f >= m_sampleRate * 0.45) {
            continue;
        }
        const double p = goertzelMagnitudeSquared(windowed, f);
        sumPower += p;
        ++bins;
        if (p > bestPower) {
            bestPower = p;
            bestFreq = f;
        }
    }

    if (bins <= 1 || bestPower <= 0.0) {
        return;
    }

    const double avgPower = qMax(kTiny, (sumPower - bestPower) / qMax(1, bins - 1));
    m_lastAfcMarginDb = 10.0 * qLn((bestPower + kTiny) / avgPower) / qLn(10.0);
    m_lastAfcBestHz = bestFreq;

    // Move only on frames that already look like keyed CW.  This prevents the
    // old behaviour where the marker wandered around a silent waterfall.
    if ((m_keyDown || m_marginDb > 8.0) && m_lastAfcMarginDb > 3.0) {
        m_trackedToneHz = qBound(lo, (0.92 * m_trackedToneHz) + (0.08 * bestFreq), hi);
    }
}

double CwDecoder::goertzelMagnitudeSquared(const QVector<double> &samples, double freqHz) const
{
    if (samples.isEmpty() || m_sampleRate <= 0) {
        return 0.0;
    }

    const double omega = kTwoPi * freqHz / static_cast<double>(m_sampleRate);
    const double coeff = 2.0 * qCos(omega);
    double q1 = 0.0;
    double q2 = 0.0;
    for (double x : samples) {
        const double q0 = (coeff * q1) - q2 + x;
        q2 = q1;
        q1 = q0;
    }
    return (q1 * q1) + (q2 * q2) - (coeff * q1 * q2);
}

void CwDecoder::updateKeyState(bool keyDown)
{
    if (keyDown == m_keyDown) {
        return;
    }

    m_keyDown = keyDown;
    if (m_keyDown) {
        handleToneStarted();
    } else {
        handleToneEnded();
    }
}

void CwDecoder::handleToneStarted()
{
    if (m_sampleRate <= 0) {
        return;
    }

    if (m_haveElement && !m_currentDurations.isEmpty()) {
        const double silenceSeconds = static_cast<double>(m_sampleCounter - m_keyUpStart) /
                                      static_cast<double>(m_sampleRate);
        rememberGap(silenceSeconds);

        const double dot = qBound(0.015, m_trackingDotSeconds, 0.250);
        const double letterThreshold = adaptiveLetterGapThreshold(true);
        const double wordThreshold = adaptiveWordGapThreshold(true);
        const QVector<float> feature = makeMindEventFeature();
        QVector<float> mindProb;
        double mindConfidence = 0.0;
        const int mindClass = mindSuggestedEventClass(feature, &mindConfidence, &mindProb);
        const bool heavyMind = mindHeavyHumanFistAssistActive();
        const double pIntra = mindEventProbability(mindProb, 2);
        const double pLetter = mindEventProbability(mindProb, 3);
        const double pWord = mindEventProbability(mindProb, 4);
        const bool mindWord = heavyMind
            ? (mindClass == 4 && mindConfidence >= 0.42 && silenceSeconds >= 2.65 * dot && pWord >= pLetter + 0.03 && pWord >= pIntra + 0.06)
            : (mindClass == 4 && mindConfidence >= 0.70 && silenceSeconds >= 3.8 * dot);
        const bool mindLetter = heavyMind
            ? (mindClass == 3 && mindConfidence >= 0.42 && silenceSeconds >= 1.35 * dot && pLetter >= pIntra + 0.02)
            : (mindClass == 3 && mindConfidence >= 0.66 && silenceSeconds >= 1.9 * dot);

        if ((silenceSeconds >= wordThreshold || mindWord) && !m_wordSpaceSent) {
            emitMindEventSample(4, QStringLiteral("word-gap"));
            if (mindWord && silenceSeconds < wordThreshold) ++m_mindAssistUsed;
            finishCurrentCharacter();
            if (!m_text.endsWith(' ') && !m_text.isEmpty()) {
                m_text.append(' ');
                emit characterReceived(" ");
                emit textUpdated(m_text);
            }
            m_wordSpaceSent = true;
        } else if (silenceSeconds >= letterThreshold || mindLetter) {
            emitMindEventSample(3, QStringLiteral("letter-gap"));
            if (mindLetter && silenceSeconds < letterThreshold) ++m_mindAssistUsed;
            finishCurrentCharacter();
        } else if (silenceSeconds > 0.35 * dot) {
            emitMindEventSample(2, QStringLiteral("intra-gap"));
        }
    }

    m_keyDownStart = m_sampleCounter;
}

void CwDecoder::handleToneEnded()
{
    if (m_sampleRate <= 0) {
        return;
    }

    const double markSeconds = static_cast<double>(m_sampleCounter - m_keyDownStart) /
                               static_cast<double>(m_sampleRate);

    const double spikeSeconds = qMax(0.006, 0.22 * m_trackingDotSeconds);
    if (markSeconds < spikeSeconds) {
        m_keyUpStart = m_sampleCounter;
        return;
    }

    maybeTrackSpeed(markSeconds);

    const double dot = qBound(0.015, m_trackingDotSeconds, 0.250);
    int symbolClass = (markSeconds <= 2.05 * dot) ? 0 : 1;
    const int nativeClass = symbolClass;
    const double ratio = markSeconds / dot;
    const bool borderline = (ratio >= 1.55 && ratio <= 2.75);
    const QVector<float> feature = makeMindEventFeature();
    QVector<float> mindProb;
    double mindConfidence = 0.0;
    const int mindClass = mindSuggestedEventClass(feature, &mindConfidence, &mindProb);
    const bool heavyMind = mindHeavyHumanFistAssistActive();
    const double pDit = mindEventProbability(mindProb, 0);
    const double pDah = mindEventProbability(mindProb, 1);
    const bool mindStrongElement = heavyMind
        ? (mindConfidence >= 0.42 && qAbs(pDit - pDah) >= 0.05)
        : (mindConfidence >= 0.60);
    if ((mindClass == 0 || mindClass == 1) &&
        (borderline || mindConfidence >= (heavyMind ? 0.54 : 0.78)) &&
        mindStrongElement) {
        symbolClass = mindClass;
        if (symbolClass != nativeClass) {
            ++m_mindAssistUsed;
            ++m_mindAssistDisagreed;
        }
    } else if (mindClass == 5 && mindConfidence >= (heavyMind ? 0.62 : 0.82) && markSeconds < (heavyMind ? 0.95 : 0.80) * dot) {
        // Active-only neural noise veto: discard very short key blips that look
        // like noise/QSB rather than a real dit.  Long or borderline elements
        // are never vetoed by MIND.
        ++m_mindAssistUsed;
        m_keyUpStart = m_sampleCounter;
        return;
    }

    m_currentDurations.append(markSeconds);
    if (symbolClass == 0) {
        emitMindEventSample(0, QStringLiteral("dit"));
        m_currentPattern.append('.');
    } else {
        emitMindEventSample(1, QStringLiteral("dah"));
        m_currentPattern.append('-');
    }

    if (m_currentDurations.size() > 7) {
        m_currentPattern.clear();
        m_currentDurations.clear();
        ++m_badPatterns;
    }

    m_haveElement = true;
    m_wordSpaceSent = false;
    m_keyUpStart = m_sampleCounter;
}

void CwDecoder::querySilenceDecoder()
{
    if (m_sampleRate <= 0 || m_keyDown || !m_haveElement) {
        return;
    }

    const double silenceSeconds = static_cast<double>(m_sampleCounter - m_keyUpStart) /
                                  static_cast<double>(m_sampleRate);

    const double dot = qBound(0.015, m_trackingDotSeconds, 0.250);
    const double letterThreshold = adaptiveLetterGapThreshold(false);
    const double wordThreshold = adaptiveWordGapThreshold(false);
    const QVector<float> feature = makeMindEventFeature();
    double mindConfidence = 0.0;
    const int mindClass = mindSuggestedEventClass(feature, &mindConfidence, nullptr);
    const bool heavyMind = mindHeavyHumanFistAssistActive();
    const bool mindLetter = heavyMind
        ? (mindClass == 3 && mindConfidence >= 0.42 && silenceSeconds >= 1.35 * dot)
        : (mindClass == 3 && mindConfidence >= 0.68 && silenceSeconds >= 2.0 * dot);
    const bool mindWord = heavyMind
        ? (mindClass == 4 && mindConfidence >= 0.44 && silenceSeconds >= 2.75 * dot)
        : (mindClass == 4 && mindConfidence >= 0.72 && silenceSeconds >= 4.0 * dot);

    if (!m_currentDurations.isEmpty() && (silenceSeconds >= letterThreshold || mindLetter)) {
        emitMindEventSample(3, QStringLiteral("letter-gap"));
        if (mindLetter && silenceSeconds < letterThreshold) ++m_mindAssistUsed;
        finishCurrentCharacter();
    }

    if (!m_wordSpaceSent && (silenceSeconds >= wordThreshold || mindWord)) {
        emitMindEventSample(4, QStringLiteral("word-gap"));
        if (mindWord && silenceSeconds < wordThreshold) ++m_mindAssistUsed;
        if (!m_text.endsWith(' ') && !m_text.isEmpty()) {
            m_text.append(' ');
            emit characterReceived(" ");
            emit textUpdated(m_text);
        }
        m_wordSpaceSent = true;
    }
}

double CwDecoder::adaptiveLetterGapThreshold(bool realtimeToneStart) const
{
    const double dot = qBound(0.015, m_trackingDotSeconds, 0.250);
    double factor = realtimeToneStart ? 2.95 : 3.30;

    // If the recent operator already shows longer intra-copy gaps, be patient
    // before closing a character.  Bound this so fast machine-sent CW still
    // copies normally.
    if (!m_recentGaps.isEmpty()) {
        QVector<double> scaled;
        scaled.reserve(m_recentGaps.size());
        for (double g : m_recentGaps) {
            if (g > 0.45 * dot && g < 6.0 * dot) {
                scaled.append(g / dot);
            }
        }
        const double med = medianOf(scaled);
        if (med > 0.0) {
            factor = qBound(factor, med * 1.55, realtimeToneStart ? 3.65 : 3.90);
        }
    }
    return factor * dot;
}

double CwDecoder::adaptiveWordGapThreshold(bool realtimeToneStart) const
{
    const double dot = qBound(0.015, m_trackingDotSeconds, 0.250);
    double factor = realtimeToneStart ? 5.10 : 5.55;

    if (!m_recentGaps.isEmpty()) {
        QVector<double> candidates;
        candidates.reserve(m_recentGaps.size());
        for (double g : m_recentGaps) {
            const double units = g / dot;
            if (units >= 3.4 && units <= 10.0) {
                candidates.append(units);
            }
        }
        const double med = medianOf(candidates);
        if (med > 0.0) {
            factor = qBound(4.7, med * 1.18, realtimeToneStart ? 7.0 : 7.4);
        }
    }

    const double letterFactor = adaptiveLetterGapThreshold(realtimeToneStart) / dot;
    return qMax((letterFactor + 1.65) * dot, factor * dot);
}

void CwDecoder::rememberGap(double silenceSeconds)
{
    if (!isFinitePositive(silenceSeconds) || m_trackingDotSeconds <= 0.0) {
        return;
    }

    const double dot = qBound(0.015, m_trackingDotSeconds, 0.250);
    if (silenceSeconds < 0.35 * dot || silenceSeconds > 12.0 * dot) {
        return;
    }

    m_recentGaps.append(silenceSeconds);
    while (m_recentGaps.size() > 28) {
        m_recentGaps.removeFirst();
    }
}

QVector<float> CwDecoder::makeMindEventFeature() const
{
    constexpr int kMindCwInput = 256;
    QVector<float> out(kMindCwInput, 0.0f);
    if (m_mindEnvelopeHistory.isEmpty()) {
        return out;
    }

    QVector<double> src = m_mindEnvelopeHistory;
    const double med = medianOf(src);
    QVector<double> deviations;
    deviations.reserve(src.size());
    for (double v : src) {
        deviations.append(qAbs(v - med));
    }
    const double mad = qMax(1.0e-8, medianOf(deviations));

    const int n = src.size();
    for (int i = 0; i < kMindCwInput; ++i) {
        const int a = (n * i) / kMindCwInput;
        const int z = qMax(a + 1, (n * (i + 1)) / kMindCwInput);
        double sum = 0.0;
        int count = 0;
        for (int j = a; j < z && j < n; ++j) {
            sum += src.at(j);
            ++count;
        }
        const double mean = count > 0 ? sum / static_cast<double>(count) : med;
        // Robust local normalization: 0.5 is recent floor, values above floor
        // show tone energy.  This keeps the CW MIND model resilient to QSB and
        // receiver gain changes.
        const double robust = 0.5 + 0.18 * ((mean - med) / mad);
        out[i] = static_cast<float>(qBound(0.0, robust, 1.0));
    }
    return out;
}

void CwDecoder::emitMindEventSample(int klass, const QString &label)
{
    if (klass < 0 || klass >= 6) {
        return;
    }
    const QVector<float> feature = makeMindEventFeature();
    if (feature.size() != 256) {
        return;
    }
    QVector<float> target(6, 0.0f);
    target[klass] = 1.0f;
    emit mindEventSampleReady(feature, target, label);
}

bool CwDecoder::mindEventAssistActive() const
{
    return m_mindEventAssistEnabled && static_cast<bool>(m_mindEventClassifier);
}

bool CwDecoder::mindHeavyHumanFistAssistActive() const
{
    // Active CW is intentionally a heavy human-fist assist mode: MIND is allowed
    // to steer low-level timing and the native event decoder becomes the text
    // source.  Training still does not alter decoding because
    // setMindEventAssistEnabled(false) is used outside Active.
    return mindEventAssistActive();
}

double CwDecoder::mindEventProbability(const QVector<float> &probabilities, int klass) const
{
    if (klass < 0 || klass >= probabilities.size()) {
        return 0.0;
    }
    return qBound(0.0, static_cast<double>(probabilities.at(klass)), 1.0);
}

int CwDecoder::mindSuggestedEventClass(const QVector<float> &feature, double *confidence, QVector<float> *probabilities) const
{
    if (confidence != nullptr) {
        *confidence = 0.0;
    }
    if (probabilities != nullptr) {
        probabilities->clear();
    }
    if (!mindEventAssistActive() || feature.size() != 256) {
        return -1;
    }

    QVector<float> pred;
    double confPercent = 0.0;
    if (!m_mindEventClassifier(feature, &pred, &confPercent) || pred.size() != 6) {
        return -1;
    }
    int best = 0;
    for (int i = 1; i < pred.size(); ++i) {
        if (pred.at(i) > pred.at(best)) {
            best = i;
        }
    }
    const double conf = qBound(0.0, static_cast<double>(pred.at(best)), 1.0);
    if (confidence != nullptr) {
        *confidence = conf;
    }
    if (probabilities != nullptr) {
        *probabilities = pred;
    }
    Q_UNUSED(confPercent);
    return best;
}

double CwDecoder::robustDotFromRecentElements() const
{
    if (m_recentElements.isEmpty()) {
        return 0.0;
    }

    QVector<double> sorted;
    sorted.reserve(m_recentElements.size());
    for (double d : m_recentElements) {
        if (d >= 0.010 && d <= 2.0) {
            sorted.append(d);
        }
    }
    if (sorted.isEmpty()) {
        return 0.0;
    }

    std::sort(sorted.begin(), sorted.end());
    const int count = qBound(1, qRound(sorted.size() * 0.40), sorted.size());
    QVector<double> dotCluster;
    dotCluster.reserve(count);
    for (int i = 0; i < count; ++i) {
        dotCluster.append(sorted.at(i));
    }

    double candidate = medianOf(dotCluster);
    if (candidate > 2.0 * m_trackingDotSeconds) {
        candidate /= 3.0;
    }
    return candidate;
}

void CwDecoder::finishCurrentCharacter()
{
    if (m_currentDurations.isEmpty() && m_currentPattern.isEmpty()) {
        return;
    }

    QString fuzzyPattern;
    double fuzzyScore = 0.0;
    QString decoded = decodeMorseFuzzy(m_currentDurations, &fuzzyPattern, &fuzzyScore);
    if (decoded.isEmpty()) {
        decoded = decodeMorse(m_currentPattern);
        fuzzyPattern = m_currentPattern;
    }

    m_lastFuzzyPattern = fuzzyPattern;
    m_lastFuzzyScore = fuzzyScore;

    if (decoded.isEmpty()) {
        ++m_badPatterns;
        m_currentPattern.clear();
        m_currentDurations.clear();
        return;
    }

    if (m_useGgmorsePrimary && !mindHeavyHumanFistAssistActive()) {
        // In Training/classic operation, do not double-print: ggmorse is the
        // selected-signal text source.  The native path still classifies events
        // for timing/status and MIND sample generation.
        ++m_decodedChars;
        m_currentPattern.clear();
        m_currentDurations.clear();
        return;
    }

    // In Active CW, MIND is meant to help heavily with human timing.  Therefore
    // the MIND-biased native event decoder must be allowed to produce the final
    // character stream; otherwise CW MIND would only collect statistics and have
    // no visible effect.
    if (mindHeavyHumanFistAssistActive()) {
        ++m_mindNativeChars;
    }
    appendDecodedText(decoded);

    m_currentPattern.clear();
    m_currentDurations.clear();
}

QString CwDecoder::decodeMorse(const QString &pattern) const
{
    static const QHash<QString, QString> table = morseDecodeTable();
    return table.value(pattern, QString());
}

QString CwDecoder::decodeMorseFuzzy(const QVector<double> &durations, QString *winningPattern,
                                    double *score) const
{
    if (durations.isEmpty() || durations.size() > 7) {
        return QString();
    }

    double maxDur = 0.0;
    for (double d : durations) {
        if (!isFinitePositive(d)) {
            return QString();
        }
        maxDur = qMax(maxDur, d);
    }
    if (maxDur <= 0.0) {
        return QString();
    }

    // fldigi's SOM path normalizes the duration vector: if the longest element
    // looks like a dash it becomes 1.0; otherwise all elements are scaled as dots
    // around 0.33.  This makes straight-key variation much less catastrophic
    // than an early hard dot/dash decision.
    const double scale = (maxDur > 2.0 * m_trackingDotSeconds) ? (1.0 / maxDur) : (0.33 / maxDur);

    double best = 1.0e99;
    QString bestText;
    QString bestPattern;

    for (const MorseSymbol *s = morseSymbols(); s->pattern; ++s) {
        const QString pattern = QString::fromLatin1(s->pattern);
        if (pattern.size() != durations.size()) {
            continue;
        }

        double diffSum = 0.0;
        for (int i = 0; i < durations.size(); ++i) {
            const double observed = durations.at(i) * scale;
            const double expected = (pattern.at(i) == QLatin1Char('-')) ? 1.0 : 0.33;
            const double diff = observed - expected;
            diffSum += diff * diff;
            if (diffSum >= best) {
                break;
            }
        }

        if (diffSum < best) {
            best = diffSum;
            bestText = QString::fromLatin1(s->text);
            bestPattern = pattern;
        }
    }

    if (winningPattern) {
        *winningPattern = bestPattern;
    }
    if (score) {
        *score = best;
    }

    // Refuse only truly absurd duration shapes.  A relaxed limit is intentional:
    // CW from straight keys and QSB would otherwise be thrown away too often.
    const double maxAllowed = 0.72 + (0.12 * durations.size());
    if (bestText.isEmpty() || best > maxAllowed) {
        return QString();
    }
    return bestText;
}

void CwDecoder::maybeTrackSpeed(double markSeconds)
{
    if (markSeconds <= 0.010 || markSeconds > 2.0) {
        return;
    }

    m_recentElements.append(markSeconds);
    while (m_recentElements.size() > 24) {
        m_recentElements.removeFirst();
    }

    m_lastElementSeconds = markSeconds;

    if (!m_autoWpm) {
        m_trackingDotSeconds = m_dotSeconds;
        return;
    }

    double candidateDot = robustDotFromRecentElements();
    if (candidateDot <= 0.0) {
        candidateDot = (markSeconds > 2.15 * m_trackingDotSeconds) ? (markSeconds / 3.0) : markSeconds;
    }

    // With ggmorse active this native tracker is a slow, robust bootstrap and
    // fallback.  It follows the cluster of short elements rather than one noisy
    // dot/dash ratio, which is much more stable for straight-key human fists.
    const double alpha = m_useGgmorsePrimary ? 0.030 : 0.060;

    if (candidateDot > 0.012 && candidateDot < 0.260) {
        const double maxStep = 0.12 * m_trackingDotSeconds;
        const double boundedDot = qBound(m_trackingDotSeconds - maxStep,
                                         candidateDot,
                                         m_trackingDotSeconds + maxStep);
        m_trackingDotSeconds = ((1.0 - alpha) * m_trackingDotSeconds) + (alpha * boundedDot);
        rebuildDetectorConstants();
        const double estimated = trackedWpm();
        const double emitStep = m_useGgmorsePrimary ? 1.0 : 0.5;
        if (qAbs(estimated - m_lastEmittedTrackedWpm) >= emitStep) {
            m_lastEmittedTrackedWpm = estimated;
            emit speedEstimateChanged(estimated);
        }
    }
}

void CwDecoder::maybeEmitStatus()
{
    ++m_statusCounter;
    if (m_statusCounter < 12) {
        return;
    }
    m_statusCounter = 0;

    const double currentTrackedWpm = trackedWpm();
    const double levelDb = 20.0 * qLn(m_gateValue + kTiny) / qLn(10.0);
    const double noiseDb = 20.0 * qLn(m_noiseLevel + kTiny) / qLn(10.0);
    const double openDb = 20.0 * qLn(m_openThreshold + kTiny) / qLn(10.0);

    emit statusChanged(QString("CW ggmorse+fldigi: %1 Hz, %2 WPM set / %3 WPM tracked (%4), %5, lvl %6 dB, noise %7 dB, thr %8 dB, margin %9 dB, AFC %10 Hz/%11 dB, pat %12→%13, score %14, ggcost %15, chars %16, bad %17, MIND %18/%19 native %20 gg-sup %21")
                           .arg(m_trackedToneHz, 0, 'f', 0)
                           .arg(m_wpm, 0, 'f', 0)
                           .arg(currentTrackedWpm, 0, 'f', 0)
                           .arg(m_autoWpm ? "auto" : "manual")
                           .arg(m_keyDown ? "key" : "space")
                           .arg(levelDb, 0, 'f', 1)
                           .arg(noiseDb, 0, 'f', 1)
                           .arg(openDb, 0, 'f', 1)
                           .arg(m_marginDb, 0, 'f', 1)
                           .arg(m_lastAfcBestHz, 0, 'f', 0)
                           .arg(m_lastAfcMarginDb, 0, 'f', 1)
                           .arg(m_currentPattern.isEmpty() ? QStringLiteral("-") : m_currentPattern)
                           .arg(m_lastFuzzyPattern.isEmpty() ? QStringLiteral("-") : m_lastFuzzyPattern)
                           .arg(m_lastFuzzyScore, 0, 'f', 2)
                           .arg(m_ggmorseLastCost, 0, 'f', 2)
                           .arg(m_decodedChars)
                           .arg(m_badPatterns)
                           .arg(m_mindEventAssistEnabled ? QStringLiteral("heavy-human-fist") : QStringLiteral("idle"))
                           .arg(m_mindAssistUsed)
                           .arg(m_mindNativeChars)
                           .arg(m_mindGgmorseSuppressedChars));
    emit markersChanged(frequencyMarkers(m_afcEnabled ? m_trackedToneHz : m_toneHz));
}
