#include "CwBayesianDecoder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace madmodem::cwskimmer {
namespace {

constexpr double kNegativeInfinity = -1.0e100;
constexpr double kMinimumUnitMs = 24.0;   // 50 WPM
constexpr double kMaximumUnitMs = 240.0;  // 5 WPM

const std::unordered_map<std::string, char>& morseTable() {
  static const std::unordered_map<std::string, char> table = {
      {".-", 'A'}, {"-...", 'B'}, {"-.-.", 'C'}, {"-..", 'D'}, {".", 'E'},
      {"..-.", 'F'}, {"--.", 'G'}, {"....", 'H'}, {"..", 'I'}, {".---", 'J'},
      {"-.-", 'K'}, {".-..", 'L'}, {"--", 'M'}, {"-.", 'N'}, {"---", 'O'},
      {".--.", 'P'}, {"--.-", 'Q'}, {".-.", 'R'}, {"...", 'S'}, {"-", 'T'},
      {"..-", 'U'}, {"...-", 'V'}, {".--", 'W'}, {"-..-", 'X'}, {"-.--", 'Y'},
      {"--..", 'Z'}, {"-----", '0'}, {".----", '1'}, {"..---", '2'},
      {"...--", '3'}, {"....-", '4'}, {".....", '5'}, {"-....", '6'},
      {"--...", '7'}, {"---..", '8'}, {"----.", '9'}, {".-.-.-", '.'},
      {"--..--", ','}, {"..--..", '?'}, {"-..-.", '/'}, {"-...-", '='},
      {".-.-.", '+'}, {"-....-", '-'}, {".--.-.", '@'}, {"---...", ':'},
      {"-.-.-.", ';'}, {"-.-.--", '!'}, {"-.--.", '('}, {"-.--.-", ')'},
      {".-..-.", '"'}, {".----.", '\''}, {"..--.-", '_'}
  };
  return table;
}

const std::unordered_set<std::string>& morsePrefixes() {
  static const std::unordered_set<std::string> prefixes = [] {
    std::unordered_set<std::string> out;
    out.insert("");
    for (const auto& item : morseTable()) {
      for (std::size_t i = 1; i <= item.first.size(); ++i) {
        out.insert(item.first.substr(0, i));
      }
    }
    return out;
  }();
  return prefixes;
}

double safeLog(double value) {
  return std::log(std::max(1.0e-12, value));
}

} // namespace

CwBayesianDecoder::CwBayesianDecoder(CwBayesianDecoderConfig config)
    : m_config(std::move(config)) {
  sanitizeConfig();
  reset();
}

void CwBayesianDecoder::sanitizeConfig() {
  m_config.initialWpm = clamp(m_config.initialWpm, 5.0, 50.0);
  m_config.beamWidth = std::max<std::size_t>(24, std::min<std::size_t>(160, m_config.beamWidth));
  m_config.maxRollingText = std::max<std::size_t>(32, m_config.maxRollingText);
}

void CwBayesianDecoder::resetBeam() {
  m_beam.clear();
  const double hintedUnit = clamp(1200.0 / m_config.initialWpm,
                                  kMinimumUnitMs, kMaximumUnitMs);

  // Auto-WPM is a weak prior, not a hard search window.  Seed the native beam
  // across the complete supported range so a wrong UI hint cannot lock the
  // decoder to half/double speed.  The hint only gives nearby hypotheses a
  // modest initial advantage.
  static constexpr std::array<double, 19> kSeedWpm = {
      5.0, 6.0, 7.5, 9.0, 11.0, 13.0, 15.0, 17.0, 19.0, 21.0,
      23.0, 25.0, 28.0, 31.0, 34.0, 38.0, 42.0, 46.0, 50.0};
  for (double wpm : kSeedWpm) {
    Hypothesis h;
    h.unitMs = clamp(1200.0 / wpm, kMinimumUnitMs, kMaximumUnitMs);
    const double ratio = std::log(h.unitMs / hintedUnit);
    h.score = -0.55 * ratio * ratio;
    m_beam.push_back(std::move(h));
  }

  Hypothesis exactHint;
  exactHint.unitMs = hintedUnit;
  exactHint.score = 0.0;
  m_beam.push_back(std::move(exactHint));
}


