#include "AppSettings.h"

#include <QCoreApplication>
#include <QDir>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QtGlobal>

namespace {

void normalizeTextMacros(QStringList &labels, QStringList &texts)
{
    const QStringList defaultLabels = {
        "CQ",
        "Reply",
        "RST",
        "Name/QTH",
        "Rig",
        "73"
    };

    const QStringList defaultTexts = {
        "CQ CQ CQ de {MYCALL} {MYCALL} {MYCALL} pse k",
        "{CALL} de {MYCALL} tnx fer call. ur rst {RST} {RST}. hw cpy? {CALL} de {MYCALL} kn",
        "{CALL} de {MYCALL} ur rst {RST} {RST} bk",
        "My name is {MYNAME} and QTH is {MYQTH}. Locator {LOC}. hw?",
        "Rig {RIG}, antenna {ANT}, power {PWR} W.",
        "{CALL} de {MYCALL} tnx qso 73 good dx sk"
    };

    while (labels.size() < 6) {
        labels.append(defaultLabels.value(labels.size(), QString("Macro %1").arg(labels.size() + 1)));
    }

    while (texts.size() < 6) {
        texts.append(defaultTexts.value(texts.size()));
    }

    while (labels.size() > 6) {
        labels.removeLast();
    }

    while (texts.size() > 6) {
        texts.removeLast();
    }

    for (int i = 0; i < labels.size(); ++i) {
        if (labels[i].trimmed().isEmpty()) {
            labels[i] = defaultLabels.value(i, QString("Macro %1").arg(i + 1));
        }
    }
}


QString safeColourName(const QVariant &value, const QString &fallback)
{
    QString text = value.toString().trimmed();
    if (text.isEmpty()) {
        text = fallback;
    }
    if (!text.startsWith(QLatin1Char('#'))) {
        text.prepend(QLatin1Char('#'));
    }
    if (text.size() != 7) {
        return fallback;
    }
    for (int i = 1; i < text.size(); ++i) {
        const QChar ch = text.at(i);
        if (!ch.isDigit() && (ch.toLower() < QLatin1Char('a') || ch.toLower() > QLatin1Char('f'))) {
            return fallback;
        }
    }
    return text.toUpper();
}


QStringList defaultRotatorBandNames()
{
    return {
        QStringLiteral("40m"), QStringLiteral("30m"), QStringLiteral("20m"), QStringLiteral("17m"),
        QStringLiteral("15m"), QStringLiteral("12m"), QStringLiteral("10m"), QStringLiteral("6m"),
        QStringLiteral("4m"), QStringLiteral("2m"), QStringLiteral("70cm"), QStringLiteral("33cm"),
        QStringLiteral("23cm"), QStringLiteral("13cm"), QStringLiteral("9cm"), QStringLiteral("6cm"),
        QStringLiteral("3cm"), QStringLiteral("10GHz")
    };
}

QString bandSettingsKey(QString band)
{
    band = band.trimmed();
    band.replace(QChar('/'), QChar('_'));
    band.replace(QChar(' '), QChar('_'));
    band.replace(QChar('.'), QChar('_'));
    return band;
}

QString normalizedBandToken(QString band)
{
    return band.trimmed().toLower().remove(QChar(' '));
}

QVector<AppSettings::RotatorBandSettings> defaultRotatorBandSettings()
{
    QVector<AppSettings::RotatorBandSettings> rows;
    const QStringList bands = defaultRotatorBandNames();
    rows.reserve(bands.size());
    for (const QString &band : bands) {
        AppSettings::RotatorBandSettings row;
        row.band = band;
        row.enabled = false;
        row.azimuthSearchSpanDeg = 30.0;
        row.elevationSearchSpanDeg = (band == QStringLiteral("40m") || band == QStringLiteral("30m") || band == QStringLiteral("20m") || band == QStringLiteral("17m") || band == QStringLiteral("15m") || band == QStringLiteral("12m") || band == QStringLiteral("10m")) ? 0.0 : 10.0;
        row.autoPeakEnabled = false;
        rows.append(row);
    }
    return rows;
}

void normalizeRotatorBandSettings(AppSettings::RotatorProfileSettings &profile)
{
    QVector<AppSettings::RotatorBandSettings> defaults = defaultRotatorBandSettings();
    QHash<QString, AppSettings::RotatorBandSettings> existing;
    for (const AppSettings::RotatorBandSettings &row : profile.bandSettings) {
        const QString key = normalizedBandToken(row.band);
        if (!key.isEmpty()) {
            existing.insert(key, row);
        }
    }

    const QStringList legacyBands = profile.bandsCsv.split(QRegularExpression(QStringLiteral("[\\s,;]+")), Qt::SkipEmptyParts);
    QSet<QString> legacyEnabled;
    for (const QString &band : legacyBands) {
        legacyEnabled.insert(normalizedBandToken(band));
    }

    QStringList enabledBands;
    QVector<AppSettings::RotatorBandSettings> normalized;
    normalized.reserve(defaults.size());
    for (AppSettings::RotatorBandSettings row : defaults) {
        const QString key = normalizedBandToken(row.band);
        if (existing.contains(key)) {
            const AppSettings::RotatorBandSettings old = existing.value(key);
            row.enabled = old.enabled;
            row.azimuthSearchSpanDeg = qBound(0.0, old.azimuthSearchSpanDeg, 180.0);
            row.elevationSearchSpanDeg = qBound(0.0, old.elevationSearchSpanDeg, 180.0);
            row.autoPeakEnabled = old.autoPeakEnabled;
        } else if (!legacyEnabled.isEmpty()) {
            row.enabled = legacyEnabled.contains(key);
        }
        if (row.enabled) {
            enabledBands << row.band;
        }
        normalized.append(row);
    }
    profile.bandSettings = normalized;
    profile.bandsCsv = enabledBands.join(QStringLiteral(","));
    if (profile.peakSearchAlgorithm.trimmed().isEmpty()) {
        profile.peakSearchAlgorithm = profile.useElevation ? QStringLiteral("nelder-mead") : QStringLiteral("bounded-adaptive");
    }
}

} // namespace

// -----------------------------------------------------------------------------
// Static path
// -----------------------------------------------------------------------------

QString AppSettings::settingsFilePath()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath("settings.mad");
}

// -----------------------------------------------------------------------------
// Persistence
// -----------------------------------------------------------------------------

