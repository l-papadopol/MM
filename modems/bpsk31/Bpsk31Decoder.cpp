#include "Bpsk31Decoder.h"

#include <QColor>
#include <QHash>
#include <QtMath>

namespace {

constexpr double kTwoPi = 6.283185307179586476925286766559;

QHash<QString, QChar> varicodeDecodeTable()
{
    return {
        {"11101111", QChar('\t')}, {"11101", QChar('\n')}, {"11111", QChar('\r')},
        {"1", QChar(' ')}, {"111111111", QChar('!')}, {"101011111", QChar('"')},
        {"111110101", QChar('#')}, {"111011011", QChar('$')}, {"1011010101", QChar('%')},
        {"1010111011", QChar('&')}, {"101111111", QChar('\'')}, {"11111011", QChar('(')},
        {"11110111", QChar(')')}, {"101101111", QChar('*')}, {"111011111", QChar('+')},
        {"1110101", QChar(',')}, {"110101", QChar('-')}, {"1010111", QChar('.')},
        {"110101111", QChar('/')}, {"10110111", QChar('0')}, {"10111101", QChar('1')},
        {"11101101", QChar('2')}, {"11111111", QChar('3')}, {"101110111", QChar('4')},
        {"101011011", QChar('5')}, {"101101011", QChar('6')}, {"110101101", QChar('7')},
        {"110101011", QChar('8')}, {"110110111", QChar('9')}, {"11110101", QChar(':')},
        {"110111101", QChar(';')}, {"111101101", QChar('<')}, {"1010101", QChar('=')},
        {"111010111", QChar('>')}, {"1010101111", QChar('?')}, {"1010111101", QChar('@')},
        {"1111101", QChar('A')}, {"11101011", QChar('B')}, {"10101101", QChar('C')},
        {"10110101", QChar('D')}, {"1110111", QChar('E')}, {"11011011", QChar('F')},
        {"11111101", QChar('G')}, {"101010101", QChar('H')}, {"1111111", QChar('I')},
        {"111111101", QChar('J')}, {"101111101", QChar('K')}, {"11010111", QChar('L')},
        {"10111011", QChar('M')}, {"11011101", QChar('N')}, {"10101011", QChar('O')},
        {"11010101", QChar('P')}, {"111011101", QChar('Q')}, {"10101111", QChar('R')},
        {"1101111", QChar('S')}, {"1101101", QChar('T')}, {"101010111", QChar('U')},
        {"110110101", QChar('V')}, {"101011101", QChar('W')}, {"101110101", QChar('X')},
        {"101111011", QChar('Y')}, {"1010101101", QChar('Z')}, {"111110111", QChar('[')},
        {"111101111", QChar('\\')}, {"111111011", QChar(']')}, {"1010111111", QChar('^')},
        {"101101101", QChar('_')}, {"1011011111", QChar('`')}, {"1011", QChar('a')},
        {"1011111", QChar('b')}, {"101111", QChar('c')}, {"101101", QChar('d')},
        {"11", QChar('e')}, {"111101", QChar('f')}, {"1011011", QChar('g')},
        {"101011", QChar('h')}, {"1101", QChar('i')}, {"111101011", QChar('j')},
        {"10111111", QChar('k')}, {"11011", QChar('l')}, {"111011", QChar('m')},
        {"1111", QChar('n')}, {"111", QChar('o')}, {"111111", QChar('p')},
        {"110111111", QChar('q')}, {"10101", QChar('r')}, {"10111", QChar('s')},
        {"101", QChar('t')}, {"110111", QChar('u')}, {"1111011", QChar('v')},
        {"1101011", QChar('w')}, {"11011111", QChar('x')}, {"1011101", QChar('y')},
        {"111010101", QChar('z')}, {"1010110111", QChar('{')}, {"110111011", QChar('|')},
        {"1010110101", QChar('}')}, {"1011010111", QChar('~')}
    };
}

} // namespace

Bpsk31Decoder::Bpsk31Decoder(QObject *parent)
    : QObject(parent)
{
    m_resampler.configure(kInternalSampleRate);
    m_lockSquelch.configure(0.62, 0.24, 0.070, 0.020);
    configureForCurrentSettings();
    reset();
}

