#ifndef MADMODEM_SSTV_VIS_H
#define MADMODEM_SSTV_VIS_H
#include <QString>
#include <QtGlobal>
#include <cmath>
#include <array>

// Standard 7-bit VIS codes (parity is generated separately).
// Tables cross-checked with bundled MMSSTV/QSSTV references and the
// OK2MNM SSTV Handbook, chapter 5. No extended VIS modes in this table.
namespace SstvVis {
struct Entry { const char *label; int code; };
inline const std::array<Entry,19> &entries() {
    static const std::array<Entry,19> table = {{
        {"Martin M1",44},{"Martin M2",40},{"Martin M3",36},{"Martin M4",32},
        {"Scottie S1",60},{"Scottie S2",56},{"Scottie S3",52},{"Scottie S4",48},{"Scottie DX",76},
        {"Robot 24",4},{"Robot 36",8},{"Robot 72",12},
        {"PD50",93},{"PD90",99},{"PD120",95},{"PD160",98},{"PD180",96},{"PD240",97},{"PD290",94}
    }}; return table;
}
inline int code(const QString &label) { for (const auto &e:entries()) if (label==QLatin1String(e.label)) return e.code; return -1; }
inline QString label(int code) { for (const auto &e:entries()) if (code==e.code) return QLatin1String(e.label); return {}; }
inline int parity(int code) { int p=0; for(int i=0;i<7;++i) p^=(code>>i)&1; return p; }
inline double headerFrequency(double ms,int code) {
    if (ms<300.0) return 1900.0;
    if (ms<310.0) return 1200.0;
    if (ms<610.0) return 1900.0;
    if (ms<640.0) return 1200.0;
    if (ms<880.0) { int bit=int((ms-640.0)/30.0);int one=bit==7?parity(code):(code>>bit)&1;return one?1100.0:1300.0; }
    return 1200.0; // 30 ms stop, total 910 ms
}

// Streaming VIS recognizer. Only a complete leader/break/leader/start,
// eight data/parity cells and stop can start a new picture. Center-cell
// averages tolerate estimator delay and ignore short transitions.
class Detector {
public:
    void reset() { stage=0;count=0;gap=0;elapsed=0;data=0;stopGood=false;sums.fill(0);numbers.fill(0); }
    int feed(double frequency,int rate) {
        if (!std::isfinite(frequency) || rate<=0) { reset();return -1; }
        const bool leader=std::abs(frequency-1900.0)<90;
        const bool sync=std::abs(frequency-1200.0)<65;
        auto ms=[rate](double n){return qMax(1,int(std::lround(n*rate/1000.0)));};
        if (stage==0) {
            if (leader) { gap=0;count=qMin(count+1,ms(600));return -1; }
            if (sync && count>=ms(220)) {stage=1;count=1;gap=0;}
            else if(count<ms(220) || ++gap>ms(5)) reset();
        } else if (stage==1) {
            if (sync) {gap=0;if(++count>ms(25))reset();return -1;}
            if (leader && count>=ms(5)) {stage=2;count=1;gap=0;}
            else if(++gap>ms(5))reset();
        } else if (stage==2) {
            if (leader) {gap=0;if(++count>ms(450))reset();return -1;}
            if (sync && count>=ms(220)) {stage=3;elapsed=0;sums.fill(0);numbers.fill(0);data=0;}
            else if(count<ms(220) || ++gap>ms(5))reset();
        } else {
            ++elapsed;
            for(int bit=0;bit<8;++bit) {
                if (elapsed>=ms(40+30*bit) && elapsed<ms(50+30*bit)) {sums[bit]+=frequency;++numbers[bit];}
            }
            if(elapsed==ms(285)) stopGood=sync;
            if(elapsed>=ms(298)) {
                int result=-1, parity=0;bool valid=stopGood;
                for(int bit=0;bit<8;++bit) {
                    if(!numbers[bit]) {valid=false;break;}
                    double f=sums[bit]/numbers[bit];
                    if(qMin(std::abs(f-1100.0),std::abs(f-1300.0))>65) valid=false;
                    const int one=f<1200.0?1:0;parity^=one;if(bit<7)data|=one<<bit;
                }
                if(valid && parity==0 && !label(data).isEmpty()) result=data;
                reset();return result;
            }
        }
        return -1;
    }
private:
    int stage=0,count=0,gap=0,elapsed=0,data=0;bool stopGood=false;
    std::array<double,8> sums{};std::array<int,8> numbers{};
};
}
#endif
