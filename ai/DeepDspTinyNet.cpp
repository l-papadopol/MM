#include "DeepDspTinyNet.h"

#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>

namespace {
constexpr quint32 kMagic = 0x4D4E524Bu; // MNRK, MIND ranker checkpoint
constexpr quint32 kVersion = 1u;

float finiteOrZero(float v)
{
    return std::isfinite(v) ? v : 0.0f;
}
} // namespace

DeepDspTinyNet::DeepDspTinyNet()
{
    reset();
}

float DeepDspTinyNet::sigmoid(float z)
{
    if (z >= 40.0f) return 1.0f;
    if (z <= -40.0f) return 0.0f;
    return 1.0f / (1.0f + std::exp(-z));
}

float DeepDspTinyNet::clamp01(float x)
{
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

float DeepDspTinyNet::clampGrad(float x)
{
    if (x > 4.0f) return 4.0f;
    if (x < -4.0f) return -4.0f;
    return x;
}

float DeepDspTinyNet::safeInputAt(const QVector<float> &input, int r, int c)
{
    if (r < 0 || r >= kRows || c < 0 || c >= kCols) return 0.0f;
    const int idx = r * kCols + c;
    if (idx < 0 || idx >= input.size()) return 0.0f;
    return finiteOrZero(input[idx]);
}

QVector<float> DeepDspTinyNet::outputVector(float probability)
{
    QVector<float> out;
    out.reserve(kOutput);
    out.append(clamp01(probability));
    return out;
}

void DeepDspTinyNet::reset(unsigned seed)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> kdist(-0.08f, 0.08f);
    std::uniform_real_distribution<float> wdist(-0.05f, 0.05f);

    for (int f = 0; f < kConvFilters; ++f) {
        for (float &v : m_kernel[f]) {
            v = kdist(rng);
        }
        m_convBias[f] = 0.0f;
    }
    for (float &w : m_dense) {
        w = wdist(rng);
    }
    m_denseBias = -0.25f; // conservative initial probability; avoid over-trusting untrained MIND
    m_lastActivity.clear();
    m_lastBatchSize = 0;
}

