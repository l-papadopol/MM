// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QVector>
#include <QtGlobal>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace MadModemFt {
struct AnalysisConfig {
    quint64 epoch = 0;
    qint64 slotUtcMs = 0;
    bool ft4 = false;
    int hop = 480;
    int lowHz = 100, highHz = 3000, markerHz = 1500;
    bool operator==(const AnalysisConfig &b) const {
        return epoch==b.epoch && slotUtcMs==b.slotUtcMs && ft4==b.ft4 && hop==b.hop &&
               lowHz==b.lowHz && highHz==b.highHz && markerHz==b.markerHz;
    }
    bool operator!=(const AnalysisConfig &b) const { return !(*this==b); }
};
inline quint64 frequencyBits(double frequency) {
    quint64 bits; static_assert(sizeof(bits)==sizeof(frequency),"double key size");
    std::memcpy(&bits,&frequency,sizeof(bits)); return bits;
}
struct PreparedAnalysis {
    AnalysisConfig config;
    QVector<double> prefix;
    std::vector<std::shared_ptr<const std::vector<double>>> rows;
    std::vector<double> frequencies;
    std::unordered_map<quint64,size_t> frequencyIndex;
    double preparationMs = 0.0;
    bool matches(const QVector<double> &samples, qint64 slotUtcMs) const {
        const int n=std::min(prefix.size(),samples.size());
        return config.slotUtcMs==slotUtcMs && n>0 &&
               std::memcmp(prefix.constData(),samples.constData(),size_t(n)*sizeof(double))==0;
    }
    const std::vector<double> *fftRow(int start,int sampleCount) const {
        if(config.ft4 || start<0 || start%config.hop || start+1920>=sampleCount || start+1920>=prefix.size())return nullptr;
        const auto index=size_t(start/config.hop);
        return index<rows.size()?rows[index].get():nullptr;
    }
    bool ft4Energy(int start,int sampleCount,double frequency,double &value) const {
        if(!config.ft4 || start<0 || start%576 || start+576>=sampleCount || start+576>=prefix.size())return false;
        const auto row=size_t(start/576);if(row>=rows.size())return false;
        const auto f=frequencyIndex.find(frequencyBits(frequency));if(f==frequencyIndex.end())return false;
        value=(*rows[row])[f->second];return true;
    }
};

// One background worker and at most one pending, contiguous input tail.
// Capture never waits for preprocessing. Published rows/prefixes are immutable;
// decode jobs can retain an old slot snapshot while capture starts a new slot.
class IncrementalAnalysis {
public:
    using FftBuilder=std::function<std::vector<double>(const QVector<double>&,int)>;
    using ToneBuilder=std::function<double(const QVector<double>&,int,double)>;
    IncrementalAnalysis(FftBuilder fft,ToneBuilder tone):m_fft(std::move(fft)),m_tone(std::move(tone)){}
    ~IncrementalAnalysis(){stop();}
    IncrementalAnalysis(const IncrementalAnalysis&)=delete;
    void invalidate() noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);++m_serial;m_pending.reset();
        std::atomic_store(&m_published,std::shared_ptr<const PreparedAnalysis>{});
    }
    void stop() noexcept {
        {std::lock_guard<std::mutex> lock(m_mutex);m_stop=true;++m_serial;m_pending.reset();}
        m_cv.notify_all();if(m_thread.joinable())m_thread.join();
    }
    bool submit(const AnalysisConfig &config,int offset,QVector<double> tail) noexcept {
        try {
            std::lock_guard<std::mutex> lock(m_mutex);if(m_stop||m_failed)return false;
            if(!m_haveConfig || config!=m_requested){
                if(offset!=0)return false;
                ++m_serial;m_requested=config;m_haveConfig=true;m_pending.reset();
                std::atomic_store(&m_published,std::shared_ptr<const PreparedAnalysis>{});
            }
            if(m_pending && m_pending->serial==m_serial.load() &&
               m_pending->offset+m_pending->tail.size()==offset)m_pending->tail+=tail;
            else m_pending=std::make_unique<Pending>(Pending{config,m_serial.load(),offset,std::move(tail)});
            if(!m_thread.joinable())m_thread=std::thread([this]{run();});
            m_cv.notify_one();return true;
        }catch(...){m_failed=true;std::atomic_store(&m_published,std::shared_ptr<const PreparedAnalysis>{});return false;}
    }
    std::shared_ptr<const PreparedAnalysis> snapshot(const QVector<double>&samples,qint64 slotUtcMs) const {
        auto p=std::atomic_load(&m_published);return p&&p->matches(samples,slotUtcMs)?p:nullptr;
    }
