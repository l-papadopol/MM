#ifndef FTSTANDARDMESSAGESET_H
#define FTSTANDARDMESSAGESET_H

#include "FtTxPlan.h"

#include <QString>
#include <QStringList>

/**
 * @brief One authoritative generator for the six standard FT4/FT8 messages.
 *
 * WSJT-X keeps the currently selected standard message as QSO state, not as a
 * collection of unrelated UI strings.  MadModem uses this small value object as
 * the single source for Tx1..Tx6 so the table, sequencer, pending FtTxPlan and
 * retry logic all agree on the exact text.
 */
class FtStandardMessageSet final
{
public:
    enum Row
    {
        Tx1Cq = 0,          ///< CQ MYCALL MYGRID
        Tx2Locator = 1,     ///< DX MYCALL MYGRID
        Tx3Report = 2,      ///< DX MYCALL -nn
        Tx4RReport = 3,     ///< DX MYCALL R-nn
        Tx5Rr73 = 4,        ///< DX MYCALL RR73
        Tx6Final73 = 5,     ///< DX MYCALL 73
        RowCount = 6
    };

    struct Inputs
    {
        QString myCall;
        QString myGrid;
        QString dxCall;
        QString dxGrid;
        QString report;      ///< -nn/+nn, no leading R
        QString rReport;     ///< R-nn/R+nn
        int audioFrequencyHz = 0;
    };

    FtStandardMessageSet() = default;
    explicit FtStandardMessageSet(const Inputs &inputs) { rebuild(inputs); }

    void rebuild(const Inputs &inputs);

    QString message(int row) const;
    QStringList messages() const { return m_messages; }
    bool isValid() const { return m_valid; }
    int findRow(const QString &message) const;

    FtTxPlan makePlan(int row, const QString &tag = QStringLiteral("SEQ")) const;
    FtTxPlan makePlanForMessage(const QString &message, const QString &tag = QStringLiteral("TX")) const;

    QString myCall() const { return m_myCall; }
    QString myGrid() const { return m_myGrid; }
    QString dxCall() const { return m_dxCall; }
    QString dxGrid() const { return m_dxGrid; }
    QString report() const { return m_report; }
    QString rReport() const { return m_rReport; }

private:
    QString m_myCall;
    QString m_myGrid;
    QString m_dxCall;
    QString m_dxGrid;
    QString m_report;
    QString m_rReport;
    int m_audioFrequencyHz = 0;
    bool m_valid = false;
    QStringList m_messages;
};

#endif // FTSTANDARDMESSAGESET_H
