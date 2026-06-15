#include "RigControlSettingsDialog.h"
#include "../utils/UiScale.h"

#include "../rig/HamlibController.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QList>
#include <QPushButton>
#include <QSerialPortInfo>
#include <QScrollArea>
#include <QSet>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStringList>
#include <QtGlobal>
#include <QVariant>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

#ifdef MADMODEM_WITH_HAMLIB
#include <hamlib/rig.h>
#endif

namespace {

struct RadioPreset
{
    QString manufacturer;
    QString model;
    int hamlibId = 1;
    QString defaultTxAudioRoute = QStringLiteral("default");
};

struct BaudSuggestion
{
    int baudRate = 0;
    QString hint;
};

QString customManufacturerText()
{
    return QStringLiteral("Custom / manual Hamlib ID");
}

QString normalizedManufacturer(QString manufacturer)
{
    manufacturer = manufacturer.trimmed();
    if (manufacturer.isEmpty()) {
        return QStringLiteral("Other / unknown");
    }

    // Keep the dialog compact and user-friendly even when different Hamlib
    // backends use slightly different spelling for the same vendor.
    const QString lower = manufacturer.toLower();
    if (lower.contains(QStringLiteral("kenwood"))) return QStringLiteral("Kenwood");
    if (lower.contains(QStringLiteral("icom"))) return QStringLiteral("Icom");
    if (lower.contains(QStringLiteral("yaesu"))) return QStringLiteral("Yaesu");
    if (lower.contains(QStringLiteral("elecraft"))) return QStringLiteral("Elecraft");
    if (lower.contains(QStringLiteral("xiegu"))) return QStringLiteral("Xiegu");
    if (lower.contains(QStringLiteral("lab599")) || lower.contains(QStringLiteral("lab 599"))) return QStringLiteral("Lab599");
    if (lower.contains(QStringLiteral("flex"))) return QStringLiteral("FlexRadio");
    if (lower.contains(QStringLiteral("ten-tec")) || lower.contains(QStringLiteral("tentec"))) return QStringLiteral("Ten-Tec");
    if (lower.contains(QStringLiteral("alinco"))) return QStringLiteral("Alinco");
    if (lower.contains(QStringLiteral("jrc"))) return QStringLiteral("JRC");
    if (lower.contains(QStringLiteral("tangerine"))) return QStringLiteral("TangerineSDR");
    if (lower.contains(QStringLiteral("elad"))) return QStringLiteral("ELAD");
    if (lower.contains(QStringLiteral("winradio"))) return QStringLiteral("Winradio");
    if (lower.contains(QStringLiteral("kit")) && lower.contains(QStringLiteral("elecraft"))) return QStringLiteral("Elecraft");
    if (lower.contains(QStringLiteral("hamlib")) || lower.contains(QStringLiteral("dummy")) || lower.contains(QStringLiteral("network"))) return QStringLiteral("Network / virtual");
    return manufacturer;
}

QString normalizedModel(QString model)
{
    model = model.simplified();
    if (model.isEmpty()) {
        return QStringLiteral("Unknown model");
    }
    return model;
}

bool looksVirtualRig(const QString &manufacturer, const QString &model)
{
    const QString text = (manufacturer + QLatin1Char(' ') + model).toLower();
    return text.contains(QStringLiteral("dummy")) ||
           text.contains(QStringLiteral("net rigctl")) ||
           text.contains(QStringLiteral("network")) ||
           text.contains(QStringLiteral("flrig")) ||
           text.contains(QStringLiteral("rigctld")) ||
           text.contains(QStringLiteral("trxmanager"));
}

bool looksReceiveOnly(const QString &manufacturer, const QString &model)
{
    const QString text = (manufacturer + QLatin1Char(' ') + model).toLower();
    return text.contains(QStringLiteral("pcr")) ||
           text.contains(QStringLiteral("perseus")) ||
           text.contains(QStringLiteral("winradio")) ||
           text.contains(QStringLiteral("rtl")) ||
           text.contains(QStringLiteral("sdrplay")) ||
           text.contains(QStringLiteral("receiver"));
}

QString defaultTxRouteFor(const QString &manufacturer, const QString &model, int hamlibId)
{
    Q_UNUSED(hamlibId);

    if (looksVirtualRig(manufacturer, model) || looksReceiveOnly(manufacturer, model)) {
        return QStringLiteral("default");
    }

    // Be conservative by default, like WSJT-X: selecting a rig model never
    // auto-selects a radio mode or vendor audio route. The user chooses the
    // TX mode policy explicitly: None, USB, or Data/Pkt.
    Q_UNUSED(manufacturer);
    Q_UNUSED(model);
    return QStringLiteral("default");
}

BaudSuggestion baudSuggestionFor(const QString &manufacturer, const QString &model, int hamlibId)
{
    Q_UNUSED(hamlibId);

    BaudSuggestion suggestion;
    const QString maker = manufacturer.toUpper();
    const QString text = (manufacturer + QLatin1Char(' ') + model).toUpper();

    if (looksVirtualRig(manufacturer, model)) {
        suggestion.hint = QStringLiteral("TCP/virtual rig: serial speed is ignored.");
        return suggestion;
    }
    if (looksReceiveOnly(manufacturer, model)) {
        suggestion.hint = QStringLiteral("Receiver/SDR preset: verify the CAT speed in the device software.");
        return suggestion;
    }

    if (maker.contains(QStringLiteral("ICOM")) || text.contains(QStringLiteral("IC-"))) {
        suggestion.baudRate = 19200;
        suggestion.hint = QStringLiteral("Suggested Icom CI-V speed: 19200 baud. Many recent Icom rigs can also use CI-V Baud=Auto; match the radio menu.");
        if (text.contains(QStringLiteral("IC-706")) || text.contains(QStringLiteral("IC706")) ||
            text.contains(QStringLiteral("IC-7000")) || text.contains(QStringLiteral("IC7000")) ||
            text.contains(QStringLiteral("IC-718")) || text.contains(QStringLiteral("IC718")) ||
            text.contains(QStringLiteral("IC-746")) || text.contains(QStringLiteral("IC746"))) {
            suggestion.baudRate = 9600;
            suggestion.hint = QStringLiteral("Suggested older Icom CI-V speed: 9600 baud. Verify CI-V Baud in the radio menu.");
        }
        return suggestion;
    }

    if (maker.contains(QStringLiteral("YAESU")) || text.contains(QStringLiteral("FT-")) || text.contains(QStringLiteral("FTDX"))) {
        suggestion.baudRate = 38400;
        suggestion.hint = QStringLiteral("Suggested Yaesu CAT speed: 38400 baud for many recent models. Match the CAT Rate/Computer Baud setting in the radio menu.");
        if (text.contains(QStringLiteral("FT-817")) || text.contains(QStringLiteral("FT817")) ||
            text.contains(QStringLiteral("FT-857")) || text.contains(QStringLiteral("FT857")) ||
            text.contains(QStringLiteral("FT-897")) || text.contains(QStringLiteral("FT897")) ||
            text.contains(QStringLiteral("FT-450")) || text.contains(QStringLiteral("FT450")) ||
            text.contains(QStringLiteral("FT-950")) || text.contains(QStringLiteral("FT950")) ||
            text.contains(QStringLiteral("FT-2000")) || text.contains(QStringLiteral("FT2000"))) {
            suggestion.baudRate = 4800;
            suggestion.hint = QStringLiteral("Suggested legacy Yaesu CAT speed: 4800 baud. Verify CAT Rate in the radio menu.");
        }
        return suggestion;
    }

    if (maker.contains(QStringLiteral("KENWOOD")) || text.contains(QStringLiteral("TS-"))) {
        suggestion.baudRate = 9600;
        suggestion.hint = QStringLiteral("Suggested Kenwood CAT speed: 9600 baud for many serial rigs. Match the COM/IF-232C/USB baud setting in the radio menu.");
        if (text.contains(QStringLiteral("TS-790")) || text.contains(QStringLiteral("TS790")) ||
            text.contains(QStringLiteral("TS-850")) || text.contains(QStringLiteral("TS850")) ||
            text.contains(QStringLiteral("TS-440")) || text.contains(QStringLiteral("TS440")) ||
            text.contains(QStringLiteral("TS-450")) || text.contains(QStringLiteral("TS450")) ||
            text.contains(QStringLiteral("TS-690")) || text.contains(QStringLiteral("TS690")) ||
            text.contains(QStringLiteral("TS-711")) || text.contains(QStringLiteral("TS711")) ||
            text.contains(QStringLiteral("TS-811")) || text.contains(QStringLiteral("TS811")) ||
            text.contains(QStringLiteral("TS-940")) || text.contains(QStringLiteral("TS940")) ||
            text.contains(QStringLiteral("TS-950")) || text.contains(QStringLiteral("TS950"))) {
            suggestion.baudRate = 4800;
            suggestion.hint = QStringLiteral("Suggested vintage Kenwood IF-232C CAT speed: 4800 baud. Hamlib backend defaults handle 8N2/hardware handshake; verify the radio/interface menu.");
        }
        if (text.contains(QStringLiteral("TS-590")) || text.contains(QStringLiteral("TS590")) ||
            text.contains(QStringLiteral("TS-890")) || text.contains(QStringLiteral("TS890")) ||
            text.contains(QStringLiteral("TS-990")) || text.contains(QStringLiteral("TS990")) ||
            text.contains(QStringLiteral("TS-990S")) || text.contains(QStringLiteral("TS990S"))) {
            suggestion.baudRate = 115200;
            suggestion.hint = QStringLiteral("Suggested modern Kenwood USB CAT speed: 115200 baud. Verify the USB/COM baud setting in the radio menu.");
        }
        return suggestion;
    }

    if (maker.contains(QStringLiteral("ELECRAFT")) || text.contains(QStringLiteral("KX3")) ||
        text.contains(QStringLiteral("KX2")) || text.contains(QStringLiteral("K3")) ||
        text.contains(QStringLiteral("K4")) || text.contains(QStringLiteral("K2"))) {
        suggestion.baudRate = 38400;
        suggestion.hint = QStringLiteral("Suggested Elecraft CAT speed: 38400 baud. Verify RS232/USB baud in the radio menu.");
        return suggestion;
    }

    if (maker.contains(QStringLiteral("XIEGU")) || text.contains(QStringLiteral("G90")) ||
        text.contains(QStringLiteral("X5105")) || text.contains(QStringLiteral("X6100")) ||
        text.contains(QStringLiteral("X6200"))) {
        suggestion.baudRate = 19200;
        suggestion.hint = QStringLiteral("Suggested Xiegu CAT speed: 19200 baud. Verify the CAT/CI-V baud setting in the radio menu.");
        return suggestion;
    }

    suggestion.hint = QStringLiteral("No model-specific baud suggestion. Use the same speed configured in the radio CAT menu.");
    return suggestion;
}

void addPresetIfUnique(QList<RadioPreset> *presets,
                       QSet<int> *seenIds,
                       const QString &manufacturer,
                       const QString &model,
                       int hamlibId)
{
    if (presets == nullptr || seenIds == nullptr || hamlibId <= 0 || seenIds->contains(hamlibId)) {
        return;
    }

    RadioPreset preset;
    preset.manufacturer = normalizedManufacturer(manufacturer);
    preset.model = normalizedModel(model);
    preset.hamlibId = hamlibId;
    preset.defaultTxAudioRoute = defaultTxRouteFor(preset.manufacturer, preset.model, hamlibId);
    presets->append(preset);
    seenIds->insert(hamlibId);
}

#ifdef MADMODEM_WITH_HAMLIB
struct HamlibPresetBuildContext
{
    QList<RadioPreset> *presets = nullptr;
    QSet<int> *seenIds = nullptr;
};

int addHamlibCapsCallback(const struct rig_caps *caps, rig_ptr_t data)
{
    HamlibPresetBuildContext *ctx = static_cast<HamlibPresetBuildContext *>(data);
    if (caps == nullptr || ctx == nullptr || ctx->presets == nullptr || ctx->seenIds == nullptr) {
        return 1;
    }

    addPresetIfUnique(ctx->presets,
                      ctx->seenIds,
                      QString::fromLocal8Bit(caps->mfg_name != nullptr ? caps->mfg_name : "Other / unknown"),
                      QString::fromLocal8Bit(caps->model_name != nullptr ? caps->model_name : "Unknown model"),
                      static_cast<int>(caps->rig_model));
    return 1;
}
#endif

QList<RadioPreset> fallbackRadioPresets()
{
    QList<RadioPreset> presets;
    QSet<int> seenIds;

    // Minimal fallback used only when the app is built without Hamlib headers
    // or when a very unusual Hamlib build reports no caps.  Normal release
    // builds populate hundreds of models from rig_load_all_backends().
    addPresetIfUnique(&presets, &seenIds, QStringLiteral("Network / virtual"), QStringLiteral("Dummy test rig"), 1);
    addPresetIfUnique(&presets, &seenIds, QStringLiteral("Network / virtual"), QStringLiteral("Net rigctl"), 2);
    addPresetIfUnique(&presets, &seenIds, QStringLiteral("Network / virtual"), QStringLiteral("Flrig"), 4);

    addPresetIfUnique(&presets, &seenIds, QStringLiteral("Kenwood"), QStringLiteral("TS-790"), 2007);
    addPresetIfUnique(&presets, &seenIds, QStringLiteral("Kenwood"), QStringLiteral("TS-850"), 2009);
    addPresetIfUnique(&presets, &seenIds, QStringLiteral("Kenwood"), QStringLiteral("TS-480"), 2028);
    addPresetIfUnique(&presets, &seenIds, QStringLiteral("Kenwood"), QStringLiteral("TS-590S"), 2031);
    addPresetIfUnique(&presets, &seenIds, QStringLiteral("Kenwood"), QStringLiteral("TS-590SG"), 2037);
    addPresetIfUnique(&presets, &seenIds, QStringLiteral("Kenwood"), QStringLiteral("TS-890S USB Audio / DATA SEND"), 2041);
    addPresetIfUnique(&presets, &seenIds, QStringLiteral("Kenwood"), QStringLiteral("TS-990S USB Audio / DATA SEND"), 2039);
    addPresetIfUnique(&presets, &seenIds, QStringLiteral("Kenwood"), QStringLiteral("TS-2000"), 2014);

    addPresetIfUnique(&presets, &seenIds, QStringLiteral("Icom"), QStringLiteral("IC-705"), 3085);
    addPresetIfUnique(&presets, &seenIds, QStringLiteral("Icom"), QStringLiteral("IC-7100"), 3070);
    addPresetIfUnique(&presets, &seenIds, QStringLiteral("Icom"), QStringLiteral("IC-7300"), 3073);
    addPresetIfUnique(&presets, &seenIds, QStringLiteral("Icom"), QStringLiteral("IC-7610"), 3078);
    addPresetIfUnique(&presets, &seenIds, QStringLiteral("Icom"), QStringLiteral("IC-9700"), 3081);
    addPresetIfUnique(&presets, &seenIds, QStringLiteral("Icom"), QStringLiteral("IC-9100"), 3068);

    addPresetIfUnique(&presets, &seenIds, QStringLiteral("Yaesu"), QStringLiteral("FT-891"), 1036);
    addPresetIfUnique(&presets, &seenIds, QStringLiteral("Yaesu"), QStringLiteral("FT-991 / FT-991A"), 1035);
    addPresetIfUnique(&presets, &seenIds, QStringLiteral("Yaesu"), QStringLiteral("FT-710"), 1049);
    addPresetIfUnique(&presets, &seenIds, QStringLiteral("Yaesu"), QStringLiteral("FTDX10"), 1042);
    addPresetIfUnique(&presets, &seenIds, QStringLiteral("Yaesu"), QStringLiteral("FTDX101D"), 1040);
    addPresetIfUnique(&presets, &seenIds, QStringLiteral("Yaesu"), QStringLiteral("FTDX101MP"), 1044);

    addPresetIfUnique(&presets, &seenIds, QStringLiteral("Elecraft"), QStringLiteral("K2"), 2021);
    addPresetIfUnique(&presets, &seenIds, QStringLiteral("Elecraft"), QStringLiteral("K3"), 2029);
    addPresetIfUnique(&presets, &seenIds, QStringLiteral("Elecraft"), QStringLiteral("K3S"), 2043);
    addPresetIfUnique(&presets, &seenIds, QStringLiteral("Elecraft"), QStringLiteral("KX2"), 2044);
    addPresetIfUnique(&presets, &seenIds, QStringLiteral("Elecraft"), QStringLiteral("KX3"), 2045);
    addPresetIfUnique(&presets, &seenIds, QStringLiteral("Elecraft"), QStringLiteral("K4"), 2047);

    addPresetIfUnique(&presets, &seenIds, QStringLiteral("Alinco"), QStringLiteral("DX-77"), 1001);
    addPresetIfUnique(&presets, &seenIds, QStringLiteral("Xiegu"), QStringLiteral("G90"), 3088);
    addPresetIfUnique(&presets, &seenIds, QStringLiteral("Xiegu"), QStringLiteral("X5105"), 3089);
    addPresetIfUnique(&presets, &seenIds, QStringLiteral("Xiegu"), QStringLiteral("X6100"), 3087);
    addPresetIfUnique(&presets, &seenIds, QStringLiteral("Xiegu"), QStringLiteral("X6200"), 3091);
    addPresetIfUnique(&presets, &seenIds, QStringLiteral("Lab599"), QStringLiteral("Discovery TX-500"), 2050);

    return presets;
}

QList<RadioPreset> buildRadioPresets()
{
    QList<RadioPreset> presets;
    QSet<int> seenIds;

#ifdef MADMODEM_WITH_HAMLIB
    rig_set_debug(RIG_DEBUG_NONE);
    rig_load_all_backends();

    HamlibPresetBuildContext ctx;
    ctx.presets = &presets;
    ctx.seenIds = &seenIds;
    rig_list_foreach(addHamlibCapsCallback, &ctx);
#endif

    if (presets.isEmpty()) {
        presets = fallbackRadioPresets();
        seenIds.clear();
        for (const RadioPreset &preset : presets) {
            seenIds.insert(preset.hamlibId);
        }
    }

    addPresetIfUnique(&presets,
                      &seenIds,
                      QStringLiteral("Network / virtual"),
                      QStringLiteral("Ham Radio Deluxe (HRD Rig Control TCP/IP)"),
                      HamlibController::hamRadioDeluxeModelId());

    std::sort(presets.begin(), presets.end(), [](const RadioPreset &a, const RadioPreset &b) {
        const int makerCmp = QString::localeAwareCompare(a.manufacturer, b.manufacturer);
        if (makerCmp != 0) {
            return makerCmp < 0;
        }
        return QString::localeAwareCompare(a.model, b.model) < 0;
    });

    return presets;
}

const QList<RadioPreset> &allRadioPresets()
{
    static const QList<RadioPreset> presets = buildRadioPresets();
    return presets;
}

const RadioPreset *presetForId(int rigModel)
{
    const QList<RadioPreset> &presets = allRadioPresets();
    for (const RadioPreset &preset : presets) {
        if (preset.hamlibId == rigModel) {
            return &preset;
        }
    }
    return nullptr;
}

} // namespace

