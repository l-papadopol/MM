#include "SystemResourceManager.h"

#include <QByteArray>
#include <QFile>
#include <QSet>
#include <QTextStream>
#include <QThread>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <thread>
#include <vector>

#if defined(Q_OS_LINUX)
#include <sched.h>
#include <unistd.h>
#elif defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(Q_OS_MACOS)
#include <sys/sysctl.h>
#endif

namespace MadModemRuntime {
namespace {

int fallbackLogicalCount()
{
    const int qtCount = QThread::idealThreadCount();
    if (qtCount > 0) {
        return qtCount;
    }
    const unsigned int stdCount = std::thread::hardware_concurrency();
    return stdCount > 0 ? static_cast<int>(stdCount) : 1;
}

#if defined(Q_OS_LINUX)
int readSysfsInt(const QString &path, int fallback = -1)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return fallback;
    }
    bool ok = false;
    const int value = QString::fromLatin1(file.readAll()).trimmed().toInt(&ok);
    return ok ? value : fallback;
}
#endif

#if defined(Q_OS_MACOS)
int sysctlInt(const char *name, int fallback)
{
    int value = fallback;
    size_t size = sizeof(value);
    if (sysctlbyname(name, &value, &size, nullptr, 0) != 0 || size != sizeof(value)) {
        return fallback;
    }
    return value;
}
#endif

QString workClassName(WorkClass workClass)
{
    switch (workClass) {
    case WorkClass::FtGate: return QStringLiteral("gate");
    case WorkClass::FtBoundary: return QStringLiteral("boundary");
    case WorkClass::FtOsd: return QStringLiteral("OSD");
    case WorkClass::FtOffline: return QStringLiteral("offline");
    }
    return QStringLiteral("unknown");
}

} // namespace

SystemResourceManager &SystemResourceManager::instance()
{
    static SystemResourceManager manager;
    return manager;
}

SystemResourceManager::SystemResourceManager()
    : m_topology(detectTopology())
{
    const int logical = qMax(1, m_topology.logicalProcessors);
    m_poolCapacity = logical;

    if (logical <= 2) {
        m_reservedLogicalProcessors = 1;
    } else if (logical <= 6) {
        m_reservedLogicalProcessors = 1;
    } else {
        m_reservedLogicalProcessors = qMax(2, static_cast<int>(std::ceil(static_cast<double>(logical) * 0.10)));
    }
    m_reservedLogicalProcessors = qMin(m_reservedLogicalProcessors, qMax(1, logical - 1));
    m_maxLiveWorkers = qMax(1, logical - m_reservedLogicalProcessors);
    m_liveWorkerTarget.store(m_maxLiveWorkers, std::memory_order_release);
    m_lastAdjustment = QStringLiteral("initial topology budget");
}

const SystemTopology &SystemResourceManager::topology() const noexcept
{
    return m_topology;
}

int SystemResourceManager::poolCapacity() const noexcept
{
    return m_poolCapacity;
}

int SystemResourceManager::recommendedWorkers(WorkClass workClass, int itemCount) const noexcept
{
    const int items = qMax(1, itemCount);
    const int live = qBound(1, m_liveWorkerTarget.load(std::memory_order_acquire), m_maxLiveWorkers);
    int target = live;
    switch (workClass) {
    case WorkClass::FtGate:
        // The early gate has few expensive candidates.  More than roughly 40%
        // of the live budget increases wake-up/synchronisation cost without
        // improving the sequencer deadline.
        target = qMax(1, static_cast<int>(std::ceil(static_cast<double>(live) * 0.40)));
        break;
    case WorkClass::FtBoundary:
        target = live;
        break;
    case WorkClass::FtOsd:
        target = qMax(1, static_cast<int>(std::ceil(static_cast<double>(live) * 0.30)));
        break;
    case WorkClass::FtOffline:
        target = m_poolCapacity;
        break;
    }
    return qBound(1, target, items);
}

