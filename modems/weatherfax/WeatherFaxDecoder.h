#ifndef WEATHERFAXDECODER_H
#define WEATHERFAXDECODER_H

#include "../../audio/AudioBlock.h"
#include "../../dsp/FrequencyMarker.h"

#include <QImage>
#include <QObject>
#include <QString>
#include <QVector>

/**
 * @brief HF WeatherFax decoder with APT start/stop, tone tracking, and line-by-line image build.
 *
 * Purpose:
 * - Receive normalized audio blocks.
 * - Demodulate the WEFAX audio tone with a streaming quadrature FM discriminator.
 * - Track the observed black/white carrier tones during live reception.
 * - Convert instantaneous tone frequency into grayscale pixels.
 * - Decode image line-by-line for stable timing.
 * - Wait for a real APT/start or phasing/image signal before drawing.
 * - Grow the received image line by line without exposing an artificial buffer.
 *
 * Current implemented direction:
 * - Searching: collect audio and wait for APT/control start in live RX.
 * - Phasing: short transitional state after APT start; generic edge phasing is disabled.
 * - Receiving: write decoded lines into the image without circular row shifting.
 * - Auto tone tracking: update black/white tones, markers, and UI while receiving.
 * - APT/end detection: emit imageCompleted() when the stop tone or signal loss is detected.
 *
 * Current limitations:
 * - Phasing is heuristic and tone/contrast based.
 * - Start/stop detection is intentionally conservative and can be bypassed by disabling Auto start.
 * - Slant correction has been removed from the active UI/decoder path.
 */
class WeatherFaxDecoder : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Decoder operating state.
     *
     * Details:
     * - Searching waits for APT start or a plausible phasing/image structure.
     * - Phasing confirms line alignment before image rows are written.
     * - Receiving writes decoded rows to the image until APT stop/end-of-signal.
     */
    enum class DecoderState
    {
        Searching,
        Phasing,
        Receiving
    };

    /**
     * @brief Result of one decoded grayscale line.
     *
     * Details:
     * - pixels contains one complete decoded WEFAX line.
     * - phaseOffsetPx is the detected horizontal offset candidate.
     * - phasingConfidence is a 0..1 score for phasing-like structure.
     * - hasPhasingSignal is true when this line looks useful for alignment.
     */
    struct DecodedLine
    {
        QVector<int> pixels;
        int phaseOffsetPx = 0;
        double phasingConfidence = 0.0;
        bool hasPhasingSignal = false;
    };

public:
    /**
     * @brief Creates the decoder and allocates the image buffer.
     *
     * Behavior:
     * - Uses 120 LPM by default.
     * - Uses 1500 Hz as black tone by default.
     * - Uses 2300 Hz as white tone by default.
     * - Starts in automatic Searching state.
     */
    explicit WeatherFaxDecoder(QObject *parent = nullptr);

    /**
     * @brief Returns the user-visible mode name.
     */
    static QString modeName();

    /**
     * @brief Returns the default waterfall markers used by HF WeatherFax.
     */
    static QVector<FrequencyMarker> frequencyMarkers();

    /**
     * @brief Returns the current waterfall markers using the active tone range.
     */
    QVector<FrequencyMarker> currentFrequencyMarkers() const;

    /**
     * @brief Returns a copy of the current decoded image.
     */
    QImage currentImage() const;

    /**
     * @brief Returns the active LPM value.
     */
    int lpm() const;

    /**
     * @brief Returns the active black tone frequency.
     */
    double blackToneHz() const;

    /**
     * @brief Returns the active white tone frequency.
     */
    double whiteToneHz() const;

    /**
     * @brief Returns the current decoder state.
     */
    DecoderState state() const;

    /**
     * @brief Returns true when automatic search/phasing is enabled.
     */
    bool autoStartEnabled() const;

    /**
     * @brief Returns true when live black/white tone tracking is enabled.
     */
    bool autoToneTrackingEnabled() const;

    /**
     * @brief Returns true when input tone-range band-pass filtering is enabled.
     */
    bool inputBandpassEnabled() const;

    /**
     * @brief Compatibility getter. Slant correction is disabled in v45.
     */
    bool autoSlantCorrectionEnabled() const;

    /**
     * @brief Compatibility getter. Manual slant correction is disabled in v45.
     */
    double manualSlantPpm() const;

    /**
     * @brief Compatibility getter. Automatic slant correction is disabled in v45.
     */
    double autoSlantPpm() const;

    /**
     * @brief Returns the internal safety maximum for received image lines.
     */
    int targetImageLines() const;

    /**
     * @brief Returns the number of received image lines in the current frame.
     */
    int receivedImageLines() const;

    /**
     * @brief Returns true when end-of-signal completion is enabled.
     */
    bool endOfSignalCompletionEnabled() const;

    /**
     * @brief Returns the end-of-signal timeout in seconds.
     */
    int endOfSignalTimeoutSec() const;

