#include "SstvDecoder.h"

#include <QColor>
#include <QtMath>

#include <algorithm>

namespace {

constexpr double kSyncToneHz = 1200.0;
constexpr double kBlackToneHz = 1500.0;
constexpr double kWhiteToneHz = 2300.0;
constexpr double kVideoLowHz = 1450.0;
constexpr double kVideoHighHz = 2350.0;
constexpr int kRed = 0;
constexpr int kGreen = 1;
constexpr int kBlue = 2;
constexpr int kY0 = 3;
constexpr int kY1 = 4;
constexpr int kRedChroma = 5;
constexpr int kBlueChroma = 6;
constexpr int kRobot36Chroma = 7;

constexpr int kEncodingRgb = 0;
constexpr int kEncodingYuvSingle = 1;
constexpr int kEncodingYuvPair = 2;
constexpr int kEncodingRobot36 = 3;

int samplesFromMs(double ms, int sampleRate)
{
    return qMax(1, static_cast<int>(qRound((ms * static_cast<double>(sampleRate)) / 1000.0)));
}

int sampleOffsetFromMs(double ms, int sampleRate)
{
    return static_cast<int>(qRound((ms * static_cast<double>(sampleRate)) / 1000.0));
}



} // namespace

// -----------------------------------------------------------------------------
// Construction
// -----------------------------------------------------------------------------

SstvDecoder::SstvDecoder(QObject *parent)
    : QObject(parent),
      m_mode(modeByName("Martin M1"))
{
    updateDemodulatorCoefficients();
    reset();
}

// -----------------------------------------------------------------------------
// Static metadata
// -----------------------------------------------------------------------------

QString SstvDecoder::modeName()
{
    return "SSTV RX";
}

QStringList SstvDecoder::availableModeNames()
{
    QStringList names;

    for (const Mode &mode : modes()) {
        names.append(mode.label);
    }

    return names;
}

QVector<FrequencyMarker> SstvDecoder::frequencyMarkers()
{
    return {
        {1200.0, "SSTV sync", QColor(220, 0, 0)},
        {1500.0, "SSTV black", QColor(220, 0, 0)},
        {1900.0, "SSTV gray", QColor(220, 0, 0)},
        {2300.0, "SSTV white", QColor(220, 0, 0)}
    };
}

QVector<SstvDecoder::Mode> SstvDecoder::modes()
{
    QVector<Mode> result;

    const auto makeSegment = [](int channel, double startMs, double durationMs) {
        Segment s;
        s.channel = channel;
        s.startMs = startMs;
        s.durationMs = durationMs;
        return s;
    };

    const auto addMartin = [&result, &makeSegment](const QString &key,
                                     const QString &label,
                                     int width,
                                     int height,
                                     double colorMs) {
        const double syncMs = 4.862;
        const double gapMs = 0.572;

        Mode mode;
        mode.key = key;
        mode.label = label;
        mode.width = width;
        mode.height = height;
        mode.transmittedLines = height;
        mode.outputLinesPerTxLine = 1;
        mode.encoding = kEncodingRgb;
        mode.syncMs = syncMs;
        mode.lineMs = syncMs + (4.0 * gapMs) + (3.0 * colorMs);
        mode.needsPreSyncSamples = false;

        double t = syncMs + gapMs;
        mode.segments.append(makeSegment(kGreen, t, colorMs));
        t += colorMs + gapMs;
        mode.segments.append(makeSegment(kBlue, t, colorMs));
        t += colorMs + gapMs;
        mode.segments.append(makeSegment(kRed, t, colorMs));

        result.append(mode);
    };

    const auto addScottie = [&result, &makeSegment](const QString &key,
                                      const QString &label,
                                      int width,
                                      int height,
                                      double colorMs) {
        const double syncMs = 9.0;
        const double gapMs = 1.5;

        Mode mode;
        mode.key = key;
        mode.label = label;
        mode.width = width;
        mode.height = height;
        mode.transmittedLines = height;
        mode.outputLinesPerTxLine = 1;
        mode.encoding = kEncodingRgb;
        mode.syncMs = syncMs;
        mode.lineMs = syncMs + (3.0 * gapMs) + (3.0 * colorMs);
        mode.needsPreSyncSamples = true;

        /*
         * Scottie line order around the sync pulse is:
         * gap, green, gap, blue, sync, gap, red.
         * Negative segment offsets therefore sample green/blue before the sync,
         * while red is sampled after the sync pulse.
         */
        mode.segments.append(makeSegment(kGreen,
                                     -(2.0 * gapMs + colorMs + colorMs),
                                     colorMs));
        mode.segments.append(makeSegment(kBlue,
                                     -(gapMs + colorMs),
                                     colorMs));
        mode.segments.append(makeSegment(kRed,
                                     syncMs + gapMs,
                                     colorMs));

        result.append(mode);
    };

    const auto addRobotSingle = [&result, &makeSegment](const QString &key,
                                                        const QString &label,
                                                        int width,
                                                        int height,
                                                        int transmittedLines,
                                                        double imageSeconds,
                                                        double syncMs,
                                                        double frontPorchMs,
                                                        double backPorchMs,
                                                        double blankMs) {
        const double lineMs = (imageSeconds * 1000.0) / static_cast<double>(qMax(1, transmittedLines));
        const double visibleMs = (lineMs - frontPorchMs - backPorchMs - (2.0 * blankMs) - syncMs) / 4.0;
        const double syncStartMs = lineMs - syncMs;

        Mode mode;
        mode.key = key;
        mode.label = label;
        mode.width = width;
        mode.height = height;
        mode.transmittedLines = transmittedLines;
        mode.outputLinesPerTxLine = 1;
        mode.encoding = kEncodingYuvSingle;
        mode.syncMs = syncMs;
        mode.lineMs = lineMs;
        mode.needsPreSyncSamples = true;

        double t = backPorchMs;
        mode.segments.append(makeSegment(kY0, t - syncStartMs, 2.0 * visibleMs));
        t += 2.0 * visibleMs + blankMs;
        mode.segments.append(makeSegment(kRedChroma, t - syncStartMs, visibleMs));
        t += visibleMs + blankMs;
        mode.segments.append(makeSegment(kBlueChroma, t - syncStartMs, visibleMs));

        result.append(mode);
    };

    const auto addRobot36 = [&result, &makeSegment]() {
        const int width = 320;
        const int height = 240;
        const int transmittedLines = 240;
        const double imageSeconds = 36.00200;
        const double syncMs = 9.0;
        const double frontPorchMs = 0.4;
        const double backPorchMs = 2.5;
        const double blankMs = 7.0;
        const double lineMs = (imageSeconds * 1000.0) / static_cast<double>(transmittedLines);
        const double visibleMs = (lineMs - frontPorchMs - backPorchMs - blankMs - syncMs) / 3.0;
        const double syncStartMs = lineMs - syncMs;

        Mode mode;
        mode.key = QStringLiteral("ROBOT_36");
        mode.label = QStringLiteral("Robot 36");
        mode.width = width;
        mode.height = height;
        mode.transmittedLines = transmittedLines;
        mode.outputLinesPerTxLine = 1;
        mode.encoding = kEncodingRobot36;
        mode.syncMs = syncMs;
        mode.lineMs = lineMs;
        mode.needsPreSyncSamples = true;

        double t = backPorchMs;
        mode.segments.append(makeSegment(kY0, t - syncStartMs, 2.0 * visibleMs));
        t += 2.0 * visibleMs + blankMs;
        mode.segments.append(makeSegment(kRobot36Chroma, t - syncStartMs, visibleMs));

        result.append(mode);
    };

    const auto addPD = [&result, &makeSegment](const QString &key,
                                               const QString &label,
                                               int width,
                                               int height,
                                               int transmittedLines,
                                               double imageSeconds,
                                               double syncMs,
                                               double frontPorchMs,
                                               double backPorchMs) {
        const double lineMs = (imageSeconds * 1000.0) / static_cast<double>(qMax(1, transmittedLines));
        const double visibleMs = (lineMs - frontPorchMs - backPorchMs - syncMs) / 4.0;
        const double syncStartMs = lineMs - syncMs;

        Mode mode;
        mode.key = key;
        mode.label = label;
        mode.width = width;
        mode.height = height;
        mode.transmittedLines = transmittedLines;
        mode.outputLinesPerTxLine = 2;
        mode.encoding = kEncodingYuvPair;
        mode.syncMs = syncMs;
        mode.lineMs = lineMs;
        mode.needsPreSyncSamples = true;

        double t = backPorchMs;
        mode.segments.append(makeSegment(kY0, t - syncStartMs, visibleMs));
        t += visibleMs;
        mode.segments.append(makeSegment(kRedChroma, t - syncStartMs, visibleMs));
        t += visibleMs;
        mode.segments.append(makeSegment(kBlueChroma, t - syncStartMs, visibleMs));
        t += visibleMs;
        mode.segments.append(makeSegment(kY1, t - syncStartMs, visibleMs));

        result.append(mode);
    };

    addMartin("MARTIN_M1", "Martin M1", 320, 256, 146.432);
    addMartin("MARTIN_M2", "Martin M2", 160, 256, 73.216);
    addMartin("MARTIN_M3", "Martin M3", 320, 128, 146.432);
    addMartin("MARTIN_M4", "Martin M4", 160, 128, 73.216);

    addScottie("SCOTTIE_S1", "Scottie S1", 320, 256, 138.240);
    addScottie("SCOTTIE_S2", "Scottie S2", 160, 256, 88.064);
    addScottie("SCOTTIE_S3", "Scottie S3", 320, 128, 138.240);
    addScottie("SCOTTIE_S4", "Scottie S4", 160, 128, 88.064);
    addScottie("SCOTTIE_DX", "Scottie DX", 320, 256, 345.600);

    addRobotSingle("ROBOT_24", "Robot 24", 160, 120, 120, 24.00150, 6.0, 0.1, 3.0, 4.5);
    addRobot36();
    addRobotSingle("ROBOT_72", "Robot 72", 320, 240, 240, 72.00500, 9.0, 0.4, 3.5, 6.0);

    addPD("PD50", "PD50", 320, 256, 128, 49.68770, 20.0, 0.0, 2.08);
    addPD("PD90", "PD90", 320, 256, 128, 89.99500, 20.0, 0.0, 2.08);
    addPD("PD120", "PD120", 640, 496, 248, 126.11150, 20.0, 0.0, 2.08);
    addPD("PD160", "PD160", 512, 400, 200, 160.89420, 20.0, 0.0, 2.00);
    addPD("PD180", "PD180", 640, 496, 248, 187.06450, 20.0, 0.0, 2.00);
    addPD("PD240", "PD240", 640, 496, 248, 248.01700, 20.0, 2.0, 2.00);
    addPD("PD290", "PD290", 800, 616, 308, 288.70200, 20.0, 0.0, 2.00);

    return result;
}

