#include "Msk144RxCore.h"
#include "MshvMsk144Adapter.h"

#include "../../third_party/mshv_gpl/port/HvGenMsk/config_rpt_msk40.h"
#include "../../third_party/mshv_gpl/port/HvGenMsk/genmesage_msk.h"

#include <QtGlobal>
#include <QtMath>
#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <complex>
#include <limits>
#include <set>
#include <utility>

namespace {
constexpr int kInternalRate = 12000;
constexpr int kSamplesPerSymbol = 6;
constexpr int kMsk144Symbols = 144;
constexpr int kMsk40Symbols = 40;
constexpr int kFrameSamples = kMsk144Symbols * kSamplesPerSymbol;       // 864
constexpr int kShortFrameSamples = kMsk40Symbols * kSamplesPerSymbol;   // 240
constexpr double kTwoPi = 6.28318530717958647692;
const QVector<int> kMsk144SyncBits{0, 1, 1, 1, 0, 0, 1, 0};
const QVector<int> kMsk144SyncOffsets{0, 56};
const QVector<int> kMsk40SyncBits{1, 0, 1, 1, 0, 0, 0, 1}; // s8r: generated MSK40 short-message sync
const QVector<int> kMsk40SyncOffsets{0};

inline int syncPolarity(int bit)
{
    return bit ? 1 : -1;
}

inline int positiveModulo(int v, int m)
{
    const int r = v % m;
    return (r < 0) ? (r + m) : r;
}

QString normalizeDecodedMessage(QString msg)
{
    msg = msg.trimmed();
    msg.replace('\t', ' ');
    while (msg.contains(QStringLiteral("  "))) msg.replace(QStringLiteral("  "), QStringLiteral(" "));
    return msg;
}

QString normalizedCallPair(QString a, QString b)
{
    a = a.trimmed().toUpper();
    b = b.trimmed().toUpper();
    if (a.isEmpty() || b.isEmpty()) return QString();
    return a + QLatin1Char(' ') + b;
}
}

Msk144RxCoreResult Msk144RxCore::decodePeriod(const QVector<float> &samples12k,
                                              const QDateTime &periodStartUtc,
                                              const Msk144RxCoreConfig &config) const
{
    // MSHV adapter is now the primary MSK144 RX path.
    // The native experimental scanner below is kept only as dead-code fallback
    // for source-level comparison, but runtime decode goes through the
    // MSHV-derived MSK144/MSK40 chain.
    MshvMsk144Adapter mshvAdapter;
    return mshvAdapter.decodePeriod(samples12k, periodStartUtc, config);

    Msk144RxCoreResult result;
    const int secondsBuffered = samples12k.size() / kInternalRate;
    const int n = samples12k.size();
    if (n < kShortFrameSamples) {
        result.status = QStringLiteral("MSK144 native fallback RX: %1 s, no complete frame").arg(secondsBuffered);
        return result;
    }

    int coarseCount = 0;
    QVector<Candidate> candidates = findReferenceCandidates(samples12k, config, &coarseCount);
    result.coarseCandidates = coarseCount;
    result.syncCandidates = candidates.size();

    int decodes = 0;
    std::set<QString> seen;
    Msk144CoreDecode d;
    for (const Candidate &c : candidates) {
        ++result.candidatesTried;
        bool ok = false;

        // Do not run both MSK40 and full MSK144 decoders for every candidate.
        // The sync classifier is deliberately cheap; use it to avoid doubling
        // LDPC/baseband work on obvious short-message pings.
        const bool strongShort = config.shortMessages && (c.shortScore > c.standardScore + 0.18);
        const bool strongStandard = (c.standardScore > c.shortScore + 0.18);

        if (strongShort) {
            ++result.shortCandidatesTried;
            ok = tryDecodeShortAt(samples12k, periodStartUtc, config, c.start, static_cast<double>(c.frequencyHz), d);
            if (!ok && c.standardScore > 0.62) {
                ok = tryDecodeCoherentAt(samples12k, periodStartUtc, config, c.start, static_cast<double>(c.frequencyHz), d);
            }
        } else if (strongStandard || !config.shortMessages) {
            ok = tryDecodeCoherentAt(samples12k, periodStartUtc, config, c.start, static_cast<double>(c.frequencyHz), d);
            if (!ok && config.shortMessages && c.shortScore > 0.62) {
                ++result.shortCandidatesTried;
                ok = tryDecodeShortAt(samples12k, periodStartUtc, config, c.start, static_cast<double>(c.frequencyHz), d);
            }
        } else {
            // Ambiguous sync: try the cheaper short path first, then full only
            // if the full-frame sync score is still credible.
            ++result.shortCandidatesTried;
            ok = tryDecodeShortAt(samples12k, periodStartUtc, config, c.start, static_cast<double>(c.frequencyHz), d);
            if (!ok && c.standardScore > 0.55) {
                ok = tryDecodeCoherentAt(samples12k, periodStartUtc, config, c.start, static_cast<double>(c.frequencyHz), d);
            }
        }

        if (!ok) continue;

        const QString key = d.message + QStringLiteral("@");
        if (seen.insert(key).second) {
            ++decodes;
            result.decodes.append(d);
        }
    }

    result.status = QStringLiteral("MSK144 native fallback RX: %1 s, sync %2/%3, attempts %4, short %5, %6 message(s); sync/time/DF + coherent EQ + MSK40%7%8")
                        .arg(secondsBuffered)
                        .arg(result.syncCandidates)
                        .arg(result.coarseCandidates)
                        .arg(result.candidatesTried)
                        .arg(result.shortCandidatesTried)
                        .arg(decodes)
                        .arg(config.shortMessages ? QStringLiteral(" on") : QStringLiteral(" off"));
    return result;
}

