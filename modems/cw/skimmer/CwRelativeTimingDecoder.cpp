#include "CwRelativeTimingDecoder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace madmodem::cwskimmer {
namespace {
inline double clampd(double value, double low, double high) {
  return std::max(low, std::min(high, value));
}

std::string trimRolling(std::string value, std::size_t maxLen) {
  if (maxLen > 0U && value.size() > maxLen)
    value.erase(0, value.size() - maxLen);
  return value;
}
}

CwRelativeTimingDecoder::CwRelativeTimingDecoder(
    CwRelativeTimingConfig config)
    : m_config(config) {
  sanitize();
  reset(false);
}

void CwRelativeTimingDecoder::setConfig(const CwRelativeTimingConfig& config) {
  m_config = config;
  sanitize();
  if (!m_config.autoWpm) reset(false);
}

const CwRelativeTimingConfig& CwRelativeTimingDecoder::config() const {
  return m_config;
}

void CwRelativeTimingDecoder::sanitize() {
  m_config.initialWpm = clampd(m_config.initialWpm, 5.0, 50.0);
  m_config.pairRatio = clampd(m_config.pairRatio, 1.55, 2.40);
  m_config.maxRollingText = std::max<std::size_t>(32U, m_config.maxRollingText);
}

void CwRelativeTimingDecoder::reset(bool keepTimingPrior) {
  const double priorDit = 1200.0 / m_config.initialWpm;
  const bool trustedPrior = keepTimingPrior && m_pairEvidence >= 3 &&
      m_shortMeanMs >= 24.0 && m_longMeanMs >= 1.65 * m_shortMeanMs;
  const int retainedPairEvidence = trustedPrior ? std::max(3, m_pairEvidence) : 0;
  const double retainedConfidence = trustedPrior
      ? std::max(0.55, m_confidence) : 0.10;
  if (!keepTimingPrior || !(m_shortMeanMs > 5.0)) {
    m_shortMarks.clear();
    m_longMarks.clear();
    m_elementSpaces.clear();
    m_characterSpaces.clear();
    m_wordSpaces.clear();
    m_shortMeanMs = priorDit;
    m_longMeanMs = 3.0 * priorDit;
    m_elementSpaceMs = priorDit;
    m_characterSpaceMs = 3.0 * priorDit;
    m_wordSpaceMs = 7.0 * priorDit;
    m_wpm = m_config.initialWpm;
  }
  m_confidence = retainedConfidence;
  m_state = trustedPrior ? CwRelativeTimingState::Track
                         : CwRelativeTimingState::Search;
  m_beamDecoder.reset();
  m_rolling.clear();
  m_characterCommittedInOpenSpace = false;
  m_wordCommittedInOpenSpace = false;
  m_openSpaceStartSec = -1.0;
  m_lastTimestampSec = 0.0;
  m_previousMarkMs = 0.0;
  m_spaceBetweenMarksMs = 0.0;
  m_havePreviousMark = false;
  m_previousMarkTrusted = false;
  m_bridgeRejectedMark = false;
  m_rejectedMarkBridgeMs = 0.0;
  m_pairEvidence = retainedPairEvidence;
  m_provisionalRebaseEvidence = 0;
  m_provisionalRebaseShortMs = 0.0;
  m_provisionalRebaseLongMs = 0.0;
  m_provisionalRebaseSpaceMs = 0.0;
  m_missEvidence = 0;
  m_epochReplayedCurrentRun = false;
  m_timingSpaceBetweenMarksMs = 0.0;
  m_epochProbeRuns.clear();
  m_recoverableRejectedMark.reset();
  m_recoverableRejectedSpace.reset();
  if (!keepTimingPrior) m_temporalEpoch = 0U;
  m_deferredMark.reset();
  m_deferredSpace.reset();
  refreshThresholds();
}

void CwRelativeTimingDecoder::beginEpoch(
    double shortMarkMs, double longMarkMs, double elementSpaceMs,
    bool keepContinuityAlternative) {
  installFreshEpoch(shortMarkMs, longMarkMs, elementSpaceMs, 0.86,
                    keepContinuityAlternative, false);
}

void CwRelativeTimingDecoder::installFreshEpoch(
    double shortMarkMs, double longMarkMs, double elementSpaceMs,
    double weight, bool keepContinuityAlternative,
    bool replayProbeRuns) {
  shortMarkMs = clampd(shortMarkMs, 8.0, 260.0);
  longMarkMs = clampd(longMarkMs,
                      std::max(1.65 * shortMarkMs, 20.0),
                      std::min(850.0, 4.20 * shortMarkMs));
  elementSpaceMs = clampd(elementSpaceMs,
                          0.30 * shortMarkMs,
                          2.10 * shortMarkMs);

  const CwMorseTimingSnapshot continuity = beamTiming();
  m_beamDecoder.reset();
  if (keepContinuityAlternative && m_pairEvidence >= 2)
    m_beamDecoder.beginEpoch(continuity, 0.30);

  m_shortMarks.assign(6U, shortMarkMs);
  m_longMarks.assign(6U, longMarkMs);
  m_elementSpaces.assign(6U, elementSpaceMs);
  m_characterSpaces.clear();
  m_wordSpaces.clear();
  m_shortMeanMs = shortMarkMs;
  m_longMeanMs = longMarkMs;
  m_elementSpaceMs = elementSpaceMs;
  m_characterSpaceMs = 3.0 * shortMarkMs;
  m_wordSpaceMs = 7.0 * shortMarkMs;
  m_pairEvidence = 1;
  m_provisionalRebaseEvidence = 0;
  m_provisionalRebaseShortMs = 0.0;
  m_provisionalRebaseLongMs = 0.0;
  m_provisionalRebaseSpaceMs = 0.0;
  m_missEvidence = 0;
  m_state = CwRelativeTimingState::PairLock;
  m_confidence = clampd(0.38 + 0.44 * weight, 0.42, 0.86);
  m_bridgeRejectedMark = false;
  m_rejectedMarkBridgeMs = 0.0;
  m_recoverableRejectedMark.reset();
  m_recoverableRejectedSpace.reset();
  m_characterCommittedInOpenSpace = false;
  m_wordCommittedInOpenSpace = false;
  ++m_temporalEpoch;
  refreshThresholds();

  if (replayProbeRuns) {
    replayEpochProbe();
    m_epochReplayedCurrentRun = true;
  }
}

