#include "RttyMultiDecoder.h"

#include <QHash>
#include <QElapsedTimer>
#include <QPair>
#include <QRegularExpression>
#include <QtMath>
#include <algorithm>
#include <utility>

namespace {
constexpr double kTwoPi = 2.0 * M_PI;
constexpr int kMinFreqHz = 300;
constexpr int kMaxFreqHz = 3000;
constexpr int kScanStepHz = 10;
constexpr int kEnhancedScanStepHz = 5;
constexpr int kTrackBucketHz = 18;
constexpr int kTrackHoldMs = 12000;
constexpr int kScanIntervalMs = 650;
constexpr int kFastScanWindowSamples = 4096;
constexpr int kEnhancedScanWindowSamples = 8192;

bool bandSuppressed(int markHz, int spaceHz, const QVector<QPair<int, int>> &bands)
{
    for (const QPair<int, int> &band : bands) {
        const int center = (band.first + band.second) / 2;
        if (qAbs(markHz - center) <= 95 || qAbs(spaceHz - center) <= 95) {
            return true;
        }
    }
    return false;
}

QString normalizedTail(QString text)
{
    text = text.toUpper();
    text.replace(QRegularExpression(QStringLiteral("[^A-Z0-9/ ]+")), QStringLiteral(" "));
    text.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    if (text.size() > 420) {
        text = text.right(420);
    }
    return text.trimmed();
}

} // namespace

RttyMultiDecoder::RttyMultiDecoder(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<RttyMultiDecoder::Callout>("RttyMultiDecoder::Callout");
    qRegisterMetaType<QVector<RttyMultiDecoder::Callout>>("QVector<RttyMultiDecoder::Callout>");
}

void RttyMultiDecoder::reset()
{
    for (Track &track : m_tracks) {
        if (track.decoder != nullptr) {
            track.decoder->deleteLater();
            track.decoder = nullptr;
        }
    }
    m_tracks.clear();
    m_callouts.clear();
    m_samplesProcessed = 0;
    m_samplesUntilScan = 0;
    emit calloutsChanged(m_callouts);
}

void RttyMultiDecoder::configure(double baud,
                                 int shiftHz,
                                 bool reverse,
                                 bool enabled,
                                 bool overlayEnabled,
                                 bool contestEnhanced,
                                 bool secondPass,
                                 int maxDecoders)
{
    const double clampedBaud = qBound(10.0, baud, 300.0);
    const int clampedShift = qBound(50, shiftHz, 1200);
    const int clampedMax = qBound(2, maxDecoders, 32);

    const bool changedTones = !qFuzzyCompare(m_baud, clampedBaud) ||
                              m_shiftHz != clampedShift ||
                              m_reverse != reverse;

    m_baud = clampedBaud;
    m_shiftHz = clampedShift;
    m_reverse = reverse;
    m_enabled = enabled;
    m_overlayEnabled = overlayEnabled;
    m_contestEnhanced = contestEnhanced;
    m_secondPass = secondPass;
    m_maxDecoders = clampedMax;

    if (!m_enabled || changedTones) {
        reset();
        return;
    }

    rebuildCallouts();
}

QVector<RttyMultiDecoder::Callout> RttyMultiDecoder::callouts() const
{
    return m_callouts;
}