QVector<Msk144RxCore::Candidate> Msk144RxCore::findReferenceCandidates(const QVector<float> &samples12k,
                                                                       const Msk144RxCoreConfig &config,
                                                                       int *coarseCount) const
{
    if (coarseCount) *coarseCount = 0;
    QVector<Candidate> coarse;
    const int n = samples12k.size();
    const int tol = qBound(10, config.dfToleranceHz, 500);
    const int f0 = qBound(300, config.rxFrequencyHz - tol, 2700);
    const int f1 = qBound(300, config.rxFrequencyHz + tol, 2700);
    const int timeStep = (config.decodeDepth <= 1) ? 24 : (config.decodeDepth == 2 ? 12 : 6);
    const int freqStep = (config.decodeDepth <= 1) ? 25 : (config.decodeDepth == 2 ? 12 : 8);
    // MSK144 pings are short: letting every weak sync blip reach LDPC is the
    // performance killer.  These budgets keep Deep useful while avoiding the
    // old 2000+ candidate storm observed during offline diagnostics.
    const int maxCoarse = (config.decodeDepth <= 1) ? 180 : (config.decodeDepth == 2 ? 320 : 520);
    const int maxFinal = (config.decodeDepth <= 1) ? 220 : (config.decodeDepth == 2 ? 520 : 900);

    struct TimeSeed { int start = 0; double energy = 0.0; };
    QVector<TimeSeed> timeSeeds;
    const int seedHop = (config.decodeDepth <= 1) ? 72 : (config.decodeDepth == 2 ? 36 : 18);
    const int seedWindow = qMin(kFrameSamples, n);
    const int maxTimeSeeds = (config.decodeDepth <= 1) ? 180 : (config.decodeDepth == 2 ? 320 : 520);
    QVector<double> prefix(n + 1, 0.0);
    for (int i = 0; i < n; ++i) {
        const double v = static_cast<double>(samples12k.at(i));
        prefix[i + 1] = prefix[i] + v * v;
    }
    for (int start = 0; start + kShortFrameSamples <= n; start += seedHop) {
        const int end = qMin(start + seedWindow, n);
        TimeSeed ts;
        ts.start = start;
        ts.energy = (prefix[end] - prefix[start]) / qMax(1, end - start);
        timeSeeds.append(ts);
    }
    std::stable_sort(timeSeeds.begin(), timeSeeds.end(), [](const TimeSeed &a, const TimeSeed &b) {
        if (!qFuzzyCompare(a.energy + 1.0, b.energy + 1.0)) return a.energy > b.energy;
        return a.start < b.start;
    });
    if (timeSeeds.size() > maxTimeSeeds) timeSeeds.resize(maxTimeSeeds);
    for (int start = 0; start + kShortFrameSamples <= n; start += qMax(240, seedHop * 8)) {
        TimeSeed ts;
        ts.start = start;
        ts.energy = 0.0;
        timeSeeds.append(ts);
    }
    std::stable_sort(timeSeeds.begin(), timeSeeds.end(), [](const TimeSeed &a, const TimeSeed &b) {
        return a.start < b.start;
    });
    QVector<TimeSeed> uniqueSeeds;
    uniqueSeeds.reserve(timeSeeds.size());
    for (const TimeSeed &ts : timeSeeds) {
        if (!uniqueSeeds.isEmpty() && std::abs(uniqueSeeds.last().start - ts.start) < seedHop) continue;
        uniqueSeeds.append(ts);
    }

    for (const TimeSeed &ts : uniqueSeeds) {
        const int start = ts.start;
        for (int f = f0; f <= f1; f += freqStep) {
            double energy = 0.0;
            const double standardScore = (start + kFrameSamples <= n)
                ? scoreSyncCandidate(samples12k, start, static_cast<double>(f), false, &energy)
                : -1.0;
            double shortEnergy = 0.0;
            const double shortScore = config.shortMessages
                ? scoreSyncCandidate(samples12k, start, static_cast<double>(f), true, &shortEnergy)
                : -1.0;
            Candidate c;
            c.start = start;
            c.frequencyHz = f;
            c.standardScore = standardScore;
            c.shortScore = shortScore;
            c.shortHint = (shortScore > standardScore + 0.08);
            c.syncMetric = qMax(standardScore, shortScore);
            c.energyMetric = qMax(ts.energy, c.shortHint ? shortEnergy : energy);
            if (c.syncMetric > 0.42 || c.energyMetric > 2.4) {
                coarse.append(c);
            }
        }
    }

    if (coarseCount) *coarseCount = coarse.size();
    std::stable_sort(coarse.begin(), coarse.end(), [](const Candidate &a, const Candidate &b) {
        if (!qFuzzyCompare(a.syncMetric + 1.0, b.syncMetric + 1.0)) return a.syncMetric > b.syncMetric;
        if (!qFuzzyCompare(a.energyMetric + 1.0, b.energyMetric + 1.0)) return a.energyMetric > b.energyMetric;
        if (a.start != b.start) return a.start < b.start;
        return a.frequencyHz < b.frequencyHz;
    });
    if (coarse.size() > maxCoarse) coarse.resize(maxCoarse);

    QVector<Candidate> refined;
    refined.reserve(qMin(maxFinal, coarse.size() * 9));
    const int fineTimeStep = 3; // 0.25 ms at 12 kHz: reference-style local time refinement
    const int fineFreqStep = qMax(1, freqStep / 2);
    for (const Candidate &seed : coarse) {
        for (int dt = -timeStep; dt <= timeStep; dt += fineTimeStep) {
            const int start = seed.start + dt;
            if (start < 0 || start + kShortFrameSamples > n) continue;
            for (int df = -freqStep; df <= freqStep; df += fineFreqStep) {
                const int f = qBound(f0, seed.frequencyHz + df, f1);
                double energy = 0.0;
                const double standardScore = (start + kFrameSamples <= n)
                    ? scoreSyncCandidate(samples12k, start, static_cast<double>(f), false, &energy)
                    : -1.0;
                double shortEnergy = 0.0;
                const double shortScore = config.shortMessages
                    ? scoreSyncCandidate(samples12k, start, static_cast<double>(f), true, &shortEnergy)
                    : -1.0;
                Candidate c;
                c.start = start;
                c.frequencyHz = f;
                c.standardScore = standardScore;
                c.shortScore = shortScore;
                c.shortHint = (shortScore > standardScore + 0.08);
                c.syncMetric = qMax(standardScore, shortScore);
                c.energyMetric = c.shortHint ? shortEnergy : energy;
                if (c.syncMetric > 0.46 || c.energyMetric > 2.2) refined.append(c);
            }
        }
    }

    std::stable_sort(refined.begin(), refined.end(), [](const Candidate &a, const Candidate &b) {
        if (!qFuzzyCompare(a.syncMetric + 1.0, b.syncMetric + 1.0)) return a.syncMetric > b.syncMetric;
        if (!qFuzzyCompare(a.energyMetric + 1.0, b.energyMetric + 1.0)) return a.energyMetric > b.energyMetric;
        if (a.start != b.start) return a.start < b.start;
        return a.frequencyHz < b.frequencyHz;
    });

    QVector<Candidate> unique;
    unique.reserve(qMin(maxFinal, refined.size()));
    for (const Candidate &c : refined) {
        bool duplicate = false;
        for (const Candidate &u : unique) {
            if (std::abs(u.start - c.start) <= 9 && std::abs(u.frequencyHz - c.frequencyHz) <= qMax(3, fineFreqStep)) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) unique.append(c);
        if (unique.size() >= maxFinal) break;
    }
    return unique;
}

