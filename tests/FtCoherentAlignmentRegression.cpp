// SPDX-License-Identifier: GPL-3.0-or-later
#include "modems/ft8/FtCoherentAlignment.h"
#include <random>
#include <iostream>
#include <stdexcept>
int main(){int cases=0;constexpr double pi2=6.2831853071795864769;constexpr int c[7]={3,1,4,0,6,5,2};std::mt19937 rng(1714);std::normal_distribution<double> noise(0.,.05);
 for(int shift:{-5,0,4})for(double df:{-1.75,-.875,-.25,.625,1.5,2.25}){
  std::vector<std::complex<double>> x(79*32+16);for(auto &v:x)v={noise(rng),noise(rng)};
  for(int symbol=0;symbol<79;++symbol){int tone=rng()%8;if(symbol<7)tone=c[symbol];if(symbol>=36&&symbol<43)tone=c[symbol-36];if(symbol>=72)tone=c[symbol-72];for(int n=0;n<32;++n){int i=8+shift+symbol*32+n;if(i>=0&&i<int(x.size())){double phase=.43+pi2*(tone*n/32.+df*i/200.);x[i]+=std::complex<double>(std::cos(phase),std::sin(phase));}}}
  auto r=MadModemFt::alignFt8Coherent(x,8,-8,8);
  if(std::abs(r.shift-shift)>1||std::abs(r.frequencyHz-df)>.14){std::cerr<<shift<<" "<<df<<" -> "<<r.shift<<" "<<r.frequencyHz<<"\n";throw std::runtime_error("Costas timing/frequency refinement");}++cases;
 }
 std::cout<<cases<<" independent complex FT8 timing/frequency cases PASS\n";
}
