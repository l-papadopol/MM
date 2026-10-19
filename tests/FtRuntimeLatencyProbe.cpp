#include "modems/ft8/Ft8RxDecoder.h"
#include "modems/ft8/tx/Ft8Transmitter.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTimer>
#include <QSet>
#include <QFile>
#include <QTextStream>
#include <random>
#include <iostream>
int main(int argc,char**argv) {
 QCoreApplication app(argc,argv);
 Ft8RxDecoder rx;
 if (argc>1 && QString(argv[1])=="cancel") {
  std::mt19937 rng(426);std::normal_distribution<double> noise(0,0.1);
  QCryptographicHash hash(QCryptographicHash::Sha256);
  for (QString mode:{QString("FT8"),QString("FT4")}) {
   QVector<double> source(mode=="FT8"?180000:90000);for(auto &x:source)x=noise(rng);
   Ft8RxDecoder::Decode decode;decode.message="IZ6NNH LZ2KSQ RR73";
   std::array<int,103> tones{};for(int i=0;i<103;++i)tones[i]=i%4;
   double elapsed=0;
   for(int rep=0;rep<43;++rep) {
    QVector<double> samples=source;
    Ft8RxDecoder::Candidate c;c.baseHz=1033;c.startSec=(rep%3==0)?-.15:((rep%3==1)?.5:3.5);
    QElapsedTimer timer;timer.start();
    if(mode=="FT8")rx.subtractDecodedSignal(samples,c,decode);else rx.subtractFt4DecodedSignal(samples,c,tones);
    if(rep>=3)elapsed+=timer.nsecsElapsed()/1e6;
    hash.addData(reinterpret_cast<const char*>(samples.constData()),samples.size()*sizeof(double));
   }
   std::cout<<mode.toStdString()<<" subtraction mean_ms="<<elapsed/40<<'\n';
  }
  std::cout<<"CANCEL_SHA256 "<<hash.result().toHex().constData()<<'\n';return 0;
 }
 for(QString mode:{QString("FT8"),QString("FT4")}) {
  rx.setModeName(mode);rx.setMyCall("IZ6NNH");rx.setDxCall("LZ2KSQ");rx.setRxMarkerHz(1033);rx.setQsoDeadlineActive(true);
  const int period=mode=="FT8"?15000:7500;
  QVector<double> samples(period*12);std::mt19937 rng(771);std::normal_distribution<double> noise(0,.002);
  for(auto &x:samples)x=noise(rng);
  int k=0;
  for(QString message:{QString("IZ6NNH LZ2KSQ RR73"),QString("CQ G0JEI IO93"),QString("CQ LA8ENA JO48"),QString("CQ F4JXL IN94"),QString("CQ DL2LAU JO31"),QString("CQ EA5GVJ IM97")}) {
   double hz=k==0?1033:300+k*375;
   Ft8Transmitter tx(mode,message,48000,hz);QVector<float>wave(period*48);int n=tx.generate(wave.data(),wave.size());
   const int offset=mode=="FT8"?6000:3600;
   for(int i=0;i<n/4 && i+offset<samples.size();++i)samples[i+offset]+=wave[i*4]*(k==0?.06:.12);
   ++k;
  }
  int count=0;Ft8RxDecoder::PerfStats stats;stats.modeName=mode;stats.phase="boundary";stats.offline=false;
  QElapsedTimer timer;timer.start();double first=-1;QVector<Ft8RxDecoder::Decode> rows;
  if(argc>1 && QString(argv[1])=="async") {
   QEventLoop loop;QSet<QString> unique;int finalBatches=0;bool contextChecked=false;double contextMs=-1;
   auto a=QObject::connect(&rx,&Ft8RxDecoder::decodeReady,&loop,[&](const Ft8RxDecoder::Decode &d){rows.append(d);unique.insert(d.message);
#ifndef BASELINE
    if(!contextChecked && rx.m_decodeJobActive.load()) {
     QElapsedTimer context;context.start();rx.setRxMarkerHz(1040);rx.setDxCall("DL1ABC");rx.setRxMarkerHz(1033);rx.setDxCall("LZ2KSQ");rx.setDriftRxOptions(true,3.0);rx.setDriftRxOptions(false,2.0);contextMs=context.nsecsElapsed()/1e6;contextChecked=true;
     if(contextMs>40)throw std::runtime_error("QSO context blocked capture-owner thread");
    }
#endif
    if(d.message.contains("LZ2KSQ") && first<0)first=timer.nsecsElapsed()/1e6;},Qt::QueuedConnection);
   auto b=QObject::connect(&rx,&Ft8RxDecoder::decodeBatchFinished,&loop,[&](qint64,const QString &phase){if(phase=="boundary")++finalBatches;},Qt::QueuedConnection);
   auto c=QObject::connect(&rx,&Ft8RxDecoder::performanceUpdated,&loop,[&](const Ft8RxDecoder::PerfStats &s){stats=s;loop.quit();},Qt::QueuedConnection);
   QTimer::singleShot(15000,&loop,&QEventLoop::quit);
   rx.startAsyncDecodeSlot(samples,QDateTime::fromMSecsSinceEpoch(1792309800000LL,Qt::UTC),"boundary");loop.exec();
   QObject::disconnect(a);QObject::disconnect(b);QObject::disconnect(c);
   if(rows.size()!=6 || unique.size()!=6 || finalBatches!=1){for(const auto &d:rows)std::cerr<<"ASYNC row "<<d.message.toStdString()<<'\n';std::cerr<<"ASYNC failure rows/unique/final="<<rows.size()<<'/'<<unique.size()<<'/'<<finalBatches<<'\n';return 1;}
#ifndef BASELINE
   // performanceUpdated may reach the owner before the coordinator posts its
   // completion callback. Wait for that callback, rather than assuming one
   // processEvents() call also waits for future events.
   QEventLoop settled; QTimer poll; poll.setInterval(1);
   QObject::connect(&poll,&QTimer::timeout,&settled,[&]{
    if(!rx.m_decodeJobActive.load() && !rx.m_hasPendingDxCall && rx.m_pendingRxMarkerHz<0)settled.quit();
   });
   QTimer::singleShot(1000,&settled,&QEventLoop::quit);poll.start();settled.exec();
#else
   QCoreApplication::processEvents(QEventLoop::AllEvents,50);
#endif
#ifndef BASELINE
   if(!contextChecked || rx.m_hasPendingDxCall || rx.m_pendingRxMarkerHz>=0 || rx.m_pendingDriftOptions || rx.m_driftRxEnabled || rx.m_dxCall!="LZ2KSQ" || rx.m_rxMarkerHz!=1033)throw std::runtime_error("pending QSO context did not coalesce/apply");
#endif
   std::cout<<"ASYNC "<<mode.toStdString()<<" six unique results and one final boundary batch PASS; context updates "<<contextMs<<" ms\n";
  } else {
#ifdef BASELINE
  rows=rx.decodeSlot(samples,QDateTime::fromMSecsSinceEpoch(1792309800000LL,Qt::UTC),&count,&stats);
#else
  rows=rx.decodeSlot(samples,QDateTime::fromMSecsSinceEpoch(1792309800000LL,Qt::UTC),&count,&stats,[&](const QVector<Ft8RxDecoder::Decode>&batch){
   std::cerr<<mode.toStdString()<<" progress at "<<timer.nsecsElapsed()/1e6<<" ms:";
   for(const auto&d:batch){std::cerr<<" ["<<d.message.toStdString()<<"]";if(d.message.contains("LZ2KSQ") && first<0)first=timer.nsecsElapsed()/1e6;}std::cerr<<'\n';
  });
#endif
  }
  if(first<0)for(const auto&d:rows)if(d.message.contains("LZ2KSQ"))first=timer.nsecsElapsed()/1e6;
  QStringList messages;for(const auto&d:rows)messages.append(d.message);messages.sort();
  std::cout<<mode.toStdString()<<" QSO_ms="<<first<<" total_ms="<<stats.totalMs<<" decodes="<<rows.size()<<" subtraction_ms="<<stats.subtractionMs<<" messages="<<messages.join(";").toStdString()<<'\n';
 }
}
