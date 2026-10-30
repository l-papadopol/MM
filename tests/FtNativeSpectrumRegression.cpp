// SPDX-License-Identifier: GPL-3.0-or-later
#include "modems/ft8/FtNativeSpectrum.h"
#include <future>
#include <iostream>
int main(){constexpr double pi2=6.2831853071795864769;double worst=0;
 for(int tone=0;tone<8;++tone){std::vector<double>x(192000);for(int i=0;i<int(x.size());++i)x[i]=std::cos(pi2*(1000.+6.25*tone)*i/12000.+.31);MadModemFt::FtNativeSpectrum f(x.data(),x.size());
  auto run=[&f,tone]{double hz=1000.;std::vector<std::complex<double>>z;f.extract(hz,z);double e=0;for(int k=0;k<int(z.size());++k){double phase=.31+pi2*6.25*tone*k/200.;auto expected=std::sqrt(60.)*.5*std::complex<double>(std::cos(phase),std::sin(phase));e=std::max(e,std::abs(z[k]-expected));}return e;};
  auto concurrent=std::async(std::launch::async,run);worst=std::max(worst,run());worst=std::max(worst,concurrent.get());
 }
 if(worst>1.e-8)throw std::runtime_error("native spectral translation/FFT phase or amplitude");
 {
  MadModemFt::FtNativeSpectrumScope outer(nullptr,123);
  {MadModemFt::FtNativeSpectrumScope inner(nullptr,456);
   if(MadModemFt::nativeFt8StopUtcMs!=456)throw std::runtime_error("native cutoff scope not entered");}
  if(MadModemFt::nativeFt8StopUtcMs!=123)throw std::runtime_error("native cutoff outer scope lost");
 }
 if(MadModemFt::nativeFt8Spectrum||MadModemFt::nativeFt8StopUtcMs)throw std::runtime_error("native context leaked to next job");
 std::cout<<"8 real-tone translations, concurrent inverse workspaces PASS; max error="<<worst<<"\n";
}
