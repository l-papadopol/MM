#include "DeepDspProfileNet.h"

#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <cmath>
#include <random>

namespace {
constexpr quint32 kMagic = 0x4D50524Fu; // MPRO, MIND profile checkpoint

float activationTo01(float x)
{
    return std::max(0.0f, std::min(1.0f, 0.5f + 0.5f * x));
}

float clamp01(float x)
{
    return std::max(0.0f, std::min(1.0f, x));
}
}

DeepDspProfileNet::DeepDspProfileNet()
{
}

DeepDspProfileNet::DeepDspProfileNet(const QVector<int> &layers, quint32 checkpointVersion)
{
    reset(layers, 0x4D494E44u, checkpointVersion);
}

void DeepDspProfileNet::reset(const QVector<int> &layers, unsigned seed, quint32 checkpointVersion)
{
    m_layers = layers;
    m_checkpointVersion = checkpointVersion;
    m_w.clear();
    m_b.clear();
    m_a.clear();
    m_lastActivity.clear();

    if (m_layers.size() < 2) return;

    std::mt19937 rng(seed);
    auto init = [&rng](int fanIn) {
        const float scale = 1.0f / std::sqrt(static_cast<float>(std::max(1, fanIn)));
        std::uniform_real_distribution<float> dist(-scale, scale);
        return dist(rng);
    };

    for (int l = 1; l < m_layers.size(); ++l) {
        const int rows = std::max(1, m_layers[l]);
        const int cols = std::max(1, m_layers[l - 1]);
        Eigen::MatrixXf w(rows, cols);
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) w(r, c) = init(cols);
        }
        m_w.append(w);
        m_b.append(Eigen::VectorXf::Zero(rows));
    }
}

float DeepDspProfileNet::sigmoid(float z)
{
    if (z >= 40.0f) return 1.0f;
    if (z <= -40.0f) return 0.0f;
    return 1.0f / (1.0f + std::exp(-z));
}

float DeepDspProfileNet::clampGrad(float x)
{
    if (x > 5.0f) return 5.0f;
    if (x < -5.0f) return -5.0f;
    return x;
}

QVector<float> DeepDspProfileNet::eigenToVector(const Eigen::VectorXf &v)
{
    QVector<float> out;
    out.reserve(static_cast<int>(v.size()));
    for (int i = 0; i < v.size(); ++i) out.append(v(i));
    return out;
}

Eigen::VectorXf DeepDspProfileNet::vectorToEigen(const QVector<float> &v, int expected)
{
    Eigen::VectorXf out = Eigen::VectorXf::Zero(std::max(1, expected));
    const int n = std::min(expected, v.size());
    for (int i = 0; i < n; ++i) out(i) = v[i];
    return out;
}

QVector<float> DeepDspProfileNet::flattenMatrix(const Eigen::MatrixXf &m)
{
    QVector<float> out;
    out.reserve(static_cast<int>(m.rows() * m.cols()));
    for (int r = 0; r < m.rows(); ++r) {
        for (int c = 0; c < m.cols(); ++c) out.append(m(r, c));
    }
    return out;
}

bool DeepDspProfileNet::assignMatrix(const QVector<float> &src, Eigen::MatrixXf &dst)
{
    if (src.size() != dst.rows() * dst.cols()) return false;
    int idx = 0;
    for (int r = 0; r < dst.rows(); ++r) {
        for (int c = 0; c < dst.cols(); ++c) dst(r, c) = src[idx++];
    }
    return true;
}

QVector<float> DeepDspProfileNet::forward(const QVector<float> &input, bool keepActivity)
{
    if (m_layers.size() < 2 || input.size() != inputCount()) return {};
    m_a.clear();
    m_a.reserve(m_layers.size());
    m_a.append(vectorToEigen(input, inputCount()));

    for (int l = 0; l < m_w.size(); ++l) {
        Eigen::VectorXf z = m_w[l] * m_a.last() + m_b[l];
        if (l == m_w.size() - 1) {
            for (int i = 0; i < z.size(); ++i) z(i) = sigmoid(z(i));
        } else {
            z = z.array().tanh().matrix();
        }
        m_a.append(z);
    }

    if (keepActivity) {
        QVector<float> act;
        for (int l = 0; l < m_a.size(); ++l) {
            const Eigen::VectorXf &v = m_a[l];
            for (int i = 0; i < v.size(); ++i) {
                if (l == 0 || l == m_a.size() - 1) act.append(clamp01(std::abs(v(i))));
                else act.append(activationTo01(v(i)));
            }
        }
        m_lastActivity = act;
    }

    return eigenToVector(m_a.last());
}