RigControlSettingsDialog::RigControlSettingsDialog(const AppSettings &settings,
                                                   QWidget *parent)
    : QDialog(parent),
      m_settings(settings)
{
    buildUi();
    loadFromSettings();
    refreshLabels();
    refreshEnabledState();
    // Keep native geometry; automatic scaling made CAT dialogs too tall on Windows.
}

void RigControlSettingsDialog::setTextTranslator(std::function<QString(const QString &)> translator)
{
    m_textTranslator = std::move(translator);
    refreshLabels();
}

void RigControlSettingsDialog::buildUi()
{
    // Keep the CAT/Hamlib dialog compact.  The previous fix made the
    // window taller; the real solution is to reduce the form height by
    // arranging the settings in two logical columns.  A scroll area is kept
    // only as a safety net for small screens / large fonts, not as the main
    // layout strategy.
    resize(780, 500);
    setMinimumSize(0, 0);

    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(12, 12, 12, 12);
    outer->setSpacing(8);

    m_lblDescription = new QLabel(this);
    m_lblDescription->setWordWrap(true);
    outer->addWidget(m_lblDescription);

    m_lblCompiled = new QLabel(this);
    m_lblCompiled->setWordWrap(true);
    m_lblCompiled->setStyleSheet(HamlibController::isCompiledWithHamlib()
                                     ? QStringLiteral("color: #118a2a; font-weight: bold;")
                                     : QStringLiteral("color: #b00020; font-weight: bold;"));
    outer->addWidget(m_lblCompiled);

    QGroupBox *catGroup = new QGroupBox;
    catGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);

    QGridLayout *grid = new QGridLayout(catGroup);
    grid->setSizeConstraint(QLayout::SetMinimumSize);
    grid->setContentsMargins(10, 10, 10, 10);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(6);

    m_chkCatEnabled = new QCheckBox(catGroup);
    m_chkPttEnabled = new QCheckBox(catGroup);
    m_chkPttEnabled->setVisible(false); // replaced by explicit WSJT-X-style PTT method combo
    m_chkUpdateFt8Band = new QCheckBox(catGroup);

    m_lblManufacturer = new QLabel(catGroup);
    m_cmbManufacturer = new QComboBox(catGroup);
    m_cmbManufacturer->setMinimumWidth(180);

    m_lblModelFilter = new QLabel(catGroup);
    m_editModelFilter = new QLineEdit(catGroup);
    m_editModelFilter->setClearButtonEnabled(true);

    m_lblModel = new QLabel(catGroup);
    m_cmbRigModel = new QComboBox(catGroup);
    m_cmbRigModel->setMinimumWidth(180);

    m_lblManualId = new QLabel(catGroup);
    m_spinRigModel = new QSpinBox(catGroup);
    m_spinRigModel->setRange(1, 999999);
    m_spinRigModel->setValue(1);

    m_lblTxAudioRoute = new QLabel(catGroup);
    m_cmbTxAudioRoute = new QComboBox(catGroup);
    m_cmbTxAudioRoute->addItem(QString(), QStringLiteral("default"));
    m_cmbTxAudioRoute->addItem(QString(), QStringLiteral("usb"));
    m_cmbTxAudioRoute->addItem(QString(), QStringLiteral("data_pkt"));

    m_lblTransmitAudioSource = new QLabel(catGroup);
    m_cmbTransmitAudioSource = new QComboBox(catGroup);
    m_cmbTransmitAudioSource->addItem(QString(), QStringLiteral("rear_data"));
    m_cmbTransmitAudioSource->addItem(QString(), QStringLiteral("front_mic"));

    m_lblSerialPort = new QLabel(catGroup);
    m_cmbSerialPort = new QComboBox(catGroup);
    m_cmbSerialPort->setEditable(true);
    m_cmbSerialPort->setMinimumWidth(180);

    m_lblTcpAddress = new QLabel(catGroup);
    m_editTcpAddress = new QLineEdit(catGroup);
    m_editTcpAddress->setMinimumWidth(180);

    m_lblBaud = new QLabel(catGroup);
    m_cmbBaud = new QComboBox(catGroup);
    const QList<int> baudRates = {1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400, 460800};
    for (int baud : baudRates) {
        m_cmbBaud->addItem(QString::number(baud), baud);
    }
    m_cmbBaud->setEditable(true);

    m_lblDataBits = new QLabel(catGroup);
    m_cmbDataBits = new QComboBox(catGroup);
    m_cmbDataBits->addItem(QStringLiteral("Default"), 0);
    m_cmbDataBits->addItem(QStringLiteral("7"), 7);
    m_cmbDataBits->addItem(QStringLiteral("8"), 8);

    m_lblStopBits = new QLabel(catGroup);
    m_cmbStopBits = new QComboBox(catGroup);
    m_cmbStopBits->addItem(QStringLiteral("Default"), 0);
    m_cmbStopBits->addItem(QStringLiteral("1"), 1);
    m_cmbStopBits->addItem(QStringLiteral("2"), 2);

    m_lblHandshake = new QLabel(catGroup);
    m_cmbHandshake = new QComboBox(catGroup);
    m_cmbHandshake->addItem(QStringLiteral("Default / unchanged"), QStringLiteral("default"));
    m_cmbHandshake->addItem(QStringLiteral("None"), QStringLiteral("none"));
    m_cmbHandshake->addItem(QStringLiteral("XON-XOFF"), QStringLiteral("xonxoff"));
    m_cmbHandshake->addItem(QStringLiteral("Hardware RTS/CTS"), QStringLiteral("hardware"));

    m_lblForceDtr = new QLabel(catGroup);
    m_cmbForceDtr = new QComboBox(catGroup);
    m_cmbForceDtr->addItem(QStringLiteral("Unchanged"), QStringLiteral("unchanged"));
    m_cmbForceDtr->addItem(QStringLiteral("Force low/OFF"), QStringLiteral("off"));
    m_cmbForceDtr->addItem(QStringLiteral("Force high/ON"), QStringLiteral("on"));

    m_lblForceRts = new QLabel(catGroup);
    m_cmbForceRts = new QComboBox(catGroup);
    m_cmbForceRts->addItem(QStringLiteral("Unchanged"), QStringLiteral("unchanged"));
    m_cmbForceRts->addItem(QStringLiteral("Force low/OFF"), QStringLiteral("off"));
    m_cmbForceRts->addItem(QStringLiteral("Force high/ON"), QStringLiteral("on"));

    m_lblPttMethod = new QLabel(catGroup);
    m_cmbPttMethod = new QComboBox(catGroup);
    m_cmbPttMethod->addItem(QStringLiteral("None / audio only"), QStringLiteral("none"));
    m_cmbPttMethod->addItem(QStringLiteral("CAT / Hamlib"), QStringLiteral("cat_hamlib"));
    m_cmbPttMethod->addItem(QStringLiteral("Serial RTS"), QStringLiteral("serial_rts"));
    m_cmbPttMethod->addItem(QStringLiteral("Serial DTR"), QStringLiteral("serial_dtr"));

    m_lblBaudHint = new QLabel(catGroup);
    m_lblBaudHint->setWordWrap(true);
    m_lblBaudHint->setStyleSheet(QStringLiteral("color: #5b6b5b; font-style: italic;"));

    m_lblPoll = new QLabel(catGroup);
    m_spinPollMs = new QSpinBox(catGroup);
    m_spinPollMs->setRange(250, 10000);
    m_spinPollMs->setSingleStep(250);
    m_spinPollMs->setSuffix(QStringLiteral(" ms"));

    int row = 0;
    grid->addWidget(m_chkCatEnabled, row, 0, 1, 2);
    grid->addWidget(m_chkUpdateFt8Band, row++, 2, 1, 2);

    grid->addWidget(m_lblManufacturer, row, 0);
    grid->addWidget(m_cmbManufacturer, row, 1);
    grid->addWidget(m_lblSerialPort, row, 2);
    grid->addWidget(m_cmbSerialPort, row++, 3);

    grid->addWidget(m_lblModelFilter, row, 0);
    grid->addWidget(m_editModelFilter, row, 1);
    grid->addWidget(m_lblTcpAddress, row, 2);
    grid->addWidget(m_editTcpAddress, row++, 3);

    grid->addWidget(m_lblModel, row, 0);
    grid->addWidget(m_cmbRigModel, row, 1);
    grid->addWidget(m_lblBaud, row, 2);
    grid->addWidget(m_cmbBaud, row++, 3);

    grid->addWidget(m_lblManualId, row, 0);
    grid->addWidget(m_spinRigModel, row, 1);
    grid->addWidget(m_lblDataBits, row, 2);
    grid->addWidget(m_cmbDataBits, row++, 3);

    grid->addWidget(m_lblTxAudioRoute, row, 0);
    grid->addWidget(m_cmbTxAudioRoute, row, 1);
    grid->addWidget(m_lblTransmitAudioSource, row, 2);
    grid->addWidget(m_cmbTransmitAudioSource, row++, 3);

    grid->addWidget(m_lblPttMethod, row, 0);
    grid->addWidget(m_cmbPttMethod, row, 1);
    grid->addWidget(m_lblStopBits, row, 2);
    grid->addWidget(m_cmbStopBits, row++, 3);

    grid->addWidget(m_lblHandshake, row, 0);
    grid->addWidget(m_cmbHandshake, row, 1);
    grid->addWidget(m_lblForceDtr, row, 2);
    grid->addWidget(m_cmbForceDtr, row++, 3);

    grid->addWidget(m_lblForceRts, row, 0);
    grid->addWidget(m_cmbForceRts, row, 1);
    grid->addWidget(m_lblPoll, row, 2);
    grid->addWidget(m_spinPollMs, row++, 3);

    grid->addWidget(m_lblBaudHint, row++, 0, 1, 4);

    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(3, 1);

    QScrollArea *catScroll = new QScrollArea(this);
    catScroll->setWidgetResizable(true);
    catScroll->setFrameShape(QFrame::StyledPanel);
    catScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    catScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    catScroll->setMinimumHeight(230);
    catScroll->setWidget(catGroup);
    outer->addWidget(catScroll, 1);

    m_lblHint = new QLabel(this);
    m_lblHint->setWordWrap(true);
    outer->addWidget(m_lblHint);

    QGroupBox *testGroup = new QGroupBox(this);
    testGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    QHBoxLayout *testRow = new QHBoxLayout(testGroup);
    testRow->setContentsMargins(12, 8, 12, 8);
    testRow->setSpacing(10);

    m_btnTestCat = new QPushButton(testGroup);
    m_btnTestPttOn = nullptr;
    m_btnTestPttOff = nullptr;

    m_lblCatLed = new QLabel(testGroup);
    m_lblCatLed->setFixedSize(18, 18);
    m_lblCatLed->setToolTip(L("CAT test status"));

    m_lblTestStatus = new QLabel(testGroup);
    m_lblTestStatus->setWordWrap(false);
    m_lblTestStatus->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_lblTestStatus->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    testRow->addWidget(m_btnTestCat);
    testRow->addSpacing(8);
    testRow->addWidget(m_lblCatLed);
    testRow->addWidget(m_lblTestStatus, 1);
    outer->addWidget(testGroup);

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    outer->addWidget(m_buttonBox);

    populateManufacturers();
    populateSerialPorts(m_settings.hamlibSerialPath.isEmpty() ? m_settings.hamlibRigPath : m_settings.hamlibSerialPath);

    connect(m_chkCatEnabled, &QCheckBox::toggled,
            this, &RigControlSettingsDialog::refreshEnabledState);
    connect(m_chkPttEnabled, &QCheckBox::toggled,
            this, [this](bool checked) {
                if (!m_loading && m_cmbPttMethod != nullptr) {
                    const int idx = m_cmbPttMethod->findData(checked ? QStringLiteral("cat_hamlib") : QStringLiteral("none"));
                    if (idx >= 0) m_cmbPttMethod->setCurrentIndex(idx);
                }
                refreshEnabledState();
            });
    connect(m_cmbPttMethod, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this]() {
                if (!m_loading && m_chkPttEnabled != nullptr && m_cmbPttMethod != nullptr) {
                    m_chkPttEnabled->setChecked(m_cmbPttMethod->currentData().toString() == QStringLiteral("cat_hamlib"));
                }
                refreshEnabledState();
            });
    connect(m_cmbManufacturer, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RigControlSettingsDialog::onManufacturerChanged);
    connect(m_cmbRigModel, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RigControlSettingsDialog::onModelChanged);
    connect(m_editModelFilter, &QLineEdit::textChanged,
            this, &RigControlSettingsDialog::onModelFilterChanged);
    connect(m_btnTestCat, &QPushButton::clicked,
            this, &RigControlSettingsDialog::onTestCatClicked);
    connect(m_buttonBox, &QDialogButtonBox::accepted,
            this, [this]() {
                applyToSettings();
                accept();
            });
    connect(m_buttonBox, &QDialogButtonBox::rejected,
            this, &RigControlSettingsDialog::reject);
}