QString Bpsk31Decoder::modeName()
{
    return "PSK Text";
}

QString Bpsk31Decoder::variantNameForSymbolRate(double symbolRate, bool qpskMode)
{
    const QString prefix = qpskMode ? QStringLiteral("QPSK") : QStringLiteral("BPSK");
    if (symbolRate >= 750.0) {
        return prefix + QStringLiteral("1000");
    }
    if (symbolRate >= 375.0) {
        return prefix + QStringLiteral("500");
    }
    if (symbolRate >= 187.5) {
        return prefix + QStringLiteral("250");
    }
    if (symbolRate >= 100.0) {
        return prefix + QStringLiteral("125");
    }
    if (symbolRate >= 50.0) {
        return prefix + QStringLiteral("63");
    }
    return prefix + QStringLiteral("31");
}

QVector<FrequencyMarker> Bpsk31Decoder::frequencyMarkers(double toneHz, double symbolRate, bool qpskMode)
{
    QVector<FrequencyMarker> markers;
    FrequencyMarker marker;
    marker.frequencyHz = toneHz;
    marker.label = variantNameForSymbolRate(symbolRate, qpskMode);
    marker.color = qpskMode ? QColor(160, 180, 255) : QColor(120, 255, 160);
    markers.append(marker);
    return markers;
}

void Bpsk31Decoder::reset()
{
    m_resampler.reset();
    m_nco.reset();
    m_channelFilter.reset();
    m_symbolClock.reset(1.0);
    m_lockSquelch.reset();

    m_costasIntegrator = 0.0;
    m_costasCorrection = 0.0;
    m_costasErrorAvg = 0.0;
    m_i = 0.0;
    m_q = 0.0;
    m_accI = 0.0;
    m_accQ = 0.0;
    m_accCount = 0;
    m_havePreviousSymbol = false;
    m_prevI = 0.0;
    m_prevQ = 0.0;
    m_pendingZero = false;
    m_currentBits.clear();
    m_text.clear();
    m_signalPower = 1.0e-10;
    m_noisePower = 1.0e-10;
    m_snrLike = 1.0;
    m_phaseConfidence = 0.0;
    m_locked = false;
    m_decodedChars = 0;
    m_badVaricode = 0;
    m_statusCounter = 0;
    m_samplesProcessed = 0;
    m_trackedToneHz = m_toneHz;
    configureForCurrentSettings();

    emit textUpdated(m_text);
    emit statusChanged(QString("%1: unlocked, waiting for carrier/Costas/Varicode")
                           .arg(variantNameForSymbolRate(m_symbolRate, m_qpskMode)));
    emit markersChanged(frequencyMarkers(m_trackedToneHz, m_symbolRate, m_qpskMode));
}

void Bpsk31Decoder::setToneHz(double toneHz)
{
    m_toneHz = qBound(300.0, toneHz, 3500.0);
    m_trackedToneHz = m_toneHz;
    configureForCurrentSettings();
    emit markersChanged(frequencyMarkers(m_trackedToneHz, m_symbolRate, m_qpskMode));
}

void Bpsk31Decoder::setSymbolRate(double symbolRate)
{
    double normalized = 31.25;
    if (symbolRate >= 750.0) {
        normalized = 1000.0;
    } else if (symbolRate >= 375.0) {
        normalized = 500.0;
    } else if (symbolRate >= 187.5) {
        normalized = 250.0;
    } else if (symbolRate >= 100.0) {
        normalized = 125.0;
    } else if (symbolRate >= 50.0) {
        normalized = 62.5;
    }

    if (qAbs(m_symbolRate - normalized) < 0.001) {
        return;
    }

    m_symbolRate = normalized;
    m_filterCutoffHz = 0.0;
    reset();
}

void Bpsk31Decoder::setQpskMode(bool enabled)
{
    if (m_qpskMode == enabled) {
        return;
    }
    m_qpskMode = enabled;
    reset();
}

void Bpsk31Decoder::setAfcEnabled(bool enabled)
{
    m_afcEnabled = enabled;
    if (!m_afcEnabled) {
        m_trackedToneHz = m_toneHz;
        m_costasIntegrator = 0.0;
        m_costasCorrection = 0.0;
        configureForCurrentSettings();
    }
}

