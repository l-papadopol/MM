#include "Msk144Decoder.h"

#include <QtGlobal>
#include <QtMath>
#include <QPointer>
#include <QMetaObject>
#include "../../third_party/mshv_gpl/port/HvGenMsk/genmesage_msk.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <set>
#include <utility>
#include <thread>

namespace {
constexpr int kInternalRate = 12000;
constexpr int kFrameSamples = 864;       // 144 symbols at 2000 baud, 12 kHz internal sample rate
constexpr int kSymbols = 144;
constexpr double kTwoPi = 6.28318530717958647692;
const std::array<int, 8> kMsk144Sync{{0, 1, 1, 1, 0, 0, 1, 0}};

inline int syncPolarity(int bit)
{
    return bit ? 1 : -1;
}

QString normalizeDecodedMessage(QString msg)
{
    msg = msg.trimmed();
    msg.replace('\t', ' ');
    while (msg.contains(QStringLiteral("  "))) msg.replace(QStringLiteral("  "), QStringLiteral(" "));
    return msg;
}
}

Msk144Decoder::Msk144Decoder(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<Msk144Decode>("Msk144Decode");
    reset();
}

void Msk144Decoder::setPeriodSeconds(int seconds)
{
    const int bounded = (seconds == 30) ? 30 : 15;
    if (m_periodSeconds == bounded) {
        return;
    }
    m_periodSeconds = bounded;
    reset();
}

void Msk144Decoder::setDecodeDepth(int depth)
{
    m_decodeDepth = qBound(1, depth, 3);
}

void Msk144Decoder::setRxFrequencyHz(int hz)
{
    m_rxFrequencyHz = qBound(300, hz, 2700);
}

void Msk144Decoder::setDfToleranceHz(int hz)
{
    m_dfToleranceHz = qBound(10, hz, 500);
}

void Msk144Decoder::setShortMessagesEnabled(bool enabled)
{
    m_shortMessages = enabled;
}

void Msk144Decoder::setSwlEnabled(bool enabled)
{
    m_swl = enabled;
}

void Msk144Decoder::setContestModeEnabled(bool enabled)
{
    m_contest = enabled;
}

void Msk144Decoder::setMyCall(const QString &call)
{
    m_myCall = call.trimmed().toUpper();
}

void Msk144Decoder::setDxCall(const QString &call)
{
    m_dxCall = call.trimmed().toUpper();
}

void Msk144Decoder::setMindIntegrationState(bool hardBypass,
                                            bool scoringEnabled,
                                            bool sampleExportEnabled,
                                            bool assistedRankingEnabled)
{
    m_mindHardBypass = hardBypass;
    m_mindScoringEnabled = !hardBypass && scoringEnabled;
    m_mindSampleExportEnabled = !hardBypass && sampleExportEnabled;
    m_mindAssistedRankingEnabled = !hardBypass && assistedRankingEnabled;
}

void Msk144Decoder::setMindScoreCallback(const std::function<bool(const QVector<float> &, double *, bool *)> &callback)
{
    m_mindScoreCallback = callback;
}

void Msk144Decoder::setMindSampleCallback(const std::function<void(const QVector<float> &, bool, const QString &)> &callback)
{
    m_mindSampleCallback = callback;
}

void Msk144Decoder::reset()
{
    m_samples12k.clear();
    m_totalInputSamples = 0;
    m_total12kSamples = 0;
    m_nextPingAnalysisSample = 0;
    m_periodStartUtc = QDateTime::currentDateTimeUtc();
    m_lastStatus.clear();
    emit statusChanged(backendStatusText());
}

QString Msk144Decoder::backendStatusText() const
{
    const QString depth = (m_decodeDepth <= 1) ? QStringLiteral("Fast") : (m_decodeDepth == 2 ? QStringLiteral("Normal") : QStringLiteral("Deep"));
    return QStringLiteral("MSK144 RX: %1 s period, %2, RX %3 Hz, DF tol ±%4 Hz%5%6%7")
        .arg(m_periodSeconds)
        .arg(depth)
        .arg(m_rxFrequencyHz)
        .arg(m_dfToleranceHz)
        .arg(m_shortMessages ? QStringLiteral(", Sh") : QString())
        .arg(m_swl ? QStringLiteral(", SWL") : QString())
        .arg(m_contest ? QStringLiteral(", contest") : QString());
}

