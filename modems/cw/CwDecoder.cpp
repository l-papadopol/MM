#include "CwDecoder.h"

#include "skimmer/CwSkimmerEngine.h"

#include <QColor>
#include <QtGlobal>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace {

constexpr int kSkimmerChannels = madmodem::cwskimmer::kSkimmerChannels;
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
    madmodem::cwskimmer::CwSkimmerConfig cfg;
    cfg.thresholdMultiplier = 9.0f;
    cfg.minEventSnrDb = 9.0f;
    cfg.maxRollingText = 96;
    cfg.emitEmptyPartials = false;

    m_skimmer = std::make_unique<madmodem::cwskimmer::CwSkimmerEngine>(cfg);
    m_lastRollingByChannel = QVector<QString>(kSkimmerChannels);
    m_priorityChannels.clear();
    m_priorityChannels.reserve(2);

    m_skimmer->setCallback([this](const madmodem::cwskimmer::CwSkimmerEvent &event) {
        if (event.channelIndex < 0 || event.channelIndex >= kSkimmerChannels) {
            return;
        }

        refreshPriorityAndOverlays();

        const QString committed = sanitizeSkimmerText(event.committedText);
        if (committed.isEmpty()) {
            return;
        }

        const int rank = priorityRankForChannel(event.channelIndex);
        if (rank < 0 || rank > 1) {
            return;
        }

        m_text += committed;
        emit priorityTextReceived(rank, committed);
        emit textUpdated(m_text);

        if (event.wpm > 0.1f) {
            m_trackedWpm = event.wpm;
            emit speedEstimateChanged(m_trackedWpm);
        }
        emitSkimmerStatus(false);
    });
}

void CwDecoder::reset()
{
    m_sampleCounter = 0;
    m_statusCounter = 0;
    m_sampleRate = 0;
    m_text.clear();
    m_trackedWpm = qBound(5.0, m_wpm, 80.0);
    resetSkimmer();

    emit textUpdated(m_text);
    emit markersChanged(frequencyMarkers(m_toneHz));
    emit speedEstimateChanged(m_trackedWpm);
    emitSkimmerStatus(true);
    refreshPriorityAndOverlays();
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
    m_toneHz = qBound(kMinUiToneHz, toneHz, kMaxUiToneHz);
    refreshPriorityAndOverlays();
    emit markersChanged(frequencyMarkers(m_toneHz));
    emitSkimmerStatus(true);
}

void CwDecoder::setSecondaryToneHz(double toneHz)
{
    m_secondaryToneHz = qBound(kMinUiToneHz, toneHz, kMaxUiToneHz);
    refreshPriorityAndOverlays();
    emitSkimmerStatus(true);
}

void CwDecoder::setSecondaryEnabled(bool enabled)
{
    m_secondaryEnabled = enabled;
    refreshPriorityAndOverlays();
    emitSkimmerStatus(true);
}

void CwDecoder::setWpm(double wpm)
{
    m_wpm = qBound(5.0, wpm, 80.0);
    if (!m_autoWpm) {
        m_trackedWpm = m_wpm;
        emit speedEstimateChanged(m_trackedWpm);
    }
}

void CwDecoder::setAutoWpm(bool enabled)
{
    m_autoWpm = enabled;
    if (!m_autoWpm) {
        m_trackedWpm = m_wpm;
        emit speedEstimateChanged(m_trackedWpm);
    }
}

void CwDecoder::setBandwidthHz(double bandwidthHz)
{
    // Retained for settings compatibility.  The new skimmer does not apply a
    // selected-tone pre-filter; it adapts threshold/noise per FFT bin instead.
    m_bandwidthHz = qBound(30.0, bandwidthHz, 500.0);
}

void CwDecoder::setAfcEnabled(bool enabled)
{
    // Retained for settings compatibility.  AFC is intentionally bypassed for
    // the skimmer engine: all six FFT channels are tracked continuously.
    m_afcEnabled = enabled;
    emitSkimmerStatus(true);
}

void CwDecoder::setAfcRangeHz(double rangeHz)
{
    m_afcRangeHz = qBound(5.0, rangeHz, 250.0);
}

double CwDecoder::toneHz() const
{
    return m_toneHz;
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
    return qBound(5.0, m_autoWpm ? m_trackedWpm : m_wpm, 80.0);
}

