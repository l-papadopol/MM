// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <vector>
namespace MadModemFt {
struct Ft8CoherentAlignment {int shift=0;double frequencyHz=0,score=0;};
// FT8 baseband is sampled at 200 Hz (32 samples per symbol). Search the
// synchronization tones only; data bits and expected messages are not inputs.
inline Ft8CoherentAlignment alignFt8Coherent(std::vector<std::complex<double>> &samples,
                                            int margin,int lowShift,int highShift)
{
 constexpr double pi2=6.283185307179586476925286766559;
 static const auto tones=[] {std::array<std::array<std::complex<double>,32>,8> b{};for(int t=0;t<8;++t)for(int n=0;n<32;++n){double a=-pi2*t*n/32.;b[t][n]={std::cos(a),std::sin(a)};}return b;}();
 constexpr int costas[7]={3,1,4,0,6,5,2},blocks[3]={0,36,72};
 lowShift=std::max(-margin,lowShift);highShift=std::min(margin,highShift);
 auto score=[&](int shift,double hz){
  std::array<std::complex<double>,32> rot{};double a=-pi2*hz/200.;std::complex<double> step(std::cos(a),std::sin(a)),phase(1,0);
  for(auto &x:rot){x=phase;phase*=step;}
  double sum=0.;for(int block:blocks)for(int k=0;k<7;++k){int begin=margin+shift+(block+k)*32;std::complex<double> c{};if(begin<0||begin+32>int(samples.size()))continue;for(int n=0;n<32;++n)c+=samples[begin+n]*tones[costas[k]][n]*rot[n];sum+=std::norm(c);}return sum;
 };
 Ft8CoherentAlignment best;best.score=-1;
 for(int shift=lowShift;shift<=highShift;++shift){double s=score(shift,0);if(s>best.score)best={shift,0,s};}
 for(int k=-10;k<=10;++k){double hz=.25*k,s=score(best.shift,hz);if(s>best.score){best.frequencyHz=hz;best.score=s;}}
 const int center=best.shift;
 for(int shift=std::max(lowShift,center-4);shift<=std::min(highShift,center+4);++shift){double s=score(shift,best.frequencyHz);if(s>best.score){best.shift=shift;best.score=s;}}
 double a=-pi2*best.frequencyHz/200.;std::complex<double> step(std::cos(a),std::sin(a)),phase(1,0);
 for(auto &x:samples){x*=phase;phase*=step;}
 return best;
}
}
