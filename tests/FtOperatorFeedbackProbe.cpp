// SPDX-License-Identifier: GPL-3.0-or-later
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QApplication>
#include <QSettings>
#include <QRadioButton>
#include <QCheckBox>
#include <QSpinBox>
#include <QPushButton>
#include <QDialog>
#include <QTableWidget>
#include <QComboBox>
#include <QSignalBlocker>
#include <iostream>
#include <stdexcept>
static void check(bool b,const char*why){if(!b)throw std::runtime_error(why);}
int main(int argc,char**argv){QApplication app(argc,argv);app.setApplicationName("MadModem");
 QSettings config(AppSettings::settingsFilePath(),QSettings::IniFormat);config.setValue("Updates/checkAtStartup",false);config.setValue("UI/language","it");config.sync();
 MainWindow w;w.m_settings.textMyCallsign="IZ6NNH";w.m_settings.textMyLocator="JN63";w.resize(1920,1080);w.show();
 for(QString mode:{QString("FT8"),QString("FT4")}){
  w.ui->cmbMode->setCurrentText(mode);w.ui->sideTabWidget->setCurrentWidget(w.ui->tabModeSettings);app.processEvents();
  auto *clock=w.findChild<QWidget*>("ftSlotClock");check(clock,"period clock missing");clock->setFixedSize(200,48);
  const int period=mode=="FT8"?15000:7500;
  for(bool txFirst:{false,true})for(bool firstNow:{false,true}){
   {QSignalBlocker first(w.m_radioFt8TxFirst),second(w.m_radioFt8TxSecond);w.m_radioFt8TxFirst->setChecked(txFirst);w.m_radioFt8TxSecond->setChecked(!txFirst);}
   const int phase=(firstNow?0:period)+period/2;
   // Feed a deliberately stale scheduler role; current radio selection wins.
   w.handleFtSlotUpdated(mode,period,period*2,firstNow,!(txFirst==firstNow),phase,period/2,period/2,QDateTime::currentMSecsSinceEpoch());
   const auto image=clock->grab().toImage();int red=0,green=0;
   for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x){auto c=image.pixelColor(x,y);if(c==QColor(194,66,66))++red;if(c==QColor(32,150,85))++green;}
   check((txFirst==firstNow)?red>green+200:green>red+200,"TX/RX period colour reversed");
  }
  w.m_rxRunning=true;w.m_ft8OperatorStopLatched=false;
  // Enable repeat during a manual CQ already on air. No output device is used.
  {QSignalBlocker block(w.m_chkFt8CqRepeat);w.m_chkFt8CqRepeat->setChecked(false);}
  w.m_ftSession.resetAll();w.m_txRunning=true;w.m_ftSession.lastTxPlan.message="CQ IZ6NNH JN63";w.m_ftSession.lastTxPlan.tag="TX";
  const qint64 base=QDateTime::currentMSecsSinceEpoch()/period*period;
  w.m_ftSession.lastTxPlan.slotBoundaryUtcMs=base;
  w.m_spinFt8CqTimeoutMin->setValue(3);w.m_spinFt8NoResponseLimit->setValue(1);w.m_chkFt8CqRepeat->setChecked(true);
  check(w.m_ftSession.cqRepeatActive&&w.m_ftSession.cqRepeatRemaining==3,"mid-CQ toggle failed to initialize series");
  for(int call=0;call<3;++call){
   w.m_ftSession.lastTxPlan.message="CQ IZ6NNH JN63";w.m_ftSession.lastTxPlan.tag="CQ";w.m_ftSession.lastTxPlan.slotBoundaryUtcMs=base+qint64(call)*period*2;
   w.m_hasDeferredFt8TxPlan=false;w.handleFt8TxCompleted();
   check(w.m_ftSession.cqRepeatRemaining==2-call,"CQ count affected by QSO limit or stale state");
   if(call<2){check(w.m_hasDeferredFt8TxPlan&&w.m_deferredFt8TxPlan.tag=="CQ","next CQ not retained");w.handleFt8TxCompleted();check(w.m_ftSession.cqRepeatRemaining==2-call,"same CQ counted twice");}
  }
  check(!w.m_chkFt8CqRepeat->isChecked()&&!w.m_ftSession.cqRepeatActive,"finished CQ series still appears active");
  w.m_hasDeferredFt8TxPlan=false;w.m_chkFt8CqRepeat->setChecked(true);
  check(w.m_ftSession.cqRepeatRemaining==3,"fresh CQ series inherited exhausted count");
  w.m_ftSession.lastTxPlan.slotBoundaryUtcMs=base+period*8;w.handleFt8TxCompleted();
  w.m_spinFt8CqTimeoutMin->setValue(5);check(w.m_ftSession.cqRepeatRemaining==4,"live CQ count increase lost completed calls");
  w.m_spinFt8NoResponseLimit->setValue(6);check(w.m_ftSession.cqRepeatRemaining==4,"QSO response limit altered CQ budget");
  w.m_spinFt8CqTimeoutMin->setValue(1);check(!w.m_ftSession.cqRepeatActive&&!w.m_hasDeferredFt8TxPlan,"reduced CQ budget retained extra transmission");
  w.m_txRunning=false;w.m_ft8OperatorStopLatched=true;w.stopFt8Sequencer("probe reset");
  {QSignalBlocker block(w.m_chkFt8CqRepeat);w.m_chkFt8CqRepeat->setChecked(false);}
  w.m_rxRunning=true;w.m_chkFt8CqRepeat->setChecked(true);w.stopFt8Shell();app.processEvents();
  check(w.m_ft8OperatorStopLatched&&!w.m_ftSession.cqRepeatActive&&!w.m_ft8PendingTxArmed,"queued CQ enable resurrected after STOP");
  // Exercise the real natural-finish owner: pending-plan cleanup followed by
  // CQ completion must retain exactly one next arm, until the third call.
  w.m_ft8OperatorStopLatched=false;w.m_ftSession.resetForCqRepeatCount("IZ6NNH","JN63",3);
  {QSignalBlocker block(w.m_chkFt8CqRepeat);w.m_chkFt8CqRepeat->setChecked(true);}
  for(int call=0;call<3;++call){
   w.m_ftSession.lastTxPlan.message="CQ IZ6NNH JN63";w.m_ftSession.lastTxPlan.tag="CQ";
   w.m_ftSession.lastTxPlan.slotBoundaryUtcMs=base-qint64(3-call)*period*2;
   w.m_pendingFt8TxPlan=w.m_ftSession.lastTxPlan;w.m_ft8PendingTxArmed=true;
   w.m_hasDeferredFt8TxPlan=false;w.m_txRunning=true;w.m_currentTxIsTextMode=true;
   w.m_txFinishedNaturally=true;w.m_returnToRxAfterTx=false;w.handleTxStopped();
   check(w.m_ftSession.cqRepeatRemaining==2-call,"natural-finish CQ accounting wrong");
   check(call<2?w.m_ft8PendingTxArmed&&!w.m_pendingFt8TxPlan.message.isEmpty():!w.m_ft8PendingTxArmed,
         "CQ natural finish lost next arm or armed after exhaustion");
  }
  w.m_ft8OperatorStopLatched=true;w.stopFt8Sequencer("natural CQ test complete");
  // CQ preference survives normal mode visibility (Evil Mode locked).
  {QSignalBlocker block(w.m_chkFt8CqRepeat);w.m_chkFt8CqRepeat->setChecked(true);}
  w.m_settings.ft8CqRepeat=true;w.m_ft8EvilModeUnlocked=false;w.updateFt8EvilModeVisibility();check(w.m_settings.ft8CqRepeat,"Evil Mode erased CQ preference");
  {QSignalBlocker block(w.m_chkFt8CqRepeat);w.m_chkFt8CqRepeat->setChecked(false);}
  w.m_settings.ft8CqRepeat=false;
  // Populate callers while a real QSO context owns the sequencer.
  w.m_ftSession.startQso("LZ2KSQ","KN23",1000);w.m_ft8OperatorStopLatched=true;
  {QSignalBlocker block(w.m_chkFt8CallerQueue);w.m_chkFt8CallerQueue->setChecked(true);}
  w.clearFt8CallerQueue();
  Ft8RxDecoder::Decode a,b;a.message="IZ6NNH G0JEI IO91";a.snrDb=-10;a.frequencyHz=1000;b=a;b.message="IZ6NNH F4ABC JN18";b.frequencyHz=1300;
  w.considerFt8CallerForQueue(a,false);w.considerFt8CallerForQueue(b,false);a.snrDb=-5;w.considerFt8CallerForQueue(a,false);
  check(w.m_ft8CallerQueue.size()==2,"caller duplicate inserted");
  w.showFt8CallerQueue();check(w.m_ft8CallerTable->rowCount()==2,"caller list not populated");
  check(w.m_ft8CallerTable->item(0,1)->text()=="G0JEI"&&w.m_ft8CallerTable->item(0,3)->text()=="-5","FIFO order or fresh SNR lost");
  if(argc>1)w.m_ft8CallerDialog->grab().save(QString(argv[1])+"-callers-"+mode+".png");
  w.m_ft8CallerTable->selectRow(0);w.findChild<QPushButton*>("ftCallerQueueRemove")->click();
  check(w.m_ft8CallerQueue.size()==1&&w.m_ft8CallerQueue.first().call=="F4ABC","selected caller removal wrong");
  w.findChild<QPushButton*>("ftCallerQueueClear")->click();check(w.m_ft8CallerTable->rowCount()==0,"caller clear did not refresh list");
  check(w.m_ftSession.qsoActive&&w.m_ftSession.dxCall=="LZ2KSQ"&&!w.m_ft8PendingTxArmed,"queue view changed active QSO/TX");
  w.m_ft8CallerDialog->hide();w.m_ftSession.resetAll();
  std::cout<<mode.toStdString()<<" period role colours, CQ repeat lifecycle/dedup/live counts, visible caller FIFO PASS\n";
 }
 w.m_txRunning=false;w.m_ftTxWorkerRunning=false;w.m_hasDeferredFt8TxPlan=false;w.m_ft8OperatorStopLatched=true;w.stopFt8Sequencer("probe complete");w.close();
}
