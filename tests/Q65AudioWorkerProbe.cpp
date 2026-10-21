// SPDX-License-Identifier: GPL-3.0-or-later
#include "modems/q65/Q65Decoder.h"
#include "modems/q65/tx/Q65Transmitter.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>
#include <QJsonDocument>
#include <QJsonObject>
#include <iostream>
#include <stdexcept>
static void need(bool b,const char *s){if(!b)throw std::runtime_error(s);}
int main(int argc,char**argv){QCoreApplication app(argc,argv);
 for(auto mode:{Q65Mode::Submode::A,Q65Mode::Submode::B,Q65Mode::Submode::C,Q65Mode::Submode::D}){
  Q65Transmitter tx("CQ IZ6NNH JN61",12000,15,mode,1373);need(tx.generationSucceeded(),"TX failed");QVector<double> wave(180000);QVector<float> b(4096);int offset=3600;
  while(!tx.isFinished()){int n=tx.generate(b.data(),b.size());for(int i=0;i<n&&offset<wave.size();++i)wave[offset++]=b[i];}
  Q65NativeEngine::Configuration config;config.periodSeconds=15;config.submode=mode;config.rxFrequencyHz=1375;config.dfToleranceHz=50;config.singleDecode=true;config.apDecode=false;config.averaging=true;
  Q65NativeEngine baseline;Q65Decoder rx;rx.setPeriodSeconds(15);rx.setSubmode(mode);rx.setRxFrequencyHz(1375);rx.setDfToleranceHz(50);rx.setSingleDecode(true);rx.setApDecodeEnabled(false);
  QVector<Q65Decode> decoded;int jobs=0;QObject::connect(&rx,&Q65Decoder::decoded,[&](const auto&d){decoded.append(d);});QObject::connect(&rx,&Q65Decoder::averageStatusChanged,[&](int,int){++jobs;});
  double maxCollection=0;
  for(int repeat=0;repeat<2;++repeat){auto expected=baseline.decode(wave,100+repeat,config);need(!expected.isEmpty(),"baseline no message");const int before=jobs;decoded.clear();rx.m_samples12k=wave;rx.m_currentPeriodId=100+repeat;rx.m_periodStartUtc=QDateTime::fromMSecsSinceEpoch((100+repeat)*15000LL,Qt::UTC);
   QElapsedTimer submit;submit.start();rx.tryPeriodDecode(true);const double submitMs=submit.nsecsElapsed()/1e6;
   // Begin next-period collection exactly as the owner does at UTC rollover.
   const qint64 captureNs=(101+repeat)*15000000000LL;
   rx.beginUtcPeriod(101+repeat,captureNs);rx.m_resampler.reset();rx.m_nextOutputUtcNs=captureNs;rx.m_lastInputEndUtcNs=0;rx.m_captureGeneration=1;
   // Collect the following period while the real native engine runs.
   for(int n=0;n<100;++n){AudioBlock block;block.sampleRate=48000;block.captureGeneration=1;block.firstSampleIndex=n*1024;block.firstSampleUtcNs=captureNs+n*1024LL*1000000000LL/48000;block.samples=QVector<float>(1024);QElapsedTimer t;t.start();rx.processAudioBlock(block);maxCollection=qMax(maxCollection,t.nsecsElapsed()/1e6);}
   QElapsedTimer wait;wait.start();while(jobs==before&&wait.elapsed()<30000){app.processEvents();QThread::msleep(1);}
   need(jobs>before,"async result timeout");need(decoded.size()==expected.size(),"async result count differs");
   for(int i=0;i<decoded.size();++i){need(decoded[i].message==expected[i].message&&decoded[i].frequencyHz==expected[i].frequencyHz&&decoded[i].dtSeconds==expected[i].dtSeconds&&decoded[i].averageCount==expected[i].averageCount,"async result/averaging differs");need(decoded[i].utc==QDateTime::fromMSecsSinceEpoch((100+repeat)*15000LL,Qt::UTC).addMSecs(qRound64(expected[i].dtSeconds*1000)),"period timestamp lost");}
   std::cout<<QJsonDocument(QJsonObject{{"mode",Q65Mode::modeName(mode)},{"repeat",repeat},{"submit_ms",submitMs},{"max_collection_ms",maxCollection},{"messages",decoded.size()},{"exact_results",true}}).toJson(QJsonDocument::Compact).constData()<<std::endl;
  }
 }
}
