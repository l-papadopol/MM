#include "CwCarrierDiscriminator.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace madmodem::cwskimmer {
namespace {
constexpr double kEpsilon = 1.0e-12;

inline double clampd(double value, double low, double high) {
  return std::max(low, std::min(high, value));
}

inline double logistic(double value) {
  if (value >= 0.0) {
    const double e = std::exp(-value);
    return 1.0 / (1.0 + e);
  }
  const double e = std::exp(value);
  return e / (1.0 + e);
}

inline double logit(double probability) {
  const double p = clampd(probability, 0.001, 0.999);
  return std::log(p / (1.0 - p));
}

inline double normalCdf(double value) {
  return 0.5 * std::erfc(-value / std::sqrt(2.0));
}

double logNormalCdf(double durationMs, double centreMs, double sigmaLog) {
  if (durationMs <= 0.0) return 0.0;
  const double z = std::log(durationMs / std::max(1.0, centreMs)) /
                   std::max(0.08, sigmaLog);
  return clampd(normalCdf(z), 0.0, 1.0);
}

double logNormalMass(int durationMs, double centreMs, double sigmaLog) {
  const double low = logNormalCdf(
      std::max(0.0, static_cast<double>(durationMs) - 0.5),
      centreMs, sigmaLog);
  const double high = logNormalCdf(
      static_cast<double>(durationMs) + 0.5, centreMs, sigmaLog);
  return std::max(0.0, high - low);
}
}

CwCarrierDiscriminator::CwCarrierDiscriminator(
    CwCarrierDiscriminatorConfig config)
    : m_config(config) {
  sanitize();
  reset();
}

void CwCarrierDiscriminator::setConfig(
    const CwCarrierDiscriminatorConfig& config) {
  m_config = config;
  sanitize();
}

const CwCarrierDiscriminatorConfig& CwCarrierDiscriminator::config() const {
  return m_config;
}

void CwCarrierDiscriminator::sanitize() {
  m_config.minSnrDb = clampd(m_config.minSnrDb, -6.0, 30.0);
  m_config.minimumStableMs = std::max(3, std::min(18, m_config.minimumStableMs));
  m_config.historyMs = std::max<std::size_t>(300U,
      std::min<std::size_t>(5000U, m_config.historyMs));
  m_config.segmentBeamWidth = std::max<std::size_t>(8U,
      std::min<std::size_t>(64U, m_config.segmentBeamWidth));
  m_config.minimumFixedLagMs = std::max(16,
      std::min(80, m_config.minimumFixedLagMs));
  m_config.maximumFixedLagMs = std::max(m_config.minimumFixedLagMs,
      std::min(260, m_config.maximumFixedLagMs));
}

void CwCarrierDiscriminator::reset() {
  m_levelHistory.clear();
  m_levelUpdateCounter = 0;
  m_noiseLog = 0.0;
  m_markLog = std::log(4.0);
  m_haveLevels = false;
  m_probability = 0.5;
  m_confidence = 0.0;
  m_keyDown = false;
  m_instantKeyDown = false;
  m_haveActiveRun = false;
  m_fixedLagMs = 0;
  m_resolvedTimestampSec = 0.0;
  m_segmentBeam.clear();
  m_pendingSamples.clear();
  m_runStartSec = 0.0;
  m_runConfidenceSum = 0.0;
  m_runSnrSum = 0.0;
  m_runPeakSnr = -99.0;
  m_runCoherenceSum = 0.0;
  m_runSamples = 0;
  m_runQsbErasure = false;
  m_runCarrierCenteredSamples = 0;
  m_runMarkProbabilitySum = 0.0;
  m_runQsbProbabilitySum = 0.0;
}

void CwCarrierDiscriminator::startRun(
    bool mark, double timestampSec,
    const CwCarrierObservation& observation,
    double confidence, double markProbability,
    double qsbProbability) {
  m_keyDown = mark;
  m_haveActiveRun = true;
  m_runStartSec = timestampSec;
  m_runConfidenceSum = 0.0;
  m_runSnrSum = 0.0;
  m_runPeakSnr = -99.0;
  m_runCoherenceSum = 0.0;
  m_runSamples = 0;
  m_runQsbErasure = false;
  m_runCarrierCenteredSamples = 0;
  m_runMarkProbabilitySum = 0.0;
  m_runQsbProbabilitySum = 0.0;
  accumulate(observation, confidence, markProbability, qsbProbability);
}

