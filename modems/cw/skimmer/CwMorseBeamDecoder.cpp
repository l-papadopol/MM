#include "CwMorseBeamDecoder.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace madmodem::cwskimmer {
namespace {
constexpr double kLogFloor = -80.0;

inline double clampd(double value, double low, double high) {
  return std::max(low, std::min(high, value));
}

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
      {"--..--", ','}, {"..--..", '?'}, {"-..-.", '/'}, {".-.-.", '+'},
      {"-....-", '-'}, {".--.-.", '@'}, {"---...", ':'}, {"-.-.-.", ';'},
      {".-..-.", '"'}, {".----.", '\''}, {"..--.-", '_'}, {"-...-", '='}
  };
  return table;
}

const std::unordered_set<std::string>& morsePrefixes() {
  static const std::unordered_set<std::string> prefixes = []() {
    std::unordered_set<std::string> values;
    values.insert("");
    for (const auto& entry : morseTable()) {
      for (std::size_t length = 1U; length <= entry.first.size(); ++length)
        values.insert(entry.first.substr(0U, length));
    }
    return values;
  }();
  return prefixes;
}

// Student-t predictive kernel without the degrees-of-freedom-only constant.
// This preserves posterior ordering and avoids lgamma in the live path.
double studentTLogKernel(double delta, double variance, double dof) {
  const double safeVariance = std::max(1.0e-6, variance);
  const double safeDof = std::max(2.1, dof);
  const double normalized = (delta * delta) / (safeDof * safeVariance);
  return -0.5 * std::log(safeVariance) -
         0.5 * (safeDof + 1.0) * std::log1p(normalized);
}

} // namespace

void CwMorseBeamDecoder::DurationPosterior::initialize(
    double centreMs, double predictiveSigma, double priorStrength) {
  meanLog = std::log(std::max(1.0, centreMs));
  kappa = clampd(priorStrength, 0.25, 20.0);
  alpha = 3.0;
  const double sigma = clampd(predictiveSigma, 0.08, 1.10);
  beta = sigma * sigma * alpha * kappa / (kappa + 1.0);
}

double CwMorseBeamDecoder::DurationPosterior::logPredictive(
    double durationMs, double extraSigma) const {
  const double x = std::log(std::max(1.0, durationMs));
  const double dof = 2.0 * std::max(1.05, alpha);
  const double baseVariance = beta * (kappa + 1.0) /
      (std::max(1.0e-6, alpha) * std::max(1.0e-6, kappa));
  const double variance = baseVariance + extraSigma * extraSigma;
  return studentTLogKernel(x - meanLog, variance, dof);
}

void CwMorseBeamDecoder::DurationPosterior::update(
    double durationMs, double weight) {
  const double w = clampd(weight, 0.02, 1.0);
  const double x = std::log(std::max(1.0, durationMs));
  const double oldKappa = kappa;
  const double nextKappa = oldKappa + w;
  const double delta = x - meanLog;
  meanLog += (w / nextKappa) * delta;
  beta += 0.5 * w * oldKappa / nextKappa * delta * delta;
  kappa = nextKappa;
  alpha += 0.5 * w;
}

double CwMorseBeamDecoder::DurationPosterior::centreMs() const {
  return std::exp(meanLog);
}

CwMorseBeamDecoder::CwMorseBeamDecoder(CwMorseBeamConfig config)
    : m_config(config) {
  sanitize();
  reset();
}

void CwMorseBeamDecoder::setConfig(const CwMorseBeamConfig& config) {
  m_config = config;
  sanitize();
  rebuild(m_lastTiming);
}

const CwMorseBeamConfig& CwMorseBeamDecoder::config() const {
  return m_config;
}

