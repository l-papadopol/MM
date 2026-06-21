#include "RttyDecoder.h"

#include <QHash>
#include <QtMath>

namespace {

constexpr double kTwoPi = 2.0 * M_PI;
constexpr double kMinStartQuality = 0.075;
constexpr double kMinDataQuality = 0.022;
constexpr double kMinStopQuality = 0.055;
constexpr double kOpenGate = 0.55;
constexpr double kCloseGate = 0.22;

QString lettersForCode(int code)
{
    static const QHash<int, QString> table = {
        {1, "E"}, {2, "\n"}, {3, "A"}, {4, " "}, {5, "S"},
        {6, "I"}, {7, "U"}, {8, "\r"}, {9, "D"}, {10, "R"},
        {11, "J"}, {12, "N"}, {13, "F"}, {14, "C"}, {15, "K"},
        {16, "T"}, {17, "Z"}, {18, "L"}, {19, "W"}, {20, "H"},
        {21, "Y"}, {22, "P"}, {23, "Q"}, {24, "O"}, {25, "B"},
        {26, "G"}, {28, "M"}, {29, "X"}, {30, "V"}
    };

    return table.value(code, QString());
}

QString figuresForCode(int code)
{
    static const QHash<int, QString> table = {
        {1, "3"}, {2, "\n"}, {3, "-"}, {4, " "}, {5, "'"},
        {6, "8"}, {7, "7"}, {8, "\r"}, {9, "$"}, {10, "4"},
        {11, "\a"}, {12, ","}, {13, "!"}, {14, ":"}, {15, "("},
        {16, "5"}, {17, "\""}, {18, ")"}, {19, "2"}, {20, "#"},
        {21, "6"}, {22, "0"}, {23, "1"}, {24, "9"}, {25, "?"},
        {26, "&"}, {28, "."}, {29, "/"}, {30, ";"}
    };

    return table.value(code, QString());
}

} // namespace

RttyDecoder::RttyDecoder(QObject *parent)
    : QObject(parent)
{
    reset();
}

QString RttyDecoder::modeName()
{
    return "RTTY Text";
}

QVector<FrequencyMarker> RttyDecoder::frequencyMarkers(double markHz,
                                                       double spaceHz,
                                                       bool reverse)
{
    QVector<FrequencyMarker> markers;
    FrequencyMarker markMarker;
    markMarker.frequencyHz = markHz;
    markMarker.label = reverse ? "Space" : "Mark";
    markMarker.color = QColor(100, 220, 255);
    markers.append(markMarker);

    FrequencyMarker spaceMarker;
    spaceMarker.frequencyHz = spaceHz;
    spaceMarker.label = reverse ? "Mark" : "Space";
    spaceMarker.color = QColor(255, 220, 80);
    markers.append(spaceMarker);

    return markers;
}

void RttyDecoder::reset()
{
    m_markPhase = 0.0;
    m_spacePhase = 0.0;
    m_markI = 0.0;
    m_markQ = 0.0;
    m_spaceI = 0.0;
    m_spaceQ = 0.0;
    m_confidence = 0.0;

    m_inputPower = 0.0;
    m_noiseFloor = 0.0;
    m_energySnr = 1.0;
    m_toneRatio = 0.0;
    m_gateScore = 0.0;
    m_carrierOpen = false;

    resetFrame();
    m_lettersShift = true;
    m_samplesProcessed = 0;
    m_decodedChars = 0;
    m_goodFrames = 0;
    m_badFrames = 0;
    m_squelchedStarts = 0;
    m_statusCounter = 0;
    m_scopeDecimator = 0;
    m_scopeTrace.clear();
    m_autoInvert = false;
    m_mindFeatureWindow.clear();
    m_mindFeatureDecimator = 0;
    m_mindScored = 0;
    m_mindAssistedBits = 0;
    m_mindSamples = 0;
    m_text.clear();

    if (m_sampleRate > 0) {
        updateOscillators(m_sampleRate);
    }

    emit textUpdated(m_text);
    emit statusChanged("RTTY: squelch closed, waiting for mark/space tones");
    emit markersChanged(frequencyMarkers(m_markHz, m_spaceHz, m_reverse));
    emit tuningScopeChanged(0.0, 0.0, 0.0, false);
    emit tuningScopeTraceChanged(QVector<QPointF>(), 0.0, false);
}