DeepDspTinyNet::ForwardCache DeepDspTinyNet::forwardInternal(const QVector<float> &input, bool keepActivity) const
{
    ForwardCache cache;
    cache.x = input;
    if (cache.x.size() != kInput) {
        cache.x = QVector<float>(kInput, 0.0f);
    }
    for (float &v : cache.x) {
        v = clamp01(finiteOrZero(v));
    }

    // 2D convolution over the FT8 time-frequency topology: 58 data symbols x 8 tones.
    for (int f = 0; f < kConvFilters; ++f) {
        float maxVal = -1.0e30f;
        int maxIdx = 0;
        float sumVal = 0.0f;
        for (int r = 0; r < kRows; ++r) {
            for (int c = 0; c < kCols; ++c) {
                float z = m_convBias[f];
                int k = 0;
                for (int kr = -1; kr <= 1; ++kr) {
                    for (int kc = -1; kc <= 1; ++kc) {
                        z += m_kernel[f][k++] * safeInputAt(cache.x, r + kr, c + kc);
                    }
                }
                const float a = std::tanh(z);
                const int idx = r * kCols + c;
                cache.conv[f][idx] = a;
                sumVal += a;
                if (a > maxVal) {
                    maxVal = a;
                    maxIdx = idx;
                }
            }
        }
        cache.features[f] = sumVal / static_cast<float>(kConvSize);
        cache.features[kConvFilters + f] = maxVal;
        cache.maxIndex[f] = maxIdx;
    }

    // Handcrafted low-cost topology metrics. These are not targets; they give the
    // classifier robust context for pruning without forcing it to learn trivial
    // energy/gap statistics from scratch.
    float meanBest = 0.0f;
    float meanGap = 0.0f;
    float meanEntropy = 0.0f;
    float meanPower = 0.0f;
    float minGap = 1.0f;
    float maxBest = 0.0f;
    float timeVariance = 0.0f;
    float toneConcentration = 0.0f;
    std::array<float, kRows> rowBest{};
    std::array<float, kCols> toneSum{};
    for (int r = 0; r < kRows; ++r) {
        float best = -1.0f;
        float second = -1.0f;
        float sum = 0.0f;
        for (int c = 0; c < kCols; ++c) {
            const float x = cache.x[r * kCols + c];
            sum += x;
            toneSum[c] += x;
            if (x > best) {
                second = best;
                best = x;
            } else if (x > second) {
                second = x;
            }
        }
        const float gap = std::max(0.0f, best - std::max(0.0f, second));
        rowBest[r] = best;
        meanBest += best;
        meanGap += gap;
        minGap = std::min(minGap, gap);
        maxBest = std::max(maxBest, best);
        meanPower += sum / static_cast<float>(kCols);
        if (sum > 1.0e-6f) {
            float entropy = 0.0f;
            for (int c = 0; c < kCols; ++c) {
                const float p = cache.x[r * kCols + c] / sum;
                if (p > 1.0e-6f) entropy -= p * std::log(p) / std::log(8.0f);
            }
            meanEntropy += entropy;
        } else {
            meanEntropy += 1.0f;
        }
    }
    meanBest /= static_cast<float>(kRows);
    meanGap /= static_cast<float>(kRows);
    meanEntropy /= static_cast<float>(kRows);
    meanPower /= static_cast<float>(kRows);
    float rowMean = 0.0f;
    for (float v : rowBest) rowMean += v;
    rowMean /= static_cast<float>(kRows);
    for (float v : rowBest) {
        const float d = v - rowMean;
        timeVariance += d * d;
    }
    timeVariance = std::sqrt(timeVariance / static_cast<float>(kRows));
    float toneMax = 0.0f;
    float toneTotal = 0.0f;
    for (float v : toneSum) {
        toneMax = std::max(toneMax, v);
        toneTotal += v;
    }
    toneConcentration = (toneTotal > 1.0e-6f) ? (toneMax / toneTotal) : 0.0f;

    const int base = kConvFilters * 2;
    cache.features[base + 0] = meanBest;
    cache.features[base + 1] = meanGap;
    cache.features[base + 2] = 1.0f - meanEntropy;
    cache.features[base + 3] = meanPower;
    cache.features[base + 4] = minGap;
    cache.features[base + 5] = maxBest;
    cache.features[base + 6] = timeVariance;
    cache.features[base + 7] = toneConcentration;

    float z = m_denseBias;
    for (int i = 0; i < kFeatureCount; ++i) {
        z += m_dense[i] * cache.features[i];
    }
    cache.probability = sigmoid(z);

    if (keepActivity) {
        QVector<float> act;
        act.reserve(kInput + kConvFilters + kFeatureCount + 1);
        for (float v : cache.x) act.append(clamp01(v));
        for (int f = 0; f < kConvFilters; ++f) act.append(clamp01(0.5f + 0.5f * cache.features[f]));
        for (float v : cache.features) act.append(clamp01(0.5f + 0.5f * v));
        act.append(cache.probability);
        m_lastActivity = act;
    }

    return cache;
}

QVector<float> DeepDspTinyNet::predict(const QVector<float> &input)
{
    if (input.size() != kInput) return {};
    const ForwardCache cache = forwardInternal(input, true);
    return outputVector(cache.probability);
}

