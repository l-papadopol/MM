// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace madmodem::cwskimmer {

// Acoustic-only bounded CTC search. No vocabulary, callsign or language prior.
// Graves et al., ICML 2006: prefix alignment sums and blank section boundaries.
class CwCtcPrefixDecoder {
public:
    CwCtcPrefixDecoder() { reset(); }
    void reset() { m_beams={{"",1.,0.}}; m_last=0; m_blankRun=0; }

    std::vector<int> consume(const std::array<float,42>& logits) {
        std::array<double,42> probability{};
        const double maximum=*std::max_element(logits.begin(),logits.end());
        double sum=0.;
        for (int j=0;j<42;++j) sum+=probability[j]=std::exp(double(logits[j])-maximum);
        for (double& value:probability) value/=sum;
        std::array<int,41> labels{};
        for (int j=0;j<41;++j) labels[j]=j+1;
        std::partial_sort(labels.begin(),labels.begin()+6,labels.end(),[&](int a,int b) {
            return probability[a]==probability[b] ? a<b : probability[a]>probability[b];
        });
        std::map<std::string,std::array<double,2>> next;
        constexpr double penalty=0.22313016014842982; // exp(-1.5), per new token
        for (const auto& beam:m_beams) {
            const double total=beam.blank+beam.nonblank;
            next[beam.prefix][0]+=total*probability[0];
            const int previous=beam.prefix.empty() ? m_last : static_cast<unsigned char>(beam.prefix.back());
            for (int k=0;k<6;++k) {
                const int label=labels[k];
                const double value=probability[label];
                if (value<1e-6) continue;
                const std::string extension=beam.prefix+static_cast<char>(label);
                if (previous==label) {
                    next[beam.prefix][1]+=beam.nonblank*value;
                    next[extension][1]+=beam.blank*value*penalty;
                } else next[extension][1]+=total*value*penalty;
            }
        }
        m_beams.clear();
        for (const auto& item:next) m_beams.push_back({item.first,item.second[0],item.second[1]});
        std::sort(m_beams.begin(),m_beams.end(),[](const Beam& a,const Beam& b) {
            const double x=a.blank+a.nonblank,y=b.blank+b.nonblank;
            return x==y ? a.prefix<b.prefix : x>y;
        });
        if (m_beams.size()>4) m_beams.resize(4);
        const double scale=m_beams.front().blank+m_beams.front().nonblank;
        std::size_t common=m_beams.front().prefix.size();
        for (auto& beam:m_beams) {
            beam.blank/=scale; beam.nonblank/=scale;
            common=std::min(common,beam.prefix.size());
            std::size_t j=0;
            while (j<common && beam.prefix[j]==m_beams.front().prefix[j]) ++j;
            common=j;
        }
        std::vector<int> emitted;
        if (common) {
            append(m_beams.front().prefix.substr(0,common),emitted);
            for (auto& beam:m_beams) beam.prefix.erase(0,common);
        }
        m_blankRun=probability[0]>=.99 ? m_blankRun+1 : 0;
        const bool bound=std::any_of(m_beams.begin(),m_beams.end(),[](const Beam& b){return b.prefix.size()>=64;});
        // A confident blank section closes the remaining ambiguous suffix
        // causally. Future frames never revise text already emitted.
        if (m_blankRun>=4 || bound) {
            append(m_beams.front().prefix,emitted);
            m_beams={{"",1.,0.}}; m_blankRun=0;
        }
        return emitted;
    }
private:
    struct Beam { std::string prefix; double blank=0.,nonblank=0.; };
    void append(const std::string& prefix,std::vector<int>& output) {
        for (unsigned char label:prefix) { output.push_back(label); m_last=label; }
    }
    std::vector<Beam> m_beams;
    int m_last=0,m_blankRun=0;
};
}
