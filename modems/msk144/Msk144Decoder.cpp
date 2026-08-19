#include "Msk144Decoder.h"
#include "../weak_signal/WeakSignalCodecLock.h"

#include <QtGlobal>
#include <QtMath>
#include <QPointer>
#include <QMetaObject>
#include "../../third_party/mshv_gpl/port/HvGenMsk/genmesage_msk.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <numeric>
#include <set>
#include <mutex>
#include <utility>
#include <thread>

namespace {
constexpr int kInternalRate = 12000;
constexpr int kFrameSamples = 864;       // 144 symbols at 2000 baud, 12 kHz internal sample rate
constexpr int kShortFrameSamples = 240;  // 40-symbol hashed short message
constexpr int kSymbols = 144;
constexpr double kTwoPi = 6.28318530717958647692;
const std::array<int, 8> kMsk144Sync{{0, 1, 1, 1, 0, 0, 1, 0}};
const std::array<int, 8> kMsk40Sync{{1, 0, 1, 1, 0, 0, 0, 1}};
const std::array<const char *, 16> kMsk40Reports{{
    "-03", "+00", "+03", "+06", "+10", "+13", "+16", "R-03",
    "R+00", "R+03", "R+06", "R+10", "R+13", "R+16", "RRR", "73"
}};

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

QVector<std::complex<double>> matchedMskSymbols(
    const QVector<std::complex<double>> &baseband,
    int symbols)
{
    QVector<std::complex<double>> matched(symbols, std::complex<double>(0.0, 0.0));
    if (symbols <= 0 || baseband.size() < symbols * 6) return matched;

    std::array<double, 12> pulse{};
    for (int i = 0; i < 12; ++i) {
        pulse[i] = std::sin(kTwoPi * static_cast<double>(i) / 24.0);
    }

    for (int i = 0; i < 6; ++i) {
        matched[0] += baseband.at(i) * pulse[i + 6];
        matched[0] += baseband.at(baseband.size() - 6 + i) * pulse[i];
    }
    for (int i = 0; i < 12; ++i) matched[1] += baseband.at(i) * pulse[i];

    for (int pair = 1; pair < symbols / 2; ++pair) {
        const int quadratureStart = pair * 12 - 6;
        const int inPhaseStart = pair * 12;
        for (int i = 0; i < 12; ++i) {
            matched[2 * pair] += baseband.at(quadratureStart + i) * pulse[i];
            matched[2 * pair + 1] += baseband.at(inPhaseStart + i) * pulse[i];
        }
    }
    return matched;
}

double softValue(const std::complex<double> &matched, int symbol, double phase)
{
    const std::complex<double> rotated = matched *
        std::complex<double>(std::cos(phase), -std::sin(phase));
    return (symbol & 1) ? rotated.real() : rotated.imag();
}

struct SyncEstimate
{
    double phase = 0.0;
    double quality = 0.0;
};

SyncEstimate estimateSyncPhase(const QVector<std::complex<double>> &matched,
                               bool shortFrame)
{
    double cosineCoefficient = 0.0;
    double sineCoefficient = 0.0;
    double magnitudeSum = 0.0;

    auto accumulate = [&](int symbol, int expectedBit) {
        if (symbol < 0 || symbol >= matched.size()) return;
        const std::complex<double> value = matched.at(symbol);
        const double sign = static_cast<double>(syncPolarity(expectedBit));
        if (symbol & 1) {
            // I symbols: Re(z exp(-j phi)) = Re(z) cos(phi) + Im(z) sin(phi).
            cosineCoefficient += sign * value.real();
            sineCoefficient += sign * value.imag();
        } else {
            // Q symbols: Im(z exp(-j phi)) = Im(z) cos(phi) - Re(z) sin(phi).
            cosineCoefficient += sign * value.imag();
            sineCoefficient -= sign * value.real();
        }
        magnitudeSum += std::abs(value);
    };

    for (int i = 0; i < 8; ++i) {
        accumulate(i, shortFrame ? kMsk40Sync[i] : kMsk144Sync[i]);
        if (!shortFrame) accumulate(56 + i, kMsk144Sync[i]);
    }

    SyncEstimate estimate;
    estimate.phase = std::atan2(sineCoefficient, cosineCoefficient);
    estimate.quality = std::hypot(cosineCoefficient, sineCoefficient) /
                       qMax(1e-12, magnitudeSum);
    return estimate;
}
}

