// SPDX-License-Identifier: GPL-3.0-or-later
// Link against app objects with -fno-access-control. No audio hardware required.
#include "runtime/RxDecoderWorker.h"
#include <QCoreApplication>
#include "modems/sstv/tx/SstvTransmitter.h"
#include "modems/weatherfax/tx/WeatherFaxTransmitter.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QElapsedTimer>
#include <random>
#include <thread>
#include <iostream>
#include <stdexcept>
int main(int argc,char **argv){
 QCoreApplication app(argc,argv);
 const int rate=argc>1?QString(argv[1]).toInt():48000;
 QStringList cases={"CW-native","CW-neural","RTTY","RTTY-skimmer","PSK31","MFSK16","MFSK32","FeldHell","FSK105","SSTV","WEFAX","MSK144","Radio Telescope"};
 for(const auto &name:cases){
  if(argc>2 && name!=QString(argv[2]))continue;
  auto *cw=new CwDecoder;auto *multi=new RttyMultiDecoder;auto *hell=new HellschreiberDecoder;
  auto *fax=new WeatherFaxDecoder;auto *mfsk=new MfskDecoder;auto *msk=new Msk144Decoder;
  auto *worker=new RxDecoderWorker({fax,new SstvDecoder,new RttyDecoder,multi,new Bpsk31Decoder,mfsk,cw,hell,msk});
  QThread thread;worker->moveToThread(&thread);QObject::connect(&thread,&QThread::finished,worker,&QObject::deleteLater);thread.start();
  RxDecoderWorker::Config c;c.enabled=true;c.filter.enabled=true;c.filter.blackHz=1000;c.filter.whiteHz=1170;
  if(name.startsWith("CW"))c.mode=CwDecoder::modeName();else if(name.startsWith("RTTY"))c.mode=RttyDecoder::modeName();else if(name=="PSK31")c.mode=Bpsk31Decoder::modeName();else if(name.startsWith("MFSK"))c.mode=MfskDecoder::modeName();else if(name=="SSTV")c.mode=SstvDecoder::modeName();else if(name=="WEFAX")c.mode=WeatherFaxDecoder::modeName();else if(name=="MSK144")c.mode=Msk144Decoder::modeName();else if(name=="Radio Telescope")c.mode=name;else c.mode=HellschreiberDecoder::modeName();
  c.multi=name=="RTTY-skimmer";c.afc=name=="RTTY"||name=="PSK31"||name.contains("Hell")||name=="FSK105";c.afcRange=20;c.hellFsk105=name=="FSK105";
  QMetaObject::invokeMethod(worker,[&]{worker->configure(c);cw->setSecondaryEnabled(true);cw->setBandSkimmerEnabled(true);cw->setDecodeEngine(name=="CW-neural"?"neural_search":"native");multi->configure(45.45,170,false,c.multi,16);
   if(c.multi){multi->m_inputSampleRate=rate;for(int n=0;n<16;++n)multi->addOrRefreshTrack({300+n*190,470+n*190,10.0},0);}
   mfsk->setVariant(name=="MFSK32"?MfskDecoder::Variant::Mfsk32:MfskDecoder::Variant::Mfsk16);
   fax->setAutoStartEnabled(false);msk->setDecodeDepth(3);if(name=="FSK105")hell->setVariant(HellschreiberDecoder::Variant::Fsk105);},Qt::BlockingQueuedConnection);
  QImage picture(320,256,QImage::Format_RGB32);picture.fill(Qt::gray);
  std::unique_ptr<TxModulator> tx;
  if(name=="SSTV")tx=std::make_unique<SstvTransmitter>(picture,"Martin M1",rate);
  if(name=="WEFAX")tx=std::make_unique<WeatherFaxTransmitter>(picture,rate,120,1500,2300);
  if(name=="Radio Telescope")worker->radioPowerWindow().begin(1792519200000000000LL,1792519201000000000LL,300,3000);
  const int count=name=="MSK144"&&rate==48000?800:150;
  std::mt19937 gen(177);std::normal_distribution<float> noise(0,.02);auto *queue=worker->queue();
  // 150 x1024 @48k = 3.2 seconds. Real-rate feed, GUI events deliberately idle.
  auto start=std::chrono::steady_clock::now();
  for(int n=0;n<count;++n){AudioBlock b;b.sampleRate=rate;b.captureGeneration=1;b.captureSequence=n+1;b.firstSampleIndex=n*1024LL;b.firstSampleUtcNs=1792519200000000000LL+b.firstSampleIndex*1000000000LL/rate;b.samples.resize(1024);
   for(int i=0;i<1024;++i){const auto t=(b.firstSampleIndex+i)/double(rate);b.samples[i]=noise(gen)+.05f*std::sin(6.283185307179586*1000*t)+.05f*std::sin(6.283185307179586*1170*t);}
   if(tx)tx->generate(b.samples.data(),b.samples.size());
   queue->enqueue(b);std::this_thread::sleep_until(start+std::chrono::nanoseconds((n+1)*1024LL*1000000000LL/rate));
  }
  QElapsedTimer wait;wait.start();while(queue->pendingBlockCount()&&wait.elapsed()<3000)QThread::msleep(1);
  QMetaObject::invokeMethod(worker,[]{},Qt::BlockingQueuedConnection);
  const auto stats=queue->statistics();const bool bandOk=!name.startsWith("CW")||cw->m_bandSkimmer.overloads()==0;
  const bool ok=bandOk&&stats.enqueued==quint64(count)&&stats.taken==quint64(count)&&stats.dropped==0&&stats.cleared==0&&stats.pending==0;
  std::cout<<QJsonDocument(QJsonObject{{"mode",name},{"sample_rate",rate},{"blocks",int(stats.enqueued)},{"taken",int(stats.taken)},{"dropped",int(stats.dropped)},{"cleared",int(stats.cleared)},{"peak_pending",stats.peakPending},{"cw_band_overloads",int(cw->m_bandSkimmer.overloads())},{"gui_events_processed",false},{"pass",ok}}).toJson(QJsonDocument::Compact).constData()<<std::endl;
  bool radioOk=true;
  if(name=="Radio Telescope"){auto power=worker->radioPowerWindow().snapshot();radioOk=power.ready&&!power.gap&&power.blocks>0;
   std::cerr<<"RADIO ready="<<power.ready<<" gap="<<power.gap<<" blocks="<<power.blocks<<" sum="<<power.sum<<" mode="<<worker->m_config.mode.toStdString()<<std::endl;}

  thread.quit();thread.wait();if(!ok||!radioOk)return 2;
 }
}
