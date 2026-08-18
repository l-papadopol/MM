#include "Q65NativeEngine.h"

#include "../weak_signal/WeakSignalCodecLock.h"
#include "../../third_party/mshv_gpl/port/HvGenQ65/gen_q65.h"
#include "../../third_party/mshv_gpl/port/HvGenQ65/q65_subs.h"

#include <QtGlobal>

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <iterator>
#include <limits>
#include <numeric>
#include <set>
#include <utility>

namespace {

constexpr int kSampleRate = 12000;
constexpr int kQ65Symbols = 85;
constexpr int kDataSymbols = 63;
constexpr int kAlphabet = 64;
constexpr double kTwoPi = 6.283185307179586476925286766559;
constexpr std::array<int, 22> kSyncPositions{{
    0, 8, 11, 12, 14, 21, 22, 25, 26, 32, 34,
    37, 45, 49, 54, 59, 61, 65, 68, 73, 75, 84
}};

bool isSyncSymbol(int symbol)
{
    return std::binary_search(kSyncPositions.begin(), kSyncPositions.end(), symbol);
}

int nextPowerOfTwo(int value)
{
    int result = 1;
    while (result < value && result < (1 << 25)) result <<= 1;
    return result;
}

void fft(QVector<std::complex<double>> &values)
{
    const int n = values.size();
    for (int i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(values[i], values[j]);
    }
    for (int len = 2; len <= n; len <<= 1) {
        const double angle = -kTwoPi / static_cast<double>(len);
        const std::complex<double> step(std::cos(angle), std::sin(angle));
        for (int i = 0; i < n; i += len) {
            std::complex<double> w(1.0, 0.0);
            const int half = len >> 1;
            for (int j = 0; j < half; ++j) {
                const std::complex<double> u = values[i + j];
                const std::complex<double> v = values[i + j + half] * w;
                values[i + j] = u + v;
                values[i + j + half] = u - v;
                w *= step;
            }
        }
    }
}

QVector<double> powerSpectrum(const QVector<double> &samples,
                              int start,
                              int length,
                              int fftSize)
{
    QVector<std::complex<double>> work(fftSize, std::complex<double>(0.0, 0.0));
    const int edge = qMax(1, length / 32);
    for (int i = 0; i < length; ++i) {
        const int source = start + i;
        if (source < 0 || source >= samples.size()) continue;
        double window = 1.0;
        if (i < edge) {
            window = 0.5 - 0.5 * std::cos(kTwoPi * static_cast<double>(i) /
                                         static_cast<double>(2 * edge));
        } else if (i >= length - edge) {
            const int tail = length - 1 - i;
            window = 0.5 - 0.5 * std::cos(kTwoPi * static_cast<double>(tail) /
                                         static_cast<double>(2 * edge));
        }
        work[i] = std::complex<double>(samples.at(source) * window, 0.0);
    }
    fft(work);
    QVector<double> power(fftSize / 2 + 1);
    for (int i = 0; i < power.size(); ++i) power[i] = std::norm(work.at(i));
    return power;
}

double spectrumEnergy(const QVector<double> &power, int fftSize, double frequencyHz)
{
    if (frequencyHz < 0.0 || frequencyHz > 0.5 * kSampleRate || power.isEmpty()) return 0.0;
    const double bin = frequencyHz * static_cast<double>(fftSize) / kSampleRate;
    const int last = static_cast<int>(power.size()) - 1;
    const int i0 = qBound(0, static_cast<int>(std::floor(bin)), last);
    const int i1 = qBound(0, i0 + 1, last);
    const double fraction = bin - static_cast<double>(i0);
    return (1.0 - fraction) * power.at(i0) + fraction * power.at(i1);
}

double toneEnergy(const QVector<double> &samples, int start, int length, double frequencyHz)
{
    if (length <= 0 || frequencyHz <= 0.0 || frequencyHz >= 0.5 * kSampleRate) return 0.0;
    const double step = kTwoPi * frequencyHz / kSampleRate;
    const std::complex<double> rotation(std::cos(step), -std::sin(step));
    std::complex<double> oscillator(1.0, 0.0);
    std::complex<double> sum(0.0, 0.0);
    const int edge = qMax(1, length / 32);
    int used = 0;
    for (int i = 0; i < length; ++i) {
        const int source = start + i;
        if (source >= 0 && source < samples.size()) {
            double window = 1.0;
            if (i < edge) {
                window = 0.5 - 0.5 * std::cos(kTwoPi * static_cast<double>(i) /
                                             static_cast<double>(2 * edge));
            } else if (i >= length - edge) {
                const int tail = length - 1 - i;
                window = 0.5 - 0.5 * std::cos(kTwoPi * static_cast<double>(tail) /
                                             static_cast<double>(2 * edge));
            }
            sum += samples.at(source) * window * oscillator;
            ++used;
        }
        oscillator *= rotation;
        if ((i & 2047) == 2047) oscillator /= std::abs(oscillator);
    }
    if (used < length / 2) return 0.0;
    return std::norm(sum) / static_cast<double>(used * used);
}

double percentile(QVector<double> values, double quantile)
{
    if (values.isEmpty()) return 1e-18;
    const int last = static_cast<int>(values.size()) - 1;
    const int index = qBound(0, qRound(quantile * static_cast<double>(last)), last);
    std::nth_element(values.begin(), values.begin() + index, values.end());
    return qMax(1e-18, values.at(index));
}

QString normalizedMessage(QString message)
{
    message = message.trimmed().toUpper();
    message.replace('\t', ' ');
    while (message.contains(QStringLiteral("  "))) message.replace(QStringLiteral("  "), QStringLiteral(" "));
    return message;
}

int submodeIndex(Q65Mode::Submode submode)
{
    switch (submode) {
    case Q65Mode::Submode::B: return 1;
    case Q65Mode::Submode::C: return 2;
    case Q65Mode::Submode::D: return 3;
    case Q65Mode::Submode::A: return 0;
    }
    return 0;
}

} // namespace

