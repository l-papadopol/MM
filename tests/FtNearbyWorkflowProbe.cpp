// SPDX-License-Identifier: GPL-3.0-or-later
#include "modems/ft8/Ft8RxDecoder.h"
#include "audio/WavFileReader.h"
#include "dsp/gpu/FtSyncCompute.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>
#include <QThread>
#include <QElapsedTimer>
#include "utils/SystemResourceManager.h"
#include <cstring>
#include <iostream>
int main(int argc,char **argv){
 QCoreApplication app(argc,argv);
 if(qEnvironmentVariableIsSet("PROBE_WORKERS")){auto &r=MadModemRuntime::SystemResourceManager::instance();r.m_poolCapacity=qEnvironmentVariableIntValue("PROBE_WORKERS");r.m_maxLiveWorkers=r.m_poolCapacity;}
 for(int i=1;i<argc;++i){
  QFile f(argv[i]);if(!f.open(QIODevice::ReadOnly))return 2;
  QVector<double> samples;
  if(QString(argv[i]).endsWith(".wav",Qt::CaseInsensitive)) {
   MadModemAudio::WavFileFormat format;QString error;
   if(!MadModemAudio::parseWavHeader(f,format,error)||!f.seek(format.dataOffset))return 7;
   auto mono=MadModemAudio::convertWavBytesToMono(f.read(format.dataSize),format);
   Ft8RxDecoder converter;auto all=converter.offlineResampleTo12k(mono,int(format.sampleRate));
   bool ok=false;double offset=qEnvironmentVariable("PROBE_OFFSET_SEC").toDouble(&ok);if(!ok)offset=0;
   const int start=qRound(offset*12000);if(start<0||start>=all.size())return 8;
   samples=all.mid(start,180000);samples.resize(180000);
  } else {const auto bytes=f.readAll();samples.resize(bytes.size()/sizeof(double));std::memcpy(samples.data(),bytes.constData(),samples.size()*sizeof(double));}
  Ft8RxDecoder rx;const int period=samples.size()<100000?7500:15000;rx.setModeName(period==7500?"FT4":"FT8");
  rx.setRxMarkerHz(1802);rx.setRadioFrequencyHz(14074000);rx.setDriftRxOptions(false,20);
  if(qEnvironmentVariableIsSet("PROBE_GPU_DECODE")){
   rx.setGpuDecodeEnabled(true);QElapsedTimer warm;warm.start();
   while(!FtSyncCompute::Engine::instance().status().ready&&!FtSyncCompute::Engine::instance().status().unavailable&&warm.elapsed()<20000)QThread::msleep(5);
  }
  const bool offline=qEnvironmentVariableIsSet("PROBE_OFFLINE");
  rx.m_offlineAnalysisActive=offline;
  if(qEnvironmentVariableIsSet("PROBE_FAST"))rx.setDeepDecodeEnabled(false);
  Ft8RxDecoder::PerfStats s;s.phase=offline?"offline":"boundary";
  auto slot=QDateTime::currentDateTimeUtc().addMSecs(-period-qEnvironmentVariableIntValue("PROBE_LATE_MS"));
  MadModemFt::AnalysisConfig c;c.epoch=1;c.slotUtcMs=slot.toMSecsSinceEpoch();c.ft4=period==7500;c.lowHz=rx.m_searchLowHz;c.highHz=rx.m_searchHighHz;c.markerHz=rx.m_rxMarkerHz;
  std::shared_ptr<const MadModemFt::PreparedAnalysis> prepared;
  if(!qEnvironmentVariableIsSet("PROBE_NO_CACHE")){
   for(int offset=0;offset<samples.size();offset+=1200)rx.m_incrementalAnalysis->submit(c,offset,samples.mid(offset,qMin(1200,samples.size()-offset)));
   const int expected=(samples.size()-(c.ft4?576:1920)-1)/(c.ft4?576:480)+1;
   QElapsedTimer wait;wait.start();while(wait.elapsed()<5000){prepared=rx.m_incrementalAnalysis->snapshot(samples,c.slotUtcMs);if(prepared&&int(prepared->rows.size())==expected)break;QThread::msleep(1);}
   if(!prepared||int(prepared->rows.size())!=expected)return 5;
  }
  slot=QDateTime::currentDateTimeUtc().addMSecs(-period-qEnvironmentVariableIntValue("PROBE_LATE_MS"));
  MadModemFt::AnalysisReadScope scope(prepared,samples);
  QElapsedTimer clock;clock.start();double first=-1;
  QSet<QString> published;
  const auto rows=rx.decodeSlot(samples,slot,nullptr,&s,[&](const QVector<Ft8RxDecoder::Decode>& batch){if(first<0&&!batch.isEmpty())first=clock.nsecsElapsed()/1e6;for(const auto &d:batch)published.insert(d.message);});
  QSet<QString> finalMessages;for(const auto &d:rows)finalMessages.insert(d.message);
  const bool retained=(published-finalMessages).isEmpty();
  QJsonArray ds;for(const auto &d:rows)ds.append(QJsonObject{{"message",d.message},{"freq",d.frequencyHz},{"dt",d.dt},{"snr",d.snrDb}});
  std::cout<<QJsonDocument(QJsonObject{{"file",QString(argv[i])},{"total_ms",s.totalMs},{"first_publish_ms",first},{"search_ms",s.candidateSearchMs},{"preparation_ms",prepared?prepared->preparationMs:0.0},{"fft_hits",int(scope.context.fftHits.load())},{"tone_hits",int(scope.context.toneHits.load())},{"workers",s.workerCount},{"passes",s.passCount},{"candidates",s.candidateCount},{"ldpc_tried",s.ldpcTried},{"sync_rejects",s.syncGateRejects},{"osd_tried",s.osdGf2Tried},{"budget_hit",s.timeBudgetHit},{"baseline_rows",rows.size()-s.coherentTailDecodes},{"tail_tried",s.coherentTailCandidates},{"tail_new",s.coherentTailDecodes},{"tail_ms",s.coherentTailMs},{"published_retained",retained},{"arm_margin_ms",double(s.replyArmDeadlineUtcMs-QDateTime::currentMSecsSinceEpoch())},{"stop",s.earlyStopReason},{"decodes",ds}}).toJson(QJsonDocument::Compact).constData()<<std::endl;
  if(!retained)return 4;
  if(qEnvironmentVariableIntValue("PROBE_LATE_MS")>=2000 && (s.coherentTailCandidates!=0 || s.coherentTailDecodes!=0))return 3;
 }
}
