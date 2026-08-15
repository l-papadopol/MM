#include "CwDecoder.h"

#include "skimmer/CwSkimmerEngine.h"
#include "skimmer/SelectedToneCwTracker.h"

#include <QColor>
#include <QtGlobal>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace {

constexpr double kMinUiToneHz = 100.0;
constexpr double kMaxUiToneHz = 3500.0;

} // namespace

CwDecoder::CwDecoder(QObject *parent)
    : QObject(parent)
{
    resetSkimmer();
    reset();
}

CwDecoder::~CwDecoder() = default;

QString CwDecoder::modeName()
{
    return QStringLiteral("CW Morse");
}

QVector<FrequencyMarker> CwDecoder::frequencyMarkers(double toneHz)
{
    QVector<FrequencyMarker> markers;
    if (toneHz <= 0.0) {
        return markers;
    }

    FrequencyMarker marker;
    marker.frequencyHz = qBound(kMinUiToneHz, toneHz, kMaxUiToneHz);
    marker.label = QStringLiteral("A");
    marker.color = QColor(80, 255, 120);
    marker.width = 2;
    marker.dashed = false;
    markers.append(marker);
    return markers;
}

void CwDecoder::resetSkimmer()
{
    m_haveDiagnosticA = false;
    m_haveDiagnosticB = false;
    // Native full-passband carrier discovery. It never decodes Morse and
    // therefore cannot inject unrelated characters into the RX terminals.
    madmodem::cwskimmer::CwSkimmerConfig cfg;
    cfg.minimumFrequencyHz = 100.0;
    cfg.maximumFrequencyHz = 3500.0;
    cfg.associationRadiusHz = 18.0;
    cfg.releaseTimeSec = 3.0;
    cfg.thresholdMultiplier = 8.0f;
    cfg.minEventSnrDb = 7.0f;

    m_skimmer = std::make_unique<madmodem::cwskimmer::CwSkimmerEngine>(cfg);
    m_skimmer->setCallback([this](const madmodem::cwskimmer::CwSkimmerEvent &) {
        refreshPriorityAndOverlays();
    });

    auto makeSelectedTracker = [this](int rank, double toneHz) {
        madmodem::cwskimmer::SelectedToneCwConfig selectedCfg;
        selectedCfg.toneHz = toneHz;
        selectedCfg.bandwidthHz = m_bandwidthHz;
        selectedCfg.minSnrDb = 3.0;
        selectedCfg.initialWpm = rank <= 0 ? m_wpmA : m_wpmB;
        selectedCfg.autoWpm = rank <= 0 ? m_autoWpmA : m_autoWpmB;
        selectedCfg.afcEnabled = m_afcEnabled;
        selectedCfg.autoBandwidth = m_autoBandwidth;
        selectedCfg.afcRangeHz = m_afcRangeHz;
        selectedCfg.maxRollingText = 160;
        selectedCfg.decoderChannelNumber = rank;
        auto tracker = std::make_unique<madmodem::cwskimmer::SelectedToneCwTracker>(selectedCfg);
        tracker->setCallback([this, rank](const madmodem::cwskimmer::SelectedToneCwEvent &event) {
            const QString committed = sanitizeSkimmerText(event.committedText);

            if (!committed.isEmpty()) {
                if (rank == 0) {
                    m_textA += committed;
                    m_selectedCommittedA = m_textA.simplified();
                } else if (rank == 1) {
                    m_textB += committed;
                    m_selectedCommittedB = m_textB.simplified();
                }
                m_text = m_textA;
                if (!m_textB.isEmpty()) {
                    if (!m_text.isEmpty() && !m_text.endsWith(QLatin1Char('\n'))) {
                        m_text += QLatin1Char('\n');
                    }
                    m_text += m_textB;
                }
                emit priorityTextReceived(rank, committed);
                emit textUpdated(m_text);
            }
            if (event.wpm > 0.1 && std::isfinite(event.wpm)) {
                emit receiverSpeedEstimateChanged(rank, event.wpm);
                if (rank == 0 || !(m_trackedWpm > 0.1)) {
                    m_trackedWpm = event.wpm;
                    emit speedEstimateChanged(m_trackedWpm);
                }
            }
            // Decoded text remains in the continuous RX panes; the
            // waterfall receives carrier-lane labels only.
            refreshPriorityAndOverlays();
            emitSkimmerStatus(false);
        });
        tracker->setDiagnosticCallback([this, rank](
            const madmodem::cwskimmer::SelectedToneCwDiagnosticSample &sample) {
            CwDiagnosticPoint point;
            point.timestampSec = sample.timestampSec;
            point.acquisitionEnvelope = static_cast<float>(sample.acquisitionEnvelope);
            point.filteredEnvelope = static_cast<float>(sample.filteredEnvelope);
            point.signalLevel = static_cast<float>(sample.signalLevel);
            point.thresholdLow = static_cast<float>(sample.thresholdLow);
            point.thresholdHigh = static_cast<float>(sample.thresholdHigh);
            point.markProbability = static_cast<float>(sample.markProbability);
            point.qsbErasure = sample.qsbErasure;
            point.qsbErasureStartSec = static_cast<float>(sample.qsbErasureStartSec);
            point.qsbErasureEndSec = static_cast<float>(sample.qsbErasureEndSec);
            point.keyDown = sample.keyDown;
            point.markerHz = static_cast<float>(sample.markerHz);
            point.trackedHz = static_cast<float>(sample.trackedHz);
            point.snrDb = static_cast<float>(sample.snrDb);
            point.requestedBandwidthHz = static_cast<float>(sample.requestedBandwidthHz);
            point.effectiveBandwidthHz = static_cast<float>(sample.effectiveBandwidthHz);
            point.wpm = static_cast<float>(sample.wpm);
            point.lockQuality = static_cast<float>(sample.lockQuality);
            point.trackingConfirmed = sample.trackingConfirmed;
            point.carrierProminenceDb = static_cast<float>(sample.carrierProminenceDb);
            point.coherentSnrDb = static_cast<float>(sample.coherentSnrDb);
            point.coherence = static_cast<float>(sample.coherence);
            point.ditMs = static_cast<float>(sample.ditMs);
            point.dahMs = static_cast<float>(sample.dahMs);
            point.markThresholdMs = static_cast<float>(sample.markThresholdMs);
            point.characterSpaceMs = static_cast<float>(sample.characterSpaceMs);
            point.wordSpaceMs = static_cast<float>(sample.wordSpaceMs);
            point.timingState = QString::fromStdString(sample.timingState);
            point.currentPattern = QString::fromStdString(sample.currentPattern);
            if (rank == 0) {
                m_lastDiagnosticA = point;
                m_haveDiagnosticA = true;
            } else {
                m_lastDiagnosticB = point;
                m_haveDiagnosticB = true;
            }
            QVector<CwDiagnosticPoint> &pending = rank == 0 ? m_pendingDiagnosticsA
                                                            : m_pendingDiagnosticsB;
            pending.append(point);
            if (pending.size() >= 8) {
                emit diagnosticSamplesReady(rank, pending);
                pending.clear();
            }
        });
        tracker->setLogCallback([this, rank](const std::string &line) {
            emit liveLogLine(rank, QString::fromStdString(line));
        });
        tracker->setSpectrumCallback([this, rank](
            const madmodem::cwskimmer::SelectedToneCwSpectrumFrame &frame) {
            QVector<float> offsets;
            QVector<float> inputPsd;
            QVector<float> filteredPsd;
            QVector<float> theoretical;
            offsets.reserve(static_cast<int>(frame.offsetsHz.size()));
            inputPsd.reserve(static_cast<int>(frame.inputPsdDb.size()));
            filteredPsd.reserve(static_cast<int>(frame.filteredPsdDb.size()));
            theoretical.reserve(static_cast<int>(frame.theoreticalResponseDb.size()));
            for (float value : frame.offsetsHz) offsets.append(value);
            for (float value : frame.inputPsdDb) inputPsd.append(value);
            for (float value : frame.filteredPsdDb) filteredPsd.append(value);
            for (float value : frame.theoreticalResponseDb) theoretical.append(value);
            emit diagnosticSpectrumReady(
                rank, offsets, inputPsd, filteredPsd, theoretical,
                static_cast<float>(frame.trackedHz - frame.markerHz),
                static_cast<float>(frame.effectiveBandwidthHz),
                static_cast<float>(frame.interfererHz - frame.markerHz),
                static_cast<float>(frame.interfererConfidence),
                static_cast<float>(frame.carrierProminenceDb),
                static_cast<float>(frame.carrierPeakWidthHz),
                static_cast<float>(frame.carrierToSecondDb));
        });
        return tracker;
    };

    m_toneTrackerA = makeSelectedTracker(0, m_toneHz);
    m_toneTrackerB = makeSelectedTracker(1, m_secondaryToneHz);
}

