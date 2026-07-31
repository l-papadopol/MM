#include "Ft8RxDecoder.h"
#include "Ft8Mode.h"
#include "../../dsp/cpu/CpuFeatures.h"

#define MN_NM_NRW_FT_174_91
#include "../../third_party/mshv_gpl/port/HvGenFt8/bpdecode_ft8_174_91.h"
#include "../../third_party/mshv_gpl/port/boost/boost_14.hpp"
#include "../../third_party/mshv_gpl/port/genpom.h"

#include <QCoreApplication>
#include <QFile>
#include <QMetaType>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <chrono>
#include <cmath>
#include <complex>
#include <condition_variable>
#include <deque>
#include <exception>
#include <functional>
#include <future>
#include <initializer_list>
#include <limits>
#include <map>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#define MADMODEM_FT8_HAVE_SSE2 1
#include <emmintrin.h>
#endif

#if (defined(__GNUC__) || defined(__clang__)) && (defined(__x86_64__) || defined(__i386__))
#define MADMODEM_FT8_HAVE_AVX2_TARGET 1
#include <immintrin.h>
#endif

namespace {
constexpr int kCostas[7] = {3, 1, 4, 0, 6, 5, 2};
constexpr int kCostasStarts[3] = {0, 36, 72};
constexpr int kGrayMap[8] = {0, 1, 3, 2, 5, 6, 4, 7};
constexpr int kFt4SyncA[4] = {0, 1, 3, 2};
constexpr int kFt4SyncB[4] = {1, 0, 2, 3};
constexpr int kFt4SyncC[4] = {2, 3, 1, 0};
constexpr int kFt4SyncD[4] = {3, 2, 0, 1};
constexpr int kFt4GrayMap[4] = {0, 1, 3, 2};
constexpr int kFt4Scrambler[77] = {
    0,1,0,0,1,0,1,0,0,1,0,1,1,1,1,0,1,0,0,0,1,0,0,1,1,0,1,1,0,
    1,0,0,1,0,1,1,0,0,0,0,1,0,0,0,1,0,1,0,0,1,1,1,1,0,0,1,0,1,
    0,1,0,1,0,1,1,0,1,1,1,1,1,0,0,0,1,0,1
};
constexpr double kTwoPi = 6.283185307179586476925286766559;
constexpr double kEps = 1.0e-18;

/*
 * 0.5.1 GF(2) OSD lab primitives.  FT8 LDPC parity rows
 * are 174 bits wide, so three 64-bit words cover one row.  Gaussian
 * elimination then becomes row swaps plus three XORs per elimination,
 * with no heap allocation in the candidate hot path.
 */
struct Gf2Row
{
    uint64_t w0 = 0ULL;
    uint64_t w1 = 0ULL;
    uint64_t w2 = 0ULL;

    void clear()
    {
        w0 = 0ULL;
        w1 = 0ULL;
        w2 = 0ULL;
    }

    void xorWith(const Gf2Row &other)
    {
        w0 ^= other.w0;
        w1 ^= other.w1;
        w2 ^= other.w2;
    }

    int getBit(int col) const
    {
        if (col < 64) {
            return static_cast<int>((w0 >> col) & 1ULL);
        }
        if (col < 128) {
            return static_cast<int>((w1 >> (col - 64)) & 1ULL);
        }
        return static_cast<int>((w2 >> (col - 128)) & 1ULL);
    }

    void setBit(int col, int value)
    {
        if (col < 64) {
            const uint64_t mask = 1ULL << col;
            if (value != 0) {
                w0 |= mask;
            } else {
                w0 &= ~mask;
            }
            return;
        }
        if (col < 128) {
            const uint64_t mask = 1ULL << (col - 64);
            if (value != 0) {
                w1 |= mask;
            } else {
                w1 &= ~mask;
            }
            return;
        }
        const uint64_t mask = 1ULL << (col - 128);
        if (value != 0) {
            w2 |= mask;
        } else {
            w2 &= ~mask;
        }
    }
};

struct Gf2Matrix83x174
{
    static constexpr int ROWS = 83;
    static constexpr int COLS = 174;
    std::array<Gf2Row, ROWS> rows{};

    void clear()
    {
        for (Gf2Row &row : rows) {
            row.clear();
        }
    }

    void swapRows(int r1, int r2)
    {
        if (r1 == r2) {
            return;
        }
        const Gf2Row tmp = rows[r1];
        rows[r1] = rows[r2];
        rows[r2] = tmp;
    }
};


/*
 * v4.12 FT decode worker pool.
 *
 * The native decoder used std::async(std::launch::async) in the hottest FT8
 * paths.  On libstdc++ this can spawn kernel threads repeatedly instead of
 * reusing warm workers.  A 15 s FT slot is short enough that this scheduling
 * jitter shows up directly in offline decode wall time.  Keep a small dedicated
 * pool for FT candidate work instead of using Qt's global pool, so waterfall,
 * UI, audio and CAT tasks do not compete with decoder bursts.
 */
class FtDecodeWorkerPool
{
public:
    static FtDecodeWorkerPool &instance()
    {
        static FtDecodeWorkerPool pool;
        return pool;
    }

    int recommendedWorkerCount(bool offline, int itemCount) const
    {
        const unsigned int hw = std::thread::hardware_concurrency();
        const int hwCount = static_cast<int>(hw > 0 ? hw : 2);
        const int cap = offline ? 12 : 8;
        return qBound(1, hwCount, qMin(cap, qMax(1, itemCount)));
    }

    void parallelFor(int itemCount, int requestedTasks, const std::function<void(int, int)> &fn)
    {
        if (itemCount <= 0) {
            return;
        }
        const int taskCount = qBound(1, requestedTasks, itemCount);
        if (taskCount <= 1 || m_workers.empty() || t_insideFtPool) {
            fn(0, itemCount);
            return;
        }

        struct Batch
        {
            std::mutex mutex;
            std::condition_variable done;
            int remaining = 0;
            std::exception_ptr exception;
        };

        auto batch = std::make_shared<Batch>();
        batch->remaining = taskCount;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            const int chunk = (itemCount + taskCount - 1) / taskCount;
            for (int task = 0; task < taskCount; ++task) {
                const int begin = task * chunk;
                const int end = qMin(itemCount, begin + chunk);
                if (begin >= end) {
                    std::lock_guard<std::mutex> batchLock(batch->mutex);
                    --batch->remaining;
                    continue;
                }
                m_tasks.emplace_back([batch, fn, begin, end]() {
                    try {
                        fn(begin, end);
                    } catch (...) {
                        std::lock_guard<std::mutex> lock(batch->mutex);
                        if (!batch->exception) {
                            batch->exception = std::current_exception();
                        }
                    }
                    {
                        std::lock_guard<std::mutex> lock(batch->mutex);
                        --batch->remaining;
                    }
                    batch->done.notify_one();
                });
            }
        }
        m_cv.notify_all();

        std::unique_lock<std::mutex> waitLock(batch->mutex);
        batch->done.wait(waitLock, [&batch]() { return batch->remaining <= 0; });
        if (batch->exception) {
            std::rethrow_exception(batch->exception);
        }
    }

private:
    FtDecodeWorkerPool()
    {
        const unsigned int hw = std::thread::hardware_concurrency();
        const int hwCount = static_cast<int>(hw > 0 ? hw : 2);
        const int count = qBound(1, hwCount, 12);
        m_workers.reserve(static_cast<size_t>(count));
        for (int i = 0; i < count; ++i) {
            m_workers.emplace_back([this]() { workerLoop(); });
        }
    }

    ~FtDecodeWorkerPool()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stopping = true;
        }
        m_cv.notify_all();
        for (std::thread &worker : m_workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    void workerLoop()
    {
        t_insideFtPool = true;
        for (;;) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cv.wait(lock, [this]() { return m_stopping || !m_tasks.empty(); });
                if (m_stopping && m_tasks.empty()) {
                    break;
                }
                task = std::move(m_tasks.front());
                m_tasks.pop_front();
            }
            task();
        }
        t_insideFtPool = false;
    }

    static thread_local bool t_insideFtPool;

    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::deque<std::function<void()>> m_tasks;
    std::vector<std::thread> m_workers;
    bool m_stopping = false;
};

thread_local bool FtDecodeWorkerPool::t_insideFtPool = false;

#if defined(MADMODEM_FT8_HAVE_AVX2_TARGET)
__attribute__((target("avx2,fma")))
std::array<double, 8> ft8ToneEnergies8Avx2Fma(const double *xv,
                                             int sampleCount,
                                             double baseFrequencyHz,
                                             double toneSpacingHz,
                                             int sampleRate)
{
    std::array<double, 8> power{};
    double coeff[8];
    for (int tone = 0; tone < 8; ++tone) {
        const double frequencyHz = baseFrequencyHz + static_cast<double>(tone) * toneSpacingHz;
        const double omega = kTwoPi * frequencyHz / static_cast<double>(sampleRate);
        coeff[tone] = 2.0 * std::cos(omega);
    }

    const __m256d c0123 = _mm256_set_pd(coeff[3], coeff[2], coeff[1], coeff[0]);
    const __m256d c4567 = _mm256_set_pd(coeff[7], coeff[6], coeff[5], coeff[4]);
    __m256d s1_0123 = _mm256_setzero_pd();
    __m256d s2_0123 = _mm256_setzero_pd();
    __m256d s1_4567 = _mm256_setzero_pd();
    __m256d s2_4567 = _mm256_setzero_pd();

    for (int n = 0; n < sampleCount; ++n) {
        const __m256d x = _mm256_set1_pd(xv[n]);
        __m256d s0 = _mm256_sub_pd(_mm256_fmadd_pd(c0123, s1_0123, x), s2_0123);
        s2_0123 = s1_0123;
        s1_0123 = s0;

        s0 = _mm256_sub_pd(_mm256_fmadd_pd(c4567, s1_4567, x), s2_4567);
        s2_4567 = s1_4567;
        s1_4567 = s0;
    }

    const __m256d p0123 = _mm256_sub_pd(
        _mm256_add_pd(_mm256_mul_pd(s1_0123, s1_0123), _mm256_mul_pd(s2_0123, s2_0123)),
        _mm256_mul_pd(c0123, _mm256_mul_pd(s1_0123, s2_0123)));
    const __m256d p4567 = _mm256_sub_pd(
        _mm256_add_pd(_mm256_mul_pd(s1_4567, s1_4567), _mm256_mul_pd(s2_4567, s2_4567)),
        _mm256_mul_pd(c4567, _mm256_mul_pd(s1_4567, s2_4567)));

    _mm256_storeu_pd(power.data(), p0123);
    _mm256_storeu_pd(power.data() + 4, p4567);
    return power;
}

__attribute__((target("avx2,fma")))
std::array<double, 4> ft4ToneEnergies4Avx2Fma(const double *xv,
                                             int sampleCount,
                                             double baseFrequencyHz,
                                             double toneSpacingHz,
                                             int sampleRate)
{
    std::array<double, 4> power{};
    double coeff[4];
    for (int tone = 0; tone < 4; ++tone) {
        const double frequencyHz = baseFrequencyHz + static_cast<double>(tone) * toneSpacingHz;
        const double omega = kTwoPi * frequencyHz / static_cast<double>(sampleRate);
        coeff[tone] = 2.0 * std::cos(omega);
    }

    const __m256d c0123 = _mm256_set_pd(coeff[3], coeff[2], coeff[1], coeff[0]);
    __m256d s1_0123 = _mm256_setzero_pd();
    __m256d s2_0123 = _mm256_setzero_pd();

    for (int n = 0; n < sampleCount; ++n) {
        const __m256d x = _mm256_set1_pd(xv[n]);
        const __m256d s0 = _mm256_sub_pd(_mm256_fmadd_pd(c0123, s1_0123, x), s2_0123);
        s2_0123 = s1_0123;
        s1_0123 = s0;
    }

    const __m256d p0123 = _mm256_sub_pd(
        _mm256_add_pd(_mm256_mul_pd(s1_0123, s1_0123), _mm256_mul_pd(s2_0123, s2_0123)),
        _mm256_mul_pd(c0123, _mm256_mul_pd(s1_0123, s2_0123)));
    _mm256_storeu_pd(power.data(), p0123);
    return power;
}
#endif

void fftRadix2Transform(std::vector<std::complex<double>> &a, bool inverse)
{
    const int n = static_cast<int>(a.size());
    int j = 0;
    for (int i = 1; i < n; ++i) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            std::swap(a[i], a[j]);
        }
    }

    for (int len = 2; len <= n; len <<= 1) {
        const double angle = (inverse ? 1.0 : -1.0) * kTwoPi / static_cast<double>(len);
        const std::complex<double> wlen(std::cos(angle), std::sin(angle));
        for (int i = 0; i < n; i += len) {
            std::complex<double> w(1.0, 0.0);
            const int half = len >> 1;
            for (int k = 0; k < half; ++k) {
                const std::complex<double> u = a[i + k];
                const std::complex<double> v = a[i + k + half] * w;
                a[i + k] = u + v;
                a[i + k + half] = u - v;
                w *= wlen;
            }
        }
    }

    if (inverse && n > 0) {
        const double scale = 1.0 / static_cast<double>(n);
        for (std::complex<double> &v : a) {
            v *= scale;
        }
    }
}

void fftRadix2(std::vector<std::complex<double>> &a)
{
    fftRadix2Transform(a, false);
}

void inverseFftRadix2(std::vector<std::complex<double>> &a)
{
    fftRadix2Transform(a, true);
}

constexpr int kFt8SubNFrame = 1920 * 79;
constexpr int kFt8SubNfft = 262144; // power-of-two replacement for MSHV's 180000-point FFT
constexpr int kFt8SubNfilt = 4000;
constexpr double kFt8SubtractGain = 1.9962; // MSHV 2.70 K_SUB

struct Ft8SubtractWorkspace
{
    std::vector<double> dphi;
    std::vector<std::complex<double>> cref;
    std::vector<std::complex<double>> cfilt;
};

const std::vector<double> &ft8RxGfskPulse()
{
    static const std::vector<double> pulse = []() {
        std::vector<double> p(static_cast<size_t>(3 * 1920), 0.0);
        gen_pulse_gfsk_(p.data(), 2880.0, 2.0, 1920);
        return p;
    }();
    return pulse;
}

const std::array<std::vector<double>, 8> &ft8RxGfskToneDphiTable()
{
    static const std::array<std::vector<double>, 8> table = []() {
        constexpr int nsps = 1920;
        constexpr double dphiPeak = kTwoPi / static_cast<double>(nsps);
        std::array<std::vector<double>, 8> out;
        const std::vector<double> &pulse = ft8RxGfskPulse();
        for (int tone = 0; tone < 8; ++tone) {
            out[static_cast<size_t>(tone)].resize(static_cast<size_t>(3 * nsps));
            const double scale = dphiPeak * static_cast<double>(tone);
            for (int i = 0; i < 3 * nsps; ++i) {
                out[static_cast<size_t>(tone)][static_cast<size_t>(i)] = pulse[static_cast<size_t>(i)] * scale;
            }
        }
        return out;
    }();
    return table;
}

const std::vector<double> &ft8RxRampInTable()
{
    static const std::vector<double> ramp = []() {
        constexpr int nsps = 1920;
        const int nramp = nsps / 8;
        std::vector<double> out(static_cast<size_t>(nramp), 1.0);
        for (int i = 0; i < nramp; ++i) {
            out[static_cast<size_t>(i)] = (1.0 - std::cos(kTwoPi * static_cast<double>(i) / (2.0 * static_cast<double>(nramp)))) * 0.5;
        }
        return out;
    }();
    return ramp;
}

const std::vector<double> &ft8RxRampOutTable()
{
    static const std::vector<double> ramp = []() {
        constexpr int nsps = 1920;
        const int nramp = nsps / 8;
        std::vector<double> out(static_cast<size_t>(nramp), 1.0);
        for (int i = 0; i < nramp; ++i) {
            out[static_cast<size_t>(i)] = (1.0 + std::cos(kTwoPi * static_cast<double>(i) / (2.0 * static_cast<double>(nramp)))) * 0.5;
        }
        return out;
    }();
    return ramp;
}

void makeFt8ReferenceWaveformRx(const int *itone,
                                  double baseHz,
                                  std::vector<std::complex<double>> &cwave,
                                  std::vector<double> &dphi)
{
    constexpr int nsym = 79;
    constexpr int nsps = 1920;
    constexpr int nwave = nsym * nsps;
    const std::array<std::vector<double>, 8> &toneDphi = ft8RxGfskToneDphiTable();

    dphi.resize(static_cast<size_t>(nwave + 2 * nsps + 16));
    std::fill(dphi.begin(), dphi.end(), 0.0);
    cwave.resize(static_cast<size_t>(nwave));

    for (int sym = 0; sym < nsym; ++sym) {
        const int ib = sym * nsps;
        const int tone = qBound(0, itone[sym], 7);
        const std::vector<double> &shape = toneDphi[static_cast<size_t>(tone)];
        for (int i = 0; i < 3 * nsps; ++i) {
            dphi[static_cast<size_t>(ib + i)] += shape[static_cast<size_t>(i)];
        }
    }

    const int bgn = nsym * nsps;
    const int firstTone = qBound(0, itone[0], 7);
    const int lastTone = qBound(0, itone[nsym - 1], 7);
    const std::vector<double> &firstShape = toneDphi[static_cast<size_t>(firstTone)];
    const std::vector<double> &lastShape = toneDphi[static_cast<size_t>(lastTone)];
    for (int i = 0; i < 2 * nsps; ++i) {
        dphi[static_cast<size_t>(i)] += firstShape[static_cast<size_t>(i + nsps)];
        dphi[static_cast<size_t>(i + bgn)] += lastShape[static_cast<size_t>(i)];
    }

    const double ofs = kTwoPi * baseHz / 12000.0;
    double phi = 0.0;
    for (int n = 0; n < nwave; ++n) {
        cwave[static_cast<size_t>(n)] = std::complex<double>(std::cos(phi), std::sin(phi));
        phi += dphi[static_cast<size_t>(n + nsps)] + ofs;
        if (phi >= kTwoPi) {
            phi -= kTwoPi;
        } else if (phi < 0.0) {
            phi += kTwoPi;
        }
    }

    const std::vector<double> &rampIn = ft8RxRampInTable();
    const std::vector<double> &rampOut = ft8RxRampOutTable();
    const int nramp = nsps / 8;
    for (int i = 0; i < nramp; ++i) {
        cwave[static_cast<size_t>(i)] *= rampIn[static_cast<size_t>(i)];
    }
    const int k2 = nsym * nsps - nramp + 1;
    for (int i = 0; i < nramp; ++i) {
        const int idx = i + k2;
        if (idx >= 0 && idx < nwave) {
            cwave[static_cast<size_t>(idx)] *= rampOut[static_cast<size_t>(i)];
        }
    }
}

std::vector<std::complex<double>> makeFt8ReferenceWaveformRx(const int *itone, double baseHz)
{
    std::vector<std::complex<double>> cwave;
    std::vector<double> dphi;
    makeFt8ReferenceWaveformRx(itone, baseHz, cwave, dphi);
    return cwave;
}

const std::vector<std::complex<double>> &ft8SubtractKernelFft()
{
    static const std::vector<std::complex<double>> kernelFft = []() {
        std::vector<std::complex<double>> kernel(static_cast<size_t>(kFt8SubNfft), std::complex<double>(0.0, 0.0));
        double sumw = 0.0;
        for (int j = -kFt8SubNfilt / 2; j < kFt8SubNfilt / 2; ++j) {
            const double w = std::pow(std::cos((0.5 * kTwoPi) * static_cast<double>(j) / static_cast<double>(kFt8SubNfilt)), 2.0);
            sumw += w;
        }
        if (sumw <= 0.0) {
            sumw = 1.0;
        }
        for (int j = -kFt8SubNfilt / 2; j < kFt8SubNfilt / 2; ++j) {
            const double w = std::pow(std::cos((0.5 * kTwoPi) * static_cast<double>(j) / static_cast<double>(kFt8SubNfilt)), 2.0) / sumw;
            int idx = j;
            while (idx < 0) {
                idx += kFt8SubNfft;
            }
            idx %= kFt8SubNfft;
            kernel[static_cast<size_t>(idx)] = std::complex<double>(w, 0.0);
        }
        fftRadix2(kernel);
        return kernel;
    }();
    return kernelFft;
}

const std::vector<double> &ft8SubtractEndCorrection()
{
    static const std::vector<double> correction = []() {
        std::vector<double> corr(static_cast<size_t>(kFt8SubNfilt / 4), 1.0);
        std::vector<double> window(static_cast<size_t>(kFt8SubNfilt), 0.0);
        double sumw = 0.0;
        for (int j = -kFt8SubNfilt / 2; j < kFt8SubNfilt / 2; ++j) {
            const int idx = j + kFt8SubNfilt / 2;
            const double w = std::pow(std::cos((0.5 * kTwoPi) * static_cast<double>(j) / static_cast<double>(kFt8SubNfilt)), 2.0);
            window[static_cast<size_t>(idx)] = w;
            sumw += w;
        }
        if (sumw <= 0.0) {
            return corr;
        }
        for (int j = 0; j < kFt8SubNfilt / 4; ++j) {
            double summ = 0.0;
            for (int z = j; z < kFt8SubNfilt / 2; ++z) {
                summ += window[static_cast<size_t>(z + kFt8SubNfilt / 2)];
            }
            double denom = 1.0 - summ / sumw;
            if (denom <= 1.0e-4) {
                denom = 1.0e-4;
            }
            corr[static_cast<size_t>(j)] = 1.0 / denom;
        }
        return corr;
    }();
    return correction;
}

void smoothComplexEnvelopeZeroPhase(std::vector<std::complex<double>> &env)
{
    if (env.empty()) {
        return;
    }

    /*
     * Fast subtractft8 envelope estimator.
     *
     * v3.30 used a reference-like FFT low-pass for every decoded signal.  It
     * worked, but each subtraction paid a 262144-point FFT + inverse FFT.  On
     * crowded 15 s slots that made adaptive decode spend 1-2 seconds just in
     * subtraction.
     *
     * MSHV's important idea is not the FFT itself; it is estimating a slow
     * complex amplitude/phase envelope from dd*conj(cref) before subtracting
     * cfilt*cref.  This O(N) forward/backward low-pass keeps that signal model
     * but avoids per-message FFT cost.  The time constant is tied to the same
     * 4000-sample reference smoothing scale, but shortened enough to track the
     * mild QSB/phase drift in real WebSDR WAVs.
     */
    const double tauSamples = static_cast<double>(kFt8SubNfilt) / 5.0; // ~67 ms at 12 kHz
    const double alpha = std::exp(-1.0 / tauSamples);
    const double beta = 1.0 - alpha;

    std::complex<double> acc = env.front();
    for (std::complex<double> &v : env) {
        acc = alpha * acc + beta * v;
        v = acc;
    }

    acc = env.back();
    for (auto it = env.rbegin(); it != env.rend(); ++it) {
        acc = alpha * acc + beta * (*it);
        *it = acc;
    }

    const int nramp = qMin(kFt8SubNfilt / 4, static_cast<int>(env.size()) / 2);
    for (int i = 0; i < nramp; ++i) {
        const double ramp = 0.5 - 0.5 * std::cos(kTwoPi * static_cast<double>(i + 1) / static_cast<double>(2 * nramp + 2));
        env[static_cast<size_t>(i)] *= ramp;
        env[env.size() - 1U - static_cast<size_t>(i)] *= ramp;
    }
}


struct OfflineWavFormat
{
    quint16 audioFormat = 0;
    quint16 channels = 0;
    quint32 sampleRate = 0;
    quint16 blockAlign = 0;
    quint16 bitsPerSample = 0;
    qint64 dataOffset = 0;
    qint64 dataSize = 0;
};

quint16 readLe16(const char *p)
{
    return static_cast<quint16>(static_cast<unsigned char>(p[0])) |
           static_cast<quint16>(static_cast<unsigned char>(p[1]) << 8);
}

quint32 readLe32(const char *p)
{
    return static_cast<quint32>(static_cast<unsigned char>(p[0])) |
           (static_cast<quint32>(static_cast<unsigned char>(p[1])) << 8) |
           (static_cast<quint32>(static_cast<unsigned char>(p[2])) << 16) |
           (static_cast<quint32>(static_cast<unsigned char>(p[3])) << 24);
}