SstvDecoder::Mode SstvDecoder::modeByName(const QString &modeName)
{
    const QString wanted = modeName.trimmed();

    for (const Mode &mode : modes()) {
        if (mode.label.compare(wanted, Qt::CaseInsensitive) == 0 ||
            mode.key.compare(wanted, Qt::CaseInsensitive) == 0) {
            return mode;
        }
    }

    return modes().first();
}

// -----------------------------------------------------------------------------
// Public getters
// -----------------------------------------------------------------------------

QImage SstvDecoder::currentImage() const
{
    return m_image;
}

QString SstvDecoder::currentModeName() const
{
    return m_mode.label;
}

bool SstvDecoder::autoSyncEnabled() const
{
    return m_autoSyncEnabled;
}

int SstvDecoder::horizontalShiftPixels() const
{
    return m_horizontalShiftPixels;
}

int SstvDecoder::redShiftPixels() const
{
    return m_redShiftPixels;
}

int SstvDecoder::blueShiftPixels() const
{
    return m_blueShiftPixels;
}

// -----------------------------------------------------------------------------
// Public slots
// -----------------------------------------------------------------------------

void SstvDecoder::processAudioBlock(const AudioBlock &block)
{
    if (block.samples.isEmpty() || block.sampleRate <= 0) {
        return;
    }

    if (block.sampleRate != m_sampleRate) {
        m_sampleRate = block.sampleRate;
        updateDemodulatorCoefficients();
        m_mmsstvFqc.setSampleRate(static_cast<double>(m_sampleRate));
        resetDemodulatorState();
    }

    m_audioBuffer.reserve(m_audioBuffer.size() + block.samples.size());
    m_frequencyBuffer.reserve(m_frequencyBuffer.size() + block.samples.size());

    for (float sample : block.samples) {
        m_audioBuffer.append(sample);
        appendDemodulatedSample(sample);
    }

    processBufferedLines();
    trimBuffers();
}

void SstvDecoder::reset()
{
    m_audioBuffer.clear();
    m_frequencyBuffer.clear();
    m_y = 0;
    m_statusCounter = 0;
    m_imageCompleteEmitted = false;
    m_robot36PendingY.clear();
    m_robot36PendingRed.clear();
    m_robot36Pending = false;
    m_robot36Phase = 0;
    m_lineClockLocked = false;
    m_autoHorizontalShiftPixels = 0;
    m_autoRedShiftPixels = 0;
    m_autoBlueShiftPixels = 0;
    m_autoRedShiftAverage = 0.0;
    m_autoBlueShiftAverage = 0.0;
    m_autoRedShiftWeight = 0.0;
    m_autoBlueShiftWeight = 0.0;
    m_lockedLineSamplesExact = 0.0;
    m_syncFrequencyCorrectionHz = 0.0;
    m_hasSyncAfc = false;
    m_syncAfcUpdates = 0;
    m_lastLocalSyncErrorSamples = 0;
    resetLineClockRemainder();

    resetDemodulatorState();
    rebuildImage();

    emit imageUpdated(m_image);
    emitConfigurationStatus();
}

void SstvDecoder::setModeName(const QString &modeName)
{
    const Mode nextMode = modeByName(modeName);

    if (nextMode.key == m_mode.key) {
        return;
    }

    m_mode = nextMode;
    reset();
}

void SstvDecoder::setAutoSyncEnabled(bool enabled)
{
    if (m_autoSyncEnabled == enabled) {
        return;
    }

    m_autoSyncEnabled = enabled;
    m_lineClockLocked = false;
    m_autoHorizontalShiftPixels = 0;
    m_autoRedShiftPixels = 0;
    m_autoBlueShiftPixels = 0;
    m_autoRedShiftAverage = 0.0;
    m_autoBlueShiftAverage = 0.0;
    m_autoRedShiftWeight = 0.0;
    m_autoBlueShiftWeight = 0.0;
    m_lockedLineSamplesExact = 0.0;
    m_syncFrequencyCorrectionHz = 0.0;
    m_hasSyncAfc = false;
    m_syncAfcUpdates = 0;
    m_lastLocalSyncErrorSamples = 0;
    resetLineClockRemainder();
    m_audioBuffer.clear();
    m_frequencyBuffer.clear();

    emitConfigurationStatus();
}

void SstvDecoder::setHorizontalShiftPixels(int pixels)
{
    const int bounded = qBound(-m_mode.width, pixels, m_mode.width);

    if (m_horizontalShiftPixels == bounded) {
        return;
    }

    m_horizontalShiftPixels = bounded;
    emitConfigurationStatus();
}

void SstvDecoder::setColorShiftPixels(int redPixels, int bluePixels)
{
    const int limit = qMax(8, m_mode.width / 3);
    const int boundedRed = qBound(-limit, redPixels, limit);
    const int boundedBlue = qBound(-limit, bluePixels, limit);

    if (m_redShiftPixels == boundedRed && m_blueShiftPixels == boundedBlue) {
        return;
    }

    m_redShiftPixels = boundedRed;
    m_blueShiftPixels = boundedBlue;
    emitConfigurationStatus();
}

