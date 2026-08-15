#pragma once

#include <cstddef>
#include <deque>
#include <optional>

namespace madmodem::cwskimmer {

struct CwCarrierObservation {
  double timestampSec = 0.0;
  double envelope = 0.0;
  double acquisitionEnvelope = 0.0;
  double snrDb = -99.0;
  double coherence = 0.0;
  bool carrierCentered = false;
};

struct CwLogicRun {
  double startSec = 0.0;
  double endSec = 0.0;
  bool mark = false;
  double durationMs = 0.0;
  double confidence = 0.0;
  double meanSnrDb = -99.0;
  double peakSnrDb = -99.0;
  double coherence = 0.0;
  bool qsbErasure = false;
  bool carrierCentered = false;
  double carrierCenteredFraction = 0.0;
  // Mean posterior from the 1 ms discriminator samples belonging to this run.
  // For a MARK run meanMarkProbability should be near one; for SPACE near zero.
  double meanMarkProbability = 0.5;
  double qsbProbability = 0.0;
  double noiseProbability = 0.5;
  // True when the tracker still sees the selected narrow PSD lane even if an
  // individual MARK was partially obscured by an adjacent carrier or QSB.
  // It may authorize text classification, never timing adaptation.
  bool carrierSessionQualified = false;
};

struct CwCarrierDiscriminatorConfig {
  double minSnrDb = 3.0;
  int minimumStableMs = 6;
  std::size_t historyMs = 2500;
};

struct CwCarrierDiscriminatorResult {
  bool keyDown = false;
  bool transitioned = false;
  std::optional<CwLogicRun> completedRun;
  double markProbability = 0.5;
  double confidence = 0.0;
  double noiseLevel = 0.0;
  double markLevel = 0.0;
  double thresholdLow = 0.0;
  double thresholdHigh = 0.0;
  bool qsbErasure = false;
};

/**
 * Exact-tone carrier discriminator.
 *
 * This class knows nothing about Morse symbols, WPM, characters or 1/3/7
 * timing. It only converts a narrow-band carrier envelope into timestamped
 * MARK/SPACE runs. The temporal decoder consumes those runs independently.
 */
class CwCarrierDiscriminator {
public:
  explicit CwCarrierDiscriminator(CwCarrierDiscriminatorConfig config = {});

  void setConfig(const CwCarrierDiscriminatorConfig& config);
  const CwCarrierDiscriminatorConfig& config() const;
  void reset();

  CwCarrierDiscriminatorResult process(const CwCarrierObservation& observation);
  std::optional<CwLogicRun> flush(double timestampSec);

  bool keyDown() const;
  double markProbability() const;
  double confidence() const;
  double noiseLevel() const;
  double markLevel() const;

private:
  void sanitize();
  CwLogicRun finishRun(double timestampSec) const;
  void startRun(bool mark, double timestampSec,
                const CwCarrierObservation& observation,
                double confidence, bool qsbErasure);
  void accumulate(const CwCarrierObservation& observation,
                  double confidence, bool qsbErasure);

private:
  CwCarrierDiscriminatorConfig m_config;
  std::deque<double> m_levelHistory;
  int m_levelUpdateCounter = 0;
  double m_noiseLog = 0.0;
  double m_markLog = 0.0;
  bool m_haveLevels = false;
  double m_probability = 0.5;
  double m_confidence = 0.0;

  bool m_keyDown = false;
  bool m_haveActiveRun = false;
  bool m_candidateState = false;
  int m_candidateStableMs = 0;
  double m_runStartSec = 0.0;
  double m_runConfidenceSum = 0.0;
  double m_runSnrSum = 0.0;
  double m_runPeakSnr = -99.0;
  double m_runCoherenceSum = 0.0;
  int m_runSamples = 0;
  bool m_runQsbErasure = false;
  int m_runCarrierCenteredSamples = 0;
  double m_runMarkProbabilitySum = 0.0;
  double m_runQsbProbabilitySum = 0.0;
};

} // namespace madmodem::cwskimmer
