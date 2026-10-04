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
int main(int argc,char **argv){
 QApplication app(argc,argv);MainWindow w;w.m_ntpClient->setEnabled(false);
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
 return ok?0:1;
}
