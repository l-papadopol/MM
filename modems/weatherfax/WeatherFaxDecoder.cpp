#include "WeatherFaxDecoder.h"

#include "../../dsp/FrequencyTracker.h"

#include <QPainter>
#include <QtMath>

#include <algorithm>

// -----------------------------------------------------------------------------
// Construction
// -----------------------------------------------------------------------------

WeatherFaxDecoder::WeatherFaxDecoder(QObject *parent)
    : QObject(parent)
{
    resetCarrierTrackingState();
    updateDemodulatorCoefficients();
    updateInputBandpassCoefficients();
    reset();
}

// -----------------------------------------------------------------------------
// Static metadata
// -----------------------------------------------------------------------------

QString WeatherFaxDecoder::modeName()
{
    return "MeteoFax / HF WEFAX RX";
}

QVector<FrequencyMarker> WeatherFaxDecoder::frequencyMarkers()
{
    return {
        {1500.0, "WEFAX black", QColor(220, 0, 0)},
        {1900.0, "WEFAX center", QColor(220, 0, 0)},
        {2300.0, "WEFAX white", QColor(220, 0, 0)}
    };
}

// -----------------------------------------------------------------------------
// Public getters
// -----------------------------------------------------------------------------

QVector<FrequencyMarker> WeatherFaxDecoder::currentFrequencyMarkers() const
{
    const double centerHz = (m_blackHz + m_whiteHz) * 0.5;

    return {
        {m_blackHz, "WEFAX black", QColor(220, 0, 0)},
        {centerHz, "WEFAX center", QColor(220, 0, 0)},
        {m_whiteHz, "WEFAX white", QColor(220, 0, 0)}
    };
}

QImage WeatherFaxDecoder::currentImage() const
{
    if (!m_image.isNull() && m_y > 0) {
        return completedImageSnapshot();
    }

    return m_image;
}

int WeatherFaxDecoder::lpm() const
{
    return m_lpm;
}

double WeatherFaxDecoder::blackToneHz() const
{
    return m_blackHz;
}

double WeatherFaxDecoder::whiteToneHz() const
{
    return m_whiteHz;
}

WeatherFaxDecoder::DecoderState WeatherFaxDecoder::state() const
{
    return m_state;
}

bool WeatherFaxDecoder::autoStartEnabled() const
{
    return m_autoStartEnabled;
}

bool WeatherFaxDecoder::autoToneTrackingEnabled() const
{
    return m_autoToneTrackingEnabled;
}

bool WeatherFaxDecoder::inputBandpassEnabled() const
{
    return m_inputBandpassEnabled;
}

bool WeatherFaxDecoder::autoSlantCorrectionEnabled() const
{
    return false;
}


double WeatherFaxDecoder::manualSlantPpm() const
{
    return 0.0;
}


double WeatherFaxDecoder::autoSlantPpm() const
{
    return 0.0;
}


int WeatherFaxDecoder::targetImageLines() const
{
    return m_targetImageLines;
}

int WeatherFaxDecoder::receivedImageLines() const
{
    return m_y;
}

bool WeatherFaxDecoder::endOfSignalCompletionEnabled() const
{
    return m_endOfSignalCompletionEnabled;
}

int WeatherFaxDecoder::endOfSignalTimeoutSec() const
{
    return m_endOfSignalTimeoutSec;
}

// -----------------------------------------------------------------------------
// Public slots
// -----------------------------------------------------------------------------

void WeatherFaxDecoder::processAudioBlock(const AudioBlock &block)
{
    if (block.samples.isEmpty() || block.sampleRate <= 0) {
        return;
    }

    if (block.sampleRate != m_sampleRate) {
        m_sampleRate = block.sampleRate;
        updateTiming();
        resetDemodulatorState();
        resetInputBandpassState();
        resetCarrierTrackingState();
    }

    m_buffer.reserve(m_buffer.size() + block.samples.size());
    m_frequencyBuffer.reserve(m_frequencyBuffer.size() + block.samples.size());

    AudioBlock decoderBlock;
    decoderBlock.sampleRate = block.sampleRate;
    decoderBlock.firstSampleIndex = block.firstSampleIndex;
    decoderBlock.samples.reserve(block.samples.size());

    QVector<float> rawControlSamples;
    rawControlSamples.reserve(block.samples.size());

    for (float sample : block.samples) {
        rawControlSamples.append(sample);

        const float filteredSample = filterInputSample(sample);
        decoderBlock.samples.append(filteredSample);
        m_buffer.append(filteredSample);
        appendDemodulatedSample(filteredSample);
    }

    updateSignalPresence(decoderBlock.samples);
    updateControlToneDetector(rawControlSamples);
    updateCarrierTracking(decoderBlock);
    processBufferedLines();
    trimBufferIfNeeded();
}

void WeatherFaxDecoder::reset()
{
    m_buffer.clear();
    m_frequencyBuffer.clear();
    m_image = QImage();
    m_imageHeight = 256;
    m_y = 0;
    m_lineSampleRemainder = 0.0;

    m_autoSlantCorrectionEnabled = false;
    m_manualSlantPpm = 0.0;
    m_autoSlantPpm = 0.0;
    m_lastSlantPhasePx = 0.0;
    m_lastSlantLine = -1;
    m_slantObservationCount = 0;

    m_controlToneBuffer.clear();
    m_controlToneSamplesSinceAnalysis = 0;
    m_controlToneActiveSamples = 0;
    m_controlToneCooldownLines = 0;
    m_lastControlTone = ControlTone::None;

    m_lastGray = 0;
    m_hasLastGray = false;

    resetSynchronizationState();
    resetLinePhaseLockState();
    resetDemodulatorState();
    resetInputBandpassState();
    resetCarrierTrackingState();
    resetCompletionState();
    updateTiming();

    emit imageUpdated(QImage());
    emit markersChanged(currentFrequencyMarkers());

    if (m_autoStartEnabled) {
        setState(DecoderState::Searching, "waiting for WEFAX APT start");
    } else {
        startNewImageFrame("manual/free-run");
        setState(DecoderState::Receiving, "auto-start disabled");
    }

    emitConfigurationStatus();
}


void WeatherFaxDecoder::finishCurrentImage(const QString &reason)
{
    if (m_y <= 0 || m_image.isNull() || m_imageCompleteEmitted) {
        return;
    }

    completeImage(reason.isEmpty() ? QString("manual/end of input") : reason);
}

void WeatherFaxDecoder::setLpm(int lpm)
{
    const int boundedLpm = qBound(30, lpm, 300);

    if (boundedLpm == m_lpm) {
        return;
    }

    m_lpm = boundedLpm;
    m_lineSampleRemainder = 0.0;

    resetSynchronizationState();
    updateTiming();
    emitConfigurationStatus();
}

void WeatherFaxDecoder::setToneRange(double blackHz, double whiteHz)
{
    applyToneRangeInternal(blackHz, whiteHz, true, true, false);
}

void WeatherFaxDecoder::setAutoStartEnabled(bool enabled)
{
    if (m_autoStartEnabled == enabled) {
        return;
    }

    m_autoStartEnabled = enabled;
    resetSynchronizationState();

    if (m_autoStartEnabled) {
        setState(DecoderState::Searching, "auto-start enabled");
    } else {
        setState(DecoderState::Receiving, "auto-start disabled");
    }
}

void WeatherFaxDecoder::setAutoToneTrackingEnabled(bool enabled)
{
    if (m_autoToneTrackingEnabled == enabled) {
        return;
    }

    m_autoToneTrackingEnabled = enabled;
    resetCarrierTrackingState();

    if (m_autoToneTrackingEnabled) {
        emit statusChanged("Decoder: live tone tracking enabled");
    } else {
        emit statusChanged("Decoder: live tone tracking disabled");
    }
}

void WeatherFaxDecoder::setInputBandpassEnabled(bool enabled)
{
    if (m_inputBandpassEnabled == enabled) {
        return;
    }

    m_inputBandpassEnabled = enabled;
    resetInputBandpassState();

    if (m_inputBandpassEnabled) {
        emit statusChanged("Decoder: tone-range band-pass enabled");
    } else {
        emit statusChanged("Decoder: tone-range band-pass disabled");
    }
}

void WeatherFaxDecoder::setAutoSlantCorrectionEnabled(bool enabled)
{
    Q_UNUSED(enabled)

    m_autoSlantCorrectionEnabled = false;
    m_autoSlantPpm = 0.0;
    m_lastSlantLine = -1;
    m_slantObservationCount = 0;
}


void WeatherFaxDecoder::setManualSlantPpm(double ppm)
{
    Q_UNUSED(ppm)

    m_manualSlantPpm = 0.0;
}


