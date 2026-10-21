// SPDX-License-Identifier: GPL-3.0-or-later
// App-linked -fno-access-control probe, independent known FM source.
#include "modems/weatherfax/WeatherFaxDecoder.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QElapsedTimer>
#include <random>
#include <cmath>
#include <iostream>
static double gray(double t,const QString &pattern){
 if(pattern=="steps"){const double levels[]={0,64,128,192,255};return levels[int(t/.2)%5];}
 if(pattern=="ramp"){double x=std::fmod(t,1.0);return 255*(x<.5?2*x:2-2*x);}
 if(pattern=="detail")return int(t*1600/4)%2?255:0;
 return 128;
}
int main(int argc,char**argv){QCoreApplication app(argc,argv);
 for(int rate:{44100,48000,96000})for(QString pattern:{"steps","ramp","detail"})for(double snr:{60.,15.,5.}){
  WeatherFaxDecoder rx;rx.m_sampleRate=rate;rx.updateTiming();rx.resetDemodulatorState();rx.resetInputBandpassState();rx.setAutoToneTrackingEnabled(false);
  const int count=rate*2;std::mt19937 random(177);std::normal_distribution<double> noise(0,1);double phase=0;
  QElapsedTimer clock;clock.start();
  for(int n=0;n<count;++n){const double f=1500+800*gray(n/double(rate),pattern)/255.;phase+=6.283185307179586*f/rate;
   const double sample=.25*std::sin(phase)+.25/std::sqrt(2.)*std::pow(10.,-snr/20)*noise(random);
   rx.appendDemodulatedSample(rx.filterInputSample(sample));}
  double best=1e99;int lag=0;
  // Compare pixel-bin means, with a fixed global filter-delay alignment.
  for(int delay=0;delay<=int(rate*.004);delay+=qMax(1,rate/48000)){
   double error=0;int total=0;
   for(int pixel=160;pixel<3000;++pixel){int a=int(pixel*rate/1600.)+delay,b=int((pixel+1)*rate/1600.)+delay;if(b>=count)break;double mean=0;for(int i=a;i<b;++i)mean+=rx.m_frequencyBuffer[i];mean/=b-a;
    double target=0;for(int i=a-delay;i<b-delay;++i)target+=1500+800*gray(i/double(rate),pattern)/255.;target/=b-a;
    error+=(mean-target)*(mean-target);++total;}
   error=std::sqrt(error/total);if(error<best){best=error;lag=delay;}}
  std::cout<<QJsonDocument(QJsonObject{{"rate",rate},{"pattern",pattern},{"snr_db",snr},{"pixel_rmse_hz",best},{"lag_ms",lag*1000./rate},{"elapsed_ms",clock.elapsed()}}).toJson(QJsonDocument::Compact).constData()<<std::endl;
 }
}
