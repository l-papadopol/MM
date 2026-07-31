#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace madmodem::cwskimmer {

struct SelectedToneCwConfig {
  double toneHz = 700.0;
  double bandwidthHz = 120.0;
  double minSnrDb = 3.0;
  double initialWpm = 20.0;
  bool autoWpm = true;
  bool afcEnabled = true;
  bool autoBandwidth = false;
  double afcRangeHz = 20.0;
  std::size_t maxRollingText = 160;
  int decoderChannelNumber = 0;
};

struct SelectedToneCwEvent {
  double timestampSec = 0.0;
  double toneHz = 0.0;
  double trackedToneHz = 0.0;
  double frequencyErrorHz = 0.0;
  double requestedBandwidthHz = 0.0;
  double effectiveBandwidthHz = 0.0;
  double acquisitionBandwidthHz = 0.0;
  double snrDb = -99.0;
  double confidence = 0.0;
  double wpm = 0.0;
  bool trackingConfirmed = false;
  std::string trackingState;
  std::string committedText;
  std::string partialText;
  std::string rollingText;
};

using SelectedToneCwCallback = std::function<void(const SelectedToneCwEvent&)>;

struct SelectedToneCwDiagnosticSample {
  double timestampSec = 0.0;
  double acquisitionEnvelope = 0.0;
  double filteredEnvelope = 0.0;
  double signalLevel = 0.0;
  double thresholdLow = 0.0;
  double thresholdHigh = 0.0;
  double markProbability = 0.0;
  bool qsbErasure = false;
  double qsbErasureStartSec = 0.0;
  double qsbErasureEndSec = 0.0;
  bool keyDown = false;
  double markerHz = 0.0;
  double trackedHz = 0.0;
  double snrDb = -99.0;
  double requestedBandwidthHz = 0.0;
  double effectiveBandwidthHz = 0.0;
  double wpm = 0.0;
  double lockQuality = 0.0;
  bool trackingConfirmed = false;
  double carrierProminenceDb = -99.0;
  double coherentSnrDb = -99.0;
  double coherence = 0.0;
};

struct SelectedToneCwSpectrumFrame {
  double timestampSec = 0.0;
  double markerHz = 0.0;
  double trackedHz = 0.0;
  double effectiveBandwidthHz = 0.0;
  double interfererHz = 0.0;
  double interfererConfidence = 0.0;
  double carrierProminenceDb = -99.0;
  double carrierPeakWidthHz = 0.0;
  double carrierToSecondDb = 0.0;
  std::vector<float> offsetsHz;
  std::vector<float> inputPsdDb;
  std::vector<float> filteredPsdDb;
  std::vector<float> theoreticalResponseDb;
};

using SelectedToneCwDiagnosticCallback =
    std::function<void(const SelectedToneCwDiagnosticSample&)>;
using SelectedToneCwSpectrumCallback =
    std::function<void(const SelectedToneCwSpectrumFrame&)>;

/**
 * Native exact-tone CW receiver. RX A and RX B instantiate this class
 * independently. The receiver produces one-millisecond soft MARK evidence;
 * only CwBayesianDecoder decides dots, dashes and separators.
 */
class SelectedToneCwTracker {
public:
  explicit SelectedToneCwTracker(SelectedToneCwConfig config = {});
  ~SelectedToneCwTracker();

  SelectedToneCwTracker(const SelectedToneCwTracker&) = delete;
  SelectedToneCwTracker& operator=(const SelectedToneCwTracker&) = delete;
  SelectedToneCwTracker(SelectedToneCwTracker&&) noexcept;
  SelectedToneCwTracker& operator=(SelectedToneCwTracker&&) noexcept;

  void setCallback(SelectedToneCwCallback callback);
  void setDiagnosticCallback(SelectedToneCwDiagnosticCallback callback);
  void setSpectrumCallback(SelectedToneCwSpectrumCallback callback);
  void setToneHz(double toneHz);
  void setBandwidthHz(double bandwidthHz);
  void setMinSnrDb(double minSnrDb);
  void setWpmHint(double wpm);
  void setAutoWpm(bool enabled);
  void setAfcEnabled(bool enabled);
  void setAutoBandwidth(bool enabled);
  void setAfcRangeHz(double rangeHz);
  const SelectedToneCwConfig& config() const;

  void reset();
  void processFloatMono(const float* samples, std::size_t count,
                        double sourceSampleRate);
  void flush();

  double snrDb() const;
  double confidence() const;
  double wpm() const;
  double trackedToneHz() const;
  double frequencyErrorHz() const;
  double effectiveBandwidthHz() const;
  double acquisitionBandwidthHz() const;
  std::string trackingState() const;
  const std::string& rollingText() const;
  const std::string& partialText() const;

private:
  class Impl;
  std::unique_ptr<Impl> m_impl;
};

} // namespace madmodem::cwskimmer
