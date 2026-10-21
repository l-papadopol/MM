// SPDX-License-Identifier: GPL-3.0-or-later
#include "dsp/gpu/FtSyncCompute.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>
#include <stdexcept>
using namespace FtSyncCompute;
static void check(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
int main(int argc,char **argv){
 QCoreApplication app(argc,argv);bool cpuTest=argc>1 && QString(argv[1])=="--opencl-cpu";
 Engine engine(cpuTest);QElapsedTimer wait;wait.start();
 while(!engine.status().ready&&!engine.status().unavailable&&wait.elapsed()<20000)QThread::msleep(5);
 auto status=engine.status();std::cout<<"device="<<status.device.toStdString()<<" gpu="<<status.gpuDevice<<" status="<<status.reason.toStdString()<<std::endl;
 if(cpuTest)check(status.ready,"explicit OpenCL validation runtime unavailable");
 for(bool ft4:{false,true})for(int scenario:{0,1,2}){
  auto input=std::make_shared<Input>();input->rate=1000;input->symbolSamples=ft4?48:160;
  input->frequencies=scenario==2?211:61;input->lowHz=850;input->stepHz=ft4?5.208333333:1.5625;
  input->spacingHz=ft4?20.833333333:6.25;input->mixHz=1055;
  input->samples.resize(ft4?7500:15000);std::mt19937 rng(189+scenario);std::normal_distribution<double> noise(0,.02);
  for(size_t i=0;i<input->samples.size();++i){double phase=6.283185307179586*((scenario==1?70.:3.)*i/1000.+10.*i*i/1e6);input->samples[i]=scenario==0?std::complex<double>(0,0):std::complex<double>(.15*std::cos(phase)+noise(rng),.15*std::sin(phase)+noise(rng));}
  int patterns[21]={3,1,4,0,6,5,2,3,1,4,0,6,5,2,3,1,4,0,6,5,2};
  const int sync=ft4?16:21;for(int k=0;k<sync;++k)input->tones.push_back(ft4?k%4:patterns[k]);
  const int timings=scenario==2?(ft4?35:26):3;
  for(int ti=0;ti<timings;++ti)for(int k=0;k<sync;++k)input->starts.push_back(100+ti*24+k*(ft4?400:600));
  // Include negative/out-of-capture windows and non-multiple workgroup lengths.
  input->starts.front()=-1;input->starts.back()=int(input->samples.size());
  check(input->valid(),"valid test input rejected");std::vector<double> reference(input->cells());
  QElapsedTimer timer;timer.start();for(int ti=0;ti<timings;++ti)cpuTiming(*input,ti,reference.data()+size_t(ti)*sync*input->frequencies);
  double cpuMs=timer.nsecsElapsed()/1e6;
  check(equivalent(reference,reference),"CPU reference invalid");
  if(!status.ready){check(!engine.submit(input).valid(),"unavailable backend accepted work");continue;}
  for(int repeat=0;repeat<2;++repeat){
   std::shared_future<Result> future;for(int attempt=0;attempt<100 && !future.valid();++attempt){future=engine.submit(input);if(!future.valid())QThread::msleep(1);}
   check(future.valid(),"ready device rejected batch");
   check(future.wait_for(std::chrono::seconds(20))==std::future_status::ready,"OpenCL batch timeout");
   const auto &result=future.get();double error=0;
   check(result.ok,"OpenCL execution failed");check(equivalent(reference,result.scores,&error),"OpenCL numerical mismatch");
   std::cout<<"mode="<<(ft4?"FT4":"FT8")<<" case="<<scenario<<" repeat="<<repeat<<" cells="<<reference.size()<<" cpu_serial_ms="<<cpuMs<<" device_total_ms="<<result.totalMs<<" error="<<error<<" gpu="<<result.gpuDevice<<std::endl;
  }
 }
 // Ordinary demodulation: both modes, silence, noise/tones, off-grid and
 // extreme audio frequencies. Repeat to check persistent-buffer reuse.
 for(bool ft4:{false,true})for(int scenario=0;scenario<4;++scenario){
  auto in=std::make_shared<Input>();in->toneEnergies=true;in->rate=12000;
  in->symbolSamples=ft4?576:1920;in->frequencies=ft4?4:8;
  in->lowHz=scenario==3?4500.:1000.37;in->stepHz=12000./in->symbolSamples;
  const int symbols=ft4?103:79;in->tones.resize(symbols,0);
  in->samples.resize(size_t(symbols)*in->symbolSamples);
  std::mt19937 random(420+scenario);std::normal_distribution<double> noise(0,.01);
  for(size_t n=0;n<in->samples.size();++n){
   const int tone=int(n/in->symbolSamples)%in->frequencies;
   const double phase=6.283185307179586*(in->lowHz+tone*in->stepHz)*n/12000.;
   in->samples[n]={scenario==0?0.:noise(random)+(scenario==1?0.:.1*std::cos(phase)),0};
  }
  for(int k=0;k<symbols;++k)in->starts.push_back(k*in->symbolSamples);
  check(in->valid(),"ordinary demodulation input invalid");
  std::vector<double> ref(in->cells());cpuTiming(*in,0,ref.data());
  if(!status.ready){check(!engine.submit(in).valid(),"unavailable ordinary backend accepted work");continue;}
  for(int repeat=0;repeat<2;++repeat){
   std::shared_future<Result> future;
   for(int attempt=0;attempt<100 && !future.valid();++attempt){future=engine.submit(in);if(!future.valid())QThread::msleep(1);}
   check(future.valid(),"ordinary batch rejected");
   check(future.wait_for(std::chrono::seconds(20))==std::future_status::ready,"ordinary batch timeout");
   const auto result=future.get();double error=0;
   check(result.ok,"ordinary OpenCL execution failed");
   check(equivalent(ref,result.scores,&error),"ordinary OpenCL energies mismatch");
   std::cout<<"demod="<<(ft4?"FT4":"FT8")<<" case="<<scenario<<" repeat="<<repeat<<" cells="<<ref.size()<<" device_total_ms="<<result.totalMs<<" error="<<error<<" gpu="<<result.gpuDevice<<std::endl;
  }
 }
 auto bad=std::make_shared<Input>();check(!engine.submit(bad).valid(),"invalid input accepted");
 auto cpu=std::vector<double>{1,2},different=std::vector<double>{1,2.1};check(!equivalent(cpu,different),"mismatch not rejected");
 std::cout<<"GPU sync CPU/reference/validation/fallback PASS"<<std::endl;
}
