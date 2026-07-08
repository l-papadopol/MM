#ifndef FT8RXDECODER_H
#define FT8RXDECODER_H

#include "../../audio/AudioBlock.h"
#include "../../third_party/mshv_gpl/port/HvGenFt8/gen_ft8.h"

#include <QObject>
#include <QDateTime>
#include <QString>
#include <QVector>
#include <QSet>

#include <array>
#include <atomic>
#include <future>
#include <functional>
#include <mutex>
#include <vector>

/**
 * @brief First MSHV-derived FT8 RX core for MadModem.
 *
 * This decoder implements the complete live-slot plumbing and a compact native
 * FT8 decoder: 48/44.1 kHz audio is resampled to 12 kHz, accumulated in UTC
 * 15-second windows, Costas candidates are searched over time/frequency, 8-FSK
 * symbols are demodulated into soft metrics, an LDPC(174,91) min-sum decoder is
 * run, the CRC14 is checked, and MSHV's 77-bit unpacker produces the message.
 *
 * The implementation intentionally keeps the class independent from the large
 * MSHV QObject decoder tree so it remains small enough to maintain inside
 * MadModem while still reusing MSHV protocol tables, CRC polynomial and message
 * unpacking code under third_party/mshv_gpl/port.
 */
class Ft8RxDecoder : public QObject
{
    Q_OBJECT

public:
    struct Decode
    {
        QString utc;
        int snrDb = 0;
        double dt = 0.0;
        int frequencyHz = 0;
        QString message;
        double syncScore = 0.0;
        qint64 slotStartUtcMs = 0;
        int slotPeriodMs = 15000;
    };

    struct PerfStats
    {
        QString modeName;       // FT8/FT4 owner of these stats; keeps live rolling averages mode-local
        QString slotUtc;
        int candidateCount = 0;
        int decodeCount = 0;
        int workerCount = 0;
        double candidateSearchMs = 0.0;
        double candidateDecodeMs = 0.0;
        double totalMs = 0.0;
        int passCount = 1;
        int secondPassCandidates = 0;
        int dedupDropped = 0;
        double subtractionMs = 0.0;

        // Live-log behaviour map counters.  These describe what the decoder did
        // with the slot without changing any decoder decisions.
        int pass1RawCandidates = 0;
        int pass1KeptCandidates = 0;
        int pass1PrunedCandidates = 0;
        int pass1Decodes = 0;
        int pass2RawCandidates = 0;
        int pass2KeptCandidates = 0;
        int pass2PrunedCandidates = 0;
        int pass2Decodes = 0;
        int residualRawCandidates = 0;
        int residualSelectedCandidates = 0;
        int residualAcceptedDecodes = 0;
        QString adaptiveDecision;
        QStringList adaptiveReasons;
        bool timeBudgetHit = false;
        QString earlyStopReason;
        QString engineName;
        QString phase;          // "wsjtx-gate ...", "boundary" or "offline" for live diagnostics
        bool offline = false;   // true for Analyze FT WAV; excludes it from live moving averages

        // v3.31 diagnostics: read-only counters. They do not affect candidate
        // ordering, LDPC, unpacking, hash tables or emitted decodes.
        int attemptedCandidates = 0;
        int boundaryRejects = 0;
        int softMetricRejects = 0;
        int syncGateRejects = 0;      // WSJT-X/MSHV ft8b-style hard Costas bail-out before LDPC
        int ldpcTried = 0;            // candidates that survived pre-LDPC sync gate
        int ldpcFailures = 0;
        int crcFailures = 0;
        int unpackFailures = 0;
        int messageRejects = 0;

        // v3.31 diagnostic quality averages. These are computed after symbol
        // extraction and before LDPC/CRC; they are read-only telemetry for
        // the Auto test report and must not affect decoder decisions.
        int decodedQualityCount = 0;
        int ldpcFailureQualityCount = 0;
        double decodedAvgSyncScore = 0.0;
        double ldpcFailureAvgSyncScore = 0.0;
        double decodedAvgHardSync = 0.0;
        double ldpcFailureAvgHardSync = 0.0;
        double decodedAvgLlrAbs = 0.0;
        double ldpcFailureAvgLlrAbs = 0.0;

        // v4.10 unified engine lab diagnostics. These counters are optional
        // telemetry: they do not drive decoder decisions and are used by the
        // WAV Auto test to prove where extra decodes came from.
        int residualAttempts = 0;
        int residualDecodes = 0;
        int osdAttempts = 0;
        int osdDecodes = 0;

