#include "RttyMultiDecoder.h"
#include <QHash>
#include <QtMath>
#include <algorithm>
#include <utility>

namespace {
constexpr double kTwoPi = 6.2831853071795864769;
constexpr int kMinFreqHz = 100, kMaxFreqHz = 3500;
constexpr int kScanStepHz = 5, kTrackBucketHz = 35;
constexpr int kTrackHoldMs = 4000, kScanIntervalMs = 350;
constexpr int kScanWindowSamples = 8192;
}

RttyMultiDecoder::RttyMultiDecoder(QObject* parent) : QObject(parent) {
    qRegisterMetaType<QVector<SkimmerSpot>>("QVector<SkimmerSpot>");
    m_resampler.configure(12000);
}
void RttyMultiDecoder::reset() {
    // Owned and used on one worker; delete immediately to bound live decoders
    // even during fast offline input, where deferred deletes may be delayed.
    for (auto& track : m_tracks) delete track.decoder;
    m_tracks.clear(); m_previousCandidates.clear(); m_scanBuffer.clear(); m_resampler.reset();
    m_samplesProcessed = m_samplesUntilScan = m_publishSamples = 0;
    emit skimmerSpotsChanged({});
}
void RttyMultiDecoder::resumeAfterLocalTransmit() { reset(); }
void RttyMultiDecoder::configure(double baud, int shiftHz, bool reverse, bool enabled, int maxDecoders) {
    baud = qBound(10.0, baud, 300.0); shiftHz = qBound(50, shiftHz, 1200);
    maxDecoders = qBound(2, maxDecoders, 16);
    const bool changed = !qFuzzyCompare(m_baud, baud) || m_shiftHz != shiftHz || m_reverse != reverse ||
                         m_enabled != enabled || m_maxDecoders != maxDecoders;
    m_baud = baud; m_shiftHz = shiftHz; m_reverse = reverse; m_enabled = enabled; m_maxDecoders = maxDecoders;
    if (changed) reset();
}
void RttyMultiDecoder::processAudioBlock(const AudioBlock& input) {
    if (!m_enabled || input.samples.isEmpty() || input.sampleRate <= 0) return;
    if (m_inputSampleRate != input.sampleRate) { m_inputSampleRate = input.sampleRate; reset(); }
    const auto reduced = m_resampler.process(input.samples, input.sampleRate);
    AudioBlock block; block.sampleRate = m_sampleRate; block.firstSampleIndex = m_samplesProcessed;
    block.samples.reserve(reduced.size());
    for (double value : reduced) block.samples.append(float(value));
    m_samplesProcessed += block.samples.size();
    for (auto& track : m_tracks) track.decoder->processAudioBlock(block);
    m_scanBuffer += block.samples;
    // A one-second history is replayed only for newly acquired stations.
    if (m_scanBuffer.size() > m_sampleRate) m_scanBuffer.remove(0, m_scanBuffer.size() - m_sampleRate);
    m_samplesUntilScan -= block.samples.size();
    if (m_samplesUntilScan <= 0 && m_scanBuffer.size() >= 2048) {
        AudioBlock scan = block; scan.samples = m_scanBuffer;
        auto candidates = scanCandidates(scan);
        std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) { return a.score > b.score; });
        QVector<Candidate> unique;
        for (const auto& candidate : candidates) {
            bool duplicate = false;
            for (const auto& kept : unique) if (qAbs(candidate.markHz - kept.markHz) < kTrackBucketHz) { duplicate = true; break; }
            if (!duplicate) unique.append(candidate);
            if (unique.size() >= m_maxDecoders) break;
        }
        for (const auto& c : unique) {
            bool established = false;
            for (const auto& t : m_tracks)
                if (qAbs(t.markHz - c.markHz) < kTrackBucketHz) { established = true; break; }
            bool persistent = false;
            for (const auto& previous : m_previousCandidates)
                if (qAbs(previous.markHz - c.markHz) <= 10) { persistent = true; break; }
            if (established || persistent) addOrRefreshTrack(c, m_samplesProcessed);
        }
        m_previousCandidates = unique;
        m_samplesUntilScan = m_sampleRate * kScanIntervalMs / 1000;
    }
    pruneTracks(m_samplesProcessed);
    m_publishSamples += block.samples.size();
    if (m_publishSamples >= m_sampleRate / 5) { m_publishSamples = 0; publishSpots(); }
}