bool parseOfflineWavHeader(QFile &file, OfflineWavFormat &format, QString &error)
{
    if (!file.seek(0)) {
        error = QStringLiteral("cannot seek to file start");
        return false;
    }

    const QByteArray riff = file.read(12);
    if (riff.size() != 12 || riff.mid(0, 4) != "RIFF" || riff.mid(8, 4) != "WAVE") {
        error = QStringLiteral("not a RIFF/WAVE file");
        return false;
    }

    bool haveFmt = false;
    bool haveData = false;

    while (!file.atEnd()) {
        const QByteArray header = file.read(8);
        if (header.size() < 8) {
            break;
        }
        const QByteArray id = header.mid(0, 4);
        const quint32 size = readLe32(header.constData() + 4);
        const qint64 payloadStart = file.pos();

        if (id == "fmt ") {
            const QByteArray fmt = file.read(qMin<quint32>(size, 40));
            if (fmt.size() < 16) {
                error = QStringLiteral("invalid fmt chunk");
                return false;
            }
            format.audioFormat = readLe16(fmt.constData() + 0);
            format.channels = readLe16(fmt.constData() + 2);
            format.sampleRate = readLe32(fmt.constData() + 4);
            format.blockAlign = readLe16(fmt.constData() + 12);
            format.bitsPerSample = readLe16(fmt.constData() + 14);
            haveFmt = true;
        } else if (id == "data") {
            format.dataOffset = payloadStart;
            format.dataSize = static_cast<qint64>(size);
            haveData = true;
        }

        const qint64 next = payloadStart + static_cast<qint64>(size) + (size & 1U);
        if (!file.seek(next)) {
            break;
        }
        if (haveFmt && haveData) {
            break;
        }
    }

    if (!haveFmt || !haveData) {
        error = QStringLiteral("missing fmt or data chunk");
        return false;
    }
    if (format.channels < 1 || format.channels > 8 || format.sampleRate < 8000 || format.sampleRate > 384000 || format.blockAlign == 0) {
        error = QStringLiteral("unsupported WAV channel/rate layout");
        return false;
    }
    const bool pcm = (format.audioFormat == 1 && (format.bitsPerSample == 8 || format.bitsPerSample == 16 || format.bitsPerSample == 24 || format.bitsPerSample == 32));
    const bool ieeeFloat = (format.audioFormat == 3 && format.bitsPerSample == 32);
    if (!pcm && !ieeeFloat) {
        error = QStringLiteral("unsupported WAV sample format; use PCM 8/16/24/32-bit or 32-bit float");
        return false;
    }
    return true;
}

QVector<float> convertOfflineWavToMono(const QByteArray &raw, const OfflineWavFormat &format)
{
    QVector<float> out;
    if (format.blockAlign == 0 || format.channels == 0 || raw.isEmpty()) {
        return out;
    }
    const int frames = raw.size() / static_cast<int>(format.blockAlign);
    out.reserve(frames);
    const char *data = raw.constData();
    const int bytesPerSample = qMax(1, static_cast<int>(format.bitsPerSample / 8));

    for (int frame = 0; frame < frames; ++frame) {
        const char *framePtr = data + frame * static_cast<int>(format.blockAlign);
        double sum = 0.0;
        for (int ch = 0; ch < static_cast<int>(format.channels); ++ch) {
            const char *p = framePtr + ch * bytesPerSample;
            double sample = 0.0;
            if (format.audioFormat == 3 && format.bitsPerSample == 32) {
                float f = 0.0f;
                std::memcpy(&f, p, sizeof(float));
                sample = qBound(-1.0, static_cast<double>(f), 1.0);
            } else if (format.bitsPerSample == 8) {
                sample = (static_cast<int>(static_cast<unsigned char>(p[0])) - 128) / 128.0;
            } else if (format.bitsPerSample == 16) {
                const qint16 v = static_cast<qint16>(readLe16(p));
                sample = static_cast<double>(v) / 32768.0;
            } else if (format.bitsPerSample == 24) {
                qint32 v = static_cast<qint32>(static_cast<unsigned char>(p[0])) |
                           (static_cast<qint32>(static_cast<unsigned char>(p[1])) << 8) |
                           (static_cast<qint32>(static_cast<unsigned char>(p[2])) << 16);
                if (v & 0x00800000) {
                    v |= static_cast<qint32>(0xff000000);
                }
                sample = static_cast<double>(v) / 8388608.0;
            } else if (format.bitsPerSample == 32) {
                const qint32 v = static_cast<qint32>(readLe32(p));
                sample = static_cast<double>(v) / 2147483648.0;
            }
            sum += sample;
        }
        out.append(static_cast<float>(sum / static_cast<double>(format.channels)));
    }
    return out;
}

bool looksLikeUsefulFt8Message(const QString &message)
{
    if (message.size() < 3) {
        return false;
    }
    const QString trimmed = message.trimmed().toUpper();
    if (trimmed.isEmpty()) {
        return false;
    }
    if (trimmed.contains("***") || trimmed.contains("???")) {
        return false;
    }
    return true;
}
QString formatSlotTime(const QDateTime &utc, int slotMs)
{
    const QString fmt = (slotMs % 1000 == 0)
        ? QStringLiteral("HH:mm:ss")
        : QStringLiteral("HH:mm:ss.zzz");
    return utc.time().toString(fmt);
}

int wsjtxFt8ReportDb(double codewordSignalPower,
                       double oppositeToneNoisePower,
                       int hardSyncCount)
{
    if (!(codewordSignalPower > 0.0) || !(oppositeToneNoisePower > 0.0) ||
        !std::isfinite(codewordSignalPower) || !std::isfinite(oppositeToneNoisePower)) {
        return -25;
    }

    /* v3.25: keep the stable v3.16/v3.22 FT8 decoder core and SNR power-unit fix
     * mismatch.  WSJT-X ft8b.f90 computes:
     *
     *   xsig = sum_i s8(itone(i), i)^2
     *   xnoi = sum_i s8(mod(itone(i)+4,7), i)^2
     *   xsnr = 10*log10(xsig/xnoi - 1) - 27
     *
     * MadModem symbolToneEnergy() returns a Goertzel POWER already, equivalent
     * to s8^2.  v3.16 accidentally squared that power again before calling
     * this function, which inflated displayed SNR values (+20 dB class reports)
     * without improving any decode.  The caller now passes sums of POWER, not
     * POWER^2, so this routine stays dimensionally consistent with ft8b().
     * No waterfall estimate, no sync-ratio fallback and no post-CRC cosmetic
     * rescue is mixed into the displayed report.
     */
    const double arg = (codewordSignalPower / qMax(oppositeToneNoisePower, kEps)) - 1.0;
    if (!(arg > 0.1) || !std::isfinite(arg)) {
        return -25;
    }

    double xsnr = 10.0 * std::log10(arg) - 27.0;

    if (hardSyncCount <= 10 && xsnr < -25.0) {
        xsnr = -25.0;
    }

    return qRound(qBound(-25.0, xsnr, 49.0));
}

int wsjtxFt4ReportDb(double candidateSnrRatio)
{
    if (!(candidateSnrRatio > 0.0) || !std::isfinite(candidateSnrRatio)) {
        return -21;
    }

    /* WSJT-X 3.0.1 lib/ft4_decode.f90 reports:
     *   snr = candidate(2,icand)-1.0
     *   xsnr = 10*log10(snr) - 14.8
     *   nsnr = nint(max(-21.0,xsnr))
     */
    const double xsnr = 10.0 * std::log10(candidateSnrRatio) - 14.8;
    return qRound(qBound(-21.0, xsnr, 49.0));
}

int ft4ReportDbFromDecodedPowers(double expectedPowerSum,
                                 double offTonePowerSum,
                                 int expectedSymbolCount,
                                 int offToneCount)
{
    if (!(expectedPowerSum > 0.0) || !(offTonePowerSum > 0.0) ||
        expectedSymbolCount <= 0 || offToneCount <= 0 ||
        !std::isfinite(expectedPowerSum) || !std::isfinite(offTonePowerSum)) {
        return -21;
    }

    /* 0.5.78-lab15: findFt4Candidates() ranks in log-power space and does not
     * carry the WSJT-X candidate(2) signal/noise ratio. Using candidate.syncRatio
     * after a CRC-valid FT4 decode therefore collapsed every displayed report to
     * the FT4 floor (-21 dB).  Rebuild the selected 4-FSK codeword after CRC and
     * compare selected-tone power with the average off-tone power in the same
     * symbols.  This is display/reporting only and never gates decode emission.
     */
    const double offToneMean = offTonePowerSum / static_cast<double>(offToneCount);
    const double noiseEquivalent = qMax(offToneMean * static_cast<double>(expectedSymbolCount), kEps);
    const double candidateSnrRatio = (expectedPowerSum / noiseEquivalent) - 1.0;
    if (!(candidateSnrRatio > 0.0) || !std::isfinite(candidateSnrRatio)) {
        return -21;
    }
    return wsjtxFt4ReportDb(candidateSnrRatio);
}

QVector<double> copyLeadingSamplesPadded(const QVector<double> &source, int sampleCount, bool padToCount)
{
    QVector<double> out;
    if (sampleCount <= 0) {
        return out;
    }

    const int copyCount = qMin(source.size(), sampleCount);
    const int outCount = padToCount ? sampleCount : copyCount;
    if (outCount <= 0) {
        return out;
    }

    out.resize(outCount);
    if (copyCount > 0) {
        std::copy(source.constData(), source.constData() + copyCount, out.data());
    }
    if (padToCount && copyCount < sampleCount) {
        std::fill(out.data() + copyCount, out.data() + sampleCount, 0.0);
    }
    return out;
}

}

Ft8RxDecoder::Ft8RxDecoder(QObject *parent)
    : QObject(parent),
      m_unpacker(true)
{
    qRegisterMetaType<Ft8RxDecoder::Decode>("Ft8RxDecoder::Decode");
    qRegisterMetaType<Ft8RxDecoder::PerfStats>("Ft8RxDecoder::PerfStats");
    m_slotSamples.reserve(kSlotSamples + 4096);
}

Ft8RxDecoder::~Ft8RxDecoder()
{
    m_shutdown.store(true);
    for (std::future<void> &task : m_decodeTasks) {
        if (task.valid()) {
            task.wait();
        }
    }
}


void Ft8RxDecoder::reset()
{
    ++m_decodeGeneration;
    reapFinishedDecodeTasks();
    m_inputSampleRate = 0;
    m_resamplePos = 0.0;
    m_resamplePrefilterRate = 0;
    m_resamplePrefilterAlpha = 1.0;
    m_resampleLp1 = 0.0;
    m_resampleLp2 = 0.0;
    m_currentSlotId = -1;
    m_earlyDecodeSlotId = -1;
    m_streamingDecodeSlotId = -1;
    m_lastStreamingDecodeSamples = 0;
    m_finalDecodeLaunchedForSlot = false;
    m_postTxIgnoreSlotId = -1;
    m_initialUtcPadSamples = 0;
    {
        std::lock_guard<std::mutex> lock(m_emittedDecodeMutex);
        m_emittedDecodeKeys.clear();
    }
    m_currentSlotStartUtc = QDateTime();
    m_slotSamples.clear();
    m_lastCandidateCount = 0;
    emit statusChanged(currentShortLabel() + QStringLiteral(" RX: waiting for first slot audio"));
}

void Ft8RxDecoder::setSearchRangeHz(int lowHz, int highHz)
{
    m_searchLowHz = qBound(50, lowHz, 3500);
    m_searchHighHz = qBound(m_searchLowHz + 50, highHz, 3600);
}

void Ft8RxDecoder::setRxMarkerHz(int hz)
{
    m_rxMarkerHz = qBound(100, hz, 3200);
    // Like WSJT-X/MSHV, decode the full FT8 audio passband.  The green RX
    // marker remains the selected/QSO audio frequency, not a narrow decode gate.
    setSearchRangeHz(100, 3000);
}

void Ft8RxDecoder::setMyCall(const QString &call)
{
    m_myCall = call.trimmed().toUpper();
    std::lock_guard<std::mutex> lock(m_unpackMutex);
    m_unpacker.save_hash_call_my_his_r1_r2(m_myCall, 0);
}

void Ft8RxDecoder::setDxCall(const QString &call)
{
    m_dxCall = call.trimmed().toUpper();
    std::lock_guard<std::mutex> lock(m_unpackMutex);
    m_unpacker.save_hash_call_my_his_r1_r2(m_dxCall, 1);
}

void Ft8RxDecoder::setModeName(const QString &modeName)
{
    const QString key = modeName.trimmed().toUpper();
    const QString next = (key == QStringLiteral("FT4")) ? QStringLiteral("FT4") : QStringLiteral("FT8");
    if (m_modeName == next) {
        return;
    }
    m_modeName = next;
    reset();
}

QString Ft8RxDecoder::modeName() const
{
    return m_modeName;
}

void Ft8RxDecoder::setDecodeEngine(const QString &engineName)
{
    Q_UNUSED(engineName)
    // v2.19: remove UI-selectable pseudo-engines.  Keep this slot for
    // backwards-compatible MainWindow/AppSettings calls, but force one
    // MSHV-derived native path.
    const QString next = QStringLiteral("mshv");
    if (m_decodeEngine == next) {
        return;
    }
    m_decodeEngine = next;
    reset();
}

QString Ft8RxDecoder::decodeEngine() const
{
    return m_decodeEngine;
}

bool Ft8RxDecoder::enhancedDecodeEngineEnabled() const
{
    // The old Decodium/Raptor enhanced branch was a strategy flavour, not a
    // complete separate decoder.  It is disabled in favour of one MSHV-style
    // pipeline that we can make faithful and testable.
    return false;
}

bool Ft8RxDecoder::deepDecodeEnabled() const
{
    return m_deepDecodeEnabled;
}

bool Ft8RxDecoder::dspPlusDecodeEnabled() const
{
    return m_dspPlusDecodeEnabled;
}

void Ft8RxDecoder::setDeepDecodeEnabled(bool enabled)
{
    Q_UNUSED(enabled)
    // v4.10: user-facing Fast/Deep/Deep Max modes are removed.  The FT8
    // receiver always runs the unified adaptive pipeline; older MainWindow and
    // settings calls may still arrive, but they must not downgrade the engine.
    if (!m_deepDecodeEnabled || !m_dspPlusDecodeEnabled) {
        m_deepDecodeEnabled = true;
        m_dspPlusDecodeEnabled = true;
        emit statusChanged(currentShortLabel() + QStringLiteral(" RX: Unified adaptive FT decoder enabled"));
        reset();
    }
}

void Ft8RxDecoder::setDspPlusDecodeEnabled(bool enabled)
{
    Q_UNUSED(enabled)
    // v4.10: this compatibility slot now means "enable the internal residual /
    // AP-OSD lab stage".  It is always on inside the single unified engine.
    if (!m_deepDecodeEnabled || !m_dspPlusDecodeEnabled) {
        m_deepDecodeEnabled = true;
        m_dspPlusDecodeEnabled = true;
        emit statusChanged(currentShortLabel() + QStringLiteral(" RX: Unified adaptive FT decoder enabled"));
        reset();
    }
}

int Ft8RxDecoder::currentSlotMs() const
{
    return Ft8Mode::profileForMode(m_modeName).slotMs;
}

int Ft8RxDecoder::currentSlotSamples() const
{
    return qMax(1, (currentSlotMs() * kDecodeSampleRate) / 1000);
}

QString Ft8RxDecoder::currentShortLabel() const
{
    return Ft8Mode::profileForMode(m_modeName).shortLabel;
}


int Ft8RxDecoder::postTxPrepadLimitMs() const
{
    // A TX audio/PTT release that is only slightly late after the UTC boundary
    // can still leave a useful FT RX slot.  Preserve its absolute timing by
    // inserting leading zero samples.  If we are much later than this, the slot
    // is considered too partial and is ignored until the next boundary.
    return (m_modeName == QStringLiteral("FT4")) ? 1200 : 2500;
}

void Ft8RxDecoder::beginUtcSlot(qint64 slotId, int maxPrepadMs, const QString &reason)
{
    const int slotMs = qMax(1000, currentSlotMs());
    const qint64 startMs = slotId * static_cast<qint64>(slotMs);
    const qint64 nowMs = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
    const qint64 elapsedMs = qBound<qint64>(qint64{0}, nowMs - startMs, static_cast<qint64>(slotMs - 1));

    m_currentSlotId = slotId;
    m_earlyDecodeSlotId = -1;
    m_streamingDecodeSlotId = -1;
    m_lastStreamingDecodeSamples = 0;
    m_finalDecodeLaunchedForSlot = false;
    m_currentSlotStartUtc = QDateTime::fromMSecsSinceEpoch(startMs, Qt::UTC);
    m_slotSamples.clear();
    m_initialUtcPadSamples = 0;

    if (maxPrepadMs > 0 && elapsedMs > 0 && elapsedMs <= maxPrepadMs) {
        const int prepadSamples = qBound(0,
                                         static_cast<int>((elapsedMs * kDecodeSampleRate) / 1000),
                                         qMax(0, currentSlotSamples() - 1));
        if (prepadSamples > 0) {
            m_slotSamples.fill(0.0, prepadSamples);
            m_initialUtcPadSamples = prepadSamples;
        }
    }

    if (!reason.isEmpty()) {
        if (m_initialUtcPadSamples > 0) {
            emit statusChanged(currentShortLabel() + QStringLiteral(" RX: %1; UTC-aligned with %2 ms pre-pad")
                                   .arg(reason)
                                   .arg(static_cast<int>((1000LL * m_initialUtcPadSamples) / kDecodeSampleRate)));
        } else {
            emit statusChanged(currentShortLabel() + QStringLiteral(" RX: %1").arg(reason));
        }
    }
}

bool Ft8RxDecoder::isPostTxIgnoredSlot() const
{
    return m_currentSlotId >= 0 && m_postTxIgnoreSlotId == m_currentSlotId;
}


void Ft8RxDecoder::noteTransmitStarting(qint64 txSlotBoundaryUtcMs)
{
    // The TX scheduler can legitimately stop the sound-card input exactly at the
    // selected UTC boundary.  If no further RX audio block arrives after that
    // boundary, maybeRotateSlot() would never get a chance to close/decode the
    // just-finished RX slot.  Finalize it explicitly here before MainWindow
    // mutes RX and starts the FT transmitter.
    const int slotMs = qMax(1000, currentSlotMs());
    const qint64 nowMs = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
    const qint64 txSlotId = (txSlotBoundaryUtcMs > 0)
        ? (txSlotBoundaryUtcMs / static_cast<qint64>(slotMs))
        : (nowMs / static_cast<qint64>(slotMs));
    const qint64 previousRxSlotId = txSlotId - 1;

    reapFinishedDecodeTasks();

    if (m_currentSlotId == previousRxSlotId && !isPostTxIgnoredSlot()) {
        finishCurrentSlot();
        emit statusChanged(currentShortLabel() +
                           QStringLiteral(" RX: TX boundary reached; finalized previous RX slot before TX"));
    }
}

void Ft8RxDecoder::noteTransmitEnded(qint64 txSlotBoundaryUtcMs)
{
    const int slotMs = qMax(1000, currentSlotMs());
    const qint64 nowMs = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
    const qint64 currentSlotId = nowMs / slotMs;
    const qint64 txSlotId = (txSlotBoundaryUtcMs > 0) ? (txSlotBoundaryUtcMs / slotMs) : -1;

    reapFinishedDecodeTasks();

    if (txSlotId >= 0 && currentSlotId == txSlotId) {
        // Normal case: FT8/FT4 message audio ends before the transmit slot is
        // over.  Restart the sound card for waterfall/level feedback, but do
        // not feed our own TX tail or the final quiet tail into the weak-signal
        // decoder and do not emit "slot skipped, not enough audio" for it.
        beginUtcSlot(currentSlotId, 0, QString());
        m_postTxIgnoreSlotId = currentSlotId;
        emit statusChanged(currentShortLabel() + QStringLiteral(" RX: post-TX guard active until next UTC slot"));
        return;
    }

    // If TX/driver/PTT release arrived after the next UTC boundary, keep the
    // current slot only when the missing beginning is still small enough to be
    // decodable.  Leading zero pre-pad preserves the real WSJT-X/MSHV time axis;
    // without it, a late RX restart shifts the whole slot and candidate timing.
    const qint64 slotStartMs = currentSlotId * static_cast<qint64>(slotMs);
    const qint64 elapsedMs = qBound<qint64>(qint64{0}, nowMs - slotStartMs, static_cast<qint64>(slotMs - 1));
    if (elapsedMs <= postTxPrepadLimitMs()) {
        m_postTxIgnoreSlotId = -1;
        beginUtcSlot(currentSlotId,
                     postTxPrepadLimitMs(),
                     QStringLiteral("post-TX RX restart inside current slot"));
        return;
    }

    beginUtcSlot(currentSlotId, 0, QString());
    m_postTxIgnoreSlotId = currentSlotId;
    emit statusChanged(currentShortLabel() + QStringLiteral(" RX: post-TX restart was too late for this slot; waiting for next UTC slot"));
}

void Ft8RxDecoder::configureResamplePrefilter(int sampleRate)
{
    if (sampleRate <= 0 || sampleRate == m_resamplePrefilterRate) {
        return;
    }

    m_resamplePrefilterRate = sampleRate;
    m_resampleLp1 = 0.0;
    m_resampleLp2 = 0.0;

    /* WSJT-X/MSHV style: before moving the live audio into the 12 kHz
     * weak-signal domain, keep a real low-pass guard in front of the sample-rate
     * conversion.  The main conditioner already limits FT audio to roughly
     * 100..3300 Hz; this extra two-pole guard catches host/driver artefacts
     * and prevents linear interpolation from folding high audio junk into the
     * Costas candidate search. */
    const double cutoffHz = qBound(3000.0, 4800.0, static_cast<double>(sampleRate) * 0.42);
    m_resamplePrefilterAlpha = qBound(0.0001,
                                      1.0 - std::exp(-kTwoPi * cutoffHz / static_cast<double>(sampleRate)),
                                      0.98);
}

QVector<double> Ft8RxDecoder::resampleTo12k(const AudioBlock &block)
{
    QVector<double> out;
    if (block.samples.size() < 2 || block.sampleRate <= 0) {
        return out;
    }

    if (m_inputSampleRate != block.sampleRate) {
        m_inputSampleRate = block.sampleRate;
        m_resamplePos = 0.0;
        configureResamplePrefilter(block.sampleRate);
    }

    QVector<double> filtered;
    filtered.resize(block.samples.size());
    for (int i = 0; i < block.samples.size(); ++i) {
        const double x = qBound(-1.0, static_cast<double>(block.samples.at(i)), 1.0);
        m_resampleLp1 += m_resamplePrefilterAlpha * (x - m_resampleLp1);
        m_resampleLp2 += m_resamplePrefilterAlpha * (m_resampleLp1 - m_resampleLp2);
        filtered[i] = m_resampleLp2;
    }

    const double step = static_cast<double>(block.sampleRate) / static_cast<double>(kDecodeSampleRate);
    const int n = filtered.size();
    out.reserve(static_cast<int>(std::ceil(n / step)) + 2);

    double pos = m_resamplePos;
    while (pos + 1.0 < static_cast<double>(n)) {
        const int i = static_cast<int>(pos);
        const double frac = pos - static_cast<double>(i);
        const double a = filtered.at(i);
        const double b = filtered.at(i + 1);
        out.append(a + (b - a) * frac);
        pos += step;
    }

    m_resamplePos = pos - static_cast<double>(n);
    if (m_resamplePos < 0.0 || m_resamplePos > step * 2.0) {
        m_resamplePos = 0.0;
    }
    return out;
}


QVector<double> Ft8RxDecoder::offlineResampleTo12k(const QVector<float> &samples, int sampleRate) const
{
    QVector<double> out;
    if (samples.isEmpty() || sampleRate <= 0) {
        return out;
    }
    if (sampleRate == kDecodeSampleRate) {
        out.reserve(samples.size());
        for (float v : samples) {
            out.append(static_cast<double>(v));
        }
        return out;
    }

    const double step = static_cast<double>(sampleRate) / static_cast<double>(kDecodeSampleRate);
    const int estimated = static_cast<int>(std::floor(static_cast<double>(samples.size()) / step));
    out.reserve(qMax(0, estimated));
    for (double pos = 0.0; pos + 1.0 < static_cast<double>(samples.size()); pos += step) {
        const int i = static_cast<int>(pos);
        const double frac = pos - static_cast<double>(i);
        const double a = static_cast<double>(samples.at(i));
        const double b = static_cast<double>(samples.at(i + 1));
        out.append(a + (b - a) * frac);
    }
    return out;
}

