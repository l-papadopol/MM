// SPDX-License-Identifier: GPL-3.0-or-later
// Link with app objects and -fno-access-control, run in isolated settings dir.
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QApplication>
#include <QSettings>
#include <QTableWidget>
#include <QPushButton>
#include <QCheckBox>
#include <QItemSelectionModel>
#include <QSpinBox>
#include <iostream>
#include <stdexcept>
static void check(bool b,const char*s){if(!b){std::cerr<<"CHECK FAILED: "<<s<<std::endl;throw std::runtime_error(s);}}
int main(int argc,char **argv){
 QApplication app(argc,argv);app.setApplicationName("MadModem");QSettings settings(AppSettings::settingsFilePath(),QSettings::IniFormat);settings.setValue("Updates/checkAtStartup",false);settings.sync();
 MainWindow w;w.show();QObject::disconnect(w.m_ftSlotScheduler,nullptr,&w,nullptr);
 for(QString mode:{QString("FT8"),QString("FT4")}){
  w.m_ft8OperatorStopLatched=true;w.stopFt8Sequencer("CQ probe reset");w.ui->cmbMode->setCurrentText(mode);w.m_ft8OperatorStopLatched=false;
  w.m_settings.textMyCallsign="IZ6NNH";w.m_settings.textMyLocator="JN63";w.m_rxRunning=true;w.m_chkFtCqQueue->setChecked(false);w.m_ftCqWorkList.clear();w.m_tableFt8Rx->setRowCount(0);
  const int period=mode=="FT8"?15000:7500;const qint64 slot=QDateTime::currentMSecsSinceEpoch()/period*period;
  auto decode=[&](const QString&message,qint64 utc,int hz){Ft8RxDecoder::Decode d;d.message=message;d.slotStartUtcMs=utc;d.slotPeriodMs=period;d.utc=QDateTime::fromMSecsSinceEpoch(utc,Qt::UTC).toString("HHmmss");d.snrDb=-10;d.frequencyHz=hz;w.handleFt8DecodeReady(d);};
  decode("CQ G0JEI IO93",slot-period*4,1000);decode("CQ DL2LAU JO31",slot-period*4,1500);
  w.m_tableFt8Rx->clearSelection();for(int row:{0,1})w.m_tableFt8Rx->selectionModel()->select(w.m_tableFt8Rx->model()->index(row,0),QItemSelectionModel::Select|QItemSelectionModel::Rows);
  w.addSelectedFtCqsToWorkList();check(w.m_ftCqWorkList.entries.size()==2,"multi-selection did not add both CQs");check(!w.m_ft8PendingTxArmed&&!w.m_ftSession.qsoActive,"adding CQ armed TX");
  check(!w.m_ft8EvilModeUnlocked,"fixture must not require Evil Mode");
  w.m_btnFtCqRun->click();check(w.m_ftCqWorkList.running&&!w.m_ft8PendingTxArmed,"old CQ caused immediate call");
  w.m_ft8DecodeBatchPhase="boundary presentation";decode("CQ DL2LAU JO31",slot,1510);w.processFtCqWorkList();check(!w.m_ft8PendingTxArmed,"presentation preview authorized CQ list TX");w.m_ft8DecodeBatchPhase.clear();
  decode("F4JXL G0JEI -11",slot,1000);decode("CQ DL2LAU JO31",slot,1510);w.processFtCqWorkList();
  check(w.m_ftSession.dxCall=="DL2LAU"&&w.m_ft8PendingTxArmed,"fresh CQ not served after busy station skipped");
  check(w.m_pendingFt8TxPlan.message.contains("DL2LAU IZ6NNH"),"wrong planned correspondent");
  check(!w.m_pendingFt8TxPlan.latePartial,"CQ list allowed a manual partial frame");
  check(w.m_ftCqWorkList.entries.isEmpty(),"selected targets not consumed");
  // Queue-mode double clicks add once and never replace an active correspondent.
  decode("CQ ON4TEST JO20",slot,1700);decode("CQ F4JXL JN18",slot,1800);
  const auto dx=w.m_ftSession.dxCall;const auto plan=w.m_pendingFt8TxPlan.message;
  for(int row:{w.m_tableFt8Rx->rowCount()-2,w.m_tableFt8Rx->rowCount()-1,w.m_tableFt8Rx->rowCount()-2})w.handleFt8DecodeDoubleClicked(w.m_tableFt8Rx->item(row,4));
  check(w.m_ftCqWorkList.entries.size()==2,"repeated double clicks did not add two unique targets");
  check(w.m_ftSession.dxCall==dx&&w.m_pendingFt8TxPlan.message==plan,"queue double click replaced active QSO");
  w.refreshFtCqWorkListUi();check(w.m_ftCqTable->columnCount()==5&&w.m_ftCqTable->item(0,0)->text()=="1"&&w.m_ftCqTable->item(1,0)->text()=="2","numbered order table missing");
  // Completion returns to the first pending target. A later fresh CQ may
  // not overtake it; neither may an observation made during the previous QSO.
  w.processFtCqWorkList();w.stopFt8Sequencer("normal QSO completion fixture");
  w.processFtCqWorkList();
  const qint64 nextSlot=QDateTime::currentMSecsSinceEpoch()/period*period;
  w.m_ftCqWorkList.entries[0].confirmed=false;
  decode("CQ F4JXL JN18",nextSlot,1800);w.processFtCqWorkList();
  check(!w.m_ft8PendingTxArmed,"later fresh CQ overtook first queued target");
  decode("CQ ON4TEST JO20",nextSlot,1700);w.processFtCqWorkList();
  check(w.m_ft8PendingTxArmed&&w.m_ftSession.dxCall=="ON4TEST"&&w.m_ftCqWorkList.entries.size()==1,"next QSO did not follow the numbered order");
  w.m_btnFtCqRun->click();check(!w.m_ftCqWorkList.running&&w.m_ft8PendingTxArmed,"Pause interrupted current QSO");
  w.m_ftCqWorkList.running=true;w.m_ft8OperatorStopLatched=true;w.stopFt8Sequencer("operator STOP fixture");check(!w.m_ftCqWorkList.running&&!w.m_ft8PendingTxArmed,"STOP failed to pause list/cancel arm");
  w.refreshFtCqWorkListUi();if(argc>1)w.m_ftCqDialog->grab().save(QString(argv[1])+"-"+mode+".png");
  std::cout<<mode.toStdString()<<" PASS multi-select/add-no-TX/explicit-start/old-CQ-wait/preview-no-TX/busy-skip/fresh-QSO/whole-frame/double-click-dedup/active-QSO-preserved/order-table/next-QSO/order-respected/pause-preserves-QSO/STOP\n";
 }
 w.m_rxRunning=false;w.close();app.processEvents();
}
