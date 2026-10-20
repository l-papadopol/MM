// SPDX-License-Identifier: GPL-3.0-or-later
#include "modems/ft8/Ft8RxDecoder.h"
#include "modems/ft8/tx/Ft8Transmitter.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTimer>
#include <QSet>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <random>
#include <iostream>
int main(int argc,char**argv) {
 QCoreApplication app(argc,argv);bool active=argc>1 && QString(argv[1])=="active";
 for(QString mode:{QString("FT8"),QString("FT4")}) {
  Ft8RxDecoder rx;rx.setModeName(mode);rx.setMyCall("IZ6NNH");rx.setDxCall("LZ2KSQ");rx.setRxMarkerHz(1033);rx.setQsoDeadlineActive(active);
  int period=mode=="FT8"?15000:7500;
  QVector<double> samples(period*12);std::mt19937 rng(771);std::normal_distribution<double> noise(0,.002);for(auto &x:samples)x=noise(rng);
  int k=0;
  for(QString message:{QString("IZ6NNH LZ2KSQ RR73"),QString("CQ G0JEI IO93"),QString("CQ LA8ENA JO48"),QString("CQ F4JXL IN94"),QString("CQ DL2LAU JO31"),QString("CQ EA5GVJ IM97")}) {
   double hz=k==0?1033:300+k*375;Ft8Transmitter tx(mode,message,48000,hz);QVector<float>wave(period*48);int n=tx.generate(wave.data(),wave.size());int offset=mode=="FT8"?6000:3600;
   for(int i=0;i<n/4 && i+offset<samples.size();++i)samples[i+offset]+=wave[i*4]*(k==0?.06:.12);++k;
  }
  Ft8RxDecoder::PerfStats stats;QElapsedTimer timer;QEventLoop loop;QString phase;QSet<QString> canonical,previews;QVector<Ft8RxDecoder::Decode> rows;double first=-1,firstCanonical=-1,final=-1;int finals=0;bool done=false;
  QObject::connect(&rx,&Ft8RxDecoder::decodeBatchStarted,&loop,[&](qint64,const QString&p){phase=p;},Qt::QueuedConnection);
  QObject::connect(&rx,&Ft8RxDecoder::decodeReady,&loop,[&](const Ft8RxDecoder::Decode&d){double now=timer.nsecsElapsed()/1e6;if(first<0)first=now;if(phase.endsWith(" presentation"))previews.insert(d.message);else{if(firstCanonical<0)firstCanonical=now;canonical.insert(d.message);rows.append(d);}},Qt::QueuedConnection);
  QObject::connect(&rx,&Ft8RxDecoder::decodeBatchFinished,&loop,[&](qint64,const QString&p){if(p=="boundary"){++finals;final=timer.nsecsElapsed()/1e6;}},Qt::QueuedConnection);
  QObject::connect(&rx,&Ft8RxDecoder::performanceUpdated,&loop,[&](const Ft8RxDecoder::PerfStats&s){stats=s;done=true;loop.quit();},Qt::QueuedConnection);
  QTimer::singleShot(15000,&loop,&QEventLoop::quit);timer.start();rx.startAsyncDecodeSlot(samples,QDateTime::fromMSecsSinceEpoch(1792309800000LL,Qt::UTC),"boundary");loop.exec();
  const bool valid=done&&rows.size()==6&&canonical.size()==6&&finals==1&&(previews-canonical).isEmpty();if(!valid) {std::cerr<<"FAIL "<<mode.toStdString()<<" done="<<done<<" rows="<<rows.size()<<" unique="<<canonical.size()<<" finals="<<finals<<" previews="<<previews.size()<<std::endl;for(const auto&d:rows)std::cerr<<d.message.toStdString()<<std::endl;}
  std::sort(rows.begin(),rows.end(),[](const auto&a,const auto&b){return a.message<b.message;});QJsonArray decodes;for(const auto&d:rows)decodes.append(QJsonObject{{"message",d.message},{"freq",d.frequencyHz},{"snr",d.snrDb},{"dt",d.dt}});
  std::cout<<QJsonDocument(QJsonObject{{"mode",mode},{"valid",valid},{"budget_hit",stats.timeBudgetHit},{"stop_reason",stats.earlyStopReason},{"total_ms",stats.totalMs},{"passes",stats.passCount},{"active_qso",active},{"first_ms",first},{"first_canonical_ms",firstCanonical},{"final_ms",final},{"previews",previews.size()},{"decodes",decodes}}).toJson(QJsonDocument::Compact).constData()<<std::endl;
  if(!valid)return 1;
 }
}