void CwDecoder::reset()
{
    m_sampleCounter = 0;
    m_statusCounter = 0;
    m_sampleRate = 0;
    m_text.clear();
    m_textA.clear();
    m_textB.clear();
    m_selectedCommittedA.clear();
    m_selectedCommittedB.clear();
    m_pendingDiagnosticsA.clear();
    m_pendingDiagnosticsB.clear();
    m_haveDiagnosticA = false;
    m_haveDiagnosticB = false;
    emit diagnosticResetRequested(0);
    emit diagnosticResetRequested(1);
    m_trackedWpm = qBound(5.0, m_wpmA, 50.0);
    resetSkimmer();

    emit textUpdated(m_text);
    emit markersChanged(frequencyMarkers(m_toneHz));
    emit speedEstimateChanged(m_trackedWpm);
    emitSkimmerStatus(true);
    refreshPriorityAndOverlays();
}

void CwDecoder::clearReceiver(int rank)
{
    if (rank <= 0) {
        if (m_toneTrackerA) {
            m_toneTrackerA->reset();
        }
        m_textA.clear();
        m_selectedCommittedA.clear();
        m_pendingDiagnosticsA.clear();
        m_haveDiagnosticA = false;
        emit diagnosticResetRequested(0);
    } else if (rank == 1) {
        if (m_toneTrackerB) {
            m_toneTrackerB->reset();
        }
        m_textB.clear();
        m_selectedCommittedB.clear();
        m_pendingDiagnosticsB.clear();
        m_haveDiagnosticB = false;
        emit diagnosticResetRequested(1);
    } else {
        return;
    }

    m_text = m_textA;
    if (!m_textB.isEmpty()) {
        if (!m_text.isEmpty() && !m_text.endsWith(QLatin1Char('\n'))) {
            m_text += QLatin1Char('\n');
        }
        m_text += m_textB;
    }
    emit textUpdated(m_text);
    refreshPriorityAndOverlays();
    emitSkimmerStatus(true);
}