// -----------------------------------------------------------------------------
// Setup helpers
// -----------------------------------------------------------------------------

void SstvDecoder::rebuildImage()
{
    m_image = QImage(m_mode.width, m_mode.height, QImage::Format_RGB32);
    m_image.fill(Qt::black);
}

void SstvDecoder::updateDemodulatorCoefficients()
{
    if (m_sampleRate <= 0) {
        m_oscIncCos = 1.0;
        m_oscIncSin = 0.0;
        m_demodAlpha = 0.20;
        return;
    }

    const double phaseIncrement =
        (2.0 * M_PI * m_demodCenterHz) / static_cast<double>(m_sampleRate);

    m_oscIncCos = qCos(phaseIncrement);
    m_oscIncSin = qSin(phaseIncrement);

    const double lowPassCutoffHz = 700.0;
    m_demodAlpha = 1.0 - qExp((-2.0 * M_PI * lowPassCutoffHz) /
                              static_cast<double>(m_sampleRate));
    m_demodAlpha = qBound(0.02, m_demodAlpha, 0.95);
}

void SstvDecoder::resetDemodulatorState()
{
    m_oscCos = 1.0;
    m_oscSin = 0.0;
    m_demodI = 0.0;
    m_demodQ = 0.0;
    m_prevDemodI = 0.0;
    m_prevDemodQ = 0.0;
    m_dcEstimate = 0.0;
    m_lastDemodFrequencyHz = m_demodCenterHz;
    m_hasDemodPrevious = false;
    m_mmsstvFqc.setSampleRate(static_cast<double>(m_sampleRate));
    m_mmsstvFqc.reset();
}

// -----------------------------------------------------------------------------
// Streaming FM discriminator
// -----------------------------------------------------------------------------

void SstvDecoder::appendDemodulatedSample(float sample)
{
    if (m_sampleRate <= 0) {
        m_frequencyBuffer.append(1900.0);
        return;
    }

    /*
     * v0.82: MMSSTV-derived receive core.
     *
     * MMSSTV's RX path does not demodulate SSTV video by looking for image
     * edges.  It first converts the audio tone stream into a stable FQC value
     * using a receive filter and zero-crossing frequency counter.  We now use
     * that source-level port here and feed the existing line/sync renderer with
     * the resulting tone estimate in Hz.
     */
    const double frequencyHz = m_mmsstvFqc.processSample(static_cast<double>(sample));
    m_lastDemodFrequencyHz = frequencyHz;
    m_frequencyBuffer.append(frequencyHz);
}


// -----------------------------------------------------------------------------
// Buffered line processing
// -----------------------------------------------------------------------------

void SstvDecoder::processBufferedLines()
{
    if (m_autoSyncEnabled) {
        processSyncLockedLines();
    } else {
        processFreeRunningLines();
    }
}

void SstvDecoder::processSyncLockedLines()
{
    int decodedThisCall = 0;
    const int maxLinesPerCall = 8;
    const int lineSamples = qMax(1, static_cast<int>(qCeil(activeLineSamples())));
    const bool canUseFixedLineClock = !m_mode.needsPreSyncSamples;
    const int guardSamples = syncGuardSamples();

    while (decodedThisCall < maxLinesPerCall) {
        if (m_y >= m_mode.height) {
            return;
        }

        if (canUseFixedLineClock && m_lineClockLocked) {
            if (m_frequencyBuffer.size() < (lineSamples + guardSamples + scaledSamplesFromMs(m_mode.syncMs))) {
                return;
            }

            int syncStart = guardSamples;
            double localScore = 0.0;
            const int radius = qMax(samplesFromMs(4.0, m_sampleRate), guardSamples);

            if (findLocalSyncNear(guardSamples, radius, &syncStart, &localScore)) {
                m_lastLocalSyncErrorSamples = syncStart - guardSamples;
            } else {
                m_lastLocalSyncErrorSamples = 0;
                syncStart = guardSamples;
            }

            if (!hasCompleteLineAroundSync(syncStart)) {
                return;
            }

            updateSyncAfcFromWindow(syncStart);

            QVector<QVector<QRgb>> lines;
            if (decodeLineAtSync(syncStart, &lines)) {
                for (const QVector<QRgb> &line : lines) {
                    writeLine(line);
                }
            }

            /*
             * MMSSTV does not blindly consume one nominal line after lock: it
             * watches the sync trace and can skip/hold a few samples to keep
             * the received base aligned.  Keep the next buffer positioned with
             * guardSamples before the following sync.
             */
            const int consume = qMax(1, syncStart + nextLineAdvanceSamples() - guardSamples);
            consumeSamples(consume);
            ++decodedThisCall;
            continue;
        }

        /*
         * v0.75/v0.77: Martin has a recurring horizontal sync at the start of every
         * scan line.  Before falling back to arbitrary free-run timing, queue a
         * few nominal lines and find the stable 1200 Hz sync phase.  This mirrors
         * the WEFAX v45 rule: lock one coherent line phase early, then keep it;
         * do not chase picture edges and do not start from VIS/header leftovers.
         */
        if (canUseFixedLineClock &&
            !m_lineClockLocked &&
            m_frequencyBuffer.size() >= (lineSamples * 3)) {
            if (tryLockRecurringSyncPhase(false)) {
                continue;
            }
        }

        int searchFrom = 0;

        for (const Segment &segment : m_mode.segments) {
            if (segment.startMs < 0.0) {
                searchFrom = qMax(searchFrom, samplesFromMs(-segment.startMs, m_sampleRate));
            }
        }

        const int syncStart = findNextSyncPulse(searchFrom);

        if (syncStart < 0) {
            /*
             * Do not immediately draw a Martin frame from buffer position zero:
             * that is what creates the cylinder/fold effect when the buffer
             * still contains VIS/header audio.  After several lines, accept a
             * weaker recurring phase rather than staying blank forever.
             */
            if (canUseFixedLineClock &&
                !m_lineClockLocked &&
                m_frequencyBuffer.size() >= (lineSamples * 6)) {
                if (tryLockRecurringSyncPhase(true)) {
                    continue;
                }
            }

            if (!canUseFixedLineClock && m_frequencyBuffer.size() >= (lineSamples * 2)) {
                QVector<QVector<QRgb>> fallbackLines;
                if (decodeFreeRunningLine(&fallbackLines)) {
                    for (const QVector<QRgb> &line : fallbackLines) {
                        writeLine(line);
                    }
                }
                consumeSamples(lineSamples);
                ++decodedThisCall;
                continue;
            }

            return;
        }

        if (!hasCompleteLineAroundSync(syncStart)) {
            return;
        }

        updateSyncAfcFromWindow(syncStart);

        QVector<QVector<QRgb>> lines;

        if (decodeLineAtSync(syncStart, &lines)) {
            for (const QVector<QRgb> &line : lines) {
                writeLine(line);
            }
            ++decodedThisCall;
        }

        if (canUseFixedLineClock) {
            /*
             * Martin M1/M2/M3/M4 place sync at the start of the scan line.  The
             * first detected sync establishes phase; subsequent lines are then
             * decoded using the nominal fixed line clock.  This removes the
             * visible vertical comb/jitter caused by per-line sync detections
             * moving a few STFT windows left/right.
             */
            resetLineClockRemainder();
            m_lineClockLocked = true;
            consumeSamples(qMax(1, syncStart + nextLineAdvanceSamples() - syncGuardSamples()));
        } else {
            const int consumeTo = qMax(1, syncStart + scaledSamplesFromMs(m_mode.syncMs));
            consumeSamples(consumeTo);
        }
    }
}

