#include "MfskDecoder.h"

#include <QColor>
#include <QtMath>
#include <algorithm>
#include <limits>

namespace {
constexpr double kMetricInf = 1.0e30;
constexpr int kViterbiStates = 64;      // K=7 -> 2^(K-1)
constexpr int kPoly1 = 0x6d;            // NASA K=7, gMFSK-compatible order
constexpr int kPoly2 = 0x4f;

// IZ8BLY/gMFSK MFSK Varicode decode table.  The incoming shift register is
// compared against these packed bit patterns after the inter-character 001
// separator is detected.
static const unsigned int kVaridecode[256] = {
    0x75C, 0x760, 0x768, 0x76C, 0x770, 0x774, 0x778, 0x77C,
    0x0A8, 0x780, 0x7A0, 0x7A8, 0x7AC, 0x0AC, 0x7B0, 0x7B4,
    0x7B8, 0x7BC, 0x7C0, 0x7D0, 0x7D4, 0x7D8, 0x7DC, 0x7E0,
    0x7E8, 0x7EC, 0x7F0, 0x7F4, 0x7F8, 0x7FC, 0x800, 0xA00,
    0x004, 0x1C0, 0x1FC, 0x2D8, 0x2A8, 0x2A0, 0x200, 0x1BC,
    0x1F4, 0x1F0, 0x2B4, 0x1E0, 0x0A0, 0x1D8, 0x1D4, 0x1E8,
    0x0E0, 0x0F0, 0x140, 0x154, 0x174, 0x160, 0x16C, 0x1A0,
    0x180, 0x1AC, 0x1EC, 0x1F8, 0x2C0, 0x1DC, 0x2BC, 0x1D0,
    0x280, 0x0BC, 0x100, 0x0D4, 0x0DC, 0x0B8, 0x0F8, 0x150,
    0x158, 0x0C0, 0x1B4, 0x17C, 0x0F4, 0x0E8, 0x0FC, 0x0D0,
    0x0EC, 0x1B0, 0x0D8, 0x0B4, 0x0B0, 0x15C, 0x1A8, 0x168,
    0x170, 0x178, 0x1B8, 0x2E8, 0x2D0, 0x2EC, 0x2D4, 0x2B0,
    0x2AC, 0x014, 0x060, 0x038, 0x034, 0x008, 0x050, 0x058,
    0x030, 0x018, 0x080, 0x070, 0x02C, 0x040, 0x01C, 0x010,
    0x054, 0x078, 0x020, 0x028, 0x00C, 0x03C, 0x06C, 0x068,
    0x074, 0x05C, 0x07C, 0x2DC, 0x2B8, 0x2E0, 0x2F0, 0xA80,
    0xAA0, 0xAA8, 0xAAC, 0xAB0, 0xAB4, 0xAB8, 0xABC, 0xAC0,
    0xAD0, 0xAD4, 0xAD8, 0xADC, 0xAE0, 0xAE8, 0xAEC, 0xAF0,
    0xAF4, 0xAF8, 0xAFC, 0xB00, 0xB40, 0xB50, 0xB54, 0xB58,
    0xB5C, 0xB60, 0xB68, 0xB6C, 0xB70, 0xB74, 0xB78, 0xB7C,
    0x2F4, 0x2F8, 0x2FC, 0x300, 0x340, 0x350, 0x354, 0x358,
    0x35C, 0x360, 0x368, 0x36C, 0x370, 0x374, 0x378, 0x37C,
    0x380, 0x3A0, 0x3A8, 0x3AC, 0x3B0, 0x3B4, 0x3B8, 0x3BC,
    0x3C0, 0x3D0, 0x3D4, 0x3D8, 0x3DC, 0x3E0, 0x3E8, 0x3EC,
    0x3F0, 0x3F4, 0x3F8, 0x3FC, 0x400, 0x500, 0x540, 0x550,
    0x554, 0x558, 0x55C, 0x560, 0x568, 0x56C, 0x570, 0x574,
    0x578, 0x57C, 0x580, 0x5A0, 0x5A8, 0x5AC, 0x5B0, 0x5B4,
    0x5B8, 0x5BC, 0x5C0, 0x5D0, 0x5D4, 0x5D8, 0x5DC, 0x5E0,
    0x5E8, 0x5EC, 0x5F0, 0x5F4, 0x5F8, 0x5FC, 0x600, 0x680,
    0x6A0, 0x6A8, 0x6AC, 0x6B0, 0x6B4, 0x6B8, 0x6BC, 0x6C0,
    0x6D0, 0x6D4, 0x6D8, 0x6DC, 0x6E0, 0x6E8, 0x6EC, 0x6F0,
    0x6F4, 0x6F8, 0x6FC, 0x700, 0x740, 0x750, 0x754, 0x758
};

static inline unsigned char clampSoft(double value)
{
    return static_cast<unsigned char>(qBound(0.0, value, 255.0));
}
} // namespace

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
    m_lastViterbiMetric = 0.0;
    m_symbolsSeen = 0;
    m_cwiCounters.fill(0, kMfsk16ToneCount);
    m_lastHardTone = -1;
    resetStandardMFSK16();
    configureForCurrentSettings();

    emit textUpdated(m_text);
    if (m_variant == Variant::Mfsk16) {
        emit statusChanged(QStringLiteral("MFSK16: waiting for standard Varicode/FEC signal"));
    } else {
        emit statusChanged(QStringLiteral("MFSK32 legacy experimental receiver: not fldigi-compatible yet"));
    }
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
    m_symbolBuffer.clear();
    m_symbolPhase = 0.0;
    resetStandardMFSK16();
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