void Msk144Decoder::processAudioBlock(const AudioBlock &block)
{
    if (block.samples.isEmpty() || block.sampleRate <= 0) {
        return;
    }
    m_inputSampleRate = block.sampleRate;
    appendResampledTo12k(block);
    analyzeRecentPingWindow();

    const int periodSamples = m_periodSeconds * kInternalRate;
    if (m_samples12k.size() >= periodSamples) {
        tryPeriodDecode(false);
        const int keep = qMin(kInternalRate, m_samples12k.size()); // one second overlap for pings across edge
        QVector<float> tail;
        tail.reserve(keep);
        for (int i = m_samples12k.size() - keep; i < m_samples12k.size(); ++i) {
            if (i >= 0) tail.append(m_samples12k.at(i));
        }
        m_samples12k.swap(tail);
        m_periodStartUtc = QDateTime::currentDateTimeUtc();
        m_nextPingAnalysisSample = qMin<qint64>(m_nextPingAnalysisSample, m_samples12k.size());
    }
}

void Msk144Decoder::flushPeriod()
{
    tryPeriodDecode(true);
}

void Msk144Decoder::appendResampledTo12k(const AudioBlock &block)
{
    const int inRate = block.sampleRate;
    if (inRate == kInternalRate) {
        m_samples12k += block.samples;
        m_total12kSamples += block.samples.size();
        m_totalInputSamples += block.samples.size();
        return;
    }

    const double ratio = static_cast<double>(kInternalRate) / static_cast<double>(inRate);
    const int outCount = qMax(1, qRound(static_cast<double>(block.samples.size()) * ratio));
    const int oldSize = m_samples12k.size();
    m_samples12k.resize(oldSize + outCount);

    for (int i = 0; i < outCount; ++i) {
        const double src = static_cast<double>(i) / ratio;
        const int i0 = qBound(0, static_cast<int>(std::floor(src)), block.samples.size() - 1);
        const int i1 = qBound(0, i0 + 1, block.samples.size() - 1);
        const double frac = src - static_cast<double>(i0);
        const double v = (1.0 - frac) * block.samples.at(i0) + frac * block.samples.at(i1);
        m_samples12k[oldSize + i] = static_cast<float>(qBound(-1.0, v, 1.0));
    }
    m_total12kSamples += outCount;
    m_totalInputSamples += block.samples.size();
}

double Msk144Decoder::bandEnergyGoertzel(const QVector<float> &samples, int start, int count, double frequencyHz) const
{
    if (count <= 16 || start < 0 || start + count > samples.size()) {
        return 0.0;
    }
    const double omega = kTwoPi * frequencyHz / static_cast<double>(kInternalRate);
    const double coeff = 2.0 * std::cos(omega);
    double q0 = 0.0;
    double q1 = 0.0;
    double q2 = 0.0;
    for (int i = 0; i < count; ++i) {
        q0 = coeff * q1 - q2 + static_cast<double>(samples.at(start + i));
        q2 = q1;
        q1 = q0;
    }
    return q1 * q1 + q2 * q2 - coeff * q1 * q2;
}

void Msk144Decoder::analyzeRecentPingWindow()
{
    const int win = kInternalRate / 5; // 200 ms, enough to catch meteor pings visually
    const int hop = kInternalRate / 10; // 100 ms
    while (m_nextPingAnalysisSample + win <= m_samples12k.size()) {
        const int start = static_cast<int>(m_nextPingAnalysisSample);
        double broadband = 1e-12;
        for (int i = 0; i < win; ++i) {
            const double v = m_samples12k.at(start + i);
            broadband += v * v;
        }
        const double e1500 = bandEnergyGoertzel(m_samples12k, start, win, 1500.0);
        const double e1000 = bandEnergyGoertzel(m_samples12k, start, win, 1000.0);
        const double e2000 = bandEnergyGoertzel(m_samples12k, start, win, 2000.0);
        const double metric = (e1000 + e1500 + e2000) / broadband;
        const int snrLike = qBound(-20, qRound(10.0 * std::log10(qMax(1e-9, metric)) - 6.0), 40);
        if (snrLike >= 0) {
            const double t = static_cast<double>(start + win / 2) / static_cast<double>(kInternalRate);
            emit pingDetected(1500.0, snrLike, t);
        }
        m_nextPingAnalysisSample += hop;
    }
}