void CwMorseBeamDecoder::sanitize() {
  m_config.beamWidth = std::max<std::size_t>(4U,
      std::min<std::size_t>(64U, m_config.beamWidth));
  m_config.maxPatternLength = std::max<std::size_t>(4U,
      std::min<std::size_t>(8U, m_config.maxPatternLength));
  m_config.maxReplayEvents = std::max<std::size_t>(24U,
      std::min<std::size_t>(256U, m_config.maxReplayEvents));
  m_config.credibleMass = clampd(m_config.credibleMass, 0.80, 0.9999);
  m_config.decisivePosterior = clampd(m_config.decisivePosterior,
                                      m_config.credibleMass, 0.99999);
  m_config.minimumCommitConfidence = clampd(
      m_config.minimumCommitConfidence, 0.30, 0.95);

  m_config.dotPrior = clampd(m_config.dotPrior, 0.05, 0.95);
  m_config.dashPrior = clampd(m_config.dashPrior, 0.05, 0.95);
  const double markSum = m_config.dotPrior + m_config.dashPrior;
  m_config.dotPrior /= markSum;
  m_config.dashPrior /= markSum;

  m_config.elementSpacePrior = clampd(m_config.elementSpacePrior, 0.02, 0.96);
  m_config.characterSpacePrior = clampd(m_config.characterSpacePrior, 0.02, 0.96);
  m_config.wordSpacePrior = clampd(m_config.wordSpacePrior, 0.005, 0.60);
  const double spaceSum = m_config.elementSpacePrior +
      m_config.characterSpacePrior + m_config.wordSpacePrior;
  m_config.elementSpacePrior /= spaceSum;
  m_config.characterSpacePrior /= spaceSum;
  m_config.wordSpacePrior /= spaceSum;
}

void CwMorseBeamDecoder::reset() {
  m_events.clear();
  m_beam.clear();
  m_confidence = 0.0;
  m_bestPosterior = 0.0;
  m_posteriorOddsDb = 0.0;
  m_haveContinuityTiming = false;
  m_continuityPrior = 0.0;
}

void CwMorseBeamDecoder::beginEpoch(
    const CwMorseTimingSnapshot& continuityTiming,
    double continuityPrior) {
  m_events.clear();
  m_beam.clear();
  m_confidence = 0.0;
  m_bestPosterior = 0.0;
  m_posteriorOddsDb = 0.0;
  m_continuityTiming = continuityTiming;
  m_continuityPrior = clampd(continuityPrior, 0.05, 0.45);
  m_haveContinuityTiming = continuityTiming.ditMs > 5.0 &&
      continuityTiming.dahMs > 1.4 * continuityTiming.ditMs;
}

void CwMorseBeamDecoder::clearContinuityAlternative() {
  m_haveContinuityTiming = false;
  m_continuityPrior = 0.0;
}

bool CwMorseBeamDecoder::isMorsePrefix(const std::string& pattern) {
  return morsePrefixes().find(pattern) != morsePrefixes().end();
}

char CwMorseBeamDecoder::decodePattern(const std::string& pattern) {
  const auto found = morseTable().find(pattern);
  return found == morseTable().end() ? '?' : found->second;
}

std::string CwMorseBeamDecoder::keyFor(const Hypothesis& hypothesis) {
  std::string key;
  key.reserve(hypothesis.tokens.size() * 12U + hypothesis.pattern.size() + 1U);
  for (const Token& token : hypothesis.tokens) {
    key.push_back(token.value);
    key.push_back('{');
    key += token.pattern;
    key.push_back('@');
    key += std::to_string(token.endEvent);
    key.push_back('}');
  }
  key.push_back('|');
  key += hypothesis.pattern;
  key.push_back('#');
  key += std::to_string(hypothesis.temporalModel);
  return key;
}

