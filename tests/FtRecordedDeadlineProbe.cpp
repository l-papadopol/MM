// SPDX-License-Identifier: GPL-3.0-or-later
#include "modems/ft8/Ft8RxDecoder.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>
#include <cstring>
#include <iostream>
int main(int argc,char **argv){
 QCoreApplication app(argc,argv);
 for(int i=1;i<argc;++i){
  QFile f(argv[i]);if(!f.open(QIODevice::ReadOnly))return 2;
  const auto bytes=f.readAll();QVector<double> samples(bytes.size()/sizeof(double));std::memcpy(samples.data(),bytes.constData(),bytes.size());
  Ft8RxDecoder rx;const int period=samples.size()<100000?7500:15000;rx.setModeName(period==7500?"FT4":"FT8");
  Ft8RxDecoder::PerfStats s;s.phase="boundary";
  const auto slot=QDateTime::currentDateTimeUtc().addMSecs(-period-qEnvironmentVariableIntValue("PROBE_LATE_MS"));
  QSet<QString> published;
  const auto rows=rx.decodeSlot(samples,slot,nullptr,&s,[&](const QVector<Ft8RxDecoder::Decode>& batch){for(const auto &d:batch)published.insert(d.message);});
  QSet<QString> finalMessages;for(const auto &d:rows)finalMessages.insert(d.message);
  const bool retained=(published-finalMessages).isEmpty();
  QJsonArray ds;for(const auto &d:rows)ds.append(QJsonObject{{"message",d.message},{"freq",d.frequencyHz},{"dt",d.dt},{"snr",d.snrDb}});
  std::cout<<QJsonDocument(QJsonObject{{"file",QString(argv[i])},{"total_ms",s.totalMs},{"baseline_rows",rows.size()-s.coherentTailDecodes},{"tail_tried",s.coherentTailCandidates},{"tail_new",s.coherentTailDecodes},{"tail_ms",s.coherentTailMs},{"published_retained",retained},{"arm_margin_ms",double(s.replyArmDeadlineUtcMs-QDateTime::currentMSecsSinceEpoch())},{"stop",s.earlyStopReason},{"decodes",ds}}).toJson(QJsonDocument::Compact).constData()<<std::endl;
  if(!retained)return 4;
  if(qEnvironmentVariableIntValue("PROBE_LATE_MS")>=2000 && (s.coherentTailCandidates!=0 || s.coherentTailDecodes!=0))return 3;
 }
}