void Msk144Decoder::tryPeriodDecode(bool force)
{
    const int secondsBuffered = m_samples12k.size() / kInternalRate;
    if (!force && secondsBuffered < m_periodSeconds) {
        return;
    }

    if (m_asyncDecodeEnabled) {
        emit periodReady(secondsBuffered, m_periodSeconds);
        if (m_samples12k.size() < kFrameSamples) {
            return;
        }
        bool expected = false;
        if (!m_decodeInProgress.compare_exchange_strong(expected, true)) {
            const QString status = QStringLiteral("MSK144 decode skipped: previous period still decoding in worker thread.");
            if (status != m_lastStatus) {
                m_lastStatus = status;
                emit statusChanged(status);
            }
            return;
        }

        const QVector<float> samples = m_samples12k;
        const QDateTime periodStartUtc = m_periodStartUtc;
        const int periodSeconds = m_periodSeconds;
        const int decodeDepth = m_decodeDepth;
        const int rxFrequencyHz = m_rxFrequencyHz;
        const int dfToleranceHz = m_dfToleranceHz;
        const bool shortMessages = m_shortMessages;
        const bool swl = m_swl;
        const bool contest = m_contest;
        const QString myCall = m_myCall;
        const QString dxCall = m_dxCall;
        const bool mindHardBypass = m_mindHardBypass;
        const bool mindScoringEnabled = m_mindScoringEnabled;
        const bool mindSampleExportEnabled = m_mindSampleExportEnabled;
        const bool mindAssistedRankingEnabled = m_mindAssistedRankingEnabled;
        const auto mindScoreCallback = m_mindScoreCallback;
        const auto mindSampleCallback = m_mindSampleCallback;
        QPointer<Msk144Decoder> self(this);

        std::thread([self, samples, periodStartUtc, periodSeconds, decodeDepth, rxFrequencyHz, dfToleranceHz,
                     shortMessages, swl, contest, myCall, dxCall, mindHardBypass, mindScoringEnabled,
                     mindSampleExportEnabled, mindAssistedRankingEnabled, mindScoreCallback, mindSampleCallback]() mutable {
            struct Result {
                QVector<Msk144Decode> decodes;
                QString status;
                bool hasMindStats = false;
                int scored = 0;
                int promoted = 0;
                double avgConfidence = 0.0;
            } result;

            Msk144Decoder worker;
            worker.m_asyncDecodeEnabled = false;
            worker.m_samples12k = samples;
            worker.m_periodStartUtc = periodStartUtc;
            worker.m_periodSeconds = periodSeconds;
            worker.m_decodeDepth = decodeDepth;
            worker.m_rxFrequencyHz = rxFrequencyHz;
            worker.m_dfToleranceHz = dfToleranceHz;
            worker.m_shortMessages = shortMessages;
            worker.m_swl = swl;
            worker.m_contest = contest;
            worker.m_myCall = myCall;
            worker.m_dxCall = dxCall;
            worker.m_mindHardBypass = mindHardBypass;
            worker.m_mindScoringEnabled = mindScoringEnabled;
            worker.m_mindSampleExportEnabled = mindSampleExportEnabled;
            worker.m_mindAssistedRankingEnabled = mindAssistedRankingEnabled;
            worker.m_mindScoreCallback = mindScoreCallback;
            worker.m_mindSampleCallback = mindSampleCallback;

            QObject::connect(&worker, &Msk144Decoder::decoded, [&result](const Msk144Decode &decode) {
                result.decodes.append(decode);
            });
            QObject::connect(&worker, &Msk144Decoder::statusChanged, [&result](const QString &status) {
                result.status = status;
            });
            QObject::connect(&worker, &Msk144Decoder::mindStats, [&result](int scored, int promoted, double avg) {
                result.hasMindStats = true;
                result.scored = scored;
                result.promoted = promoted;
                result.avgConfidence = avg;
            });

            worker.tryPeriodDecodeSync(true);

            if (self) {
                QMetaObject::invokeMethod(self.data(), [self, result]() mutable {
                    if (!self) return;
                    for (const Msk144Decode &decode : result.decodes) {
                        emit self->decoded(decode);
                    }
                    if (!result.status.isEmpty()) {
                        self->m_lastStatus = result.status;
                        emit self->statusChanged(result.status);
                    }
                    if (result.hasMindStats) {
                        emit self->mindStats(result.scored, result.promoted, result.avgConfidence);
                    }
                    self->m_decodeInProgress.store(false);
                }, Qt::QueuedConnection);
            }
        }).detach();
        return;
    }

    tryPeriodDecodeSync(force);
}

