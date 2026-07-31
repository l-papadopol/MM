#include "SelectedToneCwTracker.h"

#include "CwBayesianDecoder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <deque>
#include <limits>
#include <utility>
#include <vector>

namespace madmodem::cwskimmer {
namespace {

constexpr double kTwoPi = 6.283185307179586476925286766559;
constexpr int kPreRollMs = 900;
constexpr int kSessionReleaseMs = 2200;
constexpr int kDiagnosticPeriodMs = 5;
constexpr int kSpectrumSize = 512;
constexpr int kSpectrumHopMs = 256;
constexpr double kEpsilon = 1.0e-12;

inline double clampd(double v, double lo, double hi) {
  return std::max(lo, std::min(hi, v));
}


inline double logistic(double x) {
  if (x >= 0.0) {
    const double e = std::exp(-x);
    return 1.0 / (1.0 + e);
  }
  const double e = std::exp(x);
  return e / (1.0 + e);
}

std::string trimRolling(std::string value, std::size_t maxLen) {
  if (maxLen > 0U && value.size() > maxLen) {
    value.erase(0, value.size() - maxLen);
  }
  return value;
}

class BiquadLowPass {
public:
  void configure(double sampleRate, double cutoffHz, double q) {
    const double fs = std::max(1000.0, sampleRate);
    const double cutoff = clampd(cutoffHz, 2.0, 0.45 * fs);
    const double w0 = kTwoPi * cutoff / fs;
    const double alpha = std::sin(w0) / (2.0 * std::max(0.15, q));
    const double a0 = 1.0 + alpha;
    b0 = (1.0 - std::cos(w0)) * 0.5 / a0;
    b1 = (1.0 - std::cos(w0)) / a0;
    b2 = b0;
    a1 = -2.0 * std::cos(w0) / a0;
    a2 = (1.0 - alpha) / a0;
  }

  void reset() { x1 = x2 = y1 = y2 = 0.0; }

  double process(double x) {
    const double y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
    x2 = x1; x1 = x; y2 = y1; y1 = y;
    return y;
  }

private:
  double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
  double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;
};

void fft(std::vector<std::complex<double>>& a) {
  const std::size_t n = a.size();
  for (std::size_t i = 1, j = 0; i < n; ++i) {
    std::size_t bit = n >> 1U;
    for (; j & bit; bit >>= 1U) j ^= bit;
    j ^= bit;
    if (i < j) std::swap(a[i], a[j]);
  }
  for (std::size_t len = 2; len <= n; len <<= 1U) {
    const double angle = -kTwoPi / static_cast<double>(len);
    const std::complex<double> wlen(std::cos(angle), std::sin(angle));
    for (std::size_t i = 0; i < n; i += len) {
      std::complex<double> w(1.0, 0.0);
      for (std::size_t j = 0; j < len / 2U; ++j) {
        const auto u = a[i + j];
        const auto v = a[i + j + len / 2U] * w;
        a[i + j] = u + v;
        a[i + j + len / 2U] = u - v;
        w *= wlen;
      }
    }
  }
}

} // namespace

class SelectedToneCwTracker::Impl {
public:
  explicit Impl(SelectedToneCwConfig cfg)
      : config(std::move(cfg)), decoder(makeDecoderConfig()) {
    sanitize();
    resetAll();
  }

  CwBayesianDecoderConfig makeDecoderConfig() const {
    CwBayesianDecoderConfig cfg;
    cfg.initialWpm = config.initialWpm;
    cfg.autoWpm = config.autoWpm;
    cfg.beamWidth = 72;
    cfg.maxRollingText = config.maxRollingText;
    return cfg;
  }

  void sanitize() {
    config.toneHz = clampd(config.toneHz, 50.0, 5000.0);
    config.bandwidthHz = clampd(config.bandwidthHz, 30.0, 500.0);
    config.minSnrDb = clampd(config.minSnrDb, -3.0, 30.0);
    config.initialWpm = clampd(config.initialWpm, 5.0, 50.0);
    config.afcRangeHz = clampd(config.afcRangeHz, 3.0, 250.0);
    config.maxRollingText = std::max<std::size_t>(32, config.maxRollingText);
  }

