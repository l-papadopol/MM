#pragma once
#include <QtGlobal>

// Independent progress clocks: source pulls cannot conceal a stalled sink,
// and a backend clock cannot conceal a source that stopped delivering PCM.
class TxPlaybackWatchdog {
public:
    enum class Failure { None, SourceStalled, PlaybackStalled };
    void reset(qint64 bufferDurationMs) {
        m_timeoutMs=qMax<qint64>(qint64{1500},bufferDurationMs*2+500);
        m_produced=m_processed=m_sourceAt=m_playbackAt=0;
    }
    Failure observe(qint64 nowMs,qint64 produced,qint64 processed,bool sourceEnded) {
        if(produced>m_produced) {m_produced=produced;m_sourceAt=nowMs;}
        if(processed>m_processed) {m_processed=processed;m_playbackAt=nowMs;}
        if(nowMs-m_playbackAt>m_timeoutMs) return Failure::PlaybackStalled;
        if(!sourceEnded && nowMs-m_sourceAt>m_timeoutMs) return Failure::SourceStalled;
        return Failure::None;
    }
private:
    qint64 m_timeoutMs=1500,m_produced=0,m_processed=0,m_sourceAt=0,m_playbackAt=0;
};
