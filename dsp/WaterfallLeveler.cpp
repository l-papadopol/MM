#include "WaterfallLeveler.h"

#include <algorithm>
#include <cmath>

namespace {

double clampValue(double value, double low, double high)
{
    return std::max(low, std::min(high, value));
}

double quantile(std::vector<double> values, double q)
{
    if (values.empty()) return -130.0;
    q = clampValue(q, 0.0, 1.0);
    const std::size_t index = static_cast<std::size_t>(
        std::llround(q * static_cast<double>(values.size() - 1U)));
    std::nth_element(values.begin(),
                     values.begin() + static_cast<std::ptrdiff_t>(index),
                     values.end());
    return values[index];
}

std::vector<double> smoothProfile(const std::vector<double> &input)
{
    if (input.empty()) return {};
    const std::size_t radius = std::max<std::size_t>(3U, input.size() / 128U);
    std::vector<double> prefix(input.size() + 1U, 0.0);
    for (std::size_t i = 0; i < input.size(); ++i)
        prefix[i + 1U] = prefix[i] + input[i];

    std::vector<double> output(input.size(), 0.0);
    for (std::size_t i = 0; i < input.size(); ++i) {
        const std::size_t begin = i > radius ? i - radius : 0U;
        const std::size_t end = std::min(input.size(), i + radius + 1U);
        output[i] = (prefix[end] - prefix[begin]) /
                    static_cast<double>(end - begin);
    }
    return output;
}

bool similarBand(std::size_t a0, std::size_t a1,
                 std::size_t b0, std::size_t b1,
                 std::size_t count)
{
    if (count == 0U) return false;
    const std::size_t tolerance = std::max<std::size_t>(6U, count / 40U);
    return (a0 > b0 ? a0 - b0 : b0 - a0) <= tolerance &&
           (a1 > b1 ? a1 - b1 : b1 - a1) <= tolerance;
}

} // namespace

void WaterfallLeveler::reset()
{
    m_floorDb = -110.0;
    m_initialized = false;
    m_floorCandidates.clear();
    m_validBegin = m_validEnd = 0U;
    m_candidateBegin = m_candidateEnd = 0U;
    m_lastBinCount = 0U;
    m_partialConfirm = 0;
    m_fullConfirm = 0;
    m_haveValidBand = false;
}

