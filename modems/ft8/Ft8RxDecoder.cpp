#include "Ft8RxDecoder.h"
#include "Ft8Mode.h"
#include "../../dsp/cpu/CpuFeatures.h"
#include "../../utils/SystemResourceManager.h"
#include "../../audio/WavFileReader.h"

#define MN_NM_NRW_FT_174_91
#include "../../third_party/mshv_gpl/port/HvGenFt8/bpdecode_ft8_174_91.h"
#include "../../third_party/mshv_gpl/port/boost/boost_14.hpp"
#include "../../third_party/mshv_gpl/port/genpom.h"

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


class FtDecodeCoordinator
{
public:
    FtDecodeCoordinator()
        : m_thread([this]() { run(); })
    {
    }

    ~FtDecodeCoordinator()
    {
        stopAndJoin();
    }

    bool submit(std::function<void()> job)
    {
        if (!job) {
            return false;
        }
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_stopping || !m_jobs.empty()) {
                return false;
            }
            m_jobs.emplace_back(std::move(job));
        }
        m_cv.notify_one();
        return true;
    }

    void stopAndJoin() noexcept
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_stopping) {
                // Another caller already requested stop; still join below.
            }
            m_stopping = true;
            m_jobs.clear();
        }
        m_cv.notify_all();
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

private:
    void run()
    {
        for (;;) {
            std::function<void()> job;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cv.wait(lock, [this]() { return m_stopping || !m_jobs.empty(); });
                if (m_stopping && m_jobs.empty()) {
                    return;
                }
                job = std::move(m_jobs.front());
                m_jobs.pop_front();
            }
            try {
                job();
            } catch (...) {
                // Decoder jobs report their own exceptions.  Never let one job
                // terminate the persistent coordinator thread.
            }
        }
    }

    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::deque<std::function<void()>> m_jobs;
    bool m_stopping = false;
    std::thread m_thread;
};

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

thread_local MadModemRuntime::WorkClass t_currentFtWorkClass = MadModemRuntime::WorkClass::FtBoundary;

class ScopedFtWorkClass
{
public:
    explicit ScopedFtWorkClass(MadModemRuntime::WorkClass workClass)
        : m_previous(t_currentFtWorkClass)
    {
        t_currentFtWorkClass = workClass;
    }
    ~ScopedFtWorkClass() { t_currentFtWorkClass = m_previous; }
private:
    MadModemRuntime::WorkClass m_previous;
};

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
 * Process-wide adaptive FT worker pool.
 *
 * The pool is persistent, sized from the processors actually available to the
 * process, and receives per-job concurrency budgets from SystemResourceManager.
 * It is deliberately separate from Qt's global pool so audio, GUI, waterfall
 * and CAT work cannot be consumed by a boundary decode burst.
 */
class FtDecodeWorkerPool
{
public:
    static FtDecodeWorkerPool &instance()
    {
        static FtDecodeWorkerPool pool;
        return pool;
    }