void SystemResourceManager::beginFtCapture(const QString &modeName) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const QString mode = modeName.trimmed().toUpper().isEmpty()
        ? QStringLiteral("FT")
        : modeName.trimmed().toUpper();

    // Audio queue latency is capture-local.  Carrying its EWMA and a reduced
    // worker target across FT8 -> FT4 (or an RX restart) made the next mode
    // begin with only one worker even though the machine was idle.
    m_activeFtMode = mode;
    m_audioQueueLatencyEwmaMs = 0.0;
    m_audioPressureJobCount = 0;
    m_stableJobCount = 0;
    m_adjustmentCooldown = 1;
    m_liveWorkerTarget.store(m_maxLiveWorkers, std::memory_order_release);
    m_lastAdjustment = QStringLiteral("%1 capture reset to topology budget (%2 workers)")
        .arg(mode)
        .arg(m_maxLiveWorkers);
}


void SystemResourceManager::configureCurrentWorkerThread(int workerIndex) const noexcept
{
#if defined(Q_OS_WIN)
    const WORD activeGroupCount = GetActiveProcessorGroupCount();
    if (activeGroupCount <= 1) {
        return;
    }

    std::vector<USHORT> groups(static_cast<size_t>(activeGroupCount));
    USHORT groupCount = activeGroupCount;
    if (!GetProcessGroupAffinity(GetCurrentProcess(), &groupCount, groups.data()) || groupCount == 0) {
        groupCount = activeGroupCount;
        groups.resize(static_cast<size_t>(groupCount));
        for (USHORT group = 0; group < groupCount; ++group) {
            groups[static_cast<size_t>(group)] = group;
        }
    } else {
        groups.resize(static_cast<size_t>(groupCount));
    }

    const USHORT group = groups[static_cast<size_t>(qAbs(workerIndex) % qMax<int>(1, groupCount))];
    const DWORD activeProcessors = GetActiveProcessorCount(group);
    if (activeProcessors == 0) {
        return;
    }

    GROUP_AFFINITY affinity{};
    affinity.Group = group;
    if (activeProcessors >= sizeof(KAFFINITY) * 8U) {
        affinity.Mask = ~static_cast<KAFFINITY>(0);
    } else {
        affinity.Mask = (static_cast<KAFFINITY>(1) << activeProcessors) - 1;
    }
    SetThreadGroupAffinity(GetCurrentThread(), &affinity, nullptr);
#else
    Q_UNUSED(workerIndex)
#endif
}

bool SystemResourceManager::tryAcquireOsdPermit() noexcept
{
    const int limit = recommendedWorkers(WorkClass::FtOsd, m_poolCapacity);
    int active = m_activeOsdWorkers.load(std::memory_order_relaxed);
    while (active < limit) {
        if (m_activeOsdWorkers.compare_exchange_weak(active, active + 1,
                                                     std::memory_order_acq_rel,
                                                     std::memory_order_relaxed)) {
            return true;
        }
    }
    return false;
}

void SystemResourceManager::releaseOsdPermit() noexcept
{
    const int previous = m_activeOsdWorkers.fetch_sub(1, std::memory_order_acq_rel);
    if (previous <= 0) {
        m_activeOsdWorkers.store(0, std::memory_order_release);
    }
}

double SystemResourceManager::clampSample(double value, double maximum) noexcept
{
    if (!std::isfinite(value) || value < 0.0) {
        return 0.0;
    }
    return qMin(value, maximum);
}

void SystemResourceManager::updateEwma(double &target, double sample, double alpha) noexcept
{
    if (sample <= 0.0) {
        return;
    }
    if (target <= 0.0) {
        target = sample;
    } else {
        target = alpha * sample + (1.0 - alpha) * target;
    }
}

void SystemResourceManager::observeFtJob(WorkClass workClass,
                                         double elapsedMs,
                                         double audioQueueLatencyMs,
                                         int workersUsed,
                                         int itemCount) noexcept
{
    if (workClass == WorkClass::FtOffline) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    updateEwma(m_audioQueueLatencyEwmaMs, clampSample(audioQueueLatencyMs, 5000.0), 0.28);
    m_systemCpuLoadPercent = sampleSystemCpuLoadLocked();
    maybeAdjustLocked(workClass, clampSample(elapsedMs, 30000.0), workersUsed, itemCount);
}

void SystemResourceManager::observeGuiFrame(double frameMs) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    updateEwma(m_guiFrameEwmaMs, clampSample(frameMs, 1000.0), 0.18);
}