void Msk144Decoder::tryPeriodDecodeSync(bool force)
{
    const int secondsBuffered = m_samples12k.size() / kInternalRate;
    if (!force && secondsBuffered < m_periodSeconds) {
        return;
    }
    emit periodReady(secondsBuffered, m_periodSeconds);

    const int n = m_samples12k.size();
    if (n < kFrameSamples) {
        return;
    }

    const int timeStep = (m_decodeDepth <= 1) ? 12 : (m_decodeDepth == 2 ? 6 : 3);
    const int freqStep = (m_decodeDepth <= 1) ? 25 : (m_decodeDepth == 2 ? 10 : 5);
    const int tol = qBound(10, m_dfToleranceHz, 500);
    const int f0 = qBound(300, m_rxFrequencyHz - tol, 2700);
    const int f1 = qBound(300, m_rxFrequencyHz + tol, 2700);

    struct Candidate
    {
        int start = 0;
        int frequencyHz = 1500;
        double mindConfidence = 0.0;
        bool mindPromoted = false;
        QVector<float> mindFeatures;
    };

    QVector<Candidate> candidates;
    const int maxCandidates = 12000;
    candidates.reserve(qMin(maxCandidates, ((n - kFrameSamples) / qMax(1, timeStep) + 1) * ((f1 - f0) / qMax(1, freqStep) + 1)));

    int mindScored = 0;
    int mindPromoted = 0;
    double mindConfidenceSum = 0.0;

    /*
     * This is a self-contained MSK144 coherent frame search around the selected
     * RX frequency.  MIND never validates a message: it only ranks candidate
     * time/DF chunks before the normal sync + LDPC + unpack path is attempted.
     */
    for (int start = 0; start + kFrameSamples <= n; start += timeStep) {
        for (int f = f0; f <= f1; f += freqStep) {
            Candidate c;
            c.start = start;
            c.frequencyHz = f;
            if (m_mindScoringEnabled && m_mindScoreCallback) {
                c.mindFeatures = makeMindCandidateFeatures(start, static_cast<double>(f));
                bool mayPromote = false;
                double confidence = 0.0;
                if (m_mindScoreCallback(c.mindFeatures, &confidence, &mayPromote)) {
                    c.mindConfidence = confidence;
                    c.mindPromoted = mayPromote;
                    ++mindScored;
                    if (mayPromote) ++mindPromoted;
                    mindConfidenceSum += confidence;
                }
            }
            candidates.append(std::move(c));
            if (candidates.size() >= maxCandidates) break;
        }
        if (candidates.size() >= maxCandidates) break;
    }

    if (m_mindAssistedRankingEnabled && mindScored > 0) {
        std::stable_sort(candidates.begin(), candidates.end(), [](const Candidate &a, const Candidate &b) {
            if (a.mindPromoted != b.mindPromoted) return a.mindPromoted > b.mindPromoted;
            if (!qFuzzyCompare(a.mindConfidence + 1.0, b.mindConfidence + 1.0)) return a.mindConfidence > b.mindConfidence;
            if (a.start != b.start) return a.start < b.start;
            return a.frequencyHz < b.frequencyHz;
        });
    }

    int attempts = 0;
    int decodes = 0;
    std::set<QString> seen;
    Msk144Decode d;
    for (const Candidate &c : candidates) {
        ++attempts;
        const bool ok = tryDecodeCoherentAt(c.start, static_cast<double>(c.frequencyHz), d);
        if (m_mindSampleExportEnabled && m_mindSampleCallback) {
            const bool exportNegative = !ok && ((c.start / qMax(1, timeStep) + c.frequencyHz / qMax(1, freqStep)) % 16 == 0);
            if (ok || exportNegative) {
                const QVector<float> features = c.mindFeatures.isEmpty()
                    ? makeMindCandidateFeatures(c.start, static_cast<double>(c.frequencyHz))
                    : c.mindFeatures;
                m_mindSampleCallback(features, ok, ok ? d.message : QStringLiteral("NEG"));
            }
        }
        if (!ok) {
            continue;
        }
        const QString key = d.message + QStringLiteral("@");
        if (seen.insert(key).second) {
            ++decodes;
            emit decoded(d);
        }
    }

    if (mindScored > 0) {
        emit mindStats(mindScored, mindPromoted, mindConfidenceSum / static_cast<double>(mindScored));
    }

    const QString status = QStringLiteral("MSK144 period decoded: %1 s, %2 attempts, %3 message(s)%4%5")
                               .arg(secondsBuffered)
                               .arg(attempts)
                               .arg(decodes)
                               .arg(m_shortMessages ? QStringLiteral("; short msgs on") : QString())
                               .arg(mindScored > 0 ? QStringLiteral("; MIND %1").arg(mindScored) : QString());
    if (status != m_lastStatus) {
        m_lastStatus = status;
        emit statusChanged(status);
    }
}

