#pragma once
#include <QThread>
#include <QString>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>

struct CwSerialConfig {
    QString port;
    QString keyLine = "rts";
    QString pttLine = "none"; // Radio BK-IN, or the other line on this same port.
    int leadMs = 40;
    int tailMs = 100;
};
class CwKeyOutput {
public:
    virtual ~CwKeyOutput() = default;
    virtual bool open(const QString& port) = 0;
    virtual bool setLine(const QString& line, bool asserted) = 0;
    virtual QString errorString() const = 0;
    virtual void close() = 0;
};
// Serial ownership and dot/dash deadlines stay on this thread, including cleanup.
// Stop wakes the monotonic deadline wait directly; it never waits for GUI events.
class CwSerialTransmitter final : public QThread {
    Q_OBJECT
public:
    using Factory = std::function<std::unique_ptr<CwKeyOutput>()>;
    explicit CwSerialTransmitter(QObject* parent = nullptr, Factory factory = {});
    ~CwSerialTransmitter() override;
    bool startMessage(const QString& text, double wpm, const CwSerialConfig& config);
    void requestStop();
    bool releaseLines();
    bool linesReleased() const { return m_releaseConfirmed; }
signals:
    void keyingStarted();
    void characterSent(double progress, int position);
    void messageCompleted(bool natural, const QString& error);
protected:
    void run() override;
private:
    bool waitUntil(std::chrono::steady_clock::time_point deadline);
    void complete(bool natural, const QString& error) { m_natural = natural; m_error = error; }
    bool m_busy = false; // GUI-owned; includes pending completion delivery.
    bool m_natural = false;
    bool m_releaseOnly = false;
    bool m_releaseConfirmed = true;
    QString m_error;
    Factory m_factory;
    QString m_text;
    double m_wpm = 20;
    CwSerialConfig m_config;
    std::atomic_bool m_stop{false};
    std::mutex m_waitMutex;
    std::condition_variable m_wake;
};