void CwRelativeTimingDecoder::rememberEpochProbe(
    const CwLogicRun& run, bool timingTrusted) {
  if (run.mark) {
    if (!timingTrusted) return;
    // No supported decoder speed has a dit below 24 ms.  Sub-15 ms MARKs in
    // the live capture were Schmitt/ringing holes inside otherwise normal
    // gaps; retaining them in the reversible epoch probe manufactured dots at
    // the front of the recovered C.  Real QSB splits are merged by the
    // one-element look-ahead before reaching this function.
    if (run.durationMs < 15.0) return;
    m_epochProbeRuns.push_back(run);
  } else {
    if (m_epochProbeRuns.empty()) return;
    const double boundaryMs = std::max(90.0, 2.20 * m_shortMeanMs);
    if (run.durationMs >= boundaryMs) {
      // A character/turn boundary starts a fresh local probe.  Do not carry the
      // previous station's last element into the next operator's first pair.
      m_epochProbeRuns.clear();
      return;
    }
    m_epochProbeRuns.push_back(run);
  }
  while (m_epochProbeRuns.size() > 16U) m_epochProbeRuns.pop_front();
}

void CwRelativeTimingDecoder::replayEpochProbe() {
  if (m_epochProbeRuns.empty()) return;
  bool haveAcceptedMark = false;
  std::optional<CwLogicRun> pendingSpace;
  const auto flushPendingSpace = [&]() {
    if (!haveAcceptedMark || !pendingSpace.has_value()) return;
    m_beamDecoder.observeSpace(pendingSpace->durationMs, beamTiming(),
                               beamQuality(*pendingSpace), false);
    pendingSpace.reset();
  };
  for (const CwLogicRun& probe : m_epochProbeRuns) {
    if (probe.mark) {
      const bool fitsShort = probe.durationMs >= 0.52 * m_shortMeanMs &&
                             probe.durationMs <= 1.68 * m_shortMeanMs;
      const bool fitsLong = probe.durationMs >= 0.52 * m_longMeanMs &&
                            probe.durationMs <= 1.68 * m_longMeanMs;
      if (!fitsShort && !fitsLong) {
        // A rejected MARK lies inside what is now known to be one OFF run.
        // Bridge its duration into the surrounding SPACE instead of replaying
        // it as an extra dot under the new clock.
        if (haveAcceptedMark) {
          if (!pendingSpace.has_value()) {
            pendingSpace = probe;
            pendingSpace->mark = false;
            pendingSpace->meanMarkProbability =
                1.0 - probe.meanMarkProbability;
          }
          else {
            pendingSpace->endSec = probe.endSec;
            pendingSpace->durationMs += probe.durationMs;
            pendingSpace->mark = false;
          }
        }
        continue;
      }
      flushPendingSpace();
      m_beamDecoder.observeMark(probe.durationMs, beamTiming(),
                                beamQuality(probe));
      haveAcceptedMark = true;
    } else if (haveAcceptedMark) {
      if (!pendingSpace.has_value()) pendingSpace = probe;
      else {
        pendingSpace->endSec = probe.endSec;
        pendingSpace->durationMs += probe.durationMs;
        pendingSpace->confidence =
            std::min(pendingSpace->confidence, probe.confidence);
        pendingSpace->coherence =
            std::min(pendingSpace->coherence, probe.coherence);
      }
    }
  }
  flushPendingSpace();
}

const char* CwRelativeTimingDecoder::stateName(CwRelativeTimingState state) {
  switch (state) {
    case CwRelativeTimingState::Search: return "SEARCH";
    case CwRelativeTimingState::PairLock: return "PAIR LOCK";
    case CwRelativeTimingState::Track: return "TRACK";
    case CwRelativeTimingState::Reacquire: return "REACQUIRE";
  }
  return "SEARCH";
}

double CwRelativeTimingDecoder::robustCenter(
    const std::deque<double>& values, double fallback) const {
  if (values.empty()) return fallback;
  std::vector<double> sorted(values.begin(), values.end());
  std::sort(sorted.begin(), sorted.end());
  const std::size_t n = sorted.size();
  const double median = n % 2U != 0U ? sorted[n / 2U]
      : 0.5 * (sorted[n / 2U - 1U] + sorted[n / 2U]);
  if (n < 5U) return median;
  std::vector<double> deviations;
  deviations.reserve(n);
  for (double value : sorted) deviations.push_back(std::abs(value - median));
  std::sort(deviations.begin(), deviations.end());
  const double mad = std::max(1.0, deviations[deviations.size() / 2U]);
  double sum = 0.0;
  double weight = 0.0;
  for (double value : sorted) {
    const double z = std::abs(value - median) / (3.5 * mad);
    const double w = z >= 1.0 ? 0.0 : (1.0 - z * z) * (1.0 - z * z);
    sum += w * value;
    weight += w;
  }
  return weight > 0.1 ? sum / weight : median;
}

void CwRelativeTimingDecoder::refreshThresholds() {
  m_shortMeanMs = clampd(m_shortMeanMs, 24.0, 260.0);
  m_longMeanMs = clampd(m_longMeanMs,
                        std::max(m_shortMeanMs * 1.65, 40.0),
                        std::min(850.0, 4.20 * m_shortMeanMs));
  m_elementSpaceMs = clampd(m_elementSpaceMs, 12.0, 300.0);
  m_characterSpaceMs = clampd(m_characterSpaceMs,
                              std::max(1.45 * m_elementSpaceMs, 30.0), 1100.0);
  m_wordSpaceMs = clampd(m_wordSpaceMs,
                         std::max(1.55 * m_characterSpaceMs, 80.0), 3000.0);
  m_markThresholdMs = std::sqrt(m_shortMeanMs * m_longMeanMs);
  m_characterThresholdMs = std::sqrt(m_elementSpaceMs * m_characterSpaceMs);
  m_wordThresholdMs = std::sqrt(m_characterSpaceMs * m_wordSpaceMs);
  m_wpm = clampd(1200.0 / m_shortMeanMs, 5.0, 50.0);
}

