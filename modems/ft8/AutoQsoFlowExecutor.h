#ifndef AUTOQSOFLOWEXECUTOR_H
#define AUTOQSOFLOWEXECUTOR_H

#include <QString>
#include <QStringList>

/**
 * @brief Read-only evaluator for the visual AutoQSO flow.
 *
 * v4.13g intentionally runs this class only in shadow mode: it reads the saved
 * visual flow and produces a human-readable trace, but it never arms PTT,
 * never starts audio TX, never changes CAT, and never mutates the QSO session.
 */
class AutoQsoFlowExecutor final
{
public:
    struct Context
    {
        QString modeName;
        QString decodedMessage;
        QString myCall;
        QString targetCall;
        QString targetGrid;
        QString country;
        QString priorityText;
        QString rejectReason;
        QString suggestedTxMessage;
        QString txStrategy;
        QString txReason;
        int snrDb = 0;
        double dt = 0.0;
        int rxHz = 0;
        int suggestedTxHz = 0;
        bool evilModeUnlocked = false;
        bool autoQsoArmed = false;
        bool txAllowedByRuntime = false;
        bool qsoActive = false;
        bool activeTarget = false;
        bool directReplyToMe = false;
        bool cqCandidate = false;
        bool blacklisted = false;
        bool watched = false;
        bool workedBefore = false;
        bool duplicateRejected = false;
        bool candidateValid = false;
        bool retryAvailable = false;
        bool qsoComplete = false;
    };

    struct Trace
    {
        QStringList lines;
        QString result;
        bool wouldPrepareReply = false;
        bool wouldArmTx = false;
        bool stoppedByDecision = false;
    };

    static Trace runShadow(const QString &flowJson, const Context &ctx);
};

#endif // AUTOQSOFLOWEXECUTOR_H
