#include "CwSerialTransmitter.h"
#include "CwMorseSequence.h"
#include <QSerialPort>
#include <cmath>
#include <algorithm>
#ifdef Q_OS_WIN
#include <windows.h>
#include <mmsystem.h>
#endif
namespace {
class SerialOutput final : public CwKeyOutput {
    QSerialPort m_port;
public:
    bool open(const QString& name) override {
        m_port.setPortName(name);
        m_port.setBaudRate(QSerialPort::Baud9600); // No character data is sent.
        m_port.setFlowControl(QSerialPort::NoFlowControl);
        return m_port.open(QIODevice::ReadWrite);
    }
    bool setLine(const QString& line, bool state) override {
        return line == "rts" ? m_port.setRequestToSend(state) : m_port.setDataTerminalReady(state);
    }
    QString errorString() const override { return m_port.errorString(); }
    void close() override { m_port.close(); }
};
}
CwSerialTransmitter::CwSerialTransmitter(QObject* parent, Factory factory)
    : QThread(parent), m_factory(std::move(factory)) {
    connect(this, &QThread::finished, this, [this] { m_busy = false; emit messageCompleted(m_natural, m_error); });
}
CwSerialTransmitter::~CwSerialTransmitter() { requestStop(); wait(); }
bool CwSerialTransmitter::startMessage(const QString& text, double wpm, const CwSerialConfig& config) {
    if (m_busy || isRunning()) return false;
    m_text = text; m_wpm = wpm; m_config = config;
    m_natural = false; m_error.clear(); m_releaseOnly = false; m_releaseConfirmed = true;
    m_busy = true; m_stop.store(false); start(QThread::HighPriority); return true;
}
bool CwSerialTransmitter::releaseLines() {
    if (m_busy || isRunning()) return false;
    m_text.clear(); m_wpm = 20; m_releaseOnly = true;
    m_releaseConfirmed = false; m_natural = false; m_error.clear();
    m_busy = true; m_stop.store(false); start(QThread::HighPriority); return true;
}
void CwSerialTransmitter::requestStop() {
    // Use the wait mutex to avoid a lost wake between predicate and sleep.
    std::lock_guard<std::mutex> lock(m_waitMutex);
    m_stop.store(true); m_wake.notify_all();
}
bool CwSerialTransmitter::waitUntil(std::chrono::steady_clock::time_point deadline) {
    std::unique_lock<std::mutex> lock(m_waitMutex);
    return !m_wake.wait_until(lock, deadline, [this] { return m_stop.load(); });
}
void CwSerialTransmitter::run() {
    using Clock = std::chrono::steady_clock;
#ifdef Q_OS_WIN
    struct TimerResolution {
        bool active = timeBeginPeriod(1) == TIMERR_NOERROR;
        ~TimerResolution() { if (active) timeEndPeriod(1); }
    } timerResolution;
#endif
    QString error;
    bool natural = false;
    const auto sequence = cwMorseSequence(m_text);
    if (m_config.port.trimmed().isEmpty() ||
        (m_config.keyLine != "rts" && m_config.keyLine != "dtr") ||
        (m_config.pttLine != "none" && m_config.pttLine != "rts" && m_config.pttLine != "dtr") ||
        m_config.keyLine == m_config.pttLine || !std::isfinite(m_wpm) || m_wpm < 5 || m_wpm > 80 ||
        m_config.leadMs < 0 || m_config.leadMs > 1000 || m_config.tailMs < 0 || m_config.tailMs > 2000 ||
        m_text.size() > 8192 || (!m_releaseOnly && sequence.isEmpty())) {
        complete(false, QStringLiteral("Invalid CW KEY configuration, speed or message.")); return;
    }
    double totalUnits = 0;
    for (const auto& segment : sequence) totalUnits += segment.units;
    const double dotUs = 1200000.0 / m_wpm;
    if (totalUnits * dotUs > 900000000.0) {
        complete(false, QStringLiteral("CW KEY message exceeds 15 minutes; split it into shorter messages.")); return;
    }
    if (m_stop.load()) { complete(false, {}); return; }
    auto output = m_factory ? m_factory() : std::make_unique<SerialOutput>();
    if (!output || !output->open(m_config.port)) {
        complete(false, QStringLiteral("CW KEY cannot open %1: %2").arg(m_config.port, output ? output->errorString() : QStringLiteral("No serial output"))); return;
    }
    auto line = [&](const QString& name, bool asserted) {
        if (!output->setLine(name, asserted)) {
            if (error.isEmpty()) error = QStringLiteral("CW KEY %1 %2 failed on %3: %4")
                .arg(name.toUpper(), asserted ? QStringLiteral("ON") : QStringLiteral("OFF"), m_config.port, output->errorString());
            return false;
        }
        return true;
    };
    const bool rtsReady = line("rts", false);
    const bool dtrReady = line("dtr", false);
    if (!m_releaseOnly && rtsReady && dtrReady && !m_stop.load()) {
        bool ready = true;
        if (m_config.pttLine != "none") {
            ready = line(m_config.pttLine, true);
            if (ready) ready = waitUntil(Clock::now() + std::chrono::milliseconds(m_config.leadMs));
        }
        auto origin = Clock::now();
        double units = 0;
        bool announced = false;
        for (const auto& segment : sequence) {
            if (!ready || m_stop.load()) break;
            if (!line(m_config.keyLine, segment.down)) break;
            if (segment.down && !announced) { announced = true; emit keyingStarted(); }
            if (segment.down && segment.textPosition >= 0) emit characterSent(units / totalUnits, segment.textPosition);
            units += segment.units;
            const auto deadline = origin + std::chrono::microseconds(std::llround(units * dotUs));
            if (!waitUntil(deadline)) break;
            // Do not compress overdue edges into an unintelligible burst.
            const auto lateUs = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - deadline).count();
            if (lateUs > std::clamp(dotUs * .35, 3000.0, 40000.0)) {
                error = QStringLiteral("CW KEY timing overrun; transmission stopped instead of catching up."); break;
            }
            if (units >= totalUnits) natural = true;
        }
    }
    // Mandatory release is attempted even after Stop, a failed edge or an unplug.
    const bool keyReleased = line(m_config.keyLine, false);
    if (natural && keyReleased && error.isEmpty() && !m_stop.load() && m_config.pttLine != "none")
        waitUntil(Clock::now() + std::chrono::milliseconds(m_config.tailMs));
    const bool rtsReleased = line("rts", false);
    const bool dtrReleased = line("dtr", false);
    m_releaseConfirmed = keyReleased && rtsReleased && dtrReleased;
    output->close();
    natural = natural && !m_stop.load() && error.isEmpty() && keyReleased && rtsReleased && dtrReleased;
    complete(natural, error);
}