WaterfallLevelResult WaterfallLeveler::update(
    const std::vector<double> &dbLine, double elapsedSeconds)
{
    WaterfallLevelResult result;
    if (dbLine.empty()) {
        result.floorDb = m_floorDb;
        result.ceilingDb = m_floorDb + 48.0;
        return result;
    }

    if (m_lastBinCount != dbLine.size()) {
        reset();
        m_lastBinCount = dbLine.size();
    }

    const std::vector<double> profile = smoothProfile(dbLine);
    const double highReference = quantile(profile, 0.90);
    const double threshold = highReference - 24.0;
    const std::size_t bridgeLimit = std::max<std::size_t>(3U, dbLine.size() / 96U);

    std::size_t bestBegin = 0U;
    std::size_t bestEnd = dbLine.size() - 1U;
    std::size_t bestWidth = 0U;
    std::size_t runBegin = 0U;
    std::size_t lastAbove = 0U;
    bool inRun = false;

    for (std::size_t i = 0; i < profile.size(); ++i) {
        if (profile[i] >= threshold) {
            if (!inRun) {
                inRun = true;
                runBegin = i;
            }
            lastAbove = i;
        } else if (inRun && i - lastAbove > bridgeLimit) {
            const std::size_t width = lastAbove - runBegin + 1U;
            if (width > bestWidth) {
                bestWidth = width;
                bestBegin = runBegin;
                bestEnd = lastAbove;
            }
            inRun = false;
        }
    }
    if (inRun) {
        const std::size_t width = lastAbove - runBegin + 1U;
        if (width > bestWidth) {
            bestWidth = width;
            bestBegin = runBegin;
            bestEnd = lastAbove;
        }
    }

    const std::size_t minimumUsefulWidth = std::max<std::size_t>(24U,
        dbLine.size() / 10U);
    bool candidatePartial = bestWidth >= minimumUsefulWidth &&
                            bestWidth < (9U * dbLine.size()) / 10U;
    double candidateContrastDb = 0.0;

    if (candidatePartial) {
        const std::size_t padding = std::max<std::size_t>(4U, dbLine.size() / 128U);
        bestBegin = bestBegin > padding ? bestBegin - padding : 0U;
        bestEnd = std::min(dbLine.size() - 1U, bestEnd + padding);

        std::vector<double> inside(dbLine.begin() + static_cast<std::ptrdiff_t>(bestBegin),
                                   dbLine.begin() + static_cast<std::ptrdiff_t>(bestEnd + 1U));
        std::vector<double> outside;
        outside.reserve(dbLine.size() - inside.size());
        outside.insert(outside.end(), dbLine.begin(),
                       dbLine.begin() + static_cast<std::ptrdiff_t>(bestBegin));
        outside.insert(outside.end(),
                       dbLine.begin() + static_cast<std::ptrdiff_t>(bestEnd + 1U),
                       dbLine.end());
        const double insideFloor = quantile(inside, 0.20);
        const double outsideMedian = outside.empty() ? insideFloor : quantile(outside, 0.50);
        candidateContrastDb = insideFloor - outsideMedian;
        // Require a real sound-card/passband edge, not a broad signal or a
        // sloping receiver response.
        if (candidateContrastDb < 9.0)
            candidatePartial = false;
    }

    if (candidatePartial) {
        if (m_partialConfirm == 0 ||
            !similarBand(bestBegin, bestEnd, m_candidateBegin, m_candidateEnd,
                         dbLine.size())) {
            m_candidateBegin = bestBegin;
            m_candidateEnd = bestEnd;
            m_partialConfirm = candidateContrastDb >= 15.0 ? 4 : 1;
        } else {
            ++m_partialConfirm;
        }
        m_fullConfirm = 0;

        if (m_partialConfirm >= 4) {
            if (!m_haveValidBand) {
                m_validBegin = m_candidateBegin;
                m_validEnd = m_candidateEnd;
                m_haveValidBand = true;
            } else if (similarBand(m_candidateBegin, m_candidateEnd,
                                   m_validBegin, m_validEnd, dbLine.size())) {
                // Only small, persistent edge movement is accepted.  A broad
                // keyed signal inside the passband must never shrink the mask
                // around itself and drag the colour floor with it.
                m_validBegin = (7U * m_validBegin + m_candidateBegin) / 8U;
                m_validEnd = (7U * m_validEnd + m_candidateEnd) / 8U;
            }
        }
    } else {
        m_partialConfirm = 0;
        if (m_haveValidBand) {
            const std::size_t begin = std::min(m_validBegin, dbLine.size() - 1U);
            const std::size_t end = std::max(begin,
                std::min(m_validEnd, dbLine.size() - 1U));
            std::vector<double> inside(dbLine.begin() + static_cast<std::ptrdiff_t>(begin),
                                       dbLine.begin() + static_cast<std::ptrdiff_t>(end + 1U));
            std::vector<double> outside;
            outside.insert(outside.end(), dbLine.begin(),
                           dbLine.begin() + static_cast<std::ptrdiff_t>(begin));
            outside.insert(outside.end(),
                           dbLine.begin() + static_cast<std::ptrdiff_t>(end + 1U),
                           dbLine.end());
            const double insideFloor = quantile(inside, 0.20);
            const double outsideMedian = outside.empty() ? insideFloor : quantile(outside, 0.50);
            if (insideFloor - outsideMedian < 5.0)
                ++m_fullConfirm;
            else
                m_fullConfirm = 0;
            if (m_fullConfirm >= 200) {
                m_haveValidBand = false;
                m_fullConfirm = 0;
            }
        }
    }

    std::size_t validBegin = 0U;
    std::size_t validEnd = dbLine.size() - 1U;
    if (m_haveValidBand) {
        validBegin = std::min(m_validBegin, dbLine.size() - 1U);
        validEnd = std::max(validBegin,
            std::min(m_validEnd, dbLine.size() - 1U));
    }

    std::vector<double> valid(dbLine.begin() + static_cast<std::ptrdiff_t>(validBegin),
                              dbLine.begin() + static_cast<std::ptrdiff_t>(validEnd + 1U));
    // Use a low quantile and a temporal history. Broad keyed energy and strong
    // carriers therefore cannot be mistaken for a change in receiver noise.
    const double candidateFloor = clampValue(quantile(valid, 0.05) + 2.0,
                                              -130.0, -25.0);
    m_floorCandidates.push_back(candidateFloor);
    while (m_floorCandidates.size() > 300U) m_floorCandidates.pop_front();
    const std::vector<double> history(m_floorCandidates.begin(),
                                      m_floorCandidates.end());
    const double targetFloor = quantile(history, 0.25);

    if (!m_initialized) {
        m_floorDb = targetFloor;
        m_initialized = true;
    } else {
        elapsedSeconds = clampValue(elapsedSeconds, 0.001, 0.25);
        // Falling the floor makes the whole display hotter, so it is deliberately
        // slower than the already conservative upward movement.
        const double rate = targetFloor < m_floorDb ? 0.15 : 0.35;
        const double maximumStep = rate * elapsedSeconds;
        m_floorDb += clampValue(targetFloor - m_floorDb,
                                -maximumStep, maximumStep);
    }

    result.floorDb = m_floorDb;
    result.ceilingDb = m_floorDb + 48.0;
    result.validBegin = validBegin;
    result.validEnd = validEnd;
    result.partialBand = m_haveValidBand;
    return result;
}