void Bpsk31Decoder::setAfcRangeHz(double rangeHz)
{
    m_afcRangeHz = qBound(5.0, rangeHz, 100.0);
    if (!m_afcEnabled) {
        m_trackedToneHz = m_toneHz;
    }
}

void Bpsk31Decoder::setInvertBits(bool invert)
{
    m_invertBits = invert;
}

void Bpsk31Decoder::setCoherentTrackingEnabled(bool enabled)
{
    if (m_coherentTrackingEnabled == enabled) {
        return;
    }
    m_coherentTrackingEnabled = enabled;
    configureForCurrentSettings();
}

double Bpsk31Decoder::toneHz() const
{
    return m_toneHz;
}

double Bpsk31Decoder::symbolRate() const
{
    return m_symbolRate;
}

bool Bpsk31Decoder::qpskMode() const
{
    return m_qpskMode;
}

double Bpsk31Decoder::trackedToneHz() const
{
    return m_trackedToneHz;
}

bool Bpsk31Decoder::afcEnabled() const
{
    return m_afcEnabled;
}

double Bpsk31Decoder::afcRangeHz() const
{
    return m_afcRangeHz;
}

bool Bpsk31Decoder::invertBits() const
{
    return m_invertBits;
}

bool Bpsk31Decoder::coherentTrackingEnabled() const
{
    return m_coherentTrackingEnabled;
}

QString Bpsk31Decoder::receivedText() const
{
    return m_text;
}

void Bpsk31Decoder::processAudioBlock(const AudioBlock &block)
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

    if (m_afcEnabled) {
        m_trackedToneHz = qBound(m_toneHz - m_afcRangeHz,
                                 m_toneHz + (m_costasIntegrator * kInternalSampleRate / kTwoPi),
                                 m_toneHz + m_afcRangeHz);
    } else {
        m_trackedToneHz = m_toneHz;
    }

    maybeEmitStatus();
}

void Bpsk31Decoder::configureForCurrentSettings()
{
    m_symbolSamples = kInternalSampleRate / m_symbolRate;
    m_symbolClock.configure(m_symbolSamples);
    m_nco.configure(kInternalSampleRate, m_toneHz);

    /* Baud-aware PSK channel.  For BPSK31 this yields roughly a 45-60 Hz
     * receiver bandwidth; for BPSK250/500/1000 the ceiling opens enough
     * for the wider high-rate Varicode signal without changing the UI flow. */
    const double rolloff = 3.0;
    m_channelFilter.configure(kInternalSampleRate, m_symbolRate, rolloff, 28.0, 1800.0, 4);
    m_filterCutoffHz = m_channelFilter.cutoffHz();

    const double rateScale = qSqrt(qMax(0.25, m_symbolRate / 31.25));
    const double loopNaturalHz = (m_coherentTrackingEnabled ? 1.15 : 0.72) * rateScale;
    m_costasKp = qBound(0.0020, 2.0 * M_PI * loopNaturalHz / kInternalSampleRate, 0.0320);
    m_costasKi = m_costasKp * m_costasKp * (m_coherentTrackingEnabled ? 0.035 : 0.018);
}

void Bpsk31Decoder::processInternalSample(double sample)
{
    sample = qBound(-1.0, sample, 1.0);

    double mixedI = 0.0;
    double mixedQ = 0.0;
    const double extraStep = m_afcEnabled ? m_costasCorrection : 0.0;
    m_nco.mixDown(sample, &mixedI, &mixedQ, extraStep);
    m_channelFilter.process(mixedI, mixedQ, &m_i, &m_q);

    updateCostasLoop(m_i, m_q);

    m_accI += m_i;
    m_accQ += m_q;
    ++m_accCount;

    if (m_symbolClock.tick()) {
        if (m_accCount > 0) {
            processSymbol(m_accI / static_cast<double>(m_accCount),
                          m_accQ / static_cast<double>(m_accCount));
        }
        m_accI = 0.0;
        m_accQ = 0.0;
        m_accCount = 0;
    }

    ++m_samplesProcessed;
}