  double selectedBandwidth() const {
    if (!config.autoBandwidth) return config.bandwidthHz;
    // A clean keying passband expressed directly in hertz. It remains wide
    // enough for 50 WPM edges but does not blindly inherit the UI maximum.
    // Preserve keying edges independently of the WPM hint.  A passband derived
    // too narrowly from a wrong hint rings through the first character and can
    // turn C into G before Auto-WPM has acquired the sender.
    const double timingBandwidth = 64.0 + 1.10 * clampd(lastReliableWpm, 5.0, 50.0);
    return clampd(std::min(config.bandwidthHz, timingBandwidth), 72.0, 160.0);
  }

  void configureDsp() {
    if (!(sampleRate > 1000.0)) return;
    effectiveBandwidth = selectedBandwidth();
    const double cutoff = clampd(0.5 * effectiveBandwidth, 12.0,
                                 std::min(250.0, 0.12 * sampleRate));
    i1.configure(sampleRate, cutoff, 0.541196100146197);
    i2.configure(sampleRate, cutoff, 1.306562964876377);
    q1.configure(sampleRate, cutoff, 0.541196100146197);
    q2.configure(sampleRate, cutoff, 1.306562964876377);
    controlSamplesTarget = sampleRate / 1000.0;
  }

  void resetAll() {
    decoder = CwBayesianDecoder(makeDecoderConfig());
    phase = 0.0;
    afcOffset = 0.0;
    smoothedFrequencyError = 0.0;
    havePreviousControl = false;
    previousControl = {};
    controlSum = {};
    rawControlSum = {};
    controlPower = 0.0;
    controlCount = 0;
    controlAccumulator = 0.0;
    slowCarrier = {};
    slowCarrierPower = 0.0;
    processedSamples = 0;
    noiseLog = std::numeric_limits<double>::quiet_NaN();
    markLog = std::numeric_limits<double>::quiet_NaN();
    noiseDeviation = 1.0;
    levelHistory.clear();
    levelUpdateCounter = 0;
    lastProbability = 0.5;
    lastKeyDown = false;
    lastStrongMarkAgoMs = 100000;
    strongRunMs = 0;
    quietMs = 0;
    transitionCount = 0;
    acquisitionElapsedMs = 0;
    sessionActive = false;
    sessionTrusted = false;
    pendingCommitted.clear();
    rolling.clear();
    partial.clear();
    lastReliableWpm = config.initialWpm;
    publicSnr = -99.0;
    publicConfidence = 0.0;
    preRoll.clear();
    rawSpectrum.clear();
    filteredSpectrum.clear();
    spectrumHopCounter = 0;
    lastInterfererHz = 0.0;
    lastInterfererConfidence = 0.0;
    lastCarrierProminenceDb = -99.0;
    lastCarrierPeakWidthHz = 0.0;
    lastCarrierToSecondDb = 0.0;
    qsbErasureActive = false;
    qsbErasureStart = 0.0;
    diagnosticCounter = 0;
    i1.reset(); i2.reset(); q1.reset(); q2.reset();
  }

  void ensureSampleRate(double rate) {
    if (std::abs(sampleRate - rate) < 1.0e-6) return;
    sampleRate = rate;
    resetAll();
    configureDsp();
  }

  void process(const float* samples, std::size_t count, double rate) {
    if (!samples || count == 0U || !(rate > 1000.0)) return;
    ensureSampleRate(rate);
    for (std::size_t n = 0; n < count; ++n) {
      const double tracked = config.toneHz + afcOffset;
      phase += kTwoPi * tracked / sampleRate;
      if (phase >= kTwoPi) phase -= kTwoPi * std::floor(phase / kTwoPi);
      const std::complex<double> oscillator(std::cos(phase), -std::sin(phase));
      const std::complex<double> raw = static_cast<double>(samples[n]) * oscillator;
      const std::complex<double> filtered(
          i2.process(i1.process(raw.real())),
          q2.process(q1.process(raw.imag())));

      rawControlSum += raw;
      controlSum += filtered;
      controlPower += std::norm(filtered);
      ++controlCount;
      controlAccumulator += 1.0;
      ++processedSamples;

      if (controlAccumulator + 1.0e-9 >= controlSamplesTarget) {
        emitControlSample();
        controlAccumulator -= controlSamplesTarget;
      }
    }
  }

  void startDecoderSession() {
    CwBayesianDecoderConfig decoderConfig = makeDecoderConfig();
    decoderConfig.initialWpm = clampd(lastReliableWpm, 5.0, 50.0);
    decoder = CwBayesianDecoder(decoderConfig);
    sessionActive = true;
    sessionTrusted = false;
    pendingCommitted.clear();
    transitionCount = 0;
    quietMs = 0;
    for (const auto& pre : preRoll) consumeDecoder(pre, false);
  }