void SstvDecoder::processFreeRunningLines()
{
    int decodedThisCall = 0;
    const int maxLinesPerCall = 8;
    const int lineSamples = qMax(1, static_cast<int>(qCeil(activeLineSamples())));

    while (m_frequencyBuffer.size() >= lineSamples && decodedThisCall < maxLinesPerCall) {
        QVector<QVector<QRgb>> lines;

        if (decodeFreeRunningLine(&lines)) {
            for (const QVector<QRgb> &line : lines) {
                writeLine(line);
            }
        }

        consumeSamples(nextLineAdvanceSamples());
        ++decodedThisCall;
    }
}

int SstvDecoder::findNextSyncPulse(int fromIndex) const
{
    if (m_frequencyBuffer.isEmpty() || m_sampleRate <= 0) {
        return -1;
    }

    const int window = qMax(12, samplesFromMs(m_mode.syncMs * 0.75, m_sampleRate));
    const int last = m_frequencyBuffer.size() - window - 1;
    int bestIndex = -1;
    double bestScore = 0.0;

    int i = qBound(0, fromIndex, qMax(0, last));
    const int step = qMax(4, window / 10);

    while (i < last) {
        int syncLike = 0;
        int videoLike = 0;
        double error = 0.0;

        for (int k = 0; k < window; ++k) {
            const double f = m_frequencyBuffer[i + k];
            if (f >= 1040.0 && f <= 1380.0) {
                ++syncLike;
                error += qAbs(f - kSyncToneHz);
            }
            if (f >= kBlackToneHz && f <= kWhiteToneHz) {
                ++videoLike;
            }
        }

        const double syncRatio = static_cast<double>(syncLike) / static_cast<double>(window);
        const double videoRatio = static_cast<double>(videoLike) / static_cast<double>(window);
        const double avgError = syncLike > 0 ? error / static_cast<double>(syncLike) : 999.0;
        const double score = syncRatio - (0.30 * videoRatio) - (avgError / 900.0);

        if (score > bestScore) {
            bestScore = score;
            bestIndex = i;
        }

        if (syncRatio >= 0.48 && videoRatio <= 0.35 && avgError <= 145.0) {
            return refineSyncStart(i, window);
        }

        i += step;
    }

    if (bestIndex >= 0 && bestScore >= 0.26) {
        return refineSyncStart(bestIndex, window);
    }

    return -1;
}

bool SstvDecoder::tryLockRecurringSyncPhase(bool forceWeakLock)
{
    if (m_lineClockLocked || m_sampleRate <= 0 || m_mode.needsPreSyncSamples) {
        return false;
    }

    double confidence = 0.0;
    const int phase = detectRecurringSyncPhase(&confidence);

    const double strongThreshold = 0.23;
    const double weakThreshold = 0.12;
    const bool accept = confidence >= strongThreshold ||
                        (forceWeakLock && confidence >= weakThreshold);

    if (!accept || phase < 0) {
        if (forceWeakLock) {
            emit statusChanged(
                QString("Decoder: SSTV sync phase not stable yet, confidence %1")
                    .arg(confidence, 0, 'f', 2));
        }
        return false;
    }

    const double estimatedLineSamples = estimateRecurringLineSamples(phase);

    if (estimatedLineSamples > (exactLineSamples() * 0.96) &&
        estimatedLineSamples < (exactLineSamples() * 1.04)) {
        m_lockedLineSamplesExact = estimatedLineSamples;
    } else {
        m_lockedLineSamplesExact = 0.0;
    }

    const int guardSamples = syncGuardSamples();
    const int consume = qBound(0, phase - guardSamples, qMax(0, m_frequencyBuffer.size() - 1));

    /*
     * v0.81: keep a small pre-sync guard, following the MMSSTV receive-base
     * idea.  The guard lets the fixed-clock loop re-find the next 1200 Hz line
     * pulse a few milliseconds early/late instead of accumulating a cylinder
     * roll or colour registration error.  Retain the v0.77/v0.78 automatic
     * display compensation because it is still useful for already-trimmed WAV
     * captures; the manual H phase remains the final trim.
     */
    const int lineSamples = qMax(1, static_cast<int>(qRound(activeLineSamples())));
    int autoShift = static_cast<int>(qRound(
        (static_cast<double>(phase) * static_cast<double>(m_mode.width)) /
        static_cast<double>(lineSamples)));
    autoShift %= qMax(1, m_mode.width);
    if (autoShift < 0) {
        autoShift += m_mode.width;
    }
    m_autoHorizontalShiftPixels = autoShift;

    if (consume > 0) {
        consumeSamples(consume);
    }

    resetLineClockRemainder();
    m_lineClockLocked = true;

    emit statusChanged(
        QString("Decoder: SSTV recurring sync locked at +%1 samples, guard %2, confidence %3, auto H +%4 px, line %5 samples")
            .arg(phase)
            .arg(guardSamples)
            .arg(confidence, 0, 'f', 2)
            .arg(m_autoHorizontalShiftPixels)
            .arg(activeLineSamples(), 0, 'f', 2));

    return true;
}

int SstvDecoder::detectRecurringSyncPhase(double *confidence) const
{
    if (confidence != nullptr) {
        *confidence = 0.0;
    }

    if (m_sampleRate <= 0 || m_frequencyBuffer.isEmpty()) {
        return -1;
    }

    const int lineSamples = samplesFromMs(m_mode.lineMs, m_sampleRate);
    const int syncSamples = scaledSamplesFromMs(m_mode.syncMs);

    if (lineSamples <= syncSamples || m_frequencyBuffer.size() < (lineSamples * 3)) {
        return -1;
    }

    const int availableRows =
        (m_frequencyBuffer.size() - syncSamples - 1) / lineSamples;
    const int rows = qBound(3, availableRows, 8);

    if (availableRows < 3) {
        return -1;
    }

    const int scanLast = qMax(0, lineSamples - 1);
    const int step = qMax(4, syncSamples / 12);

    double bestScore = -1.0;
    int bestPhase = -1;

    for (int phase = 0; phase <= scanLast; phase += step) {
        double sum = 0.0;
        double minRowScore = 1.0;
        int validRows = 0;

        for (int row = 0; row < rows; ++row) {
            const int start = phase + row * lineSamples;

            if (start + syncSamples >= m_frequencyBuffer.size()) {
                break;
            }

            const double score = syncWindowScore(start, syncSamples);
            sum += score;
            minRowScore = qMin(minRowScore, score);
            ++validRows;
        }

        if (validRows < 3) {
            continue;
        }

        const double average = sum / static_cast<double>(validRows);

        /*
         * Reward repetition across rows.  A random dark picture feature can look
         * sync-like once; the real Martin sync repeats at the same phase every
         * line.  The min-row term prevents one strong row from dominating.
         */
        const double score = (0.78 * average) + (0.22 * qMax(0.0, minRowScore));

        if (score > bestScore) {
            bestScore = score;
            bestPhase = phase;
        }
    }

    if (bestPhase < 0) {
        return -1;
    }

    /* Fine search around the best coarse phase. */
    const int fineRadius = qMax(8, step * 2);
    const int fineStart = qMax(0, bestPhase - fineRadius);
    const int fineEnd = qMin(scanLast, bestPhase + fineRadius);

    for (int phase = fineStart; phase <= fineEnd; phase += 2) {
        double sum = 0.0;
        double minRowScore = 1.0;
        int validRows = 0;

        for (int row = 0; row < rows; ++row) {
            const int start = phase + row * lineSamples;

            if (start + syncSamples >= m_frequencyBuffer.size()) {
                break;
            }

            const double score = syncWindowScore(start, syncSamples);
            sum += score;
            minRowScore = qMin(minRowScore, score);
            ++validRows;
        }

        if (validRows < 3) {
            continue;
        }

        const double average = sum / static_cast<double>(validRows);
        const double score = (0.78 * average) + (0.22 * qMax(0.0, minRowScore));

        if (score > bestScore) {
            bestScore = score;
            bestPhase = phase;
        }
    }

    const int refinedPhase = refineSyncStart(bestPhase, syncSamples);
    const double bounded = qBound(0.0, bestScore, 1.0);

    if (confidence != nullptr) {
        *confidence = bounded;
    }

    return refinedPhase;
}

