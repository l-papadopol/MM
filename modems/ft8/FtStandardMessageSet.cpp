#include "FtStandardMessageSet.h"

#include "FtDecodedText.h"

#include <QtGlobal>

namespace {

QString normCall(QString s)
{
    return s.trimmed().toUpper();
}

QString normGrid(QString s)
{
    s = s.trimmed().left(4).toUpper();
    return FtDecodedText::isGrid(s) ? s : QString();
}

QString normReport(QString s, bool acknowledged)
{
    s = s.trimmed().toUpper();
    if (s.isEmpty()) {
        s = QStringLiteral("-10");
    }
    if (acknowledged) {
        if (!s.startsWith(QLatin1Char('R'))) {
            s.prepend(QLatin1Char('R'));
        }
        const QString cleaned = FtDecodedText::cleanReport(s);
        return FtDecodedText::isReport(QStringLiteral("R") + cleaned) ? QStringLiteral("R") + cleaned : QStringLiteral("R-10");
    }
    s = FtDecodedText::cleanReport(s);
    return FtDecodedText::isReport(s) ? s : QStringLiteral("-10");
}

} // namespace

void FtStandardMessageSet::rebuild(const Inputs &inputs)
{
    m_myCall = normCall(inputs.myCall);
    m_myGrid = normGrid(inputs.myGrid);
    m_dxCall = normCall(inputs.dxCall);
    m_dxGrid = normGrid(inputs.dxGrid);
    m_report = normReport(inputs.report, false);
    m_rReport = normReport(inputs.rReport.isEmpty() ? inputs.report : inputs.rReport, true);
    m_audioFrequencyHz = inputs.audioFrequencyHz;

    const QString dx = m_dxCall.isEmpty() ? QStringLiteral("DXCALL") : m_dxCall;
    m_messages.clear();

    m_valid = !m_myCall.isEmpty() && !m_myGrid.isEmpty();
    if (!m_valid) {
        for (int i = 0; i < RowCount; ++i) {
            m_messages << QString();
        }
        return;
    }

    m_messages << QStringLiteral("CQ %1 %2").arg(m_myCall, m_myGrid)
               << QStringLiteral("%1 %2 %3").arg(dx, m_myCall, m_myGrid)
               << QStringLiteral("%1 %2 %3").arg(dx, m_myCall, m_report)
               << QStringLiteral("%1 %2 %3").arg(dx, m_myCall, m_rReport)
               << QStringLiteral("%1 %2 RR73").arg(dx, m_myCall)
               << QStringLiteral("%1 %2 73").arg(dx, m_myCall);
}

QString FtStandardMessageSet::message(int row) const
{
    if (row < 0 || row >= m_messages.size()) {
        return QString();
    }
    return m_messages.at(row).trimmed().toUpper();
}

int FtStandardMessageSet::findRow(const QString &message) const
{
    const QString target = message.trimmed().toUpper();
    if (target.isEmpty()) {
        return -1;
    }
    for (int row = 0; row < m_messages.size(); ++row) {
        if (m_messages.at(row).trimmed().toUpper() == target) {
            return row;
        }
    }
    return -1;
}

FtTxPlan FtStandardMessageSet::makePlan(int row, const QString &tag) const
{
    FtTxPlan plan;
    plan.row = row;
    plan.message = message(row);
    plan.tag = tag.trimmed().isEmpty() ? QStringLiteral("SEQ") : tag.trimmed().toUpper();
    plan.dxCall = m_dxCall;
    plan.dxGrid = m_dxGrid;
    plan.reportSent = (row == Tx4RReport) ? m_rReport : m_report;
    plan.reportReceived.clear();
    plan.audioFrequencyHz = m_audioFrequencyHz;
    plan.autoSequence = (plan.tag == QStringLiteral("SEQ") || plan.tag == QStringLiteral("RETRY") || plan.tag == QStringLiteral("CQ"));
    plan.retry = (plan.tag == QStringLiteral("RETRY"));
    plan.finalMessage = (row == Tx5Rr73 || row == Tx6Final73);
    return plan;
}

FtTxPlan FtStandardMessageSet::makePlanForMessage(const QString &messageText, const QString &tag) const
{
    const int row = findRow(messageText);
    if (row >= 0) {
        FtTxPlan plan = makePlan(row, tag);
        plan.message = messageText.trimmed().toUpper();
        return plan;
    }

    FtTxPlan plan;
    plan.row = -1;
    plan.message = messageText.trimmed().toUpper();
    plan.tag = tag.trimmed().isEmpty() ? QStringLiteral("TX") : tag.trimmed().toUpper();
    plan.dxCall = m_dxCall;
    plan.dxGrid = m_dxGrid;
    plan.reportSent = m_report;
    plan.audioFrequencyHz = m_audioFrequencyHz;
    plan.autoSequence = (plan.tag == QStringLiteral("SEQ") || plan.tag == QStringLiteral("RETRY") || plan.tag == QStringLiteral("CQ"));
    plan.retry = (plan.tag == QStringLiteral("RETRY"));
    return plan;
}