  void emitControlSample() {
    if (controlCount <= 0) return;
    const double timestamp = static_cast<double>(processedSamples) / sampleRate;
    const std::complex<double> coherent = controlSum / static_cast<double>(controlCount);
    const std::complex<double> rawCoherent = rawControlSum / static_cast<double>(controlCount);
    const double totalPower = controlPower / static_cast<double>(controlCount);
    const double coherentPower = std::norm(coherent);
    const double residualPower = std::max(kEpsilon, totalPower - coherentPower);
    const double envelope = std::abs(coherent);
    const double rmsEnvelope = std::sqrt(std::max(kEpsilon, totalPower));
    // A real carrier remains phase coherent for tens of milliseconds.  Filtered
    // white noise also looks coherent over one millisecond, so it must not be
    // allowed to open a decoder session.
    constexpr double carrierAlpha = 0.045;
    slowCarrier += carrierAlpha * (coherent - slowCarrier);
    slowCarrierPower += carrierAlpha * (totalPower - slowCarrierPower);
    const double coherence = clampd(std::norm(slowCarrier) /
                                    std::max(kEpsilon, slowCarrierPower), 0.0, 1.0);
    const double snrDb = 10.0 * std::log10((coherentPower + kEpsilon) /
                                          (residualPower + kEpsilon));
    const double logEnvelope = std::log(std::max(kEpsilon, envelope));

    levelHistory.push_back(logEnvelope);
    while (levelHistory.size() > 2500U) levelHistory.pop_front();
    ++levelUpdateCounter;
    if (levelHistory.size() >= 160U && levelUpdateCounter >= 25) {
      levelUpdateCounter = 0;
      std::vector<double> levels(levelHistory.begin(), levelHistory.end());
      const std::size_t lowIndex = levels.size() / 5U;
      const std::size_t highIndex = (levels.size() * 17U) / 20U;
      std::nth_element(levels.begin(), levels.begin() + static_cast<std::ptrdiff_t>(lowIndex), levels.end());
      const double robustLow = levels[lowIndex];
      std::nth_element(levels.begin(), levels.begin() + static_cast<std::ptrdiff_t>(highIndex), levels.end());
      const double robustHigh = levels[highIndex];
      if (robustHigh > robustLow + std::log(1.45)) {
        const double alpha = levelHistory.size() < 600U ? 0.45 : 0.18;
        if (!std::isfinite(noiseLog)) noiseLog = robustLow;
        else noiseLog += alpha * (robustLow - noiseLog);
        if (!std::isfinite(markLog)) markLog = robustHigh;
        else markLog += alpha * (robustHigh - markLog);
      }
    }

    if (!std::isfinite(noiseLog)) {
      noiseLog = logEnvelope;
      markLog = logEnvelope + std::log(4.0);
      noiseDeviation = 0.5;
    }

    // A true CW threshold must follow the MARK level as well as the noise
    // floor. Using only a geometric noise/mark midpoint makes digital silence
    // dominate the model and turns the filter tail into a permanent MARK.
    if (levelHistory.size() < 300U && coherence > 0.68 &&
        snrDb > config.minSnrDb + 3.0 &&
        logEnvelope > noiseLog + std::log(4.0) &&
        markLog < logEnvelope - std::log(2.0)) {
      markLog = logEnvelope;
    }
    const double separation = std::max(std::log(2.0), markLog - noiseLog);
    const double midpoint = std::max(noiseLog + 0.62 * separation,
                                     markLog + std::log(0.24));
    const double amplitudeProbability = logistic((logEnvelope - midpoint) / 0.24);
    // The coherent/residual ratio of a one-millisecond exact-tone projection is
    // not a MARK probability: it stays high in quiet gaps because narrow-band
    // noise is coherent over such a short interval.  Let the independently
    // tracked MARK/noise amplitudes decide state; SNR and coherence only say
    // how much confidence to place in that observation.
    double pMark = clampd(amplitudeProbability, 0.001, 0.999);

    const double separationDb = 8.685889638 * separation;
    const double levelEvidence = clampd(separationDb / 14.0, 0.0, 1.0);
    const double decisionEvidence = std::abs(2.0 * pMark - 1.0);
    double observationConfidence = clampd(0.58 * decisionEvidence +
                                          0.30 * levelEvidence +
                                          0.12 * coherence, 0.0, 1.0);

    const bool strongMark = pMark > 0.74 && observationConfidence > 0.44 &&
                            snrDb > config.minSnrDb - 2.0;
    if (strongMark) {
      ++strongRunMs;
      lastStrongMarkAgoMs = 0;
      quietMs = 0;
      const double markAlpha = sessionActive ? 0.018 : 0.035;
      markLog += markAlpha * (logEnvelope - markLog);
    } else {
      strongRunMs = 0;
      ++lastStrongMarkAgoMs;
      ++quietMs;
    }

    // Separate noise and MARK models. Long dashes cannot pull the noise floor
    // upward, and a fade cannot immediately become a certain Morse SPACE.
    if (!strongMark && (pMark < 0.42 || !sessionActive)) {
      const double delta = logEnvelope - noiseLog;
      const double alpha = levelHistory.size() < 300U
          ? (delta < 0.0 ? 0.035 : 0.0025)
          : (delta < 0.0 ? 0.004 : 0.0004);
      noiseLog += alpha * delta;
      noiseDeviation += 0.02 * (std::abs(delta) - noiseDeviation);
    }
    if (markLog < noiseLog + std::log(1.8)) markLog = noiseLog + std::log(1.8);

    const int erasureWindowMs = static_cast<int>(clampd(0.85 * 1200.0 /
        std::max(5.0, lastReliableWpm), 20.0, 180.0));
    const bool possibleQsb = sessionActive && lastStrongMarkAgoMs <= erasureWindowMs &&
                             pMark < 0.45 && observationConfidence < 0.52;
    if (possibleQsb) {
      observationConfidence = std::min(observationConfidence, 0.28);
      pMark = 0.5 + observationConfidence * (pMark - 0.5);
      if (!qsbErasureActive) qsbErasureStart = timestamp;
      qsbErasureActive = true;
    } else if (qsbErasureActive && strongMark) {
      qsbErasureActive = false;
    } else if (lastStrongMarkAgoMs > erasureWindowMs) {
      qsbErasureActive = false;
    }

    if (strongMark || acquisitionElapsedMs > 0) ++acquisitionElapsedMs;

    CwSoftObservation observation;
    observation.timestampSec = timestamp;
    observation.markProbability = pMark;
    observation.confidence = observationConfidence;
    observation.snrDb = snrDb;
    observation.coherence = coherence;
    preRoll.push_back(observation);
    while (preRoll.size() > static_cast<std::size_t>(kPreRollMs)) preRoll.pop_front();

    const bool binaryMark = pMark >= 0.5;
    const bool rising = binaryMark && !lastKeyDown;
    const bool falling = !binaryMark && lastKeyDown;
    if (rising || falling) ++transitionCount;
    lastKeyDown = binaryMark;

    const bool carrierPresent = lastCarrierProminenceDb >= 8.0 ||
                                (pMark >= 0.84 && observationConfidence >= 0.54 &&
                                 strongRunMs >= 28);
    const bool acquisitionReady = acquisitionElapsedMs >= 80 || transitionCount >= 2;
    if (!sessionActive && strongRunMs >= 18 && carrierPresent && acquisitionReady) {
      startDecoderSession();
    } else if (sessionActive) {
      consumeDecoder(observation, true);
    }

    const int dynamicReleaseMs = static_cast<int>(std::max(
        static_cast<double>(kSessionReleaseMs),
        9.0 * 1200.0 / std::max(5.0, lastReliableWpm)));
    const bool interTransmissionGap = sessionActive && !lastKeyDown &&
                                      quietMs > dynamicReleaseMs;
    if (sessionActive && interTransmissionGap) {
      const auto flushed = decoder.flush(timestamp);
      handleDecoderResult(flushed, timestamp, true);
      sessionActive = false;
      sessionTrusted = false;
      pendingCommitted.clear();
      acquisitionElapsedMs = 0;
      decoder.reset();
    }

    if (config.afcEnabled && strongMark && coherence > 0.12 && havePreviousControl) {
      const std::complex<double> product = coherent * std::conj(previousControl);
      const double errorHz = std::arg(product) * 1000.0 / kTwoPi;
      if (std::abs(errorHz) <= std::max(5.0, 1.5 * config.afcRangeHz)) {
        smoothedFrequencyError += 0.08 * (errorHz - smoothedFrequencyError);
        afcOffset = clampd(afcOffset + 0.025 * smoothedFrequencyError,
                           -config.afcRangeHz, config.afcRangeHz);
      }
    }
    if (envelope > 1.0e-9) {
      previousControl = coherent;
      havePreviousControl = true;
    }

    publicSnr += 0.08 * (snrDb - publicSnr);
    publicConfidence += 0.08 * (observationConfidence - publicConfidence);

    rawSpectrum.push_back(rawCoherent);
    filteredSpectrum.push_back(coherent);
    while (rawSpectrum.size() > kSpectrumSize) rawSpectrum.pop_front();
    while (filteredSpectrum.size() > kSpectrumSize) filteredSpectrum.pop_front();
    ++spectrumHopCounter;
    if (spectrumHopCounter >= kSpectrumHopMs && rawSpectrum.size() == kSpectrumSize) {
      spectrumHopCounter = 0;
      emitSpectrum(timestamp);
    }

    ++diagnosticCounter;
    if (diagnosticCounter >= kDiagnosticPeriodMs) {
      diagnosticCounter = 0;
      emitDiagnostic(timestamp, rmsEnvelope, envelope, pMark, observationConfidence,
                     snrDb, coherence);
    }

    lastProbability = pMark;
    controlSum = {};
    rawControlSum = {};
    controlPower = 0.0;
    controlCount = 0;
  }