double Msk144RxCore::rawSyncSoftAt(const QVector<float> &samples12k,
                                      int startSample,
                                      int frameSamples,
                                      double frequencyHz,
                                      int symbolIndex) const
{
    if (symbolIndex < 0 || startSample < 0) return 0.0;
    const double phaseStep = -kTwoPi * frequencyHz / static_cast<double>(kInternalRate);
    const double cStep = std::cos(phaseStep);
    const double sStep = std::sin(phaseStep);
    auto mix = [&](int rel, double &re, double &im) {
        const int absIdx = startSample + rel;
        if (rel < 0 || rel >= frameSamples || absIdx < 0 || absIdx >= samples12k.size()) {
            re = 0.0;
            im = 0.0;
            return;
        }
        const double ph = phaseStep * static_cast<double>(absIdx);
        const double x = 2.0 * static_cast<double>(samples12k.at(absIdx));
        re = x * std::cos(ph);
        im = x * std::sin(ph);
    };

    // Same pulse template used by demodulateMskSoft(), but computed only for
    // the sync symbols.  Candidate search no longer builds/filter a complete
    // baseband frame for every time/DF point.
    std::array<double, 12> pp{};
    for (int i = 0; i < 12; ++i) pp[i] = std::sin(kTwoPi * static_cast<double>(i) / 24.0);

    double soft = 0.0;
    if (symbolIndex == 0) {
        for (int i = 0; i < 6; ++i) {
            double re = 0.0, im = 0.0;
            mix(i, re, im);
            soft += im * pp[i + 6];
            mix(i + (frameSamples - 6), re, im);
            soft += im * pp[i];
        }
        return soft;
    }
    if (symbolIndex == 1) {
        for (int i = 0; i < 12; ++i) {
            double re = 0.0, im = 0.0;
            mix(i, re, im);
            soft += re * pp[i];
        }
        return soft;
    }

    const int pair = symbolIndex / 2;
    const bool even = ((symbolIndex % 2) == 0);
    const int rel0 = even ? (pair * 12 - 6) : (pair * 12);
    if (rel0 < 0 || rel0 + 11 >= frameSamples) return 0.0;

    // Fast recurrence within the 12-sample symbol window: one sin/cos setup,
    // then only multiply-adds.  Release AVX2 builds still vectorize the simple
    // loops around this path, but the main win is avoiding thousands of full
    // complex frames per scan.
    const int abs0 = startSample + rel0;
    double ph = phaseStep * static_cast<double>(abs0);
    double c = std::cos(ph);
    double si = std::sin(ph);
    for (int j = 0; j < 12; ++j) {
        const double x = 2.0 * static_cast<double>(samples12k.at(abs0 + j));
        const double re = x * c;
        const double im = x * si;
        soft += (even ? im : re) * pp[j];
        const double nc = c * cStep - si * sStep;
        const double ns = si * cStep + c * sStep;
        c = nc;
        si = ns;
    }
    return soft;
}