QVector<float> DeepDspProfileNet::predict(const QVector<float> &input)
{
    return forward(input, true);
}

double DeepDspProfileNet::trainOne(const QVector<float> &input, const QVector<float> &target, float learningRate)
{
    if (input.size() != inputCount() || target.size() != outputCount()) return 0.0;
    const QVector<float> pred = forward(input, true);
    if (pred.size() != outputCount() || m_a.size() != m_layers.size()) return 0.0;

    Eigen::VectorXf targetVec = vectorToEigen(target, outputCount());
    QVector<Eigen::VectorXf> delta(m_w.size());

    const int last = m_w.size() - 1;
    delta[last] = Eigen::VectorXf::Zero(outputCount());
    double mse = 0.0;
    for (int i = 0; i < outputCount(); ++i) {
        const float diff = m_a.last()(i) - targetVec(i);
        mse += static_cast<double>(diff) * static_cast<double>(diff);
        delta[last](i) = clampGrad((2.0f * diff / static_cast<float>(outputCount())) *
                                   m_a.last()(i) * (1.0f - m_a.last()(i)));
    }

    for (int l = last - 1; l >= 0; --l) {
        delta[l] = (m_w[l + 1].transpose() * delta[l + 1]).array() *
                   (1.0f - m_a[l + 1].array().square());
        for (int i = 0; i < delta[l].size(); ++i) delta[l](i) = clampGrad(delta[l](i));
    }

    for (int l = 0; l < m_w.size(); ++l) {
        m_w[l] -= learningRate * (delta[l] * m_a[l].transpose());
        m_b[l] -= learningRate * delta[l];
    }

    return mse / static_cast<double>(outputCount());
}

double DeepDspProfileNet::trainBatch(const QVector<QVector<float>> &inputs,
                                     const QVector<QVector<float>> &targets,
                                     float learningRate)
{
    const int n = std::min(inputs.size(), targets.size());
    if (n <= 0) return 0.0;
    double loss = 0.0;
    int used = 0;
    for (int i = 0; i < n; ++i) {
        if (inputs[i].size() == inputCount() && targets[i].size() == outputCount()) {
            loss += trainOne(inputs[i], targets[i], learningRate);
            ++used;
        }
    }
    return used > 0 ? loss / static_cast<double>(used) : 0.0;
}

bool DeepDspProfileNet::save(const QString &path) const
{
    if (m_layers.size() < 2) return false;
    const QFileInfo info(path);
    if (!info.dir().exists() && !QDir().mkpath(info.dir().absolutePath())) return false;
    const QString tmpPath = path + QStringLiteral(".tmp");
    QFile::remove(tmpPath);
    QFile f(tmpPath);
    if (!f.open(QIODevice::WriteOnly)) return false;
    QDataStream ds(&f);
    ds.setVersion(QDataStream::Qt_5_12);
    ds << kMagic << m_checkpointVersion << m_layers;
    ds << static_cast<qint32>(m_w.size());
    for (int i = 0; i < m_w.size(); ++i) {
        ds << static_cast<qint32>(m_w[i].rows())
           << static_cast<qint32>(m_w[i].cols())
           << flattenMatrix(m_w[i]);
        ds << static_cast<qint32>(m_b[i].size())
           << eigenToVector(m_b[i]);
    }
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

bool DeepDspProfileNet::load(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    QDataStream ds(&f);
    ds.setVersion(QDataStream::Qt_5_12);
    quint32 magic = 0;
    quint32 version = 0;
    QVector<int> layers;
    int count = 0;
    ds >> magic >> version >> layers >> count;
    if (ds.status() != QDataStream::Ok || magic != kMagic || version != m_checkpointVersion || layers != m_layers || count != m_w.size()) return false;
    QVector<Eigen::MatrixXf> newW = m_w;
    QVector<Eigen::VectorXf> newB = m_b;
    for (int i = 0; i < count; ++i) {
        int rows = 0;
        int cols = 0;
        QVector<float> wFlat;
        int bSize = 0;
        QVector<float> bFlat;
        ds >> rows >> cols >> wFlat;
        ds >> bSize >> bFlat;
        if (ds.status() != QDataStream::Ok || rows != newW[i].rows() || cols != newW[i].cols() || bSize != newB[i].size()) return false;
        if (!assignMatrix(wFlat, newW[i])) return false;
        for (int j = 0; j < bSize; ++j) newB[i](j) = bFlat.value(j);
    }
    m_w = newW;
    m_b = newB;
    return true;
}

QVector<float> DeepDspProfileNet::lastActivity() const
{
    return m_lastActivity;
}