QVector<unsigned char> MfskDecoder::symbolSoftBits(const QVector<double> &symbol, double *confidenceOut, double *offsetHzOut)
{
    const GoertzelToneBank::Result result = m_toneBank.analyse(symbol, m_afcEnabled);
    if (confidenceOut != nullptr) {
        *confidenceOut = result.confidence;
    }
    if (offsetHzOut != nullptr) {
        *offsetHzOut = result.offsetHz;
    }

    QVector<unsigned char> soft(kMfsk16SymbolBits, 128);
    if (result.energies.isEmpty()) {
        return soft;
    }

    /* fldigi MFSK softdecode(): average the tone bins, detect persistent
     * single-tone CWI, double-weight the current hard symbol, then form
     * Gray-decoded soft bits in the full 0..255 range.  The earlier MM path
     * used only half-scale soft bits and therefore starved the Viterbi decoder
     * on real MFSK16 files. */
    double sum = 1.0e-12;
    const int ntones = qMin(kMfsk16ToneCount, result.energies.size());
    if (m_cwiCounters.size() != kMfsk16ToneCount) {
        m_cwiCounters.fill(0, kMfsk16ToneCount);
    }

    constexpr int kCwiMaxCount = 6;
    for (int tone = 0; tone < ntones; ++tone) {
        if (m_cwiCounters.at(tone) <= kCwiMaxCount) {
            sum += qMax(0.0, result.energies.at(tone));
        }
    }
    const double avg = sum / static_cast<double>(qMax(1, ntones));

    m_lastHardTone = result.bestIndex;
    for (int tone = 1; tone < ntones; ++tone) { // never suppress tone 0
        int next = m_cwiCounters.at(tone) + ((tone == m_lastHardTone) ? 1 : -1);
        m_cwiCounters[tone] = qBound(0, next, kCwiMaxCount + 1);
    }

    for (int bitIndex = 0; bitIndex < kMfsk16SymbolBits; ++bitIndex) {
        const int mask = 1 << (kMfsk16SymbolBits - bitIndex - 1);
        double weighted = 0.0;
        for (int tone = 0; tone < ntones; ++tone) {
            const int weight = grayWeightForTone(tone);
            double energy = qMax(0.0, result.energies.at(tone));
            if (tone > 0 && m_cwiCounters.at(tone) > kCwiMaxCount) {
                energy = avg;
            } else if (tone == m_lastHardTone) {
                energy *= 2.0;
            }
            weighted += (weight & mask) ? energy : -energy;
        }
        soft[bitIndex] = clampSoft(128.0 + 256.0 * (weighted / qMax(sum, 1.0e-12)));
    }

    return soft;
}

void MfskDecoder::processSymbol(const QVector<double> &symbol)
{
    double confidence = 0.0;
    double offsetHz = 0.0;

    if (m_variant == Variant::Mfsk16) {
        QVector<unsigned char> soft = symbolSoftBits(symbol, &confidence, &offsetHz);
        m_lastConfidence = confidence;
        m_lastToneOffsetHz = offsetHz;

        if (confidence < 1.05) {
            // Do not drop MFSK16 symbols: the convolutional decoder and
            // diagonal deinterleaver require a continuous bit stream.  Low
            // confidence symbols are fed as neutral soft bits so timing is
            // preserved without inventing a strong tone.
            ++m_badFrames;
            soft.fill(128);
        } else {
            updateAfcFromSymbol(offsetHz, confidence);
        }

        deinterleaveSoftBits(soft);
        for (unsigned char value : soft) {
            feedSoftBit(value);
        }
        return;
    }

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
    handleLegacyTone(tone, confidence);
}

