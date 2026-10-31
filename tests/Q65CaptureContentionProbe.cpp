// SPDX-License-Identifier: GPL-3.0-or-later
// App-linked -fno-access-control probe. Same native codec, slow FEC lock,
// former owner-thread placement versus production asynchronous placement.
#include "modems/q65/Q65Decoder.h"
#include "modems/q65/tx/Q65Transmitter.h"
#include "modems/weak_signal/WeakSignalCodecLock.h"
#include "audio/BoundedAudioDispatcher.h"
#include <QCoreApplication>
#include <QThread>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <thread>
#include <iostream>
int main(int argc,char **argv){QCoreApplication app(argc,argv);
 Q65Transmitter tx("CQ IZ6NNH JN61",12000,15,Q65Mode::Submode::A,1373);QVector<double> wave(180000);QVector<float> b(4096);int offset=3600;
 while(!tx.isFinished()){int n=tx.generate(b.data(),b.size());for(int i=0;i<n&&offset<wave.size();++i)wave[offset++]=b[i];}
 for(bool blocking:{true,false}){
  auto *rx=new Q65Decoder;auto *queue=new BoundedAudioDispatcher(64,rx);QThread owner;rx->moveToThread(&owner);QObject::connect(&owner,&QThread::finished,rx,&QObject::deleteLater);owner.start();double worst=0;
  QObject::connect(queue,&BoundedAudioDispatcher::blocksAvailable,rx,[&]{for(const auto&block:queue->takePending(64)){QElapsedTimer t;t.start();rx->processAudioBlock(block);worst=qMax(worst,t.nsecsElapsed()/1e6);}},Qt::QueuedConnection);
  QMetaObject::invokeMethod(rx,[&]{rx->setPeriodSeconds(15);rx->setRxFrequencyHz(1375);rx->setDfToleranceHz(50);rx->setSingleDecode(true);rx->setApDecodeEnabled(false);},Qt::BlockingQueuedConnection);
  std::atomic_bool locked{false};std::thread holder([&]{WeakSignalCodecLock::mutex().lock();locked=true;QThread::msleep(750);WeakSignalCodecLock::mutex().unlock();});while(!locked)QThread::msleep(1);
  QMetaObject::invokeMethod(rx,[&,blocking]{
   if(blocking){Q65NativeEngine engine;Q65NativeEngine::Configuration c;c.periodSeconds=15;c.rxFrequencyHz=1375;c.dfToleranceHz=50;c.singleDecode=true;c.apDecode=false;engine.decode(wave,100,c);}
   else{rx->m_samples12k=wave;rx->m_currentPeriodId=100;rx->m_periodStartUtc=QDateTime::fromMSecsSinceEpoch(1500000,Qt::UTC);rx->tryPeriodDecode(true);}
   rx->beginUtcPeriod(101,1515000000000LL);rx->m_nextOutputUtcNs=1515000000000LL;
  },Qt::QueuedConnection);
  for(int n=0;n<100;++n){AudioBlock block;block.sampleRate=48000;block.samples=QVector<float>(1024);block.firstSampleIndex=n*1024;block.captureSequence=n+1;block.captureGeneration=1;block.firstSampleUtcNs=1515000000000LL+n*1024LL*1000000000LL/48000;queue->enqueue(block);QThread::msleep(5);}
  holder.join();QMetaObject::invokeMethod(rx,[]{},Qt::BlockingQueuedConnection);while(queue->pendingBlockCount())QThread::msleep(1);QMetaObject::invokeMethod(rx,[]{},Qt::BlockingQueuedConnection);
  const auto stats=queue->statistics();bool ok=blocking?stats.dropped>0:stats.dropped==0&&stats.taken==100;
  std::cout<<QJsonDocument(QJsonObject{{"blocking_control",blocking},{"enqueued",int(stats.enqueued)},{"taken",int(stats.taken)},{"dropped",int(stats.dropped)},{"peak_pending",stats.peakPending},{"max_collection_ms",worst},{"pass",ok}}).toJson(QJsonDocument::Compact).constData()<<std::endl;
  owner.quit();owner.wait();if(!ok)return 2;
 }
}