void CwBayesianDecoder::reset() {
  sanitizeConfig();
  resetBeam();
  m_haveRun = false;
  m_runState = RunState::Space;
  m_runMs = 0;
  m_runConfidenceSum = 0.0;
  m_runSnrSum = 0.0;
  m_runSamples = 0;
  m_haveCandidate = false;
  m_candidateMs = 0;
  m_candidateConfidenceSum = 0.0;
  m_candidateSnrSum = 0.0;
  m_committedRolling.clear();
  m_partial.clear();
  m_publicWpm = m_config.initialWpm;
  m_publicConfidence = 0.0;
  m_lastKeyDown = false;
}

void CwBayesianDecoder::setWpmHint(double wpm) {
  m_config.initialWpm = clamp(wpm, 5.0, 50.0);
  if (!m_config.autoWpm || m_beam.empty()) resetBeam();
}

void CwBayesianDecoder::setAutoWpm(bool enabled) {
  m_config.autoWpm = enabled;
  if (!enabled) resetBeam();
}

void CwBayesianDecoder::setMaxRollingText(std::size_t maxLen) {
  m_config.maxRollingText = std::max<std::size_t>(32, maxLen);
  trimRolling();
}

CwBayesianDecoderResult CwBayesianDecoder::process(
    const CwSoftObservation& observation) {
  CwBayesianDecoderResult result;
  consumeSoftSample(observation, result);
  updateCommitted(result, false);
  updatePublicState(result);
  return result;
}

void CwBayesianDecoder::consumeSoftSample(
    const CwSoftObservation& observation, CwBayesianDecoderResult& result) {
  const double confidence = clamp(observation.confidence, 0.0, 1.0);
  const double rawProbability = clamp(observation.markProbability, 0.001, 0.999);
  const double probability = 0.5 + confidence * (rawProbability - 0.5);

  RunState desired;
  bool decisive = false;
  if (probability >= 0.62) {
    desired = RunState::Mark;
    decisive = true;
  } else if (probability <= 0.38) {
    desired = RunState::Space;
    decisive = true;
  } else {
    desired = m_haveRun ? m_runState : RunState::Space;
  }

  if (!m_haveRun) {
    if (!decisive) return;
    m_haveRun = true;
    m_runState = desired;
    m_runMs = 1;
    m_runConfidenceSum = confidence;
    m_runSnrSum = clamp(observation.snrDb, -30.0, 50.0);
    m_runSamples = 1;
    m_lastKeyDown = m_runState == RunState::Mark;
    return;
  }

  if (!decisive || desired == m_runState) {
    if (m_haveCandidate) {
      m_runMs += m_candidateMs;
      m_runConfidenceSum += m_candidateConfidenceSum;
      m_runSnrSum += m_candidateSnrSum;
      m_runSamples += m_candidateMs;
      m_haveCandidate = false;
      m_candidateMs = 0;
      m_candidateConfidenceSum = 0.0;
      m_candidateSnrSum = 0.0;
    }
    ++m_runMs;
    m_runConfidenceSum += confidence;
    m_runSnrSum += clamp(observation.snrDb, -30.0, 50.0);
    ++m_runSamples;
    return;
  }

  if (!m_haveCandidate || desired != m_candidateState) {
    if (m_haveCandidate) {
      m_runMs += m_candidateMs;
      m_runConfidenceSum += m_candidateConfidenceSum;
      m_runSnrSum += m_candidateSnrSum;
      m_runSamples += m_candidateMs;
    }
    m_haveCandidate = true;
    m_candidateState = desired;
    m_candidateMs = 0;
    m_candidateConfidenceSum = 0.0;
    m_candidateSnrSum = 0.0;
  }
  ++m_candidateMs;
  m_candidateConfidenceSum += confidence;
  m_candidateSnrSum += clamp(observation.snrDb, -30.0, 50.0);

  const int confirmation = transitionConfirmationMs(m_runState, confidence);
  if (m_candidateMs < confirmation) return;

  finishCurrentInterval(result);
  m_runState = m_candidateState;
  m_runMs = m_candidateMs;
  m_runConfidenceSum = m_candidateConfidenceSum;
  m_runSnrSum = m_candidateSnrSum;
  m_runSamples = m_candidateMs;
  m_haveCandidate = false;
  m_candidateMs = 0;
  m_candidateConfidenceSum = 0.0;
  m_candidateSnrSum = 0.0;
  m_lastKeyDown = m_runState == RunState::Mark;
}

