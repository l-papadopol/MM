#ifndef SSTVDECODER_H
#define SSTVDECODER_H

#include "../../audio/AudioBlock.h"
#include "../../dsp/FrequencyMarker.h"
#include "../../third_party/mmsstv_lgpl/MmsstvRxCore.h"

#include <QImage>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

/**
 * @brief Multimode analog SSTV receiver prototype.
 *
 * Purpose:
 * - Decode common RGB analog SSTV modes from the live audio stream.
 * - Use an MMSSTV-derived FQC/zero-crossing tone estimator for 1200/1500/2300 Hz SSTV tones.
 * - Synchronize lines from the 1200 Hz horizontal sync pulse.
 * - Render the progressive received picture into the shared image widget.
 *
 * Supported first-stage modes:
 * - Martin M1, M2, M3, M4.
 * - Scottie S1, S2, S3, S4, DX.
 *
 * Current limitations:
 * - VIS automatic mode detection is not implemented yet.
 * - Robot, PD, MP/MR/ML and AVT families are not implemented yet.
 * - Scottie reception uses sync-based line reconstruction and may still need
 *   manual slant/phase refinement on weak HF signals.
 */
class SstvDecoder : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Creates the decoder with Martin M1 selected by default.
     */
    explicit SstvDecoder(QObject *parent = nullptr);

    /**
     * @brief Returns the user-visible mode family name.
     */
    static QString modeName();

    /**
     * @brief Returns all supported SSTV sub-mode labels.
     */
    static QStringList availableModeNames();

    /**
     * @brief Returns default SSTV waterfall markers.
     */
    static QVector<FrequencyMarker> frequencyMarkers();

    /**
     * @brief Returns the current decoded image.
     */
    QImage currentImage() const;

    /**
     * @brief Returns the selected sub-mode label.
     */
    QString currentModeName() const;

    /**
     * @brief Returns true when horizontal sync detection is enabled.
     */
    bool autoSyncEnabled() const;

    /**
     * @brief Returns the displayed horizontal phase correction in pixels.
     *
     * Positive values roll the decoded SSTV line to the right.
     * Negative values roll it to the left.
     */
    int horizontalShiftPixels() const;

    /**
     * @brief Returns manual per-channel display registration in pixels.
     */
    int redShiftPixels() const;
    int blueShiftPixels() const;

public slots:
    /**
     * @brief Processes one live audio block.
     */
    void processAudioBlock(const AudioBlock &block);

    /**
     * @brief Clears buffers and the current image.
     */
    void reset();

    /**
     * @brief Selects one supported SSTV sub-mode.
     */
    void setModeName(const QString &modeName);

    /**
     * @brief Enables or disables horizontal sync detection.
     */
    void setAutoSyncEnabled(bool enabled);

    /**
     * @brief Sets displayed horizontal phase correction in pixels.
     */
    void setHorizontalShiftPixels(int pixels);

    /**
     * @brief Sets manual Martin/Scottie red and blue registration in pixels.
     *
     * These are deliberately manual and default to zero.  They are a safe
     * post-decode trim for residual chroma offset after the whole-line phase
     * is correct; unlike the reverted v0.79 experiment, they do not chase
     * image edges automatically.
     */
    void setColorShiftPixels(int redPixels, int bluePixels);

signals:
    /**
     * @brief Emits the progressively decoded image.
     */
    void imageUpdated(const QImage &image);

    /**
     * @brief Emits compact decoder status text.
     */
    void statusChanged(const QString &status);

    /**
     * @brief Emits when the received image reaches the configured mode height.
     */
    void imageCompleted(const QImage &image, const QString &reason);

private:
    /**
     * @brief One decoded video segment relative to a horizontal sync pulse.
     *
     * RGB modes use red/green/blue channels directly.  Robot and PD modes use
     * Y plus encoded red/blue chroma-difference channels, matching the classic
     * QSSTV/MMSSTV YUV timing model.
     */
    struct Segment
    {
        int channel = 0;
        double startMs = 0.0;
        double durationMs = 0.0;
    };

    /**
     * @brief Runtime definition of one SSTV mode.
     */
    struct Mode
    {
        QString key;
        QString label;
        int width = 320;
        int height = 256;
        int transmittedLines = 256;
        int outputLinesPerTxLine = 1;
        int encoding = 0;
        double syncMs = 4.862;
        double lineMs = 446.446;
        QVector<Segment> segments;
        bool needsPreSyncSamples = false;
    };

