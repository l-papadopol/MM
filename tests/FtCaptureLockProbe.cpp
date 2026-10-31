// SPDX-License-Identifier: GPL-3.0-or-later
// Link with app objects and -fno-access-control. No radio/audio hardware.
#include "modems/ft8/Ft8RxDecoder.h"
#include "audio/BoundedAudioDispatcher.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>
#include <QJsonDocument>
#include <QJsonObject>
#include <atomic>
#include <thread>
#include <future>
#include <iostream>
int main(int argc,char**argv){
 QCoreApplication app(argc,argv);const bool blocking=qEnvironmentVariableIsSet("EXPECT_BLOCKING");
 for(QString mode:{QString("FT8"),QString("FT4")}){
  Ft8RxDecoder rx;rx.setModeName(mode);rx.setLiveInputEnabled(true);rx.m_incrementalEnabled=false;
  const int period=mode=="FT4"?7500:15000;
  const auto base=QDateTime::currentMSecsSinceEpoch()/period*period;
  auto block=[&](int index){AudioBlock b;b.sampleRate=48000;b.samples.fill(.05f,1024);b.captureGeneration=1;b.captureSequence=index+1;b.firstSampleIndex=index*1024LL;b.firstSampleUtcNs=base*1000000LL+index*1024LL*1000000000LL/48000;return b;};
  rx.processAudioBlock(block(0));BoundedAudioDispatcher queue(64);std::promise<void> entered;std::atomic<bool> produced{false};
  std::thread holder([&]{std::lock_guard<std::recursive_mutex> lock(rx.m_decodeConfigMutex);entered.set_value();QThread::msleep(750);});entered.get_future().wait();
  std::thread feeder([&]{for(int i=1;i<=100;++i){queue.enqueue(block(i));QThread::msleep(5);}produced=true;});
  int dropped=0,accepted=0;double longest=0;QElapsedTimer elapsed;elapsed.start();
  while(!produced||queue.pendingBlockCount()>0){int lost=0;const auto ready=queue.takePending(64,&lost);dropped+=lost;for(const auto &b:ready){QElapsedTimer call;call.start();rx.processAudioBlock(b);longest=qMax(longest,call.nsecsElapsed()/1e6);++accepted;}QThread::msleep(1);}
  feeder.join();holder.join();
  std::cout<<QJsonDocument(QJsonObject{{"mode",mode},{"accepted",accepted},{"dropped",dropped},{"sequence_gaps",double(rx.m_audioSequenceGaps)},{"capture_call_max_ms",longest},{"collection_ms",double(elapsed.elapsed())},{"expected_blocking",blocking}}).toJson(QJsonDocument::Compact).constData()<<std::endl;
  if(blocking){if(dropped==0||longest<500)return 2;}
  else if(dropped!=0||accepted!=100||rx.m_audioSequenceGaps!=0||longest>100)return 3;
 }
}
