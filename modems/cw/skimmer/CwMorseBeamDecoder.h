#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace madmodem::cwskimmer {

struct CwMorseTimingSnapshot {
  double ditMs = 60.0;
  double dahMs = 180.0;
  double elementSpaceMs = 60.0;
  double characterSpaceMs = 180.0;
  double wordSpaceMs = 420.0;
  double timingConfidence = 0.0;
};

struct CwMorseObservationQuality {
  double confidence = 0.0;
  double coherence = 0.0;
  double snrDb = -99.0;
  bool carrierCentered = false;

  // Optional posterior evidence supplied by the carrier discriminator. Values
  // below zero mean "derive a conservative probability from the legacy fields".
  double stateProbability = -1.0;
  double qsbProbability = -1.0;
  double noiseProbability = -1.0;
  double centeredProbability = -1.0;
};

struct CwMorseBeamConfig {
  std::size_t beamWidth = 16U;
  std::size_t maxPatternLength = 6U;
  std::size_t maxReplayEvents = 96U;

  // Commit uses posterior mass, not arbitrary score distance. A common prefix
  // is accepted across hypotheses covering credibleMass of the beam. A single
  // best path may commit only when it exceeds decisivePosterior and has enough
  // absolute observation evidence.
  double credibleMass = 0.985;
  double decisivePosterior = 0.992;
  double minimumCommitConfidence = 0.63;

  // Weak structural priors. RF/timing likelihood remains dominant.
  double dotPrior = 0.52;
  double dashPrior = 0.48;
  double elementSpacePrior = 0.67;
  double characterSpacePrior = 0.28;
  double wordSpacePrior = 0.05;
};

struct CwMorseBeamResult {
  std::string committedText;
  std::vector<std::string> committedPatterns;
  std::string currentPattern;
  double confidence = 0.0;
  double bestPosterior = 0.0;
  double posteriorOddsDb = 0.0;
  std::size_t hypothesisCount = 0U;
  // 0 = the current/local timing model, 1 = the weak continuity model kept
  // across a temporal epoch boundary.  This is diagnostic metadata only.
  int temporalModel = 0;
  double inferredDitMs = 0.0;
};

/**
 * Bounded Bayesian Morse sequence decoder.
 *
 * Each beam path owns five Normal-Inverse-Gamma duration posteriors (dit, dah,
 * element gap, character gap and word gap). Completed MARK/SPACE observations
 * update those posteriors with fractional evidence derived from SNR, coherence,
 * carrier centering and discriminator probabilities. Predictive Student-t
 * likelihoods make clipped edges and hand-key outliers inexpensive to survive
 * without letting them drag the station clock.
 *
 * The decoder retains a short replay window. Whenever the external timing prior
 * changes, all still-uncommitted observations are re-evaluated. Publication is
 * based on posterior mass shared by the best hypotheses, rather than on a fixed
 * duration threshold or a raw beam-score margin.
 */
class CwMorseBeamDecoder {
public:
  explicit CwMorseBeamDecoder(CwMorseBeamConfig config = {});

  void setConfig(const CwMorseBeamConfig& config);
  const CwMorseBeamConfig& config() const;
  void reset();

  // Start a new temporal epoch.  The caller's current timing snapshot becomes
  // the primary model on the next observation; the previous station clock is
  // retained only as a weak competing continuity hypothesis.
  void beginEpoch(const CwMorseTimingSnapshot& continuityTiming,
                  double continuityPrior = 0.30);
  void clearContinuityAlternative();

  CwMorseBeamResult observeMark(double durationMs,
                                const CwMorseTimingSnapshot& timing,
                                const CwMorseObservationQuality& quality);
  CwMorseBeamResult observeSpace(double durationMs,
                                 const CwMorseTimingSnapshot& timing,
                                 const CwMorseObservationQuality& quality,
                                 bool forceWordBoundary = false);
  CwMorseBeamResult flush(const CwMorseTimingSnapshot& timing);
  CwMorseBeamResult snapshot() const;

private:
  struct Event {
    bool mark = false;
    bool forceWordBoundary = false;
    double durationMs = 0.0;
    CwMorseObservationQuality quality;
  };

  struct Token {
    char value = '?';
    std::string pattern;
    std::size_t endEvent = 0U;

    bool operator==(const Token& other) const {
      return value == other.value && pattern == other.pattern &&
             endEvent == other.endEvent;
    }
  };

  // Normal-Inverse-Gamma posterior in log-duration space. Fractional updates
  // are used because carrier evidence is soft rather than binary.
  struct DurationPosterior {
    double meanLog = 0.0;
    double kappa = 1.0;
    double alpha = 3.0;
    double beta = 0.1;

    void initialize(double centreMs, double predictiveSigma,
                    double priorStrength);
    double logPredictive(double durationMs,
                         double extraSigma = 0.0) const;
    void update(double durationMs, double weight);
    double centreMs() const;
  };

  struct Hypothesis {
    std::vector<Token> tokens;
    std::string pattern;
    double logPosterior = 0.0;
    double evidence = 0.0;
    int observations = 0;
    DurationPosterior dit;
    DurationPosterior dah;
    DurationPosterior elementSpace;
    DurationPosterior characterSpace;
    DurationPosterior wordSpace;
    int temporalModel = 0;
  };

  struct QualityProbabilities {
    double reliability = 0.0;
    double state = 0.5;
    double qsb = 0.0;
    double noise = 0.5;
    double centered = 0.0;
  };

  void sanitize();
  void rebuild(const CwMorseTimingSnapshot& timing);
  void prune(std::vector<Hypothesis> next);
  CwMorseBeamResult collectStablePrefix(bool forceBest = false);
  Hypothesis initialHypothesis(const CwMorseTimingSnapshot& timing,
                               int temporalModel = 0) const;

  static bool isMorsePrefix(const std::string& pattern);
  static char decodePattern(const std::string& pattern);
  static std::string keyFor(const Hypothesis& hypothesis);
  static QualityProbabilities qualityProbabilities(
      const CwMorseObservationQuality& quality);
  static double logProbability(double probability);
  static double logAddExp(double left, double right);
  static double logMixture(double firstLogLikelihood, double firstWeight,
                           double secondLogLikelihood, double secondWeight);
  static double broadOutlierLogLikelihood(double durationMs,
                                           double referenceMs);
  static double softMinimumUnitsLogPrior(double units,
                                         double minimumUnits,
                                         double softness);
  static bool appendCharacter(Hypothesis& hypothesis, std::size_t endEvent);
  static void appendWordBoundary(Hypothesis& hypothesis,
                                 std::size_t endEvent);

private:
  CwMorseBeamConfig m_config;
  std::vector<Event> m_events;
  std::vector<Hypothesis> m_beam;
  CwMorseTimingSnapshot m_lastTiming;
  double m_confidence = 0.0;
  double m_bestPosterior = 0.0;
  double m_posteriorOddsDb = 0.0;
  bool m_haveContinuityTiming = false;
  CwMorseTimingSnapshot m_continuityTiming;
  double m_continuityPrior = 0.0;
};

} // namespace madmodem::cwskimmer
