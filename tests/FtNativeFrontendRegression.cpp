// SPDX-License-Identifier: GPL-3.0-or-later
#include "modems/ft8/Ft8RxDecoder.h"
#include "modems/ft8/FtNativeSpectrum.h"
#include "modems/ft8/tx/Ft8Transmitter.h"
#include <QCoreApplication>
#include <random>
#include <iostream>
#include <stdexcept>
static void require(bool ok,const char*why){if(!ok)throw std::runtime_error(why);}
int main(int argc,char**argv){
 QCoreApplication app(argc,argv);std::mt19937 rng(41811);std::normal_distribution<double>noise(0.,.008);
 Ft8RxDecoder rx;rx.m_offlineAnalysisActive=true;rx.setDeepDecodeEnabled(true);
 const QString expected="CQ IZ6NNH JN63";int count=0;
 for(double frequency:{388.3,1033.37,2712.8})for(int offset:{5820,6000,6180}){
  Ft8Transmitter tx("FT8",expected,48000,frequency);QVector<float>pcm(15*48000);int n=tx.generate(pcm.data(),pcm.size());
  QVector<double>samples(180000);for(auto&x:samples)x=noise(rng);for(int k=0;k<n/4&&k+offset<samples.size();++k)samples[k+offset]+=.15*pcm[4*k];
  MadModemFt::FtNativeSpectrum spectrum(samples.constData(),samples.size());MadModemFt::FtNativeSpectrumScope scope(&spectrum);
  Ft8RxDecoder::Candidate c;c.baseHz=std::round(frequency/3.125)*3.125;c.startSec=double(offset)/12000.+.015;
  Ft8RxDecoder::Candidate refined;Ft8RxDecoder::Decode d;Ft8RxDecoder::CandidateAttemptQuality q;Ft8RxDecoder::DecodeRejectReason reason;
  bool ok=rx.decodeCandidate(samples,QDateTime::currentDateTimeUtc(),c,d,&refined,&reason,&q);
  if(!ok||d.message!=expected)std::cerr<<frequency<<" "<<offset<<" "<<int(reason)<<" "<<q.hardSyncCount<<" "<<d.message.toStdString()<<"\n";
  require(ok&&d.message==expected,"native spectral front-end lost a generated CRC-valid frame");
  require(std::abs(refined.baseHz-frequency)<.35,"native frequency estimate");
  require(std::abs(refined.startSec-double(offset)/12000.)<.025,"native timing estimate");++count;
 }
 for(int seed=0;seed<12;++seed){QVector<double>x(180000);for(auto&v:x)v=noise(rng)*10.;MadModemFt::FtNativeSpectrum f(x.constData(),x.size());MadModemFt::FtNativeSpectrumScope scope(&f);Ft8RxDecoder::Candidate c;c.baseHz=350+seed*177;c.startSec=.5;Ft8RxDecoder::Decode d;require(!rx.decodeCandidate(x,QDateTime::currentDateTimeUtc(),c,d),"noise accepted by native spectral front-end");}
 require(MadModemFt::nativeFt8Spectrum==nullptr,"native spectrum scope leaked to next job");
 QVector<double> mixture(180000);for(auto &v:mixture)v=noise(rng);
 for(const auto &signal:std::vector<std::pair<QString,double>>{{expected,1033.37},{"CQ DL1ABC JO62",1212.8}}){
  Ft8Transmitter tx("FT8",signal.first,48000,signal.second);QVector<float>pcm(720000);int n=tx.generate(pcm.data(),pcm.size());
  for(int k=0;k<n/4&&k+6000<mixture.size();++k)mixture[k+6000]+=.15*pcm[4*k];
 }
 Ft8RxDecoder pipeline;pipeline.m_offlineAnalysisActive=true;pipeline.setDeepDecodeEnabled(true);pipeline.setDspPlusDecodeEnabled(true);pipeline.setSearchRangeHz(900,1400);
 Ft8RxDecoder::PerfStats stats;stats.phase="offline";QVector<QString>published;
 auto decoded=pipeline.decodeSlot(mixture,QDateTime::currentDateTimeUtc(),nullptr,&stats,[&](const QVector<Ft8RxDecoder::Decode>&rows){for(const auto &d:rows)published.append(d.message);});
 for(const QString &message:{expected,QString("CQ DL1ABC JO62")})require(std::any_of(decoded.cbegin(),decoded.cend(),[&](const Ft8RxDecoder::Decode&d){return d.message==message;}),"full native integration lost generated signal");
 for(const QString &message:published)require(std::any_of(decoded.cbegin(),decoded.cend(),[&](const Ft8RxDecoder::Decode&d){return d.message==message;}),"additional native recovery removed a published result");
 require(stats.nativeRecoveryPasses>=1&&stats.nativeRecoveryCandidates>0,"full native recovery was not exercised");
 require(std::all_of(decoded.cbegin(),decoded.cend(),[&](const Ft8RxDecoder::Decode&d){return d.message==expected||d.message=="CQ DL1ABC JO62";}),"native recovery introduced a false message in controlled mixture");
 pipeline.m_offlineAnalysisActive=false;
 Ft8RxDecoder::PerfStats expired;expired.phase="boundary";
 pipeline.decodeSlot(mixture,QDateTime::currentDateTimeUtc().addSecs(-17),nullptr,&expired);
 require(expired.nativeRecoveryPasses==0&&expired.nativeRecoveryCandidates==0,"expired slot admitted additional native recovery");
 require(MadModemFt::nativeFt8Spectrum==nullptr&&MadModemFt::nativeFt8StopUtcMs==0,"full job leaked native context");
 std::cout<<count<<" generated FT8 frames, 12 noise controls, full two-signal recovery and published result retention PASS\n";
}