void RttyMultiDecoder::processAudioBlock(const AudioBlock &block)
{
    if (!m_enabled || block.samples.isEmpty() || block.sampleRate <= 0) {
        return;
    }

    if (m_sampleRate != block.sampleRate) {
        m_sampleRate = block.sampleRate;
        reset();
    }

    for (Track &track : m_tracks) {
        if (track.decoder != nullptr) {
            track.decoder->processAudioBlock(block);
        }
    }

    m_samplesProcessed += block.samples.size();
    if (m_samplesUntilScan > 0) {
        m_samplesUntilScan -= block.samples.size();
        if (m_samplesUntilScan > 0) {
            pruneTracks(m_samplesProcessed);
            return;
        }
    }

    QElapsedTimer scanTimer;
    scanTimer.start();
    QVector<Candidate> candidates = scanCandidates(block);

    const bool allowSecondPass = m_secondPass && m_lastScanMs < 120;
    if (allowSecondPass && !candidates.isEmpty()) {
        QVector<QPair<int, int>> strongBands;
        const int guardCount = qMin(6, candidates.size());
        strongBands.reserve(guardCount);
        for (int i = 0; i < guardCount; ++i) {
            strongBands.append(qMakePair(candidates.at(i).markHz, candidates.at(i).spaceHz));
        }
        QVector<Candidate> second = scanCandidates(block, strongBands);
        for (const Candidate &candidate : second) {
            candidates.append(candidate);
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const Candidate &a, const Candidate &b) {
        return a.score > b.score;
    });

    QVector<Candidate> unique;
    for (const Candidate &candidate : std::as_const(candidates)) {
        bool duplicate = false;
        for (const Candidate &kept : std::as_const(unique)) {
            if (qAbs(candidate.markHz - kept.markHz) < kTrackBucketHz ||
                qAbs(candidate.spaceHz - kept.spaceHz) < kTrackBucketHz) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            unique.append(candidate);
        }
        if (unique.size() >= m_maxDecoders) {
            break;
        }
    }

    updateTracks(unique, block);
    pruneTracks(m_samplesProcessed);
    rebuildCallouts();

    m_lastScanMs = static_cast<int>(scanTimer.elapsed());
    int intervalMs = kScanIntervalMs;
    if (m_lastScanMs > 140) {
        // v3.25: automatic CPU limiter for contest/multi-decode.  If the
        // shadow-decoder scanner is eating too much wall time, scan less often
        // rather than starving UI/audio threads.  The selected main RTTY
        // decoder is never throttled by this monitor.
        intervalMs = qMin(1600, kScanIntervalMs + (m_lastScanMs * 4));
        if (m_secondPass) {
            emit statusChanged(QStringLiteral("RTTY multi-decode CPU limiter: scan %1 ms, delaying next scan and skipping second-pass until load drops").arg(m_lastScanMs));
        }
    }
    const int interval = qMax(2048, (m_sampleRate * intervalMs) / 1000);
    m_samplesUntilScan = interval;
}

QVector<RttyMultiDecoder::Candidate> RttyMultiDecoder::scanCandidates(const AudioBlock &block,
                                                                      const QVector<QPair<int, int>> &suppressedBands) const
{
    QVector<Candidate> candidates;
    if (block.samples.isEmpty() || block.sampleRate <= 0) {
        return candidates;
    }

    const int targetWindow = m_contestEnhanced ? kEnhancedScanWindowSamples : kFastScanWindowSamples;
    const int available = qMin(block.samples.size(), targetWindow);
    if (available < 1024) {
        return candidates;
    }

    QVector<float> window;
    window.reserve(available);
    const int start = block.samples.size() - available;
    for (int i = 0; i < available; ++i) {
        window.append(block.samples.at(start + i));
    }

    const int stepHz = m_contestEnhanced ? kEnhancedScanStepHz : kScanStepHz;
    QHash<int, double> powers;
    QVector<double> noiseSamples;
    for (int f = kMinFreqHz; f <= kMaxFreqHz; f += stepHz) {
        const double power = goertzelPower(window, block.sampleRate, static_cast<double>(f));
        powers.insert(f, power);
        noiseSamples.append(power);
    }

    if (noiseSamples.isEmpty()) {
        return candidates;
    }
    std::sort(noiseSamples.begin(), noiseSamples.end());
    const double median = qMax(noiseSamples.at(noiseSamples.size() / 2), 1.0e-16);
    const double minScore = m_contestEnhanced ? 7.5 : 10.0;

    for (int mark = kMinFreqHz; mark + m_shiftHz <= kMaxFreqHz; mark += stepHz) {
        const int space = mark + m_shiftHz;
        if (bandSuppressed(mark, space, suppressedBands)) {
            continue;
        }
        const double markPower = powers.value(mark, 0.0);
        const double spacePower = powers.value(space, 0.0);
        const double pairEnergy = markPower + spacePower;
        const double ratio = pairEnergy / median;
        const double balance = qMin(markPower, spacePower) / qMax(qMax(markPower, spacePower), 1.0e-16);
        const double score = ratio * (0.35 + 0.65 * balance);
        if (score < minScore || balance < (m_contestEnhanced ? 0.11 : 0.16)) {
            continue;
        }

        Candidate candidate;
        candidate.markHz = mark;
        candidate.spaceHz = space;
        candidate.energy = pairEnergy;
        candidate.score = score;
        candidates.append(candidate);
    }

    return candidates;
}

