// Passive observability for the existing FT automation. No scheduling decisions.
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "runtime/SessionDiagnostics.h"
#include "utils/ApplicationIdentity.h"
#include <QCheckBox>
#include <QComboBox>
#include <QJsonDocument>
#include <QUuid>

namespace {
QString stateName(FtQsoSequencer::State state)
{
    using S = FtQsoSequencer::State;
    switch (state) {
    case S::Idle: return "idle";
    case S::CallingCq: return "calling_cq";
    case S::WaitingDxReply: return "waiting_dx_reply";
    case S::SendingLocator: return "sending_locator";
    case S::WaitingReport: return "waiting_report";
    case S::SendingReport: return "sending_report";
    case S::WaitingRReport: return "waiting_r_report";
    case S::SendingRReport: return "sending_r_report";
    case S::WaitingFinal73: return "waiting_final_73";
    case S::SendingRr73: return "sending_rr73";
    case S::Completed: return "completed";
    }
    return "unknown";
}
}

QString MainWindow::ftAutomationWaitReason() const
{
    if (m_ft8OperatorStopLatched) return "operator_stop";
    if (m_shutdownInProgress || m_runtimeShutdownComplete) return "shutdown";
    if (m_offlineAnalysisActive) return "offline_analysis";
    if (m_ftCqWorkList.running && !m_ftCqWorkList.entries.isEmpty()) return "manual_queue_priority";
    if (m_txRunning || m_ftTxWorkerRunning) return "transmitting";
    if (m_ft8PendingTxArmed || m_hasDeferredFt8TxPlan || m_pendingFt8PttKeyed) return "tx_pending";
    if (m_ftSession.qsoActive) return "qso_active";
    if (m_chkFt8CallerQueue && m_chkFt8CallerQueue->isChecked() && !m_ft8CallerQueue.isEmpty()) return "direct_callers_priority";
    if (stationCallsign().trimmed().isEmpty() || stationLocator().trimmed().isEmpty()) return "missing_station_identity";
    return m_ft8FullAutoCqCandidates.isEmpty() ? "no_cq" : "not_ready";
}

void MainWindow::traceFtAutomation(const QString &action, const QString &reason,
                                  QJsonObject details, bool coalesce)
{
    const bool autoQso = m_chkFt8FullAutoQso && m_chkFt8FullAutoQso->isChecked();
    const QString originalTrace = details.value("trace_id").toString();
    if (!m_ft8EvilModeUnlocked && !autoQso && m_ftAutomationTraceId.isEmpty() && originalTrace.isEmpty()) return;
    if (m_ftAutomationTraceId.isEmpty() && originalTrace.isEmpty()) m_ftAutomationTraceId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    details.insert("trace_id", originalTrace.isEmpty() ? m_ftAutomationTraceId : originalTrace);
    details.insert("trace_schema",1);
    details.insert("version",QStringLiteral(MADMODEM_VERSION_STRING));
    details.insert("build",QStringLiteral(MADMODEM_BUILD_NUMBER));
    details.insert("action", action);
    details.insert("reason", reason);
    details.insert("mode", ui && ui->cmbMode ? ui->cmbMode->currentText() : QString());
    details.insert("band", m_cmbFt8Band ? m_cmbFt8Band->currentText() : m_settings.ft8Band);
    details.insert("evil_unlocked", m_ft8EvilModeUnlocked);
    details.insert("auto_qso", autoQso);
    details.insert("operator_stop", m_ft8OperatorStopLatched);
    details.insert("state", stateName(m_ftSession.state));
    details.insert("qso_active", m_ftSession.qsoActive);
    details.insert("dx_call", m_ftSession.dxCall);
    details.insert("qso_id", m_ftSession.qsoStartUtc.isValid()
        ? m_ftSession.dxCall + QLatin1Char('|') + QString::number(m_ftSession.qsoStartUtc.toMSecsSinceEpoch()) : QString());
    details.insert("cq_queue_size", m_ft8FullAutoCqCandidates.size());
    details.insert("caller_queue_size", m_ft8CallerQueue.size());
    details.insert("manual_queue_size", m_ftCqWorkList.entries.size());
    details.insert("tx_armed", m_ft8PendingTxArmed);
    details.insert("tx_running", m_txRunning || m_ftTxWorkerRunning);
    details.insert("tx_deferred", m_hasDeferredFt8TxPlan);
    details.insert("tx_plan_id", m_ft8PendingTxToken);
    details.insert("tx_slot_utc_ms", m_pendingFt8TxPlan.slotBoundaryUtcMs);
    details.insert("tx_message", m_pendingFt8TxPlan.message);
    details.insert("tx_tag", m_pendingFt8TxPlan.tag);
    details.insert("tx_audio_hz", m_pendingFt8TxPlan.audioFrequencyHz);
    details.insert("tx_first_period", m_settings.ft8TxFirstPeriod);
    details.insert("retry_remaining", m_ftSession.retryRemaining);
    details.insert("report_sent", m_ftSession.reportSent);
    details.insert("report_received", m_ftSession.reportReceived);
    if (details.contains("decision_action")) {
        static const char *names[] = {"none","ignore","arm_row","interrupt_and_arm_row","retry_current","complete","stop_tx"};
        const int index = details.value("decision_action").toInt(-1);
        if (index >= 0 && index < 7) details.insert("decision_name",QString::fromLatin1(names[index]));
    }
    const auto compact = QJsonDocument(details).toJson(QJsonDocument::Compact);
    if (coalesce) {
        const QString key = action + QLatin1Char('|') + reason + QLatin1Char('|') + details.value("candidate_call").toString();
        const auto now = QDateTime::currentMSecsSinceEpoch();
        const auto it = m_ftAutomationLastTrace.constFind(key);
        if (it != m_ftAutomationLastTrace.cend() && it->first == compact && now - it->second < 30000) return;
        if (m_ftAutomationLastTrace.size() >= 256) m_ftAutomationLastTrace.clear();
        m_ftAutomationLastTrace.insert(key, qMakePair(compact, now));
    }
    // Text is always available in Runtime log and its export. The same fields
    // are machine-readable in the optional complete .jsonl.gz session log.
    appendLog(QStringLiteral("FT automation: ") + QString::fromUtf8(compact));
    SessionDiagnostics::instance().record("ft.automation", details);
}