void CwRelativeTimingDecoder::updateMarkCluster(
    bool shortMark, double durationMs, double weight) {
  std::deque<double>& values = shortMark ? m_shortMarks : m_longMarks;
  const double centre = shortMark ? m_shortMeanMs : m_longMeanMs;
  const bool established = m_pairEvidence >= 3;
  const double low = (established ? 0.50 : (shortMark ? 0.35 : 0.40)) * centre;
  const double high = (established ? 1.65 : (shortMark ? 1.90 : 1.85)) * centre;
  if (!values.empty() && (durationMs < low || durationMs > high)) return;
  const int copies = weight >= 0.75 ? 2 : 1;
  for (int i = 0; i < copies; ++i) values.push_back(durationMs);
  while (values.size() > 15U) values.pop_front();
  const double candidate = robustCenter(values, centre);
  if (established) {
    // Once a station clock is trustworthy, follow real hand-key drift but do
    // not let a handful of post-carrier fragments move the model by tens of
    // WPM.  Re-acquisition of a new transmission starts with pairEvidence=0
    // and is therefore still immediate.
    const double maximumStep = std::max(0.8, 0.015 * centre);
    const double bounded = centre + clampd(candidate - centre,
                                           -maximumStep, maximumStep);
    if (shortMark) m_shortMeanMs = bounded;
    else m_longMeanMs = bounded;
  } else if (shortMark) {
    m_shortMeanMs = candidate;
  } else {
    m_longMeanMs = candidate;
  }
}

bool CwRelativeTimingDecoder::updatePair(
    double previousMarkMs, double currentMarkMs,
    double separatingSpaceMs, double weight) {
  const double shortMs = std::min(previousMarkMs, currentMarkMs);
  const double longMs = std::max(previousMarkMs, currentMarkMs);
  if (shortMs < 8.0 || longMs > 1200.0) {
    m_provisionalRebaseEvidence = 0;
    return false;
  }
  const double ratio = longMs / shortMs;
  if (ratio < m_config.pairRatio || ratio > 4.20) {
    // Rebase confirmation is deliberately consecutive.  A dash/dash pair or
    // an extreme fragment between two superficially similar candidates proves
    // they do not describe one coherent new station clock.
    m_provisionalRebaseEvidence = 0;
    return false;
  }
  // A relative pair is informative only when the OFF interval is itself a
  // plausible intra-character gap. Tiny Schmitt holes inside a noisy carrier
  // must not be allowed to manufacture a new high-speed clock.
  if (separatingSpaceMs < 0.32 * shortMs ||
      separatingSpaceMs > 2.10 * shortMs) {
    m_provisionalRebaseEvidence = 0;
    return false;
  }

  const double pairThreshold = std::sqrt(shortMs * longMs);
  const bool thresholdOutsidePair =
      m_markThresholdMs <= shortMs || m_markThresholdMs >= longMs;
  const bool established = m_pairEvidence >= 3;
  const double shortScale = shortMs / std::max(1.0, m_shortMeanMs);
  const double longScale = longMs / std::max(1.0, m_longMeanMs);
  const double geometricScale = std::sqrt(shortScale * longScale);
  const bool largeFamilyContradiction = thresholdOutsidePair ||
      geometricScale <= 0.76 || geometricScale >= 1.30 ||
      shortScale <= 0.62 || shortScale >= 1.48 ||
      longScale <= 0.62 || longScale >= 1.48;

  const auto clearRebaseCandidate = [&]() {
    m_provisionalRebaseEvidence = 0;
    m_provisionalRebaseShortMs = 0.0;
    m_provisionalRebaseLongMs = 0.0;
    m_provisionalRebaseSpaceMs = 0.0;
  };
  const auto confirmRebaseCandidate = [&](bool keepContinuityAlternative) {
    if (weight < 0.68) return false;
    const bool agreesWithCandidate = m_provisionalRebaseEvidence > 0 &&
        shortMs >= 0.72 * m_provisionalRebaseShortMs &&
        shortMs <= 1.38 * m_provisionalRebaseShortMs &&
        longMs >= 0.72 * m_provisionalRebaseLongMs &&
        longMs <= 1.38 * m_provisionalRebaseLongMs;
    if (agreesWithCandidate) {
      ++m_provisionalRebaseEvidence;
      m_provisionalRebaseShortMs =
          0.5 * (m_provisionalRebaseShortMs + shortMs);
      m_provisionalRebaseLongMs =
          0.5 * (m_provisionalRebaseLongMs + longMs);
      m_provisionalRebaseSpaceMs =
          0.5 * (m_provisionalRebaseSpaceMs + separatingSpaceMs);
    } else {
      m_provisionalRebaseEvidence = 1;
      m_provisionalRebaseShortMs = shortMs;
      m_provisionalRebaseLongMs = longMs;
      m_provisionalRebaseSpaceMs = separatingSpaceMs;
    }
    if (m_provisionalRebaseEvidence < 2) return false;
    installFreshEpoch(m_provisionalRebaseShortMs,
                      m_provisionalRebaseLongMs,
                      m_provisionalRebaseSpaceMs, weight,
                      keepContinuityAlternative, true);
    return true;
  };

  if (!established && m_pairEvidence > 0) {
    // A provisional clock is deliberately easy to replace, but replacing only
    // its numeric centres is not enough: the Bayesian beam would keep the
    // uncommitted MARK/SPACE fragments collected under the discarded scale.
    // The supplied live capture did exactly this, moving from a false 29/68 ms
    // acquisition to the real 68/210 ms station while retaining a bogus Morse
    // prefix, which was later published as _/R/E around a clean CQ.
    //
    // Treat a strong contradictory pair as an atomic epoch replacement.  The
    // current contiguous probe (including the leading element) is replayed at
    // the new scale; the untrusted provisional model is not kept as a
    // continuity alternative.
    if (largeFamilyContradiction) {
      // A stale spectrum decision can label one ringing fragment as centred.
      // Require the same contradictory geometry twice before replacing a
      // provisional clock.  A real C or Q supplies this evidence within one
      // character, so acquisition latency remains below the character gap.
      if (confirmRebaseCandidate(false)) return true;
      return false;
    }
    clearRebaseCandidate();
  }
  if (established) {
    // The relative-pair bootstrap may relocate the threshold immediately only
    // before a trustworthy clock exists.  On RF audio, a single fragmented
    // noise pair must not replace an established 60/180 ms timing model with
    // a 16/28 ms model and pin Auto-WPM at its upper limit.
    const bool consistentShort = shortMs >= 0.62 * m_shortMeanMs &&
                                 shortMs <= 1.48 * m_shortMeanMs;
    const bool consistentLong = longMs >= 0.62 * m_longMeanMs &&
                                longMs <= 1.48 * m_longMeanMs;
    const bool consistentThreshold = pairThreshold >= 0.72 * m_markThresholdMs &&
                                     pairThreshold <= 1.38 * m_markThresholdMs;
    // A genuine speed change scales dit and dah together.  A clipped leading
    // or trailing edge instead shortens mainly the dit, which was slowly
    // dragging a stable 24 WPM clock toward 27 WPM in live reception.  Such a
    // pair remains usable for text classification but is not timing evidence.
    const bool commonScale = std::abs(shortScale - longScale) <= 0.075;

    // Preserve the fast path for an established, well-formed clock change:
    // when dit and dah scale together, one centred pair is already two
    // independent duration observations.  The two-pair candidate below is for
    // malformed old clocks such as 24/40 ms, where the real 75/208 ms geometry
    // necessarily scales the two families by different factors.
    const bool epochScaleAgreement =
        std::abs(std::log(std::max(0.05, shortScale) /
                          std::max(0.05, longScale))) <= std::log(1.20);
    const bool largeScaleChange = geometricScale <= 0.76 ||
                                  geometricScale >= 1.30;
    if (epochScaleAgreement && largeScaleChange && weight >= 0.68) {
      installFreshEpoch(shortMs, longMs, separatingSpaceMs, weight,
                        true, true);
      return true;
    }

    if (thresholdOutsidePair || !consistentShort || !consistentLong ||
        !consistentThreshold || !commonScale) {
      // An established but malformed fast clock (for example the 24/40 ms
      // false lock in the supplied recording) does not scale dit and dah by
      // the same factor when the real station arrives.  Two mutually
      // consistent contradictory pairs are therefore stronger evidence than
      // agreement with the old ratio.  Keep the old clock only as the weak
      // Bayesian continuity alternative after the replacement.
      if (largeFamilyContradiction && confirmRebaseCandidate(true)) return true;
      ++m_missEvidence;
      if (m_missEvidence >= 8) m_state = CwRelativeTimingState::Reacquire;
      return false;
    }
    clearRebaseCandidate();
  }

  if ((!established && thresholdOutsidePair) || m_pairEvidence == 0) {
    m_shortMarks.assign(6U, shortMs);
    m_longMarks.assign(6U, longMs);
    m_shortMeanMs = shortMs;
    m_longMeanMs = longMs;
    m_markThresholdMs = pairThreshold;
  } else {
    updateMarkCluster(true, shortMs, weight);
    updateMarkCluster(false, longMs, weight);
  }

  m_elementSpaces.push_back(separatingSpaceMs);
  while (m_elementSpaces.size() > 15U) m_elementSpaces.pop_front();
  m_elementSpaceMs = robustCenter(m_elementSpaces, shortMs);
  ++m_pairEvidence;
  m_missEvidence = 0;
  m_state = m_pairEvidence >= 3 ? CwRelativeTimingState::Track
                               : CwRelativeTimingState::PairLock;
  if (m_pairEvidence >= 3) m_beamDecoder.clearContinuityAlternative();
  m_confidence = clampd(0.25 + 0.12 * m_pairEvidence + 0.35 * weight,
                        0.0, 1.0);
  refreshThresholds();
  return false;
}