void MfskDecoder::resetStandardMFSK16()
{
    m_inlvTable.fill(0, 10 * kMfsk16SymbolBits * kMfsk16SymbolBits);
    m_viterbi.metrics.fill(kMetricInf, kViterbiStates);
    if (!m_viterbi.metrics.isEmpty()) {
        m_viterbi.metrics[0] = 0.0;
    }
    m_viterbi.paths.clear();
    m_viterbi.paths.resize(kViterbiStates);
    m_viterbi.steps = 0;
    m_viterbi.lastMetric = 0.0;
    m_softPairCount = 0;
    m_softPair[0] = 0;
    m_softPair[1] = 0;
    m_dataShiftRegister = 0;
    m_validCharacters = 0;
}

void MfskDecoder::deinterleaveSoftBits(QVector<unsigned char> &softBits)
{
    if (softBits.size() != kMfsk16SymbolBits) {
        return;
    }

    for (int stage = 0; stage < 10; ++stage) {
        for (int row = 0; row < kMfsk16SymbolBits; ++row) {
            const int base = (stage * kMfsk16SymbolBits * kMfsk16SymbolBits) + (row * kMfsk16SymbolBits);
            for (int col = 0; col < kMfsk16SymbolBits - 1; ++col) {
                m_inlvTable[base + col] = m_inlvTable.at(base + col + 1);
            }
            m_inlvTable[base + kMfsk16SymbolBits - 1] = softBits.at(row);
        }

        QVector<unsigned char> out(kMfsk16SymbolBits);
        for (int row = 0; row < kMfsk16SymbolBits; ++row) {
            const int base = (stage * kMfsk16SymbolBits * kMfsk16SymbolBits) + (row * kMfsk16SymbolBits);
            out[row] = m_inlvTable.at(base + row); // INTERLEAVE_REV, gMFSK-compatible
        }
        softBits = out;
    }
}

void MfskDecoder::feedSoftBit(unsigned char softBit)
{
    m_softPair[m_softPairCount++] = softBit;
    if (m_softPairCount < 2) {
        return;
    }

    double metric = 0.0;
    const int decoded = viterbiFeed(m_viterbi, m_softPair[0], m_softPair[1], &metric);
    m_softPairCount = 0;
    m_lastViterbiMetric = metric;

    if (decoded >= 0) {
        receiveDecodedBit(decoded);
    }
}

int MfskDecoder::viterbiFeed(ViterbiRuntime &decoder, unsigned char softA, unsigned char softB, double *metricOut)
{
    if (decoder.metrics.size() != kViterbiStates) {
        decoder.metrics.fill(kMetricInf, kViterbiStates);
        decoder.metrics[0] = 0.0;
        decoder.paths.clear();
        decoder.paths.resize(kViterbiStates);
    }

    QVector<double> nextMetrics(kViterbiStates, kMetricInf);
    QVector<QVector<unsigned char>> nextPaths(kViterbiStates);

    for (int state = 0; state < kViterbiStates; ++state) {
        const double baseMetric = decoder.metrics.at(state);
        if (baseMetric >= kMetricInf * 0.5) {
            continue;
        }

        for (int bit = 0; bit <= 1; ++bit) {
            const int reg = ((state << 1) | bit) & 0x7f;
            const int outA = parity7(reg & kPoly1);
            const int outB = parity7(reg & kPoly2);
            const double costA = outA ? (255.0 - softA) : softA;
            const double costB = outB ? (255.0 - softB) : softB;
            const int nextState = ((state << 1) | bit) & (kViterbiStates - 1);
            const double candidateMetric = baseMetric + costA + costB;
            if (candidateMetric < nextMetrics.at(nextState)) {
                nextMetrics[nextState] = candidateMetric;
                nextPaths[nextState] = decoder.paths.at(state);
                nextPaths[nextState].append(static_cast<unsigned char>(bit));
            }
        }
    }

    double bestMetric = kMetricInf;
    int bestState = 0;
    for (int state = 0; state < kViterbiStates; ++state) {
        if (nextMetrics.at(state) < bestMetric) {
            bestMetric = nextMetrics.at(state);
            bestState = state;
        }
    }

    // Renormalise to avoid metric growth during long QSOs.
    if (bestMetric < kMetricInf * 0.5) {
        for (double &metric : nextMetrics) {
            if (metric < kMetricInf * 0.5) {
                metric -= bestMetric;
            }
        }
    }

    decoder.metrics = nextMetrics;
    decoder.paths = nextPaths;
    decoder.steps++;
    decoder.lastMetric = bestMetric;
    if (metricOut != nullptr) {
        *metricOut = bestMetric;
    }

    if (decoder.paths.at(bestState).size() < kMfsk16Traceback) {
        return -1;
    }

    const int bit = decoder.paths.at(bestState).at(0);
    for (QVector<unsigned char> &path : decoder.paths) {
        if (!path.isEmpty()) {
            path.remove(0);
        }
    }
    return bit;
}

