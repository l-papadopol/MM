#ifndef APPSETTINGS_H
#define APPSETTINGS_H

#include <QString>
#include <QVector>
#include <QStringList>

/**
 * @brief Stores persistent MadModem user settings.
 *
 * Purpose:
 * - Keep audio input/output device selections.
 * - Keep PTT serial port selection.
 * - Keep current WeatherFax parameters and synchronization preference.
 * - Save and load values from settings.mad.
 */
class AppSettings
{
public:
    /**
     * @brief Returns the full settings file path.
     */
    static QString settingsFilePath();

    /**
     * @brief Loads settings from disk.
     */
    void load();

    /**
     * @brief Saves settings to disk.
     */
    bool save() const;

public:
    QString audioInputName = "default";
    QString audioOutputName = "default";
    QString pttPortName;
    // PTT method: none, serial_rts, serial_dtr, cat_hamlib.
    QString pttMethod = "none";
    int audioSampleRate = 48000;
    QString logbookFilePath;
    bool logbookStrikeWorkedCalls = true;
    bool logbookVisibleFieldsConfigured = false;
    QStringList logbookVisibleFields;
    int waterfallColorScalePercent = 80;
    QString waterfallPalette = "madmodem";
    // Global UI appearance. Theme names are stable keys, not translated labels:
    // avionica, qt_default, hacker_green, classic_dark, high_contrast.
    QString uiTheme = "avionica";
    bool uiUseThemeFont = true;
    QString uiFontFamily;
    int uiFontPointSize = 10;
    QString decodeTableFontFamily; // Empty = inherit global/application UI font.
    QString decodeTablePreset = "normal"; // custom, compact, normal, large, low_vision.
    int decodeTableFontPointSize = 9;
    int decodeTableRowHeightPx = 20;
    double audioRxClockPpm = 0.0;
    double audioTxClockPpm = 0.0;

    bool hamlibCatEnabled = false;
    bool hamlibPttEnabled = false;
    bool hamlibUpdateFt8Band = true;
    int hamlibRigModel = 1;
    // Runtime rig path passed to Hamlib, rigctld/flrig, or HRD TCP. It is
    // derived from TCP address if set, otherwise from the serial path, but the
    // two UI fields are persisted separately so reopening the dialog does not
    // lose either value.
    QString hamlibRigPath;
    QString hamlibSerialPath;
    QString hamlibTcpAddress;
    int hamlibBaudRate = 38400;
    // WSJT-X style serial options. 0/default leaves Hamlib backend defaults untouched.
    int hamlibDataBits = 0; // 0, 7, 8
    int hamlibStopBits = 0; // 0, 1, 2
    // default, none, xonxoff, hardware
    QString hamlibHandshake = "default";
    // unchanged, on, off. Applied only when explicitly selected.
    QString hamlibForceDtr = "unchanged";
    QString hamlibForceRts = "unchanged";
    int hamlibPollIntervalMs = 1000;
    // WSJT-X-like TX rig mode policy: default, usb, data_pkt.
    QString hamlibTxAudioRoute = "default";
    // WSJT-X-like transmit audio source hint for CAT PTT: rear_data or front_mic.
    QString hamlibTransmitAudioSource = "rear_data";

    QStringList schedulerQsyEnabledModes;
    QString schedulerQsyPlanJson;

    int weatherFaxLpm = 120;
    int weatherFaxBlackHz = 1500;
    int weatherFaxWhiteHz = 2300;
    bool weatherFaxAutoStartPhasing = true;
    bool weatherFaxAutoToneTracking = true;
    bool weatherFaxInputBandpass = true;
    bool weatherFaxAutoSlantCorrection = false;
    double weatherFaxManualSlantPpm = 0.0;

    QString weatherFaxLinePreset = "DWD_PINNEBERG";
    int weatherFaxImageLines = 8000;
    bool weatherFaxAutoSave = false;
    QString weatherFaxOutputFolder;
    bool weatherFaxEndOfSignal = true;
    int weatherFaxEndOfSignalTimeoutSec = 20;
    bool weatherFaxNoiseReductionEnabled = false;
    bool weatherFaxAgcEnabled = false;
    bool weatherFaxWaveletDenoiseEnabled = false;

    QString sstvMode = "Martin M1";
    bool sstvAutoSync = true;
    int sstvHorizontalShiftPixels = 0;
    int sstvRedShiftPixels = 0;
    int sstvBlueShiftPixels = 0;
    bool sstvNoiseReductionEnabled = false;
    bool sstvAgcEnabled = false;
    bool sstvWaveletDenoiseEnabled = false;