  void consumeDecoder(const CwSoftObservation& observation, bool allowCallback) {
    auto result = decoder.process(observation);
    if (!result.committedText.empty()) pendingCommitted += result.committedText;
    const int nonSpace = static_cast<int>(std::count_if(
        pendingCommitted.begin(), pendingCommitted.end(), [](char c) { return c != ' '; }));
    const bool plausibleEdgeDensity = transitionCount <= 16 * nonSpace + 16;
    if (!sessionTrusted && nonSpace >= 1 && plausibleEdgeDensity &&
        lastCarrierProminenceDb >= 5.5 &&
        std::max(publicConfidence, result.confidence) >= 0.58) {
      sessionTrusted = true;
    }
    if (sessionTrusted && !pendingCommitted.empty() && allowCallback) {
      result.committedText = pendingCommitted;
      pendingCommitted.clear();
      handleDecoderResult(result, observation.timestampSec, false);
    } else {
      partial = result.partialText;
    }
  }

  void handleDecoderResult(CwBayesianDecoderResult result, double timestamp, bool flushing) {
    if (!sessionTrusted && flushing) return;
    if (!pendingCommitted.empty()) {
      result.committedText = pendingCommitted + result.committedText;
      pendingCommitted.clear();
    }
    if (!result.committedText.empty()) {
      rolling = trimRolling(rolling + result.committedText, config.maxRollingText);
    }
    partial = result.partialText;
    if (sessionTrusted && !result.committedText.empty() &&
        result.confidence > 0.20 && result.wpm >= 5.0 && result.wpm <= 50.0) {
      lastReliableWpm = result.wpm;
      configureDsp();
    }
    if (!callback || (result.committedText.empty() && !flushing)) return;

    SelectedToneCwEvent event;
    event.timestampSec = timestamp;
    event.toneHz = config.toneHz;
    event.trackedToneHz = config.toneHz + afcOffset;
    event.frequencyErrorHz = smoothedFrequencyError;
    event.requestedBandwidthHz = config.bandwidthHz;
    event.effectiveBandwidthHz = effectiveBandwidth;
    event.acquisitionBandwidthHz = config.bandwidthHz + 2.0 * config.afcRangeHz;
    event.snrDb = publicSnr;
    event.confidence = std::max(publicConfidence, result.confidence);
    event.wpm = lastReliableWpm;
    event.trackingConfirmed = sessionActive || flushing;
    event.trackingState = sessionActive ? "TRACK" : "HOLD";
    event.committedText = result.committedText;
    event.partialText = partial;
    event.rollingText = rolling;
    callback(event);
  }

