// SPDX-License-Identifier: GPL-3.0-or-later
#include "modems/ft8/FtSignalEnvelope.h"
#include <iostream>
#include <random>
#include <stdexcept>
int main(){
 std::mt19937 rng(12519);std::normal_distribution<double> noise(0.,1.);
 double worst=0.;int cases=0;
 for(int n:{1,2,17,79,4096,8100})for(int length:{2,4,30,1400,4000})for(bool edge:{false,true})for(bool clipped:{false,true}){
  std::vector<std::complex<double>> x(n);for(auto&v:x)v={noise(rng),noise(rng)};auto y=x;
  int lo=clipped?n/5:0,hi=clipped?n-n/7:n;
  MadModemFt::raisedCosineEnvelope(y,length,edge,lo,hi);
  for(int k=0;k<n;++k){std::complex<double> sum{};double mass=0.;for(int j=std::max(lo,k-length/2);j<std::min(hi,k+length/2+1);++j){double w=.5+.5*std::cos(6.2831853071795864769*(k-j)/length);sum+=w*x[j];mass+=w;}auto expected=(edge?mass:double(length)/2)>1.e-12?sum/(edge?mass:double(length)/2):std::complex<double>{};if(k<lo||k>=hi)expected={};double error=std::abs(y[k]-expected);worst=std::max(worst,error);if(error>1.e-9){std::cerr<<"n="<<n<<" L="<<length<<" edge="<<edge<<" clipped="<<clipped<<" k="<<k<<" got="<<y[k]<<" expected="<<expected<<" error="<<error<<" mass="<<mass<<"\n";throw std::runtime_error("sliding FIR differs from independent direct convolution");}}++cases;
 }
 for(bool edge:{false,true}){std::vector<std::complex<double>> x(9000,{.3,-.2});MadModemFt::raisedCosineEnvelope(x,4000,edge);for(int k=edge?0:2000;k<(edge?9000:7000);++k)if(std::abs(x[k]-std::complex<double>(.3,-.2))>1.e-10)throw std::runtime_error("constant-envelope gain");}
 std::cout<<cases<<" FIR/direct cases PASS; max error="<<worst<<"\n";
}
