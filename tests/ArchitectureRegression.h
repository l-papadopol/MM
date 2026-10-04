#pragma once
#include "../runtime/WeakSignalTxPreparer.h"
#include "../modems/weak_signal/WeakSignalCodecLock.h"
#include "../logbook/AsyncLogbook.h"
#include "../audio/TxOutputDevice.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QTextStream>
#include <QFile>
#include <mutex>
#include <thread>
#include <vector>
#include "../audio/AudioEngine.h"
#include "../runtime/FtDecodeWorkerPool.h"
#include <stdexcept>

// A controllably slow backend exercises cancellation during device opening
// without requiring an audio device or relying on platform timing.
class RegressionAudioInput final : public AudioEngine
{
public:
    std::atomic<bool> entered{false}, release{false}, live{false};
    std::atomic<int> opens{0}, closes{0};
    bool startInput(const QString &, int) override {
        ++opens; entered = true;
        while (!release) QThread::msleep(1);
        live = true;
        return true;
    }
    void stopInput() override { ++closes; live = false; }
};

inline int runArchitectureRegression(QApplication &app)
{
    bool passed = true;
    auto check = [&](bool ok, const char *name) {
        QTextStream(stdout) << (ok ? "PASS " : "FAIL ") << name << '\n';
        passed &= ok;
    };
    auto pump = [&](const std::function<bool()> &done, int limit = 5000) {
        QElapsedTimer timer; timer.start();
        while (!done() && timer.elapsed() < limit) { app.processEvents(); QThread::msleep(1); }
        return done();
    };
    {
        FtDecodeWorkerPool pool(4);
        check(pool.workerCount() == 0, "FT pool creates no unused threads");
        std::atomic<int> visited{0};
        pool.parallelFor(37, 2, [&](int begin, int end) { visited += end - begin; });
        check(visited == 37 && pool.workerCount() == 2, "FT pool starts only requested workers and covers uneven batches");
        visited = 0;
        pool.parallelFor(19, 4, [&](int begin, int end) {
            for (int i = begin; i < end; ++i)
                pool.parallelFor(3, 4, [&](int first, int last) { visited += last - first; });
        });
        check(visited == 57 && pool.workerCount() == 4, "FT pool grows within capacity; nested parallel work cannot deadlock");
        bool caught = false;
        visited = 0;
        try {
            pool.parallelFor(8, 4, [&](int begin, int end) {
                if (begin == 0) throw std::runtime_error("test worker failure");
                visited += end - begin;
            });
        } catch (const std::runtime_error &) { caught = true; }
        check(caught && visited == 6, "FT worker exception reaches caller only after all batch tasks complete");
        visited = 0;
        std::thread first([&] { pool.parallelFor(29, 1000, [&](int b, int e) { visited += e-b; }); });
        std::thread second([&] { pool.parallelFor(31, 1000, [&](int b, int e) { visited += e-b; }); });
        first.join(); second.join();
        check(visited == 60 && pool.workerCount() == 4, "concurrent batches stay within FT pool capacity after an exception");
    }
    {
        WeakSignalCodecLock::Mutex gate;
        gate.lock();
        std::vector<char> admitted;
        std::vector<std::thread> threads;
        threads.emplace_back([&] { gate.lock(); admitted.push_back('R'); gate.unlock(); });
        check(pump([&] { return gate.waiters().receive == 1; }), "RX waiter registered");
        for (unsigned i = 0; i < 6; ++i) {
            threads.emplace_back([&] {
                if (gate.lockTx({})) { admitted.push_back('T'); gate.unlock(); }
            });
            check(pump([&] { return gate.waiters().transmit == i + 1; }), "TX waiter registered");
        }
        gate.unlock();
        for (auto &thread : threads) thread.join();
        check(admitted == std::vector<char>({'T','T','T','T','R','T','T'}),
              "TX gets priority and waiting RX is admitted after four TX jobs");
        gate.lock();
        std::atomic<bool> cancelled{false}, finished{false}, acquired{false};
        std::thread waiter([&] {
            acquired = gate.lockTx([&] { return cancelled.load(); });
            if (acquired) gate.unlock();
            finished = true;
        });
        check(pump([&] { return gate.waiters().transmit == 1; }), "cancellable waiter registered");
        cancelled = true; gate.wakeWaiters();
        check(pump([&] { return finished.load(); }, 500) && !acquired,
              "cancelled TX leaves the queue without acquiring held codec");
        gate.unlock(); waiter.join();
        check(gate.waiters().transmit == 0, "cancellation removes its waiter");
    }
    {
        QThread thread;
        auto *engine = new RegressionAudioInput;
        engine->moveToThread(&thread);
        QObject::connect(&thread, &QThread::finished, engine, &QObject::deleteLater);
        thread.start();
        quint64 expected = 0;
        int started = 0, stopped = 0;
        QObject context;
        QObject::connect(engine, &AudioEngine::inputRequestFinished, &context,
            [&](quint64 id, bool start, bool success) {
                if (id != expected || !success) return;
                if (start) ++started; else ++stopped;
            });
        expected = engine->requestStartInput("test", 48000);
        check(pump([&] { return engine->entered.load(); }), "audio backend is opening asynchronously");
        bool heartbeat = false;
        QTimer::singleShot(20, &context, [&] { heartbeat = true; });
        check(pump([&] { return heartbeat; }, 200), "GUI keeps processing events during slow input open");
        expected = engine->requestStopInput();
        engine->release = true;
        check(pump([&] { return stopped == 1; }) && started == 0 && !engine->live,
              "Stop during input open compensates stale start before acknowledging stop");
        engine->entered = false; engine->release = false;
        expected = engine->requestStartInput("old", 48000);
        check(pump([&] { return engine->entered.load(); }), "second slow capture open entered");
        for (int i = 0; i < 1000; ++i) {
            engine->requestStopInput();
            expected = engine->requestStartInput(QString::number(i), 48000);
        }
        engine->release = true;
        check(pump([&] { return started == 1; }) && engine->live && engine->opens == 3,
              "audio request storm opens only the active and latest requested devices");
        expected = engine->requestStopInput();
        check(pump([&] { return stopped == 2; }) && !engine->live, "latest capture stops cleanly");
        thread.quit(); thread.wait();
    }
    {
        std::unique_lock<WeakSignalCodecLock::Mutex> codec(WeakSignalCodecLock::mutex());
        QElapsedTimer elapsed; elapsed.start();
        bool delivered = false;
        {
            WeakSignalTxPreparer preparer;
            preparer.prepare("FT8", "CQ IZ6NNH JN63", 1500,
                [&](std::unique_ptr<TxModulator>, const QString &) { delivered = true; });
            bool heartbeat = false;
            QTimer::singleShot(30, &app, [&] { heartbeat = true; });
            check(pump([&] { return heartbeat; }, 300), "GUI remains responsive while codec mutex is held");
            preparer.cancel();
            // Destruction must not wait for the held codec mutex.
        }
        check(elapsed.elapsed() < 1000 && !delivered, "cancel and destruction interrupt queued/blocked FT preparation");
    }
    {
        WeakSignalTxPreparer preparer;
        std::unique_lock<WeakSignalCodecLock::Mutex> codec(WeakSignalCodecLock::mutex());
        int obsolete = 0, current = 0;
        preparer.prepare("FT8", "CQ K1ABC FN31", 1500,
            [&](std::unique_ptr<TxModulator>, const QString &) { ++obsolete; });
        preparer.prepare("FT4", "CQ IZ6NNH JN63", 1700,
            [&](std::unique_ptr<TxModulator> waveform, const QString &) {
                if (waveform && waveform->description().contains("FT4")) ++current;
            });
        codec.unlock();
        check(pump([&] { return current == 1; }) && obsolete == 0, "only latest FT preparation can publish a waveform");
    }
    {
        std::unique_lock<WeakSignalCodecLock::Mutex> codec(WeakSignalCodecLock::mutex());
        WeakSignalTxPreparer preparer;
        int obsolete = 0, latest = 0;
        for (int i = 0; i < 5000; ++i)
            preparer.prepare("FT8", "CQ K1ABC FN31", 1500,
                [&](std::unique_ptr<TxModulator>, const QString &) { ++obsolete; });
        preparer.prepare("FT4", "CQ IZ6NNH JN63", 1500,
            [&](std::unique_ptr<TxModulator> wave, const QString &) { if (wave) ++latest; });
        check(preparer.pendingCount() <= 1, "5000 superseded TX requests cannot grow the pending mailbox");
        codec.unlock();
        check(pump([&] { return latest == 1; }) && !obsolete,
              "request storm publishes only the latest waveform");
    }
    for (bool msk : {false, true}) {
        std::unique_lock<WeakSignalCodecLock::Mutex> codec(WeakSignalCodecLock::mutex());
        QElapsedTimer elapsed; elapsed.start();
        bool delivered = false;
        {
            WeakSignalTxPreparer preparer;
            preparer.prepareNative(msk, "CQ IZ6NNH JN63", 48000, 15, false, Q65Mode::Submode::A, 1500,
                [&](std::unique_ptr<TxModulator>, const QString &) { delivered = true; });
            bool heartbeat = false;
            QTimer::singleShot(30, &app, [&] { heartbeat = true; });
            check(pump([&] { return heartbeat; }, 300), msk ? "MSK144 codec wait leaves GUI responsive" : "Q65 codec wait leaves GUI responsive");
            preparer.cancel();
        }
        check(elapsed.elapsed() < 1000 && !delivered, msk ? "MSK144 preparation cancels while codec is locked" : "Q65 preparation cancels while codec is locked");
    }
    {
        Ft8Transmitter tone("FT8", 48000, 1500, 1.0, true);
        tone.setPlaybackWindow(10000, 10100);
        check(tone.prepareForPlayback(9975), "playback window accepts first pull before target");
        QVector<float> samples(1200);
        const int silence = tone.generate(samples.data(), samples.size());
        bool zero = silence == 1200;
        for (float value : samples) zero &= value == 0.0f;
        int produced = silence;
        while (!tone.isFinished()) produced += tone.generate(samples.data(), samples.size());
        check(zero && produced == 6000, "first pull computes 25 ms lead and 100 ms waveform cutoff without stale GUI silence");
        Ft8Transmitter late("FT8", 48000, 1500, 1.0, true);
        late.setPlaybackWindow(10000);
        check(!late.prepareForPlayback(10021), "late first pull rejects the entire FT waveform");
    }
    QTemporaryDir directory;
    check(directory.isValid(), "temporary logbook directory available");
    AdifLogbook mirror(directory.filePath("log.adi"));
    mirror.load();
    AsyncLogbook store(&mirror);
    bool callbackThread = true;
    auto entry = [&](const QString &call, const QString &id) {
        LogbookEntry value; value.callsign = call; value.band = "20m"; value.mode = "FT8";
        value.utc = QDateTime::currentDateTimeUtc();
        value.adifFields["APP_MADMODEM_QSO_ID"] = id;
        return value;
    };
    const auto first = entry("K1ABC", "first");
    int saved = 0, duplicates = 0, errors = 0;
    auto complete = [&](int result, const QString &) {
        callbackThread &= QThread::currentThread() == app.thread();
        if (result > 0) ++saved;
        else if (result == 0) ++duplicates;
        else ++errors;
    };
    store.append(first, complete, true);
    store.append(entry("K1ABC", "second-request"), complete, true);
    store.append(entry("DL1XYZ", "other"), complete, true);
    bool idle = false;
    store.whenIdle([&] { idle = true; });
    check(pump([&] { return idle; }) && saved == 2 && duplicates == 1 && !errors && callbackThread,
          "serialized FT dedupe plus commit preserves distinct QSOs and delivers GUI acknowledgments");
    AdifLogbook disk(mirror.fileName()); disk.load();
    check(mirror.count() == 2 && disk.count() == 2, "idle notification follows committed disk and mirror state");
    store.remove({first}, complete);
    store.append(first, complete, true);
    store.drain();
    check(mirror.count() == 2 && saved == 4 && duplicates == 1, "delete and next append share ownership and rebuild duplicate indexes");
    {
        QFile external(mirror.fileName()); external.open(QIODevice::Append);
        external.write("\n# external edit\n"); external.close();
        store.append(entry("EK1KE", "retry-id"), complete, true);
        store.drain();
        check(store.hasUnsaved() && mirror.count() == 2 && errors == 1,
              "failed write is retained without a false success or speculative mirror row");
        store.reload(mirror.fileName(), {});
        store.retryFailed();
        store.drain();
        disk.load();
        check(!store.hasUnsaved() && mirror.count() == 3 && disk.count() == 3 && saved == 5,
              "retry after reload saves retained QSO exactly once");
    }
    for (int i = 0; i < 12; ++i) store.append(entry(QString("W%1AAA").arg(i), QString::number(i)), {});
    store.drain();
    disk.load();
    check(!store.busy() && disk.count() == 15 && mirror.count() == 15,
          "shutdown drain commits all accepted QSO writes");
    return passed ? 0 : 1;
}
