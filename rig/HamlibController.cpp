#include "HamlibController.h"

#include "../settings/AppSettings.h"

#include <QByteArray>
#include <QTcpSocket>
#include <QtGlobal>

#include <cstdint>

#ifdef MADMODEM_WITH_HAMLIB
#include <hamlib/rig.h>
#include <cstring>
#endif


namespace {
constexpr int kHamRadioDeluxeModelId = 900001;
constexpr int kHrdDefaultPort = 7809;
constexpr quint32 kHrdMagic1 = 0x1234ABCDu;
constexpr quint32 kHrdMagic2 = 0xABCD1234u;

quint32 readLe32(const char *p)
{
    const auto b0 = static_cast<quint32>(static_cast<unsigned char>(p[0]));
    const auto b1 = static_cast<quint32>(static_cast<unsigned char>(p[1]));
    const auto b2 = static_cast<quint32>(static_cast<unsigned char>(p[2]));
    const auto b3 = static_cast<quint32>(static_cast<unsigned char>(p[3]));
    return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
}

void appendLe32(QByteArray *out, quint32 value)
{
    out->append(static_cast<char>(value & 0xffu));
    out->append(static_cast<char>((value >> 8) & 0xffu));
    out->append(static_cast<char>((value >> 16) & 0xffu));
    out->append(static_cast<char>((value >> 24) & 0xffu));
}

QStringList hrdSplitList(QString text)
{
    text = text.trimmed();
    if (text.isEmpty()) {
        return QStringList();
    }
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    QStringList parts = text.split(QLatin1Char(','), Qt::SkipEmptyParts);
#else
    QStringList parts = text.split(QLatin1Char(','), QString::SkipEmptyParts);
#endif
    for (QString &part : parts) {
        part = part.trimmed();
    }
    return parts;
}

QByteArray portPathForHamlib(QString path)
{
    path = path.trimmed();
#if defined(Q_OS_WIN)
    // WSJT-X style Windows COM path handling: COM10 and above need the \\.\ prefix.
    const QString winComPrefix = QStringLiteral("\\\\.\\");
    if (path.startsWith(QStringLiteral("COM"), Qt::CaseInsensitive) &&
        !path.startsWith(winComPrefix)) {
        bool ok = false;
        const int number = path.mid(3).toInt(&ok);
        if (ok && number >= 10) {
            path = winComPrefix + path;
        }
    }
#endif
    return path.toLocal8Bit();
}

#ifdef MADMODEM_WITH_HAMLIB
bool setHamlibConfIfPresent(RIG *rig, const char *name, const QByteArray &value, QString *error = nullptr)
{
    if (rig == nullptr || name == nullptr) {
        return false;
    }
    const hamlib_token_t token = rig_token_lookup(rig, name);
    if (token == RIG_CONF_END) {
        return true;
    }
    const int ret = rig_set_conf(rig, token, value.constData());
    if (ret != RIG_OK) {
        if (error != nullptr) {
            *error = QStringLiteral("%1: %2")
                         .arg(QString::fromLatin1(name), QString::fromLocal8Bit(rigerror(ret)));
        }
        return false;
    }
    return true;
}
#endif

bool textMatchesAny(const QString &value, const QStringList &patterns)
{
    const QString simplified = value.simplified().replace(QLatin1Char(' '), QLatin1Char('~'));
    for (const QString &pattern : patterns) {
        if (simplified.compare(pattern, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}
} // namespace


HamlibController::HamlibController(QObject *parent)
    : QObject(parent)
{
    m_pollTimer = new QTimer(this);
    m_pollTimer->setSingleShot(false);
    connect(m_pollTimer, &QTimer::timeout,
            this, &HamlibController::pollNow);
    setStatus(QStringLiteral("CAT disabled"));
}

HamlibController::~HamlibController()
{
    disconnectRig();
}

bool HamlibController::isCompiledWithHamlib()
{
#ifdef MADMODEM_WITH_HAMLIB
    return true;
#else
    return false;
#endif
}

int HamlibController::hamRadioDeluxeModelId()
{
    return kHamRadioDeluxeModelId;
}

bool HamlibController::isHamRadioDeluxeModel(int rigModel)
{
    return rigModel == kHamRadioDeluxeModelId;
}

namespace {

bool routeRequestsDataPktMode(const HamlibController::Config &cfg)
{
    return cfg.txAudioRoute == QStringLiteral("data_pkt");
}

bool routeRequestsUsbMode(const HamlibController::Config &cfg)
{
    return cfg.txAudioRoute == QStringLiteral("usb");
}

#ifdef MADMODEM_WITH_HAMLIB
constexpr pbwidth_t kDigitalWidePassbandHz = 3000;

// WSJT-X does not model the radio as "exactly two physical VFOs".  It asks
// Hamlib which *logical* split family the backend exposes and uses A/B when
// available, otherwise MAIN/SUB.  Those selectors may themselves map to a
// richer physical topology (MAIN_A/MAIN_B/SUB_A/SUB_B, satellite resources,
// slices, etc.); that mapping remains the Hamlib backend's responsibility.
//
// Preserve an already-active split TX selector when the radio reports one.
// Otherwise choose the WSJT-X-style logical opposite of the current RX side.
bool hamlibVfoListContains(const QByteArray &vfoList, vfo_t wanted)
{
    const auto tokens = vfoList.simplified().split(' ');
    for (const QByteArray &token : tokens) {
        if (!token.isEmpty() && rig_parse_vfo(token.constData()) == wanted) {
            return true;
        }
    }
    return false;
}

// Choose only among VFO selectors that Hamlib publicly reports as usable.
// Never inspect Hamlib's private rig_state: Hamlib 4.7+ deliberately hides it
// from applications.  An already-active split selector is authoritative.  For
// a new split we mirror WSJT-X's A/B or MAIN/SUB policy, but preserve the more
// specific MAIN_A/MAIN_B and SUB_A/SUB_B pairs when the backend reports them.
// If the current VFO is a concrete selector with no unambiguous partner (for
// example VFO C on a multi-VFO radio), fail closed instead of guessing.
vfo_t wsjtLikeFtRigSplitTxVfo(RIG *rig, vfo_t reportedRxVfo,
                               split_t previousSplit, vfo_t previousTxVfo)
{
    if (rig == nullptr) return RIG_VFO_NONE;

    if (previousSplit == RIG_SPLIT_ON &&
        previousTxVfo != RIG_VFO_NONE &&
        previousTxVfo != RIG_VFO_CURR) {
        return previousTxVfo;
    }

    char listBuffer[256] = {};
    if (rig_get_vfo_list(rig, listBuffer, static_cast<int>(sizeof(listBuffer))) != RIG_OK) {
        return RIG_VFO_NONE;
    }
    const QByteArray vfoList(listBuffer);
    const auto has = [&](vfo_t vfo) { return hamlibVfoListContains(vfoList, vfo); };

    const vfo_t current = reportedRxVfo;

    if (current == RIG_VFO_MAIN_A && has(RIG_VFO_MAIN_B)) return RIG_VFO_MAIN_B;
    if (current == RIG_VFO_MAIN_B && has(RIG_VFO_MAIN_A)) return RIG_VFO_MAIN_A;
    if (current == RIG_VFO_SUB_A && has(RIG_VFO_SUB_B)) return RIG_VFO_SUB_B;
    if (current == RIG_VFO_SUB_B && has(RIG_VFO_SUB_A)) return RIG_VFO_SUB_A;
    if (current == RIG_VFO_A && has(RIG_VFO_B)) return RIG_VFO_B;
    if (current == RIG_VFO_B && has(RIG_VFO_A)) return RIG_VFO_A;
    if (current == RIG_VFO_MAIN && has(RIG_VFO_SUB)) return RIG_VFO_SUB;
    if (current == RIG_VFO_SUB && has(RIG_VFO_MAIN)) return RIG_VFO_MAIN;

    // Some radios cannot report which VFO is selected.  This is the same
    // conservative startup assumption used by WSJT-X for that CAT class:
    // receive is A/MAIN, transmit is B/SUB.  We make that assumption only when
    // rig_get_vfo() did not produce a concrete selector.
    if (current == RIG_VFO_NONE || current == RIG_VFO_CURR || current == RIG_VFO_VFO) {
        if (has(RIG_VFO_A) && has(RIG_VFO_B)) return RIG_VFO_B;
        if (has(RIG_VFO_MAIN) && has(RIG_VFO_SUB)) return RIG_VFO_SUB;
    }

    return RIG_VFO_NONE;
}
#endif

} // namespace

void HamlibController::configure(const Config &config)
{
    const bool oldHrd = isHamRadioDeluxeModel(m_config.rigModel);
    const bool newHrd = isHamRadioDeluxeModel(config.rigModel);
    const bool needReconnect = (m_config.catEnabled != config.catEnabled) ||
                               (m_config.pttEnabled != config.pttEnabled) ||
                               (m_config.readOnlyTest != config.readOnlyTest) ||
                               (m_config.rigModel != config.rigModel) ||
                               (m_config.rigPath != config.rigPath) ||
                               (m_config.baudRate != config.baudRate) ||
                               (m_config.dataBits != config.dataBits) ||
                               (m_config.stopBits != config.stopBits) ||
                               (m_config.handshake != config.handshake) ||
                               (m_config.forceDtr != config.forceDtr) ||
                               (m_config.forceRts != config.forceRts) ||
                               (m_config.pttMethod != config.pttMethod) ||
                               (m_config.pttPort != config.pttPort) ||
                               (m_config.transmitAudioSource != config.transmitAudioSource) ||
                               (oldHrd != newHrd);

    m_config = config;
    m_config.rigModel = qMax(1, m_config.rigModel);
    m_config.baudRate = qMax(300, m_config.baudRate);
    if (m_config.dataBits != 0 && m_config.dataBits != 7 && m_config.dataBits != 8) {
        m_config.dataBits = 0;
    }
    if (m_config.stopBits != 0 && m_config.stopBits != 1 && m_config.stopBits != 2) {
        m_config.stopBits = 0;
    }
    m_config.handshake = m_config.handshake.trimmed().toLower();
    if (m_config.handshake != "default" && m_config.handshake != "none" &&
        m_config.handshake != "xonxoff" && m_config.handshake != "hardware") {
        m_config.handshake = "default";
    }
    m_config.forceDtr = m_config.forceDtr.trimmed().toLower();
    if (m_config.forceDtr != "unchanged" && m_config.forceDtr != "on" && m_config.forceDtr != "off") {
        m_config.forceDtr = "unchanged";
    }
    m_config.forceRts = m_config.forceRts.trimmed().toLower();
    if (m_config.forceRts != "unchanged" && m_config.forceRts != "on" && m_config.forceRts != "off") {
        m_config.forceRts = "unchanged";
    }
    m_config.pttMethod = m_config.pttMethod.trimmed().toLower();
    if (m_config.pttMethod != "none" && m_config.pttMethod != "cat_hamlib" &&
        m_config.pttMethod != "serial_rts" && m_config.pttMethod != "serial_dtr") {
        m_config.pttMethod = "none";
    }
    m_config.pollIntervalMs = qBound(250, m_config.pollIntervalMs, 10000);
    // WSJT-X-like UI exposes only the TX rig mode policy.
    // Legacy v2.59/v2.60 DATA/vendor routes are migrated to Data/Pkt.
    if (m_config.txAudioRoute == "data" ||
        m_config.txAudioRoute == "force_data_usb" ||
        m_config.txAudioRoute == "kenwood_usb" ||
        m_config.txAudioRoute == "kenwood_acc2" ||
        m_config.txAudioRoute == "kenwood_lan") {
        m_config.txAudioRoute = "data_pkt";
    }
    if (m_config.txAudioRoute != "default" &&
        m_config.txAudioRoute != "usb" &&
        m_config.txAudioRoute != "data_pkt") {
        m_config.txAudioRoute = "default";
    }
    m_config.transmitAudioSource = m_config.transmitAudioSource.trimmed().toLower();
    if (m_config.transmitAudioSource == "front" || m_config.transmitAudioSource == "mic" ||
        m_config.transmitAudioSource == "front_mic") {
        m_config.transmitAudioSource = "front_mic";
    } else {
        m_config.transmitAudioSource = "rear_data";
    }

    m_pollTimer->setInterval(m_config.pollIntervalMs);

    if (!m_config.catEnabled && !m_config.pttEnabled) {
        disconnectRig();
        setStatus(QStringLiteral("CAT disabled"));
        return;
    }

    if (needReconnect) {
        disconnectRig();
    }

    if (isHamRadioDeluxeModel(m_config.rigModel)) {
        if (m_config.catEnabled) {
            connectRig();
        } else {
            setStatus(QStringLiteral("HRD CAT ready for PTT"));
        }
        return;
    }

    if (!isCompiledWithHamlib()) {
        disconnectRig();
        setStatus(QStringLiteral("Hamlib support not compiled in"));
        emitError(QStringLiteral("Hamlib support is not compiled in this MadModem build."));
        return;
    }

    if (m_config.catEnabled) {
        connectRig();
    }
}

void HamlibController::configureFromSettings(const AppSettings &settings)
{
    Config cfg;
    cfg.catEnabled = settings.hamlibCatEnabled;
    const QString settingsPttMethod = settings.pttMethod.trimmed().toLower();
    const QString settingsPttPort = settings.pttPortName.trimmed();
    const bool pttUsesCatPort = settingsPttPort.isEmpty() ||
                                settingsPttPort.compare(QStringLiteral("CAT"), Qt::CaseInsensitive) == 0 ||
                                settingsPttPort.compare(settings.hamlibSerialPath.trimmed(), Qt::CaseInsensitive) == 0;
    cfg.pttEnabled = settings.hamlibPttEnabled ||
                     ((settingsPttMethod == QStringLiteral("serial_rts") || settingsPttMethod == QStringLiteral("serial_dtr")) && pttUsesCatPort);
    cfg.rigModel = settings.hamlibRigModel;
    cfg.rigPath = settings.hamlibRigPath;
    cfg.baudRate = settings.hamlibBaudRate;
    cfg.dataBits = settings.hamlibDataBits;
    cfg.stopBits = settings.hamlibStopBits;
    cfg.handshake = settings.hamlibHandshake;
    cfg.forceDtr = settings.hamlibForceDtr;
    cfg.forceRts = settings.hamlibForceRts;
    cfg.pttMethod = settings.pttMethod;
    cfg.pttPort = settings.pttPortName;
    cfg.pollIntervalMs = settings.hamlibPollIntervalMs;
    cfg.txAudioRoute = settings.hamlibTxAudioRoute;
    cfg.transmitAudioSource = settings.hamlibTransmitAudioSource;
    configure(cfg);
}

bool HamlibController::isConnected() const
{
    if (isHamRadioDeluxeModel(m_config.rigModel)) {
        return m_hrdSocket != nullptr && m_hrdSocket->state() == QTcpSocket::ConnectedState;
    }
#ifdef MADMODEM_WITH_HAMLIB
    return m_rig != nullptr;
#else
    return false;
#endif
}

bool HamlibController::connectRig()
{
    if (isHamRadioDeluxeModel(m_config.rigModel)) {
        return connectHrd();
    }
#ifndef MADMODEM_WITH_HAMLIB
    setStatus(QStringLiteral("Hamlib support not compiled in"));
    return false;
#else
    if (m_rig != nullptr) {
        if (m_config.catEnabled && !m_pollTimer->isActive()) {
            m_pollTimer->start();
        }
        return true;
    }

    rig_set_debug(RIG_DEBUG_NONE);

    RIG *rig = rig_init(static_cast<rig_model_t>(m_config.rigModel));
    if (rig == nullptr) {
        emitError(QStringLiteral("Hamlib rig_init failed for model %1.").arg(m_config.rigModel));
        setStatus(QStringLiteral("CAT init failed"));
        return false;
    }

    auto setHamlibConfig = [this, rig](const char *name,
                                        const QByteArray &value,
                                        bool required) -> bool {
        QString error;
        const bool ok = setHamlibConfIfPresent(rig, name, value, &error);
        if (!ok) {
            const QString msg = QStringLiteral("Hamlib configuration '%1' failed: %2")
                                    .arg(QString::fromLatin1(name),
                                         error.isEmpty() ? QStringLiteral("unknown error") : error);
            emitError(msg);
            return !required;
        }
        const hamlib_token_t token = rig_token_lookup(rig, name);
        if (token == RIG_CONF_END && required) {
            emitError(QStringLiteral("Hamlib configuration token '%1' is not supported by this Hamlib build/model.")
                          .arg(QString::fromLatin1(name)));
            return false;
        }
        return true;
    };

    // WSJT-X-derived conservative Hamlib setup: configure only the user-selected
    // port/path, serial speed and (for read-only/audio-only) ptt_type=None
    // before rig_open(). Do not invent client names, RTS/DTR states, serial
    // handshake, ptt_share or fallback TCP endpoints here.
    const QString rigPathText = m_config.rigPath.trimmed();
    if (rigPathText.isEmpty()) {
        rig_cleanup(rig);
        emitError(QStringLiteral("Hamlib CAT path is empty: choose/type a serial port or an explicit TCP/rigctld endpoint."));
        setStatus(QStringLiteral("CAT path empty"));
        return false;
    }

    const QByteArray path = portPathForHamlib(rigPathText);
    if (!setHamlibConfig("rig_pathname", path, true)) {
        rig_cleanup(rig);
        setStatus(QStringLiteral("CAT configuration failed"));
        return false;
    }

    if (m_config.baudRate > 0) {
        const QByteArray speed = QByteArray::number(m_config.baudRate);
        setHamlibConfig("serial_speed", speed, false);
    }

    // WSJT-X style: serial details are applied only when the user explicitly
    // selected non-default values.  Opening/Test CAT never forces RTS/DTR,
    // never sends PTT OFF and never silently changes handshake.
    if (m_config.dataBits == 7 || m_config.dataBits == 8) {
        setHamlibConfig("data_bits", QByteArray::number(m_config.dataBits), false);
    }
    if (m_config.stopBits == 1 || m_config.stopBits == 2) {
        setHamlibConfig("stop_bits", QByteArray::number(m_config.stopBits), false);
    }
    if (m_config.handshake == "none") {
        setHamlibConfig("serial_handshake", QByteArrayLiteral("None"), false);
    } else if (m_config.handshake == "xonxoff") {
        setHamlibConfig("serial_handshake", QByteArrayLiteral("XONXOFF"), false);
    } else if (m_config.handshake == "hardware") {
        setHamlibConfig("serial_handshake", QByteArrayLiteral("Hardware"), false);
    }
    if (m_config.forceDtr == "on" || m_config.forceDtr == "off") {
        setHamlibConfig("dtr_state", m_config.forceDtr == "on" ? QByteArrayLiteral("ON") : QByteArrayLiteral("OFF"), false);
    }
    if ((m_config.forceRts == "on" || m_config.forceRts == "off") && m_config.handshake != "hardware") {
        setHamlibConfig("rts_state", m_config.forceRts == "on" ? QByteArrayLiteral("ON") : QByteArrayLiteral("OFF"), false);
    }

    if (!m_config.readOnlyTest) {
        if (m_config.pttMethod == "cat_hamlib") {
            // Use the rig backend default CAT PTT, exactly like WSJT-X.
        } else if (m_config.pttMethod == "serial_rts" || m_config.pttMethod == "serial_dtr") {
            const QString pttPortText = m_config.pttPort.trimmed();
            const bool separatePttPort = !pttPortText.isEmpty() &&
                                        pttPortText.compare(QStringLiteral("CAT"), Qt::CaseInsensitive) != 0 &&
                                        pttPortText.compare(rigPathText, Qt::CaseInsensitive) != 0;
            if (separatePttPort) {
                setHamlibConfig("ptt_pathname", portPathForHamlib(pttPortText), false);
            }
            setHamlibConfig("ptt_type", m_config.pttMethod == "serial_dtr" ? QByteArrayLiteral("DTR") : QByteArrayLiteral("RTS"), false);
            setHamlibConfig("ptt_share", QByteArrayLiteral("1"), false);
        } else if (m_config.pttMethod == "none") {
            // Audio-only/VOX style. Avoid touching this in read-only CAT tests.
            setHamlibConfig("ptt_type", QByteArrayLiteral("None"), false);
        }
    }

    /*
     * Keep serial CAT opening deliberately conservative.  v1.56-v1.64 tried
     * to make read-only CAT tests extra safe by forcing optional Hamlib
     * configuration tokens such as ptt_type=None, dtr_state=OFF,
     * rts_state=OFF and client=MadModem before rig_open().  Real Hamlib
     * builds/backends are not consistent here: some Kenwood/Icom/Yaesu
     * models either do not expose those tokens or reject them in a way that
     * makes the following rig_open() fail, even though the same radio works
     * correctly with WSJT-X and with older MadModem releases.
     *
     * Therefore CAT open follows the WSJT-X/v1.40-style path: set only the
     * required user-selected path, serial speed and a best-effort ptt_type=None
     * for read-only/audio-only cases, then open the rig.  Read-only safety is
     * enforced by this class itself: readOnlyTest blocks setPtt(true), and the
     * CAT test performs only read operations (frequency and mode). We do not send any PTT command during
     * Test CAT.
     */

    const int openRet = rig_open(rig);
    if (openRet != RIG_OK) {
        const QString err = QString::fromLocal8Bit(rigerror(openRet));
        rig_cleanup(rig);
        emitError(QStringLiteral("Hamlib CAT open failed: %1").arg(err));
        setStatus(QStringLiteral("CAT open failed"));
        return false;
    }

    m_rig = rig;
    setStatus(QStringLiteral("CAT connected"));

    if (m_config.catEnabled) {
        m_pollTimer->start();
        pollNow();
    }

    return true;
#endif
}

bool HamlibController::disconnectRig()
{
    if (m_pollTimer != nullptr) {
        m_pollTimer->stop();
    }

    // Fail safe before releasing either backend.  Never rely on closing the
    // serial/TCP handle to imply PTT OFF: several radios latch TX state.
    const bool backendWasConnected = isConnected();
    const bool pttOffConfirmed = !backendWasConnected || setPtt(false);
    if (!pttOffConfirmed) {
        emitError(QStringLiteral("CAT disconnect: the radio did not confirm PTT OFF; transmitter state remains unknown after closing the backend"));
    }
    if (pttOffConfirmed && m_ftSplitTxActive && !endFtSplitTx()) {
        emitError(QStringLiteral("CAT disconnect: FT split state could not be fully restored before closing the backend."));
    } else if (!pttOffConfirmed && m_ftSplitTxActive) {
        emitError(QStringLiteral("CAT disconnect: FT split restore was not attempted because PTT OFF was not confirmed; inspect the radio manually."));
    }
    disconnectHrd();
#ifdef MADMODEM_WITH_HAMLIB
    if (m_rig != nullptr) {
        RIG *rig = static_cast<RIG *>(m_rig);
        rig_close(rig);
        rig_cleanup(rig);
        m_rig = nullptr;
    }
#endif
    m_lastFrequencyHz = 0.0;
    if (!m_lastModeName.isEmpty()) {
        m_lastModeName.clear();
        emit modeChanged(QString());
    }
    // setPtt(false) already published the confirmed transition.  If it failed,
    // deliberately retain the conservative ON/unknown state instead of lying
    // to the UI merely because the transport handle has been closed.
    if (pttOffConfirmed) {
        m_lastPtt = false;
    } else if (backendWasConnected) {
        m_lastPtt = true;
    }
#ifdef MADMODEM_WITH_HAMLIB
    m_havePreTxMode = false;
    m_preTxMode = 0;
    m_preTxPassband = 0;
    m_preTxModeVfo = 0;
    m_preTxModeWasSplit = false;
    m_ftSplitRxVfo = 0;
    m_ftSplitTxVfo = 0;
    m_ftSplitPreviousState = 0;
    m_ftSplitPreviousTxVfo = 0;
    m_ftSplitPreviousTxFrequencyHz = 0.0;
#endif
    m_ftSplitTxActive = false;
    m_ftSplitTxOperation.clear();
    m_ftSplitRxFrequencyHz = 0.0;
    m_ftSplitTxFrequencyHz = 0.0;
    return pttOffConfirmed;
}

void HamlibController::pollNow()
{
    if (!m_config.catEnabled) {
        return;
    }

    // Split/Fake-It temporarily changes a CAT frequency for TX. Never publish
    // that transient value as the station RX frequency or band selection.
    if (m_ftSplitTxActive) {
        return;
    }

    if (isHamRadioDeluxeModel(m_config.rigModel)) {
        pollHrd();
        return;
    }

#ifndef MADMODEM_WITH_HAMLIB
    return;
#else
    if (m_rig == nullptr && !connectRig()) {
        return;
    }
    if (m_rig == nullptr) {
        return;
    }

    RIG *rig = static_cast<RIG *>(m_rig);

    // Always ask the rig which VFO is active before reading the frequency.
    // Hamlib's Kenwood backend can otherwise keep STATE(current_vfo) cached
    // as VFO A after open; on rigs such as the TS-890S, manual changes on
    // the front panel while operating on VFO B would then look like a stale
    // frequency in MadModem.  If the backend cannot report the active VFO we
    // fall back to RIG_VFO_CURR, preserving the old behaviour.
    vfo_t activeVfo = RIG_VFO_CURR;
    const int vfoRet = rig_get_vfo(rig, &activeVfo);
    if (vfoRet != RIG_OK || activeVfo == RIG_VFO_NONE) {
        activeVfo = RIG_VFO_CURR;
    }

    freq_t freq = 0.0;
    int freqRet = rig_get_freq(rig, activeVfo, &freq);
    if (freqRet != RIG_OK && activeVfo != RIG_VFO_CURR) {
        freqRet = rig_get_freq(rig, RIG_VFO_CURR, &freq);
    }

    if (freqRet == RIG_OK && freq > 0.0) {
        const double hz = static_cast<double>(freq);
        if (qAbs(hz - m_lastFrequencyHz) >= 1.0) {
            m_lastFrequencyHz = hz;
            emit frequencyChanged(hz);
        }
        setStatus(QStringLiteral("CAT connected"));
    } else if (freqRet != RIG_OK) {
        emitError(QStringLiteral("Hamlib get frequency failed: %1")
                      .arg(QString::fromLocal8Bit(rigerror(freqRet))));
        setStatus(QStringLiteral("CAT frequency error"));
    }

    // Poll the active demodulation mode together with frequency.  RTTY uses
    // this only as a polarity prior; no mode-setting command is issued here.
    rmode_t mode = RIG_MODE_NONE;
    pbwidth_t width = 0;
    int modeRet = rig_get_mode(rig, activeVfo, &mode, &width);
    if (modeRet != RIG_OK && activeVfo != RIG_VFO_CURR) {
        modeRet = rig_get_mode(rig, RIG_VFO_CURR, &mode, &width);
    }
    if (modeRet == RIG_OK && mode != RIG_MODE_NONE) {
        const QString modeName = QString::fromLatin1(rig_strrmode(mode)).trimmed().toUpper();
        if (!modeName.isEmpty() && modeName != m_lastModeName) {
            m_lastModeName = modeName;
            emit modeChanged(modeName);
        }
    }
#endif
}


bool HamlibController::setFrequencyHz(double frequencyHz)
{
    if (frequencyHz <= 0.0) {
        return false;
    }
    if (m_ftSplitTxActive) {
        emitError(QStringLiteral("CAT frequency change blocked while an FT split TX transaction is active."));
        return false;
    }

    if (isHamRadioDeluxeModel(m_config.rigModel)) {
        if (m_hrdSocket == nullptr || m_hrdSocket->state() != QTcpSocket::ConnectedState) {
            if (!connectHrd()) {
                return false;
            }
        }
        QString error;
        const QString command = QStringLiteral("set frequency %1").arg(QString::number(frequencyHz, 'f', 0));
        if (!hrdSendSimpleCommand(command, &error)) {
            emitError(QStringLiteral("HRD set frequency failed: %1").arg(error));
            return false;
        }
        // A successful write is not a frequency measurement.  Read the radio
        // back immediately and publish that value, so the FT band selector is
        // driven by the actual rig state rather than by the requested QSY.
        m_lastFrequencyHz = 0.0;
        return pollHrd();
    }

#ifndef MADMODEM_WITH_HAMLIB
    emitError(QStringLiteral("Hamlib frequency set requested, but Hamlib support is not compiled in."));
    return false;
#else
    if (m_config.readOnlyTest) {
        emitError(QStringLiteral("Frequency set blocked: this controller instance is in read-only CAT-test mode."));
        return false;
    }
    if (m_rig == nullptr && !connectRig()) {
        return false;
    }
    if (m_rig == nullptr) {
        return false;
    }

    RIG *rig = static_cast<RIG *>(m_rig);

    // Tune the VFO that the radio reports as currently active.  This avoids
    // changing VFO A while the operator is actually listening on VFO B, which
    // was reported with the Kenwood TS-890S.
    vfo_t activeVfo = RIG_VFO_CURR;
    const int vfoRet = rig_get_vfo(rig, &activeVfo);
    if (vfoRet != RIG_OK || activeVfo == RIG_VFO_NONE) {
        activeVfo = RIG_VFO_CURR;
    }

    int ret = rig_set_freq(rig, activeVfo, static_cast<freq_t>(frequencyHz));
    if (ret != RIG_OK && activeVfo != RIG_VFO_CURR) {
        ret = rig_set_freq(rig, RIG_VFO_CURR, static_cast<freq_t>(frequencyHz));
    }
    if (ret != RIG_OK) {
        emitError(QStringLiteral("Hamlib set frequency failed: %1")
                      .arg(QString::fromLocal8Bit(rigerror(ret))));
        return false;
    }
    // A successful write is not a frequency measurement.  Force an immediate
    // CAT readback; pollNow() emits frequencyChanged only with the value that
    // the active VFO really reports.
    m_lastFrequencyHz = 0.0;
    pollNow();
    if (m_lastFrequencyHz <= 0.0) {
        emitError(QStringLiteral("Hamlib frequency set succeeded, but CAT readback failed."));
        return false;
    }
    return true;
#endif
}

bool HamlibController::beginFtSplitTx(const QString &operation, int rfShiftHz)
{
    const QString op = operation.trimmed().toLower();
    if (op != QStringLiteral("rig") && op != QStringLiteral("fake_it")) {
        emitError(QStringLiteral("FT split TX requested with an invalid operation mode: %1").arg(operation));
        return false;
    }
    if (m_config.readOnlyTest) {
        emitError(QStringLiteral("FT split TX blocked: this controller instance is in read-only CAT-test mode."));
        return false;
    }
    if (!m_config.catEnabled) {
        emitError(QStringLiteral("FT split TX requires CAT control to be enabled."));
        return false;
    }
    if (m_ftSplitTxActive) {
        emitError(QStringLiteral("FT split TX transaction is already active; refusing a second concurrent owner."));
        return false;
    }

    if (isHamRadioDeluxeModel(m_config.rigModel)) {
        if (op == QStringLiteral("rig")) {
            emitError(QStringLiteral("FT Split Operation/Rig is unavailable through the HRD backend because it does not expose Hamlib's native split-TX VFO abstraction; use Fake It or direct Hamlib CAT."));
            return false;
        }
        if (m_hrdSocket == nullptr || m_hrdSocket->state() != QTcpSocket::ConnectedState) {
            if (!connectHrd()) return false;
        }
        if (m_lastFrequencyHz <= 0.0 && !pollHrd()) {
            emitError(QStringLiteral("FT Fake It could not read the current HRD frequency before TX."));
            return false;
        }
        const double rxHz = m_lastFrequencyHz;
        const double txHz = rxHz + static_cast<double>(rfShiftHz);
        if (rxHz <= 0.0 || txHz <= 0.0) {
            emitError(QStringLiteral("FT Fake It calculated an invalid CAT frequency."));
            return false;
        }
        QString error;
        if (rfShiftHz != 0 &&
            !hrdSendSimpleCommand(QStringLiteral("set frequency %1").arg(QString::number(txHz, 'f', 0)), &error)) {
            emitError(QStringLiteral("FT Fake It could not set the temporary HRD TX frequency: %1").arg(error));
            return false;
        }
        m_ftSplitTxOperation = op;
        m_ftSplitRxFrequencyHz = rxHz;
        m_ftSplitTxFrequencyHz = txHz;
        m_ftSplitTxActive = true;
        setStatus(QStringLiteral("CAT connected / FT Fake It TX prepared"));
        return true;
    }

#ifndef MADMODEM_WITH_HAMLIB
    Q_UNUSED(rfShiftHz);
    emitError(QStringLiteral("FT split TX requested, but Hamlib support is not compiled in."));
    return false;
#else
    if (m_rig == nullptr && !connectRig()) return false;
    if (m_rig == nullptr) return false;

    RIG *rig = static_cast<RIG *>(m_rig);

    // The RX frequency is always the currently selected receive frequency.
    // Rig Split must never retune it.  A concrete VFO identity is only a hint
    // for choosing Hamlib's logical split partner; many valid backends (e.g.
    // IC-9700) intentionally do not implement rig_get_vfo().
    vfo_t reportedRxVfo = RIG_VFO_CURR;
    const int vfoRet = rig_get_vfo(rig, &reportedRxVfo);
    if (vfoRet != RIG_OK || reportedRxVfo == RIG_VFO_NONE) {
        reportedRxVfo = RIG_VFO_CURR;
    }

    freq_t rxFrequency = 0.0;
    int ret = rig_get_freq(rig, RIG_VFO_CURR, &rxFrequency);
    if (ret != RIG_OK || rxFrequency <= 0.0) {
        emitError(QStringLiteral("FT split TX could not read the current RX frequency: %1")
                      .arg(QString::fromLocal8Bit(rigerror(ret))));
        return false;
    }
    const freq_t targetFrequency = rxFrequency + static_cast<freq_t>(rfShiftHz);
    if (targetFrequency <= 0.0) {
        emitError(QStringLiteral("FT split TX calculated an invalid TX frequency."));
        return false;
    }

    if (op == QStringLiteral("fake_it")) {
        // WSJT-X/JTDX Fake It: deliberately retune the current RX VFO only for
        // the TX interval, then restore the exact RX frequency after PTT OFF.
        if (rfShiftHz != 0) {
            ret = rig_set_freq(rig, RIG_VFO_CURR, targetFrequency);
            if (ret != RIG_OK) {
                emitError(QStringLiteral("FT Fake It could not set the temporary TX frequency: %1")
                              .arg(QString::fromLocal8Bit(rigerror(ret))));
                return false;
            }
        }
        m_ftSplitTxOperation = op;
        m_ftSplitRxFrequencyHz = static_cast<double>(rxFrequency);
        m_ftSplitTxFrequencyHz = static_cast<double>(targetFrequency);
        m_ftSplitRxVfo = static_cast<qint64>(RIG_VFO_CURR);
        m_ftSplitTxVfo = static_cast<qint64>(RIG_VFO_CURR);
        m_ftSplitTxActive = true;
        setStatus(QStringLiteral("CAT connected / FT Fake It TX prepared"));
        return true;
    }

    // Native Rig Split. Preserve the operator's current split assignment.
    // For a new split, select only Hamlib's logical A/B or MAIN/SUB side using
    // the same vfo_list strategy as WSJT-X; Hamlib remains responsible for
    // mapping that selector to the radio's actual VFO/slice/bank topology.
    split_t previousSplit = RIG_SPLIT_OFF;
    vfo_t previousTxVfo = RIG_VFO_NONE;
    ret = rig_get_split_vfo(rig, RIG_VFO_CURR, &previousSplit, &previousTxVfo);
    if (ret != RIG_OK) {
        emitError(QStringLiteral("FT Split Operation/Rig could not snapshot the existing split state: %1")
                      .arg(QString::fromLocal8Bit(rigerror(ret))));
        return false;
    }

    const vfo_t txVfo = wsjtLikeFtRigSplitTxVfo(rig, reportedRxVfo, previousSplit, previousTxVfo);
    if (txVfo == RIG_VFO_NONE || txVfo == RIG_VFO_CURR) {
        emitError(QStringLiteral("FT Split Operation/Rig could not obtain a native split TX VFO from the Hamlib backend. Use Fake It for a single-VFO rig."));
        return false;
    }

    freq_t previousTxFrequency = 0.0;
    if (previousSplit == RIG_SPLIT_ON &&
        (previousTxVfo == RIG_VFO_NONE || previousTxVfo == RIG_VFO_CURR)) {
        emitError(QStringLiteral("FT Split Operation/Rig reported split ON without a concrete Hamlib TX VFO; refusing to overwrite an ambiguous radio state."));
        return false;
    }

    auto restorePreviousSplit = [&]() {
        vfo_t restoreTxVfo = previousTxVfo;
        if (restoreTxVfo == RIG_VFO_NONE || restoreTxVfo == RIG_VFO_CURR) restoreTxVfo = txVfo;

        // If we captured the TX-side frequency, restore it while split is
        // still enabled so the operator's dormant/active TX VFO is not left at
        // MadModem's 500-Hz compensated frequency.
        if (previousTxFrequency > 0.0) {
            const int selectRet = rig_set_split_vfo(rig, RIG_VFO_CURR, RIG_SPLIT_ON, restoreTxVfo);
            if (selectRet == RIG_OK) {
                rig_set_split_freq(rig, RIG_VFO_CURR, previousTxFrequency);
            }
        }
        rig_set_split_vfo(rig, RIG_VFO_CURR, previousSplit, restoreTxVfo);
    };

    // Establish the TX selector through Hamlib's public split API before asking
    // Hamlib to read/write split frequency.  Hamlib itself owns and updates its
    // internal TX-VFO state.  Split is enabled again last because some rigs
    // (notably Kenwood families) may leave split while TX-side state changes.
    ret = rig_set_split_vfo(rig, RIG_VFO_CURR, RIG_SPLIT_ON, txVfo);
    if (ret != RIG_OK) {
        emitError(QStringLiteral("FT Split Operation/Rig could not select the native split TX VFO: %1")
                      .arg(QString::fromLocal8Bit(rigerror(ret))));
        return false;
    }

    // Capture the selected TX-side frequency *after* Hamlib has established
    // the logical split selector but *before* MadModem changes it.  This also
    // preserves a dormant TX VFO when split was originally OFF, so leaving FT
    // operation cannot surprise the operator with a modified secondary VFO.
    ret = rig_get_split_freq(rig, RIG_VFO_CURR, &previousTxFrequency);
    if (ret != RIG_OK || previousTxFrequency <= 0.0) {
        restorePreviousSplit();
        emitError(QStringLiteral("FT Split Operation/Rig could not snapshot the selected split TX frequency: %1")
                      .arg(QString::fromLocal8Bit(rigerror(ret))));
        return false;
    }

    ret = rig_set_split_freq(rig, RIG_VFO_CURR, targetFrequency);
    if (ret != RIG_OK) {
        restorePreviousSplit();
        emitError(QStringLiteral("FT Split Operation/Rig could not set the split TX frequency: %1")
                      .arg(QString::fromLocal8Bit(rigerror(ret))));
        return false;
    }

    ret = rig_set_split_vfo(rig, RIG_VFO_CURR, RIG_SPLIT_ON, txVfo);
    if (ret != RIG_OK) {
        restorePreviousSplit();
        emitError(QStringLiteral("FT Split Operation/Rig could not enable native split after setting the TX frequency: %1")
                      .arg(QString::fromLocal8Bit(rigerror(ret))));
        return false;
    }

    m_ftSplitTxOperation = op;
    m_ftSplitRxFrequencyHz = static_cast<double>(rxFrequency);
    m_ftSplitTxFrequencyHz = static_cast<double>(targetFrequency);
    m_ftSplitRxVfo = static_cast<qint64>(RIG_VFO_CURR);
    m_ftSplitTxVfo = static_cast<qint64>(txVfo);
    m_ftSplitPreviousState = static_cast<int>(previousSplit);
    m_ftSplitPreviousTxVfo = static_cast<qint64>(previousTxVfo);
    m_ftSplitPreviousTxFrequencyHz = static_cast<double>(previousTxFrequency);
    m_ftSplitTxActive = true;
    setStatus(QStringLiteral("CAT connected / FT Rig split TX prepared"));
    return true;
#endif
}

bool HamlibController::endFtSplitTx()
{
    if (!m_ftSplitTxActive) return true;

    const QString op = m_ftSplitTxOperation;
    bool ok = true;

    if (isHamRadioDeluxeModel(m_config.rigModel)) {
        if (op == QStringLiteral("fake_it")) {
            QString error;
            if (m_ftSplitRxFrequencyHz > 0.0 &&
                !hrdSendSimpleCommand(QStringLiteral("set frequency %1").arg(QString::number(m_ftSplitRxFrequencyHz, 'f', 0)), &error)) {
                emitError(QStringLiteral("FT Fake It could not restore the HRD RX frequency after TX: %1").arg(error));
                ok = false;
            }
        }
        if (!ok) {
            // Keep the transaction snapshot intact so a later confirmed-RX
            // cleanup can retry restoration.  While active, CAT polling stays
            // suppressed and cannot publish the temporary TX dial as RX state.
            return false;
        }
        m_ftSplitTxActive = false;
        m_ftSplitTxOperation.clear();
        m_ftSplitTxFrequencyHz = 0.0;
        m_lastFrequencyHz = 0.0;
        if (m_config.catEnabled) pollHrd();
        m_ftSplitRxFrequencyHz = 0.0;
        setStatus(QStringLiteral("CAT connected"));
        return true;
    }

#ifndef MADMODEM_WITH_HAMLIB
    m_ftSplitTxActive = false;
    m_ftSplitTxOperation.clear();
    m_ftSplitRxFrequencyHz = 0.0;
    m_ftSplitTxFrequencyHz = 0.0;
    return false;
#else
    if (m_rig == nullptr) {
        emitError(QStringLiteral("FT split restore failed because the Hamlib connection is no longer open."));
        ok = false;
    } else {
        RIG *rig = static_cast<RIG *>(m_rig);

        if (op == QStringLiteral("fake_it")) {
            const int ret = rig_set_freq(rig, RIG_VFO_CURR, static_cast<freq_t>(m_ftSplitRxFrequencyHz));
            if (ret != RIG_OK) {
                emitError(QStringLiteral("FT Fake It could not restore the RX frequency after TX: %1")
                              .arg(QString::fromLocal8Bit(rigerror(ret))));
                ok = false;
            }
        } else if (op == QStringLiteral("rig")) {
            const vfo_t txVfo = static_cast<vfo_t>(m_ftSplitTxVfo);
            const split_t previousSplit = static_cast<split_t>(m_ftSplitPreviousState);
            vfo_t previousTxVfo = static_cast<vfo_t>(m_ftSplitPreviousTxVfo);
            if (previousTxVfo == RIG_VFO_NONE || previousTxVfo == RIG_VFO_CURR) {
                previousTxVfo = txVfo;
            }

            // Restore the operator's TX-side frequency first, while Hamlib
            // still has a real split TX selector.  This is done for both an
            // originally-active split and an originally-dormant secondary VFO.
            int ret = rig_set_split_vfo(rig, RIG_VFO_CURR, RIG_SPLIT_ON, previousTxVfo);
            if (ret != RIG_OK) {
                emitError(QStringLiteral("FT Split Operation/Rig could not reselect the previous split TX VFO: %1")
                              .arg(QString::fromLocal8Bit(rigerror(ret))));
                ok = false;
            }
            if (ok && m_ftSplitPreviousTxFrequencyHz > 0.0) {
                ret = rig_set_split_freq(rig,
                                         RIG_VFO_CURR,
                                         static_cast<freq_t>(m_ftSplitPreviousTxFrequencyHz));
                if (ret != RIG_OK) {
                    emitError(QStringLiteral("FT Split Operation/Rig could not restore the previous split TX frequency: %1")
                                  .arg(QString::fromLocal8Bit(rigerror(ret))));
                    ok = false;
                }
            }
            if (ok) {
                const int splitRet = rig_set_split_vfo(rig,
                                                       RIG_VFO_CURR,
                                                       previousSplit,
                                                       previousTxVfo);
                if (splitRet != RIG_OK) {
                    emitError(QStringLiteral("FT Split Operation/Rig could not restore the previous split state: %1")
                                  .arg(QString::fromLocal8Bit(rigerror(splitRet))));
                    ok = false;
                }
            }
        }
    }

    if (!ok) {
        // Preserve the snapshot and keep normal CAT polling blocked. This
        // makes restoration retryable and prevents a partially restored or
        // still-temporary TX frequency from becoming the advertised RX state.
        return false;
    }

    m_ftSplitTxActive = false;
    m_ftSplitTxOperation.clear();
    m_ftSplitRxFrequencyHz = 0.0;
    m_ftSplitTxFrequencyHz = 0.0;
    m_ftSplitRxVfo = 0;
    m_ftSplitTxVfo = 0;
    m_ftSplitPreviousState = 0;
    m_ftSplitPreviousTxVfo = 0;
    m_ftSplitPreviousTxFrequencyHz = 0.0;
    m_lastFrequencyHz = 0.0;
    if (m_config.catEnabled) pollNow();
    setStatus(QStringLiteral("CAT connected"));
    return true;
#endif
}

bool HamlibController::setPtt(bool enabled)
{
    if (isHamRadioDeluxeModel(m_config.rigModel)) {
        return setHrdPtt(enabled);
    }
#ifndef MADMODEM_WITH_HAMLIB
    emitError(QStringLiteral("Hamlib PTT requested, but Hamlib support is not compiled in."));
    return false;
#else
    if (m_config.readOnlyTest) {
        if (enabled) {
            emitError(QStringLiteral("PTT blocked: this controller instance is in read-only CAT-test mode."));
            return false;
        }
        return true;
    }
    if (!m_config.pttEnabled) {
        return true;
    }
    if (m_rig == nullptr && !connectRig()) {
        return false;
    }
    if (m_rig == nullptr) {
        return false;
    }

    if (enabled) {
        m_txModeChangedByMadModem = false;
        // "None / keep current mode" means exactly that: do not query or
        // rewrite the radio mode at every PTT edge.  The CAT handle remains
        // open and only the PTT state changes.
        if (m_config.txAudioRoute != QStringLiteral("default")) {
            capturePreTxMode();
        }
        if (!prepareDigitalTxAudioRoute()) {
            restorePreTxMode();
            return false;
        }
    }

    if (!enabled && m_vendorDataPttActive) {
        // Safety cleanup for sessions started by older builds.  New v2.62+ code
        // no longer uses raw vendor DATA SEND commands in the normal path.
        sendRawCatCommand(QByteArrayLiteral("RX;"));
        m_vendorDataPttActive = false;
    }

    if (m_config.pttMethod == QStringLiteral("cat_hamlib")) {
        if (setWsjtLikeCatPtt(enabled)) {
            m_vendorDataPttActive = false;
            return true;
        }
        return false;
    }

    // RTS/DTR PTT is selected through Hamlib ptt_type before rig_open(); the
    // actual keying command remains ordinary ON/OFF.
    if (setHamlibPttMode(enabled, RIG_PTT_ON, QStringLiteral("serial PTT"))) {
        m_vendorDataPttActive = false;
        return true;
    }
    return false;
#endif
}

bool HamlibController::setWsjtLikeCatPtt(bool enabled)
{
#ifndef MADMODEM_WITH_HAMLIB
    Q_UNUSED(enabled);
    return false;
#else
    /*
     * WSJT-X-like CAT/PTT policy for MM:
     * - the UI exposes generic concepts only: CAT PTT, radio TX mode, and
     *   transmit audio source Front/Mic vs Rear/Data;
     * - vendor-specific TX1/MS/USB-D commands are not part of the normal path;
     * - when the user asks for Rear/Data audio, prefer Hamlib's DATA PTT if
     *   the backend/header supports it; failure is terminal and never changes
     *   the requested audio route;
     * - when Front/Mic is requested, prefer MIC PTT when available.
     *
     * This keeps the UI close to WSJT-X while still preserving the important
     * Hamlib MIC/DATA distinction for rigs that expose it.
     */
    if (!enabled) {
        return setHamlibPttMode(false, RIG_PTT_ON, QStringLiteral("CAT PTT"));
    }

    const bool rearData = (m_config.transmitAudioSource != QStringLiteral("front_mic"));

    if (rearData) {
        return setHamlibPttMode(true,
                                RIG_PTT_ON_DATA,
                                QStringLiteral("CAT DATA PTT"),
                                true);
    }
    return setHamlibPttMode(true, RIG_PTT_ON, QStringLiteral("CAT PTT"), true);
#endif
}

bool HamlibController::setHamlibPttMode(bool enabled, int onMode, const QString &label, bool reportErrors)
{
#ifndef MADMODEM_WITH_HAMLIB
    Q_UNUSED(enabled);
    Q_UNUSED(onMode);
    Q_UNUSED(label);
    return false;
#else
    if (m_rig == nullptr && !connectRig()) {
        return false;
    }
    if (m_rig == nullptr) {
        return false;
    }

    RIG *rig = static_cast<RIG *>(m_rig);
    const ptt_t requested = enabled ? static_cast<ptt_t>(onMode) : RIG_PTT_OFF;
    const int ret = rig_set_ptt(rig, RIG_VFO_CURR, requested);
    if (ret != RIG_OK) {
        if (reportErrors) {
            emitError(QStringLiteral("Hamlib %1 %2 failed: %3")
                          .arg(label,
                               enabled ? QStringLiteral("ON") : QStringLiteral("OFF"),
                               QString::fromLocal8Bit(rigerror(ret))));
        }
        return false;
    }

    if (m_lastPtt != enabled) {
        m_lastPtt = enabled;
        emit pttChanged(enabled);
    }
    if (enabled) {
        setStatus(QStringLiteral("CAT connected / %1 ON").arg(label));
    } else {
        restorePreTxMode();
        setStatus(QStringLiteral("CAT connected"));
    }
    return true;
#endif
}

#ifdef MADMODEM_WITH_HAMLIB
void HamlibController::capturePreTxMode()
{
    if (m_havePreTxMode) {
        return;
    }
    if (m_rig == nullptr && !connectRig()) {
        return;
    }
    if (m_rig == nullptr) {
        return;
    }

    RIG *rig = static_cast<RIG *>(m_rig);
    const bool splitTx = m_ftSplitTxActive &&
                         m_ftSplitTxOperation == QStringLiteral("rig") &&
                         m_ftSplitTxVfo != 0;
    rmode_t mode = RIG_MODE_NONE;
    pbwidth_t width = 0;
    int ret = RIG_OK;
    if (splitTx) {
        // Query the active split TX mode through Hamlib's public split API.
        // beginFtSplitTx() has already established the TX selector with
        // rig_set_split_vfo(); MadModem never edits Hamlib private state.
        ret = rig_get_split_mode(rig, RIG_VFO_CURR, &mode, &width);
    } else {
        ret = rig_get_mode(rig, RIG_VFO_CURR, &mode, &width);
    }
    if (ret == RIG_OK && mode != RIG_MODE_NONE) {
        m_preTxMode = static_cast<qint64>(mode);
        m_preTxPassband = static_cast<qint64>(width);
        m_preTxModeVfo = static_cast<qint64>(RIG_VFO_CURR);
        m_preTxModeWasSplit = splitTx;
        m_havePreTxMode = true;
    }
}

void HamlibController::restorePreTxMode()
{
    if (!m_havePreTxMode) {
        return;
    }
    const rmode_t mode = static_cast<rmode_t>(m_preTxMode);
    pbwidth_t width = static_cast<pbwidth_t>(m_preTxPassband);
    const bool splitMode = m_preTxModeWasSplit;
    m_havePreTxMode = false;
    m_preTxMode = 0;
    m_preTxPassband = 0;
    m_preTxModeVfo = 0;
    m_preTxModeWasSplit = false;

    if (!m_txModeChangedByMadModem) {
        return;
    }
    m_txModeChangedByMadModem = false;

    if (mode == RIG_MODE_NONE) {
        return;
    }
    if (width <= 0) {
        width = kDigitalWidePassbandHz;
    }
    if (m_rig == nullptr && !connectRig()) {
        return;
    }
    if (m_rig == nullptr) {
        return;
    }

    RIG *rig = static_cast<RIG *>(m_rig);
    int ret = RIG_OK;
    if (splitMode && m_ftSplitTxActive && m_ftSplitTxVfo != 0) {
        ret = rig_set_split_mode(rig, RIG_VFO_CURR, mode, width);
    } else {
        ret = rig_set_mode(rig, RIG_VFO_CURR, mode, width);
    }
    if (ret != RIG_OK) {
        emitError(QStringLiteral("Hamlib could not restore transmit mode/passband after PTT: %1")
                      .arg(QString::fromLocal8Bit(rigerror(ret))));
    }
}
#endif

bool HamlibController::prepareDigitalTxAudioRoute()
{
#ifndef MADMODEM_WITH_HAMLIB
    return true;
#else
    if (m_config.txAudioRoute == "default") {
        return true;
    }

    if (routeRequestsUsbMode(m_config)) {
        return forceUsbMode(false);
    }

    if (routeRequestsDataPktMode(m_config)) {
        return forceDataUsbMode(false);
    }

    return true;
#endif
}

bool HamlibController::forceUsbMode(bool required)
{
#ifndef MADMODEM_WITH_HAMLIB
    Q_UNUSED(required);
    return true;
#else
    Q_UNUSED(required);
    if (m_rig == nullptr && !connectRig()) {
        return false;
    }
    if (m_rig == nullptr) {
        return false;
    }

    RIG *rig = static_cast<RIG *>(m_rig);
    const bool splitTx = m_ftSplitTxActive &&
                         m_ftSplitTxOperation == QStringLiteral("rig") &&
                         m_ftSplitTxVfo != 0;
    // When MadModem is about to change the real split TX mode, restoration is
    // part of the same transaction.  Do not modify a remote TX-side mode if
    // Hamlib could not first snapshot it.  This check is intentionally limited
    // to opt-in Rig Split; the long-standing None/Fake-It PTT path is unchanged.
    if (splitTx && !m_havePreTxMode) {
        emitError(QStringLiteral("FT Split Operation/Rig could not snapshot the split TX mode/passband before changing the transmit audio route; TX aborted."));
        return false;
    }

    int ret = RIG_OK;
    if (splitTx) {
        ret = rig_set_split_mode(rig, RIG_VFO_CURR, RIG_MODE_USB, kDigitalWidePassbandHz);
    } else {
        ret = rig_set_mode(rig, RIG_VFO_CURR, RIG_MODE_USB, kDigitalWidePassbandHz);
    }
    if (ret == RIG_OK) {
        m_txModeChangedByMadModem = true;
        return true;
    }

    const QString msg = QStringLiteral("Hamlib could not set USB transmit mode before PTT: %1")
                            .arg(QString::fromLocal8Bit(rigerror(ret)));
    emitError(msg + QStringLiteral("; TX aborted because automatic mode fallback is disabled."));
    return false;
#endif
}

bool HamlibController::forceDataUsbMode(bool required)
{
#ifndef MADMODEM_WITH_HAMLIB
    Q_UNUSED(required);
    return true;
#else
    Q_UNUSED(required);
    if (m_rig == nullptr && !connectRig()) {
        return false;
    }
    if (m_rig == nullptr) {
        return false;
    }

    RIG *rig = static_cast<RIG *>(m_rig);
    const bool splitTx = m_ftSplitTxActive &&
                         m_ftSplitTxOperation == QStringLiteral("rig") &&
                         m_ftSplitTxVfo != 0;
    // Same transaction rule as USB above: a real split TX mode must be
    // restorable before MadModem is allowed to change it.
    if (splitTx && !m_havePreTxMode) {
        emitError(QStringLiteral("FT Split Operation/Rig could not snapshot the split TX mode/passband before changing the transmit audio route; TX aborted."));
        return false;
    }

    const rmode_t modes[] = {
        RIG_MODE_PKTUSB,  // Hamlib's generic digital/packet USB mode.
#ifdef RIG_MODE_USBD1
        RIG_MODE_USBD1,   // Some rigs expose explicit USB-D submodes.
#endif
#ifdef RIG_MODE_USBD2
        RIG_MODE_USBD2,
#endif
#ifdef RIG_MODE_USBD3
        RIG_MODE_USBD3,
#endif
    };

    QString lastError;
    for (rmode_t mode : modes) {
        int ret = RIG_OK;
        if (splitTx) {
            ret = rig_set_split_mode(rig, RIG_VFO_CURR, mode, kDigitalWidePassbandHz);
        } else {
            ret = rig_set_mode(rig, RIG_VFO_CURR, mode, kDigitalWidePassbandHz);
        }
        if (ret == RIG_OK) {
            m_txModeChangedByMadModem = true;
            return true;
        }
        lastError = QString::fromLocal8Bit(rigerror(ret));
    }

    const QString msg = QStringLiteral("Hamlib could not set Data/Pkt transmit mode before PTT: %1")
                            .arg(lastError.isEmpty() ? QStringLiteral("unsupported by backend/model") : lastError);
    emitError(msg + QStringLiteral("; TX aborted because automatic mode fallback is disabled."));
    return false;
#endif
}

bool HamlibController::sendRawCatCommand(const QByteArray &command)
{
#ifndef MADMODEM_WITH_HAMLIB
    Q_UNUSED(command);
    return false;
#else
    if (m_rig == nullptr && !connectRig()) {
        return false;
    }
    if (m_rig == nullptr || command.isEmpty()) {
        return false;
    }

    RIG *rig = static_cast<RIG *>(m_rig);
    const int ret = rig_send_raw(rig,
                                 reinterpret_cast<const unsigned char *>(command.constData()),
                                 command.size(),
                                 nullptr,
                                 0,
                                 nullptr);
    if (ret < RIG_OK) {
        emitError(QStringLiteral("Hamlib raw CAT command '%1' failed: %2")
                      .arg(QString::fromLatin1(command), QString::fromLocal8Bit(rigerror(ret))));
        return false;
    }
    return true;
#endif
}

QString HamlibController::hrdEndpoint() const
{
    QString endpoint = m_config.rigPath.trimmed();
    if (endpoint.isEmpty()) {
        return QStringLiteral("localhost:%1").arg(kHrdDefaultPort);
    }
    if (endpoint.startsWith(QStringLiteral("hrd:"), Qt::CaseInsensitive)) {
        endpoint = endpoint.mid(4).trimmed();
    }
    if (endpoint.startsWith(QStringLiteral("tcp:"), Qt::CaseInsensitive)) {
        endpoint = endpoint.mid(4).trimmed();
    }
    if (endpoint.isEmpty()) {
        return QStringLiteral("localhost:%1").arg(kHrdDefaultPort);
    }
    return endpoint;
}

bool HamlibController::connectHrd()
{
    if (m_hrdSocket != nullptr && m_hrdSocket->state() == QTcpSocket::ConnectedState) {
        if (m_config.catEnabled && !m_pollTimer->isActive()) {
            m_pollTimer->start();
        }
        return true;
    }

    if (m_hrdSocket == nullptr) {
        m_hrdSocket = new QTcpSocket(this);
    }

    QString endpoint = hrdEndpoint();
    QString host = endpoint;
    quint16 port = kHrdDefaultPort;
    const int colon = endpoint.lastIndexOf(QLatin1Char(':'));
    if (colon > 0 && colon < endpoint.size() - 1) {
        bool ok = false;
        const int parsedPort = endpoint.mid(colon + 1).toInt(&ok);
        if (ok && parsedPort > 0 && parsedPort <= 65535) {
            host = endpoint.left(colon).trimmed();
            port = static_cast<quint16>(parsedPort);
        }
    }
    if (host.isEmpty() || host == QStringLiteral("*")) {
        host = QStringLiteral("localhost");
    }

    m_hrdSocket->abort();
    m_hrdProtocol = 5;
    m_hrdCurrentRadio = 0;
    m_hrdButtons.clear();
    m_hrdPttButton = -1;
    m_hrdAltPttButton = -1;

    m_hrdSocket->connectToHost(host, port);
    if (!m_hrdSocket->waitForConnected(3000)) {
        const QString err = m_hrdSocket->errorString();
        emitError(QStringLiteral("HRD CAT connection failed to %1:%2: %3").arg(host).arg(port).arg(err));
        setStatus(QStringLiteral("HRD CAT open failed"));
        return false;
    }

    QString reply;
    QString error;
    if (!hrdSendCommand(QStringLiteral("get context"), &reply, false, &error)) {
        m_hrdSocket->close();
        if (!m_hrdSocket->waitForDisconnected(500) && m_hrdSocket->state() != QTcpSocket::UnconnectedState) {
            m_hrdSocket->abort();
        }
        m_hrdProtocol = 4;
        m_hrdSocket->connectToHost(host, port);
        if (!m_hrdSocket->waitForConnected(3000) ||
            !hrdSendCommand(QStringLiteral("get context"), &reply, false, &error)) {
            emitError(QStringLiteral("HRD CAT protocol handshake failed at %1:%2: %3")
                          .arg(host).arg(port).arg(error.isEmpty() ? m_hrdSocket->errorString() : error));
            setStatus(QStringLiteral("HRD CAT handshake failed"));
            return false;
        }
    }

    QString radiosText;
    if (hrdSendCommand(QStringLiteral("get radios"), &radiosText, false, &error)) {
        const QStringList radios = hrdSplitList(radiosText);
        QString currentName;
        hrdSendCommand(QStringLiteral("get radio"), &currentName, false, nullptr);
        for (const QString &entry : radios) {
            const int sep = entry.indexOf(QLatin1Char(':'));
            if (sep <= 0) {
                continue;
            }
            bool idOk = false;
            const unsigned id = entry.left(sep).trimmed().toUInt(&idOk);
            const QString name = entry.mid(sep + 1).trimmed();
            if (!idOk) {
                continue;
            }
            if (m_hrdCurrentRadio == 0 || (!currentName.trimmed().isEmpty() && name == currentName.trimmed())) {
                m_hrdCurrentRadio = id;
            }
        }
    }
    if (m_hrdCurrentRadio == 0) {
        m_hrdCurrentRadio = 1;
    }

    QString buttonsText;
    if (hrdSendCommand(QStringLiteral("get buttons"), &buttonsText, true, nullptr)) {
        m_hrdButtons = hrdSplitList(buttonsText);
        m_hrdButtons.replaceInStrings(QStringLiteral(" "), QStringLiteral("~"));
        m_hrdPttButton = hrdFindButton(QStringList() << QStringLiteral("TX"));
        m_hrdAltPttButton = hrdFindButton(QStringList() << QStringLiteral("TX~Data") << QStringLiteral("TX~Alt"));
    }

    setStatus(QStringLiteral("HRD CAT connected"));

    if (m_config.catEnabled) {
        m_pollTimer->start();
        pollHrd();
    }
    return true;
}

void HamlibController::disconnectHrd()
{
    if (m_hrdSocket != nullptr) {
        if (m_hrdSocket->state() != QTcpSocket::UnconnectedState) {
            m_hrdSocket->disconnectFromHost();
            if (!m_hrdSocket->waitForDisconnected(300)) {
                m_hrdSocket->abort();
            }
        }
    }
    m_hrdProtocol = 0;
    m_hrdCurrentRadio = 0;
    m_hrdButtons.clear();
    m_hrdPttButton = -1;
    m_hrdAltPttButton = -1;
}

bool HamlibController::pollHrd()
{
    if (m_hrdSocket == nullptr || m_hrdSocket->state() != QTcpSocket::ConnectedState) {
        if (!connectHrd()) {
            return false;
        }
    }

    QString reply;
    QString error;
    if (!hrdSendCommand(QStringLiteral("get frequency"), &reply, true, &error)) {
        emitError(QStringLiteral("HRD get frequency failed: %1").arg(error));
        setStatus(QStringLiteral("HRD CAT frequency error"));
        return false;
    }

    bool ok = false;
    const double hz = reply.trimmed().toDouble(&ok);
    if (ok && hz > 0.0) {
        if (qAbs(hz - m_lastFrequencyHz) >= 1.0) {
            m_lastFrequencyHz = hz;
            emit frequencyChanged(hz);
        }
        setStatus(QStringLiteral("HRD CAT connected"));
        return true;
    }

    emitError(QStringLiteral("HRD get frequency returned an invalid value: '%1'").arg(reply.trimmed()));
    setStatus(QStringLiteral("HRD CAT frequency error"));
    return false;
}

bool HamlibController::setHrdPtt(bool enabled)
{
    if (m_config.readOnlyTest) {
        if (enabled) {
            emitError(QStringLiteral("PTT blocked: this controller instance is in read-only CAT-test mode."));
            return false;
        }
        return true;
    }
    if (!m_config.pttEnabled) {
        return true;
    }
    if (m_hrdSocket == nullptr || m_hrdSocket->state() != QTcpSocket::ConnectedState) {
        if (!connectHrd()) {
            return false;
        }
    }

    int button = m_hrdPttButton;
    if (button < 0 || button >= m_hrdButtons.size()) {
        emitError(QStringLiteral("HRD CAT PTT button is not available for this HRD radio pane."));
        return false;
    }

    QString error;
    const QString command = QStringLiteral("set button-select %1 %2")
                                .arg(m_hrdButtons.value(button), enabled ? QStringLiteral("1") : QStringLiteral("0"));
    if (!hrdSendSimpleCommand(command, &error)) {
        emitError(QStringLiteral("HRD CAT PTT %1 failed: %2")
                      .arg(enabled ? QStringLiteral("ON") : QStringLiteral("OFF"), error));
        return false;
    }

    if (m_lastPtt != enabled) {
        m_lastPtt = enabled;
        emit pttChanged(enabled);
    }
    setStatus(enabled ? QStringLiteral("HRD CAT connected / PTT ON") : QStringLiteral("HRD CAT connected"));
    return true;
}

bool HamlibController::hrdSendSimpleCommand(const QString &command, QString *errorMessage)
{
    QString reply;
    if (!hrdSendCommand(command, &reply, true, errorMessage)) {
        return false;
    }
    if (reply.trimmed() != QStringLiteral("OK")) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("unexpected reply '%1'").arg(reply.trimmed());
        }
        return false;
    }
    return true;
}

bool HamlibController::hrdSendCommand(const QString &command,
                                      QString *reply,
                                      bool prependContext,
                                      QString *errorMessage)
{
    if (reply != nullptr) {
        reply->clear();
    }
    if (m_hrdSocket == nullptr || m_hrdSocket->state() != QTcpSocket::ConnectedState) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("socket is not connected");
        }
        return false;
    }

    const QString payload = (prependContext && m_hrdCurrentRadio != 0)
                                ? (QStringLiteral("[") + QString::number(m_hrdCurrentRadio) + QStringLiteral("] ") + command)
                                : command;
    QByteArray frame;
    if (m_hrdProtocol == 4) {
        frame = (payload + QStringLiteral("\r")).toLocal8Bit();
    } else {
        const QByteArray utf16(reinterpret_cast<const char *>(payload.utf16()),
                               static_cast<int>((payload.size() + 1) * sizeof(ushort)));
        const quint32 totalSize = static_cast<quint32>(16 + utf16.size());
        appendLe32(&frame, totalSize);
        appendLe32(&frame, kHrdMagic1);
        appendLe32(&frame, kHrdMagic2);
        appendLe32(&frame, 0u);
        frame.append(utf16);
    }

    if (!hrdWriteAll(frame)) {
        if (errorMessage != nullptr) {
            *errorMessage = m_hrdSocket->errorString();
        }
        return false;
    }

    return hrdReadResponse(command, reply, errorMessage);
}

