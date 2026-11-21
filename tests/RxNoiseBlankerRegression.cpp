// Regression test for the RX noise blanker and the MSK144 carrier notch (IC8TEM).
// Synthetic signals only: impulses must go, pings, keyed CW and FT-like tones must stay.
#include "dsp/common/RxNoiseBlanker.h"

#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

using namespace MadModemNoise;

namespace {
constexpr double kPi = 3.14159265358979323846;
int failures = 0;

void check(bool ok, const char *what)
{
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) {
        ++failures;
    }
}

double energy(const std::vector<double> &x, int a, int b)
{
    double e = 0.0;
    for (int i = std::max(0, a); i < std::min<int>(b, static_cast<int>(x.size())); ++i) {
        e += x[static_cast<std::size_t>(i)] * x[static_cast<std::size_t>(i)];
    }
    return e;
}

// Feeds the blanker in irregular blocks, like the sound card does.
std::vector<double> run(ImpulseBlanker &nb, std::vector<double> x)
{
    std::mt19937 rng(7);
    std::uniform_int_distribution<int> size(1, 1500);
    std::size_t pos = 0;
    while (pos < x.size()) {
        const int n = std::min<int>(size(rng), static_cast<int>(x.size() - pos));
        nb.process(x.data() + pos, n);
        pos += static_cast<std::size_t>(n);
    }
    return x;
}

// A band-limited click: a damped 1.8 kHz oscillation (receiver filter ringing).
void addImpulse(std::vector<double> &x, int at, double amplitude, int fs)
{
    for (int k = 0; k < fs / 400; ++k) {
        const int i = at + k;
        if (i >= 0 && i < static_cast<int>(x.size())) {
            x[static_cast<std::size_t>(i)] += amplitude * std::exp(-k / (0.0002 * fs)) * std::sin(2.0 * kPi * 1800.0 * k / fs);
        }
    }
}

std::vector<double> noise(int n, double sigma, unsigned seed)
{
    std::mt19937 rng(seed);
    std::normal_distribution<double> g(0.0, sigma);
    std::vector<double> x(static_cast<std::size_t>(n));
    for (double &v : x) {
        v = g(rng);
    }
    return x;
}

