// Public RX API regression: retained pixels, progressive correction and final save.
#include "modems/sstv/SstvDecoder.h"
#include "modems/sstv/tx/SstvTransmitter.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QBuffer>
#include <iostream>
#include <stdexcept>
static void check(bool b,const char *why){if(!b)throw std::runtime_error(why);}
static QImage pattern(const QSize &size) {
 QImage image(size,QImage::Format_RGB32);
 for(int y=0;y<size.height();++y)for(int x=0;x<size.width();++x)
  image.setPixel(x,y,qRgb((x*3+y*7)%256,(x*5+y*2)%256,(x*7+y*3)%256));
 return image;
}
// Independent display oracle: integer channel registration with edge extension,
// followed by a circular horizontal roll. Free-running mode has no auto phase.
static QImage adjust(const QImage &image,int phase,int red,int blue) {
 QImage out(image.size(),QImage::Format_RGB32);out.fill(Qt::black);
 for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x){
  const QRgb r=image.pixel(qBound(0,x-red,image.width()-1),y),g=image.pixel(x,y),b=image.pixel(qBound(0,x-blue,image.width()-1),y);
  out.setPixel((x+phase%image.width()+image.width())%image.width(),y,qRgb(qRed(r),qGreen(g),qBlue(b)));
 }
 return out;
}
int main(int argc,char **argv){QCoreApplication app(argc,argv);try {
 int cases=0;double worstMs=0;
 for(const auto &mode:SstvDecoder::availableModeNames())for(bool sync:{false,true}){
  const int rate=12000;const QImage source=pattern(SstvDecoder::imageSizeForMode(mode));
  SstvTransmitter tx(source,mode,rate);SstvDecoder rx,reference;
  rx.setModeName(mode);reference.setModeName(mode);rx.setAutoVisEnabled(sync);reference.setAutoVisEnabled(sync);
  rx.setAutoSyncEnabled(sync);reference.setAutoSyncEnabled(sync);
  int completions=0;QObject::connect(&rx,&SstvDecoder::imageCompleted,[&](const QImage &,const QString &){++completions;});
  const int h=13,r=3,b=-4;
  reference.setHorizontalShiftPixels(h);reference.setColorShiftPixels(r,b);
  int blocks=0;bool partialTested=false;
  // Strip the header for the manual/free-running case; it starts on the first
  // image sample, exactly as a controlled no-VIS recording would.
  if(!sync){QVector<float> header(qRound(.910*rate));tx.generate(header.data(),header.size());}
  while(!tx.isFinished()){
   AudioBlock block;block.sampleRate=rate;block.samples.resize(511);
   block.samples.resize(tx.generate(block.samples.data(),block.samples.size()));
   rx.processAudioBlock(block);reference.processAudioBlock(block);++blocks;
   if(blocks==200){
    const QImage original=rx.currentImage();
    rx.setHorizontalShiftPixels(h);check(rx.currentImage()==adjust(original,h,0,0),"partial-frame horizontal adjustment differs from oracle");
    rx.setColorShiftPixels(r,b);
    if(!sync)check(rx.currentImage()==adjust(original,h,r,b),"partial-frame RGB adjustment differs from oracle");
    check(rx.currentImage()==reference.currentImage(),"old and new rows have inconsistent adjustment");
    rx.setHorizontalShiftPixels(0);rx.setColorShiftPixels(0,0);
    check(rx.currentImage()==original,"partial-frame zero does not restore original");partialTested=true;
   }
  }
  check(partialTested&&completions==1,"frame did not complete exactly once");
  const QImage original=rx.currentImage();
  check(original!=QImage(),"missing received image");
  for(int iteration=0;iteration<12;++iteration){
   QElapsedTimer timer;timer.start();
   rx.setHorizontalShiftPixels(h);rx.setColorShiftPixels(r,b);
   worstMs=qMax(worstMs,timer.nsecsElapsed()/1e6);
   check(rx.currentImage()==reference.currentImage(),"post-completion correction differs from correction applied during reception");
   if(!sync)check(rx.currentImage()==adjust(original,h,r,b),"complete-frame RGB adjustment differs from oracle");
   rx.setColorShiftPixels(-r,-b);rx.setHorizontalShiftPixels(-h);
   if(!sync)check(rx.currentImage()==adjust(original,-h,-r,-b),"negative adjustments differ from oracle");
   rx.setColorShiftPixels(0,0);rx.setHorizontalShiftPixels(0);
   check(rx.currentImage()==original,"adjustments accumulated resampling damage");
  }
  const int width=original.width(),limit=qMax(8,width/3);
  rx.setHorizontalShiftPixels(width);check(rx.currentImage()==original,"full-width phase is not identity");
  rx.setHorizontalShiftPixels(-width);check(rx.currentImage()==original,"negative full-width phase is not identity");
  rx.setHorizontalShiftPixels(width+10);check(rx.horizontalShiftPixels()==width,"phase bounds ignored");rx.setHorizontalShiftPixels(0);
  rx.setColorShiftPixels(width,-width);check(rx.redShiftPixels()==limit && rx.blueShiftPixels()==-limit,"channel bounds ignored");
  if(!sync)check(rx.currentImage()==adjust(original,0,limit,-limit),"channel-limit edge extension differs from oracle");
  rx.setColorShiftPixels(0,0);check(rx.currentImage()==original,"limit adjustments damaged canonical pixels");
  check(completions==1,"display change re-emitted imageCompleted");
  rx.setColorShiftPixels(r,b);rx.setHorizontalShiftPixels(h);
  QByteArray png;QBuffer buffer(&png);buffer.open(QIODevice::WriteOnly);
  check(rx.currentImage().save(&buffer,"PNG"),"corrected PNG export failed");
  check(QImage::fromData(png,"PNG")==reference.currentImage(),"saved image differs from corrected preview");
  rx.reset();QImage blank(rx.currentImage().size(),QImage::Format_RGB32);blank.fill(Qt::black);
  check(rx.currentImage()==blank,"reset retained canonical rows");
  ++cases;std::cout<<mode.toStdString()<<" sync="<<sync<<" PASS\n";
 }
 // Updating only display settings during an in-flight header must preserve
 // VIS acquisition. Reapplying an unchanged VIS flag is part of GUI settings.
 SstvDecoder acquisition;int detected=0;
 QObject::connect(&acquisition,&SstvDecoder::modeDetected,[&](const QString &mode){check(mode=="Robot 36","VIS acquisition mode changed");++detected;});
 SstvTransmitter header(pattern(QSize(320,240)),"Robot 36",12000);
 for(int index=0;index<100;++index){AudioBlock block;block.sampleRate=12000;block.samples.resize(120);header.generate(block.samples.data(),block.samples.size());
  acquisition.processAudioBlock(block);acquisition.setAutoVisEnabled(true);acquisition.setAutoSyncEnabled(true);
  acquisition.setModeName(acquisition.currentModeName());acquisition.setHorizontalShiftPixels(index%2);acquisition.setColorShiftPixels(index%2,-(index%2));}
 check(detected==1,"display settings reset VIS acquisition");
 std::cout<<"PASS "<<cases<<" complete/progressive cases; 12 reversible cycles each; maximum two-adjustment time "<<worstMs<<" ms\n";
 }catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}return 0;}