double SstvDecoder::syncWindowScore(int startSample, int syncSamples) const
{
    if (syncSamples <= 0 ||
        startSample < 0 ||
        startSample + syncSamples > m_frequencyBuffer.size()) {
        return 0.0;
    }

    int syncLike = 0;
    int videoLike = 0;
    int outOfBand = 0;
    double error = 0.0;

    for (int k = 0; k < syncSamples; ++k) {
        const double f = m_frequencyBuffer[startSample + k];

        if (f >= 1030.0 && f <= 1390.0) {
            ++syncLike;
            error += qAbs(f - kSyncToneHz);
        } else if (f >= kVideoLowHz && f <= kVideoHighHz) {
            ++videoLike;
        } else {
            ++outOfBand;
        }
    }

    const double count = static_cast<double>(qMax(1, syncSamples));
    const double syncRatio = static_cast<double>(syncLike) / count;
    const double videoRatio = static_cast<double>(videoLike) / count;
    const double outRatio = static_cast<double>(outOfBand) / count;
    const double avgError = syncLike > 0 ? error / static_cast<double>(syncLike) : 999.0;

    double score = syncRatio;
    score -= 0.22 * videoRatio;
    score -= 0.10 * outRatio;
    score -= avgError / 1050.0;

    return qBound(0.0, score, 1.0);
}


int SstvDecoder::refineSyncStart(int candidateStart, int searchRadiusSamples) const
{
    if (m_frequencyBuffer.isEmpty() || candidateStart < 0) {
        return candidateStart;
    }

    const int radius = qMax(8, searchRadiusSamples);
    const int first = qBound(0, candidateStart - radius, qMax(0, m_frequencyBuffer.size() - 1));
    const int last = qBound(first, candidateStart + radius, qMax(0, m_frequencyBuffer.size() - 1));
    const int minRun = qMax(6, samplesFromMs(m_mode.syncMs * 0.18, m_sampleRate));

    int bestStart = candidateStart;
    int bestRun = 0;
    double bestScore = -1.0;

    int i = first;
    while (i <= last) {
        if (!isSyncTone(m_frequencyBuffer[i])) {
            ++i;
            continue;
        }

        const int runStart = i;
        double error = 0.0;

        while (i <= last && isSyncTone(m_frequencyBuffer[i])) {
            error += qAbs(m_frequencyBuffer[i] - kSyncToneHz);
            ++i;
        }

        const int runLength = i - runStart;
        if (runLength < minRun) {
            continue;
        }

        const double avgError = error / static_cast<double>(qMax(1, runLength));
        const double distancePenalty =
            qAbs(runStart - candidateStart) / static_cast<double>(qMax(1, radius));
        const double score = static_cast<double>(runLength) -
                             (0.18 * avgError) -
                             (8.0 * distancePenalty);

        if (score > bestScore || (qFuzzyCompare(score, bestScore) && runLength > bestRun)) {
            bestScore = score;
            bestStart = runStart;
            bestRun = runLength;
        }
    }

    return qBound(0, bestStart, qMax(0, m_frequencyBuffer.size() - 1));
}

bool SstvDecoder::isSyncTone(double frequencyHz) const
{
    return frequencyHz >= 1030.0 && frequencyHz <= 1390.0;
}

double SstvDecoder::exactLineSamples() const
{
    if (m_sampleRate <= 0) {
        return 1.0;
    }

    return (m_mode.lineMs * static_cast<double>(m_sampleRate)) / 1000.0;
}

double SstvDecoder::activeLineSamples() const
{
    if (m_lockedLineSamplesExact > 1.0) {
        return m_lockedLineSamplesExact;
    }

    return exactLineSamples();
}

double SstvDecoder::segmentTimingScale() const
{
    const double nominal = exactLineSamples();

    if (nominal <= 1.0) {
        return 1.0;
    }

    return qBound(0.96, activeLineSamples() / nominal, 1.04);
}

int SstvDecoder::scaledSamplesFromMs(double ms) const
{
    if (m_sampleRate <= 0) {
        return 1;
    }

    const double nominal = (ms * static_cast<double>(m_sampleRate)) / 1000.0;
    return qMax(1, static_cast<int>(qRound(nominal * segmentTimingScale())));
}

int SstvDecoder::scaledSampleOffsetFromMs(double ms) const
{
    if (m_sampleRate <= 0) {
        return 0;
    }

    const double nominal = (ms * static_cast<double>(m_sampleRate)) / 1000.0;
    return static_cast<int>(qRound(nominal * segmentTimingScale()));
}

double SstvDecoder::estimateRecurringLineSamples(int phase) const
{
    if (m_sampleRate <= 0 || m_frequencyBuffer.isEmpty() || phase < 0) {
        return 0.0;
    }

    const double nominal = exactLineSamples();
    const int nominalLineSamples = qMax(1, static_cast<int>(qRound(nominal)));
    const int syncSamples = scaledSamplesFromMs(m_mode.syncMs);

    if (nominalLineSamples <= syncSamples ||
        m_frequencyBuffer.size() < (nominalLineSamples * 3)) {
        return 0.0;
    }

    const int availableRows =
        (m_frequencyBuffer.size() - syncSamples - 1) / nominalLineSamples;
    const int rows = qBound(3, availableRows, 8);

    if (availableRows < 3) {
        return 0.0;
    }

    const int searchRadius = qMax(syncSamples * 2, nominalLineSamples / 40);
    const int step = qMax(2, syncSamples / 10);
    QVector<int> starts;

    for (int row = 0; row < rows; ++row) {
        const int expected = static_cast<int>(qRound(static_cast<double>(phase) +
                                                    static_cast<double>(row) * nominal));
        const int first = qMax(0, expected - searchRadius);
        const int last = qMin(m_frequencyBuffer.size() - syncSamples - 1,
                              expected + searchRadius);

        if (first >= last) {
            continue;
        }

        int best = -1;
        double bestScore = -1.0;

        for (int pos = first; pos <= last; pos += step) {
            const double score = syncWindowScore(pos, syncSamples);

            if (score > bestScore) {
                bestScore = score;
                best = pos;
            }
        }

        if (best < 0 || bestScore < 0.12) {
            continue;
        }

        const int refined = refineSyncStart(best, syncSamples);
        starts.append(refined);
    }

    if (starts.size() < 3) {
        return 0.0;
    }

    QVector<double> diffs;

    for (int i = 1; i < starts.size(); ++i) {
        const double d = static_cast<double>(starts[i] - starts[i - 1]);

        if (d >= nominal * 0.96 && d <= nominal * 1.04) {
            diffs.append(d);
        }
    }

    if (diffs.size() < 2) {
        return 0.0;
    }

    std::sort(diffs.begin(), diffs.end());
    const int mid = diffs.size() / 2;

    if ((diffs.size() % 2) == 0) {
        return 0.5 * (diffs[mid - 1] + diffs[mid]);
    }

    return diffs[mid];
}

int SstvDecoder::nextLineAdvanceSamples()
{
    const double exact = activeLineSamples() + m_lineSampleRemainder;
    int advance = static_cast<int>(qFloor(exact));
    m_lineSampleRemainder = exact - static_cast<double>(advance);

    if (advance <= 0) {
        advance = qMax(1, static_cast<int>(qRound(exactLineSamples())));
        m_lineSampleRemainder = 0.0;
    }

    return advance;
}

void SstvDecoder::resetLineClockRemainder()
{
    m_lineSampleRemainder = 0.0;
}