private:
    /**
     * @brief Returns all internal mode definitions.
     */
    static QVector<Mode> modes();

    /**
     * @brief Looks up a mode by label or key.
     */
    static Mode modeByName(const QString &modeName);

    /**
     * @brief Rebuilds the image buffer for the selected mode.
     */
    void rebuildImage();

    /**
     * @brief Updates FM discriminator coefficients for the current sample rate.
     */
    void updateDemodulatorCoefficients();

    /**
     * @brief Resets streaming FM discriminator memory.
     */
    void resetDemodulatorState();

    /**
     * @brief Appends one demodulated instantaneous frequency estimate.
     */
    void appendDemodulatedSample(float sample);

    /**
     * @brief Processes as many complete SSTV lines as possible.
     */
    void processBufferedLines();

    /**
     * @brief Processes lines using detected horizontal sync pulses.
     */
    void processSyncLockedLines();

    /**
     * @brief Processes lines using free-running mode timing.
     */
    void processFreeRunningLines();

    /**
     * @brief Finds the next plausible 1200 Hz horizontal sync pulse.
     */
    int findNextSyncPulse(int fromIndex) const;

    /**
     * @brief Tries to lock Martin line phase from recurring queued sync pulses.
     *
     * This is deliberately similar in spirit to the WEFAX v45 one-shot phase
     * locker: it looks for a stable early line reference and then keeps a fixed
     * clock instead of free-running from an arbitrary VIS/header buffer offset.
     */
    bool tryLockRecurringSyncPhase(bool forceWeakLock);

    /**
     * @brief Returns the best recurring 1200 Hz sync phase inside one line.
     */
    int detectRecurringSyncPhase(double *confidence) const;

    /**
     * @brief Scores one possible sync window in the demodulated frequency buffer.
     */
    double syncWindowScore(int startSample, int syncSamples) const;

    /**
     * @brief Refines a sync-window hit to the leading edge of the 1200 Hz run.
     */
    int refineSyncStart(int candidateStart, int searchRadiusSamples) const;

    /**
     * @brief Returns true when an instantaneous frequency looks like SSTV sync.
     */
    bool isSyncTone(double frequencyHz) const;

    /**
     * @brief Returns the exact nominal mode line length in samples.
     */
    double exactLineSamples() const;

    /**
     * @brief Returns the currently locked line length in samples, or nominal when unlocked.
     */
    double activeLineSamples() const;

    /**
     * @brief Returns the current timing scale used for RGB scan segments.
     */
    double segmentTimingScale() const;

    /**
     * @brief Converts a mode timing in milliseconds to scaled samples.
     */
    int scaledSamplesFromMs(double ms) const;

    /**
     * @brief Converts a mode offset in milliseconds to scaled samples.
     */
    int scaledSampleOffsetFromMs(double ms) const;

    /**
     * @brief Estimates the real recurring line period from detected sync edges.
     */
    double estimateRecurringLineSamples(int phase) const;

    /**
     * @brief Returns the next integer line advance while preserving fractional drift.
     */
    int nextLineAdvanceSamples();

    /**
     * @brief Resets fractional fixed-clock timing at a confirmed sync edge.
     */
    void resetLineClockRemainder();

    /**
     * @brief Returns the pre-sync guard kept in the buffer after a fixed-clock lock.
     *
     * Inspired by MMSSTV's receive-base logic: keep a small amount of audio
     * before the expected sync so the decoder can find whether the next sync
     * arrived a little early or late instead of blindly consuming exactly one
     * nominal line.
     */
    int syncGuardSamples() const;

    /**
     * @brief Finds the strongest local sync pulse near an expected position.
     */
    bool findLocalSyncNear(int expectedStart,
                           int radiusSamples,
                           int *syncStart,
                           double *score) const;

    /**
     * @brief Updates sync-tone AFC from one horizontal sync pulse.
     *
     * This follows the same principle as MMSSTV's sync-frequency AFC: use the
     * 1200 Hz line sync, not image colours, as the trustworthy frequency
     * reference.  The resulting correction is applied to video tones before
     * black/white mapping.
     */
    void updateSyncAfcFromWindow(int syncStart);

    /**
     * @brief Averages raw uncorrected instantaneous frequency over one range.
     */
    double rawAverageFrequency(int startSample, int endSample) const;

    /**
     * @brief Returns true if a complete line can be decoded around syncStart.
     */
    bool hasCompleteLineAroundSync(int syncStart) const;

    /**
     * @brief Decodes one transmitted SSTV line relative to syncStart.
     *
     * Classic RGB modes output one image row.  PD modes output two image rows
     * from each transmitted line.  Robot 36 stores the first half-line and
     * outputs two rows when the companion chroma half-line arrives.
     */
    bool decodeLineAtSync(int syncStart, QVector<QVector<QRgb>> *lines);

    /**
     * @brief Decodes one free-running transmitted line from the current buffer start.
     */
    bool decodeFreeRunningLine(QVector<QVector<QRgb>> *lines);

    /**
     * @brief Samples one RGB segment into one channel buffer.
     */
    bool sampleSegment(int startSample,
                       int sampleCount,
                       int width,
                       QVector<int> *values) const;

    /**
     * @brief Converts classic SSTV YUV luma/chroma-difference values to RGB.
     */
    QRgb yuvToRgb(int y, int redChroma, int blueChroma) const;

    /**
     * @brief Applies a fixed, non-wrapping sub-pixel shift to one decoded colour channel.
     *
     * Fractional interpolation avoids the visible one-pixel stair steps that
     * occurred when the automatic chroma estimator slowly crossed integer
     * boundaries while a frame was being drawn.
     */
    void applyColorShiftInPlace(QVector<int> *values, double shiftPixels) const;

    /**
     * @brief Returns the active automatic red/blue sub-pixel offsets.
     */
    double activeAutoRedShiftPixels() const;
    double activeAutoBlueShiftPixels() const;

    /**
     * @brief Updates the safe global automatic R/B registration from one line.
     *
     * Unlike the reverted v0.79 row-by-row correction, this never applies a
     * different skew per raster line.  It estimates only one slowly-smoothed
     * whole-frame red and blue offset relative to green, bounded to a small
     * range, so it cannot create diagonal/slant corruption.
     */
    void updateAutomaticChromaRegistration(const QVector<int> &red,
                                           const QVector<int> &green,
                                           const QVector<int> &blue);

    /**
     * @brief Estimates one channel shift against green using edge correlation.
     */
    bool estimateStaticChannelShift(const QVector<int> &reference,
                                    const QVector<int> &channel,
                                    int *shiftPixels,
                                    double *confidence) const;


    /**
     * @brief Averages instantaneous frequency over one range.
     */
    double averageFrequency(int startSample, int endSample) const;

    /**
     * @brief Converts SSTV video tone frequency to 8-bit level.
     */
    int levelFromFrequency(double frequencyHz) const;

    /**
     * @brief Applies the manual horizontal phase roll to one decoded line.
     */
    QVector<QRgb> applyHorizontalShift(const QVector<QRgb> &line) const;

    /**
     * @brief Writes one decoded image line and emits updates.
     */
    void writeLine(const QVector<QRgb> &line);

    /**
     * @brief Removes consumed samples from all buffers.
     */
    void consumeSamples(int samples);

    /**
     * @brief Trims excessive buffered data before sync lock.
     */
    void trimBuffers();

    /**
     * @brief Emits a mode/status summary.
     */
    void emitConfigurationStatus();

