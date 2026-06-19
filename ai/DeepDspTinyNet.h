#ifndef DEEPDSPTINYNET_H
#define DEEPDSPTINYNET_H

#include <Eigen/Dense>

#include <QVector>
#include <QString>

/**
 * @brief MIND minimal neural engine for shadow learning.
 *
 * MIND is MadModem's internal neural engine: a compact MLP built on Eigen
 * for fast header-only matrix algebra, checkpointing, validation and live
 * activity visualization. It is not a generic AI framework and it remains
 * fail-closed: it observes and learns, but it does not inject decodes or drive
 * TX/CAT/AutoQSO until explicitly enabled by guarded higher-level logic.
 */
class DeepDspTinyNet
{
public:
    DeepDspTinyNet();

    void reset(unsigned seed = 0x4D4D4453u);
    QVector<float> predict(const QVector<float> &input);
    double trainOne(const QVector<float> &input, const QVector<float> &target, float learningRate = 0.01f);
    /**
     * @brief Train an FT-native batch using true Matrix×Matrix Eigen kernels.
     *
     * The old implementation looped over samples and repeatedly called
     * Matrix×Vector forward/backprop, which left Eigen mostly single-threaded.
     * This method stacks the batch as 464×N and 174×N matrices so Eigen/OpenMP
     * can use SIMD and multiple cores for the dense GEMM work.
     */
    double trainBatch(const QVector<QVector<float>> &inputs,
                      const QVector<QVector<float>> &targets,
                      float learningRate = 0.01f);

    int lastBatchSize() const { return m_lastBatchSize; }

    bool save(const QString &path) const;
    bool load(const QString &path);

    QVector<float> lastActivity() const;

    int inputCount() const { return kInput; }
    int hidden1Count() const { return kHidden1; }
    int hidden2Count() const { return kHidden2; }
    int outputCount() const { return kOutput; }

private:
    QVector<float> forward(const QVector<float> &input, bool keepActivity);

    static constexpr int kInput = 464;
    static constexpr int kHidden1 = 128;
    static constexpr int kHidden2 = 64;
    static constexpr int kOutput = 174;

    using InputVec = Eigen::Matrix<float, kInput, 1>;
    using Hidden1Vec = Eigen::Matrix<float, kHidden1, 1>;
    using Hidden2Vec = Eigen::Matrix<float, kHidden2, 1>;
    using OutputVec = Eigen::Matrix<float, kOutput, 1>;
    using W1Mat = Eigen::Matrix<float, kHidden1, kInput, Eigen::RowMajor>;
    using W2Mat = Eigen::Matrix<float, kHidden2, kHidden1, Eigen::RowMajor>;
    using W3Mat = Eigen::Matrix<float, kOutput, kHidden2, Eigen::RowMajor>;
    using InputBatchMat = Eigen::Matrix<float, kInput, Eigen::Dynamic>;
    using Hidden1BatchMat = Eigen::Matrix<float, kHidden1, Eigen::Dynamic>;
    using Hidden2BatchMat = Eigen::Matrix<float, kHidden2, Eigen::Dynamic>;
    using OutputBatchMat = Eigen::Matrix<float, kOutput, Eigen::Dynamic>;

    InputVec vectorToInput(const QVector<float> &input) const;
    OutputVec vectorToOutput(const QVector<float> &target) const;
    QVector<float> outputToVector(const OutputVec &output) const;
    QVector<float> flattenMatrix(const float *data, int count) const;
    bool assignFromVector(const QVector<float> &src, float *dst, int count);

    W1Mat m_w1;
    Hidden1Vec m_b1;
    W2Mat m_w2;
    Hidden2Vec m_b2;
    W3Mat m_w3;
    OutputVec m_b3;

    InputVec m_lastInput;
    Hidden1Vec m_a1;
    Hidden2Vec m_a2;
    OutputVec m_out;
    QVector<float> m_lastActivity;
    int m_lastBatchSize = 0;
};

#endif // DEEPDSPTINYNET_H