CwMorseBeamDecoder::QualityProbabilities
CwMorseBeamDecoder::qualityProbabilities(
    const CwMorseObservationQuality& quality) {
  QualityProbabilities result;
  const double snr = clampd((quality.snrDb + 3.0) / 22.0, 0.0, 1.0);
  const double centeredFallback = quality.carrierCentered ? 0.92 : 0.18;
  result.centered = quality.centeredProbability >= 0.0
      ? clampd(quality.centeredProbability, 0.001, 0.999)
      : centeredFallback;
  result.reliability = clampd(
      0.36 * quality.confidence + 0.27 * quality.coherence +
      0.17 * snr + 0.20 * result.centered,
      0.03, 0.995);
  result.state = quality.stateProbability >= 0.0
      ? clampd(quality.stateProbability, 0.001, 0.999)
      : clampd(0.50 + 0.48 * result.reliability, 0.51, 0.98);
  result.qsb = quality.qsbProbability >= 0.0
      ? clampd(quality.qsbProbability, 0.0, 0.999)
      : clampd((1.0 - result.state) * quality.coherence * 0.55, 0.0, 0.55);
  result.noise = quality.noiseProbability >= 0.0
      ? clampd(quality.noiseProbability, 0.0, 0.999)
      : clampd(1.0 - 0.70 * result.reliability - 0.18 * result.centered,
               0.01, 0.90);
  return result;
}

double CwMorseBeamDecoder::logProbability(double probability) {
  return std::log(clampd(probability, 1.0e-9, 1.0));
}

double CwMorseBeamDecoder::logAddExp(double left, double right) {
  if (left < right) std::swap(left, right);
  if (right <= kLogFloor) return left;
  const double delta = right - left;
  if (delta < -18.0) return left;
  return left + std::log1p(std::exp(delta));
}

double CwMorseBeamDecoder::logMixture(
    double firstLogLikelihood, double firstWeight,
    double secondLogLikelihood, double secondWeight) {
  return logAddExp(logProbability(firstWeight) + firstLogLikelihood,
                   logProbability(secondWeight) + secondLogLikelihood);
}

double CwMorseBeamDecoder::broadOutlierLogLikelihood(
    double durationMs, double referenceMs) {
  const double ratio = std::log(std::max(1.0, durationMs) /
                                std::max(1.0, referenceMs));
  return -3.25 - 0.18 * std::abs(ratio);
}

double CwMorseBeamDecoder::softMinimumUnitsLogPrior(
    double units, double minimumUnits, double softness) {
  const double x = (units - minimumUnits) / std::max(0.05, softness);
  if (x >= 0.0) return -std::log1p(std::exp(-std::min(40.0, x)));
  return x - std::log1p(std::exp(std::max(-40.0, x)));
}

bool CwMorseBeamDecoder::appendCharacter(
    Hypothesis& hypothesis, std::size_t endEvent) {
  if (hypothesis.pattern.empty()) return false;
  const char decoded = decodePattern(hypothesis.pattern);
  if (decoded == '?') return false;
  hypothesis.tokens.push_back(Token{decoded, hypothesis.pattern, endEvent});
  hypothesis.pattern.clear();
  return true;
}

void CwMorseBeamDecoder::appendWordBoundary(
    Hypothesis& hypothesis, std::size_t endEvent) {
  if (!hypothesis.tokens.empty() && hypothesis.tokens.back().value != ' ')
    hypothesis.tokens.push_back(Token{' ', {}, endEvent});
}

CwMorseBeamDecoder::Hypothesis CwMorseBeamDecoder::initialHypothesis(
    const CwMorseTimingSnapshot& timing, int temporalModel) const {
  Hypothesis hypothesis;
  const double trust = clampd(timing.timingConfidence, 0.0, 1.0);
  const double markSigma = 0.18 + 0.42 * (1.0 - trust);
  const double gapSigma = 0.24 + 0.48 * (1.0 - trust);
  const double priorStrength = 0.55 + 5.0 * trust;
  hypothesis.dit.initialize(timing.ditMs, markSigma, priorStrength);
  hypothesis.dah.initialize(timing.dahMs, markSigma + 0.04, priorStrength);
  hypothesis.elementSpace.initialize(timing.elementSpaceMs, gapSigma,
                                     0.75 * priorStrength);
  hypothesis.characterSpace.initialize(timing.characterSpaceMs,
                                       gapSigma + 0.10,
                                       0.65 * priorStrength);
  hypothesis.wordSpace.initialize(timing.wordSpaceMs, gapSigma + 0.18,
                                  0.55 * priorStrength);
  hypothesis.temporalModel = temporalModel;
  return hypothesis;
}