Q65NativeEngine::Q65NativeEngine() = default;

int Q65NativeEngine::symbolSamples(int periodSeconds)
{
    switch (periodSeconds) {
    case 15: return 1800;
    case 30: return 3600;
    case 120: return 16000;
    case 60:
    default: return 7200;
    }
}

int Q65NativeEngine::transmittedSamples(int periodSeconds)
{
    return kQ65Symbols * symbolSamples(periodSeconds);
}

void Q65NativeEngine::clearAverages()
{
    for (AverageBank &bank : m_averageBanks) bank = AverageBank{};
    m_allAverageCount = 0;
}

int Q65NativeEngine::usableAverageCount() const
{
    return qMax(m_averageBanks[0].count, m_averageBanks[1].count);
}

int Q65NativeEngine::allAverageCount() const
{
    return m_allAverageCount;
}

QVector<Q65NativeEngine::SyncCandidate> Q65NativeEngine::findSyncCandidates(
    const QVector<double> &samples,
    const Configuration &configuration) const
{
    const int nsps = symbolSamples(configuration.periodSeconds);
    const double baud = static_cast<double>(kSampleRate) / nsps;
    const int fftSize = nextPowerOfTwo(nsps);
    const double fftBinHz = static_cast<double>(kSampleRate) / fftSize;
    const double earliest = -qMin(1.0, 0.8 * 85.0 * nsps / kSampleRate);
    const double latest = configuration.emeDelay ? 6.0 : 1.2;
    const int subdivisions = configuration.decodeDepth <= 1 ? 4 :
                             (configuration.decodeDepth == 2 ? 8 : 12);
    const int timeStep = qMax(1, nsps / subdivisions);
    const int startMin = qRound(earliest * kSampleRate);
    const int startMax = qRound(latest * kSampleRate);
    const double representableMinimumHz = static_cast<double>(
        Q65Mode::minimumBaseToneHz(configuration.submode, configuration.periodSeconds));
    const double fMin = qMax(representableMinimumHz,
                             static_cast<double>(configuration.rxFrequencyHz - configuration.dfToleranceHz));
    const double representableBaseHz = static_cast<double>(
        Q65Mode::maximumBaseToneHz(configuration.submode, configuration.periodSeconds));
    const double fMax = qMin(representableBaseHz,
                             static_cast<double>(configuration.rxFrequencyHz + configuration.dfToleranceHz));
    if (fMin > fMax) return {};

    QVector<SyncCandidate> coarse;
    for (int start = startMin; start <= startMax; start += timeStep) {
        QVector<double> accumulated(fftSize / 2 + 1, 0.0);
        int valid = 0;
        for (int symbol : kSyncPositions) {
            const int symbolStart = start + symbol * nsps;
            if (symbolStart + nsps / 2 < 0 || symbolStart + nsps / 2 >= samples.size()) continue;
            const QVector<double> power = powerSpectrum(samples, symbolStart, nsps, fftSize);
            for (int bin = 0; bin < accumulated.size(); ++bin) accumulated[bin] += power.at(bin);
            ++valid;
        }
        if (valid < 12) continue;

        const int finalSpectrumBin = static_cast<int>(accumulated.size()) - 2;
        const int firstBin = qBound(1, static_cast<int>(std::floor(fMin / fftBinHz)), finalSpectrumBin);
        const int lastBin = qBound(firstBin, static_cast<int>(std::ceil(fMax / fftBinHz)), finalSpectrumBin);
        QVector<double> band;
        band.reserve(lastBin - firstBin + 1);
        for (int bin = firstBin; bin <= lastBin; ++bin) band.append(accumulated.at(bin));
        const double noise = percentile(band, 0.45);

        for (int bin = firstBin; bin <= lastBin; ++bin) {
            if (accumulated.at(bin) < accumulated.at(bin - 1) ||
                accumulated.at(bin) < accumulated.at(bin + 1)) continue;
            SyncCandidate candidate;
            candidate.startSample = start;
            candidate.frequencyHz = bin * fftBinHz;
            candidate.score = accumulated.at(bin) / noise;
            candidate.snrDb = 10.0 * std::log10(qMax(1e-12, candidate.score));
            coarse.append(candidate);
        }
    }

    std::sort(coarse.begin(), coarse.end(), [](const SyncCandidate &a, const SyncCandidate &b) {
        return a.score > b.score;
    });
    const int limit = configuration.decodeDepth <= 1 ? 4 : (configuration.decodeDepth == 2 ? 8 : 12);
    QVector<SyncCandidate> selected;
    for (const SyncCandidate &candidate : std::as_const(coarse)) {
        bool duplicate = false;
        for (const SyncCandidate &existing : std::as_const(selected)) {
            if (qAbs(existing.startSample - candidate.startSample) < nsps / 3 &&
                qAbs(existing.frequencyHz - candidate.frequencyHz) < 1.5 * qMax(baud, fftBinHz)) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) selected.append(refineSyncCandidate(samples, configuration, candidate));
        if (selected.size() >= limit) break;
    }
    std::sort(selected.begin(), selected.end(), [](const SyncCandidate &a, const SyncCandidate &b) {
        return a.score > b.score;
    });
    return selected;
}

