// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
namespace MadModemFt {
// Exactly the normalized min-sum check used by the receiver, evaluated in
// linear rather than quadratic work per row. Ties and zero signs are retained.
inline void normalizedMinSumCheck(const double *input,int degree,double *output)
{
 double first=1.e9,second=1.e9;int firstIndex=-1,totalSign=1;
 for(int k=0;k<degree;++k){
  if(input[k]<0)totalSign=-totalSign;
  const double magnitude=std::abs(input[k]);
  if(magnitude<first){second=first;first=magnitude;firstIndex=k;}
  else if(magnitude<second)second=magnitude;
 }
 for(int k=0;k<degree;++k){
  const int sign=input[k]<0?-totalSign:totalSign;
  const double magnitude=k==firstIndex?second:first;
  output[k]=std::max(-20.,std::min(20.,.78*static_cast<double>(sign)*magnitude));
 }
}
}