void CwDecoder::updateTrackerInterferers()
{
    if (!m_skimmer) return;
    const auto lanes = m_skimmer->channelStates();

    auto buildList = [&](double targetHz, double otherMarkerHz,
                         bool includeOtherMarker) {
        std::vector<madmodem::cwskimmer::SelectedToneCwInterferer> result;
        result.reserve(5U);

        auto addOrMerge = [&](double toneHz, double confidence) {
            if (!std::isfinite(toneHz) || !std::isfinite(confidence)) return;
            const double separation = std::abs(toneHz - targetHz);
            if (separation < 14.0 || separation > 260.0) return;
            for (auto &existing : result) {
                if (std::abs(existing.toneHz - toneHz) <= 5.0) {
                    if (confidence > existing.confidence) {
                        existing.toneHz = toneHz;
                        existing.confidence = confidence;
                    }
                    return;
                }
            }
            madmodem::cwskimmer::SelectedToneCwInterferer item;
            item.toneHz = toneHz;
            item.confidence = qBound(0.0, confidence, 1.0);
            result.push_back(item);
        };

        // The second operator-selected receiver is the highest-confidence known
        // adjacent lane.  A silent marker is harmless because the projection
        // only subtracts a measured coherent component at that frequency.
        if (includeOtherMarker) addOrMerge(otherMarkerHz, 1.0);

        for (const auto &lane : lanes) {
            if (lane.ageFrames < 3U || lane.confidence < 0.42f || lane.snrDb < 7.0f)
                continue;
            const double snrWeight = qBound(0.0,
                (static_cast<double>(lane.snrDb) - 7.0) / 15.0, 1.0);
            const double confidence = qBound(0.0,
                0.70 * static_cast<double>(lane.confidence) + 0.30 * snrWeight,
                1.0);
            addOrMerge(lane.audioFrequencyHz, confidence);
        }
        return result;
    };

    if (m_toneTrackerA) {
        m_toneTrackerA->setInterferers(
            buildList(m_toneHz, m_secondaryToneHz, m_secondaryEnabled));
    }
    if (m_toneTrackerB) {
        m_toneTrackerB->setInterferers(
            buildList(m_secondaryToneHz, m_toneHz, m_secondaryEnabled));
    }
}

