// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <vector>

// Learns the sample clock from capture observations, never from decoder queue
// delay. One lower-envelope point per second, robust slopes over >=10 seconds.
// The continuous mapping slews towards that model; it never drops PCM frames.
class CaptureClockTracker {
public:
    struct Span { int64_t mono=0, utc=0, duration=0; };
    void reset() { *this=CaptureClockTracker(); }
    bool initialized() const { return m_initialized; }
    bool locked() const { return m_locked; }
    double measuredRate() const { return m_nominal/m_slope; }
    double effectiveRate() const { return m_nominal/m_periodScale; }
    double residualMs() const { return m_residual*1000.; }
    // True means a new epoch (wall-clock step or explicit calibration change).
    bool observe(int64_t frames, int64_t mono, int64_t utc, int nominal, bool automatic=true) {
        if(nominal<=0 || frames<m_emitted)return false;
        bool discontinuity=m_initialized && (nominal!=m_nominal || automatic!=m_auto ||
            std::abs((utc-mono)-m_utcOffset)>250000000LL);
        if(!m_initialized || discontinuity) {
            m_nominal=nominal;m_auto=automatic;m_initialized=true;m_locked=false;
            m_slope=1;m_intercept=0;m_periodScale=1;m_residual=0;m_points.clear();m_bin=-1;
            m_baseFrame=m_emitted;m_anchorMono=mono-std::llround(1e9*(frames-m_emitted)/double(nominal));
            m_nextMono=m_anchorMono;m_utcOffset=utc-mono;
        }
        const double x=double(frames-m_baseFrame)/m_nominal;
        const double y=double(mono-m_anchorMono)/1e9;
        const int64_t bin=int64_t(std::floor(y));
        if(m_auto && bin>m_bin) {
            if(m_points.size()>=12 && m_points.back().x-m_points.front().x>=15.)fit();
            m_points.push_back({x,y});m_bin=bin;
            while(!m_points.empty() && y-m_points.front().y>60.)m_points.pop_front();
        } else if(m_auto && !m_points.empty() && bin==m_bin && y-x<m_points.back().y-m_points.back().x) {
            m_points.back()={x,y};
        }
        return discontinuity;
    }
    Span take(int frames) {
        if(!m_initialized || frames<=0)return {};
        const int64_t first=std::llround(m_nextMono);
        if(m_locked) {
            const double target=double(m_anchorMono)/1e9+m_intercept+
                m_slope*double(m_emitted+frames-m_baseFrame)/m_nominal;
            const double predicted=double(m_nextMono)/1e9+m_slope*frames/m_nominal;
            m_residual=target-predicted;
            // Twenty-second phase servo, capped at 2000 ppm. Capture delivery
            // jitter cannot create timestamp jumps or reset the DSP filters.
            const double desired=std::clamp(m_slope+std::clamp(m_residual/20.,-.002,.002),.993,1.007);
            const double maximumChange=.0002*frames/m_nominal; // 200 ppm/second
            m_periodScale+=std::clamp(desired-m_periodScale,-maximumChange,maximumChange);
        }
        m_nextMono+=1e9L*frames*m_periodScale/m_nominal;m_emitted+=frames;
        const int64_t end=std::llround(m_nextMono);
        return {first,first+m_utcOffset,end-first};
    }
private:
    struct Point { double x,y; };
    static double median(std::vector<double> &v) {
        auto middle=v.begin()+v.size()/2;std::nth_element(v.begin(),middle,v.end());return *middle;
    }
    void fit() {
        std::vector<double> slopes;
        for(size_t i=0;i<m_points.size();++i)for(size_t j=i+1;j<m_points.size();++j) {
            const double dx=m_points[j].x-m_points[i].x;
            if(dx>=10.)slopes.push_back((m_points[j].y-m_points[i].y)/dx);
        }
        if(slopes.empty())return;
        const double slope=median(slopes);
        if(!std::isfinite(slope) || slope<.995 || slope>1.005)return;
        std::vector<double> offsets;for(auto p:m_points)offsets.push_back(p.y-slope*p.x);
        m_slope=slope;m_intercept=median(offsets);m_locked=true;
    }
    bool m_initialized=false,m_locked=false,m_auto=true;
    double m_nominal=48000,m_slope=1,m_intercept=0,m_periodScale=1,m_residual=0;
    int64_t m_emitted=0,m_baseFrame=0,m_anchorMono=0,m_utcOffset=0,m_bin=-1;
    long double m_nextMono=0;
    std::deque<Point> m_points;
};