void SystemResourceManager::observeWaterfallFrame(double frameMs, int queuedRows, bool gpuBacked) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    updateEwma(m_waterfallFrameEwmaMs, clampSample(frameMs, 1000.0), 0.22);
    updateEwma(m_waterfallQueueRowsEwma, clampSample(static_cast<double>(qMax(0, queuedRows)), 4096.0), 0.28);
    m_waterfallGpuBacked = gpuBacked;
}

void SystemResourceManager::maybeAdjustLocked(WorkClass workClass,
                                              double elapsedMs,
                                              int workersUsed,
                                              int itemCount) noexcept
{
    if (m_adjustmentCooldown > 0) {
        --m_adjustmentCooldown;
    }

    const int current = qBound(1, m_liveWorkerTarget.load(std::memory_order_relaxed), m_maxLiveWorkers);
    const bool cpuPressure = m_systemCpuLoadPercent >= 98.0;
    // The FT table may legitimately spend hundreds of milliseconds inserting
    // 20-30 decoded rows after a boundary job has already completed.  That is
    // not concurrent decoder starvation and must not ratchet the next slot down
    // to one worker.  React to actual presentation backlog, or to an extreme UI
    // batch only when the whole system is also saturated.
    const bool guiPressure = m_waterfallFrameEwmaMs > 32.0 ||
                             m_waterfallQueueRowsEwma > 24.0 ||
                             (m_guiFrameEwmaMs > 500.0 && cpuPressure);
    // A 48 kHz backend may legitimately deliver 4096-frame chunks roughly
    // every 85 ms.  One scheduling spike around 140-180 ms is therefore not
    // evidence that FT workers are starving capture.  Require sustained,
    // current backlog before reducing parallelism.
    const bool audioPressureSample = m_audioQueueLatencyEwmaMs > 220.0;
    if (audioPressureSample) {
        ++m_audioPressureJobCount;
    } else {
        m_audioPressureJobCount = qMax(0, m_audioPressureJobCount - 1);
    }
    const bool audioPressure = m_audioPressureJobCount >= 2;
    const bool severeAudioPressure = m_audioQueueLatencyEwmaMs > 450.0;
    const bool saturated = workersUsed >= qMax(1, current - 1) && itemCount > workersUsed * 3;
    const double targetMs = (workClass == WorkClass::FtGate) ? 250.0 : 900.0;

    if ((audioPressure || guiPressure || (cpuPressure && elapsedMs > targetMs)) &&
        current > 1 && m_adjustmentCooldown == 0) {
        const int step = severeAudioPressure
            ? qMax(2, m_topology.logicalProcessors / 12)
            : qMax(1, m_topology.logicalProcessors / 20);
        const int next = qMax(1, current - step);
        m_liveWorkerTarget.store(next, std::memory_order_release);
        m_stableJobCount = 0;
        m_adjustmentCooldown = 2;
        m_lastAdjustment = QStringLiteral("%1→%2 workers after %3 pressure (audio %4 ms, GUI %5 ms, waterfall %6 ms, CPU %7%)")
            .arg(current)
            .arg(next)
            .arg(audioPressure ? QStringLiteral("audio") : (guiPressure ? QStringLiteral("GUI") : QStringLiteral("CPU")))
            .arg(m_audioQueueLatencyEwmaMs, 0, 'f', 1)
            .arg(m_guiFrameEwmaMs, 0, 'f', 1)
            .arg(m_waterfallFrameEwmaMs, 0, 'f', 1)
            .arg(m_systemCpuLoadPercent, 0, 'f', 1);
        return;
    }

    const bool responsive = m_audioQueueLatencyEwmaMs < 95.0 &&
                            (m_waterfallFrameEwmaMs <= 0.0 || m_waterfallFrameEwmaMs < 23.0) &&
                            m_waterfallQueueRowsEwma < 6.0 &&
                            (m_systemCpuLoadPercent < 0.0 || m_systemCpuLoadPercent < 96.0);
    const bool usefulMoreParallelism = saturated && elapsedMs > targetMs * 0.65;
    if (responsive && usefulMoreParallelism && current < m_maxLiveWorkers) {
        ++m_stableJobCount;
        if (m_stableJobCount >= 3 && m_adjustmentCooldown == 0) {
            const int next = qMin(m_maxLiveWorkers, current + 1);
            m_liveWorkerTarget.store(next, std::memory_order_release);
            m_stableJobCount = 0;
            m_adjustmentCooldown = 3;
            m_lastAdjustment = QStringLiteral("%1→%2 workers after three responsive %3 jobs")
                .arg(current)
                .arg(next)
                .arg(workClassName(workClass));
        }
    } else {
        m_stableJobCount = 0;
    }
}