void CwDecoder::processAudioBlock(const AudioBlock &block)
{
    if (block.samples.isEmpty() || block.sampleRate <= 0 || !m_skimmer) {
        return;
    }

    if (block.sampleRate != m_sampleRate) {
        m_sampleRate = block.sampleRate;
        resetSkimmer();
    }

    m_sampleCounter += block.samples.size();
    m_skimmer->processFloatMono(block.samples.constData(),
                                static_cast<std::size_t>(block.samples.size()),
                                static_cast<double>(block.sampleRate));
    updateTrackerInterferers();
    if (m_toneTrackerA) {
        m_toneTrackerA->processFloatMono(block.samples.constData(),
                                         static_cast<std::size_t>(block.samples.size()),
                                         static_cast<double>(block.sampleRate));
    }
    if (m_secondaryEnabled && m_toneTrackerB) {
        m_toneTrackerB->processFloatMono(block.samples.constData(),
                                         static_cast<std::size_t>(block.samples.size()),
                                         static_cast<double>(block.sampleRate));
    }

    // Keep waterfall OSD alive even between decode bursts.
    ++m_statusCounter;
    if (m_statusCounter >= 24) {
        m_statusCounter = 0;
        refreshPriorityAndOverlays();
        emitSkimmerStatus(false);
    }
}

void CwDecoder::setToneHz(double toneHz)
{
    const double bounded = qBound(kMinUiToneHz, toneHz, kMaxUiToneHz);
    if (std::abs(bounded - m_toneHz) < 0.5) {
        return;
    }
    m_toneHz = bounded;
    if (m_toneTrackerA) {
        m_toneTrackerA->setToneHz(m_toneHz);
    }
    updateTrackerInterferers();
    m_selectedCommittedA.clear();
    m_haveDiagnosticA = false;
    m_pendingDiagnosticsA.clear();
    emit diagnosticResetRequested(0);
    refreshPriorityAndOverlays();
    emit markersChanged(frequencyMarkers(m_toneHz));
    emitSkimmerStatus(true);
}

void CwDecoder::setSecondaryToneHz(double toneHz)
{
    const double bounded = qBound(kMinUiToneHz, toneHz, kMaxUiToneHz);
    if (std::abs(bounded - m_secondaryToneHz) < 0.5) {
        return;
    }
    m_secondaryToneHz = bounded;
    if (m_toneTrackerB) {
        m_toneTrackerB->setToneHz(m_secondaryToneHz);
    }
    updateTrackerInterferers();
    m_selectedCommittedB.clear();
    m_haveDiagnosticB = false;
    m_pendingDiagnosticsB.clear();
    emit diagnosticResetRequested(1);
    refreshPriorityAndOverlays();
    emitSkimmerStatus(true);
}