public slots:
    /**
     * @brief Processes one audio block.
     *
     * Behavior:
     * - Buffers incoming samples.
     * - Runs streaming FM demodulation for each sample.
     * - Updates live black/white tone tracking when enabled.
     * - Decodes complete WEFAX lines when enough samples are available.
     * - Runs state-machine logic:
     *   - Searching waits for APT/control start when auto-start is enabled.
     *   - Phasing transitions to free-running image reception.
     *   - Receiving writes image lines without circular row shift.
     */
    void processAudioBlock(const AudioBlock &block);

    /**
     * @brief Resets decoder state and clears the image.
     *
     * Behavior:
     * - Clears audio and demodulated frequency buffers.
     * - Clears the image.
     * - Resets line timing and phasing state.
     * - Returns to Searching when auto-start is enabled.
     */
    void reset();

    /**
     * @brief Completes the current image if at least one line was received.
     *
     * This is used by offline WAV analysis at EOF and can also be used by
     * manual/host logic. Live RX still normally completes on APT stop or
     * end-of-signal timeout.
     */
    void finishCurrentImage(const QString &reason);

    /**
     * @brief Sets the WEFAX line rate.
     *
     * Supported practical values:
     * - 60 LPM.
     * - 90 LPM.
     * - 120 LPM.
     * - 240 LPM.
     */
    void setLpm(int lpm);

    /**
     * @brief Sets black and white tone frequencies.
     *
     * Behavior:
     * - Frequencies are clamped to a safe audio range.
     * - If the range is invalid, the request is ignored.
     * - Manual/user changes reset synchronization candidates.
     */
    void setToneRange(double blackHz, double whiteHz);

    /**
     * @brief Enables or disables automatic search/phasing.
     *
     * Behavior:
     * - If enabled, reset() starts in Searching state.
     * - If disabled, reset() starts directly in Receiving state.
     */
    void setAutoStartEnabled(bool enabled);

    /**
     * @brief Enables or disables live black/white tone tracking.
     *
     * Behavior:
     * - When enabled, observed WEFAX carrier modes update the decoder tone range.
     * - When disabled, the manually selected black/white tones are used unchanged.
     */
    void setAutoToneTrackingEnabled(bool enabled);

    /**
     * @brief Enables or disables decoder input band-pass filtering.
     *
     * Behavior:
     * - When enabled, audio outside the current black/white tone zone is attenuated.
     * - The useful pass-band follows the Black/White frequency boxes.
     * - The filter affects WeatherFax decoding and tone tracking, not the raw waterfall.
     */
    void setInputBandpassEnabled(bool enabled);

    /**
     * @brief Compatibility no-op. Slant correction is disabled in v45.
     */
    void setAutoSlantCorrectionEnabled(bool enabled);

    /**
     * @brief Compatibility no-op. Slant correction is disabled in v45.
     */
    void setManualSlantPpm(double ppm);

    /**
     * @brief Sets the internal maximum image height in decoded lines.
     *
     * This is a safety limit only. It is no longer exposed to the operator as
     * an image buffer size. The visible image is cropped to actually received
     * lines and grows as lines arrive.
     */
    void setTargetImageLines(int lines);

    /**
     * @brief Enables or disables automatic completion when the signal disappears.
     *
     * Behavior:
     * - When enabled, loss of useful tone energy during Receiving can complete the image.
     * - A minimum number of received lines is required before this can trigger.
     */
    void setEndOfSignalCompletionEnabled(bool enabled);

    /**
     * @brief Sets the signal-loss timeout used for end-of-signal completion.
     */
    void setEndOfSignalTimeoutSec(int seconds);

