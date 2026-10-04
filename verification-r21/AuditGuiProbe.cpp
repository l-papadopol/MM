#include "mainwindow.h"
#include "logbook/AsyncLogbook.h"
#include "ui_mainwindow.h"
#include <QApplication>
#include <QDateTime>
#include <QComboBox>
#include <QTextStream>
#include <QTemporaryDir>
#include <QCheckBox>
#include <QLineEdit>
#include <QElapsedTimer>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <atomic>
int main(int argc,char **argv){
 QApplication app(argc,argv);
 qRegisterMetaType<AudioBlock>("AudioBlock");
 qRegisterMetaType<QVector<quint8>>("QVector<quint8>");
 qRegisterMetaType<QVector<QPointF>>("QVector<QPointF>");
 MainWindow w;w.m_ntpClient->setEnabled(false);
 bool ok=true;auto check=[&](bool value,const char *name){QTextStream(stdout)<<(value?"PASS ":"FAIL ")<<name<<'\n';ok &=value;};
 w.m_settings.pttMethod="none";w.m_settings.textMyCallsign="IZ6NNH";w.m_settings.textMyLocator="JN63";
 w.ui->cmbMode->setCurrentText("FT8");
 // A retry queued before STOP must not arm any new plan.
 w.m_ft8OperatorStopLatched=true;
 w.scheduleFt8SequencerMessage("EK1KE IZ6NNH JN63","RETRY");
 check(!w.m_ft8PendingTxArmed,"STOP blocks legacy scheduled retry at the common entry");
 w.stopFt8Shell();
 auto query=w.makeFtLogbookQuery("K1ABC","20m","FT8","291","FN31",1,0);
 QVariantMap cached;cached["generation"]=QVariant::fromValue<qulonglong>(w.m_ftLogbookCacheGeneration);
 cached["worked"]=true;cached["recentWorked"]=true;
 cached["latestCallUtc"]=QDateTime::currentDateTimeUtc().addSecs(-3602);
 const auto key=query["cacheKey"].toString();w.m_ftLogbookStatusCache[key]=cached;
 auto status=w.cachedFtLogbookStatus(query);
 check(status["worked"].toBool() && !status["recentWorked"].toBool(),"recent policy expires while static worked status stays cached");
 w.m_ftLogbookPendingKeys.insert(key);auto stale=cached;stale["cacheKey"]=key;
 stale["generation"]=QVariant::fromValue<qulonglong>(w.m_ftLogbookCacheGeneration-1);
 w.handleFtLogbookLookupReady(8,"FT_CACHE",{stale});
 check(w.m_ftLogbookPendingKeys.contains(key),"obsolete index reply cannot clear a new pending query");
 QTemporaryDir temp;w.m_logbookStore->reload(temp.filePath("audit.adi"), {});w.m_logbookStore->drain();
 LogbookEntry old;old.callsign="K1ABC";old.band="20m";old.mode="FT8";old.utc=QDateTime::currentDateTimeUtc().addSecs(-120);
 w.m_ftSession.startQso("EK1KE","LN20",1500);w.m_ftSession.autoLogDone=false;
 w.finishAutoLogFt8Qso(old,"probe",{});w.m_logbookStore->drain();
 check(w.m_logbook.containsCallsign("K1ABC") && !w.m_ftSession.autoLogDone,"old autolog saves completed QSO without marking new session logged");
 old.callsign="DL1XYZ";old.utc=old.utc.addSecs(-60);w.m_ftSession.autoLogDone=true;
 w.finishAutoLogFt8Qso(old,"probe",{});w.m_logbookStore->drain();
 check(w.m_logbook.containsCallsign("DL1XYZ"),"completed current session cannot discard an earlier pending QSO");
 // Simulate an old CAT transaction without touching a physical controller.
 w.m_catCommand->m_busy=true;w.m_txPreparationPending=true;w.m_ft8OperatorStopLatched=true;
 w.tuneFt8Shell();
 check(w.m_pendingFt8TxPlan.tune && !w.m_ft8OperatorStopLatched && !w.m_txPreparationPending,
       "Tune after STOP waits for busy CAT with fresh preparation state");
 w.m_catCommand->m_busy=false;w.stopFt8Shell();
 QElapsedTimer delay;delay.start();while(delay.elapsed()<70)app.processEvents();
 check(!w.m_pendingFt8TxPlan.tune && !w.m_txRunning && !w.m_ft8PendingTxArmed,
       "STOP invalidates Tune retry while CAT was busy");
 w.ui->cmbMode->setCurrentText(RttyDecoder::modeName());
 w.m_chkRttyContestMode->setChecked(true);
 w.m_rttyQsoForm->callsign->setText("K1ABC");w.m_rttyQsoForm->rstReceived->setText("579");
 QVariantMap candidate;candidate["dxCall"]="DL1XYZ";candidate["fields"]=QVariantMap{{"RST","599"}};
 w.applyTextAssistContestCandidate(candidate);
 check(w.m_rttyQsoForm->rstReceived->text()=="579","contest autofill cannot mix exchange from another correspondent");
 candidate["dxCall"]="K1ABC";w.applyTextAssistContestCandidate(candidate);
 check(w.m_rttyQsoForm->rstReceived->text()=="599","contest autofill still accepts the selected correspondent");

 auto pump = [&](const std::function<bool()> &done, int timeout=3000) {
   QElapsedTimer timer; timer.start();
   while (!done() && timer.elapsed()<timeout) { app.processEvents(); QThread::msleep(1); }
   return done();
 };
 // Occupy the capture thread without opening a physical audio device.
 std::atomic<bool> entered{false}, release{false};
 QMetaObject::invokeMethod(w.m_audioEngine, [&] {
   entered=true; while(!release) QThread::msleep(1);
 }, Qt::QueuedConnection);
 check(pump([&]{return entered.load();}), "capture worker paused for transport race test");
 w.m_settings.audioInputName="nonexistent-regression-device";
 w.m_rxRunning=false; w.m_txRunning=false; w.m_txPreparationPending=false;
 w.m_offlineAnalysisActive=false;
 check(w.requestAudioInputStart("nonexistent-regression-device",48000) && w.m_rxStartPending,
       "GUI records pending capture start without waiting for worker");
 w.toggleRxReady();
 check(!w.m_rxStartPending && w.m_rxStopPending,
       "RX transport Stop cancels an input that is still opening");
 release=true;
 check(pump([&]{return !w.m_rxStopPending;}) && !w.m_rxRunning,
       "capture stop acknowledgement settles GUI without a stale start");
 // An acknowledgement from an older request cannot turn RX back on.
 w.m_audioEngine->inputRequestFinished(w.m_audioInputRequest-1,true,true);
 app.processEvents();
 check(!w.m_rxRunning, "obsolete capture acknowledgement is ignored by GUI");
 entered=false; release=false;
 QMetaObject::invokeMethod(w.m_audioEngine, [&] {
   entered=true; while(!release) QThread::msleep(1);
 }, Qt::QueuedConnection);
 check(pump([&]{return entered.load();}), "capture worker paused before RTTY TX transition");
 w.m_txtRttyTx->setPlainText("CQ TEST IZ6NNH");
 w.m_rxRunning=true;
 w.startImageTx();
 check(w.m_txPreparationPending && !w.m_txRunning,
       "RTTY TX waits asynchronously for confirmed capture stop");
 w.m_returnToRxAfterTx=false; // Keep this test away from physical input devices.
 w.stopImageTx();
 release=true;
 check(pump([&]{return !w.m_rxStopPending;}) && !w.m_txRunning && !w.m_txPreparationPending,
       "Stop invalidates RTTY continuation before capture stop acknowledgement");
 // RX and TX acknowledgements may arrive in either order.
 w.m_pendingModeName="FT8"; w.m_pendingModeRestartRx=false;
 w.m_txRunning=true; w.m_ftTxWorkerRunning=true;
 w.handleAudioStopped();
 check(w.ui->cmbMode->currentText()==RttyDecoder::modeName() && !w.m_pendingModeName.isEmpty(),
       "early RX stop acknowledgement cannot change mode while FT TX worker is active");
 w.m_txRunning=false; w.m_ftTxWorkerRunning=false; w.finishPendingModeChange();
 check(w.ui->cmbMode->currentText()=="FT8" && w.m_pendingModeName.isEmpty(),
       "mode change completes after both audio directions have stopped");
 w.ui->cmbMode->setCurrentText(RttyDecoder::modeName());
 w.m_txPreparationPending=true;
 w.requestModeChange("FT8");
 check(!w.m_txPreparationPending && w.m_pendingModeName.isEmpty() &&
       w.ui->cmbMode->currentText()=="FT8",
       "mode change cancels preparation through common TX stop and completes");
 return ok?0:1;
}
