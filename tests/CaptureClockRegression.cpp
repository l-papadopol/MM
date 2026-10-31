// SPDX-License-Identifier: GPL-3.0-or-later
#include "audio/CaptureClockTracker.h"
#include "dsp/common/TimestampResampler.h"
#include <iostream>
#include <random>
#include <stdexcept>
static void check(bool b,const char *why){if(!b)throw std::runtime_error(why);}
int main(){
 const int64_t utc=1792645200000000000LL;
 for(int nominal:{44100,48000,96000})for(double ppm:{-3000.,-1378.94,0.,1378.94,3000.}){
  CaptureClockTracker clock;std::mt19937 rng(819);int64_t read=0,emitted=0,lastEnd=0;
  double worst=0;int spans=0;int64_t lastArrival=0;
  for(int call=0;read<nominal*900LL;++call){
   // Vary read sizes; occasional delayed delivery is followed by a burst.
   int frames=call%997==0?4096:819;read+=frames;
   const double rate=nominal*(1+ppm/1e6);
   const double delay=.002+double(rng()%4000)/1e6+(call%997==0?.18:0.);
   const int64_t mono=std::max<int64_t>(lastArrival,std::llround((read/rate+delay)*1e9));
   lastArrival=mono;
   clock.observe(read,mono,utc+mono,nominal);
   while(read-emitted>=1024){auto timing=clock.take(1024);
    if(spans)check(timing.mono==lastEnd,"capture span discontinuity");
    lastEnd=timing.mono+timing.duration;emitted+=1024;++spans;
    if(emitted/rate>180.)worst=std::max(worst,std::abs(lastEnd/1e9-emitted/rate));
   }
  }
  const double errorPpm=(clock.measuredRate()/(nominal*(1+ppm/1e6))-1)*1e6;
  std::cout<<"nominal="<<nominal<<" ppm="<<ppm<<" measured="<<clock.measuredRate()<<" error_ppm="<<errorPpm<<" max_time_error_ms="<<worst*1000<<" spans="<<spans<<std::endl;
  check(clock.locked()&&std::abs(errorPpm)<120,"clock frequency not learned");
  check(worst<.025,"clock accumulated time error");
 }
 // Wall-clock steps create a new epoch, not a sample-rate correction.
 CaptureClockTracker clock;for(int i=1;i<2000;++i)clock.observe(i*819,int64_t(i)*17062500,utc+int64_t(i)*17062500,48000);
 check(clock.observe(2000*819,34125000000LL,utc+34125000000LL+1000000000LL,48000),"UTC step not detected");
 check(!clock.locked(),"UTC step contaminated frequency model");
 clock.reset();check(!clock.initialized(),"capture restart did not reset tracker");
 // Explicit manual calibration keeps its chosen mapping and never learns it away.
 clock.reset();int64_t manualEnd=0;
 for(int i=1;i<=2000;++i){clock.observe(i*1024,int64_t(i)*21350000,utc+int64_t(i)*21350000,48000,false);auto span=clock.take(1024);manualEnd=span.mono+span.duration;}
 check(!clock.locked() && clock.effectiveRate()==48000,"manual calibration overridden");
 TimestampResampler gap;QVector<double> flat(1024,1.);qint64 first=0;
 gap.process(flat,utc,21333333,&first);
 const auto afterGap=gap.process(flat,utc+1000000000LL,21333333,&first);
 check(afterGap.size()<260 && first==utc+1000000000LL,"resampler synthesized audio across missing input");
 // Timestamp resampling: fifteen minutes of mismatched audio clock must still
 // produce one contiguous 12-kHz grid. Check tone frequency in its last 10 s.
 for(double ppm:{-1378.94,1378.94}){
  TimestampResampler resampler;const double actual=48000*(1+ppm/1e6);
  int64_t frame=0,outputCount=0,anchor=0,lastOut=-1;int crossings=0;
  double prev=0,firstCross=0,lastCross=0;const double twoPi=6.283185307179586;
  for(int k=0;k<35000;++k){QVector<double> samples(1024);
   for(int n=0;n<samples.size();++n)samples[n]=std::sin(twoPi*1000*(frame+n)/actual);
   const int64_t start=utc+std::llround(frame/actual*1e9);frame+=1024;
   const int64_t end=utc+std::llround(frame/actual*1e9);qint64 first=0;
   auto out=resampler.process(samples,start,end-start,&first);
   if(!out.isEmpty()){
    if(!anchor)anchor=first;
    check(std::abs(first-(anchor+std::llround(outputCount*1e9/12000.)))<=1,"output 12k UTC grid discontinuity");
    for(double x:out){
     if(outputCount>12000LL*730 && prev<0 && x>=0){
      const double where=outputCount-1+(-prev)/(x-prev);
      if(!crossings)firstCross=where;lastCross=where;++crossings;
     }
     prev=x;++outputCount;
    }
    lastOut=first;
   }
  }
  const double freq=(crossings-1)*12000./(lastCross-firstCross);
  std::cout<<"resampler ppm="<<ppm<<" output="<<outputCount<<" frequency="<<freq<<std::endl;
  check(std::abs(freq-1000)<.03,"timestamp resampler tone frequency changed");
  check(std::abs(outputCount-frame/actual*12000)<2,"timestamp resampler duration changed");
 }
 std::cout<<"Capture clock and long-duration UTC resampling PASS\n";
}