void CwMorseBeamDecoder::prune(std::vector<Hypothesis> next) {
  if (next.empty()) next.push_back(initialHypothesis(m_lastTiming));

  std::unordered_map<std::string, std::size_t> positions;
  std::vector<Hypothesis> unique;
  unique.reserve(next.size());
  for (Hypothesis& hypothesis : next) {
    const std::string key = keyFor(hypothesis);
    const auto found = positions.find(key);
    if (found == positions.end()) {
      positions.emplace(key, unique.size());
      unique.push_back(std::move(hypothesis));
    } else if (hypothesis.logPosterior >
               unique[found->second].logPosterior) {
      unique[found->second] = std::move(hypothesis);
    }
  }

  const auto better = [](const Hypothesis& left, const Hypothesis& right) {
    if (left.logPosterior != right.logPosterior)
      return left.logPosterior > right.logPosterior;
    if (left.tokens.size() != right.tokens.size())
      return left.tokens.size() > right.tokens.size();
    return left.pattern.size() > right.pattern.size();
  };

  if (unique.size() > m_config.beamWidth) {
    std::nth_element(unique.begin(),
                     unique.begin() + static_cast<std::ptrdiff_t>(m_config.beamWidth),
                     unique.end(), better);
    unique.resize(m_config.beamWidth);
  }
  std::sort(unique.begin(), unique.end(), better);

  const double best = unique.front().logPosterior;
  double posteriorSum = 0.0;
  for (Hypothesis& hypothesis : unique) {
    hypothesis.logPosterior -= best;
    posteriorSum += std::exp(std::max(kLogFloor, hypothesis.logPosterior));
  }
  m_beam = std::move(unique);

  m_bestPosterior = posteriorSum > 0.0 ? 1.0 / posteriorSum : 0.0;
  const double secondDelta = m_beam.size() > 1U
      ? -m_beam[1U].logPosterior : 20.0;
  m_posteriorOddsDb = 10.0 * secondDelta / std::log(10.0);
  const double evidenceQuality = m_beam.front().observations > 0
      ? m_beam.front().evidence /
            static_cast<double>(m_beam.front().observations)
      : 0.0;
  m_confidence = clampd(m_bestPosterior *
                            (0.34 + 0.66 * evidenceQuality),
                        0.0, 1.0);
}

