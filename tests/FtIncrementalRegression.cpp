// SPDX-License-Identifier: GPL-3.0-or-later
// Link against the application objects with -fno-access-control; no radio I/O.
#include "modems/ft8/Ft8RxDecoder.h"
#include "modems/ft8/tx/Ft8Transmitter.h"
#include "runtime/FtDecodeWorkerPool.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QThread>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <random>
#include <iostream>
#include <stdexcept>
static void need(bool b,const char *s){if(!b)throw std::runtime_error(s);}
static std::shared_ptr<const MadModemFt::PreparedAnalysis> waitFor(Ft8RxDecoder &rx,const QVector<double>&s,qint64 slot,int expected){
 QElapsedTimer t;t.start();
 while(t.elapsed()<5000){auto p=rx.m_incrementalAnalysis->snapshot(s,slot);if(p&&int(p->rows.size())==expected)return p;QThread::msleep(1);}
 throw std::runtime_error("incremental producer timeout");
}
static QVector<double> synthetic(const QString &mode){
 const int period=mode=="FT4"?7500:15000;QVector<double> s(period*12);
 std::mt19937 gen(771);std::normal_distribution<double> n(0,.002);for(auto&v:s)v=n(gen);
 int k=0;for(QString msg:{QString("IZ6NNH LZ2KSQ RR73"),QString("CQ G0JEI IO93"),QString("CQ LA8ENA JO48"),QString("CQ F4JXL IN94"),QString("CQ DL2LAU JO31"),QString("CQ EA5GVJ IM97")}){
  Ft8Transmitter tx(mode,msg,48000,k==0?1033:300+k*375);QVector<float> pcm(period*48);const int count=tx.generate(pcm.data(),pcm.size());const int offset=mode=="FT4"?3600:6000;
  for(int i=0;i<count/4&&i+offset<s.size();++i)s[i+offset]+=pcm[i*4]*(k==0?.06:.12);++k;
 }return s;
}
static void compare(const QVector<Ft8RxDecoder::Candidate>&a,const QVector<Ft8RxDecoder::Candidate>&b){
 need(a.size()==b.size(),"candidate count changed");
 for(int i=0;i<a.size();++i){const auto&x=a[i];const auto&y=b[i];
  need(x.score==y.score&&x.rankScore==y.rankScore&&x.baseHz==y.baseHz&&x.startSec==y.startSec&&x.syncRatio==y.syncRatio&&x.syncNoisePower==y.syncNoisePower&&x.spectralScore==y.spectralScore&&x.bucketRescue==y.bucketRescue&&x.qsoPriority==y.qsoPriority&&x.refined==y.refined,"candidate numeric/ranking mismatch");
 }
}
int main(int argc,char **argv){
 QCoreApplication app(argc,argv);auto &resources=MadModemRuntime::SystemResourceManager::instance();resources.m_poolCapacity=1;resources.m_maxLiveWorkers=1;
 for(int arg=1;arg<argc;++arg){
  const QString name=argv[arg];const bool ft4=name=="FT4";QVector<double> s;
  if(ft4)s=synthetic("FT4");else{QFile f(name);need(f.open(QIODevice::ReadOnly),"input missing");auto bytes=f.readAll();s.resize(bytes.size()/8);std::memcpy(s.data(),bytes.constData(),bytes.size());}
  Ft8RxDecoder rx;rx.setModeName(ft4?"FT4":"FT8");rx.setRxMarkerHz(1033);
  MadModemFt::AnalysisConfig c;c.epoch=1;c.slotUtcMs=1792309800000LL;c.ft4=ft4;c.markerHz=1033;
  // Irregular capture chunks, including a boundary not aligned to an FFT hop.
  for(int offset=0;offset<s.size();){int n=qMin(997+(offset%409),s.size()-offset);need(rx.m_incrementalAnalysis->submit(c,offset,s.mid(offset,n)),"submit failed");offset+=n;}
  const int symbol=ft4?576:1920,hop=ft4?576:c.hop;
  auto prepared=waitFor(rx,s,c.slotUtcMs,(s.size()-symbol-1)/hop+1);
  const auto plain=ft4?rx.findFt4Candidates(s,0.35):rx.findCandidates(s);
  int hits=0;QElapsedTimer elapsed;elapsed.start();
  {MadModemFt::AnalysisReadScope scope(prepared,s);auto cached=ft4?rx.findFt4Candidates(s,0.35):rx.findCandidates(s);compare(plain,cached);hits=scope.context.fftHits+scope.context.toneHits;}
  need(hits>0,"prepared analysis not used");const double cachedMs=elapsed.nsecsElapsed()/1e6;
  if(ft4){auto a=rx.findFt4DeadlineCandidates(s,1033,180,120);MadModemFt::AnalysisReadScope scope(prepared,s);compare(a,rx.findFt4DeadlineCandidates(s,1033,180,120));need(scope.context.toneHits>0,"selected-QSO cache unused");}
  QVector<double> changed=s;changed[0]+=.01;
  need(!prepared->matches(changed,c.slotUtcMs),"mismatching audio accepted");
  need(!prepared->matches(s,c.slotUtcMs+15000),"wrong slot accepted");
  {MadModemFt::AnalysisReadScope scope(prepared,s);if(ft4)rx.findFt4Candidates(changed,0.35);else rx.findCandidates(changed);need(scope.context.fftHits+scope.context.toneHits==0,"SIC/residual reused original analysis");}
  // An old immutable snapshot survives invalidation/new slot; new submissions
  // cannot publish an old generation or expose samples beyond the decode input.
  const auto retained=prepared;rx.m_incrementalAnalysis->invalidate();need(!rx.m_incrementalAnalysis->snapshot(s,c.slotUtcMs),"invalidated snapshot visible");
  need(retained->matches(s,c.slotUtcMs),"deferred slot snapshot destroyed");
  if(ft4){double e;need(!retained->ft4Energy(0,576,150,e),"future FT4 samples leaked");}
  else need(!retained->fftRow(0,1920),"future FT8 samples leaked");
  ++c.epoch;c.slotUtcMs+=ft4?7500:15000;
  need(rx.m_incrementalAnalysis->submit(c,0,s.mid(0,5000)),"new generation submit failed");
  need(!rx.m_incrementalAnalysis->snapshot(s,retained->config.slotUtcMs),"previous slot reappeared");
  std::cout<<QJsonDocument(QJsonObject{{"file",name},{"candidates",plain.size()},{"cache_hits",hits},{"rows",int(prepared->rows.size())},{"preparation_ms",prepared->preparationMs},{"cached_finder_ms",cachedMs},{"exact_parity",true},{"residual_fallback",true},{"invalidation",true}}).toJson(QJsonDocument::Compact).constData()<<std::endl;
 }
}
