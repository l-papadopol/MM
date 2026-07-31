#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace madmodem::cwskimmer {

struct CwSoftObservation {
  double timestampSec = 0.0;
  double markProbability = 0.5;
  double confidence = 0.0;
  double snrDb = -99.0;
  double coherence = 0.0;
};

struct CwBayesianDecoderConfig {
  double initialWpm = 20.0;
  bool autoWpm = true;
  std::size_t beamWidth = 72;
  std::size_t maxRollingText = 160;
};

struct CwBayesianDecoderResult {
  std::string committedText;
  std::string partialText;
  std::string rollingText;
  double wpm = 20.0;
  double confidence = 0.0;
  bool keyDown = false;
  bool erasure = false;
  int activeHypotheses = 0;
};

/**
 * Native MadModem soft-decision CW decoder.
 *
 * The input is a one-millisecond probability stream.  A small Schmitt/debounce
 * state estimator converts only sustained, high-confidence changes into raw
 * MARK/SPACE intervals.  Those measured intervals are never resized or merged.
 * A bounded Bayesian beam then assigns dot/dash and element/character/word-gap
 * meanings jointly, with a latent timing unit.  SNR affects confidence only.
 */
class CwBayesianDecoder {
public:
  explicit CwBayesianDecoder(CwBayesianDecoderConfig config = {});

  void reset();
  void setWpmHint(double wpm);
  void setAutoWpm(bool enabled);
  void setMaxRollingText(std::size_t maxLen);

  CwBayesianDecoderResult process(const CwSoftObservation& observation);
  CwBayesianDecoderResult flush(double timestampSec);

  double wpm() const;
  double confidence() const;
  const std::string& rollingText() const;
  const std::string& partialText() const;

private:
  enum class RunState { Space, Mark };

  struct Interval {
    RunState state = RunState::Space;
    double durationMs = 0.0;
    double confidence = 0.0;
    double snrDb = -99.0;
  };

  struct Hypothesis {
    double score = 0.0;
    double unitMs = 60.0;
    std::string symbol;
    std::string text;
    int acceptedElements = 0;
    double timingConfidence = 0.0;
  };

  void sanitizeConfig();
  void resetBeam();
  void consumeSoftSample(const CwSoftObservation& observation,
                         CwBayesianDecoderResult& result);
  void finishCurrentInterval(CwBayesianDecoderResult& result);
  void processInterval(const Interval& interval, CwBayesianDecoderResult& result);
  void processMark(const Interval& interval, std::vector<Hypothesis>& candidates) const;
  void processSpace(const Interval& interval, std::vector<Hypothesis>& candidates) const;
  void prune(std::vector<Hypothesis>& candidates);
  void updateCommitted(CwBayesianDecoderResult& result, bool force = false);
  void updatePublicState(CwBayesianDecoderResult& result);
  void trimRolling();

  static double durationLogLikelihood(double durationMs, double targetMs,
                                      double sigmaLog);
  static double clamp(double value, double low, double high);
  static double logSumExp(double a, double b);
  static std::string decodeSymbol(const std::string& symbol);
  static bool isMorsePrefix(const std::string& symbol);
  static std::size_t commonPrefixLength(const std::vector<Hypothesis>& beam,
                                        double scoreWindow,
                                        std::size_t maxHypotheses);
  static std::size_t posteriorPrefixLength(const std::vector<Hypothesis>& beam,
                                           double minimumPosterior);

  Hypothesis withUpdatedUnit(Hypothesis h, double candidateUnitMs,
                             double intervalConfidence) const;
  int transitionConfirmationMs(RunState from, double confidence) const;

  CwBayesianDecoderConfig m_config;
  std::vector<Hypothesis> m_beam;

  bool m_haveRun = false;
  RunState m_runState = RunState::Space;
  int m_runMs = 0;
  double m_runConfidenceSum = 0.0;
  double m_runSnrSum = 0.0;
  int m_runSamples = 0;

  bool m_haveCandidate = false;
  RunState m_candidateState = RunState::Space;
  int m_candidateMs = 0;
  double m_candidateConfidenceSum = 0.0;
  double m_candidateSnrSum = 0.0;

  std::string m_committedRolling;
  std::string m_partial;
  double m_publicWpm = 20.0;
  double m_publicConfidence = 0.0;
  bool m_lastKeyDown = false;
};

} // namespace madmodem::cwskimmer
