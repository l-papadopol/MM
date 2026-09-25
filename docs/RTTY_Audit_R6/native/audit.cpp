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
#include "afc_extracted.h"
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
int main(int argc,char**argv){QCoreApplication app(argc,argv);QString msg="CQ CQ TEST IZ6NNH IZ6NNH 599 15 TU CQ ";
 for(int rate:{44100,48000,96000}){
 trial("reference_clean",reference(msg,rate),rate,msg);
 trial("production_clean",production(msg,rate),rate,msg);
 trial("conditioned",reference(msg,rate),rate,msg,1024,1);
 trial("matched_and_enhancer",reference(msg,rate),rate,msg,1024,2);
 }
 for(int chunk:{137,4096})trial("block_size_"+QString::number(chunk),reference(msg,48000),48000,msg,chunk);
 for(double offset:{-100.,-60.,-30.,30.,60.,100.})trial("offset_"+QString::number(offset),reference(msg,48000,45.45,offset),48000,msg);
 for(double snr:{12.,6.,0.,-6.,-12.})trial("AWGN_broadband_dB_"+QString::number(snr),reference(msg,48000,45.45,0,false,snr),48000,msg);
 for(double baud:{44.5,45.,46.,46.5})trial("baud_actual_"+QString::number(baud),reference(msg,48000,baud),48000,msg);
 trial("auto_reverse",reference(msg,48000,45.45,0,true),48000,msg,1024,0,true);
 trial("single_stop_bit",reference(msg,48000,45.45,0,false,100,1),48000,msg);
 RttyDecoder rx;rx.setAutoReverseEnabled(false);QString out;QObject::connect(&rx,&RttyDecoder::characterReceived,[&](const QString&s){out+=s;});feed(rx,production("599",48000),48000);const int old=out.size();feed(rx,production("TEST",48000),48000);result({{"case","consecutive_tx_after_figures"},{"first",out.left(old)},{"second",out.mid(old)},{"expected_second","TEST"}});
 AudioContinuity continuity;AudioBlock b;b.sampleRate=48000;b.samples=QVector<float>(1024);b.captureGeneration=1;continuity.accept(b);b.captureGeneration=2;result({{"case","R6_fast_resume_generation"},{"full_reset_triggered",continuity.accept(b)}});
 for(int count:{1000,10000}){QTemporaryDir d;QString file=d.filePath("log.adi");QFile f(file);f.open(QIODevice::WriteOnly);f.write("<EOH>\n");LogbookEntry entry;entry.callsign="IZ6NNH";entry.mode="RTTY";entry.band="20m";entry.utc=QDateTime::currentDateTimeUtc();auto record=AdifLogbook::entryToAdif(entry).toUtf8()+"\n";for(int i=0;i<count;++i)f.write(record);f.close();AdifLogbook log(file);log.load();QElapsedTimer t;t.start();QString error;bool ok=log.append(entry,&error);result({{"case","logbook_append"},{"records",count},{"ms",double(t.nsecsElapsed())/1e6},{"ok",ok},{"bytes",double(QFile(file).size())}});}
 for(int rate:{48000,96000})for(int chunk:{1024,4096}){auto wave=reference(msg.repeated(2),rate,45.45,15);int mark=2125,space=2295,until=0,changes=0,minShift=170,maxShift=170;for(int i=0;i<wave.size();i+=chunk){AudioBlock b;b.sampleRate=rate;b.samples=wave.mid(i,chunk);until+=b.samples.size();if(until<std::max(2048,rate/4))continue;until=0;auto m=estimateAfcTonePeak(b,mark,20,1);auto sp=estimateAfcTonePeak(b,space,20,1);int nm=m.valid?nudgedToneValue(mark,m.frequencyHz,3,300,3500):mark;int ns=sp.valid?nudgedToneValue(space,sp.frequencyHz,3,300,3500):space;if(ns-nm>=50&&ns-nm<=1200){changes+=(nm!=mark||ns!=space);mark=nm;space=ns;}minShift=std::min(minShift,space-mark);maxShift=std::max(maxShift,space-mark);}result({{"case","AFC_15Hz_offset"},{"rate",rate},{"chunk",chunk},{"changes",changes},{"final_mark",mark},{"final_space",space},{"min_shift",minShift},{"max_shift",maxShift}});}
 for(int rate:{48000,96000})for(bool enhanced:{false,true}){auto a=reference(QString("CQ CQ DE IZ6NNH CQ ").repeated(4),rate);auto b=reference(QString("CQ CQ DE DL1ABC CQ ").repeated(4),rate,45.45,-1000);a.resize(std::min(a.size(),b.size()));for(int i=0;i<a.size();++i)a[i]+=b[i];RttyMultiDecoder multi;multi.configure(45.45,170,false,true,true,enhanced,enhanced,8);QElapsedTimer total;total.start();double maxMs=0;for(int i=0;i<a.size();i+=1024){AudioBlock block;block.sampleRate=rate;block.firstSampleIndex=i;block.samples=a.mid(i,1024);QElapsedTimer t;t.start();multi.processAudioBlock(block);maxMs=std::max(maxMs,t.nsecsElapsed()/1e6);QCoreApplication::processEvents();}QStringList labels;for(const auto&c:multi.callouts())labels<<c.label;result({{"case","multidecoder_two_stations"},{"rate",rate},{"enhanced_secondpass",enhanced},{"audio_seconds",double(a.size())/rate},{"ms",total.nsecsElapsed()/1e6},{"max_block_ms",maxMs},{"labels",labels.join(" | ")}});}
 for(const QString&call:{QString("IZ6NNH"),QString("IG9ABC"),QString("IT9ABC"),QString("K1ABC"),QString("K6ABC"),QString("IZ6NNH/MM"),QString("IZ6NNH/EA8")}){auto e=CtyCountryFile::instance().lookupCallsign(call);result({{"case","cty_lookup"},{"call",call},{"country",e.entity.name},{"dxcc",e.entity.dxcc},{"zone",e.entity.cqZone},{"continent",e.entity.continent},{"prefix",e.entity.primaryPrefix}});}
 std::mt19937 rng(42);std::normal_distribution<float> noise(0,.08);QVector<float> random(48000*30);for(auto&v:random)v=noise(rng);trial("noise_only_30s",random,48000,QString());
 return 0;
}
