// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
struct AudioBlock;
class AppSettings;
class SessionDiagnostics {
public:
    explicit SessionDiagnostics(size_t queueLimitBytes = 32 * 1024 * 1024);
    ~SessionDiagnostics();
    static SessionDiagnostics &instance();
    static QString defaultFolder();
    static void installQtCapture();
    static void restoreQtCapture();
    bool start(const QString &folder);
    void stop();
    bool flush(int timeoutMs = 2000);
    bool enabled() const { return m_active.load(std::memory_order_relaxed); }
    QString path() const;
    QString error() const;
    quint64 lost() const;
    void record(const QString &category, QJsonObject fields = {});
    void audio(const QString &stage, const AudioBlock &block);
    void settings(const AppSettings &settings);
private:
    void stopLocked();
    void run();
    const size_t m_limit;
    std::atomic<bool> m_active{false};
    mutable std::mutex m_mutex;
    std::mutex m_control;
    std::condition_variable m_cv;
    std::deque<QJsonObject> m_queue;
    size_t m_bytes = 0;
    quint64 m_sequence = 0, m_lost = 0, m_flushRequest = 0, m_flushDone = 0;
    bool m_stopping = false, m_ready = false;
    QString m_path, m_error;
    std::thread m_thread;
};
