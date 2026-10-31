// SPDX-License-Identifier: GPL-3.0-or-later
// App-linked probe with -fno-access-control: no sound card or Qt audio backend.
#include "audio/AudioEngine.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstring>
#include <iostream>
#include <stdexcept>
int main(int argc,char **argv){QCoreApplication app(argc,argv);
 for(int rate:{44100,48000,96000})for(int channels:{1,2}){
  AudioEngine engine;engine.m_channelCount=channels;engine.m_sampleRate.store(rate);engine.m_inputVolumePercent.store(100);engine.m_streamTimestampValid=true;engine.m_streamFirstUtcNs=1792519200000000000LL;engine.m_captureGeneration=7;engine.resetDiagnosticCounters();
  int received=0;bool valid=true;QObject::connect(&engine,&AudioEngine::audioBlockReady,[&](const AudioBlock &b){
   valid &= b.samples.size()==1024&&b.firstSampleIndex==received*1024LL&&b.captureSequence==quint64(received+1)&&b.captureGeneration==7&&b.sampleRate==rate;
   for(int i=0;i<b.samples.size();++i){const qint16 sample=qint16(((received*1024+i)*137)%60000-30000);valid &= b.samples[i]==sample/32768.f;}
   ++received;
  });
  const int total=1024*10;QByteArray bytes;bytes.resize(total*channels*2);
  for(int i=0;i<total;++i)for(int ch=0;ch<channels;++ch){const qint16 sample=qint16((i*137+ch*777)%60000-30000);std::memcpy(bytes.data()+(i*channels+ch)*2,&sample,2);}
  const int cutoff=(1024*9+17)*channels*2;int pos=0,chunk=0;
  while(pos<cutoff){const int sizes[]={1,2047,53,5129};int n=qMin(sizes[chunk++%4],cutoff-pos);engine.m_pendingBytes+=bytes.mid(pos,n);pos+=n;engine.processPendingBytes();}
  valid &= received==9&&engine.m_pendingBytes.size()==17*channels*2;
  engine.m_pendingBytes+=bytes.mid(cutoff);engine.processPendingBytes();valid &=received==10&&engine.m_pendingBytes.isEmpty();
  std::cout<<QJsonDocument(QJsonObject{{"sample_rate",rate},{"channels",channels},{"blocks",received},{"exact_pcm_and_sequence",valid}}).toJson(QJsonDocument::Compact).constData()<<std::endl;
  if(!valid)return 2;
 }
}
