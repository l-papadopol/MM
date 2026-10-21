#ifndef BOUNDEDAUDIODISPATCHER_H
#define BOUNDEDAUDIODISPATCHER_H

#include "AudioBlock.h"

#include <QMutex>
#include <QObject>
#include <QQueue>
#include <QVector>

/** Bounded, coalesced live-audio delivery for slower UI/non-FT consumers. */
class BoundedAudioDispatcher final : public QObject
{
    Q_OBJECT

public:
    explicit BoundedAudioDispatcher(int maxBlocks = 48, QObject *parent = nullptr);

    struct Statistics {
        quint64 enqueued=0, taken=0, dropped=0, cleared=0;
        int pending=0, peakPending=0;
    };
    Statistics statistics();
    void enqueue(const AudioBlock &block);
    QVector<AudioBlock> takePending(int maximumBlocks, int *droppedBlocks = nullptr);
    void clear();
    int pendingBlockCount();

signals:
    void blocksAvailable();

private:
    Statistics m_statistics;
    QMutex m_mutex;
    QQueue<AudioBlock> m_queue;
    int m_maxBlocks = 48;
    int m_droppedSinceTake = 0;
    bool m_notificationPending = false;
};

#endif // BOUNDEDAUDIODISPATCHER_H
