#ifndef DEEPDSPPROFILENET_H
#define DEEPDSPPROFILENET_H

#include <Eigen/Dense>

#include <QVector>
#include <QString>

/**
 * @brief Small dynamic MLP used by non-FT MIND profiles such as CW and RTTY.
 *
 * The native FT profile keeps its fixed Eigen network because the dimensions are
 * performance-sensitive and tied to FT candidate matrices.  This class is for
 * smaller profile-specific learners where clarity, checkpoint compatibility and
 * flexible layer sizes matter more than peak throughput.
 */
class DeepDspProfileNet
{
public:
    DeepDspProfileNet();
    DeepDspProfileNet(const QVector<int> &layers, quint32 checkpointVersion = 1u);

    void reset(const QVector<int> &layers, unsigned seed, quint32 checkpointVersion = 1u);
    QVector<float> predict(const QVector<float> &input);
    double trainOne(const QVector<float> &input, const QVector<float> &target, float learningRate = 0.01f);
    double trainBatch(const QVector<QVector<float>> &inputs,
                      const QVector<QVector<float>> &targets,
                      float learningRate = 0.01f);

    bool save(const QString &path) const;
    bool load(const QString &path);

    QVector<float> lastActivity() const;
    QVector<int> layers() const { return m_layers; }
    int inputCount() const { return m_layers.isEmpty() ? 0 : m_layers.first(); }
    int outputCount() const { return m_layers.isEmpty() ? 0 : m_layers.last(); }

private:
    QVector<float> forward(const QVector<float> &input, bool keepActivity);
    static float sigmoid(float z);
    static float clampGrad(float x);
    static QVector<float> eigenToVector(const Eigen::VectorXf &v);
    static Eigen::VectorXf vectorToEigen(const QVector<float> &v, int expected);
    static QVector<float> flattenMatrix(const Eigen::MatrixXf &m);
    static bool assignMatrix(const QVector<float> &src, Eigen::MatrixXf &dst);

    QVector<int> m_layers;
    quint32 m_checkpointVersion = 1u;
    QVector<Eigen::MatrixXf> m_w;
    QVector<Eigen::VectorXf> m_b;
    QVector<Eigen::VectorXf> m_a;
    QVector<float> m_lastActivity;
};

#endif // DEEPDSPPROFILENET_H