    QString rttyPreset = "45.45 / 170 / high tones";
    double rttyBaudRate = 45.45;
    int rttyShiftHz = 170;
    int rttyMarkHz = 2125;
    bool rttyReverse = false;
    bool rttyAutoReverseEnabled = true;
    bool rttyAfcEnabled = true;
    int rttyAfcRangeHz = 20;
    bool rttyNoiseReductionEnabled = false;
    // RTTY Baudot slicers often work worse with a broad audio AGC because it
    // changes Mark/Space balance during QSB. Keep it opt-in.
    bool rttyAgcEnabled = false;
    bool rttyAdaptiveLineEnhancerEnabled = false;
    bool rttyMatchedFilterEnabled = false;
    bool rttyMarkSpaceEnhancerEnabled = false;
    bool rttyMultiDecodeEnabled = false;
    bool rttyOverlayCallsignsEnabled = true;
    bool rttyContestEnhancedEnabled = false;
    bool rttySecondPassEnabled = false;
    int rttyMaxParallelDecoders = 8;

    QString bpsk31Variant = "BPSK31";
    int bpsk31ToneHz = 1000;
    bool bpsk31AfcEnabled = true;
    int bpsk31AfcRangeHz = 20;
    bool bpsk31InvertBits = false;
    bool bpsk31NoiseReductionEnabled = false;
    bool bpsk31AgcEnabled = true;
    bool bpsk31CoherentTrackingEnabled = true;

    QString mfskVariant = "MFSK16";
    int mfskCenterHz = 1000;
    bool mfskAfcEnabled = false;
    int mfskAfcRangeHz = 50;
    bool mfskNoiseReductionEnabled = false;
    bool mfskAgcEnabled = true;

    int cwToneHz = 1000;
    int cwSecondaryToneHz = 1400;
    bool cwSecondaryEnabled = false;
    int cwWpm = 20;
    int cwBandwidthHz = 120;
    bool cwAfcEnabled = true;
    int cwAfcRangeHz = 20;
    bool cwAutoWpm = true;
    bool cwNoiseReductionEnabled = false;
    bool cwAgcEnabled = true;
    bool cwAdaptiveLineEnhancerEnabled = false;

    QString hellVariant = "FeldHell";
    int hellToneHz = 1000;
    double hellColumnRate = 17.5;
    int hellBandwidthHz = 245;
    bool hellAfcEnabled = true;
    int hellAfcRangeHz = 20;
    int hellPaperScale = 4;
    bool hellNoiseReductionEnabled = false;
    bool hellAgcEnabled = true;

    QString ft8MyCallsign;
    QString ft8MyGrid;
    QString ft8Band = "20m";
    QString ft8DxCallsign;
    QString ft8DxGrid;
    int ft8RxFrequencyHz = 1500;
    int ft8TxFrequencyHz = 1500;
    bool ft8TxFirstPeriod = true;
    bool ft8AutoSequence = true;
    bool ft8CqRepeat = false;
    bool ft8AutoLog = true;
    bool ft8HoldTxFrequency = false;
    // fixed = manual TX marker / Hold TX, auto_free = choose a clear slot, follow_rx = follow correspondent RX.
    QString ft8TxFrequencyStrategy = "auto_free";
    int ft8TxGuardHz = 110;
    // FT live decode depth: fast = one pass, adaptive = guarded subtract/rescan only
    // after a successful first pass.  Deep/4-pass UI was removed in v3.22.
    QString ft8LiveDecodeDepth = "adaptive";
    bool ft8DeepDecode = true; // internal compatibility flag: adaptive enables subtract/rescan.
    bool ft8DspPlusDecode = false; // deprecated compatibility flag; forced false.
    int ft8CqRepeatTimeoutMinutes = 5;
    QString ft8DecodeEngine = "mshv"; // v2.19: fixed MSHV-derived native pipeline

    // FT4/FT8 decode table highlight colours. Stored as #RRGGBB strings so
    // settings.mad remains human-editable and no GUI type leaks into AppSettings.
    QString ftHighlightMyCallBackground = "#FFEBEB";
    QString ftHighlightMyCallForeground = "#DC0000";
    QString ftHighlightCqBackground = "#445C1C";
    QString ftHighlightCqForeground = "#FFE478";
    QString ftHighlightWorkedBackground = "#242424";
    QString ftHighlightWorkedForeground = "#969696";
    QString ftHighlightTxBackground = "#FFF75F";
    QString ftHighlightTxForeground = "#000000";

    // FT4/FT8 operator filters and supervised automation policy.  Callsign
    // lists are normalized on use and stored as one call per line/list item.
    bool ftHighlightNewCountryEnabled = false;
    bool ftWatchListIconEnabled = true;
    QStringList ftBlacklistCalls;
    QStringList ftWatchListCalls;
    // never_worked = skip CQs already present in the log; recent = skip calls
    // worked within ftAutoQsoRecentHours; none = answer any CQ not blacklisted.
    QString ftAutoQsoDuplicatePolicy = "never_worked";
    int ftAutoQsoRecentHours = 24;
    int ft8CqRepeatCount = 3;
    // Maximum number of unanswered FT TX attempts during a started QSO.
    // Counts the first call plus automatic retries; after this MM stops and returns to RX standby.
    int ft8NoResponseRetryCount = 3;