void CwMorseBeamDecoder::rebuild(const CwMorseTimingSnapshot& timing) {
  m_lastTiming = timing;
  m_beam.clear();
  Hypothesis primary = initialHypothesis(timing, 0);
  if (m_haveContinuityTiming) {
    primary.logPosterior = logProbability(1.0 - m_continuityPrior);
    Hypothesis continuity = initialHypothesis(m_continuityTiming, 1);
    continuity.logPosterior = logProbability(m_continuityPrior);
    m_beam.push_back(std::move(primary));
    m_beam.push_back(std::move(continuity));
  } else {
    m_beam.push_back(std::move(primary));
  }

  const double timingTrust = clampd(timing.timingConfidence, 0.0, 1.0);
  for (std::size_t eventIndex = 0U; eventIndex < m_events.size(); ++eventIndex) {
    const Event& event = m_events[eventIndex];
    const QualityProbabilities probability = qualityProbabilities(event.quality);
    const double validProbability = clampd(
        probability.state * (0.58 + 0.42 * probability.reliability) *
        (1.0 - 0.40 * probability.noise),
        0.03, 0.997);
    const double outlierProbability = 1.0 - validProbability;
    const double extraSigma = 0.05 + 0.24 * (1.0 - probability.reliability) +
        0.16 * probability.qsb + 0.12 * (1.0 - probability.centered) +
        0.12 * (1.0 - timingTrust);

    std::vector<Hypothesis> next;
    if (event.mark) {
      next.reserve(m_beam.size() * 3U);
      for (const Hypothesis& base : m_beam) {
        for (const char symbol : {'.', '-'}) {
          Hypothesis candidate = base;
          candidate.pattern.push_back(symbol);
          if (candidate.pattern.size() > m_config.maxPatternLength ||
              !isMorsePrefix(candidate.pattern)) {
            continue;
          }

          DurationPosterior& posterior = symbol == '.'
              ? candidate.dit : candidate.dah;
          const double timingLogLikelihood = posterior.logPredictive(
              event.durationMs, extraSigma);
          const double outlierLogLikelihood = broadOutlierLogLikelihood(
              event.durationMs, posterior.centreMs());
          candidate.logPosterior += logMixture(
              timingLogLikelihood, validProbability,
              outlierLogLikelihood, outlierProbability);
          candidate.logPosterior += logProbability(
              symbol == '.' ? m_config.dotPrior : m_config.dashPrior);
          posterior.update(event.durationMs,
                           probability.reliability * validProbability);
          candidate.evidence += probability.reliability * validProbability;
          ++candidate.observations;
          next.push_back(std::move(candidate));
        }

        const double skipProbability = clampd(
            probability.noise + 0.55 * probability.qsb *
                (1.0 - probability.state),
            0.0, 0.92);
        if (skipProbability > 0.06) {
          Hypothesis skipped = base;
          skipped.logPosterior += logProbability(skipProbability) - 0.55;
          skipped.evidence += 0.20 * probability.reliability;
          ++skipped.observations;
          next.push_back(std::move(skipped));
        }
      }
    } else {
      next.reserve(m_beam.size() * 3U);
      for (const Hypothesis& base : m_beam) {
        const double dit = std::max(1.0, base.dit.centreMs());
        const double units = event.durationMs / dit;
        const auto familyLikelihood = [&](const DurationPosterior& family,
                                          double canonicalUnits,
                                          double familyWeight) {
          const double learned = family.logPredictive(event.durationMs,
                                                      extraSigma + 0.05);
          const double canonicalDelta = std::log(
              std::max(1.0, event.durationMs) /
              std::max(1.0, canonicalUnits * dit));
          const double canonical = studentTLogKernel(
              canonicalDelta,
              std::pow(0.34 + 0.26 * (1.0 - timingTrust) + extraSigma, 2.0),
              5.0);
          return logMixture(learned, familyWeight,
                            canonical, 1.0 - familyWeight);
        };

        const double learnedWeight = 0.58 + 0.34 * timingTrust;
        const double elementLogLikelihood = familyLikelihood(
            base.elementSpace, 1.0, learnedWeight);
        const double characterLogLikelihood = familyLikelihood(
            base.characterSpace, 3.0, learnedWeight) +
            softMinimumUnitsLogPrior(units, 1.55, 0.24);
        const double wordLogLikelihood = familyLikelihood(
            base.wordSpace, 7.0, learnedWeight) +
            softMinimumUnitsLogPrior(units, 3.75, 0.38);

        if (!event.forceWordBoundary && !base.pattern.empty()) {
          Hypothesis element = base;
          const double outlier = broadOutlierLogLikelihood(
              event.durationMs, element.elementSpace.centreMs());
          element.logPosterior += logMixture(
              elementLogLikelihood, validProbability,
              outlier, outlierProbability);
          element.logPosterior += logProbability(m_config.elementSpacePrior);
          element.elementSpace.update(
              event.durationMs, probability.reliability * validProbability);
          element.evidence += probability.reliability * validProbability;
          ++element.observations;
          next.push_back(std::move(element));
        }

        if (!base.pattern.empty()) {
          Hypothesis character = base;
          if (appendCharacter(character, eventIndex + 1U)) {
            const double outlier = broadOutlierLogLikelihood(
                event.durationMs, character.characterSpace.centreMs());
            character.logPosterior += logMixture(
                characterLogLikelihood, validProbability,
                outlier, outlierProbability);
            character.logPosterior += logProbability(
                m_config.characterSpacePrior);
            character.characterSpace.update(
                event.durationMs, probability.reliability * validProbability);
            character.evidence += probability.reliability * validProbability;
            ++character.observations;
            if (!event.forceWordBoundary) next.push_back(std::move(character));

            Hypothesis word = base;
            if (appendCharacter(word, eventIndex + 1U)) {
              appendWordBoundary(word, eventIndex + 1U);
              const double wordOutlier = broadOutlierLogLikelihood(
                  event.durationMs, word.wordSpace.centreMs());
              word.logPosterior += logMixture(
                  wordLogLikelihood, validProbability,
                  wordOutlier, outlierProbability);
              word.logPosterior += logProbability(m_config.wordSpacePrior);
              word.wordSpace.update(
                  event.durationMs,
                  probability.reliability * validProbability);
              word.evidence += probability.reliability * validProbability;
              ++word.observations;
              next.push_back(std::move(word));
            }
          }
        } else if (!base.tokens.empty()) {
          Hypothesis word = base;
          appendWordBoundary(word, eventIndex + 1U);
          const double outlier = broadOutlierLogLikelihood(
              event.durationMs, word.wordSpace.centreMs());
          word.logPosterior += logMixture(
              wordLogLikelihood, validProbability,
              outlier, outlierProbability);
          word.logPosterior += logProbability(m_config.wordSpacePrior);
          word.wordSpace.update(
              event.durationMs, probability.reliability * validProbability);
          word.evidence += probability.reliability * validProbability;
          ++word.observations;
          next.push_back(std::move(word));
        }
      }
    }

    prune(std::move(next));
  }
}