void CwDecoder::setSecondaryEnabled(bool enabled)
{
    if (m_secondaryEnabled == enabled) {
        return;
    }
    m_secondaryEnabled = enabled;
    // Disabling freezes a clean receiver for the next activation.  Enabling
    // must not reset it again: a preceding tone selection has already done so,
    // and a receiver disabled without a tone change was cleaned here when it
    // was switched off.
    if (!enabled && m_toneTrackerB) {
        m_toneTrackerB->reset();
    }
    updateTrackerInterferers();
    m_selectedCommittedB.clear();
    m_haveDiagnosticB = false;
    m_pendingDiagnosticsB.clear();
    emit diagnosticResetRequested(1);
    refreshPriorityAndOverlays();
    emitSkimmerStatus(true);
}

void CwDecoder::setWpm(double wpm)
{
    setReceiverWpm(0, wpm);
    setReceiverWpm(1, wpm);
}

void CwDecoder::setAutoWpm(bool enabled)
{
    setReceiverAutoWpm(0, enabled);
    setReceiverAutoWpm(1, enabled);
}

void CwDecoder::setReceiverWpm(int rank, double wpm)
{
    const double bounded = qBound(5.0, wpm, 50.0);
    if (rank <= 0) {
        if (std::abs(m_wpmA - bounded) < 1.0e-9) return;
        m_wpmA = bounded;
        if (m_toneTrackerA) m_toneTrackerA->setWpmHint(m_wpmA);
        m_pendingDiagnosticsA.clear();
        m_haveDiagnosticA = false;
        emit diagnosticResetRequested(0);
        if (!m_autoWpmA) {
            m_trackedWpm = m_wpmA;
            emit receiverSpeedEstimateChanged(0, m_wpmA);
            emit speedEstimateChanged(m_trackedWpm);
        }
        return;
    }
    if (rank == 1) {
        if (std::abs(m_wpmB - bounded) < 1.0e-9) return;
        m_wpmB = bounded;
        if (m_toneTrackerB) m_toneTrackerB->setWpmHint(m_wpmB);
        m_pendingDiagnosticsB.clear();
        m_haveDiagnosticB = false;
        emit diagnosticResetRequested(1);
        if (!m_autoWpmB) {
            emit receiverSpeedEstimateChanged(1, m_wpmB);
        }
    }
}

void CwDecoder::setReceiverAutoWpm(int rank, bool enabled)
{
    if (rank <= 0) {
        if (m_autoWpmA == enabled) return;
        m_autoWpmA = enabled;
        if (m_toneTrackerA) m_toneTrackerA->setAutoWpm(enabled);
        m_pendingDiagnosticsA.clear();
        m_haveDiagnosticA = false;
        emit diagnosticResetRequested(0);
        if (!m_autoWpmA) {
            m_trackedWpm = m_wpmA;
            emit receiverSpeedEstimateChanged(0, m_wpmA);
            emit speedEstimateChanged(m_trackedWpm);
        }
        return;
    }
    if (rank == 1) {
        if (m_autoWpmB == enabled) return;
        m_autoWpmB = enabled;
        if (m_toneTrackerB) m_toneTrackerB->setAutoWpm(enabled);
        m_pendingDiagnosticsB.clear();
        m_haveDiagnosticB = false;
        emit diagnosticResetRequested(1);
        if (!m_autoWpmB) {
            emit receiverSpeedEstimateChanged(1, m_wpmB);
        }
    }
}

