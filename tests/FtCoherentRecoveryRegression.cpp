#include "modems/ft8/Ft8RxDecoder.h"
#include "modems/ft8/tx/Ft8Transmitter.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <random>
#include <iostream>
#include <stdexcept>
#include "third_party/mshv_gpl/port/HvGenFt8/bpdecode_ft8_174_91.h"
#include "third_party/mshv_gpl/port/genpom.h"
static void check(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
int main(int argc,char**argv){QCoreApplication app(argc,argv);
 Ft8RxDecoder fec;
 bool msg[77]={}, cw[174]={}; GenPomFt encoder; encoder.encode174_91(msg,cw);
 std::array<int,174> valid{}, invalid{}, decoded{}; std::array<double,174> llr{};
 for(int i=0;i<174;++i)valid[i]=cw[i];
 invalid[0]=1;
 for(int row=0;row<83;++row){int parity=0;for(int col=0;col<91;++col){const int nib=g_ft8_174_91[row].mid(col/4,1).toInt(nullptr,16);parity^=invalid[col]*((nib>>(3-col%4))&1);}invalid[91+row]=parity;}
 check(fec.syndromeOk(invalid)&&!fec.crc14Ok(invalid),"invalid CRC fixture must pass LDPC parity");
 int it=0;
 for(bool bad:{false,true}){const auto &bits=bad?invalid:valid;for(int i=0;i<174;++i)llr[i]=bits[i]?-15.:15.;
 check(fec.ldpcDecode174_91(llr,decoded,it)==!bad,"min-sum accepted CRC-invalid codeword");
 check(fec.ldpcDecode174_91SumProduct(llr,decoded,it)==!bad,"SPA accepted CRC-invalid codeword");}
 int normalCorrect=0;
 const QString expected="CQ IZ6NNH JN63";Ft8Transmitter tx("FT4",expected,48000,1500);QVector<float> pcm(360000);const int n=tx.generate(pcm.data(),pcm.size());QVector<double> clean(90000);for(int i=0;i<n/4&&4032+i<clean.size();++i)clean[4032+i]=pcm[4*i];
 Ft8RxDecoder finder;finder.setModeName("FT4");auto candidates=finder.findFt4Candidates(clean);Ft8RxDecoder::Candidate c;bool found=false;for(const auto &cand:candidates){Ft8RxDecoder::Decode d;if(finder.decodeFt4Candidate(clean,QDateTime::currentDateTimeUtc(),cand,d)&&d.message==expected){c=cand;found=true;break;}}
 if(!found)return 2;
 for(double amp:{0.0,.006,.009,.013,.018,.025,.035})for(int seed=0;seed<6;++seed){std::mt19937 rng(91828+seed);std::normal_distribution<double> noise(0,.1);QVector<double> samples=clean;for(auto &v:samples)v=amp*v+noise(rng);
 for(bool offline:{false,true}){Ft8RxDecoder rx;rx.setModeName("FT4");rx.m_offlineAnalysisActive=offline;Ft8RxDecoder::Decode d;Ft8RxDecoder::CandidateAttemptQuality q;Ft8RxDecoder::DecodeRejectReason reject;QElapsedTimer timer;timer.start();const bool ok=rx.decodeFt4Candidate(samples,QDateTime::currentDateTimeUtc(),c,d,&q,&reject);double ms=timer.nsecsElapsed()/1e6;
 if(!offline&&ok&&d.message==expected)++normalCorrect;
 check(!ok||d.message==expected,"unexpected decoded text");
 if(amp==0)check(!ok,"noise decoded");
 std::cout<<QJsonDocument(QJsonObject{{"amp",amp},{"seed",seed},{"offline",offline},{"accepted",ok},{"correct",ok&&d.message==expected},{"message",ok?d.message:QString()},{"reject",int(reject)},{"hard_sync",q.hardSyncCount},{"coherent_attempts",q.coherentMetricAttempts},{"coherent_recovered",q.coherentMetricRecovered},{"osd_recovered",q.osdGf2Recovered},{"spa_attempts",q.sumProductAttempts},{"spa_recovered",q.sumProductRecovered},{"ms",ms},{"known_candidate_freq",c.baseHz},{"known_candidate_start",c.startSec}}).toJson(QJsonDocument::Compact).constData()<<std::endl;
 }}
 check(normalCorrect>=9,"lost FT4 coherent witnesses");
 QVector<double> silence(90000);Ft8RxDecoder rx;rx.setModeName("FT4");Ft8RxDecoder::Decode d;
 check(!rx.decodeFt4Candidate(silence,QDateTime::currentDateTimeUtc(),c,d),"silence decoded");
 for(const QString phase:{QString("wsjtx-gate"),QString("boundary")}){Ft8RxDecoder::PerfStats st;st.phase=phase;
 auto utc=QDateTime::currentDateTimeUtc().addMSecs(phase=="boundary"?-9500:-7000);
 rx.decodeSlot(clean,utc,nullptr,&st);check(st.coherentMetricAttempts==0&&st.osdGf2Tried==0,"gate/expired slot ran heavy recovery");}
 std::cerr<<"PASS CRC-valid/parity-only codewords, FT4 coherent witnesses, noise/silence, gate/expired slot\n";
}