CwMorseBeamResult CwMorseBeamDecoder::collectStablePrefix(bool forceBest) {
  CwMorseBeamResult result;
  if (m_beam.empty()) return result;

  std::vector<double> posterior(m_beam.size(), 0.0);
  double total = 0.0;
  for (std::size_t index = 0U; index < m_beam.size(); ++index) {
    posterior[index] = std::exp(std::max(kLogFloor,
                                         m_beam[index].logPosterior));
    total += posterior[index];
  }
  if (total <= 0.0) total = 1.0;
  for (double& value : posterior) value /= total;

  std::size_t active = 0U;
  double cumulative = 0.0;
  do {
    cumulative += posterior[active];
    ++active;
  } while (active < posterior.size() && cumulative < m_config.credibleMass);

  std::size_t common = m_beam.front().tokens.size();
  if (!forceBest) {
    const bool decisive = m_bestPosterior >= m_config.decisivePosterior &&
                          m_confidence >= m_config.minimumCommitConfidence;
    if (!decisive) {
      active = std::min(m_beam.size(), std::max<std::size_t>(2U, active));
      for (std::size_t index = 1U; index < active; ++index) {
        common = std::min(common, m_beam[index].tokens.size());
        std::size_t matched = 0U;
        while (matched < common &&
               m_beam.front().tokens[matched] ==
                   m_beam[index].tokens[matched]) {
          ++matched;
        }
        common = matched;
      }
      if (m_confidence < 0.46) common = 0U;
    }
  }

  std::size_t consumedEvents = 0U;
  for (std::size_t index = 0U; index < common; ++index) {
    const Token& token = m_beam.front().tokens[index];
    result.committedText.push_back(token.value);
    result.committedPatterns.push_back(token.pattern);
    consumedEvents = std::max(consumedEvents, token.endEvent);
  }

  if (consumedEvents > 0U && consumedEvents <= m_events.size()) {
    m_events.erase(m_events.begin(),
                   m_events.begin() +
                       static_cast<std::ptrdiff_t>(consumedEvents));
    rebuild(m_lastTiming);
  }

  result.currentPattern = m_beam.empty() ? std::string{}
                                         : m_beam.front().pattern;
  result.confidence = m_confidence;
  result.bestPosterior = m_bestPosterior;
  result.posteriorOddsDb = m_posteriorOddsDb;
  result.hypothesisCount = m_beam.size();
  if (!m_beam.empty()) {
    result.temporalModel = m_beam.front().temporalModel;
    result.inferredDitMs = m_beam.front().dit.centreMs();
  }
  return result;
}