void WeatherFaxDecoder::setTargetImageLines(int lines)
{
    const int boundedLines = qBound(256, lines, 12000);

    if (m_targetImageLines == boundedLines) {
        return;
    }

    m_targetImageLines = boundedLines;
    m_imageHeight = qMin(qMax(256, m_imageHeight), m_targetImageLines);

    if (!m_image.isNull() && m_image.height() > m_targetImageLines) {
        m_image = completedImageSnapshot();
        m_imageHeight = qMax(1, m_image.height());
    }

    emit statusChanged(QString("Decoder: safety maximum %1 received lines").arg(m_targetImageLines));
}


void WeatherFaxDecoder::setEndOfSignalCompletionEnabled(bool enabled)
{
    if (m_endOfSignalCompletionEnabled == enabled) {
        return;
    }

    m_endOfSignalCompletionEnabled = enabled;
    m_signalLossSamples = 0;

    emit statusChanged(
        QString("Decoder: end-of-signal completion %1")
            .arg(m_endOfSignalCompletionEnabled ? "enabled" : "disabled")
        );
}

void WeatherFaxDecoder::setEndOfSignalTimeoutSec(int seconds)
{
    const int boundedSeconds = qBound(3, seconds, 180);

    if (m_endOfSignalTimeoutSec == boundedSeconds) {
        return;
    }

    m_endOfSignalTimeoutSec = boundedSeconds;
    m_signalLossSamples = 0;

    emit statusChanged(
        QString("Decoder: end-of-signal timeout %1 s").arg(m_endOfSignalTimeoutSec)
        );
}

// -----------------------------------------------------------------------------
// Timing
// -----------------------------------------------------------------------------

void WeatherFaxDecoder::updateTiming()
{
    const double secondsPerLine = 60.0 / static_cast<double>(m_lpm);

    m_samplesPerLine =
        static_cast<double>(m_sampleRate) * secondsPerLine;

    if (m_sampleRate <= 24000) {
        m_analysisWindow = 256;
    } else if (m_lpm >= 180) {
        m_analysisWindow = 384;
    } else {
        m_analysisWindow = 512;
    }

    const double pixelsPerSecond =
        static_cast<double>(m_pixelsPerLine) *
        static_cast<double>(m_lpm) / 60.0;

    const int targetEstimatesPerSecond = 850;

    m_estimateStepPixels = qMax(
        1,
        static_cast<int>(qRound(pixelsPerSecond /
                                static_cast<double>(targetEstimatesPerSecond)))
        );

    updateDemodulatorCoefficients();
    updateInputBandpassCoefficients();
}

// -----------------------------------------------------------------------------
// Synchronization state
// -----------------------------------------------------------------------------

void WeatherFaxDecoder::resetSynchronizationState()
{
    m_phaseOffsetPx = 0;
    m_candidatePhaseOffsetPx = 0;
    m_searchCandidateCount = 0;
    m_phasingConfirmCount = 0;
    m_searchLinesSeen = 0;
    m_statusLineCounter = 0;
}

// -----------------------------------------------------------------------------
// Streaming FM demodulator
// -----------------------------------------------------------------------------

void WeatherFaxDecoder::resetDemodulatorState()
{
    m_oscCos = 1.0;
    m_oscSin = 0.0;
    m_demodI = 0.0;
    m_demodQ = 0.0;
    m_prevDemodI = 0.0;
    m_prevDemodQ = 0.0;
    m_dcEstimate = 0.0;
    m_lastDemodFrequencyHz = (m_blackHz + m_whiteHz) * 0.5;
    m_hasDemodPrevious = false;
}

void WeatherFaxDecoder::updateDemodulatorCoefficients()
{
    m_demodCenterHz = (m_blackHz + m_whiteHz) * 0.5;

    if (m_sampleRate <= 0) {
        m_oscIncCos = 1.0;
        m_oscIncSin = 0.0;
        m_demodAlpha = 0.20;
        return;
    }

    const double toneSpanHz = qMax(100.0, m_whiteHz - m_blackHz);
    const double lowPassCutoffHz = qBound(250.0, toneSpanHz * 0.55, 600.0);
    const double phaseIncrement =
        (2.0 * M_PI * m_demodCenterHz) / static_cast<double>(m_sampleRate);

    m_oscIncCos = qCos(phaseIncrement);
    m_oscIncSin = qSin(phaseIncrement);

    m_demodAlpha = 1.0 - qExp((-2.0 * M_PI * lowPassCutoffHz) /
                              static_cast<double>(m_sampleRate));
    m_demodAlpha = qBound(0.02, m_demodAlpha, 0.95);
}


void WeatherFaxDecoder::updateInputBandpassCoefficients()
{
    if (m_sampleRate <= 0) {
        m_bpHpAlpha = 0.95;
        m_bpLpAlpha = 0.20;
        return;
    }

    const double toneLowHz = qMin(m_blackHz, m_whiteHz);
    const double toneHighHz = qMax(m_blackHz, m_whiteHz);
    const double toneSpanHz = qMax(100.0, toneHighHz - toneLowHz);
    const double guardHz = qBound(120.0, toneSpanHz * 0.18, 260.0);

    double lowCutHz = qMax(40.0, toneLowHz - guardHz);
    double highCutHz = qMin(static_cast<double>(m_sampleRate) * 0.45,
                            toneHighHz + guardHz);

    if (highCutHz <= lowCutHz + 100.0) {
        lowCutHz = qMax(40.0, toneLowHz - 150.0);
        highCutHz = qMin(static_cast<double>(m_sampleRate) * 0.45,
                         toneHighHz + 150.0);
    }

    const double dt = 1.0 / static_cast<double>(m_sampleRate);
    const double hpRc = 1.0 / (2.0 * M_PI * lowCutHz);
    const double lpRc = 1.0 / (2.0 * M_PI * highCutHz);

    m_bpHpAlpha = hpRc / (hpRc + dt);
    m_bpLpAlpha = dt / (lpRc + dt);

    m_bpHpAlpha = qBound(0.001, m_bpHpAlpha, 0.999);
    m_bpLpAlpha = qBound(0.001, m_bpLpAlpha, 0.999);
}

void WeatherFaxDecoder::resetInputBandpassState()
{
    m_bpHpPrevInput1 = 0.0;
    m_bpHpPrevInput2 = 0.0;
    m_bpHpStage1 = 0.0;
    m_bpHpStage2 = 0.0;
    m_bpLpStage1 = 0.0;
    m_bpLpStage2 = 0.0;
}

float WeatherFaxDecoder::filterInputSample(float sample)
{
    if (!m_inputBandpassEnabled || m_sampleRate <= 0) {
        return sample;
    }

    const double input = static_cast<double>(sample);

    /*
     * Two cascaded one-pole high-pass sections followed by two cascaded
     * one-pole low-pass sections.  This is intentionally lightweight for live
     * RX: it rejects audio outside the current black/white useful zone without
     * adding FFT latency or a large FIR delay.
     */
    const double hpStage1 =
        m_bpHpAlpha * (m_bpHpStage1 + input - m_bpHpPrevInput1);
    m_bpHpPrevInput1 = input;
    m_bpHpStage1 = hpStage1;

    const double hpStage2 =
        m_bpHpAlpha * (m_bpHpStage2 + hpStage1 - m_bpHpPrevInput2);
    m_bpHpPrevInput2 = hpStage1;
    m_bpHpStage2 = hpStage2;

    m_bpLpStage1 += m_bpLpAlpha * (hpStage2 - m_bpLpStage1);
    m_bpLpStage2 += m_bpLpAlpha * (m_bpLpStage1 - m_bpLpStage2);

    return static_cast<float>(qBound(-4.0, m_bpLpStage2, 4.0));
}