Q65NativeEngine::SyncCandidate Q65NativeEngine::refineSyncCandidate(
    const QVector<double> &samples,
    const Configuration &configuration,
    const SyncCandidate &coarse) const
{
    const int nsps = symbolSamples(configuration.periodSeconds);
    const double baud = static_cast<double>(kSampleRate) / nsps;
    const int timeStep = qMax(1, nsps / (configuration.decodeDepth >= 3 ? 32 : 16));
    const double frequencyStep = baud / (configuration.decodeDepth >= 3 ? 4.0 : 2.0);
    const std::array<double, 5> driftValues{{-30.0, -15.0, 0.0, 15.0, 30.0}};

    SyncCandidate best = coarse;
    best.score = -std::numeric_limits<double>::infinity();
    for (int dt = -2 * timeStep; dt <= 2 * timeStep; dt += timeStep) {
        for (int dfIndex = -3; dfIndex <= 3; ++dfIndex) {
            const double frequency = coarse.frequencyHz + dfIndex * frequencyStep;
            const int driftCount = configuration.maxDrift ? static_cast<int>(driftValues.size()) : 1;
            for (int driftIndex = 0; driftIndex < driftCount; ++driftIndex) {
                const double drift = configuration.maxDrift ? driftValues[driftIndex] : 0.0;
                double snrDb = -40.0;
                const double score = syncPatternScore(samples, nsps, coarse.startSample + dt,
                                                      frequency, drift, &snrDb);
                if (score > best.score) {
                    best.startSample = coarse.startSample + dt;
                    best.frequencyHz = frequency;
                    best.driftHz = drift;
                    best.score = score;
                    best.snrDb = snrDb;
                }
            }
        }
    }
    return best;
}

