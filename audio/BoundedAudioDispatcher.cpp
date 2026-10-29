#include "../runtime/SessionDiagnostics.h"
#include "BoundedAudioDispatcher.h"

#include <QMutexLocker>
#include <QtGlobal>

namespace {
void traceDispatcherBlock(const QObject *dispatcher,const QString &operation,const AudioBlock &block) {
    auto &log=SessionDiagnostics::instance();if(!log.enabled())return;
    const QString route=dispatcher->objectName().isEmpty()?QString::number(quintptr(dispatcher),16):dispatcher->objectName();
    log.audio("dispatcher."+operation+"."+route,block);
}
}

BoundedAudioDispatcher::BoundedAudioDispatcher(int maxBlocks, QObject *parent)
    : QObject(parent),
      m_maxBlocks(qBound(4, maxBlocks, 512))
{
}

void BoundedAudioDispatcher::enqueue(const AudioBlock &block)
{
    traceDispatcherBlock(this,"enqueue",block);
    bool notify = false;
    {
        QMutexLocker lock(&m_mutex);
        while (m_queue.size() >= m_maxBlocks) {
            traceDispatcherBlock(this,"drop",m_queue.head());
            m_queue.dequeue();
            ++m_droppedSinceTake;
            ++m_statistics.dropped;
        }
        m_queue.enqueue(block);
        ++m_statistics.enqueued;
        m_statistics.peakPending=qMax(m_statistics.peakPending,m_queue.size());
        if (!m_notificationPending) {
            m_notificationPending = true;
            notify = true;
        }
    }
    if (notify) {
        emit blocksAvailable();
    }
}

QVector<AudioBlock> BoundedAudioDispatcher::takePending(int maximumBlocks, int *droppedBlocks)
{
    QVector<AudioBlock> result;
    bool notifyAgain = false;
    {
        QMutexLocker lock(&m_mutex);
        const int takeCount = qMin(qMax(1, maximumBlocks), m_queue.size());
        result.reserve(takeCount);
        m_statistics.taken+=takeCount;
        for (int i = 0; i < takeCount; ++i) {
            result.append(m_queue.dequeue());
        }
        if (droppedBlocks != nullptr) {
            *droppedBlocks = m_droppedSinceTake;
        }
        m_droppedSinceTake = 0;
        m_notificationPending = false;
        if (!m_queue.isEmpty()) {
            m_notificationPending = true;
            notifyAgain = true;
        }
    }
    if (notifyAgain) {
        emit blocksAvailable();
    }
    for(const auto &block:result)traceDispatcherBlock(this,"take",block);
    return result;
}

void BoundedAudioDispatcher::clear()
{
    QMutexLocker lock(&m_mutex);
    for(const auto &block:m_queue)traceDispatcherBlock(this,"clear",block);
    m_statistics.cleared+=m_queue.size();
    m_queue.clear();
    m_droppedSinceTake = 0;
    m_notificationPending = false;
}

int BoundedAudioDispatcher::pendingBlockCount() { QMutexLocker lock(&m_mutex); return m_queue.size(); }

BoundedAudioDispatcher::Statistics BoundedAudioDispatcher::statistics()
{
    QMutexLocker lock(&m_mutex);
    auto result=m_statistics;result.pending=m_queue.size();return result;
}