double Msk144RxCore::scoreSyncCandidate(const QVector<float> &samples12k,
                                        int startSample,
                                        double frequencyHz,
                                        bool shortMessage,
                                        double *energyMetric) const
{
    const int samplesNeeded = shortMessage ? kShortFrameSamples : kFrameSamples;
    if (startSample < 0 || startSample + samplesNeeded > samples12k.size()) {
        if (energyMetric) *energyMetric = 0.0;
        return -1.0;
    }

    const QVector<int> &offsets = shortMessage ? kMsk40SyncOffsets : kMsk144SyncOffsets;
    const QVector<int> &bits = shortMessage ? kMsk40SyncBits : kMsk144SyncBits;
    QVector<double> syncSoft;
    syncSoft.reserve(offsets.size() * bits.size());
    double meanAbs = 1e-9;
    for (int off : offsets) {
        for (int i = 0; i < bits.size(); ++i) {
            const int idx = off + i;
            const double v = rawSyncSoftAt(samples12k, startSample, samplesNeeded, frequencyHz, idx);
            syncSoft.append(v);
            meanAbs += std::abs(v);
        }
    }
    meanAbs /= static_cast<double>(qMax(1, syncSoft.size()));

    double pos = 0.0;
    double neg = 0.0;
    int k = 0;
    for (int off : offsets) {
        Q_UNUSED(off)
        for (int i = 0; i < bits.size(); ++i) {
            const double v = syncSoft.at(k++) / meanAbs;
            const int e = syncPolarity(bits.at(i));
            pos += v * static_cast<double>(e);
            neg -= v * static_cast<double>(e);
        }
    }
    const int count = syncSoft.size();
    const double sync = (count > 0) ? qMax(pos, neg) / static_cast<double>(count) : -1.0;

    double framePower = 1e-12;
    // Use a compact power probe around the expected burst instead of scanning
    // the whole full frame during candidate generation.
    const int powerSamples = qMin(samplesNeeded, shortMessage ? 240 : 864);
    for (int i = 0; i < powerSamples; ++i) {
        const double v = static_cast<double>(samples12k.at(startSample + i));
        framePower += v * v;
    }
    framePower /= static_cast<double>(powerSamples);
    const int guard = qMin(2400, samples12k.size());
    double refPower = 1e-12;
    for (int i = 0; i < guard; i += 3) {
        const double v = static_cast<double>(samples12k.at((startSample + i) % samples12k.size()));
        refPower += v * v;
    }
    refPower /= qMax(1, guard / 3);
    if (energyMetric) *energyMetric = framePower / qMax(1e-12, refPower);
    return sync;
}

