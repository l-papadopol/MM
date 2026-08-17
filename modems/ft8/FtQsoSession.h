#ifndef FTQSOSESSION_H
#define FTQSOSESSION_H

#include "FtQsoSequencer.h"
#include "FtTxPlan.h"
#include "FtDecodedText.h"

#include <QDateTime>
#include <QHash>
#include <QString>

/**
 * @brief Owns the mutable FT4/FT8 QSO session state outside MainWindow.
 *
 * MainWindow still owns widgets, CAT/audio objects and the slot scheduler, but it
 * must not also be the QSO state container.  This class is the single mutable
 * session record used by the sequencer, retry logic and auto-log path.
 */
class FtQsoSession final
{
public:
    using State = FtQsoSequencer::State;

    State state = State::Idle;
    State deferredState = State::Idle;

    bool cqRepeatActive = false;
    bool qsoActive = false;
    bool resumeCqAfterQso = false;
    bool autoLogDone = false;

    QDateTime cqRepeatDeadlineUtc;
    int cqRepeatRemaining = 0;
    QDateTime qsoStartUtc;

    QString dxCall;
    QString dxGrid;
    QString reportSent;
    QString reportReceived;
    int audioFreqHz = 0;

    QHash<QString, QString> observedCallGrids;

    QString retryMessage;
    QString retryTag;
    int retryRemaining = 0;

    int activeTxRow = -1;
    QString lastTxMessage;
    QString lastTxTag;
    bool lastTxWasTune = false;

    bool haveLastSnr = false;
    int lastSnrDb = 0;
    QString lastSnrMessage;

    void resetAll();
    void clearQso();
    void clearRetry();
    void clearLastTx();
    void resetForCqRepeat(const QString &myCall, const QString &myGrid, int timeoutMinutes);
    void resetForCqRepeatCount(const QString &myCall, const QString &myGrid, int repeatCount);
    void startQso(const QString &call, const QString &grid, int audioFrequencyHz, const QDateTime &startUtc = QDateTime::currentDateTimeUtc());
    void applyDecision(const FtQsoSequencer::Decision &decision);
    FtQsoSequencer::Context makeContext(const QString &myCall, int rxAudioFreqHz, int txAudioFreqHz) const;
    FtTxPlan makePlan(const QString &message, const QString &tag, int txAudioFreqHz) const;
    void rememberGrid(const QString &call, const QString &grid);
    QString knownGridFor(const QString &call) const;
};

#endif // FTQSOSESSION_H