void CwCarrierDiscriminator::accumulate(
    const CwCarrierObservation& observation,
    double confidence, double markProbability,
    double qsbProbability) {
  m_runConfidenceSum += clampd(confidence, 0.0, 1.0);
  m_runSnrSum += observation.snrDb;
  m_runPeakSnr = std::max(m_runPeakSnr, observation.snrDb);
  m_runCoherenceSum += clampd(observation.coherence, 0.0, 1.0);
  ++m_runSamples;
  m_runQsbErasure = m_runQsbErasure || qsbProbability >= 0.35;
  if (observation.carrierCentered) ++m_runCarrierCenteredSamples;
  m_runMarkProbabilitySum += clampd(markProbability, 0.001, 0.999);
  m_runQsbProbabilitySum += clampd(qsbProbability, 0.0, 1.0);
}

CwLogicRun CwCarrierDiscriminator::finishRun(double timestampSec) const {
  CwLogicRun run;
  run.startSec = m_runStartSec;
  run.endSec = std::max(m_runStartSec, timestampSec);
  run.mark = m_keyDown;
  run.durationMs = 1000.0 * (run.endSec - run.startSec);
  const double count = static_cast<double>(std::max(1, m_runSamples));
  run.confidence = clampd(m_runConfidenceSum / count, 0.0, 1.0);
  run.meanSnrDb = m_runSnrSum / count;
  run.peakSnrDb = m_runPeakSnr;
  run.coherence = clampd(m_runCoherenceSum / count, 0.0, 1.0);
  run.qsbErasure = m_runQsbErasure;
  run.carrierCenteredFraction = clampd(
      static_cast<double>(m_runCarrierCenteredSamples) / count, 0.0, 1.0);
  run.meanMarkProbability = clampd(m_runMarkProbabilitySum / count,
                                      0.001, 0.999);
  run.qsbProbability = clampd(m_runQsbProbabilitySum / count, 0.0, 1.0);
  const double stateCertainty = std::abs(2.0 * run.meanMarkProbability - 1.0);
  run.noiseProbability = clampd(
      1.0 - (0.42 * run.confidence + 0.25 * run.coherence +
             0.20 * stateCertainty +
             0.13 * run.carrierCenteredFraction),
      0.0, 0.98);
  // A single stale spectrum frame must not qualify an entire noise run.
  // MARKs need majority support from a centered narrow carrier; SPACE runs
  // retain the fraction for diagnostics but are not themselves carrier events.
  run.carrierCentered = run.mark && run.carrierCenteredFraction >= 0.55;
  return run;
}

double CwCarrierDiscriminator::durationHazard(
    bool mark, int durationMs, double ditMs, double timingConfidence) {
  const int duration = std::max(1, durationMs);
  const double dit = clampd(ditMs, 24.0, 260.0);
  const double trust = clampd(timingConfidence, 0.0, 1.0);
  const double sigma = mark
      ? 0.19 + 0.43 * (1.0 - trust)
      : 0.24 + 0.50 * (1.0 - trust);
  const double boundary = std::max(
      0.0, static_cast<double>(duration) - 0.5);

  double mass = 0.0;
  double survival = 0.0;
  const auto addLogNormal = [&](double weight, double centre,
                                double familySigma) {
    mass += weight * logNormalMass(duration, centre, familySigma);
    survival += weight * (1.0 - logNormalCdf(
        boundary, centre, familySigma));
  };
  const auto addExponentialTail = [&](double weight, double meanMs) {
    const double safeMean = std::max(1.0, meanMs);
    const double lowSurvival = std::exp(-boundary / safeMean);
    const double highSurvival = std::exp(
        -(static_cast<double>(duration) + 0.5) / safeMean);
    mass += weight * std::max(0.0, lowSurvival - highSurvival);
    survival += weight * lowSurvival;
  };

  if (mark) {
    // A MARK ends near one or three local units.  The small heavy tail keeps a
    // hand-sent/clipped observation alive instead of forcing a false edge.
    addLogNormal(0.55, dit, sigma);
    addLogNormal(0.42, 3.0 * dit, sigma + 0.05);
    addExponentialTail(0.03, 5.0 * dit);
  } else {
    // Explicit 1/3/7-unit gap families plus an idle/Farnsworth tail.  Thus a
    // long SPACE is legal and cannot be chopped merely to satisfy the model.
    addLogNormal(0.60, dit, sigma);
    addLogNormal(0.26, 3.0 * dit, sigma + 0.08);
    addLogNormal(0.10, 7.0 * dit, sigma + 0.13);
    addExponentialTail(0.04, 18.0 * dit);
  }

  if (duration < 3) return 1.0e-5;
  return clampd(mass / std::max(1.0e-12, survival), 1.0e-5, 0.96);
}

