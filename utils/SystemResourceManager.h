#ifndef SYSTEMRESOURCEMANAGER_H
#define SYSTEMRESOURCEMANAGER_H

#include <QString>
#include <QtGlobal>

#include <atomic>
#include <mutex>

namespace MadModemRuntime {

enum class WorkClass
{
    FtGate,
    FtBoundary,
    FtOsd,
    FtOffline
};

struct SystemTopology
{
    int logicalProcessors = 1;
    int physicalCores = 1;
    int performanceCores = 0;
    int efficiencyCores = 0;
    int processorGroups = 1;
    int smtWidth = 1;
    QString affinityDescription;
};

struct RuntimeResourceSnapshot
{
    SystemTopology topology;
    int poolCapacity = 1;
    int reservedLogicalProcessors = 1;
    int liveWorkerTarget = 1;
    int gateWorkerTarget = 1;
    int boundaryWorkerTarget = 1;
    int osdWorkerTarget = 1;
    double audioQueueLatencyMs = 0.0;
    double guiFrameMs = 0.0;
    double waterfallFrameMs = 0.0;
    double waterfallQueueRows = 0.0;
    bool waterfallGpuBacked = false;
    double systemCpuLoadPercent = -1.0;
    QString lastAdjustment;
};

/**
 * Process-wide adaptive resource controller for live DSP work.
 *
 * The controller discovers the processors actually available to the process,
 * keeps FT worker budgets separate for gate/boundary/OSD/offline work and
 * adjusts only between jobs.  Audio callbacks and the GUI never execute work
 * from the FT worker pool.
 */
class SystemResourceManager
{
public:
    static SystemResourceManager &instance();

    const SystemTopology &topology() const noexcept;
    int poolCapacity() const noexcept;
    int recommendedWorkers(WorkClass workClass, int itemCount) const noexcept;
    void configureCurrentWorkerThread(int workerIndex) const noexcept;

    // Starts a new continuous FT capture session.  A mode change or RX
    // stop/start must not inherit a worker budget that was reduced by an old
    // capture's transient audio backlog.
    void beginFtCapture(const QString &modeName) noexcept;

    bool tryAcquireOsdPermit() noexcept;
    void releaseOsdPermit() noexcept;

    void observeFtJob(WorkClass workClass,
                      double elapsedMs,
                      double audioQueueLatencyMs,
                      int workersUsed,
                      int itemCount) noexcept;
    void observeGuiFrame(double frameMs) noexcept;
    void observeWaterfallFrame(double frameMs, int queuedRows, bool gpuBacked) noexcept;

    RuntimeResourceSnapshot snapshot() const;
    QString startupSummary() const;
    QString runtimeSummary() const;

private:
    SystemResourceManager();

    static SystemTopology detectTopology();
    static double clampSample(double value, double maximum) noexcept;
    static void updateEwma(double &target, double sample, double alpha) noexcept;
    double sampleSystemCpuLoadLocked() noexcept;
    void maybeAdjustLocked(WorkClass workClass,
                           double elapsedMs,
                           int workersUsed,
                           int itemCount) noexcept;

private:
    SystemTopology m_topology;
    int m_poolCapacity = 1;
    int m_reservedLogicalProcessors = 1;
    int m_maxLiveWorkers = 1;
    std::atomic<int> m_liveWorkerTarget {1};
    std::atomic<int> m_activeOsdWorkers {0};

    mutable std::mutex m_mutex;
    double m_audioQueueLatencyEwmaMs = 0.0;
    double m_guiFrameEwmaMs = 0.0;
    double m_waterfallFrameEwmaMs = 0.0;
    double m_waterfallQueueRowsEwma = 0.0;
    bool m_waterfallGpuBacked = false;
    double m_systemCpuLoadPercent = -1.0;
    int m_stableJobCount = 0;
    int m_audioPressureJobCount = 0;
    int m_adjustmentCooldown = 0;
    QString m_lastAdjustment;
    QString m_activeFtMode;

    quint64 m_cpuPrevTotal = 0;
    quint64 m_cpuPrevIdle = 0;
};

} // namespace MadModemRuntime

#endif // SYSTEMRESOURCEMANAGER_H