    // CatRotator integration. The original CatRotator WSJT-X/AirScout
    // tracking inputs are intentionally not exposed here; the rotator follows only
    // MadModem-selected QSO targets or manual commands.
    // Rotator hardware communication is intentionally separate from radio CAT/PTT.
    struct RotatorBandSettings
    {
        QString band;
        bool enabled = false;
        double azimuthSearchSpanDeg = 30.0;
        double elevationSearchSpanDeg = 0.0;
        bool autoPeakEnabled = false;
    };

    struct RotatorProfileSettings
    {
        QString label = QStringLiteral("Rotator");
        QString bandsCsv; // Derived from bandSettings for legacy matching and older configs.
        QVector<RotatorBandSettings> bandSettings;
        QString peakSearchAlgorithm = QStringLiteral("bounded-adaptive");
        int hamlibModel = 1;
        QString path;
        int baudRate = 9600;
        int pollIntervalMs = 750;
        bool useElevation = false;
        bool overlap = false;
        QString azimuthGeometryPreset = QStringLiteral("north-stop-360");
        double azimuthStopDeg = 0.0;
        bool autoReverseOnStall = true;
        int noMovementTimeoutMs = 3000;
        double noMovementThresholdDeg = 2.0;
        double parkAzimuth = 0.0;
        double parkElevation = 0.0;
        int targetToleranceDeg = 3;
        double antennaBeamWidthDeg = 15.0; // Radio Telescope / auto-peak tiling beamwidth.
        double azimuthMinDeg = 0.0;
        double azimuthMaxDeg = 359.9;
        double elevationMinDeg = 0.0;
        double elevationMaxDeg = 90.0;
        double azimuthMsPerDeg = 0.0;
        double elevationMsPerDeg = 0.0;
        int startupDelayMs = 250;
        int settleDelayMs = 700;
        int txGuardMarginMs = 800;
        QString calibrationStampUtc;
    };

    bool rotatorEnabled = false;
    bool rotatorAutoConnect = false;
    bool rotatorShowWindowOnStart = false;
    bool rotatorTrackSelectedQso = true;
    bool rotatorTrackOnlyWhenQsoActive = true;
    // When true, FT4/FT8 TX is delayed until the connected rotator is within
    // target tolerance for the current QSO bearing.  Operators who prefer
    // immediate TX can disable this guard from Settings -> Rotator.
    bool rotatorBlockFtTxUntilReady = true;
    int rotatorActiveProfile = 0;
    RotatorProfileSettings rotatorProfiles[3] = {
        { QStringLiteral("Rotator 1"), QStringLiteral("40m,30m,20m,17m,15m,12m,10m,6m,4m,2m,70cm,33cm,23cm,13cm,9cm,6cm,3cm,10GHz") },
        { QStringLiteral("Rotator 2"), QString() },
        { QStringLiteral("Rotator 3"), QString() }
    };

    // Legacy aliases kept for older code/settings.mad files. They mirror profile 1.
    int rotatorHamlibModel = 1;
    QString rotatorPath;
    int rotatorBaudRate = 9600;
    int rotatorPollIntervalMs = 750;
    bool rotatorUseElevation = false;
    double rotatorParkAzimuth = 0.0;
    double rotatorParkElevation = 0.0;
    int rotatorTargetToleranceDeg = 3;

    QString textMyCallsign;
    QString textMyName;
    QString textMyQth;
    QString textMyLocator;
    bool textMyCoordinatesEnabled = false;
    double textMyLatitudeDeg = 0.0;
    double textMyLongitudeDeg = 0.0;
    QString textRig;
    QString textAntenna;
    QString textPower;
    QStringList textMacroLabels = {
        "CQ",
        "Reply",
        "RST",
        "Name/QTH",
        "Rig",
        "73"
    };
    QStringList textMacroTexts = {
        "CQ CQ CQ de {MYCALL} {MYCALL} {MYCALL} pse k",
        "{CALL} de {MYCALL} tnx fer call. ur rst {RST} {RST}. hw cpy? {CALL} de {MYCALL} kn",
        "{CALL} de {MYCALL} ur rst {RST} {RST} bk",
        "My name is {MYNAME} and QTH is {MYQTH}. Locator {LOC}. hw?",
        "Rig {RIG}, antenna {ANT}, power {PWR} W.",
        "{CALL} de {MYCALL} tnx qso 73 good dx sk"
    };
};

#endif // APPSETTINGS_H