void CwRelativeTimingDecoder::updateSpaceCluster(
    double durationMs, double weight) {
  const auto logCost = [durationMs](double centre, double sigma) {
    const double ratio = std::max(1.0, durationMs) /
                         std::max(1.0, centre);
    const double z = std::log(ratio) / sigma;
    return 0.5 * z * z;
  };

  // Select the spacing family by relative likelihood rather than by crossing a
  // single geometric threshold.  Ambiguous observations near two families are
  // deliberately weak/no training samples; the sequence beam may still keep
  // both boundary interpretations alive.
  // Score every SPACE against both the measured family and the canonical
  // 1/3/7-unit fallback at the acquired dit scale.  The initial UI WPM remains
  // a useful prior, but can no longer dominate a station that is much slower or
  // faster.  Soft semi-Markov floors keep a jittered 1-unit gap from becoming a
  // character/word boundary merely because one absolute centre is uncertain.
  const double dit = std::max(1.0, m_shortMeanMs);
  const double units = durationMs / dit;
  const auto belowFamilyPenalty = [units](double minimumUnits,
                                           double sigma) {
    if (units >= minimumUnits) return 0.0;
    const double z = std::log(minimumUnits / std::max(0.10, units)) / sigma;
    return 0.5 * z * z;
  };
  const std::array<double, 3> centres = {
      m_elementSpaceMs, m_characterSpaceMs, m_wordSpaceMs};
  const double elementCost = std::min(
      logCost(m_elementSpaceMs, 0.28),
      logCost(dit, 0.38) + 0.03);
  const double characterCost = std::min(
      logCost(m_characterSpaceMs, 0.38),
      logCost(3.0 * dit, 0.48) + 0.03) +
      belowFamilyPenalty(1.70, 0.22) + 0.05;
  const double wordCost = std::min(
      logCost(m_wordSpaceMs, 0.50),
      logCost(7.0 * dit, 0.62) + 0.04) +
      belowFamilyPenalty(4.00, 0.24) + 0.10;
  const std::array<double, 3> costs = {
      elementCost, characterCost, wordCost};
  std::array<std::size_t, 3> order = {0U, 1U, 2U};
  std::sort(order.begin(), order.end(), [&](std::size_t left, std::size_t right) {
    return costs[left] < costs[right];
  });
  const double bestCost = costs[order[0U]];
  const double margin = costs[order[1U]] - bestCost;
  if (bestCost > 6.0 || (margin < 0.10 && weight < 0.82)) return;

  std::deque<double>* target = order[0U] == 0U ? &m_elementSpaces
      : (order[0U] == 1U ? &m_characterSpaces : &m_wordSpaces);
  const double centre = centres[order[0U]];
  const double lowScale = order[0U] == 2U ? 0.45 : 0.50;
  const double highScale = order[0U] == 2U ? 1.85 : 1.70;
  if (!target->empty() &&
      (durationMs < lowScale * centre || durationMs > highScale * centre))
    return;

  const int copies = weight >= 0.75 && margin >= 0.25 ? 2 : 1;
  for (int i = 0; i < copies; ++i) target->push_back(durationMs);
  while (target->size() > 15U) target->pop_front();
  if (target == &m_elementSpaces)
    m_elementSpaceMs = robustCenter(*target, m_elementSpaceMs);
  else if (target == &m_characterSpaces)
    m_characterSpaceMs = robustCenter(*target, m_characterSpaceMs);
  else
    m_wordSpaceMs = robustCenter(*target, m_wordSpaceMs);
  refreshThresholds();
}