  void emitDiagnostic(double timestamp, double acquisitionEnvelope,
                      double filteredEnvelope, double pMark, double confidence,
                      double snrDb, double coherence) {
    if (!diagnosticCallback) return;
    const double sep = std::max(std::log(2.0), markLog - noiseLog);
    SelectedToneCwDiagnosticSample d;
    d.timestampSec = timestamp;
    d.acquisitionEnvelope = acquisitionEnvelope;
    d.filteredEnvelope = filteredEnvelope;
    d.signalLevel = std::exp(markLog);
    d.thresholdLow = std::exp(noiseLog + 0.32 * sep);
    d.thresholdHigh = std::exp(noiseLog + 0.54 * sep);
    d.markProbability = pMark;
    d.qsbErasure = qsbErasureActive;
    d.qsbErasureStartSec = qsbErasureStart;
    d.qsbErasureEndSec = qsbErasureActive ? timestamp : qsbErasureStart;
    d.keyDown = lastKeyDown;
    d.markerHz = config.toneHz;
    d.trackedHz = config.toneHz + afcOffset;
    d.snrDb = snrDb;
    d.requestedBandwidthHz = config.bandwidthHz;
    d.effectiveBandwidthHz = effectiveBandwidth;
    d.wpm = lastReliableWpm;
    d.lockQuality = confidence;
    d.trackingConfirmed = sessionActive;
    d.carrierProminenceDb = lastCarrierProminenceDb;
    d.coherentSnrDb = snrDb;
    d.coherence = coherence;
    diagnosticCallback(d);
  }