void CwDecoder::setBandwidthHz(double bandwidthHz)
{
    const double bounded = qBound(30.0, bandwidthHz, 500.0);
    if (std::abs(m_bandwidthHz - bounded) < 1.0e-9) return;
    m_bandwidthHz = bounded;
    if (m_toneTrackerA) {
        m_toneTrackerA->setBandwidthHz(m_bandwidthHz);
    }
    if (m_toneTrackerB) {
        m_toneTrackerB->setBandwidthHz(m_bandwidthHz);
    }
    m_pendingDiagnosticsA.clear();
    m_pendingDiagnosticsB.clear();
    m_haveDiagnosticA = false;
    m_haveDiagnosticB = false;
    emit diagnosticResetRequested(0);
    emit diagnosticResetRequested(1);
}

void CwDecoder::setAutoBandwidth(bool enabled)
{
    if (m_autoBandwidth == enabled) return;
    m_autoBandwidth = enabled;
    if (m_toneTrackerA) m_toneTrackerA->setAutoBandwidth(enabled);
    if (m_toneTrackerB) m_toneTrackerB->setAutoBandwidth(enabled);
    m_pendingDiagnosticsA.clear();
    m_pendingDiagnosticsB.clear();
    m_haveDiagnosticA = false;
    m_haveDiagnosticB = false;
    emit diagnosticResetRequested(0);
    emit diagnosticResetRequested(1);
}

void CwDecoder::setAfcEnabled(bool enabled)
{
    m_afcEnabled = enabled;
    if (m_toneTrackerA) m_toneTrackerA->setAfcEnabled(enabled);
    if (m_toneTrackerB) m_toneTrackerB->setAfcEnabled(enabled);
    emitSkimmerStatus(true);
}

void CwDecoder::setAfcRangeHz(double rangeHz)
{
    m_afcRangeHz = qBound(5.0, rangeHz, 250.0);
    if (m_toneTrackerA) m_toneTrackerA->setAfcRangeHz(m_afcRangeHz);
    if (m_toneTrackerB) m_toneTrackerB->setAfcRangeHz(m_afcRangeHz);
}

double CwDecoder::toneHz() const
{
    return m_toneHz;
}

double CwDecoder::wpm() const
{
    return m_wpmA;
}

bool CwDecoder::autoWpm() const
{
    return m_autoWpmA;
}

double CwDecoder::receiverWpm(int rank) const
{
    return rank <= 0 ? m_wpmA : m_wpmB;
}

bool CwDecoder::receiverAutoWpm(int rank) const
{
    return rank <= 0 ? m_autoWpmA : m_autoWpmB;
}

double CwDecoder::trackedWpm() const
{
    return qBound(5.0, m_autoWpmA ? m_trackedWpm : m_wpmA, 50.0);
}

double CwDecoder::bandwidthHz() const
{
    return m_bandwidthHz;
}

double CwDecoder::trackedToneHz(int rank) const
{
    const auto *tracker = rank <= 0 ? m_toneTrackerA.get() : m_toneTrackerB.get();
    if (tracker != nullptr) {
        return tracker->trackedToneHz();
    }
    return rank <= 0 ? m_toneHz : m_secondaryToneHz;
}

double CwDecoder::effectiveBandwidthHz(int rank) const
{
    const auto *tracker = rank <= 0 ? m_toneTrackerA.get() : m_toneTrackerB.get();
    return tracker != nullptr ? tracker->effectiveBandwidthHz() : m_bandwidthHz;
}

double CwDecoder::acquisitionBandwidthHz(int rank) const
{
    const auto *tracker = rank <= 0 ? m_toneTrackerA.get() : m_toneTrackerB.get();
    return tracker != nullptr ? tracker->acquisitionBandwidthHz() : 0.0;
}

QString CwDecoder::trackingState(int rank) const
{
    const auto *tracker = rank <= 0 ? m_toneTrackerA.get() : m_toneTrackerB.get();
    return tracker != nullptr ? QString::fromStdString(tracker->trackingState())
                              : QStringLiteral("ACQUIRE");
}

QString CwDecoder::receivedText() const
{
    return m_text;
}