private:
    struct Pending {AnalysisConfig config;quint64 serial;int offset;QVector<double> tail;};
    void frequencies(PreparedAnalysis &frame) {
        constexpr double spacing=12000.0/576.0;
        auto addGrid=[&](double low,double high){
            const int count=high>=low?int(std::floor((high-low)/spacing))+1:0;
            for(int fi=0;fi<count;++fi){
                const double base=low+double(fi)*spacing;
                for(int tone=0;tone<4;++tone){
                    const double f=base+tone*spacing;const auto key=frequencyBits(f);
                    if(frame.frequencyIndex.emplace(key,frame.frequencies.size()).second)frame.frequencies.push_back(f);
                }
            }
        };
        const auto &c=frame.config;
        addGrid(std::max(150.0,double(c.lowHz)),std::min(double(c.highHz),3000.0-3.0*spacing));
        const int center=std::max(150,std::min(2930,c.markerHz));
        addGrid(std::max(std::max(150.0,double(c.lowHz)),double(center-180)),
                std::min(std::min(double(c.highHz),3000.0-3.0*spacing),double(center+180)));
    }
    void run() noexcept {
        try {
            std::unique_ptr<PreparedAnalysis> build;quint64 serial=0;
            for(;;){
                std::unique_ptr<Pending> input;
                {std::unique_lock<std::mutex> lock(m_mutex);m_cv.wait(lock,[this]{return m_stop||m_pending;});
                 if(m_stop)return;input=std::move(m_pending);}
                if(input->serial!=m_serial.load())continue;
                if(!build||serial!=input->serial){
                    if(input->offset!=0)continue;
                    build=std::make_unique<PreparedAnalysis>();build->config=input->config;serial=input->serial;
                    build->prefix.reserve(build->config.ft4?90000:180000);
                    if(build->config.ft4)frequencies(*build);
                }
                if(input->offset!=build->prefix.size())continue;
                build->prefix+=input->tail;
                const int symbol=build->config.ft4?576:1920, hop=build->config.ft4?576:build->config.hop;
                const auto start=std::chrono::steady_clock::now();
                for(int offset=int(build->rows.size())*hop;offset+symbol<build->prefix.size();offset+=hop){
                    if(serial!=m_serial.load())break;
                    std::vector<double> row;
                    if(build->config.ft4){row.reserve(build->frequencies.size());
                        for(double f:build->frequencies)row.push_back(m_tone(build->prefix,offset,f));
                    }else row=m_fft(build->prefix,offset);
                    build->rows.push_back(std::make_shared<const std::vector<double>>(std::move(row)));
                }
                build->preparationMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
                {std::lock_guard<std::mutex> lock(m_mutex);
                 if(!m_stop && serial==m_serial.load())std::atomic_store(&m_published,std::shared_ptr<const PreparedAnalysis>(std::make_shared<PreparedAnalysis>(*build)));}
            }
        }catch(...){m_failed=true;std::atomic_store(&m_published,std::shared_ptr<const PreparedAnalysis>{});}
    }
    FftBuilder m_fft;ToneBuilder m_tone;
    mutable std::mutex m_mutex;std::condition_variable m_cv;std::thread m_thread;
    std::unique_ptr<Pending> m_pending;AnalysisConfig m_requested;bool m_haveConfig=false,m_stop=false;
    std::atomic<bool> m_failed{false};std::atomic<quint64> m_serial{0};
    std::shared_ptr<const PreparedAnalysis> m_published;
};

struct AnalysisReadContext {
    std::shared_ptr<const PreparedAnalysis> frame;
    const double *original = nullptr;
    std::atomic<int> fftHits{0},toneHits{0};
    bool originalInput(const QVector<double>&samples)const{return frame&&original==samples.constData();}
};
inline thread_local AnalysisReadContext *currentAnalysis = nullptr;
class AnalysisReadScope {
public:
    AnalysisReadScope(std::shared_ptr<const PreparedAnalysis> frame,const QVector<double>&samples):previous(currentAnalysis){
        context.frame=std::move(frame);context.original=samples.constData();currentAnalysis=&context;
    }
    ~AnalysisReadScope(){currentAnalysis=previous;}
    AnalysisReadContext context;
private: AnalysisReadContext *previous;
};
}
