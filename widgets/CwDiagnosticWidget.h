#ifndef CWDIAGNOSTICWIDGET_H
#define CWDIAGNOSTICWIDGET_H

#include "../modems/cw/CwDecoder.h"

#include <QVector>
#include <QWidget>

#include <functional>

class CwDiagnosticWidget : public QWidget
{
public:
    explicit CwDiagnosticWidget(QWidget *parent = nullptr);

    void appendSamples(int rank, const QVector<CwDiagnosticPoint> &samples);
    void setSpectrum(int rank,
                     const QVector<float> &offsetsHz,
                     const QVector<float> &inputPsdDb,
                     const QVector<float> &filteredPsdDb,
                     const QVector<float> &theoreticalResponseDb,
                     float trackedOffsetHz,
                     float effectiveBandwidthHz,
                     float interfererOffsetHz,
                     float interfererConfidence,
                     float carrierProminenceDb,
                     float carrierPeakWidthHz,
                     float carrierToSecondDb);
    void clearChannel(int rank);
    void setTextTranslator(std::function<QString(const QString &, const QString &)> translator);
    void retranslateUi();

private:
    class ChannelPane;
    ChannelPane *m_channelA = nullptr;
    ChannelPane *m_channelB = nullptr;
    std::function<QString(const QString &, const QString &)> m_translator;
};

#endif // CWDIAGNOSTICWIDGET_H