double DeepDspTinyNet::trainOne(const QVector<float> &input, const QVector<float> &target, float learningRate)
{
    if (input.size() != kInput || target.isEmpty()) return 0.0;
    const float y = clamp01(target.constFirst());
    ForwardCache cache = forwardInternal(input, true);
    const float p = clamp01(cache.probability);
    const float eps = 1.0e-5f;
    const float loss = -(y * std::log(std::max(eps, p)) + (1.0f - y) * std::log(std::max(eps, 1.0f - p)));

    // BCE + sigmoid derivative: dL/dz = p-y. Give failed/negative candidates a
    // slightly higher weight so the replay buffer learns pruning conservatively.
    const float classWeight = (y >= 0.5f) ? 1.0f : 1.25f;
    const float dz = clampGrad((p - y) * classWeight);

    std::array<float, kFeatureCount> oldDense = m_dense;
    for (int i = 0; i < kFeatureCount; ++i) {
        m_dense[i] -= learningRate * dz * cache.features[i];
    }
    m_denseBias -= learningRate * dz;

    // Backprop through average/max pooled convolution features. Handcrafted
    // features are read-only; they help the ranker but do not need gradients.
    for (int f = 0; f < kConvFilters; ++f) {
        std::array<float, kKernel * kKernel> gradKernel{};
        float gradBias = 0.0f;
        for (int idx = 0; idx < kConvSize; ++idx) {
            const int r = idx / kCols;
            const int c = idx % kCols;
            float dAct = dz * oldDense[f] / static_cast<float>(kConvSize);
            if (idx == cache.maxIndex[f]) {
                dAct += dz * oldDense[kConvFilters + f];
            }
            const float a = cache.conv[f][idx];
            const float dPre = clampGrad(dAct * (1.0f - a * a));
            int k = 0;
            for (int kr = -1; kr <= 1; ++kr) {
                for (int kc = -1; kc <= 1; ++kc) {
                    gradKernel[k++] += dPre * safeInputAt(cache.x, r + kr, c + kc);
                }
            }
            gradBias += dPre;
        }
        for (int k = 0; k < kKernel * kKernel; ++k) {
            m_kernel[f][k] -= learningRate * gradKernel[k] / static_cast<float>(kConvSize);
        }
        m_convBias[f] -= learningRate * gradBias / static_cast<float>(kConvSize);
    }

    return static_cast<double>(loss);
}

double DeepDspTinyNet::trainBatch(const QVector<QVector<float>> &inputs,
                                  const QVector<QVector<float>> &targets,
                                  float learningRate)
{
    const int nAll = std::min(inputs.size(), targets.size());
    if (nAll <= 0) return 0.0;
    double loss = 0.0;
    int n = 0;
    for (int i = 0; i < nAll; ++i) {
        if (inputs[i].size() != kInput || targets[i].isEmpty()) continue;
        loss += trainOne(inputs[i], targets[i], learningRate);
        ++n;
    }
    m_lastBatchSize = n;
    return n > 0 ? loss / static_cast<double>(n) : 0.0;
}

bool DeepDspTinyNet::save(const QString &path) const
{
    QFileInfo info(path);
    QDir().mkpath(info.absolutePath());
    QFile f(path + QStringLiteral(".tmp"));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    QDataStream out(&f);
    out.setVersion(QDataStream::Qt_5_12);
    out << kMagic << kVersion;
    for (int fi = 0; fi < kConvFilters; ++fi) {
        for (float v : m_kernel[fi]) out << v;
        out << m_convBias[fi];
    }
    for (float v : m_dense) out << v;
    out << m_denseBias;
    f.close();
    QFile::remove(path);
    if (!QFile::rename(f.fileName(), path)) {
        QFile::remove(f.fileName());
        return false;
    }
    return true;
}

bool DeepDspTinyNet::load(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    QDataStream in(&f);
    in.setVersion(QDataStream::Qt_5_12);
    quint32 magic = 0;
    quint32 version = 0;
    in >> magic >> version;
    if (magic != kMagic || version != kVersion) return false;
    for (int fi = 0; fi < kConvFilters; ++fi) {
        for (float &v : m_kernel[fi]) in >> v;
        in >> m_convBias[fi];
    }
    for (float &v : m_dense) in >> v;
    in >> m_denseBias;
    return in.status() == QDataStream::Ok;
}

QVector<float> DeepDspTinyNet::lastActivity() const
{
    return m_lastActivity;
}