void testRate(int fs, const char *label)
{
    const int n = fs * 20;
    // Delay and transparency: noise plus an FT-like tone, no impulses.
    std::vector<double> x = noise(n, 0.01, 1);
    for (int i = 0; i < n; ++i) {
        x[static_cast<std::size_t>(i)] += 0.05 * std::sin(2.0 * kPi * 1500.0 * i / fs);
    }
    ImpulseBlanker nb;
    nb.configure(BlankerMode::Nb2, nb2Threshold(BlankerLevel::Normal), fs);
    const int delay = nb.latencySamples();
    std::vector<double> y = run(nb, x);
    double worst = 0.0;
    for (int i = delay; i < n; ++i) {
        worst = std::max(worst, std::fabs(y[static_cast<std::size_t>(i)] - x[static_cast<std::size_t>(i - delay)]));
    }
    char text[200];
    std::snprintf(text, sizeof(text), "%s: output = input delayed by %d samples (%.1f ms), %llu false events, max error %.2e",
                  label, delay, 1000.0 * delay / fs, static_cast<unsigned long long>(nb.events()), worst);
    check(worst < 1e-12 && nb.events() == 0, text);
    check(std::fabs(1000.0 * delay / fs - 48.0) < 1.0, "delay is 48 ms");

    // Impulses on noise and tone, plus a 60 ms ping and keyed CW: only the impulses go.
    std::vector<double> z = x;
    std::vector<int> clicks;
    for (int k = 0; k < 40; ++k) {
        const int at = fs / 2 + k * (fs * 3 / 8) + (k * 977) % (fs / 50);
        clicks.push_back(at);
        addImpulse(z, at, 1.0, fs);
    }
    const int ping = fs * 17;
    for (int i = 0; i < fs * 6 / 100; ++i) {
        const double ramp = std::min(1.0, std::min(i, fs * 6 / 100 - i) / (0.002 * fs));
        z[static_cast<std::size_t>(ping + i)] += 0.6 * ramp * std::sin(2.0 * kPi * 1400.0 * i / fs);
    }
    const int cw = fs * 18;
    for (int i = 0; i < fs; ++i) {
        const int dit = static_cast<int>(0.06 * fs);
        const bool on = (i / dit) % 2 == 0;
        const int inDit = i % dit;
        const double ramp = on ? std::min(1.0, std::min(inDit, dit - inDit) / (0.005 * fs)) : 0.0;
        z[static_cast<std::size_t>(cw + i)] += 0.4 * ramp * std::sin(2.0 * kPi * 700.0 * i / fs);
    }
    ImpulseBlanker nb2;
    nb2.configure(BlankerMode::Nb2, nb2Threshold(BlankerLevel::Normal), fs);
    std::vector<double> w = run(nb2, z);
    ImpulseBlanker automatic;
    automatic.configure(BlankerMode::Auto, nb2Threshold(BlankerLevel::Normal), fs);
    check(automatic.latencySamples() == delay, "Auto and manual use the same audio delay");
    check(run(automatic, x) == y && automatic.events() == 0 &&
              automatic.evaluations() * 20 < nb.evaluations(),
          "Auto passes clean input without running the envelope analysis on almost all blocks");
    automatic.reset();
    const auto previousEvaluations = automatic.evaluations();
    check(run(automatic, z) == w && automatic.events() == nb2.events() &&
              automatic.evaluations() - previousEvaluations < nb2.evaluations(),
          "Auto catches isolated clicks and protects pings and CW like manual NB2");
    double before = 0.0, after = 0.0;
    for (int at : clicks) {
        before += energy(z, at, at + fs / 400) - energy(x, at, at + fs / 400);
        after += energy(w, at + delay, at + delay + fs / 400) - energy(x, at, at + fs / 400);
    }
    const double reduction = 10.0 * std::log10(before / std::max(after, 1e-12));
    std::snprintf(text, sizeof(text), "%s: 40 clicks, %llu events, click energy above the background down by %.1f dB",
                  label, static_cast<unsigned long long>(nb2.events()), reduction);
    check(reduction > 15.0, text);
    const double pingIn = energy(z, ping, ping + fs * 6 / 100);
    const double pingOut = energy(w, ping + delay, ping + delay + fs * 6 / 100);
    std::snprintf(text, sizeof(text), "%s: 60 ms ping kept (%.3f of its energy)", label, pingOut / pingIn);
    check(std::fabs(pingOut / pingIn - 1.0) < 0.01, text);
    const double cwIn = energy(z, cw, cw + fs);
    const double cwOut = energy(w, cw + delay, cw + delay + fs);
    std::snprintf(text, sizeof(text), "%s: keyed CW kept (%.3f of its energy)", label, cwOut / cwIn);
    check(std::fabs(cwOut / cwIn - 1.0) < 0.01, text);
}