double CwRelativeTimingDecoder::markClassificationConfidence(
    double durationMs) const {
  const double shortDistance = std::abs(std::log(
      std::max(1.0, durationMs) / std::max(1.0, m_shortMeanMs)));
  const double longDistance = std::abs(std::log(
      std::max(1.0, durationMs) / std::max(1.0, m_longMeanMs)));
  const double separation = std::max(0.05,
      std::abs(std::log(m_longMeanMs / m_shortMeanMs)));
  return clampd(std::abs(longDistance - shortDistance) / separation, 0.0, 1.0);
}

void CwRelativeTimingDecoder::maybeCommitWordSpace(
    CwRelativeTimingResult& result) {
  if (m_wordCommittedInOpenSpace) return;
  CwMorseObservationQuality quality;
  quality.confidence = 0.95;
  quality.coherence = 0.90;
  quality.snrDb = 20.0;
  quality.carrierCentered = true;
  absorbBeam(result, m_beamDecoder.observeSpace(
      std::max(m_wordSpaceMs, 7.0 * m_shortMeanMs),
      beamTiming(), quality, true));
  m_characterCommittedInOpenSpace = true;
  m_wordCommittedInOpenSpace = true;
}

CwMorseTimingSnapshot CwRelativeTimingDecoder::beamTiming() const {
  CwMorseTimingSnapshot timing;
  timing.ditMs = m_shortMeanMs;
  timing.dahMs = m_longMeanMs;
  timing.elementSpaceMs = m_elementSpaceMs;
  timing.characterSpaceMs = m_characterSpaceMs;
  timing.wordSpaceMs = m_wordSpaceMs;
  timing.timingConfidence = m_confidence;
  return timing;
}

CwMorseObservationQuality CwRelativeTimingDecoder::beamQuality(
    const CwLogicRun& run) const {
  CwMorseObservationQuality quality;
  quality.confidence = run.confidence;
  quality.coherence = run.coherence;
  quality.snrDb = run.meanSnrDb;
  quality.carrierCentered = run.carrierCentered ||
                            run.carrierCenteredFraction >= 0.55;
  quality.stateProbability = run.mark
      ? run.meanMarkProbability : (1.0 - run.meanMarkProbability);
  quality.qsbProbability = run.qsbProbability;
  quality.noiseProbability = run.noiseProbability;
  quality.centeredProbability = run.mark
      ? run.carrierCenteredFraction
      : clampd(0.65 * run.confidence + 0.35 * (1.0 - run.noiseProbability),
               0.0, 1.0);
  return quality;
}

void CwRelativeTimingDecoder::absorbBeam(
    CwRelativeTimingResult& result, const CwMorseBeamResult& beamResult) {
  if (beamResult.committedText.empty()) return;
  result.committedText += beamResult.committedText;
  result.committedPatterns.insert(result.committedPatterns.end(),
                                  beamResult.committedPatterns.begin(),
                                  beamResult.committedPatterns.end());
  m_rolling = trimRolling(m_rolling + beamResult.committedText,
                          m_config.maxRollingText);
}

double CwRelativeTimingDecoder::qsbRepairGapLimitMs() const {
  // A genuine intra-element gap is centred near one dit.  Only notches clearly
  // shorter than that family are eligible for repair.  The absolute clamp
  // keeps the rule useful from slow hand CW to high-speed machine CW.
  return clampd(0.56 * m_elementSpaceMs, 9.0, 78.0);
}

bool CwRelativeTimingDecoder::shouldMergeQsbSplit(
    const CwLogicRun& first, const CwLogicRun& gap,
    const CwLogicRun& second) const {
  if (!first.mark || gap.mark || !second.mark) return false;
  if (gap.durationMs <= 0.0 || gap.durationMs > qsbRepairGapLimitMs())
    return false;

  const double fragmentMin = std::max(1.0,
      std::min(first.durationMs, second.durationMs));
  const double fragmentMax = std::max(first.durationMs, second.durationMs);
  const double combinedMs = first.durationMs + gap.durationMs + second.durationMs;

  // The two visible fragments must themselves look like portions of a short or
  // long MARK rather than two complete, well-separated Morse elements.
  const bool fragmentsPlausible =
      fragmentMax <= std::max(1.18 * m_markThresholdMs,
                              1.95 * m_shortMeanMs);
  const bool combinedLooksLong =
      combinedMs >= std::max(0.78 * m_markThresholdMs,
                             1.55 * m_shortMeanMs) &&
      combinedMs <= std::max(1.80 * m_longMeanMs,
                             4.8 * m_shortMeanMs);

  // A deep QSB hole often retains coherence but lasts only a fraction of the
  // normal element gap.  qsbErasure is useful when available, while the strict
  // relative-duration test also covers notches that fell fully below the
  // detector threshold and therefore could not be tagged in advance.
  const bool similarFragments = fragmentMax / fragmentMin <= 1.45;
  const bool erasureEvidence = first.qsbErasure || gap.qsbErasure ||
      second.qsbErasure ||
      (similarFragments &&
       (gap.durationMs <= 0.50 * m_elementSpaceMs ||
        gap.durationMs <= 0.36 * fragmentMin));
  const bool coherentFragments =
      0.5 * (first.coherence + second.coherence) >= 0.42 ||
      0.5 * (first.confidence + second.confidence) >= 0.78;

  return fragmentsPlausible && combinedLooksLong && erasureEvidence &&
         coherentFragments;
}

CwLogicRun CwRelativeTimingDecoder::mergeQsbSplit(
    const CwLogicRun& first, const CwLogicRun& gap,
    const CwLogicRun& second) const {
  CwLogicRun merged;
  merged.startSec = first.startSec;
  merged.endSec = second.endSec;
  merged.mark = true;
  merged.durationMs = first.durationMs + gap.durationMs + second.durationMs;
  const double firstWeight = std::max(1.0, first.durationMs);
  const double secondWeight = std::max(1.0, second.durationMs);
  const double weight = firstWeight + secondWeight;
  merged.confidence = clampd(
      (firstWeight * first.confidence + secondWeight * second.confidence) /
          weight,
      0.0, 1.0);
  merged.meanSnrDb =
      (firstWeight * first.meanSnrDb + secondWeight * second.meanSnrDb) /
      weight;
  merged.peakSnrDb = std::max(first.peakSnrDb, second.peakSnrDb);
  merged.coherence = clampd(
      (firstWeight * first.coherence + secondWeight * second.coherence) /
          weight,
      0.0, 1.0);
  merged.qsbErasure = true;
  merged.carrierCenteredFraction = clampd(
      (firstWeight * first.carrierCenteredFraction +
       secondWeight * second.carrierCenteredFraction) / weight,
      0.0, 1.0);
  merged.meanMarkProbability = clampd(
      (firstWeight * first.meanMarkProbability +
       secondWeight * second.meanMarkProbability) / weight,
      0.001, 0.999);
  merged.qsbProbability = clampd(
      std::max({first.qsbProbability, gap.qsbProbability,
                second.qsbProbability}),
      0.0, 1.0);
  merged.noiseProbability = clampd(
      (firstWeight * first.noiseProbability +
       secondWeight * second.noiseProbability) / weight,
      0.0, 1.0);
  merged.carrierCentered = first.carrierCentered || second.carrierCentered ||
      merged.carrierCenteredFraction >= 0.55;
  merged.carrierSessionQualified = first.carrierSessionQualified ||
                                   second.carrierSessionQualified;
  return merged;
}