void Ft8RxDecoder::analyzeAudioFile(const QString &filePath)
{
    reapFinishedDecodeTasks();
    if (!m_decodeTasks.empty()) {
        emit offlineAnalysisFinished(filePath, false, 0, QStringLiteral("FT decoder is busy"));
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit offlineAnalysisFinished(filePath, false, 0, file.errorString());
        return;
    }

    OfflineWavFormat wav;
    QString error;
    if (!parseOfflineWavHeader(file, wav, error)) {
        emit offlineAnalysisFinished(filePath, false, 0, error);
        return;
    }
    if (!file.seek(wav.dataOffset)) {
        emit offlineAnalysisFinished(filePath, false, 0, QStringLiteral("cannot seek to WAV data"));
        return;
    }

    emit statusChanged(QStringLiteral("%1 offline WAV: loading %2 Hz, %3 channel(s), %4-bit")
                           .arg(currentShortLabel())
                           .arg(wav.sampleRate)
                           .arg(wav.channels)
                           .arg(wav.bitsPerSample));

    QVector<float> mono;
    const int framesPerChunk = 16384;
    const qint64 preferredBytes = static_cast<qint64>(framesPerChunk) * static_cast<qint64>(wav.blockAlign);
    qint64 remaining = wav.dataSize;
    mono.reserve(static_cast<int>(qMin<qint64>(wav.dataSize / qMax<quint16>(quint16{1}, wav.blockAlign), static_cast<qint64>(20) * 60 * wav.sampleRate)));

    while (remaining > 0 && !m_shutdown.load()) {
        qint64 bytesToRead = qMin(remaining, preferredBytes);
        bytesToRead -= bytesToRead % static_cast<qint64>(wav.blockAlign);
        if (bytesToRead <= 0) {
            break;
        }
        const QByteArray raw = file.read(bytesToRead);
        if (raw.isEmpty()) {
            break;
        }
        mono += convertOfflineWavToMono(raw, wav);
        remaining -= raw.size();
    }

    if (mono.isEmpty()) {
        emit offlineAnalysisFinished(filePath, false, 0, QStringLiteral("WAV contains no readable audio"));
        return;
    }

    ++m_decodeGeneration;
    {
        std::lock_guard<std::mutex> lock(m_emittedDecodeMutex);
        m_emittedDecodeKeys.clear();
    }

    double peak = 0.0;
    double sumSquares = 0.0;
    int clippedSamples = 0;
    for (float sample : mono) {
        const double v = static_cast<double>(sample);
        const double a = std::abs(v);
        peak = qMax(peak, a);
        sumSquares += v * v;
        if (a >= 0.995) {
            ++clippedSamples;
        }
    }
    const double rms = mono.isEmpty() ? 0.0 : std::sqrt(sumSquares / static_cast<double>(mono.size()));
    const double durationSec = (wav.sampleRate > 0)
        ? static_cast<double>(mono.size()) / static_cast<double>(wav.sampleRate)
        : 0.0;

    const QVector<double> resampled = offlineResampleTo12k(mono, static_cast<int>(wav.sampleRate));
    if (resampled.isEmpty()) {
        emit offlineAnalysisFinished(filePath, false, 0, QStringLiteral("resampling produced no audio"));
        return;
    }

    const int slotSamples = currentSlotSamples();
    const int slotMs = currentSlotMs();
    const int slotCount = qMax(1, static_cast<int>(std::ceil(static_cast<double>(resampled.size()) / static_cast<double>(slotSamples))));
    const QDateTime baseUtc = QDateTime::fromMSecsSinceEpoch(0, Qt::UTC);
    int totalCandidates = 0;
    int totalDecodes = 0;
    int totalClampedLowReports = 0;
    int zeroDecodeSlots = 0;
    int analyzedSlots = 0;
    double totalMs = 0.0;

    const bool ft4LiveEngineTest = (m_modeName == QStringLiteral("FT4"));
    emit statusChanged(QStringLiteral("%1 offline WAV: decoding %2 live slot(s), %3 mode")
                           .arg(currentShortLabel())
                           .arg(slotCount)
                           .arg(m_deepDecodeEnabled ? QStringLiteral("adaptive") : QStringLiteral("single-pass")));

    // Manual/offline FT analysis must exercise the same decoder core used on air. FT8 keeps
    // the historical offline reference budget for the 88-decode regression set;
    // FT4 deliberately does NOT enable the offline/deep special path because it
    // would test a different engine than live RX.
    m_offlineAnalysisActive.store(!ft4LiveEngineTest);

    for (int slot = 0; slot < slotCount && !m_shutdown.load(); ++slot) {
        QVector<double> slotData = resampled.mid(slot * slotSamples, slotSamples);
        if (slotData.size() < slotSamples / 3) {
            break;
        }
        if (slotData.size() != slotSamples) {
            slotData.resize(slotSamples);
        }
        int candidateCount = 0;
        PerfStats stats;
        const QDateTime slotUtc = baseUtc.addMSecs(static_cast<qint64>(slot) * slotMs);
        stats.modeName = m_modeName;
        stats.slotUtc = formatSlotTime(slotUtc, slotMs);
        stats.phase = ft4LiveEngineTest ? QStringLiteral("offline-live-slot") : QStringLiteral("offline");
        stats.offline = true;
        QVector<Decode> decodes = decodeSlot(slotData, slotUtc, &candidateCount, &stats);
        ++analyzedSlots;
        totalCandidates += candidateCount;
        totalMs += stats.totalMs;

        int emitted = 0;
        for (const Decode &decode : decodes) {
            if (markDecodeEmitted(decode, slotUtc)) {
                ++emitted;
                ++totalDecodes;
                if (decode.snrDb <= -25) {
                    ++totalClampedLowReports;
                }
                emit decodeReady(decode);
            }
        }
        if (emitted == 0) {
            ++zeroDecodeSlots;
        }
        stats.decodeCount = emitted;
        emit performanceUpdated(stats);
        emit statusChanged(QStringLiteral("%1 offline live slot %2/%3: %4 candidate(s), %5 decode(s), %6 ms")
                               .arg(currentShortLabel())
                               .arg(slot + 1)
                               .arg(slotCount)
                               .arg(candidateCount)
                               .arg(emitted)
                               .arg(QString::number(stats.totalMs, 'f', 0)));
        QCoreApplication::processEvents();
    }

    m_offlineAnalysisActive.store(false);

    QString summary = QStringLiteral("%1 decode(s), %2 candidate(s), %3 ms total; WAV %4 Hz, %5 ch, %6-bit, %7 s, peak %8, RMS %9, clipped %10; slots %11, zero-decode slots %12, -25 dB clamp %13; engine %14")
        .arg(totalDecodes)
        .arg(totalCandidates)
        .arg(QString::number(totalMs, 'f', 0))
        .arg(wav.sampleRate)
        .arg(wav.channels)
        .arg(wav.bitsPerSample)
        .arg(QString::number(durationSec, 'f', 2))
        .arg(QString::number(peak, 'f', 3))
        .arg(QString::number(rms, 'f', 4))
        .arg(clippedSamples)
        .arg(analyzedSlots)
        .arg(zeroDecodeSlots)
        .arg(totalClampedLowReports)
        .arg(ft4LiveEngineTest ? QStringLiteral("live-slot") : QStringLiteral("offline-reference"));

    if (totalDecodes == 0 && totalCandidates > 0) {
        summary += QStringLiteral("; candidates were found but no CRC-valid FT message decoded — this is a real live-engine miss, not a special offline-window result");
    }

    emit offlineAnalysisFinished(filePath, true, totalDecodes, summary);
}

int Ft8RxDecoder::wsjtxDecodeGateSamples() const
{
    // v2.90: use the live MSHV/WSJT-style early decode gate, not a late
    // boundary batch.  MSHV's DisplayMs comments describe the FT8 live cadence
    // as roughly 11800 -> 13500 -> 14500 ms; the practical first complete FT8
    // codeword is available around 13.5 s because the 79-symbol waveform starts
    // after the standard 0.5 s transmit delay.  FT4 is similarly launched around
    // 6.1 s.  Starting here gives the sequencer one slot-transition window back
    // on old dual-core PCs.
    if (m_modeName == QStringLiteral("FT4")) {
        return qRound(6.10 * static_cast<double>(kDecodeSampleRate));
    }
    return qRound(13.50 * static_cast<double>(kDecodeSampleRate));
}

void Ft8RxDecoder::processAudioBlock(const AudioBlock &block)
{
    reapFinishedDecodeTasks();
    maybeRotateSlot();

    if (isPostTxIgnoredSlot()) {
        return;
    }

    const QVector<double> resampled = resampleTo12k(block);
    if (!resampled.isEmpty()) {
        const int slotSamples = currentSlotSamples();
        const int overflowLimit = slotSamples + kDecodeSampleRate * 2;

        // Keep the same overflow policy as before (newest slot-sized material
        // after a backend burst), but do it without QVector::mid(), which
        // allocates and copies a fresh vector in the live audio path.
        if (resampled.size() >= slotSamples) {
            if (m_slotSamples.capacity() < slotSamples) {
                m_slotSamples.reserve(slotSamples + 4096);
            }
            m_slotSamples.resize(slotSamples);
            std::copy(resampled.constData() + (resampled.size() - slotSamples),
                      resampled.constData() + resampled.size(),
                      m_slotSamples.data());
        } else {
            const int projectedSize = m_slotSamples.size() + resampled.size();
            if (projectedSize > overflowLimit) {
                const int keepExisting = qMax(0, slotSamples - resampled.size());
                const int trimCount = qMax(0, m_slotSamples.size() - keepExisting);
                if (trimCount > 0) {
                    m_slotSamples.remove(0, trimCount);
                }
            }

            const int neededSize = m_slotSamples.size() + resampled.size();
            if (m_slotSamples.capacity() < neededSize) {
                m_slotSamples.reserve(qMax(neededSize, slotSamples + 4096));
            }
            m_slotSamples += resampled;
        }
        maybeStartStreamingDecodeSlot();
    }
}

void Ft8RxDecoder::maybeRotateSlot()
{
    const qint64 slotMs = qMax(1000, currentSlotMs());
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const qint64 nowMs = now.toMSecsSinceEpoch();
    const qint64 slotId = nowMs / slotMs;
    if (m_currentSlotId < 0) {
        beginUtcSlot(slotId, 0, QStringLiteral("collecting first timed slot"));
        return;
    }

    if (slotId != m_currentSlotId) {
        if (!isPostTxIgnoredSlot()) {
            finishCurrentSlot();
        }
        if (m_postTxIgnoreSlotId >= 0 && slotId != m_postTxIgnoreSlotId) {
            m_postTxIgnoreSlotId = -1;
        }
        // For normal live RX, compensate only for small scheduler/audio-callback
        // delays at the UTC boundary.  Larger gaps are handled explicitly by
        // noteTransmitEnded() so a late post-TX restart is not misinterpreted.
        beginUtcSlot(slotId, 250, QString());
    }
}

void Ft8RxDecoder::maybeStartEarlyDecodeSlot()
{
    // Kept for source compatibility with older MadModem branches.  v1.61 uses
    // maybeStartStreamingDecodeSlot(), which can launch several guarded live
    // passes instead of a single late early pass.
    maybeStartStreamingDecodeSlot();
}

void Ft8RxDecoder::maybeStartStreamingDecodeSlot()
{
    if (m_currentSlotId < 0 || m_slotSamples.isEmpty()) {
        return;
    }

    const int slotSamples = currentSlotSamples();
    const int availableSamples = qMin(m_slotSamples.size(), slotSamples);
    const int gateSamples = qMin(slotSamples, wsjtxDecodeGateSamples());

    // WSJT-X-like policy: one timed live decode per slot, launched at the same
    // class of symbol-count gate WSJT-X uses, rather than many speculative
    // overlapping passes.  This is the architectural fix for the observed
    // end-of-period CPU burst: keep RX collection continuous, but avoid stacking
    // redundant decode jobs that contend with sequencer/TX pre-arm.
    if (availableSamples < gateSamples) {
        return;
    }
    if (m_streamingDecodeSlotId == m_currentSlotId) {
        return;
    }

    reapFinishedDecodeTasks();
    if (!m_decodeTasks.empty()) {
        // Equivalent to WSJT-X asking jt9 to bail/finish before accepting more
        // work: MadModem's native decoder cannot be asynchronously aborted, so
        // we do not queue stale overlapping passes.
        emit statusChanged(currentShortLabel() + QStringLiteral(" RX: decoder busy at WSJT-X gate; not queueing overlapping pass"));
        return;
    }

    const QVector<double> snapshot = copyLeadingSamplesPadded(m_slotSamples, availableSamples, false);

    m_streamingDecodeSlotId = m_currentSlotId;
    m_lastStreamingDecodeSamples = availableSamples;
    m_earlyDecodeSlotId = m_currentSlotId;
    const int percent = qBound(0, static_cast<int>((100.0 * availableSamples) / qMax(1, slotSamples)), 100);
    startAsyncDecodeSlot(snapshot, m_currentSlotStartUtc, QStringLiteral("wsjtx-gate %1%" ).arg(percent));
}

void Ft8RxDecoder::finishCurrentSlot()
{
    if (isPostTxIgnoredSlot()) {
        return;
    }

    if (m_finalDecodeLaunchedForSlot) {
        return;
    }

    if (m_streamingDecodeSlotId == m_currentSlotId) {
        // v4.13k: realtime live policy.  The WSJT-X-gated pass is the pass
        // used by the live sequencer.  The previous v4.13 unified branch then
        // launched a second full boundary decode for the same slot whenever
        // the gate task had already finished; in real radio logs this usually
        // re-found the same messages, added 0 new rows, and burned about
        // another second right at the TX decision boundary.  Skip that redundant
        // final pass.  Full/deeper analysis remains available via offline WAV
        // analysis; live RX keeps the scheduler and AutoQSO responsive.
        reapFinishedDecodeTasks();
        if (!m_decodeTasks.empty()) {
            emit statusChanged(currentShortLabel() + QStringLiteral(" live decode boundary: gate pass still running; final pass skipped for slot %1")
                                   .arg(formatSlotTime(m_currentSlotStartUtc, currentSlotMs())));
        } else {
            emit statusChanged(currentShortLabel() + QStringLiteral(" live decode boundary: final pass skipped for slot %1; WSJT-X gate pass already handled live RX")
                                   .arg(formatSlotTime(m_currentSlotStartUtc, currentSlotMs())));
        }
        m_finalDecodeLaunchedForSlot = true;
        return;
    }

    const int slotSamples = currentSlotSamples();
    const int minSamples = (m_modeName == QStringLiteral("FT4")) ? (kDecodeSampleRate * 3) : (kDecodeSampleRate * 8);
    if (m_slotSamples.size() < minSamples) {
        emit statusChanged(currentShortLabel() + QStringLiteral(" RX: slot skipped, not enough audio"));
        return;
    }

    reapFinishedDecodeTasks();
    if (!m_decodeTasks.empty()) {
        emit statusChanged(currentShortLabel() + QStringLiteral(" RX: boundary reached but decoder is still busy; no overlapping final pass"));
        return;
    }

    const QVector<double> slot = copyLeadingSamplesPadded(m_slotSamples, slotSamples, true);

    m_finalDecodeLaunchedForSlot = true;
    startAsyncDecodeSlot(slot, m_currentSlotStartUtc, QStringLiteral("boundary"));
}

void Ft8RxDecoder::startAsyncDecodeSlot(const QVector<double> &samples, const QDateTime &slotStartUtc, const QString &phaseLabel)
{
    reapFinishedDecodeTasks();
    if (!m_decodeTasks.empty()) {
        emit statusChanged(currentShortLabel() + QStringLiteral(" RX: decoder busy, skipping overlapping %1 pass for slot %2")
                               .arg(phaseLabel.isEmpty() ? QStringLiteral("boundary") : phaseLabel)
                               .arg(formatSlotTime(slotStartUtc, currentSlotMs())));
        return;
    }

    const int generation = m_decodeGeneration.load();
    const QString label = currentShortLabel();
    const QString phase = phaseLabel.isEmpty() ? QStringLiteral("boundary") : phaseLabel;
    const QString phaseForLog = phase.startsWith(QStringLiteral("wsjtx-gate"))
        ? QStringLiteral("gate")
        : (phase == QStringLiteral("boundary") ? QStringLiteral("boundary") : phase);
    emit statusChanged(label + QStringLiteral(" live decode %1: slot %2 queued")
                           .arg(phaseForLog)
                           .arg(formatSlotTime(slotStartUtc, currentSlotMs())));

    m_decodeTasks.emplace_back(std::async(std::launch::async, [this, samples, slotStartUtc, generation, phase]() {
        int candidateCount = 0;
        PerfStats stats;
        stats.modeName = m_modeName;
        stats.slotUtc = formatSlotTime(slotStartUtc, currentSlotMs());
        stats.phase = phase;
        stats.offline = m_offlineAnalysisActive.load();
        const QVector<Decode> decodes = decodeSlot(samples, slotStartUtc, &candidateCount, &stats);

        if (m_shutdown.load() || generation != m_decodeGeneration.load()) {
            return;
        }

        int emittedCount = 0;
        for (const Decode &decode : decodes) {
            if (markDecodeEmitted(decode, slotStartUtc)) {
                ++emittedCount;
                emit decodeReady(decode);
            }
        }

        stats.decodeCount = emittedCount;
        emit performanceUpdated(stats);
        if (phase.startsWith(QStringLiteral("wsjtx-gate"))) {
            emit statusChanged(QStringLiteral("%1 live decode gate: slot %2, %3 candidate(s), %4 decode(s), %5 ms")
                                   .arg(m_modeName)
                                   .arg(formatSlotTime(slotStartUtc, currentSlotMs()))
                                   .arg(candidateCount)
                                   .arg(emittedCount)
                                   .arg(QString::number(stats.totalMs, 'f', 0)));
        } else if (phase == QStringLiteral("boundary")) {
            emit statusChanged(QStringLiteral("%1 live decode boundary: slot %2, %3 candidate(s), added %4 extra decode(s), %5 ms")
                                   .arg(m_modeName)
                                   .arg(formatSlotTime(slotStartUtc, currentSlotMs()))
                                   .arg(candidateCount)
                                   .arg(emittedCount)
                                   .arg(QString::number(stats.totalMs, 'f', 0)));
        } else {
            emit statusChanged(QStringLiteral("%1 RX %2: %3 candidate(s), %4 decode(s), %5 ms in slot %6").arg(m_modeName)
                                   .arg(phase)
                                   .arg(candidateCount)
                                   .arg(emittedCount)
                                   .arg(QString::number(stats.totalMs, 'f', 0))
                                   .arg(formatSlotTime(slotStartUtc, currentSlotMs())));
        }
    }));
}

bool Ft8RxDecoder::markDecodeEmitted(const Decode &decode, const QDateTime &slotStartUtc)
{
    const int roundedHz = qRound(static_cast<double>(decode.frequencyHz) / 5.0) * 5;
    const QString key = QString::number(slotStartUtc.toMSecsSinceEpoch()) + QLatin1Char('|') +
                        QString::number(roundedHz) + QLatin1Char('|') +
                        decode.message.trimmed().toUpper();
    std::lock_guard<std::mutex> lock(m_emittedDecodeMutex);
    if (m_emittedDecodeKeys.contains(key)) {
        return false;
    }
    if (m_emittedDecodeKeys.size() > 512) {
        const qint64 cutoffMs = slotStartUtc.addSecs(-90).toMSecsSinceEpoch();
        QVector<QString> staleKeys;
        staleKeys.reserve(m_emittedDecodeKeys.size());
        for (const QString &existingKey : m_emittedDecodeKeys) {
            const int sep = existingKey.indexOf(QLatin1Char('|'));
            bool ok = false;
            const qint64 existingSlotMs = sep > 0 ? existingKey.left(sep).toLongLong(&ok) : 0;
            if (!ok || existingSlotMs < cutoffMs) {
                staleKeys.append(existingKey);
            }
        }
        for (const QString &staleKey : staleKeys) {
            m_emittedDecodeKeys.remove(staleKey);
        }

        if (m_emittedDecodeKeys.size() > 768) {
            std::vector<std::pair<qint64, QString>> ordered;
            ordered.reserve(static_cast<size_t>(m_emittedDecodeKeys.size()));
            for (const QString &existingKey : m_emittedDecodeKeys) {
                const int sep = existingKey.indexOf(QLatin1Char('|'));
                bool ok = false;
                const qint64 existingSlotMs = sep > 0 ? existingKey.left(sep).toLongLong(&ok) : 0;
                ordered.emplace_back(ok ? existingSlotMs : 0, existingKey);
            }
            std::sort(ordered.begin(), ordered.end(), [](const auto &a, const auto &b) {
                return a.first < b.first;
            });
            const int removeCount = qMax(0, static_cast<int>(ordered.size()) - 512);
            for (int i = 0; i < removeCount; ++i) {
                m_emittedDecodeKeys.remove(ordered[static_cast<size_t>(i)].second);
            }
        }
    }
    m_emittedDecodeKeys.insert(key);
    return true;
}

void Ft8RxDecoder::reapFinishedDecodeTasks()
{
    auto it = m_decodeTasks.begin();
    while (it != m_decodeTasks.end()) {
        if (!it->valid() || it->wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            if (it->valid()) {
                it->wait();
            }
            it = m_decodeTasks.erase(it);
        } else {
            ++it;
        }
    }
}