  void emitSpectrum(double timestamp) {
    std::vector<std::complex<double>> input(rawSpectrum.begin(), rawSpectrum.end());
    std::vector<std::complex<double>> output(filteredSpectrum.begin(), filteredSpectrum.end());
    for (int i = 0; i < kSpectrumSize; ++i) {
      const double w = 0.5 - 0.5 * std::cos(kTwoPi * static_cast<double>(i) /
                                           static_cast<double>(kSpectrumSize - 1));
      input[static_cast<std::size_t>(i)] *= w;
      output[static_cast<std::size_t>(i)] *= w;
    }
    fft(input);
    fft(output);

    SelectedToneCwSpectrumFrame frame;
    frame.timestampSec = timestamp;
    frame.markerHz = config.toneHz;
    frame.trackedHz = config.toneHz + afcOffset;
    frame.effectiveBandwidthHz = effectiveBandwidth;
    frame.offsetsHz.reserve(kSpectrumSize);
    frame.inputPsdDb.reserve(kSpectrumSize);
    frame.filteredPsdDb.reserve(kSpectrumSize);
    frame.theoreticalResponseDb.reserve(kSpectrumSize);

    double mainPeak = -200.0;
    double secondPeak = -200.0;
    double secondOffset = 0.0;
    int mainIndex = 0;
    for (int display = -kSpectrumSize / 2; display < kSpectrumSize / 2; ++display) {
      const int index = display < 0 ? display + kSpectrumSize : display;
      const double offset = static_cast<double>(display) * 1000.0 /
                            static_cast<double>(kSpectrumSize);
      const double inDb = 10.0 * std::log10(std::norm(input[static_cast<std::size_t>(index)]) + kEpsilon);
      const double outDb = 10.0 * std::log10(std::norm(output[static_cast<std::size_t>(index)]) + kEpsilon);
      frame.offsetsHz.push_back(static_cast<float>(offset));
      frame.inputPsdDb.push_back(static_cast<float>(inDb));
      frame.filteredPsdDb.push_back(static_cast<float>(outDb));
      const double cutoff = std::max(1.0, 0.5 * effectiveBandwidth);
      const double response = -10.0 * std::log10(1.0 + std::pow(std::abs(offset) / cutoff, 8.0));
      frame.theoreticalResponseDb.push_back(static_cast<float>(response));
      if (std::abs(offset) <= 12.0 && outDb > mainPeak) {
        mainPeak = outDb;
        mainIndex = display;
      }
    }

    for (int display = -kSpectrumSize / 2; display < kSpectrumSize / 2; ++display) {
      const double offset = static_cast<double>(display) * 1000.0 /
                            static_cast<double>(kSpectrumSize);
      if (std::abs(offset) < 15.0 || std::abs(offset) > 250.0) continue;
      const int index = display < 0 ? display + kSpectrumSize : display;
      const double value = 10.0 * std::log10(std::norm(input[static_cast<std::size_t>(index)]) + kEpsilon);
      if (value > secondPeak) {
        secondPeak = value;
        secondOffset = offset;
      }
    }

    std::vector<double> floorValues;
    floorValues.reserve(kSpectrumSize);
    for (std::size_t i = 0; i < frame.inputPsdDb.size(); ++i) {
      if (std::abs(frame.offsetsHz[i]) > 20.0) floorValues.push_back(frame.inputPsdDb[i]);
    }
    std::nth_element(floorValues.begin(), floorValues.begin() + floorValues.size() / 2,
                     floorValues.end());
    const double floorDb = floorValues[floorValues.size() / 2];
    lastCarrierProminenceDb = mainPeak - floorDb;
    lastCarrierToSecondDb = mainPeak - secondPeak;
    lastInterfererHz = config.toneHz + afcOffset + secondOffset;
    lastInterfererConfidence = clampd((secondPeak - floorDb - 4.0) / 12.0, 0.0, 1.0);

    int widthBins = 1;
    const double halfPower = mainPeak - 3.0;
    for (int delta = 1; delta < 30; ++delta) {
      bool any = false;
      for (int sign : {-1, 1}) {
        const int display = mainIndex + sign * delta;
        if (display < -kSpectrumSize / 2 || display >= kSpectrumSize / 2) continue;
        const int index = display < 0 ? display + kSpectrumSize : display;
        const double value = 10.0 * std::log10(std::norm(output[static_cast<std::size_t>(index)]) + kEpsilon);
        if (value >= halfPower) any = true;
      }
      if (!any) break;
      widthBins = 2 * delta + 1;
    }
    lastCarrierPeakWidthHz = widthBins * 1000.0 / static_cast<double>(kSpectrumSize);

    frame.interfererHz = lastInterfererHz;
    frame.interfererConfidence = lastInterfererConfidence;
    frame.carrierProminenceDb = lastCarrierProminenceDb;
    frame.carrierPeakWidthHz = lastCarrierPeakWidthHz;
    frame.carrierToSecondDb = lastCarrierToSecondDb;
    if (spectrumCallback) spectrumCallback(frame);
  }