void Msk144Decoder::makeBasebandFrame(int startSample, double frequencyHz, QVector<std::complex<double>> &frame) const
{
    frame.resize(kFrameSamples);
    const double phaseStep = -kTwoPi * frequencyHz / static_cast<double>(kInternalRate);
    double phase = phaseStep * static_cast<double>(startSample);

    QVector<std::complex<double>> mixed(kFrameSamples);
    for (int i = 0; i < kFrameSamples; ++i) {
        const double x = static_cast<double>(m_samples12k.at(startSample + i));
        mixed[i] = std::complex<double>(2.0 * x * std::cos(phase), 2.0 * x * std::sin(phase));
        phase += phaseStep;
    }

    // Small symmetric low-pass smoother after quadrature mixing.  The original
    // MSHV path builds an analytic signal with FFT filtering; this lightweight
    // FIR is sufficient for the bounded, selected-frequency live decoder path.
    for (int i = 0; i < kFrameSamples; ++i) {
        std::complex<double> acc(0.0, 0.0);
        double wsum = 0.0;
        for (int k = -5; k <= 5; ++k) {
            const int j = qBound(0, i + k, kFrameSamples - 1);
            const double w = 1.0 + std::cos(kTwoPi * static_cast<double>(k) / 12.0);
            acc += mixed[j] * w;
            wsum += w;
        }
        frame[i] = (wsum > 0.0) ? (acc / wsum) : mixed[i];
    }
}

QVector<float> Msk144Decoder::makeMindCandidateFeatures(int startSample, double frequencyHz) const
{
    constexpr int kFeatureCount = 464;
    QVector<float> features;
    features.resize(kFeatureCount);
    if (startSample < 0 || startSample + kFrameSamples > m_samples12k.size()) {
        std::fill(features.begin(), features.end(), 0.0f);
        return features;
    }

    // 58×8-compatible feature vector, intentionally matching the FT MIND ranker
    // input geometry.  It describes one candidate ping/chunk after complex
    // down-mixing: envelope, local energy, derivative and tone balance.
    QVector<std::complex<double>> bb;
    makeBasebandFrame(startSample, frequencyHz, bb);

    double maxEnv = 1e-9;
    double meanEnv = 0.0;
    QVector<double> env(bb.size());
    for (int i = 0; i < bb.size(); ++i) {
        const double e = std::abs(bb.at(i));
        env[i] = e;
        meanEnv += e;
        maxEnv = qMax(maxEnv, e);
    }
    meanEnv /= qMax(1, bb.size());

    for (int k = 0; k < kFeatureCount; ++k) {
        const int a = (k * kFrameSamples) / kFeatureCount;
        const int b = ((k + 1) * kFrameSamples) / kFeatureCount;
        double sum = 0.0;
        double diff = 0.0;
        for (int i = a; i < b; ++i) {
            sum += env.at(i);
            if (i > a) diff += std::abs(env.at(i) - env.at(i - 1));
        }
        const double count = qMax(1, b - a);
        const double local = (sum / count) / maxEnv;
        const double edge = (diff / count) / maxEnv;
        const double centered = (sum / count - meanEnv) / maxEnv;
        features[k] = static_cast<float>(qBound(0.0, 0.72 * local + 0.18 * edge + 0.10 * (centered + 0.5), 1.0));
    }
    return features;
}



