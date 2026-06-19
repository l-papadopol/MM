#include "DeepDspTinyNet.h"

#include <QDataStream>
#include <QFile>
#include <QFileInfo>
#include <QDir>

#include <algorithm>
#include <cmath>
#include <random>

namespace {
constexpr quint32 kMagic = 0x4D4E4E53u; // MNNS, MIND Eigen checkpoint
constexpr quint32 kVersion = 4u;

float clampGrad(float x)
{
    if (x > 5.0f) return 5.0f;
    if (x < -5.0f) return -5.0f;
    return x;
}

float activationTo01(float x)
{
    return std::max(0.0f, std::min(1.0f, 0.5f + 0.5f * x));
}

float clamp01(float x)
{
    return std::max(0.0f, std::min(1.0f, x));
}
} // namespace

DeepDspTinyNet::DeepDspTinyNet()
{
    reset();
}

void DeepDspTinyNet::reset(unsigned seed)
{
    std::mt19937 rng(seed);
    auto init = [&rng](int fanIn) {
        const float scale = 1.0f / std::sqrt(static_cast<float>(std::max(1, fanIn)));
        std::uniform_real_distribution<float> dist(-scale, scale);
        return dist(rng);
    };

    for (int r = 0; r < kHidden1; ++r) {
        for (int c = 0; c < kInput; ++c) m_w1(r, c) = init(kInput);
    }
    m_b1.setZero();

    for (int r = 0; r < kHidden2; ++r) {
        for (int c = 0; c < kHidden1; ++c) m_w2(r, c) = init(kHidden1);
    }
    m_b2.setZero();

    for (int r = 0; r < kOutput; ++r) {
        for (int c = 0; c < kHidden2; ++c) m_w3(r, c) = init(kHidden2);
    }
    m_b3.setZero();

    m_lastInput.setZero();
    m_a1.setZero();
    m_a2.setZero();
    m_out.setZero();
    m_lastActivity.clear();
}

DeepDspTinyNet::InputVec DeepDspTinyNet::vectorToInput(const QVector<float> &input) const
{
    InputVec v;
    v.setZero();
    const int n = std::min(kInput, input.size());
    for (int i = 0; i < n; ++i) v(i) = input[i];
    return v;
}

DeepDspTinyNet::OutputVec DeepDspTinyNet::vectorToOutput(const QVector<float> &target) const
{
    OutputVec v;
    v.setZero();
    const int n = std::min(kOutput, target.size());
    for (int i = 0; i < n; ++i) v(i) = target[i];
    return v;
}

QVector<float> DeepDspTinyNet::outputToVector(const OutputVec &output) const
{
    QVector<float> v;
    v.reserve(kOutput);
    for (int i = 0; i < kOutput; ++i) v.append(output(i));
    return v;
}

QVector<float> DeepDspTinyNet::flattenMatrix(const float *data, int count) const
{
    QVector<float> v;
    v.reserve(count);
    for (int i = 0; i < count; ++i) v.append(data[i]);
    return v;
}

bool DeepDspTinyNet::assignFromVector(const QVector<float> &src, float *dst, int count)
{
    if (src.size() != count) return false;
    for (int i = 0; i < count; ++i) dst[i] = src[i];
    return true;
}

QVector<float> DeepDspTinyNet::forward(const QVector<float> &input, bool keepActivity)
{
    if (input.size() != kInput) return {};
    m_lastInput = vectorToInput(input);

    m_a1 = (m_w1 * m_lastInput + m_b1).array().tanh().matrix();
    m_a2 = (m_w2 * m_a1 + m_b2).array().tanh().matrix();

    const OutputVec z3 = m_w3 * m_a2 + m_b3;
    for (int i = 0; i < kOutput; ++i) {
        const float z = z3(i);
        if (z >= 40.0f) m_out(i) = 1.0f;
        else if (z <= -40.0f) m_out(i) = 0.0f;
        else m_out(i) = 1.0f / (1.0f + std::exp(-z));
    }

    if (keepActivity) {
        QVector<float> act;
        act.reserve(kInput + kHidden1 + kHidden2 + kOutput);
        for (int i = 0; i < kInput; ++i) act.append(clamp01(std::abs(m_lastInput(i))));
        for (int i = 0; i < kHidden1; ++i) act.append(activationTo01(m_a1(i)));
        for (int i = 0; i < kHidden2; ++i) act.append(activationTo01(m_a2(i)));
        for (int i = 0; i < kOutput; ++i) act.append(clamp01(m_out(i)));
        m_lastActivity = act;
    }

    return outputToVector(m_out);
}

