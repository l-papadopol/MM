#include "modems/rtty/RttyDecoder.h"
#include "modems/rtty/RttyMultiDecoder.h"
#include "modems/rtty/tx/RttyTransmitter.h"
#include "dsp/common/DspConditioner.h"
#include "audio/AudioContinuity.h"
#include "logbook/AdifLogbook.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QFile>
#include <random>
#include <cmath>
#include <iostream>
#include <numeric>
#include "modems/rtty/RttyAfc.h"
#include "logbook/CqWwRtty.h"
#include "network/QsoUdpBroadcaster.h"
#include <QUdpSocket>
#include <QDataStream>
#include "dxcc/CtyCountryFile.h"
static constexpr double pi=3.14159265358979323846;
void result(QJsonObject obj){std::cout<<QJsonDocument(obj).toJson(QJsonDocument::Compact).constData()<<'\n';}
int distance(const QString&a,const QString&b){QVector<int> row(b.size()+1);std::iota(row.begin(),row.end(),0);for(int i=0;i<a.size();++i){int prev=row[0];row[0]=i+1;for(int j=0;j<b.size();++j){int old=row[j+1];row[j+1]=std::min({row[j+1]+1,row[j]+1,prev+(a[i]!=b[j])});prev=old;}}return row.last();}
// Independent ITA2 encoder: phase-continuous sine; symbol boundaries rounded
// from cumulative ideal time, not from production TX segments. Explicit LTRS.
QVector<float> reference(const QString& text,int rate,double baud=45.45,double offset=0,bool reverse=false,double snr=100,double stop=1.5){
 const QString letters=QString::fromLatin1("\0E\nA SIU\rDRJNFCKTZLWHYPQOBG\0MXV\0",32);
 const QString figures=QString::fromLatin1("\0003\n- '\0707\r$4\a,!:(5\")2#6019?&\0./;\0",32);
 QVector<float> out;double bits=0,phase=0;std::mt19937 rng(0x52675454);std::normal_distribution<double> gauss;
 auto bit=[&](bool mark,double length){bits+=length;int end=std::lround(bits*rate/baud);double f=(mark!=reverse?2125:2295)+offset;while(out.size()<end){double s=.2*std::sin(phase);if(snr<90)s+=.2/std::sqrt(2.)*std::pow(10.,-snr/20.)*gauss(rng);out.append(s);phase=std::fmod(phase+2*pi*f/rate,2*pi);}};
 auto code=[&](int c){bit(false,1);for(int b=0;b<5;++b)bit(c&(1<<b),1);bit(true,stop);};
 bit(true,20);code(31);bool figs=false;
 for(QChar c:text){int idx=letters.indexOf(c);bool need=false;if(idx<0){idx=figures.indexOf(c);need=true;}if(idx<0)continue;if(need!=figs){code(need?27:31);figs=need;}code(idx);}
 bit(true,20);return out;
}
QVector<float> production(const QString&text,int rate){RttyTransmitter tx(text,rate,45.45,2125,2295,false);QVector<float> all;while(!tx.isFinished()){QVector<float>b(137);int n=tx.generate(b.data(),b.size());b.resize(n);all+=b;}return all;}
void feed(RttyDecoder&rx,const QVector<float>& samples,int rate,int chunk=1024,DspConditioner*cond=nullptr){for(int i=0;i<samples.size();i+=chunk){AudioBlock b;b.sampleRate=rate;b.firstSampleIndex=i;b.samples=samples.mid(i,chunk);rx.processAudioBlock(cond?cond->processBlock(b):b);QCoreApplication::processEvents();}}
void trial(const QString& name,const QVector<float>& wave,int rate,const QString&expected,int chunk=1024,int conditioning=0,bool autoPol=false){RttyDecoder rx;rx.setAutoReverseEnabled(autoPol);QString decoded;QObject::connect(&rx,&RttyDecoder::characterReceived,[&](const QString&s){decoded+=s;});QObject::connect(&rx,&RttyDecoder::reversePolarityRequested,&rx,&RttyDecoder::setReverse,Qt::QueuedConnection);DspConditioner cond;DspConditioner::Config cfg;cfg.profile=DspConditioner::Profile::Rtty;cfg.enabled=true;cfg.blackHz=2125;cfg.whiteHz=2295;cfg.modeBandpassEnabled=true;cfg.humNotchEnabled=true;cfg.noiseReductionEnabled=false;cfg.agcEnabled=false;cfg.rttyMatchedFilterEnabled=conditioning==2;cfg.rttyMarkSpaceEnhancerEnabled=conditioning==2;cond.setConfig(cfg);QElapsedTimer timer;timer.start();feed(rx,wave,rate,chunk,conditioning?&cond:nullptr);result({{"case",name},{"rate",rate},{"samples",wave.size()},{"ms",double(timer.nsecsElapsed())/1e6},{"expected",expected},{"decoded",decoded},{"edit_distance",distance(expected,decoded)},{"exact",expected==decoded}});}
int main(int argc,char **argv) {
 QCoreApplication app(argc,argv); bool ok=true;
 auto check=[&](bool v,const char *name){std::cout<<(v?"PASS ":"FAIL ")<<name<<"\n";ok &= v;};
 const QString msg="CQ TEST IZ6NNH 599 15 TU ";
 for(int rate:{44100,48000,96000}) for(int chunk:{137,1024,4096}) {
  RttyDecoder rx;rx.setAutoReverseEnabled(false);QString out;
  QObject::connect(&rx,&RttyDecoder::characterReceived,[&](const QString&t){out+=t;});
  feed(rx,production("599",rate),rate,chunk);check(out=="599","initial FIGS");
  out.clear();rx.resumeAfterLocalTransmit();feed(rx,production("TEST",rate),rate,chunk);check(out=="TEST","new TX restores LTRS after FIGS and RX resume");
  out.clear();feed(rx,reference(msg,rate),rate,chunk);check(out==msg,"independent encoder clean");
  RttyAfc::Tracker afc;auto wave=reference(msg.repeated(2),rate,45.45,15);
  for(int i=0;i<wave.size();i+=chunk){AudioBlock b;b.sampleRate=rate;b.samples=wave.mid(i,chunk);afc.process(b,2125,170,20);if(std::abs(afc.offsetHz())>20)check(false,"AFC bounded");}
  std::cout<<"AFC "<<rate<<" chunk "<<chunk<<" offset "<<afc.offsetHz()<<"\n";
  check(std::abs(afc.offsetHz()-15)<=4,"AFC common offset acquired");
 }
 AudioContinuity c;AudioBlock b;b.samples=QVector<float>(137);b.captureGeneration=1;
 check(!c.accept(b),"first capture");b.firstSampleIndex=137;check(!c.accept(b),"contiguous capture");
 b.firstSampleIndex=500;check(c.accept(b),"unexpected gap resets");c.reset();b.captureGeneration=2;b.firstSampleIndex=0;check(!c.accept(b),"expected restart preserves decoder state");
 LogbookEntry e;e.callsign="K6ABC";e.stationCallsign="IZ6NNH";e.operatorCall="IZ6NNH";e.mode="RTTY";e.band="20m";e.freq="14.085000";e.rstSent=e.rstReceived="599";e.utc=QDateTime(QDate(2026,9,26),QTime(0,5),Qt::UTC);
 e.adifFields={{"CONTEST_ID","CQ-WW-RTTY"},{"APP_MADMODEM_RTTY_SESSION","session1"},{"APP_MADMODEM_RTTY_TX_CQZONE","15"},{"APP_MADMODEM_RTTY_RX_CQZONE","03"},{"APP_MADMODEM_RTTY_RX_QTH","CA"}};
 check(CqWwRtty::zoneKey("03")==CqWwRtty::zoneKey("3"),"zone leading zero is not another multiplier");
 check(CqWwRtty::validate(e,true).isEmpty(),"valid complete CQ WW QSO");
 auto other=e;other.callsign="K1ABC";other.adifFields["APP_MADMODEM_RTTY_RX_CQZONE"]="3";
 auto invalid=e;invalid.adifFields.remove("APP_MADMODEM_RTTY_RX_CQZONE");
 const auto score=CqWwRtty::score({invalid,e,e,other},2026);
 check(score.qsos==2 && score.points==6 && score.multipliers.size()==3,"score skips invalid/dupe and uses received zone");
 auto island=e;island.callsign="IG9ABC";island.adifFields["APP_MADMODEM_RTTY_RX_CQZONE"]="33";
 auto sicily=island;sicily.callsign="IT9ABC";sicily.adifFields["APP_MADMODEM_RTTY_RX_CQZONE"]="15";
 check(CqWwRtty::score({island,sicily},2026).multipliers.size()==4,"separate WAE countries and zones");
 QString err;auto cab=CqWwRtty::cabrillo({e},{},&err);
 check(cab.contains("IZ6NNH 599 15 DX K6ABC 599 03 CA") && cab.endsWith("END-OF-LOG:\n"),"Cabrillo received zone preserved");
 auto bad=e;bad.adifFields["APP_MADMODEM_RTTY_RX_CQZONE"]="00";check(!CqWwRtty::validate(bad).isEmpty(),"reject invalid zone");
 bad=e;bad.adifFields.remove("APP_MADMODEM_RTTY_RX_QTH");check(!CqWwRtty::validate(bad).isEmpty(),"require US state");
 bad=e;bad.adifFields["APP_MADMODEM_RTTY_RX_QTH"]="ON";check(!CqWwRtty::validate(bad).isEmpty(),"US state not Canadian province");
 bad=e;bad.freq.clear();check(CqWwRtty::cabrillo({bad},{},&err).isEmpty(),"export rejects absent frequency");
 bad=e;bad.utc=bad.utc.addDays(-1);check(CqWwRtty::cabrillo({bad},{},&err).isEmpty(),"export rejects practice date");
 bad=e;bad.adifFields["APP_MADMODEM_RTTY_SESSION"]="other";check(CqWwRtty::cabrillo({e,bad},{},&err).isEmpty(),"export rejects mixed sessions");
 bad=e;bad.callsign="IG9ABC";check(CqWwRtty::points(bad)==3,"IG9 intercontinental despite shared DXCC");
 bad.callsign="IT9ABC";check(CqWwRtty::points(bad)==2,"IT9 different contest country");
 bad.callsign="I1ABC";check(CqWwRtty::points(bad)==1,"Italian same contest country");
 check(CtyCountryFile::instance().lookupCallsign("IZ6NNH/EA8").entity.primaryPrefix=="EA8","geographical portable suffix");
 for(int format:{0,1,2}) {
  QUdpSocket socket;check(socket.bind(QHostAddress(QHostAddress::LocalHost),quint16(0)),"UDP bind");QsoUdpBroadcaster::SendContext ctx;ctx.messageFormat=format;
  auto result=QsoUdpBroadcaster::sendQsoLoggedBundle(e,ctx,"127.0.0.1",socket.localPort(),"test");if(!result.ok) std::cout<<result.error.toStdString()<<"\n";check(result.ok,"UDP send");
  QList<quint32> types;while(socket.hasPendingDatagrams()||socket.waitForReadyRead(100)) {QByteArray data;data.resize(socket.pendingDatagramSize());socket.readDatagram(data.data(),data.size());QDataStream stream(data);quint32 magic,schema,type;stream>>magic>>schema>>type;types<<type;}
  check(types== (format==0 ? QList<quint32>{0,5,12} : format==1?QList<quint32>{0,5}:QList<quint32>{0,12}),"UDP format emits expected notifications");
 }
 for(int rate:{48000,96000}) {
  auto a=reference(QString("CQ CQ DE IZ6NNH CQ ").repeated(4),rate),b=reference(QString("CQ CQ DE DL1ABC CQ ").repeated(4),rate,45.45,-1000);
  a.resize(std::min(a.size(),b.size()));for(int i=0;i<a.size();++i)a[i]+=b[i];RttyMultiDecoder multi;multi.configure(45.45,170,false,true,true,true,true,8);
  double maxMs=0;for(int i=0;i<a.size();i+=1024){AudioBlock block;block.sampleRate=rate;block.samples=a.mid(i,1024);QElapsedTimer t;t.start();multi.processAudioBlock(block);maxMs=std::max(maxMs,t.nsecsElapsed()/1e6);QCoreApplication::processEvents();}
  QStringList labels;for(const auto &v:multi.callouts())labels<<v.label;std::cout<<"MULTI "<<rate<<" max_ms="<<maxMs<<" labels="<<labels.join("|").toStdString()<<"\n";
  check(labels.size()==2 && labels.count("CQ IZ6NNH")==1 && labels.count("CQ DL1ABC")==1,"two stations decoded without duplicate callouts");
 }
 return ok?0:1;
}