QVector<Ft8RxDecoder::Decode> Ft8RxDecoder::decodeSlot(const QVector<double> &samples,
                                                       const QDateTime &slotStartUtc,
                                                       int *candidateCount,
                                                       PerfStats *stats)
{
    if (m_modeName == QStringLiteral("FT4")) {
        return decodeSlotFt4(samples, slotStartUtc, candidateCount, stats);
    }

    using Clock = std::chrono::steady_clock;
    const auto totalStart = Clock::now();

    struct CandidateDecode
    {
        Candidate candidate;
        Decode decode;
    };

    auto betterDecode = [](const Decode &a, const Decode &b) {
        if (a.snrDb != b.snrDb) {
            return a.snrDb > b.snrDb;
        }
        if (a.syncScore != b.syncScore) {
            return a.syncScore > b.syncScore;
        }
        return std::abs(a.dt) < std::abs(b.dt);
    };

    std::atomic<int> diagAttemptedCandidates {0};
    std::atomic<int> diagBoundaryRejects {0};
    std::atomic<int> diagSoftMetricRejects {0};
    std::atomic<int> diagSyncGateRejects {0};
    std::atomic<int> diagLdpcTried {0};
    std::atomic<int> diagLdpcFailures {0};
    std::atomic<int> diagCrcFailures {0};
    std::atomic<int> diagUnpackFailures {0};
    std::atomic<int> diagMessageRejects {0};

    std::mutex diagOsdMutex;
    int diagOsdGf2Tried = 0;
    int diagOsdGf2Recovered = 0;
    int diagOsdGf2RankFails = 0;
    int diagOsdGf2PivotSkips = 0;
    int diagOsdGf2Order0Hits = 0;
    int diagOsdGf2Order1Hits = 0;
    int diagOsdGf2Order2Hits = 0;
    int diagOsdGf2PostCrcRejects = 0;
    int diagOsdGf2BudgetSkips = 0;
    double diagOsdGf2TotalMs = 0.0;

    std::mutex diagQualityMutex;
    int diagDecodedQualityCount = 0;
    int diagLdpcFailureQualityCount = 0;
    double diagDecodedSyncSum = 0.0;
    double diagLdpcFailureSyncSum = 0.0;
    double diagDecodedHardSyncSum = 0.0;
    double diagLdpcFailureHardSyncSum = 0.0;
    double diagDecodedLlrAbsSum = 0.0;
    double diagLdpcFailureLlrAbsSum = 0.0;

    auto noteQuality = [&diagQualityMutex,
                        &diagDecodedQualityCount,
                        &diagLdpcFailureQualityCount,
                        &diagDecodedSyncSum,
                        &diagLdpcFailureSyncSum,
                        &diagDecodedHardSyncSum,
                        &diagLdpcFailureHardSyncSum,
                        &diagDecodedLlrAbsSum,
                        &diagLdpcFailureLlrAbsSum](bool decoded, const CandidateAttemptQuality &quality) {
        if (!quality.valid) {
            return;
        }
        std::lock_guard<std::mutex> lock(diagQualityMutex);
        if (decoded) {
            ++diagDecodedQualityCount;
            diagDecodedSyncSum += quality.syncScore;
            diagDecodedHardSyncSum += static_cast<double>(quality.hardSyncCount);
            diagDecodedLlrAbsSum += quality.meanAbsLlr;
        } else {
            ++diagLdpcFailureQualityCount;
            diagLdpcFailureSyncSum += quality.syncScore;
            diagLdpcFailureHardSyncSum += static_cast<double>(quality.hardSyncCount);
            diagLdpcFailureLlrAbsSum += quality.meanAbsLlr;
        }
    };

    auto noteOsdQuality = [&diagOsdMutex,
                           &diagOsdGf2Tried,
                           &diagOsdGf2Recovered,
                           &diagOsdGf2RankFails,
                           &diagOsdGf2PivotSkips,
                           &diagOsdGf2Order0Hits,
                           &diagOsdGf2Order1Hits,
                           &diagOsdGf2Order2Hits,
                           &diagOsdGf2PostCrcRejects,
                           &diagOsdGf2BudgetSkips,
                           &diagOsdGf2TotalMs](const CandidateAttemptQuality &quality) {
        if (quality.osdGf2Tried <= 0 && quality.osdGf2BudgetSkips <= 0) {
            return;
        }
        std::lock_guard<std::mutex> lock(diagOsdMutex);
        diagOsdGf2Tried += quality.osdGf2Tried;
        diagOsdGf2Recovered += quality.osdGf2Recovered;
        diagOsdGf2RankFails += quality.osdGf2RankFails;
        diagOsdGf2PivotSkips += quality.osdGf2PivotSkips;
        diagOsdGf2Order0Hits += quality.osdGf2Order0Hits;
        diagOsdGf2Order1Hits += quality.osdGf2Order1Hits;
        diagOsdGf2Order2Hits += quality.osdGf2Order2Hits;
        diagOsdGf2PostCrcRejects += quality.osdGf2PostCrcRejects;
        diagOsdGf2BudgetSkips += quality.osdGf2BudgetSkips;
        diagOsdGf2TotalMs += quality.osdGf2TotalMs;
    };

    auto noteRejectReason = [&diagBoundaryRejects,
                             &diagSoftMetricRejects,
                             &diagSyncGateRejects,
                             &diagLdpcFailures,
                             &diagCrcFailures,
                             &diagUnpackFailures,
                             &diagMessageRejects](DecodeRejectReason reason) {
        switch (reason) {
        case DecodeRejectReason::Boundary:
            ++diagBoundaryRejects;
            break;
        case DecodeRejectReason::SoftMetric:
            ++diagSoftMetricRejects;
            break;
        case DecodeRejectReason::SyncGate:
            ++diagSyncGateRejects;
            break;
        case DecodeRejectReason::Ldpc:
            ++diagLdpcFailures;
            break;
        case DecodeRejectReason::Crc:
            ++diagCrcFailures;
            break;
        case DecodeRejectReason::Unpack:
            ++diagUnpackFailures;
            break;
        case DecodeRejectReason::Message:
            ++diagMessageRejects;
            break;
        case DecodeRejectReason::None:
            break;
        }
    };

    auto decodeCandidateSet = [this, &slotStartUtc, &betterDecode, &diagAttemptedCandidates, &diagLdpcTried, &noteRejectReason, &noteQuality, &noteOsdQuality](const QVector<double> &slotSamples,
                                                                                                               const QVector<Candidate> &candidateSet,
                                                                                                               int *workerCountOut) {
        QVector<CandidateDecode> rawPairs;
        if (candidateSet.isEmpty()) {
            if (workerCountOut != nullptr) {
                *workerCountOut = 0;
            }
            return rawPairs;
        }

        const bool offline = m_offlineAnalysisActive.load();
        const int workerCount = FtDecodeWorkerPool::instance().recommendedWorkerCount(offline, candidateSet.size());
        if (workerCountOut != nullptr) {
            *workerCountOut = workerCount;
        }

        /*
         * 0.5.1: GF(2) OSD is now useful, but alpha24 proved
         * that unbounded OSD spends too much CPU. Keep it as a tactical
         * recovery pass: cap the number of OSD candidates and the summed
         * OSD worker time per candidate set. Parallel workers may overshoot
         * slightly, but the cap prevents alpha24-style 100+ ms bursts.
         */
        const int osdGf2TryLimit = offline ? 16 : 8;
        const int osdGf2BudgetTenthsMs = offline ? 600 : 250;
        std::atomic<int> osdGf2TriedInSet {0};
        std::atomic<int> osdGf2TenthsMsInSet {0};

        std::mutex rawMutex;
        std::mutex diagMutex;
        std::atomic<int> nextCandidate {0};
        // v4.12: keep the persistent FT worker pool from v4.11, but restore
        // dynamic candidate stealing inside the pool.  LDPC/metric retries have
        // highly variable cost; static chunks can leave one worker stuck on a
        // hard cluster while the others are idle.  This keeps the no-thread-churn
        // benefit while recovering the load balance of the old atomic scheduler.
        FtDecodeWorkerPool::instance().parallelFor(workerCount, workerCount, [this, &slotSamples, &slotStartUtc, &candidateSet, &rawPairs, &rawMutex, &diagMutex, &nextCandidate, &diagAttemptedCandidates, &diagLdpcTried, &noteRejectReason, &noteQuality, &noteOsdQuality, &osdGf2TriedInSet, &osdGf2TenthsMsInSet, osdGf2TryLimit, osdGf2BudgetTenthsMs](int, int) {
            QVector<CandidateDecode> localPairs;
            localPairs.reserve(8);
            int localAttempted = 0;
            int localLdpcTried = 0;
            QVector<DecodeRejectReason> localRejects;
            QVector<CandidateAttemptQuality> localDecodedQualities;
            QVector<CandidateAttemptQuality> localLdpcFailureQualities;
            QVector<CandidateAttemptQuality> localOsdQualities;
            localRejects.reserve(64);
            for (;;) {
                const int i = nextCandidate.fetch_add(1);
                if (i >= candidateSet.size()) {
                    break;
                }
                ++localAttempted;
                Decode decode;
                const Candidate candidate = candidateSet.at(i);
                Candidate refinedCandidate = candidate;
                DecodeRejectReason rejectReason = DecodeRejectReason::None;
                CandidateAttemptQuality quality;
                const bool allowGf2OsdForCandidate =
                        osdGf2TriedInSet.load(std::memory_order_relaxed) < osdGf2TryLimit &&
                        osdGf2TenthsMsInSet.load(std::memory_order_relaxed) < osdGf2BudgetTenthsMs;
                const bool decoded = decodeCandidate(slotSamples,
                                                     slotStartUtc,
                                                     candidate,
                                                     decode,
                                                     &refinedCandidate,
                                                     &rejectReason,
                                                     &quality,
                                                     true,
                                                     allowGf2OsdForCandidate);
                if (quality.osdGf2Tried > 0) {
                    osdGf2TriedInSet.fetch_add(quality.osdGf2Tried, std::memory_order_relaxed);
                    osdGf2TenthsMsInSet.fetch_add(qMax(1, static_cast<int>(std::lround(quality.osdGf2TotalMs * 10.0))),
                                                  std::memory_order_relaxed);
                }
                if (decoded || rejectReason == DecodeRejectReason::Ldpc ||
                    rejectReason == DecodeRejectReason::Crc ||
                    rejectReason == DecodeRejectReason::Unpack ||
                    rejectReason == DecodeRejectReason::Message) {
                    ++localLdpcTried;
                }
                if (quality.osdGf2Tried > 0 || quality.osdGf2BudgetSkips > 0) {
                    localOsdQualities.append(quality);
                }
                if (decoded) {
                    localDecodedQualities.append(quality);
                    CandidateDecode pair;
                    pair.candidate = refinedCandidate;
                    pair.decode = decode;
                    localPairs.append(pair);
                } else {
                    localRejects.append(rejectReason);
                    if (rejectReason == DecodeRejectReason::Ldpc) {
                        localLdpcFailureQualities.append(quality);
                    }
                }
            }

            {
                std::lock_guard<std::mutex> lock(diagMutex);
                diagAttemptedCandidates += localAttempted;
                diagLdpcTried += localLdpcTried;
                for (const CandidateAttemptQuality &q : localDecodedQualities) {
                    noteQuality(true, q);
                }
                for (const CandidateAttemptQuality &q : localOsdQualities) {
                    noteOsdQuality(q);
                }
                for (const DecodeRejectReason reason : localRejects) {
                    noteRejectReason(reason);
                }
                for (const CandidateAttemptQuality &q : localLdpcFailureQualities) {
                    noteQuality(false, q);
                }
            }

            if (!localPairs.isEmpty()) {
                std::lock_guard<std::mutex> lock(rawMutex);
                for (const CandidateDecode &pair : localPairs) {
                    rawPairs.append(pair);
                }
            }
        });

        std::sort(rawPairs.begin(), rawPairs.end(), [&betterDecode](const CandidateDecode &a, const CandidateDecode &b) {
            if (a.decode.message == b.decode.message && std::abs(a.decode.frequencyHz - b.decode.frequencyHz) < 10) {
                return betterDecode(a.decode, b.decode);
            }
            if (a.candidate.rankScore != b.candidate.rankScore) {
                return a.candidate.rankScore > b.candidate.rankScore;
            }
            return a.decode.frequencyHz < b.decode.frequencyHz;
        });
        return rawPairs;
    };

    auto deduplicate = [&betterDecode](const QVector<CandidateDecode> &rawPairs, int *droppedOut) {
        QVector<CandidateDecode> deduped;
        int dropped = 0;
        for (const CandidateDecode &pair : rawPairs) {
            int existingIndex = -1;
            for (int i = 0; i < deduped.size(); ++i) {
                const Decode &existing = deduped.at(i).decode;
                if (existing.message == pair.decode.message &&
                    std::abs(existing.frequencyHz - pair.decode.frequencyHz) <= 12 &&
                    std::abs(existing.dt - pair.decode.dt) <= 0.45) {
                    existingIndex = i;
                    break;
                }
            }

            if (existingIndex < 0) {
                deduped.append(pair);
                continue;
            }

            ++dropped;
            if (betterDecode(pair.decode, deduped.at(existingIndex).decode)) {
                deduped[existingIndex] = pair;
            }
        }
        if (droppedOut != nullptr) {
            *droppedOut = dropped;
        }
        return deduped;
    };

    auto alreadyDecoded = [](const QVector<CandidateDecode> &decoded, const Candidate &c) {
        for (const CandidateDecode &d : decoded) {
            if (std::abs(d.candidate.baseHz - c.baseHz) < 25.0 &&
                std::abs(d.candidate.startSec - c.startSec) < 0.35) {
                return true;
            }
        }
        return false;
    };

    auto sameDecodedSignal = [](const CandidateDecode &a, const CandidateDecode &b) {
        if (a.decode.message.trimmed().toUpper() != b.decode.message.trimmed().toUpper()) {
            return false;
        }
        return std::abs(a.decode.frequencyHz - b.decode.frequencyHz) <= 12.0 &&
               std::abs(a.decode.dt - b.decode.dt) <= 0.45;
    };

    auto strongestList = [](QVector<CandidateDecode> decoded, int maxCount) {
        std::sort(decoded.begin(), decoded.end(), [](const CandidateDecode &a, const CandidateDecode &b) {
            if (a.decode.snrDb != b.decode.snrDb) {
                return a.decode.snrDb > b.decode.snrDb;
            }
            return a.decode.syncScore > b.decode.syncScore;
        });
        const int count = qMin(maxCount, decoded.size());
        if (decoded.size() > count) {
            decoded.resize(count);
        }
        return decoded;
    };

    auto subtractDecodeList = [this](QVector<double> &cleaned, const QVector<CandidateDecode> &decoded) {
        for (const CandidateDecode &pair : decoded) {
            subtractDecodedSignal(cleaned, pair.candidate, pair.decode);
        }
        return decoded.size();
    };

    auto subtractStrongest = [&strongestList, &subtractDecodeList](QVector<double> &cleaned, QVector<CandidateDecode> decoded, int maxCount) {
        const QVector<CandidateDecode> selected = strongestList(decoded, maxCount);
        return subtractDecodeList(cleaned, selected);
    };

    QVector<CandidateDecode> rawPairs;
    QVector<CandidateDecode> decodedSoFar;
    QVector<CandidateDecode> alreadySubtractedFromWorking;
    QVector<double> working = samples;
    int firstWorkerCount = 0;
    int totalCandidates = 0;
    int secondPassCandidates = 0;
    int dedupDropped = 0;
    double searchMs = 0.0;
    double decodeMs = 0.0;
    double subtractionMs = 0.0;
    int passCount = 0;
    bool timeBudgetHit = false;
    QString earlyStopReason;

    const bool offlineAnalysis = m_offlineAnalysisActive.load();
    const QString decodePhase = (stats != nullptr) ? stats->phase : QString();
    const bool liveRealtimeDecode = !offlineAnalysis &&
                                    (decodePhase.startsWith(QStringLiteral("wsjtx-gate")) ||
                                     decodePhase == QStringLiteral("boundary"));
    const bool offlineFastReference = offlineAnalysis &&
                                      !m_deepDecodeEnabled &&
                                      !m_dspPlusDecodeEnabled;

    // v2.89: follow MSHV's practical live policy instead of doing blind rescue
    // passes. In decoderft8.cpp, subtraction/rescan passes are useful only after
    // earlier passes have produced decodes worth subtracting. Do not burn CPU on
    // pass 2/3/4 when pass 1 found nothing. Offline WAV analysis keeps the wide
    // regression-test path.
    const struct PassSpec { double threshold; int maxCandidates; int maxSubtract; } passes[4] = {
        offlineAnalysis
            ? (offlineFastReference ? PassSpec{1.40, 360, 0}
                                    : PassSpec{1.30, 900, 64})
            : (m_dspPlusDecodeEnabled
                ? PassSpec{1.26, 340, 48}
                : (m_deepDecodeEnabled ? PassSpec{1.30, 300, 40} : PassSpec{1.44, 128, 0})),
        offlineAnalysis
            ? PassSpec{1.18, 950, 0}
            : (m_dspPlusDecodeEnabled ? PassSpec{1.03, 380, 0} : PassSpec{1.08, 340, 0}),
        offlineAnalysis
            ? PassSpec{1.10, 0, 0}
            : PassSpec{0.99, 0, 0},
        offlineAnalysis ? PassSpec{1.06, 0, 0} : PassSpec{0.98, 0, 0}
    };
    // v4.13l: live RX is no longer "deep always" and no longer "fast only".
    // The first WSJT-X-gated pass stays as light as v4.13k, then a second
    // targeted pass is allowed only when the slot itself says it is worth it:
    // A) candidate pressure, B) overlap/pile-up, C) active-QSO context,
    // D) useful CQ/value context, E) real time budget.
    const int requestedPasses = offlineAnalysis
        ? ((m_dspPlusDecodeEnabled || m_deepDecodeEnabled) ? 2 : 1)
        : (liveRealtimeDecode ? 2 : 1);
    int decodesBeforePass = 0;
    bool liveAdaptiveDeepTriggered = false;
    bool liveAllowResidual = false;
    QStringList liveAdaptiveReasons;
    constexpr double kLiveAdaptiveBudgetMs = 480.0;
    constexpr double kLiveSecondPassLatestStartMs = 330.0;
    constexpr double kLiveResidualLatestStartMs = 405.0;
    auto elapsedLiveMs = [&totalStart]() {
        return std::chrono::duration<double, std::milli>(Clock::now() - totalStart).count();
    };

    for (int pass = 0; pass < requestedPasses; ++pass) {
        // v3.22: no blind/weak-rescue passes.  WSJT-X/MSHV subtraction
        // passes are decode-driven: if a previous pass produced no CRC-valid
        // signal, there is nothing reference-like to subtract or rescan.
        // This keeps Adaptive/Deep from spending CPU on non-reference guesses
        // and prevents late live decodes from being biased toward speculative
        // candidates.
        if (pass > 0 && decodedSoFar.isEmpty()) {
            if (!(liveRealtimeDecode && liveAdaptiveDeepTriggered && elapsedLiveMs() < kLiveAdaptiveBudgetMs)) {
                break;
            }
        }
        if (pass > 1 && decodedSoFar.size() <= decodesBeforePass && !m_dspPlusDecodeEnabled) {
            break;
        }
        decodesBeforePass = decodedSoFar.size();
        const auto searchStart = Clock::now();
        QVector<Candidate> passCandidates = findCandidates(working, passes[pass].threshold);
        QVector<Candidate> filtered;
        filtered.reserve(passCandidates.size());
        for (const Candidate &c : passCandidates) {
            if (pass > 0 && alreadyDecoded(decodedSoFar, c)) {
                continue;
            }
            filtered.append(c);
            if (filtered.size() >= passes[pass].maxCandidates) {
                break;
            }
        }
        const auto searchEnd = Clock::now();
        searchMs += std::chrono::duration<double, std::milli>(searchEnd - searchStart).count();
        totalCandidates += filtered.size();
        if (pass == 1) {
            secondPassCandidates = filtered.size();
        }
        if (filtered.isEmpty()) {
            continue;
        }

        int workersThisPass = 0;
        const auto decodeStart = Clock::now();
        QVector<CandidateDecode> passPairs = decodeCandidateSet(working, filtered, &workersThisPass);
        const auto decodeEnd = Clock::now();
        decodeMs += std::chrono::duration<double, std::milli>(decodeEnd - decodeStart).count();
        firstWorkerCount = qMax(firstWorkerCount, workersThisPass);
        for (const CandidateDecode &pair : passPairs) {
            rawPairs.append(pair);
        }
        decodedSoFar = deduplicate(rawPairs, &dedupDropped);
        passCount = pass + 1;

        if (liveRealtimeDecode && pass == 0) {
            const double elapsedAfterFast = elapsedLiveMs();
            const int firstPassDecodes = decodedSoFar.size();
            const int attempted = diagAttemptedCandidates.load();
            const int syncGateRejects = diagSyncGateRejects.load();
            const int ldpcTried = diagLdpcTried.load();
            const int ldpcFailures = diagLdpcFailures.load();
            const QString targetCall = m_dxCall.trimmed().toUpper();
            const bool haveTargetContext = !targetCall.isEmpty();

            auto messageMentions = [](const QString &message, const QString &call) {
                if (call.isEmpty()) {
                    return false;
                }
                const QString upper = message.toUpper();
                int pos = upper.indexOf(call);
                while (pos >= 0) {
                    const int before = pos - 1;
                    const int after = pos + call.size();
                    const bool leftOk = before < 0 || !upper.at(before).isLetterOrNumber();
                    const bool rightOk = after >= upper.size() || !upper.at(after).isLetterOrNumber();
                    if (leftOk && rightOk) {
                        return true;
                    }
                    pos = upper.indexOf(call, pos + 1);
                }
                return false;
            };

            bool targetDecoded = false;
            int cqDecodeCount = 0;
            for (const CandidateDecode &pair : decodedSoFar) {
                const QString msg = pair.decode.message.trimmed().toUpper();
                if (msg.startsWith(QStringLiteral("CQ "))) {
                    ++cqDecodeCount;
                }
                if (messageMentions(msg, targetCall)) {
                    targetDecoded = true;
                }
            }

            bool nearRxFocusCandidate = false;
            bool overlapPressure = false;
            int candidatesNearDecoded = 0;
            QVector<Candidate> byFreq = filtered;
            std::sort(byFreq.begin(), byFreq.end(), [](const Candidate &a, const Candidate &b) {
                return a.baseHz < b.baseHz;
            });
            for (int i = 1; i < byFreq.size(); ++i) {
                const double df = std::abs(byFreq.at(i).baseHz - byFreq.at(i - 1).baseHz);
                const double dt = std::abs(byFreq.at(i).startSec - byFreq.at(i - 1).startSec);
                if (df <= 28.0 && dt <= 0.70) {
                    overlapPressure = true;
                    break;
                }
            }
            for (const Candidate &candidate : filtered) {
                if (std::abs(candidate.baseHz - static_cast<double>(m_rxMarkerHz)) <= 85.0) {
                    nearRxFocusCandidate = true;
                }
                for (const CandidateDecode &pair : decodedSoFar) {
                    const double df = std::abs(candidate.baseHz - pair.candidate.baseHz);
                    const double dt = std::abs(candidate.startSec - pair.candidate.startSec);
                    if (df > 8.0 && df <= 55.0 && dt <= 0.80) {
                        ++candidatesNearDecoded;
                        if (candidatesNearDecoded >= 3) {
                            overlapPressure = true;
                            break;
                        }
                    }
                }
                if (overlapPressure) {
                    break;
                }
            }

            const bool triggerA = (filtered.size() >= 315) ||
                                  (syncGateRejects >= 210 && ldpcTried >= 18) ||
                                  (ldpcFailures >= 12 && firstPassDecodes <= 6) ||
                                  (attempted >= 240 && firstPassDecodes <= 2);
            const bool triggerB = overlapPressure && !decodedSoFar.isEmpty();
            const bool triggerC = haveTargetContext && !targetDecoded &&
                                  (nearRxFocusCandidate || filtered.size() >= 190 || ldpcTried >= 10);
            const bool triggerD = (cqDecodeCount == 0 && filtered.size() >= 230 && ldpcTried >= 10) ||
                                  (firstPassDecodes <= 2 && filtered.size() >= 260);
            const bool triggerE = elapsedAfterFast <= kLiveSecondPassLatestStartMs &&
                                  elapsedAfterFast < kLiveAdaptiveBudgetMs;

            if (triggerA && triggerE) {
                liveAdaptiveReasons.append(QStringLiteral("A:candidate-pressure"));
            }
            if (triggerB && triggerE) {
                liveAdaptiveReasons.append(QStringLiteral("B:overlap"));
                liveAllowResidual = true;
            }
            if (triggerC && triggerE) {
                liveAdaptiveReasons.append(QStringLiteral("C:qso-target"));
                liveAllowResidual = true;
            }
            if (triggerD && triggerE) {
                liveAdaptiveReasons.append(QStringLiteral("D:cq-value"));
            }
            if (!triggerE) {
                earlyStopReason = QStringLiteral("live adaptive budget: fast pass already used %1 ms").arg(QString::number(elapsedAfterFast, 'f', 0));
            }

            liveAdaptiveDeepTriggered = !liveAdaptiveReasons.isEmpty();
            if (!liveAdaptiveDeepTriggered) {
                if (earlyStopReason.isEmpty()) {
                    earlyStopReason = QStringLiteral("live fast: no A/B/C/D trigger");
                }
                break;
            }
            earlyStopReason = QStringLiteral("live adaptive deep: %1").arg(liveAdaptiveReasons.join(QLatin1Char(',')));
        }

        if (liveRealtimeDecode && pass > 0 && elapsedLiveMs() >= kLiveAdaptiveBudgetMs) {
            timeBudgetHit = true;
            earlyStopReason = QStringLiteral("live adaptive budget hit after pass %1").arg(pass + 1);
            break;
        }

        if (pass + 1 < requestedPasses && passes[pass].maxSubtract > 0 && !decodedSoFar.isEmpty()) {
            if (liveRealtimeDecode && !liveAdaptiveDeepTriggered) {
                break;
            }
            if (liveRealtimeDecode && elapsedLiveMs() > kLiveSecondPassLatestStartMs) {
                timeBudgetHit = true;
                earlyStopReason = QStringLiteral("live adaptive budget: no time left for subtraction/rescan");
                break;
            }
            const auto subStart = Clock::now();
            working = samples;
            alreadySubtractedFromWorking = strongestList(decodedSoFar, liveRealtimeDecode ? 18 : passes[pass].maxSubtract);
            subtractDecodeList(working, alreadySubtractedFromWorking);
            const auto subEnd = Clock::now();
            subtractionMs += std::chrono::duration<double, std::milli>(subEnd - subStart).count();
        }

    }

    // v3.40 Deep Max isolated speed path.  Keep the v3.38/v3.33
    // Fast and normal Deep baseline untouched.  Only the optional Deep Max
    // residual scan is changed: fewer residual candidates are sent to LDPC,
    // and the heavy ft8b-style multi-metric retry is reserved for overlap or
    // top-ranked candidates.  This is a cost reduction, not a wall-clock abort.
    if (((!liveRealtimeDecode) || (liveAdaptiveDeepTriggered && liveAllowResidual && elapsedLiveMs() <= kLiveResidualLatestStartMs)) &&
        m_dspPlusDecodeEnabled && !decodedSoFar.isEmpty()) {
        const bool liveAdaptiveResidual = liveRealtimeDecode && liveAdaptiveDeepTriggered && liveAllowResidual;
        const auto subStart = Clock::now();
        // v4.12: residual recovery reuses the pass-2 working buffer whenever
        // possible.  v4.10/v4.11 rebuilt the residual from the original slot and
        // re-subtracted all baseline decodes, paying the same SIC cost twice.
        // Here the decodes already removed before pass 2 remain removed; only
        // newly decoded/not-yet-subtracted signals are subtracted.
        QVector<double> residual = alreadySubtractedFromWorking.isEmpty() ? samples : working;
        QVector<CandidateDecode> missingSubtract;
        const QVector<CandidateDecode> strongestResidualDecodes = strongestList(decodedSoFar, offlineAnalysis ? 80 : 48);
        for (const CandidateDecode &pair : strongestResidualDecodes) {
            bool alreadySubtracted = false;
            for (const CandidateDecode &oldPair : alreadySubtractedFromWorking) {
                if (sameDecodedSignal(pair, oldPair)) {
                    alreadySubtracted = true;
                    break;
                }
            }
            if (!alreadySubtracted) {
                missingSubtract.append(pair);
            }
        }
        subtractDecodeList(residual, missingSubtract);
        const auto subEnd = Clock::now();
        subtractionMs += std::chrono::duration<double, std::milli>(subEnd - subStart).count();

        const auto searchStart = Clock::now();
        QVector<Candidate> residualCandidates = findCandidates(residual,
                                                                 offlineAnalysis ? 1.10 : (liveAdaptiveResidual ? 1.12 : 1.04));
        const auto searchEnd = Clock::now();
        searchMs += std::chrono::duration<double, std::milli>(searchEnd - searchStart).count();

        auto residualProximity = [&decodedSoFar](const Candidate &c) {
            double proximity = 0.0;
            for (const CandidateDecode &d : decodedSoFar) {
                const double df = std::abs(d.candidate.baseHz - c.baseHz);
                const double dt = std::abs(d.candidate.startSec - c.startSec);
                if (df <= 25.0 && dt <= 0.45) {
                    proximity = qMax(proximity, 1.0);
                } else if (df <= 40.0 && dt <= 0.60) {
                    proximity = qMax(proximity, 0.75);
                } else if (df <= 70.0 && dt <= 0.80) {
                    proximity = qMax(proximity, 0.35);
                }
            }
            return proximity;
        };

        auto residualRank = [&residualProximity](const Candidate &c) {
            const double proximity = residualProximity(c);
            return c.rankScore * (1.0 + 0.90 * proximity);
        };
        std::sort(residualCandidates.begin(), residualCandidates.end(), [&residualRank](const Candidate &a, const Candidate &b) {
            return residualRank(a) > residualRank(b);
        });

        // v4.12: restore enough residual breadth to recover the second
        // overlapping test_21 signal lost in v4.11, but rely on residual reuse
        // above to avoid re-paying the full SIC cost.
        const int residualCandidateCap = offlineAnalysis ? 56 : (liveAdaptiveResidual ? 18 : 34);
        const int residualLdpcCap = offlineAnalysis ? 34 : (liveAdaptiveResidual ? 8 : 22);
        const int residualHeavyMetricCap = offlineAnalysis ? 18 : (liveAdaptiveResidual ? 4 : 10);
        const int residualNonProximityCap = offlineAnalysis ? 18 : (liveAdaptiveResidual ? 3 : 10);
        int residualLdpcUsed = 0;
        int residualTried = 0;
        int residualHeavyMetricUsed = 0;
        int residualAccepted = 0;

        QVector<Candidate> selectedResidualCandidates;
        selectedResidualCandidates.reserve(qMin(residualCandidateCap, residualCandidates.size()));
        int residualNonProximitySelected = 0;
        for (const Candidate &candidate : residualCandidates) {
            if (selectedResidualCandidates.size() >= residualCandidateCap) {
                break;
            }
            const double proximity = residualProximity(candidate);
            if (proximity <= 0.0) {
                if (residualNonProximitySelected >= residualNonProximityCap) {
                    continue;
                }
                ++residualNonProximitySelected;
            }
            selectedResidualCandidates.append(candidate);
        }

        totalCandidates += selectedResidualCandidates.size();
        secondPassCandidates += selectedResidualCandidates.size();

        auto alreadyHaveExactMessage = [](const QVector<CandidateDecode> &decoded, const QString &message) {
            const QString key = message.trimmed().toUpper();
            for (const CandidateDecode &d : decoded) {
                if (d.decode.message.trimmed().toUpper() == key) {
                    return true;
                }
            }
            return false;
        };

        for (const Candidate &candidate : selectedResidualCandidates) {
            if (residualLdpcUsed >= residualLdpcCap) {
                break;
            }
            if (liveAdaptiveResidual && elapsedLiveMs() >= kLiveAdaptiveBudgetMs) {
                timeBudgetHit = true;
                earlyStopReason = QStringLiteral("live adaptive budget hit during residual");
                break;
            }

            const double proximity = residualProximity(candidate);
            const bool allowHeavyMetricRecovery = residualHeavyMetricUsed < residualHeavyMetricCap &&
                                                  (proximity > 0.0 || residualTried < (offlineAnalysis ? 10 : 6));
            if (allowHeavyMetricRecovery) {
                ++residualHeavyMetricUsed;
            }

            ++residualTried;
            ++diagAttemptedCandidates;
            Decode decode;
            Candidate refinedCandidate = candidate;
            DecodeRejectReason rejectReason = DecodeRejectReason::None;
            CandidateAttemptQuality quality;
            const auto decodeStart = Clock::now();
            const bool decoded = decodeCandidate(residual,
                                                 slotStartUtc,
                                                 candidate,
                                                 decode,
                                                 &refinedCandidate,
                                                 &rejectReason,
                                                 &quality,
                                                 allowHeavyMetricRecovery,
                                                 (!liveAdaptiveResidual || elapsedLiveMs() < 500.0));
            const auto decodeEnd = Clock::now();
            decodeMs += std::chrono::duration<double, std::milli>(decodeEnd - decodeStart).count();

            if (decoded || rejectReason == DecodeRejectReason::Ldpc ||
                rejectReason == DecodeRejectReason::Crc ||
                rejectReason == DecodeRejectReason::Unpack ||
                rejectReason == DecodeRejectReason::Message) {
                ++diagLdpcTried;
                ++residualLdpcUsed;
            }

            if (quality.osdGf2Tried > 0 || quality.osdGf2BudgetSkips > 0) {
                noteOsdQuality(quality);
            }

            if (decoded) {
                noteQuality(true, quality);
                CandidateDecode pair;
                pair.candidate = refinedCandidate;
                pair.decode = decode;
                if (!alreadyHaveExactMessage(decodedSoFar, pair.decode.message)) {
                    rawPairs.append(pair);
                    decodedSoFar = deduplicate(rawPairs, &dedupDropped);
                    const auto oneSubStart = Clock::now();
                    subtractDecodedSignal(residual, pair.candidate, pair.decode);
                    const auto oneSubEnd = Clock::now();
                    subtractionMs += std::chrono::duration<double, std::milli>(oneSubEnd - oneSubStart).count();
                    ++residualAccepted;
                } else {
                    ++dedupDropped;
                }
            } else {
                noteRejectReason(rejectReason);
                if (rejectReason == DecodeRejectReason::Ldpc) {
                    noteQuality(false, quality);
                }
            }
        }

        if (residualAccepted > 0) {
            passCount = qMax(passCount, liveAdaptiveResidual ? 3 : 3);
        }
    }

    QVector<CandidateDecode> finalPairs = deduplicate(rawPairs, &dedupDropped);
    std::sort(finalPairs.begin(), finalPairs.end(), [](const CandidateDecode &a, const CandidateDecode &b) {
        if (a.decode.frequencyHz == b.decode.frequencyHz) {
            return a.decode.syncScore > b.decode.syncScore;
        }
        return a.decode.frequencyHz < b.decode.frequencyHz;
    });

    QVector<Decode> out;
    for (const CandidateDecode &pair : finalPairs) {
        out.append(pair.decode);
    }

    m_lastCandidateCount.store(totalCandidates);
    if (candidateCount != nullptr) {
        *candidateCount = totalCandidates;
    }

    if (stats != nullptr) {
        stats->candidateCount = totalCandidates;
        stats->decodeCount = out.size();
        stats->workerCount = firstWorkerCount;
        stats->candidateSearchMs = searchMs;
        stats->candidateDecodeMs = decodeMs;
        stats->totalMs = std::chrono::duration<double, std::milli>(Clock::now() - totalStart).count();
        stats->passCount = qMax(1, passCount);
        stats->secondPassCandidates = secondPassCandidates;
        stats->dedupDropped = dedupDropped;
        stats->subtractionMs = subtractionMs;
        stats->timeBudgetHit = timeBudgetHit;
        stats->earlyStopReason = earlyStopReason;
        stats->attemptedCandidates = diagAttemptedCandidates.load();
        stats->boundaryRejects = diagBoundaryRejects.load();
        stats->softMetricRejects = diagSoftMetricRejects.load();
        stats->syncGateRejects = diagSyncGateRejects.load();
        stats->ldpcTried = diagLdpcTried.load();
        stats->ldpcFailures = diagLdpcFailures.load();
        stats->crcFailures = diagCrcFailures.load();
        stats->unpackFailures = diagUnpackFailures.load();
        stats->messageRejects = diagMessageRejects.load();
        auto avgOrZero = [](double sum, int count) {
            return count > 0 ? (sum / static_cast<double>(count)) : 0.0;
        };
        stats->decodedQualityCount = diagDecodedQualityCount;
        stats->ldpcFailureQualityCount = diagLdpcFailureQualityCount;
        stats->decodedAvgSyncScore = avgOrZero(diagDecodedSyncSum, diagDecodedQualityCount);
        stats->ldpcFailureAvgSyncScore = avgOrZero(diagLdpcFailureSyncSum, diagLdpcFailureQualityCount);
        stats->decodedAvgHardSync = avgOrZero(diagDecodedHardSyncSum, diagDecodedQualityCount);
        stats->ldpcFailureAvgHardSync = avgOrZero(diagLdpcFailureHardSyncSum, diagLdpcFailureQualityCount);
        stats->decodedAvgLlrAbs = avgOrZero(diagDecodedLlrAbsSum, diagDecodedQualityCount);
        stats->ldpcFailureAvgLlrAbs = avgOrZero(diagLdpcFailureLlrAbsSum, diagLdpcFailureQualityCount);
        stats->osdGf2Tried = diagOsdGf2Tried;
        stats->osdGf2Recovered = diagOsdGf2Recovered;
        stats->osdGf2RankFails = diagOsdGf2RankFails;
        stats->osdGf2PivotSkips = diagOsdGf2PivotSkips;
        stats->osdGf2Order0Hits = diagOsdGf2Order0Hits;
        stats->osdGf2Order1Hits = diagOsdGf2Order1Hits;
        stats->osdGf2Order2Hits = diagOsdGf2Order2Hits;
        stats->osdGf2PostCrcRejects = diagOsdGf2PostCrcRejects;
        stats->osdGf2BudgetSkips = diagOsdGf2BudgetSkips;
        stats->osdGf2TotalMs = diagOsdGf2TotalMs;
        stats->engineName = liveRealtimeDecode
            ? (liveAdaptiveDeepTriggered
                ? QStringLiteral("FT8 Live Adaptive Residual")
                : QStringLiteral("FT8 Live Fast Gate"))
            : QStringLiteral("FT8 Unified Smart Residual");
        if (timeBudgetHit) {
            stats->engineName += QStringLiteral(" (budget-limited)");
        }
    }

    return out;
}