int SstvDecoder::syncGuardSamples() const
{
    if (!m_autoSyncEnabled || m_mode.needsPreSyncSamples || m_sampleRate <= 0) {
        return 0;
    }

    /*
     * MMSSTV's Martin receive path uses an image offset around 7.2 ms from
     * the sync reference.  A slightly larger guard keeps the sync pulse inside
     * the buffer even with small sound-card clock errors.
     */
    return samplesFromMs(10.0, m_sampleRate);
}

bool SstvDecoder::findLocalSyncNear(int expectedStart,
                                    int radiusSamples,
                                    int *syncStart,
                                    double *score) const
{
    if (syncStart != nullptr) {
        *syncStart = expectedStart;
    }

    if (score != nullptr) {
        *score = 0.0;
    }

    if (m_frequencyBuffer.isEmpty() || m_sampleRate <= 0) {
        return false;
    }

    const int syncSamples = scaledSamplesFromMs(m_mode.syncMs);
    const int first = qMax(0, expectedStart - qMax(1, radiusSamples));
    const int last = qMin(m_frequencyBuffer.size() - syncSamples - 1,
                          expectedStart + qMax(1, radiusSamples));

    if (first >= last) {
        return false;
    }

    const int step = qMax(1, syncSamples / 14);
    int best = -1;
    double bestScore = -1.0;

    for (int pos = first; pos <= last; pos += step) {
        const double s = syncWindowScore(pos, syncSamples);

        if (s > bestScore) {
            bestScore = s;
            best = pos;
        }
    }

    if (best < 0 || bestScore < 0.16) {
        return false;
    }

    const int refined = refineSyncStart(best, syncSamples);
    const double refinedScore = syncWindowScore(refined, syncSamples);

    if (refinedScore < 0.12) {
        return false;
    }

    if (syncStart != nullptr) {
        *syncStart = refined;
    }

    if (score != nullptr) {
        *score = refinedScore;
    }

    return true;
}

void SstvDecoder::updateSyncAfcFromWindow(int syncStart)
{
    if (m_sampleRate <= 0 || syncStart < 0 || m_frequencyBuffer.isEmpty()) {
        return;
    }

    const int syncSamples = scaledSamplesFromMs(m_mode.syncMs);
    const int edgeSkip = qMax(1, syncSamples / 5);
    const int start = syncStart + edgeSkip;
    const int end = syncStart + syncSamples - edgeSkip;

    if (end <= start || end > m_frequencyBuffer.size()) {
        return;
    }

    const double measured = rawAverageFrequency(start, end);

    if (measured < 1040.0 || measured > 1390.0) {
        return;
    }

    const double wantedCorrection = qBound(-180.0, kSyncToneHz - measured, 180.0);

    if (!m_hasSyncAfc) {
        m_syncFrequencyCorrectionHz = wantedCorrection;
        m_hasSyncAfc = true;
    } else {
        const double alpha = 0.08;
        m_syncFrequencyCorrectionHz += alpha * (wantedCorrection - m_syncFrequencyCorrectionHz);
        m_syncFrequencyCorrectionHz = qBound(-180.0, m_syncFrequencyCorrectionHz, 180.0);
    }

    ++m_syncAfcUpdates;
}

double SstvDecoder::rawAverageFrequency(int startSample, int endSample) const
{
    if (m_frequencyBuffer.isEmpty()) {
        return 0.0;
    }

    startSample = qBound(0, startSample, m_frequencyBuffer.size() - 1);
    endSample = qBound(startSample + 1, endSample, m_frequencyBuffer.size());

    double sum = 0.0;
    int count = 0;

    for (int i = startSample; i < endSample; ++i) {
        const double f = m_frequencyBuffer[i];

        if (f >= 850.0 && f <= 2700.0) {
            sum += f;
            ++count;
        }
    }

    if (count <= 0) {
        return m_demodCenterHz;
    }

    return sum / static_cast<double>(count);
}

bool SstvDecoder::hasCompleteLineAroundSync(int syncStart) const
{
    if (m_frequencyBuffer.isEmpty() || syncStart < 0) {
        return false;
    }

    int minIndex = syncStart;
    int maxIndex = syncStart + scaledSamplesFromMs(m_mode.syncMs);

    for (const Segment &s : m_mode.segments) {
        const int start = syncStart + scaledSampleOffsetFromMs(s.startMs);
        const int end = start + scaledSamplesFromMs(s.durationMs);
        minIndex = qMin(minIndex, start);
        maxIndex = qMax(maxIndex, end);
    }

    return minIndex >= 0 && maxIndex <= m_frequencyBuffer.size();
}

bool SstvDecoder::decodeLineAtSync(int syncStart, QVector<QVector<QRgb>> *lines)
{
    if (lines == nullptr || !hasCompleteLineAroundSync(syncStart)) {
        return false;
    }

    lines->clear();

    QVector<int> red(m_mode.width, 0);
    QVector<int> green(m_mode.width, 0);
    QVector<int> blue(m_mode.width, 0);
    QVector<int> y0(m_mode.width, 0);
    QVector<int> y1(m_mode.width, 0);
    QVector<int> redChroma(m_mode.width, 128);
    QVector<int> blueChroma(m_mode.width, 128);
    QVector<int> robotChroma(m_mode.width, 128);

    for (const Segment &s : m_mode.segments) {
        QVector<int> *target = nullptr;

        if (s.channel == kRed) {
            target = &red;
        } else if (s.channel == kGreen) {
            target = &green;
        } else if (s.channel == kBlue) {
            target = &blue;
        } else if (s.channel == kY0) {
            target = &y0;
        } else if (s.channel == kY1) {
            target = &y1;
        } else if (s.channel == kRedChroma) {
            target = &redChroma;
        } else if (s.channel == kBlueChroma) {
            target = &blueChroma;
        } else if (s.channel == kRobot36Chroma) {
            target = &robotChroma;
        }

        if (target == nullptr) {
            continue;
        }

        const int start = syncStart + scaledSampleOffsetFromMs(s.startMs);

        /*
         * MMSSTV DrawSSTV uses m_KSS rather than the full m_KS for pixel
         * mapping in standard RGB modes: m_KSS = m_KS - m_KS/240.  This avoids
         * endpoint smear where the last video samples of one colour bleed into
         * the inter-channel gap/next scan.
         */
        const double effectiveDurationMs = s.durationMs - (s.durationMs / 240.0);
        const int count = scaledSamplesFromMs(effectiveDurationMs);

        if (!sampleSegment(start, count, m_mode.width, target)) {
            return false;
        }
    }

    if (m_mode.encoding == kEncodingRgb) {
        updateAutomaticChromaRegistration(red, green, blue);

        const double redShift = static_cast<double>(m_redShiftPixels) + activeAutoRedShiftPixels();
        const double blueShift = static_cast<double>(m_blueShiftPixels) + activeAutoBlueShiftPixels();

        applyColorShiftInPlace(&red, redShift);
        applyColorShiftInPlace(&blue, blueShift);

        QVector<QRgb> line;
        line.resize(m_mode.width);

        for (int x = 0; x < m_mode.width; ++x) {
            line[x] = qRgb(red[x], green[x], blue[x]);
        }

        lines->append(line);
        return true;
    }

    if (m_mode.encoding == kEncodingYuvSingle) {
        QVector<QRgb> line;
        line.resize(m_mode.width);

        for (int x = 0; x < m_mode.width; ++x) {
            line[x] = yuvToRgb(y0[x], redChroma[x], blueChroma[x]);
        }

        lines->append(line);
        return true;
    }

    if (m_mode.encoding == kEncodingYuvPair) {
        QVector<QRgb> first;
        QVector<QRgb> second;
        first.resize(m_mode.width);
        second.resize(m_mode.width);

        for (int x = 0; x < m_mode.width; ++x) {
            first[x] = yuvToRgb(y0[x], redChroma[x], blueChroma[x]);
            second[x] = yuvToRgb(y1[x], redChroma[x], blueChroma[x]);
        }

        lines->append(first);
        lines->append(second);
        return true;
    }

    if (m_mode.encoding == kEncodingRobot36) {
        const bool firstHalfOfPair = ((m_robot36Phase % 2) == 0);
        ++m_robot36Phase;

        if (firstHalfOfPair) {
            m_robot36PendingY = y0;
            m_robot36PendingRed = robotChroma;
            m_robot36Pending = true;
            return true;
        }

        const QVector<int> firstY = m_robot36Pending ? m_robot36PendingY : y0;
        const QVector<int> redDiff = m_robot36Pending ? m_robot36PendingRed : QVector<int>(m_mode.width, 128);
        const QVector<int> blueDiff = robotChroma;

        QVector<QRgb> first;
        QVector<QRgb> second;
        first.resize(m_mode.width);
        second.resize(m_mode.width);

        for (int x = 0; x < m_mode.width; ++x) {
            first[x] = yuvToRgb(firstY.value(x, y0.value(x, 0)), redDiff.value(x, 128), blueDiff[x]);
            second[x] = yuvToRgb(y0[x], redDiff.value(x, 128), blueDiff[x]);
        }

        m_robot36Pending = false;
        m_robot36PendingY.clear();
        m_robot36PendingRed.clear();

        lines->append(first);
        lines->append(second);
        return true;
    }

    return false;
}