std::optional<CwLogicRun> CwCarrierDiscriminator::commitResolvedState(
    bool mark, double posteriorMarkProbability,
    const CwCarrierObservation& observation,
    double sampleConfidence, double qsbProbability) {
  m_resolvedTimestampSec = observation.timestampSec;
  if (!m_haveActiveRun) {
    startRun(mark, observation.timestampSec, observation,
             sampleConfidence, posteriorMarkProbability, qsbProbability);
    return std::nullopt;
  }
  if (mark == m_keyDown) {
    accumulate(observation, sampleConfidence,
               posteriorMarkProbability, qsbProbability);
    return std::nullopt;
  }

  CwLogicRun completed = finishRun(observation.timestampSec);
  startRun(mark, observation.timestampSec, observation,
           sampleConfidence, posteriorMarkProbability, qsbProbability);
  return completed;
}

void CwCarrierDiscriminator::advanceSegmentalBeam(
    const CwCarrierObservation& observation,
    double markProbability, double emissionErasureProbability,
    double qsbProbability,
    CwCarrierDiscriminatorResult& result) {
  const double dit = clampd(observation.timingDitMs, 24.0, 260.0);
  const int targetLag = std::max(m_config.minimumFixedLagMs,
      std::min(m_config.maximumFixedLagMs,
               static_cast<int>(std::lround(1.30 * dit))));
  if (m_fixedLagMs <= 0) {
    m_fixedLagMs = targetLag;
  } else if (targetLag > m_fixedLagMs) {
    ++m_fixedLagMs;
  } else if (targetLag < m_fixedLagMs) {
    --m_fixedLagMs;
  }

  const double snrEvidence = clampd(
      (observation.snrDb - m_config.minSnrDb + 4.0) / 18.0,
      0.0, 1.0);
  double reliability = clampd(
      0.18 + 0.34 * m_confidence +
      0.22 * clampd(observation.coherence, 0.0, 1.0) +
      0.18 * snrEvidence +
      0.08 * clampd(observation.carrierSessionProbability, 0.0, 1.0),
      0.10, 0.995);
  if (!observation.carrierCentered &&
      observation.carrierSessionProbability < 0.20) {
    reliability *= 0.82;
  }
  // The fixed-lag duration model supplies the temporal correlation.  Keep the
  // per-sample RF evidence conservative (never sharpen it above the calibrated
  // discriminator posterior), but do not flatten clean short dits merely
  // because the spectrum estimator has not yet accumulated a full frame.
  const double evidenceScale = clampd(
      0.42 + 0.72 * reliability, 0.42, 0.98);
  double softMark = logistic(logit(markProbability) * evidenceScale);
  // A QSB notch is not declared MARK.  It merely restores enough posterior
  // mass for the existing MARK path to compete until the fixed lag sees the
  // recovery (or proves that the dip was a real SPACE).
  if (emissionErasureProbability > 0.0) {
    softMark = (1.0 - emissionErasureProbability) * softMark +
               0.5 * emissionErasureProbability;
  }
  softMark = clampd(softMark, 0.001, 0.999);

  PendingSample pending;
  pending.observation = observation;
  pending.markProbability = softMark;
  pending.qsbProbability = qsbProbability;
  pending.confidence = m_confidence;
  m_pendingSamples.push_back(std::move(pending));

  const auto emission = [&](bool mark) {
    return std::log(mark ? softMark : (1.0 - softMark));
  };

  if (m_segmentBeam.empty()) {
    SegmentHypothesis off;
    off.mark = false;
    off.durationMs = 1;
    off.logScore = std::log(0.72) + emission(false);
    off.unresolvedStates.push_back(false);
    SegmentHypothesis on;
    on.mark = true;
    on.durationMs = 1;
    on.logScore = std::log(0.28) + emission(true);
    on.unresolvedStates.push_back(true);
    m_segmentBeam.push_back(std::move(off));
    m_segmentBeam.push_back(std::move(on));
  } else {
    std::vector<SegmentHypothesis> next;
    next.reserve(2U * m_segmentBeam.size());
    for (const SegmentHypothesis& base : m_segmentBeam) {
      const double hazard = durationHazard(
          base.mark, base.durationMs, dit, observation.timingConfidence);

      SegmentHypothesis continued = base;
      ++continued.durationMs;
      continued.logScore += std::log1p(-hazard) + emission(base.mark);
      continued.unresolvedStates.push_back(base.mark);
      next.push_back(std::move(continued));

      SegmentHypothesis changed = base;
      changed.mark = !base.mark;
      changed.durationMs = 1;
      changed.logScore += std::log(hazard) + emission(changed.mark);
      changed.unresolvedStates.push_back(changed.mark);
      next.push_back(std::move(changed));
    }

    const auto better = [](const SegmentHypothesis& left,
                           const SegmentHypothesis& right) {
      return left.logScore > right.logScore;
    };
    std::sort(next.begin(), next.end(), better);
    std::vector<SegmentHypothesis> unique;
    unique.reserve(std::min(m_config.segmentBeamWidth, next.size()));
    for (SegmentHypothesis& candidate : next) {
      const bool duplicate = std::any_of(
          unique.begin(), unique.end(), [&](const SegmentHypothesis& existing) {
            return existing.mark == candidate.mark &&
                   existing.durationMs == candidate.durationMs &&
                   existing.unresolvedStates == candidate.unresolvedStates;
          });
      if (!duplicate) unique.push_back(std::move(candidate));
      if (unique.size() >= m_config.segmentBeamWidth) break;
    }
    const double best = unique.front().logScore;
    for (SegmentHypothesis& hypothesis : unique)
      hypothesis.logScore -= best;
    m_segmentBeam = std::move(unique);
  }

  m_instantKeyDown = !m_segmentBeam.empty() && m_segmentBeam.front().mark;
  result.keyDown = m_instantKeyDown;
  result.fixedLagMs = m_fixedLagMs;

  if (m_pendingSamples.size() <= static_cast<std::size_t>(m_fixedLagMs)) {
    result.resolvedKeyDown = m_keyDown;
    result.resolvedTimestampSec = m_resolvedTimestampSec;
    return;
  }

  double total = 0.0;
  double markMass = 0.0;
  for (const SegmentHypothesis& hypothesis : m_segmentBeam) {
    if (hypothesis.unresolvedStates.empty()) continue;
    const double weight = std::exp(std::max(-60.0, hypothesis.logScore));
    total += weight;
    if (hypothesis.unresolvedStates.front()) markMass += weight;
  }
  const double resolvedMarkProbability = total > 0.0
      ? clampd(markMass / total, 0.001, 0.999)
      : (m_segmentBeam.front().unresolvedStates.front() ? 0.999 : 0.001);
  const bool resolvedMark = resolvedMarkProbability >= 0.50;
  const PendingSample sample = m_pendingSamples.front();
  const double certainty = std::abs(2.0 * resolvedMarkProbability - 1.0);
  const double confidence = clampd(
      0.55 * sample.confidence + 0.45 * certainty, 0.0, 1.0);
  if (const auto completed = commitResolvedState(
          resolvedMark, resolvedMarkProbability, sample.observation,
          confidence, sample.qsbProbability); completed.has_value()) {
    result.completedRun = *completed;
    result.transitioned = true;
  }
  m_pendingSamples.pop_front();
  for (SegmentHypothesis& hypothesis : m_segmentBeam) {
    if (!hypothesis.unresolvedStates.empty())
      hypothesis.unresolvedStates.pop_front();
  }
  result.resolvedKeyDown = m_keyDown;
  result.resolvedTimestampSec = m_resolvedTimestampSec;
}