QVector<Ft8RxDecoder::Candidate> Ft8RxDecoder::findCandidates(const QVector<double> &samples, double threshold) const
{
    /*
     * v2.27: MSHV-style candidate search must also be MSHV-style in cost.
     *
     * v2.20-v2.26 used the right high-level idea (Costas sync, 3.125 Hz grid,
     * finer time grid), but implemented it by running a full 1920-sample
     * Goertzel for every candidate/start/frequency/tone.  That is the wrong
     * computational shape: it creates millions of long Goertzel evaluations per
     * slot and explains the slow WAV/deep-decode tests.
     *
     * MSHV builds a spectral matrix once, then scores candidates by indexing
     * the already-computed tone powers.  MM now follows that shape here: for
     * each DT hypothesis we FFT only the 21 Costas symbols, cache the passband
     * powers, and then score all base-frequency hypotheses from the cache.
     * The final LDPC candidate demodulator still uses the accurate per-candidate
     * Goertzel path, but only for the reduced candidate list.
     */
    QVector<Candidate> raw;
    if (samples.size() < kSymbols * kSamplesPerSymbol) {
        return raw;
    }

    const bool offlineAnalysis = m_offlineAnalysisActive.load();
    const bool liveAdaptive = !offlineAnalysis && (m_deepDecodeEnabled || m_dspPlusDecodeEnabled);
    // v4.13k: the old "liveDeep" candidate breadth was accidentally active in
    // normal live RX because MainWindow forced both compatibility flags on.
    // Keep full breadth for offline analysis, but live radio uses the adaptive
    // candidate matrix and lets the scheduler stay responsive.
    const bool liveDeep = false;
    const double kSyncMin = threshold;
    const double kTimeStepSec = offlineAnalysis ? 0.04 : (liveDeep ? 0.035 : (liveAdaptive ? 0.04 : 0.06));
    const double kFreqStepHz = offlineAnalysis ? 3.125 : (liveAdaptive ? 3.125 : 6.25);
    // v3.32: after the reference hard-sync bail-out, Deep can afford to look at
    // more sync-ranked candidates without feeding all of them into LDPC.  Fast
    // limits stay unchanged for live QSO timing.
    const int kMaxPreCandidates = offlineAnalysis ? 1500 : (liveDeep ? 1100 : (liveAdaptive ? 900 : 280));
    const int kMaxCandidates = offlineAnalysis ? 950 : (liveDeep ? 420 : (liveAdaptive ? 340 : 128));
    constexpr int kFftSize = 4096;           // power-of-two fast FFT; bin ~= 2.93 Hz
    constexpr double kFftBinHz = static_cast<double>(kDecodeSampleRate) / static_cast<double>(kFftSize);
    constexpr int kCostasSymbolCount = 21;

    const int lowHz = qMax(100, m_searchLowHz);
    const int highHz = qMin(3000, m_searchHighHz);
    const int maxEnergyBin = qMin(kFftSize / 2 - 2,
                                  static_cast<int>(std::ceil((highHz + 7.0 * kToneSpacingHz + 30.0) / kFftBinHz)) + 2);
    if (maxEnergyBin <= 0 || highHz <= lowHz) {
        return raw;
    }

    const int maxStart = qMin(samples.size() - kSymbols * kSamplesPerSymbol - 1,
                              qRound(3.00 * kDecodeSampleRate));
    if (maxStart <= 0) {
        return raw;
    }

    const int timeStepSamples = qMax(1, qRound(kTimeStepSec * kDecodeSampleRate));
    const int firstStart = 0;
    const int lastStart = maxStart;
    const int startCount = ((lastStart - firstStart) / timeStepSamples) + 1;

    struct CostasSpectrumCache
    {
        double binHz = 1.0;
        int rowSize = 0;
        std::vector<double> energy; // 21 contiguous rows; allocated once per worker chunk.

        void ensureCapacity(int binsPerRow)
        {
            rowSize = qMax(0, binsPerRow);
            const size_t required = static_cast<size_t>(21 * rowSize);
            if (energy.size() != required) {
                energy.resize(required);
            }
        }

        double *rowData(int costasIndex)
        {
            return energy.data() + static_cast<size_t>(costasIndex * rowSize);
        }

        double read(double freqHz, int costasIndex) const
        {
            if (costasIndex < 0 || costasIndex >= 21 || rowSize <= 0 || energy.empty()) {
                return 0.0;
            }
            const double *row = energy.data() + static_cast<size_t>(costasIndex * rowSize);
            const double pos = freqHz / binHz;
            int bin = static_cast<int>(std::floor(pos));
            const double frac = pos - static_cast<double>(bin);
            if (bin < 0) {
                return row[0];
            }
            if (bin + 1 >= rowSize) {
                return row[rowSize - 1];
            }
            return row[bin] * (1.0 - frac) + row[bin + 1] * frac;
        }
    };

    static const std::vector<double> costasHannWindow = []() {
        std::vector<double> window(static_cast<size_t>(kSamplesPerSymbol), 1.0);
        for (int n = 0; n < kSamplesPerSymbol; ++n) {
            window[static_cast<size_t>(n)] = 0.5 - 0.5 * std::cos(kTwoPi * static_cast<double>(n) /
                                                                 static_cast<double>(kSamplesPerSymbol - 1));
        }
        return window;
    }();

    auto buildCacheForStart = [&samples, maxEnergyBin](int startSample,
                                                                          CostasSpectrumCache &cache,
                                                                          std::vector<std::complex<double>> &fft) {
        cache.binHz = kFftBinHz;
        cache.ensureCapacity(maxEnergyBin + 2);
        int costasIndex = 0;
        for (int block = 0; block < 3; ++block) {
            const int syncStart = kCostasStarts[block];
            for (int i = 0; i < 7; ++i) {
                const int sym = syncStart + i;
                const int symStart = startSample + sym * kSamplesPerSymbol;
                std::fill(fft.begin(), fft.end(), std::complex<double>(0.0, 0.0));
                if (symStart >= 0 && symStart + kSamplesPerSymbol < samples.size()) {
                    const double *x = samples.constData() + symStart;
                    for (int n = 0; n < kSamplesPerSymbol; ++n) {
                        // Hann coefficients are precomputed once.  This removes
                        // 21 * startCount * 1920 cos() calls from findCandidates()
                        // and keeps the candidate matrix numerically identical.
                        fft[static_cast<size_t>(n)] = std::complex<double>(x[n] * costasHannWindow[static_cast<size_t>(n)], 0.0);
                    }
                    fftRadix2(fft);
                }

                double *row = cache.rowData(costasIndex);
                for (int bin = 0; bin <= maxEnergyBin + 1; ++bin) {
                    const std::complex<double> v = fft[static_cast<size_t>(bin)];
                    row[bin] = std::norm(v);
                }
                ++costasIndex;
            }
        }
    };

    auto scoreCandidate = [](const CostasSpectrumCache &cache, double baseHz, double *noiseOut) {
        double tABC = 0.0;
        double allABC = 0.0;
        double tBC = 0.0;
        double allBC = 0.0;
        int costasIndex = 0;

        for (int block = 0; block < 3; ++block) {
            for (int i = 0; i < 7; ++i) {
                const int expectedTone = kCostas[i];
                double expected = 0.0;
                double all = 0.0;
                for (int tone = 0; tone < 8; ++tone) {
                    const double e = cache.read(baseHz + tone * kToneSpacingHz, costasIndex);
                    all += e;
                    if (tone == expectedTone) {
                        expected = e;
                    }
                }
                tABC += expected;
                allABC += all;
                if (block >= 1) {
                    tBC += expected;
                    allBC += all;
                }
                ++costasIndex;
            }
        }

        const double offABC = qMax((allABC - tABC) / 7.0, kEps);
        const double offBC = qMax((allBC - tBC) / 7.0, kEps);
        const double syncABC = tABC / offABC;
        const double syncBC = tBC / offBC;
        const double sync = qMax(syncABC, syncBC);
        if (noiseOut != nullptr) {
            *noiseOut = (syncBC > syncABC) ? offBC : offABC;
        }
        return sync;
    };

    const bool offline = m_offlineAnalysisActive.load();
    // v4.12: persistent FT worker pool.  Split the start-time grid into chunks
    // instead of spawning std::async workers for every pass.
    const int workerCount = FtDecodeWorkerPool::instance().recommendedWorkerCount(offline, startCount);
    QVector<Candidate> merged;
    merged.reserve(kMaxPreCandidates);
    std::mutex mergedMutex;

    FtDecodeWorkerPool::instance().parallelFor(startCount, workerCount, [&](int begin, int end) {
        QVector<Candidate> local;
        local.reserve(128);
        CostasSpectrumCache cache;
        std::vector<std::complex<double>> fft(static_cast<size_t>(kFftSize));
        for (int startIndex = begin; startIndex < end; ++startIndex) {
            const int startSample = firstStart + startIndex * timeStepSamples;
            const double startSec = static_cast<double>(startSample) / static_cast<double>(kDecodeSampleRate);
            buildCacheForStart(startSample, cache, fft);
            for (double baseHz = static_cast<double>(lowHz);
                 baseHz <= static_cast<double>(highHz);
                 baseHz += kFreqStepHz) {
                double noise = 0.0;
                const double sync = scoreCandidate(cache, baseHz, &noise);
                if (!(sync >= kSyncMin) || !std::isfinite(sync)) {
                    continue;
                }

                Candidate c;
                c.score = sync;
                c.syncRatio = sync;
                c.syncNoisePower = noise;
                c.startSec = startSec;
                c.baseHz = baseHz;
                // v4.13a: the FT RX marker is a focus marker, not a gate.
                // Decode remains full-passband, but candidates close to the
                // selected/QSO frequency get a small rank boost so their LDPC
                // attempts are scheduled earlier and survive bucket pruning in
                // crowded slots.  The boost is capped and never excludes other
                // signals.
                const double focusDelta = std::abs(baseHz - static_cast<double>(m_rxMarkerHz));
                const double focusBoost = (focusDelta <= 140.0)
                    ? (0.18 * (1.0 - (focusDelta / 140.0)))
                    : 0.0;
                c.rankScore = sync + focusBoost;
                local.append(c);
            }
        }
        std::sort(local.begin(), local.end(), [](const Candidate &a, const Candidate &b) {
            return a.rankScore > b.rankScore;
        });
        if (local.size() > kMaxPreCandidates / workerCount + 16) {
            local.resize(kMaxPreCandidates / workerCount + 16);
        }
        if (!local.isEmpty()) {
            std::lock_guard<std::mutex> lock(mergedMutex);
            for (const Candidate &c : local) {
                merged.append(c);
            }
        }
    });

    std::sort(merged.begin(), merged.end(), [](const Candidate &a, const Candidate &b) {
        return a.rankScore > b.rankScore;
    });

    QVector<Candidate> candidates;
    candidates.reserve(kMaxCandidates);
    std::map<int, int> frequencyBucketUse;
    const double bucketHz = offlineAnalysis ? 18.0 : (liveDeep ? 22.0 : (liveAdaptive ? 28.0 : 45.0));
    const int bucketLimit = offlineAnalysis ? 10 : (liveDeep ? 7 : (liveAdaptive ? 5 : 3));
    for (const Candidate &c : merged) {
        bool duplicate = false;
        for (const Candidate &kept : candidates) {
            if (std::abs(kept.baseHz - c.baseHz) < 4.0 &&
                std::abs(kept.startSec - c.startSec) < 0.08) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            continue;
        }
        const int bucket = static_cast<int>(std::floor(c.baseHz / bucketHz));
        if (frequencyBucketUse[bucket] >= bucketLimit) {
            continue;
        }
        candidates.append(c);
        ++frequencyBucketUse[bucket];
        if (candidates.size() >= kMaxCandidates) {
            break;
        }
    }

    return candidates;
}