int CwBayesianDecoder::transitionConfirmationMs(RunState from,
                                                 double confidence) const {
  const double unit = clamp(1200.0 / std::max(5.0, m_publicWpm),
                            kMinimumUnitMs, kMaximumUnitMs);
  const double fraction = from == RunState::Mark ? 0.28 : 0.16;
  const double nominal = clamp(fraction * unit, from == RunState::Mark ? 7.0 : 5.0,
                               from == RunState::Mark ? 20.0 : 12.0);
  // Very clear opposite-state evidence may confirm slightly faster; low
  // confidence never causes a faster edge.
  const double scale = 1.08 - 0.18 * clamp(confidence, 0.0, 1.0);
  return static_cast<int>(std::lround(nominal * scale));
}

void CwBayesianDecoder::finishCurrentInterval(CwBayesianDecoderResult& result) {
  if (!m_haveRun || m_runMs <= 0) return;
  Interval interval;
  interval.state = m_runState;
  interval.durationMs = static_cast<double>(m_runMs);
  interval.confidence = m_runSamples > 0
      ? clamp(m_runConfidenceSum / static_cast<double>(m_runSamples), 0.0, 1.0) : 0.0;
  interval.snrDb = m_runSamples > 0
      ? m_runSnrSum / static_cast<double>(m_runSamples) : -99.0;
  processInterval(interval, result);
}

void CwBayesianDecoder::processInterval(const Interval& interval,
                                        CwBayesianDecoderResult& result) {
  std::vector<Hypothesis> candidates;
  candidates.reserve(m_beam.size() * 4U);
  if (interval.state == RunState::Mark) {
    processMark(interval, candidates);
  } else {
    processSpace(interval, candidates);
  }

  if (candidates.empty()) {
    // A single impossible interval must not resurrect a legacy decoder path or
    // poison the rest of the transmission.  Start a fresh native timing beam;
    // the tracker retains the carrier and pre-roll independently.
    resetBeam();
    m_partial.clear();
    return;
  }
  prune(candidates);
  m_beam.swap(candidates);
  updateCommitted(result, false);
}

void CwBayesianDecoder::processMark(
    const Interval& interval, std::vector<Hypothesis>& candidates) const {
  for (const auto& h : m_beam) {
    const double confidence = clamp(interval.confidence, 0.0, 1.0);
    const double sigmaScale = 1.0 + 0.55 * (1.0 - confidence);
    const struct Candidate { char code; double multiplier; double sigma; } classes[] = {
        {'.', 1.0, 0.31}, {'-', 3.0, 0.29}};
    for (const auto& cls : classes) {
      const std::string nextSymbol = h.symbol + cls.code;
      if (!isMorsePrefix(nextSymbol)) continue;
      const double likelihood = durationLogLikelihood(
          interval.durationMs, cls.multiplier * h.unitMs, cls.sigma * sigmaScale);
      if (likelihood <= kNegativeInfinity / 2.0) continue;
      Hypothesis next = h;
      next.symbol = nextSymbol;
      next.score += likelihood;
      ++next.acceptedElements;
      next.timingConfidence = 0.90 * next.timingConfidence + 0.10 * confidence;
      next = withUpdatedUnit(std::move(next), interval.durationMs / cls.multiplier,
                             confidence);
      candidates.push_back(std::move(next));
    }

    // A very short, low-confidence MARK may be a narrow-band impulse.  Keeping
    // the unchanged hypothesis is a Bayesian noise alternative, not a second
    // decoder or a hidden fallback.
    if (interval.durationMs < 0.48 * h.unitMs && confidence < 0.58) {
      Hypothesis ignored = h;
      ignored.score -= 3.2 + 2.0 * confidence;
      candidates.push_back(std::move(ignored));
    }
  }
}