void RigControlSettingsDialog::populateManufacturers()
{
    if (m_cmbManufacturer == nullptr) {
        return;
    }

    QSignalBlocker blocker(m_cmbManufacturer);
    m_cmbManufacturer->clear();

    QStringList manufacturers;
    const QList<RadioPreset> &presets = allRadioPresets();
    for (const RadioPreset &preset : presets) {
        if (!manufacturers.contains(preset.manufacturer)) {
            manufacturers.append(preset.manufacturer);
        }
    }
    manufacturers.sort(Qt::CaseInsensitive);
    manufacturers.append(customManufacturerText());

    for (const QString &manufacturer : manufacturers) {
        m_cmbManufacturer->addItem(manufacturer, manufacturer);
    }
}

void RigControlSettingsDialog::populateModelsForManufacturer(const QString &manufacturer, int selectedRigModel)
{
    if (m_cmbRigModel == nullptr) {
        return;
    }

    QSignalBlocker blocker(m_cmbRigModel);
    m_cmbRigModel->clear();

    const QString filter = (m_editModelFilter != nullptr) ? m_editModelFilter->text().trimmed() : QString();
    const QList<RadioPreset> &presets = allRadioPresets();
    for (const RadioPreset &preset : presets) {
        if (manufacturer != preset.manufacturer) {
            continue;
        }
        if (!filter.isEmpty() && !preset.model.contains(filter, Qt::CaseInsensitive)) {
            continue;
        }
        m_cmbRigModel->addItem(QStringLiteral("%1  [%2]").arg(preset.model).arg(preset.hamlibId), preset.hamlibId);
    }

    int idx = -1;
    if (selectedRigModel > 0) {
        idx = m_cmbRigModel->findData(selectedRigModel);
    }
    if (idx < 0 && m_cmbRigModel->count() > 0) {
        idx = 0;
    }
    if (idx >= 0) {
        m_cmbRigModel->setCurrentIndex(idx);
    }
}