bool SstvDecoder::decodeFreeRunningLine(QVector<QVector<QRgb>> *lines)
{
    if (lines == nullptr || m_frequencyBuffer.isEmpty()) {
        return false;
    }

    double virtualSyncMs = 0.0;

    if (m_mode.needsPreSyncSamples) {
        for (const Segment &segment : m_mode.segments) {
            if (segment.startMs < 0.0) {
                virtualSyncMs = qMax(virtualSyncMs, -segment.startMs);
            }
        }
    }

    return decodeLineAtSync(scaledSamplesFromMs(virtualSyncMs), lines);
}

bool SstvDecoder::sampleSegment(int startSample,
                                int sampleCount,
                                int width,
                                QVector<int> *values) const
{
    if (values == nullptr || startSample < 0 || sampleCount <= 0 || width <= 0) {
        return false;
    }

    if (startSample + sampleCount > m_frequencyBuffer.size()) {
        return false;
    }

    values->resize(width);

    for (int x = 0; x < width; ++x) {
        const int a = startSample + static_cast<int>(qFloor(
                          (static_cast<double>(x) * static_cast<double>(sampleCount)) /
                          static_cast<double>(width)));
        int b = startSample + static_cast<int>(qCeil(
                    (static_cast<double>(x + 1) * static_cast<double>(sampleCount)) /
                    static_cast<double>(width)));

        b = qMax(a + 1, b);
        const double frequency = averageFrequency(a, b);
        (*values)[x] = levelFromFrequency(frequency);
    }

    return true;
}

QRgb SstvDecoder::yuvToRgb(int y, int redChroma, int blueChroma) const
{
    const int yy = qBound(0, y, 255);
    const int rr = qBound(0, redChroma, 255);
    const int bb = qBound(0, blueChroma, 255);

    /*
     * QSSTV/MMSSTV-compatible SSTV YUV conversion.  The transmitted chroma
     * arrays are encoded around neutral values rather than direct RGB channels.
     */
    int r = ((100 * yy) + (140 * rr) - 17850) / 100;
    int b = ((100 * yy) + (178 * bb) - 22695) / 100;
    int g = ((100 * yy) - (71 * rr) - (33 * bb) + 13260) / 100;

    r = qBound(0, r, 255);
    g = qBound(0, g, 255);
    b = qBound(0, b, 255);

    return qRgb(r, g, b);
}

void SstvDecoder::applyColorShiftInPlace(QVector<int> *values, double shiftPixels) const
{
    if (values == nullptr || values->isEmpty() || qAbs(shiftPixels) < 0.01) {
        return;
    }

    const QVector<int> source = *values;
    const int width = source.size();

    if (width <= 1) {
        return;
    }

    /*
     * Non-wrapping fractional shift.  Integer-only chroma shifts make the
     * red/cyan grid lines jump by one pixel whenever the smoothed automatic
     * estimate crosses a rounding boundary.  A tiny linear interpolation keeps
     * the same safe global correction, but removes that visible stair/comb.
     */
    for (int x = 0; x < width; ++x) {
        const double src = static_cast<double>(x) - shiftPixels;

        if (src <= 0.0) {
            (*values)[x] = source.first();
            continue;
        }

        if (src >= static_cast<double>(width - 1)) {
            (*values)[x] = source.last();
            continue;
        }

        const int a = static_cast<int>(qFloor(src));
        const int b = qMin(width - 1, a + 1);
        const double frac = src - static_cast<double>(a);
        const double v = (static_cast<double>(source[a]) * (1.0 - frac)) +
                         (static_cast<double>(source[b]) * frac);
        (*values)[x] = qBound(0, static_cast<int>(qRound(v)), 255);
    }
}

double SstvDecoder::activeAutoRedShiftPixels() const
{
    if (m_autoRedShiftWeight <= 0.02) {
        return 0.0;
    }

    return qBound(-18.0, m_autoRedShiftAverage, 18.0);
}

double SstvDecoder::activeAutoBlueShiftPixels() const
{
    if (m_autoBlueShiftWeight <= 0.02) {
        return 0.0;
    }

    return qBound(-18.0, m_autoBlueShiftAverage, 18.0);
}


void SstvDecoder::updateAutomaticChromaRegistration(const QVector<int> &red,
                                                    const QVector<int> &green,
                                                    const QVector<int> &blue)
{
    if (green.size() < 64 || red.size() != green.size() || blue.size() != green.size()) {
        return;
    }

    int redShift = 0;
    int blueShift = 0;
    double redConfidence = 0.0;
    double blueConfidence = 0.0;

    const bool redOk = estimateStaticChannelShift(green, red, &redShift, &redConfidence);
    const bool blueOk = estimateStaticChannelShift(green, blue, &blueShift, &blueConfidence);

    const auto updateOne = [](bool ok,
                              int candidate,
                              double confidence,
                              double *average,
                              double *weight,
                              int *current) {
        if (!ok || average == nullptr || weight == nullptr || current == nullptr) {
            return;
        }

        const double boundedConfidence = qBound(0.0, confidence, 1.0);

        /*
         * Strong confidence updates quickly; weak but valid confidence still
         * contributes slowly.  Keep this global and small so it cannot create
         * the diagonal/slant failure seen in v0.79.  Once a stable average
         * exists, ignore large low-confidence jumps: periodic grid patterns can
         * otherwise pull the estimator by one repeat and make red/cyan combs.
         */
        if (*weight > 0.45 &&
            qAbs(static_cast<double>(candidate) - *average) > 6.0 &&
            boundedConfidence < 0.55) {
            return;
        }

        const double w = qBound(0.025, boundedConfidence * 0.18, 0.22);

        if (*weight <= 0.0) {
            *average = static_cast<double>(candidate);
            *weight = w;
        } else {
            *average += w * (static_cast<double>(candidate) - *average);
            *weight = qMin(1.0, *weight + w * 0.20);
        }

        *average = qBound(-18.0, *average, 18.0);
        *current = qBound(-18, static_cast<int>(qRound(*average)), 18);
    };

    updateOne(redOk, redShift, redConfidence,
              &m_autoRedShiftAverage, &m_autoRedShiftWeight, &m_autoRedShiftPixels);
    updateOne(blueOk, blueShift, blueConfidence,
              &m_autoBlueShiftAverage, &m_autoBlueShiftWeight, &m_autoBlueShiftPixels);
}

