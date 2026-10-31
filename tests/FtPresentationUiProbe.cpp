// SPDX-License-Identifier: GPL-3.0-or-later
// Compile with the application objects and -fno-access-control, like FtResponseUiProbe.
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QApplication>
#include <QSettings>
#include <QCheckBox>
#include <QSpinBox>
#include <QTableWidget>
#include <iostream>
#include <stdexcept>
static void require(bool value,const char*message){if(!value)throw std::runtime_error(message);}
int main(int argc,char**argv){
 QApplication app(argc,argv);app.setApplicationName("MadModem");QSettings settings(AppSettings::settingsFilePath(),QSettings::IniFormat);settings.setValue("Updates/checkAtStartup",false);settings.sync();MainWindow w;
 for(QString mode:{QString("FT8"),QString("FT4")}){
  w.ui->cmbMode->setCurrentText(mode);w.m_settings.textMyCallsign="IZ6NNH";w.m_settings.textMyLocator="JN63";w.stopFt8Sequencer("probe reset");w.m_ft8OperatorStopLatched=false;
  const int period=mode=="FT8"?15000:7500;const qint64 slot=QDateTime::currentDateTimeUtc().toMSecsSinceEpoch()/period*period;
  auto publish=[&](const Ft8RxDecoder::Decode&d,const QString&phase){w.handleFt8DecodeBatchStarted(slot,phase);w.handleFt8DecodeReady(d);w.handleFt8DecodeBatchFinished(slot,phase);};
  Ft8RxDecoder::Decode d;d.message="IZ6NNH LZ2KSQ -03";d.frequencyHz=1033;d.snrDb=-15;d.slotStartUtcMs=slot;d.slotPeriodMs=period;d.utc="12:00:00";
  const int before=w.m_tableFt8Rx->rowCount();const bool hadSnr=w.m_ftSession.haveLastSnr;const int snr=w.m_ftSession.lastSnrDb;
  publish(d,"boundary presentation");require(w.m_tableFt8Rx->rowCount()==before+1,"preview was not displayed");require(!w.m_ft8PendingTxArmed&&!w.m_ftSession.qsoActive,"preview started a QSO");require(w.m_ftSession.haveLastSnr==hadSnr&&w.m_ftSession.lastSnrDb==snr,"preview changed TX report context");require(w.m_ft8PendingSequencerBatch.isEmpty(),"preview queued a sequencer action");
  publish(d,"boundary");require(w.m_tableFt8Rx->rowCount()==before+1,"canonical result duplicated preview");require(w.m_ft8PendingTxArmed&&w.m_pendingFt8TxPlan.message=="LZ2KSQ IZ6NNH R-15","canonical report did not arm response");
  // A previously displayed non-direct result must still reach QRM handling.
  w.m_spinFt8TxFreq->setValue(1033);d.message="G0JEI LZ2KSQ -03";d.frequencyHz=1033;publish(d,"boundary presentation");require(w.m_ftSession.qsoActive,"preview ended QSO");publish(d,"boundary");require(!w.m_ftSession.qsoActive,"canonical QRM result was lost to visual deduplication");
  // Idle Evil/Auto CQ must not select a caller from partial display batches.
  w.m_ft8OperatorStopLatched=false;w.m_ft8EvilModeUnlocked=true;w.m_chkFt8FullAutoQso->setChecked(true);d.message="CQ G0JEI IO93";publish(d,"boundary presentation");require(!w.m_ft8PendingTxArmed&&!w.m_ftSession.qsoActive&&w.m_ft8FullAutoCqCandidates.isEmpty(),"preview triggered Auto CQ");w.m_chkFt8FullAutoQso->setChecked(false);w.stopFt8Sequencer("probe complete");
  // Dense slots must also deduplicate early rows outside the last 80.
  w.m_tableFt8Rx->setRowCount(0);w.m_ft8OperatorStopLatched=true;
  for(const QString phase:{QString("boundary presentation"),QString("boundary")}) {
   w.handleFt8DecodeBatchStarted(slot,phase);
   for(int i=0;i<95;++i){d.message="CQ G0JEI IO93";d.frequencyHz=300+i*20;w.handleFt8DecodeReady(d);}
   w.handleFt8DecodeBatchFinished(slot,phase);
   require(w.m_tableFt8Rx->rowCount()==95,"dense canonical batch duplicated preview rows");
  }
  std::cout<<mode.toStdString()<<" preview display/no-TX/canonical reply/QRM/AutoCQ/95-row dedup PASS\n";
 }
 w.m_ft8OperatorStopLatched=true;
}