void RigControlSettingsDialog::populateSerialPorts(const QString &selectedPath)
{
    if (m_cmbSerialPort == nullptr) {
        return;
    }

    QSignalBlocker blocker(m_cmbSerialPort);
    m_cmbSerialPort->clear();

    // WSJT-X-like behaviour: never auto-select the first detected serial port.
    // The user must keep the saved port, pick a port, or type one manually.
    // This avoids accidentally opening/keying the wrong USB adapter when several
    // CAT/PTT interfaces are present.
    m_cmbSerialPort->addItem(L(QStringLiteral("Select/type CAT serial port")), QString());

    const QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &port : ports) {
        const QString systemPath = port.systemLocation().trimmed().isEmpty()
                                   ? port.portName().trimmed()
                                   : port.systemLocation().trimmed();
        QString label = port.portName();
        if (!port.description().trimmed().isEmpty()) {
            label += QStringLiteral(" - ") + port.description().trimmed();
        }
        if (!systemPath.isEmpty() && systemPath != port.portName()) {
            label += QStringLiteral("  (") + systemPath + QStringLiteral(")");
        }
        m_cmbSerialPort->addItem(label, systemPath);
    }

    if (ports.isEmpty()) {
        m_cmbSerialPort->addItem(L(QStringLiteral("No serial port found - type one manually")), QString());
    }

    const QString path = selectedPath.trimmed();
    if (!path.isEmpty() && !rigPathLooksNetwork(path)) {
        int idx = m_cmbSerialPort->findData(path);
        if (idx < 0) {
            idx = m_cmbSerialPort->findText(path);
        }
        if (idx >= 0) {
            m_cmbSerialPort->setCurrentIndex(idx);
        } else {
            m_cmbSerialPort->setEditText(path);
        }
    } else if (m_cmbSerialPort->count() > 0) {
        m_cmbSerialPort->setCurrentIndex(0);
    }
}


