#pragma once

#include "BackgroundQueue.h"
#include "../modems/ft8/tx/Ft8Transmitter.h"
#include "../modems/q65/tx/Q65Transmitter.h"
#include "../modems/msk144/tx/Msk144Transmitter.h"
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <exception>
#include "../modems/weak_signal/WeakSignalCodecLock.h"

// Codec generation can wait for a decoder's codec mutex. It must never run in
// the GUI or in the audio-output thread. Superseded jobs cannot publish output.
class WeakSignalTxPreparer final : public QObject
{
public:
    using Completion = std::function<void(std::unique_ptr<TxModulator>, const QString &)>;
    explicit WeakSignalTxPreparer(QObject *parent = nullptr) : QObject(parent) {}
    ~WeakSignalTxPreparer() override { cancel(); m_queue.stop(); }
    void cancel() {
        ++m_generation;
        { std::lock_guard<std::mutex> lock(m_mailboxMutex); m_pending = {}; }
        WeakSignalCodecLock::mutex().wakeWaiters();
    }
    // At most one pending request, in addition to the currently executing job.
    int pendingCount() const {
        std::lock_guard<std::mutex> lock(m_mailboxMutex);
        return m_pending ? 1 : 0;
    }
    void prepare(const QString &mode, const QString &message, double hz, Completion done) {
        submit([mode, message, hz](std::function<bool()> cancelled) {
            return std::make_unique<Ft8Transmitter>(mode, message, 48000, hz, 0, std::move(cancelled));
        }, std::move(done));
    }
    void prepareNative(bool msk144, const QString &message, int sampleRate, int period,
                       bool shortMessage, Q65Mode::Submode submode, double hz, Completion done) {
        if (msk144) {
            submit([=](std::function<bool()> cancelled) {
                return std::make_unique<Msk144Transmitter>(message, sampleRate, period, shortMessage, hz, std::move(cancelled));
            }, std::move(done));
        } else {
            submit([=](std::function<bool()> cancelled) {
                return std::make_unique<Q65Transmitter>(message, sampleRate, period, submode, hz, std::move(cancelled));
            }, std::move(done));
        }
    }
private:
    template<class Factory> void submit(Factory factory, Completion done) {
        const auto generation = ++m_generation;
        auto task = [this, generation, factory, done] {
            if (generation != m_generation.load()) return;
            auto waveform = std::make_shared<std::unique_ptr<TxModulator>>();
            QString error;
            try {
                auto transmitter = factory([this, generation] { return generation != m_generation.load(); });
                error = transmitter->generationError();
                if (transmitter->generationSucceeded()) *waveform = std::move(transmitter);
            } catch (const std::exception &failure) {
                error = QString::fromLocal8Bit(failure.what());
            } catch (...) {
                error = QStringLiteral("Waveform preparation failed");
            }
            if (generation != m_generation.load()) return;
            QMetaObject::invokeMethod(this, [this, generation, waveform, error, done] {
                if (generation == m_generation.load() && done) done(std::move(*waveform), error);
            }, Qt::QueuedConnection);
        };
        bool schedule = false;
        {
            std::lock_guard<std::mutex> lock(m_mailboxMutex);
            m_pending = std::move(task); // Release the replaced request immediately.
            if (!m_scheduled) { m_scheduled = true; schedule = true; }
        }
        WeakSignalCodecLock::mutex().wakeWaiters();
        if (schedule) m_queue.post([this] {
            for (;;) {
                std::function<void()> next;
                {
                    std::lock_guard<std::mutex> lock(m_mailboxMutex);
                    next = std::move(m_pending);
                    m_pending = {};
                    if (!next) { m_scheduled = false; return; }
                }
                next();
            }
        });
    }
private:
    std::atomic<quint64> m_generation{0};
    mutable std::mutex m_mailboxMutex;
    std::function<void()> m_pending;
    bool m_scheduled = false;
    BackgroundQueue m_queue;
};
