// Replays the failed audio-start lifecycle seen in the R31 Windows session.
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "audio/FtTxWorker.h"
#include "audio/AudioEngine.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QElapsedTimer>
#include <QSignalBlocker>
#include <QThread>
#include <QSettings>
#include <iostream>
#include <stdexcept>
static void require(bool ok,const char *why){if(!ok){std::cerr<<"FAIL: "<<why<<std::endl;throw std::runtime_error(why);}}
static void pump(){QElapsedTimer t;t.start();while(t.elapsed()<80){QCoreApplication::processEvents();QThread::msleep(1);}}
int main(int argc,char **argv){
 QApplication app(argc,argv);app.setApplicationName("MadModem");
 QSettings settings(AppSettings::settingsFilePath(),QSettings::IniFormat);settings.setValue("Updates/checkAtStartup",false);settings.sync();
 MainWindow w;w.m_settings.pttMethod="none";
 // Model an already active RX capture; no test may open a physical device.
 w.m_audioEngine->m_running.store(true);
 for(const QString mode:{QString("FT8"),QString("FT4")})for(bool evil:{false,true}){
  w.ui->cmbMode->setCurrentText(mode);w.m_settings.textMyCallsign="IZ6NNH";w.m_settings.textMyLocator="JN63";
  w.stopFt8Sequencer("test setup");w.m_ft8OperatorStopLatched=false;w.m_ft8TxStartFailureCount=0;w.m_ft8EvilModeUnlocked=evil;
  {QSignalBlocker b(w.m_chkFt8FullAutoQso);w.m_chkFt8FullAutoQso->setChecked(evil);}
  const auto profile=Ft8Mode::profileForMode(mode);const auto boundary=QDateTime::currentMSecsSinceEpoch()/profile.slotMs*profile.slotMs;
  w.m_ftSession.startQso("LZ2KSQ","KN23",1033);w.m_ftSession.state=FtQsoSequencer::State::SendingLocator;
  w.m_ftSession.beginRetry("LZ2KSQ IZ6NNH JN63","SEQ",3);
  {QSignalBlocker b(w.m_cmbFt8TxStrategy);w.m_cmbFt8TxStrategy->setCurrentIndex(w.m_cmbFt8TxStrategy->findData("follow_rx"));}
  w.m_settings.ft8TxFrequencyStrategy="follow_rx";
  w.m_spinFt8TxFreq->setValue(1033);
  w.m_pendingFt8TxPlan.message="LZ2KSQ IZ6NNH JN63";w.m_pendingFt8TxPlan.tag="SEQ";w.m_pendingFt8TxPlan.audioFrequencyHz=1033;
  w.m_pendingFt8TxPlan.slotBoundaryUtcMs=QDateTime::currentMSecsSinceEpoch()-700;
  w.m_pendingFt8TxPlan.audioTargetDelayMs=500;w.m_ft8PendingTxArmed=true;w.m_ft8PendingTxToken="same-armed";
  w.scheduleFt8SequencerMessage("LZ2KSQ IZ6NNH JN63","RETRY");
  require(w.m_ft8PendingTxToken=="same-armed","duplicate message restarted a still-valid current-slot arm");
  w.m_ft8PendingTxArmed=false;w.m_ft8PendingTxToken.clear();
  w.m_ftSession.retryRemaining=2; // One earlier completed frame has already used an attempt.
  for(int failure=1;failure<=3;++failure){
   w.m_pendingFt8TxPlan=FtTxPlan();w.m_pendingFt8TxPlan.message="LZ2KSQ IZ6NNH JN63";w.m_pendingFt8TxPlan.tag="RETRY";
   w.m_pendingFt8TxPlan.slotBoundaryUtcMs=boundary;w.m_pendingFt8TxPlan.audioTargetDelayMs=500;
   // No physical PTT or audio. Keep every replacement well in the future.
   w.m_ft8LastAttemptedSlotBoundaryUtcMs=QDateTime::currentMSecsSinceEpoch()+60000;
   w.m_txRunning=true;w.m_ftTxWorkerRunning=true;w.m_ftTxActiveRequest=41+failure;w.m_returnToRxAfterTx=false;w.m_rxRunning=true;
   const auto oldRequest=w.m_ftTxActiveRequest;
   w.handleTxError("TX audio cancelled before PCM pull: authorization revoked or start deadline expired.");
   w.m_ftTxWorker->stopped(oldRequest);
   pump();
   if(failure<3){
    require(w.m_ftSession.qsoActive&&w.m_ft8PendingTxArmed,"failed first PCM left QSO active without a replacement transmission");
    require(w.m_ftSession.retryRemaining==2,"untransmitted frame consumed or reset QSO attempts");
    require(w.m_ftSession.lastTxPlan.message.isEmpty(),"failed audio was recorded as transmitted");
    require(w.m_ft8TxStartFailureCount==failure,"start-failure budget was reset by automatic rearm");
   }else{
    require(!w.m_ftSession.qsoActive&&!w.m_ft8PendingTxArmed&&w.m_ft8OperatorStopLatched,"repeated backend failures looped indefinitely");
    require(!w.m_chkFt8FullAutoQso->isChecked(),"Evil automation survived the backend failure limit");
   }
  }
  w.m_ft8OperatorStopLatched=false;w.m_ft8TxStartFailureCount=0;
  w.m_ftSession.startQso("LZ2KSQ","KN23",1033);w.m_ftSession.state=FtQsoSequencer::State::SendingRReport;
  w.m_pendingFt8TxPlan.message="LZ2KSQ IZ6NNH R-10";w.m_pendingFt8TxPlan.tag="SEQ";w.m_pendingFt8TxPlan.slotBoundaryUtcMs=boundary;
  w.m_hasDeferredFt8TxPlan=true;w.m_deferredFt8TxPlan.message="LZ2KSQ IZ6NNH 73";w.m_deferredFt8TxPlan.tag="SEQ";
  w.m_ftSession.deferredState=FtQsoSequencer::State::SendingRr73;w.m_returnToRxAfterTx=false;
  w.handleTxError("test backend failure while final reply is pending");pump();
  require(w.m_ft8PendingTxArmed&&w.m_pendingFt8TxPlan.message=="LZ2KSQ IZ6NNH 73","audio failure discarded a newer final reply");
  require(w.m_ftSession.state==FtQsoSequencer::State::SendingRr73,"audio failure restored an obsolete exchange state");
  w.stopFt8Sequencer("deferred reply tested");
  // STOP between an error and the queued replacement must remain authoritative.
  w.m_ft8OperatorStopLatched=false;w.m_ft8TxStartFailureCount=0;w.m_pendingFt8TxPlan.message="LZ2KSQ IZ6NNH 73";w.m_pendingFt8TxPlan.tag="SEQ";w.m_pendingFt8TxPlan.slotBoundaryUtcMs=boundary;
  w.deferPendingFtTx("test backend delay");w.stopFt8Shell();pump();require(!w.m_ft8PendingTxArmed,"STOP was undone by failed-start recovery");
  std::cout<<mode.toStdString()<<" "<<(evil?"Evil":"normal")<<": failed PCM retry, stale stop, attempt count, bounded failure and STOP PASS\n";
 }
 w.m_audioEngine->m_running.store(false);w.m_rxRunning=false;w.m_ft8OperatorStopLatched=true;
}