void WeatherFaxDecoder::appendDemodulatedSample(float sample)
{
    if (m_sampleRate <= 0) {
        m_frequencyBuffer.append(0.0);
        return;
    }

    m_dcEstimate += 0.0005 * (static_cast<double>(sample) - m_dcEstimate);
    const double centeredSample = static_cast<double>(sample) - m_dcEstimate;

    const double mixedI = centeredSample * m_oscCos;
    const double mixedQ = centeredSample * (-m_oscSin);

    m_demodI += m_demodAlpha * (mixedI - m_demodI);
    m_demodQ += m_demodAlpha * (mixedQ - m_demodQ);

    double frequencyHz = m_lastDemodFrequencyHz;

    if (m_hasDemodPrevious) {
        const double cross = (m_prevDemodI * m_demodQ) -
                             (m_prevDemodQ * m_demodI);
        const double dot = (m_prevDemodI * m_demodI) +
                           (m_prevDemodQ * m_demodQ);
        const double amplitude = qSqrt((m_demodI * m_demodI) +
                                       (m_demodQ * m_demodQ));

        if (amplitude > 1.0e-5 && qAbs(dot) + qAbs(cross) > 1.0e-12) {
            const double deltaPhase = qAtan2(cross, dot);
            const double rawFrequencyHz =
                m_demodCenterHz +
                (deltaPhase * static_cast<double>(m_sampleRate) / (2.0 * M_PI));

            const double boundedFrequencyHz =
                qBound(300.0, rawFrequencyHz, 3000.0);

            const double alpha = 0.35;
            frequencyHz = (alpha * boundedFrequencyHz) +
                          ((1.0 - alpha) * m_lastDemodFrequencyHz);
        }
    }

    m_prevDemodI = m_demodI;
    m_prevDemodQ = m_demodQ;
    m_hasDemodPrevious = true;
    m_lastDemodFrequencyHz = frequencyHz;

    const double nextCos = (m_oscCos * m_oscIncCos) - (m_oscSin * m_oscIncSin);
    const double nextSin = (m_oscSin * m_oscIncCos) + (m_oscCos * m_oscIncSin);

    m_oscCos = nextCos;
    m_oscSin = nextSin;

    const double norm = qSqrt((m_oscCos * m_oscCos) + (m_oscSin * m_oscSin));

    if (norm > 0.0 && (m_frequencyBuffer.size() % 4096) == 0) {
        m_oscCos /= norm;
        m_oscSin /= norm;
    }

    m_frequencyBuffer.append(frequencyHz);
}

// -----------------------------------------------------------------------------
// Live carrier / tone tracking
// -----------------------------------------------------------------------------

void WeatherFaxDecoder::resetCarrierTrackingState()
{
    m_carrierBuffer.clear();
    m_carrierSamplesSinceAnalysis = 0;
    m_toneObservationCount = 0;
    m_autoToneStatusCounter = 0;
    m_carrierTrackingGuardSamples = 0;

    const int binCount =
        static_cast<int>(qFloor((m_toneHistogramMaxHz - m_toneHistogramMinHz) /
                                static_cast<double>(m_toneHistogramBinHz))) + 1;

    m_toneHistogram.fill(0.0, qMax(1, binCount));
}

void WeatherFaxDecoder::startCarrierTrackingGuard()
{
    resetCarrierTrackingState();

    /*
     * The official phasing/start section may contain a single dominant tone.
     * That tone is useful for line sync, but it is not reliable evidence for
     * the black/white AFC pair.  Keep the AFC idle briefly after entering
     * Receiving so the tracker starts from real image content instead of the
     * sync tail.
     */
    m_carrierTrackingGuardSamples = qMax(
        m_sampleRate,
        static_cast<int>(qCeil(m_samplesPerLine * 2.0))
        );
}

void WeatherFaxDecoder::updateCarrierTracking(const AudioBlock &block)
{
    if (!m_autoToneTrackingEnabled || block.samples.isEmpty() || m_sampleRate <= 0) {
        return;
    }

    if (m_autoStartEnabled && m_state != DecoderState::Receiving) {
        if (!m_carrierBuffer.isEmpty() || m_toneObservationCount > 0) {
            resetCarrierTrackingState();
        }

        return;
    }

    if (m_carrierTrackingGuardSamples > 0) {
        m_carrierTrackingGuardSamples -= block.samples.size();

        if (m_carrierTrackingGuardSamples <= 0) {
            resetCarrierTrackingState();
        }

        return;
    }

    m_carrierBuffer.reserve(m_carrierBuffer.size() + block.samples.size());

    for (float sample : block.samples) {
        m_carrierBuffer.append(sample);
    }

    const int maxCarrierBufferSamples = qMax(m_sampleRate * 3, 8192);

    if (m_carrierBuffer.size() > maxCarrierBufferSamples) {
        m_carrierBuffer.remove(0, m_carrierBuffer.size() - maxCarrierBufferSamples);
    }

    m_carrierSamplesSinceAnalysis += block.samples.size();

    const int analysisHopSamples = qMax(1024, m_sampleRate / 3);

    while (m_carrierSamplesSinceAnalysis >= analysisHopSamples) {
        m_carrierSamplesSinceAnalysis -= analysisHopSamples;
        analyzeCarrierTrackingWindow();
    }
}

void WeatherFaxDecoder::analyzeCarrierTrackingWindow()
{
    const int analysisSize = qMin(
        m_carrierBuffer.size(),
        qBound(2048, m_sampleRate / 3, 8192)
        );

    if (analysisSize < 1024) {
        return;
    }

    QVector<double> window;
    window.resize(analysisSize);

    double mean = 0.0;
    const int start = m_carrierBuffer.size() - analysisSize;

    for (int i = 0; i < analysisSize; ++i) {
        mean += static_cast<double>(m_carrierBuffer[start + i]);
    }

    mean /= static_cast<double>(analysisSize);

    for (int i = 0; i < analysisSize; ++i) {
        const double hann = 0.5 - 0.5 * qCos(
                                (2.0 * M_PI * static_cast<double>(i)) /
                                static_cast<double>(analysisSize - 1));
        window[i] = (static_cast<double>(m_carrierBuffer[start + i]) - mean) * hann;
    }

    struct Peak
    {
        double frequencyHz = 0.0;
        double power = 0.0;
    };

    QVector<Peak> peaks;
    peaks.reserve(2);

    double totalPower = 0.0;
    int powerCount = 0;

    Peak best;
    Peak second;

    for (double frequencyHz = m_toneHistogramMinHz;
         frequencyHz <= m_toneHistogramMaxHz;
         frequencyHz += static_cast<double>(m_toneHistogramBinHz)) {
        const double power = goertzelPowerAt(window, frequencyHz);
        totalPower += power;
        ++powerCount;

        if (power > best.power) {
            second = best;
            best.frequencyHz = frequencyHz;
            best.power = power;
        } else if (power > second.power &&
                   qAbs(frequencyHz - best.frequencyHz) >= 80.0) {
            second.frequencyHz = frequencyHz;
            second.power = power;
        }
    }

    const double averagePower = totalPower / static_cast<double>(qMax(1, powerCount));
    const double bestConfidence = best.power / qMax(averagePower, 1.0e-18);
    const double secondConfidence = second.power / qMax(averagePower, 1.0e-18);

    if (best.frequencyHz > 0.0 && bestConfidence >= 6.0) {
        updateToneHistogram(best.frequencyHz, bestConfidence);
    }

    if (second.frequencyHz > 0.0 &&
        secondConfidence >= 4.0 &&
        qAbs(second.frequencyHz - best.frequencyHz) >= 150.0) {
        updateToneHistogram(second.frequencyHz, secondConfidence * 0.65);
    }

    double trackedBlackHz = 0.0;
    double trackedWhiteHz = 0.0;
    double trackedConfidence = 0.0;

    if (!findTrackedToneRange(&trackedBlackHz, &trackedWhiteHz, &trackedConfidence)) {
        return;
    }

    const double smoothAlpha = qBound(0.045,
                                      0.06 + (trackedConfidence * 0.12),
                                      0.16);
    const double trackedCenterHz = (trackedBlackHz + trackedWhiteHz) * 0.5;
    const double currentCenterHz = (m_blackHz + m_whiteHz) * 0.5;
    const double newCenterHz = (smoothAlpha * trackedCenterHz) +
                               ((1.0 - smoothAlpha) * currentCenterHz);
    const double baseSpacingHz = qBound(300.0, m_autoToneSpacingBaseHz, 1400.0);
    const double halfSpacingHz = qBound(baseSpacingHz - 35.0, m_autoToneSpacingHz, baseSpacingHz + 35.0) * 0.5;
    const double newBlackHz = newCenterHz - halfSpacingHz;
    const double newWhiteHz = newCenterHz + halfSpacingHz;

    if (qAbs(newBlackHz - m_blackHz) < 2.5 &&
        qAbs(newWhiteHz - m_whiteHz) < 2.5) {
        return;
    }

    if (applyToneRangeInternal(newBlackHz, newWhiteHz, false, false, true)) {
        ++m_autoToneStatusCounter;

        if ((m_autoToneStatusCounter % 3) == 0) {
            emit statusChanged(
                QString("Decoder: auto tones %1-%2 Hz, confidence %3")
                    .arg(m_blackHz, 0, 'f', 0)
                    .arg(m_whiteHz, 0, 'f', 0)
                    .arg(trackedConfidence, 0, 'f', 1)
                );
        }
    }
}