void CwBayesianDecoder::processSpace(
    const Interval& interval, std::vector<Hypothesis>& candidates) const {
  for (const auto& h : m_beam) {
    if (h.symbol.empty()) {
      Hypothesis next = h;
      // Leading/inter-transmission silence carries no Morse meaning.
      next.score -= interval.durationMs < 0.35 * h.unitMs ? 0.5 : 0.0;
      candidates.push_back(std::move(next));
      continue;
    }

    const double confidence = clamp(interval.confidence, 0.0, 1.0);
    const double sigmaScale = 1.0 + 0.65 * (1.0 - confidence);

    // Element gap: retain the current character.
    {
      Hypothesis next = h;
      next.score += durationLogLikelihood(interval.durationMs, h.unitMs,
                                          0.34 * sigmaScale);
      next = withUpdatedUnit(std::move(next), interval.durationMs, confidence);
      candidates.push_back(std::move(next));
    }

    const std::string decoded = decodeSymbol(h.symbol);
    if (decoded.empty()) continue;

    // Character gap.
    {
      Hypothesis next = h;
      next.symbol.clear();
      next.text += decoded;
      next.score += durationLogLikelihood(interval.durationMs, 3.0 * h.unitMs,
                                          0.38 * sigmaScale);
      candidates.push_back(std::move(next));
    }

    // Word gap.  Only one normalized space is emitted.
    {
      Hypothesis next = h;
      next.symbol.clear();
      next.text += decoded;
      if (next.text.empty() || next.text.back() != ' ') next.text.push_back(' ');
      next.score += durationLogLikelihood(interval.durationMs, 7.0 * h.unitMs,
                                          0.50 * sigmaScale);
      candidates.push_back(std::move(next));
    }
  }
}

void CwBayesianDecoder::prune(std::vector<Hypothesis>& candidates) {
  std::sort(candidates.begin(), candidates.end(), [](const Hypothesis& a,
                                                      const Hypothesis& b) {
    if (a.score != b.score) return a.score > b.score;
    if (a.text.size() != b.text.size()) return a.text.size() > b.text.size();
    return a.symbol.size() > b.symbol.size();
  });

  std::unordered_map<std::string, std::size_t> merged;
  std::vector<Hypothesis> compact;
  compact.reserve(std::min<std::size_t>(candidates.size(), m_config.beamWidth * 2U));
  for (auto& h : candidates) {
    const int unitBucket = static_cast<int>(std::lround(h.unitMs * 0.5));
    std::string key;
    key.reserve(h.text.size() + h.symbol.size() + 24U);
    key += h.text;
    key.push_back('|');
    key += h.symbol;
    key.push_back('|');
    key += std::to_string(unitBucket);
    const auto it = merged.find(key);
    if (it == merged.end()) {
      merged.emplace(std::move(key), compact.size());
      compact.push_back(std::move(h));
    } else {
      auto& keep = compact[it->second];
      keep.score = logSumExp(keep.score, h.score);
    }
    if (compact.size() >= m_config.beamWidth * 3U) break;
  }
  std::sort(compact.begin(), compact.end(), [](const Hypothesis& a,
                                                const Hypothesis& b) {
    return a.score > b.score;
  });
  if (compact.size() > m_config.beamWidth) compact.resize(m_config.beamWidth);
  if (!compact.empty()) {
    const double offset = compact.front().score;
    for (auto& h : compact) h.score -= offset;
  }
  candidates.swap(compact);
}

void CwBayesianDecoder::updateCommitted(CwBayesianDecoderResult& result,
                                        bool force) {
  if (m_beam.empty()) return;

  std::size_t prefix = 0U;
  if (force) {
    prefix = m_beam.front().text.size();
  } else {
    // Commit as soon as the posterior mass agrees on a completed character.
    // The previous implementation required at least two common characters and
    // could therefore hold perfectly decoded text for many seconds.
    const std::size_t common = commonPrefixLength(m_beam, 2.8, 8);
    const std::size_t posterior = posteriorPrefixLength(m_beam, 0.74);
    prefix = std::max(common, posterior);
  }
  if (prefix == 0U) return;

  const std::string committed = m_beam.front().text.substr(0, prefix);
  m_beam.erase(std::remove_if(m_beam.begin(), m_beam.end(),
      [&](const Hypothesis& h) {
        return h.text.size() < prefix || h.text.compare(0, prefix, committed) != 0;
      }), m_beam.end());
  for (auto& h : m_beam) h.text.erase(0, prefix);
  m_committedRolling += committed;
  result.committedText += committed;
  trimRolling();
}