void RttyDecoder::setBaudRate(double baud)
{
    m_baudRate = qBound(10.0, baud, 300.0);
    if (m_sampleRate > 0) {
        updateOscillators(m_sampleRate);
    }
    reset();
}

void RttyDecoder::setTones(double markHz, double spaceHz)
{
    retuneTones(markHz, spaceHz);
    resetFrame();
}

void RttyDecoder::retuneTones(double markHz, double spaceHz)
{
    m_markHz = qBound(300.0, markHz, 3500.0);
    m_spaceHz = qBound(300.0, spaceHz, 3500.0);
    if (qAbs(m_spaceHz - m_markHz) < 20.0) {
        m_spaceHz = m_markHz + 170.0;
    }
    if (m_sampleRate > 0) {
        updateOscillators(m_sampleRate);
    }
    emit markersChanged(frequencyMarkers(m_markHz, m_spaceHz, m_reverse));
}

void RttyDecoder::setReverse(bool reverse)
{
    if (m_reverse == reverse) {
        return;
    }
    m_reverse = reverse;
    m_autoInvert = false;
    reset();
}

double RttyDecoder::baudRate() const
{
    return m_baudRate;
}

double RttyDecoder::markHz() const
{
    return m_markHz;
}

double RttyDecoder::spaceHz() const
{
    return m_spaceHz;
}

bool RttyDecoder::reverse() const
{
    return m_reverse;
}

QString RttyDecoder::receivedText() const
{
    return m_text;
}

void RttyDecoder::setMindSoftSlicerEnabled(bool enabled)
{
    m_mindSoftSlicerEnabled = enabled;
}

void RttyDecoder::setMindSoftSlicerClassifier(MindRttyClassifier classifier)
{
    m_mindClassifier = std::move(classifier);
}

