#ifndef Q65DECODER_H
#define Q65DECODER_H

#include "../../audio/AudioBlock.h"
#include "Q65Mode.h"

#include <QObject>
#include <QDateTime>
#include <QString>
#include <QVector>

struct Q65Decode
{
    QDateTime utc;
    double dtSeconds = 0.0;
    int snrDb = 0;
    int dfHz = 0;
    double frequencyHz = 1500.0;
    QString message;
    int averageCount = 0;
    QString submode;
};
Q_DECLARE_METATYPE(Q65Decode)

class DecoderQ65;

class Q65Decoder : public QObject
{
    Q_OBJECT
public:
    explicit Q65Decoder(QObject *parent = nullptr);
    ~Q65Decoder() override;

    void setPeriodSeconds(int seconds);
    void setDecodeDepth(int depth); // 1 fast, 2 normal, 3 deep; MSHV menu semantics
    void setSubmode(Q65Mode::Submode submode);
    void setRxFrequencyHz(int hz);
    void setDfToleranceHz(int hz);
    void setAveragingEnabled(bool enabled);
    void setAutoClearAverages(bool enabled);
    void setSingleDecode(bool enabled);
    void setApDecodeEnabled(bool enabled);
    void setMaxDriftEnabled(bool enabled);
    void setEmeDelayEnabled(bool enabled);
    void setMyCall(const QString &call);
    void setDxCall(const QString &call);
    void setDxGrid(const QString &grid);

public slots:
    void reset();
    void clearAverages();
    void processAudioBlock(const AudioBlock &block);
    void flushPeriod();

signals:
    void decoded(const Q65Decode &decode);
    void statusChanged(const QString &status);
    void periodReady(int secondsBuffered, int periodSeconds);
    void averageStatusChanged(int usableAverageCount, int allAverageCount);

private:
    void appendResampledTo12k(const AudioBlock &block);
    void tryPeriodDecode(bool force);
    void ensureMshvBackend();
    void configureMshvBackend();
    void handleMshvDecodeList(const QStringList &list);
    QString backendStatusText() const;
    QString depthName() const;
    QString submodeName() const;

private:
    QVector<double> m_samples12k;
    int m_inputSampleRate = 48000;
    int m_periodSeconds = 60;
    int m_decodeDepth = 2;
    Q65Mode::Submode m_submode = Q65Mode::Submode::A;
    int m_rxFrequencyHz = 1500;
    int m_dfToleranceHz = 100;
    bool m_averaging = true;
    bool m_autoClearAverages = true;
    bool m_singleDecode = false;
    bool m_apDecode = true;
    bool m_maxDrift = false;
    bool m_emeDelay = false;
    QString m_myCall;
    QString m_dxCall;
    QString m_dxGrid;
    QDateTime m_periodStartUtc;
    QString m_lastStatus;
    int m_avgUsable = 0;
    int m_avgAll = 0;
    DecoderQ65 *m_mshv = nullptr;
};

#endif // Q65DECODER_H
