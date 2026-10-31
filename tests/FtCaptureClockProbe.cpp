// SPDX-License-Identifier: GPL-3.0-or-later
// Native pipeline probe; built with the same private-access flags as FT probes.
#include "audio/AudioEngine.h"
#include "modems/ft8/Ft8RxDecoder.h"
#include "dsp/common/DspConditioner.h"
#include <QCoreApplication>
#include <QFile>
#include <cstring>
#include <iostream>
#include <stdexcept>
static void check(bool b,const char *why){if(!b)throw std::runtime_error(why);}
int main(int argc,char**argv){QCoreApplication app(argc,argv);if(argc!=3)return 2;
 constexpr qint64 utc=1792309800000000000LL;
 for(bool ft4:{false,true})for(double ppm:{-3000.,-1378.94,0.,1378.94,3000.}){
  QFile file(argv[ft4?2:1]);check(file.open(QIODevice::ReadOnly),"fixture missing");const auto bytes=file.readAll();
  QVector<double> fixture(bytes.size()/8);std::memcpy(fixture.data(),bytes.constData(),bytes.size());
  const double actual=48000*(1+ppm/1e6);AudioEngine engine;engine.m_sampleRate=48000;engine.m_captureGeneration=1;
  engine.resetDiagnosticCounters();qint64 read=0,emitted=0;
  while(read/actual<600.){
   read+=819;const qint64 mono=std::llround((read/actual+.002)*1e9);
   engine.m_captureTracker.observe(read,mono,utc+mono,48000);
   while(read-emitted>=1024){engine.m_captureTracker.take(1024);emitted+=1024;}
  }
  check(engine.m_captureTracker.locked(),"capture estimator not locked");engine.m_totalSamples=emitted;
  const double period=ft4?7.5:15.;const double slot=std::ceil((read/actual+.1)/period)*period;
  const qint64 slotUtc=utc+std::llround(slot*1e9);const int count=ft4?90000:180000;
  QVector<double> recovered(count,0.);QVector<unsigned char> occupied(count,0);
  Ft8RxDecoder rx;rx.setModeName(ft4?"FT4":"FT8");
  DspConditioner conditioner;DspConditioner::Config config;config.profile=DspConditioner::Profile::FtWeakSignal;
  config.humNotchEnabled=false;config.impulseBlankerEnabled=false;config.noiseReductionEnabled=false;config.agcEnabled=false;
  conditioner.setConfig(config);qint64 previousEnd=0,nextInput=emitted,outputAnchor=0,outputSamples=0;int blocks=0;
  QObject::connect(&engine,&AudioEngine::audioBlockReady,[&](const AudioBlock &raw){
   check(raw.firstSampleIndex==nextInput,"PCM input index gap");nextInput+=raw.samples.size();
   check(raw.captureSpanNs>0&&raw.enqueuedSteadyNs>0,"capture metadata missing");
   if(previousEnd)check(previousEnd==raw.firstSampleUtcNs,"capture timestamp discontinuity");
   previousEnd=raw.firstSampleUtcNs+raw.captureSpanNs;
   const auto block=conditioner.processBlock(raw);
   check(block.captureSpanNs==raw.captureSpanNs && block.enqueuedSteadyNs==raw.enqueuedSteadyNs,"conditioner lost timing metadata");
   qint64 first=0;const auto out=rx.resampleTo12k(block,&first);
   if(!out.isEmpty()){
    if(!outputAnchor)outputAnchor=first;
    check(std::abs(first-(outputAnchor+std::llround(outputSamples*1e9/12000.)))<=1,"native FT output grid gap");
    outputSamples+=out.size();const qint64 offset=std::llround((first-slotUtc)*12000./1e9);
    for(int k=0;k<out.size();++k)if(offset+k>=0&&offset+k<count){
     check(!occupied[int(offset+k)],"FT output overlap");occupied[int(offset+k)]=1;recovered[int(offset+k)]=out[k];
    }
   }
   ++blocks;
  });
  auto appendPcm=[&](qint64 from,qint64 to){
   for(qint64 frame=from;frame<to;++frame){const double position=(frame/actual-slot)*12000.;double value=0;
    if(position>=0&&position+1<fixture.size()){const int index=int(position);const double f=position-index;value=fixture[index]+f*(fixture[index+1]-fixture[index]);}
    const qint16 pcm=qint16(std::llround(qBound(-1.,value,1.)*32767));engine.m_pendingBytes.append(reinterpret_cast<const char*>(&pcm),2);
   }
  };
  appendPcm(emitted,read);
  while(read/actual<slot+period+.1){const qint64 from=read;read+=819;appendPcm(from,read);
   const qint64 mono=std::llround((read/actual+.002)*1e9);engine.m_captureTracker.observe(read,mono,utc+mono,48000);engine.processPendingBytes();
  }
  check(std::all_of(occupied.begin(),occupied.end(),[](auto b){return b==1;}),"complete FT UTC slot not collected");
  Ft8RxDecoder::Candidate candidate;candidate.baseHz=1033;candidate.startSec=ft4?.336:.5; // ordinary FT4 candidate grid, after shaping lead
  Ft8RxDecoder::Decode decoded;const auto slotTime=QDateTime::fromMSecsSinceEpoch(slotUtc/1000000,Qt::UTC);
  const bool ok=ft4?rx.decodeFt4Candidate(recovered,slotTime,candidate,decoded):rx.decodeCandidate(recovered,slotTime,candidate,decoded,nullptr,nullptr,nullptr,false,false);
  std::cout<<(ft4?"FT4":"FT8")<<" ppm="<<ppm<<" blocks="<<blocks<<" rate="<<engine.m_captureTracker.measuredRate()<<" decoded="<<ok<<" message="<<decoded.message.toStdString()<<std::endl;
  check(ok&&decoded.message=="IZ6NNH LZ2KSQ RR73","native clock-corrected CRC/message failed");
 }
 std::cout<<"Native PCM/conditioner/FT resampler/CRC message clock integration PASS\n";
}