        // 0.5.1: GF(2) OSD fallback lab.  These are
        // diagnostic counters only; they do not drive candidate ranking or
        // scheduling decisions.
        int osdGf2Tried = 0;
        int osdGf2Recovered = 0;
        int osdGf2RankFails = 0;
        int osdGf2PivotSkips = 0;
        int osdGf2Order0Hits = 0;
        int osdGf2Order1Hits = 0;
        int osdGf2Order2Hits = 0;
        int osdGf2PostCrcRejects = 0;
        int osdGf2BudgetSkips = 0;
        double osdGf2TotalMs = 0.0;

        int apHypotheses = 0;
        int apDecodes = 0;

        // MIND Ranker diagnostics. The DNN may score/prune candidates before
        // LDPC; final messages still require LDPC+CRC+unpack+parser.
        int mindAssistTried = 0;       // scored candidates
        int mindAssistRecovered = 0;   // legacy name: pruned candidates
        int mindAssistExtraDecodes = 0; // CRC-valid decodes that used a MIND-opened recovery path
        int mindAssistUnavailable = 0;
        double mindAssistAvgConfidence = 0.0; // average candidate_success_probability percent

        // 0.5.78-lab14 / MIND Phase 3 passive telemetry.
        // Computed only after normal candidate decode/deduplication, so it
        // does not affect LDPC, CRC, candidate ordering, pruning or emitted
        // decodes. It is intentionally a measurement-only signal for later
        // erasure-like experiments.
        int mindReliabilitySamples = 0;
        int mindReliabilityWeakSymbols = 0;
        int mindReliabilityTotalSymbols = 0;
        double mindReliabilityAvgPercent = 0.0;
        double mindReliabilityWeakPercent = 0.0;
        double mindReliabilityQsbDepthPercent = 0.0;
    };

    using MindAssistCallback = std::function<bool(const QString &mode,
                                                  const QVector<float> &candidateMagnitudes,
                                                  QVector<float> *predictedBits,
                                                  double *confidencePercent)>;

    explicit Ft8RxDecoder(QObject *parent = nullptr);
    ~Ft8RxDecoder() override;

    void setMindAssistCallback(MindAssistCallback callback);
    // Hard isolation switch for MIND.  When bypassed, the FT core must not
    // allocate candidate-ranker features, acquire MIND locks or emit MIND
    // training samples.  This preserves the native 0.5.x decoder timing when
    // MIND Assist is Off.
    void setMindIntegrationState(bool bypassed,
                                 bool scoringEnabled,
                                 bool sampleExportEnabled,
                                 bool ultraDeepAssistedEnabled = false);

public slots:
    void reset();
    void processAudioBlock(const AudioBlock &block);

    // Called at the exact FT TX boundary before RX audio is stopped.
    // This finalizes the just-finished RX slot even when the next event is TX,
    // avoiding the old "armed TX skipped the decode" race.
    void noteTransmitStarting(qint64 txSlotBoundaryUtcMs);

    // Called by MainWindow when an FT transmit cycle has released PTT/audio.
    // The decoder must not treat the tail of our own TX slot as a short RX slot.
    void noteTransmitEnded(qint64 txSlotBoundaryUtcMs);

    void setSearchRangeHz(int lowHz, int highHz);
    void setRxMarkerHz(int hz);
    void setMyCall(const QString &call);
    void setDxCall(const QString &call);
    void setModeName(const QString &modeName);
    void setDecodeEngine(const QString &engineName);
    void setDeepDecodeEnabled(bool enabled);
    void setDspPlusDecodeEnabled(bool enabled);
    void analyzeAudioFile(const QString &filePath);

public:
    QString modeName() const;
    QString decodeEngine() const;
    bool deepDecodeEnabled() const;
    bool dspPlusDecodeEnabled() const;
    bool enhancedDecodeEngineEnabled() const; // always false since v2.19

signals:
    void decodeReady(const Ft8RxDecoder::Decode &decode);
    void nativeTrainingSampleReady(const QString &mode,
                                   const QVector<float> &candidateMagnitudes,
                                   const QVector<float> &targetBits,
                                   const QString &message);
    void statusChanged(const QString &status);
    void performanceUpdated(const Ft8RxDecoder::PerfStats &stats);
    void offlineAnalysisFinished(const QString &filePath, bool ok, int decodeCount, const QString &message);

private:
    struct Candidate
    {
        double score = 0.0;
        double startSec = 0.0;
        double baseHz = 0.0;
        double rankScore = 0.0;
        double syncRatio = 0.0;
        double syncNoisePower = 0.0;
        bool refined = false;
    };