void CwBayesianDecoder::updatePublicState(CwBayesianDecoderResult& result) {
  if (m_beam.empty()) {
    result.rollingText = m_committedRolling;
    result.wpm = m_publicWpm;
    result.confidence = m_publicConfidence;
    result.keyDown = m_lastKeyDown;
    return;
  }
  const Hypothesis& best = m_beam.front();
  result.rollingText = m_committedRolling + best.text;
  result.partialText = best.symbol;
  result.confidence = clamp(0.18 + 0.58 * best.timingConfidence +
                            0.24 * std::min(1.0, best.acceptedElements / 14.0),
                            0.0, 1.0);
  if (m_beam.size() > 1U) {
    const double margin = m_beam[0].score - m_beam[1].score;
    result.confidence *= clamp(0.50 + 0.14 * margin, 0.50, 1.0);
  }

  // Keep one public clock per lane.  The best beam may change on every
  // millisecond; exposing that directly caused 27 -> 50 WPM jumps and changed
  // debounce timing mid-character.  Update only after committed text and bound
  // each step.
  const double candidateWpm = clamp(1200.0 / std::max(kMinimumUnitMs, best.unitMs),
                                    5.0, 50.0);
  if (!result.committedText.empty() && result.confidence >= 0.20) {
    const double maxStep = m_publicConfidence < 0.45 ? 1.5 : 0.65;
    m_publicWpm += clamp(candidateWpm - m_publicWpm, -maxStep, maxStep);
    m_publicWpm = clamp(m_publicWpm, 5.0, 50.0);
  }

  result.wpm = m_publicWpm;
  result.keyDown = m_lastKeyDown;
  result.activeHypotheses = static_cast<int>(m_beam.size());
  result.erasure = m_haveCandidate;

  m_partial = result.partialText;
  m_publicConfidence = result.confidence;
}


CwBayesianDecoderResult CwBayesianDecoder::flush(double timestampSec) {
  (void)timestampSec;
  CwBayesianDecoderResult result;
  if (m_haveCandidate) {
    m_runMs += m_candidateMs;
    m_runConfidenceSum += m_candidateConfidenceSum;
    m_runSnrSum += m_candidateSnrSum;
    m_runSamples += m_candidateMs;
    m_haveCandidate = false;
  }
  finishCurrentInterval(result);

  if (!m_beam.empty() && !m_beam.front().symbol.empty()) {
    const std::string decoded = decodeSymbol(m_beam.front().symbol);
    if (!decoded.empty()) {
      for (auto& h : m_beam) {
        if (!h.symbol.empty()) {
          const std::string tail = decodeSymbol(h.symbol);
          if (!tail.empty()) h.text += tail;
          h.symbol.clear();
        }
      }
    }
  }
  updateCommitted(result, true);
  updatePublicState(result);
  m_haveRun = false;
  m_runMs = 0;
  return result;
}

void CwBayesianDecoder::trimRolling() {
  if (m_committedRolling.size() > m_config.maxRollingText) {
    m_committedRolling.erase(0, m_committedRolling.size() - m_config.maxRollingText);
  }
}

double CwBayesianDecoder::durationLogLikelihood(double durationMs,
                                                 double targetMs,
                                                 double sigmaLog) {
  if (!(durationMs > 0.0) || !(targetMs > 0.0)) return kNegativeInfinity;
  const double ratio = std::max(0.04, durationMs / targetMs);
  const double sigma = std::max(0.08, sigmaLog);
  const double z = std::log(ratio) / sigma;
  // Cap the loss instead of deleting the entire beam.  Real hand keying and
  // QSB can produce outliers, while sequence-level evidence still resolves the
  // correct path.
  return std::max(-18.0, -0.5 * z * z - safeLog(sigma));
}