double Msk144RxCore::bandEnergyGoertzel(const QVector<float> &samples, int start, int count, double frequencyHz) const
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

void Msk144RxCore::makeBaseband(const QVector<float> &samples12k,
                                int startSample,
                                double frequencyHz,
                                int count,
                                QVector<std::complex<double>> &frame) const
{
    frame.resize(qMax(0, count));
    if (count <= 0 || startSample < 0 || startSample + count > samples12k.size()) {
        return;
    }

    const double phaseStep = -kTwoPi * frequencyHz / static_cast<double>(kInternalRate);
    const double cStep = std::cos(phaseStep);
    const double sStep = std::sin(phaseStep);
    double phase = phaseStep * static_cast<double>(startSample);
    double c = std::cos(phase);
    double si = std::sin(phase);

    QVector<std::complex<double>> mixed(count);
    for (int i = 0; i < count; ++i) {
        const double x = 2.0 * static_cast<double>(samples12k.at(startSample + i));
        mixed[i] = std::complex<double>(x * c, x * si);
        const double nc = c * cStep - si * sStep;
        const double ns = si * cStep + c * sStep;
        c = nc;
        si = ns;
    }

    // Reference-style analytic/baseband stage: a compact symmetric low-pass and
    // light amplitude equalizer before symbol sampling. This replaces the old
    // brute-force point sampler while keeping the MSHV LDPC/unpack backend.
    QVector<std::complex<double>> lp(count);
    for (int i = 0; i < count; ++i) {
        std::complex<double> acc(0.0, 0.0);
        double wsum = 0.0;
        for (int k = -5; k <= 5; ++k) {
            const int j = qBound(0, i + k, count - 1);
            const double w = 1.0 + std::cos(kTwoPi * static_cast<double>(k) / 12.0);
            acc += mixed.at(j) * w;
            wsum += w;
        }
        lp[i] = (wsum > 0.0) ? (acc / wsum) : mixed.at(i);
    }

    double meanMag = 1e-9;
    for (const std::complex<double> &v : lp) meanMag += std::abs(v);
    meanMag /= static_cast<double>(qMax(1, lp.size()));
    for (int i = 0; i < count; ++i) {
        frame[i] = lp.at(i) / meanMag;
    }
}

void Msk144RxCore::makeBasebandFrame(const QVector<float> &samples12k,
                                     int startSample,
                                     double frequencyHz,
                                     QVector<std::complex<double>> &frame) const
{
    makeBaseband(samples12k, startSample, frequencyHz, kFrameSamples, frame);
}