RuntimeResourceSnapshot SystemResourceManager::snapshot() const
{
    RuntimeResourceSnapshot out;
    out.topology = m_topology;
    out.poolCapacity = m_poolCapacity;
    out.reservedLogicalProcessors = m_reservedLogicalProcessors;
    out.liveWorkerTarget = qBound(1, m_liveWorkerTarget.load(std::memory_order_acquire), m_maxLiveWorkers);
    out.gateWorkerTarget = recommendedWorkers(WorkClass::FtGate, m_poolCapacity);
    out.boundaryWorkerTarget = recommendedWorkers(WorkClass::FtBoundary, m_poolCapacity);
    out.osdWorkerTarget = recommendedWorkers(WorkClass::FtOsd, m_poolCapacity);
    std::lock_guard<std::mutex> lock(m_mutex);
    out.audioQueueLatencyMs = m_audioQueueLatencyEwmaMs;
    out.guiFrameMs = m_guiFrameEwmaMs;
    out.waterfallFrameMs = m_waterfallFrameEwmaMs;
    out.waterfallQueueRows = m_waterfallQueueRowsEwma;
    out.waterfallGpuBacked = m_waterfallGpuBacked;
    out.systemCpuLoadPercent = m_systemCpuLoadPercent;
    out.lastAdjustment = m_lastAdjustment;
    return out;
}

QString SystemResourceManager::startupSummary() const
{
    const RuntimeResourceSnapshot s = snapshot();
    return QStringLiteral("CPU resources: physical cores=%1, logical available=%2, SMT=%3, P/E=%4/%5, processor groups=%6, FT pool=%7, reserved=%8, live gate/boundary/OSD=%9/%10/%11, affinity=%12")
        .arg(s.topology.physicalCores)
        .arg(s.topology.logicalProcessors)
        .arg(s.topology.smtWidth)
        .arg(s.topology.performanceCores)
        .arg(s.topology.efficiencyCores)
        .arg(s.topology.processorGroups)
        .arg(s.poolCapacity)
        .arg(s.reservedLogicalProcessors)
        .arg(s.gateWorkerTarget)
        .arg(s.boundaryWorkerTarget)
        .arg(s.osdWorkerTarget)
        .arg(s.topology.affinityDescription.isEmpty() ? QStringLiteral("system default") : s.topology.affinityDescription);
}

QString SystemResourceManager::runtimeSummary() const
{
    const RuntimeResourceSnapshot s = snapshot();
    return QStringLiteral("FT resources: live target %1 (gate %2, boundary %3, OSD %4), audio q %5 ms, GUI %6 ms, waterfall %7 ms/q%8 (%9), CPU %10%, last: %11")
        .arg(s.liveWorkerTarget)
        .arg(s.gateWorkerTarget)
        .arg(s.boundaryWorkerTarget)
        .arg(s.osdWorkerTarget)
        .arg(s.audioQueueLatencyMs, 0, 'f', 1)
        .arg(s.guiFrameMs, 0, 'f', 1)
        .arg(s.waterfallFrameMs, 0, 'f', 1)
        .arg(s.waterfallQueueRows, 0, 'f', 1)
        .arg(s.waterfallGpuBacked ? QStringLiteral("GPU") : QStringLiteral("CPU"))
        .arg(s.systemCpuLoadPercent, 0, 'f', 1)
        .arg(s.lastAdjustment);
}

