// SPDX-License-Identifier: GPL-3.0-or-later
// Build with private-access probe flags, as for the other native DSP probes.
#include "modems/ft8/Ft8RxDecoder.h"
#include "dsp/gpu/FtSyncCompute.h"
#include <QCoreApplication>
#include <QThread>
#include <QElapsedTimer>
#include <random>
#include <iostream>
#include <stdexcept>
using namespace FtSyncCompute;
static void check(bool b,const char *why){if(!b)throw std::runtime_error(why);}
int main(int argc,char **argv){
 QCoreApplication app(argc,argv);Engine engine(true);QElapsedTimer wait;wait.start();
 while(!engine.status().ready&&!engine.status().unavailable&&wait.elapsed()<20000)QThread::msleep(5);
 check(engine.status().ready,"OpenCL CPU test runtime unavailable");
 Ft8RxDecoder rx;std::cout<<"OpenCL device="<<engine.status().device.toStdString()<<" GPU="<<engine.status().gpuDevice<<std::endl;
 for(bool ft4:{false,true})for(int scenario=0;scenario<4;++scenario){
  const int window=ft4?576:1920,tones=ft4?4:8,symbols=ft4?103:79;
  auto in=std::make_shared<Input>();in->toneEnergies=true;in->symbolSamples=window;
  in->rate=12000;in->lowHz=scenario==3?4500.:1000.37;in->stepHz=12000./window;in->frequencies=tones;
  in->tones.resize(symbols,0);QVector<double> samples(symbols*window+1);
  std::mt19937 rng(420+scenario);std::normal_distribution<double> noise(0,.01);
  for(int n=0;n<samples.size();++n){const int tone=(n/window)%tones;
   samples[n]=scenario==0?0.:noise(rng)+(scenario==1?0.:.1*std::cos(6.283185307179586*(in->lowHz+tone*in->stepHz)*n/12000.));
   in->samples.emplace_back(samples[n],0.);
  }
  for(int k=0;k<symbols;++k)in->starts.push_back(k*window);
  QElapsedTimer cpu;cpu.start();std::vector<double> native(size_t(symbols)*tones);
  for(int k=0;k<symbols;++k){
   if(ft4)for(int tone=0;tone<tones;++tone)native[size_t(k)*tones+tone]=rx.ft4SymbolToneEnergy(samples,k*window,in->lowHz+tone*in->stepHz,window);
   else {const auto e=rx.symbolToneEnergies8(samples,k*window,in->lowHz);std::copy(e.begin(),e.end(),native.begin()+size_t(k)*tones);}
  }
  const double cpuMs=cpu.nsecsElapsed()/1e6;
  std::vector<double> ref(in->cells());cpuTiming(*in,0,ref.data());double refError=0;
  check(equivalent(native,ref,&refError),"native SIMD/Goertzel reference mismatch");
  std::shared_future<Result> future;
  for(int i=0;i<100&&!future.valid();++i){future=engine.submit(in);if(!future.valid())QThread::msleep(1);}
  check(future.valid(),"OpenCL native batch rejected");
  check(future.wait_for(std::chrono::seconds(20))==std::future_status::ready,"OpenCL native timeout");
  auto result=future.get();double error=0;check(result.ok&&equivalent(native,result.scores,&error),"native/OpenCL energy mismatch");
  std::cout<<"mode="<<(ft4?"FT4":"FT8")<<" case="<<scenario<<" cells="<<native.size()<<" native_cpu_ms="<<cpuMs<<" opencl_cpu_ms="<<result.totalMs<<" reference_error="<<refError<<" device_error="<<error<<std::endl;
 }
 std::cout<<"Native SIMD/scalar/OpenCL tone energy parity PASS\n";
}
