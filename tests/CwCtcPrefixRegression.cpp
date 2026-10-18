// SPDX-License-Identifier: GPL-3.0-or-later
#include "modems/cw/neural/CwCtcPrefixDecoder.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

static std::array<float,42> frame(double blank,double a) {
    std::array<float,42> z; z.fill(-40.f);
    z[0]=std::log(blank); z[2]=std::log(a); return z;
}
static void require(bool ok,const char* why) { if(!ok)throw std::runtime_error(why); }
int main() {
    using madmodem::cwskimmer::CwCtcPrefixDecoder;
    CwCtcPrefixDecoder decoder; std::vector<int> result;
    auto feed=[&](const auto& z) { auto v=decoder.consume(z);result.insert(result.end(),v.begin(),v.end()); };
    // Exhaustive binary three-frame CTC: P(A)=.739584, P(empty)=.140608,
    // P(AA)=.119808. With exp(-1.5) per token, A remains most likely.
    // Every individual frame prefers blank, so best-path would miss A.
    for(int i=0;i<3;++i) feed(frame(.52,.48));
    for(int i=0;i<8;++i) feed(frame(.999999,1e-6));
    require(result==std::vector<int>{2},"prefix alignment probability did not recover A");
    decoder.reset(); result.clear();
    for(int repeat=0;repeat<2;++repeat) {
        feed(frame(.00001,.99999));
        for(int i=0;i<8;++i) feed(frame(.999999,1e-6));
    }
    require(result==std::vector<int>({2,2}),"blank-separated repeated characters were merged");
    decoder.reset();result.clear();feed(frame(.2,.8));
    decoder.reset();result.clear();
    for(int i=0;i<1000;++i) feed(frame(.999999,1e-6));
    require(result.empty(),"reset/blank sections released stale text");
    std::cout << "PASS marginal alignment recovery, blank-separated repeats, reset and quiet sections\n";
}
