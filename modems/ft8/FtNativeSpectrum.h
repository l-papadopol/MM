// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "../../dsp/FftwPlannerLock.h"
#include "../../third_party/mshv_gpl/port/Hv_Lib_fftw/fftw3.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>
namespace MadModemFt {
// Native C++ implementation of the FT8 spectral downconversion described by
// WSJT-X 3.0.2 ft8_downsample.f90. No decoder engine is called here.
// One immutable 16 s spectrum is shared by candidate workers for ONE residual.
class FtNativeSpectrum {
 static constexpr int inputSize=192000,outputSize=3200;
 std::vector<std::complex<double>> bins;
 struct RealFree {void operator()(double*p)const{fftw_free(p);}};
 struct ComplexFree {void operator()(fftw_complex*p)const{fftw_free(p);}};
 struct Inverse {
  std::unique_ptr<fftw_complex,ComplexFree> data{fftw_alloc_complex(outputSize)};
  fftw_plan plan=nullptr;
  Inverse(){if(!data)throw std::bad_alloc();std::lock_guard<std::mutex> lock(MadModemDsp::fftwPlannerMutex);plan=fftw_plan_dft_1d(outputSize,data.get(),data.get(),FFTW_BACKWARD,FFTW_ESTIMATE);if(!plan)throw std::runtime_error("FT8 inverse FFT plan");}
  ~Inverse(){std::lock_guard<std::mutex> lock(MadModemDsp::fftwPlannerMutex);fftw_destroy_plan(plan);}
 };
public:
 FtNativeSpectrum(const double*samples,int count):bins(inputSize/2+1){
  std::unique_ptr<double,RealFree> input(fftw_alloc_real(inputSize));
  std::unique_ptr<fftw_complex,ComplexFree> output(fftw_alloc_complex(inputSize/2+1));
  if(!input||!output)throw std::bad_alloc();count=std::max(0,std::min(inputSize,count));
  std::copy_n(samples,count,input.get());std::fill(input.get()+count,input.get()+inputSize,0.);
  fftw_plan plan;{std::lock_guard<std::mutex> lock(MadModemDsp::fftwPlannerMutex);plan=fftw_plan_dft_r2c_1d(inputSize,input.get(),output.get(),FFTW_ESTIMATE);}
  if(!plan)throw std::runtime_error("FT8 forward FFT plan");
  fftw_execute(plan);{std::lock_guard<std::mutex> lock(MadModemDsp::fftwPlannerMutex);fftw_destroy_plan(plan);}
  for(int k=0;k<=inputSize/2;++k)bins[k]={output.get()[k][0],output.get()[k][1]};
 }
 void extract(double &frequency,std::vector<std::complex<double>>&out)const{
  if(!std::isfinite(frequency)||frequency<50||frequency>5000)throw std::runtime_error("FT8 baseband frequency");
  thread_local Inverse inverse;
  constexpr double df=12000./inputSize,baud=6.25,pi=3.14159265358979323846;
  int center=int(std::lround(frequency/df)),low=std::max(1,int(std::lround((frequency-1.5*baud)/df))),high=std::min(inputSize/2,int(std::lround((frequency+8.5*baud)/df)));
  frequency=center*df;auto data=inverse.data.get();
  for(int k=0;k<outputSize;++k)data[k][0]=data[k][1]=0.;
  for(int k=low;k<=high;++k){
   double taper=1.;if(k-low<100)taper*=.5*(1.-std::cos(pi*(k-low)/100.));
   if(high-k<100)taper*=.5*(1.-std::cos(pi*(high-k)/100.));
   int dest=(k-center+outputSize)%outputSize;data[dest][0]=taper*bins[k].real();data[dest][1]=taper*bins[k].imag();
  }
  fftw_execute(inverse.plan);out.resize(outputSize);const double scale=1./std::sqrt(double(inputSize)*outputSize);
  for(int k=0;k<outputSize;++k)out[k]={data[k][0]*scale,data[k][1]*scale};
 }
};
inline thread_local const FtNativeSpectrum *nativeFt8Spectrum=nullptr;
inline thread_local std::int64_t nativeFt8StopUtcMs=0;
struct FtNativeSpectrumScope {
 const FtNativeSpectrum *previous;
 std::int64_t previousStop;
 explicit FtNativeSpectrumScope(const FtNativeSpectrum*p,std::int64_t stop=0):previous(nativeFt8Spectrum),previousStop(nativeFt8StopUtcMs){nativeFt8Spectrum=p;nativeFt8StopUtcMs=stop;}
 ~FtNativeSpectrumScope(){nativeFt8Spectrum=previous;nativeFt8StopUtcMs=previousStop;}
 FtNativeSpectrumScope(const FtNativeSpectrumScope&)=delete;
 FtNativeSpectrumScope&operator=(const FtNativeSpectrumScope&)=delete;
};
}