bool Msk144RxCore::demodulateMskSoft(const QVector<std::complex<double>> &frame,
                                     int symbols,
                                     QVector<double> &soft) const
{
    if (symbols <= 0 || frame.size() < symbols * kSamplesPerSymbol || (symbols % 2) != 0) {
        return false;
    }
    soft.fill(0.0, symbols);
    std::array<double, 12> pp{};
    for (int i = 0; i < 12; ++i) pp[i] = std::sin(kTwoPi * static_cast<double>(i) / 24.0);

    const int frameSamples = symbols * kSamplesPerSymbol;
    soft[0] = 0.0;
    soft[1] = 0.0;
    for (int i = 0; i < 6; ++i) {
        soft[0] += frame.at(i).imag() * pp[i + 6];
        soft[0] += frame.at(i + (frameSamples - 6)).imag() * pp[i];
    }
    for (int i = 0; i < 12 && i < frame.size(); ++i) {
        soft[1] += frame.at(i).real() * pp[i];
    }

    for (int i = 1; i < symbols / 2; ++i) {
        double q = 0.0;
        double in = 0.0;
        const int q0 = i * 12 - 6;
        const int i0 = i * 12;
        if (q0 < 0 || i0 + 11 >= frame.size()) return false;
        for (int j = 0; j < 12; ++j) {
            q += frame.at(q0 + j).imag() * pp[j];
            in += frame.at(i0 + j).real() * pp[j];
        }
        soft[2 * i] = q;
        soft[2 * i + 1] = in;
    }
    return true;
}

double Msk144RxCore::orientAndNormalizeSoft(QVector<double> &soft,
                                            const QVector<int> &syncOffsets,
                                            const QVector<int> &syncBits) const
{
    if (soft.isEmpty()) return 0.0;
    double mean = 0.0;
    double mean2 = 0.0;
    for (double v : soft) {
        mean += v;
        mean2 += v * v;
    }
    mean /= static_cast<double>(soft.size());
    mean2 /= static_cast<double>(soft.size());
    double sigma = std::sqrt(qMax(1e-12, mean2 - mean * mean));
    if (sigma <= 0.0) sigma = 1.0;
    for (double &v : soft) v = (v - mean) / sigma;

    double pos = 0.0;
    double neg = 0.0;
    int count = 0;
    for (int off : syncOffsets) {
        for (int i = 0; i < syncBits.size(); ++i) {
            const int idx = off + i;
            if (idx < 0 || idx >= soft.size()) continue;
            const int e = syncPolarity(syncBits.at(i));
            pos += soft.at(idx) * static_cast<double>(e);
            neg -= soft.at(idx) * static_cast<double>(e);
            ++count;
        }
    }
    if (neg > pos) {
        for (double &v : soft) v = -v;
        pos = neg;
    }
    return (count > 0) ? pos / static_cast<double>(count) : 0.0;
}

bool Msk144RxCore::tryDecodeCoherentAt(const QVector<float> &samples12k,
                                       const QDateTime &periodStartUtc,
                                       const Msk144RxCoreConfig &config,
                                       int startSample,
                                       double frequencyHz,
                                       Msk144CoreDecode &decode) const
{
    if (tryDecodeFrameAt(samples12k, periodStartUtc, config, startSample, frequencyHz, decode)) {
        decode.navg = 1;
        return true;
    }

    QVector<int> navgs;
    if (config.decodeDepth == 2) {
        navgs = {4};
    } else if (config.decodeDepth >= 3) {
        navgs = {3, 4, 5, 7};
    }

    for (int navg : navgs) {
        const int needed = startSample + navg * kFrameSamples;
        if (needed > samples12k.size()) continue;
        QVector<std::complex<double>> avg(kFrameSamples);
        std::fill(avg.begin(), avg.end(), std::complex<double>(0.0, 0.0));
        for (int k = 0; k < navg; ++k) {
            QVector<std::complex<double>> fr;
            makeBasebandFrame(samples12k, startSample + k * kFrameSamples, frequencyHz, fr);
            for (int i = 0; i < kFrameSamples; ++i) avg[i] += fr.at(i);
        }
        for (std::complex<double> &v : avg) v /= static_cast<double>(navg);

        QString message;
        double quality = 0.0;
        if (!decodeMsk144Frame(avg, message, quality)) continue;
        decode.utc = periodStartUtc.addMSecs(static_cast<qint64>(1000.0 * startSample / kInternalRate));
        decode.tSeconds = static_cast<double>(startSample) / static_cast<double>(kInternalRate);
        decode.frequencyHz = frequencyHz;
        decode.dfHz = qRound(frequencyHz - 1500.0);
        decode.snrDb = qBound(-8, qRound(estimateFrameSnrDb(samples12k, startSample, navg * kFrameSamples) + 10.0 * std::log10(static_cast<double>(navg))), 24);
        decode.message = normalizeDecodedMessage(message);
        decode.navg = navg;
        decode.eye = quality;
        decode.shortMessage = false;
        return !decode.message.isEmpty();
    }
    return false;
}