double Q65NativeEngine::syncPatternScore(const QVector<double> &samples,
                                         int symbolLength,
                                         int startSample,
                                         double frequencyHz,
                                         double driftHz,
                                         double *snrDb) const
{
    QVector<double> syncEnergies;
    QVector<double> dataEnergies;
    syncEnergies.reserve(kSyncPositions.size());
    dataEnergies.reserve(kDataSymbols);
    for (int symbol = 0; symbol < kQ65Symbols; ++symbol) {
        const double relative = (static_cast<double>(symbol) - 42.0) / 84.0;
        const double energy = toneEnergy(samples, startSample + symbol * symbolLength,
                                         symbolLength, frequencyHz + driftHz * relative);
        if (energy <= 0.0) continue;
        if (isSyncSymbol(symbol)) syncEnergies.append(energy);
        else dataEnergies.append(energy);
    }
    if (syncEnergies.size() < 12 || dataEnergies.size() < 30) return -1e30;
    const double sync = percentile(syncEnergies, 0.35);
    const double off = percentile(dataEnergies, 0.55);
    const double ratio = sync / qMax(1e-18, off);
    if (snrDb) *snrDb = 10.0 * std::log10(qMax(1e-12, ratio - 1.0));
    return std::log(qMax(1e-12, ratio)) + 0.05 * std::log(qMax(1e-18, sync));
}

QVector<float> Q65NativeEngine::extractSymbolEnergies(
    const QVector<double> &samples,
    const Configuration &configuration,
    const SyncCandidate &candidate) const
{
    const int nsps = symbolSamples(configuration.periodSeconds);
    const int multiplier = Q65Mode::mshvToneSpacingMultiplier(configuration.submode);
    const int binsPerSymbol = kAlphabet * (2 + multiplier);
    const double baud = static_cast<double>(kSampleRate) / nsps;
    // Two-times zero padding resolves the Q65 baud grid while keeping the
    // 60/120 s modes bounded enough for live use on ordinary CPUs.
    const int fftSize = nextPowerOfTwo(2 * nsps);
    QVector<float> energies;
    energies.reserve(kDataSymbols * binsPerSymbol);

    for (int symbol = 0; symbol < kQ65Symbols; ++symbol) {
        if (isSyncSymbol(symbol)) continue;
        const QVector<double> power = powerSpectrum(samples,
                                                    candidate.startSample + symbol * nsps,
                                                    nsps,
                                                    fftSize);
        const double relative = (static_cast<double>(symbol) - 42.0) / 84.0;
        const double center = candidate.frequencyHz + candidate.driftHz * relative;
        QVector<double> row;
        row.reserve(binsPerSymbol);
        for (int bin = 0; bin < binsPerSymbol; ++bin) {
            // Q65 sync is protocol tone 0, while QRA values 0...63 are sent
            // as protocol tones 1...64. q65_intrinsics_fastfading expects
            // energy bin 64 to be the centre of QRA value 0 and advances by
            // `multiplier` bins per value. The +multiplier term below is thus
            // part of the protocol mapping, not an accidental frequency shift.
            const double frequency = center + (bin - 64 + multiplier) * baud;
            row.append(spectrumEnergy(power, fftSize, frequency));
        }
        const double base = percentile(row, 0.40);
        for (double value : std::as_const(row)) {
            energies.append(static_cast<float>(qBound(0.0, value / base, 20.0)));
        }
    }

    // A stable carrier/birdie can otherwise win the same relative bin in most
    // data symbols.  Q65's reference decoder zaps any bin selected in more
    // than fifteen of the 63 rows; keep the same invariant before QRA metrics.
    QVector<int> peakHistogram(binsPerSymbol, 0);
    for (int row = 0; row < kDataSymbols; ++row) {
        int peak = 0;
        for (int bin = 1; bin < binsPerSymbol; ++bin) {
            if (energies.at(row * binsPerSymbol + bin) >
                energies.at(row * binsPerSymbol + peak)) {
                peak = bin;
            }
        }
        ++peakHistogram[peak];
    }
    for (int bin = 0; bin < binsPerSymbol; ++bin) {
        if (peakHistogram.at(bin) <= 15) continue;
        for (int row = 0; row < kDataSymbols; ++row) {
            energies[row * binsPerSymbol + bin] = 1.0f;
        }
    }
    return energies;
}