double WeatherFaxDecoder::goertzelPowerAt(const QVector<double> &samples,
                                          double frequencyHz) const
{
    if (samples.isEmpty() || m_sampleRate <= 0 || frequencyHz <= 0.0) {
        return 0.0;
    }

    const double omega = (2.0 * M_PI * frequencyHz) /
                         static_cast<double>(m_sampleRate);
    const double coeff = 2.0 * qCos(omega);

    double s0 = 0.0;
    double s1 = 0.0;
    double s2 = 0.0;

    for (double sample : samples) {
        s0 = sample + (coeff * s1) - s2;
        s2 = s1;
        s1 = s0;
    }

    return (s1 * s1) + (s2 * s2) - (coeff * s1 * s2);
}

void WeatherFaxDecoder::updateToneHistogram(double frequencyHz, double confidence)
{
    if (m_toneHistogram.isEmpty()) {
        resetCarrierTrackingState();
    }

    for (double &value : m_toneHistogram) {
        value *= 0.985;
    }

    const int bin = qBound(
        0,
        static_cast<int>(qRound((frequencyHz - m_toneHistogramMinHz) /
                                static_cast<double>(m_toneHistogramBinHz))),
        m_toneHistogram.size() - 1
        );

    m_toneHistogram[bin] += qBound(0.0, confidence, 30.0);
    ++m_toneObservationCount;
}

bool WeatherFaxDecoder::findTrackedToneRange(double *blackHz,
                                             double *whiteHz,
                                             double *confidence) const
{
    if (blackHz != nullptr) {
        *blackHz = 0.0;
    }

    if (whiteHz != nullptr) {
        *whiteHz = 0.0;
    }

    if (confidence != nullptr) {
        *confidence = 0.0;
    }

    if (m_toneHistogram.size() < 2 || m_toneObservationCount < 3) {
        return false;
    }

    const double currentCenterHz = (m_blackHz + m_whiteHz) * 0.5;
    const double baseSpacingHz = qBound(300.0, m_autoToneSpacingBaseHz, 1400.0);
    const double targetSpacingHz = qBound(baseSpacingHz - 35.0, m_autoToneSpacingHz, baseSpacingHz + 35.0);
    const double halfSpacingHz = targetSpacingHz * 0.5;
    const double spacingToleranceHz = qMax(70.0, targetSpacingHz * 0.16);

    int strongestIndex = -1;
    double strongestValue = 0.0;

    for (int i = 0; i < m_toneHistogram.size(); ++i) {
        if (m_toneHistogram[i] > strongestValue) {
            strongestValue = m_toneHistogram[i];
            strongestIndex = i;
        }
    }

    if (strongestIndex < 0 || strongestValue < 8.0) {
        return false;
    }

    double bestPairScore = 0.0;
    double bestPairCenterHz = 0.0;

    for (int i = 0; i < m_toneHistogram.size(); ++i) {
        const double valueA = m_toneHistogram[i];

        if (valueA < 4.0) {
            continue;
        }

        const double frequencyA = m_toneHistogramMinHz +
                                  static_cast<double>(i * m_toneHistogramBinHz);

        for (int j = i + 1; j < m_toneHistogram.size(); ++j) {
            const double valueB = m_toneHistogram[j];

            if (valueB < 4.0) {
                continue;
            }

            const double frequencyB = m_toneHistogramMinHz +
                                      static_cast<double>(j * m_toneHistogramBinHz);
            const double separationHz = frequencyB - frequencyA;
            const double spacingErrorHz = qAbs(separationHz - targetSpacingHz);

            if (spacingErrorHz > spacingToleranceHz) {
                continue;
            }

            const double centerHz = (frequencyA + frequencyB) * 0.5;
            const double centerErrorHz = qAbs(centerHz - currentCenterHz);

            /*
             * Score a pair as a WEFAX AFC candidate.
             *
             * The important point is that the spacing is constrained by the
             * configured black/white span.  The tracker is allowed to move the
             * pair center, but it is not allowed to invent a new black/white
             * distance from random image content or noise.
             */
            const double pairStrength = qSqrt(valueA * valueB);
            const double spacingScore =
                1.0 - (spacingErrorHz / qMax(1.0, spacingToleranceHz));
            const double centerScore = qMax(0.20, 1.0 - (centerErrorHz / 450.0));
            const double score = pairStrength * spacingScore * centerScore;

            if (score > bestPairScore) {
                bestPairScore = score;
                bestPairCenterHz = centerHz;
            }
        }
    }

    if (bestPairScore >= 6.0) {
        const double lowFrequency = bestPairCenterHz - halfSpacingHz;
        const double highFrequency = bestPairCenterHz + halfSpacingHz;

        if (lowFrequency >= m_toneHistogramMinHz &&
            highFrequency <= m_toneHistogramMaxHz) {
            if (blackHz != nullptr) {
                *blackHz = lowFrequency;
            }

            if (whiteHz != nullptr) {
                *whiteHz = highFrequency;
            }

            if (confidence != nullptr) {
                *confidence = qBound(0.30, bestPairScore / 30.0, 1.0);
            }

            return true;
        }
    }

    const bool allowSingleSideFallback =
        (m_state == DecoderState::Receiving && m_y >= 8);

    if (!allowSingleSideFallback) {
        return false;
    }

    const double strongestFrequencyHz =
        m_toneHistogramMinHz +
        static_cast<double>(strongestIndex * m_toneHistogramBinHz);

    double inferredCenterHz = 0.0;
    const double blackDistanceHz = qAbs(strongestFrequencyHz - m_blackHz);
    const double whiteDistanceHz = qAbs(strongestFrequencyHz - m_whiteHz);
    const double sideToleranceHz = qMax(90.0, targetSpacingHz * 0.18);

    /*
     * Single-side fallback.
     *
     * If only one side of the modulation is visible, use it only when it is
     * very close to the expected black or white side.  A middle-gray dominant
     * tone must not pull the black/white spacing apart.
     */
    if (strongestValue >= 14.0 && blackDistanceHz <= sideToleranceHz) {
        inferredCenterHz = strongestFrequencyHz + halfSpacingHz;
    } else if (strongestValue >= 14.0 && whiteDistanceHz <= sideToleranceHz) {
        inferredCenterHz = strongestFrequencyHz - halfSpacingHz;
    }

    if (inferredCenterHz <= 0.0) {
        return false;
    }

    const double lowFrequency = inferredCenterHz - halfSpacingHz;
    const double highFrequency = inferredCenterHz + halfSpacingHz;

    if (lowFrequency < m_toneHistogramMinHz ||
        highFrequency > m_toneHistogramMaxHz) {
        return false;
    }

    if (blackHz != nullptr) {
        *blackHz = lowFrequency;
    }

    if (whiteHz != nullptr) {
        *whiteHz = highFrequency;
    }

    if (confidence != nullptr) {
        *confidence = 0.18;
    }

    return true;
}

bool WeatherFaxDecoder::applyToneRangeInternal(double blackHz,
                                               double whiteHz,
                                               bool resetSync,
                                               bool emitStatus,
                                               bool emitUiUpdate)
{
    const double boundedBlack = qBound(300.0, blackHz, 3000.0);
    const double boundedWhite = qBound(300.0, whiteHz, 3000.0);

    if (boundedWhite <= boundedBlack + 50.0) {
        if (emitStatus) {
            emit statusChanged("Decoder: invalid WEFAX tone range");
        }
        return false;
    }

    const bool changed =
        !qFuzzyCompare(m_blackHz + 1.0, boundedBlack + 1.0) ||
        !qFuzzyCompare(m_whiteHz + 1.0, boundedWhite + 1.0);

    if (!changed) {
        return false;
    }

    m_blackHz = boundedBlack;
    m_whiteHz = boundedWhite;

    if (resetSync) {
        m_autoToneSpacingBaseHz = qBound(300.0, boundedWhite - boundedBlack, 1400.0);
        m_autoToneSpacingHz = m_autoToneSpacingBaseHz;
    }

    updateDemodulatorCoefficients();
    updateInputBandpassCoefficients();

    if (resetSync) {
        resetInputBandpassState();
        resetSynchronizationState();
    }

    emit markersChanged(currentFrequencyMarkers());

    if (emitUiUpdate) {
        emit toneRangeUpdated(m_blackHz, m_whiteHz);
    }

    if (emitStatus) {
        emitConfigurationStatus();
    }

    return true;
}

// -----------------------------------------------------------------------------
// Buffered line processing
// -----------------------------------------------------------------------------

