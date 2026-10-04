#pragma once
#include "../utils/SystemResourceManager.h"
#include <algorithm>
#include <condition_variable>
#include <exception>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

// Persistent FT-only pool: audio, CAT and the GUI never share its work queue.
// Threads are created on demand up to the topology budget and reused. Nested
// parallel work executes inline, so pool workers never wait for their own pool.
class FtDecodeWorkerPool final
{
public:
    static FtDecodeWorkerPool &instance() {
        static FtDecodeWorkerPool pool(MadModemRuntime::SystemResourceManager::instance().poolCapacity());
        return pool;
    }
    explicit FtDecodeWorkerPool(int capacity) : m_capacity(std::max(1, capacity)) {}
    ~FtDecodeWorkerPool() {
        { std::lock_guard<std::mutex> lock(m_mutex); m_stopping = true; }
        m_available.notify_all();
        for (auto &worker : m_workers) if (worker.joinable()) worker.join();
    }
    FtDecodeWorkerPool(const FtDecodeWorkerPool &) = delete;
    FtDecodeWorkerPool &operator=(const FtDecodeWorkerPool &) = delete;
    int recommendedWorkerCount(MadModemRuntime::WorkClass work, int items) const {
        return MadModemRuntime::SystemResourceManager::instance().recommendedWorkers(work, items);
    }
    int workerCount() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return static_cast<int>(m_workers.size());
    }
    void parallelFor(int items, int requestedTasks, const std::function<void(int, int)> &fn) {
        if (items <= 0) return;
        const int tasks = std::max(1, std::min({requestedTasks, items, m_capacity}));
        if (tasks == 1 || s_insidePool) { fn(0, items); return; }
        struct Batch {
            std::mutex mutex;
            std::condition_variable done;
            int remaining = 0;
            std::exception_ptr exception;
        };
        auto batch = std::make_shared<Batch>();
        std::list<std::function<void()>> pending;
        // Construct the entire batch before publishing any task. Allocation or
        // capture-copy failure cannot leave workers referencing an unwound caller.
        for (int task = 0; task < tasks; ++task) {
            const int begin = int((static_cast<long long>(items) * task) / tasks);
            const int end = int((static_cast<long long>(items) * (task + 1)) / tasks);
            pending.emplace_back([batch, fn, begin, end] {
                std::exception_ptr failure;
                try { fn(begin, end); } catch (...) { failure = std::current_exception(); }
                {
                    std::lock_guard<std::mutex> lock(batch->mutex);
                    if (failure && !batch->exception) batch->exception = failure;
                    --batch->remaining;
                }
                batch->done.notify_one();
            });
        }
        batch->remaining = tasks;
        bool inlineFallback = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            while (static_cast<int>(m_workers.size()) < tasks) {
                try {
                    // Grow storage BEFORE creating a joinable thread: a vector
                    // allocation failure must not destroy a joinable temporary.
                    m_workers.reserve(static_cast<size_t>(tasks));
                    const int index = static_cast<int>(m_workers.size());
                    m_workers.emplace_back([this, index] { workerLoop(index); });
                } catch (const std::system_error &) {
                    break; // OS thread limit: use the workers already available.
                }
            }
            inlineFallback = m_workers.empty();
            if (!inlineFallback) m_queue.splice(m_queue.end(), pending); // no allocation
        }
        if (inlineFallback) { fn(0, items); return; }
        m_available.notify_all();
        std::unique_lock<std::mutex> lock(batch->mutex);
        batch->done.wait(lock, [&] { return batch->remaining == 0; });
        if (batch->exception) std::rethrow_exception(batch->exception);
    }
private:
    void workerLoop(int index) {
        MadModemRuntime::SystemResourceManager::instance().configureCurrentWorkerThread(index);
        s_insidePool = true;
        for (;;) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_available.wait(lock, [&] { return m_stopping || !m_queue.empty(); });
                if (m_stopping && m_queue.empty()) break;
                task = std::move(m_queue.front());
                m_queue.pop_front();
            }
            task();
        }
        s_insidePool = false;
    }
    inline static thread_local bool s_insidePool = false;
    const int m_capacity;
    mutable std::mutex m_mutex;
    std::condition_variable m_available;
    std::list<std::function<void()>> m_queue;
    std::vector<std::thread> m_workers;
    bool m_stopping = false;
};