QString Q65NativeEngine::unpackSymbols(const int decoded[13],
                                       const Configuration &configuration) const
{
    bool bits[100]{};
    int position = 0;
    for (int symbol = 0; symbol < 13; ++symbol) {
        int value = decoded[symbol];
        int width = 6;
        if (symbol == 12) {
            value /= 2;
            width = 5;
        }
        for (int bit = width - 1; bit >= 0; --bit) bits[position++] = ((value >> bit) & 1) != 0;
    }
    bool ok = false;
    GenQ65 generator(true);
    generator.save_hash_call_my_his_r1_r2(configuration.myCall.trimmed().toUpper(), 0);
    generator.save_hash_call_my_his_r1_r2(configuration.dxCall.trimmed().toUpper(), 1);
    const QString message = generator.unpack77(bits, ok);
    return ok ? normalizedMessage(message) : QString();
}

bool Q65NativeEngine::decodeEnergies(const QVector<float> &energies,
                                     const Configuration &configuration,
                                     Result *result) const
{
    const int multiplier = Q65Mode::mshvToneSpacingMultiplier(configuration.submode);
    const int binsPerSymbol = kAlphabet * (2 + multiplier);
    if (energies.size() != kDataSymbols * binsPerSymbol || result == nullptr) return false;

    QVector<float> probabilities(kDataSymbols * kAlphabet, 0.0f);
    int apMask[13]{};
    int apSymbols[13]{};
    const int maximumIterations = configuration.decodeDepth <= 1 ? 30 :
                                  (configuration.decodeDepth == 2 ? 60 : 100);
    const double baud = static_cast<double>(kSampleRate) / symbolSamples(configuration.periodSeconds);
    const int firstBandwidth = configuration.decodeDepth <= 1 ? 2 : 0;
    const int lastBandwidth = configuration.decodeDepth >= 3 ? 10 : 7;

    std::lock_guard<std::mutex> guard(WeakSignalCodecLock::mutex());
    q65subs codec;
    for (int bandwidth = firstBandwidth; bandwidth <= lastBandwidth; ++bandwidth) {
        const float b90ts = static_cast<float>(std::pow(1.72, bandwidth) / baud);
        codec.q65_intrinsics_ff(const_cast<float *>(energies.constData()),
                                submodeIndex(configuration.submode), b90ts, 1,
                                probabilities.data());
        int decoded[13]{};
        float esNoDb = 0.0f;
        int iterations = -2;
        codec.q65_dec(const_cast<float *>(energies.constData()), probabilities.data(),
                      apMask, apSymbols, maximumIterations, esNoDb, decoded, iterations);
        if (iterations < 0) continue;
        const int symbolSum = std::accumulate(std::begin(decoded), std::end(decoded), 0);
        if (symbolSum <= 0) continue;
        const QString message = unpackSymbols(decoded, configuration);
        if (message.isEmpty()) continue;
        result->message = message;
        result->iterations = iterations;
        const double bandwidthCorrection = 10.0 * std::log10(qMax(1.0, 2500.0 / baud));
        result->snrDb = esNoDb - bandwidthCorrection + 3.0;
        return true;
    }
    return false;
}