void WeatherFaxDecoder::processBufferedLines()
{
    const int requiredSamples = requiredSamplesForOneLine();

    int decodedLinesThisCall = 0;
    const int maxLinesPerCall = 12;

    while (m_buffer.size() >= requiredSamples &&
           m_frequencyBuffer.size() >= requiredSamples &&
           decodedLinesThisCall < maxLinesPerCall) {
        const DecodedLine line = decodeLineFromBuffer();

        handleDecodedLine(line);
        consumeOneLineFromBuffer();

        ++decodedLinesThisCall;
    }
}

int WeatherFaxDecoder::requiredSamplesForOneLine() const
{
    return static_cast<int>(qCeil(effectiveSamplesPerLine())) + qMax(8, m_analysisWindow / 8) + 2;
}

double WeatherFaxDecoder::effectiveSamplesPerLine() const
{
    return m_samplesPerLine;
}


double WeatherFaxDecoder::totalSlantPixelsPerLine() const
{
    return 0.0;
}


int WeatherFaxDecoder::wrappedPhaseDelta(int a, int b) const
{
    const int n = qMax(1, m_pixelsPerLine);
    int delta = (a - b) % n;

    if (delta > n / 2) {
        delta -= n;
    } else if (delta < -n / 2) {
        delta += n;
    }

    return delta;
}

// -----------------------------------------------------------------------------
// Line decoder
// -----------------------------------------------------------------------------

WeatherFaxDecoder::DecodedLine WeatherFaxDecoder::decodeLineFromBuffer() const
{
    DecodedLine line;
    line.pixels.resize(m_pixelsPerLine);

    const double samplesPerPixel =
        effectiveSamplesPerLine() / static_cast<double>(m_pixelsPerLine);

    int currentGray = m_hasLastGray ? m_lastGray : 0;
    bool hasLocalGray = m_hasLastGray;

    for (int x = 0; x < m_pixelsPerLine; ++x) {
        const int sampleStart =
            static_cast<int>(qFloor(static_cast<double>(x) * samplesPerPixel));
        int sampleEnd =
            static_cast<int>(qCeil(static_cast<double>(x + 1) * samplesPerPixel));

        sampleEnd = qMax(sampleStart + 1, sampleEnd);

        double frequency = averageDemodulatedFrequency(sampleStart, sampleEnd);

        if (frequency <= 0.0) {
            frequency = estimateFrequencyAt(sampleStart);
        }

        int rawGray = currentGray;

        if (frequency > 0.0) {
            const double normalized =
                (frequency - m_blackHz) / (m_whiteHz - m_blackHz);

            rawGray = qBound(
                0,
                static_cast<int>(qRound(normalized * 255.0)),
                255
                );
        } else if (!hasLocalGray) {
            rawGray = 0;
        }

        if (!hasLocalGray) {
            currentGray = rawGray;
            hasLocalGray = true;
        } else {
            const double alpha = 0.55;
            currentGray = qBound(
                0,
                static_cast<int>(
                    qRound((alpha * static_cast<double>(rawGray)) +
                           ((1.0 - alpha) * static_cast<double>(currentGray)))
                    ),
                255
                );
        }

        line.pixels[x] = currentGray;
    }

    analyzePhasingForLine(line);

    return line;
}

double WeatherFaxDecoder::estimateFrequencyAt(int sampleOffset) const
{
    if (m_buffer.isEmpty()) {
        return 0.0;
    }

    sampleOffset = qBound(0, sampleOffset, qMax(0, m_buffer.size() - 1));

    if (sampleOffset + m_analysisWindow >= m_buffer.size()) {
        sampleOffset = qMax(0, m_buffer.size() - m_analysisWindow - 1);
    }

    if (sampleOffset < 0 || sampleOffset + m_analysisWindow > m_buffer.size()) {
        return 0.0;
    }

    QVector<float> window;
    window.reserve(m_analysisWindow);

    for (int i = 0; i < m_analysisWindow; ++i) {
        window.append(m_buffer[sampleOffset + i]);
    }

    const double lowSearchHz = qMax(300.0, m_blackHz - 350.0);
    const double highSearchHz = qMin(3000.0, m_whiteHz + 350.0);

    const FrequencyEstimate estimate =
        FrequencyTracker::estimateAutocorrelation(
            window,
            m_sampleRate,
            lowSearchHz,
            highSearchHz,
            0.0025
            );

    if (!estimate.valid) {
        return 0.0;
    }

    return estimate.frequencyHz;
}

double WeatherFaxDecoder::averageDemodulatedFrequency(int startSample,
                                                      int endSample) const
{
    if (m_frequencyBuffer.isEmpty()) {
        return 0.0;
    }

    startSample = qBound(0, startSample, m_frequencyBuffer.size() - 1);
    endSample = qBound(startSample + 1, endSample, m_frequencyBuffer.size());

    double sum = 0.0;
    int count = 0;

    for (int i = startSample; i < endSample; ++i) {
        const double frequencyHz = m_frequencyBuffer[i];

        if (frequencyHz >= 250.0 && frequencyHz <= 3500.0) {
            sum += frequencyHz;
            ++count;
        }
    }

    if (count <= 0) {
        return 0.0;
    }

    return sum / static_cast<double>(count);
}

// -----------------------------------------------------------------------------
// Frequency / grayscale conversion
// -----------------------------------------------------------------------------

int WeatherFaxDecoder::grayFromFrequency(double frequencyHz)
{
    if (frequencyHz <= 0.0) {
        if (!m_hasLastGray) {
            return 0;
        }

        m_lastGray = static_cast<int>(
            qRound(static_cast<double>(m_lastGray) * 0.98)
            );

        return m_lastGray;
    }

    const double normalized =
        (frequencyHz - m_blackHz) / (m_whiteHz - m_blackHz);

    const int rawGray =
        qBound(0, static_cast<int>(qRound(normalized * 255.0)), 255);

    if (!m_hasLastGray) {
        m_lastGray = rawGray;
        m_hasLastGray = true;
        return m_lastGray;
    }

    const double alpha = 0.60;

    m_lastGray = qBound(
        0,
        static_cast<int>(
            qRound((alpha * static_cast<double>(rawGray)) +
                   ((1.0 - alpha) * static_cast<double>(m_lastGray)))
            ),
        255
        );

    return m_lastGray;
}

// -----------------------------------------------------------------------------
// Phasing detection
// -----------------------------------------------------------------------------

void WeatherFaxDecoder::analyzePhasingForLine(DecodedLine &line) const
{
    double confidence = 0.0;
    const int offset = detectPhaseOffset(line.pixels, &confidence);

    line.phaseOffsetPx = offset;
    line.phasingConfidence = confidence;
    line.hasPhasingSignal = confidence >= m_minSearchConfidence;
}

int WeatherFaxDecoder::detectPhaseOffset(const QVector<int> &pixels,
                                         double *confidence) const
{
    if (confidence != nullptr) {
        *confidence = 0.0;
    }

    const int n = pixels.size();

    if (n < 64) {
        return 0;
    }

    int minGray = 255;
    int maxGray = 0;

    for (int value : pixels) {
        minGray = qMin(minGray, value);
        maxGray = qMax(maxGray, value);
    }

    const double globalContrast =
        static_cast<double>(maxGray - minGray) / 255.0;

    if (globalContrast < 0.14) {
        return 0;
    }

    /*
     * Always use a small bell-shaped smoothing filter for phasing detection.
     * The image line itself remains untouched; only the sync/phase detector uses
     * this Gaussian-like copy so random grain does not win over a real phasing
     * transition.
     */
    const int smoothRadius = qBound(4, n / 100, 18);
    const double smoothSigma = qMax(1.0, static_cast<double>(smoothRadius) * 0.48);

    QVector<double> smooth;
    smooth.resize(n);

    for (int x = 0; x < n; ++x) {
        double weightedSum = 0.0;
        double weightSum = 0.0;

        for (int k = -smoothRadius; k <= smoothRadius; ++k) {
            const int index = qBound(0, x + k, n - 1);
            const double distance = static_cast<double>(k);
            const double weight = qExp(-(distance * distance) /
                                       (2.0 * smoothSigma * smoothSigma));

            weightedSum += weight * static_cast<double>(pixels[index]);
            weightSum += weight;
        }

        smooth[x] = weightedSum / qMax(weightSum, 1.0e-12);
    }

    const int window = qMax(10, n / 70);
    const int guard = window + smoothRadius + 2;
    const double lobeSigma = qMax(1.0, static_cast<double>(window) * 0.45);

    double bestScore = 0.0;
    int bestX = 0;

    for (int x = guard; x < n - guard; ++x) {
        double leftSum = 0.0;
        double rightSum = 0.0;
        double weightSum = 0.0;

        for (int i = 0; i < window; ++i) {
            const double distance = static_cast<double>(i);
            const double weight = qExp(-(distance * distance) /
                                       (2.0 * lobeSigma * lobeSigma));

            leftSum += weight * smooth[x - 1 - i];
            rightSum += weight * smooth[x + i];
            weightSum += weight;
        }

        const double leftAvg = leftSum / qMax(weightSum, 1.0e-12);
        const double rightAvg = rightSum / qMax(weightSum, 1.0e-12);
        const double edgeScore = qAbs(rightAvg - leftAvg);

        if (edgeScore > bestScore) {
            bestScore = edgeScore;
            bestX = x;
        }
    }

    const double edgeStrength = bestScore / 255.0;

    double score = edgeStrength * qMin(1.0, globalContrast * 1.8);

    /*
     * Prefer phasing-like transitions not too close to the extreme edges.
     * This reduces false triggers caused by random noise bursts.
     */
    if (bestX < n / 20 || bestX > (n * 19) / 20) {
        score *= 0.65;
    }

    score = qBound(0.0, score, 1.0);

    if (confidence != nullptr) {
        *confidence = score;
    }

    return bestX;
}