bool Ft8RxDecoder::decodeCandidate(const QVector<double> &samples,
                                   const QDateTime &slotStartUtc,
                                   const Candidate &candidate,
                                   Decode &decodeOut,
                                   Candidate *refinedCandidateOut,
                                   DecodeRejectReason *rejectReasonOut,
                                   CandidateAttemptQuality *qualityOut,
                                   bool allowMetricRecovery,
                                   bool allowGf2Osd)
{
    /*
     * v3.27: first real recovery step: keep the fast v3.25 candidate matrix,
     * but stop feeding the LDPC stage with the raw 40 ms / 3.125 Hz grid hit.
     * MSHV/WSJT-X refine DT and frequency before soft-symbol extraction; our
     * v3.25 path did not, so weak/tilted candidates often entered LDPC one
     * half-bin or a few milliseconds off.  This local Costas-only refinement
     * is intentionally small and decode-local: no audio, CAT, UI, sequencer or
     * subtraction policy is touched by this patch.
     */
    auto reject = [rejectReasonOut](DecodeRejectReason reason) {
        if (rejectReasonOut != nullptr) {
            *rejectReasonOut = reason;
        }
        return false;
    };
    if (rejectReasonOut != nullptr) {
        *rejectReasonOut = DecodeRejectReason::None;
    }
    if (qualityOut != nullptr) {
        *qualityOut = CandidateAttemptQuality();
    }

    auto costasSyncScore = [this, &samples](int startSample, double baseHz, int *hardSyncOut) {
        if (startSample < 0 || startSample + kSymbols * kSamplesPerSymbol + kSamplesPerSymbol >= samples.size()) {
            if (hardSyncOut != nullptr) {
                *hardSyncOut = 0;
            }
            return 0.0;
        }

        double tABC = 0.0;
        double allABC = 0.0;
        double tBC = 0.0;
        double allBC = 0.0;
        int hardSync = 0;

        for (int block = 0; block < 3; ++block) {
            const int blockStart = kCostasStarts[block];
            for (int i = 0; i < 7; ++i) {
                const int sym = blockStart + i;
                const int expectedTone = kCostas[i];
                const std::array<double, 8> energies = symbolToneEnergies8(samples,
                                                                            startSample + sym * kSamplesPerSymbol,
                                                                            baseHz);
                double all = 0.0;
                int bestTone = 0;
                double bestEnergy = energies[0];
                for (int tone = 0; tone < 8; ++tone) {
                    const double e = energies[tone];
                    all += e;
                    if (tone == 0 || e > bestEnergy) {
                        bestEnergy = e;
                        bestTone = tone;
                    }
                }
                if (bestTone == expectedTone) {
                    ++hardSync;
                }

                const double expected = energies[expectedTone];
                tABC += expected;
                allABC += all;
                if (block >= 1) {
                    tBC += expected;
                    allBC += all;
                }
            }
        }

        const double offABC = qMax((allABC - tABC) / 7.0, kEps);
        const double offBC = qMax((allBC - tBC) / 7.0, kEps);
        const double syncABC = tABC / offABC;
        const double syncBC = tBC / offBC;
        if (hardSyncOut != nullptr) {
            *hardSyncOut = hardSync;
        }
        return qMax(syncABC, syncBC);
    };

    int startSample = qRound(candidate.startSec * kDecodeSampleRate);
    double baseHz = candidate.baseHz;
    if (startSample < 0 || startSample + kSymbols * kSamplesPerSymbol + kSamplesPerSymbol >= samples.size()) {
        return reject(DecodeRejectReason::Boundary);
    }

    double refinedSync = costasSyncScore(startSample, baseHz, nullptr);

    auto tryRefine = [&](const std::initializer_list<int> sampleOffsets,
                         const std::initializer_list<double> freqOffsets) {
        int bestStart = startSample;
        double bestBaseHz = baseHz;
        double bestScore = refinedSync;
        for (int dt : sampleOffsets) {
            const int trialStart = startSample + dt;
            for (double df : freqOffsets) {
                const double trialBaseHz = baseHz + df;
                if (trialBaseHz < 50.0 || trialBaseHz > 5000.0) {
                    continue;
                }
                const double score = costasSyncScore(trialStart, trialBaseHz, nullptr);
                if (score > bestScore && std::isfinite(score)) {
                    bestScore = score;
                    bestStart = trialStart;
                    bestBaseHz = trialBaseHz;
                }
            }
        }
        startSample = bestStart;
        baseHz = bestBaseHz;
        refinedSync = bestScore;
    };

    // Stage 1: recover the half-grid timing error from the 40 ms candidate grid.
    tryRefine({-240, -120, 0, 120, 240}, {0.0});
    // Stage 2: WSJT-X/MSHV-like fine frequency nudge around the 3.125 Hz bin.
    tryRefine({0}, {-2.50, -1.25, 0.0, 1.25, 2.50});
    // Stage 3: final small DT clean-up after frequency correction.
    tryRefine({-60, -30, 0, 30, 60}, {0.0});

    if (startSample < 0 || startSample + kSymbols * kSamplesPerSymbol + kSamplesPerSymbol >= samples.size()) {
        return reject(DecodeRejectReason::Boundary);
    }

    Candidate refinedCandidate = candidate;
    refinedCandidate.startSec = static_cast<double>(startSample) / static_cast<double>(kDecodeSampleRate);
    refinedCandidate.baseHz = baseHz;
    refinedCandidate.score = qMax(candidate.score, refinedSync);
    refinedCandidate.rankScore = qMax(candidate.rankScore, refinedSync);
    refinedCandidate.syncRatio = refinedSync;
    refinedCandidate.refined = true;
    if (refinedCandidateOut != nullptr) {
        *refinedCandidateOut = refinedCandidate;
    }

    std::array<double, 174> llr{};
    int dataIndex = 0;
    double syncScore = 0.0;
    int hardSyncCount = 0;
    double totalPower = 0.0;
    std::array<std::array<double, 8>, kSymbols> symbolEnergies{};
    std::array<std::array<double, 8>, 58> dataMagnitudes{};
    double llrAbsSum = 0.0;
    int llrAbsCount = 0;

    for (int sym = 0; sym < kSymbols; ++sym) {
        const int symStart = startSample + sym * kSamplesPerSymbol;
        const std::array<double, 8> energies = symbolToneEnergies8(samples, symStart, baseHz);
        double maxEnergy = 0.0;
        double sumEnergy = 0.0;
        for (int tone = 0; tone < 8; ++tone) {
            symbolEnergies[sym][tone] = energies[tone];
            maxEnergy = std::max(maxEnergy, energies[tone]);
            sumEnergy += energies[tone];
        }
        totalPower += sumEnergy;

        if (isSyncSymbol(sym)) {
            int expectedTone = 0;
            if (sym < 7) {
                expectedTone = kCostas[sym];
            } else if (sym >= 36 && sym < 43) {
                expectedTone = kCostas[sym - 36];
            } else {
                expectedTone = kCostas[sym - 72];
            }
            int bestTone = 0;
            double bestEnergy = energies[0];
            for (int tone = 1; tone < 8; ++tone) {
                if (energies[tone] > bestEnergy) {
                    bestEnergy = energies[tone];
                    bestTone = tone;
                }
            }
            if (bestTone == expectedTone) {
                ++hardSyncCount;
            }
            syncScore += std::log((energies[expectedTone] + kEps) / ((sumEnergy - energies[expectedTone]) / 7.0 + kEps));
            continue;
        }

        if (dataIndex >= 58) {
            return reject(DecodeRejectReason::SoftMetric);
        }

        double metric[8];
        for (int tone = 0; tone < 8; ++tone) {
            const int idx = grayInverse(tone);
            metric[idx] = std::log(energies[tone] + kEps);
            dataMagnitudes[dataIndex][idx] = std::sqrt(qMax(energies[tone], 0.0));
        }

        for (int bit = 0; bit < 3; ++bit) {
            double best0 = -1.0e9;
            double best1 = -1.0e9;
            for (int idx = 0; idx < 8; ++idx) {
                const bool one = ((idx >> (2 - bit)) & 1) != 0;
                if (one) {
                    best1 = std::max(best1, metric[idx]);
                } else {
                    best0 = std::max(best0, metric[idx]);
                }
            }
            const double bitLlr = qBound(-18.0, (best0 - best1) * 2.2, 18.0);
            llr[dataIndex * 3 + bit] = bitLlr;
            llrAbsSum += std::abs(bitLlr);
            ++llrAbsCount;
        }
        ++dataIndex;
    }

    if (dataIndex != 58) {
        return reject(DecodeRejectReason::SoftMetric);
    }

    const double meanAbsLlr = (llrAbsCount > 0)
        ? (llrAbsSum / static_cast<double>(llrAbsCount))
        : 0.0;

    if (qualityOut != nullptr) {
        qualityOut->valid = true;
        qualityOut->syncScore = refinedSync;
        qualityOut->hardSyncCount = hardSyncCount;
        qualityOut->meanAbsLlr = meanAbsLlr;
    }

    /*
     * v3.32 synthesis: stop using LDPC as a trash filter.  WSJT-X/MSHV ft8b()
     * checks the hard Costas synchronization before calling the expensive LDPC
     * stage and returns early when nsync <= 6.  Offline diagnostics showed
     * LDPC failures averaging only about 4--5 correct Costas symbols out of 21,
     * while valid decodes are about 18--19/21.  This is not a new invented
     * heuristic; it is the reference ft8b bail-out placed at the same point in
     * the MadModem pipeline: after symbol extraction, before LDPC.
     */
    constexpr int hardSyncBailout = 6;
    if (hardSyncCount <= hardSyncBailout) {
        return reject(DecodeRejectReason::SyncGate);
    }

    /*
     * v4.13 LDPC load-shed gate.
     *
     * The v4.12 Auto-test profile showed that candidate search and subtract
     * are now cheap enough; the remaining hot block is LDPC.  Many failures
     * entering LDPC have only marginal hard Costas agreement and a weak mean
     * LLR.  Do not spend BP/min-sum iterations on those ghost candidates.
     *
     * This gate is deliberately conservative: it only rejects candidates just
     * above the WSJT-X nsync>6 gate and with very poor soft evidence.  Stronger
     * overlap/residual candidates still reach the normal and multi-metric paths.
     */
    const bool ldpcGhostCandidate = (hardSyncCount <= 8 && meanAbsLlr < 3.25) ||
                                    (hardSyncCount == 9 && meanAbsLlr < 2.65);
    if (ldpcGhostCandidate) {
        return reject(DecodeRejectReason::SoftMetric);
    }

    std::array<int, 174> bits{};
    int iterations = 0;
    QString message;

    auto rejectPriority = [](DecodeRejectReason reason) {
        switch (reason) {
        case DecodeRejectReason::Message: return 5;
        case DecodeRejectReason::Unpack: return 4;
        case DecodeRejectReason::Crc: return 3;
        case DecodeRejectReason::Ldpc: return 2;
        case DecodeRejectReason::SoftMetric: return 1;
        case DecodeRejectReason::Boundary:
        case DecodeRejectReason::SyncGate:
        case DecodeRejectReason::None:
            break;
        }
        return 0;
    };

    DecodeRejectReason bestFailure = DecodeRejectReason::Ldpc;
    auto rememberFailure = [&bestFailure, &rejectPriority](DecodeRejectReason reason) {
        if (rejectPriority(reason) > rejectPriority(bestFailure)) {
            bestFailure = reason;
        }
    };

    auto tryDecodeMetric = [&](const std::array<double, 174> &candidateLlr, bool allowOsdLite) {
        std::array<int, 174> candidateBits{};
        std::array<double, 174> posterior{};
        int candidateIterations = 0;
        bool decodedViaGf2Osd = false;
        int gf2OsdHitOrder = -1;
        if (!ldpcDecode174_91(candidateLlr, candidateBits, candidateIterations, &posterior)) {
            /*
             * 0.5.1 GF(2) OSD pivot-completion lab: use a true systematic
             * OSD fallback only after BP/min-sum failed, and only for
             * candidates that are plausible enough to justify the extra
             * algebra.  The normal LDPC path and the v4.10 OSD-lite path
             * remain intact.
             */
            const bool gf2BaseCandidate = hardSyncCount >= 10 && meanAbsLlr >= 3.8;
            const bool gf2OsdCandidate = allowOsdLite && allowMetricRecovery &&
                                         m_dspPlusDecodeEnabled &&
                                         gf2BaseCandidate;
            const bool tryGf2Osd = gf2OsdCandidate && allowGf2Osd;
            if (gf2OsdCandidate && !allowGf2Osd && qualityOut != nullptr) {
                ++qualityOut->osdGf2BudgetSkips;
            }
            bool osdSuccess = false;
            if (tryGf2Osd) {
                if (qualityOut != nullptr) {
                    ++qualityOut->osdGf2Tried;
                }
                const auto osdStart = std::chrono::steady_clock::now();
                int hitOrder = -1;
                bool rankFail = false;
                int pivotSkips = 0;
                /*
                 * 0.5.1: alpha24/25 proved that order-2 is not useful
                 * on the current WAV set, while the lost recoveries are order-1.
                 * Run complete order-1 over all 91 information bits, keep order-2
                 * disabled, and let the per-slot OSD budget decide how many
                 * candidates may enter this full single-bit repair.
                 */
                const int order1Depth = 91;
                const int order2Depth = 0;
                if (osdGf2Repair174_91(posterior,
                                        candidateBits,
                                        hitOrder,
                                        rankFail,
                                        pivotSkips,
                                        order1Depth,
                                        order2Depth)) {
                    osdSuccess = true;
                    decodedViaGf2Osd = true;
                    gf2OsdHitOrder = hitOrder;
                } else if (rankFail && qualityOut != nullptr) {
                    ++qualityOut->osdGf2RankFails;
                }
                if (qualityOut != nullptr) {
                    qualityOut->osdGf2PivotSkips += pivotSkips;
                }
                const auto osdEnd = std::chrono::steady_clock::now();
                if (qualityOut != nullptr) {
                    qualityOut->osdGf2TotalMs += std::chrono::duration<double, std::milli>(osdEnd - osdStart).count();
                }
            }

            if (!osdSuccess) {
                /*
                 * Keep the old tiny OSD repair as a conservative secondary
                 * fallback for the high-sync/high-LLR cases where it was
                 * already allowed.
                 */
                const bool osdLiteBaseAllowed = hardSyncCount >= 13 && meanAbsLlr >= 4.8;
                const bool osdLiteAllowed = allowOsdLite && allowMetricRecovery &&
                                            m_dspPlusDecodeEnabled &&
                                            osdLiteBaseAllowed;
                if (!osdLiteAllowed || !osdLiteRepair174_91(posterior, candidateBits)) {
                     rememberFailure(DecodeRejectReason::Ldpc);
                     return false;
                 }
            }
        }
        if (!crc14Ok(candidateBits)) {
            if (decodedViaGf2Osd && qualityOut != nullptr) {
                ++qualityOut->osdGf2PostCrcRejects;
            }
            rememberFailure(DecodeRejectReason::Crc);
            return false;
        }
        const QString candidateMessage = unpackMessage77(candidateBits).trimmed().toUpper();
        if (candidateMessage.isEmpty()) {
            if (decodedViaGf2Osd && qualityOut != nullptr) {
                ++qualityOut->osdGf2PostCrcRejects;
            }
            rememberFailure(DecodeRejectReason::Unpack);
            return false;
        }
        if (!looksLikeUsefulFt8Message(candidateMessage)) {
            if (decodedViaGf2Osd && qualityOut != nullptr) {
                ++qualityOut->osdGf2PostCrcRejects;
            }
            rememberFailure(DecodeRejectReason::Message);
            return false;
        }

        if (decodedViaGf2Osd && qualityOut != nullptr) {
            ++qualityOut->osdGf2Recovered;
            if (gf2OsdHitOrder == 0) {
                ++qualityOut->osdGf2Order0Hits;
            } else if (gf2OsdHitOrder == 1) {
                ++qualityOut->osdGf2Order1Hits;
            } else if (gf2OsdHitOrder == 2) {
                ++qualityOut->osdGf2Order2Hits;
            }
        }

        bits = candidateBits;
        iterations = candidateIterations;
        message = candidateMessage;
        return true;
    };

    bool metricDecoded = tryDecodeMetric(llr, true);

    /*
     * v3.33 synthesis: after the WSJT-X/MSHV hard Costas bail-out, use the
     * extra CPU budget on the candidates that are at least sync-plausible.
     * This follows ft8b's multi-metric idea more closely than the rejected
     * v3.28 experiment: Fast stays on the legacy metric, while Deep tries
     * normalized 1-symbol, 2-symbol, 3-symbol, strongest-family and bit-ratio
     * soft metrics only if the normal LDPC/CRC/unpack path fails.
     */
    // MSHV raises the hard-sync requirement for the heavier metric paths in
    // normal/deep operation. Keep the legacy metric available immediately after
    // the WSJT-X nsync>6 gate, but spend the extra multi-metric LDPC attempts
    // only on candidates with at least 9/21 hard Costas hits.
    /*
     * v4.13: keep the expensive ft8b-style metric families, but stop trying
     * all of them on every barely-plausible ghost candidate.  Valid decodes in
     * the v4.12 diagnostics have high Costas agreement; the costly recovery is
     * therefore reserved for candidates with either stronger hard sync or a
     * clearly usable LLR.  This targets LDPC wall-time without disabling the
     * overlap/residual recovery that recovered test_21/test_05.
     */
    const bool promisingForMetricRecovery =
            hardSyncCount >= 10 ||
            (hardSyncCount >= 9 && meanAbsLlr >= 4.2);
    const bool allowFt8bMetricRecovery = allowMetricRecovery &&
                                         (m_deepDecodeEnabled || m_dspPlusDecodeEnabled) &&
                                         promisingForMetricRecovery;
    if (!metricDecoded && allowFt8bMetricRecovery) {
        auto normalizeBmet = [](std::array<double, 174> &bmet) {
            double mean = 0.0;
            double mean2 = 0.0;
            for (double v : bmet) {
                mean += v;
                mean2 += v * v;
            }
            mean /= 174.0;
            mean2 /= 174.0;
            const double var = mean2 - mean * mean;
            double sigma = (var > 0.0) ? std::sqrt(var) : std::sqrt(qMax(mean2, 0.0));
            if (!(sigma > 1.0e-5) || !std::isfinite(sigma)) {
                sigma = 1.0e-5;
            }
            for (double &v : bmet) {
                v /= sigma;
            }
        };

        auto computeGroupedMetric = [&dataMagnitudes](int groupSize) {
            std::array<double, 174> out{};
            out.fill(0.0);
            if (groupSize < 1 || groupSize > 3) {
                return out;
            }

            const int bitsPerGroup = 3 * groupSize;
            const int comboCount = 1 << bitsPerGroup;
            for (int half = 0; half < 2; ++half) {
                const int dataBase = half * 29;
                const int bitBase = half * 87;
                for (int groupStart = 0; groupStart + groupSize <= 29; groupStart += groupSize) {
                    double score[512];
                    for (int combo = 0; combo < comboCount; ++combo) {
                        double s = 0.0;
                        for (int g = 0; g < groupSize; ++g) {
                            const int shift = 3 * (groupSize - 1 - g);
                            const int idx = (combo >> shift) & 0x7;
                            s += dataMagnitudes[dataBase + groupStart + g][idx];
                        }
                        score[combo] = s;
                    }

                    for (int bit = 0; bit < bitsPerGroup; ++bit) {
                        const int globalBit = bitBase + groupStart * 3 + bit;
                        if (globalBit < 0 || globalBit >= 174) {
                            continue;
                        }
                        double best0 = -1.0e99;
                        double best1 = -1.0e99;
                        const int mask = 1 << (bitsPerGroup - 1 - bit);
                        for (int combo = 0; combo < comboCount; ++combo) {
                            if ((combo & mask) != 0) {
                                best1 = std::max(best1, score[combo]);
                            } else {
                                best0 = std::max(best0, score[combo]);
                            }
                        }
                        // MadModem LDPC convention: positive LLR means bit 0,
                        // negative means bit 1.  This is the sign opposite of
                        // the boolean helper arrays used inside the MSHV source.
                        out[globalBit] = best0 - best1;
                    }
                }
            }
            return out;
        };

        auto scaledMetric = [](const std::array<double, 174> &metric, double scale) {
            std::array<double, 174> out{};
            for (int i = 0; i < 174; ++i) {
                out[i] = qBound(-18.0, metric[i] * scale, 18.0);
            }
            return out;
        };

        std::array<double, 174> bmeta = computeGroupedMetric(1);
        std::array<double, 174> bmetb = computeGroupedMetric(2);
        std::array<double, 174> bmetc = computeGroupedMetric(3);
        std::array<double, 174> bmetd{};
        bmetd.fill(0.0);

        for (int i = 0; i < 174; ++i) {
            const int dataSym = i / 3;
            const int bit = i % 3;
            if (dataSym < 0 || dataSym >= 58) {
                continue;
            }
            double den = 0.0;
            double best0 = -1.0e99;
            double best1 = -1.0e99;
            for (int idx = 0; idx < 8; ++idx) {
                const double v = dataMagnitudes[dataSym][idx];
                den = std::max(den, v);
                const bool one = ((idx >> (2 - bit)) & 1) != 0;
                if (one) {
                    best1 = std::max(best1, v);
                } else {
                    best0 = std::max(best0, v);
                }
            }
            bmetd[i] = (den > 0.0) ? ((best0 - best1) / den) : 0.0;
        }

        std::array<double, 174> bmete{};
        for (int i = 0; i < 174; ++i) {
            bmete[i] = bmeta[i];
            if (std::abs(bmetb[i]) > std::abs(bmete[i])) {
                bmete[i] = bmetb[i];
            }
            if (std::abs(bmetc[i]) > std::abs(bmete[i])) {
                bmete[i] = bmetc[i];
            }
        }

        normalizeBmet(bmeta);
        normalizeBmet(bmetb);
        normalizeBmet(bmetc);
        normalizeBmet(bmetd);
        normalizeBmet(bmete);

        // MSHV/WSJT-X typically feed normalized bit metrics with a scale around
        // 2.8 into the BP decoder.  Keep the legacy LLR first to avoid
        // regressions, then try the ft8b families in the same spirit.
        constexpr double kFt8bMetricScale = 2.83;
        if (!metricDecoded) metricDecoded = tryDecodeMetric(scaledMetric(bmete, kFt8bMetricScale), true);
        if (!metricDecoded) metricDecoded = tryDecodeMetric(scaledMetric(bmeta, kFt8bMetricScale), false);
        if (!metricDecoded) metricDecoded = tryDecodeMetric(scaledMetric(bmetb, kFt8bMetricScale), false);
        if (!metricDecoded) metricDecoded = tryDecodeMetric(scaledMetric(bmetc, kFt8bMetricScale), false);
        if (!metricDecoded) metricDecoded = tryDecodeMetric(scaledMetric(bmetd, kFt8bMetricScale), false);
    }
    if (!metricDecoded) {
        return reject(bestFailure);
    }

    decodeOut.utc = slotStartUtc.time().toString(QStringLiteral("HHmmss"));
    decodeOut.slotStartUtcMs = slotStartUtc.toMSecsSinceEpoch();
    decodeOut.slotPeriodMs = currentSlotMs();
    decodeOut.dt = static_cast<double>(startSample) / static_cast<double>(kDecodeSampleRate);
    decodeOut.frequencyHz = qRound(baseHz);
    decodeOut.message = message;
    decodeOut.syncScore = refinedSync;
    double codewordSignalPower = 0.0;
    double oppositeToneNoisePower = 0.0;
    int reconstructedSymbolCount = 0;
    dataIndex = 0;
    for (int sym = 0; sym < kSymbols; ++sym) {
        int expectedTone = 0;
        if (isSyncSymbol(sym)) {
            if (sym < 7) {
                expectedTone = kCostas[sym];
            } else if (sym >= 36 && sym < 43) {
                expectedTone = kCostas[sym - 36];
            } else {
                expectedTone = kCostas[sym - 72];
            }
        } else {
            if (dataIndex >= 58) {
                break;
            }
            const int idx = ((bits[dataIndex * 3 + 0] & 1) << 2) |
                            ((bits[dataIndex * 3 + 1] & 1) << 1) |
                            ((bits[dataIndex * 3 + 2] & 1) << 0);
            expectedTone = kGrayMap[qBound(0, idx, 7)];
            ++dataIndex;
        }

        const double expectedEnergy = symbolEnergies[sym][expectedTone];
        if (expectedEnergy > 0.0 && std::isfinite(expectedEnergy)) {
            codewordSignalPower += expectedEnergy;
            ++reconstructedSymbolCount;
        }

        // WSJT-X ft8b.f90 uses one reference off-tone per symbol:
        // ios = mod(itone(i)+4,7); xnoi += s8(ios,i)^2
        const int oppositeTone = (expectedTone + 4) % 7;
        const double oppositeEnergy = symbolEnergies[sym][oppositeTone];
        if (oppositeEnergy > 0.0 && std::isfinite(oppositeEnergy)) {
            oppositeToneNoisePower += oppositeEnergy;
        }
    }

    Q_UNUSED(syncScore)
    Q_UNUSED(totalPower)
    Q_UNUSED(iterations)
    Q_UNUSED(candidate)
    Q_UNUSED(reconstructedSymbolCount)
    decodeOut.snrDb = wsjtxFt8ReportDb(codewordSignalPower, oppositeToneNoisePower, hardSyncCount);
    return true;
}


void Ft8RxDecoder::subtractDecodedSignal(QVector<double> &samples, const Candidate &candidate, const Decode &decode) const
{
    const int startSample = qRound(candidate.startSec * kDecodeSampleRate);
    if (startSample < -kSamplesPerSymbol || startSample >= samples.size()) {
        return;
    }

    int reconstructedTones[100];
    std::fill(reconstructedTones, reconstructedTones + 100, 0);
    {
        GenFt8 toneGenerator(false);
        toneGenerator.pack77_make_c77_i4tone(decode.message.trimmed().toUpper(), reconstructedTones);
    }

    int plausible = 0;
    for (int sym = 0; sym < kSymbols; ++sym) {
        if (reconstructedTones[sym] >= 0 && reconstructedTones[sym] <= 7) {
            ++plausible;
        }
    }
    if (plausible != kSymbols) {
        return;
    }

    /*
     * v3.31: keep the MSHV subtractft8 signal model, remove the prohibitive
     * per-decode FFT cost.
     *
     * We still rebuild a continuous GFSK reference waveform, multiply the RX
     * samples by conj(cref), low-pass the complex amplitude/phase envelope, and
     * subtract cfilt*cref.  The low-pass is now O(N) forward/backward smoothing
     * instead of a 262144-point FFT per decoded signal.  This is the part that
     * must be cheap enough for live 2-pass FT8.
     */
    thread_local Ft8SubtractWorkspace subtractWorkspace;
    makeFt8ReferenceWaveformRx(reconstructedTones,
                               candidate.baseHz,
                               subtractWorkspace.cref,
                               subtractWorkspace.dphi);
    const std::vector<std::complex<double>> &cref = subtractWorkspace.cref;
    if (cref.size() != static_cast<size_t>(kFt8SubNFrame)) {
        return;
    }

    std::vector<std::complex<double>> &cfilt = subtractWorkspace.cfilt;
    cfilt.resize(static_cast<size_t>(kFt8SubNFrame));
    std::fill(cfilt.begin(), cfilt.end(), std::complex<double>(0.0, 0.0));
    for (int i = 0; i < kFt8SubNFrame; ++i) {
        const int sampleIndex = startSample + i;
        if (sampleIndex >= 0 && sampleIndex < samples.size()) {
            cfilt[static_cast<size_t>(i)] = samples.at(sampleIndex) * std::conj(cref[static_cast<size_t>(i)]);
        }
    }

    smoothComplexEnvelopeZeroPhase(cfilt);

    for (int i = 0; i < kFt8SubNFrame; ++i) {
        const int sampleIndex = startSample + i;
        if (sampleIndex >= 0 && sampleIndex < samples.size()) {
            const std::complex<double> reconstructed = cfilt[static_cast<size_t>(i)] * cref[static_cast<size_t>(i)];
            samples[sampleIndex] -= kFt8SubtractGain * reconstructed.real();
        }
    }
}