QVector<QString> Q65NativeEngine::assistedMessages(const Configuration &configuration) const
{
    const QString my = configuration.myCall.trimmed().toUpper();
    const QString dx = configuration.dxCall.trimmed().toUpper();
    const QString grid = configuration.dxGrid.trimmed().left(4).toUpper();
    QVector<QString> messages;
    if (my.isEmpty() || dx.isEmpty()) return messages;
    const QString directed = QStringLiteral("%1 %2").arg(my, dx);
    if (!grid.isEmpty()) {
        messages << QStringLiteral("%1 %2").arg(directed, grid)
                 << QStringLiteral("%1 R %2").arg(directed, grid)
                 << QStringLiteral("CQ %1 %2").arg(dx, grid);
    }
    for (const QString &ending : {QStringLiteral("RRR"), QStringLiteral("RR73"), QStringLiteral("73")}) {
        messages << QStringLiteral("%1 %2").arg(directed, ending);
    }
    // The Q65 full-AP primitive accepts at most 256 codewords.  The complete
    // standard directed exchange is 200 signed reports/replies plus grid and
    // final messages, matching the protocol's -50...+49 report range.
    for (int report = -50; report <= 49; ++report) {
        const QString numeric = QStringLiteral("%1%2")
                                    .arg(report >= 0 ? QLatin1Char('+') : QLatin1Char('-'))
                                    .arg(qAbs(report), 2, 10, QLatin1Char('0'));
        messages << QStringLiteral("%1 %2").arg(directed, numeric)
                 << QStringLiteral("%1 R%2").arg(directed, numeric);
    }
    QVector<QString> unique;
    std::set<QString> seen;
    for (const QString &message : std::as_const(messages)) {
        if (seen.insert(message).second) unique.append(message);
    }
    return unique;
}

bool Q65NativeEngine::decodeAssistedList(const QVector<float> &energies,
                                         const Configuration &configuration,
                                         Result *result) const
{
    if (!configuration.apDecode || result == nullptr) return false;
    const QVector<QString> messages = assistedMessages(configuration);
    if (messages.isEmpty()) return false;
    const int multiplier = Q65Mode::mshvToneSpacingMultiplier(configuration.submode);
    const int binsPerSymbol = kAlphabet * (2 + multiplier);
    if (energies.size() != kDataSymbols * binsPerSymbol) return false;

    std::lock_guard<std::mutex> guard(WeakSignalCodecLock::mutex());
    QVector<int> codewords;
    codewords.reserve(messages.size() * kDataSymbols);
    GenQ65 generator(true);
    for (const QString &message : messages) {
        int tones[kQ65Symbols]{};
        generator.genq65itone(message, tones, false);
        for (int symbol = 0; symbol < kQ65Symbols; ++symbol) {
            if (!isSyncSymbol(symbol)) codewords.append(tones[symbol]);
        }
    }

    QVector<float> probabilities(kDataSymbols * kAlphabet, 0.0f);
    const double baud = static_cast<double>(kSampleRate) / symbolSamples(configuration.periodSeconds);
    q65subs codec;
    for (int bandwidth = 0; bandwidth <= (configuration.decodeDepth >= 3 ? 10 : 7); ++bandwidth) {
        const float b90ts = static_cast<float>(std::pow(1.72, bandwidth) / baud);
        codec.q65_intrinsics_ff(const_cast<float *>(energies.constData()),
                                submodeIndex(configuration.submode), b90ts, 1,
                                probabilities.data());
        int decoded[13]{};
        float esNoDb = 0.0f;
        float likelihood = -1000.0f;
        int iterations = -2;
        codec.q65_dec_fullaplist(const_cast<float *>(energies.constData()), probabilities.data(),
                                 codewords.data(), messages.size(), esNoDb, decoded,
                                 likelihood, iterations);
        if (iterations < 0) continue;
        const int symbolSum = std::accumulate(std::begin(decoded), std::end(decoded), 0);
        if (symbolSum <= 0) continue;
        const QString message = unpackSymbols(decoded, configuration);
        if (message.isEmpty()) continue;
        result->message = message;
        result->iterations = iterations;
        result->assisted = true;
        const double correction = 10.0 * std::log10(qMax(1.0, 2500.0 / baud));
        result->snrDb = esNoDb - correction + 3.0;
        return true;
    }
    return false;
}