QString RigControlSettingsDialog::selectedSerialPath() const
{
    if (m_cmbSerialPort == nullptr) {
        return QString();
    }
    const QString data = m_cmbSerialPort->currentData().toString().trimmed();
    if (!data.isEmpty()) {
        return data;
    }
    const QString text = m_cmbSerialPort->currentText().trimmed();
    if (text.startsWith(QStringLiteral("No serial port"), Qt::CaseInsensitive) ||
        text == L(QStringLiteral("No serial port found - type one manually")) ||
        text == L(QStringLiteral("Select/type CAT serial port"))) {
        return QString();
    }
    return text;
}

bool RigControlSettingsDialog::rigPathLooksNetwork(const QString &path) const
{
    const QString p = path.trimmed();
    if (p.isEmpty()) {
        return false;
    }
    if (p.startsWith(QStringLiteral("rigctld:"), Qt::CaseInsensitive) ||
        p.startsWith(QStringLiteral("tcp:"), Qt::CaseInsensitive)) {
        return true;
    }
    // Host:port or IPv4:port.  Windows COM ports such as COM3 must not match.
    const int colon = p.lastIndexOf(QChar(':'));
    if (colon > 0 && colon < p.size() - 1) {
        bool portOk = false;
        p.mid(colon + 1).toInt(&portOk);
        return portOk && !p.left(colon).startsWith(QStringLiteral("COM"), Qt::CaseInsensitive);
    }
    return false;
}

void RigControlSettingsDialog::selectPresetForRigModel(int rigModel)
{
    m_loading = true;

    const RadioPreset *preset = presetForId(rigModel);
    if (preset != nullptr) {
        const QString manufacturer = preset->manufacturer;
        const int manufacturerIndex = m_cmbManufacturer != nullptr
                                          ? m_cmbManufacturer->findData(manufacturer)
                                          : -1;
        if (manufacturerIndex >= 0 && m_cmbManufacturer != nullptr) {
            m_cmbManufacturer->setCurrentIndex(manufacturerIndex);
            if (m_editModelFilter != nullptr) {
                m_editModelFilter->clear();
            }
            populateModelsForManufacturer(manufacturer, rigModel);
        }
    } else {
        const QString custom = customManufacturerText();
        const int manufacturerIndex = m_cmbManufacturer != nullptr
                                          ? m_cmbManufacturer->findData(custom)
                                          : -1;
        if (manufacturerIndex >= 0 && m_cmbManufacturer != nullptr) {
            m_cmbManufacturer->setCurrentIndex(manufacturerIndex);
        }
        if (m_cmbRigModel != nullptr) {
            m_cmbRigModel->clear();
        }
    }

    if (m_spinRigModel != nullptr) {
        m_spinRigModel->setValue(qMax(1, rigModel));
    }

    m_loading = false;
    refreshEnabledState();
}

void RigControlSettingsDialog::loadFromSettings()
{
    m_loading = true;

    if (m_chkCatEnabled != nullptr) {
        m_chkCatEnabled->setChecked(m_settings.hamlibCatEnabled);
    }
    if (m_chkPttEnabled != nullptr) {
        m_chkPttEnabled->setChecked(m_settings.hamlibPttEnabled);
    }
    if (m_chkUpdateFt8Band != nullptr) {
        m_chkUpdateFt8Band->setChecked(m_settings.hamlibUpdateFt8Band);
    }

    m_loading = false;
    selectPresetForRigModel(qMax(1, m_settings.hamlibRigModel));
    m_loading = true;

    if (m_cmbTxAudioRoute != nullptr) {
        QString txMode = m_settings.hamlibTxAudioRoute;
        if (txMode == QStringLiteral("data") ||
            txMode == QStringLiteral("force_data_usb") ||
            txMode == QStringLiteral("kenwood_usb") ||
            txMode == QStringLiteral("kenwood_acc2") ||
            txMode == QStringLiteral("kenwood_lan")) {
            txMode = QStringLiteral("data_pkt");
        }
        int idx = m_cmbTxAudioRoute->findData(txMode);
        if (idx < 0) {
            idx = m_cmbTxAudioRoute->findData(QStringLiteral("default"));
        }
        if (idx >= 0) {
            m_cmbTxAudioRoute->setCurrentIndex(idx);
        }
    }
    if (m_cmbTransmitAudioSource != nullptr) {
        QString source = m_settings.hamlibTransmitAudioSource.trimmed().toLower();
        if (source != QStringLiteral("front_mic")) {
            source = QStringLiteral("rear_data");
        }
        int idx = m_cmbTransmitAudioSource->findData(source);
        if (idx < 0) {
            idx = m_cmbTransmitAudioSource->findData(QStringLiteral("rear_data"));
        }
        if (idx >= 0) {
            m_cmbTransmitAudioSource->setCurrentIndex(idx);
        }
    }
    if (m_editTcpAddress != nullptr) {
        m_editTcpAddress->setText(!m_settings.hamlibTcpAddress.isEmpty() ? m_settings.hamlibTcpAddress : (rigPathLooksNetwork(m_settings.hamlibRigPath) ? m_settings.hamlibRigPath : QString()));
    }
    populateSerialPorts(m_settings.hamlibSerialPath.isEmpty() ? m_settings.hamlibRigPath : m_settings.hamlibSerialPath);
    if (m_cmbBaud != nullptr) {
        setBaudRateValue(m_settings.hamlibBaudRate);
    }
    auto setComboData = [](QComboBox *combo, const QVariant &data) {
        if (combo == nullptr) {
            return;
        }
        const int idx = combo->findData(data);
        if (idx >= 0) {
            combo->setCurrentIndex(idx);
        }
    };
    setComboData(m_cmbDataBits, m_settings.hamlibDataBits);
    setComboData(m_cmbStopBits, m_settings.hamlibStopBits);
    setComboData(m_cmbHandshake, m_settings.hamlibHandshake);
    setComboData(m_cmbForceDtr, m_settings.hamlibForceDtr);
    setComboData(m_cmbForceRts, m_settings.hamlibForceRts);
    const QString pttMethod = m_settings.pttMethod.trimmed().toLower();
    setComboData(m_cmbPttMethod, pttMethod.isEmpty() ? QStringLiteral("none") : pttMethod);
    if (m_chkPttEnabled != nullptr) {
        m_chkPttEnabled->setChecked(pttMethod == QStringLiteral("cat_hamlib"));
    }
    if (m_spinPollMs != nullptr) {
        m_spinPollMs->setValue(qBound(250, m_settings.hamlibPollIntervalMs, 10000));
    }

    m_loading = false;
    refreshBaudSuggestionForRigModel(selectedRigModel());
}