void AppSettings::load()
{
    QSettings settings(settingsFilePath(), QSettings::IniFormat);

    audioInputName = settings.value("Audio/inputDevice", audioInputName).toString();
    audioOutputName = settings.value("Audio/outputDevice", audioOutputName).toString();
    audioSampleRate = settings.value("Audio/sampleRate", audioSampleRate).toInt();
    if (audioSampleRate != 44100 && audioSampleRate != 48000 && audioSampleRate != 96000) {
        audioSampleRate = 48000;
    }
    logbookFilePath = settings.value("Logbook/filePath", logbookFilePath).toString().trimmed();
    logbookStrikeWorkedCalls = settings.value("Logbook/strikeWorkedCalls", logbookStrikeWorkedCalls).toBool();
    logbookVisibleFieldsConfigured = settings.value("Logbook/visibleFieldsConfigured", logbookVisibleFieldsConfigured).toBool();
    logbookVisibleFields = settings.value("Logbook/visibleFields", logbookVisibleFields).toStringList();
    for (QString &field : logbookVisibleFields) { field = field.trimmed().toUpper(); }
    logbookVisibleFields.removeAll(QString());
    waterfallColorScalePercent = settings.value("Display/waterfallColorScalePercent", waterfallColorScalePercent).toInt();
    if (waterfallColorScalePercent < 0 || waterfallColorScalePercent > 100) {
        waterfallColorScalePercent = 100;
    }
    waterfallPalette = settings.value("Display/waterfallPalette", waterfallPalette).toString().trimmed().toLower();
    if (waterfallPalette == "default" || waterfallPalette == "wsjt-x") {
        waterfallPalette = "wsjtx";
    }
    if (waterfallPalette != "madmodem" && waterfallPalette != "wsjtx" &&
        waterfallPalette != "mshv" && waterfallPalette != "fldigi" &&
        waterfallPalette != "raptor" && waterfallPalette != "grayscale") {
        waterfallPalette = "madmodem";
    }
    audioRxClockPpm = settings.value("Audio/rxClockPpm", audioRxClockPpm).toDouble();
    audioTxClockPpm = settings.value("Audio/txClockPpm", audioTxClockPpm).toDouble();
    if (audioRxClockPpm < -5000.0 || audioRxClockPpm > 5000.0) {
        audioRxClockPpm = 0.0;
    }
    if (audioTxClockPpm < -5000.0 || audioTxClockPpm > 5000.0) {
        audioTxClockPpm = 0.0;
    }

    pttPortName = settings.value("PTT/serialPort", pttPortName).toString();
    const bool storedPttMethod = settings.contains("PTT/method");
    pttMethod = settings.value("PTT/method", pttMethod).toString().trimmed().toLower();
    if (pttMethod != "none" && pttMethod != "serial_rts" &&
        pttMethod != "serial_dtr" && pttMethod != "cat_hamlib") {
        pttMethod = "none";
    }

    hamlibCatEnabled = settings.value("Hamlib/catEnabled", hamlibCatEnabled).toBool();
    hamlibPttEnabled = settings.value("Hamlib/pttEnabled", hamlibPttEnabled).toBool();
    if (!storedPttMethod && hamlibPttEnabled) {
        pttMethod = "cat_hamlib";
    }
    hamlibPttEnabled = (pttMethod == "cat_hamlib");
    hamlibUpdateFt8Band = settings.value("Hamlib/updateFt8Band", hamlibUpdateFt8Band).toBool();
    hamlibRigModel = settings.value("Hamlib/rigModel", hamlibRigModel).toInt();
    if (hamlibRigModel < 1) {
        hamlibRigModel = 1;
    }
    const QString legacyHamlibRigPath = settings.value("Hamlib/rigPath", hamlibRigPath).toString().trimmed();
    hamlibSerialPath = settings.value("Hamlib/serialPort", hamlibSerialPath).toString().trimmed();
    hamlibTcpAddress = settings.value("Hamlib/tcpAddress", hamlibTcpAddress).toString().trimmed();
    if (hamlibSerialPath.isEmpty() && !legacyHamlibRigPath.isEmpty() &&
        !legacyHamlibRigPath.startsWith(QStringLiteral("rigctld:"), Qt::CaseInsensitive) &&
        !legacyHamlibRigPath.startsWith(QStringLiteral("tcp:"), Qt::CaseInsensitive)) {
        const int colon = legacyHamlibRigPath.lastIndexOf(QChar(':'));
        bool looksNetwork = false;
        if (colon > 0 && colon < legacyHamlibRigPath.size() - 1) {
            bool portOk = false;
            legacyHamlibRigPath.mid(colon + 1).toInt(&portOk);
            looksNetwork = portOk && !legacyHamlibRigPath.left(colon).startsWith(QStringLiteral("COM"), Qt::CaseInsensitive);
        }
        if (!looksNetwork) {
            hamlibSerialPath = legacyHamlibRigPath;
        }
    }
    // Do not migrate legacy Hamlib/rigPath host:port values into the live TCP
    // field automatically. A stale localhost:7809 from old builds must not
    // override serial CAT. Only the explicit Hamlib/tcpAddress key is trusted.
    hamlibRigPath = hamlibTcpAddress.isEmpty() ? hamlibSerialPath : hamlibTcpAddress;
    hamlibBaudRate = settings.value("Hamlib/baudRate", hamlibBaudRate).toInt();
    if (hamlibBaudRate <= 0) {
        hamlibBaudRate = 38400;
    }
    hamlibDataBits = settings.value("Hamlib/dataBits", hamlibDataBits).toInt();
    if (hamlibDataBits != 0 && hamlibDataBits != 7 && hamlibDataBits != 8) {
        hamlibDataBits = 0;
    }
    hamlibStopBits = settings.value("Hamlib/stopBits", hamlibStopBits).toInt();
    if (hamlibStopBits != 0 && hamlibStopBits != 1 && hamlibStopBits != 2) {
        hamlibStopBits = 0;
    }
    hamlibHandshake = settings.value("Hamlib/handshake", hamlibHandshake).toString().trimmed().toLower();
    if (hamlibHandshake != "default" && hamlibHandshake != "none" &&
        hamlibHandshake != "xonxoff" && hamlibHandshake != "hardware") {
        hamlibHandshake = "default";
    }
    hamlibForceDtr = settings.value("Hamlib/forceDtr", hamlibForceDtr).toString().trimmed().toLower();
    if (hamlibForceDtr != "unchanged" && hamlibForceDtr != "on" && hamlibForceDtr != "off") {
        hamlibForceDtr = "unchanged";
    }
    hamlibForceRts = settings.value("Hamlib/forceRts", hamlibForceRts).toString().trimmed().toLower();
    if (hamlibForceRts != "unchanged" && hamlibForceRts != "on" && hamlibForceRts != "off") {
        hamlibForceRts = "unchanged";
    }
    hamlibPollIntervalMs = settings.value("Hamlib/pollIntervalMs", hamlibPollIntervalMs).toInt();
    if (hamlibPollIntervalMs < 250 || hamlibPollIntervalMs > 10000) {
        hamlibPollIntervalMs = 1000;
    }
    hamlibTxAudioRoute = settings.value("Hamlib/txAudioRoute", hamlibTxAudioRoute).toString().trimmed();
    // v2.62 cleanup: collapse legacy DATA/vendor PTT routes into a single
    // WSJT-X-like Data/Pkt rig-mode selection. PTT remains controlled only by
    // PTT method.
    if (hamlibTxAudioRoute == "data" ||
        hamlibTxAudioRoute == "force_data_usb" ||
        hamlibTxAudioRoute == "kenwood_usb" ||
        hamlibTxAudioRoute == "kenwood_acc2" ||
        hamlibTxAudioRoute == "kenwood_lan") {
        hamlibTxAudioRoute = "data_pkt";
    }
    if (hamlibTxAudioRoute != "default" &&
        hamlibTxAudioRoute != "usb" &&
        hamlibTxAudioRoute != "data_pkt") {
        hamlibTxAudioRoute = "default";
    }
    hamlibTransmitAudioSource = settings.value("Hamlib/transmitAudioSource", hamlibTransmitAudioSource).toString().trimmed().toLower();
    if (hamlibTransmitAudioSource == "rear" || hamlibTransmitAudioSource == "data" ||
        hamlibTransmitAudioSource == "rear_data" || hamlibTransmitAudioSource == "back") {
        hamlibTransmitAudioSource = "rear_data";
    } else if (hamlibTransmitAudioSource == "front" || hamlibTransmitAudioSource == "mic" ||
               hamlibTransmitAudioSource == "front_mic") {
        hamlibTransmitAudioSource = "front_mic";
    } else {
        hamlibTransmitAudioSource = "rear_data";
    }

    schedulerQsyEnabledModes = settings.value("Scheduler/enabledModes", schedulerQsyEnabledModes).toStringList();
    for (QString &mode : schedulerQsyEnabledModes) { mode = mode.trimmed(); }
    schedulerQsyEnabledModes.removeAll(QString());
    schedulerQsyEnabledModes.removeDuplicates();
    schedulerQsyPlanJson = settings.value("Scheduler/qsyPlanJson", schedulerQsyPlanJson).toString().trimmed();

    weatherFaxLpm = settings.value("WeatherFax/lpm", weatherFaxLpm).toInt();
    // Marker frequencies are intentionally session-only in v50.
    weatherFaxBlackHz = 1500;
    weatherFaxWhiteHz = 2300;
    weatherFaxAutoStartPhasing = settings.value("WeatherFax/autoStartPhasing", weatherFaxAutoStartPhasing).toBool();
    weatherFaxAutoToneTracking = settings.value("WeatherFax/autoToneTracking", weatherFaxAutoToneTracking).toBool();
    weatherFaxInputBandpass = settings.value("WeatherFax/inputBandpass", weatherFaxInputBandpass).toBool();
    weatherFaxAutoSlantCorrection = false;
    weatherFaxManualSlantPpm = 0.0;
    weatherFaxLinePreset = settings.value("WeatherFax/linePreset", weatherFaxLinePreset).toString();
    weatherFaxImageLines = settings.value("WeatherFax/imageLines", weatherFaxImageLines).toInt();
    weatherFaxAutoSave = settings.value("WeatherFax/autoSave", weatherFaxAutoSave).toBool();
    weatherFaxOutputFolder = settings.value("WeatherFax/outputFolder", weatherFaxOutputFolder).toString();
    weatherFaxEndOfSignal = settings.value("WeatherFax/endOfSignal", weatherFaxEndOfSignal).toBool();
    weatherFaxEndOfSignalTimeoutSec = settings.value("WeatherFax/endOfSignalTimeoutSec", weatherFaxEndOfSignalTimeoutSec).toInt();
    weatherFaxNoiseReductionEnabled = settings.value("WeatherFax/noiseReductionEnabled", weatherFaxNoiseReductionEnabled).toBool();
    weatherFaxAgcEnabled = settings.value("WeatherFax/agcEnabled", weatherFaxAgcEnabled).toBool();
    weatherFaxWaveletDenoiseEnabled = settings.value("WeatherFax/waveletDenoiseEnabled", weatherFaxWaveletDenoiseEnabled).toBool();

    sstvMode = settings.value("SSTV/mode", sstvMode).toString();
    sstvAutoSync = settings.value("SSTV/autoSync", sstvAutoSync).toBool();
    sstvHorizontalShiftPixels = settings.value("SSTV/horizontalShiftPixels", sstvHorizontalShiftPixels).toInt();
    if (sstvHorizontalShiftPixels < -320 || sstvHorizontalShiftPixels > 320) {
        sstvHorizontalShiftPixels = 0;
    }
    sstvRedShiftPixels = settings.value("SSTV/redShiftPixels", sstvRedShiftPixels).toInt();
    sstvBlueShiftPixels = settings.value("SSTV/blueShiftPixels", sstvBlueShiftPixels).toInt();
    sstvNoiseReductionEnabled = settings.value("SSTV/noiseReductionEnabled", sstvNoiseReductionEnabled).toBool();
    sstvAgcEnabled = settings.value("SSTV/agcEnabled", sstvAgcEnabled).toBool();
    sstvWaveletDenoiseEnabled = settings.value("SSTV/waveletDenoiseEnabled", sstvWaveletDenoiseEnabled).toBool();
    if (sstvRedShiftPixels < -96 || sstvRedShiftPixels > 96) {
        sstvRedShiftPixels = 0;
    }
    if (sstvBlueShiftPixels < -96 || sstvBlueShiftPixels > 96) {
        sstvBlueShiftPixels = 0;
    }

    rttyPreset = settings.value("RTTY/preset", rttyPreset).toString();
    rttyBaudRate = settings.value("RTTY/baudRate", rttyBaudRate).toDouble();
    rttyShiftHz = settings.value("RTTY/shiftHz", rttyShiftHz).toInt();
    rttyMarkHz = 2125;
    rttyReverse = settings.value("RTTY/reverse", rttyReverse).toBool();
    rttyAfcEnabled = settings.value("RTTY/afcEnabled", rttyAfcEnabled).toBool();
    rttyAfcRangeHz = settings.value("RTTY/afcRangeHz", rttyAfcRangeHz).toInt();
    rttyNoiseReductionEnabled = settings.value("RTTY/noiseReductionEnabled", rttyNoiseReductionEnabled).toBool();
    rttyAgcEnabled = settings.value("RTTY/agcEnabled", rttyAgcEnabled).toBool();
    rttyAdaptiveLineEnhancerEnabled = settings.value("RTTY/adaptiveLineEnhancerEnabled", rttyAdaptiveLineEnhancerEnabled).toBool();
    rttyMatchedFilterEnabled = settings.value("RTTY/matchedFilterEnabled", rttyMatchedFilterEnabled).toBool();
    rttyMarkSpaceEnhancerEnabled = settings.value("RTTY/markSpaceEnhancerEnabled", rttyMarkSpaceEnhancerEnabled).toBool();
    rttyMultiDecodeEnabled = settings.value("RTTY/multiDecodeEnabled", rttyMultiDecodeEnabled).toBool();
    rttyOverlayCallsignsEnabled = settings.value("RTTY/overlayCallsignsEnabled", rttyOverlayCallsignsEnabled).toBool();
    rttyContestEnhancedEnabled = settings.value("RTTY/contestEnhancedEnabled", rttyContestEnhancedEnabled).toBool();
    rttySecondPassEnabled = settings.value("RTTY/secondPassEnabled", rttySecondPassEnabled).toBool();
    rttyMaxParallelDecoders = settings.value("RTTY/maxParallelDecoders", rttyMaxParallelDecoders).toInt();
    if (rttyAfcRangeHz < 5 || rttyAfcRangeHz > 100) {
        rttyAfcRangeHz = 20;
    }
    if (rttyMaxParallelDecoders < 2 || rttyMaxParallelDecoders > 32) {
        rttyMaxParallelDecoders = 8;
    }

    bpsk31Variant = settings.value("BPSK31/variant", bpsk31Variant).toString();
    if (bpsk31Variant != "BPSK31" && bpsk31Variant != "BPSK63" &&
        bpsk31Variant != "BPSK125" && bpsk31Variant != "BPSK250" &&
        bpsk31Variant != "BPSK500" && bpsk31Variant != "BPSK1000" &&
        bpsk31Variant != "QPSK31" && bpsk31Variant != "QPSK63" &&
        bpsk31Variant != "QPSK125" && bpsk31Variant != "QPSK250" &&
        bpsk31Variant != "QPSK500") {
        bpsk31Variant = "BPSK31";
    }
    bpsk31ToneHz = 1000;
    bpsk31AfcEnabled = settings.value("BPSK31/afcEnabled", bpsk31AfcEnabled).toBool();
    bpsk31AfcRangeHz = settings.value("BPSK31/afcRangeHz", bpsk31AfcRangeHz).toInt();
    if (bpsk31AfcRangeHz < 5 || bpsk31AfcRangeHz > 100) {
        bpsk31AfcRangeHz = 20;
    }
    bpsk31InvertBits = settings.value("BPSK31/invertBits", bpsk31InvertBits).toBool();
    bpsk31NoiseReductionEnabled = settings.value("BPSK31/noiseReductionEnabled", bpsk31NoiseReductionEnabled).toBool();
    bpsk31AgcEnabled = settings.value("BPSK31/agcEnabled", bpsk31AgcEnabled).toBool();
    bpsk31CoherentTrackingEnabled = settings.value("BPSK31/coherentTrackingEnabled", bpsk31CoherentTrackingEnabled).toBool();

    mfskVariant = settings.value("MFSK/variant", mfskVariant).toString().toUpper();
    if (mfskVariant != "MFSK16" && mfskVariant != "MFSK32") {
        mfskVariant = "MFSK16";
    }
    mfskCenterHz = settings.value("MFSK/centerHz", mfskCenterHz).toInt();
    if (mfskCenterHz < 300 || mfskCenterHz > 3300) {
        mfskCenterHz = 1000;
    }
    mfskAfcEnabled = settings.value("MFSK/afcEnabled", mfskAfcEnabled).toBool();
    mfskAfcRangeHz = settings.value("MFSK/afcRangeHz", mfskAfcRangeHz).toInt();
    if (mfskAfcRangeHz < 5 || mfskAfcRangeHz > 200) {
        mfskAfcRangeHz = 50;
    }
    mfskNoiseReductionEnabled = settings.value("MFSK/noiseReductionEnabled", mfskNoiseReductionEnabled).toBool();
    mfskAgcEnabled = settings.value("MFSK/agcEnabled", mfskAgcEnabled).toBool();

    cwToneHz = settings.value("CW/toneHz", cwToneHz).toInt();
    if (cwToneHz < 250 || cwToneHz > 2500) {
        cwToneHz = 1000;
    }
    cwSecondaryToneHz = settings.value("CW/secondaryToneHz", cwSecondaryToneHz).toInt();
    if (cwSecondaryToneHz < 250 || cwSecondaryToneHz > 2500) {
        cwSecondaryToneHz = 1400;
    }
    cwSecondaryEnabled = settings.value("CW/secondaryEnabled", cwSecondaryEnabled).toBool();
    cwWpm = settings.value("CW/wpm", cwWpm).toInt();
    cwBandwidthHz = settings.value("CW/bandwidthHz", cwBandwidthHz).toInt();
    cwAfcEnabled = settings.value("CW/afcEnabled", cwAfcEnabled).toBool();
    cwAfcRangeHz = settings.value("CW/afcRangeHz", cwAfcRangeHz).toInt();
    cwAutoWpm = settings.value("CW/autoWpm", cwAutoWpm).toBool();
    if (cwAfcRangeHz < 5 || cwAfcRangeHz > 100) {
        cwAfcRangeHz = 20;
    }
    cwNoiseReductionEnabled = settings.value("CW/noiseReductionEnabled", cwNoiseReductionEnabled).toBool();
    cwAgcEnabled = settings.value("CW/agcEnabled", cwAgcEnabled).toBool();
    cwAdaptiveLineEnhancerEnabled = settings.value("CW/adaptiveLineEnhancerEnabled", cwAdaptiveLineEnhancerEnabled).toBool();

    hellVariant = settings.value("Hell/variant", hellVariant).toString();
    if (hellVariant != "FeldHell" && hellVariant != "FSK105") {
        hellVariant = "FeldHell";
    }
    hellToneHz = 1000;
    hellColumnRate = settings.value("Hell/columnRate", hellColumnRate).toDouble();
    if (hellColumnRate < 2.0 || hellColumnRate > 80.0) {
        hellColumnRate = 17.5;
    }
    hellBandwidthHz = settings.value("Hell/bandwidthHz", hellBandwidthHz).toInt();
    if (hellBandwidthHz < 40 || hellBandwidthHz > 800) {
        hellBandwidthHz = 245;
    }
    hellAfcEnabled = settings.value("Hell/afcEnabled", hellAfcEnabled).toBool();
    hellAfcRangeHz = settings.value("Hell/afcRangeHz", hellAfcRangeHz).toInt();
    if (hellAfcRangeHz < 5 || hellAfcRangeHz > 100) {
        hellAfcRangeHz = 20;
    }
    hellNoiseReductionEnabled = settings.value("Hell/noiseReductionEnabled", hellNoiseReductionEnabled).toBool();
    hellAgcEnabled = settings.value("Hell/agcEnabled", hellAgcEnabled).toBool();

    // Station identity is no longer migrated from old FT8-only keys.
    // It must be explicitly configured in Settings -> User/QTH.
    ft8MyCallsign.clear();
    ft8MyGrid.clear();
    ft8Band = settings.value("FT8/band", ft8Band).toString().trimmed();
    if (ft8Band.isEmpty()) {
        ft8Band = "20m";
    }
    ft8DxCallsign = settings.value("FT8/dxCallsign", ft8DxCallsign).toString().trimmed().toUpper();
    ft8DxGrid = settings.value("FT8/dxGrid", ft8DxGrid).toString().trimmed().toUpper();
    ft8RxFrequencyHz = settings.value("FT8/rxFrequencyHz", ft8RxFrequencyHz).toInt();
    ft8TxFrequencyHz = settings.value("FT8/txFrequencyHz", ft8TxFrequencyHz).toInt();
    ft8RxFrequencyHz = qBound(100, ft8RxFrequencyHz, 3000);
    ft8TxFrequencyHz = qBound(100, ft8TxFrequencyHz, 3000);
    ft8TxFirstPeriod = settings.value("FT8/txFirstPeriod", ft8TxFirstPeriod).toBool();
    ft8AutoSequence = settings.value("FT8/autoSequence", ft8AutoSequence).toBool();
    ft8CqRepeat = settings.value("FT8/cqRepeat", ft8CqRepeat).toBool();
    ft8AutoLog = settings.value("FT8/autoLog", ft8AutoLog).toBool();
    ft8HoldTxFrequency = settings.value("FT8/holdTxFrequency", ft8HoldTxFrequency).toBool();
    ft8TxFrequencyStrategy = settings.value("FT8/txFrequencyStrategy",
                                            ft8HoldTxFrequency ? QStringLiteral("fixed") : ft8TxFrequencyStrategy)
                               .toString().trimmed().toLower();
    if (ft8TxFrequencyStrategy != QStringLiteral("fixed") &&
        ft8TxFrequencyStrategy != QStringLiteral("auto_free") &&
        ft8TxFrequencyStrategy != QStringLiteral("follow_rx")) {
        ft8TxFrequencyStrategy = ft8HoldTxFrequency ? QStringLiteral("fixed") : QStringLiteral("auto_free");
    }
    ft8TxGuardHz = qBound(50, settings.value("FT8/txGuardHz", ft8TxGuardHz).toInt(), 250);
    ft8LiveDecodeDepth = settings.value("FT8/liveDecodeDepth", ft8LiveDecodeDepth).toString().trimmed().toLower();
    if (ft8LiveDecodeDepth == "deep") {
        // v3.22: Deep/4-pass was removed from the operator UI because it did not
        // add decodes on the reference WAVs and only consumed latency.  Migrate
        // old profiles to the adaptive 2-pass live default.
        ft8LiveDecodeDepth = "adaptive";
    }
    if (ft8LiveDecodeDepth != "fast" && ft8LiveDecodeDepth != "adaptive") {
        // migrate older boolean profiles without exposing the former checkbox mess
        ft8LiveDecodeDepth = "adaptive";
    }
    ft8DeepDecode = (ft8LiveDecodeDepth == "adaptive");
    ft8DspPlusDecode = false;
    // v1.94 migration: WSJT-X default is that selecting/double-clicking a
    // decode moves TX with RX.  Hold-TX-frequency is an explicit exception,
    // not the default.  Migrate old v1.90-v1.93 profiles once.
    if (!settings.value("FT8/holdTxFrequencyDefaultMigrated194", false).toBool()) {
        ft8HoldTxFrequency = false;
        if (!settings.contains("FT8/txFrequencyStrategy")) {
            ft8TxFrequencyStrategy = QStringLiteral("auto_free");
        }
        settings.setValue("FT8/holdTxFrequency", false);
        settings.setValue("FT8/txFrequencyStrategy", ft8TxFrequencyStrategy);
        settings.setValue("FT8/holdTxFrequencyDefaultMigrated194", true);
    }
    ft8CqRepeatTimeoutMinutes = settings.value("FT8/cqRepeatTimeoutMinutes", ft8CqRepeatTimeoutMinutes).toInt();
    if (ft8CqRepeatTimeoutMinutes < 1 || ft8CqRepeatTimeoutMinutes > 60) {
        ft8CqRepeatTimeoutMinutes = 5;
    }
    // v2.19: migrate old profiles that stored "classic" or "enhanced".
    // The UI no longer exposes multiple pseudo-engines: MM uses one
    // MSHV-derived native FT pipeline and future work should make that path
    // progressively more faithful to upstream MSHV algorithms.
    const QString oldFtDecodeEngine = settings.value("FT8/decodeEngine", ft8DecodeEngine).toString();
    Q_UNUSED(oldFtDecodeEngine);
    ft8DecodeEngine = "mshv";

    ftHighlightMyCallBackground = safeColourName(settings.value("FT8/highlightMyCallBackground", ftHighlightMyCallBackground), "#FFEBEB");
    ftHighlightMyCallForeground = safeColourName(settings.value("FT8/highlightMyCallForeground", ftHighlightMyCallForeground), "#DC0000");
    ftHighlightCqBackground = safeColourName(settings.value("FT8/highlightCqBackground", ftHighlightCqBackground), "#E8FFE8");
    ftHighlightCqForeground = safeColourName(settings.value("FT8/highlightCqForeground", ftHighlightCqForeground), "#145014");
    ftHighlightWorkedBackground = safeColourName(settings.value("FT8/highlightWorkedBackground", ftHighlightWorkedBackground), "#F0F0F0");
    ftHighlightWorkedForeground = safeColourName(settings.value("FT8/highlightWorkedForeground", ftHighlightWorkedForeground), "#777777");
    ftHighlightTxBackground = safeColourName(settings.value("FT8/highlightTxBackground", ftHighlightTxBackground), "#FFF75F");
    ftHighlightTxForeground = safeColourName(settings.value("FT8/highlightTxForeground", ftHighlightTxForeground), "#000000");
    ftHighlightNewCountryEnabled = settings.value("FT8/highlightNewCountryEnabled", ftHighlightNewCountryEnabled).toBool();
    ftWatchListIconEnabled = settings.value("FT8/watchListIconEnabled", ftWatchListIconEnabled).toBool();
    ftBlacklistCalls = settings.value("FT8/blacklistCalls", ftBlacklistCalls).toStringList();
    ftWatchListCalls = settings.value("FT8/watchListCalls", ftWatchListCalls).toStringList();
    ftAutoQsoDuplicatePolicy = settings.value("FT8/autoQsoDuplicatePolicy", ftAutoQsoDuplicatePolicy).toString().trimmed().toLower();
    if (ftAutoQsoDuplicatePolicy != "none" && ftAutoQsoDuplicatePolicy != "never_worked" && ftAutoQsoDuplicatePolicy != "recent") {
        ftAutoQsoDuplicatePolicy = "never_worked";
    }
    ftAutoQsoRecentHours = settings.value("FT8/autoQsoRecentHours", ftAutoQsoRecentHours).toInt();
    if (ftAutoQsoRecentHours < 1 || ftAutoQsoRecentHours > 168) {
        ftAutoQsoRecentHours = 24;
    }
    ft8CqRepeatCount = settings.value("FT8/cqRepeatCount", ft8CqRepeatCount).toInt();
    if (ft8CqRepeatCount < 1 || ft8CqRepeatCount > 99) {
        ft8CqRepeatCount = 3;
    }
    ft8NoResponseRetryCount = settings.value("FT8/noResponseRetryCount", ft8NoResponseRetryCount).toInt();
    if (ft8NoResponseRetryCount < 1 || ft8NoResponseRetryCount > 12) {
        ft8NoResponseRetryCount = 3;
    }
    ftAutoQsoFlowJson = settings.value("FT8/autoQsoFlowJson", ftAutoQsoFlowJson).toString();
    ftAutoQsoFlowShadowMode = settings.value("FT8/autoQsoFlowShadowMode", ftAutoQsoFlowShadowMode).toBool();


    rotatorEnabled = settings.value("Rotator/enabled", rotatorEnabled).toBool();
    rotatorAutoConnect = settings.value("Rotator/autoConnect", rotatorAutoConnect).toBool();
    rotatorShowWindowOnStart = settings.value("Rotator/showWindowOnStart", rotatorShowWindowOnStart).toBool();
    rotatorTrackSelectedQso = settings.value("Rotator/trackSelectedQso", rotatorTrackSelectedQso).toBool();
    rotatorTrackOnlyWhenQsoActive = settings.value("Rotator/trackOnlyWhenQsoActive", rotatorTrackOnlyWhenQsoActive).toBool();
    rotatorBlockFtTxUntilReady = settings.value("Rotator/blockFtTxUntilReady", rotatorBlockFtTxUntilReady).toBool();
    rotatorActiveProfile = settings.value("Rotator/activeProfile", rotatorActiveProfile).toInt();
    if (rotatorActiveProfile < 0 || rotatorActiveProfile > 2) rotatorActiveProfile = 0;

    // Load legacy single-rotator fields first so existing settings.mad files migrate into profile 1.
    rotatorHamlibModel = settings.value("Rotator/hamlibModel", rotatorHamlibModel).toInt();
    if (rotatorHamlibModel < 1) rotatorHamlibModel = 1;
    rotatorPath = settings.value("Rotator/path", rotatorPath).toString().trimmed();
    rotatorBaudRate = settings.value("Rotator/baudRate", rotatorBaudRate).toInt();
    if (rotatorBaudRate < 300 || rotatorBaudRate > 1000000) rotatorBaudRate = 9600;
    rotatorPollIntervalMs = settings.value("Rotator/pollIntervalMs", rotatorPollIntervalMs).toInt();
    if (rotatorPollIntervalMs < 250 || rotatorPollIntervalMs > 10000) rotatorPollIntervalMs = 750;
    rotatorUseElevation = settings.value("Rotator/useElevation", rotatorUseElevation).toBool();
    rotatorParkAzimuth = settings.value("Rotator/parkAzimuth", rotatorParkAzimuth).toDouble();
    if (rotatorParkAzimuth < 0.0 || rotatorParkAzimuth >= 360.0) rotatorParkAzimuth = 0.0;
    rotatorParkElevation = settings.value("Rotator/parkElevation", rotatorParkElevation).toDouble();
    if (rotatorParkElevation < -10.0 || rotatorParkElevation > 180.0) rotatorParkElevation = 0.0;
    rotatorTargetToleranceDeg = settings.value("Rotator/targetToleranceDeg", rotatorTargetToleranceDeg).toInt();
    if (rotatorTargetToleranceDeg < 0 || rotatorTargetToleranceDeg > 45) rotatorTargetToleranceDeg = 3;

    rotatorProfiles[0].hamlibModel = rotatorHamlibModel;
    rotatorProfiles[0].path = rotatorPath;
    rotatorProfiles[0].baudRate = rotatorBaudRate;
    rotatorProfiles[0].pollIntervalMs = rotatorPollIntervalMs;
    rotatorProfiles[0].useElevation = rotatorUseElevation;
    rotatorProfiles[0].parkAzimuth = rotatorParkAzimuth;
    rotatorProfiles[0].parkElevation = rotatorParkElevation;
    rotatorProfiles[0].targetToleranceDeg = rotatorTargetToleranceDeg;

    for (int i = 0; i < 3; ++i) {
        const QString group = QStringLiteral("Rotator/Profile%1/").arg(i + 1);
        RotatorProfileSettings &rp = rotatorProfiles[i];
        rp.label = settings.value(group + QStringLiteral("label"), rp.label).toString().trimmed();
        if (rp.label.isEmpty()) rp.label = QStringLiteral("Rotator %1").arg(i + 1);
        rp.bandsCsv = settings.value(group + QStringLiteral("bands"), rp.bandsCsv).toString().trimmed();
        rp.hamlibModel = settings.value(group + QStringLiteral("hamlibModel"), rp.hamlibModel).toInt();
        if (rp.hamlibModel < 1) rp.hamlibModel = 1;
        rp.path = settings.value(group + QStringLiteral("path"), rp.path).toString().trimmed();
        rp.baudRate = settings.value(group + QStringLiteral("baudRate"), rp.baudRate).toInt();
        if (rp.baudRate < 300 || rp.baudRate > 1000000) rp.baudRate = 9600;
        rp.pollIntervalMs = settings.value(group + QStringLiteral("pollIntervalMs"), rp.pollIntervalMs).toInt();
        if (rp.pollIntervalMs < 250 || rp.pollIntervalMs > 10000) rp.pollIntervalMs = 750;
        rp.useElevation = settings.value(group + QStringLiteral("useElevation"), rp.useElevation).toBool();
        rp.overlap = settings.value(group + QStringLiteral("overlap"), rp.overlap).toBool();
        rp.azimuthGeometryPreset = settings.value(group + QStringLiteral("azimuthGeometryPreset"), rp.azimuthGeometryPreset).toString().trimmed();
        if (rp.azimuthGeometryPreset.isEmpty()) rp.azimuthGeometryPreset = rp.overlap ? QStringLiteral("yaesu-450") : QStringLiteral("north-stop-360");
        rp.azimuthStopDeg = settings.value(group + QStringLiteral("azimuthStopDeg"), rp.azimuthStopDeg).toDouble();
        if (rp.azimuthStopDeg < -360.0 || rp.azimuthStopDeg > 540.0) rp.azimuthStopDeg = 0.0;
        rp.autoReverseOnStall = settings.value(group + QStringLiteral("autoReverseOnStall"), rp.autoReverseOnStall).toBool();
        rp.noMovementTimeoutMs = settings.value(group + QStringLiteral("noMovementTimeoutMs"), rp.noMovementTimeoutMs).toInt();
        if (rp.noMovementTimeoutMs < 500 || rp.noMovementTimeoutMs > 30000) rp.noMovementTimeoutMs = 3000;
        rp.noMovementThresholdDeg = settings.value(group + QStringLiteral("noMovementThresholdDeg"), rp.noMovementThresholdDeg).toDouble();
        if (rp.noMovementThresholdDeg < 0.1 || rp.noMovementThresholdDeg > 30.0) rp.noMovementThresholdDeg = 2.0;
        rp.parkAzimuth = settings.value(group + QStringLiteral("parkAzimuth"), rp.parkAzimuth).toDouble();
        if (rp.parkAzimuth < 0.0 || rp.parkAzimuth >= 360.0) rp.parkAzimuth = 0.0;
        rp.parkElevation = settings.value(group + QStringLiteral("parkElevation"), rp.parkElevation).toDouble();
        if (rp.parkElevation < -10.0 || rp.parkElevation > 180.0) rp.parkElevation = 0.0;
        rp.targetToleranceDeg = settings.value(group + QStringLiteral("targetToleranceDeg"), rp.targetToleranceDeg).toInt();
        if (rp.targetToleranceDeg < 0 || rp.targetToleranceDeg > 45) rp.targetToleranceDeg = 3;
        rp.azimuthMinDeg = settings.value(group + QStringLiteral("azimuthMinDeg"), rp.azimuthMinDeg).toDouble();
        if (rp.azimuthMinDeg < -360.0 || rp.azimuthMinDeg > 540.0) rp.azimuthMinDeg = 0.0;
        rp.azimuthMaxDeg = settings.value(group + QStringLiteral("azimuthMaxDeg"), rp.azimuthMaxDeg).toDouble();
        if (rp.azimuthMaxDeg <= rp.azimuthMinDeg || rp.azimuthMaxDeg > 540.0) rp.azimuthMaxDeg = rp.overlap ? 450.0 : 359.9;
        rp.elevationMinDeg = settings.value(group + QStringLiteral("elevationMinDeg"), rp.elevationMinDeg).toDouble();
        if (rp.elevationMinDeg < -10.0 || rp.elevationMinDeg > 180.0) rp.elevationMinDeg = 0.0;
        rp.elevationMaxDeg = settings.value(group + QStringLiteral("elevationMaxDeg"), rp.elevationMaxDeg).toDouble();
        if (rp.elevationMaxDeg < rp.elevationMinDeg || rp.elevationMaxDeg > 180.0) rp.elevationMaxDeg = 90.0;
        rp.azimuthMsPerDeg = settings.value(group + QStringLiteral("azimuthMsPerDeg"), rp.azimuthMsPerDeg).toDouble();
        if (rp.azimuthMsPerDeg < 0.0) rp.azimuthMsPerDeg = 0.0;
        rp.elevationMsPerDeg = settings.value(group + QStringLiteral("elevationMsPerDeg"), rp.elevationMsPerDeg).toDouble();
        if (rp.elevationMsPerDeg < 0.0) rp.elevationMsPerDeg = 0.0;
        rp.startupDelayMs = settings.value(group + QStringLiteral("startupDelayMs"), rp.startupDelayMs).toInt();
        if (rp.startupDelayMs < 0 || rp.startupDelayMs > 30000) rp.startupDelayMs = 250;
        rp.settleDelayMs = settings.value(group + QStringLiteral("settleDelayMs"), rp.settleDelayMs).toInt();
        if (rp.settleDelayMs < 0 || rp.settleDelayMs > 30000) rp.settleDelayMs = 700;
        rp.txGuardMarginMs = settings.value(group + QStringLiteral("txGuardMarginMs"), rp.txGuardMarginMs).toInt();
        if (rp.txGuardMarginMs < 0 || rp.txGuardMarginMs > 30000) rp.txGuardMarginMs = 800;
        rp.calibrationStampUtc = settings.value(group + QStringLiteral("calibrationStampUtc"), rp.calibrationStampUtc).toString();
        rp.peakSearchAlgorithm = settings.value(group + QStringLiteral("peakSearchAlgorithm"), rp.peakSearchAlgorithm).toString().trimmed();

        QVector<RotatorBandSettings> rows = defaultRotatorBandSettings();
        for (RotatorBandSettings &row : rows) {
            const QString bandGroup = group + QStringLiteral("Band/") + bandSettingsKey(row.band) + QLatin1Char('/');
            row.enabled = settings.value(bandGroup + QStringLiteral("enabled"), row.enabled).toBool();
            row.azimuthSearchSpanDeg = settings.value(bandGroup + QStringLiteral("azimuthSearchSpanDeg"), row.azimuthSearchSpanDeg).toDouble();
            row.elevationSearchSpanDeg = settings.value(bandGroup + QStringLiteral("elevationSearchSpanDeg"), row.elevationSearchSpanDeg).toDouble();
            row.autoPeakEnabled = settings.value(bandGroup + QStringLiteral("autoPeakEnabled"), row.autoPeakEnabled).toBool();
        }
        rp.bandSettings = rows;
        normalizeRotatorBandSettings(rp);
    }

    // Keep legacy aliases synchronized with active profile 1.
    rotatorHamlibModel = rotatorProfiles[0].hamlibModel;
    rotatorPath = rotatorProfiles[0].path;
    rotatorBaudRate = rotatorProfiles[0].baudRate;
    rotatorPollIntervalMs = rotatorProfiles[0].pollIntervalMs;
    rotatorUseElevation = rotatorProfiles[0].useElevation;
    rotatorParkAzimuth = rotatorProfiles[0].parkAzimuth;
    rotatorParkElevation = rotatorProfiles[0].parkElevation;
    rotatorTargetToleranceDeg = rotatorProfiles[0].targetToleranceDeg;

    textMyCallsign = settings.value("Text/myCallsign", textMyCallsign).toString();
    textMyName = settings.value("Text/myName", textMyName).toString();
    textMyQth = settings.value("Text/myQth", textMyQth).toString();
    textMyLocator = settings.value("Text/myLocator", textMyLocator).toString();
    // Per-QSO values are transient runtime form fields, not persistent settings.
    settings.remove("Text/remoteCallsign");
    settings.remove("Text/remoteName");
    settings.remove("Text/remoteQth");
    settings.remove("Text/report");
    textRig = settings.value("Text/rig", textRig).toString();
    textAntenna = settings.value("Text/antenna", textAntenna).toString();
    textPower = settings.value("Text/power", textPower).toString();
    textMacroLabels = settings.value("Text/macroLabels", textMacroLabels).toStringList();
    textMacroTexts = settings.value("Text/macroTexts", textMacroTexts).toStringList();
    normalizeTextMacros(textMacroLabels, textMacroTexts);

    // Station identity belongs only to Settings -> User/QTH.  Do not migrate or
    // fall back to legacy FT8-only keys; if User/QTH is empty, TX will be blocked
    // until the operator explicitly configures it.
    ft8MyCallsign = textMyCallsign.trimmed().toUpper();
    ft8MyGrid = textMyLocator.trimmed().toUpper();
}

