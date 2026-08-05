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
  m_haveActiveRun = false;
  m_candidateState = false;
  m_candidateStableMs = 0;
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
    double confidence, bool qsbErasure) {
  m_keyDown = mark;
  m_haveActiveRun = true;
  m_candidateState = mark;
  m_candidateStableMs = 0;
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
  accumulate(observation, confidence, qsbErasure);
}

void CwCarrierDiscriminator::accumulate(
    const CwCarrierObservation& observation,
    double confidence, bool qsbErasure) {
  m_runConfidenceSum += clampd(confidence, 0.0, 1.0);
  m_runSnrSum += observation.snrDb;
  m_runPeakSnr = std::max(m_runPeakSnr, observation.snrDb);
  m_runCoherenceSum += clampd(observation.coherence, 0.0, 1.0);
  ++m_runSamples;
  m_runQsbErasure = m_runQsbErasure || qsbErasure;
  if (observation.carrierCentered) ++m_runCarrierCenteredSamples;
  m_runMarkProbabilitySum += clampd(m_probability, 0.001, 0.999);
  m_runQsbProbabilitySum += qsbErasure ? 1.0 : 0.0;
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
    const double alpha = m_keyDown ? 0.018 : 0.035;
    m_markLog += alpha * (logEnvelope - m_markLog);
  } else if (m_probability < 0.42 || !m_keyDown) {
    const double delta = logEnvelope - m_noiseLog;
    const double alpha = delta < 0.0 ? 0.004 : 0.0004;
    m_noiseLog += alpha * delta;
  }

  // A short ambiguous dip inside a MARK is tagged as a possible erasure but is
  // not allowed to create an immediate SPACE edge.
  const bool aboveNoiseShoulder =
      logEnvelope > m_noiseLog + 0.20 * separation;
  const bool possibleQsb = m_keyDown && m_probability > 0.18 &&
      m_probability < 0.45 && m_confidence < 0.40 && aboveNoiseShoulder;

  const bool requestedState = m_keyDown ? (m_probability > 0.34)
                                        : (m_probability >= 0.66);
  if (!m_haveActiveRun) {
    // reset() can be called while the parent tracker is already many seconds
    // into a live stream.  The old implementation then accumulated a SPACE
    // whose start timestamp was still zero, so the first post-reset edge was
    // reported as a multi-second (or multi-minute) run.  Anchor the first run
    // to the current observation instead; there is no completed run to emit.
    startRun(requestedState, observation.timestampSec,
             observation, m_confidence, possibleQsb);
  } else if (requestedState == m_keyDown) {
    m_candidateState = m_keyDown;
    m_candidateStableMs = 0;
    accumulate(observation, m_confidence, possibleQsb);
  } else {
    if (m_candidateState != requestedState) {
      m_candidateState = requestedState;
      m_candidateStableMs = 1;
    } else {
      ++m_candidateStableMs;
    }

    int requiredMs = m_config.minimumStableMs;
    if (m_confidence > 0.80) requiredMs = std::max(3, requiredMs - 2);
    if (m_confidence < 0.35) requiredMs = std::min(18, requiredMs + 4);
    if (possibleQsb && !requestedState) requiredMs = std::min(18, requiredMs + 5);

    if (m_candidateStableMs >= requiredMs) {
      result.completedRun = finishRun(observation.timestampSec);
      result.transitioned = true;
      startRun(requestedState, observation.timestampSec,
               observation, m_confidence, possibleQsb);
    } else {
      accumulate(observation, m_confidence, possibleQsb);
    }
  }

  result.keyDown = m_keyDown;
  result.markProbability = m_probability;
  result.confidence = m_confidence;
  result.noiseLevel = std::exp(m_noiseLog);
  result.markLevel = std::exp(m_markLog);
  result.thresholdLow = std::exp(midpoint - 0.18);
  result.thresholdHigh = std::exp(midpoint + 0.18);
  result.qsbErasure = possibleQsb;
  return result;
}

std::optional<CwLogicRun> CwCarrierDiscriminator::flush(double timestampSec) {
  if (!m_haveActiveRun || m_runSamples <= 0) return std::nullopt;
  const CwLogicRun run = finishRun(timestampSec);
  m_runSamples = 0;
  m_haveActiveRun = false;
  return run;
}

bool CwCarrierDiscriminator::keyDown() const { return m_keyDown; }
double CwCarrierDiscriminator::markProbability() const { return m_probability; }
double CwCarrierDiscriminator::confidence() const { return m_confidence; }
double CwCarrierDiscriminator::noiseLevel() const { return std::exp(m_noiseLog); }
double CwCarrierDiscriminator::markLevel() const { return std::exp(m_markLog); }

} // namespace madmodem::cwskimmer