void RigControlSettingsDialog::refreshLabels()
{
    setWindowTitle(L("CAT / Rig control"));
    if (m_lblDescription != nullptr) {
        m_lblDescription->setText(L("Configure CAT frequency polling, PTT, Hamlib radios and Ham Radio Deluxe TCP control."));
    }
    if (m_lblCompiled != nullptr) {
        const int modelCount = allRadioPresets().count();
        m_lblCompiled->setText(HamlibController::isCompiledWithHamlib()
                                   ? L("Hamlib available") + QStringLiteral(" — ") + L("radio models:") + QStringLiteral(" %1").arg(modelCount)
                                   : L("Hamlib not compiled in"));
    }
    if (m_chkCatEnabled != nullptr) {
        m_chkCatEnabled->setText(L("Enable CAT frequency polling"));
    }
    if (m_chkPttEnabled != nullptr) {
        m_chkPttEnabled->setText(L("Use CAT PTT for transmit"));
    }
    if (m_chkUpdateFt8Band != nullptr) {
        m_chkUpdateFt8Band->setText(L("Update FT8 band field from CAT frequency"));
    }
    if (m_cmbManufacturer != nullptr) {
        const int customIndex = m_cmbManufacturer->findData(customManufacturerText());
        if (customIndex >= 0) {
            m_cmbManufacturer->setItemText(customIndex, L(customManufacturerText()));
        }
    }
    if (m_lblManufacturer != nullptr) {
        m_lblManufacturer->setText(L("Radio maker"));
    }
    if (m_lblModelFilter != nullptr) {
        m_lblModelFilter->setText(L("Filter models"));
    }
    if (m_editModelFilter != nullptr) {
        m_editModelFilter->setPlaceholderText(L("Type part of the model name, e.g. 7300, FT-991, TS-590"));
    }
    if (m_lblModel != nullptr) {
        m_lblModel->setText(L("Radio model"));
    }
    if (m_lblManualId != nullptr) {
        m_lblManualId->setText(L("Custom Hamlib model ID"));
    }
    if (m_lblTxAudioRoute != nullptr) {
        m_lblTxAudioRoute->setText(L("Radio mode on TX"));
    }
    if (m_cmbTxAudioRoute != nullptr) {
        const int current = m_cmbTxAudioRoute->currentIndex();
        m_cmbTxAudioRoute->setItemText(0, L("None / keep current mode"));
        m_cmbTxAudioRoute->setItemText(1, L("USB"));
        m_cmbTxAudioRoute->setItemText(2, L("Data/Pkt"));
        if (current >= 0) {
            m_cmbTxAudioRoute->setCurrentIndex(current);
        }
    }
    if (m_lblTransmitAudioSource != nullptr) {
        m_lblTransmitAudioSource->setText(L("Transmit audio source"));
    }
    if (m_cmbTransmitAudioSource != nullptr) {
        const int current = m_cmbTransmitAudioSource->currentIndex();
        m_cmbTransmitAudioSource->setItemText(0, L("Rear/Data"));
        m_cmbTransmitAudioSource->setItemText(1, L("Front/Mic"));
        if (current >= 0) {
            m_cmbTransmitAudioSource->setCurrentIndex(current);
        }
    }
    if (m_lblSerialPort != nullptr) {
        m_lblSerialPort->setText(L("CAT serial port"));
    }
    if (m_lblTcpAddress != nullptr) {
        m_lblTcpAddress->setText(L("CAT TCP address"));
    }
    if (m_editTcpAddress != nullptr) {
        m_editTcpAddress->setPlaceholderText(L("Optional: host:port, rigctld:host:port or HRD localhost:7809; leave empty for serial CAT"));
    }
    if (m_lblBaud != nullptr) {
        m_lblBaud->setText(L("Serial speed"));
    }
    if (m_lblDataBits != nullptr) {
        m_lblDataBits->setText(L("Data bits"));
    }
    if (m_cmbDataBits != nullptr) {
        const int current = m_cmbDataBits->currentIndex();
        m_cmbDataBits->setItemText(0, L("Default"));
        m_cmbDataBits->setItemText(1, QStringLiteral("7"));
        m_cmbDataBits->setItemText(2, QStringLiteral("8"));
        if (current >= 0) m_cmbDataBits->setCurrentIndex(current);
    }
    if (m_lblStopBits != nullptr) {
        m_lblStopBits->setText(L("Stop bits"));
    }
    if (m_cmbStopBits != nullptr) {
        const int current = m_cmbStopBits->currentIndex();
        m_cmbStopBits->setItemText(0, L("Default"));
        m_cmbStopBits->setItemText(1, QStringLiteral("1"));
        m_cmbStopBits->setItemText(2, QStringLiteral("2"));
        if (current >= 0) m_cmbStopBits->setCurrentIndex(current);
    }
    if (m_lblHandshake != nullptr) {
        m_lblHandshake->setText(L("Handshake"));
    }
    if (m_cmbHandshake != nullptr) {
        const int current = m_cmbHandshake->currentIndex();
        m_cmbHandshake->setItemText(0, L("Default / unchanged"));
        m_cmbHandshake->setItemText(1, L("None"));
        m_cmbHandshake->setItemText(2, L("XON-XOFF"));
        m_cmbHandshake->setItemText(3, L("Hardware RTS/CTS"));
        if (current >= 0) m_cmbHandshake->setCurrentIndex(current);
    }
    if (m_lblForceDtr != nullptr) {
        m_lblForceDtr->setText(L("Force DTR"));
    }
    if (m_cmbForceDtr != nullptr) {
        const int current = m_cmbForceDtr->currentIndex();
        m_cmbForceDtr->setItemText(0, L("Unchanged"));
        m_cmbForceDtr->setItemText(1, L("Force low/OFF"));
        m_cmbForceDtr->setItemText(2, L("Force high/ON"));
        if (current >= 0) m_cmbForceDtr->setCurrentIndex(current);
    }
    if (m_lblForceRts != nullptr) {
        m_lblForceRts->setText(L("Force RTS"));
    }
    if (m_cmbForceRts != nullptr) {
        const int current = m_cmbForceRts->currentIndex();
        m_cmbForceRts->setItemText(0, L("Unchanged"));
        m_cmbForceRts->setItemText(1, L("Force low/OFF"));
        m_cmbForceRts->setItemText(2, L("Force high/ON"));
        if (current >= 0) m_cmbForceRts->setCurrentIndex(current);
    }
    if (m_lblPttMethod != nullptr) {
        m_lblPttMethod->setText(L("PTT method"));
    }
    if (m_cmbPttMethod != nullptr) {
        const int current = m_cmbPttMethod->currentIndex();
        m_cmbPttMethod->setItemText(0, L("None / audio only"));
        m_cmbPttMethod->setItemText(1, L("CAT / Hamlib"));
        m_cmbPttMethod->setItemText(2, L("Serial RTS"));
        m_cmbPttMethod->setItemText(3, L("Serial DTR"));
        if (current >= 0) m_cmbPttMethod->setCurrentIndex(current);
    }
    if (m_lblBaudHint != nullptr && m_lblBaudHint->text().isEmpty()) {
        refreshBaudSuggestionForRigModel(selectedRigModel());
    }
    if (m_lblPoll != nullptr) {
        m_lblPoll->setText(L("Poll interval"));
    }
    if (m_lblHint != nullptr) {
        m_lblHint->setText(L("CAT test reads only the rig frequency. It never keys PTT; DTR/RTS and handshake are used only if explicitly selected here."));
    }
    if (m_btnTestCat != nullptr) {
        m_btnTestCat->setText(L("Test CAT / read frequency"));
        m_btnTestCat->setToolTip(L("Read the rig frequency using the settings shown here. No TX/PTT command is sent."));
    }
    if (m_lblTestStatus != nullptr && m_lblTestStatus->text().isEmpty()) {
        setTestStatus(L("CAT not tested"), false);
    }
    if (m_buttonBox != nullptr) {
        if (QPushButton *ok = m_buttonBox->button(QDialogButtonBox::Ok)) {
            ok->setText(L("OK"));
        }
        if (QPushButton *cancel = m_buttonBox->button(QDialogButtonBox::Cancel)) {
            cancel->setText(L("Cancel"));
        }
    }
}

void RigControlSettingsDialog::applyToSettings()
{
    m_settings.hamlibCatEnabled = (m_chkCatEnabled != nullptr) && m_chkCatEnabled->isChecked();
    const QString selectedPttMethod = (m_cmbPttMethod != nullptr)
                                          ? m_cmbPttMethod->currentData().toString()
                                          : ((m_chkPttEnabled != nullptr && m_chkPttEnabled->isChecked()) ? QStringLiteral("cat_hamlib") : m_settings.pttMethod);
    m_settings.pttMethod = selectedPttMethod.isEmpty() ? QStringLiteral("none") : selectedPttMethod;
    m_settings.hamlibPttEnabled = (m_settings.pttMethod == QStringLiteral("cat_hamlib"));
    m_settings.hamlibUpdateFt8Band = (m_chkUpdateFt8Band != nullptr) && m_chkUpdateFt8Band->isChecked();
    m_settings.hamlibRigModel = selectedRigModel();
    const QString tcpAddress = (m_editTcpAddress != nullptr) ? m_editTcpAddress->text().trimmed() : QString();
    const QString serialPath = selectedSerialPath().trimmed();
    m_settings.hamlibSerialPath = serialPath;
    m_settings.hamlibTcpAddress = tcpAddress;
    m_settings.hamlibRigPath = tcpAddress.isEmpty() ? serialPath : tcpAddress;
    m_settings.hamlibBaudRate = (m_cmbBaud != nullptr) ? m_cmbBaud->currentText().trimmed().toInt() : 38400;
    if (m_settings.hamlibBaudRate <= 0) {
        m_settings.hamlibBaudRate = 38400;
    }
    m_settings.hamlibDataBits = (m_cmbDataBits != nullptr) ? m_cmbDataBits->currentData().toInt() : 0;
    m_settings.hamlibStopBits = (m_cmbStopBits != nullptr) ? m_cmbStopBits->currentData().toInt() : 0;
    m_settings.hamlibHandshake = (m_cmbHandshake != nullptr) ? m_cmbHandshake->currentData().toString() : QStringLiteral("default");
    m_settings.hamlibForceDtr = (m_cmbForceDtr != nullptr) ? m_cmbForceDtr->currentData().toString() : QStringLiteral("unchanged");
    m_settings.hamlibForceRts = (m_cmbForceRts != nullptr) ? m_cmbForceRts->currentData().toString() : QStringLiteral("unchanged");
    m_settings.hamlibPollIntervalMs = (m_spinPollMs != nullptr) ? m_spinPollMs->value() : 1000;
    m_settings.hamlibTxAudioRoute = (m_cmbTxAudioRoute != nullptr)
                                        ? m_cmbTxAudioRoute->currentData().toString()
                                        : QStringLiteral("default");
    if (m_settings.hamlibTxAudioRoute.isEmpty()) {
        m_settings.hamlibTxAudioRoute = QStringLiteral("default");
    }
    if (m_settings.hamlibTxAudioRoute == QStringLiteral("data") ||
        m_settings.hamlibTxAudioRoute == QStringLiteral("force_data_usb") ||
        m_settings.hamlibTxAudioRoute == QStringLiteral("kenwood_usb") ||
        m_settings.hamlibTxAudioRoute == QStringLiteral("kenwood_acc2") ||
        m_settings.hamlibTxAudioRoute == QStringLiteral("kenwood_lan")) {
        m_settings.hamlibTxAudioRoute = QStringLiteral("data_pkt");
    }
    m_settings.hamlibTransmitAudioSource = (m_cmbTransmitAudioSource != nullptr)
                                              ? m_cmbTransmitAudioSource->currentData().toString()
                                              : QStringLiteral("rear_data");
    if (m_settings.hamlibTransmitAudioSource.isEmpty()) {
        m_settings.hamlibTransmitAudioSource = QStringLiteral("rear_data");
    }
}