void testCarrierNotch()
{
    const int fs = CarrierNotch::kSampleRate;
    const int n = fs * 12;
    std::vector<double> x = noise(n, 0.01, 3);
    for (int i = 0; i < n; ++i) {
        x[static_cast<std::size_t>(i)] += 0.05 * std::sin(2.0 * kPi * 1234.0 * i / fs);
    }
    CarrierNotch zap;
    std::vector<double> y = x;
    zap.process(y.data(), n);
    const QVector<double> lines = zap.lines();
    char text[200];
    std::snprintf(text, sizeof(text), "carrier notch: %d line(s), first at %.1f Hz", lines.size(), lines.isEmpty() ? 0.0 : lines.first());
    check(lines.size() == 1 && std::fabs(lines.first() - 1234.0) < 3.0, text);
    // Carrier level in the last 4 s by correlation.
    auto level = [&](const std::vector<double> &v) {
        double c = 0.0, s = 0.0;
        for (int i = n - 4 * fs; i < n; ++i) {
            c += v[static_cast<std::size_t>(i)] * std::cos(2.0 * kPi * 1234.0 * i / fs);
            s += v[static_cast<std::size_t>(i)] * std::sin(2.0 * kPi * 1234.0 * i / fs);
        }
        return std::hypot(c, s);
    };
    const double drop = 20.0 * std::log10(level(x) / std::max(level(y), 1e-12));
    std::snprintf(text, sizeof(text), "carrier notch: carrier down by %.1f dB", drop);
    check(drop > 20.0, text);
    // Lines 50 Hz apart (MSK144 short messages): a signal, never notched.
    std::vector<double> comb = noise(n, 0.01, 4);
    for (int i = 0; i < n; ++i) {
        for (int k = 0; k < 5; ++k) {
            comb[static_cast<std::size_t>(i)] += 0.02 * std::sin(2.0 * kPi * (1300.0 + 50.0 * k) * i / fs + k);
        }
    }
    CarrierNotch zap2;
    zap2.process(comb.data(), n);
    std::snprintf(text, sizeof(text), "carrier notch: evenly spaced lines left alone (%d notches)", zap2.lineCount());
    check(zap2.lineCount() == 0, text);
}

void testStage()
{
    RxNoiseStage stage(true);
    Settings s;
    s.mode = BlankerMode::Off;
    setSettings(s);
    check(!stage.update(12000) && stage.latencyNs() == 0, "stage off: no delay");
    s.mode = BlankerMode::Nb2;
    setSettings(s);
    check(stage.update(12000) && stage.latencyNs() == 48000000, "stage on: 48 ms delay reported");
    check(!stage.update(12000), "stage: no change without new settings");
    s.mskCarrierNotch = true;
    setSettings(s);
    check(!stage.update(12000) && stage.active(), "stage: carrier notch adds no delay");
    s.mode = BlankerMode::Auto;
    setSettings(s);
    check(!stage.update(12000) && stage.latencyNs() == 48000000,
          "switching from manual NB2 to Auto keeps the audio timeline");
    s.mode = BlankerMode::Off;
    s.mskCarrierNotch = false;
    setSettings(s);
    check(stage.update(12000) && stage.latencyNs() == 0 && !stage.active(), "stage off again");
}

void testAutoTransitions()
{
    constexpr int fs = 12000;
    const double threshold = nb2Threshold(BlankerLevel::Normal);
    auto x = noise(fs * 3, 0.01, 13);
    addImpulse(x, fs + 181, 0.9, fs);
    ImpulseBlanker automatic;
    automatic.configure(BlankerMode::Auto, threshold, fs);
    auto y = x;
    automatic.process(y.data(), fs);
    const auto clean = automatic.evaluations();
    automatic.process(y.data() + fs, fs);
    const auto afterClick = automatic.evaluations();
    automatic.process(y.data() + 2 * fs, fs);
    check(automatic.events() == 1 && afterClick > clean,
          "Auto catches the first isolated click after quiet audio");
    check(automatic.evaluations() - afterClick < 4,
          "Auto stops running NB2 when the impulse has left the window");

    auto switched = x;
    const int split = fs + 99;
    automatic.reset();
    automatic.process(switched.data(), split);
    automatic.configure(BlankerMode::Nb2, threshold, fs);
    automatic.process(switched.data() + split, fs * 3 - split);
    check(switched == y, "manual/Auto setting change preserves buffered audio and alignment");
}
} // namespace

int main()
{
    testRate(12000, "NB2 12 kHz");
    testRate(48000, "NB2 48 kHz");
    testCarrierNotch();
    testStage();
    testAutoTransitions();
    std::printf(failures ? "%d FAILED\n" : "ALL PASSED\n", failures);
    return failures ? 1 : 0;
}
