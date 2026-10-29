// SPDX-License-Identifier: GPL-3.0-or-later
#include "FtReferenceRecovery.h"
#include "../../runtime/FtDecodeWorkerPool.h"
#include "../../runtime/SessionDiagnostics.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QElapsedTimer>
#include <QScopeGuard>
#include <QSet>
#include <QHash>
#include <algorithm>
#include <cmath>
#include <mutex>
#include <vector>
#include "../../third_party/mshv_gpl/ft_reference/decoderms.h"
#include "../../third_party/mshv_gpl/ft_reference/budget.h"
struct FtReferenceRecovery::State {
 struct Frame {std::mutex mutex;QVector<Row> rows;QSet<QString> seen,inputs;};
 std::mutex job;QString mode,myCall,dxCall;double radioHz=0;int low=0,high=0,lanes=0,epoch=-1;
 std::vector<std::unique_ptr<MshvReference::DecoderFt8>> ft8;
 std::vector<std::unique_ptr<MshvReference::DecoderFt4>> ft4;
 QHash<qint64,std::shared_ptr<Frame>> frames;
 qint64 lastSlot=0;
};
FtReferenceRecovery::FtReferenceRecovery():state(new State){}
FtReferenceRecovery::~FtReferenceRecovery()=default;
FtReferenceRecovery::Result FtReferenceRecovery::decode(const QVector<double>&samples,const Request&r,const std::function<void(const Row&)>&onRow){
 std::lock_guard<std::mutex> owner(state->job);Result result;QElapsedTimer timer;timer.start();
 auto stopped=[&]{return (r.cancelled&&r.cancelled())||(!r.offline&&QDateTime::currentMSecsSinceEpoch()>=r.deadlineUtcMs)||timer.elapsed()>6500;};
 if ((r.mode!=QStringLiteral("FT8") && r.mode!=QStringLiteral("FT4")) || r.lowHz<0 || r.highHz>5000 || r.highHz<=r.lowHz || !std::isfinite(r.radioHz)) return result;
 const bool is4=r.mode==QStringLiteral("FT4");
 const int lanes=std::max(1,std::min(4,r.workers));
 if(state->epoch!=r.epoch||state->mode!=r.mode||state->myCall!=r.myCall||state->dxCall!=r.dxCall||std::abs(state->radioHz-r.radioHz)>100.0||state->low!=r.lowHz||state->high!=r.highHz||state->lanes!=lanes){
  state->ft8.clear();state->ft4.clear();state->frames.clear();state->lastSlot=0;
  state->epoch=r.epoch;state->mode=r.mode;state->myCall=r.myCall;state->dxCall=r.dxCall;state->low=r.lowHz;state->high=r.highHz;state->lanes=lanes;
 }
 state->radioHz=r.radioHz; // Small, continuous CAT/Doppler steps retain history.
 auto &entry=state->frames[r.slotUtcMs];if(!entry)entry=std::make_shared<State::Frame>();auto frame=entry;
 if (r.cancelled && r.cancelled()) return result;
 result.rows=frame->rows;
 if(stopped()||samples.isEmpty()){result.cached=true;result.timedOut=stopped();return result;}
 double power=0;for(double x:samples)power+=x*x;if(!std::isfinite(power)||power<1.e-16)return result;
 const QByteArray digest=QCryptographicHash::hash(QByteArray::fromRawData(reinterpret_cast<const char*>(samples.constData()),int(samples.size()*sizeof(double))),QCryptographicHash::Sha256);
 const QString input=r.phase+QString::fromLatin1(digest.toHex());
 if(frame->inputs.contains(input)){result.rows=frame->rows;result.cached=true;return result;}
 for(auto it=state->frames.begin();it!=state->frames.end();){if(it.key()<r.slotUtcMs-60000)it=state->frames.erase(it);else ++it;}
 const QString utc=QDateTime::fromMSecsSinceEpoch(r.slotUtcMs,Qt::UTC).time().toString(QStringLiteral("HHmmss"));
 const bool fresh=state->lastSlot!=r.slotUtcMs;
 for(int i=0;i<lanes;++i){if(stopped())break;
  if(is4){if(int(state->ft4.size())<=i)state->ft4.emplace_back(new MshvReference::DecoderFt4(i));auto &d=*state->ft4[i];
   d.SetStWords(r.myCall,r.myCall,0,0,QStringLiteral("CQ"));d.SetStHisCall(r.dxCall);d.SetStDecode(utc,0,r.offline);d.SetStDecoderDeep(3);d.SetStApDecode(true);d.SetStQSOProgress(0);d.SetStTxFreq(r.markerHz);
  }else{if(int(state->ft8.size())<=i)state->ft8.emplace_back(new MshvReference::DecoderFt8(i));auto &d=*state->ft8[i];
   if(fresh&&state->lastSlot)d.SetEndPStaticAll();d.SetNewP(fresh);
   d.SetStWords(r.myCall,r.myCall,0,0,QStringLiteral("CQ"));d.SetStHisCallGrid(r.dxCall,r.dxCall,QString());d.SetStDecode(utc,0,r.offline);d.SetStDecoderDeep(3);d.SetStApDecode(true);d.SetStQSOProgress(0,0);d.SetStTxFreq(r.markerHz);d.SetVarDecodeFtPar(true,r.offline?3:2,r.offline?3:2);d.Decode3intFt(false);d.SetFreqGlobal(QString::number(qRound64(r.radioHz)));
  }
 }
 const int ready=std::min(lanes,int(is4?state->ft4.size():state->ft8.size()));result.workers=ready;
 const auto clear=qScopeGuard([&]{for(auto &e:state->ft8)e->onMessage={};for(auto &e:state->ft4)e->onMessage={};});
 auto accept=[&](const QStringList&fields){if(fields.size()<8||stopped())return;
  Row row;row.message=fields[4].trimmed().toUpper();row.method=fields[5];row.snr=fields[1].toInt();row.dt=fields[2].toDouble();row.frequency=fields[7].toInt();row.quality=fields[6].toDouble();
  const bool valid=!row.method.contains(QLatin1Char('?'))&&row.message.size()>=3&&!row.message.contains(QStringLiteral("???"))&&!row.message.contains(QStringLiteral("***"))&&std::isfinite(row.dt)&&row.frequency>=r.lowHz-1&&row.frequency<=r.highHz+1;
  std::lock_guard<std::mutex> lock(frame->mutex);
  SessionDiagnostics::instance().record("ft.reference.row",{{"mode",r.mode},{"slot_utc_ms",QString::number(r.slotUtcMs)},{"frequency",row.frequency},{"dt",row.dt},{"message",row.message},{"method",row.method},{"quality",row.quality},{"accepted",valid}});
  if(!valid){++result.rejected;return;}if(frame->seen.contains(row.message))return;
  frame->seen.insert(row.message);frame->rows.append(row);if(onRow)onRow(row);
 };
 for(int i=0;i<ready;++i){if(is4)state->ft4[i]->onMessage=accept;else state->ft8[i]->onMessage=accept;}
 if(!stopped()&&ready){
  FtDecodeWorkerPool::instance().parallelFor(ready,ready,[&](int begin,int end){
   const auto reset=qScopeGuard([]{MshvReference::stopRequested={};});MshvReference::stopRequested=stopped;
   for(int lane=begin;lane<end&&!stopped();++lane){QVector<double> audio(std::max(190000,int(samples.size())));for(int n=0;n<std::min(int(samples.size()),180000);++n)audio[n]=samples[n]*32768.;
    const double width=double(r.highHz-r.lowHz)/ready;const double lo=std::max(double(r.lowHz),r.lowHz+lane*width-55.0),hi=std::min(double(r.highHz),r.lowHz+(lane+1)*width+55.0);bool got=false;
    if(is4)state->ft4[lane]->ft4_decode(audio.data(),lo,hi,r.lowHz,r.highHz,r.markerHz,got);
    else state->ft8[lane]->ft8_decode(audio.data(),180000,lo,hi,r.markerHz,got,fresh?100:3,r.lowHz,r.highHz);
   }
  });
 }
 state->lastSlot=r.slotUtcMs;if(!stopped())frame->inputs.insert(input);result.rows=frame->rows;result.elapsedMs=timer.nsecsElapsed()/1.e6;result.timedOut=stopped();
 SessionDiagnostics::instance().record("ft.reference.finished",{{"mode",r.mode},{"slot_utc_ms",QString::number(r.slotUtcMs)},{"workers",result.workers},{"decoded",result.rows.size()},{"rejected",result.rejected},{"elapsed_ms",result.elapsedMs},{"cutoff",result.timedOut}});
 return result;
}