  void flush() {
    if (!sessionActive) return;
    const double timestamp = sampleRate > 0.0 ? static_cast<double>(processedSamples) / sampleRate : 0.0;
    auto result = decoder.flush(timestamp);
    handleDecoderResult(result, timestamp, true);
    sessionActive = false;
  }

  SelectedToneCwConfig config;
  SelectedToneCwCallback callback;
  SelectedToneCwDiagnosticCallback diagnosticCallback;
  SelectedToneCwSpectrumCallback spectrumCallback;
  CwBayesianDecoder decoder;

  double sampleRate = 0.0;
  double effectiveBandwidth = 120.0;
  double controlSamplesTarget = 48.0;
  double phase = 0.0;
  double afcOffset = 0.0;
  double smoothedFrequencyError = 0.0;
  BiquadLowPass i1, i2, q1, q2;

  std::complex<double> controlSum{};
  std::complex<double> rawControlSum{};
  double controlPower = 0.0;
  int controlCount = 0;
  double controlAccumulator = 0.0;
  std::uint64_t processedSamples = 0;
  std::complex<double> previousControl{};
  bool havePreviousControl = false;
  std::complex<double> slowCarrier{};
  double slowCarrierPower = 0.0;

  double noiseLog = std::numeric_limits<double>::quiet_NaN();
  double markLog = std::numeric_limits<double>::quiet_NaN();
  double noiseDeviation = 1.0;
  std::deque<double> levelHistory;
  int levelUpdateCounter = 0;
  double lastProbability = 0.5;
  bool lastKeyDown = false;
  int lastStrongMarkAgoMs = 100000;
  int strongRunMs = 0;
  int quietMs = 0;
  int transitionCount = 0;
  int acquisitionElapsedMs = 0;
  bool sessionActive = false;
  bool sessionTrusted = false;
  std::string pendingCommitted;
  std::string rolling;
  std::string partial;
  double lastReliableWpm = 20.0;
  double publicSnr = -99.0;
  double publicConfidence = 0.0;
  std::deque<CwSoftObservation> preRoll;

  std::deque<std::complex<double>> rawSpectrum;
  std::deque<std::complex<double>> filteredSpectrum;
  int spectrumHopCounter = 0;
  double lastInterfererHz = 0.0;
  double lastInterfererConfidence = 0.0;
  double lastCarrierProminenceDb = -99.0;
  double lastCarrierPeakWidthHz = 0.0;
  double lastCarrierToSecondDb = 0.0;

