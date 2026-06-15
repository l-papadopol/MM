#include "AppSettingsDialog.h"

#include "AudioSettingsDialog.h"
#include "../widgets/AutoQsoFlowEditorWidget.h"
#include "LogbookSettingsDialog.h"
#include "RigControlSettingsDialog.h"
#include "../rotator/RotatorPeakSearch.h"
#include "SoundCardCalibrationDialog.h"
#include "TextMacroSettingsDialog.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QGuiApplication>
#include <QGroupBox>
#include <QLabel>
#include <QIcon>
#include <QLineEdit>
#include <QListWidget>
#include <QMetaObject>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPixmap>
#include <QPainter>
#include <QPen>
#include <QProgressBar>
#include <QRegularExpression>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSet>
#include <QScreen>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
#include <QWindow>
#include <QVBoxLayout>
#include <QtGlobal>

#include <algorithm>
#include <utility>

#ifdef MADMODEM_WITH_HAMLIB
#include <hamlib/rotator.h>
#endif

namespace {

QString endpointKey(QString value)
{
    value = value.trimmed();
    if (value.isEmpty()) {
        return QString();
    }
    value.replace(QChar('\\'), QChar('/'));
    value = value.toLower();
    while (value.startsWith(QStringLiteral("serial:"))) {
        value = value.mid(7).trimmed();
    }
    while (value.startsWith(QStringLiteral("hamlib:"))) {
        value = value.mid(7).trimmed();
    }
    return value;
}

bool sameEndpoint(const QString &a, const QString &b)
{
    const QString ka = endpointKey(a);
    const QString kb = endpointKey(b);
    return !ka.isEmpty() && !kb.isEmpty() && ka == kb;
}


struct RotatorModelPreset
{
    int id = 1;
    QString manufacturer;
    QString model;
};

void addRotatorPresetIfUnique(QList<RotatorModelPreset> *presets, QSet<int> *seen, int id, const QString &manufacturer, const QString &model)
{
    if (presets == nullptr || seen == nullptr || id <= 0 || seen->contains(id)) {
        return;
    }
    RotatorModelPreset preset;
    preset.id = id;
    preset.manufacturer = manufacturer.trimmed().isEmpty() ? QStringLiteral("Other") : manufacturer.trimmed();
    preset.model = model.trimmed().isEmpty() ? QStringLiteral("Unknown rotator") : model.trimmed();
    presets->append(preset);
    seen->insert(id);
}

#ifdef MADMODEM_WITH_HAMLIB
struct HamlibRotatorPresetBuildContext
{
    QList<RotatorModelPreset> *presets = nullptr;
    QSet<int> *seen = nullptr;
};

int addHamlibRotatorCapsCallback(const struct rot_caps *caps, rig_ptr_t data)
{
    HamlibRotatorPresetBuildContext *ctx = static_cast<HamlibRotatorPresetBuildContext *>(data);
    if (caps == nullptr || ctx == nullptr || ctx->presets == nullptr || ctx->seen == nullptr) {
        return 1;
    }
    addRotatorPresetIfUnique(ctx->presets,
                             ctx->seen,
                             static_cast<int>(caps->rot_model),
                             QString::fromLocal8Bit(caps->mfg_name != nullptr ? caps->mfg_name : "Other"),
                             QString::fromLocal8Bit(caps->model_name != nullptr ? caps->model_name : "Unknown rotator"));
    return 1;
}
#endif

QList<RotatorModelPreset> fallbackRotatorPresets()
{
    QList<RotatorModelPreset> presets;
    QSet<int> seen;

    // Conservative fallback used when Hamlib rotator capability enumeration is
    // unavailable.  Normal Hamlib builds populate the complete backend list
    // through rot_list_foreach(), similar to the CAT radio model dialog.
    addRotatorPresetIfUnique(&presets, &seen, 1,   QStringLiteral("Hamlib"), QStringLiteral("Dummy rotator"));
    addRotatorPresetIfUnique(&presets, &seen, 2,   QStringLiteral("Hamlib"), QStringLiteral("Net rotctld"));

    addRotatorPresetIfUnique(&presets, &seen, 601, QStringLiteral("Yaesu"), QStringLiteral("G-601 / GS-232A azimuth"));
    addRotatorPresetIfUnique(&presets, &seen, 603, QStringLiteral("Yaesu"), QStringLiteral("GS-232B azimuth/elevation"));
    addRotatorPresetIfUnique(&presets, &seen, 611, QStringLiteral("Yaesu"), QStringLiteral("GS-232B azimuth"));
    addRotatorPresetIfUnique(&presets, &seen, 612, QStringLiteral("Yaesu"), QStringLiteral("GS-232B elevation"));

    addRotatorPresetIfUnique(&presets, &seen, 202, QStringLiteral("EasyComm"), QStringLiteral("EasyComm I compatible"));
    addRotatorPresetIfUnique(&presets, &seen, 204, QStringLiteral("EasyComm"), QStringLiteral("EasyComm II compatible"));
    addRotatorPresetIfUnique(&presets, &seen, 401, QStringLiteral("M2"), QStringLiteral("RC2800 / RC2800PX"));
    addRotatorPresetIfUnique(&presets, &seen, 501, QStringLiteral("HyGain"), QStringLiteral("DCU-1 / compatible"));
    addRotatorPresetIfUnique(&presets, &seen, 601, QStringLiteral("Yaesu"), QStringLiteral("GS-232A compatible"));
    addRotatorPresetIfUnique(&presets, &seen, 801, QStringLiteral("Prosistel"), QStringLiteral("D-series compatible"));
    addRotatorPresetIfUnique(&presets, &seen, 901, QStringLiteral("SPID"), QStringLiteral("Rot2Prog compatible"));
    addRotatorPresetIfUnique(&presets, &seen, 1001, QStringLiteral("AlfaSpid"), QStringLiteral("RAK / RAS compatible"));

    return presets;
}

QList<RotatorModelPreset> buildRotatorPresets()
{
    QList<RotatorModelPreset> presets;
    QSet<int> seen;

#ifdef MADMODEM_WITH_HAMLIB
    rig_set_debug(RIG_DEBUG_NONE);
    rot_load_all_backends();
    HamlibRotatorPresetBuildContext ctx;
    ctx.presets = &presets;
    ctx.seen = &seen;
    rot_list_foreach(addHamlibRotatorCapsCallback, &ctx);
#endif

    if (presets.isEmpty()) {
        presets = fallbackRotatorPresets();
        seen.clear();
        for (const RotatorModelPreset &preset : presets) {
            seen.insert(preset.id);
        }
    }

    // Always make the user's Yaesu G-601 / GS-232A easy to find even if a
    // particular Hamlib build reports a sparse or renamed rotator list.
    addRotatorPresetIfUnique(&presets, &seen, 601, QStringLiteral("Yaesu"), QStringLiteral("G-601 / GS-232A azimuth"));

    std::sort(presets.begin(), presets.end(), [](const RotatorModelPreset &a, const RotatorModelPreset &b) {
        const int makerCmp = QString::localeAwareCompare(a.manufacturer, b.manufacturer);
        if (makerCmp != 0) {
            return makerCmp < 0;
        }
        const int modelCmp = QString::localeAwareCompare(a.model, b.model);
        if (modelCmp != 0) {
            return modelCmp < 0;
        }
        return a.id < b.id;
    });

    return presets;
}

const QList<RotatorModelPreset> &allRotatorPresets()
{
    static const QList<RotatorModelPreset> presets = buildRotatorPresets();
    return presets;
}

void populateRotatorModelCombo(QComboBox *combo, int currentModel)
{
    if (combo == nullptr) return;
    combo->setEditable(true);
    combo->clear();

    int selected = -1;
    const QList<RotatorModelPreset> &presets = allRotatorPresets();
    for (const RotatorModelPreset &preset : presets) {
        const QString text = QStringLiteral("%1 %2 — %3")
            .arg(preset.id)
            .arg(preset.manufacturer, preset.model);
        combo->addItem(text, preset.id);
        if (preset.id == currentModel) selected = combo->count() - 1;
    }
    if (selected >= 0) {
        combo->setCurrentIndex(selected);
    } else {
        combo->setEditText(QString::number(qMax(1, currentModel)) + QStringLiteral(" Hamlib rotator model"));
    }
}

int rotatorModelFromCombo(const QComboBox *combo, int fallback)
{
    if (combo == nullptr) return qMax(1, fallback);
    const QVariant data = combo->currentData();
    if (data.isValid()) {
        bool ok = false;
        const int id = data.toInt(&ok);
        if (ok && id > 0) return id;
    }
    const QRegularExpression re(QStringLiteral("(\\d+)"));
    const QRegularExpressionMatch m = re.match(combo->currentText());
    if (m.hasMatch()) {
        bool ok = false;
        const int id = m.captured(1).toInt(&ok);
        if (ok && id > 0) return id;
    }
    return qMax(1, fallback);
}

void populateRotatorPortCombo(QComboBox *combo, const QString &current)
{
    if (combo == nullptr) return;
    combo->setEditable(true);
    combo->clear();
    QStringList ports;
    ports << current.trimmed()
          << QStringLiteral("/dev/ttyUSB0") << QStringLiteral("/dev/ttyUSB1")
          << QStringLiteral("/dev/ttyACM0") << QStringLiteral("/dev/ttyACM1")
          << QStringLiteral("COM1") << QStringLiteral("COM2") << QStringLiteral("COM3") << QStringLiteral("COM4")
          << QStringLiteral("COM5") << QStringLiteral("COM6") << QStringLiteral("COM7") << QStringLiteral("COM8")
          << QStringLiteral("rotctld:localhost:4533");
    QSet<QString> seen;
    for (const QString &port : ports) {
        const QString trimmed = port.trimmed();
        if (trimmed.isEmpty()) continue;
        const QString key = trimmed.toLower();
        if (seen.contains(key)) continue;
        seen.insert(key);
        combo->addItem(trimmed);
    }
    if (combo->count() == 0) combo->addItem(QString());
    const int idx = combo->findText(current.trimmed(), Qt::MatchFixedString);
    combo->setCurrentIndex(idx >= 0 ? idx : 0);
    combo->lineEdit()->setPlaceholderText(QObject::tr("independent rotator port, e.g. /dev/ttyUSB1, COM5 or rotctld:localhost:4533"));
}

QString rotatorBandObjectKey(QString band)
{
    band = band.trimmed();
    band.replace(QChar('/'), QChar('_'));
    band.replace(QChar(' '), QChar('_'));
    band.replace(QChar('.'), QChar('_'));
    return band;
}

QString rotatorBandDisplayName(const QString &band)
{
    if (band == QStringLiteral("10GHz")) return QStringLiteral("10 GHz");
    return band;
}

QVector<AppSettings::RotatorBandSettings> defaultRotatorBandRowsForUi()
{
    const QStringList bands = {
        QStringLiteral("40m"), QStringLiteral("30m"), QStringLiteral("20m"), QStringLiteral("17m"),
        QStringLiteral("15m"), QStringLiteral("12m"), QStringLiteral("10m"), QStringLiteral("6m"),
        QStringLiteral("4m"), QStringLiteral("2m"), QStringLiteral("70cm"), QStringLiteral("33cm"),
        QStringLiteral("23cm"), QStringLiteral("13cm"), QStringLiteral("9cm"), QStringLiteral("6cm"),
        QStringLiteral("3cm"), QStringLiteral("10GHz")
    };
    QVector<AppSettings::RotatorBandSettings> rows;
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

QVector<AppSettings::RotatorBandSettings> rotatorBandRowsForProfile(const AppSettings::RotatorProfileSettings &profile)
{
    return profile.bandSettings.isEmpty() ? defaultRotatorBandRowsForUi() : profile.bandSettings;
}

QString rotatorAxisModeLabel(bool useElevation)
{
    return useElevation ? QStringLiteral("AZ+EL") : QStringLiteral("AZ only");
}

void populatePeakAlgorithmList(QListWidget *list, bool useElevation, const QString &currentId)
{
    if (list == nullptr) return;
    list->clear();
    const auto axisMode = useElevation ? mm::RotatorPeakSearch::AxisMode::AzimuthElevation
                                       : mm::RotatorPeakSearch::AxisMode::AzimuthOnly;
    QString selectedId = currentId.trimmed();
    if (selectedId.isEmpty() || !mm::RotatorPeakSearch::isCompatible(selectedId, axisMode)) {
        selectedId = mm::RotatorPeakSearch::defaultAlgorithm(axisMode);
    }

    int selectedRow = -1;
    const QVector<mm::RotatorPeakSearch::AlgorithmInfo> algorithms = mm::RotatorPeakSearch::algorithms();
    for (const mm::RotatorPeakSearch::AlgorithmInfo &info : algorithms) {
        const bool compatible = mm::RotatorPeakSearch::isCompatible(info.id, axisMode);
        QListWidgetItem *item = new QListWidgetItem(info.displayName, list);
        item->setData(Qt::UserRole, info.id);
        QString suffix;
        if (info.continuousTracking) suffix += QStringLiteral(" • continuous");
        if (info.stochastic) suffix += QStringLiteral(" • stochastic");
        item->setText(info.displayName + suffix);
        item->setToolTip(info.description);
        if (!compatible) {
            Qt::ItemFlags flags = item->flags();
            flags &= ~Qt::ItemIsEnabled;
            flags &= ~Qt::ItemIsSelectable;
            item->setFlags(flags);
            QFont font = item->font();
            font.setStrikeOut(true);
            item->setFont(font);
            item->setForeground(Qt::gray);
            item->setToolTip(info.description + QStringLiteral("\nNot compatible with %1 rotator mode.").arg(rotatorAxisModeLabel(useElevation)));
        }
        if (compatible && info.id == selectedId) {
            selectedRow = list->row(item);
        }
    }
    if (selectedRow >= 0) {
        list->setCurrentRow(selectedRow);
    }
}

QString rotatorEndpointConflictText(const AppSettings &settings)
{
    if (!settings.rotatorEnabled) {
        return QString();
    }

    QStringList messages;
    for (int i = 0; i < 3; ++i) {
        const QString path = settings.rotatorProfiles[i].path.trimmed();
        if (path.isEmpty()) {
            continue;
        }
        QStringList conflicts;
        if (sameEndpoint(path, settings.hamlibSerialPath)) {
            conflicts << QStringLiteral("radio CAT serial port");
        }
        if (sameEndpoint(path, settings.hamlibRigPath)) {
            conflicts << QStringLiteral("radio CAT runtime path");
        }
        if (sameEndpoint(path, settings.hamlibTcpAddress)) {
            conflicts << QStringLiteral("radio CAT TCP endpoint");
        }
        if (sameEndpoint(path, settings.pttPortName)) {
            conflicts << QStringLiteral("serial PTT port");
        }
        conflicts.removeDuplicates();
        if (!conflicts.isEmpty()) {
            messages << QStringLiteral("Rotator %1 endpoint '%2' matches the %3")
                            .arg(i + 1)
                            .arg(path, conflicts.join(QStringLiteral(", ")));
        }
    }

    if (messages.isEmpty()) {
        return QString();
    }

    return messages.join(QStringLiteral("; ")) + QStringLiteral(". The CatRotator backend must use independent ports/endpoints from radio CAT/PTT, even when both use Hamlib. Use shared access only with an external multiplexer/proxy designed for it.");
}
QString colourName(const QColor &colour, const QString &fallback)
{
    return colour.isValid() ? colour.name(QColor::HexRgb).toUpper() : fallback;
}

QStringList callsFromText(const QString &text)
{
    QStringList out;
    const QStringList parts = text.split(QRegularExpression(QStringLiteral("[\\s,;]+")), Qt::SkipEmptyParts);
    for (QString part : parts) {
        part = part.trimmed().toUpper();
        part.remove(QRegularExpression(QStringLiteral("[^A-Z0-9/]+")));
        if (!part.isEmpty() && !out.contains(part)) {
            out << part;
        }
    }
    return out;
}

QString callsToText(const QStringList &calls)
{
    QStringList cleaned;
    for (QString call : calls) {
        call = call.trimmed().toUpper();
        call.remove(QRegularExpression(QStringLiteral("[^A-Z0-9/]+")));
        if (!call.isEmpty() && !cleaned.contains(call)) {
            cleaned << call;
        }
    }
    return cleaned.join(QStringLiteral("\n"));
}

void compactEmbeddedSettingsWidgets(QWidget *root)
{
    if (root == nullptr) {
        return;
    }

    const QList<QComboBox *> combos = root->findChildren<QComboBox *>();
    for (QComboBox *combo : combos) {
        if (combo == nullptr) {
            continue;
        }
        combo->setMinimumWidth(qMin(qMax(combo->minimumWidth(), 170), 260));
        combo->setMaximumWidth(16777215);
        combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    const QList<QLineEdit *> edits = root->findChildren<QLineEdit *>();
    for (QLineEdit *edit : edits) {
        if (edit == nullptr) {
            continue;
        }
        edit->setMinimumWidth(qMin(qMax(edit->minimumWidth(), 170), 280));
        edit->setMaximumWidth(16777215);
        edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    const QList<QSpinBox *> spinBoxes = root->findChildren<QSpinBox *>();
    for (QSpinBox *spin : spinBoxes) {
        if (spin == nullptr) {
            continue;
        }
        spin->setMaximumWidth(140);
        spin->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    }

    const QList<QDoubleSpinBox *> doubleSpinBoxes = root->findChildren<QDoubleSpinBox *>();
    for (QDoubleSpinBox *spin : doubleSpinBoxes) {
        if (spin == nullptr) {
            continue;
        }
        spin->setMaximumWidth(160);
        spin->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    }

    const QList<QProgressBar *> progressBars = root->findChildren<QProgressBar *>();
    for (QProgressBar *bar : progressBars) {
        if (bar == nullptr) {
            continue;
        }
        bar->setMaximumWidth(16777215);
        bar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    const QList<QPlainTextEdit *> plainTextEdits = root->findChildren<QPlainTextEdit *>();
    for (QPlainTextEdit *edit : plainTextEdits) {
        if (edit == nullptr) {
            continue;
        }
        edit->setMaximumWidth(16777215);
        edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    }

}

void hideEmbeddedIntroLabels(QWidget *root)
{
    if (root == nullptr) {
        return;
    }

    const QList<QLabel *> labels = root->findChildren<QLabel *>();
    for (QLabel *label : labels) {
        if (label == nullptr) {
            continue;
        }
        const QString text = label->text();
        if (text.startsWith(QStringLiteral("Configure persistent user")) ||
            text.startsWith(QStringLiteral("Daily UTC QSY plan"))) {
            label->hide();
        }
    }
}

} // namespace

AppSettingsDialog::AppSettingsDialog(const AppSettings &settings,
                                     const QString &currentLogbookPath,
                                     const QString &defaultLogbookPath,
                                     const QList<ScheduledQsyEntry> &schedulerEntries,
                                     TextTranslator textTranslator,
                                     SchedulerTranslator schedulerTranslator,
                                     InitialPage initialPage,
                                     QWidget *parent)
    : QDialog(parent),
      m_initialSettings(settings),
      m_resultSettings(settings),
      m_currentLogbookPath(currentLogbookPath),
      m_defaultLogbookPath(defaultLogbookPath),
      m_selectedLogbookPath(currentLogbookPath),
      m_initialSchedulerEntries(schedulerEntries),
      m_resultSchedulerEntries(schedulerEntries),
      m_textTranslator(std::move(textTranslator)),
      m_schedulerTranslator(std::move(schedulerTranslator))
{
    setWindowTitle(L(QStringLiteral("Settings")));
    resize(820, 588);
    setMinimumSize(760, 525);

    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(10, 10, 10, 10);
    outer->setSpacing(8);

    m_tabs = new QTabWidget(this);
    outer->addWidget(m_tabs, 1);

    m_tabs->addTab(makeAudioCatPage(), L(QStringLiteral("Audio / PTT / CAT")));

    m_textMacroPage = new TextMacroSettingsDialog(settings, this);
    hideEmbeddedIntroLabels(m_textMacroPage);
    m_tabs->addTab(embedDialogPage(m_textMacroPage), L(QStringLiteral("User / QTH / Macros")));

    m_tabs->addTab(makeLogbookPage(), L(QStringLiteral("Logbook / FT colours")));

    m_rotatorPage = makeRotatorPage();
    m_tabs->addTab(m_rotatorPage, L(QStringLiteral("Rotator")));

    m_autoQsoFlowTabIndex = m_tabs->addTab(makeAutoQsoFlowPage(), L(QStringLiteral("MM Flow Studio")));

    m_schedulerPage = new BandSchedulerDialog(schedulerEntries,
                                              m_schedulerTranslator,
                                              this);
    hideEmbeddedIntroLabels(m_schedulerPage);
    m_tabs->addTab(embedDialogPage(m_schedulerPage), L(QStringLiteral("Scheduler")));

    m_calibrationPage = new SoundCardCalibrationDialog(settings, this);
    m_calibrationPage->setTextTranslator([this](const QString &source) { return L(source); });
    m_tabs->addTab(embedDialogPage(m_calibrationPage), L(QStringLiteral("Soundcard calibration")));

    connect(m_tabs, &QTabWidget::currentChanged,
            this, &AppSettingsDialog::updateAutoQsoFlowWindowMode);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    if (QPushButton *ok = m_buttons->button(QDialogButtonBox::Ok)) {
        ok->setText(L(QStringLiteral("OK")));
    }
    if (QPushButton *cancel = m_buttons->button(QDialogButtonBox::Cancel)) {
        cancel->setText(L(QStringLiteral("Cancel")));
    }
    connect(m_buttons, &QDialogButtonBox::accepted, this, &AppSettingsDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &AppSettingsDialog::reject);
    outer->addWidget(m_buttons);

    setInitialPage(initialPage);
    QTimer::singleShot(0, this, [this]() {
        updateAutoQsoFlowWindowMode(m_tabs != nullptr ? m_tabs->currentIndex() : -1);
    });
}

void AppSettingsDialog::prepareEmbeddedDialog(QDialog *dialog)
{
    if (dialog == nullptr) {
        return;
    }

    dialog->setWindowFlags(Qt::Widget);
    dialog->setModal(false);
    dialog->setMinimumSize(0, 0);
    dialog->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    compactEmbeddedSettingsWidgets(dialog);

    const QList<QDialogButtonBox *> buttonBoxes = dialog->findChildren<QDialogButtonBox *>();
    for (QDialogButtonBox *box : buttonBoxes) {
        if (box != nullptr) {
            box->hide();
        }
    }
}

QWidget *AppSettingsDialog::embedDialogPage(QDialog *dialog)
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(8);

    if (dialog == nullptr) {
        layout->addStretch(1);
        return page;
    }

    prepareEmbeddedDialog(dialog);
    layout->addWidget(dialog, 1);
    return page;
}

QWidget *AppSettingsDialog::makeAudioCatPage()
{
    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    QWidget *holder = new QWidget(scroll);
    QVBoxLayout *layout = new QVBoxLayout(holder);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(10);

    QGroupBox *audioGroup = new QGroupBox(L(QStringLiteral("Audio / PTT")), holder);
    audioGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    QVBoxLayout *audioLayout = new QVBoxLayout(audioGroup);
    audioLayout->setContentsMargins(10, 8, 10, 10);
    audioLayout->setSpacing(0);
    m_audioPage = new AudioSettingsDialog(m_initialSettings, audioGroup);
    m_audioPage->setTextTranslator([this](const QString &source) { return L(source); });
    prepareEmbeddedDialog(m_audioPage);
    compactEmbeddedSettingsWidgets(m_audioPage);
    audioLayout->addWidget(m_audioPage);
    layout->addWidget(audioGroup);

    QGroupBox *catGroup = new QGroupBox(L(QStringLiteral("Radio / CAT")), holder);
    catGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    QVBoxLayout *catLayout = new QVBoxLayout(catGroup);
    catLayout->setContentsMargins(10, 8, 10, 10);
    catLayout->setSpacing(0);
    m_rigPage = new RigControlSettingsDialog(m_initialSettings, catGroup);
    m_rigPage->setTextTranslator([this](const QString &source) { return L(source); });
    connect(m_rigPage, &RigControlSettingsDialog::catTestRequested,
            this, &AppSettingsDialog::catTestRequested);
    connect(m_rigPage, &RigControlSettingsDialog::pttTestRequested,
            this, &AppSettingsDialog::pttTestRequested);
    prepareEmbeddedDialog(m_rigPage);
    compactEmbeddedSettingsWidgets(m_rigPage);
    catLayout->addWidget(m_rigPage);
    layout->addWidget(catGroup);
    layout->addStretch(1);

    scroll->setWidget(holder);
    return scroll;
}

QWidget *AppSettingsDialog::makeLogbookPage()
{
    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    QWidget *holder = new QWidget(scroll);
    QVBoxLayout *layout = new QVBoxLayout(holder);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(10);

    QGroupBox *logbookGroup = new QGroupBox(L(QStringLiteral("Logbook file")), holder);
    logbookGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    QVBoxLayout *logbookLayout = new QVBoxLayout(logbookGroup);
    logbookLayout->setContentsMargins(10, 8, 10, 10);
    logbookLayout->setSpacing(0);
    m_logbookPage = new LogbookSettingsDialog(m_currentLogbookPath, m_defaultLogbookPath, logbookGroup);
    m_logbookPage->setTextTranslator([this](const QString &source) { return L(source); });
    prepareEmbeddedDialog(m_logbookPage);
    compactEmbeddedSettingsWidgets(m_logbookPage);
    logbookLayout->addWidget(m_logbookPage);
    layout->addWidget(logbookGroup);

    QWidget *colourEditor = makeFtColourEditor();
    colourEditor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    layout->addWidget(colourEditor);
    layout->addStretch(1);

    scroll->setWidget(holder);
    return scroll;
}

QWidget *AppSettingsDialog::makeAutoQsoFlowPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(8);

    m_autoQsoFlowPage = new AutoQsoFlowEditorWidget(page);
    m_autoQsoFlowPage->setFlowJson(m_initialSettings.ftAutoQsoFlowJson);
    layout->addWidget(m_autoQsoFlowPage, 1);
    return page;
}

QWidget *AppSettingsDialog::makeFtColourEditor()
{
    QGroupBox *group = new QGroupBox(L(QStringLiteral("FT4 / FT8 decode highlighting")), this);
    QGridLayout *grid = new QGridLayout(group);
    grid->setContentsMargins(10, 10, 10, 10);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(8);

    auto addRow = [this, grid](int row,
                               const QString &label,
                               ColourButton *background,
                               const QString &backgroundValue,
                               ColourButton *foreground,
                               const QString &foregroundValue) {
        QLabel *caption = new QLabel(L(label), grid->parentWidget());
        caption->setMinimumWidth(190);
        grid->addWidget(caption, row, 0);

        QLabel *bgLabel = new QLabel(L(QStringLiteral("Background")), grid->parentWidget());
        grid->addWidget(bgLabel, row, 1);
        background->title = label + QStringLiteral(" — ") + QStringLiteral("Background");
        background->value = colourName(colourFromString(backgroundValue, QColor(Qt::white)), backgroundValue);
        background->button = new QPushButton(grid->parentWidget());
        background->button->setMinimumWidth(110);
        setColourButton(background->button, colourFromString(background->value, QColor(Qt::white)));
        connect(background->button, &QPushButton::clicked, this, [this, background]() { chooseColour(background); });
        grid->addWidget(background->button, row, 2);

        QLabel *fgLabel = new QLabel(L(QStringLiteral("Text")), grid->parentWidget());
        grid->addWidget(fgLabel, row, 3);
        foreground->title = label + QStringLiteral(" — ") + QStringLiteral("Text");
        foreground->value = colourName(colourFromString(foregroundValue, QColor(Qt::black)), foregroundValue);
        foreground->button = new QPushButton(grid->parentWidget());
        foreground->button->setMinimumWidth(110);
        setColourButton(foreground->button, colourFromString(foreground->value, QColor(Qt::black)));
        connect(foreground->button, &QPushButton::clicked, this, [this, foreground]() { chooseColour(foreground); });
        grid->addWidget(foreground->button, row, 4);
    };

    addRow(0, QStringLiteral("My call in message"),
           &m_colourMyCallBg, m_initialSettings.ftHighlightMyCallBackground,
           &m_colourMyCallFg, m_initialSettings.ftHighlightMyCallForeground);
    addRow(1, QStringLiteral("CQ message"),
           &m_colourCqBg, m_initialSettings.ftHighlightCqBackground,
           &m_colourCqFg, m_initialSettings.ftHighlightCqForeground);
    addRow(2, QStringLiteral("Worked before"),
           &m_colourWorkedBg, m_initialSettings.ftHighlightWorkedBackground,
           &m_colourWorkedFg, m_initialSettings.ftHighlightWorkedForeground);
    addRow(3, QStringLiteral("Local TX row"),
           &m_colourTxBg, m_initialSettings.ftHighlightTxBackground,
           &m_colourTxFg, m_initialSettings.ftHighlightTxForeground);

    m_chkHighlightNewCountry = new QCheckBox(L(QStringLiteral("Highlight new DXCC countries with a red row outline")), group);
    m_chkHighlightNewCountry->setChecked(m_initialSettings.ftHighlightNewCountryEnabled);
    grid->addWidget(m_chkHighlightNewCountry, 4, 0, 1, 5);

    m_chkWatchListIcon = new QCheckBox(L(QStringLiteral("Show alert icon for calls in watch list")), group);
    m_chkWatchListIcon->setChecked(m_initialSettings.ftWatchListIconEnabled);
    grid->addWidget(m_chkWatchListIcon, 5, 0, 1, 5);

    QLabel *blackLabel = new QLabel(L(QStringLiteral("Blacklist calls")), group);
    m_editFtBlacklist = new QPlainTextEdit(group);
    m_editFtBlacklist->setPlainText(callsToText(m_initialSettings.ftBlacklistCalls));
    m_editFtBlacklist->setPlaceholderText(L(QStringLiteral("one callsign per line; hidden from FT traffic and ignored by Evil Auto QSO")));
    m_editFtBlacklist->setMinimumHeight(78);
    m_editFtBlacklist->setMaximumHeight(96);
    grid->addWidget(blackLabel, 6, 0);
    grid->addWidget(m_editFtBlacklist, 6, 1, 1, 4);

    QLabel *watchLabel = new QLabel(L(QStringLiteral("Watch-list calls")), group);
    m_editFtWatchList = new QPlainTextEdit(group);
    m_editFtWatchList->setPlainText(callsToText(m_initialSettings.ftWatchListCalls));
    m_editFtWatchList->setPlaceholderText(L(QStringLiteral("one callsign per line; rows get an alert icon")));
    m_editFtWatchList->setMinimumHeight(78);
    m_editFtWatchList->setMaximumHeight(96);
    grid->addWidget(watchLabel, 7, 0);
    grid->addWidget(m_editFtWatchList, 7, 1, 1, 4);

    QLabel *autoPolicyLabel = new QLabel(L(QStringLiteral("Evil Auto QSO duplicate policy")), group);
    m_cmbAutoQsoDuplicatePolicy = new QComboBox(group);
    m_cmbAutoQsoDuplicatePolicy->addItem(L(QStringLiteral("Answer all non-blacklisted CQs")), QStringLiteral("none"));
    m_cmbAutoQsoDuplicatePolicy->addItem(L(QStringLiteral("Exclude calls already in log")), QStringLiteral("never_worked"));
    m_cmbAutoQsoDuplicatePolicy->addItem(L(QStringLiteral("Exclude calls worked in the last period")), QStringLiteral("recent"));
    int policyIndex = m_cmbAutoQsoDuplicatePolicy->findData(m_initialSettings.ftAutoQsoDuplicatePolicy);
    m_cmbAutoQsoDuplicatePolicy->setCurrentIndex(policyIndex >= 0 ? policyIndex : 1);
    m_spinAutoQsoRecentHours = new QSpinBox(group);
    m_spinAutoQsoRecentHours->setRange(1, 168);
    m_spinAutoQsoRecentHours->setSuffix(QStringLiteral(" h"));
    m_spinAutoQsoRecentHours->setValue(qBound(1, m_initialSettings.ftAutoQsoRecentHours, 168));
    grid->addWidget(autoPolicyLabel, 8, 0);
    grid->addWidget(m_cmbAutoQsoDuplicatePolicy, 8, 1, 1, 3);
    grid->addWidget(m_spinAutoQsoRecentHours, 8, 4);

    QLabel *cqRepeatLabel = new QLabel(L(QStringLiteral("CQ retry count")), group);
    m_spinCqRepeatCount = new QSpinBox(group);
    m_spinCqRepeatCount->setRange(1, 99);
    m_spinCqRepeatCount->setValue(qBound(1, m_initialSettings.ft8CqRepeatCount, 99));
    grid->addWidget(cqRepeatLabel, 9, 0);
    grid->addWidget(m_spinCqRepeatCount, 9, 1);

    m_chkAutoQsoFlowShadowMode = new QCheckBox(L(QStringLiteral("AutoQSO Flow shadow logging while Evil AutoQSO is armed")), group);
    m_chkAutoQsoFlowShadowMode->setToolTip(L(QStringLiteral("Runs the saved visual AutoQSO flow in read-only mode for each relevant FT decode. It writes [Flow][shadow] diagnostics to the runtime log but never transmits.")));
    m_chkAutoQsoFlowShadowMode->setChecked(m_initialSettings.ftAutoQsoFlowShadowMode);
    grid->addWidget(m_chkAutoQsoFlowShadowMode, 10, 0, 1, 5);

    QPushButton *reset = new QPushButton(L(QStringLiteral("Reset highlight colours")), group);
    connect(reset, &QPushButton::clicked, this, [this]() {
        const AppSettings defaults;
        m_colourMyCallBg.value = defaults.ftHighlightMyCallBackground;
        m_colourMyCallFg.value = defaults.ftHighlightMyCallForeground;
        m_colourCqBg.value = defaults.ftHighlightCqBackground;
        m_colourCqFg.value = defaults.ftHighlightCqForeground;
        m_colourWorkedBg.value = defaults.ftHighlightWorkedBackground;
        m_colourWorkedFg.value = defaults.ftHighlightWorkedForeground;
        m_colourTxBg.value = defaults.ftHighlightTxBackground;
        m_colourTxFg.value = defaults.ftHighlightTxForeground;
        const QList<ColourButton *> entries = {&m_colourMyCallBg, &m_colourMyCallFg,
                                               &m_colourCqBg, &m_colourCqFg,
                                               &m_colourWorkedBg, &m_colourWorkedFg,
                                               &m_colourTxBg, &m_colourTxFg};
        for (ColourButton *entry : entries) {
            if (entry != nullptr && entry->button != nullptr) {
                setColourButton(entry->button, colourFromString(entry->value, QColor(Qt::white)));
            }
        }
    });
    grid->addWidget(reset, 11, 0, 1, 5, Qt::AlignLeft);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(5, 2);
    return group;
}


QWidget *AppSettingsDialog::makeRotatorPage()
{
    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    QWidget *page = new QWidget(scroll);
    QVBoxLayout *outer = new QVBoxLayout(page);
    outer->setContentsMargins(12, 10, 12, 10);
    outer->setSpacing(10);

    QGroupBox *global = new QGroupBox(L(QStringLiteral("Module and tracking")), page);
    QGridLayout *globalGrid = new QGridLayout(global);
    m_chkRotatorEnabled = new QCheckBox(L(QStringLiteral("Enable CatRotator module")), global);
    m_chkRotatorEnabled->setChecked(m_initialSettings.rotatorEnabled);
    m_chkRotatorAutoConnect = new QCheckBox(L(QStringLiteral("Auto-connect selected rotator at startup")), global);
    m_chkRotatorAutoConnect->setChecked(m_initialSettings.rotatorAutoConnect);
    m_chkRotatorTrackSelectedQso = new QCheckBox(L(QStringLiteral("Track selected MM QSO correspondent")), global);
    m_chkRotatorTrackSelectedQso->setChecked(m_initialSettings.rotatorTrackSelectedQso);
    m_chkRotatorTrackOnlyQso = new QCheckBox(L(QStringLiteral("Only move while a QSO is active")), global);
    m_chkRotatorTrackOnlyQso->setChecked(m_initialSettings.rotatorTrackOnlyWhenQsoActive);
    globalGrid->addWidget(m_chkRotatorEnabled, 0, 0);
    globalGrid->addWidget(m_chkRotatorAutoConnect, 0, 1);
    globalGrid->addWidget(m_chkRotatorTrackSelectedQso, 1, 0);
    globalGrid->addWidget(m_chkRotatorTrackOnlyQso, 1, 1);
    outer->addWidget(global);

    QTabWidget *rotTabs = new QTabWidget(page);
    rotTabs->setDocumentMode(false);

    auto makeRotatorTab = [this](int index, QTabWidget *parentTabs) {
        const AppSettings::RotatorProfileSettings &profile = m_initialSettings.rotatorProfiles[index];
        QWidget *tab = new QWidget(parentTabs);
        QVBoxLayout *v = new QVBoxLayout(tab);
        v->setContentsMargins(10, 10, 10, 10);
        v->setSpacing(10);

        QGroupBox *connGroup = new QGroupBox(L(QStringLiteral("Connection")), tab);
        QGridLayout *conn = new QGridLayout(connGroup);
        QComboBox *model = new QComboBox(connGroup);
        model->setObjectName(QStringLiteral("rotatorModel_%1").arg(index));
        populateRotatorModelCombo(model, qMax(1, profile.hamlibModel));
        model->setToolTip(L(QStringLiteral("Select the Hamlib rotator model. The first number is the model ID passed to rot_init().")));
        QComboBox *path = new QComboBox(connGroup);
        path->setObjectName(QStringLiteral("rotatorPath_%1").arg(index));
        populateRotatorPortCombo(path, profile.path);
        QSpinBox *baud = new QSpinBox(connGroup);
        baud->setObjectName(QStringLiteral("rotatorBaud_%1").arg(index));
        baud->setRange(300, 1000000);
        baud->setValue(qBound(300, profile.baudRate, 1000000));
        conn->addWidget(new QLabel(L(QStringLiteral("Model")), connGroup), 0, 0);
        conn->addWidget(model, 0, 1, 1, 3);
        conn->addWidget(new QLabel(L(QStringLiteral("COM port / rotctld endpoint")), connGroup), 1, 0);
        conn->addWidget(path, 1, 1, 1, 3);
        conn->addWidget(new QLabel(L(QStringLiteral("Speed")), connGroup), 2, 0);
        conn->addWidget(baud, 2, 1);
        QLabel *endpointHint = new QLabel(L(QStringLiteral("Use a dedicated serial port such as /dev/ttyUSB1 or COM5, or a dedicated rotctld endpoint such as rotctld:localhost:4533.")), connGroup);
        endpointHint->setWordWrap(true);
        conn->addWidget(endpointHint, 3, 0, 1, 4);
        conn->setColumnStretch(1, 1);
        v->addWidget(connGroup);

        QGroupBox *settingsGroup = new QGroupBox(L(QStringLiteral("Settings")), tab);
        QGridLayout *settingsGrid = new QGridLayout(settingsGroup);
        QLineEdit *labelEdit = new QLineEdit(profile.label.trimmed().isEmpty() ? QStringLiteral("Rotator %1").arg(index + 1) : profile.label, settingsGroup);
        labelEdit->setObjectName(QStringLiteral("rotatorLabel_%1").arg(index));
        QDoubleSpinBox *parkAz = new QDoubleSpinBox(settingsGroup);
        parkAz->setObjectName(QStringLiteral("rotatorParkAz_%1").arg(index));
        parkAz->setRange(0.0, 359.9);
        parkAz->setDecimals(1);
        parkAz->setSuffix(QStringLiteral("°"));
        parkAz->setValue(qBound(0.0, profile.parkAzimuth, 359.9));
        QDoubleSpinBox *parkEl = new QDoubleSpinBox(settingsGroup);
        parkEl->setObjectName(QStringLiteral("rotatorParkEl_%1").arg(index));
        parkEl->setRange(-10.0, 180.0);
        parkEl->setDecimals(1);
        parkEl->setSuffix(QStringLiteral("°"));
        parkEl->setValue(qBound(-10.0, profile.parkElevation, 180.0));
        QComboBox *geometry = new QComboBox(settingsGroup);
        geometry->setObjectName(QStringLiteral("rotatorGeometryPreset_%1").arg(index));
        geometry->addItem(L(QStringLiteral("Standard 360°, stop at North (N-E-S-W-N)")), QStringLiteral("north-stop-360"));
        geometry->addItem(L(QStringLiteral("Standard 360°, stop at South (S-W-N-E-S)")), QStringLiteral("south-stop-360"));
        geometry->addItem(L(QStringLiteral("Yaesu 450° overlap (N-E-S-W-N-E)")), QStringLiteral("yaesu-450"));
        geometry->addItem(L(QStringLiteral("Custom mechanical range")), QStringLiteral("custom"));
        QString geometryPreset = profile.azimuthGeometryPreset.trimmed();
        if (geometryPreset.isEmpty()) geometryPreset = profile.overlap ? QStringLiteral("yaesu-450") : QStringLiteral("north-stop-360");
        int geometryIndex = geometry->findData(geometryPreset);
        if (geometryIndex < 0) geometryIndex = geometry->findData(QStringLiteral("custom"));
        geometry->setCurrentIndex(qMax(0, geometryIndex));
        geometry->setToolTip(L(QStringLiteral("Describe the real mechanical azimuth scale, including where the stop is and whether the rotor has 450 degree overlap.")));

        QCheckBox *overlap = new QCheckBox(L(QStringLiteral("Overlap / 450° rotator")), settingsGroup);
        overlap->setObjectName(QStringLiteral("rotatorOverlap_%1").arg(index));
        overlap->setChecked(profile.overlap || geometryPreset == QStringLiteral("yaesu-450"));
        QCheckBox *autoReverse = new QCheckBox(L(QStringLiteral("Auto-reverse if end-stop is detected")), settingsGroup);
        autoReverse->setObjectName(QStringLiteral("rotatorAutoReverseOnStall_%1").arg(index));
        autoReverse->setChecked(profile.autoReverseOnStall);
        autoReverse->setToolTip(L(QStringLiteral("If a movement command produces no appreciable position change, stop and retry using the other valid mechanical path.")));
        QCheckBox *useElevation = new QCheckBox(L(QStringLiteral("Use elevation axis")), settingsGroup);
        useElevation->setObjectName(QStringLiteral("rotatorUseElevation_%1").arg(index));
        useElevation->setChecked(profile.useElevation);
        QSpinBox *tolerance = new QSpinBox(settingsGroup);
        tolerance->setObjectName(QStringLiteral("rotatorTolerance_%1").arg(index));
        tolerance->setRange(0, 45);
        tolerance->setSuffix(QStringLiteral("°"));
        tolerance->setValue(qBound(0, profile.targetToleranceDeg, 45));
        QSpinBox *poll = new QSpinBox(settingsGroup);
        poll->setObjectName(QStringLiteral("rotatorPollMs_%1").arg(index));
        poll->setRange(250, 10000);
        poll->setSuffix(QStringLiteral(" ms"));
        poll->setValue(qBound(250, profile.pollIntervalMs, 10000));

        QDoubleSpinBox *azMin = new QDoubleSpinBox(settingsGroup);
        azMin->setObjectName(QStringLiteral("rotatorAzMin_%1").arg(index));
        azMin->setRange(-360.0, 540.0);
        azMin->setDecimals(1);
        azMin->setSuffix(QStringLiteral("°"));
        azMin->setValue(qBound(-360.0, profile.azimuthMinDeg, 540.0));
        QDoubleSpinBox *azMax = new QDoubleSpinBox(settingsGroup);
        azMax->setObjectName(QStringLiteral("rotatorAzMax_%1").arg(index));
        azMax->setRange(-359.0, 540.0);
        azMax->setDecimals(1);
        azMax->setSuffix(QStringLiteral("°"));
        azMax->setValue(qBound(1.0, profile.azimuthMaxDeg, 540.0));
        QDoubleSpinBox *elMin = new QDoubleSpinBox(settingsGroup);
        elMin->setObjectName(QStringLiteral("rotatorElMin_%1").arg(index));
        elMin->setRange(-10.0, 180.0);
        elMin->setDecimals(1);
        elMin->setSuffix(QStringLiteral("°"));
        elMin->setValue(qBound(-10.0, profile.elevationMinDeg, 180.0));
        QDoubleSpinBox *elMax = new QDoubleSpinBox(settingsGroup);
        elMax->setObjectName(QStringLiteral("rotatorElMax_%1").arg(index));
        elMax->setRange(-10.0, 180.0);
        elMax->setDecimals(1);
        elMax->setSuffix(QStringLiteral("°"));
        elMax->setValue(qBound(-10.0, profile.elevationMaxDeg, 180.0));
        QDoubleSpinBox *azMs = new QDoubleSpinBox(settingsGroup);
        azMs->setObjectName(QStringLiteral("rotatorAzMsPerDeg_%1").arg(index));
        azMs->setRange(0.0, 10000.0);
        azMs->setDecimals(1);
        azMs->setSuffix(QStringLiteral(" ms/°"));
        azMs->setValue(qMax(0.0, profile.azimuthMsPerDeg));
        QDoubleSpinBox *elMs = new QDoubleSpinBox(settingsGroup);
        elMs->setObjectName(QStringLiteral("rotatorElMsPerDeg_%1").arg(index));
        elMs->setRange(0.0, 10000.0);
        elMs->setDecimals(1);
        elMs->setSuffix(QStringLiteral(" ms/°"));
        elMs->setValue(qMax(0.0, profile.elevationMsPerDeg));
        QSpinBox *startupMs = new QSpinBox(settingsGroup);
        startupMs->setObjectName(QStringLiteral("rotatorStartupDelayMs_%1").arg(index));
        startupMs->setRange(0, 30000);
        startupMs->setSuffix(QStringLiteral(" ms"));
        startupMs->setValue(qBound(0, profile.startupDelayMs, 30000));
        QSpinBox *settleMs = new QSpinBox(settingsGroup);
        settleMs->setObjectName(QStringLiteral("rotatorSettleDelayMs_%1").arg(index));
        settleMs->setRange(0, 30000);
        settleMs->setSuffix(QStringLiteral(" ms"));
        settleMs->setValue(qBound(0, profile.settleDelayMs, 30000));
        QSpinBox *guardMs = new QSpinBox(settingsGroup);
        guardMs->setObjectName(QStringLiteral("rotatorTxGuardMarginMs_%1").arg(index));
        guardMs->setRange(0, 30000);
        guardMs->setSuffix(QStringLiteral(" ms"));
        guardMs->setValue(qBound(0, profile.txGuardMarginMs, 30000));
        QSpinBox *noMoveMs = new QSpinBox(settingsGroup);
        noMoveMs->setObjectName(QStringLiteral("rotatorNoMovementTimeoutMs_%1").arg(index));
        noMoveMs->setRange(500, 30000);
        noMoveMs->setSuffix(QStringLiteral(" ms"));
        noMoveMs->setValue(qBound(500, profile.noMovementTimeoutMs, 30000));
        QDoubleSpinBox *noMoveDeg = new QDoubleSpinBox(settingsGroup);
        noMoveDeg->setObjectName(QStringLiteral("rotatorNoMovementThresholdDeg_%1").arg(index));
        noMoveDeg->setRange(0.1, 30.0);
        noMoveDeg->setDecimals(1);
        noMoveDeg->setSuffix(QStringLiteral("°"));
        noMoveDeg->setValue(qBound(0.1, profile.noMovementThresholdDeg, 30.0));
        QDoubleSpinBox *stopDeg = new QDoubleSpinBox(settingsGroup);
        stopDeg->setObjectName(QStringLiteral("rotatorAzStopDeg_%1").arg(index));
        stopDeg->setRange(-360.0, 540.0);
        stopDeg->setDecimals(1);
        stopDeg->setSuffix(QStringLiteral("°"));
        stopDeg->setValue(qBound(-360.0, profile.azimuthStopDeg, 540.0));

        settingsGrid->addWidget(new QLabel(L(QStringLiteral("Label")), settingsGroup), 0, 0);
        settingsGrid->addWidget(labelEdit, 0, 1, 1, 3);
        settingsGrid->addWidget(new QLabel(L(QStringLiteral("Azimuth geometry")), settingsGroup), 1, 0);
        settingsGrid->addWidget(geometry, 1, 1, 1, 3);
        settingsGrid->addWidget(new QLabel(L(QStringLiteral("Az Park")), settingsGroup), 2, 0);
        settingsGrid->addWidget(parkAz, 2, 1);
        settingsGrid->addWidget(new QLabel(L(QStringLiteral("El Park")), settingsGroup), 2, 2);
        settingsGrid->addWidget(parkEl, 2, 3);
        settingsGrid->addWidget(overlap, 3, 0, 1, 2);
        settingsGrid->addWidget(useElevation, 3, 2, 1, 2);
        settingsGrid->addWidget(autoReverse, 4, 0, 1, 2);
        settingsGrid->addWidget(new QLabel(L(QStringLiteral("No movement")), settingsGroup), 4, 2);
        settingsGrid->addWidget(noMoveMs, 4, 3);
        settingsGrid->addWidget(new QLabel(L(QStringLiteral("Tracking tolerance")), settingsGroup), 5, 0);
        settingsGrid->addWidget(tolerance, 5, 1);
        settingsGrid->addWidget(new QLabel(L(QStringLiteral("Refresh rate")), settingsGroup), 5, 2);
        settingsGrid->addWidget(poll, 5, 3);
        settingsGrid->addWidget(new QLabel(L(QStringLiteral("Az min / max")), settingsGroup), 6, 0);
        settingsGrid->addWidget(azMin, 6, 1);
        settingsGrid->addWidget(azMax, 6, 2);
        settingsGrid->addWidget(new QLabel(L(QStringLiteral("Stop point")), settingsGroup), 6, 3);
        settingsGrid->addWidget(stopDeg, 6, 4);
        settingsGrid->addWidget(new QLabel(L(QStringLiteral("El min / max")), settingsGroup), 7, 0);
        settingsGrid->addWidget(elMin, 7, 1);
        settingsGrid->addWidget(elMax, 7, 2);
        settingsGrid->addWidget(new QLabel(L(QStringLiteral("Stall threshold")), settingsGroup), 7, 3);
        settingsGrid->addWidget(noMoveDeg, 7, 4);
        settingsGrid->addWidget(new QLabel(L(QStringLiteral("Az speed")), settingsGroup), 8, 0);
        settingsGrid->addWidget(azMs, 8, 1);
        settingsGrid->addWidget(new QLabel(L(QStringLiteral("El speed")), settingsGroup), 8, 2);
        settingsGrid->addWidget(elMs, 8, 3);
        settingsGrid->addWidget(new QLabel(L(QStringLiteral("Start delay")), settingsGroup), 9, 0);
        settingsGrid->addWidget(startupMs, 9, 1);
        settingsGrid->addWidget(new QLabel(L(QStringLiteral("Settle delay")), settingsGroup), 9, 2);
        settingsGrid->addWidget(settleMs, 9, 3);
        settingsGrid->addWidget(new QLabel(L(QStringLiteral("TX guard margin")), settingsGroup), 10, 0);
        settingsGrid->addWidget(guardMs, 10, 1);
        QLabel *calibStamp = new QLabel(profile.calibrationStampUtc.trimmed().isEmpty()
            ? L(QStringLiteral("Calibration: not yet run"))
            : L(QStringLiteral("Calibration: %1")).arg(profile.calibrationStampUtc), settingsGroup);
        calibStamp->setObjectName(QStringLiteral("rotatorCalibrationStamp_%1").arg(index));
        calibStamp->setWordWrap(true);
        QPushButton *autoCalAz = new QPushButton(L(QStringLiteral("Auto-calibrate Az")), settingsGroup);
        autoCalAz->setObjectName(QStringLiteral("rotatorAutoCalAz_%1").arg(index));
        autoCalAz->setToolTip(L(QStringLiteral("Apply the current rotator profile and start azimuth stop-to-stop calibration from the live Rotator module.")));
        QPushButton *autoCalEl = new QPushButton(L(QStringLiteral("Auto-calibrate El")), settingsGroup);
        autoCalEl->setObjectName(QStringLiteral("rotatorAutoCalEl_%1").arg(index));
        autoCalEl->setToolTip(L(QStringLiteral("Apply the current rotator profile and start elevation stop-to-stop calibration from the live Rotator module.")));
        settingsGrid->addWidget(calibStamp, 11, 0, 1, 5);
        settingsGrid->addWidget(autoCalAz, 12, 0, 1, 2);
        settingsGrid->addWidget(autoCalEl, 12, 2, 1, 2);
        settingsGrid->setColumnStretch(4, 1);
        connect(autoCalAz, &QPushButton::clicked, this, [this, index]() {
            collectSettings();
            emit rotatorCalibrationRequested(index, false);
        });
        connect(autoCalEl, &QPushButton::clicked, this, [this, index]() {
            collectSettings();
            emit rotatorCalibrationRequested(index, true);
        });
        auto applyGeometryPreset = [geometry, overlap, azMin, azMax, stopDeg](int comboIndex) {
            const QString preset = geometry->itemData(comboIndex).toString();
            const bool custom = (preset == QStringLiteral("custom"));
            if (preset == QStringLiteral("north-stop-360")) {
                overlap->setChecked(false);
                azMin->setValue(0.0);
                azMax->setValue(359.9);
                stopDeg->setValue(0.0);
            } else if (preset == QStringLiteral("south-stop-360")) {
                overlap->setChecked(false);
                azMin->setValue(-180.0);
                azMax->setValue(180.0);
                stopDeg->setValue(180.0);
            } else if (preset == QStringLiteral("yaesu-450")) {
                overlap->setChecked(true);
                azMin->setValue(0.0);
                azMax->setValue(450.0);
                stopDeg->setValue(450.0);
            }
            overlap->setEnabled(custom || preset == QStringLiteral("yaesu-450"));
            azMin->setEnabled(custom);
            azMax->setEnabled(custom);
            stopDeg->setEnabled(custom);
        };
        connect(geometry, QOverload<int>::of(&QComboBox::currentIndexChanged), this, applyGeometryPreset);
        applyGeometryPreset(geometry->currentIndex());
        v->addWidget(settingsGroup);

        QGroupBox *algorithmGroup = new QGroupBox(L(QStringLiteral("Auto peak search algorithm")), tab);
        QVBoxLayout *algorithmLayout = new QVBoxLayout(algorithmGroup);
        QLabel *algorithmHint = new QLabel(L(QStringLiteral("Only algorithms compatible with this rotator axis configuration are selectable; incompatible algorithms are shown struck through.")), algorithmGroup);
        algorithmHint->setWordWrap(true);
        QListWidget *algorithmList = new QListWidget(algorithmGroup);
        algorithmList->setObjectName(QStringLiteral("rotatorPeakAlgorithm_%1").arg(index));
        algorithmList->setAlternatingRowColors(true);
        algorithmList->setMaximumHeight(142);
        populatePeakAlgorithmList(algorithmList, profile.useElevation, profile.peakSearchAlgorithm);
        algorithmLayout->addWidget(algorithmHint);
        algorithmLayout->addWidget(algorithmList);
        connect(useElevation, &QCheckBox::toggled, this, [algorithmList, profile](bool checked) {
            QString current = profile.peakSearchAlgorithm;
            if (algorithmList->currentItem() != nullptr) {
                current = algorithmList->currentItem()->data(Qt::UserRole).toString();
            }
            populatePeakAlgorithmList(algorithmList, checked, current);
        });
        v->addWidget(algorithmGroup);

        QGroupBox *bandGroup = new QGroupBox(L(QStringLiteral("Band assignment and antenna search span")), tab);
        QGridLayout *bandGrid = new QGridLayout(bandGroup);
        bandGrid->setHorizontalSpacing(10);
        bandGrid->setVerticalSpacing(4);
        bandGrid->addWidget(new QLabel(L(QStringLiteral("Band")), bandGroup), 0, 0);
        bandGrid->addWidget(new QLabel(L(QStringLiteral("Use")), bandGroup), 0, 1);
        bandGrid->addWidget(new QLabel(L(QStringLiteral("Az search ±")), bandGroup), 0, 2);
        bandGrid->addWidget(new QLabel(L(QStringLiteral("El search ±")), bandGroup), 0, 3);
        bandGrid->addWidget(new QLabel(L(QStringLiteral("Auto peak")), bandGroup), 0, 4);
        const QVector<AppSettings::RotatorBandSettings> bandRows = rotatorBandRowsForProfile(profile);
        for (int rowIndex = 0; rowIndex < bandRows.size(); ++rowIndex) {
            const AppSettings::RotatorBandSettings &row = bandRows.at(rowIndex);
            QLabel *bandLabel = new QLabel(rotatorBandDisplayName(row.band), bandGroup);
            QCheckBox *useBand = new QCheckBox(bandGroup);
            useBand->setObjectName(QStringLiteral("rotatorBandUse_%1_%2").arg(index).arg(rowIndex));
            useBand->setChecked(row.enabled);
            useBand->setToolTip(L(QStringLiteral("Assign this operating band to this rotator.")));
            useBand->setProperty("band", row.band);
            QDoubleSpinBox *azSpan = new QDoubleSpinBox(bandGroup);
            azSpan->setObjectName(QStringLiteral("rotatorBandAzSpan_%1_%2").arg(index).arg(rowIndex));
            azSpan->setRange(0.0, 180.0);
            azSpan->setDecimals(1);
            azSpan->setSuffix(QStringLiteral("°"));
            azSpan->setValue(qBound(0.0, row.azimuthSearchSpanDeg, 180.0));
            azSpan->setToolTip(L(QStringLiteral("Search half-span around the theoretical bearing. A value of 30 means ±30 degrees.")));
            QDoubleSpinBox *elSpan = new QDoubleSpinBox(bandGroup);
            elSpan->setObjectName(QStringLiteral("rotatorBandElSpan_%1_%2").arg(index).arg(rowIndex));
            elSpan->setRange(0.0, 180.0);
            elSpan->setDecimals(1);
            elSpan->setSuffix(QStringLiteral("°"));
            elSpan->setValue(qBound(0.0, row.elevationSearchSpanDeg, 180.0));
            elSpan->setEnabled(useElevation->isChecked());
            elSpan->setToolTip(L(QStringLiteral("Elevation search half-span; disabled for azimuth-only rotators.")));
            QCheckBox *autoPeak = new QCheckBox(bandGroup);
            autoPeak->setObjectName(QStringLiteral("rotatorBandAutoPeak_%1_%2").arg(index).arg(rowIndex));
            autoPeak->setChecked(row.autoPeakEnabled);
            autoPeak->setToolTip(L(QStringLiteral("Allow automatic signal peak search on this band while tracking a QSO.")));
            connect(useElevation, &QCheckBox::toggled, elSpan, &QWidget::setEnabled);
            const int uiRow = rowIndex + 1;
            bandGrid->addWidget(bandLabel, uiRow, 0);
            bandGrid->addWidget(useBand, uiRow, 1, Qt::AlignCenter);
            bandGrid->addWidget(azSpan, uiRow, 2);
            bandGrid->addWidget(elSpan, uiRow, 3);
            bandGrid->addWidget(autoPeak, uiRow, 4, Qt::AlignCenter);
        }
        bandGrid->setColumnStretch(0, 1);
        v->addWidget(bandGroup);

        QLabel *bandHint = new QLabel(L(QStringLiteral("Band assignment chooses which rotator MM uses for the current operating band. Empty assignment keeps that rotator manual-only.")), tab);
        bandHint->setWordWrap(true);
        v->addWidget(bandHint);
        v->addStretch(1);

        connect(path, &QComboBox::currentTextChanged, this, &AppSettingsDialog::updateRotatorEndpointWarning);
        parentTabs->addTab(tab, L(QStringLiteral("Rotator %1")).arg(index + 1));
    };

    for (int i = 0; i < 3; ++i) {
        makeRotatorTab(i, rotTabs);
    }
    rotTabs->setCurrentIndex(qBound(0, m_initialSettings.rotatorActiveProfile, 2));
    outer->addWidget(rotTabs, 1);

    m_lblRotatorEndpointWarning = new QLabel(page);
    m_lblRotatorEndpointWarning->setWordWrap(true);
    m_lblRotatorEndpointWarning->setStyleSheet(QStringLiteral("QLabel { color: #b00020; font-weight: 600; }"));
    m_lblRotatorEndpointWarning->setVisible(false);
    outer->addWidget(m_lblRotatorEndpointWarning);

    connect(m_chkRotatorEnabled, &QCheckBox::toggled, this, &AppSettingsDialog::updateRotatorEndpointWarning);
    updateRotatorEndpointWarning();

    scroll->setWidget(page);
    return scroll;
}
void AppSettingsDialog::setInitialPage(InitialPage page)
{
    if (m_tabs == nullptr) {
        return;
    }

    int index = 0;
    switch (page) {
    case InitialPage::AudioPtt: index = 0; break;
    case InitialPage::RadioCat: index = 0; break;
    case InitialPage::UserQthMacros: index = 1; break;
    case InitialPage::Logbook: index = 2; break;
    case InitialPage::Rotator: index = 3; break;
    case InitialPage::Scheduler: index = 5; break;
    case InitialPage::AutoQsoFlow: index = m_autoQsoFlowTabIndex; break;
    case InitialPage::SoundCardCalibration: index = 6; break;
    }
    if (index >= 0 && index < m_tabs->count()) {
        m_tabs->setCurrentIndex(index);
    }
}

void AppSettingsDialog::updateAutoQsoFlowWindowMode(int index)
{
    if (m_tabs == nullptr || m_autoQsoFlowTabIndex < 0) {
        return;
    }

    if (index == m_autoQsoFlowTabIndex) {
        expandForAutoQsoFlow();
    } else {
        restoreAfterAutoQsoFlow();
    }
}

void AppSettingsDialog::expandForAutoQsoFlow()
{
    if (m_autoQsoFlowWindowExpanded) {
        return;
    }

    m_autoQsoFlowWindowExpanded = true;
    m_preAutoQsoFlowGeometry = geometry();
    m_preAutoQsoFlowWindowState = windowState();
    m_preAutoQsoFlowMinimumSize = minimumSize();

    auto applyLargeGeometry = [this]() {
        QScreen *screen = nullptr;
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
        screen = QGuiApplication::screenAt(mapToGlobal(rect().center()));
#endif
        if (screen == nullptr && windowHandle() != nullptr) {
            screen = windowHandle()->screen();
        }
        if (screen == nullptr) {
            screen = QGuiApplication::primaryScreen();
        }

        QRect target;
        if (screen != nullptr) {
            target = screen->availableGeometry().adjusted(14, 14, -14, -14);
        }
        if (!target.isValid()) {
            target = QRect(40, 40, 1180, 760);
        }

        const QSize flowMinimum(qMin(1180, target.width()),
                                qMin(760, target.height()));
        setMinimumSize(qMax(760, flowMinimum.width()),
                       qMax(500, flowMinimum.height()));

        Qt::WindowStates state = windowState();
        state &= ~Qt::WindowMinimized;
        state &= ~Qt::WindowMaximized;
        state &= ~Qt::WindowFullScreen;
        setWindowState(state);
        showNormal();
        setGeometry(target);
        raise();
        activateWindow();
    };

    if (isVisible()) {
        applyLargeGeometry();
    } else {
        QTimer::singleShot(0, this, applyLargeGeometry);
    }
}

void AppSettingsDialog::restoreAfterAutoQsoFlow()
{
    if (!m_autoQsoFlowWindowExpanded) {
        return;
    }

    m_autoQsoFlowWindowExpanded = false;

    if (m_preAutoQsoFlowMinimumSize.isValid()) {
        setMinimumSize(m_preAutoQsoFlowMinimumSize);
    } else {
        setMinimumSize(760, 525);
    }

    Qt::WindowStates state = m_preAutoQsoFlowWindowState;
    state &= ~Qt::WindowMinimized;
    showNormal();
    if (m_preAutoQsoFlowGeometry.isValid()) {
        setGeometry(m_preAutoQsoFlowGeometry);
    } else {
        resize(820, 588);
    }
    setWindowState(state);
}

QString AppSettingsDialog::L(const QString &source) const
{
    if (m_textTranslator) {
        const QString translated = m_textTranslator(source);
        if (!translated.isEmpty()) {
            return translated;
        }
    }
    return source;
}

QColor AppSettingsDialog::colourFromString(const QString &name, const QColor &fallback) const
{
    QColor colour(name);
    return colour.isValid() ? colour : fallback;
}

void AppSettingsDialog::setColourButton(QPushButton *button, const QColor &colour)
{
    if (button == nullptr) {
        return;
    }
    const QString name = colourName(colour, QStringLiteral("#FFFFFF"));

    QPixmap swatch(34, 18);
    swatch.fill(Qt::transparent);
    {
        QPainter painter(&swatch);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(QPen(QColor(45, 45, 45), 1));
        painter.setBrush(colour.isValid() ? colour : QColor(Qt::gray));
        painter.drawRoundedRect(swatch.rect().adjusted(1, 1, -2, -2), 3, 3);
    }

    button->setText(QStringLiteral("  %1").arg(name));
    button->setIcon(QIcon(swatch));
    button->setIconSize(swatch.size());
    button->setMinimumWidth(128);
    button->setStyleSheet(QStringLiteral(
        "QPushButton { text-align: left; padding-left: 6px; font-family: monospace; }"
        "QPushButton:disabled { color: palette(disabled, text); }"));
    button->setToolTip(name);
}

void AppSettingsDialog::chooseColour(ColourButton *entry)
{
    if (entry == nullptr || entry->button == nullptr) {
        return;
    }
    const QColor current = colourFromString(entry->value, QColor(Qt::white));
    const QColor chosen = QColorDialog::getColor(current,
                                                  this,
                                                  L(QStringLiteral("Choose colour")) + QStringLiteral(" — ") + L(entry->title));
    if (!chosen.isValid()) {
        return;
    }
    entry->value = colourName(chosen, entry->value);
    setColourButton(entry->button, chosen);
}

void AppSettingsDialog::setExternalCatTestStatus(const QString &message, bool ok)
{
    if (m_rigPage != nullptr) {
        m_rigPage->setExternalTestStatus(message, ok);
    }
}

void AppSettingsDialog::setRotatorCalibrationResult(int profileIndex,
                                                    double azimuthMsPerDeg,
                                                    double elevationMsPerDeg,
                                                    const QString &stampUtc,
                                                    const QString &message)
{
    const int i = qBound(0, profileIndex, 2);
    if (m_rotatorPage == nullptr) {
        return;
    }
    if (QDoubleSpinBox *az = m_rotatorPage->findChild<QDoubleSpinBox *>(QStringLiteral("rotatorAzMsPerDeg_%1").arg(i))) {
        az->setValue(qMax(0.0, azimuthMsPerDeg));
    }
    if (QDoubleSpinBox *el = m_rotatorPage->findChild<QDoubleSpinBox *>(QStringLiteral("rotatorElMsPerDeg_%1").arg(i))) {
        el->setValue(qMax(0.0, elevationMsPerDeg));
    }
    if (QLabel *label = m_rotatorPage->findChild<QLabel *>(QStringLiteral("rotatorCalibrationStamp_%1").arg(i))) {
        const QString cleanMessage = message.trimmed();
        if (!cleanMessage.isEmpty()) {
            label->setText(cleanMessage);
        } else if (!stampUtc.trimmed().isEmpty()) {
            label->setText(L(QStringLiteral("Calibration: %1")).arg(stampUtc.trimmed()));
        } else {
            label->setText(L(QStringLiteral("Calibration: not yet run")));
        }
    }
    collectSettings();
}

void AppSettingsDialog::collectSettings()
{
    AppSettings merged = m_initialSettings;

    if (m_audioPage != nullptr) {
        const AppSettings audio = m_audioPage->settings();
        merged.audioInputName = audio.audioInputName;
        merged.audioOutputName = audio.audioOutputName;
        merged.audioSampleRate = audio.audioSampleRate;
        merged.pttPortName = audio.pttPortName;
        merged.pttMethod = audio.pttMethod;
        merged.hamlibPttEnabled = audio.hamlibPttEnabled;
    }

    if (m_calibrationPage != nullptr) {
        const AppSettings cal = m_calibrationPage->settings();
        merged.audioRxClockPpm = cal.audioRxClockPpm;
        merged.audioTxClockPpm = cal.audioTxClockPpm;
    }

    if (m_textMacroPage != nullptr) {
        // TextMacroSettingsDialog commits the editable fields in accept().
        QMetaObject::invokeMethod(m_textMacroPage, "accept", Qt::DirectConnection);
        const AppSettings text = m_textMacroPage->settings();
        merged.textMyCallsign = text.textMyCallsign;
        merged.textMyName = text.textMyName;
        merged.textMyQth = text.textMyQth;
        merged.textMyLocator = text.textMyLocator;
        merged.textRig = text.textRig;
        merged.textAntenna = text.textAntenna;
        merged.textPower = text.textPower;
        merged.textMacroLabels = text.textMacroLabels;
        merged.textMacroTexts = text.textMacroTexts;
        merged.ft8MyCallsign = text.ft8MyCallsign;
        merged.ft8MyGrid = text.ft8MyGrid;
    }

    if (m_rigPage != nullptr) {
        // RigControlSettingsDialog also commits via its private slot before its own accept().
        QMetaObject::invokeMethod(m_rigPage, "applyToSettings", Qt::DirectConnection);
        const AppSettings rig = m_rigPage->settings();
        merged.hamlibCatEnabled = rig.hamlibCatEnabled;
        merged.hamlibUpdateFt8Band = rig.hamlibUpdateFt8Band;
        merged.hamlibRigModel = rig.hamlibRigModel;
        merged.hamlibRigPath = rig.hamlibRigPath;
        merged.hamlibSerialPath = rig.hamlibSerialPath;
        merged.hamlibTcpAddress = rig.hamlibTcpAddress;
        merged.hamlibBaudRate = rig.hamlibBaudRate;
        merged.hamlibDataBits = rig.hamlibDataBits;
        merged.hamlibStopBits = rig.hamlibStopBits;
        merged.hamlibHandshake = rig.hamlibHandshake;
        merged.hamlibForceDtr = rig.hamlibForceDtr;
        merged.hamlibForceRts = rig.hamlibForceRts;
        merged.hamlibPollIntervalMs = rig.hamlibPollIntervalMs;
        merged.hamlibTxAudioRoute = rig.hamlibTxAudioRoute;
        merged.hamlibTransmitAudioSource = rig.hamlibTransmitAudioSource;
        // Radio/CAT owns the CAT-Hamlib PTT case; Audio/PTT owns serial/none.
        if (rig.pttMethod == QStringLiteral("cat_hamlib") || merged.pttMethod == QStringLiteral("cat_hamlib")) {
            merged.pttMethod = rig.pttMethod;
            merged.hamlibPttEnabled = rig.hamlibPttEnabled;
        }
    }

    if (m_logbookPage != nullptr) {
        m_selectedLogbookPath = m_logbookPage->selectedPath().trimmed();
        merged.logbookFilePath = m_selectedLogbookPath;
    }

    if (m_schedulerPage != nullptr) {
        m_resultSchedulerEntries = m_schedulerPage->entries();
        merged.schedulerQsyPlanJson = BandSchedulerDialog::serializeEntries(m_resultSchedulerEntries);
    }

    merged.ftHighlightMyCallBackground = m_colourMyCallBg.value;
    merged.ftHighlightMyCallForeground = m_colourMyCallFg.value;
    merged.ftHighlightCqBackground = m_colourCqBg.value;
    merged.ftHighlightCqForeground = m_colourCqFg.value;
    merged.ftHighlightWorkedBackground = m_colourWorkedBg.value;
    merged.ftHighlightWorkedForeground = m_colourWorkedFg.value;
    merged.ftHighlightTxBackground = m_colourTxBg.value;
    merged.ftHighlightTxForeground = m_colourTxFg.value;
    merged.ftHighlightNewCountryEnabled = (m_chkHighlightNewCountry != nullptr) ? m_chkHighlightNewCountry->isChecked() : merged.ftHighlightNewCountryEnabled;
    merged.ftWatchListIconEnabled = (m_chkWatchListIcon != nullptr) ? m_chkWatchListIcon->isChecked() : merged.ftWatchListIconEnabled;
    if (m_editFtBlacklist != nullptr) {
        merged.ftBlacklistCalls = callsFromText(m_editFtBlacklist->toPlainText());
    }
    if (m_editFtWatchList != nullptr) {
        merged.ftWatchListCalls = callsFromText(m_editFtWatchList->toPlainText());
    }
    if (m_cmbAutoQsoDuplicatePolicy != nullptr) {
        merged.ftAutoQsoDuplicatePolicy = m_cmbAutoQsoDuplicatePolicy->currentData().toString();
    }
    if (m_spinAutoQsoRecentHours != nullptr) {
        merged.ftAutoQsoRecentHours = m_spinAutoQsoRecentHours->value();
    }
    if (m_spinCqRepeatCount != nullptr) {
        merged.ft8CqRepeatCount = m_spinCqRepeatCount->value();
    }
    if (m_chkAutoQsoFlowShadowMode != nullptr) {
        merged.ftAutoQsoFlowShadowMode = m_chkAutoQsoFlowShadowMode->isChecked();
    }
    if (m_autoQsoFlowPage != nullptr) {
        merged.ftAutoQsoFlowJson = m_autoQsoFlowPage->flowJson();
    }

    if (m_chkRotatorEnabled != nullptr) merged.rotatorEnabled = m_chkRotatorEnabled->isChecked();
    if (m_chkRotatorAutoConnect != nullptr) merged.rotatorAutoConnect = m_chkRotatorAutoConnect->isChecked();
    merged.rotatorShowWindowOnStart = false;
    if (m_chkRotatorTrackSelectedQso != nullptr) merged.rotatorTrackSelectedQso = m_chkRotatorTrackSelectedQso->isChecked();
    if (m_chkRotatorTrackOnlyQso != nullptr) merged.rotatorTrackOnlyWhenQsoActive = m_chkRotatorTrackOnlyQso->isChecked();

    for (int i = 0; i < 3; ++i) {
        auto findLine = [this, i](const QString &name) { return m_rotatorPage != nullptr ? m_rotatorPage->findChild<QLineEdit *>(QStringLiteral("%1_%2").arg(name).arg(i)) : nullptr; };
        auto findCombo = [this, i](const QString &name) { return m_rotatorPage != nullptr ? m_rotatorPage->findChild<QComboBox *>(QStringLiteral("%1_%2").arg(name).arg(i)) : nullptr; };
        auto findSpin = [this, i](const QString &name) { return m_rotatorPage != nullptr ? m_rotatorPage->findChild<QSpinBox *>(QStringLiteral("%1_%2").arg(name).arg(i)) : nullptr; };
        auto findDSpin = [this, i](const QString &name) { return m_rotatorPage != nullptr ? m_rotatorPage->findChild<QDoubleSpinBox *>(QStringLiteral("%1_%2").arg(name).arg(i)) : nullptr; };
        auto findCheck = [this, i](const QString &name) { return m_rotatorPage != nullptr ? m_rotatorPage->findChild<QCheckBox *>(QStringLiteral("%1_%2").arg(name).arg(i)) : nullptr; };

        AppSettings::RotatorProfileSettings &rp = merged.rotatorProfiles[i];
        if (QLineEdit *w = findLine(QStringLiteral("rotatorLabel"))) rp.label = w->text().trimmed();
        if (rp.label.isEmpty()) rp.label = QStringLiteral("Rotator %1").arg(i + 1);
        if (QListWidget *w = (m_rotatorPage != nullptr ? m_rotatorPage->findChild<QListWidget *>(QStringLiteral("rotatorPeakAlgorithm_%1").arg(i)) : nullptr)) {
            if (w->currentItem() != nullptr && (w->currentItem()->flags() & Qt::ItemIsEnabled)) {
                rp.peakSearchAlgorithm = w->currentItem()->data(Qt::UserRole).toString().trimmed();
            }
        }
        if (QComboBox *w = findCombo(QStringLiteral("rotatorModel"))) rp.hamlibModel = rotatorModelFromCombo(w, rp.hamlibModel);
        if (QComboBox *w = findCombo(QStringLiteral("rotatorGeometryPreset"))) rp.azimuthGeometryPreset = w->currentData().toString().trimmed();
        if (rp.azimuthGeometryPreset.isEmpty()) rp.azimuthGeometryPreset = rp.overlap ? QStringLiteral("yaesu-450") : QStringLiteral("north-stop-360");
        if (QComboBox *w = findCombo(QStringLiteral("rotatorPath"))) rp.path = w->currentText().trimmed();
        if (QSpinBox *w = findSpin(QStringLiteral("rotatorBaud"))) rp.baudRate = w->value();
        if (QSpinBox *w = findSpin(QStringLiteral("rotatorPollMs"))) rp.pollIntervalMs = w->value();
        if (QCheckBox *w = findCheck(QStringLiteral("rotatorUseElevation"))) rp.useElevation = w->isChecked();
        if (QCheckBox *w = findCheck(QStringLiteral("rotatorOverlap"))) rp.overlap = w->isChecked();
        if (QCheckBox *w = findCheck(QStringLiteral("rotatorAutoReverseOnStall"))) rp.autoReverseOnStall = w->isChecked();
        if (QDoubleSpinBox *w = findDSpin(QStringLiteral("rotatorAzStopDeg"))) rp.azimuthStopDeg = w->value();
        if (QSpinBox *w = findSpin(QStringLiteral("rotatorNoMovementTimeoutMs"))) rp.noMovementTimeoutMs = w->value();
        if (QDoubleSpinBox *w = findDSpin(QStringLiteral("rotatorNoMovementThresholdDeg"))) rp.noMovementThresholdDeg = w->value();
        if (QDoubleSpinBox *w = findDSpin(QStringLiteral("rotatorParkAz"))) rp.parkAzimuth = w->value();
        if (QDoubleSpinBox *w = findDSpin(QStringLiteral("rotatorParkEl"))) rp.parkElevation = w->value();
        if (QSpinBox *w = findSpin(QStringLiteral("rotatorTolerance"))) rp.targetToleranceDeg = w->value();
        if (QDoubleSpinBox *w = findDSpin(QStringLiteral("rotatorAzMin"))) rp.azimuthMinDeg = w->value();
        if (QDoubleSpinBox *w = findDSpin(QStringLiteral("rotatorAzMax"))) rp.azimuthMaxDeg = w->value();
        if (QDoubleSpinBox *w = findDSpin(QStringLiteral("rotatorElMin"))) rp.elevationMinDeg = w->value();
        if (QDoubleSpinBox *w = findDSpin(QStringLiteral("rotatorElMax"))) rp.elevationMaxDeg = w->value();
        if (QDoubleSpinBox *w = findDSpin(QStringLiteral("rotatorAzMsPerDeg"))) rp.azimuthMsPerDeg = w->value();
        if (QDoubleSpinBox *w = findDSpin(QStringLiteral("rotatorElMsPerDeg"))) rp.elevationMsPerDeg = w->value();
        if (QSpinBox *w = findSpin(QStringLiteral("rotatorStartupDelayMs"))) rp.startupDelayMs = w->value();
        if (QSpinBox *w = findSpin(QStringLiteral("rotatorSettleDelayMs"))) rp.settleDelayMs = w->value();
        if (QSpinBox *w = findSpin(QStringLiteral("rotatorTxGuardMarginMs"))) rp.txGuardMarginMs = w->value();

        QVector<AppSettings::RotatorBandSettings> rows = rotatorBandRowsForProfile(rp);
        QStringList enabledBands;
        for (int rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
            AppSettings::RotatorBandSettings &row = rows[rowIndex];
            if (QCheckBox *w = (m_rotatorPage != nullptr ? m_rotatorPage->findChild<QCheckBox *>(QStringLiteral("rotatorBandUse_%1_%2").arg(i).arg(rowIndex)) : nullptr)) {
                row.enabled = w->isChecked();
            }
            if (QDoubleSpinBox *w = (m_rotatorPage != nullptr ? m_rotatorPage->findChild<QDoubleSpinBox *>(QStringLiteral("rotatorBandAzSpan_%1_%2").arg(i).arg(rowIndex)) : nullptr)) {
                row.azimuthSearchSpanDeg = w->value();
            }
            if (QDoubleSpinBox *w = (m_rotatorPage != nullptr ? m_rotatorPage->findChild<QDoubleSpinBox *>(QStringLiteral("rotatorBandElSpan_%1_%2").arg(i).arg(rowIndex)) : nullptr)) {
                row.elevationSearchSpanDeg = w->value();
            }
            if (QCheckBox *w = (m_rotatorPage != nullptr ? m_rotatorPage->findChild<QCheckBox *>(QStringLiteral("rotatorBandAutoPeak_%1_%2").arg(i).arg(rowIndex)) : nullptr)) {
                row.autoPeakEnabled = w->isChecked();
            }
            if (row.enabled) {
                enabledBands << row.band;
            }
        }
        rp.bandSettings = rows;
        rp.bandsCsv = enabledBands.join(QStringLiteral(","));
        const auto axisMode = rp.useElevation ? mm::RotatorPeakSearch::AxisMode::AzimuthElevation
                                              : mm::RotatorPeakSearch::AxisMode::AzimuthOnly;
        if (!mm::RotatorPeakSearch::isCompatible(rp.peakSearchAlgorithm, axisMode)) {
            rp.peakSearchAlgorithm = mm::RotatorPeakSearch::defaultAlgorithm(axisMode);
        }
    }

    // Keep legacy aliases synchronized with profile 1.
    merged.rotatorHamlibModel = merged.rotatorProfiles[0].hamlibModel;
    merged.rotatorPath = merged.rotatorProfiles[0].path;
    merged.rotatorBaudRate = merged.rotatorProfiles[0].baudRate;
    merged.rotatorPollIntervalMs = merged.rotatorProfiles[0].pollIntervalMs;
    merged.rotatorUseElevation = merged.rotatorProfiles[0].useElevation;
    merged.rotatorParkAzimuth = merged.rotatorProfiles[0].parkAzimuth;
    merged.rotatorParkElevation = merged.rotatorProfiles[0].parkElevation;
    merged.rotatorTargetToleranceDeg = merged.rotatorProfiles[0].targetToleranceDeg;

    // FT live decode depth is controlled from the FT decode tab; the general
    // settings dialog must preserve it instead of forcing single-pass.
    merged.ft8LiveDecodeDepth = m_initialSettings.ft8LiveDecodeDepth;
    if (merged.ft8LiveDecodeDepth == "deep") {
        merged.ft8LiveDecodeDepth = "adaptive";
    }
    merged.ft8DeepDecode = (merged.ft8LiveDecodeDepth == "adaptive");
    merged.ft8DspPlusDecode = false;

    m_resultSettings = merged;
}

void AppSettingsDialog::updateRotatorEndpointWarning()
{
    if (m_lblRotatorEndpointWarning == nullptr) {
        return;
    }

    AppSettings probe = m_initialSettings;
    if (m_chkRotatorEnabled != nullptr) {
        probe.rotatorEnabled = m_chkRotatorEnabled->isChecked();
    }
    for (int i = 0; i < 3; ++i) {
        if (m_rotatorPage != nullptr) {
            if (QComboBox *path = m_rotatorPage->findChild<QComboBox *>(QStringLiteral("rotatorPath_%1").arg(i))) {
                probe.rotatorProfiles[i].path = path->currentText().trimmed();
            }
        }
    }

    const QString conflict = rotatorEndpointConflictText(probe);
    if (conflict.isEmpty()) {
        m_lblRotatorEndpointWarning->setVisible(false);
        m_lblRotatorEndpointWarning->clear();
        return;
    }

    m_lblRotatorEndpointWarning->setText(L(QStringLiteral("Warning")) + QStringLiteral(": ") + L(conflict));
    m_lblRotatorEndpointWarning->setVisible(true);
}

bool AppSettingsDialog::rotatorEndpointConflictRequiresConfirmation() const
{
    return !rotatorEndpointConflictText(m_resultSettings).isEmpty();
}

void AppSettingsDialog::accept()
{
    collectSettings();
    const QString conflict = rotatorEndpointConflictText(m_resultSettings);
    if (!conflict.isEmpty()) {
        const QMessageBox::StandardButton answer = QMessageBox::warning(
            this,
            L(QStringLiteral("Rotator endpoint conflict")),
            L(conflict) + QStringLiteral("\n\n") +
                L(QStringLiteral("Recommended action: assign CatRotator a dedicated serial port or a dedicated rotctld TCP endpoint before enabling auto-connect.")),
            QMessageBox::Cancel | QMessageBox::Ok,
            QMessageBox::Cancel);
        if (answer != QMessageBox::Ok) {
            updateRotatorEndpointWarning();
            return;
        }
    }
    QDialog::accept();
}
