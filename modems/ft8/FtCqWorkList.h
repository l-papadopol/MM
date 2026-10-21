// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Ft8RxDecoder.h"
#include <QString>
#include <QVector>

// Operator-selected CQ targets. Incoming callers remain in their own FIFO.
// Observation/selection live on the GUI owner; timing is injected for tests.
class FtCqWorkList {
public:
    struct Entry {
        QString call, grid, mode, band;
        Ft8RxDecoder::Decode decode;
        bool confirmed = false;
        bool checkedSinceIdle = false;
        bool cq = true;
    };
    enum class Availability { Wait, Ready, Busy, Absent };
    QVector<Entry> entries;
    bool running = false;
    qint64 confirmFromSlot = 0, waitingSinceMs = 0;
    static constexpr int MaxEntries = 30;
    bool add(const Entry &entry) {
        for(const auto &e:entries)if(e.call==entry.call)return false;
        if(entry.call.isEmpty()||entries.size()>=MaxEntries)return false;
        entries.append(entry);return true;
    }
    void observe(const QString &call, bool isCq, const Ft8RxDecoder::Decode &decode,
                 const QString &mode, const QString &band) {
        if(decode.slotStartUtcMs<=0)return;
        for(auto &e:entries) {
            if(e.call!=call||e.mode!=mode||e.band!=band)continue;
            // Older publications never undo a later directed message. At equal
            // slot, busy evidence wins over CQ, including duplicate batches.
            if(e.confirmed && decode.slotStartUtcMs<e.decode.slotStartUtcMs)continue;
            e.checkedSinceIdle=true;
            if(e.confirmed && decode.slotStartUtcMs==e.decode.slotStartUtcMs && !e.cq)continue;
            e.decode=decode;e.cq=isCq;e.confirmed=true;
        }
    }
    void requireFreshCheck(){confirmFromSlot=0;waitingSinceMs=0;for(auto &e:entries)e.checkedSinceIdle=false;}
    void beginCheck(qint64 now,int period) {
        if(confirmFromSlot==0){confirmFromSlot=now/period*period;waitingSinceMs=now;}
    }
    Availability availability(const Entry &e,qint64 now,int period)const {
        const auto slot=e.decode.slotStartUtcMs;
        if(e.confirmed && e.checkedSinceIdle && slot>=confirmFromSlot && slot<=now && now-slot<=2*period+1000)
            return e.cq?Availability::Ready:Availability::Busy;
        return waitingSinceMs>0 && now-waitingSinceMs>=2*period+2500 ? Availability::Absent : Availability::Wait;
    }
    void clear(){entries.clear();running=false;requireFreshCheck();}
};