void Bpsk31Decoder::updateCostasLoop(double i, double q)
{
    if (m_qpskMode) {
        Q_UNUSED(i)
        Q_UNUSED(q)
        m_costasErrorAvg = 0.0;
        m_costasIntegrator = 0.0;
        m_costasCorrection = 0.0;
        return;
    }

    const double decision = (i >= 0.0) ? 1.0 : -1.0;
    const double error = qBound(-1.0, decision * q, 1.0);
    m_costasErrorAvg = (0.995 * m_costasErrorAvg) + (0.005 * qAbs(error));

    if (m_afcEnabled) {
        const double integratorLimitHz = qBound(5.0, m_afcRangeHz, 100.0);
        const double correctionLimitHz = qBound(8.0, m_afcRangeHz * 1.35, 135.0);
        m_costasIntegrator = qBound(-kTwoPi * integratorLimitHz / kInternalSampleRate,
                                    m_costasIntegrator + (m_costasKi * error),
                                    kTwoPi * integratorLimitHz / kInternalSampleRate);
        m_costasCorrection = qBound(-kTwoPi * correctionLimitHz / kInternalSampleRate,
                                    (m_costasKp * error) + m_costasIntegrator,
                                    kTwoPi * correctionLimitHz / kInternalSampleRate);
    } else {
        m_costasIntegrator = 0.0;
        m_costasCorrection = 0.0;
    }
}

void Bpsk31Decoder::processSymbol(double symbolI, double symbolQ)
{
    const double mag = qSqrt((symbolI * symbolI) + (symbolQ * symbolQ));
    if (mag < 1.0e-8) {
        updateLockMetrics(mag, 0.0);
        return;
    }

    if (!m_havePreviousSymbol) {
        m_prevI = symbolI;
        m_prevQ = symbolQ;
        m_havePreviousSymbol = true;
        updateLockMetrics(mag, 0.0);
        return;
    }

    const double dot = (m_prevI * symbolI) + (m_prevQ * symbolQ);
    const double cross = (m_prevI * symbolQ) - (m_prevQ * symbolI);
    const double prevMag = qSqrt((m_prevI * m_prevI) + (m_prevQ * m_prevQ));
    const double norm = qMax(1.0e-12, prevMag * mag);
    const double phaseError = qAtan2(cross, dot);

    if (m_qpskMode) {
        const double quarter = (M_PI / 2.0);
        int quadrant = static_cast<int>(qRound(phaseError / quarter));
        if (quadrant > 2) {
            quadrant -= 4;
        }
        if (quadrant < -2) {
            quadrant += 4;
        }
        const double nearestPhase = static_cast<double>(quadrant) * quarter;
        const double nearestError = qAbs(qAtan2(qSin(phaseError - nearestPhase), qCos(phaseError - nearestPhase)));
        const double differentialConfidence = qBound(0.0, 1.0 - (nearestError / ((M_PI / 4.0) + 1.0e-12)), 1.0);

        updateLockMetrics(mag, differentialConfidence);

        if (m_locked) {
            bool first = true;
            bool second = true;
            switch (quadrant) {
            case 0:  first = true;  second = true;  break;  // 0 deg: 11
            case 1:  first = false; second = true;  break;  // +90 deg: 01
            case -1: first = true;  second = false; break;  // -90 deg: 10
            default: first = false; second = false; break;  // 180 deg: 00
            }
            if (m_invertBits) {
                first = !first;
                second = !second;
            }
            handleVaricodeBit(first);
            handleVaricodeBit(second);
        }

        m_prevI = symbolI;
        m_prevQ = symbolQ;
        return;
    }

    const double differentialConfidence = qAbs(dot) / norm;

    updateLockMetrics(mag, differentialConfidence);

    /* Symbol timing helper: very small decision-directed nudge near phase
     * transitions.  8000/31.25 is exact, so this only compensates tiny drift. */
    if (m_coherentTrackingEnabled && qAbs(phaseError) > 1.0 && differentialConfidence > 0.45) {
        // Gardner-like decision-directed timing nudge.  The PSK path already
        // uses a Costas carrier loop; this keeps symbol sampling centred when
        // sound-card/radio clocks drift slightly.
        m_symbolClock.nudge(qBound(-0.35, -phaseError * 0.035, 0.35));
    }

    if (m_locked) {
        bool bitOne = dot >= 0.0; // PSK31: no phase reversal is bit 1.
        if (m_invertBits) {
            bitOne = !bitOne;
        }
        handleVaricodeBit(bitOne);
    }

    m_prevI = symbolI;
    m_prevQ = symbolQ;
}