void RttyMultiDecoder::updateTracks(const QVector<Candidate> &candidates, const AudioBlock &block)
{
    Q_UNUSED(block)
    for (const Candidate &candidate : candidates) {
        addOrRefreshTrack(candidate, m_samplesProcessed);
    }
}

void RttyMultiDecoder::addOrRefreshTrack(const Candidate &candidate, qint64 sampleIndex)
{
    for (Track &track : m_tracks) {
        if (qAbs(track.markHz - candidate.markHz) < kTrackBucketHz) {
            track.markHz = candidate.markHz;
            track.spaceHz = candidate.spaceHz;
            track.score = candidate.score;
            track.lastSeenSample = sampleIndex;
            if (track.decoder != nullptr) {
                track.decoder->retuneTones(candidate.markHz, candidate.spaceHz);
            }
            return;
        }
    }

    if (m_tracks.size() >= m_maxDecoders) {
        auto weakest = std::min_element(m_tracks.begin(), m_tracks.end(), [](const Track &a, const Track &b) {
            return a.score < b.score;
        });
        if (weakest == m_tracks.end() || weakest->score >= candidate.score) {
            return;
        }
        if (weakest->decoder != nullptr) {
            weakest->decoder->deleteLater();
        }
        m_tracks.erase(weakest);
    }

    Track track;
    track.decoder = new RttyDecoder(this);
    track.markHz = candidate.markHz;
    track.spaceHz = candidate.spaceHz;
    track.score = candidate.score;
    track.lastSeenSample = sampleIndex;
    track.decoder->setBaudRate(m_baud);
    track.decoder->setTones(candidate.markHz, candidate.spaceHz);
    track.decoder->setReverse(m_reverse);

    RttyDecoder *decoder = track.decoder;
    connect(decoder, &RttyDecoder::characterReceived,
            this, [this, decoder](const QString &text) { appendTrackText(decoder, text); });

    m_tracks.append(track);
}

void RttyMultiDecoder::pruneTracks(qint64 sampleIndex)
{
    if (m_sampleRate <= 0) {
        return;
    }

    const qint64 holdSamples = static_cast<qint64>(m_sampleRate) * kTrackHoldMs / 1000;
    for (int i = m_tracks.size() - 1; i >= 0; --i) {
        if (sampleIndex - m_tracks.at(i).lastSeenSample <= holdSamples) {
            continue;
        }
        if (m_tracks[i].decoder != nullptr) {
            m_tracks[i].decoder->deleteLater();
        }
        m_tracks.removeAt(i);
    }
}

void RttyMultiDecoder::appendTrackText(RttyDecoder *decoder, const QString &text)
{
    if (decoder == nullptr || text.isEmpty()) {
        return;
    }

    for (Track &track : m_tracks) {
        if (track.decoder != decoder) {
            continue;
        }
        track.recentText.append(text.toUpper());
        if (track.recentText.size() > 520) {
            track.recentText.remove(0, track.recentText.size() - 520);
        }
        bool cq = false;
        const QString label = extractBestLabel(track.recentText, &cq);
        if (!label.isEmpty()) {
            track.label = label;
            track.cq = cq;
            rebuildCallouts();
        }
        return;
    }
}