bool AppSettings::save() const
{
    QSettings settings(settingsFilePath(), QSettings::IniFormat);

    settings.setValue("Audio/inputDevice", audioInputName);
    settings.setValue("Audio/outputDevice", audioOutputName);
    settings.setValue("Audio/sampleRate", audioSampleRate);
    settings.setValue("Logbook/filePath", logbookFilePath);
    settings.setValue("Logbook/strikeWorkedCalls", logbookStrikeWorkedCalls);
    settings.setValue("Logbook/visibleFieldsConfigured", logbookVisibleFieldsConfigured);
    settings.setValue("Logbook/visibleFields", logbookVisibleFields);
    settings.setValue("Display/waterfallColorScalePercent", waterfallColorScalePercent);
    settings.setValue("Display/waterfallPalette", waterfallPalette);
    settings.setValue("Audio/rxClockPpm", audioRxClockPpm);
    settings.setValue("Audio/txClockPpm", audioTxClockPpm);

    settings.setValue("PTT/serialPort", pttPortName);
    settings.setValue("PTT/method", pttMethod);

    settings.setValue("Hamlib/catEnabled", hamlibCatEnabled);
    settings.setValue("Hamlib/pttEnabled", hamlibPttEnabled);
    settings.setValue("Hamlib/updateFt8Band", hamlibUpdateFt8Band);
    settings.setValue("Hamlib/rigModel", hamlibRigModel);
    const QString effectiveHamlibRigPath = hamlibTcpAddress.trimmed().isEmpty()
                                              ? hamlibSerialPath.trimmed()
                                              : hamlibTcpAddress.trimmed();
    settings.setValue("Hamlib/rigPath", effectiveHamlibRigPath);
    settings.setValue("Hamlib/serialPort", hamlibSerialPath.trimmed());
    settings.setValue("Hamlib/tcpAddress", hamlibTcpAddress.trimmed());
    settings.setValue("Hamlib/baudRate", hamlibBaudRate);
    settings.setValue("Hamlib/dataBits", hamlibDataBits);
    settings.setValue("Hamlib/stopBits", hamlibStopBits);
    settings.setValue("Hamlib/handshake", hamlibHandshake);
    settings.setValue("Hamlib/forceDtr", hamlibForceDtr);
    settings.setValue("Hamlib/forceRts", hamlibForceRts);
    settings.setValue("Hamlib/pollIntervalMs", hamlibPollIntervalMs);
    settings.setValue("Hamlib/txAudioRoute", hamlibTxAudioRoute);
    settings.setValue("Hamlib/transmitAudioSource", hamlibTransmitAudioSource);

    settings.setValue("Scheduler/enabledModes", schedulerQsyEnabledModes);
    settings.setValue("Scheduler/qsyPlanJson", schedulerQsyPlanJson);

    settings.setValue("WeatherFax/lpm", weatherFaxLpm);
    settings.remove("WeatherFax/blackHz");
    settings.remove("WeatherFax/whiteHz");
    settings.setValue("WeatherFax/autoStartPhasing", weatherFaxAutoStartPhasing);
    settings.setValue("WeatherFax/autoToneTracking", weatherFaxAutoToneTracking);
    settings.setValue("WeatherFax/inputBandpass", weatherFaxInputBandpass);
    settings.setValue("WeatherFax/autoSlantCorrection", false);
    settings.setValue("WeatherFax/manualSlantPpm", 0.0);
    settings.setValue("WeatherFax/linePreset", weatherFaxLinePreset);
    settings.setValue("WeatherFax/imageLines", weatherFaxImageLines);
    settings.setValue("WeatherFax/autoSave", weatherFaxAutoSave);
    settings.setValue("WeatherFax/outputFolder", weatherFaxOutputFolder);
    settings.setValue("WeatherFax/endOfSignal", weatherFaxEndOfSignal);
    settings.setValue("WeatherFax/endOfSignalTimeoutSec", weatherFaxEndOfSignalTimeoutSec);
    settings.setValue("WeatherFax/noiseReductionEnabled", weatherFaxNoiseReductionEnabled);
    settings.setValue("WeatherFax/agcEnabled", weatherFaxAgcEnabled);
    settings.setValue("WeatherFax/waveletDenoiseEnabled", weatherFaxWaveletDenoiseEnabled);

    settings.setValue("SSTV/mode", sstvMode);
    settings.setValue("SSTV/autoSync", sstvAutoSync);
    settings.setValue("SSTV/horizontalShiftPixels", sstvHorizontalShiftPixels);
    settings.setValue("SSTV/redShiftPixels", sstvRedShiftPixels);
    settings.setValue("SSTV/blueShiftPixels", sstvBlueShiftPixels);
    settings.setValue("SSTV/noiseReductionEnabled", sstvNoiseReductionEnabled);
    settings.setValue("SSTV/agcEnabled", sstvAgcEnabled);
    settings.setValue("SSTV/waveletDenoiseEnabled", sstvWaveletDenoiseEnabled);

    settings.setValue("RTTY/preset", rttyPreset);
    settings.setValue("RTTY/baudRate", rttyBaudRate);
    settings.setValue("RTTY/shiftHz", rttyShiftHz);
    settings.remove("RTTY/markHz");
    settings.setValue("RTTY/reverse", rttyReverse);
    settings.setValue("RTTY/afcEnabled", rttyAfcEnabled);
    settings.setValue("RTTY/afcRangeHz", rttyAfcRangeHz);
    settings.setValue("RTTY/noiseReductionEnabled", rttyNoiseReductionEnabled);
    settings.setValue("RTTY/agcEnabled", rttyAgcEnabled);
    settings.setValue("RTTY/adaptiveLineEnhancerEnabled", rttyAdaptiveLineEnhancerEnabled);
    settings.setValue("RTTY/matchedFilterEnabled", rttyMatchedFilterEnabled);
    settings.setValue("RTTY/markSpaceEnhancerEnabled", rttyMarkSpaceEnhancerEnabled);
    settings.setValue("RTTY/multiDecodeEnabled", rttyMultiDecodeEnabled);
    settings.setValue("RTTY/overlayCallsignsEnabled", rttyOverlayCallsignsEnabled);
    settings.setValue("RTTY/contestEnhancedEnabled", rttyContestEnhancedEnabled);
    settings.setValue("RTTY/secondPassEnabled", rttySecondPassEnabled);
    settings.setValue("RTTY/maxParallelDecoders", rttyMaxParallelDecoders);

    settings.setValue("BPSK31/variant", bpsk31Variant);
    settings.remove("BPSK31/toneHz");
    settings.setValue("BPSK31/afcEnabled", bpsk31AfcEnabled);
    settings.setValue("BPSK31/afcRangeHz", bpsk31AfcRangeHz);
    settings.setValue("BPSK31/invertBits", bpsk31InvertBits);
    settings.setValue("BPSK31/noiseReductionEnabled", bpsk31NoiseReductionEnabled);
    settings.setValue("BPSK31/agcEnabled", bpsk31AgcEnabled);
    settings.setValue("BPSK31/coherentTrackingEnabled", bpsk31CoherentTrackingEnabled);

    settings.setValue("MFSK/variant", mfskVariant);
    settings.setValue("MFSK/centerHz", mfskCenterHz);
    settings.setValue("MFSK/afcEnabled", mfskAfcEnabled);
    settings.setValue("MFSK/afcRangeHz", mfskAfcRangeHz);
    settings.setValue("MFSK/noiseReductionEnabled", mfskNoiseReductionEnabled);
    settings.setValue("MFSK/agcEnabled", mfskAgcEnabled);

    settings.setValue("CW/toneHz", cwToneHz);
    settings.setValue("CW/secondaryToneHz", cwSecondaryToneHz);
    settings.setValue("CW/secondaryEnabled", cwSecondaryEnabled);
    settings.setValue("CW/wpm", cwWpm);
    settings.setValue("CW/bandwidthHz", cwBandwidthHz);
    settings.setValue("CW/afcEnabled", cwAfcEnabled);
    settings.setValue("CW/afcRangeHz", cwAfcRangeHz);
    settings.setValue("CW/autoWpm", cwAutoWpm);
    settings.setValue("CW/noiseReductionEnabled", cwNoiseReductionEnabled);
    settings.setValue("CW/agcEnabled", cwAgcEnabled);
    settings.setValue("CW/adaptiveLineEnhancerEnabled", cwAdaptiveLineEnhancerEnabled);

    settings.setValue("Hell/variant", hellVariant);
    settings.remove("Hell/toneHz");
    settings.setValue("Hell/columnRate", hellColumnRate);
    settings.setValue("Hell/bandwidthHz", hellBandwidthHz);
    settings.setValue("Hell/afcEnabled", hellAfcEnabled);
    settings.setValue("Hell/afcRangeHz", hellAfcRangeHz);
    settings.setValue("Hell/noiseReductionEnabled", hellNoiseReductionEnabled);
    settings.setValue("Hell/agcEnabled", hellAgcEnabled);

    settings.remove("FT8/myCallsign");
    settings.remove("FT8/myGrid");
    settings.setValue("FT8/band", ft8Band);
    settings.setValue("FT8/dxCallsign", ft8DxCallsign);
    settings.setValue("FT8/dxGrid", ft8DxGrid);
    settings.setValue("FT8/rxFrequencyHz", ft8RxFrequencyHz);
    settings.setValue("FT8/txFrequencyHz", ft8TxFrequencyHz);
    settings.setValue("FT8/txFirstPeriod", ft8TxFirstPeriod);
    settings.setValue("FT8/autoSequence", ft8AutoSequence);
    settings.setValue("FT8/cqRepeat", ft8CqRepeat);
    settings.setValue("FT8/autoLog", ft8AutoLog);
    settings.setValue("FT8/holdTxFrequency", ft8HoldTxFrequency);
    settings.setValue("FT8/txFrequencyStrategy", ft8TxFrequencyStrategy);
    settings.setValue("FT8/txGuardHz", ft8TxGuardHz);
    settings.setValue("FT8/liveDecodeDepth", ft8LiveDecodeDepth);
    settings.setValue("FT8/deepDecode", ft8DeepDecode);
    settings.setValue("FT8/dspPlusDecode", false);
    settings.setValue("FT8/cqRepeatTimeoutMinutes", ft8CqRepeatTimeoutMinutes);
    settings.setValue("FT8/decodeEngine", ft8DecodeEngine);
    settings.setValue("FT8/highlightMyCallBackground", ftHighlightMyCallBackground);
    settings.setValue("FT8/highlightMyCallForeground", ftHighlightMyCallForeground);
    settings.setValue("FT8/highlightCqBackground", ftHighlightCqBackground);
    settings.setValue("FT8/highlightCqForeground", ftHighlightCqForeground);
    settings.setValue("FT8/highlightWorkedBackground", ftHighlightWorkedBackground);
    settings.setValue("FT8/highlightWorkedForeground", ftHighlightWorkedForeground);
    settings.setValue("FT8/highlightTxBackground", ftHighlightTxBackground);
    settings.setValue("FT8/highlightTxForeground", ftHighlightTxForeground);
    settings.setValue("FT8/highlightNewCountryEnabled", ftHighlightNewCountryEnabled);
    settings.setValue("FT8/watchListIconEnabled", ftWatchListIconEnabled);
    settings.setValue("FT8/blacklistCalls", ftBlacklistCalls);
    settings.setValue("FT8/watchListCalls", ftWatchListCalls);
    settings.setValue("FT8/autoQsoDuplicatePolicy", ftAutoQsoDuplicatePolicy);
    settings.setValue("FT8/autoQsoRecentHours", ftAutoQsoRecentHours);
    settings.setValue("FT8/cqRepeatCount", ft8CqRepeatCount);
    settings.setValue("FT8/noResponseRetryCount", ft8NoResponseRetryCount);
    settings.setValue("FT8/autoQsoFlowJson", ftAutoQsoFlowJson);
    settings.setValue("FT8/autoQsoFlowShadowMode", ftAutoQsoFlowShadowMode);


    settings.setValue("Rotator/enabled", rotatorEnabled);
    settings.setValue("Rotator/autoConnect", rotatorAutoConnect);
    settings.setValue("Rotator/showWindowOnStart", rotatorShowWindowOnStart);
    settings.setValue("Rotator/trackSelectedQso", rotatorTrackSelectedQso);
    settings.setValue("Rotator/trackOnlyWhenQsoActive", rotatorTrackOnlyWhenQsoActive);
    settings.setValue("Rotator/blockFtTxUntilReady", rotatorBlockFtTxUntilReady);
    settings.setValue("Rotator/activeProfile", rotatorActiveProfile);

    // Legacy profile-1 keys for backward compatibility.
    settings.setValue("Rotator/hamlibModel", rotatorProfiles[0].hamlibModel);
    settings.setValue("Rotator/path", rotatorProfiles[0].path);
    settings.setValue("Rotator/baudRate", rotatorProfiles[0].baudRate);
    settings.setValue("Rotator/pollIntervalMs", rotatorProfiles[0].pollIntervalMs);
    settings.setValue("Rotator/useElevation", rotatorProfiles[0].useElevation);
    settings.setValue("Rotator/parkAzimuth", rotatorProfiles[0].parkAzimuth);
    settings.setValue("Rotator/parkElevation", rotatorProfiles[0].parkElevation);
    settings.setValue("Rotator/targetToleranceDeg", rotatorProfiles[0].targetToleranceDeg);

    for (int i = 0; i < 3; ++i) {
        const QString group = QStringLiteral("Rotator/Profile%1/").arg(i + 1);
        const RotatorProfileSettings &rp = rotatorProfiles[i];
        settings.setValue(group + QStringLiteral("label"), rp.label);
        settings.setValue(group + QStringLiteral("bands"), rp.bandsCsv);
        settings.setValue(group + QStringLiteral("hamlibModel"), rp.hamlibModel);
        settings.setValue(group + QStringLiteral("path"), rp.path);
        settings.setValue(group + QStringLiteral("baudRate"), rp.baudRate);
        settings.setValue(group + QStringLiteral("pollIntervalMs"), rp.pollIntervalMs);
        settings.setValue(group + QStringLiteral("useElevation"), rp.useElevation);
        settings.setValue(group + QStringLiteral("overlap"), rp.overlap);
        settings.setValue(group + QStringLiteral("azimuthGeometryPreset"), rp.azimuthGeometryPreset);
        settings.setValue(group + QStringLiteral("azimuthStopDeg"), rp.azimuthStopDeg);
        settings.setValue(group + QStringLiteral("autoReverseOnStall"), rp.autoReverseOnStall);
        settings.setValue(group + QStringLiteral("noMovementTimeoutMs"), rp.noMovementTimeoutMs);
        settings.setValue(group + QStringLiteral("noMovementThresholdDeg"), rp.noMovementThresholdDeg);
        settings.setValue(group + QStringLiteral("parkAzimuth"), rp.parkAzimuth);
        settings.setValue(group + QStringLiteral("parkElevation"), rp.parkElevation);
        settings.setValue(group + QStringLiteral("targetToleranceDeg"), rp.targetToleranceDeg);
        settings.setValue(group + QStringLiteral("azimuthMinDeg"), rp.azimuthMinDeg);
        settings.setValue(group + QStringLiteral("azimuthMaxDeg"), rp.azimuthMaxDeg);
        settings.setValue(group + QStringLiteral("elevationMinDeg"), rp.elevationMinDeg);
        settings.setValue(group + QStringLiteral("elevationMaxDeg"), rp.elevationMaxDeg);
        settings.setValue(group + QStringLiteral("azimuthMsPerDeg"), rp.azimuthMsPerDeg);
        settings.setValue(group + QStringLiteral("elevationMsPerDeg"), rp.elevationMsPerDeg);
        settings.setValue(group + QStringLiteral("startupDelayMs"), rp.startupDelayMs);
        settings.setValue(group + QStringLiteral("settleDelayMs"), rp.settleDelayMs);
        settings.setValue(group + QStringLiteral("txGuardMarginMs"), rp.txGuardMarginMs);
        settings.setValue(group + QStringLiteral("calibrationStampUtc"), rp.calibrationStampUtc);
        settings.setValue(group + QStringLiteral("peakSearchAlgorithm"), rp.peakSearchAlgorithm);
        for (const RotatorBandSettings &row : rp.bandSettings) {
            const QString bandGroup = group + QStringLiteral("Band/") + bandSettingsKey(row.band) + QLatin1Char('/');
            settings.setValue(bandGroup + QStringLiteral("enabled"), row.enabled);
            settings.setValue(bandGroup + QStringLiteral("azimuthSearchSpanDeg"), row.azimuthSearchSpanDeg);
            settings.setValue(bandGroup + QStringLiteral("elevationSearchSpanDeg"), row.elevationSearchSpanDeg);
            settings.setValue(bandGroup + QStringLiteral("autoPeakEnabled"), row.autoPeakEnabled);
        }
    }

    settings.setValue("Text/myCallsign", textMyCallsign);
    settings.setValue("Text/myName", textMyName);
    settings.setValue("Text/myQth", textMyQth);
    settings.setValue("Text/myLocator", textMyLocator);
    settings.remove("Text/remoteCallsign");
    settings.remove("Text/remoteName");
    settings.remove("Text/remoteQth");
    settings.remove("Text/report");
    settings.setValue("Text/rig", textRig);
    settings.setValue("Text/antenna", textAntenna);
    settings.setValue("Text/power", textPower);
    settings.setValue("Text/macroLabels", textMacroLabels);
    settings.setValue("Text/macroTexts", textMacroTexts);

    settings.sync();

    return settings.status() == QSettings::NoError;
}
