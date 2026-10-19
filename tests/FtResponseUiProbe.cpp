#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "callsign/QrzLookupPanel.h"
#include <QApplication>
#include <QTextBrowser>
#include <QLineEdit>
#include <QComboBox>
#include <QSignalBlocker>
#include <QSettings>
#include <iostream>
#include <stdexcept>
void require(bool value,const char*message){if(!value)throw std::runtime_error(message);}
int main(int argc,char**argv){
 QApplication app(argc,argv);app.setApplicationName("MadModem");
 QSettings settings(AppSettings::settingsFilePath(),QSettings::IniFormat);settings.setValue("Updates/checkAtStartup",false);settings.sync();
 MainWindow w;
 w.m_settings.textMyCallsign="IZ6NNH";w.m_settings.textMyLocator="JN63";w.m_settings.ft8NoResponseRetryCount=1;
 for(QString mode:{QString("FT8"),QString("FT4")}) {
  w.ui->cmbMode->setCurrentText(mode);w.m_settings.textMyCallsign="IZ6NNH";w.m_settings.textMyLocator="JN63";
  w.m_ft8OperatorStopLatched=false;
  int period=mode=="FT8"?15000:7500;
  qint64 boundary=QDateTime::currentDateTimeUtc().toMSecsSinceEpoch()/period*period;
  w.m_ftSession.clearQso();w.m_ftSession.clearLastTx();w.m_ftSession.startQso("LZ2KSQ","KN23",1033);w.m_ftSession.state=FtQsoSequencer::State::WaitingFinal73;
  w.m_ftSession.reportSent="R-15";w.m_ftSession.reportReceived="-02";
  w.m_ftSession.beginRetry("LZ2KSQ IZ6NNH R-15","SEQ",1);
  w.m_ftSession.lastTxPlan.message=w.m_ftSession.retryMessage;w.m_ftSession.lastTxPlan.slotBoundaryUtcMs=boundary;
  w.scheduleFt8RetryIfNeeded();
  require(w.m_ftSession.qsoActive && w.m_ftSession.retryRemaining==0,"last TX erased QSO");
  require(!w.m_ft8PendingTxArmed,"last attempt armed an extra transmission");
  auto epoch=w.m_ftSession.retryGeneration;auto replySlot=w.m_ftSession.retryReplySlotUtcMs;
  Ft8RxDecoder::Decode reply;reply.message="IZ6NNH LZ2KSQ RR73";reply.frequencyHz=1033;reply.snrDb=-15;reply.slotStartUtcMs=replySlot;reply.slotPeriodMs=period;
  w.processFt8SequencerDecode(reply);
  require(w.m_ft8PendingTxArmed && w.m_pendingFt8TxPlan.message=="LZ2KSQ IZ6NNH 73","late RR73 did not arm final 73");
  w.finishFt8ResponseWait(epoch,replySlot);
  require(w.m_ftSession.qsoActive && w.m_ft8PendingTxArmed,"obsolete timeout cancelled final 73");
  w.stopFt8Sequencer("test reset");
  w.m_ftSession.clearLastTx();w.m_ftSession.startQso("LZ2KSQ","KN23",1033);w.m_ftSession.state=FtQsoSequencer::State::WaitingFinal73;
  w.m_ftSession.beginRetry("LZ2KSQ IZ6NNH R-15","SEQ",1);w.m_ftSession.lastTxPlan.message=w.m_ftSession.retryMessage;w.m_ftSession.lastTxPlan.slotBoundaryUtcMs=boundary;
  w.scheduleFt8RetryIfNeeded();epoch=w.m_ftSession.retryGeneration;replySlot=w.m_ftSession.retryReplySlotUtcMs;
  w.m_rxRunning=true;w.finishFt8ResponseWait(epoch,replySlot);w.m_rxRunning=false;
  require(!w.m_ftSession.qsoActive,"genuine no-response did not stop after listening");
  // Reproduce the blocked DX-field signal in the screenshot; use cached data
  // so no QRZ account, credentials or internet are involved in this test.
  for(QString call:{QString("LZ2KSQ"),QString("G0JEI")}) {
   QrzResponse response;response.valid=true;response.fields.insert("call",call);response.fields.insert("fname",call=="G0JEI"?"New station":"Old station");
   w.m_qrzLookupPanel->m_cache.insert(call,{response,QDateTime::currentDateTimeUtc()});
  }
  {QSignalBlocker block(w.m_editFt8DxCall);w.m_editFt8DxCall->setText("LZ2KSQ");}w.updateFt8SequencerUi();
  require(w.m_qrzLookupPanel->m_call=="LZ2KSQ","initial QRZ selection missing");
  w.m_ft8OperatorStopLatched=false;
  Ft8RxDecoder::Decode report;report.message="IZ6NNH G0JEI -03";report.frequencyHz=778;report.snrDb=-21;report.slotPeriodMs=period;
  w.processFt8SequencerDecode(report);
  require(w.m_qrzLookupPanel->m_call=="G0JEI","QRZ remained on previous correspondent");
  require(w.m_qrzLookupPanel->m_details->toPlainText().contains("G0JEI"),"QRZ text did not follow active DX");
  require(w.m_pendingFt8TxPlan.message=="G0JEI IZ6NNH R-21","report response was not armed");
  w.stopFt8Sequencer("test complete");
  std::cout<<mode.toStdString()<<" GUI: last-reply/final-73/stale-timeout/no-response/QRZ PASS\n";
 }
 w.m_ft8OperatorStopLatched=true;
}