  bool qsbErasureActive = false;
  double qsbErasureStart = 0.0;
  int diagnosticCounter = 0;
};

SelectedToneCwTracker::SelectedToneCwTracker(SelectedToneCwConfig config)
    : m_impl(std::make_unique<Impl>(std::move(config))) {}
SelectedToneCwTracker::~SelectedToneCwTracker() = default;
SelectedToneCwTracker::SelectedToneCwTracker(SelectedToneCwTracker&&) noexcept = default;
SelectedToneCwTracker& SelectedToneCwTracker::operator=(SelectedToneCwTracker&&) noexcept = default;

void SelectedToneCwTracker::setCallback(SelectedToneCwCallback callback) {
  m_impl->callback = std::move(callback);
}
void SelectedToneCwTracker::setDiagnosticCallback(SelectedToneCwDiagnosticCallback callback) {
  m_impl->diagnosticCallback = std::move(callback);
}
void SelectedToneCwTracker::setSpectrumCallback(SelectedToneCwSpectrumCallback callback) {
  m_impl->spectrumCallback = std::move(callback);
}
void SelectedToneCwTracker::setToneHz(double toneHz) {
  m_impl->config.toneHz = clampd(toneHz, 50.0, 5000.0);
  m_impl->resetAll();
  m_impl->configureDsp();
}
void SelectedToneCwTracker::setBandwidthHz(double bandwidthHz) {
  m_impl->config.bandwidthHz = clampd(bandwidthHz, 30.0, 500.0);
  m_impl->configureDsp();
}
void SelectedToneCwTracker::setMinSnrDb(double minSnrDb) {
  m_impl->config.minSnrDb = clampd(minSnrDb, -3.0, 30.0);
}
void SelectedToneCwTracker::setWpmHint(double wpm) {
  m_impl->config.initialWpm = clampd(wpm, 5.0, 50.0);
  m_impl->decoder.setWpmHint(m_impl->config.initialWpm);
  if (!m_impl->config.autoWpm) m_impl->lastReliableWpm = m_impl->config.initialWpm;
  m_impl->configureDsp();
}
void SelectedToneCwTracker::setAutoWpm(bool enabled) {
  m_impl->config.autoWpm = enabled;
  m_impl->decoder.setAutoWpm(enabled);
}
void SelectedToneCwTracker::setAfcEnabled(bool enabled) {
  m_impl->config.afcEnabled = enabled;
  if (!enabled) m_impl->afcOffset = 0.0;
}
void SelectedToneCwTracker::setAutoBandwidth(bool enabled) {
  m_impl->config.autoBandwidth = enabled;
  m_impl->configureDsp();
}
void SelectedToneCwTracker::setAfcRangeHz(double rangeHz) {
  m_impl->config.afcRangeHz = clampd(rangeHz, 3.0, 250.0);
  m_impl->afcOffset = clampd(m_impl->afcOffset, -m_impl->config.afcRangeHz,
                             m_impl->config.afcRangeHz);
}
const SelectedToneCwConfig& SelectedToneCwTracker::config() const { return m_impl->config; }
void SelectedToneCwTracker::reset() { m_impl->resetAll(); m_impl->configureDsp(); }
void SelectedToneCwTracker::processFloatMono(const float* samples, std::size_t count,
                                              double sourceSampleRate) {
  m_impl->process(samples, count, sourceSampleRate);
}
void SelectedToneCwTracker::flush() { m_impl->flush(); }
double SelectedToneCwTracker::snrDb() const { return m_impl->publicSnr; }
double SelectedToneCwTracker::confidence() const { return m_impl->publicConfidence; }
double SelectedToneCwTracker::wpm() const { return clampd(m_impl->lastReliableWpm, 5.0, 50.0); }
double SelectedToneCwTracker::trackedToneHz() const { return m_impl->config.toneHz + m_impl->afcOffset; }
double SelectedToneCwTracker::frequencyErrorHz() const { return m_impl->smoothedFrequencyError; }
double SelectedToneCwTracker::effectiveBandwidthHz() const { return m_impl->effectiveBandwidth; }
double SelectedToneCwTracker::acquisitionBandwidthHz() const {
  return m_impl->config.bandwidthHz + 2.0 * m_impl->config.afcRangeHz;
}
std::string SelectedToneCwTracker::trackingState() const {
  if (m_impl->sessionActive) return m_impl->sessionTrusted ? "TRACK" : "ACQUIRE";
  return m_impl->quietMs < kSessionReleaseMs ? "HOLD" : "IDLE";
}
const std::string& SelectedToneCwTracker::rollingText() const { return m_impl->rolling; }
const std::string& SelectedToneCwTracker::partialText() const { return m_impl->partial; }

} // namespace madmodem::cwskimmer
