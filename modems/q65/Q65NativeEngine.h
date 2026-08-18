#ifndef Q65NATIVEENGINE_H
#define Q65NATIVEENGINE_H

#include "Q65Mode.h"

#include <QString>
#include <QVector>
#include <QtGlobal>

#include <array>

class Q65NativeEngine
{
public:
    struct Configuration
    {
        int periodSeconds = 60;
        int decodeDepth = 2;
        Q65Mode::Submode submode = Q65Mode::Submode::A;
        int rxFrequencyHz = 1500;
        int dfToleranceHz = 100;
        bool averaging = true;
        bool autoClearAverages = true;
        bool singleDecode = false;
        bool apDecode = true;
        bool maxDrift = false;
        bool emeDelay = false;
        QString myCall;
        QString dxCall;
        QString dxGrid;
    };

    struct Result
    {
        QString message;
        double dtSeconds = 0.0;
        double frequencyHz = 1500.0;
        double driftHz = 0.0;
        double snrDb = -40.0;
        int iterations = 0;
        int averageCount = 1;
        bool assisted = false;
    };

    Q65NativeEngine();

    QVector<Result> decode(const QVector<double> &samples12k,
                           qint64 periodId,
                           const Configuration &configuration);
    void clearAverages();
    int usableAverageCount() const;
    int allAverageCount() const;

    static int symbolSamples(int periodSeconds);
    static int transmittedSamples(int periodSeconds);

private:
    struct SyncCandidate
    {
        int startSample = 0;
        double frequencyHz = 1500.0;
        double driftHz = 0.0;
        double score = 0.0;
        double snrDb = -40.0;
    };

    struct AverageBank
    {
        QVector<float> energies;
        int count = 0;
        int binsPerSymbol = 0;
        int periodSeconds = 0;
        int submodeMultiplier = 0;
    };

    QVector<SyncCandidate> findSyncCandidates(const QVector<double> &samples,
                                              const Configuration &configuration) const;
    SyncCandidate refineSyncCandidate(const QVector<double> &samples,
                                      const Configuration &configuration,
                                      const SyncCandidate &coarse) const;
    double syncPatternScore(const QVector<double> &samples,
                            int symbolLength,
                            int startSample,
                            double frequencyHz,
                            double driftHz,
                            double *snrDb) const;
    QVector<float> extractSymbolEnergies(const QVector<double> &samples,
                                         const Configuration &configuration,
                                         const SyncCandidate &candidate) const;
    bool decodeEnergies(const QVector<float> &energies,
                        const Configuration &configuration,
                        Result *result) const;
    bool decodeAssistedList(const QVector<float> &energies,
                            const Configuration &configuration,
                            Result *result) const;
    QVector<QString> assistedMessages(const Configuration &configuration) const;
    QString unpackSymbols(const int decoded[13],
                          const Configuration &configuration) const;
    QVector<float> updateAverage(const QVector<float> &energies,
                                 int parity,
                                 const Configuration &configuration);

    std::array<AverageBank, 2> m_averageBanks;
    int m_allAverageCount = 0;
};

#endif // Q65NATIVEENGINE_H