QString CwDecoder::sanitizeSkimmerText(const std::string &text) const
{
    QString out = QString::fromStdString(text);
    out.remove(QChar('\a'));
    out.replace(QChar('\r'), QChar('\n'));
    while (out.contains(QStringLiteral("\n\n\n"))) {
        out.replace(QStringLiteral("\n\n\n"), QStringLiteral("\n\n"));
    }
    return out;
}

void CwDecoder::refreshPriorityAndOverlays()
{
    QStringList labels;
    QVector<double> frequencies;
    QVector<float> confidences;

    // The waterfall displays only persistent carrier lanes. Decoded letters
    // belong exclusively to the continuous RX A/RX B text panes.
    if (m_skimmer) {
        const auto lanes = m_skimmer->priorityChannels(12);
        for (const auto &lane : lanes) {
            if (lane.confidence < 0.20f || lane.audioFrequencyHz < 100.0 ||
                lane.audioFrequencyHz > 3500.0) {
                continue;
            }
            labels.append(QStringLiteral("CW %1 Hz").arg(qRound(lane.audioFrequencyHz)));
            frequencies.append(lane.audioFrequencyHz);
            confidences.append(lane.confidence);
        }
    }

    emit skimmerOverlaysChanged(labels, frequencies, confidences);
}

void CwDecoder::emitSkimmerStatus(bool force)
{
    Q_UNUSED(force);
    QStringList parts;
    if (m_toneTrackerA) {
        const CwDiagnosticPoint *snapshot = m_haveDiagnosticA ? &m_lastDiagnosticA : nullptr;
        parts << QStringLiteral("A:%1Hz %2 BW%3/%4 %5dB %6WPM %7%")
                     .arg(qRound(snapshot ? snapshot->trackedHz : m_toneTrackerA->trackedToneHz()))
                     .arg(QString::fromStdString(m_toneTrackerA->trackingState()))
                     .arg(qRound(snapshot ? snapshot->effectiveBandwidthHz
                                          : m_toneTrackerA->effectiveBandwidthHz()))
                     .arg(qRound(m_bandwidthHz))
                     .arg(snapshot ? snapshot->snrDb : m_toneTrackerA->snrDb(), 0, 'f', 1)
                     .arg(snapshot ? snapshot->wpm : m_toneTrackerA->wpm(), 0, 'f', 1)
                     .arg(qRound(100.0 * (snapshot ? snapshot->lockQuality
                                                  : m_toneTrackerA->confidence())));
    } else {
        parts << QStringLiteral("A:%1Hz").arg(qRound(m_toneHz));
    }
    if (m_secondaryEnabled) {
        if (m_toneTrackerB) {
            const CwDiagnosticPoint *snapshot = m_haveDiagnosticB ? &m_lastDiagnosticB : nullptr;
            parts << QStringLiteral("B:%1Hz %2 BW%3/%4 %5dB %6WPM %7%")
                         .arg(qRound(snapshot ? snapshot->trackedHz : m_toneTrackerB->trackedToneHz()))
                         .arg(QString::fromStdString(m_toneTrackerB->trackingState()))
                         .arg(qRound(snapshot ? snapshot->effectiveBandwidthHz
                                              : m_toneTrackerB->effectiveBandwidthHz()))
                         .arg(qRound(m_bandwidthHz))
                         .arg(snapshot ? snapshot->snrDb : m_toneTrackerB->snrDb(), 0, 'f', 1)
                         .arg(snapshot ? snapshot->wpm : m_toneTrackerB->wpm(), 0, 'f', 1)
                         .arg(qRound(100.0 * (snapshot ? snapshot->lockQuality
                                                      : m_toneTrackerB->confidence())));
        } else {
            parts << QStringLiteral("B:%1Hz").arg(qRound(m_secondaryToneHz));
        }
    }
    emit statusChanged(QStringLiteral("CW native RX: %1 · carrier scanner active")
                           .arg(parts.join(QStringLiteral(" · "))));
}