double CwBayesianDecoder::clamp(double value, double low, double high) {
  return std::max(low, std::min(high, value));
}

double CwBayesianDecoder::logSumExp(double a, double b) {
  if (a < b) std::swap(a, b);
  if (b <= kNegativeInfinity / 2.0) return a;
  return a + std::log1p(std::exp(b - a));
}

std::string CwBayesianDecoder::decodeSymbol(const std::string& symbol) {
  const auto it = morseTable().find(symbol);
  return it == morseTable().end() ? std::string() : std::string(1, it->second);
}

bool CwBayesianDecoder::isMorsePrefix(const std::string& symbol) {
  return morsePrefixes().find(symbol) != morsePrefixes().end();
}

std::size_t CwBayesianDecoder::commonPrefixLength(
    const std::vector<Hypothesis>& beam, double scoreWindow,
    std::size_t maxHypotheses) {
  if (beam.empty() || beam.front().text.empty()) return 0U;
  const double bestScore = beam.front().score;
  std::size_t count = 0U;
  std::size_t prefix = beam.front().text.size();
  for (const auto& h : beam) {
    if (count >= maxHypotheses || h.score < bestScore - scoreWindow) break;
    prefix = std::min(prefix, h.text.size());
    for (std::size_t i = 0; i < prefix; ++i) {
      if (h.text[i] != beam.front().text[i]) {
        prefix = i;
        break;
      }
    }
    ++count;
    if (prefix == 0U) break;
  }
  if (beam.size() > 1U && count < 2U) return 0U;
  return prefix;
}

std::size_t CwBayesianDecoder::posteriorPrefixLength(
    const std::vector<Hypothesis>& beam, double minimumPosterior) {
  if (beam.empty() || beam.front().text.empty()) return 0U;
  minimumPosterior = clamp(minimumPosterior, 0.50, 0.99);

  double totalMass = 0.0;
  for (const auto& h : beam) {
    if (h.score < -12.0) continue;
    totalMass += std::exp(h.score);
  }
  if (!(totalMass > 0.0)) return 0U;

  const std::string& bestText = beam.front().text;
  std::size_t accepted = 0U;
  for (std::size_t length = 1U; length <= bestText.size(); ++length) {
    double support = 0.0;
    for (const auto& h : beam) {
      if (h.score < -12.0 || h.text.size() < length) continue;
      if (h.text.compare(0, length, bestText, 0, length) == 0)
        support += std::exp(h.score);
    }
    if (support / totalMass < minimumPosterior) break;
    accepted = length;
  }
  return accepted;
}


CwBayesianDecoder::Hypothesis CwBayesianDecoder::withUpdatedUnit(
    Hypothesis h, double candidateUnitMs, double intervalConfidence) const {
  if (!m_config.autoWpm || intervalConfidence < 0.42 ||
      !std::isfinite(candidateUnitMs)) {
    return h;
  }
  candidateUnitMs = clamp(candidateUnitMs, kMinimumUnitMs, kMaximumUnitMs);
  const double ratio = candidateUnitMs / std::max(kMinimumUnitMs, h.unitMs);
  if (ratio < 0.58 || ratio > 1.72) return h;

  // Each hypothesis already represents a timing family.  Let it adapt slowly
  // to hand-keying, but never let one interval collapse the lane to double or
  // half speed.
  const double alpha = clamp(0.006 + 0.012 * intervalConfidence, 0.006, 0.018);
  const double desired = (1.0 - alpha) * h.unitMs + alpha * candidateUnitMs;
  const double maxStep = 0.018 * h.unitMs;
  h.unitMs = clamp(h.unitMs + clamp(desired - h.unitMs, -maxStep, maxStep),
                   kMinimumUnitMs, kMaximumUnitMs);
  return h;
}


double CwBayesianDecoder::wpm() const { return m_publicWpm; }
double CwBayesianDecoder::confidence() const { return m_publicConfidence; }
const std::string& CwBayesianDecoder::rollingText() const { return m_committedRolling; }
const std::string& CwBayesianDecoder::partialText() const { return m_partial; }

} // namespace madmodem::cwskimmer