signals:
    /**
     * @brief Emits the current decoded image.
     */
    void imageUpdated(const QImage &image);

    /**
     * @brief Emits decoder state text.
     */
    void statusChanged(const QString &status);

    /**
     * @brief Emits when mode markers should be refreshed.
     */
    void markersChanged(const QVector<FrequencyMarker> &markers);

    /**
     * @brief Emits when automatic tone tracking updates black/white frequencies.
     */
    void toneRangeUpdated(double blackHz, double whiteHz);

    /**
     * @brief Emits when the current WEFAX image is considered complete.
     *
     * Details:
     * - image is cropped to the actually received line count when needed.
     * - reason is a short human-readable completion reason.
     */
    void imageCompleted(const QImage &image, const QString &reason);

private:
    /**
     * @brief Updates line timing from sample rate and LPM.
     */
    void updateTiming();

    /**
     * @brief Resets search/phasing counters without clearing the image.
     */
    void resetSynchronizationState();

    /**
     * @brief Resets streaming FM demodulator state.
     */
    void resetDemodulatorState();

    /**
     * @brief Updates streaming FM demodulator oscillator and filters.
     */
    void updateDemodulatorCoefficients();

    /**
     * @brief Updates decoder input band-pass coefficients from current tones.
     */
    void updateInputBandpassCoefficients();

    /**
     * @brief Resets decoder input band-pass filter memory.
     */
    void resetInputBandpassState();

    /**
     * @brief Filters one decoder input sample through the optional tone band-pass.
     */
    float filterInputSample(float sample);

    /**
     * @brief Resets automatic tone tracking buffers and histogram.
     */
    void resetCarrierTrackingState();

    /**
     * @brief Starts a short AFC guard interval after sync/phasing.
     *
     * Behavior:
     * - Prevents the single sync/phasing tone from pulling the AFC center.
     * - Clears carrier history so only real image tones affect tracking.
     */
    void startCarrierTrackingGuard();

    /**
     * @brief Appends one sample and one corresponding demodulated frequency.
     */
    void appendDemodulatedSample(float sample);

    /**
     * @brief Updates live carrier mode tracking from incoming audio.
     */
    void updateCarrierTracking(const AudioBlock &block);

    /**
     * @brief Runs one live carrier analysis over the latest samples.
     */
    void analyzeCarrierTrackingWindow();

    /**
     * @brief Computes Goertzel power for one frequency on a windowed block.
     */
    double goertzelPowerAt(const QVector<double> &samples, double frequencyHz) const;

    /**
     * @brief Adds one observed tone frequency to the decaying mode histogram.
     */
    void updateToneHistogram(double frequencyHz, double confidence);

    /**
     * @brief Extracts a constrained AFC black/white tone pair from history.
     *
     * Behavior:
     * - Keeps black/white spacing locked to the configured WEFAX span.
     * - Uses repeated tone observations mainly to move the center frequency.
     * - Falls back to single-side evidence only when it is close to the expected side.
     */
    bool findTrackedToneRange(double *blackHz, double *whiteHz, double *confidence) const;

    /**
     * @brief Applies a tone range internally with optional synchronization reset.
     */
    bool applyToneRangeInternal(double blackHz,
                                double whiteHz,
                                bool resetSync,
                                bool emitStatus,
                                bool emitUiUpdate);

    /**
     * @brief Processes complete buffered WEFAX lines.
     */
    void processBufferedLines();

    /**
     * @brief Returns the number of samples required to decode one line safely.
     */
    int requiredSamplesForOneLine() const;

    /**
     * @brief Returns nominal line timing. Slant correction is disabled in v45.
     */
    double effectiveSamplesPerLine() const;

    /**
     * @brief Compatibility helper. Always returns 0.0 in v45.
     */
    double totalSlantPixelsPerLine() const;

    /**
     * @brief Compatibility no-op. Slant correction is disabled in v45.
     */
    void updateAutoSlantFromLine(const DecodedLine &line);

    /**
     * @brief Returns a signed shortest phase distance in pixels.
     */
    int wrappedPhaseDelta(int a, int b) const;

    enum class ControlTone
    {
        None,
        AptStart,
        AptStop,
        StructuredImage
    };

    /**
     * @brief Updates control-tone based start/stop detector.
     */
    void updateControlToneDetector(const QVector<float> &samples);

    /**
     * @brief Analyzes the latest control-tone window.
     */
    ControlTone analyzeControlToneWindow() const;

    /**
     * @brief Clears the current frame and prepares a new received image.
     */
    void startNewImageFrame(const QString &reason);

    /**
     * @brief Decodes one line from the current demodulated frequency buffer.
     */
    DecodedLine decodeLineFromBuffer() const;

    /**
     * @brief Estimates tone frequency at an offset inside the current line.
     */
    double estimateFrequencyAt(int sampleOffset) const;

    /**
     * @brief Averages demodulated live frequency over one sample range.
     */
    double averageDemodulatedFrequency(int startSample, int endSample) const;

    /**
     * @brief Converts audio frequency into grayscale intensity.
     */
    int grayFromFrequency(double frequencyHz);

    /**
     * @brief Detects whether a decoded line looks like a phasing/alignment line.
     */
    void analyzePhasingForLine(DecodedLine &line) const;

    /**
     * @brief Finds the most likely horizontal phase offset in a grayscale line.
     */
    int detectPhaseOffset(const QVector<int> &pixels, double *confidence) const;

    /**
     * @brief Applies the automatic decoder state machine to one decoded line.
     */
    void handleDecodedLine(const DecodedLine &line);

    /**
     * @brief Handles one line while searching for a WEFAX start/phasing pattern.
     */
    void handleSearchingLine(const DecodedLine &line);

    /**
     * @brief Handles one line while confirming phasing alignment.
     */
    void handlePhasingLine(const DecodedLine &line);

    /**
     * @brief Handles one line while receiving the image.
     */
    void handleReceivingLine(const DecodedLine &line);

    /**
     * @brief Writes one decoded line into the image buffer.
     */
    void writeImageLine(const QVector<int> &pixels);

    /**
     * @brief Resets the one-shot WEFAX line phase locker.
     */
    void resetLinePhaseLockState();

    /**
     * @brief Holds early lines until a stable sync/phasing stripe is found.
     */
    void queueLineForPhaseLock(const QVector<int> &pixels);

    /**
     * @brief Attempts to lock horizontal phase from queued early lines.
     */
    bool tryLockPhaseFromQueuedLines(bool forceFallback);

    /**
     * @brief Writes queued early lines after the phase decision is known.
     */
    void flushQueuedPhaseLines();

    /**
     * @brief Returns the best sync/phasing offset across queued early lines.
     */
    int detectQueuedPhaseOffset(double *confidence) const;

    /**
     * @brief Applies the fixed horizontal phase offset to one decoded line.
     */
    QVector<int> phaseShiftedLine(const QVector<int> &pixels) const;

    /**
     * @brief Consumes one line worth of samples using fractional timing.
     */
    void consumeOneLineFromBuffer();

    /**
     * @brief Trims excessive buffered audio.
     */
    void trimBufferIfNeeded();

    /**
     * @brief Grows the hidden backing image when more received lines arrive.
     */
    void growImageIfNeeded();

    /**
     * @brief Updates the end-of-signal detector from one filtered audio block.
     */
    void updateSignalPresence(const QVector<float> &samples);

    /**
     * @brief Resets end-of-signal detector memory.
     */
    void resetCompletionState();

    /**
     * @brief Emits imageCompleted once and prevents repeated completion events.
     */
    void completeImage(const QString &reason);

    /**
     * @brief Builds an image snapshot cropped to the received line count.
     */
    QImage completedImageSnapshot() const;

    /**
     * @brief Returns a readable state name.
     */
    QString stateName() const;

    /**
     * @brief Emits a compact configuration/status summary.
     */
    void emitConfigurationStatus();

    /**
     * @brief Changes decoder state and emits status if needed.
     */
    void setState(DecoderState state, const QString &reason);