void CwRelativeTimingDecoder::absorbCommitted(
    CwRelativeTimingResult& aggregate,
    const CwRelativeTimingResult& part) const {
  aggregate.committedText += part.committedText;
  aggregate.committedPatterns.insert(aggregate.committedPatterns.end(),
                                     part.committedPatterns.begin(),
                                     part.committedPatterns.end());
}

CwRelativeTimingResult CwRelativeTimingDecoder::processStableRun(
    const CwLogicRun& run) {
  CwRelativeTimingResult result = snapshot();
  m_lastTimestampSec = std::max(m_lastTimestampSec, run.endSec);
  const double runWeight = clampd(0.45 * run.confidence +
                                  0.25 * run.coherence +
                                  0.20 * clampd((run.meanSnrDb + 3.0) / 15.0, 0.0, 1.0) +
                                  0.10 * (run.carrierCentered ? 1.0 : 0.0),
                                  0.05, 1.0);

  if (run.mark) {
    if (run.durationMs < 5.0) return result;

    // The temporal task is allowed to learn or publish only from MARKs that
    // were supported by the selected narrow carrier for most of their run.
    // Local coherent/residual ratios can be high in filtered noise and are not
    // a substitute for an actual centred PSD lane.
    const bool timingTrusted = run.carrierCentered ||
                               run.carrierCenteredFraction >= 0.55;

    // The fastest supported clock has a 24 ms dit.  A centred 5-14 ms pulse is
    // therefore detector chatter, not a Morse element.  Do not let it replace
    // the previous real MARK used by relative-pair acquisition; bridge it into
    // the surrounding OFF time so the true dash/dit pair remains adjacent.
    if (run.durationMs < 15.0) {
      m_bridgeRejectedMark = true;
      m_rejectedMarkBridgeMs += run.durationMs;
      return result;
    }

    const bool preliminaryFitsShort =
        run.durationMs >= 0.52 * m_shortMeanMs &&
        run.durationMs <= 1.68 * m_shortMeanMs;
    const bool preliminaryFitsLong =
        run.durationMs >= 0.52 * m_longMeanMs &&
        run.durationMs <= 1.68 * m_longMeanMs;
    const bool marginalShortFragment = preliminaryFitsShort &&
        run.durationMs < 0.62 * m_shortMeanMs && !preliminaryFitsLong;
    const bool marginalFragmentSupported = !marginalShortFragment ||
        (run.confidence >= 0.86 && run.coherence >= 0.50);
    const bool preliminaryDurationPlausible =
        (preliminaryFitsShort || preliminaryFitsLong) &&
        marginalFragmentSupported;
    const bool strongContradictoryMark = timingTrusted &&
        !preliminaryDurationPlausible &&
        run.confidence >= 0.86 && run.coherence >= 0.50 &&
        run.meanSnrDb >= 8.0 && run.noiseProbability <= 0.30;
    const bool pairEndpointTrusted = timingTrusted &&
        (preliminaryDurationPlausible || strongContradictoryMark);

    m_epochReplayedCurrentRun = false;
    // The PSD lane can become centred one or two elements after a new station
    // starts.  Keep a very strong coherent leading MARK in the reversible probe
    // buffer even when it is not yet allowed to update timing.  It is replayed
    // only if a later fully-centred short/long pair proves a fresh epoch.
    const bool epochProbeEligible = pairEndpointTrusted ||
        (run.confidence >= 0.90 && run.coherence >= 0.60 &&
         run.meanSnrDb >= 8.0 && run.noiseProbability <= 0.30);
    rememberEpochProbe(run, epochProbeEligible);

    // Recover a strong leading element that arrived before the PSD lane became
    // centred.  It was not allowed to alter timing, but when the following
    // centred MARK and the intervening one-unit SPACE agree with the established
    // local geometry, replay it into the beam in the correct order.
    if (timingTrusted && m_pairEvidence >= 1 && m_confidence >= 0.40 &&
        m_recoverableRejectedMark.has_value() &&
        m_recoverableRejectedSpace.has_value()) {
      const CwLogicRun& leading = *m_recoverableRejectedMark;
      const CwLogicRun& gap = *m_recoverableRejectedSpace;
      const auto fitsCurrentMark = [&](double durationMs) {
        return (durationMs >= 0.68 * m_shortMeanMs &&
                durationMs <= 1.42 * m_shortMeanMs) ||
               (durationMs >= 0.68 * m_longMeanMs &&
                durationMs <= 1.42 * m_longMeanMs);
      };
      const bool oneElementGap = gap.durationMs >= 0.45 * m_elementSpaceMs &&
                                 gap.durationMs <= 1.65 * m_elementSpaceMs;
      if (fitsCurrentMark(leading.durationMs) &&
          fitsCurrentMark(run.durationMs) && oneElementGap &&
          leading.confidence >= 0.90 && leading.coherence >= 0.60 &&
          leading.noiseProbability <= 0.30) {
        absorbBeam(result, m_beamDecoder.observeMark(
            leading.durationMs, beamTiming(), beamQuality(leading)));
        absorbBeam(result, m_beamDecoder.observeSpace(
            gap.durationMs, beamTiming(), beamQuality(gap), false));
        m_bridgeRejectedMark = false;
        m_rejectedMarkBridgeMs = 0.0;
      }
      m_recoverableRejectedMark.reset();
      m_recoverableRejectedSpace.reset();
    }
    if (m_havePreviousMark && m_previousMarkTrusted && pairEndpointTrusted) {
      updatePair(m_previousMarkMs, run.durationMs,
                 m_timingSpaceBetweenMarksMs, runWeight);
    }

    // The pair update above may have installed a fresh epoch.  Re-evaluate the
    // current MARK against that local geometry rather than against the previous
    // operator's clock.
    const bool established = m_pairEvidence >= 3;
    const bool fitsShort = run.durationMs >= 0.52 * m_shortMeanMs &&
                           run.durationMs <= 1.68 * m_shortMeanMs;
    const bool fitsLong = run.durationMs >= 0.52 * m_longMeanMs &&
                          run.durationMs <= 1.68 * m_longMeanMs;
    const bool currentMarginalShortFragment = fitsShort &&
        run.durationMs < 0.62 * m_shortMeanMs && !fitsLong;
    const bool currentMarginalSupported = !currentMarginalShortFragment ||
        (run.confidence >= 0.86 && run.coherence >= 0.50);
    const bool durationPlausible =
        (fitsShort || fitsLong) && currentMarginalSupported;
    const bool textTrusted = timingTrusted ||
        (established && run.carrierSessionQualified && durationPlausible &&
         run.confidence >= 0.76 && run.coherence >= 0.20);

    if (pairEndpointTrusted) {
      m_previousMarkMs = run.durationMs;
      m_previousMarkTrusted = true;
      m_havePreviousMark = true;
    } else if (!timingTrusted) {
      m_previousMarkTrusted = false;
    }

    // A PSD-qualified active carrier may rescue the text decision under QSB or
    // adjacent-channel masking, but only a genuinely centred MARK can update
    // dit/dah timing. When the carrier lane disappears, neither condition is
    // true and receiver-idle pulses are bridged into SPACE.
    if (!textTrusted || (established && !durationPlausible)) {
      if (epochProbeEligible) {
        m_recoverableRejectedMark = run;
        m_recoverableRejectedSpace.reset();
      }
      // A fully centred, high-quality MARK that contradicts an established
      // clock is a possible first element from a new operator.  Keep it out of
      // the text beam until the new geometry is confirmed, but do not erase it
      // into the surrounding SPACE: doing so made the following real dit/dah
      // pair appear to have an impossibly long separating gap.  Unqualified
      // carrier fragments are still bridged exactly as before.
      if (!strongContradictoryMark) {
        m_bridgeRejectedMark = true;
        m_rejectedMarkBridgeMs += run.durationMs;
      }
      return result;
    }

    const double classificationConfidence = markClassificationConfidence(run.durationMs);
    // Do not move the clock from an isolated MARK.  Dit/dah adaptation is
    // deliberately pair-only: a short and a long element separated by a
    // plausible intra-character SPACE must agree before timing changes. This
    // prevents a long idle train of small noise pulses from walking the short
    // cluster toward 24 ms and pinning Auto-WPM at 50.

    if (!m_epochReplayedCurrentRun) {
      absorbBeam(result, m_beamDecoder.observeMark(
          run.durationMs, beamTiming(), beamQuality(run)));
    }
    m_openSpaceStartSec = run.endSec;
    m_characterCommittedInOpenSpace = false;
    m_wordCommittedInOpenSpace = false;

    if (classificationConfidence < 0.12) {
      ++m_missEvidence;
      if (m_missEvidence >= 4 && m_state == CwRelativeTimingState::Track)
        m_state = CwRelativeTimingState::Reacquire;
    } else {
      m_missEvidence = std::max(0, m_missEvidence - 1);
      if (m_state == CwRelativeTimingState::Reacquire && m_pairEvidence >= 2)
        m_state = CwRelativeTimingState::PairLock;
    }
  } else {
    rememberEpochProbe(run, false);
    if (m_recoverableRejectedMark.has_value()) {
      const double maximumRecoverableGap =
          std::max(1.75 * m_elementSpaceMs, 1.35 * m_shortMeanMs);
      if (run.durationMs <= maximumRecoverableGap)
        m_recoverableRejectedSpace = run;
      else {
        m_recoverableRejectedMark.reset();
        m_recoverableRejectedSpace.reset();
      }
    }
    double effectiveSpaceMs = run.durationMs;
    if (m_bridgeRejectedMark) {
      effectiveSpaceMs += m_rejectedMarkBridgeMs + m_spaceBetweenMarksMs;
      m_bridgeRejectedMark = false;
      m_rejectedMarkBridgeMs = 0.0;
    }
    m_timingSpaceBetweenMarksMs = effectiveSpaceMs;
    m_spaceBetweenMarksMs = effectiveSpaceMs;
    if (m_provisionalRebaseEvidence > 0 &&
        effectiveSpaceMs >= std::max(
            90.0, 2.20 * m_provisionalRebaseShortMs)) {
      m_provisionalRebaseEvidence = 0;
    }
    // Long off-air pauses are useful for committing a word, but they are not
    // training samples. Learn SPACE families only near an actual trusted Morse
    // sequence and never from an arbitrarily long receiver-idle interval.
    const double maximumTrainingSpace = std::max(2.4 * m_wordSpaceMs,
                                                 14.0 * m_shortMeanMs);
    if (m_pairEvidence > 0 && (m_havePreviousMark || !m_beamDecoder.snapshot().currentPattern.empty()) &&
        effectiveSpaceMs <= maximumTrainingSpace) {
      updateSpaceCluster(effectiveSpaceMs, runWeight);
    }
    if (!m_wordCommittedInOpenSpace) {
      // A completed OFF run carries the same information as the live open-gap
      // timer.  Previously only advance() could force an unambiguous word
      // boundary; if the worker received the completed SPACE first, this path
      // merely marked the boundary as "already committed" even when the beam
      // had selected a character gap.  The result was CQCQ for an exact
      // 7-dit "CQ CQ" separation.
      const double canonicalWordThresholdMs =
          std::sqrt(21.0) * std::max(1.0, m_shortMeanMs);
      const double adaptiveWordThresholdMs =
          std::max(m_wordThresholdMs, canonicalWordThresholdMs);
      const bool forceWordBoundary =
          effectiveSpaceMs >= adaptiveWordThresholdMs;
      const CwMorseBeamResult beamResult = m_beamDecoder.observeSpace(
          effectiveSpaceMs, beamTiming(), beamQuality(run),
          forceWordBoundary);
      const bool committedWord =
          beamResult.committedText.find(' ') != std::string::npos;
      const bool committedCharacter = std::any_of(
          beamResult.committedText.begin(), beamResult.committedText.end(),
          [](char value) { return value != ' '; });
      absorbBeam(result, beamResult);
      m_characterCommittedInOpenSpace = committedCharacter || committedWord;
      m_wordCommittedInOpenSpace = committedWord;
    }
  }

  const std::string committed = result.committedText;
  const std::vector<std::string> committedPatterns = result.committedPatterns;
  result = snapshot();
  result.committedText = committed;
  result.committedPatterns = committedPatterns;
  return result;
}

