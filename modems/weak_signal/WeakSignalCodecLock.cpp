#include "WeakSignalCodecLock.h"
#include <algorithm>
#include <chrono>

namespace WeakSignalCodecLock {

bool Mutex::acquire(bool transmit, const std::function<bool()> &cancelled)
{
    std::unique_lock<std::mutex> state(m_state);
    Waiter waiter;
    auto &queue = transmit ? m_tx : m_rx;
    queue.push_back(&waiter);
    try {
        for (;;) {
            if (cancelled && cancelled()) {
                queue.erase(std::find(queue.begin(), queue.end(), &waiter));
                m_changed.notify_all();
                return false;
            }
            const bool nextTx = !m_tx.empty() && (m_rx.empty() || m_txBurst < MaxTxBurst);
            if (!m_held && transmit == nextTx && queue.front() == &waiter) {
                queue.pop_front();
                m_held = true;
                m_txBurst = transmit ? std::min(MaxTxBurst, m_txBurst + 1) : 0;
                return true;
            }
            // Cancellation is also sampled without an explicit wake (callers
            // may supply a deadline predicate). No busy polling or spin lock.
            if (cancelled) m_changed.wait_for(state, std::chrono::milliseconds(5));
            else m_changed.wait(state);
        }
    } catch (...) {
        queue.erase(std::find(queue.begin(), queue.end(), &waiter));
        m_changed.notify_all();
        throw;
    }
}

void Mutex::lock() { acquire(false, {}); }
bool Mutex::lockTx(const std::function<bool()> &cancelled) { return acquire(true, cancelled); }
void Mutex::unlock()
{
    { std::lock_guard<std::mutex> state(m_state); m_held = false; }
    m_changed.notify_all();
}
Mutex::Waiters Mutex::waiters() const
{
    std::lock_guard<std::mutex> state(m_state);
    return {static_cast<unsigned>(m_rx.size()), static_cast<unsigned>(m_tx.size())};
}
Mutex &mutex() { static Mutex codecMutex; return codecMutex; }
} // namespace WeakSignalCodecLock
