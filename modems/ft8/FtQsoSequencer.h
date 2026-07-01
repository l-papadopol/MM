#ifndef FTQSOSEQUENCER_H
#define FTQSOSEQUENCER_H

#include "FtTxPlan.h"

#include <QString>
#include <QStringList>

/**
 * @brief WSJT-X-style FT4/FT8 QSO state machine core.
 *
 * No audio, CAT, UI or QTimer code belongs here.  The sequencer consumes a
 * parsed decode context and returns a deterministic TX plan/action.  This is
 * the equivalent architectural role of WSJT-X auto_sequence()/processMessage(),
 * but adapted to MadModem's separated scheduler/audio workers.
 */
class FtQsoSequencer final
{
public:
    enum class State
    {
        Idle,
        CallingCq,
        WaitingDxReply,
        SendingLocator,
        WaitingReport,
        SendingReport,
        WaitingRReport,
        SendingRReport,
        WaitingFinal73,
        SendingRr73,
        Completed
    };

    enum class Action
    {
        None,
        Ignore,
        ArmRow,
        RetryCurrent,
        Complete,
        StopTx
    };

    struct ParsedMessage
    {
        QString text;
        QStringList parts;
        QString firstCall;
        QString secondCall;
        QString senderCall;
        QString receiverCall;
        QString grid;
        QString report;
        QString rawReport;
        QString contestExchange;
        bool cq = false;
        bool directed = false;
        bool addressedToMe = false;
        bool containsMyCall = false;
        bool final73 = false;
        bool rr73 = false;
        bool rrr = false;
        bool rReport = false;
        bool contestExchangeLike = false;
        bool contestAck = false;
    };

    struct Decode
    {
        QString message;
        int snrDb = -10;
        int frequencyHz = 0;
        qint64 slotStartUtcMs = 0;
        int slotPeriodMs = 15000;
    };

    struct Context
    {
        QString myCall;
        QString dxCall;
        QString dxGrid;
        QString reportSent;
        QString reportReceived;
        QString lastTxMessage;
        State state = State::Idle;
        bool qsoActive = false;
        bool cqRepeatActive = false;
        bool autoTxEnabled = true;
        int rxAudioFreqHz = 1500;
        int txAudioFreqHz = 1500;
        int startToleranceHz = 25;
        int stopToleranceHz = 50;
    };

    struct Decision
    {
        Action action = Action::None;
        State nextState = State::Idle;
        ParsedMessage parsed;
        FtTxPlan plan;
        bool startsQso = false;
        bool stopsCqRepeat = false;
        QString dxCall;
        QString dxGrid;
        QString reportSent;
        QString reportReceived;
        int audioFreqHz = 0;
        int txRow = -1;          // 0-based row in the six standard FT messages
        QString tag = QStringLiteral("SEQ");
        QString completeReason;
        QString logLine;
        bool refreshStandardMessages = false;
    };

    struct TxCompleteDecision
    {
        Action action = Action::None;
        State nextState = State::Idle;
        QString logLine;
        QString completeReason;
    };

    static bool isCallsignToken(const QString &token);
    static bool isGridToken(const QString &token);
    static bool isReportToken(const QString &token);
    static QString cleanReport(QString report);
    static QString formatSignalReport(int snrDb, bool acknowledged);
    static ParsedMessage parseMessage(const QString &message, const QString &myCall);

    static Decision evaluateDecode(const Decode &decode, const Context &context);
    static TxCompleteDecision onTxCompleted(State state);
};

#endif // FTQSOSEQUENCER_H