bool HamlibController::hrdWriteAll(const QByteArray &data)
{
    qint64 written = 0;
    while (written < data.size()) {
        const qint64 n = m_hrdSocket->write(data.constData() + written, data.size() - written);
        if (n <= 0 || !m_hrdSocket->waitForBytesWritten(1000)) {
            return false;
        }
        written += n;
    }
    return true;
}

bool HamlibController::hrdReadResponse(const QString &command, QString *reply, QString *errorMessage)
{
    Q_UNUSED(command);
    QByteArray buffer;
    const int maxTries = 24;
    for (int i = 0; i < maxTries; ++i) {
        if (m_hrdSocket->bytesAvailable() <= 0 && !m_hrdSocket->waitForReadyRead(250)) {
            if (m_hrdSocket->error() != QAbstractSocket::SocketTimeoutError) {
                if (errorMessage != nullptr) {
                    *errorMessage = m_hrdSocket->errorString();
                }
                return false;
            }
            continue;
        }
        buffer += m_hrdSocket->readAll();
        if (buffer.size() > 1024 * 1024) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("HRD reply exceeds the 1 MiB safety limit");
            }
            return false;
        }
        if (m_hrdProtocol == 4) {
            // HRD v4 is a CR/LF-delimited text protocol.  A non-empty TCP
            // fragment is not a complete reply: TCP may split even "OK".
            if (buffer.contains('\r') || buffer.contains('\n')) {
                if (reply != nullptr) {
                    *reply = QString::fromLocal8Bit(buffer).trimmed();
                }
                return true;
            }
        } else if (buffer.size() >= 16) {
            const quint32 totalSize = readLe32(buffer.constData());
            const quint32 magic1 = readLe32(buffer.constData() + 4);
            const quint32 magic2 = readLe32(buffer.constData() + 8);
            if (totalSize < 16 || totalSize > 1024 * 1024 || magic1 != kHrdMagic1 || magic2 != kHrdMagic2) {
                if (errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("invalid HRD binary reply");
                }
                return false;
            }
            if (static_cast<quint32>(buffer.size()) >= totalSize) {
                const QByteArray payloadBytes = buffer.mid(16, static_cast<int>(totalSize) - 16);
                QString text = QString::fromUtf16(reinterpret_cast<const ushort *>(payloadBytes.constData()),
                                                  payloadBytes.size() / static_cast<int>(sizeof(ushort)));
                const int nul = text.indexOf(QChar(0));
                if (nul >= 0) {
                    text.truncate(nul);
                }
                if (reply != nullptr) {
                    *reply = text.trimmed();
                }
                return true;
            }
        }
    }

    if (errorMessage != nullptr) {
        *errorMessage = QStringLiteral("timeout waiting for HRD reply");
    }
    return false;
}

int HamlibController::hrdFindButton(const QStringList &patterns) const
{
    for (int i = 0; i < m_hrdButtons.size(); ++i) {
        if (textMatchesAny(m_hrdButtons.value(i), patterns)) {
            return i;
        }
    }
    return -1;
}

void HamlibController::setStatus(const QString &status)
{
    if (m_lastStatus == status) {
        return;
    }
    m_lastStatus = status;
    emit statusChanged(status);
}

void HamlibController::emitError(const QString &message)
{
    emit errorOccurred(message);
}