CwMorseBeamResult CwMorseBeamDecoder::observeMark(
    double durationMs, const CwMorseTimingSnapshot& timing,
    const CwMorseObservationQuality& quality) {
  m_events.push_back(Event{true, false, durationMs, quality});
  if (m_events.size() > m_config.maxReplayEvents) {
    reset();
    m_events.push_back(Event{true, false, durationMs, quality});
  }
  rebuild(timing);
  return collectStablePrefix(false);
}

CwMorseBeamResult CwMorseBeamDecoder::observeSpace(
    double durationMs, const CwMorseTimingSnapshot& timing,
    const CwMorseObservationQuality& quality, bool forceWordBoundary) {
  m_events.push_back(Event{false, forceWordBoundary, durationMs, quality});
  if (m_events.size() > m_config.maxReplayEvents) {
    reset();
    return snapshot();
  }
  rebuild(timing);
  return collectStablePrefix(forceWordBoundary);
}

CwMorseBeamResult CwMorseBeamDecoder::flush(
    const CwMorseTimingSnapshot& timing) {
  if (m_events.empty()) return snapshot();
  CwMorseObservationQuality quality;
  quality.confidence = 1.0;
  quality.coherence = 1.0;
  quality.snrDb = 30.0;
  quality.carrierCentered = true;
  quality.stateProbability = 0.999;
  quality.qsbProbability = 0.0;
  quality.noiseProbability = 0.001;
  quality.centeredProbability = 0.999;
  CwMorseBeamResult result = observeSpace(
      std::max(timing.wordSpaceMs, 7.0 * timing.ditMs),
      timing, quality, true);

  CwMorseBeamResult tail = collectStablePrefix(true);
  result.committedText += tail.committedText;
  result.committedPatterns.insert(result.committedPatterns.end(),
                                  tail.committedPatterns.begin(),
                                  tail.committedPatterns.end());
  result.currentPattern = tail.currentPattern;
  result.confidence = tail.confidence;
  result.bestPosterior = tail.bestPosterior;
  result.posteriorOddsDb = tail.posteriorOddsDb;
  result.hypothesisCount = tail.hypothesisCount;
  result.temporalModel = tail.temporalModel;
  result.inferredDitMs = tail.inferredDitMs;
  // The synthetic force-word observation exists only to close the final Morse
  // character. A real inter-word SPACE is committed by observeSpace/advance;
  // do not expose an artificial trailing blank during receiver flush.
  if (!result.committedText.empty() && result.committedText.back() == ' ') {
    result.committedText.pop_back();
    if (!result.committedPatterns.empty()) result.committedPatterns.pop_back();
  }
  return result;
}

CwMorseBeamResult CwMorseBeamDecoder::snapshot() const {
  CwMorseBeamResult result;
  if (!m_beam.empty()) result.currentPattern = m_beam.front().pattern;
  result.confidence = m_confidence;
  result.bestPosterior = m_bestPosterior;
  result.posteriorOddsDb = m_posteriorOddsDb;
  result.hypothesisCount = m_beam.size();
  if (!m_beam.empty()) {
    result.temporalModel = m_beam.front().temporalModel;
    result.inferredDitMs = m_beam.front().dit.centreMs();
  }
  return result;
}

} // namespace madmodem::cwskimmer