SystemTopology SystemResourceManager::detectTopology()
{
    SystemTopology t;
    t.logicalProcessors = fallbackLogicalCount();
    t.physicalCores = qMax(1, t.logicalProcessors);

#if defined(Q_OS_LINUX)
    cpu_set_t set;
    CPU_ZERO(&set);
    QSet<int> allowed;
    if (sched_getaffinity(0, sizeof(set), &set) == 0) {
        for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu) {
            if (CPU_ISSET(cpu, &set)) {
                allowed.insert(cpu);
            }
        }
    }
    if (!allowed.isEmpty()) {
        t.logicalProcessors = allowed.size();
        t.affinityDescription = QStringLiteral("sched_getaffinity %1 CPU(s)").arg(allowed.size());
    } else {
        for (int cpu = 0; cpu < t.logicalProcessors; ++cpu) {
            allowed.insert(cpu);
        }
        t.affinityDescription = QStringLiteral("affinity unavailable; online CPU count");
    }

    QSet<QString> physicalIds;
    QSet<QString> performanceIds;
    QSet<QString> efficiencyIds;
    for (int cpu : allowed) {
        const QString base = QStringLiteral("/sys/devices/system/cpu/cpu%1/topology/").arg(cpu);
        const int core = readSysfsInt(base + QStringLiteral("core_id"), cpu);
        const int package = readSysfsInt(base + QStringLiteral("physical_package_id"), 0);
        const QString physicalId = QStringLiteral("%1:%2").arg(package).arg(core);
        physicalIds.insert(physicalId);
        // Linux exposes Intel hybrid core types when the kernel/CPU supports it:
        // 1 = Atom/E-core, 2 = Core/P-core. Unknown values remain unclassified.
        const int coreType = readSysfsInt(base + QStringLiteral("core_type"), 0);
        if (coreType == 2) {
            performanceIds.insert(physicalId);
        } else if (coreType == 1) {
            efficiencyIds.insert(physicalId);
        }
    }
    if (!physicalIds.isEmpty()) {
        t.physicalCores = physicalIds.size();
    }
    t.performanceCores = performanceIds.size();
    t.efficiencyCores = efficiencyIds.size();
#elif defined(Q_OS_WIN)
    const WORD activeGroupCount = GetActiveProcessorGroupCount();
    std::vector<USHORT> processGroups(static_cast<size_t>(qMax<WORD>(1, activeGroupCount)));
    USHORT processGroupCount = static_cast<USHORT>(processGroups.size());
    if (!GetProcessGroupAffinity(GetCurrentProcess(), &processGroupCount, processGroups.data()) ||
        processGroupCount == 0) {
        processGroupCount = activeGroupCount;
        processGroups.resize(static_cast<size_t>(processGroupCount));
        for (USHORT group = 0; group < processGroupCount; ++group) {
            processGroups[static_cast<size_t>(group)] = group;
        }
    } else {
        processGroups.resize(static_cast<size_t>(processGroupCount));
    }
    t.processorGroups = qMax(1, static_cast<int>(processGroupCount));
    int logical = 0;
    for (USHORT group : processGroups) {
        logical += static_cast<int>(GetActiveProcessorCount(group));
    }
    if (logical > 0) {
        t.logicalProcessors = logical;
    }

    DWORD bytes = 0;
    GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &bytes);
    if (bytes > 0) {
        QByteArray buffer(static_cast<int>(bytes), char(0));
        if (GetLogicalProcessorInformationEx(RelationProcessorCore,
                                             reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data()),
                                             &bytes)) {
            struct VisibleCore
            {
                BYTE efficiencyClass = 0;
            };
            std::vector<VisibleCore> visibleCores;
            BYTE maximumEfficiencyClass = 0;
            DWORD offset = 0;
            while (offset < bytes) {
                const auto *entry = reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *>(buffer.constData() + offset);
                if (entry->Relationship == RelationProcessorCore) {
                    bool visibleToProcess = false;
                    for (WORD maskIndex = 0; maskIndex < entry->Processor.GroupCount; ++maskIndex) {
                        const WORD entryGroup = entry->Processor.GroupMask[maskIndex].Group;
                        if (std::find(processGroups.begin(), processGroups.end(), entryGroup) != processGroups.end()) {
                            visibleToProcess = true;
                            break;
                        }
                    }
                    if (visibleToProcess) {
                        visibleCores.push_back({entry->Processor.EfficiencyClass});
                        maximumEfficiencyClass = qMax(maximumEfficiencyClass, entry->Processor.EfficiencyClass);
                    }
                }
                if (entry->Size == 0) break;
                offset += entry->Size;
            }
            if (!visibleCores.empty()) {
                t.physicalCores = static_cast<int>(visibleCores.size());
                // Windows defines larger EfficiencyClass values as faster cores.
                // Homogeneous CPUs commonly report zero for every core; in that
                // case classify all cores as performance-equivalent.
                if (maximumEfficiencyClass == 0) {
                    t.performanceCores = t.physicalCores;
                    t.efficiencyCores = 0;
                } else {
                    for (const VisibleCore &core : visibleCores) {
                        if (core.efficiencyClass == maximumEfficiencyClass) {
                            ++t.performanceCores;
                        } else {
                            ++t.efficiencyCores;
                        }
                    }
                }
            }
        }
    }
    t.affinityDescription = QStringLiteral("process affinity spans %1 Windows processor group(s)").arg(t.processorGroups);