    QVector<double> resampleTo12k(const AudioBlock &block);
    QVector<double> offlineResampleTo12k(const QVector<float> &samples, int sampleRate) const;
    void configureResamplePrefilter(int sampleRate);
    void maybeRotateSlot();
    void finishCurrentSlot();
    void maybeStartEarlyDecodeSlot();
    void maybeStartStreamingDecodeSlot();
    bool markDecodeEmitted(const Decode &decode, const QDateTime &slotStartUtc);
    void startAsyncDecodeSlot(const QVector<double> &samples, const QDateTime &slotStartUtc, const QString &phaseLabel = QString());
    void reapFinishedDecodeTasks();
    QVector<Decode> decodeSlot(const QVector<double> &samples,
                               const QDateTime &slotStartUtc,
                               int *candidateCount = nullptr,
                               PerfStats *stats = nullptr);
    QVector<Candidate> findCandidates(const QVector<double> &samples, double threshold = 0.35) const;
    enum class DecodeRejectReason
    {
        None,
        Boundary,
        SoftMetric,
        SyncGate,
        Ldpc,
        Crc,
        Unpack,
        Message
    };

    struct CandidateAttemptQuality
    {
        bool valid = false;
        double syncScore = 0.0;
        int hardSyncCount = 0;
        double meanAbsLlr = 0.0;

        // Per-candidate GF(2) OSD fallback diagnostics.  decodeSlot()
        // aggregates these into PerfStats after the worker pool joins.
        int osdGf2Tried = 0;
        int osdGf2Recovered = 0;
        int osdGf2RankFails = 0;
        int osdGf2PivotSkips = 0;
        int osdGf2Order0Hits = 0;
        int osdGf2Order1Hits = 0;
        int osdGf2Order2Hits = 0;
        int osdGf2PostCrcRejects = 0;
        int osdGf2BudgetSkips = 0;
        double osdGf2TotalMs = 0.0;

        int mindAssistTried = 0;       // scored candidates
        int mindAssistRecovered = 0;   // legacy name: pruned candidates
        int mindAssistExtraDecodes = 0; // CRC-valid decodes that used a MIND-opened recovery path
        int mindAssistUnavailable = 0;
        double mindAssistConfidence = 0.0; // candidate_success_probability percent
    };

    bool decodeCandidate(const QVector<double> &samples,
                         const QDateTime &slotStartUtc,
                         const Candidate &candidate,
                         Decode &decodeOut,
                         Candidate *refinedCandidateOut = nullptr,
                         DecodeRejectReason *rejectReasonOut = nullptr,
                         CandidateAttemptQuality *qualityOut = nullptr,
                         bool allowMetricRecovery = true,
                         bool allowGf2Osd = true);
    void subtractDecodedSignal(QVector<double> &samples, const Candidate &candidate, const Decode &decode) const;
    QVector<Decode> decodeSlotFt4(const QVector<double> &samples,
                                  const QDateTime &slotStartUtc,
                                  int *candidateCount = nullptr,
                                  PerfStats *stats = nullptr);
    QVector<Candidate> findFt4Candidates(const QVector<double> &samples, double threshold = 0.95) const;
    bool decodeFt4Candidate(const QVector<double> &samples,
                            const QDateTime &slotStartUtc,
                            const Candidate &candidate,
                            Decode &decodeOut,
                            CandidateAttemptQuality *qualityOut = nullptr,
                            DecodeRejectReason *rejectReasonOut = nullptr);
    void subtractFt4DecodedSignal(QVector<double> &samples, const Candidate &candidate) const;
    double ft4SymbolToneEnergy(const QVector<double> &samples,
                               int startSample,
                               double frequencyHz,
                               int sampleCount) const;
    std::array<double, 4> ft4SymbolToneEnergies4(const QVector<double> &samples,
                                                 int startSample,
                                                 double baseFrequencyHz,
                                                 double toneSpacingHz,
                                                 int sampleCount) const;
    QString unpackFt4Message77(const std::array<int, 174> &bits);

    int currentSlotSamples() const;
    int currentSlotMs() const;
    QString currentShortLabel() const;
    void beginUtcSlot(qint64 slotId, int maxPrepadMs, const QString &reason = QString());
    bool isPostTxIgnoredSlot() const;
    int postTxPrepadLimitMs() const;

    double symbolToneEnergy(const QVector<double> &samples,
                            int startSample,
                            double frequencyHz) const;
    std::array<double, 8> symbolToneEnergies8(const QVector<double> &samples,
                                              int startSample,
                                              double baseFrequencyHz) const;