QVector<Ft8RxDecoder::Decode> Ft8RxDecoder::decodeSlotFt4(const QVector<double> &samples,
                                                          const QDateTime &slotStartUtc,
                                                          int *candidateCount,
                                                          PerfStats *stats)
{
    using Clock = std::chrono::steady_clock;
    const auto totalStart = Clock::now();
    const bool enhancedEngine = m_deepDecodeEnabled; // v3.22: Deep/DSP++ removed; adaptive only.

    struct CandidateDecode
    {
        Candidate candidate;
        Decode decode;
    };

    auto betterDecode = [](const Decode &a, const Decode &b) {
        if (a.snrDb != b.snrDb) {
            return a.snrDb > b.snrDb;
        }
        return a.syncScore > b.syncScore;
    };

    auto decodeCandidateSet = [this, &slotStartUtc](const QVector<double> &slotSamples,
                                                    const QVector<Candidate> &candidateSet,
                                                    int *workerCountOut) {
        QVector<CandidateDecode> rawPairs;
        if (candidateSet.isEmpty()) {
            if (workerCountOut != nullptr) {
                *workerCountOut = 0;
            }
            return rawPairs;
        }

        const int workerCount = FtDecodeWorkerPool::instance().recommendedWorkerCount(m_offlineAnalysisActive.load(), candidateSet.size());
        if (workerCountOut != nullptr) {
            *workerCountOut = workerCount;
        }

        std::mutex rawMutex;
        FtDecodeWorkerPool::instance().parallelFor(candidateSet.size(), workerCount, [this, &slotSamples, &slotStartUtc, &candidateSet, &rawPairs, &rawMutex](int begin, int end) {
            QVector<CandidateDecode> localPairs;
            for (int i = begin; i < end; ++i) {
                Decode decode;
                const Candidate candidate = candidateSet.at(i);
                if (decodeFt4Candidate(slotSamples, slotStartUtc, candidate, decode)) {
                    CandidateDecode pair;
                    pair.candidate = candidate;
                    pair.decode = decode;
                    localPairs.append(pair);
                }
            }

            if (!localPairs.isEmpty()) {
                std::lock_guard<std::mutex> lock(rawMutex);
                for (const CandidateDecode &pair : localPairs) {
                    rawPairs.append(pair);
                }
            }
        });
        return rawPairs;
    };

    auto deduplicate = [&betterDecode](const QVector<CandidateDecode> &rawPairs, int *droppedOut) {
        QVector<CandidateDecode> deduped;
        int dropped = 0;
        for (const CandidateDecode &pair : rawPairs) {
            int existingIndex = -1;
            for (int i = 0; i < deduped.size(); ++i) {
                const Decode &existing = deduped.at(i).decode;
                if (existing.message == pair.decode.message &&
                    std::abs(existing.frequencyHz - pair.decode.frequencyHz) <= 18 &&
                    std::abs(existing.dt - pair.decode.dt) <= 0.35) {
                    existingIndex = i;
                    break;
                }
            }

            if (existingIndex < 0) {
                deduped.append(pair);
                continue;
            }

            ++dropped;
            if (betterDecode(pair.decode, deduped.at(existingIndex).decode)) {
                deduped[existingIndex] = pair;
            }
        }
        if (droppedOut != nullptr) {
            *droppedOut = dropped;
        }
        return deduped;
    };

    QVector<Decode> out;
    int workerCount = 0;
    int dropped = 0;
    int secondPassCandidates = 0;

    const auto searchStart = Clock::now();
    const QVector<Candidate> candidates = findFt4Candidates(samples, 0.0);
    const auto searchEnd = Clock::now();

    const auto decodeStart = Clock::now();
    QVector<CandidateDecode> rawPairs = decodeCandidateSet(samples, candidates, &workerCount);
    QVector<CandidateDecode> deduped = deduplicate(rawPairs, &dropped);
    const auto decodeEnd = Clock::now();

    double subtractionMs = 0.0;
    int passCount = 1;
    const int requestedPasses = m_dspPlusDecodeEnabled ? 4 : (enhancedEngine ? 2 : 1);
    QVector<double> working = samples;
    for (int pass = 1; pass < requestedPasses && !deduped.isEmpty(); ++pass) {
        const auto subtractionStart = Clock::now();
        QVector<CandidateDecode> sorted = deduped;
        std::sort(sorted.begin(), sorted.end(), [&betterDecode](const CandidateDecode &a, const CandidateDecode &b) {
            return betterDecode(a.decode, b.decode);
        });
        const int subtractLimit = qMin(m_dspPlusDecodeEnabled ? 8 : 5, sorted.size());
        for (int i = 0; i < subtractLimit; ++i) {
            subtractFt4DecodedSignal(working, sorted.at(i).candidate);
        }
        const auto subtractionEnd = Clock::now();
        subtractionMs += std::chrono::duration<double, std::milli>(subtractionEnd - subtractionStart).count();

        const QVector<Candidate> nextCandidates = findFt4Candidates(working, 0.0);
        secondPassCandidates += nextCandidates.size();
        QVector<CandidateDecode> nextRaw = decodeCandidateSet(working, nextCandidates, nullptr);
        for (const CandidateDecode &pair : nextRaw) {
            rawPairs.append(pair);
        }
        deduped = deduplicate(rawPairs, &dropped);
        passCount = pass + 1;
    }

    std::sort(deduped.begin(), deduped.end(), [](const CandidateDecode &a, const CandidateDecode &b) {
        if (a.decode.utc != b.decode.utc) {
            return a.decode.utc < b.decode.utc;
        }
        return a.decode.frequencyHz < b.decode.frequencyHz;
    });

    for (const CandidateDecode &pair : deduped) {
        out.append(pair.decode);
    }

    const auto totalEnd = Clock::now();
    if (candidateCount != nullptr) {
        *candidateCount = candidates.size() + secondPassCandidates;
    }
    if (stats != nullptr) {
        stats->candidateCount = candidates.size() + secondPassCandidates;
        stats->decodeCount = out.size();
        stats->workerCount = workerCount;
        stats->candidateSearchMs = std::chrono::duration<double, std::milli>(searchEnd - searchStart).count();
        stats->candidateDecodeMs = std::chrono::duration<double, std::milli>(decodeEnd - decodeStart).count();
        stats->subtractionMs = subtractionMs;
        stats->passCount = passCount;
        stats->secondPassCandidates = secondPassCandidates;
        stats->dedupDropped = dropped;
        stats->totalMs = std::chrono::duration<double, std::milli>(totalEnd - totalStart).count();
        stats->attemptedCandidates = candidates.size() + secondPassCandidates;
        stats->ldpcTried = 0;
        stats->ldpcFailures = 0;
        stats->crcFailures = 0;
        stats->messageRejects = 0;
        stats->syncGateRejects = 0;
        stats->engineName = QStringLiteral("FT4 0.5.72d live engine");
    }

    m_lastCandidateCount = candidates.size() + secondPassCandidates;
    return out;
}



QVector<Ft8RxDecoder::Candidate> Ft8RxDecoder::findFt4Candidates(const QVector<double> &samples, double threshold) const
{
    Q_UNUSED(threshold)
    QVector<Candidate> candidates;
    if (samples.size() < kDecodeSampleRate * 4) {
        return candidates;
    }

    constexpr int kFt4Symbols = 103;
    constexpr int kFt4SamplesPerSymbol = 576;
    constexpr double kFt4ToneSpacingHz = 12000.0 / 576.0;
    const int frameSamples = kFt4Symbols * kFt4SamplesPerSymbol;
    const int maxStart = samples.size() - frameSamples - 1;
    if (maxStart <= 0) {
        return candidates;
    }

    struct SyncBlock { int pos; const int *tones; };
    const SyncBlock blocks[4] = {{0, kFt4SyncA}, {33, kFt4SyncB}, {66, kFt4SyncC}, {99, kFt4SyncD}};

    const int startStep = kFt4SamplesPerSymbol; // 48 ms coarse DT grid.
    const int firstStart = qMax(0, qRound(0.00 * kDecodeSampleRate));
    const int lastStart = qMin(maxStart, qRound(2.50 * kDecodeSampleRate));
    const double freqStep = kFt4ToneSpacingHz;
    const double low = qMax(150.0, static_cast<double>(m_searchLowHz));
    const double high = qMin(static_cast<double>(m_searchHighHz), 3000.0 - 3.0 * kFt4ToneSpacingHz);
    const int freqCount = qMax(0, static_cast<int>(std::floor((high - low) / freqStep)) + 1);

    auto syncScoreAt = [this, &samples, &blocks](double baseHz, int startSample) {
        double blockScores[4] = {0.0, 0.0, 0.0, 0.0};
        for (int b = 0; b < 4; ++b) {
            double block = 0.0;
            for (int k = 0; k < 4; ++k) {
                const int sym = blocks[b].pos + k;
                const int tone = blocks[b].tones[k];
                const int symStart = startSample + sym * kFt4SamplesPerSymbol;
                const double e = ft4SymbolToneEnergy(samples,
                                                      symStart,
                                                      baseHz + tone * kFt4ToneSpacingHz,
                                                      kFt4SamplesPerSymbol);
                block += std::log(e + kEps);
            }
            blockScores[b] = block / 4.0;
        }
        std::sort(blockScores, blockScores + 4);
        return blockScores[1] + blockScores[2] + blockScores[3];
    };

    const int workerCount = FtDecodeWorkerPool::instance().recommendedWorkerCount(m_offlineAnalysisActive.load(), qMax(1, freqCount));

    QVector<Candidate> raw;
    raw.reserve(256);
    std::mutex rawMutex;
    FtDecodeWorkerPool::instance().parallelFor(freqCount, workerCount, [&](int begin, int end) {
        QVector<Candidate> local;
        local.reserve(64);
        for (int fi = begin; fi < end; ++fi) {
            const double baseHz = low + static_cast<double>(fi) * freqStep;
            for (int start = firstStart; start <= lastStart; start += startStep) {
                const double score = syncScoreAt(baseHz, start);
                Candidate c;
                c.score = score;
                c.startSec = static_cast<double>(start) / static_cast<double>(kDecodeSampleRate);
                c.baseHz = baseHz;
                // v3.25: FT4 candidate ranking is also passband-neutral.
                // Do not prefer the selected RX marker over other CQ/QSO
                // traffic in the slot.
                c.rankScore = score;
                local.append(c);
            }
        }
        if (!local.isEmpty()) {
            std::lock_guard<std::mutex> lock(rawMutex);
            for (const Candidate &c : local) {
                raw.append(c);
            }
        }
    });

    std::sort(raw.begin(), raw.end(), [](const Candidate &a, const Candidate &b) {
        return a.rankScore > b.rankScore;
    });

    // Keep the FT4 live decoder candidate budget on the last known stable
    // live-slot policy. Auto-tests must exercise this same path, not tune a
    // separate benchmark-only engine.
    constexpr int kMaxCandidates = 120;
    for (const Candidate &candidate : raw) {
        bool duplicate = false;
        for (const Candidate &existing : candidates) {
            if (std::abs(existing.baseHz - candidate.baseHz) <= kFt4ToneSpacingHz &&
                std::abs(existing.startSec - candidate.startSec) <= 0.10) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            candidates.append(candidate);
        }
        if (candidates.size() >= kMaxCandidates) {
            break;
        }
    }

    return candidates;
}

bool Ft8RxDecoder::decodeFt4Candidate(const QVector<double> &samples,
                                      const QDateTime &slotStartUtc,
                                      const Candidate &candidate,
                                      Decode &decodeOut,
                                      CandidateAttemptQuality *qualityOut,
                                      DecodeRejectReason *rejectReasonOut)
{
    constexpr int kFt4Symbols = 103;
    constexpr int kFt4DataSymbols = 87;
    constexpr int kFt4SamplesPerSymbol = 576;
    constexpr double kFt4ToneSpacingHz = 12000.0 / 576.0;
    constexpr double kMetricScale = 2.83;

    if (rejectReasonOut != nullptr) {
        *rejectReasonOut = DecodeRejectReason::None;
    }
    if (qualityOut != nullptr) {
        *qualityOut = CandidateAttemptQuality{};
    }
    auto reject = [&](DecodeRejectReason reason) {
        if (rejectReasonOut != nullptr) {
            *rejectReasonOut = reason;
        }
        return false;
    };

    struct SyncBlock { int pos; const int *tones; };
    const SyncBlock blocks[4] = {{0, kFt4SyncA}, {33, kFt4SyncB}, {66, kFt4SyncC}, {99, kFt4SyncD}};

    auto syncScoreAt = [this, &samples, &blocks](double baseHz, int startSample, int *hardSyncOut) {
        double blockScores[4] = {0.0, 0.0, 0.0, 0.0};
        int hardSync = 0;
        for (int b = 0; b < 4; ++b) {
            double block = 0.0;
            for (int k = 0; k < 4; ++k) {
                const int sym = blocks[b].pos + k;
                const int expectedTone = blocks[b].tones[k];
                const int symStart = startSample + sym * kFt4SamplesPerSymbol;
                double bestEnergy = -1.0;
                int bestTone = 0;
                for (int tone = 0; tone < 4; ++tone) {
                    const double e = ft4SymbolToneEnergy(samples,
                                                          symStart,
                                                          baseHz + tone * kFt4ToneSpacingHz,
                                                          kFt4SamplesPerSymbol);
                    if (e > bestEnergy) {
                        bestEnergy = e;
                        bestTone = tone;
                    }
                    if (tone == expectedTone) {
                        block += std::log(e + kEps);
                    }
                }
                if (bestTone == expectedTone) {
                    ++hardSync;
                }
            }
            blockScores[b] = block / 4.0;
        }
        std::sort(blockScores, blockScores + 4);
        if (hardSyncOut != nullptr) {
            *hardSyncOut = hardSync;
        }
        return blockScores[1] + blockScores[2] + blockScores[3];
    };

    int bestStart = qRound(candidate.startSec * kDecodeSampleRate);
    double bestBaseHz = candidate.baseHz;
    double bestScore = -1.0e99;
    int bestHardSync = 0;

    for (int dtStep = -2; dtStep <= 2; ++dtStep) {
        const int start = qRound(candidate.startSec * kDecodeSampleRate) + dtStep * (kFt4SamplesPerSymbol / 4);
        if (start < 0 || start + kFt4Symbols * kFt4SamplesPerSymbol >= samples.size()) {
            continue;
        }
        for (double df = -8.0; df <= 8.0; df += 4.0) {
            int hardSync = 0;
            const double score = syncScoreAt(candidate.baseHz + df, start, &hardSync);
            if (score > bestScore) {
                bestScore = score;
                bestStart = start;
                bestBaseHz = candidate.baseHz + df;
                bestHardSync = hardSync;
            }
        }
    }

    if (bestHardSync < 8) {
        return reject(DecodeRejectReason::SyncGate);
    }

    std::array<double, 174> llr{};
    int dataIndex = 0;
    double totalEnergy = 0.0;
    double syncEnergy = 0.0;
    std::array<std::array<double, 4>, kFt4Symbols> symbolEnergies{};

    auto isFt4Sync = [](int sym) {
        return (sym >= 0 && sym < 4) ||
               (sym >= 33 && sym < 37) ||
               (sym >= 66 && sym < 70) ||
               (sym >= 99 && sym < 103);
    };

    for (int sym = 0; sym < kFt4Symbols; ++sym) {
        const int symStart = bestStart + sym * kFt4SamplesPerSymbol;
        double energies[4];
        for (int tone = 0; tone < 4; ++tone) {
            energies[tone] = ft4SymbolToneEnergy(samples,
                                                  symStart,
                                                  bestBaseHz + tone * kFt4ToneSpacingHz,
                                                  kFt4SamplesPerSymbol);
            symbolEnergies[sym][tone] = energies[tone];
            totalEnergy += energies[tone];
        }

        if (isFt4Sync(sym)) {
            int expectedTone = 0;
            if (sym < 4) {
                expectedTone = kFt4SyncA[sym];
            } else if (sym < 37) {
                expectedTone = kFt4SyncB[sym - 33];
            } else if (sym < 70) {
                expectedTone = kFt4SyncC[sym - 66];
            } else {
                expectedTone = kFt4SyncD[sym - 99];
            }
            syncEnergy += energies[expectedTone];
            continue;
        }

        if (dataIndex >= kFt4DataSymbols) {
            return reject(DecodeRejectReason::SoftMetric);
        }

        double metric[4];
        for (int tone = 0; tone < 4; ++tone) {
            int idx = 0;
            for (int i = 0; i < 4; ++i) {
                if (kFt4GrayMap[i] == tone) {
                    idx = i;
                    break;
                }
            }
            metric[idx] = std::log(energies[tone] + kEps);
        }

        for (int bit = 0; bit < 2; ++bit) {
            double best0 = -1.0e9;
            double best1 = -1.0e9;
            for (int idx = 0; idx < 4; ++idx) {
                const bool one = ((idx >> (1 - bit)) & 1) != 0;
                if (one) {
                    best1 = std::max(best1, metric[idx]);
                } else {
                    best0 = std::max(best0, metric[idx]);
                }
            }
            llr[dataIndex * 2 + bit] = qBound(-18.0, (best0 - best1) * kMetricScale, 18.0);
        }
        ++dataIndex;
    }

    if (dataIndex != kFt4DataSymbols) {
        return reject(DecodeRejectReason::SoftMetric);
    }

    if (qualityOut != nullptr) {
        qualityOut->valid = true;
        qualityOut->syncScore = bestScore;
        qualityOut->hardSyncCount = bestHardSync;
        double meanAbsLlr = 0.0;
        for (double v : llr) {
            meanAbsLlr += std::abs(v);
        }
        qualityOut->meanAbsLlr = meanAbsLlr / static_cast<double>(llr.size());
    }

    std::array<int, 174> bits{};
    int iterations = 0;
    if (!ldpcDecode174_91(llr, bits, iterations)) {
        return reject(DecodeRejectReason::Ldpc);
    }
    if (!crc14Ok(bits)) {
        return reject(DecodeRejectReason::Crc);
    }

    const QString message = unpackFt4Message77(bits).trimmed().toUpper();
    if (!looksLikeUsefulFt8Message(message)) {
        return reject(DecodeRejectReason::Message);
    }

    if (rejectReasonOut != nullptr) {
        *rejectReasonOut = DecodeRejectReason::None;
    }

    decodeOut.utc = slotStartUtc.time().toString(QStringLiteral("HHmmss"));
    decodeOut.slotStartUtcMs = slotStartUtc.toMSecsSinceEpoch();
    decodeOut.slotPeriodMs = currentSlotMs();
    decodeOut.dt = static_cast<double>(bestStart) / static_cast<double>(kDecodeSampleRate) - 0.5;
    decodeOut.frequencyHz = qRound(bestBaseHz);
    decodeOut.message = message;
    decodeOut.syncScore = bestScore;

    double codewordSignalPower = 0.0;
    double offTonePower = 0.0;
    int reconstructedSymbolCount = 0;
    int offToneCount = 0;
    dataIndex = 0;
    for (int sym = 0; sym < kFt4Symbols; ++sym) {
        int expectedTone = 0;
        if (isFt4Sync(sym)) {
            if (sym < 4) {
                expectedTone = kFt4SyncA[sym];
            } else if (sym < 37) {
                expectedTone = kFt4SyncB[sym - 33];
            } else if (sym < 70) {
                expectedTone = kFt4SyncC[sym - 66];
            } else {
                expectedTone = kFt4SyncD[sym - 99];
            }
        } else {
            if (dataIndex >= kFt4DataSymbols) {
                break;
            }
            const int idx = ((bits[dataIndex * 2 + 0] & 1) << 1) |
                            ((bits[dataIndex * 2 + 1] & 1) << 0);
            expectedTone = kFt4GrayMap[qBound(0, idx, 3)];
            ++dataIndex;
        }

        const double expectedEnergy = symbolEnergies[sym][expectedTone];
        if (expectedEnergy > 0.0 && std::isfinite(expectedEnergy)) {
            codewordSignalPower += expectedEnergy;
            ++reconstructedSymbolCount;
        }
        for (int tone = 0; tone < 4; ++tone) {
            if (tone == expectedTone) {
                continue;
            }
            const double e = symbolEnergies[sym][tone];
            if (!(e > 0.0) || !std::isfinite(e)) {
                continue;
            }
            offTonePower += e;
            ++offToneCount;
        }
    }

    Q_UNUSED(syncEnergy)
    Q_UNUSED(totalEnergy)
    Q_UNUSED(iterations)
    Q_UNUSED(bestHardSync)
    decodeOut.snrDb = ft4ReportDbFromDecodedPowers(codewordSignalPower,
                                                   offTonePower,
                                                   reconstructedSymbolCount,
                                                   offToneCount);
    return true;
}




void Ft8RxDecoder::subtractFt4DecodedSignal(QVector<double> &samples, const Candidate &candidate) const
{
    constexpr int kFt4Symbols = 103;
    constexpr int kFt4SamplesPerSymbol = 576;
    constexpr double kFt4ToneSpacingHz = 12000.0 / 576.0;
    const int startSample = qRound(candidate.startSec * kDecodeSampleRate);
    if (startSample < 0 || startSample + kFt4Symbols * kFt4SamplesPerSymbol >= samples.size()) {
        return;
    }

    auto isFt4Sync = [](int sym) {
        return (sym >= 0 && sym < 4) ||
               (sym >= 33 && sym < 37) ||
               (sym >= 66 && sym < 70) ||
               (sym >= 99 && sym < 103);
    };

    for (int sym = 0; sym < kFt4Symbols; ++sym) {
        const int symStart = startSample + sym * kFt4SamplesPerSymbol;
        int selectedTone = 0;
        if (isFt4Sync(sym)) {
            if (sym < 4) {
                selectedTone = kFt4SyncA[sym];
            } else if (sym < 37) {
                selectedTone = kFt4SyncB[sym - 33];
            } else if (sym < 70) {
                selectedTone = kFt4SyncC[sym - 66];
            } else {
                selectedTone = kFt4SyncD[sym - 99];
            }
        } else {
            const std::array<double, 4> toneEnergies = ft4SymbolToneEnergies4(samples,
                                                                              symStart,
                                                                              candidate.baseHz,
                                                                              kFt4ToneSpacingHz,
                                                                              kFt4SamplesPerSymbol);
            double bestEnergy = -1.0;
            for (int tone = 0; tone < 4; ++tone) {
                const double e = toneEnergies[tone];
                if (e > bestEnergy) {
                    bestEnergy = e;
                    selectedTone = tone;
                }
            }
        }

        const double frequencyHz = candidate.baseHz + selectedTone * kFt4ToneSpacingHz;
        const double omega = kTwoPi * frequencyHz / static_cast<double>(kDecodeSampleRate);
        double iSum = 0.0;
        double qSum = 0.0;
        for (int n = 0; n < kFt4SamplesPerSymbol; ++n) {
            const double phase = omega * static_cast<double>(n);
            const double x = samples.at(symStart + n);
            iSum += x * std::cos(phase);
            qSum += x * std::sin(phase);
        }

        const double scale = 2.0 / static_cast<double>(kFt4SamplesPerSymbol);
        const double iAmp = iSum * scale;
        const double qAmp = qSum * scale;
        constexpr int kFadeSamples = 36;
        constexpr double kSubtractGain = 0.68;
        for (int n = 0; n < kFt4SamplesPerSymbol; ++n) {
            const double phase = omega * static_cast<double>(n);
            double taper = 1.0;
            const int edge = qMin(n, kFt4SamplesPerSymbol - 1 - n);
            if (edge < kFadeSamples) {
                taper = static_cast<double>(edge) / static_cast<double>(kFadeSamples);
            }
            const double tone = (iAmp * std::cos(phase)) + (qAmp * std::sin(phase));
            samples[symStart + n] -= kSubtractGain * taper * tone;
        }
    }
}