CwRelativeTimingResult CwRelativeTimingDecoder::processRun(
    const CwLogicRun& run) {
  CwRelativeTimingResult aggregate = snapshot();
  aggregate.committedText.clear();
  aggregate.committedPatterns.clear();
  m_lastTimestampSec = std::max(m_lastTimestampSec, run.endSec);

  if (run.mark) {
    if (m_deferredMark.has_value() && m_deferredSpace.has_value()) {
      if (shouldMergeQsbSplit(*m_deferredMark, *m_deferredSpace, run)) {
        m_deferredMark = mergeQsbSplit(
            *m_deferredMark, *m_deferredSpace, run);
        m_deferredSpace.reset();
        m_openSpaceStartSec = run.endSec;
      } else {
        absorbCommitted(aggregate, processStableRun(*m_deferredMark));
        absorbCommitted(aggregate, processStableRun(*m_deferredSpace));
        m_deferredMark = run;
        m_deferredSpace.reset();
        m_openSpaceStartSec = run.endSec;
      }
    } else if (m_deferredMark.has_value()) {
      // Consecutive completed MARK runs should not normally occur, but keeping
      // their order is safer than silently replacing one.
      absorbCommitted(aggregate, processStableRun(*m_deferredMark));
      m_deferredMark = run;
      m_openSpaceStartSec = run.endSec;
    } else {
      m_deferredMark = run;
      m_openSpaceStartSec = run.endSec;
    }
  } else {
    if (m_deferredMark.has_value() && !m_deferredSpace.has_value()) {
      m_deferredSpace = run;
    } else if (m_deferredMark.has_value() && m_deferredSpace.has_value()) {
      absorbCommitted(aggregate, processStableRun(*m_deferredMark));
      absorbCommitted(aggregate, processStableRun(*m_deferredSpace));
      m_deferredMark.reset();
      m_deferredSpace.reset();
      absorbCommitted(aggregate, processStableRun(run));
    } else {
      absorbCommitted(aggregate, processStableRun(run));
    }
  }

  const std::string committed = aggregate.committedText;
  const std::vector<std::string> committedPatterns = aggregate.committedPatterns;
  aggregate = snapshot();
  aggregate.committedText = committed;
  aggregate.committedPatterns = committedPatterns;
  return aggregate;
}