private:
    Mode m_mode;
    QImage m_image;

    QVector<float> m_audioBuffer;
    QVector<double> m_frequencyBuffer;

    int m_sampleRate = 48000;
    int m_y = 0;
    bool m_autoSyncEnabled = true;
    int m_horizontalShiftPixels = 0;
    int m_autoHorizontalShiftPixels = 0;
    int m_redShiftPixels = 0;
    int m_blueShiftPixels = 0;

    /*
     * v0.83/v0.84: safe automatic chroma registration.  These are global, slowly
     * smoothed frame-level offsets, not row-by-row corrections.  v0.84 applies
     * them as fractional shifts to avoid integer stair-step colour combing.
     */
    int m_autoRedShiftPixels = 0;
    int m_autoBlueShiftPixels = 0;
    double m_autoRedShiftAverage = 0.0;
    double m_autoBlueShiftAverage = 0.0;
    double m_autoRedShiftWeight = 0.0;
    double m_autoBlueShiftWeight = 0.0;

    bool m_imageCompleteEmitted = false;
    int m_statusCounter = 0;

    QVector<int> m_robot36PendingY;
    QVector<int> m_robot36PendingRed;
    bool m_robot36Pending = false;
    int m_robot36Phase = 0;

    /*
     * Martin modes have the horizontal sync at the beginning of each line.
     * Locking every single line independently creates visible vertical combing
     * when the detector jitters by even a millisecond.  After the first stable
     * sync, keep a fixed line clock and only reset it on mode/reset changes.
     */
    bool m_lineClockLocked = false;
    double m_lineSampleRemainder = 0.0;

    /*
     * If the captured/transmitted SSTV clock is slightly fast or slow, using
     * only the nominal Martin line period keeps luminance recognizable but
     * leaves the three sequential colour scans horizontally displaced.  Once
     * the recurring sync phase is found, estimate the real line period and use
     * it for both line advance and RGB segment timing.
     */
    double m_lockedLineSamplesExact = 0.0;

    /*
     * MMSSTV-style receive corrections.  The decoder keeps a short pre-sync
     * guard after lock, recentres each line from the 1200 Hz pulse, and uses
     * the measured sync tone as an AFC reference for the video tones.
     */
    double m_syncFrequencyCorrectionHz = 0.0;
    bool m_hasSyncAfc = false;
    int m_syncAfcUpdates = 0;
    int m_lastLocalSyncErrorSamples = 0;

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

    /*
     * v0.82: source-level port of MMSSTV's CFQC-style zero-crossing
     * tone estimator.  The old quadrature FM discriminator is retained in
     * the file for fallback/reference, but the receive path now appends the
     * MMSSTV-derived FQC frequency estimate to m_frequencyBuffer.
     */
    mmsstv_lgpl::FqcToneEstimator m_mmsstvFqc;
};

#endif // SSTVDECODER_H