double Ft8RxDecoder::ft4SymbolToneEnergy(const QVector<double> &samples,
                                         int startSample,
                                         double frequencyHz,
                                         int sampleCount) const
{
    if (startSample < 0 || sampleCount <= 0 || startSample + sampleCount >= samples.size()) {
        return 0.0;
    }

    const double omega = kTwoPi * frequencyHz / static_cast<double>(kDecodeSampleRate);
    const double coeff = 2.0 * std::cos(omega);
    double s1 = 0.0;
    double s2 = 0.0;

    const double *xv = samples.constData() + startSample;
    for (int n = 0; n < sampleCount; ++n) {
        const double s0 = xv[n] + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }

    const double power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
    return power > 0.0 ? power : 0.0;
}

std::array<double, 4> Ft8RxDecoder::ft4SymbolToneEnergies4(const QVector<double> &samples,
                                                           int startSample,
                                                           double baseFrequencyHz,
                                                           double toneSpacingHz,
                                                           int sampleCount) const
{
    std::array<double, 4> power{};
    if (startSample < 0 || sampleCount <= 0 || startSample + sampleCount >= samples.size()) {
        return power;
    }

    const double *xv = samples.constData() + startSample;

#if defined(MADMODEM_FT8_HAVE_AVX2_TARGET)
    static const bool useAvx2FmaToneEngine = []() {
        const MadModemCpu::Features f = MadModemCpu::detect();
        return f.avx2 && f.fma;
    }();
    if (useAvx2FmaToneEngine) {
        power = ft4ToneEnergies4Avx2Fma(xv, sampleCount, baseFrequencyHz, toneSpacingHz, kDecodeSampleRate);
        for (double &p : power) {
            if (!(p > 0.0) || !std::isfinite(p)) {
                p = 0.0;
            }
        }
        return power;
    }
#endif

    double coeff[4];
    for (int tone = 0; tone < 4; ++tone) {
        const double frequencyHz = baseFrequencyHz + static_cast<double>(tone) * toneSpacingHz;
        const double omega = kTwoPi * frequencyHz / static_cast<double>(kDecodeSampleRate);
        coeff[tone] = 2.0 * std::cos(omega);
    }

#if defined(MADMODEM_FT8_HAVE_SSE2)
    const __m128d c01 = _mm_set_pd(coeff[1], coeff[0]);
    const __m128d c23 = _mm_set_pd(coeff[3], coeff[2]);
    __m128d s1_01 = _mm_setzero_pd();
    __m128d s2_01 = _mm_setzero_pd();
    __m128d s1_23 = _mm_setzero_pd();
    __m128d s2_23 = _mm_setzero_pd();

    for (int n = 0; n < sampleCount; ++n) {
        const __m128d x = _mm_set1_pd(xv[n]);
        __m128d s0 = _mm_sub_pd(_mm_add_pd(x, _mm_mul_pd(c01, s1_01)), s2_01);
        s2_01 = s1_01;
        s1_01 = s0;

        s0 = _mm_sub_pd(_mm_add_pd(x, _mm_mul_pd(c23, s1_23)), s2_23);
        s2_23 = s1_23;
        s1_23 = s0;
    }

    auto finishPair = [](const __m128d s1, const __m128d s2, const __m128d c, double *out) {
        const __m128d p = _mm_sub_pd(_mm_add_pd(_mm_mul_pd(s1, s1), _mm_mul_pd(s2, s2)),
                                    _mm_mul_pd(c, _mm_mul_pd(s1, s2)));
        _mm_storeu_pd(out, p);
    };
    finishPair(s1_01, s2_01, c01, power.data());
    finishPair(s1_23, s2_23, c23, power.data() + 2);
#else
    double s1[4] = {0.0, 0.0, 0.0, 0.0};
    double s2[4] = {0.0, 0.0, 0.0, 0.0};
    for (int n = 0; n < sampleCount; ++n) {
        const double x = xv[n];
        for (int tone = 0; tone < 4; ++tone) {
            const double s0 = x + coeff[tone] * s1[tone] - s2[tone];
            s2[tone] = s1[tone];
            s1[tone] = s0;
        }
    }
    for (int tone = 0; tone < 4; ++tone) {
        power[tone] = s1[tone] * s1[tone] + s2[tone] * s2[tone] - coeff[tone] * s1[tone] * s2[tone];
    }
#endif

    for (double &p : power) {
        if (!(p > 0.0) || !std::isfinite(p)) {
            p = 0.0;
        }
    }
    return power;
}

QString Ft8RxDecoder::unpackFt4Message77(const std::array<int, 174> &bits)
{
    std::lock_guard<std::mutex> lock(m_unpackMutex);
    bool c77[100];
    for (bool &b : c77) {
        b = false;
    }
    for (int i = 0; i < 77; ++i) {
        c77[i] = ((bits[i] & 1) ^ kFt4Scrambler[i]) != 0;
    }
    bool success = false;
    QString message = m_unpacker.unpack77(c77, success);
    if (!success) {
        return QString();
    }
    return message;
}

// Goertzel single-bin energy.  This is intentionally dependency-free so
// MadModem does not inherit MSHV's FFTW build complexity for the first RX stage.
double Ft8RxDecoder::symbolToneEnergy(const QVector<double> &samples,
                                      int startSample,
                                      double frequencyHz) const
{
    if (startSample < 0 || startSample + kSamplesPerSymbol >= samples.size()) {
        return 0.0;
    }

    const double omega = kTwoPi * frequencyHz / static_cast<double>(kDecodeSampleRate);
    const double coeff = 2.0 * std::cos(omega);
    double s1 = 0.0;
    double s2 = 0.0;

    const double *xv = samples.constData() + startSample;
    for (int n = 0; n < kSamplesPerSymbol; ++n) {
        const double s0 = xv[n] + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }

    const double power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
    return power > 0.0 ? power : 0.0;
}

std::array<double, 8> Ft8RxDecoder::symbolToneEnergies8(const QVector<double> &samples,
                                                        int startSample,
                                                        double baseFrequencyHz) const
{
    std::array<double, 8> power{};
    if (startSample < 0 || startSample + kSamplesPerSymbol >= samples.size()) {
        return power;
    }

    double coeff[8];
    for (int tone = 0; tone < 8; ++tone) {
        const double frequencyHz = baseFrequencyHz + static_cast<double>(tone) * kToneSpacingHz;
        const double omega = kTwoPi * frequencyHz / static_cast<double>(kDecodeSampleRate);
        coeff[tone] = 2.0 * std::cos(omega);
    }

    const double *xv = samples.constData() + startSample;

#if defined(MADMODEM_FT8_HAVE_AVX2_TARGET)
    static const bool useAvx2FmaToneEngine = []() {
        const MadModemCpu::Features f = MadModemCpu::detect();
        return f.avx2 && f.fma;
    }();
    if (useAvx2FmaToneEngine) {
        power = ft8ToneEnergies8Avx2Fma(xv, kSamplesPerSymbol, baseFrequencyHz, kToneSpacingHz, kDecodeSampleRate);
        for (double &p : power) {
            if (!(p > 0.0) || !std::isfinite(p)) {
                p = 0.0;
            }
        }
        return power;
    }
#endif

#if defined(MADMODEM_FT8_HAVE_SSE2)
    /*
     * v3.25: FT8 symbol demodulation is the hottest offline/live decode path.
     * The old code ran one full Goertzel pass for every tone, so each FT8
     * symbol reread the same 1920 samples eight times.  This computes the
     * eight Goertzel resonators in one sample sweep, using SSE2 pairs on
     * x86/x86_64 and a scalar fallback elsewhere.  It does not change the
     * decoding algorithm or candidate ranking: it is just the same Goertzel
     * recurrence evaluated with fewer memory passes and SIMD lanes.
     */
    __m128d c01 = _mm_set_pd(coeff[1], coeff[0]);
    __m128d c23 = _mm_set_pd(coeff[3], coeff[2]);
    __m128d c45 = _mm_set_pd(coeff[5], coeff[4]);
    __m128d c67 = _mm_set_pd(coeff[7], coeff[6]);

    __m128d s1_01 = _mm_setzero_pd();
    __m128d s2_01 = _mm_setzero_pd();
    __m128d s1_23 = _mm_setzero_pd();
    __m128d s2_23 = _mm_setzero_pd();
    __m128d s1_45 = _mm_setzero_pd();
    __m128d s2_45 = _mm_setzero_pd();
    __m128d s1_67 = _mm_setzero_pd();
    __m128d s2_67 = _mm_setzero_pd();

    for (int n = 0; n < kSamplesPerSymbol; ++n) {
        const __m128d x = _mm_set1_pd(xv[n]);

        __m128d s0 = _mm_sub_pd(_mm_add_pd(x, _mm_mul_pd(c01, s1_01)), s2_01);
        s2_01 = s1_01;
        s1_01 = s0;

        s0 = _mm_sub_pd(_mm_add_pd(x, _mm_mul_pd(c23, s1_23)), s2_23);
        s2_23 = s1_23;
        s1_23 = s0;

        s0 = _mm_sub_pd(_mm_add_pd(x, _mm_mul_pd(c45, s1_45)), s2_45);
        s2_45 = s1_45;
        s1_45 = s0;

        s0 = _mm_sub_pd(_mm_add_pd(x, _mm_mul_pd(c67, s1_67)), s2_67);
        s2_67 = s1_67;
        s1_67 = s0;
    }

    auto finishPair = [](const __m128d s1, const __m128d s2, const __m128d c, double *out) {
        const __m128d p = _mm_sub_pd(_mm_add_pd(_mm_mul_pd(s1, s1), _mm_mul_pd(s2, s2)),
                                    _mm_mul_pd(c, _mm_mul_pd(s1, s2)));
        _mm_storeu_pd(out, p);
    };

    finishPair(s1_01, s2_01, c01, power.data() + 0);
    finishPair(s1_23, s2_23, c23, power.data() + 2);
    finishPair(s1_45, s2_45, c45, power.data() + 4);
    finishPair(s1_67, s2_67, c67, power.data() + 6);
#else
    double s1[8] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    double s2[8] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    for (int n = 0; n < kSamplesPerSymbol; ++n) {
        const double x = xv[n];
        for (int tone = 0; tone < 8; ++tone) {
            const double s0 = x + coeff[tone] * s1[tone] - s2[tone];
            s2[tone] = s1[tone];
            s1[tone] = s0;
        }
    }
    for (int tone = 0; tone < 8; ++tone) {
        power[tone] = s1[tone] * s1[tone] + s2[tone] * s2[tone] - coeff[tone] * s1[tone] * s2[tone];
    }
#endif

    for (double &p : power) {
        if (!(p > 0.0) || !std::isfinite(p)) {
            p = 0.0;
        }
    }
    return power;
}

bool Ft8RxDecoder::ldpcDecode174_91(const std::array<double, 174> &llr,
                                    std::array<int, 174> &hardBits,
                                    int &iterationsUsed,
                                    std::array<double, 174> *posteriorOut) const
{
    constexpr int N = 174;
    constexpr int M = 83;
    constexpr int MAX_CHECK_DEG = 7;

    struct EdgeRef { int check = -1; int edge = -1; };
    // MSHV keeps the FT8 LDPC parity/check matrices as fixed tables. Do the
    // same here: do not rebuild std::vector edge lists for every Costas
    // candidate during live RX.
    static const std::array<std::array<EdgeRef, 3>, N> variableEdges = []() {
        constexpr int kNLocal = 174;
        constexpr int kMLocal = 83;
        std::array<std::array<EdgeRef, 3>, kNLocal> edges{};
        std::array<int, kNLocal> used{};
        for (int v = 0; v < kNLocal; ++v) {
            for (int k = 0; k < 3; ++k) {
                edges[v][k] = EdgeRef{};
            }
        }
        for (int c = 0; c < kMLocal; ++c) {
            for (int e = 0; e < nrw_ft8_174_91[c]; ++e) {
                const int v = Nm_ft8_174_91_[c][e] - 1;
                if (v >= 0 && v < kNLocal && used[v] < 3) {
                    edges[v][used[v]++] = EdgeRef{c, e};
                }
            }
        }
        return edges;
    }();

    double q[M][MAX_CHECK_DEG] = {{0.0}};
    double r[M][MAX_CHECK_DEG] = {{0.0}};

    for (int c = 0; c < M; ++c) {
        for (int e = 0; e < nrw_ft8_174_91[c]; ++e) {
            const int v = Nm_ft8_174_91_[c][e] - 1;
            if (v >= 0 && v < N) {
                q[c][e] = llr[v];
            }
        }
    }

    for (int iter = 0; iter < 35; ++iter) {
        // Check-node update: normalized min-sum.
        for (int c = 0; c < M; ++c) {
            const int deg = nrw_ft8_174_91[c];
            for (int e = 0; e < deg; ++e) {
                double minAbs = 1.0e9;
                int sign = 1;
                for (int k = 0; k < deg; ++k) {
                    if (k == e) {
                        continue;
                    }
                    const double val = q[c][k];
                    if (val < 0.0) {
                        sign = -sign;
                    }
                    minAbs = std::min(minAbs, std::abs(val));
                }
                // Normalized min-sum.  Keep the proven v4.12 scale as
                // the default; v4.13 attacks wasted LDPC attempts first rather
                // than changing decoder sensitivity and risking regressions.
                constexpr double kFt8NmsScale = 0.78;
                r[c][e] = qBound(-20.0, kFt8NmsScale * static_cast<double>(sign) * minAbs, 20.0);
            }
        }

        // Variable-node update and hard decisions.
        for (int v = 0; v < N; ++v) {
            double post = llr[v];
            for (const EdgeRef &edge : variableEdges[v]) {
                if (edge.check >= 0) {
                    post += r[edge.check][edge.edge];
                }
            }
            if (posteriorOut != nullptr) {
                (*posteriorOut)[v] = post;
            }
            hardBits[v] = (post < 0.0) ? 1 : 0;
        }

        if (syndromeOk(hardBits)) {
            iterationsUsed = iter + 1;
            return true;
        }

        for (int v = 0; v < N; ++v) {
            for (const EdgeRef &edge : variableEdges[v]) {
                if (edge.check < 0) {
                    continue;
                }
                double msg = llr[v];
                for (const EdgeRef &other : variableEdges[v]) {
                    if (other.check < 0 || (other.check == edge.check && other.edge == edge.edge)) {
                        continue;
                    }
                    msg += r[other.check][other.edge];
                }
                q[edge.check][edge.edge] = qBound(-20.0, msg, 20.0);
            }
        }
    }

    iterationsUsed = 35;
    return syndromeOk(hardBits);
}

bool Ft8RxDecoder::osdGf2Repair174_91(const std::array<double, 174> &posterior,
                                      std::array<int, 174> &bits,
                                      int &outOrder,
                                      bool &rankFail,
                                      int &pivotSkips,
                                      int order1Depth,
                                      int order2Depth) const
{
    outOrder = -1;
    rankFail = false;
    pivotSkips = 0;

    struct OsdBit
    {
        int originalIndex = 0;
        double absPost = 0.0;
        int hardDecision = 0;
    };

    std::array<OsdBit, 174> orderedBits{};
    for (int i = 0; i < 174; ++i) {
        orderedBits[i] = OsdBit{i, std::abs(posterior[i]), (posterior[i] < 0.0) ? 1 : 0};
    }

    /*
     * 0.5.1: pivot completion.  alpha23 forced the first 83
     * least-reliable columns to become the systematic side and therefore
     * failed rank on all tested candidates.  Here columns are still tried in
     * reliability order, but a dependent column is skipped and a later column
     * is used as pivot.  This keeps the pivot set as unreliable as possible
     * while allowing H to reach rank 83 when the code matrix permits it.
     */
    std::sort(orderedBits.begin(), orderedBits.end(), [](const OsdBit &a, const OsdBit &b) {
        return a.absPost < b.absPost;
    });

    std::array<int, 174> inverseMap{};
    for (int col = 0; col < 174; ++col) {
        inverseMap[orderedBits[col].originalIndex] = col;
    }

    Gf2Matrix83x174 matrix;
    matrix.clear();
    for (int r = 0; r < 83; ++r) {
        for (int e = 0; e < nrw_ft8_174_91[r]; ++e) {
            const int originalCol = Nm_ft8_174_91_[r][e] - 1;
            if (originalCol >= 0 && originalCol < 174) {
                matrix.rows[r].setBit(inverseMap[originalCol], 1);
            }
        }
    }

    std::array<int, 83> pivotColAtRow{};
    std::array<unsigned char, 174> isPivotCol{};
    pivotColAtRow.fill(-1);
    isPivotCol.fill(0);

    int pivotRow = 0;
    for (int col = 0; col < 174 && pivotRow < 83; ++col) {
        int swapRow = pivotRow;
        while (swapRow < 83 && matrix.rows[swapRow].getBit(col) == 0) {
            ++swapRow;
        }

        if (swapRow == 83) {
            // Dependent for the current partial basis: keep it as information
            // bit and try the next, slightly more reliable, column.
            ++pivotSkips;
            continue;
        }

        matrix.swapRows(pivotRow, swapRow);
        for (int r = 0; r < 83; ++r) {
            if (r != pivotRow && matrix.rows[r].getBit(col) != 0) {
                matrix.rows[r].xorWith(matrix.rows[pivotRow]);
            }
        }

        pivotColAtRow[pivotRow] = col;
        isPivotCol[col] = 1;
        ++pivotRow;
    }

    if (pivotRow != 83) {
        rankFail = true;
        return false;
    }

    std::array<int, 91> infoCols{};
    int infoCount = 0;
    for (int col = 0; col < 174; ++col) {
        if (isPivotCol[col] == 0) {
            if (infoCount >= 91) {
                rankFail = true;
                return false;
            }
            infoCols[infoCount++] = col;
        }
    }
    if (infoCount != 91) {
        rankFail = true;
        return false;
    }

    std::array<int, 91> infoBits{};
    for (int i = 0; i < 91; ++i) {
        infoBits[i] = orderedBits[infoCols[i]].hardDecision;
    }

    auto verifyPattern = [&](const std::array<int, 91> &testInfo) -> bool {
        std::array<int, 174> testBits{};

        for (int i = 0; i < 91; ++i) {
            testBits[orderedBits[infoCols[i]].originalIndex] = testInfo[i];
        }

        for (int r = 0; r < 83; ++r) {
            int pivotBit = 0;
            for (int i = 0; i < 91; ++i) {
                if (matrix.rows[r].getBit(infoCols[i]) != 0) {
                    pivotBit ^= testInfo[i];
                }
            }
            const int pivotCol = pivotColAtRow[r];
            if (pivotCol < 0 || pivotCol >= 174) {
                rankFail = true;
                return false;
            }
            testBits[orderedBits[pivotCol].originalIndex] = pivotBit;
        }

        if (!syndromeOk(testBits)) {
            return false;
        }
        if (!crc14Ok(testBits)) {
            return false;
        }
        bits = testBits;
        return true;
    };

    if (verifyPattern(infoBits)) {
        outOrder = 0;
        return true;
    }

    /*
     * 0.5.1 fast budget: alpha24 proved that order-0 and
     * order-1 recover the useful cases in the current WAV set, while
     * order-2 recovered nothing and consumed CPU. Keep the algebra
     * unchanged, but bound the pattern search. Depths are supplied by
     * the caller so live RX can be stricter than offline analysis.
     */
    const int singleDepth = qBound(0, order1Depth, 91);
    for (int i = 0; i < singleDepth; ++i) {
        std::array<int, 91> testInfo = infoBits;
        testInfo[i] ^= 1;
        if (verifyPattern(testInfo)) {
            outOrder = 1;
            return true;
        }
    }

    const int pairDepth = qBound(0, qMin(order2Depth, singleDepth), 91);
    for (int i = 0; i < pairDepth; ++i) {
        for (int j = i + 1; j < pairDepth; ++j) {
            std::array<int, 91> testInfo = infoBits;
            testInfo[i] ^= 1;
            testInfo[j] ^= 1;
            if (verifyPattern(testInfo)) {
                outOrder = 2;
                return true;
            }
        }
    }

    return false;
}

bool Ft8RxDecoder::osdLiteRepair174_91(const std::array<double, 174> &posterior,
                                      std::array<int, 174> &bits) const
{
    /*
     * v4.10 AP/OSD lab: tiny ordered-statistics style repair pass.  This is
     * intentionally conservative and is NOT a full WSJT-X osd174_91 port yet.
     * It is used only after BP/min-sum failed to satisfy the syndrome on a
     * sync-plausible candidate.  Try the least-reliable hard decisions first:
     * one-bit flips, then two-bit flips in a very small reliability window.
     * CRC+unpack validation remains mandatory in decodeCandidate(), so this
     * cannot emit an unchecked fabricated message.
     */
    struct Reliability
    {
        int bit = 0;
        double absPost = 0.0;
    };

    std::array<Reliability, 174> order{};
    for (int i = 0; i < 174; ++i) {
        bits[i] = (posterior[i] < 0.0) ? 1 : 0;
        order[i] = Reliability{i, std::abs(posterior[i])};
    }

    if (syndromeOk(bits)) {
        return true;
    }

    std::sort(order.begin(), order.end(), [](const Reliability &a, const Reliability &b) {
        return a.absPost < b.absPost;
    });

    constexpr int kSingleWindow = 14;
    constexpr int kPairWindow = 10;

    const std::array<int, 174> base = bits;
    for (int a = 0; a < kSingleWindow; ++a) {
        bits = base;
        bits[order[a].bit] ^= 1;
        if (syndromeOk(bits)) {
            return true;
        }
    }

    for (int a = 0; a < kPairWindow; ++a) {
        for (int b = a + 1; b < kPairWindow; ++b) {
            bits = base;
            bits[order[a].bit] ^= 1;
            bits[order[b].bit] ^= 1;
            if (syndromeOk(bits)) {
                return true;
            }
        }
    }

    bits = base;
    return false;
}

bool Ft8RxDecoder::syndromeOk(const std::array<int, 174> &bits) const
{
    for (int c = 0; c < 83; ++c) {
        int parity = 0;
        for (int e = 0; e < nrw_ft8_174_91[c]; ++e) {
            const int v = Nm_ft8_174_91_[c][e] - 1;
            if (v >= 0 && v < 174) {
                parity ^= (bits[v] & 1);
            }
        }
        if (parity != 0) {
            return false;
        }
    }
    return true;
}

unsigned int Ft8RxDecoder::crc14(const unsigned char *data, int length) const
{
    return static_cast<unsigned int>(boost::augmented_crc<14, TRUNCATED_POLYNOMIAL14>(data, length)) & 0x3fffu;
}

bool Ft8RxDecoder::crc14Ok(const std::array<int, 174> &bits) const
{
    unsigned char bytes[12];
    for (unsigned char &b : bytes) {
        b = 0;
    }
    int pos = 0;
    for (int i = 0; i < 10; ++i) {
        int v = 0;
        for (int j = 0; j < 8; ++j) {
            v <<= 1;
            if (pos < 77) {
                v |= (bits[pos] & 1);
            } else {
                v |= 0; // FT8 CRC is calculated with bits 78..80 forced to zero.
            }
            ++pos;
        }
        bytes[i] = static_cast<unsigned char>(v);
    }

    const unsigned int expected = crc14(bytes, 12);
    unsigned int received = 0;
    for (int i = 0; i < 14; ++i) {
        received <<= 1;
        received |= static_cast<unsigned int>(bits[77 + i] & 1);
    }
    return (expected & 0x3fffu) == (received & 0x3fffu);
}

QString Ft8RxDecoder::unpackMessage77(const std::array<int, 174> &bits)
{
    std::lock_guard<std::mutex> lock(m_unpackMutex);
    bool c77[100];
    for (bool &b : c77) {
        b = false;
    }
    for (int i = 0; i < 77; ++i) {
        c77[i] = bits[i] != 0;
    }
    bool success = false;
    QString message = m_unpacker.unpack77(c77, success);
    if (!success) {
        return QString();
    }
    return message;
}

bool Ft8RxDecoder::isSyncSymbol(int symbolIndex)
{
    return (symbolIndex >= 0 && symbolIndex < 7) ||
           (symbolIndex >= 36 && symbolIndex < 43) ||
           (symbolIndex >= 72 && symbolIndex < 79);
}

int Ft8RxDecoder::dataSymbolIndex(int symbolIndex)
{
    if (isSyncSymbol(symbolIndex)) {
        return -1;
    }
    if (symbolIndex < 36) {
        return symbolIndex - 7;
    }
    if (symbolIndex < 72) {
        return 29 + (symbolIndex - 43);
    }
    return -1;
}

int Ft8RxDecoder::grayInverse(int tone)
{
    for (int i = 0; i < 8; ++i) {
        if (kGrayMap[i] == tone) {
            return i;
        }
    }
    return 0;
}
