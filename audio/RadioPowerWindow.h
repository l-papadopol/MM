// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "AudioBlock.h"
#include "RadioBandPower.h"
#include <QMutex>
#include <QMutexLocker>
// Timestamped measurement on the RX owner; GUI only starts/reads a short snapshot.
class RadioPowerWindow {
public:
    struct Snapshot { double sum=0;quint64 blocks=0;bool ready=false,gap=false; };
    void begin(qint64 startNs,qint64 endNs,int low,int high){QMutexLocker lock(&mutex);start=startNs;end=endNs;lowHz=low;highHz=high;++epoch;result={};lastEnd=0;}
    void markGap(){QMutexLocker lock(&mutex);if(end>start&&!result.ready)result.gap=true;}
    Snapshot snapshot(){QMutexLocker lock(&mutex);return result;}
    void process(const AudioBlock &block){
        qint64 first,last;int low,high;quint64 generation;
        {QMutexLocker lock(&mutex);first=start;last=end;low=lowHz;high=highHz;generation=epoch;}
        if(last<=first||block.samples.isEmpty()||block.sampleRate<=0)return;
        if(block.firstSampleUtcNs<=0){markGap();return;}
        const auto middle=block.firstSampleUtcNs+block.samples.size()*500000000LL/block.sampleRate;
        if(middle<first)return;
        if(middle>=last){QMutexLocker lock(&mutex);if(epoch==generation && !result.ready){
            if(result.blocks && block.firstSampleUtcNs>lastEnd+5000000LL)result.gap=true;
            result.ready=true;}return;}
        const auto power=MadModemAudio::radioTelescopeWelchBandPower(block.samples,block.sampleRate,low,high);
        QMutexLocker lock(&mutex);if(epoch==generation){
            const auto duration=block.samples.size()*1000000000LL/block.sampleRate;
            if(!result.blocks && block.firstSampleUtcNs>first+duration)result.gap=true;
            result.sum+=power;++result.blocks;lastEnd=block.firstSampleUtcNs+duration;}
    }
private:
    QMutex mutex;Snapshot result;qint64 start=0,end=0,lastEnd=0;int lowHz=300,highHz=3000;quint64 epoch=0;
};