QVector<RttyMultiDecoder::Candidate> RttyMultiDecoder::scanCandidates(const AudioBlock &block) const
{
    QVector<Candidate> candidates;
    if (block.samples.isEmpty() || block.sampleRate <= 0) {
        return candidates;
    }

    const int targetWindow = qMax(1024, qRound(kScanWindowSamples * block.sampleRate / 48000.0));
    const int available = qMin(static_cast<int>(block.samples.size()), targetWindow);
    if (available < 1024) {
        return candidates;
    }

    QVector<float> window;
    window.reserve(available);
    const int start = block.samples.size() - available;
    for (int i = 0; i < available; ++i) {
        window.append(block.samples.at(start + i) * (0.5 - 0.5 * qCos(kTwoPi * i / qMax(1, available - 1))));
    }

    const int stepHz = kScanStepHz;
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
    const double minScore = 10.0;

    auto powerAt = [&](int frequency) {
        if (frequency < kMinFreqHz || frequency > kMaxFreqHz) return 0.0;
        const int lower = kMinFreqHz + ((frequency - kMinFreqHz) / stepHz) * stepHz;
        const double fraction = double(frequency - lower) / stepHz;
        return (1.0 - fraction) * powers.value(lower) + fraction * powers.value(lower + stepHz);
    };
    // Data sidebands can be far above the global noise floor. Require both
    // tones to form a local spectral peak, and compare each with its own
    // neighbourhood; otherwise one station consumes many decoder lanes.
    const int guardHz = qBound(25, qRound(m_baud * 2.5), 140);
    auto prominentPair = [&](int mark, int space, double markPower, double spacePower) {
        for (int delta = -guardHz; delta <= guardHz; delta += stepHz) {
            if (qAbs(delta) < 20) continue;
            // Compare pairs, not isolated peaks: a neighbouring station's
            // single tone must not veto an otherwise distinct valid pair.
            if (powerAt(mark + delta) > markPower * 1.4 &&
                powerAt(space + delta) > spacePower * 1.4) return false;
        }
        return true;
    };
    for (int mark = kMinFreqHz; mark + m_shiftHz <= kMaxFreqHz; mark += stepHz) {
        const int space = mark + m_shiftHz;
        const double markPower = powerAt(mark), spacePower = powerAt(space);
        const double pairEnergy = markPower + spacePower;
        const double balance = qMin(markPower, spacePower) / qMax(qMax(markPower, spacePower), 1.0e-16);
        const double score = pairEnergy / median * (0.35 + 0.65 * balance);
        if (score < minScore || balance < 0.16) continue;
        if (pairEnergy < powerAt(mark - stepHz) + powerAt(space - stepHz) ||
            pairEnergy <= powerAt(mark + stepHz) + powerAt(space + stepHz)) continue;
        if (!prominentPair(mark, space, markPower, spacePower)) continue;
        candidates.append({mark, space, score});
    }

    return candidates;
}


void RttyMultiDecoder::addOrRefreshTrack(const Candidate& candidate, qint64 index) {
    for (auto& track : m_tracks) {
        if (qAbs(track.markHz - candidate.markHz) >= kTrackBucketHz) continue;
        const int offset = qBound(-5, candidate.markHz - track.markHz, 5);
        track.markHz += offset; track.spaceHz = track.markHz + m_shiftHz;
        track.score = candidate.score; track.lastSeenSample = index;
        track.decoder->retuneTones(track.markHz, track.spaceHz);
        return;
    }
    if (m_tracks.size() >= m_maxDecoders) {
        auto weakest = std::min_element(m_tracks.begin(), m_tracks.end(), [](const auto& a, const auto& b) { return a.score < b.score; });
        if (weakest == m_tracks.end() || weakest->score >= candidate.score) return;
        delete weakest->decoder; m_tracks.erase(weakest);
    }
    Track track;
    track.id = m_nextId++; track.markHz = candidate.markHz; track.spaceHz = candidate.spaceHz;
    track.score = candidate.score; track.lastSeenSample = index;
    track.decoder = new RttyDecoder(this);
    track.decoder->setBaudRate(m_baud); track.decoder->setTones(track.markHz, track.spaceHz);
    track.decoder->setReverse(m_reverse); track.decoder->setVisualizationEnabled(false);
    RttyDecoder* decoder = track.decoder;
    connect(decoder, &RttyDecoder::characterReceived, this, [this, decoder](const QString& text) { appendTrackText(decoder, text); });
    m_tracks.append(track);
    AudioBlock history; history.sampleRate = m_sampleRate; history.samples = m_scanBuffer;
    history.firstSampleIndex = m_samplesProcessed - history.samples.size();
    decoder->processAudioBlock(history);
}
void RttyMultiDecoder::pruneTracks(qint64 sampleIndex) {
    const qint64 hold = static_cast<qint64>(m_sampleRate) * kTrackHoldMs / 1000;
    for (int i = m_tracks.size() - 1; i >= 0; --i) {
        if (sampleIndex - m_tracks[i].lastSeenSample > hold) { delete m_tracks[i].decoder; m_tracks.removeAt(i); }
    }
}
void RttyMultiDecoder::appendTrackText(RttyDecoder* decoder, const QString& text) {
    if (text.isEmpty()) return;
    for (auto& track : m_tracks) if (track.decoder == decoder) {
        QString clean;
        for (QChar c : text.toUpper()) {
            if (c.isPrint()) clean += c;
            else if (c.isSpace()) clean += QLatin1Char(' ');
        }
        track.recentText = (track.recentText + clean).right(192);
        track.cq.append(clean, double(m_samplesProcessed) / m_sampleRate);
        return;
    }
}
void RttyMultiDecoder::publishSpots() {
    QVector<SkimmerSpot> result;
    for (auto& track : m_tracks) {
        track.cq.expire(double(m_samplesProcessed) / m_sampleRate);
        SkimmerSpot spot; spot.id = track.id;
        spot.frequencyHz = (track.markHz + track.spaceHz) * .5;
        spot.lowerToneHz = track.markHz; spot.upperToneHz = track.spaceHz;
        spot.text = track.recentText; spot.cqCall = track.cq.call(); result.append(spot);
    }
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.frequencyHz < b.frequencyHz; });
    emit skimmerSpotsChanged(result);
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
        s0 = static_cast<double>(samples.at(i)) + (coeff * s1) - s2;
        s2 = s1;
        s1 = s0;
    }

    return (s1 * s1) + (s2 * s2) - (coeff * s1 * s2);
}
