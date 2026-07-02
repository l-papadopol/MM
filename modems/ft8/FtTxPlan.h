#ifndef FTTXPLAN_H
#define FTTXPLAN_H

#include <QString>
#include <QDateTime>

/**
 * @brief Single source of truth for one pending/current FT4/FT8 transmission.
 *
 * MadModem previously kept the armed message in several independent strings
 * (pending message, retry message, table row, banner text, last TX message).
 * FT automation is timing-critical; UI, scheduler and modulator must refer to
 * the same immutable plan for the selected slot.
 */
struct FtTxPlan
{
    int row = -1;                  ///< 0-based Tx1..Tx6 row, or -1 for manual/tune.
    QString message;               ///< Exact message passed to the FT modulator.
    QString tag = QStringLiteral("TX");
    QString dxCall;
    QString dxGrid;
    QString reportSent;
    QString reportReceived;
    QString reason;
    int audioFrequencyHz = 0;
    qint64 slotBoundaryUtcMs = 0;
    int audioTargetDelayMs = 0;
    int pttLeadMs = 0;
    bool autoSequence = false;
    bool retry = false;
    bool tune = false;
    bool finalMessage = false;

    bool isValid() const { return tune || !message.trimmed().isEmpty(); }
};

#endif // FTTXPLAN_H
