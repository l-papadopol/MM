#include "CwRelativeTimingTask.h"

#include <utility>

namespace madmodem::cwskimmer {

CwRelativeTimingTask::CwRelativeTimingTask(CwRelativeTimingConfig config)
    : m_latest(CwRelativeTimingDecoder(config).snapshot()),
      m_worker(&CwRelativeTimingTask::workerMain, this, config) {}

CwRelativeTimingTask::~CwRelativeTimingTask() {
  stop();
}

void CwRelativeTimingTask::enqueue(Command command, bool coalesceAdvance) {
  {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    if (m_stopping && command.type != CommandType::Stop) return;
    if (coalesceAdvance && command.type == CommandType::Advance &&
        !m_commands.empty() &&
        m_commands.back().type == CommandType::Advance &&
        !m_commands.back().completion) {
      m_commands.back().timestampSec = command.timestampSec;
      m_commands.back().flag = command.flag;
      return;
    }
    m_commands.push_back(std::move(command));
  }
  m_queueReady.notify_one();
}

void CwRelativeTimingTask::enqueueAndWait(Command command) {
  auto completion = std::make_shared<std::promise<void>>();
  std::future<void> finished = completion->get_future();
  command.completion = std::move(completion);
  enqueue(std::move(command));
  finished.get();
}

void CwRelativeTimingTask::setConfig(
    const CwRelativeTimingConfig& config) {
  Command command;
  command.type = CommandType::SetConfig;
  command.config = config;
  enqueueAndWait(std::move(command));
}

void CwRelativeTimingTask::reset(bool keepTimingPrior) {
  Command command;
  command.type = CommandType::Reset;
  command.flag = keepTimingPrior;
  enqueueAndWait(std::move(command));
}

void CwRelativeTimingTask::beginEpoch(
    double shortMarkMs, double longMarkMs, double elementSpaceMs,
    bool keepContinuityAlternative) {
  Command command;
  command.type = CommandType::BeginEpoch;
  command.value1 = shortMarkMs;
  command.value2 = longMarkMs;
  command.value3 = elementSpaceMs;
  command.flag = keepContinuityAlternative;
  enqueueAndWait(std::move(command));
}

void CwRelativeTimingTask::submitRun(const CwLogicRun& run) {
  Command command;
  command.type = CommandType::Run;
  command.run = run;
  enqueue(std::move(command));
}

void CwRelativeTimingTask::submitAdvance(double timestampSec, bool keyDown) {
  Command command;
  command.type = CommandType::Advance;
  command.timestampSec = timestampSec;
  command.flag = keyDown;
  enqueue(std::move(command), true);
}

void CwRelativeTimingTask::flush(double timestampSec) {
  Command command;
  command.type = CommandType::Flush;
  command.timestampSec = timestampSec;
  enqueueAndWait(std::move(command));
}

bool CwRelativeTimingTask::takeResult(CwRelativeTimingResult& result) {
  std::lock_guard<std::mutex> lock(m_resultMutex);
  if (!m_resultDirty && m_pendingCommitted.empty()) return false;
  result = m_latest;
  result.committedText = std::move(m_pendingCommitted);
  result.committedPatterns = std::move(m_pendingCommittedPatterns);
  m_pendingCommitted.clear();
  m_pendingCommittedPatterns.clear();
  m_resultDirty = false;
  return true;
}

CwRelativeTimingResult CwRelativeTimingTask::snapshot() const {
  std::lock_guard<std::mutex> lock(m_resultMutex);
  CwRelativeTimingResult result = m_latest;
  result.committedText.clear();
  result.committedPatterns.clear();
  return result;
}

void CwRelativeTimingTask::publish(const CwRelativeTimingResult& result) {
  std::lock_guard<std::mutex> lock(m_resultMutex);
  if (!result.committedText.empty())
    m_pendingCommitted += result.committedText;
  if (!result.committedPatterns.empty())
    m_pendingCommittedPatterns.insert(m_pendingCommittedPatterns.end(),
                                      result.committedPatterns.begin(),
                                      result.committedPatterns.end());
  m_latest = result;
  m_latest.committedText.clear();
  m_latest.committedPatterns.clear();
  m_resultDirty = true;
}

void CwRelativeTimingTask::workerMain(
    CwRelativeTimingConfig initialConfig) {
  CwRelativeTimingDecoder decoder(initialConfig);
  publish(decoder.snapshot());

  for (;;) {
    Command command;
    {
      std::unique_lock<std::mutex> lock(m_queueMutex);
      m_queueReady.wait(lock, [this]() {
        return !m_commands.empty();
      });
      command = std::move(m_commands.front());
      m_commands.pop_front();
    }

    bool finish = false;
    switch (command.type) {
      case CommandType::SetConfig:
        decoder.setConfig(command.config);
        publish(decoder.snapshot());
        break;
      case CommandType::Reset:
        decoder.reset(command.flag);
        publish(decoder.snapshot());
        break;
      case CommandType::BeginEpoch:
        decoder.beginEpoch(command.value1, command.value2, command.value3,
                           command.flag);
        publish(decoder.snapshot());
        break;
      case CommandType::Run:
        publish(decoder.processRun(command.run));
        break;
      case CommandType::Advance:
        publish(decoder.advance(command.timestampSec, command.flag));
        break;
      case CommandType::Flush:
        publish(decoder.flush(command.timestampSec));
        break;
      case CommandType::Stop:
        finish = true;
        break;
    }

    if (command.completion) command.completion->set_value();
    if (finish) break;
  }
}

void CwRelativeTimingTask::stop() {
  if (!m_worker.joinable()) return;
  {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_stopping = true;
  }
  Command command;
  command.type = CommandType::Stop;
  enqueueAndWait(std::move(command));
  m_worker.join();
}

} // namespace madmodem::cwskimmer