// -----------------------------------------------------------------------------
// Decoder state machine
// -----------------------------------------------------------------------------

void WeatherFaxDecoder::handleDecodedLine(const DecodedLine &line)
{
    switch (m_state) {
    case DecoderState::Searching:
        handleSearchingLine(line);
        break;

    case DecoderState::Phasing:
        handlePhasingLine(line);
        break;

    case DecoderState::Receiving:
        handleReceivingLine(line);
        break;
    }
}

void WeatherFaxDecoder::handleSearchingLine(const DecodedLine &line)
{
    ++m_searchLinesSeen;

    if (!m_autoStartEnabled) {
        if (m_image.isNull()) {
            startNewImageFrame("manual/free-run from first sample");
            setState(DecoderState::Receiving, "manual/free-run");
        }

        handleReceivingLine(line);
        return;
    }

    /*
     * v45 rule: do not use arbitrary image edges as phasing sync.  v43 could
     * lock on a map border or printed text, then circularly rotate every row: the
     * result looked like the image was wrapped around a cylinder.  Automatic
     * start is therefore driven only by the APT/control-tone detector.
     */
    if ((m_searchLinesSeen % qMax(4, m_lpm / 10)) == 0) {
        emit statusChanged("Decoder: waiting for WEFAX APT start");
    }

    Q_UNUSED(line)
}


void WeatherFaxDecoder::handlePhasingLine(const DecodedLine &line)
{
    /*
     * v45: do not phase from arbitrary image edges.  Instead, hold the first
     * received lines and look for the real WEFAX phasing stripe: a narrow white
     * band that is vertically stable across several early lines.  Once found,
     * the offset is locked once and then reused for the whole image.
     */
    queueLineForPhaseLock(line.pixels);

    if (m_linePhaseLocked || m_linePhaseFallback) {
        setState(DecoderState::Receiving,
                 m_linePhaseFallback
                     ? QString("WEFAX phase fallback, free-running")
                     : QString("WEFAX phase locked at %1 px").arg(m_phaseOffsetPx));
    }
}

void WeatherFaxDecoder::handleReceivingLine(const DecodedLine &line)
{
    /*
     * If the decoder started in manual/free-run mode, the first decoded lines
     * may still contain the phasing stripe.  Queue only those early lines so the
     * whole visible image is shifted coherently after one stable phase decision.
     * During normal reception, never chase later map/text edges.
     */
    if (m_autoStartEnabled && !m_linePhaseLocked && !m_linePhaseFallback && m_y == 0) {
        queueLineForPhaseLock(line.pixels);
        return;
    }

    // In manual/free-running WEFAX, draw from the first decoded line.  The
    // phasing queue remains reserved for normal automatic APT/phasing RX.
    writeImageLine(phaseShiftedLine(line.pixels));
}


// -----------------------------------------------------------------------------
// Image generation
// -----------------------------------------------------------------------------

void WeatherFaxDecoder::writeImageLine(const QVector<int> &pixels)
{
    if (m_imageCompleteEmitted) {
        return;
    }

    if (m_image.isNull()) {
        m_imageHeight = qMin(qMax(256, m_imageHeight), m_targetImageLines);
        m_image = QImage(m_pixelsPerLine, m_imageHeight, QImage::Format_RGB32);
        m_image.fill(Qt::black);
        m_y = 0;
        m_statusLineCounter = 0;
        m_imageCompleteEmitted = false;
        emit imageUpdated(QImage());
        emit statusChanged("Decoder: new WEFAX image - first received line");
    }

    growImageIfNeeded();

    if (m_y >= m_image.height()) {
        completeImage(QString("maximum safety height reached at %1 lines").arg(m_y));
        return;
    }

    const int count = qMin(m_pixelsPerLine, pixels.size());

    for (int x = 0; x < count; ++x) {
        const int gray = qBound(0, pixels[x], 255);
        m_image.setPixel(x, m_y, qRgb(gray, gray, gray));
    }

    for (int x = count; x < m_pixelsPerLine; ++x) {
        m_image.setPixel(x, m_y, qRgb(0, 0, 0));
    }

    if (!pixels.isEmpty()) {
        m_lastGray = qBound(0, pixels.last(), 255);
        m_hasLastGray = true;
    }

    ++m_y;
    ++m_statusLineCounter;

    emit imageUpdated(completedImageSnapshot());

    if ((m_statusLineCounter % 8) == 0) {
        emit statusChanged(
            QString("Decoder: WEFAX receiving line %1")
                .arg(m_y)
            );
    }
}


void WeatherFaxDecoder::resetLinePhaseLockState()
{
    m_phaseOffsetPx = 0;
    m_candidatePhaseOffsetPx = 0;
    m_searchCandidateCount = 0;
    m_phasingConfirmCount = 0;
    m_searchLinesSeen = 0;
    m_linePhaseLocked = false;
    m_linePhaseFallback = false;
    m_pendingPhaseLines.clear();
}

void WeatherFaxDecoder::queueLineForPhaseLock(const QVector<int> &pixels)
{
    if (pixels.isEmpty() || m_linePhaseLocked || m_linePhaseFallback) {
        return;
    }

    m_pendingPhaseLines.append(pixels);

    if ((m_pendingPhaseLines.size() % 8) == 0 &&
        m_pendingPhaseLines.size() < m_minPhaseQueueLines) {
        emit statusChanged(
            QString("Decoder: acquiring WEFAX line sync %1/%2")
                .arg(m_pendingPhaseLines.size())
                .arg(m_minPhaseQueueLines)
            );
    }

    if (m_pendingPhaseLines.size() >= m_minPhaseQueueLines) {
        if (tryLockPhaseFromQueuedLines(false)) {
            flushQueuedPhaseLines();
            return;
        }
    }

    if (m_pendingPhaseLines.size() >= m_maxPhaseQueueLines) {
        tryLockPhaseFromQueuedLines(true);
        flushQueuedPhaseLines();
    }
}

bool WeatherFaxDecoder::tryLockPhaseFromQueuedLines(bool forceFallback)
{
    double confidence = 0.0;
    const int offset = detectQueuedPhaseOffset(&confidence);

    if (confidence >= m_minQueuedPhaseConfidence) {
        m_phaseOffsetPx = qBound(0, offset, qMax(0, m_pixelsPerLine - 1));
        m_linePhaseLocked = true;
        m_linePhaseFallback = false;
        emit statusChanged(
            QString("Decoder: WEFAX sync stripe locked at %1 px, confidence %2")
                .arg(m_phaseOffsetPx)
                .arg(confidence, 0, 'f', 2)
            );
        return true;
    }

    if (forceFallback) {
        m_phaseOffsetPx = 0;
        m_linePhaseLocked = true;
        m_linePhaseFallback = true;
        emit statusChanged("Decoder: WEFAX sync stripe not found, using free-running phase");
        return true;
    }

    return false;
}

void WeatherFaxDecoder::flushQueuedPhaseLines()
{
    if (m_pendingPhaseLines.isEmpty()) {
        return;
    }

    const QVector<QVector<int>> queued = m_pendingPhaseLines;
    m_pendingPhaseLines.clear();

    for (const QVector<int> &pixels : queued) {
        writeImageLine(phaseShiftedLine(pixels));
    }
}