void RttyDecoder::processAudioBlock(const AudioBlock &block)
{
    if (block.samples.isEmpty() || block.sampleRate <= 0) {
        return;
    }

    if (block.sampleRate != m_sampleRate) {
        updateOscillators(block.sampleRate);
    }

    for (float rawSample : block.samples) {
        const double sample = qBound(-1.0, static_cast<double>(rawSample), 1.0);

        m_inputPower = (0.9994 * m_inputPower) + (0.0006 * sample * sample);

        const double markSin = qSin(m_markPhase);
        const double markCos = qCos(m_markPhase);
        const double spaceSin = qSin(m_spacePhase);
        const double spaceCos = qCos(m_spacePhase);

        m_markI = ((1.0 - m_energyAlpha) * m_markI) + (m_energyAlpha * sample * markCos);
        m_markQ = ((1.0 - m_energyAlpha) * m_markQ) + (m_energyAlpha * sample * markSin);
        m_spaceI = ((1.0 - m_energyAlpha) * m_spaceI) + (m_energyAlpha * sample * spaceCos);
        m_spaceQ = ((1.0 - m_energyAlpha) * m_spaceQ) + (m_energyAlpha * sample * spaceSin);

        m_markPhase += m_markInc;
        m_spacePhase += m_spaceInc;
        if (m_markPhase >= kTwoPi) {
            m_markPhase -= kTwoPi;
        }
        if (m_spacePhase >= kTwoPi) {
            m_spacePhase -= kTwoPi;
        }

        const double markEnergy = (m_markI * m_markI) + (m_markQ * m_markQ);
        const double spaceEnergy = (m_spaceI * m_spaceI) + (m_spaceQ * m_spaceQ);
        const double sumEnergy = markEnergy + spaceEnergy + 1.0e-14;
        const double diffNorm = (markEnergy - spaceEnergy) / sumEnergy;
        const double bitQuality = qAbs(diffNorm);
        updateMindFeatureWindow(diffNorm, bitQuality, sumEnergy);

        ++m_scopeDecimator;
        if (m_scopeDecimator >= 24) {
            m_scopeDecimator = 0;

            /*
             * Crossed-ellipse tuning display.
             *
             * A real RTTY oscilloscope does not plot random decoder decisions.
             * It drives the X/Y plates from the two tuned tone channels and the
             * phosphor brightness from received energy.  The previous MM trace
             * plotted low-rate I samples from the Mark/Space mixers directly;
             * that creates a square cloud when the bit stream changes state and
             * is not useful for tuning.
             *
             * Keep the decoder signal path unchanged, but synthesize the CRT
             * deflection from the *measured* Mark/Space envelopes and the live
             * oscillator phases:
             *   - Mark dominant  -> horizontal ellipse on the X axis.
             *   - Space dominant -> vertical ellipse on the Y axis.
             *   - transition / mistune -> diagonal smear between the two.
             * This matches the classic MMTTY/Penn crossed-ellipse instrument:
             * two clean right-angle ellipses mean the RX tones and shift are
             * centered; crooked/asymmetric figures mean mistune or wrong shift.
             */
            const double markLevel = qSqrt(markEnergy);
            const double spaceLevel = qSqrt(spaceEnergy);
            const double levelSum = markLevel + spaceLevel + 1.0e-12;
            const double markWeight = markLevel / levelSum;
            const double spaceWeight = spaceLevel / levelSum;
            const double strength = sumEnergy / qMax(m_noiseFloor, 1.0e-12);

            if ((m_carrierOpen || m_gateScore > 0.30) && strength > 2.0) {
                const double bitPurity = qBound(0.0, qAbs(markWeight - spaceWeight), 1.0);
                const double amplitude = qBound(0.18, 0.28 + 0.17 * qLn(1.0 + strength), 0.94);
                const double minor = qBound(0.10, 0.14 + 0.22 * (1.0 - bitPurity), 0.34);
                QPointF point;

                if (bitPurity < 0.18) {
                    // Transition or wrong shift: show the real crossed/diagonal
                    // energy rather than snapping to either ellipse.
                    const double mx = qSin(m_markPhase);
                    const double sy = qSin(m_spacePhase);
                    point = QPointF(qBound(-1.0, amplitude * mx, 1.0),
                                    qBound(-1.0, amplitude * sy, 1.0));
                } else if (markWeight >= spaceWeight) {
                    const double x = amplitude * qCos(m_markPhase);
                    const double y = minor * qSin(m_markPhase);
                    point = QPointF(qBound(-1.0, x, 1.0),
                                    qBound(-1.0, y, 1.0));
                } else {
                    const double x = minor * qSin(m_spacePhase);
                    const double y = amplitude * qCos(m_spacePhase);
                    point = QPointF(qBound(-1.0, x, 1.0),
                                    qBound(-1.0, y, 1.0));
                }

                m_scopeTrace.append(point);
                if (m_scopeTrace.size() > 520) {
                    m_scopeTrace.remove(0, m_scopeTrace.size() - 520);
                }
            }
        }

        m_confidence = (0.9985 * m_confidence) + (0.0015 * bitQuality);
        updateCarrierGate(sumEnergy, bitQuality);

        bool bitIsMark = diffNorm >= 0.0;
        if (m_reverse ^ m_autoInvert) {
            bitIsMark = !bitIsMark;
        }

        if (bitIsMark) {
            m_markRunSamples = qMin(m_markRunSamples + 1, static_cast<int>(m_symbolSamples * 8.0));
            m_spaceRunSamples = 0;
        } else {
            m_spaceRunSamples = qMin(m_spaceRunSamples + 1, static_cast<int>(m_symbolSamples * 8.0));
            m_markRunSamples = 0;
        }

        if (m_carrierOpen &&
            m_goodFrames == 0 &&
            m_badFrames == 0 &&
            m_state == RxState::WaitingStart &&
            m_spaceRunSamples > static_cast<int>(m_symbolSamples * 3.0) &&
            m_markRunSamples == 0) {
            m_autoInvert = !m_autoInvert;
            resetFrame();
            m_markRunSamples = 0;
            m_spaceRunSamples = 0;
        }

        if (!m_carrierOpen) {
            if (m_state != RxState::WaitingStart) {
                resetFrame();
            }
            m_previousBitMark = true;
            ++m_samplesProcessed;
            continue;
        }

        advanceSymbolClock(bitIsMark, bitQuality);
        ++m_samplesProcessed;
    }

    maybeEmitStatus();
}