CwCarrierDiscriminatorResult CwCarrierDiscriminator::process(
    const CwCarrierObservation& observation) {
  CwCarrierDiscriminatorResult result;
  const double envelope = std::max(kEpsilon, observation.envelope);
  const double logEnvelope = std::log(envelope);

  m_levelHistory.push_back(logEnvelope);
  while (m_levelHistory.size() > m_config.historyMs) m_levelHistory.pop_front();
  ++m_levelUpdateCounter;

  if (!m_haveLevels) {
    m_noiseLog = logEnvelope;
    m_markLog = logEnvelope + std::log(4.0);
    m_haveLevels = true;
  }

  if (m_levelHistory.size() >= 120U && m_levelUpdateCounter >= 20) {
    m_levelUpdateCounter = 0;
    std::vector<double> values(m_levelHistory.begin(), m_levelHistory.end());
    const std::size_t lowIndex = values.size() / 5U;
    const std::size_t highIndex = (values.size() * 17U) / 20U;
    std::nth_element(values.begin(), values.begin() +
        static_cast<std::ptrdiff_t>(lowIndex), values.end());
    const double low = values[lowIndex];
    std::nth_element(values.begin(), values.begin() +
        static_cast<std::ptrdiff_t>(highIndex), values.end());
    const double high = values[highIndex];
    if (high > low + std::log(1.45)) {
      const double alpha = m_levelHistory.size() < 500U ? 0.42 : 0.16;
      m_noiseLog += alpha * (low - m_noiseLog);
      m_markLog += alpha * (high - m_markLog);
    }
  }

  // One-way coherent bootstrap: acquisition must not require a MARK level that
  // can only be learned after a MARK has already crossed the threshold.
  if (logEnvelope > m_noiseLog + std::log(2.8) &&
      (observation.coherence > 0.40 ||
       observation.snrDb > m_config.minSnrDb - 1.5) &&
      m_markLog < logEnvelope - std::log(1.25)) {
    m_markLog = logEnvelope;
  }

  if (m_markLog < m_noiseLog + std::log(1.8))
    m_markLog = m_noiseLog + std::log(1.8);

  const double separation = std::max(std::log(2.0), m_markLog - m_noiseLog);
  const double midpoint = std::max(m_noiseLog + 0.58 * separation,
                                   m_markLog + std::log(0.18));
  m_probability = clampd(logistic((logEnvelope - midpoint) / 0.24),
                         0.001, 0.999);
  const double separationDb = 8.685889638 * separation;
  const double levelEvidence = clampd(separationDb / 14.0, 0.0, 1.0);
  const double decisionEvidence = std::abs(2.0 * m_probability - 1.0);
  m_confidence = clampd(0.60 * decisionEvidence +
                        0.26 * levelEvidence +
                        0.14 * clampd(observation.coherence, 0.0, 1.0),
                        0.0, 1.0);

  const bool strongMark = m_probability > 0.74 && m_confidence > 0.42 &&
      observation.snrDb > m_config.minSnrDb - 2.0;
  if (strongMark) {
    const double alpha = m_instantKeyDown ? 0.018 : 0.035;
    m_markLog += alpha * (logEnvelope - m_markLog);
  } else if (m_probability < 0.42 || !m_instantKeyDown) {
    const double delta = logEnvelope - m_noiseLog;
    const double alpha = delta < 0.0 ? 0.004 : 0.0004;
    m_noiseLog += alpha * delta;
  }

  // A short ambiguous dip inside a MARK is tagged as a possible erasure but is
  // not allowed to create an immediate SPACE edge.  It only changes the soft
  // evidence of the segmental beam; a sustained dip still becomes SPACE.
  const bool aboveNoiseShoulder =
      logEnvelope > m_noiseLog + 0.20 * separation;
  const double session = clampd(
      observation.carrierSessionProbability, 0.0, 1.0);
  double qsbProbability = 0.0;
  const bool localCarrierSupport = observation.carrierCentered ||
      (observation.coherence >= 0.48 &&
       observation.snrDb >= m_config.minSnrDb);
  if (m_instantKeyDown && aboveNoiseShoulder &&
      m_probability > 0.06 && m_probability < 0.48 &&
      (session >= 0.18 || localCarrierSupport)) {
    const double dip = clampd((0.50 - m_probability) / 0.44, 0.0, 1.0);
    const double ambiguity = clampd((0.62 - m_confidence) / 0.62, 0.0, 1.0);
    qsbProbability = clampd(
        dip * (0.28 + 0.36 * ambiguity +
               0.20 * clampd(observation.coherence, 0.0, 1.0) +
               0.16 * session),
        0.0, 0.95);
  }
  advanceSegmentalBeam(observation, m_probability,
                       qsbProbability,
                       qsbProbability, result);

  result.markProbability = m_probability;
  result.confidence = m_confidence;
  result.noiseLevel = std::exp(m_noiseLog);
  result.markLevel = std::exp(m_markLog);
  result.thresholdLow = std::exp(midpoint - 0.18);
  result.thresholdHigh = std::exp(midpoint + 0.18);
  result.qsbErasure = qsbProbability >= 0.35;
  return result;
}