void MfskDecoder::receiveDecodedBit(int bit)
{
    m_dataShiftRegister = ((m_dataShiftRegister << 1) | (bit ? 1u : 0u)) & 0x00ffffffu;

    // MFSK Varicode uses the inter-character pattern 001.  The final 1 is the
    // first bit of the next character, so decode the register shifted by one
    // and keep that 1 as the new seed.
    if ((m_dataShiftRegister & 0x7u) == 0x1u) {
        const int code = varicodeDecode(m_dataShiftRegister >> 1);
        receiveVaricodeCharacter(code);
        m_dataShiftRegister = 1u;
    }
}

void MfskDecoder::receiveVaricodeCharacter(int code)
{
    if (code < 0) {
        ++m_badFrames;
        return;
    }

    QString decoded;
    if (code == 0) {
        return; // idle/fill
    }
    if (code == 8) {
        if (!m_text.isEmpty()) {
            m_text.chop(1);
            emit textUpdated(m_text);
        }
        return;
    }
    if (code == 10 || code == 13) {
        decoded = QStringLiteral("\n");
    } else if (code >= 32 && code <= 126) {
        decoded = QString(QChar(static_cast<ushort>(code)));
    } else {
        return;
    }

    m_text.append(decoded);
    if (m_text.size() > 20000) {
        m_text.remove(0, m_text.size() - 20000);
    }
    ++m_decodedChars;
    ++m_validCharacters;
    emit characterReceived(decoded);
    emit textUpdated(m_text);
}

void MfskDecoder::handleLegacyTone(int toneIndex, double confidence)
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

    finishLegacyCharacter(code);
    m_rxState = 0;
}

void MfskDecoder::finishLegacyCharacter(int code)
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

int MfskDecoder::grayWeightForTone(int toneIndex)
{
    return toneIndex ^ (toneIndex >> 1);
}

int MfskDecoder::parity7(int value)
{
    value ^= value >> 4;
    value ^= value >> 2;
    value ^= value >> 1;
    return value & 1;
}

int MfskDecoder::varicodeDecode(unsigned int symbol)
{
    for (int i = 0; i < 256; ++i) {
        if (kVaridecode[i] == symbol) {
            return i;
        }
    }
    return -1;
}

void MfskDecoder::maybeEmitStatus()
{
    ++m_statusCounter;
    if (m_statusCounter < 8) {
        return;
    }
    m_statusCounter = 0;

    if (m_variant == Variant::Mfsk16) {
        emit statusChanged(QStringLiteral("MFSK16 fldigi-core: center %1 Hz%2, FEC/Varicode chars %3, confidence %4, bad %5")
                               .arg(m_effectiveCenterHz, 0, 'f', 1)
                               .arg(m_afcEnabled ? (QStringLiteral(" (AFC ") + (m_afcOffsetHz >= 0.0 ? QStringLiteral("+") : QString()) + QString::number(m_afcOffsetHz, 'f', 1) + QStringLiteral(" Hz)")) : QString())
                               .arg(m_decodedChars)
                               .arg(m_lastConfidence, 0, 'f', 2)
                               .arg(m_badFrames));
    } else {
        emit statusChanged(QStringLiteral("%1 legacy experimental: center %2 Hz%3, confidence %4, chars %5, bad %6")
                               .arg(variantName(m_variant))
                               .arg(m_effectiveCenterHz, 0, 'f', 1)
                               .arg(m_afcEnabled ? (QStringLiteral(" (AFC ") + (m_afcOffsetHz >= 0.0 ? QStringLiteral("+") : QString()) + QString::number(m_afcOffsetHz, 'f', 1) + QStringLiteral(" Hz)")) : QString())
                               .arg(m_lastConfidence, 0, 'f', 2)
                               .arg(m_decodedChars)
                               .arg(m_badFrames));
    }
    emit markersChanged(frequencyMarkers(m_effectiveCenterHz, m_variant));
}
