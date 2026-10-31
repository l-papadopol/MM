// SPDX-License-Identifier: GPL-3.0-or-later
#include "modems/ft8/Ft8RxDecoder.h"
#include "utils/SystemResourceManager.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <iostream>
#include <cstring>
int main(int argc,char **argv){
 QCoreApplication app(argc,argv);
 // A fixed worker count can isolate arithmetic changes from OSD permit races.
 if(qEnvironmentVariableIsSet("PROBE_WORKERS")){
  int n=qMax(1,qEnvironmentVariableIntValue("PROBE_WORKERS"));
  auto &r=MadModemRuntime::SystemResourceManager::instance();r.m_poolCapacity=n;r.m_maxLiveWorkers=n;
 }
 for(int i=1;i<argc;++i){QFile f(argv[i]);if(!f.open(QIODevice::ReadOnly))return 2;
  auto bytes=f.readAll();QVector<double> samples(bytes.size()/sizeof(double));std::memcpy(samples.data(),bytes.constData(),samples.size()*sizeof(double));
  Ft8RxDecoder rx;rx.setModeName(samples.size()<100000?"FT4":"FT8");
  const bool offline=qEnvironmentVariableIsSet("PROBE_OFFLINE");rx.m_offlineAnalysisActive=offline;
  Ft8RxDecoder::PerfStats s;s.phase=offline?"offline":"boundary";
  auto rows=rx.decodeSlot(samples,QDateTime::fromMSecsSinceEpoch(1792309800000LL,Qt::UTC),nullptr,&s);
  QJsonArray ds;for(const auto&d:rows)ds.append(QJsonObject{{"message",d.message},{"freq",d.frequencyHz},{"dt",d.dt},{"snr",d.snrDb}});
  std::cout<<QJsonDocument(QJsonObject{{"file",QString(argv[i])},{"total_ms",s.totalMs},{"passes",s.passCount},{"workers",s.workerCount},{"search_ms",s.candidateSearchMs},{"decode_ms",s.candidateDecodeMs},{"osd_tried",s.osdGf2Tried},{"osd_skips",s.osdGf2BudgetSkips},{"coherent_attempts",s.coherentMetricAttempts},{"coherent_decodes",s.coherentMetricDecodes},{"candidates",s.candidateCount},{"budget_hit",s.timeBudgetHit},{"stop",s.earlyStopReason},{"decodes",ds}}).toJson(QJsonDocument::Compact).constData()<<std::endl;
 }
}