#elif defined(Q_OS_MACOS)
    t.logicalProcessors = qMax(1, sysctlInt("hw.logicalcpu", t.logicalProcessors));
    t.physicalCores = qMax(1, sysctlInt("hw.physicalcpu", t.physicalCores));
    t.performanceCores = qMax(0, sysctlInt("hw.perflevel0.physicalcpu", 0));
    t.efficiencyCores = qMax(0, sysctlInt("hw.perflevel1.physicalcpu", 0));
    t.affinityDescription = QStringLiteral("macOS scheduler-visible CPUs");
#else
    t.affinityDescription = QStringLiteral("portable hardware_concurrency fallback");
#endif

    t.logicalProcessors = qMax(1, t.logicalProcessors);
    t.physicalCores = qBound(1, t.physicalCores, t.logicalProcessors);
    t.smtWidth = qMax(1, static_cast<int>(std::ceil(static_cast<double>(t.logicalProcessors) /
                                                   static_cast<double>(t.physicalCores))));
    return t;
}

double SystemResourceManager::sampleSystemCpuLoadLocked() noexcept
{
#if defined(Q_OS_LINUX)
    QFile file(QStringLiteral("/proc/stat"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return m_systemCpuLoadPercent;
    }
    const QByteArray line = file.readLine();
    const QList<QByteArray> fields = line.simplified().split(' ');
    if (fields.size() < 8 || fields.first() != "cpu") {
        return m_systemCpuLoadPercent;
    }
    quint64 values[10] = {};
    const int count = qMin(10, fields.size() - 1);
    for (int i = 0; i < count; ++i) {
        bool ok = false;
        values[i] = fields.at(i + 1).toULongLong(&ok);
        if (!ok) values[i] = 0;
    }
    const quint64 idle = values[3] + values[4];
    quint64 total = 0;
    for (int i = 0; i < count; ++i) total += values[i];
#elif defined(Q_OS_WIN)
    FILETIME idleFt{}, kernelFt{}, userFt{};
    if (!GetSystemTimes(&idleFt, &kernelFt, &userFt)) {
        return m_systemCpuLoadPercent;
    }
    auto to64 = [](const FILETIME &ft) -> quint64 {
        return (static_cast<quint64>(ft.dwHighDateTime) << 32U) | static_cast<quint64>(ft.dwLowDateTime);
    };
    const quint64 idle = to64(idleFt);
    const quint64 total = to64(kernelFt) + to64(userFt);
#else
    return m_systemCpuLoadPercent;
#endif

#if defined(Q_OS_LINUX) || defined(Q_OS_WIN)
    if (m_cpuPrevTotal == 0 || total <= m_cpuPrevTotal) {
        m_cpuPrevTotal = total;
        m_cpuPrevIdle = idle;
        return m_systemCpuLoadPercent;
    }
    const quint64 totalDelta = total - m_cpuPrevTotal;
    const quint64 idleDelta = idle >= m_cpuPrevIdle ? idle - m_cpuPrevIdle : 0;
    m_cpuPrevTotal = total;
    m_cpuPrevIdle = idle;
    if (totalDelta == 0) {
        return m_systemCpuLoadPercent;
    }
    return qBound(0.0, 100.0 * (1.0 - static_cast<double>(idleDelta) / static_cast<double>(totalDelta)), 100.0);
#endif
}

} // namespace MadModemRuntime
