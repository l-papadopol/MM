// SPDX-License-Identifier: GPL-3.0-or-later
#include "../modems/ft8/FtCoherentCorrelation.h"
#include <random>
#include <cstring>
#include <chrono>
#include <iostream>
static std::complex<double> original(const std::vector<std::complex<double>>& samples,int begin,int tone) {
 std::complex<double> sum(0,0);constexpr double twoPi=6.283185307179586476925286766559;
 for(int n=0;n<32;++n){int index=begin+n;if(index<0||index>=int(samples.size()))continue;
  double phase=-twoPi*double(tone*n)/32.0;sum+=samples[index]*std::complex<double>(std::cos(phase),std::sin(phase));}
 return sum;
}
int main(){std::mt19937_64 rng(113);std::normal_distribution<double> normal(0,1);
 std::vector<std::complex<double>> samples(3300);for(auto&v:samples)v={normal(rng),normal(rng)};
 for(int begin=-35;begin<3310;++begin)for(int tone=0;tone<8;++tone){
  auto a=original(samples,begin,tone),b=MadModemFt::coherentCorrelation32<8>(samples,begin,tone);
  if(std::memcmp(&a,&b,sizeof(a)))return 1;
  if(tone<4){auto c=MadModemFt::coherentCorrelation32<4>(samples,begin,tone);if(std::memcmp(&a,&c,sizeof(a)))return 2;}
 }
 volatile double sink=0;using Clock=std::chrono::steady_clock;
 for(int mode=0;mode<2;++mode){auto start=Clock::now();for(int i=0;i<200000;++i){int begin=i%3200,tone=i%8;
   sink+= (mode==0?original(samples,begin,tone):MadModemFt::coherentCorrelation32<8>(samples,begin,tone)).real();}
  std::cout<<(mode==0?"original":"cached")<<" correlation_ms="<<std::chrono::duration<double,std::milli>(Clock::now()-start).count()<<"\n";}
 std::cout<<"PASS: 40140 complex correlations bit-identical (FT8/FT4, including buffer edges)\n";
}