double CwDecoder::bandwidthHz() const
{
    return m_bandwidthHz;
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
    if (!m_skimmer) {
        return;
    }

    m_priorityChannels.clear();
    const int channelA = channelForFrequency(m_toneHz);
    if (channelA >= 0) {
        m_priorityChannels.append(channelA);
    }
    const int channelB = channelForFrequency(m_secondaryToneHz);
    if (m_secondaryEnabled && channelB >= 0 && channelB != channelA) {
        m_priorityChannels.append(channelB);
    }

    QStringList labels;
    QVector<double> frequencies;
    QVector<float> confidences;

    const auto states = m_skimmer->channelStates();
    for (const auto &st : states) {
        QString rolling = QString::fromStdString(st.rollingText).simplified();
        if (rolling.size() > 56) {
            rolling = QStringLiteral("…") + rolling.right(55);
        }

        // Do not draw the raw FFT channel grid.  Waterfall OSD is only for
        // actual decoded text, not for internal channel activity or SNR-only
        // energy.  This prevents the flickering internal-channel labels seen
        // in the first skimmer UI attempt.
        if (rolling.size() < 2) {
            continue;
        }

        const int rank = priorityRankForChannel(st.channelIndex);
        const bool priority = (rank == 0 || rank == 1);
        const bool plausible = priority || st.confidence > 0.12f || st.snrDb > 8.0f;
        if (!plausible) {
            continue;
        }

        QString label;
        if (rank == 0) {
            label = QStringLiteral("A %1Hz %2").arg(qRound(st.audioFrequencyHz)).arg(rolling);
        } else if (rank == 1) {
            label = QStringLiteral("B %1Hz %2").arg(qRound(st.audioFrequencyHz)).arg(rolling);
        } else {
            label = QStringLiteral("%1Hz %2").arg(qRound(st.audioFrequencyHz)).arg(rolling);
        }

        labels.append(label.trimmed());
        frequencies.append(st.audioFrequencyHz);
        confidences.append(st.confidence);
    }

    emit skimmerOverlaysChanged(labels, frequencies, confidences);
}

int CwDecoder::channelForFrequency(double frequencyHz) const
{
    if (frequencyHz <= 0.0) {
        return -1;
    }
    const double channelWidthHz = static_cast<double>(madmodem::cwskimmer::kChannelBins) *
                                  madmodem::cwskimmer::kBinHz;
    if (channelWidthHz <= 0.0) {
        return -1;
    }
    const int channel = static_cast<int>(std::floor(frequencyHz / channelWidthHz));
    return qBound(0, channel, kSkimmerChannels - 1);
}

int CwDecoder::priorityRankForChannel(int channelIndex) const
{
    const int channelA = channelForFrequency(m_toneHz);
    if (channelIndex == channelA) {
        return 0;
    }

    const int channelB = channelForFrequency(m_secondaryToneHz);
    if (m_secondaryEnabled && channelIndex == channelB && channelB != channelA) {
        return 1;
    }

    return -1;
}

void CwDecoder::emitSkimmerStatus(bool force)
{
    if (!m_skimmer) {
        return;
    }

    QStringList parts;
    const auto states = m_skimmer->channelStates();
    const int channelA = channelForFrequency(m_toneHz);
    const int channelB = channelForFrequency(m_secondaryToneHz);

    for (const auto &st : states) {
        const int rank = priorityRankForChannel(st.channelIndex);
        if (rank < 0) {
            continue;
        }
        const QString prefix = (rank == 0) ? QStringLiteral("A") : QStringLiteral("B");
        parts << QStringLiteral("%1:%2Hz %3dB %4WPM conf%5")
                     .arg(prefix)
                     .arg(qRound(st.audioFrequencyHz))
                     .arg(st.snrDb, 0, 'f', 1)
                     .arg(st.wpm, 0, 'f', 1)
                     .arg(st.confidence, 0, 'f', 2);
    }

    if (parts.isEmpty()) {
        parts << QStringLiteral("A:%1Hz ch%2")
                     .arg(qRound(m_toneHz))
                     .arg(channelA + 1);
        if (m_secondaryEnabled) {
            parts << QStringLiteral("B:%1Hz ch%2")
                         .arg(qRound(m_secondaryToneHz))
                         .arg(channelB + 1);
        }
    }

    if (!force && parts.isEmpty()) {
        return;
    }

    emit statusChanged(QStringLiteral("CW skimmer: %1").arg(parts.join(QStringLiteral(" · "))));
}