void RigControlSettingsDialog::refreshEnabledState()
{
    const QString pttMethodForEnable = (m_cmbPttMethod != nullptr) ? m_cmbPttMethod->currentData().toString() : QStringLiteral("none");
    const bool pttRequested = (pttMethodForEnable == QStringLiteral("cat_hamlib") ||
                               pttMethodForEnable == QStringLiteral("serial_rts") ||
                               pttMethodForEnable == QStringLiteral("serial_dtr"));
    const bool enabled = ((m_chkCatEnabled != nullptr) && m_chkCatEnabled->isChecked()) || pttRequested;
    const bool cat = (m_chkCatEnabled != nullptr) && m_chkCatEnabled->isChecked();
    const bool custom = (m_cmbManufacturer != nullptr) &&
                        (m_cmbManufacturer->currentData().toString() == customManufacturerText());

    if (m_cmbManufacturer != nullptr) m_cmbManufacturer->setEnabled(enabled);
    if (m_editModelFilter != nullptr) m_editModelFilter->setEnabled(enabled && !custom);
    if (m_cmbRigModel != nullptr) m_cmbRigModel->setEnabled(enabled && !custom);
    if (m_lblManualId != nullptr) m_lblManualId->setVisible(custom);
    if (m_spinRigModel != nullptr) {
        m_spinRigModel->setVisible(custom);
        m_spinRigModel->setEnabled(enabled && custom);
    }
    const bool hrd = HamlibController::isHamRadioDeluxeModel(selectedRigModel());
    const QString pttMethod = (m_cmbPttMethod != nullptr) ? m_cmbPttMethod->currentData().toString() : QStringLiteral("none");
    const bool pttViaHamlib = (pttMethod == QStringLiteral("cat_hamlib") || pttMethod == QStringLiteral("serial_rts") || pttMethod == QStringLiteral("serial_dtr"));
    if (m_cmbTxAudioRoute != nullptr) m_cmbTxAudioRoute->setEnabled(enabled && pttMethod == QStringLiteral("cat_hamlib"));
    if (m_cmbTransmitAudioSource != nullptr) m_cmbTransmitAudioSource->setEnabled(enabled && pttMethod == QStringLiteral("cat_hamlib"));
    if (m_cmbSerialPort != nullptr) m_cmbSerialPort->setEnabled(enabled && !hrd);
    if (m_editTcpAddress != nullptr) m_editTcpAddress->setEnabled(enabled);
    if (m_cmbBaud != nullptr) m_cmbBaud->setEnabled(enabled && !hrd);
    if (m_cmbDataBits != nullptr) m_cmbDataBits->setEnabled(enabled && !hrd);
    if (m_cmbStopBits != nullptr) m_cmbStopBits->setEnabled(enabled && !hrd);
    if (m_cmbHandshake != nullptr) m_cmbHandshake->setEnabled(enabled && !hrd);
    if (m_cmbForceDtr != nullptr) m_cmbForceDtr->setEnabled(enabled && !hrd);
    if (m_cmbForceRts != nullptr) m_cmbForceRts->setEnabled(enabled && !hrd);
    if (m_cmbPttMethod != nullptr) m_cmbPttMethod->setEnabled(enabled);
    if (m_lblBaudHint != nullptr) m_lblBaudHint->setEnabled(enabled);
    if (m_btnTestCat != nullptr) m_btnTestCat->setEnabled(enabled);
    if (m_btnTestPttOn != nullptr) m_btnTestPttOn->setEnabled(enabled && pttViaHamlib);
    if (m_btnTestPttOff != nullptr) m_btnTestPttOff->setEnabled(enabled && pttViaHamlib);
    if (m_spinPollMs != nullptr) m_spinPollMs->setEnabled(cat);
    if (m_chkUpdateFt8Band != nullptr) m_chkUpdateFt8Band->setEnabled(cat);
    if (m_lblSerialPort != nullptr) m_lblSerialPort->setEnabled(!hrd);
    if (m_lblBaud != nullptr) m_lblBaud->setEnabled(!hrd);
    if (m_lblDataBits != nullptr) m_lblDataBits->setEnabled(!hrd);
    if (m_lblStopBits != nullptr) m_lblStopBits->setEnabled(!hrd);
    if (m_lblHandshake != nullptr) m_lblHandshake->setEnabled(!hrd);
    if (m_lblForceDtr != nullptr) m_lblForceDtr->setEnabled(!hrd);
    if (m_lblForceRts != nullptr) m_lblForceRts->setEnabled(!hrd);
    if (m_lblPttMethod != nullptr) m_lblPttMethod->setEnabled(enabled);
    if (m_lblTcpAddress != nullptr) {
        m_lblTcpAddress->setText(hrd ? L("HRD TCP address") : L("CAT TCP address"));
    }
    if (m_editTcpAddress != nullptr) {
        m_editTcpAddress->setPlaceholderText(hrd
            ? L("HRD Rig Control TCP server, normally localhost:7809")
            : L("Optional: host:port or rigctld:host:port; leave empty for serial CAT"));
    }
}

void RigControlSettingsDialog::onManufacturerChanged()
{
    if (m_cmbManufacturer == nullptr) {
        return;
    }

    const QString manufacturer = m_cmbManufacturer->currentData().toString();
    if (manufacturer == customManufacturerText()) {
        if (m_cmbRigModel != nullptr) {
            m_cmbRigModel->clear();
        }
    } else {
        populateModelsForManufacturer(manufacturer, -1);
        onModelChanged();
    }
    refreshBaudSuggestionForRigModel(selectedRigModel());
    refreshEnabledState();
}

void RigControlSettingsDialog::onModelChanged()
{
    const int rigId = (m_cmbRigModel != nullptr) ? m_cmbRigModel->currentData().toInt() : 0;
    if (rigId <= 0) {
        return;
    }

    if (m_spinRigModel != nullptr) {
        m_spinRigModel->setValue(rigId);
    }

    // Do not auto-fill operational CAT/PTT fields when the user changes radio model.
    // Older MadModem versions behaved conservatively: model selection only selects the
    // Hamlib backend ID and shows hints. Serial port, TCP address, baud rate and TX/PTT
    // route must remain exactly as loaded from settings or manually edited by the user.
    refreshBaudSuggestionForRigModel(rigId);

    refreshEnabledState();
}

void RigControlSettingsDialog::onModelFilterChanged(const QString &text)
{
    Q_UNUSED(text);
    if (m_loading || m_cmbManufacturer == nullptr) {
        return;
    }
    const QString manufacturer = m_cmbManufacturer->currentData().toString();
    if (manufacturer == customManufacturerText()) {
        return;
    }
    const int previousRig = selectedRigModel();
    populateModelsForManufacturer(manufacturer, previousRig);
    onModelChanged();
}

int RigControlSettingsDialog::selectedRigModel() const
{
    const bool custom = (m_cmbManufacturer != nullptr) &&
                        (m_cmbManufacturer->currentData().toString() == customManufacturerText());
    if (custom) {
        return (m_spinRigModel != nullptr) ? qMax(1, m_spinRigModel->value()) : 1;
    }
    const int rigId = (m_cmbRigModel != nullptr) ? m_cmbRigModel->currentData().toInt() : 0;
    return qMax(1, rigId);
}

