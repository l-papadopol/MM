#pragma once
#include "CwBandSkimmer.h"
#include "../../audio/AudioBlock.h"
#include <algorithm>
#include <utility>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

// One independent, bounded band receiver. Selected RX A/B never wait for it.
// Only value snapshots cross threads; all lane DSP belongs to run().
class CwBandSkimmerWorker {
public:
    CwBandSkimmerWorker() : m_thread([this] { run(); }) {}
    ~CwBandSkimmerWorker() {
        { std::lock_guard<std::mutex> lock(m_mutex); m_stop = true; m_queue.clear(); }
        m_ready.notify_one();
        m_thread.join();
    }
    void reset() {
        std::lock_guard<std::mutex> lock(m_mutex);
        ++m_generation; m_queue.clear(); m_queuedSeconds = 0; m_spots.clear();
    }
    void submit(const AudioBlock& block,
                std::vector<madmodem::cwskimmer::CwSkimmerChannelState> carriers) {
        if (block.sampleRate <= 0 || block.samples.isEmpty()) return;
        const double seconds = double(block.samples.size()) / block.sampleRate;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            // Drop only skimmer history. Never reset the selected receivers.
            if (m_queuedSeconds + seconds > .75 || m_queue.size() >= 96) {
                ++m_generation; ++m_overloads;
                m_queue.clear(); m_queuedSeconds = 0; m_spots.clear();
                m_limit = std::max(2, m_limit / 2);
            }
            m_queue.push_back({block, std::move(carriers), m_generation});
            m_queuedSeconds += seconds;
        }
        m_ready.notify_one();
    }
    QVector<SkimmerSpot> spots() const {
        std::lock_guard<std::mutex> lock(m_mutex); return m_spots;
    }
    int channelLimit() const { std::lock_guard<std::mutex> lock(m_mutex); return m_limit; }
    quint64 overloads() const { std::lock_guard<std::mutex> lock(m_mutex); return m_overloads; }
private:
    struct Work {
        AudioBlock block;
        std::vector<madmodem::cwskimmer::CwSkimmerChannelState> carriers;
        quint64 generation;
    };
    void run() {
        CwBandSkimmer band;
        quint64 generation = 0;
        double load = 0, recoverySeconds = 0, measuredSeconds = 0;
        while (true) {
            Work work; int limit;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_ready.wait(lock, [this] { return m_stop || !m_queue.empty(); });
                if (m_stop) return;
                work = std::move(m_queue.front()); m_queue.pop_front();
                m_queuedSeconds -= double(work.block.samples.size()) / work.block.sampleRate;
                limit = m_limit;
            }
            if (generation != work.generation) {
                band.reset(); generation = work.generation;
                load = measuredSeconds = recoverySeconds = 0;
            }
            band.setChannelLimit(limit);
            const auto start = std::chrono::steady_clock::now();
            band.process(work.block.samples, work.block.sampleRate, work.carriers);
            auto spots = band.spots();
            const double audio = double(work.block.samples.size()) / work.block.sampleRate;
            const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            const double alpha = std::min(1.0, audio / 2.0);
            load += alpha * (elapsed / audio - load);
            measuredSeconds += audio;
            std::lock_guard<std::mutex> lock(m_mutex);
            if (generation != m_generation || m_stop) continue;
            m_spots = std::move(spots);
            // Leave headroom for audio, A/B and the rest of the station.
            if (measuredSeconds > 2 && load > .65 && m_limit > 2) {
                m_limit = std::max(2, m_limit - 2); measuredSeconds = recoverySeconds = 0;
            } else if (load < .35 && m_queuedSeconds < .1) {
                recoverySeconds += audio;
                if (recoverySeconds > 30 && m_limit < 16) {
                    ++m_limit; recoverySeconds = measuredSeconds = 0;
                }
            } else recoverySeconds = 0;
        }
    }
    mutable std::mutex m_mutex;
    std::condition_variable m_ready;
    std::deque<Work> m_queue;
    QVector<SkimmerSpot> m_spots;
    double m_queuedSeconds = 0;
    quint64 m_generation = 1, m_overloads = 0;
    int m_limit = 16;
    bool m_stop = false;
    std::thread m_thread;
};
