#pragma once

#include <QObject>
#include <QThread>
#include <QMetaObject>

// Serial worker queue. All tasks must own their input; no widget or borrowed
// GUI state may be accessed in a task. Shutdown drains accepted tasks before
// quitting, so neither queued work nor its captured ownership is abandoned.
class BackgroundQueue final
{
public:
    BackgroundQueue() {
        m_worker.moveToThread(&m_thread);
        m_thread.start();
    }
    ~BackgroundQueue() { stop(); }
    BackgroundQueue(const BackgroundQueue &) = delete;
    BackgroundQueue &operator=(const BackgroundQueue &) = delete;

    template<class Task> void post(Task task) {
        QMetaObject::invokeMethod(&m_worker, std::move(task), Qt::QueuedConnection);
    }
    void drain() {
        if (m_thread.isRunning())
            QMetaObject::invokeMethod(&m_worker, [] {}, Qt::BlockingQueuedConnection);
    }
    void stop() {
        if (!m_thread.isRunning()) return;
        drain();
        // Return ownership to the creating thread before destroying the object.
        QThread *owner = QThread::currentThread();
        QMetaObject::invokeMethod(&m_worker, [this, owner] { m_worker.moveToThread(owner); },
                                  Qt::BlockingQueuedConnection);
        m_thread.quit();
        m_thread.wait();
    }
private:
    QThread m_thread;
    QObject m_worker;
};