    int recommendedWorkerCount(MadModemRuntime::WorkClass workClass, int itemCount) const
    {
        return MadModemRuntime::SystemResourceManager::instance().recommendedWorkers(workClass, itemCount);
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
        const int count = qMax(1, MadModemRuntime::SystemResourceManager::instance().poolCapacity());
        m_workers.reserve(static_cast<size_t>(count));
        for (int i = 0; i < count; ++i) {
            m_workers.emplace_back([this, i]() { workerLoop(i); });
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

    void workerLoop(int workerIndex)
    {
        MadModemRuntime::SystemResourceManager::instance().configureCurrentWorkerThread(workerIndex);
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


// Robust percentile helper used by the FT8/FT4 spectral baseline estimators.
double percentileCopy(std::vector<double> values, double fraction)
{
    if (values.empty()) {
        return 0.0;
    }
    fraction = std::max(0.0, std::min(1.0, fraction));
    const size_t index = static_cast<size_t>(std::llround(fraction * static_cast<double>(values.size() - 1U)));
    std::nth_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(index), values.end());
    return values[index];
}

std::array<double, 5> fitPolynomial4(const std::vector<std::pair<double, double>> &points, bool *okOut = nullptr)
{
    std::array<std::array<double, 6>, 5> a{};
    if (okOut != nullptr) {
        *okOut = false;
    }
    if (points.size() < 8U) {
        return {};
    }

    for (const auto &point : points) {
        double xp[9];
        xp[0] = 1.0;
        for (int i = 1; i < 9; ++i) {
            xp[i] = xp[i - 1] * point.first;
        }
        for (int row = 0; row < 5; ++row) {
            for (int col = 0; col < 5; ++col) {
                a[row][col] += xp[row + col];
            }
            a[row][5] += point.second * xp[row];
        }
    }

    for (int col = 0; col < 5; ++col) {
        int pivot = col;
        for (int row = col + 1; row < 5; ++row) {
            if (std::abs(a[row][col]) > std::abs(a[pivot][col])) {
                pivot = row;
            }
        }
        if (std::abs(a[pivot][col]) < 1.0e-12) {
            return {};
        }
        if (pivot != col) {
            std::swap(a[pivot], a[col]);
        }
        const double inv = 1.0 / a[col][col];
        for (int j = col; j < 6; ++j) {
            a[col][j] *= inv;
        }
        for (int row = 0; row < 5; ++row) {
            if (row == col) {
                continue;
            }
            const double factor = a[row][col];
            for (int j = col; j < 6; ++j) {
                a[row][j] -= factor * a[col][j];
            }
        }
    }

    std::array<double, 5> coefficients{};
    for (int i = 0; i < 5; ++i) {
        coefficients[i] = a[i][5];
    }
    if (okOut != nullptr) {
        *okOut = true;
    }
    return coefficients;
}

std::vector<double> robustPolynomialBaseline(const std::vector<double> &power,
                                             int firstBin,
                                             int lastBin,
                                             int segmentCount = 10,
                                             double lowerFraction = 0.10)
{
    std::vector<double> baseline(power.size(), 1.0);
    if (power.empty()) {
        return baseline;
    }
    firstBin = std::max(0, firstBin);
    lastBin = std::min(static_cast<int>(power.size()) - 1, lastBin);
    if (lastBin - firstBin < 20) {
        return baseline;
    }

    const int span = lastBin - firstBin + 1;
    const double midpoint = 0.5 * static_cast<double>(firstBin + lastBin);
    const double xScale = std::max(1.0, 0.5 * static_cast<double>(span));
    std::vector<double> db(power.size(), -180.0);
    for (int i = firstBin; i <= lastBin; ++i) {
        db[static_cast<size_t>(i)] = 10.0 * std::log10(std::max(power[static_cast<size_t>(i)], kEps));
    }

    std::vector<std::pair<double, double>> lowerEnvelope;
    lowerEnvelope.reserve(static_cast<size_t>(span / 5));
    segmentCount = std::max(4, std::min(segmentCount, span / 4));
    for (int segment = 0; segment < segmentCount; ++segment) {
        const int a = firstBin + (span * segment) / segmentCount;
        const int b = firstBin + (span * (segment + 1)) / segmentCount - 1;
        if (b < a) {
            continue;
        }
        std::vector<double> segmentValues;
        segmentValues.reserve(static_cast<size_t>(b - a + 1));
        for (int i = a; i <= b; ++i) {
            segmentValues.push_back(db[static_cast<size_t>(i)]);
        }
        const double cut = percentileCopy(segmentValues, lowerFraction);
        for (int i = a; i <= b; ++i) {
            if (db[static_cast<size_t>(i)] <= cut) {
                const double x = (static_cast<double>(i) - midpoint) / xScale;
                lowerEnvelope.emplace_back(x, db[static_cast<size_t>(i)]);
            }
        }
    }

    bool fitOk = false;
    const std::array<double, 5> c = fitPolynomial4(lowerEnvelope, &fitOk);
    if (!fitOk) {
        const std::vector<double> all(db.begin() + firstBin, db.begin() + lastBin + 1);
        const double floorDb = percentileCopy(all, 0.20);
        const double floorPower = std::pow(10.0, floorDb / 10.0);
        std::fill(baseline.begin() + firstBin, baseline.begin() + lastBin + 1,
                  std::max(floorPower, kEps));
        return baseline;
    }

    // WSJT-X FT4 adds 0.65 dB above the fitted lower envelope.  Keep the same
    // conservative lift so normalization does not exaggerate tiny notches.
    constexpr double kBaselineLiftDb = 0.65;
    for (int i = firstBin; i <= lastBin; ++i) {
        const double x = (static_cast<double>(i) - midpoint) / xScale;
        const double fittedDb = c[0] + x * (c[1] + x * (c[2] + x * (c[3] + x * c[4]))) + kBaselineLiftDb;
        baseline[static_cast<size_t>(i)] = std::max(std::pow(10.0, fittedDb / 10.0), kEps);
    }
    return baseline;
}

const std::vector<double> &nuttallWindow4096()
{
    static const std::vector<double> window = []() {
        constexpr int n = 4096;
        std::vector<double> out(static_cast<size_t>(n), 0.0);
        constexpr double a0 = 0.355768;
        constexpr double a1 = 0.487396;
        constexpr double a2 = 0.144232;
        constexpr double a3 = 0.012604;
        for (int i = 0; i < n; ++i) {
            const double x = kTwoPi * static_cast<double>(i) / static_cast<double>(n - 1);
            out[static_cast<size_t>(i)] = a0 - a1 * std::cos(x) + a2 * std::cos(2.0 * x) - a3 * std::cos(3.0 * x);
        }
        return out;
    }();
    return window;
}

std::vector<double> makeWindowedSincLowpass(int tapCount, double cutoffHz, double sampleRate)
{
    if ((tapCount & 1) == 0) {
        ++tapCount;
    }
    tapCount = std::max(9, tapCount);
    const int half = tapCount / 2;
    const double fc = std::max(1.0, std::min(cutoffHz, sampleRate * 0.49)) / sampleRate;
    std::vector<double> taps(static_cast<size_t>(tapCount), 0.0);
    double sum = 0.0;
    for (int i = -half; i <= half; ++i) {
        const double sinc = (i == 0)
            ? (2.0 * fc)
            : (std::sin(kTwoPi * fc * static_cast<double>(i)) / ((kTwoPi * 0.5) * static_cast<double>(i)));
        const double w = 0.54 + 0.46 * std::cos((kTwoPi * 0.5) * static_cast<double>(i) / static_cast<double>(half + 1));
        const double value = sinc * w;
        taps[static_cast<size_t>(i + half)] = value;
        sum += value;
    }
    if (std::abs(sum) < 1.0e-12) {
        sum = 1.0;
    }
    for (double &tap : taps) {
        tap /= sum;
    }
    return taps;
}

bool extractComplexBaseband(const QVector<double> &samples,
                            int firstCenterSample,
                            int outputCount,
                            int decimation,
                            double mixFrequencyHz,
                            const std::vector<double> &taps,
                            std::vector<std::complex<double>> &out)
{
    if (outputCount <= 0 || decimation <= 0 || taps.empty()) {
        out.clear();
        return false;
    }
    const int half = static_cast<int>(taps.size()) / 2;
    out.assign(static_cast<size_t>(outputCount), std::complex<double>(0.0, 0.0));

    const double omega = -kTwoPi * mixFrequencyHz / 12000.0;
    std::vector<std::complex<double>> mixedTaps(taps.size());
    for (int k = -half; k <= half; ++k) {
        const double phase = omega * static_cast<double>(k);
        mixedTaps[static_cast<size_t>(k + half)] = taps[static_cast<size_t>(k + half)] *
            std::complex<double>(std::cos(phase), std::sin(phase));
    }

    const double firstPhase = omega * static_cast<double>(firstCenterSample);
    std::complex<double> centerOsc(std::cos(firstPhase), std::sin(firstPhase));
    const double stepPhase = omega * static_cast<double>(decimation);
    const std::complex<double> centerStep(std::cos(stepPhase), std::sin(stepPhase));
    bool any = false;
    for (int m = 0; m < outputCount; ++m) {
        const int center = firstCenterSample + m * decimation;
        std::complex<double> sum(0.0, 0.0);
        for (int k = -half; k <= half; ++k) {
            const int index = center + k;
            if (index >= 0 && index < samples.size()) {
                sum += samples.at(index) * mixedTaps[static_cast<size_t>(k + half)];
                any = true;
            }
        }
        out[static_cast<size_t>(m)] = sum * centerOsc;
        centerOsc *= centerStep;
        if ((m & 255) == 255) {
            const double mag = std::abs(centerOsc);
            if (mag > 0.0) {
                centerOsc /= mag;
            }
        }
    }
    return any;
}

void normalizeSoftMetric(std::array<double, 174> &metric, double targetSigma = 2.83)
{
    double mean = 0.0;
    double mean2 = 0.0;
    for (double value : metric) {
        mean += value;
        mean2 += value * value;
    }
    mean /= static_cast<double>(metric.size());
    mean2 /= static_cast<double>(metric.size());
    const double variance = std::max(0.0, mean2 - mean * mean);
    const double sigma = std::sqrt(variance);
    const double scale = (sigma > 1.0e-8 && std::isfinite(sigma)) ? (targetSigma / sigma) : 1.0;
    for (double &value : metric) {
        value = qBound(-18.0, value * scale, 18.0);
    }
}

void smoothComplexEnvelopeZeroPhaseGeneric(std::vector<std::complex<double>> &env,
                                           double tauSamples,
                                           int rampSamples)
{
    if (env.empty()) {
        return;
    }
    tauSamples = std::max(1.0, tauSamples);
    const double alpha = std::exp(-1.0 / tauSamples);
    const double beta = 1.0 - alpha;
    std::complex<double> acc = env.front();
    for (std::complex<double> &value : env) {
        acc = alpha * acc + beta * value;
        value = acc;
    }
    acc = env.back();
    for (auto it = env.rbegin(); it != env.rend(); ++it) {
        acc = alpha * acc + beta * (*it);
        *it = acc;
    }
    rampSamples = std::max(0, std::min(rampSamples, static_cast<int>(env.size()) / 2));
    for (int i = 0; i < rampSamples; ++i) {
        const double ramp = 0.5 - 0.5 * std::cos(kTwoPi * static_cast<double>(i + 1) /
                                                   static_cast<double>(2 * rampSamples + 2));
        env[static_cast<size_t>(i)] *= ramp;
        env[env.size() - 1U - static_cast<size_t>(i)] *= ramp;
    }
}

const std::vector<double> &ft4RxGfskPulse()
{
    static const std::vector<double> pulse = []() {
        std::vector<double> out(static_cast<size_t>(3 * 576), 0.0);
        gen_pulse_gfsk_(out.data(), 864.0, 1.0, 576);
        return out;
    }();
    return pulse;
}

const std::array<std::vector<double>, 4> &ft4RxGfskToneDphiTable()
{
    static const std::array<std::vector<double>, 4> table = []() {
        constexpr int nsps = 576;
        constexpr double dphiPeak = kTwoPi / static_cast<double>(nsps);
        std::array<std::vector<double>, 4> result;
        const std::vector<double> &pulse = ft4RxGfskPulse();
        for (int tone = 0; tone < 4; ++tone) {
            result[static_cast<size_t>(tone)].resize(static_cast<size_t>(3 * nsps));
            for (int i = 0; i < 3 * nsps; ++i) {
                result[static_cast<size_t>(tone)][static_cast<size_t>(i)] =
                    pulse[static_cast<size_t>(i)] * dphiPeak * static_cast<double>(tone);
            }
        }
        return result;
    }();
    return table;
}

void makeFt4ReferenceWaveformRx(const std::array<int, 103> &tones,
                                double baseHz,
                                std::vector<std::complex<double>> &wave,
                                std::vector<double> &dphi)
{
    constexpr int nsym = 103;
    constexpr int nsps = 576;
    constexpr int nwave = (nsym + 2) * nsps;
    const auto &toneDphi = ft4RxGfskToneDphiTable();
    dphi.assign(static_cast<size_t>(nwave + 16), 0.0);
    wave.resize(static_cast<size_t>(nwave));
    for (int sym = 0; sym < nsym; ++sym) {
        const int tone = qBound(0, tones[static_cast<size_t>(sym)], 3);
        const int base = sym * nsps;
        const auto &shape = toneDphi[static_cast<size_t>(tone)];
        for (int i = 0; i < 3 * nsps; ++i) {
            dphi[static_cast<size_t>(base + i)] += shape[static_cast<size_t>(i)];
        }
    }
    const double carrierStep = kTwoPi * baseHz / 12000.0;
    double phase = 0.0;
    for (int i = 0; i < nwave; ++i) {
        wave[static_cast<size_t>(i)] = std::complex<double>(std::cos(phase), std::sin(phase));
        phase += dphi[static_cast<size_t>(i)] + carrierStep;
        phase = std::fmod(phase, kTwoPi);
        if (phase < 0.0) {
            phase += kTwoPi;
        }
    }
    for (int i = 0; i < nsps; ++i) {
        const double rampIn = 0.5 * (1.0 - std::cos(kTwoPi * static_cast<double>(i) /
                                                   static_cast<double>(2 * nsps)));
        wave[static_cast<size_t>(i)] *= rampIn;
    }
    const int rampOutStart = (nsym + 1) * nsps;
    for (int i = 0; i < nsps; ++i) {
        const int index = rampOutStart + i;
        if (index >= 0 && index < nwave) {
            const double rampOut = 0.5 * (1.0 + std::cos(kTwoPi * static_cast<double>(i) /
                                                        static_cast<double>(2 * nsps)));
            wave[static_cast<size_t>(index)] *= rampOut;
        }
    }
}



using OfflineWavFormat = MadModemAudio::WavFileFormat;

bool parseOfflineWavHeader(QFile &file, OfflineWavFormat &format, QString &error)
{
    return MadModemAudio::parseWavHeader(file, format, error);
}

QVector<float> convertOfflineWavToMono(const QByteArray &raw, const OfflineWavFormat &format)
{
    return MadModemAudio::convertWavBytesToMono(raw, format);
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

    /* findFt4Candidates() ranks a normalized spectral/Costas score and does
     * not carry the WSJT-X candidate(2) signal/noise ratio. Using candidate.syncRatio
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
      m_decodeCoordinator(std::make_unique<FtDecodeCoordinator>()),
      m_unpacker(true)
{
    qRegisterMetaType<Ft8RxDecoder::Decode>("Ft8RxDecoder::Decode");
    qRegisterMetaType<Ft8RxDecoder::PerfStats>("Ft8RxDecoder::PerfStats");
    m_slotSamples.reserve(kSlotSamples + 4096);
    // Create the persistent workers during application initialization rather
    // than on the first busy FT slot, avoiding a one-time RX-start stall.
    (void)FtDecodeWorkerPool::instance();
}

Ft8RxDecoder::~Ft8RxDecoder()
{
    requestShutdown();
    if (m_decodeCoordinator) {
        m_decodeCoordinator->stopAndJoin();
    }
}

void Ft8RxDecoder::requestShutdown() noexcept
{
    m_shutdown.store(true, std::memory_order_release);
    m_decodeGeneration.fetch_add(1, std::memory_order_acq_rel);
}


void Ft8RxDecoder::reset()
{
    std::lock_guard<std::recursive_mutex> configLock(m_decodeConfigMutex);
    ++m_decodeGeneration;
    m_inputSampleRate = 0;
    m_resampleAbsoluteInputIndex = 0.0;
    m_resampleNextOutputInputIndex = 0.0;
    m_resamplePreviousSample = 0.0;
    m_resampleHavePreviousSample = false;
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
    m_audioBlocksReceived = 0;
    m_audioBoundarySplits = 0;
    m_audioSequenceGaps = 0;
    m_lastCaptureSequence = 0;
    m_audioGapSamples = 0;
    m_audioOverlapSamples = 0;
    m_maxCaptureQueueLatencyMs = 0.0;
    m_latestCaptureQueueLatencyMs = 0.0;
    m_activeCaptureGeneration = 0;
    m_staleCaptureBlocks = 0;
    m_timestampJumps = 0;
    m_invalidSlotsSkipped = 0;
    m_lastAcceptedEndSampleUtcNs = 0;
    m_lastTimelineDiagnosticUtcMs = 0;
    m_skipCurrentSlotDecode = false;
    m_firstSlotInCapture = true;
    m_skipCurrentSlotReason.clear();
    m_resourceSummaryEmitted = false;
    {
        std::lock_guard<std::mutex> lock(m_emittedDecodeMutex);
        m_emittedDecodeKeys.clear();
    }
    m_currentSlotStartUtc = QDateTime();
    m_slotSamples.clear();
    m_pendingFinalSamples.clear();
    m_pendingFinalSlotStartUtc = QDateTime();
    m_pendingFinalDecode = false;
    m_lastCandidateCount = 0;
    emit statusChanged(currentShortLabel() + QStringLiteral(" RX: waiting for first slot audio"));
}

void Ft8RxDecoder::setSearchRangeHz(int lowHz, int highHz)
{
    std::lock_guard<std::recursive_mutex> configLock(m_decodeConfigMutex);
    m_searchLowHz = qBound(50, lowHz, 3500);
    m_searchHighHz = qBound(m_searchLowHz + 50, highHz, 3600);
}

void Ft8RxDecoder::setRxMarkerHz(int hz)
{
    std::lock_guard<std::recursive_mutex> configLock(m_decodeConfigMutex);
    m_rxMarkerHz = qBound(100, hz, 3200);
    // Like WSJT-X/MSHV, decode the full FT8 audio passband.  The green RX
    // marker remains the selected/QSO audio frequency, not a narrow decode gate.
    setSearchRangeHz(100, 3000);
}

void Ft8RxDecoder::setMyCall(const QString &call)
{
    std::lock_guard<std::recursive_mutex> configLock(m_decodeConfigMutex);
    m_myCall = call.trimmed().toUpper();
    std::lock_guard<std::mutex> lock(m_unpackMutex);
    m_unpacker.save_hash_call_my_his_r1_r2(m_myCall, 0);
}

void Ft8RxDecoder::setDxCall(const QString &call)
{
    std::lock_guard<std::recursive_mutex> configLock(m_decodeConfigMutex);
    m_dxCall = call.trimmed().toUpper();
    std::lock_guard<std::mutex> lock(m_unpackMutex);
    m_unpacker.save_hash_call_my_his_r1_r2(m_dxCall, 1);
}

void Ft8RxDecoder::setQsoDeadlineActive(bool active)
{
    m_qsoDeadlineActive.store(active, std::memory_order_release);
}

void Ft8RxDecoder::setModeName(const QString &modeName)
{
    std::lock_guard<std::recursive_mutex> configLock(m_decodeConfigMutex);
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
    std::lock_guard<std::recursive_mutex> configLock(m_decodeConfigMutex);
    return m_modeName;
}

void Ft8RxDecoder::setDecodeEngine(const QString &engineName)
{
    std::lock_guard<std::recursive_mutex> configLock(m_decodeConfigMutex);
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
    std::lock_guard<std::recursive_mutex> configLock(m_decodeConfigMutex);
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
    std::lock_guard<std::recursive_mutex> configLock(m_decodeConfigMutex);
    return m_deepDecodeEnabled;
}

bool Ft8RxDecoder::dspPlusDecodeEnabled() const
{
    std::lock_guard<std::recursive_mutex> configLock(m_decodeConfigMutex);
    return m_dspPlusDecodeEnabled;
}

void Ft8RxDecoder::setDeepDecodeEnabled(bool enabled)
{
    std::lock_guard<std::recursive_mutex> configLock(m_decodeConfigMutex);
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
    std::lock_guard<std::recursive_mutex> configLock(m_decodeConfigMutex);
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
    std::lock_guard<std::recursive_mutex> configLock(m_decodeConfigMutex);
    return Ft8Mode::profileForMode(m_modeName).slotMs;
}

int Ft8RxDecoder::currentSlotSamples() const
{
    return qMax(1, (currentSlotMs() * kDecodeSampleRate) / 1000);
}

QString Ft8RxDecoder::currentShortLabel() const
{
    std::lock_guard<std::recursive_mutex> configLock(m_decodeConfigMutex);
    return Ft8Mode::profileForMode(m_modeName).shortLabel;
}


int Ft8RxDecoder::postTxPrepadLimitMs() const
{
    std::lock_guard<std::recursive_mutex> configLock(m_decodeConfigMutex);
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

QVector<double> Ft8RxDecoder::resampleTo12k(const AudioBlock &block, qint64 *firstOutputUtcNs)
{
    QVector<double> out;
    if (firstOutputUtcNs != nullptr) {
        *firstOutputUtcNs = 0;
    }
    if (block.samples.size() < 2 || block.sampleRate <= 0) {
        return out;
    }

    if (m_inputSampleRate != block.sampleRate) {
        m_inputSampleRate = block.sampleRate;
        m_resampleAbsoluteInputIndex = 0.0;
        m_resampleNextOutputInputIndex = 0.0;
        m_resamplePreviousSample = 0.0;
        m_resampleHavePreviousSample = false;
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
    out.reserve(static_cast<int>(std::ceil(filtered.size() / step)) + 2);
    const double blockAbsoluteStart = m_resampleAbsoluteInputIndex;
    bool recordedFirstTime = false;
    for (double currentSample : filtered) {
        const double currentIndex = m_resampleAbsoluteInputIndex;
        if (!m_resampleHavePreviousSample) {
            m_resamplePreviousSample = currentSample;
            m_resampleHavePreviousSample = true;
            if (m_resampleNextOutputInputIndex <= currentIndex) {
                if (firstOutputUtcNs != nullptr && block.firstSampleUtcNs > 0) {
                    *firstOutputUtcNs = block.firstSampleUtcNs;
                }
                recordedFirstTime = true;
                out.append(currentSample);
                m_resampleNextOutputInputIndex += step;
            }
            m_resampleAbsoluteInputIndex += 1.0;
            continue;
        }

        const double previousIndex = currentIndex - 1.0;
        while (m_resampleNextOutputInputIndex <= currentIndex) {
            if (!recordedFirstTime && firstOutputUtcNs != nullptr && block.firstSampleUtcNs > 0) {
                const double relativeInputIndex = m_resampleNextOutputInputIndex - blockAbsoluteStart;
                const qint64 offsetNs = static_cast<qint64>(std::llround(
                    (1000000000.0 * relativeInputIndex) / static_cast<double>(block.sampleRate)));
                *firstOutputUtcNs = block.firstSampleUtcNs + offsetNs;
                recordedFirstTime = true;
            }
            const double frac = qBound(0.0, m_resampleNextOutputInputIndex - previousIndex, 1.0);
            out.append(m_resamplePreviousSample + frac * (currentSample - m_resamplePreviousSample));
            m_resampleNextOutputInputIndex += step;
        }
        m_resamplePreviousSample = currentSample;
        m_resampleAbsoluteInputIndex += 1.0;
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
    if (m_decodeJobActive.load(std::memory_order_acquire)) {
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

    while (remaining > 0 && !decodeCancellationRequested()) {
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
    // its established offline reference budget for optional reproducible analysis;
    // FT4 deliberately does NOT enable the offline/deep special path because it
    // would test a different engine than live RX.
    m_offlineAnalysisActive.store(!ft4LiveEngineTest);

    for (int slot = 0; slot < slotCount && !decodeCancellationRequested(); ++slot) {
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
        // Signals above are queued to the GUI automatically.  Pumping this
        // worker's event queue here allowed settings/reset slots to re-enter
        // the offline decode loop between slots; shutdown cancellation is
        // already atomic and is checked by the loop condition.
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

bool Ft8RxDecoder::decodeCancellationRequested() const noexcept
{
    if (m_shutdown.load(std::memory_order_acquire)) {
        return true;
    }
    const int running = m_runningDecodeGeneration.load(std::memory_order_acquire);
    return running > 0 && running != m_decodeGeneration.load(std::memory_order_acquire);
}

void Ft8RxDecoder::emitTimelineDiagnosticThrottled(const QString &message)
{
    const qint64 nowMs = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
    if (m_lastTimelineDiagnosticUtcMs > 0 && nowMs - m_lastTimelineDiagnosticUtcMs < 1000) {
        return;
    }
    m_lastTimelineDiagnosticUtcMs = nowMs;
    emit runtimeDiagnostic(message);
}

void Ft8RxDecoder::markCurrentSlotInvalid(const QString &reason)
{
    if (reason.trimmed().isEmpty()) {
        return;
    }
    m_skipCurrentSlotDecode = true;
    if (m_skipCurrentSlotReason.isEmpty()) {
        m_skipCurrentSlotReason = reason.trimmed();
    } else if (!m_skipCurrentSlotReason.contains(reason.trimmed())) {
        m_skipCurrentSlotReason += QStringLiteral("; ") + reason.trimmed();
    }
}

void Ft8RxDecoder::resetCaptureTimeline(quint64 generation, const QString &reason)
{
    m_decodeGeneration.fetch_add(1, std::memory_order_acq_rel);
    m_activeCaptureGeneration = generation > 0 ? generation : 1;
    m_inputSampleRate = 0;
    m_resampleAbsoluteInputIndex = 0.0;
    m_resampleNextOutputInputIndex = 0.0;
    m_resamplePreviousSample = 0.0;
    m_resampleHavePreviousSample = false;
    m_resamplePrefilterRate = 0;
    m_resampleLp1 = 0.0;
    m_resampleLp2 = 0.0;
    m_currentSlotId = -1;
    m_earlyDecodeSlotId = -1;
    m_streamingDecodeSlotId = -1;
    m_lastStreamingDecodeSamples = 0;
    m_finalDecodeLaunchedForSlot = false;
    m_postTxIgnoreSlotId = -1;
    m_initialUtcPadSamples = 0;
    m_skipCurrentSlotDecode = false;
    m_firstSlotInCapture = true;
    m_skipCurrentSlotReason.clear();
    m_currentSlotStartUtc = QDateTime();
    m_slotSamples.clear();
    m_pendingFinalSamples.clear();
    m_pendingFinalSlotStartUtc = QDateTime();
    m_pendingFinalDecode = false;
    m_lastCaptureSequence = 0;
    m_lastAcceptedEndSampleUtcNs = 0;
    m_audioBlocksReceived = 0;
    m_audioBoundarySplits = 0;
    m_audioSequenceGaps = 0;
    m_audioGapSamples = 0;
    m_audioOverlapSamples = 0;
    m_maxCaptureQueueLatencyMs = 0.0;
    m_latestCaptureQueueLatencyMs = 0.0;
    m_staleCaptureBlocks = 0;
    m_timestampJumps = 0;
    m_invalidSlotsSkipped = 0;
    m_lastTimelineDiagnosticUtcMs = 0;
    MadModemRuntime::SystemResourceManager::instance().beginFtCapture(m_modeName);
    emitTimelineDiagnosticThrottled(QStringLiteral("FT capture generation %1: timeline reset (%2)")
                                      .arg(m_activeCaptureGeneration)
                                      .arg(reason));
}

void Ft8RxDecoder::beginUtcSlotAtOffset(qint64 slotId, int prepadSamples, const QString &reason)
{
    const int slotSamples = currentSlotSamples();
    m_currentSlotId = slotId;
    m_earlyDecodeSlotId = -1;
    m_streamingDecodeSlotId = -1;
    m_lastStreamingDecodeSamples = 0;
    m_finalDecodeLaunchedForSlot = false;
    const qint64 startMs = slotId * static_cast<qint64>(currentSlotMs());
    m_currentSlotStartUtc = QDateTime::fromMSecsSinceEpoch(startMs, Qt::UTC);
    m_slotSamples.clear();
    m_initialUtcPadSamples = qBound(0, prepadSamples, qMax(0, slotSamples - 1));
    m_skipCurrentSlotDecode = false;
    m_skipCurrentSlotReason.clear();
    m_maxCaptureQueueLatencyMs = m_latestCaptureQueueLatencyMs;
    if (m_initialUtcPadSamples > 0) {
        m_slotSamples.fill(0.0, m_initialUtcPadSamples);
    }

    // The first timestamped slot of every RX capture is a synchronisation slot.
    // If capture began after its UTC boundary, never send that partial period to
    // Costas/LDPC. The following complete slot starts at offset zero normally.
    const bool firstSlot = m_firstSlotInCapture;
    m_firstSlotInCapture = false;
    if (firstSlot && m_initialUtcPadSamples > 0) {
        markCurrentSlotInvalid(QStringLiteral("first capture slot is partial: %1 samples missing at UTC start")
                                   .arg(m_initialUtcPadSamples));
    }

    // Later slots tolerate only ordinary callback jitter. Larger missing
    // prefixes indicate a restart/jump and are never decoded.
    const int maximumJitterPad = qRound(0.12 * static_cast<double>(kDecodeSampleRate));
    if (!firstSlot && m_initialUtcPadSamples > maximumJitterPad) {
        markCurrentSlotInvalid(QStringLiteral("partial slot: %1 real samples missing at UTC start")
                                   .arg(m_initialUtcPadSamples));
    }
    if (!reason.isEmpty()) {
        emit statusChanged(currentShortLabel() + QStringLiteral(" RX: %1; capture-timestamp aligned with %2 sample pre-pad")
                               .arg(reason)
                               .arg(m_initialUtcPadSamples));
    }
}

void Ft8RxDecoder::appendTimedResampled(const QVector<double> &samples, qint64 firstSampleUtcNs)
{
    if (samples.isEmpty()) {
        return;
    }

    const int slotSamples = currentSlotSamples();
    const qint64 slotNs = static_cast<qint64>(currentSlotMs()) * 1000000LL;
    constexpr qint64 kNsPerSecond = 1000000000LL;
    const int discontinuitySamples = kDecodeSampleRate / 2;

    if (firstSampleUtcNs <= 0) {
        const qint64 durationNs = static_cast<qint64>(std::llround(
            (1000000000.0 * static_cast<double>(samples.size())) /
            static_cast<double>(kDecodeSampleRate)));
        firstSampleUtcNs = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch() * 1000000LL - durationNs;
    }

    int sourcePos = 0;
    while (sourcePos < samples.size()) {
        const qint64 sampleUtcNs = firstSampleUtcNs + static_cast<qint64>(std::llround(
            (1000000000.0 * static_cast<double>(sourcePos)) /
            static_cast<double>(kDecodeSampleRate)));
        qint64 slotId = sampleUtcNs / slotNs;
        qint64 slotStartNs = slotId * slotNs;
        int slotOffset = static_cast<int>(std::llround(
            (static_cast<double>(sampleUtcNs - slotStartNs) *
             static_cast<double>(kDecodeSampleRate)) /
            static_cast<double>(kNsPerSecond)));
        if (slotOffset >= slotSamples) {
            ++slotId;
            slotOffset = 0;
        } else {
            slotOffset = qMax(0, slotOffset);
        }

        if (m_currentSlotId >= 0 && slotId < m_currentSlotId) {
            ++m_staleCaptureBlocks;
            emitTimelineDiagnosticThrottled(QStringLiteral("FT stale timestamp block rejected: slot %1 arrived while collecting %2")
                                              .arg(slotId).arg(m_currentSlotId));
            return;
        }

        if (m_currentSlotId >= 0 && slotId > m_currentSlotId + 1) {
            ++m_timestampJumps;
            markCurrentSlotInvalid(QStringLiteral("timestamp jump skipped %1 slot(s)")
                                       .arg(slotId - m_currentSlotId - 1));
            ++m_invalidSlotsSkipped;
            emitTimelineDiagnosticThrottled(QStringLiteral("FT timestamp jump %1→%2: discarded partial slot; no artificial intermediate slots generated")
                                              .arg(m_currentSlotId).arg(slotId));
            if (m_postTxIgnoreSlotId >= 0 && slotId != m_postTxIgnoreSlotId) {
                m_postTxIgnoreSlotId = -1;
            }
            beginUtcSlotAtOffset(slotId, slotOffset, QStringLiteral("capture timestamp jump realignment"));
            markCurrentSlotInvalid(QStringLiteral("slot began after timestamp jump"));
        } else if (m_currentSlotId != slotId) {
            if (m_currentSlotId >= 0 && !isPostTxIgnoredSlot()) {
                finishCurrentSlot();
            }
            if (m_postTxIgnoreSlotId >= 0 && slotId != m_postTxIgnoreSlotId) {
                m_postTxIgnoreSlotId = -1;
            }
            beginUtcSlotAtOffset(slotId, slotOffset,
                                 m_currentSlotId < 0
                                     ? QStringLiteral("collecting first timestamped slot")
                                     : QString());
        }

        int writable = qMin(samples.size() - sourcePos, slotSamples - slotOffset);
        if (writable <= 0) {
            ++m_audioBoundarySplits;
            beginUtcSlotAtOffset(slotId + 1, 0);
            continue;
        }

        int copySource = sourcePos;
        int copyOffset = slotOffset;
        int copyCount = writable;
        if (m_slotSamples.size() < copyOffset) {
            const int gap = copyOffset - m_slotSamples.size();
            m_slotSamples.resize(copyOffset);
            std::fill(m_slotSamples.end() - gap, m_slotSamples.end(), 0.0);
            m_audioGapSamples += gap;
            if (gap > discontinuitySamples) {
                ++m_timestampJumps;
                markCurrentSlotInvalid(QStringLiteral("intra-slot audio gap %1 samples").arg(gap));
            }
        } else if (m_slotSamples.size() > copyOffset) {
            const int overlap = qMin(copyCount, m_slotSamples.size() - copyOffset);
            copySource += overlap;
            copyOffset += overlap;
            copyCount -= overlap;
            m_audioOverlapSamples += overlap;
            if (overlap > discontinuitySamples) {
                ++m_timestampJumps;
                markCurrentSlotInvalid(QStringLiteral("intra-slot overlap %1 samples").arg(overlap));
            }
        }

        if (copyCount > 0) {
            if (m_slotSamples.size() < copyOffset + copyCount) {
                m_slotSamples.resize(copyOffset + copyCount);
            }
            std::copy(samples.constData() + copySource,
                      samples.constData() + copySource + copyCount,
                      m_slotSamples.data() + copyOffset);
        }

        sourcePos += writable;
        maybeStartStreamingDecodeSlot();

        if (slotOffset + writable >= slotSamples && sourcePos < samples.size()) {
            ++m_audioBoundarySplits;
            if (!isPostTxIgnoredSlot()) {
                finishCurrentSlot();
            }
            beginUtcSlotAtOffset(slotId + 1, 0);
        }
    }
}

void Ft8RxDecoder::processAudioBlock(const AudioBlock &block)
{
    if (!m_liveInputEnabled) {
        return;
    }

    maybeLaunchPendingFinalDecode();

    const quint64 incomingGeneration = block.captureGeneration > 0
        ? block.captureGeneration
        : (m_activeCaptureGeneration > 0 ? m_activeCaptureGeneration : quint64{1});
    if (m_activeCaptureGeneration == 0 || incomingGeneration > m_activeCaptureGeneration) {
        resetCaptureTimeline(incomingGeneration,
                             m_activeCaptureGeneration == 0
                                 ? QStringLiteral("first RX capture")
                                 : QStringLiteral("new AudioEngine capture session"));
    } else if (incomingGeneration < m_activeCaptureGeneration) {
        ++m_staleCaptureBlocks;
        emitTimelineDiagnosticThrottled(QStringLiteral("FT stale capture block rejected: generation %1, active %2")
                                          .arg(incomingGeneration).arg(m_activeCaptureGeneration));
        return;
    }

    if (!m_resourceSummaryEmitted) {
        m_resourceSummaryEmitted = true;
        emit runtimeDiagnostic(MadModemRuntime::SystemResourceManager::instance().startupSummary());
        emit runtimeDiagnostic(QStringLiteral("FT SIMD backend: %1").arg(MadModemCpu::summary()));
    }

    ++m_audioBlocksReceived;
    if (block.captureSequence > 0) {
        if (m_lastCaptureSequence > 0 && block.captureSequence > m_lastCaptureSequence + 1) {
            const quint64 missing = block.captureSequence - (m_lastCaptureSequence + 1);
            m_audioSequenceGaps += missing;
            markCurrentSlotInvalid(QStringLiteral("capture sequence gap %1 block(s)").arg(missing));
        } else if (m_lastCaptureSequence > 0 && block.captureSequence <= m_lastCaptureSequence) {
            ++m_staleCaptureBlocks;
            emitTimelineDiagnosticThrottled(QStringLiteral("FT non-monotonic capture sequence rejected: %1 after %2")
                                              .arg(block.captureSequence).arg(m_lastCaptureSequence));
            return;
        }
        m_lastCaptureSequence = block.captureSequence;
    }

    qint64 blockEndNs = 0;
    if (block.firstSampleUtcNs > 0) {
        const qint64 durationNs = static_cast<qint64>(std::llround(
            (1000000000.0 * static_cast<double>(block.samples.size())) /
            static_cast<double>(qMax(1, block.sampleRate))));
        blockEndNs = block.firstSampleUtcNs + durationNs;
        if (m_lastAcceptedEndSampleUtcNs > 0 && blockEndNs + 50000000LL < m_lastAcceptedEndSampleUtcNs) {
            ++m_staleCaptureBlocks;
            emitTimelineDiagnosticThrottled(QStringLiteral("FT stale audio timestamp rejected: block ended %1 ms behind active timeline")
                                              .arg(static_cast<double>(m_lastAcceptedEndSampleUtcNs - blockEndNs) / 1000000.0, 0, 'f', 1));
            return;
        }
        if (m_lastAcceptedEndSampleUtcNs > 0 && block.firstSampleUtcNs - m_lastAcceptedEndSampleUtcNs > 500000000LL) {
            ++m_timestampJumps;
            markCurrentSlotInvalid(QStringLiteral("capture timestamp advanced by %1 ms")
                                       .arg(static_cast<double>(block.firstSampleUtcNs - m_lastAcceptedEndSampleUtcNs) / 1000000.0, 0, 'f', 1));
        }

        const qint64 nowNs = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch() * 1000000LL;
        m_latestCaptureQueueLatencyMs =
            static_cast<double>(qMax<qint64>(qint64{0}, nowNs - blockEndNs)) / 1000000.0;
        m_maxCaptureQueueLatencyMs = qMax(m_maxCaptureQueueLatencyMs,
                                         m_latestCaptureQueueLatencyMs);
        m_lastAcceptedEndSampleUtcNs = qMax(m_lastAcceptedEndSampleUtcNs, blockEndNs);
    }

    qint64 firstOutputUtcNs = 0;
    const QVector<double> resampled = resampleTo12k(block, &firstOutputUtcNs);
    appendTimedResampled(resampled, firstOutputUtcNs);
}

void Ft8RxDecoder::setLiveInputEnabled(bool enabled)
{
    m_liveInputEnabled = enabled;
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
    const int realCapturedSamples = qMax(0, availableSamples - m_initialUtcPadSamples);
    const int minimumCompleteFrameSamples = (m_modeName == QStringLiteral("FT4"))
        ? qRound(4.48 * static_cast<double>(kDecodeSampleRate))
        : (kSymbols * kSamplesPerSymbol);

    // A restart near the end of a UTC period can make the slot vector appear
    // 90-97% full because its missing prefix is represented by intentional UTC
    // padding.  Do not launch a Costas/LDPC job on that padding: require enough
    // genuinely captured audio to contain at least one complete protocol frame.
    if (realCapturedSamples < minimumCompleteFrameSamples) {
        return;
    }

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

    if (m_decodeJobActive.load(std::memory_order_acquire)) {
        const qint64 startedMs = m_decodeJobStartedUtcMs.load(std::memory_order_acquire);
        const qint64 ageMs = startedMs > 0
            ? qMax<qint64>(qint64{0}, QDateTime::currentDateTimeUtc().toMSecsSinceEpoch() - startedMs)
            : 0;
        const QString message = currentShortLabel() +
            QStringLiteral(" RX: decoder busy at WSJT-X gate; gate not overlapped (active job age %1 ms)").arg(ageMs);
        emit statusChanged(message);
        emit runtimeDiagnostic(message);
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
    if (isPostTxIgnoredSlot() || m_finalDecodeLaunchedForSlot) {
        return;
    }

    const int slotSamples = currentSlotSamples();
    const int availableSamples = qMin(m_slotSamples.size(), slotSamples);
    const int realCapturedSamples = qMax(0, availableSamples - m_initialUtcPadSamples);
    const int minimumCompleteFrameSamples = (m_modeName == QStringLiteral("FT4"))
        ? qRound(4.48 * static_cast<double>(kDecodeSampleRate))
        : (kSymbols * kSamplesPerSymbol);

    if (m_skipCurrentSlotDecode || realCapturedSamples < minimumCompleteFrameSamples) {
        ++m_invalidSlotsSkipped;
        const QString reason = m_skipCurrentSlotDecode
            ? m_skipCurrentSlotReason
            : QStringLiteral("only %1 real samples, need %2")
                  .arg(realCapturedSamples).arg(minimumCompleteFrameSamples);
        const QString message = QStringLiteral("%1 RX: UTC slot %2 skipped before candidate search (%3; pre-pad %4, real %5/%6)")
            .arg(currentShortLabel())
            .arg(formatSlotTime(m_currentSlotStartUtc, currentSlotMs()))
            .arg(reason)
            .arg(m_initialUtcPadSamples)
            .arg(realCapturedSamples)
            .arg(slotSamples);
        emit statusChanged(message);
        emit runtimeDiagnostic(message);
        m_finalDecodeLaunchedForSlot = true;
        return;
    }

    const QVector<double> fullSlot = copyLeadingSamplesPadded(m_slotSamples, slotSamples, true);
    m_finalDecodeLaunchedForSlot = true;
    if (!m_decodeJobActive.load(std::memory_order_acquire)) {
        startAsyncDecodeSlot(fullSlot, m_currentSlotStartUtc, QStringLiteral("boundary"));
        return;
    }

    deferFinalDecode(fullSlot,
                     m_currentSlotStartUtc,
                     m_streamingDecodeSlotId == m_currentSlotId
                         ? QStringLiteral("gate worker still active at boundary")
                         : QStringLiteral("previous decode job still active at boundary"));
}

void Ft8RxDecoder::deferFinalDecode(const QVector<double> &samples,
                                    const QDateTime &slotStartUtc,
                                    const QString &reason)
{
    QString replacement;
    if (m_pendingFinalDecode && m_pendingFinalSlotStartUtc.isValid()) {
        replacement = QStringLiteral("; replacing stale pending slot %1")
                          .arg(formatSlotTime(m_pendingFinalSlotStartUtc, currentSlotMs()));
    }
    m_pendingFinalSamples = samples;
    m_pendingFinalSlotStartUtc = slotStartUtc;
    m_pendingFinalDecode = true;

    const qint64 startedMs = m_decodeJobStartedUtcMs.load(std::memory_order_acquire);
    const qint64 ageMs = startedMs > 0
        ? qMax<qint64>(qint64{0}, QDateTime::currentDateTimeUtc().toMSecsSinceEpoch() - startedMs)
        : 0;
    const QString message = QStringLiteral("%1 live decode boundary deferred for slot %2: %3; active job age %4 ms%5")
        .arg(currentShortLabel())
        .arg(formatSlotTime(slotStartUtc, currentSlotMs()))
        .arg(reason)
        .arg(ageMs)
        .arg(replacement);
    emit statusChanged(message);
    emit runtimeDiagnostic(message);
}

void Ft8RxDecoder::startAsyncDecodeSlot(const QVector<double> &samples,
                                          const QDateTime &slotStartUtc,
                                          const QString &phaseLabel)
{
    bool expected = false;
    if (!m_decodeJobActive.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        const qint64 startedMs = m_decodeJobStartedUtcMs.load(std::memory_order_acquire);
        const qint64 ageMs = startedMs > 0
            ? qMax<qint64>(qint64{0}, QDateTime::currentDateTimeUtc().toMSecsSinceEpoch() - startedMs)
            : 0;
        const QString message = currentShortLabel() +
            QStringLiteral(" RX: decoder busy, refusing overlapping %1 pass for slot %2 (active job age %3 ms)")
                .arg(phaseLabel.isEmpty() ? QStringLiteral("boundary") : phaseLabel)
                .arg(formatSlotTime(slotStartUtc, currentSlotMs()))
                .arg(ageMs);
        emit statusChanged(message);
        emit runtimeDiagnostic(message);
        return;
    }

    const int generation = m_decodeGeneration.load(std::memory_order_acquire);
    const QString label = currentShortLabel();
    const QString jobModeName = modeName();
    const int jobSlotMs = currentSlotMs();
    const QString phase = phaseLabel.isEmpty() ? QStringLiteral("boundary") : phaseLabel;
    const QString phaseForLog = phase.startsWith(QStringLiteral("wsjtx-gate"))
        ? QStringLiteral("gate")
        : (phase == QStringLiteral("boundary") ? QStringLiteral("boundary") : phase);
    const qint64 jobStartedUtcMs = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
    m_decodeJobStartedUtcMs.store(jobStartedUtcMs, std::memory_order_release);
    m_decodeJobSlotUtcMs.store(slotStartUtc.toMSecsSinceEpoch(), std::memory_order_release);
    m_runningDecodeGeneration.store(generation, std::memory_order_release);

    emit statusChanged(label + QStringLiteral(" live decode %1: slot %2 queued")
                           .arg(phaseForLog)
                           .arg(formatSlotTime(slotStartUtc, currentSlotMs())));

    const quint64 audioBlocksReceived = m_audioBlocksReceived;
    const quint64 audioBoundarySplits = m_audioBoundarySplits;
    const quint64 audioSequenceGaps = m_audioSequenceGaps;
    const qint64 audioGapSamples = m_audioGapSamples;
    const qint64 audioOverlapSamples = m_audioOverlapSamples;
    const double currentCaptureQueueLatencyMs = m_latestCaptureQueueLatencyMs;
    const double maxCaptureQueueLatencyMs = m_maxCaptureQueueLatencyMs;
    const int initialUtcPadSamples = m_initialUtcPadSamples;
    const quint64 captureGeneration = m_activeCaptureGeneration;
    const quint64 staleCaptureBlocks = m_staleCaptureBlocks;
    const quint64 timestampJumps = m_timestampJumps;
    const quint64 invalidSlotsSkipped = m_invalidSlotsSkipped;

    auto job = [this, samples, slotStartUtc, generation, phase, jobModeName, jobSlotMs,
                audioBlocksReceived, audioBoundarySplits, audioSequenceGaps,
                audioGapSamples, audioOverlapSamples, currentCaptureQueueLatencyMs,
                maxCaptureQueueLatencyMs,
                initialUtcPadSamples, captureGeneration, staleCaptureBlocks,
                timestampJumps, invalidSlotsSkipped]() {
        auto completeJob = [this]() {
            m_decodeJobStartedUtcMs.store(0, std::memory_order_release);
            m_decodeJobSlotUtcMs.store(0, std::memory_order_release);
            m_runningDecodeGeneration.store(0, std::memory_order_release);
            m_decodeJobActive.store(false, std::memory_order_release);
            QMetaObject::invokeMethod(this, [this]() {
                maybeLaunchPendingFinalDecode();
            }, Qt::QueuedConnection);
        };

        try {
            if (!decodeCancellationRequested()) {
                int candidateCount = 0;
                PerfStats stats;
                stats.modeName = jobModeName;
                stats.slotUtc = formatSlotTime(slotStartUtc, jobSlotMs);
                stats.phase = phase;
                stats.offline = m_offlineAnalysisActive.load(std::memory_order_acquire);
                stats.audioBlocksReceived = audioBlocksReceived;
                stats.audioBoundarySplits = audioBoundarySplits;
                stats.audioSequenceGaps = audioSequenceGaps;
                stats.audioGapSamples = audioGapSamples;
                stats.audioOverlapSamples = audioOverlapSamples;
                stats.currentCaptureQueueLatencyMs = currentCaptureQueueLatencyMs;
                stats.maxCaptureQueueLatencyMs = maxCaptureQueueLatencyMs;
                stats.initialUtcPadSamples = initialUtcPadSamples;
                stats.captureGeneration = captureGeneration;
                stats.staleCaptureBlocks = staleCaptureBlocks;
                stats.timestampJumps = timestampJumps;
                stats.invalidSlotsSkipped = invalidSlotsSkipped;
                const QVector<Decode> decodes = decodeSlot(samples, slotStartUtc, &candidateCount, &stats);

                if (!decodeCancellationRequested() &&
                    generation == m_decodeGeneration.load(std::memory_order_acquire)) {
                    int emittedCount = 0;
                    emit decodeBatchStarted(slotStartUtc.toMSecsSinceEpoch(), phase);
                    for (const Decode &decode : decodes) {
                        if (markDecodeEmitted(decode, slotStartUtc)) {
                            ++emittedCount;
                            emit decodeReady(decode);
                        }
                    }
                    emit decodeBatchFinished(slotStartUtc.toMSecsSinceEpoch(), phase);

                    stats.decodeCount = emittedCount;
                    emit performanceUpdated(stats);
                    if (phase.startsWith(QStringLiteral("wsjtx-gate"))) {
                        emit statusChanged(QStringLiteral("%1 live decode gate: slot %2, %3 candidate(s), %4 decode(s), %5 ms")
                                               .arg(jobModeName)
                                               .arg(formatSlotTime(slotStartUtc, jobSlotMs))
                                               .arg(candidateCount)
                                               .arg(emittedCount)
                                               .arg(QString::number(stats.totalMs, 'f', 0)) +
                                           QStringLiteral("; audio qmax %1 ms, gap %2, overlap %3, splits %4, sequence gaps %5")
                                               .arg(QString::number(stats.maxCaptureQueueLatencyMs, 'f', 1))
                                               .arg(stats.audioGapSamples)
                                               .arg(stats.audioOverlapSamples)
                                               .arg(stats.audioBoundarySplits)
                                               .arg(stats.audioSequenceGaps));
                    } else if (phase == QStringLiteral("boundary")) {
                        emit statusChanged(QStringLiteral("%1 live decode boundary: slot %2, %3 candidate(s), added %4 extra decode(s), %5 ms")
                                               .arg(jobModeName)
                                               .arg(formatSlotTime(slotStartUtc, jobSlotMs))
                                               .arg(candidateCount)
                                               .arg(emittedCount)
                                               .arg(QString::number(stats.totalMs, 'f', 0)));
                    } else {
                        emit statusChanged(QStringLiteral("%1 RX %2: %3 candidate(s), %4 decode(s), %5 ms in slot %6")
                                               .arg(jobModeName)
                                               .arg(phase)
                                               .arg(candidateCount)
                                               .arg(emittedCount)
                                               .arg(QString::number(stats.totalMs, 'f', 0))
                                               .arg(formatSlotTime(slotStartUtc, jobSlotMs)));
                    }
                }
            }
        } catch (const std::exception &error) {
            const QString message = QStringLiteral("%1 decoder job failed for slot %2 (%3): %4")
                .arg(jobModeName)
                .arg(formatSlotTime(slotStartUtc, jobSlotMs))
                .arg(phase)
                .arg(QString::fromLocal8Bit(error.what()));
            emit runtimeDiagnostic(message);
            emit statusChanged(message);
        } catch (...) {
            const QString message = QStringLiteral("%1 decoder job failed for slot %2 (%3): unknown exception")
                .arg(jobModeName)
                .arg(formatSlotTime(slotStartUtc, jobSlotMs))
                .arg(phase);
            emit runtimeDiagnostic(message);
            emit statusChanged(message);
        }
        completeJob();
    };

    if (!m_decodeCoordinator || !m_decodeCoordinator->submit(std::move(job))) {
        m_decodeJobStartedUtcMs.store(0, std::memory_order_release);
        m_decodeJobSlotUtcMs.store(0, std::memory_order_release);
        m_runningDecodeGeneration.store(0, std::memory_order_release);
        m_decodeJobActive.store(false, std::memory_order_release);
        const QString message = QStringLiteral("%1 persistent decode coordinator rejected slot %2 (%3)")
            .arg(m_modeName)
            .arg(formatSlotTime(slotStartUtc, currentSlotMs()))
            .arg(phase);
        emit runtimeDiagnostic(message);
        emit statusChanged(message);
    }
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

void Ft8RxDecoder::maybeLaunchPendingFinalDecode()
{
    if (!m_pendingFinalDecode || m_decodeJobActive.load(std::memory_order_acquire) ||
        m_pendingFinalSamples.isEmpty() || !m_pendingFinalSlotStartUtc.isValid()) {
        return;
    }
    const QVector<double> samples = m_pendingFinalSamples;
    const QDateTime slotUtc = m_pendingFinalSlotStartUtc;
    m_pendingFinalSamples.clear();
    m_pendingFinalSlotStartUtc = QDateTime();
    m_pendingFinalDecode = false;
    const QString message = QStringLiteral("%1 launching deferred boundary decode for slot %2")
        .arg(currentShortLabel())
        .arg(formatSlotTime(slotUtc, currentSlotMs()));
    emit runtimeDiagnostic(message);
    startAsyncDecodeSlot(samples, slotUtc, QStringLiteral("boundary"));
}

QVector<Ft8RxDecoder::Decode> Ft8RxDecoder::decodeSlot(const QVector<double> &samples,
                                                       const QDateTime &slotStartUtc,
                                                       int *candidateCount,
                                                       PerfStats *stats)
{
    std::lock_guard<std::recursive_mutex> configLock(m_decodeConfigMutex);
    if (decodeCancellationRequested()) {
        if (candidateCount != nullptr) *candidateCount = 0;
        if (stats != nullptr) stats->earlyStopReason = QStringLiteral("shutdown or capture-generation change requested");
        return {};
    }
    const MadModemRuntime::WorkClass workClass = (stats != nullptr && stats->offline)
        ? MadModemRuntime::WorkClass::FtOffline
        : ((stats != nullptr && stats->phase.startsWith(QStringLiteral("wsjtx-gate")))
            ? MadModemRuntime::WorkClass::FtGate
            : MadModemRuntime::WorkClass::FtBoundary);
    ScopedFtWorkClass scopedWorkClass(workClass);

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
    std::atomic<int> diagBucketRescueCandidates {0};
    std::atomic<int> diagBucketRescueDecodes {0};

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
    int diagSumProductAttempts = 0;
    int diagSumProductRecovered = 0;
    int diagCoherentMetricAttempts = 0;
    int diagCoherentMetricRecovered = 0;

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
                           &diagOsdGf2TotalMs,
                           &diagSumProductAttempts,
                           &diagSumProductRecovered,
                           &diagCoherentMetricAttempts,
                           &diagCoherentMetricRecovered](const CandidateAttemptQuality &quality) {
        if (quality.osdGf2Tried <= 0 && quality.osdGf2BudgetSkips <= 0 &&
            quality.sumProductAttempts <= 0 && quality.coherentMetricAttempts <= 0) {
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
        diagSumProductAttempts += quality.sumProductAttempts;
        diagSumProductRecovered += quality.sumProductRecovered;
        diagCoherentMetricAttempts += quality.coherentMetricAttempts;
        diagCoherentMetricRecovered += quality.coherentMetricRecovered;
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

    const bool gateCandidateSet = stats != nullptr &&
                                  stats->phase.startsWith(QStringLiteral("wsjtx-gate"));
    auto decodeCandidateSet = [this, &slotStartUtc, &betterDecode, &diagAttemptedCandidates, &diagLdpcTried, &diagBucketRescueCandidates, &diagBucketRescueDecodes, &noteRejectReason, &noteQuality, &noteOsdQuality, gateCandidateSet](const QVector<double> &slotSamples,
                                                                                                               const QVector<Candidate> &candidateSet,
                                                                                                               int *workerCountOut) {
        QVector<CandidateDecode> rawPairs;
        if (candidateSet.isEmpty()) {
            if (workerCountOut != nullptr) {
                *workerCountOut = 0;
            }
            return rawPairs;
        }

        for (const Candidate &candidate : candidateSet) {
            if (candidate.bucketRescue) {
                diagBucketRescueCandidates.fetch_add(1, std::memory_order_relaxed);
            }
        }

        const bool offline = m_offlineAnalysisActive.load();
        const int workerCount = FtDecodeWorkerPool::instance().recommendedWorkerCount(t_currentFtWorkClass, candidateSet.size());
        if (workerCountOut != nullptr) {
            *workerCountOut = workerCount;
        }

        /*
         * GF(2) OSD is a tactical near-miss recovery stage. Cap both the
         * number of candidates and cumulative worker time so it cannot consume
         * the boundary budget or starve audio and presentation work.
         */
        const bool classicalDeepRecovery = m_dspPlusDecodeEnabled;
        const int osdGf2TryLimit = classicalDeepRecovery ? (offline ? 42 : 16) : (offline ? 16 : 8);
        const int osdGf2BudgetTenthsMs = classicalDeepRecovery ? (offline ? 1600 : 500) : (offline ? 600 : 250);
        std::atomic<int> osdGf2TriedInSet {0};
        std::atomic<int> osdGf2TenthsMsInSet {0};

        std::mutex rawMutex;
        std::mutex diagMutex;
        std::atomic<int> nextCandidate {0};
        // LDPC and metric retries have variable cost. Dynamic candidate
        // stealing keeps every permitted worker useful without creating threads
        // or statically pinning one expensive cluster to one worker.
        FtDecodeWorkerPool::instance().parallelFor(workerCount, workerCount, [this, &slotSamples, &slotStartUtc, &candidateSet, &rawPairs, &rawMutex, &diagMutex, &nextCandidate, &diagAttemptedCandidates, &diagLdpcTried, &diagBucketRescueDecodes, &noteRejectReason, &noteQuality, &noteOsdQuality, &osdGf2TriedInSet, &osdGf2TenthsMsInSet, osdGf2TryLimit, osdGf2BudgetTenthsMs, gateCandidateSet](int, int) {
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
                if (decodeCancellationRequested()) {
                    break;
                }
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
                // The 90% gate is a latency path, not a deep-recovery pass.
                // Boundary decoding sees the complete frame and retains the
                // expensive metric/OSD recovery.  This follows the practical
                // MSHV rule: do not spend rescue CPU before a complete slot is
                // available.
                const bool osdCandidateBudget = !gateCandidateSet &&
                        osdGf2TriedInSet.load(std::memory_order_relaxed) < osdGf2TryLimit &&
                        osdGf2TenthsMsInSet.load(std::memory_order_relaxed) < osdGf2BudgetTenthsMs;
                const bool osdPermit = osdCandidateBudget &&
                    MadModemRuntime::SystemResourceManager::instance().tryAcquireOsdPermit();
                const bool decoded = decodeCandidate(slotSamples,
                                                     slotStartUtc,
                                                     candidate,
                                                     decode,
                                                     &refinedCandidate,
                                                     &rejectReason,
                                                     &quality,
                                                     !gateCandidateSet,
                                                     osdPermit);
                if (osdPermit) {
                    MadModemRuntime::SystemResourceManager::instance().releaseOsdPermit();
                }
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
                if (quality.osdGf2Tried > 0 || quality.osdGf2BudgetSkips > 0 ||
                    quality.sumProductAttempts > 0 || quality.coherentMetricAttempts > 0) {
                    localOsdQualities.append(quality);
                }
                if (decoded) {
                    if (candidate.bucketRescue) {
                        diagBucketRescueDecodes.fetch_add(1, std::memory_order_relaxed);
                    }
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
    const bool liveGateDecode = !offlineAnalysis &&
                                decodePhase.startsWith(QStringLiteral("wsjtx-gate"));
    const bool liveBoundaryDecode = !offlineAnalysis &&
                                    decodePhase == QStringLiteral("boundary");
    const bool liveRealtimeDecode = liveGateDecode || liveBoundaryDecode;
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
                                    : (m_dspPlusDecodeEnabled ? PassSpec{1.18, 1400, 96}
                                                              : PassSpec{1.30, 900, 64}))
            : (liveBoundaryDecode
                ? PassSpec{1.10, 900, 96}
                : (m_dspPlusDecodeEnabled
                    ? PassSpec{1.16, 260, 0}
                    : (m_deepDecodeEnabled ? PassSpec{1.30, 220, 0} : PassSpec{1.44, 128, 0}))),
        offlineAnalysis
            ? (m_dspPlusDecodeEnabled ? PassSpec{1.02, 1600, 0} : PassSpec{1.18, 950, 0})
            : (liveBoundaryDecode ? PassSpec{0.86, 1000, 64}
                                  : (m_dspPlusDecodeEnabled ? PassSpec{0.92, 620, 0} : PassSpec{1.08, 340, 0})),
        offlineAnalysis
            ? (m_dspPlusDecodeEnabled ? PassSpec{0.94, 900, 0} : PassSpec{1.10, 0, 0})
            : (liveBoundaryDecode ? PassSpec{0.82, 520, 0}
                                  : (m_dspPlusDecodeEnabled ? PassSpec{0.88, 360, 0} : PassSpec{0.99, 0, 0})),
        offlineAnalysis
            ? (m_dspPlusDecodeEnabled ? PassSpec{0.90, 500, 0} : PassSpec{1.06, 0, 0})
            : PassSpec{0.98, 0, 0}
    };
    // v4.13l: live RX is no longer "deep always" and no longer "fast only".
    // The first WSJT-X-gated pass stays as light as v4.13k, then a second
    // targeted pass is allowed only when the slot itself says it is worth it:
    // A) candidate pressure, B) overlap/pile-up, C) active-QSO context,
    // D) useful CQ/value context, E) real time budget.
    const int requestedPasses = offlineAnalysis
        ? (m_dspPlusDecodeEnabled ? 3 : ((m_deepDecodeEnabled) ? 2 : 1))
        : (liveGateDecode ? 1
                          : (liveBoundaryDecode ? (m_dspPlusDecodeEnabled ? 3 : 2) : 1));
    int decodesBeforePass = 0;
    bool liveAdaptiveDeepTriggered = false;
    bool liveAllowResidual = false;
    QStringList liveAdaptiveReasons;
    const double kLiveAdaptiveBudgetMs = liveBoundaryDecode ? 1400.0 : 480.0;
    const double kLiveSecondPassLatestStartMs = liveBoundaryDecode ? 900.0 : 330.0;
    const double kLiveResidualLatestStartMs = liveBoundaryDecode ? 1150.0 : 405.0;
    auto elapsedLiveMs = [&totalStart]() {
        return std::chrono::duration<double, std::milli>(Clock::now() - totalStart).count();
    };

    for (int pass = 0; pass < requestedPasses; ++pass) {
        if (decodeCancellationRequested()) {
            earlyStopReason = QStringLiteral("application shutdown requested");
            break;
        }
        // v3.22: no blind/weak-rescue passes.  WSJT-X/MSHV subtraction
        // passes are decode-driven: if a previous pass produced no CRC-valid
        // signal, there is nothing reference-like to subtract or rescan.
        // This keeps Adaptive/Deep from spending CPU on non-reference guesses
        // and prevents late live decodes from being biased toward speculative
        // candidates.
        if (pass > 0 && decodedSoFar.isEmpty()) {
            earlyStopReason = QStringLiteral("no CRC-valid decode to subtract; later passes skipped");
            break;
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

        if (liveBoundaryDecode && pass == 0) {
            // WSJT-X 3.1 Improved always gives the final full-signal stage its
            // own search/subtraction cycle. Do not make that stage conditional
            // on CQ/QSO heuristics: wideband recovery must stand on its own.
            liveAdaptiveDeepTriggered = true;
            liveAllowResidual = true;
            liveAdaptiveReasons.append(QStringLiteral("F:full-slot-wideband"));
            earlyStopReason = QStringLiteral("live boundary deep: full-slot wideband pass");
        }

        if (liveRealtimeDecode && !liveBoundaryDecode && pass == 0) {
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
            const int subtractCap = liveBoundaryDecode ? 64
                : (liveRealtimeDecode ? 18 : passes[pass].maxSubtract);
            alreadySubtractedFromWorking = strongestList(decodedSoFar, subtractCap);
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
                                                                 offlineAnalysis ? 0.90 : (liveAdaptiveResidual ? 0.96 : 0.92));
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
        const int residualCandidateCap = offlineAnalysis ? 140
            : (liveBoundaryDecode ? 120 : (liveAdaptiveResidual ? 34 : 70));
        const int residualLdpcCap = offlineAnalysis ? 72
            : (liveBoundaryDecode ? 64 : (liveAdaptiveResidual ? 16 : 38));
        const int residualHeavyMetricCap = offlineAnalysis ? 42
            : (liveBoundaryDecode ? 34 : (liveAdaptiveResidual ? 8 : 22));
        const int residualNonProximityCap = offlineAnalysis ? 52
            : (liveBoundaryDecode ? 44 : (liveAdaptiveResidual ? 8 : 22));
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
            const bool residualOsdBudget = (!liveAdaptiveResidual || elapsedLiveMs() < 500.0);
            const bool residualOsdPermit = residualOsdBudget &&
                MadModemRuntime::SystemResourceManager::instance().tryAcquireOsdPermit();
            const bool decoded = decodeCandidate(residual,
                                                 slotStartUtc,
                                                 candidate,
                                                 decode,
                                                 &refinedCandidate,
                                                 &rejectReason,
                                                 &quality,
                                                 allowHeavyMetricRecovery,
                                                 residualOsdPermit);
            if (residualOsdPermit) {
                MadModemRuntime::SystemResourceManager::instance().releaseOsdPermit();
            }
            const auto decodeEnd = Clock::now();
            decodeMs += std::chrono::duration<double, std::milli>(decodeEnd - decodeStart).count();

            if (decoded || rejectReason == DecodeRejectReason::Ldpc ||
                rejectReason == DecodeRejectReason::Crc ||
                rejectReason == DecodeRejectReason::Unpack ||
                rejectReason == DecodeRejectReason::Message) {
                ++diagLdpcTried;
                ++residualLdpcUsed;
            }

            if (quality.osdGf2Tried > 0 || quality.osdGf2BudgetSkips > 0 ||
                quality.sumProductAttempts > 0 || quality.coherentMetricAttempts > 0) {
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

    const double finalTotalMs = std::chrono::duration<double, std::milli>(Clock::now() - totalStart).count();
    MadModemRuntime::SystemResourceManager::instance().observeFtJob(
        workClass,
        finalTotalMs,
        stats != nullptr ? stats->currentCaptureQueueLatencyMs : 0.0,
        firstWorkerCount,
        totalCandidates);
    const MadModemRuntime::RuntimeResourceSnapshot resourceSnapshot =
        MadModemRuntime::SystemResourceManager::instance().snapshot();

    if (stats != nullptr) {
        stats->candidateCount = totalCandidates;
        stats->decodeCount = out.size();
        stats->workerCount = firstWorkerCount;
        stats->candidateSearchMs = searchMs;
        stats->candidateDecodeMs = decodeMs;
        stats->totalMs = finalTotalMs;
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
        stats->sumProductAttempts = diagSumProductAttempts;
        stats->sumProductDecodes = diagSumProductRecovered;
        stats->coherentMetricAttempts = diagCoherentMetricAttempts;
        stats->coherentMetricDecodes = diagCoherentMetricRecovered;
        stats->bucketRescueCandidates = diagBucketRescueCandidates.load(std::memory_order_relaxed);
        stats->bucketRescueDecodes = diagBucketRescueDecodes.load(std::memory_order_relaxed);
        stats->physicalCores = resourceSnapshot.topology.physicalCores;
        stats->logicalProcessors = resourceSnapshot.topology.logicalProcessors;
        stats->poolCapacity = resourceSnapshot.poolCapacity;
        stats->liveWorkerTarget = resourceSnapshot.liveWorkerTarget;
        stats->gateWorkerTarget = resourceSnapshot.gateWorkerTarget;
        stats->boundaryWorkerTarget = resourceSnapshot.boundaryWorkerTarget;
        stats->osdWorkerTarget = resourceSnapshot.osdWorkerTarget;
        stats->guiFrameMs = resourceSnapshot.guiFrameMs;
        stats->waterfallFrameMs = resourceSnapshot.waterfallFrameMs;
        stats->waterfallQueueRows = resourceSnapshot.waterfallQueueRows;
        stats->waterfallGpuBacked = resourceSnapshot.waterfallGpuBacked;
        stats->systemCpuLoadPercent = resourceSnapshot.systemCpuLoadPercent;
        stats->simdBackend = MadModemCpu::ft8ToneEngineName();
        stats->resourceAdjustment = resourceSnapshot.lastAdjustment;
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

    // Persistent scalable FT worker pool. Split the start-time grid into more
    // tasks than workers so large/uneven grids remain balanced on 16/24+ core CPUs.
    const int workerCount = FtDecodeWorkerPool::instance().recommendedWorkerCount(t_currentFtWorkClass, startCount);
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
            // One physical FT8 signal creates a DT/DF cloud on the 40 ms /
            // 3.125 Hz search grid.  Keep the strongest local maximum instead
            // of sending adjacent replicas to LDPC.  The frequency radius is
            // well below one FT8 tone spacing, so genuinely distinct signals
            // remain separate.
            if (std::abs(kept.baseHz - c.baseHz) <= 6.5 &&
                std::abs(kept.startSec - c.startSec) <= 0.12) {
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
    if (decodeCancellationRequested()) {
        return reject(DecodeRejectReason::Boundary);
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
    // Deep Max uses the classical weak-signal recovery envelope that was
    // previously hidden behind the removed MIND switch. No neural score is
    // involved: entry depends only on Costas agreement and demodulator LLR.
    const bool classicalDeepRecovery = allowMetricRecovery && m_dspPlusDecodeEnabled;
    const int hardSyncBailout = classicalDeepRecovery ? 5 : 6;
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
    const bool ldpcGhostCandidate = classicalDeepRecovery
            ? ((hardSyncCount <= 7 && meanAbsLlr < 2.20) ||
               (hardSyncCount == 8 && meanAbsLlr < 1.85))
            : ((hardSyncCount <= 8 && meanAbsLlr < 3.25) ||
               (hardSyncCount == 9 && meanAbsLlr < 2.65));
    if (ldpcGhostCandidate) {
        return reject(DecodeRejectReason::SoftMetric);
    }

    const bool classicalWeakPlausible = classicalDeepRecovery &&
            (((hardSyncCount >= 8 && hardSyncCount <= 11) && meanAbsLlr >= 2.75) ||
             (hardSyncCount >= 9 && meanAbsLlr >= 2.35));

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
        bool decodedViaSumProduct = false;
        int gf2OsdHitOrder = -1;
        bool ldpcSuccess = ldpcDecode174_91(candidateLlr,
                                               candidateBits,
                                               candidateIterations,
                                               &posterior);
        // The first live deployment attempted SPA on hundreds of candidates
        // but recovered none, while consuming the boundary budget needed by
        // the proven min-sum/OSD/residual pipeline. Keep it available for
        // controlled offline A/B only until same-WAV evidence justifies live use.
        const bool sumProductAllowed = m_offlineAnalysisActive.load();
        if (!ldpcSuccess && sumProductAllowed) {
            if (qualityOut != nullptr) {
                ++qualityOut->sumProductAttempts;
            }
            std::array<double, 174> sumProductPosterior{};
            if (ldpcDecode174_91SumProduct(candidateLlr,
                                           candidateBits,
                                           candidateIterations,
                                           &sumProductPosterior)) {
                posterior = sumProductPosterior;
                ldpcSuccess = true;
                decodedViaSumProduct = true;
            }
        }
        if (!ldpcSuccess) {
            /*
             * 0.5.1 GF(2) OSD pivot-completion lab: use a true systematic
             * OSD fallback only after BP/min-sum failed, and only for
             * candidates that are plausible enough to justify the extra
             * algebra.  The normal LDPC path and the v4.10 OSD-lite path
             * remain intact.
             */
            const bool gf2BaseCandidate = hardSyncCount >= 10 && meanAbsLlr >= 3.8;
            const bool gf2ClassicalWeakCandidate = classicalWeakPlausible &&
                                                   hardSyncCount >= 8 &&
                                                   meanAbsLlr >= 2.75;
            const bool gf2OsdCandidate = allowOsdLite && allowMetricRecovery &&
                                         m_dspPlusDecodeEnabled &&
                                         (gf2BaseCandidate || gf2ClassicalWeakCandidate);
            const bool tryGf2Osd = gf2OsdCandidate && allowGf2Osd;
            if (gf2OsdCandidate && !allowGf2Osd && qualityOut != nullptr) {
                ++qualityOut->osdGf2BudgetSkips;
            }
            bool osdSuccess = false;
            if (tryGf2Osd && decodeCancellationRequested()) {
                return reject(DecodeRejectReason::Boundary);
            }
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
                const bool osdLiteClassicalWeakAllowed = classicalWeakPlausible &&
                                                         hardSyncCount >= 9 &&
                                                         meanAbsLlr >= 3.15;
                const bool osdLiteAllowed = allowOsdLite && allowMetricRecovery &&
                                            m_dspPlusDecodeEnabled &&
                                            (osdLiteBaseAllowed || osdLiteClassicalWeakAllowed);
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

        if (decodedViaSumProduct && qualityOut != nullptr) {
            ++qualityOut->sumProductRecovered;
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
            classicalWeakPlausible ||
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
    // Deep boundary-only coherent path.  The candidate is mixed and
    // decimated to 200 Hz (32 complex samples/symbol), preserving phase before
    // the 1/2/3-symbol metrics are formed.  This is never run by the early gate
    // and does not alter the ghost-candidate policy above.
    if (!metricDecoded && allowFt8bMetricRecovery &&
        m_offlineAnalysisActive.load() &&
        t_currentFtWorkClass != MadModemRuntime::WorkClass::FtGate) {
        if (qualityOut != nullptr) {
            ++qualityOut->coherentMetricAttempts;
        }
        static const std::vector<double> coherentTaps =
            makeWindowedSincLowpass(241, 70.0, 12000.0);
        constexpr int kDecimation = 60;
        constexpr int kBasebandSamplesPerSymbol = 32;
        constexpr int kMargin = 8;
        std::vector<std::complex<double>> baseband;
        if (extractComplexBaseband(samples,
                                   startSample - kMargin * kDecimation,
                                   kSymbols * kBasebandSamplesPerSymbol + 2 * kMargin,
                                   kDecimation,
                                   baseHz,
                                   coherentTaps,
                                   baseband)) {
            auto correlationAt = [&baseband](int symbol, int tone, int shift) {
                std::complex<double> sum(0.0, 0.0);
                const int begin = kMargin + shift + symbol * kBasebandSamplesPerSymbol;
                for (int n = 0; n < kBasebandSamplesPerSymbol; ++n) {
                    const int index = begin + n;
                    if (index < 0 || index >= static_cast<int>(baseband.size())) {
                        continue;
                    }
                    const double phase = -kTwoPi * static_cast<double>(tone * n) /
                                         static_cast<double>(kBasebandSamplesPerSymbol);
                    sum += baseband[static_cast<size_t>(index)] *
                           std::complex<double>(std::cos(phase), std::sin(phase));
                }
                return sum;
            };

            int coherentShift = 0;
            double coherentSync = -1.0;
            for (int shift = -4; shift <= 4; ++shift) {
                double score = 0.0;
                int costasIndex = 0;
                for (int blockIndex = 0; blockIndex < 3; ++blockIndex) {
                    const int syncStart = kCostasStarts[blockIndex];
                    for (int i = 0; i < 7; ++i) {
                        score += std::abs(correlationAt(syncStart + i, kCostas[i], shift));
                        ++costasIndex;
                    }
                }
                Q_UNUSED(costasIndex)
                if (score > coherentSync) {
                    coherentSync = score;
                    coherentShift = shift;
                }
            }

            // Preserve the complete 79-symbol correlation grid.  The WSJT-X
            // coherent families are built over 1, 2 and 3 symbols; the final
            // group in each 29-symbol data half intentionally reaches into the
            // following Costas block, while only the data-bit positions are
            // retained.
            std::array<std::array<std::complex<double>, 8>, kSymbols> symbolCorrelation{};
            for (int sym = 0; sym < kSymbols; ++sym) {
                for (int tone = 0; tone < 8; ++tone) {
                    const int grayIndex = grayInverse(tone);
                    symbolCorrelation[static_cast<size_t>(sym)]
                                     [static_cast<size_t>(grayIndex)] =
                        correlationAt(sym, tone, coherentShift);
                }
            }

            auto coherentMetricRaw = [&](int groupSize,
                                         std::array<double, 174> *ratioOut) {
                std::array<double, 174> metric{};
                metric.fill(0.0);
                if (ratioOut != nullptr) {
                    ratioOut->fill(0.0);
                }
                const int comboCount = 1 << (3 * groupSize);
                for (int half = 0; half < 2; ++half) {
                    const int symbolBase = half == 0 ? 7 : 43;
                    const int bitBase = half * 87;
                    const int bitEnd = bitBase + 87;
                    for (int localStart = 0; localStart < 29; localStart += groupSize) {
                        std::vector<double> scores(static_cast<size_t>(comboCount), 0.0);
                        for (int combo = 0; combo < comboCount; ++combo) {
                            std::complex<double> coherentSum(0.0, 0.0);
                            for (int g = 0; g < groupSize; ++g) {
                                const int shift = 3 * (groupSize - 1 - g);
                                const int symbolIndex = (combo >> shift) & 0x7;
                                const int symbol = symbolBase + localStart + g;
                                coherentSum += symbolCorrelation[static_cast<size_t>(symbol)]
                                                                [static_cast<size_t>(symbolIndex)];
                            }
                            scores[static_cast<size_t>(combo)] = std::abs(coherentSum);
                        }
                        for (int bit = 0; bit < 3 * groupSize; ++bit) {
                            const int globalBit = bitBase + localStart * 3 + bit;
                            if (globalBit >= bitEnd || globalBit >= 174) {
                                continue;
                            }
                            const int mask = 1 << (3 * groupSize - 1 - bit);
                            double best0 = -1.0e99;
                            double best1 = -1.0e99;
                            for (int combo = 0; combo < comboCount; ++combo) {
                                if ((combo & mask) != 0) {
                                    best1 = std::max(best1, scores[static_cast<size_t>(combo)]);
                                } else {
                                    best0 = std::max(best0, scores[static_cast<size_t>(combo)]);
                                }
                            }
                            const double value = best0 - best1;
                            metric[static_cast<size_t>(globalBit)] = value;
                            if (ratioOut != nullptr) {
                                const double denominator = std::max(best0, best1);
                                (*ratioOut)[static_cast<size_t>(globalBit)] =
                                    denominator > 0.0 ? value / denominator : 0.0;
                            }
                        }
                    }
                }
                return metric;
            };

            std::array<double, 174> ratioMetric{};
            std::array<double, 174> metric1 = coherentMetricRaw(1, &ratioMetric);
            std::array<double, 174> metric2 = coherentMetricRaw(2, nullptr);
            std::array<double, 174> metric3 = coherentMetricRaw(3, nullptr);
            std::array<double, 174> cherry{};
            for (int i = 0; i < 174; ++i) {
                double value = metric1[static_cast<size_t>(i)];
                if (std::abs(metric2[static_cast<size_t>(i)]) > std::abs(value)) {
                    value = metric2[static_cast<size_t>(i)];
                }
                if (std::abs(metric3[static_cast<size_t>(i)]) > std::abs(value)) {
                    value = metric3[static_cast<size_t>(i)];
                }
                cherry[static_cast<size_t>(i)] = value;
            }
            normalizeSoftMetric(metric1);
            normalizeSoftMetric(metric2);
            normalizeSoftMetric(metric3);
            normalizeSoftMetric(ratioMetric);
            normalizeSoftMetric(cherry);

            metricDecoded = tryDecodeMetric(cherry, true);
            if (!metricDecoded) metricDecoded = tryDecodeMetric(metric1, false);
            if (!metricDecoded) metricDecoded = tryDecodeMetric(metric2, false);
            if (!metricDecoded) metricDecoded = tryDecodeMetric(metric3, false);
            if (!metricDecoded) metricDecoded = tryDecodeMetric(ratioMetric, false);
            if (metricDecoded && qualityOut != nullptr) {
                ++qualityOut->coherentMetricRecovered;
            }
        }
    }

    if (!metricDecoded) {
        return reject(bestFailure);
    }

    decodeOut.utc = slotStartUtc.time().toString(QStringLiteral("HHmmss"));
    decodeOut.slotStartUtcMs = slotStartUtc.toMSecsSinceEpoch();
    // decodeSlot() keeps m_decodeConfigMutex for the whole pass, therefore the
    // mode is immutable while candidate workers run.  Do not call the locking
    // currentSlotMs() accessor from this pool thread: the coordinator owns the
    // recursive mutex while waiting for the pool and that would deadlock.
    decodeOut.slotPeriodMs = Ft8Mode::profileForMode(m_modeName).slotMs;
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
    const int nominalStartSample = qRound(candidate.startSec * kDecodeSampleRate);
    if (nominalStartSample < -kSamplesPerSymbol || nominalStartSample >= samples.size()) {
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

    int startSample = nominalStartSample;
    // Keep the established live SIC timing. The post-decode timing search is
    // retained for offline A/B, where it cannot damage a realtime residual
    // pass before its benefit has been demonstrated on identical WAV input.
    if (m_offlineAnalysisActive.load()) {
        double bestCorrelation = -1.0;
        for (int offset : {-120, -60, 0, 60, 120}) {
            std::complex<double> correlation(0.0, 0.0);
            double referencePower = 0.0;
            int used = 0;
            const int trialStart = nominalStartSample + offset;
            for (int i = 0; i < kFt8SubNFrame; i += 8) {
                const int sampleIndex = trialStart + i;
                if (sampleIndex < 0 || sampleIndex >= samples.size()) {
                    continue;
                }
                correlation += samples.at(sampleIndex) * std::conj(cref[static_cast<size_t>(i)]);
                referencePower += std::norm(cref[static_cast<size_t>(i)]);
                ++used;
            }
            const double score = (used > 0 && referencePower > 0.0)
                ? std::norm(correlation) / referencePower
                : 0.0;
            if (score > bestCorrelation) {
                bestCorrelation = score;
                startSample = trialStart;
            }
        }
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
        std::array<int, 103> decodedTones{};
        bool hasDecodedTones = false;
    };

    auto betterDecode = [](const Decode &a, const Decode &b) {
        if (a.snrDb != b.snrDb) {
            return a.snrDb > b.snrDb;
        }
        return a.syncScore > b.syncScore;
    };

    int diagAttemptedCandidates = 0;
    int diagLdpcTried = 0;
    int diagSyncGateRejects = 0;
    int diagLdpcFailures = 0;
    int diagCrcFailures = 0;
    int diagMessageRejects = 0;
    int diagSumProductAttempts = 0;
    int diagSumProductRecovered = 0;
    int diagCoherentMetricAttempts = 0;
    int diagCoherentMetricRecovered = 0;
    const bool gateCandidateSet = stats != nullptr &&
                                  stats->phase.startsWith(QStringLiteral("wsjtx-gate"));

    auto decodeCandidateSet = [this, &slotStartUtc,
                               &diagAttemptedCandidates, &diagLdpcTried,
                               &diagSyncGateRejects, &diagLdpcFailures,
                               &diagCrcFailures, &diagMessageRejects,
                               &diagSumProductAttempts, &diagSumProductRecovered,
                               &diagCoherentMetricAttempts, &diagCoherentMetricRecovered](const QVector<double> &slotSamples,
                                                                     const QVector<Candidate> &candidateSet,
                                                                     int *workerCountOut) {
        QVector<CandidateDecode> rawPairs;
        if (candidateSet.isEmpty()) {
            if (workerCountOut != nullptr) {
                *workerCountOut = 0;
            }
            return rawPairs;
        }

        const int workerCount = FtDecodeWorkerPool::instance().recommendedWorkerCount(t_currentFtWorkClass, candidateSet.size());
        if (workerCountOut != nullptr) {
            *workerCountOut = workerCount;
        }

        std::mutex rawMutex;
        std::mutex diagMutex;
        std::atomic<int> nextCandidate {0};

        // FT4 candidate cost varies substantially after the sync gate.  Use
        // the same atomic work stealing as FT8 so one hard LDPC cluster cannot
        // leave the other persistent workers idle.
        FtDecodeWorkerPool::instance().parallelFor(workerCount, workerCount,
            [this, &slotSamples, &slotStartUtc, &candidateSet, &rawPairs,
             &rawMutex, &diagMutex, &nextCandidate,
             &diagAttemptedCandidates, &diagLdpcTried,
             &diagSyncGateRejects, &diagLdpcFailures,
             &diagCrcFailures, &diagMessageRejects,
             &diagSumProductAttempts, &diagSumProductRecovered,
             &diagCoherentMetricAttempts, &diagCoherentMetricRecovered](int, int) {
            QVector<CandidateDecode> localPairs;
            localPairs.reserve(8);
            int localAttempted = 0;
            int localLdpcTried = 0;
            int localSyncGateRejects = 0;
            int localLdpcFailures = 0;
            int localCrcFailures = 0;
            int localMessageRejects = 0;
            int localSumProductAttempts = 0;
            int localSumProductRecovered = 0;
            int localCoherentMetricAttempts = 0;
            int localCoherentMetricRecovered = 0;

            for (;;) {
                if (decodeCancellationRequested()) {
                    break;
                }
                const int i = nextCandidate.fetch_add(1, std::memory_order_relaxed);
                if (i >= candidateSet.size()) {
                    break;
                }

                ++localAttempted;
                Decode decode;
                const Candidate candidate = candidateSet.at(i);
                CandidateAttemptQuality quality;
                DecodeRejectReason rejectReason = DecodeRejectReason::None;
                std::array<int, 103> decodedTones{};
                const bool decoded = decodeFt4Candidate(slotSamples,
                                                        slotStartUtc,
                                                        candidate,
                                                        decode,
                                                        &quality,
                                                        &rejectReason,
                                                        &decodedTones);
                localSumProductAttempts += quality.sumProductAttempts;
                localSumProductRecovered += quality.sumProductRecovered;
                localCoherentMetricAttempts += quality.coherentMetricAttempts;
                localCoherentMetricRecovered += quality.coherentMetricRecovered;
                if (decoded || rejectReason == DecodeRejectReason::Ldpc ||
                    rejectReason == DecodeRejectReason::Crc ||
                    rejectReason == DecodeRejectReason::Message ||
                    rejectReason == DecodeRejectReason::Unpack) {
                    ++localLdpcTried;
                }
                switch (rejectReason) {
                case DecodeRejectReason::SyncGate:
                    ++localSyncGateRejects;
                    break;
                case DecodeRejectReason::Ldpc:
                    ++localLdpcFailures;
                    break;
                case DecodeRejectReason::Crc:
                    ++localCrcFailures;
                    break;
                case DecodeRejectReason::Message:
                case DecodeRejectReason::Unpack:
                    ++localMessageRejects;
                    break;
                default:
                    break;
                }

                if (decoded) {
                    CandidateDecode pair;
                    pair.candidate = candidate;
                    pair.candidate.startSec = decode.dt + 0.5;
                    pair.candidate.baseHz = static_cast<double>(decode.frequencyHz);
                    pair.candidate.refined = true;
                    pair.decode = decode;
                    pair.decodedTones = decodedTones;
                    pair.hasDecodedTones = true;
                    localPairs.append(pair);
                }
            }

            {
                std::lock_guard<std::mutex> lock(diagMutex);
                diagAttemptedCandidates += localAttempted;
                diagLdpcTried += localLdpcTried;
                diagSyncGateRejects += localSyncGateRejects;
                diagLdpcFailures += localLdpcFailures;
                diagCrcFailures += localCrcFailures;
                diagMessageRejects += localMessageRejects;
                diagSumProductAttempts += localSumProductAttempts;
                diagSumProductRecovered += localSumProductRecovered;
                diagCoherentMetricAttempts += localCoherentMetricAttempts;
                diagCoherentMetricRecovered += localCoherentMetricRecovered;
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
    int passCount = 1;
    QString earlyStopReason;
    double candidateSearchMs = 0.0;
    double candidateDecodeMs = 0.0;

    const auto searchStart = Clock::now();
    const QVector<Candidate> candidates = findFt4Candidates(samples, 0.0);
    const auto searchEnd = Clock::now();
    candidateSearchMs += std::chrono::duration<double, std::milli>(searchEnd - searchStart).count();

    const auto decodeStart = Clock::now();
    QVector<CandidateDecode> rawPairs = decodeCandidateSet(samples, candidates, &workerCount);
    QVector<CandidateDecode> deduped = deduplicate(rawPairs, &dropped);
    const auto decodeEnd = Clock::now();
    candidateDecodeMs += std::chrono::duration<double, std::milli>(decodeEnd - decodeStart).count();

    // The wideband FT4 gate is deliberately small and fast.  During an active
    // QSO it gets one additional, explicit deadline pass around the selected
    // correspondent frequency.  This pass runs before the opposite TX boundary
    // and replaces reliance on the full boundary decode, whose results can only
    // arrive after PTT/audio have already started.
    const QString deadlineCall = m_dxCall.trimmed().toUpper();
    const bool qsoDeadlinePassEnabled = gateCandidateSet &&
                                        m_qsoDeadlineActive.load(std::memory_order_acquire) &&
                                        !deadlineCall.isEmpty();
    auto messageMentionsCall = [](const QString &message, const QString &call) {
        if (call.isEmpty()) {
            return false;
        }
        const QString upper = message.trimmed().toUpper();
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

    bool deadlineTargetDecoded = false;
    if (qsoDeadlinePassEnabled) {
        for (const CandidateDecode &pair : deduped) {
            if (messageMentionsCall(pair.decode.message, deadlineCall)) {
                deadlineTargetDecoded = true;
                break;
            }
        }
    }

    constexpr double kFt4DeadlinePassLatestStartMs = 260.0;
    constexpr double kFt4DeadlinePassBudgetMs = 520.0;
    if (qsoDeadlinePassEnabled && !deadlineTargetDecoded) {
        const double elapsedBeforeDeadlinePassMs =
            std::chrono::duration<double, std::milli>(Clock::now() - totalStart).count();
        if (elapsedBeforeDeadlinePassMs <= kFt4DeadlinePassLatestStartMs) {
            const auto focusedSearchStart = Clock::now();
            const QVector<Candidate> focusedAll = findFt4DeadlineCandidates(samples,
                                                                            m_rxMarkerHz,
                                                                            180,
                                                                            220);
            QVector<Candidate> focusedCandidates;
            focusedCandidates.reserve(focusedAll.size());
            constexpr double kFt4ToneSpacingHz = 12000.0 / 576.0;
            for (const Candidate &candidate : focusedAll) {
                bool alreadyAttempted = false;
                for (const Candidate &wideCandidate : candidates) {
                    if (std::abs(wideCandidate.baseHz - candidate.baseHz) <= kFt4ToneSpacingHz &&
                        std::abs(wideCandidate.startSec - candidate.startSec) <= 0.10) {
                        alreadyAttempted = true;
                        break;
                    }
                }
                if (!alreadyAttempted) {
                    focusedCandidates.append(candidate);
                }
            }
            const auto focusedSearchEnd = Clock::now();
            candidateSearchMs += std::chrono::duration<double, std::milli>(focusedSearchEnd - focusedSearchStart).count();
            secondPassCandidates = focusedCandidates.size();

            if (!focusedCandidates.isEmpty()) {
                int focusedWorkerCount = 0;
                const auto focusedDecodeStart = Clock::now();
                const QVector<CandidateDecode> focusedRaw = decodeCandidateSet(samples,
                                                                                focusedCandidates,
                                                                                &focusedWorkerCount);
                const auto focusedDecodeEnd = Clock::now();
                candidateDecodeMs += std::chrono::duration<double, std::milli>(focusedDecodeEnd - focusedDecodeStart).count();
                workerCount = qMax(workerCount, focusedWorkerCount);
                for (const CandidateDecode &pair : focusedRaw) {
                    rawPairs.append(pair);
                }
                deduped = deduplicate(rawPairs, &dropped);
                passCount = 2;
            }

            const double elapsedAfterDeadlinePassMs =
                std::chrono::duration<double, std::milli>(Clock::now() - totalStart).count();
            earlyStopReason = elapsedAfterDeadlinePassMs > kFt4DeadlinePassBudgetMs
                ? QStringLiteral("FT4 QSO deadline pass exceeded %1 ms budget").arg(kFt4DeadlinePassBudgetMs, 0, 'f', 0)
                : QStringLiteral("FT4 QSO deadline pass around %1 Hz for %2")
                      .arg(m_rxMarkerHz)
                      .arg(deadlineCall);
        } else {
            earlyStopReason = QStringLiteral("FT4 QSO deadline pass skipped: wideband gate already used %1 ms")
                .arg(elapsedBeforeDeadlinePassMs, 0, 'f', 0);
        }
    }

    double subtractionMs = 0.0;
    const bool liveBoundaryDecode = stats != nullptr && stats->phase == QStringLiteral("boundary");
    // Match the efficient FT8 live policy: the early gate is strictly one
    // pass, while the complete boundary may use decode-driven subtraction and
    // at most two rescans.  The old FT4 path blindly ran four full 120-candidate
    // passes at both phases (480 attempts), which consumed 1.4-1.9 seconds even
    // before the boundary result was useful to the sequencer.
    const int requestedPasses = gateCandidateSet
        ? 1
        : (liveBoundaryDecode
            ? (m_dspPlusDecodeEnabled ? 3 : (enhancedEngine ? 2 : 1))
            : (m_dspPlusDecodeEnabled ? 3 : (enhancedEngine ? 2 : 1)));
    const double livePassBudgetMs = gateCandidateSet ? 650.0 : 950.0;
    QVector<double> working = samples;
    for (int pass = 1; pass < requestedPasses && !deduped.isEmpty(); ++pass) {
        const double elapsedMs = std::chrono::duration<double, std::milli>(Clock::now() - totalStart).count();
        if ((gateCandidateSet || liveBoundaryDecode) && elapsedMs >= livePassBudgetMs) {
            earlyStopReason = QStringLiteral("FT4 live adaptive budget exhausted before pass %1")
                .arg(pass + 1);
            break;
        }
        const auto subtractionStart = Clock::now();
        QVector<CandidateDecode> sorted = deduped;
        std::sort(sorted.begin(), sorted.end(), [&betterDecode](const CandidateDecode &a, const CandidateDecode &b) {
            return betterDecode(a.decode, b.decode);
        });
        const int subtractLimit = qMin(m_dspPlusDecodeEnabled ? 8 : 5, sorted.size());
        for (int i = 0; i < subtractLimit; ++i) {
            if (sorted.at(i).hasDecodedTones) {
                subtractFt4DecodedSignal(working,
                                         sorted.at(i).candidate,
                                         sorted.at(i).decodedTones);
            }
        }
        const auto subtractionEnd = Clock::now();
        subtractionMs += std::chrono::duration<double, std::milli>(subtractionEnd - subtractionStart).count();

        const auto rescanSearchStart = Clock::now();
        const QVector<Candidate> nextCandidates = findFt4Candidates(working, 0.0);
        const auto rescanSearchEnd = Clock::now();
        candidateSearchMs += std::chrono::duration<double, std::milli>(rescanSearchEnd - rescanSearchStart).count();
        secondPassCandidates += nextCandidates.size();
        const auto rescanDecodeStart = Clock::now();
        QVector<CandidateDecode> nextRaw = decodeCandidateSet(working, nextCandidates, nullptr);
        const auto rescanDecodeEnd = Clock::now();
        candidateDecodeMs += std::chrono::duration<double, std::milli>(rescanDecodeEnd - rescanDecodeStart).count();
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
    const double finalTotalMs = std::chrono::duration<double, std::milli>(totalEnd - totalStart).count();
    const MadModemRuntime::WorkClass workClass = t_currentFtWorkClass;
    MadModemRuntime::SystemResourceManager::instance().observeFtJob(
        workClass,
        finalTotalMs,
        stats != nullptr ? stats->currentCaptureQueueLatencyMs : 0.0,
        workerCount,
        candidates.size() + secondPassCandidates);
    const MadModemRuntime::RuntimeResourceSnapshot resourceSnapshot =
        MadModemRuntime::SystemResourceManager::instance().snapshot();
    if (candidateCount != nullptr) {
        *candidateCount = candidates.size() + secondPassCandidates;
    }
    if (stats != nullptr) {
        stats->candidateCount = candidates.size() + secondPassCandidates;
        stats->decodeCount = out.size();
        stats->workerCount = workerCount;
        stats->candidateSearchMs = candidateSearchMs;
        stats->candidateDecodeMs = candidateDecodeMs;
        stats->subtractionMs = subtractionMs;
        stats->passCount = passCount;
        stats->secondPassCandidates = secondPassCandidates;
        stats->dedupDropped = dropped;
        stats->totalMs = finalTotalMs;
        stats->attemptedCandidates = diagAttemptedCandidates;
        stats->ldpcTried = diagLdpcTried;
        stats->ldpcFailures = diagLdpcFailures;
        stats->crcFailures = diagCrcFailures;
        stats->messageRejects = diagMessageRejects;
        stats->syncGateRejects = diagSyncGateRejects;
        stats->sumProductAttempts = diagSumProductAttempts;
        stats->sumProductDecodes = diagSumProductRecovered;
        stats->coherentMetricAttempts = diagCoherentMetricAttempts;
        stats->coherentMetricDecodes = diagCoherentMetricRecovered;
        stats->earlyStopReason = earlyStopReason;
        stats->physicalCores = resourceSnapshot.topology.physicalCores;
        stats->logicalProcessors = resourceSnapshot.topology.logicalProcessors;
        stats->poolCapacity = resourceSnapshot.poolCapacity;
        stats->liveWorkerTarget = resourceSnapshot.liveWorkerTarget;
        stats->gateWorkerTarget = resourceSnapshot.gateWorkerTarget;
        stats->boundaryWorkerTarget = resourceSnapshot.boundaryWorkerTarget;
        stats->osdWorkerTarget = resourceSnapshot.osdWorkerTarget;
        stats->guiFrameMs = resourceSnapshot.guiFrameMs;
        stats->waterfallFrameMs = resourceSnapshot.waterfallFrameMs;
        stats->waterfallQueueRows = resourceSnapshot.waterfallQueueRows;
        stats->waterfallGpuBacked = resourceSnapshot.waterfallGpuBacked;
        stats->systemCpuLoadPercent = resourceSnapshot.systemCpuLoadPercent;
        stats->simdBackend = MadModemCpu::ft8ToneEngineName();
        stats->resourceAdjustment = resourceSnapshot.lastAdjustment;
        stats->engineName = QStringLiteral("FT4 adaptive atomic live engine");
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

    const int workerCount = FtDecodeWorkerPool::instance().recommendedWorkerCount(t_currentFtWorkClass, qMax(1, freqCount));

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


QVector<Ft8RxDecoder::Candidate> Ft8RxDecoder::findFt4DeadlineCandidates(const QVector<double> &samples,
                                                                          int centerHz,
                                                                          int halfSpanHz,
                                                                          int maxCandidates) const
{
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
    const int boundedCenterHz = qBound(150, centerHz, 2930);
    const int boundedHalfSpanHz = qBound(40, halfSpanHz, 600);
    const double low = qMax(qMax(150.0, static_cast<double>(m_searchLowHz)),
                            static_cast<double>(boundedCenterHz - boundedHalfSpanHz));
    const double high = qMin(qMin(static_cast<double>(m_searchHighHz),
                                  3000.0 - 3.0 * kFt4ToneSpacingHz),
                             static_cast<double>(boundedCenterHz + boundedHalfSpanHz));
    const int freqCount = high >= low
        ? qMax(0, static_cast<int>(std::floor((high - low) / freqStep)) + 1)
        : 0;

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

    const int workerCount = FtDecodeWorkerPool::instance().recommendedWorkerCount(t_currentFtWorkClass, qMax(1, freqCount));

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

    // This is a separate, selected-QSO deadline lane.  The validated
    // whole-passband FT4 finder above remains byte-for-byte unchanged.
    const int kMaxCandidates = qBound(1, maxCandidates, 512);
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
                                      DecodeRejectReason *rejectReasonOut,
                                      std::array<int, 103> *decodedTonesOut)
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
    if (decodedTonesOut != nullptr) {
        decodedTonesOut->fill(0);
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
    bool sawCrcFailure = false;
    bool finalViaSumProduct = false;
    bool finalViaCoherentMetric = false;

    auto tryFt4Metric = [&](const std::array<double, 174> &metric,
                            bool countCoherent) {
        std::array<int, 174> trialBits{};
        std::array<double, 174> posterior{};
        int trialIterations = 0;
        bool ok = ldpcDecode174_91(metric, trialBits, trialIterations, &posterior);
        if (ok && crc14Ok(trialBits)) {
            bits = trialBits;
            iterations = trialIterations;
            finalViaSumProduct = false;
            finalViaCoherentMetric = countCoherent;
            return true;
        }
        if (ok) {
            sawCrcFailure = true;
        }

        // Keep the FT4 SPA comparison offline until it demonstrates real
        // CRC-valid recoveries on identical WAV input. The first live test
        // showed added work but no measured recovery contribution.
        const bool sumProductAllowed = m_offlineAnalysisActive.load();
        if (!sumProductAllowed) {
            return false;
        }
        if (qualityOut != nullptr) {
            ++qualityOut->sumProductAttempts;
        }
        ok = ldpcDecode174_91SumProduct(metric, trialBits, trialIterations, &posterior);
        if (ok && crc14Ok(trialBits)) {
            bits = trialBits;
            iterations = trialIterations;
            finalViaSumProduct = true;
            finalViaCoherentMetric = countCoherent;
            return true;
        }
        if (ok) {
            sawCrcFailure = true;
        }
        return false;
    };

    bool fecDecoded = tryFt4Metric(llr, false);

    // Boundary/offline deep path: isolate the candidate as a complex 666.67 Hz
    // baseband (32 complex samples/symbol) and form coherent 1/2/4-symbol
    // metrics.  The early gate keeps the proven inexpensive metric only.
    if (!fecDecoded && m_offlineAnalysisActive.load() &&
        t_currentFtWorkClass != MadModemRuntime::WorkClass::FtGate) {
        if (qualityOut != nullptr) {
            ++qualityOut->coherentMetricAttempts;
        }
        static const std::vector<double> coherentTaps =
            makeWindowedSincLowpass(161, 96.0, 12000.0);
        constexpr int kDecimation = 18;
        constexpr int kSamplesPerBasebandSymbol = 32;
        constexpr int kMargin = 8;
        std::vector<std::complex<double>> baseband;
        if (extractComplexBaseband(samples,
                                   bestStart - kMargin * kDecimation,
                                   kFt4Symbols * kSamplesPerBasebandSymbol + 2 * kMargin,
                                   kDecimation,
                                   bestBaseHz,
                                   coherentTaps,
                                   baseband)) {
            auto correlationAt = [&baseband](int symbol, int tone, int shift) {
                std::complex<double> sum(0.0, 0.0);
                const int begin = kMargin + shift + symbol * kSamplesPerBasebandSymbol;
                for (int n = 0; n < kSamplesPerBasebandSymbol; ++n) {
                    const int index = begin + n;
                    if (index < 0 || index >= static_cast<int>(baseband.size())) {
                        continue;
                    }
                    const double phase = -kTwoPi * static_cast<double>(tone * n) /
                                         static_cast<double>(kSamplesPerBasebandSymbol);
                    sum += baseband[static_cast<size_t>(index)] *
                           std::complex<double>(std::cos(phase), std::sin(phase));
                }
                return sum;
            };

            int coherentShift = 0;
            double coherentSync = -1.0;
            for (int shift = -4; shift <= 4; ++shift) {
                double blockScore[4] = {0.0, 0.0, 0.0, 0.0};
                for (int block = 0; block < 4; ++block) {
                    for (int k = 0; k < 4; ++k) {
                        blockScore[block] += std::abs(correlationAt(blocks[block].pos + k,
                                                                    blocks[block].tones[k],
                                                                    shift));
                    }
                }
                std::sort(blockScore, blockScore + 4);
                const double score = blockScore[1] + blockScore[2] + blockScore[3];
                if (score > coherentSync) {
                    coherentSync = score;
                    coherentShift = shift;
                }
            }

            // Keep correlations for all 103 symbols, including Costas
            // blocks.  WSJT-X forms 1/2/4-symbol groups over the complete
            // symbol stream and only afterwards extracts the 87 data symbols;
            // the final group of a data block may therefore extend into the
            // following sync block.
            std::array<std::array<std::complex<double>, 4>, kFt4Symbols> symbolCorrelation{};
            for (int sym = 0; sym < kFt4Symbols; ++sym) {
                for (int tone = 0; tone < 4; ++tone) {
                    int grayIndex = 0;
                    for (int i = 0; i < 4; ++i) {
                        if (kFt4GrayMap[i] == tone) {
                            grayIndex = i;
                            break;
                        }
                    }
                    symbolCorrelation[static_cast<size_t>(sym)]
                                     [static_cast<size_t>(grayIndex)] =
                        correlationAt(sym, tone, coherentShift);
                }
            }

            auto normalizeGlobalMetric = [](std::array<double, 206> &metric) {
                double mean = 0.0;
                double mean2 = 0.0;
                for (double value : metric) {
                    mean += value;
                    mean2 += value * value;
                }
                mean /= static_cast<double>(metric.size());
                mean2 /= static_cast<double>(metric.size());
                const double sigma = std::sqrt(std::max(0.0, mean2 - mean * mean));
                const double scale = (sigma > 1.0e-8 && std::isfinite(sigma))
                    ? (2.83 / sigma)
                    : 1.0;
                for (double &value : metric) {
                    value = qBound(-18.0, value * scale, 18.0);
                }
            };

            auto globalCoherentMetric = [&](int groupSize,
                                            std::array<double, 206> *ratioOut) {
                std::array<double, 206> metric{};
                metric.fill(0.0);
                if (ratioOut != nullptr) {
                    ratioOut->fill(0.0);
                }
                const int comboCount = 1 << (2 * groupSize);
                for (int symbolStart = 0;
                     symbolStart + groupSize <= kFt4Symbols;
                     symbolStart += groupSize) {
                    std::vector<double> scores(static_cast<size_t>(comboCount), 0.0);
                    for (int combo = 0; combo < comboCount; ++combo) {
                        std::complex<double> coherentSum(0.0, 0.0);
                        for (int g = 0; g < groupSize; ++g) {
                            const int shift = 2 * (groupSize - 1 - g);
                            const int symbolIndex = (combo >> shift) & 0x3;
                            coherentSum += symbolCorrelation[static_cast<size_t>(symbolStart + g)]
                                                            [static_cast<size_t>(symbolIndex)];
                        }
                        scores[static_cast<size_t>(combo)] = std::abs(coherentSum);
                    }
                    for (int bit = 0; bit < 2 * groupSize; ++bit) {
                        const int globalBit = symbolStart * 2 + bit;
                        if (globalBit >= static_cast<int>(metric.size())) {
                            continue;
                        }
                        const int mask = 1 << (2 * groupSize - 1 - bit);
                        double best0 = -1.0e99;
                        double best1 = -1.0e99;
                        for (int combo = 0; combo < comboCount; ++combo) {
                            if ((combo & mask) != 0) {
                                best1 = std::max(best1, scores[static_cast<size_t>(combo)]);
                            } else {
                                best0 = std::max(best0, scores[static_cast<size_t>(combo)]);
                            }
                        }
                        const double value = best0 - best1;
                        metric[static_cast<size_t>(globalBit)] = value;
                        if (ratioOut != nullptr) {
                            const double denominator = std::max(best0, best1);
                            (*ratioOut)[static_cast<size_t>(globalBit)] =
                                denominator > 0.0 ? value / denominator : 0.0;
                        }
                    }
                }
                return metric;
            };

            std::array<double, 206> ratioGlobal{};
            std::array<double, 206> metric1Global = globalCoherentMetric(1, &ratioGlobal);
            std::array<double, 206> metric2Global = globalCoherentMetric(2, nullptr);
            std::array<double, 206> metric4Global = globalCoherentMetric(4, nullptr);

            // Reference tail handling: a two-symbol group leaves the final
            // symbol, while a four-symbol group leaves the final three.  Fill
            // those positions from the shorter coherent metrics.
            metric2Global[204] = metric1Global[204];
            metric2Global[205] = metric1Global[205];
            for (int bit = 200; bit <= 203; ++bit) {
                metric4Global[static_cast<size_t>(bit)] = metric2Global[static_cast<size_t>(bit)];
            }
            metric4Global[204] = metric1Global[204];
            metric4Global[205] = metric1Global[205];

            std::array<double, 206> cherryGlobal{};
            for (int bit = 0; bit < 206; ++bit) {
                double value = metric1Global[static_cast<size_t>(bit)];
                if (std::abs(metric2Global[static_cast<size_t>(bit)]) > std::abs(value)) {
                    value = metric2Global[static_cast<size_t>(bit)];
                }
                if (std::abs(metric4Global[static_cast<size_t>(bit)]) > std::abs(value)) {
                    value = metric4Global[static_cast<size_t>(bit)];
                }
                cherryGlobal[static_cast<size_t>(bit)] = value;
            }

            normalizeGlobalMetric(metric1Global);
            normalizeGlobalMetric(metric2Global);
            normalizeGlobalMetric(metric4Global);
            normalizeGlobalMetric(ratioGlobal);
            normalizeGlobalMetric(cherryGlobal);

            auto extractDataMetric = [](const std::array<double, 206> &global) {
                std::array<double, 174> metric{};
                int out = 0;
                const int first[3] = {8, 74, 140};
                for (int block = 0; block < 3; ++block) {
                    for (int bit = 0; bit < 58; ++bit) {
                        metric[static_cast<size_t>(out++)] =
                            global[static_cast<size_t>(first[block] + bit)];
                    }
                }
                return metric;
            };

            const std::array<double, 174> metric1 = extractDataMetric(metric1Global);
            const std::array<double, 174> metric2 = extractDataMetric(metric2Global);
            const std::array<double, 174> metric4 = extractDataMetric(metric4Global);
            const std::array<double, 174> ratioMetric = extractDataMetric(ratioGlobal);
            const std::array<double, 174> cherry = extractDataMetric(cherryGlobal);

            // The cherry metric is cheapest to try first in the live boundary
            // because it often contains the most reliable bit from the three
            // coherent groupings.  The individual reference families remain
            // available as independent fallbacks.
            fecDecoded = tryFt4Metric(cherry, true);
            if (!fecDecoded) fecDecoded = tryFt4Metric(metric1, true);
            if (!fecDecoded) fecDecoded = tryFt4Metric(metric2, true);
            if (!fecDecoded) fecDecoded = tryFt4Metric(metric4, true);
            if (!fecDecoded) fecDecoded = tryFt4Metric(ratioMetric, true);
        }
    }

    if (!fecDecoded) {
        return reject(sawCrcFailure ? DecodeRejectReason::Crc : DecodeRejectReason::Ldpc);
    }
    const QString message = unpackFt4Message77(bits).trimmed().toUpper();
    if (!looksLikeUsefulFt8Message(message)) {
        return reject(DecodeRejectReason::Message);
    }
    if (qualityOut != nullptr) {
        if (finalViaSumProduct) {
            ++qualityOut->sumProductRecovered;
        }
        if (finalViaCoherentMetric) {
            ++qualityOut->coherentMetricRecovered;
        }
    }

    std::array<int, 103> decodedTones{};
    int toneDataIndex = 0;
    for (int sym = 0; sym < kFt4Symbols; ++sym) {
        if (sym < 4) {
            decodedTones[static_cast<size_t>(sym)] = kFt4SyncA[sym];
        } else if (sym >= 33 && sym < 37) {
            decodedTones[static_cast<size_t>(sym)] = kFt4SyncB[sym - 33];
        } else if (sym >= 66 && sym < 70) {
            decodedTones[static_cast<size_t>(sym)] = kFt4SyncC[sym - 66];
        } else if (sym >= 99) {
            decodedTones[static_cast<size_t>(sym)] = kFt4SyncD[sym - 99];
        } else {
            const int bitIndex = toneDataIndex * 2;
            const int idxBits = ((bits[static_cast<size_t>(bitIndex)] & 1) << 1) |
                                (bits[static_cast<size_t>(bitIndex + 1)] & 1);
            decodedTones[static_cast<size_t>(sym)] = kFt4GrayMap[qBound(0, idxBits, 3)];
            ++toneDataIndex;
        }
    }
    if (decodedTonesOut != nullptr) {
        *decodedTonesOut = decodedTones;
    }

    if (rejectReasonOut != nullptr) {
        *rejectReasonOut = DecodeRejectReason::None;
    }

    decodeOut.utc = slotStartUtc.time().toString(QStringLiteral("HHmmss"));
    decodeOut.slotStartUtcMs = slotStartUtc.toMSecsSinceEpoch();
    // See the FT8 candidate path above: the coordinator owns the configuration
    // lock for this complete pass, so pool workers read the stable mode directly
    // and must not recurse into the accessor from another thread.
    decodeOut.slotPeriodMs = Ft8Mode::profileForMode(m_modeName).slotMs;
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




void Ft8RxDecoder::subtractFt4DecodedSignal(QVector<double> &samples,
                                              const Candidate &candidate,
                                              const std::array<int, 103> &decodedTones) const
{
    constexpr int kFt4SamplesPerSymbol = 576;
    std::vector<std::complex<double>> reference;
    std::vector<double> dphi;
    makeFt4ReferenceWaveformRx(decodedTones, candidate.baseHz, reference, dphi);
    if (reference.empty()) {
        return;
    }

    // The GFSK pulse spans three symbols, so the reference begins one symbol
    // before the first FT4 sync symbol, as in the WSJT-X/MSHV SIC path.
    const int nominalStart = qRound(candidate.startSec * kDecodeSampleRate) - kFt4SamplesPerSymbol;

    // Use the known CRC-valid waveform to refine cancellation timing only.
    // The published decode DT remains unchanged.
    int bestOffset = 0;
    // The exact CRC-derived FT4 waveform remains active in live RX, but the
    // additional timing search is kept offline until it has a measured
    // residual-energy advantage without collateral cancellation.
    if (m_offlineAnalysisActive.load()) {
        double bestCorrelation = -1.0;
        for (int offset : {-72, -36, 0, 36, 72}) {
            std::complex<double> corr(0.0, 0.0);
            double refPower = 0.0;
            int used = 0;
            const int frameStart = nominalStart + offset;
            for (int i = 0; i < static_cast<int>(reference.size()); i += 4) {
                const int sampleIndex = frameStart + i;
                if (sampleIndex < 0 || sampleIndex >= samples.size()) {
                    continue;
                }
                corr += samples.at(sampleIndex) * std::conj(reference[static_cast<size_t>(i)]);
                refPower += std::norm(reference[static_cast<size_t>(i)]);
                ++used;
            }
            const double score = (used > 0 && refPower > 0.0)
                ? (std::norm(corr) / refPower)
                : 0.0;
            if (score > bestCorrelation) {
                bestCorrelation = score;
                bestOffset = offset;
            }
        }
    }

    const int frameStart = nominalStart + bestOffset;
    std::vector<std::complex<double>> envelope(reference.size(), std::complex<double>(0.0, 0.0));
    bool any = false;
    for (int i = 0; i < static_cast<int>(reference.size()); ++i) {
        const int sampleIndex = frameStart + i;
        if (sampleIndex < 0 || sampleIndex >= samples.size()) {
            continue;
        }
        envelope[static_cast<size_t>(i)] =
            samples.at(sampleIndex) * std::conj(reference[static_cast<size_t>(i)]);
        any = true;
    }
    if (!any) {
        return;
    }

    smoothComplexEnvelopeZeroPhaseGeneric(envelope, 300.0, 144);
    constexpr double kFt4SubtractGain = 2.0;
    for (int i = 0; i < static_cast<int>(reference.size()); ++i) {
        const int sampleIndex = frameStart + i;
        if (sampleIndex < 0 || sampleIndex >= samples.size()) {
            continue;
        }
        const double reconstructed =
            std::real(envelope[static_cast<size_t>(i)] * reference[static_cast<size_t>(i)]);
        samples[sampleIndex] -= kFt4SubtractGain * reconstructed;
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

bool Ft8RxDecoder::ldpcDecode174_91SumProduct(const std::array<double, 174> &llr,
                                               std::array<int, 174> &hardBits,
                                               int &iterationsUsed,
                                               std::array<double, 174> *posteriorOut) const
{
    constexpr int N = 174;
    constexpr int M = 83;
    constexpr int MAX_CHECK_DEG = 7;
    struct EdgeRef { int check = -1; int edge = -1; };
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
                q[c][e] = qBound(-20.0, llr[static_cast<size_t>(v)], 20.0);
            }
        }
    }

    for (int iter = 0; iter < 35; ++iter) {
        for (int c = 0; c < M; ++c) {
            const int degree = nrw_ft8_174_91[c];
            for (int e = 0; e < degree; ++e) {
                double product = 1.0;
                for (int k = 0; k < degree; ++k) {
                    if (k == e) {
                        continue;
                    }
                    product *= std::tanh(0.5 * q[c][k]);
                }
                product = qBound(-1.0 + 1.0e-12, product, 1.0 - 1.0e-12);
                r[c][e] = qBound(-20.0, 2.0 * std::atanh(product), 20.0);
            }
        }

        for (int v = 0; v < N; ++v) {
            double posterior = llr[static_cast<size_t>(v)];
            for (const EdgeRef &edge : variableEdges[v]) {
                if (edge.check >= 0) {
                    posterior += r[edge.check][edge.edge];
                }
            }
            posterior = qBound(-40.0, posterior, 40.0);
            if (posteriorOut != nullptr) {
                (*posteriorOut)[static_cast<size_t>(v)] = posterior;
            }
            hardBits[static_cast<size_t>(v)] = posterior < 0.0 ? 1 : 0;
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
                double message = llr[static_cast<size_t>(v)];
                for (const EdgeRef &other : variableEdges[v]) {
                    if (other.check < 0 ||
                        (other.check == edge.check && other.edge == edge.edge)) {
                        continue;
                    }
                    message += r[other.check][other.edge];
                }
                q[edge.check][edge.edge] = qBound(-20.0, message, 20.0);
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