void RttyDecoder::updateOscillators(int sampleRate)
{
    m_sampleRate = sampleRate;
    m_symbolSamples = static_cast<double>(sampleRate) / qMax(1.0, m_baudRate);
    m_markInc = kTwoPi * m_markHz / static_cast<double>(sampleRate);
    m_spaceInc = kTwoPi * m_spaceHz / static_cast<double>(sampleRate);

    const double lpSamples = qMax(12.0, m_symbolSamples * 0.38);
    m_energyAlpha = qBound(0.0015, 1.0 / lpSamples, 0.0300);
}

void RttyDecoder::updateCarrierGate(double sumEnergy, double bitQuality)
{
    if (m_noiseFloor <= 0.0) {
        m_noiseFloor = qMax(sumEnergy, 1.0e-12);
    }

    if (!m_carrierOpen) {
        const double attack = (sumEnergy < m_noiseFloor) ? 0.0200 : 0.0004;
        m_noiseFloor = ((1.0 - attack) * m_noiseFloor) + (attack * sumEnergy);
    } else if (sumEnergy < m_noiseFloor) {
        m_noiseFloor = (0.9950 * m_noiseFloor) + (0.0050 * sumEnergy);
    } else {
        m_noiseFloor = (0.99998 * m_noiseFloor) + (0.00002 * sumEnergy);
    }

    m_energySnr = sumEnergy / qMax(m_noiseFloor, 1.0e-12);
    m_toneRatio = sumEnergy / qMax(m_inputPower, 1.0e-12);

    const bool enoughAudio = m_inputPower > 1.0e-8;
    const bool strongTwoTonePresence = (m_toneRatio > 0.035 && bitQuality > 0.11);
    const bool veryStrongNarrowTone = (m_toneRatio > 0.085 && bitQuality > 0.055);
    const bool aboveNoiseFloor = (m_energySnr > 7.0 && bitQuality > 0.12);
    const bool carrierCandidate = enoughAudio && (strongTwoTonePresence || veryStrongNarrowTone || aboveNoiseFloor);

    const double target = carrierCandidate ? 1.0 : 0.0;
    m_gateScore = (0.99925 * m_gateScore) + (0.00075 * target);

    if (!m_carrierOpen && m_gateScore >= kOpenGate) {
        m_carrierOpen = true;
        resetFrame();
    } else if (m_carrierOpen && m_gateScore <= kCloseGate) {
        m_carrierOpen = false;
        resetFrame();
    }
}

void RttyDecoder::resetFrame()
{
    m_state = RxState::WaitingStart;
    m_previousBitMark = true;
    m_samplesToNextDecision = 0.0;
    m_idleMarkSamples = 0;
    m_startSpaceSamples = 0;
    m_dataBitIndex = 0;
    m_currentCode = 0;
}

