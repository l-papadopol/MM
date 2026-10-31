// SPDX-License-Identifier: GPL-3.0-or-later
#include "modems/ft8/Ft8RxDecoder.h"
#include "modems/ft8/tx/Ft8Transmitter.h"
#include <QCoreApplication>
#include <QFile>
#include <QTimer>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <iostream>
#include <stdexcept>
int main(int argc,char **argv){
 QCoreApplication app(argc,argv);
 for(int arg=1;arg<argc;++arg){
  const bool ft4=QString(argv[arg])=="FT4";const int period=ft4?7500:15000;QVector<double>s;
  if(ft4){s.fill(0,period*12);Ft8Transmitter tx("FT4","CQ G0JEI IO93",48000,1500);QVector<float> pcm(period*48);int n=tx.generate(pcm.data(),pcm.size());for(int i=0;i<n/4&&i+3600<s.size();++i)s[i+3600]=pcm[i*4]*.15;}
  else {QFile f(argv[arg]);if(!f.open(QIODevice::ReadOnly))return 2;auto bytes=f.readAll();s.resize(bytes.size()/8);std::memcpy(s.data(),bytes.constData(),bytes.size());}
  Ft8RxDecoder rx;rx.setModeName(ft4?"FT4":"FT8");rx.setLiveInputEnabled(true);
  const qint64 base=QDateTime::currentMSecsSinceEpoch()/period*period;
  QEventLoop loop;QTimer feed;feed.setTimerType(Qt::PreciseTimer);feed.setInterval(100);int offset=0,jobs=0,hits=0,rows=0;double maxFeed=0;bool complete=false;
  QObject::connect(&rx,&Ft8RxDecoder::decodeReady,&loop,[&](const Ft8RxDecoder::Decode&){++rows;});
  QObject::connect(&rx,&Ft8RxDecoder::performanceUpdated,&loop,[&](const Ft8RxDecoder::PerfStats&stats){
   ++jobs;hits+=stats.incrementalFftHits+stats.incrementalToneHits;
   std::cout<<QJsonDocument(QJsonObject{{"mode",ft4?"FT4":"FT8"},{"phase",stats.phase},{"total_ms",stats.totalMs},{"fft_hits",stats.incrementalFftHits},{"tone_hits",stats.incrementalToneHits},{"prepared_rows",stats.incrementalPreparedRows},{"preparation_ms",stats.incrementalPreparationMs},{"capture_call_max_ms",maxFeed},{"rows",rows}}).toJson(QJsonDocument::Compact).constData()<<std::endl;
   if(complete&&stats.phase=="boundary")loop.quit();
  });
  QObject::connect(&feed,&QTimer::timeout,&loop,[&]{
   QElapsedTimer elapsed;elapsed.start();
   if(offset<s.size()){const int n=qMin(1200,s.size()-offset);rx.appendTimedResampled(s.mid(offset,n),base*1000000LL+qint64(offset)*1000000000LL/12000);offset+=n;}
   else {complete=true;feed.stop();rx.finishCurrentSlot();}
   maxFeed=qMax(maxFeed,elapsed.nsecsElapsed()/1e6);
  });
  QTimer::singleShot(period+7000,&loop,&QEventLoop::quit);feed.start();loop.exec();
  if(!complete||jobs==0||hits==0)throw std::runtime_error("timestamped capture did not reuse incremental work");
 }
}