bool SstvDecoder::estimateStaticChannelShift(const QVector<int> &reference,
                                             const QVector<int> &channel,
                                             int *shiftPixels,
                                             double *confidence) const
{
    if (shiftPixels != nullptr) {
        *shiftPixels = 0;
    }

    if (confidence != nullptr) {
        *confidence = 0.0;
    }

    if (reference.size() != channel.size() || reference.size() < 64) {
        return false;
    }

    const int width = reference.size();
    const int maxShift = qBound(4, width / 20, 18);
    const int margin = qMax(12, maxShift + 3);

    double bestScore = -2.0;
    double secondScore = -2.0;
    int bestShift = 0;

    for (int shift = -maxShift; shift <= maxShift; ++shift) {
        double cross = 0.0;
        double refEnergy = 0.0;
        double chEnergy = 0.0;
        int used = 0;

        for (int x = margin; x < width - margin; ++x) {
            const int cx = x - shift;
            if (cx <= 1 || cx >= width - 2) {
                continue;
            }

            /* Use derivatives, not absolute colour.  This makes the estimator
             * prefer shared vertical edges/grid/circle boundaries and ignore
             * areas where one colour channel is deliberately different.
             */
            const double rd = static_cast<double>(reference[x + 1] - reference[x - 1]);
            const double cd = static_cast<double>(channel[cx + 1] - channel[cx - 1]);

            if (qAbs(rd) < 3.0 && qAbs(cd) < 3.0) {
                continue;
            }

            cross += rd * cd;
            refEnergy += rd * rd;
            chEnergy += cd * cd;
            ++used;
        }

        if (used < width / 10 || refEnergy <= 1.0 || chEnergy <= 1.0) {
            continue;
        }

        const double score = cross / qSqrt(refEnergy * chEnergy);

        if (score > bestScore) {
            secondScore = bestScore;
            bestScore = score;
            bestShift = shift;
        } else if (score > secondScore) {
            secondScore = score;
        }
    }

    if (bestScore < 0.22) {
        return false;
    }

    /* Require a little separation from the runner-up to avoid locking to
     * repeated grid patterns ambiguously.  With very periodic test charts the
     * margin may be small, so do not make it too strict.
     */
    const double separation = bestScore - secondScore;
    if (secondScore > -1.5 && separation < 0.012 && qAbs(bestShift) > 2) {
        return false;
    }

    if (shiftPixels != nullptr) {
        *shiftPixels = bestShift;
    }

    if (confidence != nullptr) {
        *confidence = qBound(0.0, bestScore, 1.0);
    }

    return true;
}


// -----------------------------------------------------------------------------
// Frequency conversion
// -----------------------------------------------------------------------------

double SstvDecoder::averageFrequency(int startSample, int endSample) const
{
    const double raw = rawAverageFrequency(startSample, endSample);

    if (raw <= 0.0) {
        return m_demodCenterHz;
    }

    return raw + m_syncFrequencyCorrectionHz;
}

int SstvDecoder::levelFromFrequency(double frequencyHz) const
{
    const double normalized =
        (frequencyHz - kBlackToneHz) / (kWhiteToneHz - kBlackToneHz);

    return qBound(0, static_cast<int>(qRound(normalized * 255.0)), 255);
}

// -----------------------------------------------------------------------------
// Image output
// -----------------------------------------------------------------------------

QVector<QRgb> SstvDecoder::applyHorizontalShift(const QVector<QRgb> &line) const
{
    const int width = line.size();

    const int totalShiftPixels = m_horizontalShiftPixels + m_autoHorizontalShiftPixels;

    if (width <= 0 || totalShiftPixels == 0) {
        return line;
    }

    int shift = totalShiftPixels % width;

    if (shift < 0) {
        shift += width;
    }

    if (shift == 0) {
        return line;
    }

    QVector<QRgb> shifted(width);

    for (int x = 0; x < width; ++x) {
        shifted[(x + shift) % width] = line[x];
    }

    return shifted;
}

void SstvDecoder::writeLine(const QVector<QRgb> &line)
{
    if (line.isEmpty() || m_image.isNull()) {
        return;
    }

    if (m_y >= m_image.height()) {
        /*
         * Keep the completed frame visible.  Older code wrapped y back to zero
         * and cleared the image as soon as extra trailing audio produced one
         * more line; that looked like the last line erased the whole picture.
         */
        if (!m_imageCompleteEmitted) {
            m_imageCompleteEmitted = true;
            emit imageUpdated(m_image);
            emit imageCompleted(m_image, QString("%1 frame complete").arg(m_mode.label));
        }
        return;
    }

    const QVector<QRgb> shiftedLine = applyHorizontalShift(line);
    QRgb *dst = reinterpret_cast<QRgb *>(m_image.scanLine(m_y));

    for (int x = 0; x < m_image.width() && x < shiftedLine.size(); ++x) {
        dst[x] = shiftedLine[x];
    }

    ++m_y;
    ++m_statusCounter;

    // Manual/forced SSTV RX must feel live: update the preview after every
    // decoded line instead of batching four lines.  Decoder DSP is unchanged.
    emit imageUpdated(m_image);

    if ((m_statusCounter % 12) == 0) {
        emit statusChanged(QString("Decoder: SSTV %1 line %2/%3, sync AFC %4 Hz, sync err %5 smp, RGB auto R %6 px B %7 px")
                               .arg(m_mode.label)
                               .arg(m_y)
                               .arg(m_mode.height)
                               .arg(m_syncFrequencyCorrectionHz, 0, 'f', 1)
                               .arg(m_lastLocalSyncErrorSamples)
                               .arg(activeAutoRedShiftPixels(), 0, 'f', 1)
                               .arg(activeAutoBlueShiftPixels(), 0, 'f', 1));
    }

    if (!m_imageCompleteEmitted && m_y >= m_image.height()) {
        m_imageCompleteEmitted = true;
        emit imageUpdated(m_image);
        emit imageCompleted(m_image, QString("%1 frame complete").arg(m_mode.label));
        emit statusChanged(QString("Decoder: SSTV %1 complete").arg(m_mode.label));
    }
}

// -----------------------------------------------------------------------------
// Buffer helpers
// -----------------------------------------------------------------------------

void SstvDecoder::consumeSamples(int samples)
{
    const int count = qBound(0, samples, m_frequencyBuffer.size());

    if (count <= 0) {
        return;
    }

    m_frequencyBuffer.erase(m_frequencyBuffer.begin(), m_frequencyBuffer.begin() + count);
    m_audioBuffer.erase(m_audioBuffer.begin(), m_audioBuffer.begin() + qMin(count, m_audioBuffer.size()));
}

void SstvDecoder::trimBuffers()
{
    const int maxSamples = samplesFromMs(qMax(1000.0, m_mode.lineMs * 3.0), m_sampleRate);

    if (m_frequencyBuffer.size() <= maxSamples) {
        return;
    }

    const int excess = m_frequencyBuffer.size() - maxSamples;
    consumeSamples(excess);
}

void SstvDecoder::emitConfigurationStatus()
{
    const int effectivePhase = m_horizontalShiftPixels + m_autoHorizontalShiftPixels;
    const double scalePercent = segmentTimingScale() * 100.0;

    emit statusChanged(QString("Decoder: SSTV %1, %2x%3, sync %4, H phase %5 px, effective %6 px, RGB timing %7%, RGB manual R %8 px B %9 px, RGB auto R %10 px B %11 px, sync AFC %12 Hz, sync err %13 smp")
                           .arg(m_mode.label)
                           .arg(m_mode.width)
                           .arg(m_mode.height)
                           .arg(m_autoSyncEnabled ? "ON" : "OFF")
                           .arg(m_horizontalShiftPixels)
                           .arg(effectivePhase)
                           .arg(scalePercent, 0, 'f', 3)
                           .arg(m_redShiftPixels)
                           .arg(m_blueShiftPixels)
                           .arg(activeAutoRedShiftPixels(), 0, 'f', 1)
                           .arg(activeAutoBlueShiftPixels(), 0, 'f', 1)
                           .arg(m_syncFrequencyCorrectionHz, 0, 'f', 1)
                           .arg(m_lastLocalSyncErrorSamples));
}