int WeatherFaxDecoder::detectQueuedPhaseOffset(double *confidence) const
{
    if (confidence != nullptr) {
        *confidence = 0.0;
    }

    if (m_pendingPhaseLines.size() < m_minPhaseQueueLines || m_pixelsPerLine < 64) {
        return 0;
    }

    const int n = m_pixelsPerLine;
    const int rows = qMin(m_pendingPhaseLines.size(), qMax(m_minPhaseQueueLines, 64));
    const int narrowBand = qBound(8, n / 40, 28);
    const int wideBand = qBound(narrowBand * 2, n / 18, n / 9);

    QVector<double> columnMean(n, 0.0);
    QVector<double> columnBright(n, 0.0);
    QVector<double> columnGradient(n, 0.0);

    for (int row = 0; row < rows; ++row) {
        const QVector<int> &line = m_pendingPhaseLines[row];

        if (line.size() < n) {
            continue;
        }

        for (int x = 0; x < n; ++x) {
            const int value = qBound(0, line[x], 255);
            const int left = line[(x + n - 1) % n];
            const int right = line[(x + 1) % n];

            columnMean[x] += static_cast<double>(value);
            columnBright[x] += (value >= 190) ? 1.0 : 0.0;
            columnGradient[x] += qAbs(right - left) / 255.0;
        }
    }

    for (int x = 0; x < n; ++x) {
        columnMean[x] /= static_cast<double>(rows);
        columnBright[x] /= static_cast<double>(rows);
        columnGradient[x] /= static_cast<double>(rows);
    }

    double bestScore = -1.0;
    int bestOffset = 0;

    for (int offset = 0; offset < n; ++offset) {
        double syncSum = 0.0;
        double syncBright = 0.0;
        double sideSum = 0.0;
        double sideGrad = 0.0;
        int syncCount = 0;
        int sideCount = 0;

        for (int i = -narrowBand; i <= narrowBand; ++i) {
            const int index = (offset + i + n) % n;
            syncSum += columnMean[index];
            syncBright += columnBright[index];
            ++syncCount;
        }

        for (int i = -wideBand; i <= wideBand; ++i) {
            if (qAbs(i) <= narrowBand) {
                continue;
            }

            const int index = (offset + i + n) % n;
            sideSum += columnMean[index];
            sideGrad += columnGradient[index];
            ++sideCount;
        }

        const double syncMean = syncSum / static_cast<double>(qMax(1, syncCount));
        const double syncBrightFrac = syncBright / static_cast<double>(qMax(1, syncCount));
        const double sideMean = sideSum / static_cast<double>(qMax(1, sideCount));
        const double sideGradient = sideGrad / static_cast<double>(qMax(1, sideCount));
        const double contrast = (syncMean - sideMean) / 255.0;

        /*
         * A real WEFAX phasing stripe is a bright vertical band that persists
         * across many early lines.  Map edges can be sharp, but they usually do
         * not produce this combination of brightness, local contrast, and
         * row-to-row persistence in the first queued lines.
         */
        const double score = (0.58 * qMax(0.0, contrast)) +
                             (0.34 * syncBrightFrac) +
                             (0.08 * sideGradient);

        if (score > bestScore) {
            bestScore = score;
            bestOffset = offset;
        }
    }

    const double boundedConfidence = qBound(0.0, bestScore, 1.0);

    if (confidence != nullptr) {
        *confidence = boundedConfidence;
    }

    return bestOffset;
}


QVector<int> WeatherFaxDecoder::phaseShiftedLine(const QVector<int> &pixels) const
{
    if (pixels.isEmpty()) {
        return pixels;
    }

    QVector<int> shifted;
    shifted.resize(pixels.size());

    const int n = pixels.size();
    int offset = m_phaseOffsetPx % n;

    if (offset < 0) {
        offset += n;
    }

    offset = qBound(0, offset, n - 1);

    for (int x = 0; x < n; ++x) {
        const int sourceIndex = (x + offset) % n;
        shifted[x] = pixels[sourceIndex];
    }

    return shifted;
}

void WeatherFaxDecoder::consumeOneLineFromBuffer()
{
    const double exactConsume =
        effectiveSamplesPerLine() + m_lineSampleRemainder;

    int consumeSamples = static_cast<int>(qFloor(exactConsume));
    m_lineSampleRemainder = exactConsume - static_cast<double>(consumeSamples);

    consumeSamples = qBound(1, consumeSamples, m_buffer.size());

    m_buffer.remove(0, consumeSamples);

    if (m_frequencyBuffer.size() >= consumeSamples) {
        m_frequencyBuffer.remove(0, consumeSamples);
    } else {
        m_frequencyBuffer.clear();
    }
}

void WeatherFaxDecoder::trimBufferIfNeeded()
{
    const int maxBufferedSamples =
        static_cast<int>(qCeil(m_samplesPerLine * 3.0)) + (m_analysisWindow * 2);

    if (m_buffer.size() > maxBufferedSamples) {
        const int removeCount = m_buffer.size() - maxBufferedSamples;
        m_buffer.remove(0, removeCount);

        if (m_frequencyBuffer.size() >= removeCount) {
            m_frequencyBuffer.remove(0, removeCount);
        } else {
            m_frequencyBuffer.clear();
        }

        emit statusChanged("Decoder: WEFAX input backlog trimmed");
    }

    if (m_frequencyBuffer.size() > maxBufferedSamples) {
        m_frequencyBuffer.remove(0, m_frequencyBuffer.size() - maxBufferedSamples);
    }
}

void WeatherFaxDecoder::growImageIfNeeded()
{
    if (m_image.isNull() || m_y < m_image.height()) {
        return;
    }

    if (m_image.height() >= m_targetImageLines) {
        return;
    }

    const int nextHeight = qMin(m_targetImageLines, qMax(m_image.height() + 128, m_image.height() * 2));

    QImage grown(m_pixelsPerLine, nextHeight, QImage::Format_RGB32);
    grown.fill(Qt::black);

    QPainter painter(&grown);
    painter.drawImage(QPoint(0, 0), m_image);
    painter.end();

    m_image = grown;
    m_imageHeight = nextHeight;
}



// -----------------------------------------------------------------------------
// Control tone and slant correction
// -----------------------------------------------------------------------------

void WeatherFaxDecoder::updateAutoSlantFromLine(const DecodedLine &line)
{
    Q_UNUSED(line)

    m_autoSlantPpm = 0.0;
}


void WeatherFaxDecoder::updateControlToneDetector(const QVector<float> &samples)
{
    if (samples.isEmpty() || m_sampleRate <= 0) {
        return;
    }

    m_controlToneBuffer.reserve(m_controlToneBuffer.size() + samples.size());

    for (float sample : samples) {
        m_controlToneBuffer.append(sample);
    }

    const int maxBufferSamples = qMax(m_sampleRate * 2, 8192);

    if (m_controlToneBuffer.size() > maxBufferSamples) {
        m_controlToneBuffer.remove(0, m_controlToneBuffer.size() - maxBufferSamples);
    }

    m_controlToneSamplesSinceAnalysis += samples.size();

    const int analysisHopSamples = qMax(m_sampleRate / 4, 1024);

    if (m_controlToneSamplesSinceAnalysis < analysisHopSamples) {
        return;
    }

    m_controlToneSamplesSinceAnalysis = 0;

    const ControlTone tone = analyzeControlToneWindow();
    const bool active = (tone != ControlTone::None);

    if (active && tone == m_lastControlTone) {
        m_controlToneActiveSamples += analysisHopSamples;
    } else if (active) {
        m_lastControlTone = tone;
        m_controlToneActiveSamples = analysisHopSamples;
    } else {
        m_controlToneActiveSamples = qMax(0, m_controlToneActiveSamples - analysisHopSamples * 2);
        if (m_controlToneActiveSamples == 0) {
            m_lastControlTone = ControlTone::None;
        }
    }

    const int startToneSamples = qMax(m_sampleRate / 2, 2048);
    const int stopToneSamples = qMax((m_sampleRate * 3) / 2, 4096);

    if (m_state != DecoderState::Receiving && m_state != DecoderState::Phasing) {
        if (m_autoStartEnabled &&
            tone == ControlTone::AptStart &&
            m_controlToneActiveSamples >= startToneSamples) {
            startNewImageFrame("APT start tone detected");
            setState(DecoderState::Phasing, "APT start detected");
            m_controlToneActiveSamples = 0;
            m_lastControlTone = ControlTone::None;
        }
        return;
    }

    if (m_y < qMax(40, m_lpm / 2) || m_imageCompleteEmitted) {
        return;
    }

    if (tone == ControlTone::AptStop && m_controlToneActiveSamples >= stopToneSamples) {
        completeImage(QString("APT stop tone detected after %1 lines").arg(m_y));
        m_controlToneActiveSamples = 0;
        m_lastControlTone = ControlTone::None;
    }
}


