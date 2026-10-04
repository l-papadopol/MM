// Local GCC/Qt probe, linked against the actual application objects.
// PTT=None and preparation held pending: this test never opens TX audio.
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QApplication>
#include <QDateTime>
#include <QDialog>
#include <QComboBox>
#include <QRadioButton>
#include <QTextStream>
#include <QTableWidget>
int main(int argc,char **argv){
 QApplication app(argc,argv);MainWindow w;w.m_ntpClient->setEnabled(false);
 bool ok=true;auto check=[&](bool value,const char *name){QTextStream(stdout)<<(value?"PASS ":"FAIL ")<<name<<'\n';ok &=value;};
 w.m_settings.pttMethod="none";w.m_settings.textMyCallsign="IZ6NNH";w.m_settings.textMyLocator="JN63";
 for(const QString &mode:{QStringLiteral("FT8"),QStringLiteral("FT4")}){
  w.ui->cmbMode->setCurrentText(mode);w.m_ft8OperatorStopLatched=false;
  const auto profile=Ft8Mode::profileForMode(mode);const qint64 now=QDateTime::currentMSecsSinceEpoch();
  const qint64 boundary=now-now%profile.slotMs;
  w.m_pendingFt8TxMessage="EK1KE IZ6NNH JN63";w.m_pendingFt8TxTag="SEQ";
  w.m_ft8PendingTxArmed=true;w.m_ft8PendingTxToken=mode+"-probe";
  w.m_pendingFt8SlotBoundaryUtcMs=boundary;
  w.m_pendingFt8AudioTargetDelayMs=int(now-boundary)+700;
  w.m_pendingFt8PttKeyed=false;w.m_pendingFt8PttPrearmed=false;w.m_txPreparationPending=true;
  w.handleFtSchedulerPendingChanged(false,0);
  check(w.m_ft8PendingTxArmed && !w.m_ft8PendingTxToken.isEmpty(),"old scheduler idle notification cannot revoke new plan");
  const int rows=w.m_tableFt8Rx->rowCount();
  w.startFtPreparedSlotTransmit();
  check(w.m_ft8PendingTxArmed && w.m_ft8AudioStartRequested && !w.m_txRunning,"audio due waits for CAT acknowledgement instead of cancelling slot");
  check(w.m_tableFt8Rx->rowCount()==rows,"CAT wait does not invent TX row");
  w.deferPendingFtTx("test expired attempt");
  check(w.m_ft8LastAttemptedSlotBoundaryUtcMs==boundary,"failed physical slot is consumed once");
  w.stopFt8Shell();app.processEvents();
  check(!w.m_ft8PendingTxArmed && w.m_pendingFt8TxMessage.isEmpty() && !w.m_txPreparationPending,"STOP suppresses queued retry and pending preparation");
  check(w.selectedFt8TxSlotBoundaryUtcMs()>boundary,"retry selection cannot reuse failed slot");
 }
 w.showRuntimeLogDialog();app.processEvents();
 check(w.m_runtimeLogDialog->windowType()==Qt::Window && !w.m_runtimeLogDialog->isModal(),"runtime log is a normal non-modal window");
 QEvent deactivated(QEvent::WindowDeactivate);app.sendEvent(w.m_runtimeLogDialog,&deactivated);
 check(w.m_runtimeLogDialog->isVisible(),"runtime log stays visible after deactivation");
 return ok?0:1;
}
