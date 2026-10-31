// Exercise operator takeover and the persistent Evil CQ queue without a radio.
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "audio/AudioEngine.h"
#include "audio/FtTxWorker.h"
#include "modems/ft8/FtSlotScheduler.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QRadioButton>
#include <QTableWidget>
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
 MainWindow w;w.m_settings.pttMethod="none";w.m_audioEngine->m_running.store(true);w.m_rxRunning=true;
 // Suppress physical starts. We inspect the real UI's armed/replacement plans.
 QObject::disconnect(w.m_ftSlotScheduler,nullptr,&w,nullptr);
 for(const QString mode:{QString("FT8"),QString("FT4")}){
  w.ui->cmbMode->setCurrentText(mode);w.m_settings.textMyCallsign="IZ6NNH";w.m_settings.textMyLocator="JN63";
  w.m_ft8OperatorStopLatched=true;w.stopFt8Sequencer("operator queue test setup");w.m_ft8OperatorStopLatched=false;
  w.m_ft8EvilModeUnlocked=true;w.m_settings.ftAutoQsoDuplicatePolicy="none";
  {QSignalBlocker b(w.m_chkFt8FullAutoQso);w.m_chkFt8FullAutoQso->setChecked(true);}
  {QSignalBlocker b(w.m_chkFt8CallerQueue);w.m_chkFt8CallerQueue->setChecked(false);}
  const auto profile=Ft8Mode::profileForMode(mode);const qint64 boundary=QDateTime::currentMSecsSinceEpoch()/profile.slotMs*profile.slotMs;
  w.m_ft8LastAttemptedSlotBoundaryUtcMs=boundary+60000;
  w.m_ftSession.startQso("LZ2KSQ","KN23",1033);w.m_ftSession.state=FtQsoSequencer::State::WaitingReport;
  auto cq=[&](QString call,QString grid,int snr,bool workedGrid,qint64 slot){
   Ft8RxDecoder::Decode d;d.message="CQ "+call+" "+grid;d.snrDb=snr;d.frequencyHz=1200;d.slotStartUtcMs=slot;d.slotPeriodMs=profile.slotMs;
   d.utc=QDateTime::fromMSecsSinceEpoch(slot,Qt::UTC).toString("HHmmss");
   QVariantMap status{{"worked",false},{"gridWorkedAny",workedGrid},{"gridWorkedBand",workedGrid},{"gridWorkedMode",workedGrid},
    {"countryWorkedAny",true},{"countryWorkedBand",true},{"countryWorkedMode",true},{"generation",QVariant::fromValue<qulonglong>(w.m_ftLogbookCacheGeneration)}};
   auto c=w.buildFt8FullAutoCqCandidate(d,status);
   const auto q=w.makeFtLogbookQuery(c.call,c.band,c.mode,c.dxcc,c.grid,qBound(1,w.m_settings.ftAutoQsoRecentHours,168),0);
   status.insert("cacheKey",q.value("cacheKey"));w.m_ftLogbookStatusCache.insert(q.value("cacheKey").toString(),status);
   return qMakePair(d,status);
  };
  auto a=cq("M0ABC","IO91",5,true,boundary-profile.slotMs);
  auto b=cq("JA1ABC","PM95",-20,false,boundary-profile.slotMs);
  require(w.queueFt8FullAutoCqCandidate(a.first,a.second),"CQ received during a QSO was discarded");
  // A logbook lookup that finishes while busy must also enqueue its CQ.
  const QString key=b.second.value("cacheKey").toString();w.m_ftPendingAutoQsoByKey[key].append(b.first);
  w.handleFtLogbookLookupReady(0,QString(),QVariantList{b.second});
  w.processFt8FullAutoCqCandidates();
  require(w.m_ftSession.dxCall=="LZ2KSQ"&&w.m_ft8FullAutoCqCandidates.size()==2,"queued CQ interrupted the active QSO or was lost");
  require(w.m_ft8FullAutoCqCandidates.first().call=="JA1ABC","new-grid priority lost to stronger SNR");
  w.stopFt8Sequencer("simulate exhausted attempts");
  require(w.m_ft8FullAutoCqCandidates.size()==2,"no-response cleanup discarded the waiting CQ queue");
  w.processFt8FullAutoCqCandidates();
  require(w.m_ftSession.dxCall=="JA1ABC"&&w.m_ft8PendingTxArmed,"idle Evil did not start the highest-priority queued CQ");
  require(w.m_ft8FullAutoCqCandidates.size()==1&&w.m_ft8FullAutoCqCandidates.first().call=="M0ABC","dispatch discarded other queued targets");
  w.stopFt8Sequencer("queue next target test");
  Ft8RxDecoder::Decode busy=a.first;busy.message="DL1ABC M0ABC -10";
  w.observeFt8AutoCqActivity(busy);
  require(!w.queueFt8FullAutoCqCandidate(a.first,a.second),"same-slot stale CQ overrode directed traffic");
  w.processFt8FullAutoCqCandidates();require(!w.m_ftSession.qsoActive,"busy station was called from an old CQ");
  a=cq("M0ABC","IO91",-23,true,boundary);
  require(w.queueFt8FullAutoCqCandidate(a.first,a.second),"fresh weaker CQ did not refresh queued target");
  w.processFt8FullAutoCqCandidates();require(w.m_ftSession.dxCall=="M0ABC"&&w.m_ft8PendingTxArmed,"fresh CQ did not release waiting target");
  w.m_ft8OperatorStopLatched=true;w.stopFt8Sequencer("STOP test");
  require(w.m_ft8FullAutoCqCandidates.isEmpty()&&!w.queueFt8FullAutoCqCandidate(b.first,b.second),"STOP allowed Auto QSO queue to restart");
  w.m_ft8OperatorStopLatched=false;
  // The automatic duplicate-slot guard must not suppress explicit operator TX.
  {QSignalBlocker x(w.m_radioFt8TxFirst),y(w.m_radioFt8TxSecond);const bool first=(boundary%(2*profile.slotMs))==0;w.m_radioFt8TxFirst->setChecked(first);w.m_radioFt8TxSecond->setChecked(!first);}
  require(w.selectedFt8TxSlotBoundaryUtcMs(true)<w.selectedFt8TxSlotBoundaryUtcMs(false),"operator takeover inherited the automatic attempted-period lockout");
  // Change correspondent by the actual double-click path, with Manage CQ queue on.
  w.m_ftSession.startQso("LZ2KSQ","KN23",1033);w.m_ftSession.state=FtQsoSequencer::State::SendingLocator;
  w.m_pendingFt8TxPlan=FtTxPlan();w.m_pendingFt8TxPlan.message="LZ2KSQ IZ6NNH JN63";w.m_pendingFt8TxPlan.slotBoundaryUtcMs=boundary;
  w.m_txRunning=true;w.m_ftTxWorkerRunning=true;w.m_ftTxActiveRequest=901;w.m_returnToRxAfterTx=false;
  auto chosen=cq("K1ABC","FN42",-10,false,boundary/ (2*profile.slotMs)*(2*profile.slotMs)+profile.slotMs).first;
  w.handleFt8DecodeReady(chosen);
  QTableWidgetItem *item=nullptr;for(int row=0;row<w.m_tableFt8Rx->rowCount();++row){auto *x=w.m_tableFt8Rx->item(row,4);if(x&&x->data(Qt::UserRole+430).value<Ft8RxDecoder::Decode>().message==chosen.message)item=x;}
  require(item,"probe decode row missing");
  {QSignalBlocker block(w.m_chkFtCqQueue);w.m_chkFtCqQueue->setChecked(true);}
  w.handleFt8DecodeDoubleClicked(item);
  require(w.m_ftSession.dxCall=="K1ABC"&&w.m_hasDeferredFt8TxPlan&&w.m_deferredFt8TxPlan.operatorRequested,"double-click did not take ownership from active TX/queue");
  require(w.m_settings.ft8TxFirstPeriod,"exact second-period decode metadata was ignored");
  w.m_ftTxWorker->stopped(901);pump();
  require(w.m_ft8PendingTxArmed&&w.m_pendingFt8TxPlan.message.startsWith("K1ABC ")&&w.m_pendingFt8TxPlan.operatorRequested,"operator replacement lost its late-start permission after old TX stopped");
  require(w.pendingFt8AudioStartDeadlineUtcMs()-w.m_pendingFt8TxPlan.slotBoundaryUtcMs>profile.slotMs/2,"manual backend delay was restricted to the automatic full-frame cutoff");
  const auto token=w.m_ft8PendingTxToken;w.m_ftTxWorker->stopped(901);w.m_ftTxWorker->finished(901);pump();
  require(w.m_ft8PendingTxToken==token&&w.m_ft8PendingTxArmed,"late completion of old TX damaged the manual replacement");
  w.m_ft8OperatorStopLatched=true;w.stopFt8Sequencer("test complete");
  std::cout<<mode.toStdString()<<": busy/logbook CQ queue, priority, fresh/busy checks, STOP, manual takeover and old worker signals PASS\n";
 }
 w.m_audioEngine->m_running.store(false);w.m_rxRunning=false;
}
