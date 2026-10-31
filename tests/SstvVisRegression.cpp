#include "modems/sstv/SstvDecoder.h"
#include "modems/sstv/SstvVis.h"
#include "modems/sstv/tx/SstvTransmitter.h"
#include "third_party/mmsstv_lgpl/MmsstvRxCore.h"
#include <QCoreApplication>
#include <iostream>
#include <stdexcept>
#include <random>
#include <vector>
static void check(bool value,const char *why){if(!value)throw std::runtime_error(why);}
// Independent tones: deliberately do not call the production header generator.
static double frequency(double ms,int code,bool badParity=false,bool badStop=false) {
 if(ms<300)return 1900;if(ms<310)return 1200;if(ms<610)return 1900;if(ms<640)return 1200;
 if(ms<880){int bit=int((ms-640)/30);int parity=0;for(int i=0;i<7;++i)parity^=(code>>i)&1;
  return ((bit==7?(parity^int(badParity)):(code>>bit)&1)?1100:1300);}
 return badStop?1900:1200;
}
int main(int argc,char **argv){QCoreApplication app(argc,argv);try {
 int cases=0;
 for(int rate:{11025,44100,48000,96000})for(const auto &e:SstvVis::entries()) {
  mmsstv_lgpl::FqcToneEstimator fqc;fqc.setSampleRate(rate);SstvVis::Detector detector;
  double phase=0;int got=-1,hits=0;std::mt19937 gen(987);std::normal_distribution<double> noise(0,0.006);
  for(int n=0;n<int(rate*0.94);++n){const double f=frequency(n*1000.0/rate,e.code);phase+=2*3.141592653589793*f/rate;
   int value=detector.feed(fqc.processSample(0.6*std::sin(phase)+noise(gen)),rate);if(value>=0){got=value;++hits;}}
  if(got!=e.code||hits!=1){std::cerr<<e.label<<" rate="<<rate<<" got="<<got<<" hits="<<hits<<"\n";throw std::runtime_error("independent VIS waveform failed");}++cases;
  SstvTransmitter tx(QImage(320,256,QImage::Format_RGB32),QString(e.label),rate);std::vector<float> audio(int(rate*0.91));
  check(tx.generate(audio.data(),int(audio.size()))==int(audio.size()),"header generation length");
  // Project the generated samples at independently expected center-cell tones.
  for(double center:{150.,305.,450.,625.,655.,685.,715.,745.,775.,805.,835.,865.,895.}) {
   const double expected=frequency(center,e.code);double real=0,imag=0,wrongReal=0,wrongImag=0;
   for(int n=int((center-3)*rate/1000);n<int((center+3)*rate/1000);++n){real+=audio[n]*std::cos(2*3.141592653589793*expected*n/rate);imag+=audio[n]*std::sin(2*3.141592653589793*expected*n/rate);
    double wrong=expected==1900?1300:1900;wrongReal+=audio[n]*std::cos(2*3.141592653589793*wrong*n/rate);wrongImag+=audio[n]*std::sin(2*3.141592653589793*wrong*n/rate);}
   check(real*real+imag*imag>4*(wrongReal*wrongReal+wrongImag*wrongImag),"TX header tone mismatch");
  }
 }
 // No leader, invalid parity, invalid stop and unknown modes never reset RX.
 for(int type=0;type<4;++type){SstvVis::Detector d;int hits=0;
  for(int n=0;n<48000;++n){double ms=n/48.;double f=frequency(ms,type==3?1:44,type==1,type==2);if(type==0&&ms<610)f=1500;
   if(d.feed(f,48000)>=0)++hits;}check(hits==0,"invalid header accepted");}
 // Two consecutive valid headers select and re-arm complete frames.
 SstvDecoder rx;int detected=0;QObject::connect(&rx,&SstvDecoder::modeDetected,[&](const QString &mode){check(mode=="Robot 36","wrong selected mode");++detected;});
 double phase=0;for(int frame=0;frame<2;++frame) {
  for(int start=0;start<46080;start+=512){AudioBlock b;b.sampleRate=48000;for(int n=start;n<start+512;++n){phase+=2*3.141592653589793*frequency(n/48.,8)/48000;b.samples.append(float(0.6*std::sin(phase)));}rx.processAudioBlock(b);}
 }
 check(detected==2,"consecutive valid headers did not rearm");
 rx.setAutoVisEnabled(false);for(int start=0;start<46080;start+=512){AudioBlock b;b.sampleRate=48000;for(int n=start;n<start+512;++n){phase+=2*3.141592653589793*frequency(n/48.,8)/48000;b.samples.append(float(0.6*std::sin(phase)));}rx.processAudioBlock(b);}
 check(detected==2,"manual VIS disable ignored");
 for(const auto &entry:SstvVis::entries()) {
  QImage source(320,256,QImage::Format_RGB32);const QColor colors[]={Qt::red,Qt::green,Qt::blue,Qt::yellow,Qt::cyan,Qt::magenta,Qt::white,Qt::black};
  for(int y=0;y<source.height();++y)for(int x=0;x<source.width();++x)source.setPixelColor(x,y,colors[x*8/source.width()]);
  SstvTransmitter tx(source,QString(entry.label),12000);SstvDecoder decoder;int frames=0;
  QObject::connect(&decoder,&SstvDecoder::imageCompleted,[&](const QImage &,const QString &){++frames;});
  while(!tx.isFinished()){AudioBlock block;block.sampleRate=12000;block.samples.resize(512);int count=tx.generate(block.samples.data(),512);block.samples.resize(count);decoder.processAudioBlock(block);}
  const QImage image=decoder.currentImage(),expected=tx.previewImage();double squared=0;int values=0;
  for(int bar=0;bar<8;++bar)for(int row=1;row<4;++row){QSize fitted=source.size().scaled(image.size(),Qt::KeepAspectRatio);int x=(image.width()-fitted.width())/2+(2*bar+1)*fitted.width()/16,y=row*image.height()/4;QColor got=image.pixelColor(x,y),want=expected.pixelColor(x,y);for(int delta:{got.red()-want.red(),got.green()-want.green(),got.blue()-want.blue()}){squared+=delta*delta;++values;}}
  std::cout<<entry.label<<" frames="<<frames<<" size="<<image.width()<<"x"<<image.height()<<" patchRMSE="<<std::sqrt(squared/values)<<"\n";
  check(frames==1,"generated image did not complete");check(image.size()==expected.size(),"RX/TX raster mismatch");check(std::sqrt(squared/values)<10,"clean color bars badly corrupted");
 }
 std::cout<<"PASS "<<cases<<" independent noisy VIS RX + TX tone matrices; negatives and repeated headers\n";
 }catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}return 0;}