bool Msk144RxCore::tryDecodeFrameAt(const QVector<float> &samples12k,
                                    const QDateTime &periodStartUtc,
                                    const Msk144RxCoreConfig &config,
                                    int startSample,
                                    double frequencyHz,
                                    Msk144CoreDecode &decode) const
{
    Q_UNUSED(config)
    if (startSample < 0 || startSample + kFrameSamples > samples12k.size()) {
        return false;
    }
    QVector<std::complex<double>> frame;
    makeBasebandFrame(samples12k, startSample, frequencyHz, frame);
    QString message;
    double quality = 0.0;
    if (!decodeMsk144Frame(frame, message, quality)) {
        return false;
    }

    decode.utc = periodStartUtc.addMSecs(static_cast<qint64>(1000.0 * startSample / kInternalRate));
    decode.tSeconds = static_cast<double>(startSample) / static_cast<double>(kInternalRate);
    decode.frequencyHz = frequencyHz;
    decode.dfHz = qRound(frequencyHz - 1500.0);
    decode.snrDb = qBound(-8, qRound(estimateFrameSnrDb(samples12k, startSample, kFrameSamples)), 24);
    decode.message = normalizeDecodedMessage(message);
    decode.navg = 1;
    decode.eye = quality;
    decode.shortMessage = false;
    return !decode.message.isEmpty();
}

bool Msk144RxCore::tryDecodeShortAt(const QVector<float> &samples12k,
                                    const QDateTime &periodStartUtc,
                                    const Msk144RxCoreConfig &config,
                                    int startSample,
                                    double frequencyHz,
                                    Msk144CoreDecode &decode) const
{
    if (startSample < 0 || startSample + kShortFrameSamples > samples12k.size()) {
        return false;
    }
    QVector<std::complex<double>> frame;
    makeBaseband(samples12k, startSample, frequencyHz, kShortFrameSamples, frame);
    QString message;
    double quality = 0.0;
    if (!decodeMsk40Frame(frame, config, message, quality)) {
        return false;
    }
    decode.utc = periodStartUtc.addMSecs(static_cast<qint64>(1000.0 * startSample / kInternalRate));
    decode.tSeconds = static_cast<double>(startSample) / static_cast<double>(kInternalRate);
    decode.frequencyHz = frequencyHz;
    decode.dfHz = qRound(frequencyHz - 1500.0);
    decode.snrDb = qBound(-8, qRound(estimateFrameSnrDb(samples12k, startSample, kShortFrameSamples)), 24);
    decode.message = normalizeDecodedMessage(message);
    decode.navg = 1;
    decode.eye = quality;
    decode.shortMessage = true;
    return !decode.message.isEmpty();
}

bool Msk144RxCore::decodeMsk144Frame(const QVector<std::complex<double>> &frame,
                                     QString &message,
                                     double &qualityMetric) const
{
    if (frame.size() < kFrameSamples) return false;

    QVector<double> soft;
    if (!demodulateMskSoft(frame, kMsk144Symbols, soft)) return false;
    const double syncQuality = orientAndNormalizeSoft(soft, kMsk144SyncOffsets, kMsk144SyncBits);
    if (syncQuality < 0.55) return false;

    int hardErrors = 0;
    for (int off : kMsk144SyncOffsets) {
        for (int i = 0; i < kMsk144SyncBits.size(); ++i) {
            const int idx = off + i;
            if (idx < 0 || idx >= soft.size()) continue;
            const int hard = (soft.at(idx) >= 0.0) ? 1 : 0;
            if (hard != kMsk144SyncBits.at(i)) ++hardErrors;
        }
    }
    if (hardErrors > 3) return false;

    double llr[128];
    int k = 0;
    for (int i = 8; i < 56; ++i) llr[k++] = qBound(-12.0, 2.4 * soft.at(i), 12.0);
    for (int i = 64; i < 144; ++i) llr[k++] = qBound(-12.0, 2.4 * soft.at(i), 12.0);

    GenMsk gen(true);
    bool decoded77[120];
    for (bool &b : decoded77) b = false;
    int nHardError = -1;
    gen.bpdecode128_90(llr, 20, decoded77, nHardError);
    if (nHardError < 0 || nHardError >= 18) return false;

    const int n3 = 4 * static_cast<int>(decoded77[71]) + 2 * static_cast<int>(decoded77[72]) + static_cast<int>(decoded77[73]);
    const int i3 = 4 * static_cast<int>(decoded77[74]) + 2 * static_cast<int>(decoded77[75]) + static_cast<int>(decoded77[76]);
    if ((i3 == 0 && (n3 == 1 || n3 == 3 || n3 == 4 || n3 > 5)) || i3 == 3 || i3 > 5) return false;

    bool unpackOk = false;
    message = normalizeDecodedMessage(gen.unpack77(decoded77, unpackOk));
    if (!unpackOk || message.isEmpty()) return false;
    qualityMetric = qMax(0.0, syncQuality) + qMax(0.0, 18.0 - static_cast<double>(nHardError)) / 3.0;
    return true;
}