void RttyDecoder::advanceSymbolClock(bool bitIsMark, double bitQuality)
{
    switch (m_state) {
    case RxState::WaitingStart:
        if (bitIsMark) {
            if (bitQuality >= kMinDataQuality) {
                m_idleMarkSamples = qMin(m_idleMarkSamples + 1,
                                         static_cast<int>(m_symbolSamples * 4.0));
            }
            m_startSpaceSamples = 0;
        } else {
            ++m_startSpaceSamples;
            const bool hadStableIdle = m_idleMarkSamples >= static_cast<int>(m_symbolSamples * 0.32);
            const bool plausibleStart = hadStableIdle &&
                                        bitQuality >= kMinStartQuality &&
                                        m_carrierOpen &&
                                        m_startSpaceSamples <= static_cast<int>(m_symbolSamples * 0.80);
            if (plausibleStart) {
                m_state = RxState::ValidateStart;
                m_samplesToNextDecision = qMax(1.0,
                                               (m_symbolSamples * 0.50) - static_cast<double>(m_startSpaceSamples));
            } else if (m_startSpaceSamples > static_cast<int>(m_symbolSamples * 1.20)) {
                ++m_squelchedStarts;
                m_idleMarkSamples = 0;
                m_startSpaceSamples = 0;
            }
        }
        break;

    case RxState::ValidateStart:
        m_samplesToNextDecision -= 1.0;
        if (m_samplesToNextDecision <= 0.0) {
            const bool sampledBitIsMark = maybeApplyMindSoftSlicer(bitIsMark, bitQuality);
            submitMindRttyBitSample(sampledBitIsMark, bitQuality);
            if (!sampledBitIsMark && bitQuality >= kMinStartQuality && m_carrierOpen) {
                m_state = RxState::DataBits;
                m_samplesToNextDecision = m_symbolSamples;
                m_dataBitIndex = 0;
                m_currentCode = 0;
            } else {
                ++m_squelchedStarts;
                resetFrame();
            }
        }
        break;

    case RxState::DataBits:
        m_samplesToNextDecision -= 1.0;
        if (m_samplesToNextDecision <= 0.0) {
            if (bitQuality < kMinDataQuality) {
                ++m_badFrames;
                resetFrame();
                break;
            }

            const bool sampledBitIsMark = maybeApplyMindSoftSlicer(bitIsMark, bitQuality);
            submitMindRttyBitSample(sampledBitIsMark, bitQuality);
            if (sampledBitIsMark) {
                m_currentCode |= (1 << m_dataBitIndex);
            }
            ++m_dataBitIndex;
            m_samplesToNextDecision += m_symbolSamples;

            if (m_dataBitIndex >= 5) {
                m_state = RxState::StopBits;
                m_samplesToNextDecision = m_symbolSamples;
            }
        }
        break;

    case RxState::StopBits:
        m_samplesToNextDecision -= 1.0;
        if (m_samplesToNextDecision <= 0.0) {
            const bool sampledBitIsMark = maybeApplyMindSoftSlicer(bitIsMark, bitQuality);
            submitMindRttyBitSample(sampledBitIsMark, bitQuality);
            if (sampledBitIsMark && bitQuality >= kMinStopQuality && m_carrierOpen) {
                ++m_goodFrames;
                handleCode(m_currentCode);
                resetFrame();
                m_idleMarkSamples = static_cast<int>(m_symbolSamples);
                m_startSpaceSamples = 0;
            } else {
                ++m_badFrames;
                if ((m_badFrames % 3) == 0) {
                    m_gateScore *= 0.55;
                }
                resetFrame();
            }
        }
        break;
    }

    m_previousBitMark = bitIsMark;
}