QVector<float> DeepDspTinyNet::predict(const QVector<float> &input)
{
    return forward(input, true);
}

double DeepDspTinyNet::trainOne(const QVector<float> &input, const QVector<float> &target, float learningRate)
{
    if (input.size() != kInput || target.size() != kOutput) return 0.0;
    const QVector<float> predVec = forward(input, true);
    if (predVec.size() != kOutput) return 0.0;

    const OutputVec targetVec = vectorToOutput(target);
    OutputVec d3;
    double mse = 0.0;
    for (int o = 0; o < kOutput; ++o) {
        const float diff = m_out(o) - targetVec(o);
        mse += static_cast<double>(diff) * static_cast<double>(diff);
        d3(o) = clampGrad((2.0f * diff / static_cast<float>(kOutput)) * m_out(o) * (1.0f - m_out(o)));
    }

    Hidden2Vec d2 = (m_w3.transpose() * d3).array() * (1.0f - m_a2.array().square());
    Hidden1Vec d1 = (m_w2.transpose() * d2).array() * (1.0f - m_a1.array().square());
    for (int i = 0; i < kHidden2; ++i) d2(i) = clampGrad(d2(i));
    for (int i = 0; i < kHidden1; ++i) d1(i) = clampGrad(d1(i));

    m_w3 -= learningRate * (d3 * m_a2.transpose());
    m_b3 -= learningRate * d3;
    m_w2 -= learningRate * (d2 * m_a1.transpose());
    m_b2 -= learningRate * d2;
    m_w1 -= learningRate * (d1 * m_lastInput.transpose());
    m_b1 -= learningRate * d1;

    return mse / static_cast<double>(kOutput);
}

double DeepDspTinyNet::trainBatch(const QVector<QVector<float>> &inputs,
                                  const QVector<QVector<float>> &targets,
                                  float learningRate)
{
    const int nAll = std::min(inputs.size(), targets.size());
    if (nAll <= 0) return 0.0;

    int n = 0;
    for (int i = 0; i < nAll; ++i) {
        if (inputs[i].size() == kInput && targets[i].size() == kOutput) ++n;
    }
    if (n <= 0) return 0.0;

    InputBatchMat x(kInput, n);
    OutputBatchMat target(kOutput, n);
    int col = 0;
    for (int i = 0; i < nAll; ++i) {
        if (inputs[i].size() != kInput || targets[i].size() != kOutput) continue;
        for (int r = 0; r < kInput; ++r) x(r, col) = inputs[i][r];
        for (int r = 0; r < kOutput; ++r) target(r, col) = targets[i][r];
        ++col;
    }
    m_lastBatchSize = n;

    Hidden1BatchMat a1 = (m_w1 * x).colwise() + m_b1;
    a1 = a1.array().tanh().matrix();

    Hidden2BatchMat a2 = (m_w2 * a1).colwise() + m_b2;
    a2 = a2.array().tanh().matrix();

    OutputBatchMat z3 = (m_w3 * a2).colwise() + m_b3;
    OutputBatchMat out(kOutput, n);
    for (int c = 0; c < n; ++c) {
        for (int r = 0; r < kOutput; ++r) {
            const float z = z3(r, c);
            if (z >= 40.0f) out(r, c) = 1.0f;
            else if (z <= -40.0f) out(r, c) = 0.0f;
            else out(r, c) = 1.0f / (1.0f + std::exp(-z));
        }
    }

    const OutputBatchMat diff = out - target;
    const double mse = diff.array().square().mean();

    // Gradient of mean squared error across output bits and batch columns.
    OutputBatchMat d3 = ((2.0f / static_cast<float>(kOutput * n)) * diff.array() *
                         out.array() * (1.0f - out.array())).matrix();
    d3 = d3.unaryExpr([](float v) { return clampGrad(v); });

    Hidden2BatchMat d2 = (m_w3.transpose() * d3).array() * (1.0f - a2.array().square());
    d2 = d2.unaryExpr([](float v) { return clampGrad(v); });

    Hidden1BatchMat d1 = (m_w2.transpose() * d2).array() * (1.0f - a1.array().square());
    d1 = d1.unaryExpr([](float v) { return clampGrad(v); });

    m_w3 -= learningRate * (d3 * a2.transpose());
    m_b3 -= learningRate * d3.rowwise().sum();
    m_w2 -= learningRate * (d2 * a1.transpose());
    m_b2 -= learningRate * d2.rowwise().sum();
    m_w1 -= learningRate * (d1 * x.transpose());
    m_b1 -= learningRate * d1.rowwise().sum();

    // Preserve live visualization from the first column of the current batch.
    m_lastInput = x.col(0);
    m_a1 = a1.col(0);
    m_a2 = a2.col(0);
    m_out = out.col(0);
    QVector<float> act;
    act.reserve(kInput + kHidden1 + kHidden2 + kOutput);
    for (int i = 0; i < kInput; ++i) act.append(clamp01(std::abs(m_lastInput(i))));
    for (int i = 0; i < kHidden1; ++i) act.append(activationTo01(m_a1(i)));
    for (int i = 0; i < kHidden2; ++i) act.append(activationTo01(m_a2(i)));
    for (int i = 0; i < kOutput; ++i) act.append(clamp01(m_out(i)));
    m_lastActivity = act;

    return mse;
}