bool Msk144RxCore::decodeMsk40Frame(const QVector<std::complex<double>> &frame,
                                    const Msk144RxCoreConfig &config,
                                    QString &message,
                                    double &qualityMetric) const
{
    if (frame.size() < kShortFrameSamples) return false;
    QVector<double> soft;
    if (!demodulateMskSoft(frame, kMsk40Symbols, soft)) return false;
    const double syncQuality = orientAndNormalizeSoft(soft, kMsk40SyncOffsets, kMsk40SyncBits);
    if (syncQuality < 0.55) return false;

    double llr[32];
    for (int i = 0; i < 32; ++i) {
        llr[i] = qBound(-12.0, 2.2 * soft.at(8 + i), 12.0);
    }

    GenMsk gen(true);
    char decoded[16];
    for (char &c : decoded) c = 0;
    int niterations = -1;
    gen.bpdecode40(llr, 20, decoded, niterations);
    if (niterations < 0) return false;

    int ig = 0;
    for (int i = 0; i < 16; ++i) {
        if (decoded[i]) ig |= (1 << i);
    }
    const int irpt = ig & 0x0f;
    const int ihash = (ig >> 4) & 0x0fff;
    if (irpt < 0 || irpt >= 16) return false;
    const QString rpt = rpt_msk40[irpt].trimmed();

    QString calls;
    const QString ab = normalizedCallPair(config.myCall, config.dxCall);
    const QString ba = normalizedCallPair(config.dxCall, config.myCall);
    if (!ab.isEmpty() && gen.hash_msk40(ab) == ihash) {
        calls = ab;
    } else if (!ba.isEmpty() && gen.hash_msk40(ba) == ihash) {
        calls = ba;
    }

    if (!calls.isEmpty()) {
        message = QStringLiteral("<%1> %2").arg(calls, rpt);
    } else {
        message = QStringLiteral("<MSK40:%1> %2").arg(ihash, 4, 16, QLatin1Char('0')).arg(rpt).toUpper();
    }
    qualityMetric = syncQuality + qMax(0.0, 20.0 - static_cast<double>(niterations)) / 10.0;
    return !message.isEmpty();
}

double Msk144RxCore::estimateFrameSnrDb(const QVector<float> &samples12k,
                                        int startSample,
                                        int frameSamples) const
{
    if (samples12k.isEmpty() || startSample < 0 || startSample >= samples12k.size()) {
        return 0.0;
    }
    const int end = qMin(startSample + frameSamples, samples12k.size());
    double framePower = 1e-12;
    for (int i = startSample; i < end; ++i) {
        const double v = samples12k.at(i);
        framePower += v * v;
    }
    framePower /= qMax(1, end - startSample);

    QVector<double> powers;
    const int win = qMax(240, frameSamples);
    for (int start = 0; start + win <= samples12k.size(); start += win) {
        if (std::abs(start - startSample) < win) continue;
        double p = 1e-12;
        for (int i = start; i < start + win; ++i) {
            const double v = samples12k.at(i);
            p += v * v;
        }
        powers.append(p / static_cast<double>(win));
    }
    double noisePower = 1e-12;
    if (!powers.isEmpty()) {
        std::sort(powers.begin(), powers.end());
        noisePower = powers.at(powers.size() / 2);
    } else {
        for (float fv : samples12k) {
            const double v = static_cast<double>(fv);
            noisePower += v * v;
        }
        noisePower /= qMax(1, samples12k.size());
    }
    const double signalExcess = qMax(1e-6, framePower / qMax(1e-12, noisePower));
    return 10.0 * std::log10(signalExcess) + 2.0;
}
