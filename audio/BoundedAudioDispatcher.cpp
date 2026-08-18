#include "BoundedAudioDispatcher.h"

#include <QMutexLocker>
#include <QtGlobal>

BoundedAudioDispatcher::BoundedAudioDispatcher(int maxBlocks, QObject *parent)
    : QObject(parent),
      m_maxBlocks(qBound(4, maxBlocks, 512))
{
}

void BoundedAudioDispatcher::enqueue(const AudioBlock &block)
{
    bool notify = false;
    {
        QMutexLocker lock(&m_mutex);
        while (m_queue.size() >= m_maxBlocks) {
            m_queue.dequeue();
            ++m_droppedSinceTake;
        }
        m_queue.enqueue(block);
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
    return result;
}

void BoundedAudioDispatcher::clear()
{
    QMutexLocker lock(&m_mutex);
    m_queue.clear();
    m_droppedSinceTake = 0;
    m_notificationPending = false;
}