void Bpsk31Decoder::updateLockMetrics(double mag, double differentialConfidence)
{
    const double power = mag * mag;
    m_signalPower = (0.985 * m_signalPower) + (0.015 * power);

    if (!m_locked || power < m_noisePower * 2.0) {
        m_noisePower = (0.997 * m_noisePower) + (0.003 * qMax(power, 1.0e-10));
    } else {
        m_noisePower = (0.9998 * m_noisePower) + (0.0002 * qMin(power, m_noisePower * 3.0));
    }

    m_snrLike = m_signalPower / qMax(m_noisePower, 1.0e-12);
    m_phaseConfidence = (0.94 * m_phaseConfidence) + (0.06 * differentialConfidence);

    const bool carrierPresent = m_snrLike > 4.5;
    const bool phaseStable = m_phaseConfidence > 0.56;
    const bool costasQuiet = m_qpskMode || m_costasErrorAvg < 0.11 || m_samplesProcessed < 8000;
    m_locked = m_lockSquelch.update(carrierPresent && phaseStable && costasQuiet);

    if (!m_locked) {
        m_pendingZero = false;
        m_currentBits.clear();
    }
}

void Bpsk31Decoder::handleVaricodeBit(bool bitOne)
{
    if (bitOne) {
        if (m_pendingZero) {
            m_currentBits.append('0');
            m_pendingZero = false;
        }
        m_currentBits.append('1');
        if (m_currentBits.size() > 12) {
            ++m_badVaricode;
            m_currentBits.clear();
            m_pendingZero = false;
            if ((m_badVaricode % 4) == 0) {
                m_lockSquelch.forceClosed();
                m_locked = false;
            }
        }
        return;
    }

    if (m_pendingZero) {
        finishVaricodeCharacter();
        m_currentBits.clear();
        m_pendingZero = false;
        return;
    }

    m_pendingZero = true;
}

void Bpsk31Decoder::finishVaricodeCharacter()
{
    if (m_currentBits.isEmpty()) {
        return;
    }

    const QString decoded = decodeVaricode(m_currentBits);
    if (decoded.isEmpty()) {
        ++m_badVaricode;
        if ((m_badVaricode % 5) == 0) {
            m_lockSquelch.forceClosed();
            m_locked = false;
        }
        return;
    }

    m_text.append(decoded);
    if (m_text.size() > 20000) {
        m_text.remove(0, m_text.size() - 20000);
    }

    ++m_decodedChars;
    emit characterReceived(decoded);
    emit textUpdated(m_text);
}

QString Bpsk31Decoder::decodeVaricode(const QString &bits) const
{
    static const QHash<QString, QChar> table = varicodeDecodeTable();
    const QChar ch = table.value(bits, QChar());
    if (ch.isNull()) {
        return QString();
    }
    return QString(ch);
}

void Bpsk31Decoder::maybeEmitStatus()
{
    ++m_statusCounter;
    if (m_statusCounter < 10) {
        return;
    }
    m_statusCounter = 0;

    emit statusChanged(QString("%1: tone %2 Hz, AFC %3 Hz, sym %4 baud, BW %5 Hz, %6, lock %7%, SNR %8, chars %9, bad %10")
                           .arg(variantNameForSymbolRate(m_symbolRate, m_qpskMode))
                           .arg(m_toneHz, 0, 'f', 0)
                           .arg(m_trackedToneHz - m_toneHz, 0, 'f', 1)
                           .arg(m_symbolRate, 0, 'f', 2)
                           .arg(m_filterCutoffHz, 0, 'f', 1)
                           .arg(m_locked ? "locked" : "unlocked")
                           .arg(m_lockSquelch.score() * 100.0, 0, 'f', 0)
                           .arg(m_snrLike, 0, 'f', 1)
                           .arg(m_decodedChars)
                           .arg(m_badVaricode));
    emit markersChanged(frequencyMarkers(m_trackedToneHz, m_symbolRate, m_qpskMode));
}
