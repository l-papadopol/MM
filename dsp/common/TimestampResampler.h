// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QVector>
#include <QtGlobal>
#include <cmath>

// Streaming linear interpolation onto an exact 12-kHz UTC grid. Input timing
// may change smoothly with the measured hardware clock; output time never does.
class TimestampResampler {
public:
    void reset() { m_have=false;m_index=0;m_anchor=0;m_previousTime=0;m_previous=0; }
    QVector<double> process(const QVector<double> &input,qint64 first,qint64 span,qint64 *firstOut) {
        QVector<double> out;if(firstOut)*firstOut=0;
        if(input.isEmpty() || first<=0 || span<=0)return out;
        const long double period=static_cast<long double>(span)/input.size();
        if(!m_have || first<m_anchor ||
           std::abs(static_cast<long double>(first-m_anchor)-m_previousTime-period)>4*period) {
            reset();m_anchor=first;
        }
        out.reserve(int(std::ceil(span*12000./1e9))+2);
        for(int i=0;i<input.size();++i) {
            const long double currentTime=static_cast<long double>(first-m_anchor)+i*period;
            if(!m_have){m_previousTime=currentTime;m_previous=input[i];m_have=true;}
            long double outputTime=m_index*1000000000.L/12000.;
            while(outputTime<=currentTime) {
                if(out.isEmpty() && firstOut)*firstOut=m_anchor+qint64(std::llround(outputTime));
                const long double width=currentTime-m_previousTime;
                const double fraction=width>0?double((outputTime-m_previousTime)/width):0.;
                out.push_back(m_previous+qBound(0.,fraction,1.)*(input[i]-m_previous));
                ++m_index;outputTime=m_index*1000000000.L/12000.;
            }
            m_previousTime=currentTime;m_previous=input[i];
        }
        return out;
    }
private:
    bool m_have=false;qint64 m_anchor=0,m_index=0;
    long double m_previousTime=0;double m_previous=0;
};
