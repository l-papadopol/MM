// SPDX-License-Identifier: GPL-3.0-or-later
#include "modems/ft8/FtLdpcCheck.h"
#include <array>
#include <cstring>
#include <random>
#include <stdexcept>
#include <iostream>
int main(){std::mt19937 rng(51319);std::uniform_real_distribution<double>d(-20,20);int cases=0;
 for(int degree=2;degree<=7;++degree)for(int trial=0;trial<10000;++trial){
  std::array<double,7>x{},fast{},slow{};for(auto&v:x)v=trial%3?d(rng):double(int(rng()%9)-4);if(trial%7==0)x[0]=0.;if(trial%11==0)x[1]=-0.;
  for(int e=0;e<degree;++e){double minimum=1.e9;int sign=1;for(int k=0;k<degree;++k)if(k!=e){if(x[k]<0)sign=-sign;minimum=std::min(minimum,std::abs(x[k]));}slow[e]=std::max(-20.,std::min(20.,.78*double(sign)*minimum));}
  MadModemFt::normalizedMinSumCheck(x.data(),degree,fast.data());
  if(std::memcmp(fast.data(),slow.data(),degree*sizeof(double))!=0)throw std::runtime_error("min-sum row differs bit-for-bit, including ties and signed zero");++cases;
 }
 std::cout<<cases<<" min-sum rows identical bit-for-bit to independent quadratic reference PASS\n";
}