bool Msk144Decoder::tryDecodeCoherentAt(int startSample, double frequencyHz, Msk144Decode &decode) const
{
    // First attempt the best single 72 ms frame.  If that fails, follow the
    // WSJT-X/MSHV MSK144 depth semantics with coherent averages of repeated
    // frames: Normal adds 4-frame averages; Deep adds 4-, 5- and 7-frame
    // averages.  Final validity is still LDPC + unpack; MIND only ranks the
    // time/DF candidates before this point.
    if (tryDecodeFrameAt(startSample, frequencyHz, decode)) {
        decode.navg = 1;
        return true;
    }

    QVector<int> navgs;
    if (m_decodeDepth == 2) {
        navgs = {4};
    } else if (m_decodeDepth >= 3) {
        navgs = {4, 5, 7};
    }

    for (int navg : navgs) {
        const int needed = startSample + navg * kFrameSamples;
        if (needed > m_samples12k.size()) continue;
        QVector<std::complex<double>> avg(kFrameSamples);
        std::fill(avg.begin(), avg.end(), std::complex<double>(0.0, 0.0));
        for (int k = 0; k < navg; ++k) {
            QVector<std::complex<double>> fr;
            makeBasebandFrame(startSample + k * kFrameSamples, frequencyHz, fr);
            for (int i = 0; i < kFrameSamples; ++i) avg[i] += fr.at(i);
        }
        for (std::complex<double> &v : avg) v /= static_cast<double>(navg);

        QString message;
        double quality = 0.0;
        if (!decodeMsk144Frame(avg, message, quality)) continue;
        decode.utc = m_periodStartUtc.addMSecs(static_cast<qint64>(1000.0 * startSample / kInternalRate));
        decode.tSeconds = static_cast<double>(startSample) / static_cast<double>(kInternalRate);
        decode.frequencyHz = frequencyHz;
        decode.dfHz = qRound(frequencyHz - 1500.0);
        decode.snrDb = qBound(-8, qRound(estimateFrameSnrDb(startSample, navg * kFrameSamples) + 10.0 * std::log10(static_cast<double>(navg))), 24);
        decode.message = normalizeDecodedMessage(message);
        decode.navg = navg;
        decode.eye = quality;
        decode.shortMessage = false;
        return !decode.message.isEmpty();
    }
    return false;
}

bool Msk144Decoder::tryDecodeFrameAt(int startSample, double frequencyHz, Msk144Decode &decode) const
{
    if (startSample < 0 || startSample + kFrameSamples > m_samples12k.size()) {
        return false;
    }
    QVector<std::complex<double>> frame;
    makeBasebandFrame(startSample, frequencyHz, frame);
    QString message;
    double quality = 0.0;
    if (!decodeMsk144Frame(frame, message, quality)) {
        return false;
    }

    decode.utc = m_periodStartUtc.addMSecs(static_cast<qint64>(1000.0 * startSample / kInternalRate));
    decode.tSeconds = static_cast<double>(startSample) / static_cast<double>(kInternalRate);
    decode.frequencyHz = frequencyHz;
    decode.dfHz = qRound(frequencyHz - 1500.0);
    decode.snrDb = qBound(-8, qRound(estimateFrameSnrDb(startSample, kFrameSamples)), 24);
    decode.message = normalizeDecodedMessage(message);
    decode.navg = 1;
    decode.eye = quality;
    decode.shortMessage = false;
    return !decode.message.isEmpty();
}