std::vector<CwLogicRun> CwCarrierDiscriminator::flush(double timestampSec) {
  std::vector<CwLogicRun> completed;
  if (!m_segmentBeam.empty() && !m_pendingSamples.empty()) {
    const double best = m_segmentBeam.front().logScore;
    std::vector<double> weights;
    weights.reserve(m_segmentBeam.size());
    double total = 0.0;
    for (const SegmentHypothesis& hypothesis : m_segmentBeam) {
      const double weight = std::exp(
          std::max(-60.0, hypothesis.logScore - best));
      weights.push_back(weight);
      total += weight;
    }
    if (total <= 0.0) total = 1.0;

    const std::size_t count = m_pendingSamples.size();
    for (std::size_t index = 0U; index < count; ++index) {
      double markMass = 0.0;
      for (std::size_t path = 0U; path < m_segmentBeam.size(); ++path) {
        const auto& states = m_segmentBeam[path].unresolvedStates;
        if (index < states.size() && states[index]) markMass += weights[path];
      }
      const double posterior = clampd(markMass / total, 0.001, 0.999);
      const bool mark = posterior >= 0.50;
      const PendingSample& sample = m_pendingSamples[index];
      const double confidence = clampd(
          0.55 * sample.confidence +
          0.45 * std::abs(2.0 * posterior - 1.0),
          0.0, 1.0);
      if (const auto run = commitResolvedState(
              mark, posterior, sample.observation, confidence,
              sample.qsbProbability); run.has_value()) {
        completed.push_back(*run);
      }
    }
  }
  m_pendingSamples.clear();
  m_segmentBeam.clear();

  if (m_haveActiveRun && m_runSamples > 0) {
    completed.push_back(finishRun(std::max(
        timestampSec, m_resolvedTimestampSec)));
  }
  m_runSamples = 0;
  m_haveActiveRun = false;
  m_keyDown = false;
  m_instantKeyDown = false;
  return completed;
}

bool CwCarrierDiscriminator::keyDown() const { return m_instantKeyDown; }
double CwCarrierDiscriminator::markProbability() const { return m_probability; }
double CwCarrierDiscriminator::confidence() const { return m_confidence; }
double CwCarrierDiscriminator::noiseLevel() const { return std::exp(m_noiseLog); }
double CwCarrierDiscriminator::markLevel() const { return std::exp(m_markLog); }

} // namespace madmodem::cwskimmer