void RttyMultiDecoder::rebuildCallouts()
{
    QVector<Callout> next;
    if (m_overlayEnabled) {
        for (const Track &track : std::as_const(m_tracks)) {
            if (track.label.isEmpty()) {
                continue;
            }
            Callout callout;
            callout.markHz = track.markHz;
            callout.spaceHz = track.spaceHz;
            callout.label = track.label;
            callout.score = track.score;
            callout.cq = track.cq;
            next.append(callout);
        }
    }

    std::sort(next.begin(), next.end(), [](const Callout &a, const Callout &b) {
        if (a.cq != b.cq) {
            return a.cq;
        }
        return a.score > b.score;
    });

    if (next.size() > m_maxDecoders) {
        next.resize(m_maxDecoders);
    }

    m_callouts = next;
    emit calloutsChanged(m_callouts);
}

QString RttyMultiDecoder::extractBestLabel(const QString &text, bool *cqOut)
{
    if (cqOut != nullptr) {
        *cqOut = false;
    }

    const QString tail = normalizedTail(text);
    if (tail.isEmpty()) {
        return QString();
    }

    const QStringList words = tail.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (int i = qMax(0, words.size() - 18); i < words.size(); ++i) {
        const QString w = words.value(i);
        if (w != QStringLiteral("CQ") && w != QStringLiteral("QRZ")) {
            continue;
        }
        for (int j = i + 1; j < qMin(words.size(), i + 5); ++j) {
            const QString candidate = words.value(j);
            if (candidate == QStringLiteral("TEST") || candidate == QStringLiteral("DX") ||
                candidate == QStringLiteral("DE") || candidate == QStringLiteral("CQ")) {
                continue;
            }
            if (looksLikeCallsign(candidate)) {
                if (cqOut != nullptr) {
                    *cqOut = true;
                }
                return QStringLiteral("CQ %1").arg(candidate);
            }
        }
    }

    for (int i = qMax(0, words.size() - 18); i < words.size(); ++i) {
        if (words.value(i) != QStringLiteral("DE")) {
            continue;
        }
        const QString candidate = words.value(i + 1);
        if (looksLikeCallsign(candidate)) {
            return candidate;
        }
    }

    for (int i = words.size() - 1; i >= qMax(0, words.size() - 10); --i) {
        const QString candidate = words.value(i);
        if (looksLikeCallsign(candidate)) {
            return candidate;
        }
    }

    return QString();
}

bool RttyMultiDecoder::looksLikeCallsign(const QString &token)
{
    QString call = token.trimmed().toUpper();
    if (call.size() < 3 || call.size() > 12) {
        return false;
    }
    if (call.contains(QStringLiteral("/"))) {
        const QStringList parts = call.split(QLatin1Char('/'), Qt::SkipEmptyParts);
        for (const QString &part : parts) {
            if (part.contains(QRegularExpression(QStringLiteral("[0-9]"))) && part.contains(QRegularExpression(QStringLiteral("[A-Z]")))) {
                call = part;
                break;
            }
        }
    }

    static const QRegularExpression re(QStringLiteral("^[A-Z0-9]{1,3}[0-9][A-Z]{1,4}$"));
    return re.match(call).hasMatch();
}

double RttyMultiDecoder::goertzelPower(const QVector<float> &samples, int sampleRate, double frequencyHz)
{
    if (samples.isEmpty() || sampleRate <= 0 || frequencyHz <= 0.0) {
        return 0.0;
    }

    const double omega = kTwoPi * frequencyHz / static_cast<double>(sampleRate);
    const double coeff = 2.0 * qCos(omega);
    double s0 = 0.0;
    double s1 = 0.0;
    double s2 = 0.0;
    const int n = samples.size();
    for (int i = 0; i < n; ++i) {
        const double w = 0.5 - (0.5 * qCos(kTwoPi * static_cast<double>(i) / qMax(1, n - 1)));
        s0 = (w * static_cast<double>(samples.at(i))) + (coeff * s1) - s2;
        s2 = s1;
        s1 = s0;
    }

    return (s1 * s1) + (s2 * s2) - (coeff * s1 * s2);
}

