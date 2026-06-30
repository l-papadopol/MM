#ifndef DEEPDSPTINYNET_H
#define DEEPDSPTINYNET_H

#include <QVector>
#include <QString>

#include <array>

/**
 * MIND FT candidate ranker.
 *
 * This class intentionally keeps the historical DeepDspTinyNet file/API name
 * so the surrounding controller/UI remains stable, but the model is no longer
 * a 174-bit codeword predictor.  It is a small topology-preserving 2D CNN-like
 * binary classifier over the 58x8 FT8 candidate symbol/tone matrix.
 *
 * Output vector size is 1:
 *   out[0] = P(candidate reaches CRC-valid decode if sent to classical LDPC)
 *
 * Final FT text is still accepted only by the classical decoder path.
 */
class DeepDspTinyNet
{
public:
    DeepDspTinyNet();

    void reset(unsigned seed = 1u);
    QVector<float> predict(const QVector<float> &input);
    double trainOne(const QVector<float> &input, const QVector<float> &target, float learningRate = 0.0025f);
    double trainBatch(const QVector<QVector<float>> &inputs,
                      const QVector<QVector<float>> &targets,
                      float learningRate = 0.0025f);

    int lastBatchSize() const { return m_lastBatchSize; }

    bool save(const QString &path) const;
    bool load(const QString &path);

    QVector<float> lastActivity() const;

    int inputCount() const { return kInput; }
    int hidden1Count() const { return kConvFilters; }
    int hidden2Count() const { return kFeatureCount; }
    int outputCount() const { return kOutput; }

private:
    static constexpr int kRows = 58;
    static constexpr int kCols = 8;
    static constexpr int kInput = kRows * kCols;
    static constexpr int kConvFilters = 8;
    static constexpr int kKernel = 3;
    static constexpr int kConvSize = kRows * kCols;
    static constexpr int kHandFeatures = 8;
    static constexpr int kFeatureCount = kConvFilters * 2 + kHandFeatures;
    static constexpr int kOutput = 1;

    using Kernel = std::array<float, kKernel * kKernel>;

    struct ForwardCache
    {
        QVector<float> x;
        std::array<std::array<float, kConvSize>, kConvFilters> conv{};
        std::array<int, kConvFilters> maxIndex{};
        std::array<float, kFeatureCount> features{};
        float probability = 0.5f;
    };

    static float sigmoid(float z);
    static float clamp01(float x);
    static float clampGrad(float x);
    static float safeInputAt(const QVector<float> &input, int r, int c);
    ForwardCache forwardInternal(const QVector<float> &input, bool keepActivity) const;
    static QVector<float> outputVector(float probability);

    std::array<Kernel, kConvFilters> m_kernel{};
    std::array<float, kConvFilters> m_convBias{};
    std::array<float, kFeatureCount> m_dense{};
    float m_denseBias = 0.0f;

    mutable QVector<float> m_lastActivity;
    int m_lastBatchSize = 0;
};

#endif // DEEPDSPTINYNET_H