bool DeepDspTinyNet::save(const QString &path) const
{
    const QFileInfo info(path);
    if (!info.dir().exists() && !QDir().mkpath(info.dir().absolutePath())) return false;

    const QString tmpPath = path + QStringLiteral(".tmp");
    QFile::remove(tmpPath);
    QFile f(tmpPath);
    if (!f.open(QIODevice::WriteOnly)) return false;
    QDataStream ds(&f);
    ds.setVersion(QDataStream::Qt_5_12);
    ds << kMagic << kVersion;
    ds << kInput << kHidden1 << kHidden2 << kOutput;
    ds << flattenMatrix(m_w1.data(), kHidden1 * kInput);
    ds << flattenMatrix(m_b1.data(), kHidden1);
    ds << flattenMatrix(m_w2.data(), kHidden2 * kHidden1);
    ds << flattenMatrix(m_b2.data(), kHidden2);
    ds << flattenMatrix(m_w3.data(), kOutput * kHidden2);
    ds << flattenMatrix(m_b3.data(), kOutput);
    if (ds.status() != QDataStream::Ok) {
        f.close();
        QFile::remove(tmpPath);
        return false;
    }
    f.flush();
    f.close();
    QFile::remove(path);
    if (!QFile::rename(tmpPath, path)) {
        QFile::remove(tmpPath);
        return false;
    }
    return true;
}

bool DeepDspTinyNet::load(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    QDataStream ds(&f);
    ds.setVersion(QDataStream::Qt_5_12);
    quint32 magic = 0;
    quint32 version = 0;
    int input = 0;
    int h1 = 0;
    int h2 = 0;
    int output = 0;
    QVector<float> w1, b1, w2, b2, w3, b3;
    ds >> magic >> version;
    ds >> input >> h1 >> h2 >> output;
    ds >> w1 >> b1 >> w2 >> b2 >> w3 >> b3;
    if (ds.status() != QDataStream::Ok || magic != kMagic || version != kVersion) return false;
    if (input != kInput || h1 != kHidden1 || h2 != kHidden2 || output != kOutput) return false;
    if (!assignFromVector(w1, m_w1.data(), kHidden1 * kInput) ||
        !assignFromVector(b1, m_b1.data(), kHidden1) ||
        !assignFromVector(w2, m_w2.data(), kHidden2 * kHidden1) ||
        !assignFromVector(b2, m_b2.data(), kHidden2) ||
        !assignFromVector(w3, m_w3.data(), kOutput * kHidden2) ||
        !assignFromVector(b3, m_b3.data(), kOutput)) {
        return false;
    }
    return true;
}

QVector<float> DeepDspTinyNet::lastActivity() const
{
    return m_lastActivity;
}
