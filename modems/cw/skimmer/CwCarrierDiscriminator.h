#pragma once

#include <cstddef>
#include <deque>
#include <optional>
#include <vector>

namespace madmodem::cwskimmer {

struct CwCarrierObservation {
  double timestampSec = 0.0;
  double envelope = 0.0;
  double acquisitionEnvelope = 0.0;
  double snrDb = -99.0;
  double coherence = 0.0;
  bool carrierCentered = false;
  // Soft temporal prior supplied by the single live CW timing owner.  WPM is
  // never used as a hard clock: low timingConfidence deliberately broadens
  // the semi-Markov duration families.
  double timingDitMs = 60.0;
  double timingConfidence = 0.0;
  // Probability that the selected narrow carrier is still the same active
  // session.  This lets the segmenter preserve a MARK alternative through a
  // short QSB notch without turning an ordinary inter-element SPACE into MARK.
  double carrierSessionProbability = 0.0;
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
  std::size_t segmentBeamWidth = 24U;
  int minimumFixedLagMs = 28;
  int maximumFixedLagMs = 180;
};

struct CwCarrierDiscriminatorResult {
  bool keyDown = false;
  // keyDown is the best present-time state, used by AFC/diagnostics.  The
  // resolved fields refer to the sample which has just left the fixed-lag
  // segmental beam and must be used to advance the temporal decoder.
  bool resolvedKeyDown = false;
  double resolvedTimestampSec = 0.0;
  int fixedLagMs = 0;
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
 * This class knows nothing about Morse symbols or characters.  It converts
 * one-millisecond soft carrier probabilities into timestamped MARK/SPACE runs
 * with a bounded fixed-lag explicit-duration (HSMM) beam.  The beam preserves
 * competing "real SPACE" and "QSB notch" paths until later samples resolve
 * the ambiguity; WPM remains only a broad duration prior.
 */
class CwCarrierDiscriminator {
public:
  explicit CwCarrierDiscriminator(CwCarrierDiscriminatorConfig config = {});

  void setConfig(const CwCarrierDiscriminatorConfig& config);
  const CwCarrierDiscriminatorConfig& config() const;
  void reset();

  CwCarrierDiscriminatorResult process(const CwCarrierObservation& observation);
  std::vector<CwLogicRun> flush(double timestampSec);

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
                double confidence, double markProbability,
                double qsbProbability);
  void accumulate(const CwCarrierObservation& observation,
                  double confidence, double markProbability,
                  double qsbProbability);
  std::optional<CwLogicRun> commitResolvedState(
      bool mark, double posteriorMarkProbability,
      const CwCarrierObservation& observation,
      double sampleConfidence, double qsbProbability);
  void advanceSegmentalBeam(const CwCarrierObservation& observation,
                            double markProbability,
                            double emissionErasureProbability,
                            double qsbProbability,
                            CwCarrierDiscriminatorResult& result);
  static double durationHazard(bool mark, int durationMs,
                               double ditMs, double timingConfidence);

  struct SegmentHypothesis {
    bool mark = false;
    int durationMs = 0;
    double logScore = 0.0;
    std::deque<bool> unresolvedStates;
  };

  struct PendingSample {
    CwCarrierObservation observation;
    double markProbability = 0.5;
    double qsbProbability = 0.0;
    double confidence = 0.0;
  };

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
  bool m_instantKeyDown = false;
  bool m_haveActiveRun = false;
  int m_fixedLagMs = 0;
  double m_resolvedTimestampSec = 0.0;
  std::vector<SegmentHypothesis> m_segmentBeam;
  std::deque<PendingSample> m_pendingSamples;
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
