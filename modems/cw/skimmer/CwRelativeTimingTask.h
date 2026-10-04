#pragma once

#include "CwRelativeTimingDecoder.h"

#include <condition_variable>
#include <deque>
#include <future>
#include <mutex>
#include <thread>

namespace madmodem::cwskimmer {

/**
 * Dedicated temporal task for one CW receiver.
 *
 * The carrier discriminator timestamps MARK/SPACE runs in the DSP path and
 * submits them here.  The worker owns the complete timing model; no audio,
 * filters or FFT state cross this boundary.  Results are drained by the owner
 * thread, so UI callbacks never execute on the temporal worker itself.
 */
class CwRelativeTimingTask {
public:
  explicit CwRelativeTimingTask(CwRelativeTimingConfig config = {});
  ~CwRelativeTimingTask();

  CwRelativeTimingTask(const CwRelativeTimingTask&) = delete;
  CwRelativeTimingTask& operator=(const CwRelativeTimingTask&) = delete;

  void setConfig(const CwRelativeTimingConfig& config);
  void reset(bool keepTimingPrior = false);
  void beginEpoch(double shortMarkMs, double longMarkMs,
                  double elementSpaceMs,
                  bool keepContinuityAlternative = true);
  void submitRun(const CwLogicRun& run);
  void submitAdvance(double timestampSec, bool keyDown);
  void flush(double timestampSec = 0.0);

  bool takeResult(CwRelativeTimingResult& result);
  CwRelativeTimingResult snapshot() const;

private:
  enum class CommandType {
    SetConfig, Reset, BeginEpoch, Run, Advance, Flush, Stop
  };

  struct Command {
    CommandType type = CommandType::Advance;
    CwRelativeTimingConfig config;
    CwLogicRun run;
    double timestampSec = 0.0;
    double value1 = 0.0;
    double value2 = 0.0;
    double value3 = 0.0;
    bool flag = false;
    std::shared_ptr<std::promise<void>> completion;
  };

  void enqueue(Command command, bool coalesceAdvance = false);
  void enqueueAndWait(Command command);
  void workerMain(CwRelativeTimingConfig initialConfig);
  void publish(const CwRelativeTimingResult& result);
  void stop();

private:
  mutable std::mutex m_queueMutex;
  std::condition_variable m_queueReady;
  std::deque<Command> m_commands;
  bool m_stopping = false;

  mutable std::mutex m_resultMutex;
  CwRelativeTimingResult m_latest;
  std::string m_pendingCommitted;
  std::vector<std::string> m_pendingCommittedPatterns;
  bool m_resultDirty = false;

  // Declared last so every queue/result primitive is fully constructed before
  // the worker can enter workerMain().
  std::thread m_worker;
};

} // namespace madmodem::cwskimmer
