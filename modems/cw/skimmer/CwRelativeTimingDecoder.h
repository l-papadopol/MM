#pragma once

#include "CwCarrierDiscriminator.h"
#include "CwMorseBeamDecoder.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

namespace madmodem::cwskimmer {

enum class CwRelativeTimingState {
  Search,
  PairLock,
  Track,
  Reacquire
};

struct CwRelativeTimingConfig {
  double initialWpm = 20.0;
  bool autoWpm = true;
  double pairRatio = 1.80;
  std::size_t maxRollingText = 160;
};

struct CwRelativeTimingResult {
  std::string committedText;
  // One entry per committed character.  A word-space uses an empty entry.
  // Keeping this metadata beside committedText prevents the asynchronous task
  // from logging a character together with the next, still-open Morse pattern.
  std::vector<std::string> committedPatterns;
  std::string partialText;
  std::string rollingText;
  std::string currentPattern;
  CwRelativeTimingState state = CwRelativeTimingState::Search;
  double wpm = 20.0;
  double confidence = 0.0;
  double sequenceConfidence = 0.0;
  double sequenceBestPosterior = 0.0;
  double sequencePosteriorOddsDb = 0.0;
  std::size_t sequenceHypotheses = 0U;
  double ditMs = 60.0;
  double dahMs = 180.0;
  double markThresholdMs = 104.0;
  double elementSpaceMs = 60.0;
  double characterSpaceMs = 180.0;
  double wordSpaceMs = 420.0;
  std::uint64_t temporalEpoch = 0U;
  int temporalModel = 0;
  double inferredDitMs = 0.0;
};

/**
 * Relative temporal CW decoder.
 *
 * The decoder consumes completed MARK/SPACE runs. It never sees audio or FFT
 * data. Short/long MARK families are acquired continuously from informative
 * neighbouring pairs. SPACE families are learned independently by relative
 * likelihood. A bounded Bayesian CwMorseBeamDecoder replays the still-
 * uncommitted observations whenever the timing model changes. Each path owns
 * duration posteriors and publication is based on credible posterior mass.
 */
class CwRelativeTimingDecoder {
public:
  explicit CwRelativeTimingDecoder(CwRelativeTimingConfig config = {});

  void setConfig(const CwRelativeTimingConfig& config);
  const CwRelativeTimingConfig& config() const;
  void reset(bool keepTimingPrior = false);
  void beginEpoch(double shortMarkMs, double longMarkMs,
                  double elementSpaceMs,
                  bool keepContinuityAlternative = true);

  CwRelativeTimingResult processRun(const CwLogicRun& run);
  CwRelativeTimingResult advance(double timestampSec, bool keyDown);
  CwRelativeTimingResult flush(double timestampSec = 0.0);

  CwRelativeTimingResult snapshot() const;
  static const char* stateName(CwRelativeTimingState state);

private:
  void sanitize();
  CwRelativeTimingResult processStableRun(const CwLogicRun& run);
  bool shouldMergeQsbSplit(const CwLogicRun& first,
                           const CwLogicRun& gap,
                           const CwLogicRun& second) const;
  CwLogicRun mergeQsbSplit(const CwLogicRun& first,
                           const CwLogicRun& gap,
                           const CwLogicRun& second) const;
  double qsbRepairGapLimitMs() const;
  void absorbCommitted(CwRelativeTimingResult& aggregate,
                       const CwRelativeTimingResult& part) const;
  bool updatePair(double previousMarkMs, double currentMarkMs,
                  double separatingSpaceMs, double weight);
  void installFreshEpoch(double shortMarkMs, double longMarkMs,
                         double elementSpaceMs, double weight,
                         bool keepContinuityAlternative,
                         bool replayProbeRuns);
  void rememberEpochProbe(const CwLogicRun& run, bool timingTrusted);
  void replayEpochProbe();
  void updateMarkCluster(bool shortMark, double durationMs, double weight);
  void updateSpaceCluster(double durationMs, double weight);
  void refreshThresholds();
  void maybeCommitWordSpace(CwRelativeTimingResult& result);
  CwMorseTimingSnapshot beamTiming() const;
  CwMorseObservationQuality beamQuality(const CwLogicRun& run) const;
  void absorbBeam(CwRelativeTimingResult& result,
                  const CwMorseBeamResult& beamResult);
  double robustCenter(const std::deque<double>& values, double fallback) const;
  double markClassificationConfidence(double durationMs) const;

private:
  CwRelativeTimingConfig m_config;
  CwMorseBeamDecoder m_beamDecoder;
  CwRelativeTimingState m_state = CwRelativeTimingState::Search;

  std::deque<double> m_shortMarks;
  std::deque<double> m_longMarks;
  std::deque<double> m_elementSpaces;
  std::deque<double> m_characterSpaces;
  std::deque<double> m_wordSpaces;

  double m_shortMeanMs = 60.0;
  double m_longMeanMs = 180.0;
  double m_elementSpaceMs = 60.0;
  double m_characterSpaceMs = 180.0;
  double m_wordSpaceMs = 420.0;
  double m_markThresholdMs = 104.0;
  double m_characterThresholdMs = 104.0;
  double m_wordThresholdMs = 275.0;
  double m_wpm = 20.0;
  double m_confidence = 0.0;

  std::string m_rolling;
  bool m_characterCommittedInOpenSpace = false;
  bool m_wordCommittedInOpenSpace = false;
  double m_openSpaceStartSec = -1.0;
  double m_lastTimestampSec = 0.0;
  double m_previousMarkMs = 0.0;
  double m_spaceBetweenMarksMs = 0.0;
  bool m_havePreviousMark = false;
  bool m_previousMarkTrusted = false;
  bool m_bridgeRejectedMark = false;
  double m_rejectedMarkBridgeMs = 0.0;
  int m_pairEvidence = 0;
  int m_missEvidence = 0;
  std::uint64_t m_temporalEpoch = 0U;
  bool m_epochReplayedCurrentRun = false;
  double m_timingSpaceBetweenMarksMs = 0.0;
  std::deque<CwLogicRun> m_epochProbeRuns;
  std::optional<CwLogicRun> m_recoverableRejectedMark;
  std::optional<CwLogicRun> m_recoverableRejectedSpace;

  // One-element look-ahead keeps very short OFF notches inside a MARK from
  // becoming false Morse spaces. The runs are still timestamped by the carrier
  // discriminator; only their temporal interpretation is deferred.
  std::optional<CwLogicRun> m_deferredMark;
  std::optional<CwLogicRun> m_deferredSpace;
};

} // namespace madmodem::cwskimmer
