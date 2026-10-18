// SPDX-License-Identifier: GPL-3.0-or-later
// Same application route and settings for Native, R8 and candidate.
#include "modems/cw/CwDecoder.h"
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <vector>
#include <stdexcept>
#include <chrono>
#include <ctime>

int main(int argc,char** argv){
 QCoreApplication app(argc,argv);
 try{
  if(argc<5)throw std::runtime_error("usage: CwRouteProbe native|neural_search RATE TONE INPUT.f32 [CHUNK]");
  const std::string engine=argv[1];
  if(engine!="native"&&engine!="neural_search")throw std::runtime_error("invalid engine");
  const int rate=std::stoi(argv[2]);const std::size_t chunk=argc>5?std::stoul(argv[5]):997;
  if(rate<=0||!chunk)throw std::runtime_error("invalid rate/chunk");
  std::ifstream in(argv[4],std::ios::binary|std::ios::ate);
  if(!in)throw std::runtime_error("cannot open input");
  const auto bytes=in.tellg();if(bytes<0||bytes%4)throw std::runtime_error("invalid input size");
  std::vector<float> audio(static_cast<std::size_t>(bytes)/4);in.seekg(0);in.read(reinterpret_cast<char*>(audio.data()),bytes);
  if(!in)throw std::runtime_error("short input");
  CwDecoder decoder;decoder.setDecodeEngine(QString::fromStdString(engine));decoder.setToneHz(std::stod(argv[3]));
  decoder.setBandSkimmerEnabled(false);
  if(qEnvironmentVariableIsSet("CW_TRACE")) QObject::connect(&decoder,&CwDecoder::liveLogLine,[](int,const QString& s){std::cerr<<s.toStdString()<<'\n';});
  QString text;QJsonArray events;double audioTime=0,cpu=0,wall=0;
  QObject::connect(&decoder,&CwDecoder::priorityTextReceived,[&](int rank,const QString& s){
   if(rank==0){text+=s;events.append(QJsonObject{{"audio_seconds",audioTime},{"text",s}});}
  });
  // Exclude file I/O, construction and block packing. Include synchronous DSP,
  // scanner, selected-tone diagnostics, decoder and signal callback equally.
  for(std::size_t i=0;i<audio.size();i+=chunk){
   AudioBlock b;b.sampleRate=rate;b.firstSampleIndex=i;b.captureGeneration=1;
   const auto count=std::min(chunk,audio.size()-i);b.samples.reserve(int(count));
   for(std::size_t j=0;j<count;++j)b.samples.append(audio[i+j]);
   audioTime=double(i+count)/rate;
   const auto c0=std::clock();const auto t0=std::chrono::steady_clock::now();
   decoder.processAudioBlock(b);
   wall+=std::chrono::duration<double>(std::chrono::steady_clock::now()-t0).count();
   cpu+=double(std::clock()-c0)/CLOCKS_PER_SEC;
  }
  QJsonObject result{{"engine",QString::fromStdString(engine)},{"text",text},{"cpu_seconds",cpu},{"wall_seconds",wall},{"input_seconds",double(audio.size())/rate},{"chunk_samples",int(chunk)},{"events",events}};
  std::cout<<QJsonDocument(result).toJson(QJsonDocument::Compact).constData()<<'\n';return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
}
