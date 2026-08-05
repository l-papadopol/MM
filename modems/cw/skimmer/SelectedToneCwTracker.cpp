#include "SelectedToneCwTracker.h"

#include "CwCarrierDiscriminator.h"
#include "CwRelativeTimingTask.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <deque>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

namespace madmodem::cwskimmer {
namespace {

constexpr double kTwoPi = 6.283185307179586476925286766559;
constexpr int kDiagnosticPeriodMs = 5;
constexpr int kSpectrumSize = 512;
constexpr int kSpectrumHopMs = 256;
constexpr double kEpsilon = 1.0e-12;

inline double clampd(double v, double lo, double hi) {
  return std::max(lo, std::min(hi, v));
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


class SlidingComplexAverage {
public:
  void configure(double sampleRate, double windowMs) {
    const double fs = std::max(1000.0, sampleRate);
    const std::size_t count = static_cast<std::size_t>(std::lround(
        clampd(windowMs, 2.0, 40.0) * fs / 1000.0));
    samples.assign(std::max<std::size_t>(8U, count), {});
    reset();
  }

  void reset() {
    std::fill(samples.begin(), samples.end(), std::complex<double>{});
    sum = {};
    index = 0U;
    filled = 0U;
  }

  std::complex<double> process(const std::complex<double>& value) {
    if (samples.empty()) return value;
    sum -= samples[index];
    samples[index] = value;
    sum += value;
    index = (index + 1U) % samples.size();
    filled = std::min(samples.size(), filled + 1U);
    return sum / static_cast<double>(std::max<std::size_t>(1U, filled));
  }

private:
  std::vector<std::complex<double>> samples;
  std::complex<double> sum{};
  std::size_t index = 0U;
  std::size_t filled = 0U;
};


class ComplexLaneSeparator {
public:
  struct Lane {
    double toneHz = 0.0;
    double confidence = 0.0;
    double phase = 0.0;
    SlidingComplexAverage average;
  };

  void setInterferers(const std::vector<SelectedToneCwInterferer>& requested,
                      double targetHz) {
    std::vector<SelectedToneCwInterferer> cleaned;
    cleaned.reserve(requested.size());
    for (const auto& item : requested) {
      if (!std::isfinite(item.toneHz) || !std::isfinite(item.confidence)) continue;
      if (item.toneHz < 50.0 || item.toneHz > 5000.0) continue;
      if (std::abs(item.toneHz - targetHz) < 12.0) continue;
      SelectedToneCwInterferer copy = item;
      copy.confidence = clampd(copy.confidence, 0.0, 1.0);
      bool duplicate = false;
      for (auto& existing : cleaned) {
        if (std::abs(existing.toneHz - copy.toneHz) <= 5.0) {
          if (copy.confidence > existing.confidence) existing = copy;
          duplicate = true;
          break;
        }
      }
      if (!duplicate) cleaned.push_back(copy);
    }
    std::sort(cleaned.begin(), cleaned.end(), [targetHz](const auto& a, const auto& b) {
      const double scoreA = 2.0 * a.confidence - 0.0015 * std::abs(a.toneHz - targetHz);
      const double scoreB = 2.0 * b.confidence - 0.0015 * std::abs(b.toneHz - targetHz);
      return scoreA > scoreB;
    });
    if (cleaned.size() > 4U) cleaned.resize(4U);

    std::vector<Lane> next;
    next.reserve(cleaned.size());
    std::vector<bool> used(lanes.size(), false);
    for (const auto& item : cleaned) {
      std::size_t best = lanes.size();
      double bestDistance = 6.0;
      for (std::size_t i = 0; i < lanes.size(); ++i) {
        if (used[i]) continue;
        const double distance = std::abs(lanes[i].toneHz - item.toneHz);
        if (distance < bestDistance) {
          bestDistance = distance;
          best = i;
        }
      }
      if (best < lanes.size()) {
        used[best] = true;
        Lane lane = std::move(lanes[best]);
        lane.toneHz = item.toneHz;
        lane.confidence = item.confidence;
        next.push_back(std::move(lane));
      } else {
        Lane lane;
        lane.toneHz = item.toneHz;
        lane.confidence = item.confidence;
        if (sampleRate > 1000.0) lane.average.configure(sampleRate, windowMs);
        next.push_back(std::move(lane));
      }
    }
    lanes = std::move(next);
    configuredTargetHz = targetHz;
  }

  void configure(double rate, double targetHz) {
    sampleRate = rate;
    configuredTargetHz = targetHz;
    configureWindow(targetHz);
  }

  void reset() {
    centerAverage.reset();
    for (auto& lane : lanes) {
      lane.phase = 0.0;
      lane.average.reset();
    }
  }

  std::complex<double> process(const std::complex<double>& raw,
                               double trackedHz,
                               double rate) {
    if (!(rate > 1000.0)) return raw;
    if (std::abs(sampleRate - rate) > 1.0e-6 ||
        std::abs(configuredTargetHz - trackedHz) > 2.0) {
      sampleRate = rate;
      configuredTargetHz = trackedHz;
      configureWindow(trackedHz);
    }

    std::vector<Lane*> active;
    active.reserve(lanes.size());
    double minimumSeparation = 1000.0;
    for (auto& lane : lanes) {
      const double separation = std::abs(lane.toneHz - trackedHz);
      // Below roughly 55 Hz the short edge-preserving basis becomes poorly
      // conditioned and an aggressive inversion damages the wanted keying more
      // than it removes the neighbour.  Such close lanes are handled by the AFC
      // guard below; coherent cancellation starts only where it is numerically
      // trustworthy in realtime.
      if (lane.confidence < 0.35 || separation < 55.0 || separation > 260.0) continue;
      active.push_back(&lane);
      minimumSeparation = std::min(minimumSeparation, separation);
    }

    const std::complex<double> centre = centerAverage.process(raw);
    if (active.empty()) return centre;

    const std::size_t count = active.size() + 1U;
    std::vector<double> offsets(count, 0.0);
    std::vector<std::complex<double>> rotations(count, {1.0, 0.0});
    std::vector<std::complex<double>> observations(count, std::complex<double>{});
    observations[0] = centre;

    for (std::size_t j = 1U; j < count; ++j) {
      Lane& lane = *active[j - 1U];
      offsets[j] = lane.toneHz - trackedHz;
      lane.phase += kTwoPi * offsets[j] / sampleRate;
      if (lane.phase >= kTwoPi || lane.phase <= -kTwoPi)
        lane.phase -= kTwoPi * std::floor(lane.phase / kTwoPi);
      rotations[j] = std::complex<double>(std::cos(lane.phase), std::sin(lane.phase));
      const std::complex<double> baseband =
          lane.average.process(raw * std::conj(rotations[j]));
      observations[j] = baseband * rotations[j];
    }

    std::vector<std::vector<std::complex<double>>> matrix(
        count, std::vector<std::complex<double>>(count, std::complex<double>{}));
    for (std::size_t row = 0; row < count; ++row) {
      for (std::size_t column = 0; column < count; ++column) {
        matrix[row][column] = movingAverageResponse(offsets[column] - offsets[row]);
      }
    }

    // Regularize only the multi-lane inversion. The value grows for very close
    // carriers, where a finite edge-safe window cannot make the bases fully
    // orthogonal, but remains small enough to retain a real one-dit gap.
    const double closeFactor = clampd((45.0 - minimumSeparation) / 30.0, 0.0, 1.0);
    const double regularization = 0.004 + 0.022 * closeFactor;
    for (std::size_t i = 0; i < count; ++i) matrix[i][i] += regularization;

    std::vector<std::complex<double>> solution;
    if (!solve(matrix, observations, solution) || solution.empty()) return centre;
    const double allowed = 8.0 * (std::abs(centre) + 1.0e-5);
    if (!std::isfinite(solution[0].real()) || !std::isfinite(solution[0].imag()) ||
        std::abs(solution[0]) > allowed) {
      return centre;
    }
    return solution[0];
  }

  double nearestInterfererSeparation(double targetHz) const {
    double nearest = std::numeric_limits<double>::infinity();
    for (const auto& lane : lanes) {
      if (lane.confidence < 0.35) continue;
      const double separation = std::abs(lane.toneHz - targetHz);
      if (separation >= 12.0 && separation <= 260.0)
        nearest = std::min(nearest, separation);
    }
    return nearest;
  }

private:
  void configureWindow(double targetHz) {
    double nearest = 1000.0;
    for (const auto& lane : lanes) {
      if (lane.confidence < 0.35) continue;
      const double separation = std::abs(lane.toneHz - targetHz);
      if (separation >= 55.0 && separation <= 260.0)
        nearest = std::min(nearest, separation);
    }
    const double desiredWindowMs = 8.0;
    if (std::abs(windowMs - desiredWindowMs) < 0.5 && windowSamples > 0U) return;
    windowMs = desiredWindowMs;
    windowSamples = static_cast<std::size_t>(std::max<long long>(8,
        std::llround(windowMs * sampleRate / 1000.0)));
    centerAverage.configure(sampleRate, windowMs);
    for (auto& lane : lanes) lane.average.configure(sampleRate, windowMs);
  }

  std::complex<double> movingAverageResponse(double offsetHz) const {
    if (std::abs(offsetHz) < 1.0e-12 || windowSamples <= 1U) return {1.0, 0.0};
    const double omega = kTwoPi * offsetHz / sampleRate;
    const double denominator = std::sin(0.5 * omega);
    if (std::abs(denominator) < 1.0e-12) return {1.0, 0.0};
    const double magnitude = std::sin(0.5 * omega * static_cast<double>(windowSamples)) /
        (static_cast<double>(windowSamples) * denominator);
    const double phase = -0.5 * omega * static_cast<double>(windowSamples - 1U);
    return std::polar(magnitude, phase);
  }

  static bool solve(std::vector<std::vector<std::complex<double>>> matrix,
                    std::vector<std::complex<double>> rhs,
                    std::vector<std::complex<double>>& solution) {
    const std::size_t n = rhs.size();
    if (matrix.size() != n) return false;
    for (std::size_t column = 0; column < n; ++column) {
      std::size_t pivot = column;
      double pivotMagnitude = std::abs(matrix[pivot][column]);
      for (std::size_t row = column + 1U; row < n; ++row) {
        const double candidate = std::abs(matrix[row][column]);
        if (candidate > pivotMagnitude) {
          pivot = row;
          pivotMagnitude = candidate;
        }
      }
      if (pivotMagnitude < 1.0e-8 || !std::isfinite(pivotMagnitude)) return false;
      if (pivot != column) {
        std::swap(matrix[pivot], matrix[column]);
        std::swap(rhs[pivot], rhs[column]);
      }
      const std::complex<double> divisor = matrix[column][column];
      for (std::size_t j = column; j < n; ++j) matrix[column][j] /= divisor;
      rhs[column] /= divisor;
      for (std::size_t row = 0; row < n; ++row) {
        if (row == column) continue;
        const std::complex<double> factor = matrix[row][column];
        if (std::abs(factor) < 1.0e-15) continue;
        for (std::size_t j = column; j < n; ++j)
          matrix[row][j] -= factor * matrix[column][j];
        rhs[row] -= factor * rhs[column];
      }
    }
    solution = std::move(rhs);
    return true;
  }

  double sampleRate = 0.0;
  double configuredTargetHz = 0.0;
  double windowMs = 8.0;
  std::size_t windowSamples = 0U;
  SlidingComplexAverage centerAverage;
  std::vector<Lane> lanes;
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
      : config(std::move(cfg)),
        discriminator(makeDiscriminatorConfig()),
        timingTask(makeTimingConfig()) {
    sanitize();
    resetAll();
  }

  CwCarrierDiscriminatorConfig makeDiscriminatorConfig() const {
    CwCarrierDiscriminatorConfig cfg;
    cfg.minSnrDb = config.minSnrDb;
    cfg.minimumStableMs = 6;
    cfg.historyMs = 2500U;
    return cfg;
  }

  CwRelativeTimingConfig makeTimingConfig() const {
    CwRelativeTimingConfig cfg;
    cfg.initialWpm = config.initialWpm;
    cfg.autoWpm = config.autoWpm;
    cfg.pairRatio = 1.80;
    cfg.maxRollingText = config.maxRollingText;
    return cfg;
  }

  void sanitize() {
    config.toneHz = clampd(config.toneHz, 50.0, 5000.0);
    config.bandwidthHz = clampd(config.bandwidthHz, 30.0, 500.0);
    config.minSnrDb = clampd(config.minSnrDb, -3.0, 30.0);
    config.initialWpm = clampd(config.initialWpm, 5.0, 70.0);
    config.afcRangeHz = clampd(config.afcRangeHz, 3.0, 250.0);
    config.maxRollingText = std::max<std::size_t>(32U, config.maxRollingText);
  }

  double selectedBandwidth() const {
    if (!config.autoBandwidth) return config.bandwidthHz;
    // Audio bandwidth is an RF/carrier setting.  It must not inherit the
    // previous operator's keying speed and delay a temporal epoch change.
    return clampd(config.bandwidthHz, 60.0, 190.0);
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
    edgeAverage.configure(sampleRate, 8.0);
    carrierAverage.configure(sampleRate, 24.0);
    contrastCenterAverage.configure(sampleRate, 16.0);
    sidePlusAverage.configure(sampleRate, 16.0);
    sideMinusAverage.configure(sampleRate, 16.0);
    laneSeparator.configure(sampleRate, config.toneHz + afcOffset);
    controlSamplesTarget = sampleRate / 1000.0;
  }

  void log(const std::string& line) {
    if (logCallback) logCallback(line);
  }

  void logLogicRun(const CwLogicRun& run) {
    std::ostringstream line;
    line << "discriminator: " << (run.mark ? "MARK" : "SPACE")
         << ' ' << std::fixed << std::setprecision(1) << run.durationMs
         << " ms conf=" << std::setprecision(2) << run.confidence
         << " snr=" << std::setprecision(1) << run.meanSnrDb
         << " dB coh=" << std::setprecision(2) << run.coherence
         << " ctr=" << std::setprecision(2) << run.carrierCenteredFraction;
    if (run.qsbErasure) line << " QSB";
    log(line.str());
  }

  void resetAll() {
    discriminator = CwCarrierDiscriminator(makeDiscriminatorConfig());
    timingTask.setConfig(makeTimingConfig());
    timingTask.reset(false);
    phase = 0.0;
    sidePhase = 0.0;
    afcOffset = 0.0;
    smoothedFrequencyError = 0.0;
    havePreviousControl = false;
    previousControlWasMark = false;
    previousControl = {};
    controlSum = {};
    edgeControlSum = {};
    carrierControlSum = {};
    contrastCenterControlSum = {};
    sidePlusControlSum = {};
    sideMinusControlSum = {};
    rawControlSum = {};
    controlPower = 0.0;
    controlCount = 0;
    controlAccumulator = 0.0;
    slowCarrier = {};
    slowCarrierPower = 0.0;
    processedSamples = 0;
    publicSnr = -99.0;
    publicConfidence = 0.0;
    publicWpm = clampd(config.initialWpm, 5.0, 50.0);
    lastTemporalEpoch = 0U;
    rolling.clear();
    partial.clear();
    lastTiming = timingTask.snapshot();
    rawSpectrum.clear();
    filteredSpectrum.clear();
    spectrumHopCounter = 0;
    lastInterfererHz = 0.0;
    lastInterfererConfidence = 0.0;
    lastCarrierProminenceDb = -99.0;
    lastCarrierPeakWidthHz = 0.0;
    lastCarrierToSecondDb = 0.0;
    carrierEvidence = 0;
    timingFeedEnabled = false;
    temporalAwaitingSpace = false;
    lastCompletedSpaceMs = 0.0;
    carrierSessionProbability = 0.0;
    carrierSessionTimestampSec = 0.0;
    carrierLostSinceSec = -1.0;
    runPreRoll.clear();
    pendingPreRollSpace.reset();
    suppressedPreLockRuns = 0U;
    qsbErasureActive = false;
    qsbErasureStart = 0.0;
    diagnosticCounter = 0;
    i1.reset(); i2.reset(); q1.reset(); q2.reset();
    edgeAverage.reset();
    carrierAverage.reset();
    contrastCenterAverage.reset();
    sidePlusAverage.reset();
    sideMinusAverage.reset();
    laneSeparator.reset();
    log("clean restart: carrier discriminator and relative timing decoder reset");
  }

  void ensureSampleRate(double rate) {
    if (std::abs(sampleRate - rate) < 1.0e-6) return;
    if (!(sampleRate > 1000.0)) {
      // The tracker is already clean after construction/tone selection.  The
      // first audio block only supplies the missing rate and must not create a
      // second visible restart.  A real rate change during reception still
      // performs the full reset below.
      sampleRate = rate;
      configureDsp();
      return;
    }
    sampleRate = rate;
    resetAll();
    configureDsp();
  }

  void setInterferers(const std::vector<SelectedToneCwInterferer>& requested) {
    laneSeparator.setInterferers(requested, config.toneHz + afcOffset);
    nearestKnownLaneSeparationHz = std::numeric_limits<double>::infinity();
    for (const auto& item : requested) {
      if (!std::isfinite(item.toneHz) || !std::isfinite(item.confidence)) continue;
      if (item.confidence < 0.25) continue;
      const double separation = std::abs(item.toneHz - config.toneHz);
      if (separation > 0.5)
        nearestKnownLaneSeparationHz = std::min(nearestKnownLaneSeparationHz,
                                                separation);
    }
  }

  double guardedAfcRange(double trackedHz) const {
    const double nearest = laneSeparator.nearestInterfererSeparation(trackedHz);
    if (!std::isfinite(nearest)) return config.afcRangeHz;
    return std::min(config.afcRangeHz,
                    clampd(0.18 * nearest - 1.5, 2.0, config.afcRangeHz));
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
      const std::complex<double> edgeCoherent =
          laneSeparator.process(raw, tracked, sampleRate);
      const std::complex<double> filtered(
          i2.process(i1.process(edgeCoherent.real())),
          q2.process(q1.process(edgeCoherent.imag())));
      const std::complex<double> carrierCoherent = carrierAverage.process(edgeCoherent);

      constexpr double sideOffsetHz = 60.0;
      sidePhase += kTwoPi * sideOffsetHz / sampleRate;
      if (sidePhase >= kTwoPi) sidePhase -= kTwoPi;
      const std::complex<double> sideOsc(std::cos(sidePhase), -std::sin(sidePhase));
      const std::complex<double> contrastCenter = contrastCenterAverage.process(raw);
      const std::complex<double> sidePlus = sidePlusAverage.process(raw * sideOsc);
      const std::complex<double> sideMinus = sideMinusAverage.process(raw * std::conj(sideOsc));

      rawControlSum += raw;
      controlSum += filtered;
      edgeControlSum += edgeCoherent;
      carrierControlSum += carrierCoherent;
      contrastCenterControlSum += contrastCenter;
      sidePlusControlSum += sidePlus;
      sideMinusControlSum += sideMinus;
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

  void drainTimingResults(double timestamp, bool forceCallback = false) {
    CwRelativeTimingResult result;
    bool drained = false;
    while (timingTask.takeResult(result)) {
      handleTimingResult(result, timestamp, forceCallback);
      drained = true;
    }
    if (forceCallback && !drained)
      handleTimingResult(timingTask.snapshot(), timestamp, true);
  }

  void handleTimingResult(const CwRelativeTimingResult& result,
                          double timestamp, bool forceCallback = false) {
    const bool newTemporalEpoch = result.temporalEpoch != 0U &&
                                  result.temporalEpoch != lastTemporalEpoch;
    if (newTemporalEpoch) {
      lastTemporalEpoch = result.temporalEpoch;
      std::ostringstream epoch;
      epoch << "temporal epoch " << result.temporalEpoch
            << ": local geometry dit=" << std::fixed << std::setprecision(1)
            << result.ditMs << " ms dah=" << result.dahMs
            << " ms; continuity model remains Bayesian alternative";
      log(epoch.str());
    }
    lastTiming = result;
    partial = result.partialText;
    rolling = result.rollingText;
    // Never move the public WPM while the selected carrier is absent. The
    // temporal decoder may still be advancing an open SPACE to commit a
    // character, but receiver-idle noise is not timing evidence.
    const bool timingClockObservable = timingFeedEnabled && carrierEvidence >= 2 &&
        (result.state == CwRelativeTimingState::Track ||
         result.state == CwRelativeTimingState::PairLock) &&
        result.confidence >= 0.45;
    if (timingClockObservable && result.wpm > 0.1 &&
        std::isfinite(result.wpm)) {
      // WPM is display metadata.  On an epoch change show the new local scale
      // immediately; within one operator retain gentle visual smoothing.
      const double maxStep = newTemporalEpoch ? 50.0 : 1.0;
      publicWpm += clampd(result.wpm - publicWpm, -maxStep, maxStep);
      publicWpm = clampd(publicWpm, 5.0, 50.0);
    }
    publicConfidence = std::max(publicConfidence * 0.96, result.confidence);

    if (!result.committedText.empty()) {
      for (std::size_t i = 0; i < result.committedText.size(); ++i) {
        const char committed = result.committedText[i];
        const std::string pattern = i < result.committedPatterns.size()
            ? result.committedPatterns[i] : std::string{};
        const std::string patternLabel = !pattern.empty() ? pattern
            : (committed == ' ' ? "<space>" : "<unknown>");
        std::ostringstream line;
        line << "timing " << CwRelativeTimingDecoder::stateName(result.state)
             << ": commit=\"" << committed << "\""
             << " pattern=" << patternLabel
             << " dit=" << std::fixed << std::setprecision(1) << result.ditMs
             << " ms dah=" << result.dahMs
             << " ms tg=" << result.markThresholdMs
             << " ms WPM=" << result.wpm
             << " bayes=" << result.sequenceHypotheses
             << "/" << std::setprecision(2) << result.sequenceConfidence
             << " post=" << result.sequenceBestPosterior
             << " odds=" << std::setprecision(1)
             << result.sequencePosteriorOddsDb << "dB"
             << " epoch=" << result.temporalEpoch
             << " model=" << (result.temporalModel == 0 ? "LOCAL" : "CONT")
             << " beamDit=" << std::setprecision(1) << result.inferredDitMs;
        log(line.str());
      }
    }

    if (!callback || (result.committedText.empty() && !forceCallback)) return;
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
    event.wpm = publicWpm;
    event.trackingConfirmed = result.state == CwRelativeTimingState::Track ||
                              result.state == CwRelativeTimingState::PairLock;
    event.trackingState = CwRelativeTimingDecoder::stateName(result.state);
    event.committedText = result.committedText;
    event.partialText = result.partialText;
    event.rollingText = result.rollingText;
    callback(event);
  }

  void emitControlSample() {
    if (controlCount <= 0) return;
    const double timestamp = static_cast<double>(processedSamples) / sampleRate;
    const std::complex<double> coherent = controlSum / static_cast<double>(controlCount);
    const std::complex<double> edgeCoherent = edgeControlSum / static_cast<double>(controlCount);
    const std::complex<double> carrierCoherent = carrierControlSum / static_cast<double>(controlCount);
    const std::complex<double> contrastCenter = contrastCenterControlSum / static_cast<double>(controlCount);
    const std::complex<double> sidePlus = sidePlusControlSum / static_cast<double>(controlCount);
    const std::complex<double> sideMinus = sideMinusControlSum / static_cast<double>(controlCount);
    const std::complex<double> rawCoherent = rawControlSum / static_cast<double>(controlCount);
    const double totalPower = controlPower / static_cast<double>(controlCount);
    const double coherentPower = std::norm(coherent);
    const double residualPower = std::max(kEpsilon, totalPower - coherentPower);
    const double edgeEnvelope = std::abs(edgeCoherent);
    const double sideEnvelope = std::max(std::abs(sidePlus), std::abs(sideMinus));
    const double centerContrastEnvelope = std::abs(contrastCenter);
    const double envelope = std::max(kEpsilon, std::max(
        0.12 * edgeEnvelope,
        edgeEnvelope - 0.75 * std::max(0.0, sideEnvelope - 0.18 * centerContrastEnvelope)));

    constexpr double carrierAlpha = 0.045;
    slowCarrier += carrierAlpha * (carrierCoherent - slowCarrier);
    slowCarrierPower += carrierAlpha * (std::norm(carrierCoherent) - slowCarrierPower);
    const double coherence = clampd(std::norm(slowCarrier) /
                                    std::max(kEpsilon, slowCarrierPower), 0.0, 1.0);
    const double snrDb = 10.0 * std::log10((coherentPower + kEpsilon) /
                                          (residualPower + kEpsilon));
    publicSnr = snrDb;

    const bool separatedKnownNeighbour =
        std::isfinite(nearestKnownLaneSeparationHz) &&
        nearestKnownLaneSeparationHz >= 55.0 &&
        nearestKnownLaneSeparationHz <= 260.0;
    // Keep two independent carrier decisions.  A permissive lane-presence
    // test maintains an already valid session through QSB and crowded bands.
    // A much stricter timing-centred test is required to acquire or adapt the
    // dit/dah clock.  The screenshots that triggered this fix showed a
    // 15.9 dB peak with only 0.4 dB advantage over the second peak: visible
    // energy, but not a uniquely identified selected carrier.
    const bool spectrumLanePresent = lastCarrierProminenceDb >= 6.0 &&
        lastCarrierPeakWidthHz > 0.0 && lastCarrierPeakWidthHz <= 40.0 &&
        (lastCarrierToSecondDb >= -4.0 ||
         (separatedKnownNeighbour && lastCarrierToSecondDb >= -14.0));
    const bool spectrumTimingCentered = lastCarrierProminenceDb >= 9.5 &&
        lastCarrierPeakWidthHz > 0.0 && lastCarrierPeakWidthHz <= 28.0 &&
        (lastCarrierToSecondDb >= 2.0 ||
         (separatedKnownNeighbour && lastCarrierToSecondDb >= -10.0));
    // Before the first measured PSD frame, allow a coherent exact-tone MARK
    // to tag its own run so acquisition does not lose the leading dash.  It
    // does not enable the temporal decoder by itself: the later stableCarrier
    // gate still requires a narrow, prominent PSD lane.
    const double instantCenterToSideDb = 20.0 * std::log10(
        (centerContrastEnvelope + kEpsilon) / (sideEnvelope + kEpsilon));
    const bool acquisitionCentered = lastCarrierPeakWidthHz <= 0.0 &&
        coherence > 0.58 && snrDb > config.minSnrDb + 1.5 &&
        instantCenterToSideDb > 3.0;
    // Session continuity is a probability, not an N-dit timeout.  A proven
    // lane attacks quickly; in an OFF interval it decays with a time constant
    // derived from the learned word-space family and the prior carrier
    // stability.  This survives Farnsworth/8.8-dit gaps without keeping a dead
    // noisy channel qualified for a fixed magic duration.
    const double ditSec = std::max(0.024, lastTiming.ditMs / 1000.0);
    const double learnedWordSec = clampd(
        std::max(lastTiming.wordSpaceMs / 1000.0, 5.5 * ditSec),
        0.16, 2.50);
    const double sessionDt = carrierSessionTimestampSec > 0.0
        ? clampd(timestamp - carrierSessionTimestampSec, 0.0, 0.10)
        : 0.001;
    carrierSessionTimestampSec = timestamp;
    const double evidenceStability = clampd(carrierEvidence / 8.0, 0.0, 1.0);
    if (spectrumLanePresent) {
      const double attack = 1.0 - std::exp(-sessionDt / 0.055);
      carrierSessionProbability +=
          attack * (1.0 - carrierSessionProbability);
      carrierLostSinceSec = -1.0;
    } else {
      const double decayTau = clampd(
          (1.05 + 0.30 * evidenceStability) * learnedWordSec +
              0.20 * ditSec,
          0.20, 3.20);
      carrierSessionProbability *= std::exp(-sessionDt / decayTau);
    }
    if (!timingFeedEnabled && acquisitionCentered)
      carrierSessionProbability =
          std::max(carrierSessionProbability, 0.24);
    carrierSessionProbability = clampd(carrierSessionProbability, 0.0, 1.0);

    // acquisitionCentered is only a bootstrap hint before temporal lock.
    // Once timing is live, it must not keep a dead/noisy lane qualified: the
    // exact-tone coherent/residual ratio can look excellent in narrow-band
    // noise even when the PSD contains no real selected carrier.
    const bool carrierQualified = spectrumLanePresent ||
        (!timingFeedEnabled && acquisitionCentered) ||
        carrierSessionProbability >= 0.22;
    const bool currentStableCarrier = carrierEvidence >= 2 &&
        lastCarrierProminenceDb >= 8.0 &&
        lastCarrierPeakWidthHz > 0.0 && lastCarrierPeakWidthHz <= 30.0 &&
        (lastCarrierToSecondDb >= 0.5 ||
         (separatedKnownNeighbour && lastCarrierToSecondDb >= -10.0));

    CwCarrierObservation observation;
    observation.timestampSec = timestamp;
    observation.envelope = envelope;
    observation.acquisitionEnvelope = edgeEnvelope;
    observation.snrDb = snrDb;
    observation.coherence = coherence;
    // Per-run carrier evidence must describe the current measured lane, not
    // the session hold timer.  The hold keeps a valid decoder session alive
    // through Morse spaces; it must not turn every noise pulse into a centred
    // MARK.
    observation.carrierCentered = spectrumTimingCentered ||
        (!timingFeedEnabled && acquisitionCentered);

    const CwCarrierDiscriminatorResult carrier = discriminator.process(observation);
    publicConfidence = carrier.confidence;

    if (carrier.transitioned && carrier.completedRun.has_value()) {
      const CwLogicRun run = *carrier.completedRun;
      if (timingFeedEnabled) {
        logLogicRun(run);
      } else {
        const double priorDitMs = clampd(lastTiming.ditMs, 24.0, 260.0);
        const double priorDahMs = clampd(lastTiming.dahMs,
                                         1.65 * priorDitMs,
                                         4.20 * priorDitMs);
        const double minimumPreLockMarkMs = std::max(8.0, 0.38 * priorDitMs);
        const double preLockContinuityLimitMs = clampd(
            std::max(1.60 * lastTiming.wordSpaceMs, 5.0 * priorDitMs),
            180.0, 1250.0);
        const bool centeredPreLockMark = run.mark &&
            run.durationMs >= minimumPreLockMarkMs &&
            run.confidence >= 0.76 && run.coherence >= 0.35 &&
            run.carrierCenteredFraction >= 0.25;
        const bool priorFamilyMark = run.mark &&
            ((run.durationMs >= 0.62 * priorDitMs &&
              run.durationMs <= 1.45 * priorDitMs) ||
             (run.durationMs >= 0.62 * priorDahMs &&
              run.durationMs <= 1.45 * priorDahMs)) &&
            run.confidence >= 0.88 && run.coherence >= 0.50;
        const bool crediblePreLockMark = centeredPreLockMark || priorFamilyMark;

        if (crediblePreLockMark) {
          if (!pendingPreRollSpace.has_value() && !runPreRoll.empty() &&
              runPreRoll.back().mark) {
            // A rejected/unbounded OFF interval broke continuity.  Do not let
            // two non-adjacent MARKs masquerade as an informative pair.
            runPreRoll.clear();
          }
          if (pendingPreRollSpace.has_value() && !runPreRoll.empty() &&
              runPreRoll.back().mark) {
            logLogicRun(*pendingPreRollSpace);
            runPreRoll.push_back(*pendingPreRollSpace);
          }
          pendingPreRollSpace.reset();
          // A high-quality prior-family element may precede the first PSD frame
          // and is retained only for same-character pre-roll recovery.  Log it
          // once the selected lane itself has supplied centred evidence; this
          // keeps idle-noise diagnostics compact.
          if (centeredPreLockMark) logLogicRun(run);
          runPreRoll.push_back(run);
        } else if (!run.mark && !runPreRoll.empty() && runPreRoll.back().mark &&
                   run.durationMs <= preLockContinuityLimitMs) {
          // Hold one OFF run until a trustworthy following MARK proves that it
          // belongs to a potential Morse pair.  Receiver-idle oscillations are
          // therefore neither logged nor sent to the temporal worker.
          pendingPreRollSpace = run;
        } else {
          ++suppressedPreLockRuns;
          if (run.mark) {
            pendingPreRollSpace.reset();
          } else if (run.durationMs > preLockContinuityLimitMs) {
            runPreRoll.clear();
            pendingPreRollSpace.reset();
          }
        }

        while (!runPreRoll.empty() &&
               runPreRoll.front().endSec < timestamp - 1.25)
          runPreRoll.pop_front();
        if (pendingPreRollSpace.has_value() &&
            pendingPreRollSpace->endSec < timestamp - 1.25)
          pendingPreRollSpace.reset();
      }

      const bool stableCarrier = carrierEvidence >= 2 &&
          lastCarrierProminenceDb >= 8.0 &&
          lastCarrierPeakWidthHz > 0.0 && lastCarrierPeakWidthHz <= 30.0 &&
          (lastCarrierToSecondDb >= 0.5 ||
           (separatedKnownNeighbour && lastCarrierToSecondDb >= -10.0));
      const bool strictAcquisitionCarrier = carrierEvidence >= 3 &&
          spectrumTimingCentered;
      if (strictAcquisitionCarrier && !timingFeedEnabled) {
        const double priorDitMs = 1200.0 / std::max(5.0, config.initialWpm);
        const double minimumShortMs = std::max(8.0, 0.45 * priorDitMs);
        std::size_t pairStart = runPreRoll.size();
        double acquisitionShortMs = 0.0;
        double acquisitionLongMs = 0.0;
        double acquisitionGapMs = 0.0;
        for (std::size_t i = 0; i + 2U < runPreRoll.size(); ++i) {
          const CwLogicRun& first = runPreRoll[i];
          const CwLogicRun& gap = runPreRoll[i + 1U];
          const CwLogicRun& second = runPreRoll[i + 2U];
          if (!first.mark || gap.mark || !second.mark) continue;
          const auto crediblePairMark = [](const CwLogicRun& mark) {
            return mark.carrierCentered &&
                   mark.carrierCenteredFraction >= 0.55;
          };
          if (!crediblePairMark(first) || !crediblePairMark(second)) continue;
          const double shortMs = std::min(first.durationMs, second.durationMs);
          const double longMs = std::max(first.durationMs, second.durationMs);
          const double ratio = longMs / std::max(1.0, shortMs);
          const double meanCoherence = 0.5 * (first.coherence + second.coherence);
          const double meanConfidence = 0.5 * (first.confidence + second.confidence);
          if (shortMs < minimumShortMs || ratio < 1.80 || ratio > 4.00 ||
              gap.durationMs > 2.10 * shortMs || meanCoherence < 0.48 ||
              meanConfidence < 0.72)
            continue;
          // Start at the first trustworthy relative pair.  Carrier evidence
          // gathered later in the same pre-roll validates these two marks,
          // while the strict PSD gate above prevents noise-only acquisition.
          acquisitionShortMs = shortMs;
          acquisitionLongMs = longMs;
          acquisitionGapMs = gap.durationMs;
          pairStart = i;
          // Recover the complete contiguous pre-lock sequence, not a fixed
          // number of dits. A C at 38 WPM already spans about eleven units, so
          // the previous 6.5-dit walk-back could begin at its third element and
          // combine the tail with the following Q. The pre-roll is already
          // bounded to 1.25 s and is cleared by rejected/unbounded OFF runs; walk
          // back to the last real character-size SPACE, or to the first
          // carrier-qualified run when acquisition started inside a character.
          pairStart = 0U;
          for (std::size_t back = i; back > 0U; --back) {
            const CwLogicRun& previous = runPreRoll[back - 1U];
            if (!previous.mark && previous.durationMs >= 2.25 * shortMs) {
              pairStart = back;
              break;
            }
          }
          break;
        }
        if (pairStart < runPreRoll.size()) {
          // Acquisition can become strict on the SPACE->MARK transition.  In
          // that case the completed SPACE is still parked in
          // pendingPreRollSpace: the following MARK has begun, but has not yet
          // completed and therefore has not had a chance to promote the gap
          // into runPreRoll.  Replay that proven contiguous gap now.  Dropping
          // it merged the recovered first character with the next one (for
          // example C + Q became K1 at high speed).
          if (pendingPreRollSpace.has_value() && !runPreRoll.empty() &&
              runPreRoll.back().mark) {
            logLogicRun(*pendingPreRollSpace);
            runPreRoll.push_back(*pendingPreRollSpace);
            pendingPreRollSpace.reset();
          }

          // The centred pair defines a new local temporal epoch.  The previous
          // station clock is not discarded, but remains only as a weak
          // continuity hypothesis inside the Bayesian beam.
          timingTask.beginEpoch(acquisitionShortMs, acquisitionLongMs,
                                acquisitionGapMs, true);
          timingFeedEnabled = true;
          temporalAwaitingSpace = false;
          lastCompletedSpaceMs = 0.0;
          if (suppressedPreLockRuns > 0U) {
            std::ostringstream summary;
            summary << "pre-lock noise gate: suppressed "
                    << suppressedPreLockRuns << " unqualified run(s)";
            log(summary.str());
          }
          suppressedPreLockRuns = 0U;
          log("carrier lock: informative centred short/long pair found; fresh temporal epoch started");
          for (std::size_t i = pairStart; i < runPreRoll.size(); ++i) {
            CwLogicRun replay = runPreRoll[i];
            if (replay.mark) {
              const bool centred = replay.carrierCentered ||
                  replay.carrierCenteredFraction >= 0.55;
              const bool pairFamily = acquisitionShortMs > 0.0 &&
                  ((replay.durationMs >= 0.62 * acquisitionShortMs &&
                    replay.durationMs <= 1.48 * acquisitionShortMs) ||
                   (replay.durationMs >= 0.62 * acquisitionLongMs &&
                    replay.durationMs <= 1.48 * acquisitionLongMs));
              const bool leadingSameCharacter = !centred && pairFamily &&
                  replay.confidence >= 0.90 && replay.coherence >= 0.45;
              if (!centred && !leadingSameCharacter) continue;
              if (leadingSameCharacter) {
                replay.carrierCentered = true;
                replay.carrierCenteredFraction = 0.55;
              }
              timingTask.submitRun(replay);
              temporalAwaitingSpace = true;
            } else if (temporalAwaitingSpace) {
              timingTask.submitRun(replay);
              temporalAwaitingSpace = false;
            }
          }
          runPreRoll.clear();
          pendingPreRollSpace.reset();
        }
      } else if (timingFeedEnabled) {
        CwLogicRun accepted = run;
        const double priorDitMs = clampd(lastTiming.ditMs, 24.0, 260.0);
        const double priorDahMs = clampd(lastTiming.dahMs,
                                         1.65 * priorDitMs,
                                         4.20 * priorDitMs);
        const bool maintainedCarrierSession = carrierSessionProbability >= 0.22;
        const bool plausiblePriorMark = run.mark &&
            ((run.durationMs >= 0.56 * priorDitMs &&
              run.durationMs <= 1.60 * priorDitMs) ||
             (run.durationMs >= 0.56 * priorDahMs &&
              run.durationMs <= 1.60 * priorDahMs));
        const bool followsCharacterOrWordGap = lastCompletedSpaceMs >= 1.55 * priorDitMs &&
                                               lastCompletedSpaceMs <= 13.5 * priorDitMs;
        // Rescue only the first plausible element after a real character/word
        // gap while the previously proven carrier session is still alive.
        // The earlier code marked every run as session-qualified whenever the
        // PSD looked stable; narrow noise pulses inside spaces could therefore
        // become E/T/N. Conversely, the first dash after a long word gap was
        // rejected when the 256 ms PSD had not yet recovered. This one-element
        // boundary rescue fixes both failure modes and remains text-only.
        const bool boundaryRescue = maintainedCarrierSession &&
            followsCharacterOrWordGap && plausiblePriorMark &&
            run.confidence >= 0.84 && run.coherence >= 0.40;
        const bool qsbRescue = stableCarrier && run.mark && run.qsbErasure &&
            plausiblePriorMark && run.confidence >= 0.80 && run.coherence >= 0.38;
        accepted.carrierSessionQualified = stableCarrier || boundaryRescue ||
            qsbRescue || separatedKnownNeighbour;
        timingTask.submitRun(accepted);
        temporalAwaitingSpace = run.mark;
        if (!run.mark) lastCompletedSpaceMs = run.durationMs;
        runPreRoll.clear();
        pendingPreRollSpace.reset();
      }
    }
    if (timingFeedEnabled) {
      const bool qualifiedKeyDown = carrier.keyDown &&
          (observation.carrierCentered || currentStableCarrier ||
           (separatedKnownNeighbour && timingFeedEnabled));
      timingTask.submitAdvance(timestamp, qualifiedKeyDown);
    }
    drainTimingResults(timestamp);

    // A timing lock is not permission to decode noise forever.  Once the PSD
    // no longer supports the selected carrier beyond the normal word-gap hold,
    // stop feeding the temporal task and preserve only its last trustworthy
    // timing prior for the next transmission.
    if (timingFeedEnabled && !carrierQualified && carrierEvidence == 0) {
      if (carrierLostSinceSec < 0.0) carrierLostSinceSec = timestamp;
      const double ditSec = std::max(0.024, lastTiming.ditMs / 1000.0);
      const double lossConfirmSec = clampd(1.8 * ditSec, 0.08, 0.45);
      if (timestamp - carrierLostSinceSec >= lossConfirmSec) {
        // Finish the last real character, then freeze the temporal model. Do
        // not reset its pair evidence: reset(true) used to preserve the means
        // but clear the trust counter, allowing the next noise pair to install
        // a 50 WPM clock.
        timingTask.flush(timestamp);
        drainTimingResults(timestamp);
        timingFeedEnabled = false;
        temporalAwaitingSpace = false;
        lastCompletedSpaceMs = 0.0;
        runPreRoll.clear();
        pendingPreRollSpace.reset();
        discriminator.reset();
        lastTiming = timingTask.snapshot();
        log("carrier lost: timing frozen; WPM and dit/dah prior retained");
        carrierLostSinceSec = -1.0;
      }
    } else if (carrierQualified) {
      carrierLostSinceSec = -1.0;
    }

    // AFC uses only consecutive coherent MARK observations. It never bridges a
    // SPACE or follows a neighbouring station across the midpoint.
    if (config.afcEnabled && carrier.keyDown && previousControlWasMark &&
        havePreviousControl && std::abs(coherent) > 1.0e-9 &&
        std::abs(previousControl) > 1.0e-9) {
      const double phaseError = std::arg(coherent * std::conj(previousControl));
      const double frequencyError = phaseError / (kTwoPi * 0.001);
      if (std::isfinite(frequencyError) && std::abs(frequencyError) < 80.0) {
        smoothedFrequencyError += 0.08 * (frequencyError - smoothedFrequencyError);
        const double range = guardedAfcRange(config.toneHz + afcOffset);
        afcOffset = clampd(afcOffset + 0.004 * smoothedFrequencyError,
                           -range, range);
      }
    }
    previousControl = coherent;
    havePreviousControl = carrier.keyDown;
    previousControlWasMark = carrier.keyDown;

    qsbErasureActive = carrier.qsbErasure;
    if (qsbErasureActive && qsbErasureStart <= 0.0) qsbErasureStart = timestamp;
    if (!qsbErasureActive) qsbErasureStart = 0.0;

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
      emitDiagnostic(timestamp, edgeEnvelope, envelope, carrier, snrDb, coherence);
    }

    controlSum = {};
    edgeControlSum = {};
    carrierControlSum = {};
    contrastCenterControlSum = {};
    sidePlusControlSum = {};
    sideMinusControlSum = {};
    rawControlSum = {};
    controlPower = 0.0;
    controlCount = 0;
  }

  void emitDiagnostic(double timestamp, double acquisitionEnvelope,
                      double filteredEnvelope,
                      const CwCarrierDiscriminatorResult& carrier,
                      double snrDb, double coherence) {
    if (!diagnosticCallback) return;
    SelectedToneCwDiagnosticSample d;
    d.timestampSec = timestamp;
    d.acquisitionEnvelope = acquisitionEnvelope;
    d.filteredEnvelope = filteredEnvelope;
    d.signalLevel = carrier.markLevel;
    d.thresholdLow = carrier.thresholdLow;
    d.thresholdHigh = carrier.thresholdHigh;
    d.markProbability = carrier.markProbability;
    d.qsbErasure = carrier.qsbErasure;
    d.qsbErasureStartSec = qsbErasureStart;
    d.qsbErasureEndSec = carrier.qsbErasure ? timestamp : qsbErasureStart;
    d.keyDown = carrier.keyDown;
    d.markerHz = config.toneHz;
    d.trackedHz = config.toneHz + afcOffset;
    d.snrDb = snrDb;
    d.requestedBandwidthHz = config.bandwidthHz;
    d.effectiveBandwidthHz = effectiveBandwidth;
    d.wpm = publicWpm;
    d.lockQuality = carrier.confidence;
    d.trackingConfirmed = lastTiming.state == CwRelativeTimingState::Track ||
                          lastTiming.state == CwRelativeTimingState::PairLock;
    d.carrierProminenceDb = lastCarrierProminenceDb;
    d.coherentSnrDb = snrDb;
    d.coherence = coherence;
    d.ditMs = lastTiming.ditMs;
    d.dahMs = lastTiming.dahMs;
    d.markThresholdMs = lastTiming.markThresholdMs;
    d.characterSpaceMs = lastTiming.characterSpaceMs;
    d.wordSpaceMs = lastTiming.wordSpaceMs;
    d.timingState = CwRelativeTimingDecoder::stateName(lastTiming.state);
    d.currentPattern = lastTiming.currentPattern;
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
    double mainOutputPeak = -200.0;
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
      frame.theoreticalResponseDb.push_back(static_cast<float>(
          -10.0 * std::log10(1.0 + std::pow(std::abs(offset) / cutoff, 8.0))));
      if (std::abs(offset) <= 12.0 && inDb > mainPeak) {
        mainPeak = inDb;
        mainOutputPeak = outDb;
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
    for (std::size_t i = 0; i < frame.inputPsdDb.size(); ++i)
      if (std::abs(frame.offsetsHz[i]) > 20.0) floorValues.push_back(frame.inputPsdDb[i]);
    if (!floorValues.empty()) {
      std::nth_element(floorValues.begin(),
                       floorValues.begin() + static_cast<std::ptrdiff_t>(floorValues.size() / 2U),
                       floorValues.end());
    }
    const double floorDb = floorValues.empty() ? -120.0
                                                : floorValues[floorValues.size() / 2U];
    lastCarrierProminenceDb = mainPeak - floorDb;
    lastCarrierToSecondDb = mainPeak - secondPeak;
    const bool separatedKnownNeighbour =
        std::isfinite(nearestKnownLaneSeparationHz) &&
        nearestKnownLaneSeparationHz >= 55.0 &&
        nearestKnownLaneSeparationHz <= 260.0;
    if (lastCarrierProminenceDb >= 8.0 &&
        (lastCarrierToSecondDb >= 0.5 ||
         (separatedKnownNeighbour && lastCarrierToSecondDb >= -14.0))) {
      // Two consistent 512 ms PSD observations are enough to acquire, while
      // two clearly bad observations remove stale carrier evidence. The old
      // +1/-1 integrator could keep a dead lane alive for more than two
      // seconds after a strong transmission ended.
      carrierEvidence = std::min(8, carrierEvidence + 2);
    } else {
      carrierEvidence = std::max(0, carrierEvidence - 4);
    }

    lastInterfererHz = config.toneHz + afcOffset + secondOffset;
    lastInterfererConfidence = clampd((secondPeak - floorDb - 4.0) / 12.0, 0.0, 1.0);

    int widthBins = 1;
    const double halfPower = mainOutputPeak - 3.0;
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
    const double timestamp = sampleRate > 0.0
        ? static_cast<double>(processedSamples) / sampleRate : 0.0;
    if (const auto run = discriminator.flush(timestamp); run.has_value()) {
      timingTask.submitRun(*run);
    }
    timingTask.flush(timestamp);
    drainTimingResults(timestamp, true);
  }

  SelectedToneCwConfig config;
  SelectedToneCwCallback callback;
  SelectedToneCwDiagnosticCallback diagnosticCallback;
  SelectedToneCwSpectrumCallback spectrumCallback;
  SelectedToneCwLogCallback logCallback;
  CwCarrierDiscriminator discriminator;
  CwRelativeTimingTask timingTask;
  CwRelativeTimingResult lastTiming;

  double sampleRate = 0.0;
  double effectiveBandwidth = 120.0;
  double controlSamplesTarget = 48.0;
  double phase = 0.0;
  double sidePhase = 0.0;
  double afcOffset = 0.0;
  double smoothedFrequencyError = 0.0;
  BiquadLowPass i1, i2, q1, q2;
  SlidingComplexAverage edgeAverage;
  SlidingComplexAverage carrierAverage;
  SlidingComplexAverage contrastCenterAverage;
  SlidingComplexAverage sidePlusAverage;
  SlidingComplexAverage sideMinusAverage;
  ComplexLaneSeparator laneSeparator;
  double nearestKnownLaneSeparationHz = std::numeric_limits<double>::infinity();

  std::complex<double> controlSum{};
  std::complex<double> edgeControlSum{};
  std::complex<double> carrierControlSum{};
  std::complex<double> contrastCenterControlSum{};
  std::complex<double> sidePlusControlSum{};
  std::complex<double> sideMinusControlSum{};
  std::complex<double> rawControlSum{};
  double controlPower = 0.0;
  int controlCount = 0;
  double controlAccumulator = 0.0;
  std::uint64_t processedSamples = 0;
  std::complex<double> previousControl{};
  bool havePreviousControl = false;
  bool previousControlWasMark = false;
  std::complex<double> slowCarrier{};
  double slowCarrierPower = 0.0;

  double publicSnr = -99.0;
  double publicConfidence = 0.0;
  double publicWpm = 20.0;
  std::uint64_t lastTemporalEpoch = 0U;
  std::string rolling;
  std::string partial;

  std::deque<std::complex<double>> rawSpectrum;
  std::deque<std::complex<double>> filteredSpectrum;
  int spectrumHopCounter = 0;
  double lastInterfererHz = 0.0;
  double lastInterfererConfidence = 0.0;
  double lastCarrierProminenceDb = -99.0;
  double lastCarrierPeakWidthHz = 0.0;
  double lastCarrierToSecondDb = 0.0;
  int carrierEvidence = 0;
  bool timingFeedEnabled = false;
  bool temporalAwaitingSpace = false;
  double lastCompletedSpaceMs = 0.0;
  double carrierSessionProbability = 0.0;
  double carrierSessionTimestampSec = 0.0;
  double carrierLostSinceSec = -1.0;
  std::deque<CwLogicRun> runPreRoll;
  std::optional<CwLogicRun> pendingPreRollSpace;
  std::size_t suppressedPreLockRuns = 0U;
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
void SelectedToneCwTracker::setLogCallback(SelectedToneCwLogCallback callback) {
  m_impl->logCallback = std::move(callback);
}
void SelectedToneCwTracker::setToneHz(double toneHz) {
  const double clamped = clampd(toneHz, 50.0, 5000.0);
  // UI signal fan-out can deliver the same marker value more than once.  A
  // duplicate update must not destroy an active CW timing lock.
  if (std::abs(clamped - m_impl->config.toneHz) < 0.5) return;
  m_impl->config.toneHz = clamped;
  m_impl->resetAll();
  m_impl->configureDsp();
}
void SelectedToneCwTracker::setBandwidthHz(double bandwidthHz) {
  m_impl->config.bandwidthHz = clampd(bandwidthHz, 30.0, 500.0);
  m_impl->configureDsp();
}
void SelectedToneCwTracker::setMinSnrDb(double minSnrDb) {
  m_impl->config.minSnrDb = clampd(minSnrDb, -3.0, 30.0);
  auto cfg = m_impl->discriminator.config();
  cfg.minSnrDb = m_impl->config.minSnrDb;
  m_impl->discriminator.setConfig(cfg);
}
void SelectedToneCwTracker::setWpmHint(double wpm) {
  m_impl->config.initialWpm = clampd(wpm, 5.0, 50.0);
  m_impl->timingTask.setConfig(m_impl->makeTimingConfig());
  if (!m_impl->config.autoWpm) m_impl->timingTask.reset(false);
}
void SelectedToneCwTracker::setAutoWpm(bool enabled) {
  m_impl->config.autoWpm = enabled;
  auto cfg = m_impl->makeTimingConfig();
  m_impl->timingTask.setConfig(cfg);
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
void SelectedToneCwTracker::setInterferers(
    const std::vector<SelectedToneCwInterferer>& interferers) {
  m_impl->setInterferers(interferers);
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
double SelectedToneCwTracker::wpm() const { return clampd(m_impl->publicWpm, 5.0, 50.0); }
double SelectedToneCwTracker::trackedToneHz() const { return m_impl->config.toneHz + m_impl->afcOffset; }
double SelectedToneCwTracker::frequencyErrorHz() const { return m_impl->smoothedFrequencyError; }
double SelectedToneCwTracker::effectiveBandwidthHz() const { return m_impl->effectiveBandwidth; }
double SelectedToneCwTracker::acquisitionBandwidthHz() const {
  return m_impl->config.bandwidthHz + 2.0 * m_impl->config.afcRangeHz;
}
std::string SelectedToneCwTracker::trackingState() const {
  return CwRelativeTimingDecoder::stateName(m_impl->lastTiming.state);
}
const std::string& SelectedToneCwTracker::rollingText() const { return m_impl->rolling; }
const std::string& SelectedToneCwTracker::partialText() const { return m_impl->partial; }

} // namespace madmodem::cwskimmer