QVector<float> Q65NativeEngine::updateAverage(const QVector<float> &energies,
                                              int parity,
                                              const Configuration &configuration)
{
    AverageBank &bank = m_averageBanks[qBound(0, parity, 1)];
    const int binsPerSymbol = kAlphabet * (2 + Q65Mode::mshvToneSpacingMultiplier(configuration.submode));
    const bool incompatible = bank.energies.size() != energies.size() ||
                              bank.binsPerSymbol != binsPerSymbol ||
                              bank.periodSeconds != configuration.periodSeconds ||
                              bank.submodeMultiplier != Q65Mode::mshvToneSpacingMultiplier(configuration.submode);
    if (incompatible) bank = AverageBank{};
    if (bank.energies.isEmpty()) {
        bank.energies = energies;
        bank.count = 1;
        bank.binsPerSymbol = binsPerSymbol;
        bank.periodSeconds = configuration.periodSeconds;
        bank.submodeMultiplier = Q65Mode::mshvToneSpacingMultiplier(configuration.submode);
        return bank.energies;
    }
    const int newCount = qMin(8, bank.count + 1);
    const float alpha = 1.0f / static_cast<float>(newCount);
    for (int i = 0; i < energies.size(); ++i) {
        bank.energies[i] += alpha * (energies.at(i) - bank.energies.at(i));
    }
    bank.count = newCount;
    return bank.energies;
}

QVector<Q65NativeEngine::Result> Q65NativeEngine::decode(
    const QVector<double> &samples12k,
    qint64 periodId,
    const Configuration &configuration)
{
    QVector<Result> results;
    if (samples12k.size() < transmittedSamples(configuration.periodSeconds) / 2) return results;
    ++m_allAverageCount;
    const QVector<SyncCandidate> candidates = findSyncCandidates(samples12k, configuration);
    std::set<QString> seen;
    bool averageUpdated = false;

    for (const SyncCandidate &candidate : candidates) {
        if (candidate.score <= 0.0) continue;
        const QVector<float> energies = extractSymbolEnergies(samples12k, configuration, candidate);
        Result result;
        result.dtSeconds = static_cast<double>(candidate.startSample) / kSampleRate;
        result.frequencyHz = candidate.frequencyHz;
        result.driftHz = candidate.driftHz;
        result.snrDb = candidate.snrDb;
        bool decoded = decodeEnergies(energies, configuration, &result);
        if (!decoded) decoded = decodeAssistedList(energies, configuration, &result);

        if (!decoded && configuration.averaging && !averageUpdated) {
            const QVector<float> averaged = updateAverage(energies, static_cast<int>(periodId & 1), configuration);
            averageUpdated = true;
            decoded = decodeEnergies(averaged, configuration, &result);
            if (!decoded) decoded = decodeAssistedList(averaged, configuration, &result);
            result.averageCount = m_averageBanks[static_cast<int>(periodId & 1)].count;
        }
        if (!decoded || result.message.isEmpty()) continue;
        if (!seen.insert(result.message).second) continue;
        results.append(result);
        if (configuration.autoClearAverages) clearAverages();
        if (configuration.singleDecode) break;
    }
    return results;
}
