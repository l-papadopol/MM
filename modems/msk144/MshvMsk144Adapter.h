#ifndef MSHVMSK144ADAPTER_H
#define MSHVMSK144ADAPTER_H

#include "Msk144RxCore.h"

#include <QObject>
#include <QStringList>
#include <QVector>

class DecoderMs;

class MshvMsk144Adapter : public QObject
{
    Q_OBJECT
public:
    explicit MshvMsk144Adapter(QObject *parent = nullptr);
    ~MshvMsk144Adapter() override;

    Msk144RxCoreResult decodePeriod(const QVector<float> &samples12k,
                                    const QDateTime &periodStartUtc,
                                    const Msk144RxCoreConfig &config);

private slots:
    void handleMshvDecode(QStringList fields, bool, bool);

private:
    QVector<Msk144CoreDecode> m_decodes;
    QDateTime m_periodStartUtc;
    int m_rxFrequencyHz = 1500;
};

#endif // MSHVMSK144ADAPTER_H