int RigControlSettingsDialog::currentBaudRate() const
{
    const int baud = (m_cmbBaud != nullptr) ? m_cmbBaud->currentText().trimmed().toInt() : 0;
    return baud > 0 ? baud : 38400;
}

void RigControlSettingsDialog::setBaudRateValue(int baudRate)
{
    if (m_cmbBaud == nullptr || baudRate <= 0) {
        return;
    }

    const QSignalBlocker blocker(m_cmbBaud);
    int idx = m_cmbBaud->findData(baudRate);
    if (idx < 0) {
        idx = m_cmbBaud->findText(QString::number(baudRate));
    }
    if (idx >= 0) {
        m_cmbBaud->setCurrentIndex(idx);
    } else {
        m_cmbBaud->setEditText(QString::number(baudRate));
    }
}

void RigControlSettingsDialog::refreshBaudSuggestionForRigModel(int rigModel)
{
    const RadioPreset *preset = presetForId(rigModel);
    BaudSuggestion suggestion;
    if (preset != nullptr) {
        suggestion = baudSuggestionFor(preset->manufacturer, preset->model, preset->hamlibId);
    } else if ((m_cmbManufacturer != nullptr) &&
               (m_cmbManufacturer->currentData().toString() == customManufacturerText())) {
        suggestion.hint = QStringLiteral("Manual Hamlib ID: use the same baud rate configured in the radio CAT menu.");
    } else {
        suggestion.hint = QStringLiteral("Use the same baud rate configured in the radio CAT menu.");
    }

    // Hints only. Never apply the suggested speed to the combo automatically.
    // The real source of truth is the radio CAT menu and the user must choose
    // the value explicitly, like WSJT-X does.
    if (m_lblBaudHint != nullptr) {
        QString text;
        if (suggestion.baudRate > 0) {
            text = L("Suggested serial speed") + QStringLiteral(": %1 baud — ").arg(suggestion.baudRate) + L(suggestion.hint);
        } else {
            text = L(suggestion.hint);
        }
        if (suggestion.baudRate > 0 && currentBaudRate() != suggestion.baudRate) {
            text += QStringLiteral(" ") + L("Current value differs from the suggestion; this is OK if it matches the radio menu.");
        }
        m_lblBaudHint->setText(text);
    }
}


HamlibController::Config RigControlSettingsDialog::configFromCurrentUi(bool forceCat, bool forcePtt, bool readOnlyTest) const
{
    HamlibController::Config cfg;
    cfg.catEnabled = forceCat || ((m_chkCatEnabled != nullptr) && m_chkCatEnabled->isChecked());
    cfg.readOnlyTest = readOnlyTest;
    const QString selectedPttMethod = (m_cmbPttMethod != nullptr)
                                          ? m_cmbPttMethod->currentData().toString()
                                          : (((m_chkPttEnabled != nullptr) && m_chkPttEnabled->isChecked()) ? QStringLiteral("cat_hamlib") : QStringLiteral("none"));
    cfg.pttMethod = readOnlyTest ? QStringLiteral("none") : selectedPttMethod;
    cfg.pttEnabled = readOnlyTest ? false : (forcePtt || cfg.pttMethod == QStringLiteral("cat_hamlib") ||
                                             cfg.pttMethod == QStringLiteral("serial_rts") ||
                                             cfg.pttMethod == QStringLiteral("serial_dtr"));
    cfg.updateFt8BandFromCat = (m_chkUpdateFt8Band != nullptr) && m_chkUpdateFt8Band->isChecked();
    cfg.rigModel = selectedRigModel();

    const QString tcpAddress = (m_editTcpAddress != nullptr) ? m_editTcpAddress->text().trimmed() : QString();
    const QString serialPath = selectedSerialPath().trimmed();
    cfg.rigPath = tcpAddress.isEmpty() ? serialPath : tcpAddress;
    // No hidden default TCP address here. If HRD/rigctld is desired, the user must
    // type the address explicitly; otherwise an empty TCP field means serial CAT.
    cfg.baudRate = (m_cmbBaud != nullptr) ? m_cmbBaud->currentText().trimmed().toInt() : 38400;
    if (cfg.baudRate <= 0) {
        cfg.baudRate = 38400;
    }
    cfg.dataBits = (m_cmbDataBits != nullptr) ? m_cmbDataBits->currentData().toInt() : 0;
    cfg.stopBits = (m_cmbStopBits != nullptr) ? m_cmbStopBits->currentData().toInt() : 0;
    cfg.handshake = (m_cmbHandshake != nullptr) ? m_cmbHandshake->currentData().toString() : QStringLiteral("default");
    cfg.forceDtr = (m_cmbForceDtr != nullptr) ? m_cmbForceDtr->currentData().toString() : QStringLiteral("unchanged");
    cfg.forceRts = (m_cmbForceRts != nullptr) ? m_cmbForceRts->currentData().toString() : QStringLiteral("unchanged");
    cfg.pttPort = m_settings.pttPortName;
    cfg.pollIntervalMs = (m_spinPollMs != nullptr) ? m_spinPollMs->value() : 1000;
    cfg.txAudioRoute = (m_cmbTxAudioRoute != nullptr)
                           ? m_cmbTxAudioRoute->currentData().toString()
                           : QStringLiteral("default");
    if (cfg.txAudioRoute.isEmpty() || readOnlyTest) {
        cfg.txAudioRoute = QStringLiteral("default");
    }
    cfg.transmitAudioSource = (m_cmbTransmitAudioSource != nullptr)
                                  ? m_cmbTransmitAudioSource->currentData().toString()
                                  : QStringLiteral("rear_data");
    if (cfg.transmitAudioSource.isEmpty()) {
        cfg.transmitAudioSource = QStringLiteral("rear_data");
    }
    return cfg;
}

void RigControlSettingsDialog::setTestStatus(const QString &message, bool ok)
{
    if (m_lblTestStatus != nullptr) {
        m_lblTestStatus->setText(message);
        m_lblTestStatus->setStyleSheet(ok
            ? QStringLiteral("color: #118a2a; font-weight: bold;")
            : QStringLiteral("color: #b00020; font-weight: bold;"));
    }

    if (m_lblCatLed != nullptr) {
        const QString color = ok ? QStringLiteral("#18a340") : QStringLiteral("#c62828");
        m_lblCatLed->setStyleSheet(QStringLiteral(
            "background-color: %1; border: 1px solid #555; border-radius: 9px;")
            .arg(color));
        m_lblCatLed->setToolTip(ok ? L("CAT OK") : L("CAT failed or not tested"));
    }
}


void RigControlSettingsDialog::setExternalTestStatus(const QString &message, bool ok)
{
    setTestStatus(message, ok);
    refreshEnabledState();
}

void RigControlSettingsDialog::onTestCatClicked()
{
    if (m_btnTestCat != nullptr) m_btnTestCat->setEnabled(false);
    if (m_btnTestPttOn != nullptr) m_btnTestPttOn->setEnabled(false);
    if (m_btnTestPttOff != nullptr) m_btnTestPttOff->setEnabled(false);
    setTestStatus(L("Testing CAT frequency read..."), false);
    emit catTestRequested(configFromCurrentUi(true, false, true));
}

bool RigControlSettingsDialog::runPttTest(bool enabled)
{
    setTestStatus(enabled ? L("PTT test request queued...") : L("PTT release request queued..."), false);
    emit pttTestRequested(configFromCurrentUi(false, true), enabled);
    return true;
}

void RigControlSettingsDialog::onTestPttOnClicked()
{
    const int answer = QMessageBox::warning(this,
                                            L("Confirm PTT test"),
                                            L("This test will intentionally switch the radio to TX for a very short time, then force RX. Continue?"),
                                            QMessageBox::Yes | QMessageBox::No,
                                            QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        setTestStatus(L("PTT TX test cancelled — no TX command sent."), false);
        refreshEnabledState();
        return;
    }

    if (m_btnTestPttOn != nullptr) m_btnTestPttOn->setEnabled(false);
    setTestStatus(L("Testing CAT PTT TX..."), false);
    runPttTest(true);
    refreshEnabledState();
}

void RigControlSettingsDialog::onTestPttOffClicked()
{
    if (m_btnTestPttOff != nullptr) m_btnTestPttOff->setEnabled(false);
    setTestStatus(L("Sending CAT PTT OFF / RX..."), false);
    runPttTest(false);
    refreshEnabledState();
}

QString RigControlSettingsDialog::L(const QString &source) const
{
    if (m_textTranslator) {
        const QString translated = m_textTranslator(source);
        if (!translated.isEmpty()) {
            return translated;
        }
    }
    return source;
}