private:
    QVector<float> m_buffer;
    QVector<double> m_frequencyBuffer;
    QImage m_image;

    DecoderState m_state = DecoderState::Searching;

    int m_sampleRate = 48000;

    int m_pixelsPerLine = 800;
    int m_targetImageLines = 8000;
    int m_imageHeight = 256;
    int m_lpm = 120;

    int m_analysisWindow = 512;
    int m_estimateStepPixels = 2;

    double m_samplesPerLine = 24000.0;
    double m_lineSampleRemainder = 0.0;

    int m_y = 0;

    double m_blackHz = 1500.0;
    double m_whiteHz = 2300.0;

    int m_lastGray = 0;
    bool m_hasLastGray = false;

    bool m_autoStartEnabled = true;
    bool m_autoToneTrackingEnabled = true;
    bool m_inputBandpassEnabled = true;
    bool m_autoSlantCorrectionEnabled = false;
    bool m_endOfSignalCompletionEnabled = true;

    int m_endOfSignalTimeoutSec = 20;
    double m_manualSlantPpm = 0.0;
    double m_autoSlantPpm = 0.0;
    double m_lastSlantPhasePx = 0.0;
    int m_lastSlantLine = -1;
    int m_slantObservationCount = 0;
    int m_minLinesBeforeEndOfSignal = 80;
    int m_signalLossSamples = 0;
    double m_signalRmsEstimate = 0.0;
    bool m_imageCompleteEmitted = false;

    int m_phaseOffsetPx = 0;
    int m_candidatePhaseOffsetPx = 0;
    int m_searchCandidateCount = 0;
    int m_phasingConfirmCount = 0;
    int m_searchLinesSeen = 0;

    bool m_linePhaseLocked = false;
    bool m_linePhaseFallback = false;
    QVector<QVector<int>> m_pendingPhaseLines;

    int m_requiredSearchHits = 2;
    int m_requiredPhasingHits = 3;
    int m_minPhaseQueueLines = 48;
    int m_maxPhaseQueueLines = 96;

    double m_minSearchConfidence = 0.42;
    double m_minPhasingConfidence = 0.50;
    double m_minQueuedPhaseConfidence = 0.18;

    int m_statusLineCounter = 0;

    double m_demodCenterHz = 1900.0;
    double m_demodAlpha = 0.20;
    double m_oscCos = 1.0;
    double m_oscSin = 0.0;
    double m_oscIncCos = 1.0;
    double m_oscIncSin = 0.0;
    double m_demodI = 0.0;
    double m_demodQ = 0.0;
    double m_prevDemodI = 0.0;
    double m_prevDemodQ = 0.0;
    double m_dcEstimate = 0.0;
    double m_lastDemodFrequencyHz = 1900.0;
    bool m_hasDemodPrevious = false;

    double m_bpHpAlpha = 0.95;
    double m_bpLpAlpha = 0.20;
    double m_bpHpPrevInput1 = 0.0;
    double m_bpHpPrevInput2 = 0.0;
    double m_bpHpStage1 = 0.0;
    double m_bpHpStage2 = 0.0;
    double m_bpLpStage1 = 0.0;
    double m_bpLpStage2 = 0.0;

    QVector<float> m_carrierBuffer;
    QVector<double> m_toneHistogram;
    int m_carrierSamplesSinceAnalysis = 0;
    int m_toneObservationCount = 0;
    int m_autoToneStatusCounter = 0;
    int m_carrierTrackingGuardSamples = 0;

    QVector<float> m_controlToneBuffer;
    int m_controlToneSamplesSinceAnalysis = 0;
    int m_controlToneActiveSamples = 0;
    int m_controlToneCooldownLines = 0;
    ControlTone m_lastControlTone = ControlTone::None;

    double m_autoToneSpacingBaseHz = 800.0;
    double m_autoToneSpacingHz = 800.0;

    double m_toneHistogramMinHz = 300.0;
    double m_toneHistogramMaxHz = 3000.0;
    int m_toneHistogramBinHz = 10;
};

#endif // WEATHERFAXDECODER_H
