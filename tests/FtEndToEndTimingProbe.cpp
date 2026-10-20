// SPDX-License-Identifier: GPL-3.0-or-later
// Diagnostic probe linked with application objects and -fno-access-control.
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "modems/ft8/FtSlotScheduler.h"
#include "modems/ft8/tx/Ft8Transmitter.h"
#include <QApplication>
#include <QSettings>
#include <QTableWidget>
#include <QRadioButton>
#include <QCheckBox>
#include <QSpinBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTimer>
#include <random>
#include <iostream>
#include <stdexcept>
static void need(bool v,const char *why){if(!v)throw std::runtime_error(why);}
int main(int argc,char **argv){
 QApplication app(argc,argv);app.setApplicationName("MadModem");
 QSettings settings(AppSettings::settingsFilePath(),QSettings::IniFormat);
 settings.setValue("Updates/checkAtStartup",false);settings.sync();
 MainWindow w;w.show();app.processEvents();
 // Exercise real UI, sequencer and waveform preparation; never key a radio
 // or open an output device in this diagnostic environment.
 QObject::disconnect(w.m_ftSlotScheduler,nullptr,&w,nullptr);
 auto reset=[&](const QString &mode){
  w.stopFt8Sequencer("timing probe reset");w.ui->cmbMode->setCurrentText(mode);
  w.m_settings.textMyCallsign="IZ6NNH";w.m_settings.textMyLocator="JN63";
  w.m_ft8OperatorStopLatched=false;w.m_ftSession.clearQso();w.m_ftSession.clearLastTx();
  w.m_ftSession.startQso("LZ2KSQ","KN23",1033);
  w.m_ftSession.state=FtQsoSequencer::State::WaitingFinal73;
  w.m_ftSession.reportSent="R-15";w.m_ftSession.reportReceived="-02";
  w.m_lastCompletedFt8Call.clear();w.m_tableFt8Rx->setRowCount(0);
  const int period=mode=="FT8"?15000:7500;
  const bool first=(QDateTime::currentMSecsSinceEpoch()/(period))%2==0;
  w.m_radioFt8TxFirst->setChecked(!first);w.m_radioFt8TxSecond->setChecked(first);
 };
 auto awaitPrepared=[&](QElapsedTimer &clock){
  QEventLoop loop;QTimer tick;tick.setInterval(1);double ready=-1;
  QObject::connect(&tick,&QTimer::timeout,&loop,[&]{
   if(w.m_pendingFt8PreparedModulator){ready=clock.nsecsElapsed()/1e6;loop.quit();}
  });tick.start();QTimer::singleShot(3000,&loop,&QEventLoop::quit);loop.exec();
  need(ready>=0,"prepared waveform did not arrive");return ready;
 };
 for(QString mode:{QString("FT8"),QString("FT4")}){
  const int period=mode=="FT8"?15000:7500;
  for(int dense:{0,50,100})for(int repeat=0;repeat<3;++repeat){
   reset(mode);const qint64 slot=QDateTime::currentMSecsSinceEpoch()/period*period;
   Ft8RxDecoder::Decode d;d.slotStartUtcMs=slot;d.slotPeriodMs=period;d.utc="120000";d.snrDb=-15;
   QElapsedTimer clock;clock.start();
   // Queue the complete batch through the GUI event loop, as decoder signals do.
   double dispatch=-1,batch=-1;QEventLoop queued;
   QTimer::singleShot(0,&w,[&]{dispatch=clock.nsecsElapsed()/1e6;
    w.handleFt8DecodeBatchStarted(slot,"boundary");
    for(int i=0;i<dense;++i){d.message="CQ G0JEI IO93";d.frequencyHz=300+i*20;w.handleFt8DecodeReady(d);}
    d.message="IZ6NNH LZ2KSQ RR73";d.frequencyHz=1033;w.handleFt8DecodeReady(d);
    w.handleFt8DecodeBatchFinished(slot,"boundary");batch=clock.nsecsElapsed()/1e6;queued.quit();
   });queued.exec();
   need(w.m_ft8PendingTxArmed&&w.m_pendingFt8TxPlan.message=="LZ2KSQ IZ6NNH 73","reply was not armed");
   w.m_tableFt8Rx->viewport()->repaint();const double painted=clock.nsecsElapsed()/1e6;
   const double prepared=awaitPrepared(clock);
   std::cout<<QJsonDocument(QJsonObject{{"kind","dense-ui"},{"mode",mode},{"rows",dense+1},{"repeat",repeat},{"queue_ms",dispatch},{"armed_ms",batch},{"paint_ms",painted},{"prepared_ms",prepared}}).toJson(QJsonDocument::Compact).constData()<<std::endl;
  }
  for(QString phase:{QString("wsjtx-gate"),QString("boundary")}){
   reset(mode);Ft8RxDecoder rx;rx.setModeName(mode);rx.setMyCall("IZ6NNH");rx.setDxCall("LZ2KSQ");rx.setRxMarkerHz(1033);rx.setQsoDeadlineActive(true);
   QVector<double> samples(period*12);std::mt19937 rng(771);std::normal_distribution<double> noise(0,.002);for(auto &v:samples)v=noise(rng);
   int k=0;for(QString message:{QString("IZ6NNH LZ2KSQ RR73"),QString("CQ G0JEI IO93"),QString("CQ LA8ENA JO48"),QString("CQ F4JXL IN94"),QString("CQ DL2LAU JO31"),QString("CQ EA5GVJ IM97")}){
    Ft8Transmitter tx(mode,message,48000,k==0?1033:300+k*375);QVector<float> pcm(period*48);int n=tx.generate(pcm.data(),pcm.size());int offset=mode=="FT8"?6000:3600;
    for(int i=0;i<n/4&&i+offset<samples.size();++i)samples[i+offset]+=pcm[i*4]*(k==0?.06:.12);++k;
   }
   if(phase=="wsjtx-gate")samples.resize(mode=="FT8"?162000:73200);
   QElapsedTimer clock;QEventLoop loop;Ft8RxDecoder::PerfStats stats;bool done=false;double arm=-1,ready=-1;int rows=0;
   QObject::connect(&rx,&Ft8RxDecoder::decodeBatchStarted,&w,&MainWindow::handleFt8DecodeBatchStarted,Qt::QueuedConnection);
   QObject::connect(&rx,&Ft8RxDecoder::decodeReady,&w,[&](const Ft8RxDecoder::Decode &d){++rows;w.handleFt8DecodeReady(d);},Qt::QueuedConnection);
   QObject::connect(&rx,&Ft8RxDecoder::decodeBatchFinished,&w,[&](qint64 slot,const QString &p){w.handleFt8DecodeBatchFinished(slot,p);if(arm<0&&w.m_ft8PendingTxArmed)arm=clock.nsecsElapsed()/1e6;},Qt::QueuedConnection);
   QObject::connect(&rx,&Ft8RxDecoder::performanceUpdated,&loop,[&](const Ft8RxDecoder::PerfStats &s){stats=s;done=true;},Qt::QueuedConnection);
   QTimer tick;tick.setInterval(1);QObject::connect(&tick,&QTimer::timeout,&loop,[&]{if(w.m_pendingFt8PreparedModulator&&ready<0)ready=clock.nsecsElapsed()/1e6;if(done&&ready>=0)loop.quit();});
   tick.start();QTimer::singleShot(10000,&loop,&QEventLoop::quit);clock.start();
   rx.startAsyncDecodeSlot(samples,QDateTime::fromMSecsSinceEpoch(QDateTime::currentMSecsSinceEpoch()/period*period,Qt::UTC),phase);loop.exec();
   std::cout<<QJsonDocument(QJsonObject{{"kind","rx-to-prepared"},{"mode",mode},{"phase",phase},{"done",done},{"rows",rows},{"decoder_ms",stats.totalMs},{"armed_ms",arm},{"prepared_ms",ready},{"post_decode_ms",ready-stats.totalMs}}).toJson(QJsonDocument::Compact).constData()<<std::endl;
   need(done,"RX decoding did not complete");
  }
 }
 w.stopFt8Sequencer("timing probe complete");w.close();app.processEvents();
}
