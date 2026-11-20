#include "RxNoiseBlanker.h"

/*
 * RX noise blanker and MSK144 carrier notch (IC8TEM). See RxNoiseBlanker.h.
 * Copyright (C) 2026 Costantino Cerrotta, IC8TEM. GNU GPL v3 or later.
 */

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>

namespace MadModemNoise {
namespace {

constexpr double kPi = 3.14159265358979323846;

std::atomic<int> g_mode{static_cast<int>(BlankerMode::Off)};
std::atomic<int> g_level{static_cast<int>(BlankerLevel::Normal)};
std::atomic<bool> g_notch{false};
std::atomic<quint64> g_generation{1};
std::atomic<quint64> g_events{0};

double medianOf(const double *values, int count, std::vector<double> &scratch)
{
    if (count <= 0) {
        return 0.0;
    }
    scratch.assign(values, values + count);
    const auto middle = scratch.begin() + count / 2;
    std::nth_element(scratch.begin(), middle, scratch.end());
    return *middle;
}

} // namespace

Settings settings()
{
    Settings s;
    s.mode = static_cast<BlankerMode>(g_mode.load(std::memory_order_acquire));
    s.level = static_cast<BlankerLevel>(g_level.load(std::memory_order_acquire));
    s.mskCarrierNotch = g_notch.load(std::memory_order_acquire);
    return s;
}

void setSettings(const Settings &s)
{
    const int mode = qBound(0, static_cast<int>(s.mode), 2);
    const int level = qBound(0, static_cast<int>(s.level), 2);
    const bool modeChanged = g_mode.exchange(mode, std::memory_order_acq_rel) != mode;
    const bool levelChanged = g_level.exchange(level, std::memory_order_acq_rel) != level;
    const bool notchChanged = g_notch.exchange(s.mskCarrierNotch, std::memory_order_acq_rel) != s.mskCarrierNotch;
    if (modeChanged || levelChanged || notchChanged) {
        g_generation.fetch_add(1, std::memory_order_acq_rel);
    }
}

quint64 settingsGeneration()
{
    return g_generation.load(std::memory_order_acquire);
}

quint64 totalEvents()
{
    return g_events.load(std::memory_order_relaxed);
}

QString modeName(BlankerMode mode)
{
    switch (mode) {
    case BlankerMode::Nb2: return QStringLiteral("NB2");
    case BlankerMode::Lpc: return QStringLiteral("LPC");
    case BlankerMode::Off: break;
    }
    return QStringLiteral("Off");
}

double nb2Threshold(BlankerLevel level)
{
    switch (level) {
    case BlankerLevel::Sensitive: return 4.0;
    case BlankerLevel::StrongOnly: return 7.0;
    case BlankerLevel::Normal: break;
    }
    return 5.0; // MSHV-IC8TEM value
}

double lpcThreshold(BlankerLevel level)
{
    switch (level) {
    case BlankerLevel::Sensitive: return 8.0;
    case BlankerLevel::StrongOnly: return 14.0;
    case BlankerLevel::Normal: break;
    }
    return 10.0; // MSHV-IC8TEM value
}

// ------------------------------------------------------------------ FFT

void Fft::resize(int n)
{
    if (n == m_n) {
        return;
    }
    m_n = n;
    m_twiddles.resize(static_cast<std::size_t>(n / 2));
    for (int k = 0; k < n / 2; ++k) {
        const double angle = -2.0 * kPi * k / n;
        m_twiddles[static_cast<std::size_t>(k)] = std::complex<double>(std::cos(angle), std::sin(angle));
    }
    m_bitReverse.assign(static_cast<std::size_t>(n), 0);
    for (int i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        m_bitReverse[static_cast<std::size_t>(i)] = j;
    }
}

void Fft::run(std::complex<double> *data, bool inverse) const
{
    const int n = m_n;
    for (int i = 1; i < n; ++i) {
        const int j = m_bitReverse[static_cast<std::size_t>(i)];
        if (i < j) {
            std::swap(data[i], data[j]);
        }
    }
    for (int len = 2; len <= n; len <<= 1) {
        const int half = len / 2;
        const int stride = n / len;
        for (int i = 0; i < n; i += len) {
            for (int k = 0; k < half; ++k) {
                std::complex<double> w = m_twiddles[static_cast<std::size_t>(k * stride)];
                if (inverse) {
                    w = std::conj(w);
                }
                const std::complex<double> t = w * data[i + k + half];
                data[i + k + half] = data[i + k] - t;
                data[i + k] += t;
            }
        }
    }
}

// ------------------------------------------------------------------ impulse blanker

void ImpulseBlanker::configure(BlankerMode mode, double threshold, int sampleRate)
{
    const bool geometry = mode != m_mode || sampleRate != m_fs;
    m_mode = mode;
    m_threshold = threshold;
    if (!geometry) {
        return;
    }
    m_fs = sampleRate;
    if (mode == BlankerMode::Off || sampleRate <= 0) {
        m_buffer.clear();
        m_gain.clear();
        m_out.clear();
        m_outRead = 0;
        m_fill = 0;
        return;
    }
    const double scale = sampleRate / 12000.0;
    auto samples = [sampleRate](double seconds, int minimum) {
        return std::max(minimum, static_cast<int>(std::lround(seconds * sampleRate)));
    };
    // LPC frames as in DL2FW/MSHV at 12 kHz (128 samples, order 10, 11-sample repair).
    m_frame = std::max(32, static_cast<int>(std::lround(128.0 * scale)));
    m_order = qBound(6, static_cast<int>(std::lround(10.0 * scale)), 40);
    m_half = qBound(2, static_cast<int>(std::lround(5.0 * scale)), 20);
    m_c = 2 * m_frame;                 // 21.3 ms chunk
    m_h = samples(0.024, 8);           // history: level before the event, reference blocks
    m_l = samples(0.0267, 8);          // look-ahead: longest event plus the level after it
    m_w = m_h + m_c + m_l;
    m_block = samples(0.004, 4);       // 4 ms blocks
    m_gap = samples(0.001, 1);         // closer than 1 ms = same event
    m_coreMax = samples(0.003, 2);     // core longer than 3 ms: a ping or a keyed signal, kept
    m_eventMax = samples(0.015, 4);    // never blank more than 15 ms
    m_guard = samples(0.0005, 1);      // 0.5 ms advance and hang time
    m_slew = samples(0.0005, 1);       // 0.5 ms raised-cosine slews
    m_side = samples(0.004, 4);        // level around the event, 4 ms each side
    int n = 1;
    while (n < m_w) {
        n <<= 1;
    }
    m_fft.resize(n);
    reset();
}

void ImpulseBlanker::reset()
{
    if (m_mode == BlankerMode::Off || m_w <= 0) {
        return;
    }
    m_buffer.assign(static_cast<std::size_t>(m_w), 0.0);
    m_gain.assign(static_cast<std::size_t>(m_w), 1.0);
    m_out.assign(static_cast<std::size_t>(m_c), 0.0);
    m_outRead = 0;
    m_fill = 0;
    m_windowStart = -static_cast<qint64>(m_w - m_c);
    m_lastEventEnd = std::numeric_limits<qint64>::min() / 2;
}

void ImpulseBlanker::process(double *x, int n)
{
    if (m_mode == BlankerMode::Off || m_w <= 0 || x == nullptr) {
        return;
    }
    const int tail = m_w - m_c;
    for (int i = 0; i < n; ++i) {
        m_buffer[static_cast<std::size_t>(tail + m_fill)] = x[i];
        if (++m_fill == m_c) {
            step();
        }
        x[i] = m_out[m_outRead++];
    }
}

void ImpulseBlanker::step()
{
    if (m_mode == BlankerMode::Nb2) {
        detectNb2();
    } else if (m_mode == BlankerMode::Lpc) {
        // Frames start half a repair window after the chunk, so that a repair never
        // reaches samples already delivered.
        for (int start = m_h + m_half; start < m_h + m_c + m_half; start += m_frame) {
            repairLpcFrame(start);
        }
    }
    if (m_outRead > 0) {
        m_out.erase(m_out.begin(), m_out.begin() + static_cast<std::ptrdiff_t>(m_outRead));
        m_outRead = 0;
    }
    for (int i = 0; i < m_c; ++i) {
        const std::size_t k = static_cast<std::size_t>(m_h + i);
        m_out.push_back(m_buffer[k] * m_gain[k]);
    }
    std::copy(m_buffer.begin() + m_c, m_buffer.end(), m_buffer.begin());
    std::copy(m_gain.begin() + m_c, m_gain.end(), m_gain.begin());
    std::fill(m_gain.end() - m_c, m_gain.end(), 1.0);
    m_windowStart += m_c;
    m_fill = 0;
}

void ImpulseBlanker::detectNb2()
{
    // Envelope of the analytic signal of the whole window.
    const int n = m_fft.size();
    m_analytic.assign(static_cast<std::size_t>(n), std::complex<double>());
    for (int i = 0; i < m_w; ++i) {
        m_analytic[static_cast<std::size_t>(i)] = std::complex<double>(m_buffer[static_cast<std::size_t>(i)], 0.0);
    }
    m_fft.forward(m_analytic.data());
    for (int k = 1; k < n / 2; ++k) {
        m_analytic[static_cast<std::size_t>(k)] *= 2.0;
    }
    std::fill(m_analytic.begin() + n / 2 + 1, m_analytic.end(), std::complex<double>());
    m_fft.inverse(m_analytic.data());
    m_env.resize(static_cast<std::size_t>(m_w));
    const double scale = 1.0 / n;
    for (int i = 0; i < m_w; ++i) {
        m_env[static_cast<std::size_t>(i)] = std::abs(m_analytic[static_cast<std::size_t>(i)]) * scale;
    }

    // Median of 4 ms blocks, reference = median of the nine blocks around.
    const int blocks = m_w / m_block;
    if (blocks < 3) {
        return;
    }
    m_blockMedian.resize(static_cast<std::size_t>(blocks));
    for (int b = 0; b < blocks; ++b) {
        m_blockMedian[static_cast<std::size_t>(b)] = medianOf(&m_env[static_cast<std::size_t>(b * m_block)], m_block, m_scratch);
    }
    m_reference.resize(static_cast<std::size_t>(blocks));
    for (int b = 0; b < blocks; ++b) {
        const int a = std::max(0, b - 4);
        const int z = std::min(blocks - 1, b + 4);
        m_reference[static_cast<std::size_t>(b)] = medianOf(&m_blockMedian[static_cast<std::size_t>(a)], z - a + 1, m_scratch);
    }
    auto reference = [this, blocks](int i) {
        return m_reference[static_cast<std::size_t>(std::min(i / m_block, blocks - 1))];
    };
    auto env = [this](int i) { return m_env[static_cast<std::size_t>(i)]; };

    const int lead = m_guard + m_slew;
    const int scanBegin = m_h + lead;
    const int scanEnd = m_h + m_c + lead;
    for (int i = scanBegin; i < scanEnd; ++i) {
        if (m_windowStart + i <= m_lastEventEnd + m_gap) {
            continue; // part of an event already judged
        }
        const double ref = reference(i);
        if (!(ref > 0.0 && env(i) > m_threshold * ref)) {
            continue;
        }
        const int s = i;
        int e = i;
        double peak = env(i);
        for (int j = i + 1; j < m_w && j - e <= m_gap; ++j) {
            if (env(j) > m_threshold * reference(j)) {
                e = j;
                peak = std::max(peak, env(j));
            }
        }
        m_lastEventEnd = m_windowStart + e;
        i = e;
        if (e - s + 1 > m_eventMax || e + m_gap + m_side >= m_w) {
            continue; // a long crash or a signal: keep it
        }
        // Width of the core (above peak/4): an impulse is short even when the
        // ringing of the receiver filters is long.
        int c0 = s;
        int c1 = e;
        while (c0 < e && env(c0) < 0.25 * peak) {
            ++c0;
        }
        while (c1 > s && env(c1) < 0.25 * peak) {
            --c1;
        }
        if (c1 - c0 + 1 > m_coreMax) {
            continue; // a ping
        }
        // The level just before and after must be low too (not the edge of a ping).
        const int before0 = std::max(0, s - m_gap - m_side);
        const int before1 = std::max(0, s - m_gap);
        const int after0 = std::min(m_w, e + m_gap + 1);
        const int after1 = std::min(m_w, e + m_gap + 1 + m_side);
        const double before = medianOf(&m_env[static_cast<std::size_t>(before0)], before1 - before0, m_scratch);
        const double after = medianOf(&m_env[static_cast<std::size_t>(after0)], after1 - after0, m_scratch);
        const double side = std::max(before, after);
        if (!(side > 0.0) || peak < m_threshold * side) {
            continue;
        }
        const int a = s - m_guard;
        const int z = e + m_guard;
        for (int k = 0; k < m_slew; ++k) {
            const double w = 0.5 + 0.5 * std::cos(kPi * (k + 1) / (m_slew + 1));
            m_gain[static_cast<std::size_t>(a - m_slew + k)] *= w;
        }
        for (int k = a; k <= z; ++k) {
            m_gain[static_cast<std::size_t>(k)] = 0.0;
        }
        for (int k = 0; k < m_slew; ++k) {
            const double w = 0.5 - 0.5 * std::cos(kPi * (k + 1) / (m_slew + 1));
            m_gain[static_cast<std::size_t>(z + 1 + k)] *= w;
        }
        ++m_events;
        g_events.fetch_add(1, std::memory_order_relaxed);
    }
}

void ImpulseBlanker::repairLpcFrame(int start)
{
    const int order = m_order;
    const int frame = m_frame;
    const int half = m_half;
    const int span = 2 * half + 1;
    constexpr int kMaxImpulses = 5; // DL2FW
    constexpr int kMaxOrder = 40;
    double *x = m_buffer.data();
    if (start - 2 * order < 0 || start + frame > m_w) {
        return;
    }

    // Autocorrelation and Levinson-Durbin.
    double r[kMaxOrder + 1] = {};
    for (int lag = 0; lag <= order; ++lag) {
        double acc = 0.0;
        for (int t = 0; t < frame - lag; ++t) {
            acc += x[start + t] * x[start + t + lag];
        }
        r[lag] = acc;
    }
    if (!(r[0] > 0.0)) {
        return;
    }
    r[0] *= (order > 10 ? 1.0 + 1.0e-7 : 1.0 + 1.0e-9);
    double a[kMaxOrder + 1] = {};
    double next[kMaxOrder + 1] = {};
    a[0] = 1.0;
    for (int k = 1; k <= order; ++k) {
        a[k] = 0.0;
    }
    double alfa = r[0];
    for (int m = 1; m <= order; ++m) {
        double sum = 0.0;
        for (int u = 1; u < m; ++u) {
            sum += a[u] * r[m - u];
        }
        const double k = -(r[m] + sum) / alfa;
        for (int v = 1; v < m; ++v) {
            next[v] = a[v] + k * a[m - v];
        }
        for (int v = 1; v < m; ++v) {
            a[v] = next[v];
        }
        a[m] = k;
        alfa *= (1.0 - k * k);
        if (!(alfa > 0.0)) {
            return;
        }
    }

    // Inverse filter (prediction error), then matched filter: peak delayed by the order.
    std::vector<double> &e = m_env;
    std::vector<double> &matched = m_reference;
    e.assign(static_cast<std::size_t>(frame + order), 0.0);
    matched.assign(static_cast<std::size_t>(frame), 0.0);
    for (int t = -order; t < frame; ++t) {
        double acc = 0.0;
        for (int j = 0; j <= order; ++j) {
            acc += a[j] * x[start + t - j];
        }
        e[static_cast<std::size_t>(t + order)] = acc;
    }
    for (int t = 0; t < frame; ++t) {
        double acc = 0.0;
        for (int j = 0; j <= order; ++j) {
            acc += a[order - j] * e[static_cast<std::size_t>(t - j + order)];
        }
        matched[static_cast<std::size_t>(t)] = acc;
    }

    // Robust level of the matched filter output (median absolute deviation).
    const double med = medianOf(matched.data(), frame, m_scratch);
    m_blockMedian.resize(static_cast<std::size_t>(frame));
    for (int t = 0; t < frame; ++t) {
        m_blockMedian[static_cast<std::size_t>(t)] = std::fabs(matched[static_cast<std::size_t>(t)] - med);
    }
    const double sigma = 1.4826 * medianOf(m_blockMedian.data(), frame, m_scratch);
    if (!(sigma > 0.0)) {
        return;
    }
    const double threshold = m_threshold * sigma;
    auto excess = [&matched, med](int t) { return std::fabs(matched[static_cast<std::size_t>(t)] - med); };
    // An impulse must also stand out of the audio itself. Without this, the symbol
    // transitions of a strong, clean digital signal (tiny prediction error everywhere
    // else) would be taken for impulses.
    for (int t = 0; t < frame; ++t) {
        m_blockMedian[static_cast<std::size_t>(t)] = std::fabs(x[start + t]);
    }
    const double amplitude = 1.4826 * medianOf(m_blockMedian.data(), frame, m_scratch);

    int count = 0;
    double forward[kMaxOrder + 2 * 20 + 2];
    double backward[kMaxOrder + 2 * 20 + 2];
    if (order + span > static_cast<int>(sizeof(forward) / sizeof(forward[0]))) {
        return;
    }
    for (int t = order; t < frame && count < kMaxImpulses; ++t) {
        if (excess(t) <= threshold) {
            continue;
        }
        // Only short isolated impulses: the excess must end within the repair window.
        int r0 = t;
        int r1 = t;
        while (r0 > 0 && excess(r0 - 1) > 0.5 * threshold) {
            --r0;
        }
        while (r1 < frame - 1 && excess(r1 + 1) > 0.5 * threshold) {
            ++r1;
        }
        if (r1 - r0 + 1 > span) {
            t = r1;
            continue;
        }
        const int centre = start + t - order;
        const int first = centre - half;
        const int last = centre + half;
        if (first < m_h || first - order < 0 || last + order >= m_w) {
            t += half;
            continue;
        }
        double peak = 0.0;
        for (int q = first; q <= last; ++q) {
            peak = std::max(peak, std::fabs(x[q]));
        }
        if (!(peak > 4.0 * amplitude)) {
            t += half;
            continue;
        }
        for (int k = 0; k < order; ++k) {
            forward[k] = x[first - order + k];
        }
        for (int q = 0; q < span; ++q) {
            double acc = 0.0;
            for (int j = 1; j <= order; ++j) {
                acc -= a[j] * forward[order + q - j];
            }
            forward[order + q] = acc;
        }
        for (int k = 0; k < order; ++k) {
            backward[span + k] = x[last + 1 + k];
        }
        for (int q = span - 1; q >= 0; --q) {
            double acc = 0.0;
            for (int j = 1; j <= order; ++j) {
                acc -= a[j] * backward[q + j];
            }
            backward[q] = acc;
        }
        for (int q = 0; q < span; ++q) {
            const double wb = static_cast<double>(q) / static_cast<double>(span - 1);
            x[first + q] = (1.0 - wb) * forward[order + q] + wb * backward[q];
        }
        ++count;
        ++m_events;
        g_events.fetch_add(1, std::memory_order_relaxed);
        t += half;
    }
}

// ------------------------------------------------------------------ carrier notch

namespace {
constexpr int kZapN = 2048;      // 5.86 Hz bins at 12 kHz, 0.17 s slices
constexpr int kZapRing = 32;     // last 32 slices, about 5.5 s
constexpr int kZapMaxLines = 8;
constexpr double kZapTone = 12.0; // MSK tone metric above which nothing is notched
constexpr double kNotchWidthHz = 10.0;
} // namespace

void CarrierNotch::reset()
{
    m_slice.clear();
    m_ring.clear();
    m_tones.clear();
    m_bins = 0;
    m_firstBin = 0;
    m_slices = 0;
    m_pos = 0;
    m_notches.clear();
}

QVector<double> CarrierNotch::lines() const
{
    QVector<double> out;
    for (const Notch &n : m_notches) {
        out.append(n.hz);
    }
    return out;
}

void CarrierNotch::process(double *x, int n)
{
    if (x == nullptr) {
        return;
    }
    for (int i = 0; i < n; ++i) {
        m_slice.push_back(x[i]);
        double y = x[i];
        for (Notch &f : m_notches) {
            const double out = f.b0 * y + f.b1 * f.x1 + f.b2 * f.x2 - f.a1 * f.y1 - f.a2 * f.y2;
            f.x2 = f.x1;
            f.x1 = y;
            f.y2 = f.y1;
            f.y1 = out;
            y = out;
        }
        x[i] = y;
        if (static_cast<int>(m_slice.size()) == kZapN) {
            analyzeSlice();
            m_slice.clear();
        }
    }
}

void CarrierNotch::analyzeSlice()
{
    const double fs = kSampleRate;
    m_fft.resize(kZapN);
    m_work.resize(kZapN);
    std::vector<double> scratch;

    // Squared signal: a continuous MSK signal gives two lines 2000 Hz apart.
    for (int t = 0; t < kZapN; ++t) {
        const double w = 0.5 - 0.5 * std::cos(2.0 * kPi * t / kZapN);
        const double v = m_slice[static_cast<std::size_t>(t)];
        m_work[static_cast<std::size_t>(t)] = std::complex<double>(w * v * v, 0.0);
    }
    m_fft.forward(m_work.data());
    const int q0 = static_cast<int>(1000.0 * kZapN / fs);
    const int q1 = static_cast<int>(5000.0 * kZapN / fs);
    std::vector<double> sq(static_cast<std::size_t>(q1 - q0 + 1));
    for (int k = q0; k <= q1; ++k) {
        sq[static_cast<std::size_t>(k - q0)] = std::norm(m_work[static_cast<std::size_t>(k)]);
    }
    const double sqMedian = medianOf(sq.data(), static_cast<int>(sq.size()), scratch);
    double p2 = 0.0, p4 = 0.0;
    int k2 = -1, k4 = -1;
    for (int k = q0; k <= q1; ++k) {
        const double f = k * fs / kZapN;
        const double v = sq[static_cast<std::size_t>(k - q0)];
        if (std::fabs(f - 2000.0) <= 420.0 && v > p2) { p2 = v; k2 = k; }
        if (std::fabs(f - 4000.0) <= 420.0 && v > p4) { p4 = v; k4 = k; }
    }
    const double separation = (k2 >= 0 && k4 >= 0) ? std::fabs((k4 - k2) * fs / kZapN - 2000.0) : 1.0e9;
    const double tone = (sqMedian > 0.0 && separation <= 18.0) ? std::min(p2, p4) / sqMedian : 0.0;

    // Normalized power spectrum 300..2700 Hz.
    for (int t = 0; t < kZapN; ++t) {
        const double w = 0.5 - 0.5 * std::cos(2.0 * kPi * t / kZapN);
        m_work[static_cast<std::size_t>(t)] = std::complex<double>(w * m_slice[static_cast<std::size_t>(t)], 0.0);
    }
    m_fft.forward(m_work.data());
    const int kb0 = static_cast<int>(300.0 * kZapN / fs);
    const int kb1 = static_cast<int>(2700.0 * kZapN / fs);
    const int bins = kb1 - kb0 + 1;
    if (m_bins != bins) {
        m_bins = bins;
        m_firstBin = kb0;
        m_ring.assign(static_cast<std::size_t>(kZapRing * bins), 0.0);
        m_tones.assign(kZapRing, 0.0);
        m_slices = 0;
        m_pos = 0;
    }
    double *row = &m_ring[static_cast<std::size_t>(m_pos * bins)];
    for (int k = 0; k < bins; ++k) {
        row[k] = std::norm(m_work[static_cast<std::size_t>(kb0 + k)]);
    }
    const double rowMedian = medianOf(row, bins, scratch);
    if (rowMedian > 0.0) {
        for (int k = 0; k < bins; ++k) {
            row[k] /= rowMedian;
        }
    }
    m_tones[static_cast<std::size_t>(m_pos)] = tone;
    m_pos = (m_pos + 1) % kZapRing;
    m_slices = std::min(m_slices + 1, kZapRing);

    std::vector<double> found;
    if (m_slices < 12 || bins < 40) {
        updateNotches(found);
        return;
    }
    // An MSK signal present most of the time (tropo, Es): its spectrum is made of
    // steady lines too, so nothing is notched.
    if (medianOf(m_tones.data(), m_slices, scratch) > kZapTone) {
        updateNotches(found);
        return;
    }
    // Median over time of each bin: the pings do not count.
    std::vector<double> med(static_cast<std::size_t>(bins)), ref(static_cast<std::size_t>(bins)), column(static_cast<std::size_t>(m_slices));
    for (int k = 0; k < bins; ++k) {
        for (int s = 0; s < m_slices; ++s) {
            column[static_cast<std::size_t>(s)] = m_ring[static_cast<std::size_t>(s * bins + k)];
        }
        med[static_cast<std::size_t>(k)] = medianOf(column.data(), m_slices, scratch);
    }
    for (int k = 0; k < bins; ++k) {
        const int a = std::max(0, k - 8);
        const int z = std::min(bins - 1, k + 8);
        ref[static_cast<std::size_t>(k)] = medianOf(&med[static_cast<std::size_t>(a)], z - a + 1, scratch);
    }
    auto M = [&med](int k) { return med[static_cast<std::size_t>(k)]; };
    std::vector<std::pair<double, double>> lines; // (power ratio, Hz)
    for (int k = 3; k < bins - 3 && lines.size() < 64; ++k) {
        const double r = ref[static_cast<std::size_t>(k)];
        if (!(r > 0.0) || M(k) <= 8.0 * r || M(k) < M(k - 1) || M(k) < M(k + 1)) {
            continue;
        }
        // Narrow on both sides (not the edge of a wide signal).
        if (M(k) < 4.0 * M(k - 3) || M(k) < 4.0 * M(k + 3)) {
            continue;
        }
        // Not a tooth of a comb: MSK144 frames repeat every 72 ms (lines 13.9 Hz
        // apart), short messages every 20 ms (50 Hz). A tooth is a peak of its own,
        // not the window leakage of a strong carrier.
        bool comb = false;
        const double spacing[2] = {fs / 864.0, 50.0};
        for (int q = 0; q < 2 && !comb; ++q) {
            for (int sign = -1; sign <= 1; sign += 2) {
                const double kk = k + sign * spacing[q] * kZapN / fs;
                const int k0 = static_cast<int>(std::floor(kk));
                if (k0 < 1 || k0 + 2 >= bins) {
                    continue;
                }
                const int peak = M(k0 + 1) > M(k0) ? k0 + 1 : k0;
                const bool ownPeak = M(peak) >= M(peak - 1) && M(peak) >= M(peak + 1);
                if (ownPeak && M(peak) > 3.0 * r) {
                    comb = true;
                }
            }
        }
        if (comb) {
            continue;
        }
        // Peak position by parabolic interpolation of the log power (Hann window).
        const double lm = std::log(std::max(M(k - 1), 1e-12));
        const double l0 = std::log(std::max(M(k), 1e-12));
        const double lp = std::log(std::max(M(k + 1), 1e-12));
        const double den = lm - 2.0 * l0 + lp;
        double d = den != 0.0 ? 0.5 * (lm - lp) / den : 0.0;
        d = qBound(-0.5, d, 0.5);
        lines.push_back({M(k) / r, (m_firstBin + k + d) * fs / kZapN});
    }
    // Evenly spaced lines (13.9 Hz MSK144 frames, 50 Hz short messages) are a signal.
    int pairs = 0;
    for (std::size_t p = 0; p < lines.size(); ++p) {
        for (std::size_t q = p + 1; q < lines.size(); ++q) {
            const double delta = std::fabs(lines[q].second - lines[p].second);
            if (delta > 600.0) {
                continue;
            }
            const double r50 = std::fabs(delta - 50.0 * std::floor(delta / 50.0 + 0.5));
            const double frame = 12000.0 / 864.0;
            const double r14 = std::fabs(delta - frame * std::floor(delta / frame + 0.5));
            if (r50 < 1.5 || r14 < 1.0) {
                ++pairs;
            }
        }
    }
    if (pairs < 2 && static_cast<int>(lines.size()) <= kZapMaxLines) {
        std::sort(lines.begin(), lines.end(), [](const auto &l, const auto &r) { return l.first > r.first; });
        for (const auto &line : lines) {
            found.push_back(line.second);
        }
    }
    updateNotches(found);
}

void CarrierNotch::updateNotches(const std::vector<double> &hz)
{
    std::vector<Notch> next;
    for (double f : hz) {
        auto same = std::find_if(m_notches.begin(), m_notches.end(),
                                 [f](const Notch &n) { return std::fabs(n.hz - f) < 1.5; });
        if (same != m_notches.end()) {
            next.push_back(*same); // keep its state: no click while the carrier stays
            continue;
        }
        Notch n;
        n.hz = f;
        const double w0 = 2.0 * kPi * f / kSampleRate;
        const double c = std::cos(w0);
        const double r = 1.0 - kPi * kNotchWidthHz / kSampleRate;
        n.a1 = -2.0 * r * c;
        n.a2 = r * r;
        const double gain = (1.0 + n.a1 + n.a2) / (2.0 - 2.0 * c); // unity gain at DC
        n.b0 = gain;
        n.b1 = -2.0 * c * gain;
        n.b2 = gain;
        next.push_back(n);
    }
    m_notches.swap(next);
}

// ------------------------------------------------------------------ stage

bool RxNoiseStage::update(int sampleRate)
{
    const quint64 generation = settingsGeneration();
    if (generation == m_generation && sampleRate == m_fs) {
        return false;
    }
    const Settings s = settings();
    const qint64 before = latencyNs();
    const bool wasActive = m_blanker.mode() != BlankerMode::Off;
    m_generation = generation;
    const bool rateChanged = sampleRate != m_fs;
    m_fs = sampleRate;
    const double threshold = s.mode == BlankerMode::Lpc ? lpcThreshold(s.level) : nb2Threshold(s.level);
    m_blanker.configure(s.mode, threshold, sampleRate);
    const bool notch = m_notchAllowed && s.mskCarrierNotch && sampleRate == CarrierNotch::kSampleRate;
    if (notch != m_notchOn || rateChanged) {
        m_notchOn = notch;
        m_notch.reset();
    }
    return latencyNs() != before || (rateChanged && wasActive);
}

qint64 RxNoiseStage::latencyNs() const
{
    if (m_fs <= 0) {
        return 0;
    }
    return static_cast<qint64>(std::llround(1.0e9 * m_blanker.latencySamples() / m_fs));
}

void RxNoiseStage::reset()
{
    m_blanker.reset();
    m_notch.reset();
}

void RxNoiseStage::process(double *x, int n)
{
    if (n <= 0) {
        return;
    }
    m_blanker.process(x, n);
    if (m_notchOn) {
        m_notch.process(x, n);
    }
}

void RxNoiseStage::process(QVector<double> &samples)
{
    if (!active() || samples.isEmpty()) {
        return;
    }
    process(samples.data(), samples.size());
}

void RxNoiseStage::processRecording(QVector<double> &samples, int sampleRate, bool carrierNotchAllowed)
{
    RxNoiseStage stage(carrierNotchAllowed);
    stage.update(sampleRate);
    if (!stage.active() || samples.isEmpty()) {
        return;
    }
    const int delay = stage.latencySamples();
    std::vector<double> x(samples.cbegin(), samples.cend());
    x.resize(x.size() + static_cast<std::size_t>(delay), 0.0);
    stage.process(x.data(), static_cast<int>(x.size()));
    for (int i = 0; i < samples.size(); ++i) {
        samples[i] = x[static_cast<std::size_t>(i + delay)];
    }
}

void RxNoiseStage::processRecording(QVector<float> &samples, int sampleRate, bool carrierNotchAllowed)
{
    QVector<double> x(samples.cbegin(), samples.cend());
    processRecording(x, sampleRate, carrierNotchAllowed);
    for (int i = 0; i < samples.size(); ++i) {
        samples[i] = static_cast<float>(x[i]);
    }
}

void RxNoiseStage::process(QVector<float> &samples)
{
    if (!active() || samples.isEmpty()) {
        return;
    }
    m_temp.assign(samples.cbegin(), samples.cend());
    process(m_temp.data(), static_cast<int>(m_temp.size()));
    for (int i = 0; i < samples.size(); ++i) {
        samples[i] = static_cast<float>(m_temp[static_cast<std::size_t>(i)]);
    }
}

} // namespace MadModemNoise