Msk144Decoder::Msk144Decoder(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<Msk144Decode>("Msk144Decode");
    m_resampler.configure(kInternalRate);
    reset();
}

Msk144Decoder::~Msk144Decoder()
{
    m_decodeGeneration.fetch_add(1, std::memory_order_acq_rel);
    if (m_decodeThread.joinable()) {
        m_decodeThread.join();
    }
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
    // MSK144 uses tones at centre ±500 Hz.  Keep both tones inside the
    // positive audio passband used by the native real-sample demodulator.
    m_rxFrequencyHz = qBound(600, hz, 2700);
}

void Msk144Decoder::setFrequencyToleranceHz(int hz)
{
    m_frequencyToleranceHz = qBound(50, hz, 500);
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

QVector<Msk144Decode> Msk144Decoder::decodeRecordedPeriod(const QVector<float> &samples12k,
                                                          const QDateTime &periodStartUtc)
{
    QVector<Msk144Decode> results;
    if (samples12k.size() < kShortFrameSamples) return results;
    Msk144Decoder worker;
    worker.m_asyncDecodeEnabled = false;
    worker.m_samples12k = samples12k;
    worker.m_periodStartUtc = periodStartUtc.isValid() ? periodStartUtc.toUTC()
                                                       : QDateTime::currentDateTimeUtc();
    worker.m_periodSeconds = m_periodSeconds;
    worker.m_decodeDepth = m_decodeDepth;
    worker.m_rxFrequencyHz = m_rxFrequencyHz;
    worker.m_frequencyToleranceHz = m_frequencyToleranceHz;
    worker.m_shortMessages = m_shortMessages;
    worker.m_swl = m_swl;
    worker.m_contest = m_contest;
    worker.m_myCall = m_myCall;
    worker.m_dxCall = m_dxCall;
    QObject::connect(&worker, &Msk144Decoder::decoded,
                     [&results](const Msk144Decode &decode) { results.append(decode); });
    worker.tryPeriodDecodeSync(true);
    return results;
}

void Msk144Decoder::reset()
{
    m_decodeGeneration.fetch_add(1, std::memory_order_acq_rel);
    m_samples12k.clear();
    m_resampler.reset();
    m_totalInputSamples = 0;
    m_total12kSamples = 0;
    m_nextPingAnalysisSample = 0;
    m_periodStartUtc = QDateTime();
    m_currentPeriodId = -1;
    m_nextOutputUtcNs = 0;
    m_outputTimeRemainder = 0;
    m_lastInputEndUtcNs = 0;
    m_captureGeneration = 0;
    m_periodTimelineValid = false;
    m_lastStatus.clear();
    emit statusChanged(backendStatusText());
}

QString Msk144Decoder::backendStatusText() const
{
    const QString depth = (m_decodeDepth <= 1) ? QStringLiteral("Fast") : (m_decodeDepth == 2 ? QStringLiteral("Normal") : QStringLiteral("Deep"));
    return QStringLiteral("MSK144 RX: %1 s period, %2, RX %3 Hz, F Tol ±%4 Hz%5%6%7")
        .arg(m_periodSeconds)
        .arg(depth)
        .arg(m_rxFrequencyHz)
        .arg(m_frequencyToleranceHz)
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
}

void Msk144Decoder::flushPeriod()
{
    finishUtcPeriod(true);
}

void Msk144Decoder::appendResampledTo12k(const AudioBlock &block)
{
    qint64 blockStartUtcNs = block.firstSampleUtcNs;
    if (blockStartUtcNs <= 0) {
        const qint64 durationNs = (static_cast<qint64>(block.samples.size()) * 1000000000LL) /
                                 qMax(1, block.sampleRate);
        blockStartUtcNs = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch() * 1000000LL - durationNs;
    }

    const bool generationChanged = block.captureGeneration != 0 &&
                                   m_captureGeneration != 0 &&
                                   block.captureGeneration != m_captureGeneration;
    const qint64 inputSampleNs = 1000000000LL / qMax(1, block.sampleRate);
    const bool timestampJump = m_lastInputEndUtcNs > 0 &&
                               qAbs(blockStartUtcNs - m_lastInputEndUtcNs) > qMax<qint64>(qint64{5000000}, inputSampleNs * qint64{4});
    if (generationChanged || timestampJump) {
        m_resampler.reset();
        m_samples12k.clear();
        m_currentPeriodId = -1;
        m_nextOutputUtcNs = 0;
        m_outputTimeRemainder = 0;
        m_periodTimelineValid = false;
        m_nextPingAnalysisSample = 0;
    }
    if (block.captureGeneration != 0) {
        m_captureGeneration = block.captureGeneration;
    }

    const QVector<double> resampled = m_resampler.process(block.samples, block.sampleRate);
    m_totalInputSamples += block.samples.size();
    m_lastInputEndUtcNs = blockStartUtcNs +
        (static_cast<qint64>(block.samples.size()) * 1000000000LL) / qMax(1, block.sampleRate);
    if (resampled.isEmpty()) {
        return;
    }

    if (m_nextOutputUtcNs <= 0) {
        m_nextOutputUtcNs = blockStartUtcNs;
        m_outputTimeRemainder = 0;
    }

    const qint64 periodNs = static_cast<qint64>(m_periodSeconds) * 1000000000LL;
    for (double value : resampled) {
        const qint64 periodId = m_nextOutputUtcNs / periodNs;
        if (m_currentPeriodId != periodId) {
            if (m_currentPeriodId >= 0) {
                finishUtcPeriod(false);
            }
            beginUtcPeriod(periodId, m_nextOutputUtcNs);
        }
        m_samples12k.append(static_cast<float>(qBound(-1.0, value, 1.0)));
        ++m_total12kSamples;
        m_outputTimeRemainder += 1000000000LL;
        m_nextOutputUtcNs += m_outputTimeRemainder / kInternalRate;
        m_outputTimeRemainder %= kInternalRate;
    }
    analyzeRecentPingWindow();
}

void Msk144Decoder::beginUtcPeriod(qint64 periodId, qint64 firstSampleUtcNs)
{
    m_currentPeriodId = periodId;
    const qint64 periodNs = static_cast<qint64>(m_periodSeconds) * 1000000000LL;
    const qint64 periodStartNs = periodId * periodNs;
    m_periodStartUtc = QDateTime::fromMSecsSinceEpoch(periodStartNs / 1000000LL, Qt::UTC);
    m_samples12k.clear();
    m_nextPingAnalysisSample = 0;

    const qint64 offsetNs = qMax<qint64>(qint64{0}, firstSampleUtcNs - periodStartNs);
    const int missingSamples = static_cast<int>(qMin<qint64>(
        static_cast<qint64>(m_periodSeconds * kInternalRate),
        (offsetNs * kInternalRate) / 1000000000LL));
    m_periodTimelineValid = missingSamples <= kInternalRate / 20; // at most 50 ms startup tolerance
    if (missingSamples > 0) {
        m_samples12k.fill(0.0f, missingSamples);
        m_nextPingAnalysisSample = missingSamples;
    }
}

void Msk144Decoder::finishUtcPeriod(bool force)
{
    if (m_currentPeriodId < 0 || m_samples12k.isEmpty()) {
        return;
    }
    const int periodSamples = m_periodSeconds * kInternalRate;
    if (!force && (!m_periodTimelineValid || m_samples12k.size() < periodSamples - 2)) {
        emit statusChanged(QStringLiteral("MSK144 period skipped: incomplete or discontinuous UTC audio window."));
        return;
    }
    if (m_samples12k.size() > periodSamples) {
        m_samples12k.resize(periodSamples);
    }
    tryPeriodDecode(force);
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
        const int tolerance = qBound(50, m_frequencyToleranceHz, 500);
        const int firstCenter = qBound(600, m_rxFrequencyHz - tolerance, 2700);
        const int lastCenter = qBound(600, m_rxFrequencyHz + tolerance, 2700);
        double bestEnergy = 0.0;
        int bestCenter = m_rxFrequencyHz;
        for (int center = firstCenter; center <= lastCenter; center += 25) {
            const double energy =
                bandEnergyGoertzel(m_samples12k, start, win, center - 500.0) +
                bandEnergyGoertzel(m_samples12k, start, win, center + 500.0);
            if (energy > bestEnergy) {
                bestEnergy = energy;
                bestCenter = center;
            }
        }
        const double metric = bestEnergy / broadband;
        const int snrLike = qBound(-20, qRound(10.0 * std::log10(qMax(1e-9, metric)) - 6.0), 40);
        if (snrLike >= 0) {
            const double t = static_cast<double>(start + win / 2) / static_cast<double>(kInternalRate);
            emit pingDetected(static_cast<double>(bestCenter), snrLike, t);
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
        const int frequencyToleranceHz = m_frequencyToleranceHz;
        const bool shortMessages = m_shortMessages;
        const bool swl = m_swl;
        const bool contest = m_contest;
        const QString myCall = m_myCall;
        const QString dxCall = m_dxCall;
        const quint64 generation = m_decodeGeneration.load(std::memory_order_acquire);
        if (m_decodeThread.joinable()) {
            m_decodeThread.join();
        }

        m_decodeThread = std::thread([this, generation, samples, periodStartUtc, periodSeconds, decodeDepth, rxFrequencyHz, frequencyToleranceHz,
                     shortMessages, swl, contest, myCall, dxCall]() mutable {
            struct Result {
                QVector<Msk144Decode> decodes;
                QString status;
            } result;

            Msk144Decoder worker;
            worker.m_asyncDecodeEnabled = false;
            worker.m_samples12k = samples;
            worker.m_periodStartUtc = periodStartUtc;
            worker.m_periodSeconds = periodSeconds;
            worker.m_decodeDepth = decodeDepth;
            worker.m_rxFrequencyHz = rxFrequencyHz;
            worker.m_frequencyToleranceHz = frequencyToleranceHz;
            worker.m_shortMessages = shortMessages;
            worker.m_swl = swl;
            worker.m_contest = contest;
            worker.m_myCall = myCall;
            worker.m_dxCall = dxCall;

            QObject::connect(&worker, &Msk144Decoder::decoded, [&result](const Msk144Decode &decode) {
                result.decodes.append(decode);
            });
            QObject::connect(&worker, &Msk144Decoder::statusChanged, [&result](const QString &status) {
                result.status = status;
            });
            worker.tryPeriodDecodeSync(true);

            m_decodeInProgress.store(false, std::memory_order_release);
            if (generation == m_decodeGeneration.load(std::memory_order_acquire)) {
                QPointer<Msk144Decoder> self(this);
                QMetaObject::invokeMethod(this, [self, generation, result]() mutable {
                    if (!self || generation != self->m_decodeGeneration.load(std::memory_order_acquire)) return;
                    for (const Msk144Decode &decode : result.decodes) {
                        emit self->decoded(decode);
                    }
                    if (!result.status.isEmpty()) {
                        self->m_lastStatus = result.status;
                        emit self->statusChanged(result.status);
                    }
                }, Qt::QueuedConnection);
            }
        });
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
    const int tol = qBound(50, m_frequencyToleranceHz, 500);
    const int f0 = qBound(600, m_rxFrequencyHz - tol, 2700);
    const int f1 = qBound(600, m_rxFrequencyHz + tol, 2700);

    struct Candidate
    {
        int start = 0;
        int frequencyHz = 1500;
        double score = 0.0;
        bool shortFrame = false;
    };

    QVector<Candidate> candidates;
    const int maxCandidates = (m_decodeDepth <= 1) ? 48 : (m_decodeDepth == 2 ? 96 : 160);
    const int regions = (m_decodeDepth <= 1) ? 24 : (m_decodeDepth == 2 ? 32 : 64);
    const int coarseStep = 72; // 6 ms: shorter than one 72 ms MSK144 frame
    const int regionSize = qMax(kFrameSamples, (n + regions - 1) / regions);
    QVector<double> powerPrefix(n + 1, 0.0);
    for (int i = 0; i < n; ++i) {
        const double value = static_cast<double>(m_samples12k.at(i));
        powerPrefix[i + 1] = powerPrefix.at(i) + value * value;
    }

    // Each UTC-period region contributes its strongest independent windows.
    // Within each window we first estimate the two-tone centre, then scan one
    // complete cyclic MSK144 phase with the known sync words.  Candidate
    // budgets therefore cover the whole period without spending LDPC work on
    // arbitrary high-energy offsets.
    for (int region = 0; region < regions; ++region) {
        const int begin = region * regionSize;
        const int end = qMin(n - kFrameSamples, (region + 1) * regionSize - 1);
        if (begin > end) break;

        QVector<Candidate> windowScores;
        for (int start = begin; start <= end; start += coarseStep) {
            const double energy = powerPrefix.at(start + kFrameSamples) - powerPrefix.at(start);
            windowScores.append(Candidate{start, m_rxFrequencyHz, energy, false});
        }
        std::sort(windowScores.begin(), windowScores.end(), [](const Candidate &a, const Candidate &b) {
            return a.score > b.score;
        });

        QVector<int> energeticStarts;
        for (const Candidate &window : std::as_const(windowScores)) {
            bool separated = true;
            for (int selected : std::as_const(energeticStarts)) {
                if (qAbs(selected - window.start) < kFrameSamples) {
                    separated = false;
                    break;
                }
            }
            if (separated) energeticStarts.append(window.start);
            if (energeticStarts.size() >= 2) break;
        }

        for (int coarseStart : std::as_const(energeticStarts)) {
            QVector<Candidate> frequencyScores;
            for (int f = f0; f <= f1; f += freqStep) {
                const double low = bandEnergyGoertzel(m_samples12k, coarseStart, kFrameSamples,
                                                       qMax(100.0, f - 500.0));
                const double high = bandEnergyGoertzel(m_samples12k, coarseStart, kFrameSamples,
                                                        qMin(5900.0, f + 500.0));
                frequencyScores.append(Candidate{coarseStart, f, low + high, false});
            }
            std::sort(frequencyScores.begin(), frequencyScores.end(), [](const Candidate &a, const Candidate &b) {
                return a.score > b.score;
            });

            QVector<Candidate> selectedFrequencies;
            for (const Candidate &frequency : std::as_const(frequencyScores)) {
                bool separated = true;
                for (const Candidate &selected : std::as_const(selectedFrequencies)) {
                    if (qAbs(selected.frequencyHz - frequency.frequencyHz) < 2 * freqStep) {
                        separated = false;
                        break;
                    }
                }
                if (separated) selectedFrequencies.append(frequency);
                if (selectedFrequencies.size() >= 3) break;
            }

            for (const Candidate &frequency : std::as_const(selectedFrequencies)) {
                Candidate refinedFrequency = frequency;
                refinedFrequency.score = -1.0;
                const int fineFrequencyStep = qMax(1, freqStep / 5);
                for (int fineFrequency = frequency.frequencyHz - freqStep;
                     fineFrequency <= frequency.frequencyHz + freqStep;
                     fineFrequency += fineFrequencyStep) {
                    if (fineFrequency < f0 || fineFrequency > f1) continue;
                    const double low = bandEnergyGoertzel(m_samples12k, coarseStart,
                                                          kFrameSamples,
                                                          fineFrequency - 500.0);
                    const double high = bandEnergyGoertzel(m_samples12k, coarseStart,
                                                           kFrameSamples,
                                                           fineFrequency + 500.0);
                    if (low + high > refinedFrequency.score) {
                        refinedFrequency.frequencyHz = fineFrequency;
                        refinedFrequency.score = low + high;
                    }
                }
                for (int frameKind = 0; frameKind < (m_shortMessages ? 2 : 1); ++frameKind) {
                    const bool shortFrame = frameKind == 1;
                    const int frameSamples = shortFrame ? kShortFrameSamples : kFrameSamples;
                    const int localBegin = qMax(0, coarseStart - kFrameSamples / 2);
                    const int localEnd = qMin(n - frameSamples, coarseStart + kFrameSamples / 2);
                    QVector<Candidate> syncScores;
                    for (int start = localBegin; start <= localEnd; start += timeStep) {
                        const double sync = frameSyncMetricAt(start, frameSamples,
                                                              refinedFrequency.frequencyHz, shortFrame);
                        syncScores.append(Candidate{start, refinedFrequency.frequencyHz, sync, shortFrame});
                    }
                    std::sort(syncScores.begin(), syncScores.end(), [](const Candidate &a, const Candidate &b) {
                        return a.score > b.score;
                    });
                    int accepted = 0;
                    for (const Candidate &coarseSync : std::as_const(syncScores)) {
                        Candidate sync = coarseSync;
                        for (int fineStart = coarseSync.start - timeStep;
                             fineStart <= coarseSync.start + timeStep;
                             ++fineStart) {
                            if (fineStart < 0 || fineStart + frameSamples > n) continue;
                            const double fineScore = frameSyncMetricAt(
                                fineStart, frameSamples, coarseSync.frequencyHz, shortFrame);
                            if (fineScore > sync.score) {
                                sync.start = fineStart;
                                sync.score = fineScore;
                            }
                        }
                        bool duplicate = false;
                        for (const Candidate &existing : std::as_const(candidates)) {
                            if (existing.shortFrame == sync.shortFrame &&
                                qAbs(existing.start - sync.start) < 2 * timeStep &&
                                qAbs(existing.frequencyHz - sync.frequencyHz) < 2 * freqStep) {
                                duplicate = true;
                                break;
                            }
                        }
                        if (!duplicate) {
                            candidates.append(sync);
                            ++accepted;
                        }
                        if (accepted >= 2) break;
                    }
                }
            }
        }
    }
    std::sort(candidates.begin(), candidates.end(), [](const Candidate &a, const Candidate &b) {
        return a.score > b.score;
    });
    if (candidates.size() > maxCandidates) candidates.resize(maxCandidates);

    int attempts = 0;
    int decodes = 0;
    std::set<QString> seen;
    Msk144Decode d;
    for (const Candidate &c : candidates) {
        ++attempts;
        d = Msk144Decode{};
        const bool decodeSucceeded = c.shortFrame
            ? tryDecodeShortAt(c.start, static_cast<double>(c.frequencyHz), d)
            : tryDecodeCoherentAt(c.start, static_cast<double>(c.frequencyHz), d);
        if (!decodeSucceeded) continue;
        const QString key = d.message + QStringLiteral("@") +
                            QString::number(qRound(d.frequencyHz / 5.0));
        if (seen.insert(key).second) {
            ++decodes;
            emit decoded(d);
        }
    }

    const QString status = QStringLiteral("MSK144 period decoded: %1 s, %2 attempts, %3 message(s)%4")
                               .arg(secondsBuffered)
                               .arg(attempts)
                               .arg(decodes)
                               .arg(m_shortMessages ? QStringLiteral("; short msgs on") : QString());
    if (status != m_lastStatus) {
        m_lastStatus = status;
        emit statusChanged(status);
    }
}

void Msk144Decoder::makeBasebandFrame(int startSample, double frequencyHz, QVector<std::complex<double>> &frame) const
{
    makeBaseband(startSample, kFrameSamples, frequencyHz, frame);
}

void Msk144Decoder::makeMixedBaseband(int startSample, int sampleCount, double frequencyHz,
                                      QVector<std::complex<double>> &frame) const
{
    frame.resize(sampleCount);
    const double phaseStep = -kTwoPi * frequencyHz / static_cast<double>(kInternalRate);
    const double startPhase = std::fmod(phaseStep * static_cast<double>(startSample), kTwoPi);
    std::complex<double> oscillator(std::cos(startPhase), std::sin(startPhase));
    const std::complex<double> rotation(std::cos(phaseStep), std::sin(phaseStep));
    for (int i = 0; i < sampleCount; ++i) {
        const double value = static_cast<double>(m_samples12k.at(startSample + i));
        frame[i] = 2.0 * value * oscillator;
        oscillator *= rotation;
    }
}

void Msk144Decoder::makeBaseband(int startSample, int sampleCount, double frequencyHz,
                                 QVector<std::complex<double>> &frame) const
{
    QVector<std::complex<double>> mixed;
    makeMixedBaseband(startSample, sampleCount, frequencyHz, mixed);
    frame.resize(sampleCount);

    // Small symmetric low-pass smoother after quadrature mixing.  The original
    // MSHV path builds an analytic signal with FFT filtering; this lightweight
    // FIR is sufficient for the bounded, selected-frequency live decoder path.
    for (int i = 0; i < sampleCount; ++i) {
        std::complex<double> acc(0.0, 0.0);
        double wsum = 0.0;
        for (int k = -5; k <= 5; ++k) {
            const int j = (i + k + sampleCount) % sampleCount;
            const double w = 1.0 + std::cos(kTwoPi * static_cast<double>(k) / 12.0);
            acc += mixed[j] * w;
            wsum += w;
        }
        frame[i] = (wsum > 0.0) ? (acc / wsum) : mixed[i];
    }
}

double Msk144Decoder::frameSyncMetricAt(int startSample,
                                        int frameSamples,
                                        double frequencyHz,
                                        bool shortFrame) const
{
    if (startSample < 0 || startSample + frameSamples > m_samples12k.size()) return 0.0;
    QVector<std::complex<double>> mixed;
    makeBaseband(startSample, frameSamples, frequencyHz, mixed);
    const QVector<std::complex<double>> matched = matchedMskSymbols(
        mixed, shortFrame ? 40 : kSymbols);
    return estimateSyncPhase(matched, shortFrame).quality;
}

bool Msk144Decoder::tryDecodeCoherentAt(int startSample, double frequencyHz, Msk144Decode &decode) const
{
    // First attempt the best single 72 ms frame.  If that fails, follow the
    // WSJT-X/MSHV MSK144 depth semantics with coherent averages of repeated
    // frames: Normal adds 4-frame averages; Deep adds 4-, 5- and 7-frame
    // averages. Final validity remains sync + LDPC + unpack.
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
        double referencePhase = 0.0;
        bool haveReferencePhase = false;
        for (int k = 0; k < navg; ++k) {
            QVector<std::complex<double>> fr;
            makeBasebandFrame(startSample + k * kFrameSamples, frequencyHz, fr);
            const SyncEstimate estimate = estimateSyncPhase(
                matchedMskSymbols(fr, kSymbols), false);
            if (!haveReferencePhase) {
                referencePhase = estimate.phase;
                haveReferencePhase = true;
            }
            const double correction = estimate.phase - referencePhase;
            const std::complex<double> rotation(std::cos(correction), -std::sin(correction));
            for (int i = 0; i < kFrameSamples; ++i) avg[i] += fr.at(i) * rotation;
        }
        for (std::complex<double> &v : avg) v /= static_cast<double>(navg);

        QString message;
        double quality = 0.0;
        if (!decodeMsk144Frame(avg, message, quality)) continue;
        decode.utc = m_periodStartUtc.addMSecs(static_cast<qint64>(1000.0 * startSample / kInternalRate));
        decode.tSeconds = static_cast<double>(startSample) / static_cast<double>(kInternalRate);
        decode.frequencyHz = frequencyHz;
        decode.dfHz = qRound(frequencyHz - static_cast<double>(m_rxFrequencyHz));
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
    decode.dfHz = qRound(frequencyHz - static_cast<double>(m_rxFrequencyHz));
    decode.snrDb = qBound(-8, qRound(estimateFrameSnrDb(startSample, kFrameSamples)), 24);
    decode.message = normalizeDecodedMessage(message);
    decode.navg = 1;
    decode.eye = quality;
    decode.shortMessage = false;
    return !decode.message.isEmpty();
}

bool Msk144Decoder::tryDecodeShortAt(int startSample, double frequencyHz, Msk144Decode &decode) const
{
    if (startSample < 0 || startSample + kShortFrameSamples > m_samples12k.size()) return false;
    QVector<std::complex<double>> frame;
    makeBaseband(startSample, kShortFrameSamples, frequencyHz, frame);
    QString message;
    double quality = 0.0;
    if (!decodeMsk40Frame(frame, message, quality)) return false;
    decode.utc = m_periodStartUtc.addMSecs(static_cast<qint64>(1000.0 * startSample / kInternalRate));
    decode.tSeconds = static_cast<double>(startSample) / static_cast<double>(kInternalRate);
    decode.frequencyHz = frequencyHz;
    decode.dfHz = qRound(frequencyHz - static_cast<double>(m_rxFrequencyHz));
    decode.snrDb = qBound(-8, qRound(estimateFrameSnrDb(startSample, kShortFrameSamples)), 24);
    decode.message = normalizeDecodedMessage(message);
    decode.navg = 1;
    decode.eye = quality;
    decode.shortMessage = true;
    return !decode.message.isEmpty();
}

bool Msk144Decoder::decodeMsk144Frame(const QVector<std::complex<double>> &c, QString &message, double &qualityMetric) const
{
    if (c.size() < kFrameSamples) {
        return false;
    }

    const QVector<std::complex<double>> matched = matchedMskSymbols(c, kSymbols);
    const SyncEstimate syncEstimate = estimateSyncPhase(matched, false);
    std::array<double, kSymbols> soft{};
    std::array<int, kSymbols> hard{};
    for (int i = 0; i < kSymbols; ++i) {
        soft[i] = softValue(matched.at(i), i, syncEstimate.phase);
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

    bool decoded77[120];
    for (bool &b : decoded77) b = false;
    int hardErrors = -1;
    QString unpacked;
    bool unpackOk = false;
    {
        std::lock_guard<std::mutex> guard(WeakSignalCodecLock::mutex());
        GenMsk gen(true);
        gen.save_hash_call_my_his_r1_r2(m_myCall, 0);
        gen.save_hash_call_my_his_r1_r2(m_dxCall, 1);
        gen.bpdecode128_90(llr, 10, decoded77, hardErrors);
        if (hardErrors >= 0 && hardErrors < 18) {
            unpacked = gen.unpack77(decoded77, unpackOk);
        }
    }
    if (hardErrors < 0 || hardErrors >= 18) {
        return false;
    }

    int n3 = 4 * static_cast<int>(decoded77[71]) + 2 * static_cast<int>(decoded77[72]) + static_cast<int>(decoded77[73]);
    int i3 = 4 * static_cast<int>(decoded77[74]) + 2 * static_cast<int>(decoded77[75]) + static_cast<int>(decoded77[76]);
    if ((i3 == 0 && (n3 == 1 || n3 == 3 || n3 == 4 || n3 > 5)) || i3 == 3 || i3 > 5) {
        return false;
    }

    message = normalizeDecodedMessage(unpacked);
    if (!unpackOk || message.isEmpty()) {
        return false;
    }
    qualityMetric = 8.0 * syncEstimate.quality +
                    qMax(0.0, 8.0 - static_cast<double>(badSync)) +
                    qMax(0.0, 18.0 - static_cast<double>(hardErrors)) / 3.0;
    return true;
}

bool Msk144Decoder::decodeMsk40Frame(const QVector<std::complex<double>> &c,
                                     QString &message,
                                     double &qualityMetric) const
{
    constexpr int symbols = 40;
    if (c.size() < kShortFrameSamples) return false;
    const QVector<std::complex<double>> matched = matchedMskSymbols(c, symbols);
    const SyncEstimate syncEstimate = estimateSyncPhase(matched, true);
    std::array<double, symbols> soft{};
    std::array<int, symbols> hard{};
    for (int i = 0; i < symbols; ++i) {
        soft[i] = softValue(matched.at(i), i, syncEstimate.phase);
        hard[i] = soft[i] >= 0.0 ? 1 : 0;
    }

    int syncScore = 0;
    for (int i = 0; i < 8; ++i) syncScore += (2 * hard[i] - 1) * syncPolarity(kMsk40Sync[i]);
    const int badSync = (8 - syncScore) / 2;
    if (badSync > 2) return false;

    double mean = std::accumulate(soft.begin(), soft.end(), 0.0) / symbols;
    double variance = 0.0;
    for (double value : soft) variance += (value - mean) * (value - mean);
    const double scale = std::sqrt(qMax(1e-12, variance / symbols));
    double llr[32]{};
    for (int i = 0; i < 32; ++i) llr[i] = 2.0 * soft[i + 8] / (0.36 * scale);

    char decoded[16]{};
    int iterations = -1;
    {
        std::lock_guard<std::mutex> guard(WeakSignalCodecLock::mutex());
        GenMsk generator(true);
        generator.bpdecode40(llr, 20, decoded, iterations);
    }
    if (iterations < 0) return false;
    int packed = 0;
    for (int i = 0; i < 16; ++i) packed |= (static_cast<int>(decoded[i]) & 1) << i;
    const int reportIndex = packed & 15;
    const int receivedHash = (packed >> 4) & 4095;

    QString calls;
    const QString forward = QStringLiteral("%1 %2").arg(m_dxCall, m_myCall).trimmed();
    const QString reverse = QStringLiteral("%1 %2").arg(m_myCall, m_dxCall).trimmed();
    {
        std::lock_guard<std::mutex> guard(WeakSignalCodecLock::mutex());
        GenMsk generator(true);
        if (!forward.isEmpty() && generator.hash_msk40(forward) == receivedHash) calls = forward;
        else if (!reverse.isEmpty() && generator.hash_msk40(reverse) == receivedHash) calls = reverse;
    }
    if (calls.isEmpty()) {
        if (m_swl) calls = QStringLiteral("HASH %1").arg(receivedHash, 4, 10, QLatin1Char('0'));
        else return false;
    }

    message = QStringLiteral("<%1> %2").arg(calls, QString::fromLatin1(kMsk40Reports[reportIndex]));
    qualityMetric = 8.0 * syncEstimate.quality +
                    qMax(0.0, 8.0 - badSync) + qMax(0, 20 - iterations) / 4.0;
    return true;
}

double Msk144Decoder::estimateFrameSnrDb(int startSample, int frameSamples) const
{
    if (m_samples12k.isEmpty() || startSample < 0 || startSample >= m_samples12k.size()) {
        return 0.0;
    }
    const int end = qMin(startSample + frameSamples,
                         static_cast<int>(m_samples12k.size()));
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
    allPower /= qMax(1, static_cast<int>(m_samples12k.size()));
    const double ratio = qMax(1e-6, framePower / qMax(1e-12, allPower));
    return 10.0 * std::log10(ratio) + 4.0;
}