void RttyDecoder::handleCode(int code)
{
    if (code == 31) {
        m_lettersShift = true;
        return;
    }

    if (code == 27) {
        m_lettersShift = false;
        return;
    }

    const QString decoded = decodeCode(code);
    if (decoded.isEmpty()) {
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

QString RttyDecoder::decodeCode(int code) const
{
    if (m_lettersShift) {
        return lettersForCode(code);
    }

    return figuresForCode(code);
}


void RttyDecoder::updateMindFeatureWindow(double diffNorm, double bitQuality, double sumEnergy)
{
    const int stride = qMax(1, static_cast<int>(m_symbolSamples / 48.0));
    ++m_mindFeatureDecimator;
    if (m_mindFeatureDecimator < stride) {
        return;
    }
    m_mindFeatureDecimator = 0;

    const double energyRatio = sumEnergy / qMax(m_noiseFloor, 1.0e-12);
    const float polarity = static_cast<float>(qBound(0.0, 0.5 + 0.5 * diffNorm, 1.0));
    const float confidence = static_cast<float>(qBound(0.0, bitQuality, 1.0));
    const float strength = static_cast<float>(qBound(0.0, qLn(1.0 + energyRatio) / qLn(1.0 + 40.0), 1.0));

    // 96 values = 32 compact time samples × 3 features:
    // mark/space polarity, instantaneous bit confidence and carrier strength.
    m_mindFeatureWindow.append(polarity);
    m_mindFeatureWindow.append(confidence);
    m_mindFeatureWindow.append(strength);
    while (m_mindFeatureWindow.size() > 96) {
        m_mindFeatureWindow.remove(0, m_mindFeatureWindow.size() - 96);
    }
}

QVector<float> RttyDecoder::mindRttyFeature() const
{
    QVector<float> out(96, 0.0f);
    const int copy = qMin(out.size(), m_mindFeatureWindow.size());
    const int dstOffset = out.size() - copy;
    const int srcOffset = m_mindFeatureWindow.size() - copy;
    for (int i = 0; i < copy; ++i) {
        out[dstOffset + i] = m_mindFeatureWindow.at(srcOffset + i);
    }
    return out;
}

QVector<float> RttyDecoder::mindRttyTarget(bool bitIsMark, double bitQuality) const
{
    QVector<float> target(8, 0.0f);
    int klass = 7; // noise/ambiguous
    if (!m_carrierOpen) {
        klass = 5; // carrier drop
    } else if (bitQuality < 0.025) {
        klass = 4; // transition/uncertain zero crossing
    } else if (bitIsMark) {
        klass = (bitQuality >= 0.18) ? 0 : 2; // strong/weak mark
    } else {
        klass = (bitQuality >= 0.18) ? 1 : 3; // strong/weak space
    }
    target[klass] = 1.0f;
    return target;
}

bool RttyDecoder::maybeApplyMindSoftSlicer(bool classicBitIsMark, double bitQuality)
{
    if (!m_mindSoftSlicerEnabled || !m_mindClassifier || m_mindFeatureWindow.size() < 48) {
        return classicBitIsMark;
    }

    QVector<float> probs;
    double confidence = 0.0;
    if (!m_mindClassifier(mindRttyFeature(), &probs, &confidence) || probs.size() != 8) {
        return classicBitIsMark;
    }

    ++m_mindScored;
    int best = 0;
    for (int i = 1; i < probs.size(); ++i) {
        if (probs.at(i) > probs.at(best)) {
            best = i;
        }
    }

    const bool neuralHasBit = (best == 0 || best == 1 || best == 2 || best == 3);
    if (!neuralHasBit) {
        return classicBitIsMark;
    }
    const bool neuralBitIsMark = (best == 0 || best == 2);

    // Conservative policy: strong classical bit wins.  MIND only fixes weak or
    // borderline slicer decisions where the RTTY profile is very confident.
    const bool lowClassicalConfidence = bitQuality < 0.095;
    const bool veryHighNeuralConfidence = confidence >= 86.0;
    const bool highNeuralConfidence = confidence >= 74.0;
    if ((lowClassicalConfidence && highNeuralConfidence) || veryHighNeuralConfidence) {
        if (neuralBitIsMark != classicBitIsMark) {
            ++m_mindAssistedBits;
        }
        return neuralBitIsMark;
    }

    return classicBitIsMark;
}

void RttyDecoder::submitMindRttyBitSample(bool bitIsMark, double bitQuality)
{
    if (m_mindFeatureWindow.size() < 48) {
        return;
    }
    const QVector<float> input = mindRttyFeature();
    const QVector<float> target = mindRttyTarget(bitIsMark, bitQuality);
    const QString label = bitIsMark
        ? (bitQuality >= 0.18 ? QStringLiteral("strong-mark") : QStringLiteral("weak-mark"))
        : (bitQuality >= 0.18 ? QStringLiteral("strong-space") : QStringLiteral("weak-space"));
    ++m_mindSamples;
    emit mindRttyBitSampleReady(input, target, label);
}

void RttyDecoder::maybeEmitStatus()
{
    ++m_statusCounter;
    if (m_statusCounter < 10) {
        return;
    }

    m_statusCounter = 0;
    const double markLevel = qSqrt((m_markI * m_markI) + (m_markQ * m_markQ));
    const double spaceLevel = qSqrt((m_spaceI * m_spaceI) + (m_spaceQ * m_spaceQ));
    emit statusChanged(QString("RTTY: %1 baud, mark %2 Hz, space %3 Hz, squelch %4, tone %5%, conf %6%, good/bad %7/%8, chars %9%10")
                           .arg(m_baudRate, 0, 'f', 2)
                           .arg(m_markHz, 0, 'f', 0)
                           .arg(m_spaceHz, 0, 'f', 0)
                           .arg(m_carrierOpen ? "open" : "closed")
                           .arg(qBound(0.0, m_toneRatio * 100.0, 999.0), 0, 'f', 0)
                           .arg(m_confidence * 100.0, 0, 'f', 0)
                           .arg(m_goodFrames)
                           .arg(m_badFrames)
                           .arg(m_decodedChars)
                           .arg(m_autoInvert ? ", auto-rev" : "") +
                       QStringLiteral(", MIND %1/%2")
                           .arg(m_mindAssistedBits)
                           .arg(m_mindScored));
    const bool scopeLocked = m_carrierOpen && (m_confidence > 0.20);
    emit tuningScopeChanged(markLevel, spaceLevel, m_energySnr, scopeLocked);
    emit tuningScopeTraceChanged(m_scopeTrace, m_energySnr, scopeLocked);
}