    bool ldpcDecode174_91(const std::array<double, 174> &llr,
                          std::array<int, 174> &hardBits,
                          int &iterationsUsed,
                          std::array<double, 174> *posteriorOut = nullptr) const;
    bool osdLiteRepair174_91(const std::array<double, 174> &posterior,
                             std::array<int, 174> &bits) const;
    bool osdGf2Repair174_91(const std::array<double, 174> &posterior,
                            std::array<int, 174> &bits,
                            int &outOrder,
                            bool &rankFail,
                            int &pivotSkips,
                            int order1Depth,
                            int order2Depth) const;
    bool syndromeOk(const std::array<int, 174> &bits) const;
    bool crc14Ok(const std::array<int, 174> &bits) const;
    unsigned int crc14(const unsigned char *data, int length) const;
    QString unpackMessage77(const std::array<int, 174> &bits);

    static bool isSyncSymbol(int symbolIndex);
    static int dataSymbolIndex(int symbolIndex);
    static int grayInverse(int tone);

private:
    // FT8 decoder working rate. 12 kHz is the conventional WSJT/MSHV internal
    // rate: it comfortably covers the 100..3000 Hz audio passband while keeping
    // Costas/candidate search and LDPC soft-symbol extraction much lighter than
    // doing the whole receiver at 48 kHz. Input audio is still accepted at the
    // selected sound-card rate and resampled here.
    static constexpr int kDecodeSampleRate = 12000;
    static constexpr int kSlotSamples = 15 * kDecodeSampleRate;
    static constexpr int kSymbols = 79;
    static constexpr int kSamplesPerSymbol = 1920;
    static constexpr double kToneSpacingHz = 6.25;

    int m_searchLowHz = 200;
    int m_searchHighHz = 3000;
    int m_rxMarkerHz = 1500;
    QString m_myCall;
    QString m_dxCall;
    QString m_modeName = QStringLiteral("FT8");
    QString m_decodeEngine = QStringLiteral("mshv"); // fixed MSHV-derived native pipeline
    bool m_deepDecodeEnabled = true; // v4.10 unified FT8 engine: adaptive baseline always enabled
    bool m_dspPlusDecodeEnabled = true; // v4.10: internal residual/AP-OSD lab stage, not an exposed UI mode
    std::atomic<bool> m_offlineAnalysisActive {false}; // true only while decoding user-loaded WAV files
    std::atomic<bool> m_mindHardBypass {true};
    std::atomic<bool> m_mindScoringEnabled {false};
    std::atomic<bool> m_mindSampleExportEnabled {false};
    std::atomic<bool> m_mindUltraDeepAssistedEnabled {false};

    int m_inputSampleRate = 0;
    double m_resamplePos = 0.0;
    int m_resamplePrefilterRate = 0;
    double m_resamplePrefilterAlpha = 1.0;
    double m_resampleLp1 = 0.0;
    double m_resampleLp2 = 0.0;
    qint64 m_currentSlotId = -1;
    qint64 m_earlyDecodeSlotId = -1;
    qint64 m_streamingDecodeSlotId = -1;
    int m_lastStreamingDecodeSamples = 0;
    bool m_finalDecodeLaunchedForSlot = false;
    qint64 m_postTxIgnoreSlotId = -1;
    int m_initialUtcPadSamples = 0;

    // WSJT-X-style decode launch gate.  WSJT-X does not continuously launch
    // arbitrary decode jobs during the whole RX period; it starts decoding at
    // specific symbol-count gates (FT8 normal hsym=49, FT4 hsym=21) so the
    // sequencer gets decodes before the next TX period without creating a
    // burst of overlapping work.  MadModem keeps the native decoder but uses
    // the same architectural policy here.
    int wsjtxDecodeGateSamples() const;
    QDateTime m_currentSlotStartUtc;
    QVector<double> m_slotSamples;

    std::atomic<int> m_lastCandidateCount {0};
    std::atomic<int> m_decodeGeneration {0};
    std::atomic<bool> m_shutdown {false};
    mutable std::mutex m_unpackMutex;
    mutable std::mutex m_emittedDecodeMutex;
    mutable std::mutex m_mindAssistMutex;
    MindAssistCallback m_mindAssistCallback;
    QSet<QString> m_emittedDecodeKeys;
    std::vector<std::future<void>> m_decodeTasks;
    GenFt8 m_unpacker;
};

Q_DECLARE_METATYPE(Ft8RxDecoder::Decode)
Q_DECLARE_METATYPE(Ft8RxDecoder::PerfStats)

#endif // FT8RXDECODER_H
