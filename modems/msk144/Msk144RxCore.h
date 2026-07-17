#ifndef MSK144RXCORE_H
#define MSK144RXCORE_H

#include <QDateTime>
#include <QString>
#include <QVector>
#include <complex>

struct Msk144CoreDecode
{
    QDateTime utc;
    double tSeconds = 0.0;
    int snrDb = 0;
    int dfHz = 0;
    double frequencyHz = 1500.0;
    QString message;
    int navg = 0;
    double eye = 0.0;
    bool shortMessage = false;
};

struct Msk144RxCoreConfig
{
    int periodSeconds = 15;
    int decodeDepth = 2;       // 1 fast, 2 normal, 3 deep
    int rxFrequencyHz = 1500;
    int dfToleranceHz = 100;
    bool shortMessages = false;
    bool swl = false;
    bool contest = false;
    QString myCall;
    QString dxCall;

};

struct Msk144RxCoreResult
{
    QVector<Msk144CoreDecode> decodes;
    QString status;
    int candidatesTried = 0;
    int syncCandidates = 0;
    int coarseCandidates = 0;
    int shortCandidatesTried = 0;
};

class Msk144RxCore
{
public:
    Msk144RxCore() = default;

    static constexpr int internalSampleRate() { return 12000; }
    static constexpr int frameSamples() { return 864; }

    Msk144RxCoreResult decodePeriod(const QVector<float> &samples12k,
                                    const QDateTime &periodStartUtc,
                                    const Msk144RxCoreConfig &config) const;

    double bandEnergyGoertzel(const QVector<float> &samples, int start, int count, double frequencyHz) const;

private:
    struct Candidate
    {
        int start = 0;
        int frequencyHz = 1500;
        double syncMetric = 0.0;
        double energyMetric = 0.0;
        double standardScore = -1.0;
        double shortScore = -1.0;
        bool shortHint = false;
    };

    QVector<Candidate> findReferenceCandidates(const QVector<float> &samples12k,
                                               const Msk144RxCoreConfig &config,
                                               int *coarseCount) const;
    double scoreSyncCandidate(const QVector<float> &samples12k,
                              int startSample,
                              double frequencyHz,
                              bool shortMessage,
                              double *energyMetric) const;
    double rawSyncSoftAt(const QVector<float> &samples12k,
                         int startSample,
                         int frameSamples,
                         double frequencyHz,
                         int symbolIndex) const;
    void makeBaseband(const QVector<float> &samples12k,
                      int startSample,
                      double frequencyHz,
                      int count,
                      QVector<std::complex<double>> &frame) const;
    void makeBasebandFrame(const QVector<float> &samples12k,
                           int startSample,
                           double frequencyHz,
                           QVector<std::complex<double>> &frame) const;
    bool demodulateMskSoft(const QVector<std::complex<double>> &frame,
                           int symbols,
                           QVector<double> &soft) const;
    double orientAndNormalizeSoft(QVector<double> &soft,
                                  const QVector<int> &syncOffsets,
                                  const QVector<int> &syncBits) const;
    bool tryDecodeFrameAt(const QVector<float> &samples12k,
                          const QDateTime &periodStartUtc,
                          const Msk144RxCoreConfig &config,
                          int startSample,
                          double frequencyHz,
                          Msk144CoreDecode &decode) const;
    bool tryDecodeCoherentAt(const QVector<float> &samples12k,
                             const QDateTime &periodStartUtc,
                             const Msk144RxCoreConfig &config,
                             int startSample,
                             double frequencyHz,
                             Msk144CoreDecode &decode) const;
    bool tryDecodeShortAt(const QVector<float> &samples12k,
                          const QDateTime &periodStartUtc,
                          const Msk144RxCoreConfig &config,
                          int startSample,
                          double frequencyHz,
                          Msk144CoreDecode &decode) const;
    bool decodeMsk144Frame(const QVector<std::complex<double>> &frame,
                           QString &message,
                           double &qualityMetric) const;
    bool decodeMsk40Frame(const QVector<std::complex<double>> &frame,
                          const Msk144RxCoreConfig &config,
                          QString &message,
                          double &qualityMetric) const;
    double estimateFrameSnrDb(const QVector<float> &samples12k,
                              int startSample,
                              int frameSamples) const;
};

#endif // MSK144RXCORE_H