bool Msk144Decoder::decodeMsk144Frame(const QVector<std::complex<double>> &c, QString &message, double &qualityMetric) const
{
    if (c.size() < kFrameSamples) {
        return false;
    }

    std::array<double, kSymbols> soft{};
    std::array<int, kSymbols> hard{};
    std::array<double, 12> pp{};
    for (int i = 0; i < 12; ++i) {
        pp[i] = std::sin(kTwoPi * static_cast<double>(i) / 24.0);
    }

    soft[0] = 0.0;
    soft[1] = 0.0;
    for (int i = 0; i < 6; ++i) {
        soft[0] += c[i].imag() * pp[i + 6];
        soft[0] += c[i + (kFrameSamples - 6)].imag() * pp[i];
    }
    for (int i = 0; i < 12; ++i) {
        soft[1] += c[i].real() * pp[i];
    }

    for (int i = 1; i < 72; ++i) {
        double q = 0.0;
        double in = 0.0;
        const int q0 = i * 12 - 6;
        const int i0 = i * 12;
        for (int j = 0; j < 12; ++j) {
            q += c[q0 + j].imag() * pp[j];
            in += c[i0 + j].real() * pp[j];
        }
        soft[2 * i] = q;
        soft[2 * i + 1] = in;
    }

    for (int i = 0; i < kSymbols; ++i) {
        hard[i] = (soft[i] >= 0.0) ? 1 : 0;
    }

    int syncScore1 = 0;
    int syncScore2 = 0;
    for (int i = 0; i < 8; ++i) {
        syncScore1 += (2 * hard[i] - 1) * syncPolarity(kMsk144Sync[i]);
        syncScore2 += (2 * hard[i + 56] - 1) * syncPolarity(kMsk144Sync[i]);
    }
    const int badSync = (8 - syncScore1) / 2 + (8 - syncScore2) / 2;
    if (badSync > 3) {
        return false;
    }

    double mean = 0.0;
    double mean2 = 0.0;
    for (double v : soft) {
        mean += v;
        mean2 += v * v;
    }
    mean /= static_cast<double>(soft.size());
    mean2 /= static_cast<double>(soft.size());
    double ssig = std::sqrt(qMax(1e-12, mean2 - mean * mean));
    if (ssig <= 0.0) ssig = 1.0;
    for (double &v : soft) v /= ssig;

    double llr[128];
    const double sigma = 0.60;
    int k = 0;
    for (int i = 8; i < 56; ++i) llr[k++] = 2.0 * soft[i] / (sigma * sigma);
    for (int i = 64; i < 144; ++i) llr[k++] = 2.0 * soft[i] / (sigma * sigma);

    GenMsk gen(true);
    bool decoded77[120];
    for (bool &b : decoded77) b = false;
    int hardErrors = -1;
    gen.bpdecode128_90(llr, 10, decoded77, hardErrors);
    if (hardErrors < 0 || hardErrors >= 18) {
        return false;
    }

    int n3 = 4 * static_cast<int>(decoded77[71]) + 2 * static_cast<int>(decoded77[72]) + static_cast<int>(decoded77[73]);
    int i3 = 4 * static_cast<int>(decoded77[74]) + 2 * static_cast<int>(decoded77[75]) + static_cast<int>(decoded77[76]);
    if ((i3 == 0 && (n3 == 1 || n3 == 3 || n3 == 4 || n3 > 5)) || i3 == 3 || i3 > 5) {
        return false;
    }

    bool unpackOk = false;
    message = normalizeDecodedMessage(gen.unpack77(decoded77, unpackOk));
    if (!unpackOk || message.isEmpty()) {
        return false;
    }
    qualityMetric = qMax(0.0, 8.0 - static_cast<double>(badSync)) + qMax(0.0, 18.0 - static_cast<double>(hardErrors)) / 3.0;
    return true;
}

double Msk144Decoder::estimateFrameSnrDb(int startSample, int frameSamples) const
{
    if (m_samples12k.isEmpty() || startSample < 0 || startSample >= m_samples12k.size()) {
        return 0.0;
    }
    const int end = qMin(startSample + frameSamples, m_samples12k.size());
    double framePower = 1e-12;
    for (int i = startSample; i < end; ++i) {
        const double v = m_samples12k.at(i);
        framePower += v * v;
    }
    framePower /= qMax(1, end - startSample);

    double allPower = 1e-12;
    for (float fv : m_samples12k) {
        const double v = static_cast<double>(fv);
        allPower += v * v;
    }
    allPower /= qMax(1, m_samples12k.size());
    const double ratio = qMax(1e-6, framePower / qMax(1e-12, allPower));
    return 10.0 * std::log10(ratio) + 4.0;
}
