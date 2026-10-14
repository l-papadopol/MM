#ifndef WEAKSIGNALCODECLOCK_H
#define WEAKSIGNALCODECLOCK_H

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>

namespace WeakSignalCodecLock {

// The imported codecs share process-wide hash/QRA state. Keep one owner, but
// admit TX ahead of RX at codec boundaries. FIFO within each class and a bounded
// TX burst prevent either class from starving. Never acquire another application
// lock while holding this lock if that lock's owner can enter a codec.
class Mutex final
{
public:
    void lock();
    void unlock();
    bool lockTx(const std::function<bool()> &cancelled);
    void wakeWaiters() { m_changed.notify_all(); }
    struct Waiters { unsigned receive; unsigned transmit; };
    Waiters waiters() const;
private:
    struct Waiter {};
    bool acquire(bool transmit, const std::function<bool()> &cancelled);
    mutable std::mutex m_state;
    std::condition_variable m_changed;
    std::deque<Waiter *> m_rx, m_tx;
    bool m_held = false;
    unsigned m_txBurst = 0;
    static constexpr unsigned MaxTxBurst = 4;
};

Mutex &mutex();

class TxGuard final
{
public:
    explicit TxGuard(const std::function<bool()> &cancelled)
        : m_owned(mutex().lockTx(cancelled)) {}
    ~TxGuard() { if (m_owned) mutex().unlock(); }
    explicit operator bool() const { return m_owned; }
    TxGuard(const TxGuard &) = delete;
    TxGuard &operator=(const TxGuard &) = delete;
private:
    bool m_owned;
};
} // namespace WeakSignalCodecLock
#endif