WeatherFaxDecoder::ControlTone WeatherFaxDecoder::analyzeControlToneWindow() const
{
    if (m_controlToneBuffer.size() < qMax(1024, m_sampleRate / 2) ||
        m_sampleRate <= 0) {
        return ControlTone::None;
    }

    const int analysisSize = qMin(m_controlToneBuffer.size(),
                                  qBound(2048, m_sampleRate, 8192));
    const int start = m_controlToneBuffer.size() - analysisSize;

    QVector<double> window;
    window.resize(analysisSize);

    double mean = 0.0;

    for (int i = 0; i < analysisSize; ++i) {
        mean += static_cast<double>(m_controlToneBuffer[start + i]);
    }

    mean /= static_cast<double>(analysisSize);

    for (int i = 0; i < analysisSize; ++i) {
        const double hann = 0.5 - 0.5 * qCos(
                                (2.0 * M_PI * static_cast<double>(i)) /
                                static_cast<double>(analysisSize - 1));
        window[i] = (static_cast<double>(m_controlToneBuffer[start + i]) - mean) * hann;
    }

    const double startPower = goertzelPowerAt(window, 300.0);
    const double stopPower = goertzelPowerAt(window, 450.0);
    const double blackPower = goertzelPowerAt(window, m_blackHz);
    const double centerPower = goertzelPowerAt(window, (m_blackHz + m_whiteHz) * 0.5);
    const double whitePower = goertzelPowerAt(window, m_whiteHz);

    double wideAveragePower = 0.0;
    int wideCount = 0;

    for (double frequencyHz = 300.0; frequencyHz <= 3000.0; frequencyHz += 100.0) {
        wideAveragePower += goertzelPowerAt(window, frequencyHz);
        ++wideCount;
    }

    wideAveragePower /= static_cast<double>(qMax(1, wideCount));
    wideAveragePower = qMax(wideAveragePower, 1.0e-12);

    const double imagePower = qMax(whitePower, qMax(centerPower, blackPower));
    const double startRatio = startPower / wideAveragePower;
    const double stopRatio = stopPower / wideAveragePower;
    const double imageRatio = imagePower / wideAveragePower;

    /*
     * Conservative APT logic inspired by fldigi's state model: WEFAX 576 uses
     * 300 Hz APT start and 450 Hz APT stop.  We require the wanted APT tone to
     * dominate the other APT tone and the image tones so an image row does not
     * accidentally stop the frame.
     */
    if (startRatio >= 10.0 && startPower > stopPower * 2.8 && startPower > imagePower * 2.2) {
        return ControlTone::AptStart;
    }

    if (stopRatio >= 10.0 && stopPower > startPower * 2.8 && stopPower > imagePower * 2.2) {
        return ControlTone::AptStop;
    }

    if (imageRatio >= 8.0 && imagePower > startPower * 1.4 && imagePower > stopPower * 1.4) {
        return ControlTone::StructuredImage;
    }

    return ControlTone::None;
}



void WeatherFaxDecoder::startNewImageFrame(const QString &reason)
{
    m_buffer.clear();
    m_frequencyBuffer.clear();
    m_lineSampleRemainder = 0.0;

    m_imageHeight = qMin(qMax(256, m_imageHeight), m_targetImageLines);
    m_image = QImage(m_pixelsPerLine, m_imageHeight, QImage::Format_RGB32);
    m_image.fill(Qt::black);

    m_y = 0;
    m_statusLineCounter = 0;
    m_phaseOffsetPx = 0;
    m_candidatePhaseOffsetPx = 0;
    m_phasingConfirmCount = 0;
    m_searchCandidateCount = 0;
    m_searchLinesSeen = 0;
    m_lastGray = 0;
    m_hasLastGray = false;
    m_imageCompleteEmitted = false;
    m_signalLossSamples = 0;
    m_signalRmsEstimate = 0.0;
    resetLinePhaseLockState();

    emit imageUpdated(QImage());

    if (!reason.isEmpty()) {
        emit statusChanged("Decoder: new WEFAX image - " + reason);
    }
}

// -----------------------------------------------------------------------------
// Completion / end-of-signal detection
// -----------------------------------------------------------------------------

void WeatherFaxDecoder::updateSignalPresence(const QVector<float> &samples)
{
    if (samples.isEmpty() || m_sampleRate <= 0) {
        return;
    }

    double sumSquares = 0.0;

    for (float sample : samples) {
        const double value = static_cast<double>(sample);
        sumSquares += value * value;
    }

    const double rms = qSqrt(sumSquares / static_cast<double>(samples.size()));

    if (m_signalRmsEstimate <= 0.0) {
        m_signalRmsEstimate = rms;
    } else {
        const double alpha = (rms > m_signalRmsEstimate) ? 0.08 : 0.015;
        m_signalRmsEstimate = (alpha * rms) + ((1.0 - alpha) * m_signalRmsEstimate);
    }

    if (!m_endOfSignalCompletionEnabled ||
        m_state != DecoderState::Receiving ||
        m_imageCompleteEmitted ||
        m_y < m_minLinesBeforeEndOfSignal) {
        m_signalLossSamples = 0;
        return;
    }

    const double adaptiveThreshold = qMax(0.0025, m_signalRmsEstimate * 0.20);
    const bool signalPresent = (rms >= adaptiveThreshold);

    if (signalPresent) {
        m_signalLossSamples = 0;
        return;
    }

    m_signalLossSamples += samples.size();

    const int timeoutSamples = qMax(1, m_endOfSignalTimeoutSec * m_sampleRate);

    if (m_signalLossSamples >= timeoutSamples) {
        completeImage(
            QString("end of signal after %1 s, %2 lines")
                .arg(m_endOfSignalTimeoutSec)
                .arg(m_y)
            );
    }
}


void WeatherFaxDecoder::resetCompletionState()
{
    m_signalLossSamples = 0;
    m_signalRmsEstimate = 0.0;
    m_imageCompleteEmitted = false;
}

void WeatherFaxDecoder::completeImage(const QString &reason)
{
    if (!m_pendingPhaseLines.isEmpty() && !m_imageCompleteEmitted) {
        tryLockPhaseFromQueuedLines(true);
        flushQueuedPhaseLines();
    }

    if (m_imageCompleteEmitted || m_y <= 0 || m_image.isNull()) {
        return;
    }

    m_imageCompleteEmitted = true;

    const QImage snapshot = completedImageSnapshot();
    emit imageUpdated(snapshot);
    emit statusChanged("Decoder: WEFAX image complete - " + reason);
    emit imageCompleted(snapshot, reason);

    m_buffer.clear();
    m_frequencyBuffer.clear();
    m_lineSampleRemainder = 0.0;

    if (m_autoStartEnabled) {
        setState(DecoderState::Searching, "waiting for next WEFAX APT start");
    }
}


QImage WeatherFaxDecoder::completedImageSnapshot() const
{
    if (m_image.isNull()) {
        return QImage();
    }

    const int usedLines = qBound(1, m_y, m_image.height());

    if (usedLines >= m_image.height()) {
        return m_image.copy();
    }

    return m_image.copy(QRect(0, 0, m_image.width(), usedLines));
}

// -----------------------------------------------------------------------------
// Status
// -----------------------------------------------------------------------------

QString WeatherFaxDecoder::stateName() const
{
    switch (m_state) {
    case DecoderState::Searching:
        return "searching";

    case DecoderState::Phasing:
        return "phasing";

    case DecoderState::Receiving:
        return "receiving";
    }

    return "unknown";
}

void WeatherFaxDecoder::emitConfigurationStatus()
{
    emit statusChanged(
        QString("Decoder: WEFAX %1 LPM, %2-%3 Hz, %4, tone tracking %5, band-pass %6, stop detector %7")
            .arg(m_lpm)
            .arg(m_blackHz, 0, 'f', 0)
            .arg(m_whiteHz, 0, 'f', 0)
            .arg(stateName())
            .arg(m_autoToneTrackingEnabled ? "ON" : "OFF")
            .arg(m_inputBandpassEnabled ? "ON" : "OFF")
            .arg(m_endOfSignalCompletionEnabled ? "ON" : "OFF")
        );
}


void WeatherFaxDecoder::setState(DecoderState state, const QString &reason)
{
    const DecoderState previousState = m_state;
    const bool changed = (m_state != state);
    m_state = state;

    if (state == DecoderState::Receiving &&
        previousState != DecoderState::Receiving) {
        m_signalLossSamples = 0;

        if (m_autoStartEnabled) {
            startCarrierTrackingGuard();
        }
    }

    if (changed || !reason.isEmpty()) {
        QString message = QString("Decoder: WEFAX %1").arg(stateName());

        if (!reason.isEmpty()) {
            message += " - " + reason;
        }

        emit statusChanged(message);
    }
}