CwRelativeTimingResult CwRelativeTimingDecoder::advance(
    double timestampSec, bool keyDown) {
  CwRelativeTimingResult aggregate = snapshot();
  aggregate.committedText.clear();
  aggregate.committedPatterns.clear();
  m_lastTimestampSec = std::max(m_lastTimestampSec, timestampSec);

  if (!keyDown && m_deferredMark.has_value() &&
      !m_deferredSpace.has_value()) {
    const double openAfterMarkMs = 1000.0 * std::max(
        0.0, timestampSec - m_deferredMark->endSec);
    if (openAfterMarkMs >= qsbRepairGapLimitMs()) {
      absorbCommitted(aggregate, processStableRun(*m_deferredMark));
      m_deferredMark.reset();
    }
  }

  if (!keyDown && m_openSpaceStartSec >= 0.0) {
    const double openSpaceMs = 1000.0 *
        std::max(0.0, timestampSec - m_openSpaceStartSec);
    // Do not force a word from a stale fast initial-WPM prior.  The learned
    // character/word midpoint is retained, with a canonical 1/3/7-unit lower
    // bound at the acquired dit scale. Completed gaps still go through the full
    // beam likelihood model below.
    const double canonicalWordThresholdMs =
        std::sqrt(21.0) * std::max(1.0, m_shortMeanMs);
    const double adaptiveWordThresholdMs =
        std::max(m_wordThresholdMs, canonicalWordThresholdMs);
    if (openSpaceMs >= adaptiveWordThresholdMs) {
      maybeCommitWordSpace(aggregate);
    }
  }

  const std::string committed = aggregate.committedText;
  const std::vector<std::string> committedPatterns = aggregate.committedPatterns;
  aggregate = snapshot();
  aggregate.committedText = committed;
  aggregate.committedPatterns = committedPatterns;
  return aggregate;
}

CwRelativeTimingResult CwRelativeTimingDecoder::flush(double timestampSec) {
  CwRelativeTimingResult aggregate = snapshot();
  aggregate.committedText.clear();
  aggregate.committedPatterns.clear();
  const double now = timestampSec > 0.0 ? timestampSec : m_lastTimestampSec;
  m_lastTimestampSec = std::max(m_lastTimestampSec, now);

  if (m_deferredMark.has_value()) {
    absorbCommitted(aggregate, processStableRun(*m_deferredMark));
    m_deferredMark.reset();
  }
  if (m_deferredSpace.has_value()) {
    absorbCommitted(aggregate, processStableRun(*m_deferredSpace));
    m_deferredSpace.reset();
  }

  absorbCommitted(aggregate, advance(now, false));
  absorbBeam(aggregate, m_beamDecoder.flush(beamTiming()));

  const std::string committed = aggregate.committedText;
  const std::vector<std::string> committedPatterns = aggregate.committedPatterns;
  aggregate = snapshot();
  aggregate.committedText = committed;
  aggregate.committedPatterns = committedPatterns;
  return aggregate;
}

CwRelativeTimingResult CwRelativeTimingDecoder::snapshot() const {
  CwRelativeTimingResult result;
  const CwMorseBeamResult beam = m_beamDecoder.snapshot();
  result.partialText = beam.currentPattern;
  result.rollingText = m_rolling;
  result.currentPattern = beam.currentPattern;
  result.state = m_state;
  result.wpm = m_wpm;
  result.confidence = m_confidence;
  result.sequenceConfidence = beam.confidence;
  result.sequenceBestPosterior = beam.bestPosterior;
  result.sequencePosteriorOddsDb = beam.posteriorOddsDb;
  result.sequenceHypotheses = beam.hypothesisCount;
  result.ditMs = m_shortMeanMs;
  result.dahMs = m_longMeanMs;
  result.markThresholdMs = m_markThresholdMs;
  result.elementSpaceMs = m_elementSpaceMs;
  result.characterSpaceMs = m_characterSpaceMs;
  result.wordSpaceMs = m_wordSpaceMs;
  result.temporalEpoch = m_temporalEpoch;
  result.temporalModel = beam.temporalModel;
  result.inferredDitMs = beam.inferredDitMs;
  return result;
}

} // namespace madmodem::cwskimmer
