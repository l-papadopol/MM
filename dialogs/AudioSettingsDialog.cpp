#include "AudioSettingsDialog.h"
#include "ui_AudioSettingsDialog.h"
#include "../utils/UiScale.h"

#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QSerialPortInfo>

#include <utility>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QAudioDevice>
#include <QMediaDevices>
#else
#include <QAudio>
#include <QAudioDeviceInfo>
#endif

// -----------------------------------------------------------------------------
// Construction
// -----------------------------------------------------------------------------

AudioSettingsDialog::AudioSettingsDialog(const AppSettings &initialSettings,
                                         QWidget *parent)
    : QDialog(parent),
    ui(new Ui::AudioSettingsDialog),
    m_settings(initialSettings)
{
    ui->setupUi(this);
    // Compact, native-size dialog: the v1.45 high-DPI scaler made this
    // window much too tall on 1920x1080 Windows desktops.
    resize(540, 310);
    setMinimumSize(500, 280);

    connect(ui->btnRefresh, &QPushButton::clicked,
            this, &AudioSettingsDialog::refreshDevices);

    connect(ui->buttonBox, &QDialogButtonBox::accepted,
            this, &AudioSettingsDialog::accept);

    connect(ui->buttonBox, &QDialogButtonBox::rejected,
            this, &AudioSettingsDialog::reject);

    ui->cmbSampleRate->addItem("44.1 kHz", 44100);
    ui->cmbSampleRate->addItem("48 kHz", 48000);
    ui->cmbSampleRate->addItem("96 kHz", 96000);

    ui->cmbPttMethod->addItem("CAT / Hamlib", "cat_hamlib");
    ui->cmbPttMethod->addItem("Serial RTS", "serial_rts");
    ui->cmbPttMethod->addItem("Serial DTR", "serial_dtr");
    ui->cmbPttMethod->addItem("None / audio only", "none");
    int pttMethodIndex = ui->cmbPttMethod->findData(m_settings.pttMethod);
    if (pttMethodIndex < 0 && m_settings.hamlibPttEnabled) {
        pttMethodIndex = ui->cmbPttMethod->findData(QStringLiteral("cat_hamlib"));
    }
    ui->cmbPttMethod->setCurrentIndex(pttMethodIndex >= 0 ? pttMethodIndex : ui->cmbPttMethod->findData(QStringLiteral("none")));
    connect(ui->cmbPttMethod, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this]() { updatePttSerialUi(); });

    const int sampleRateIndex = ui->cmbSampleRate->findData(m_settings.audioSampleRate);
    ui->cmbSampleRate->setCurrentIndex(sampleRateIndex >= 0 ? sampleRateIndex : 1);

    refreshDevices();
    refreshLabels();
}

AudioSettingsDialog::~AudioSettingsDialog()
{
    delete ui;
}

void AudioSettingsDialog::setTextTranslator(std::function<QString(const QString &)> translator)
{
    m_textTranslator = std::move(translator);
    refreshLabels();
}

QString AudioSettingsDialog::L(const QString &source) const
{
    if (m_textTranslator) {
        const QString translated = m_textTranslator(source);
        if (!translated.isEmpty()) {
            return translated;
        }
    }
    return source;
}

void AudioSettingsDialog::refreshLabels()
{
    setWindowTitle(L(QStringLiteral("Audio / PTT settings")));
    if (ui->grpAudio != nullptr) ui->grpAudio->setTitle(L(QStringLiteral("Audio devices")));
    if (ui->grpPtt != nullptr) ui->grpPtt->setTitle(L(QStringLiteral("PTT control")));
    if (ui->lblAudioInput != nullptr) ui->lblAudioInput->setText(L(QStringLiteral("Audio input")));
    if (ui->lblAudioOutput != nullptr) ui->lblAudioOutput->setText(L(QStringLiteral("Audio output")));
    if (ui->lblSampleRate != nullptr) ui->lblSampleRate->setText(L(QStringLiteral("Sample rate")));
    if (ui->lblPttMethod != nullptr) ui->lblPttMethod->setText(L(QStringLiteral("PTT method")));
    if (ui->lblPttPort != nullptr) ui->lblPttPort->setText(L(QStringLiteral("PTT serial port")));
    updatePttSerialUi();
    if (ui->btnRefresh != nullptr) ui->btnRefresh->setText(L(QStringLiteral("Refresh")));
    if (ui->cmbPttMethod != nullptr && ui->cmbPttMethod->count() >= 4) {
        const int current = ui->cmbPttMethod->currentIndex();
        ui->cmbPttMethod->setItemText(0, L(QStringLiteral("CAT / Hamlib")));
        ui->cmbPttMethod->setItemText(1, L(QStringLiteral("Serial RTS")));
        ui->cmbPttMethod->setItemText(2, L(QStringLiteral("Serial DTR")));
        ui->cmbPttMethod->setItemText(3, L(QStringLiteral("None / audio only")));
        if (current >= 0) ui->cmbPttMethod->setCurrentIndex(current);
    }
    if (ui->buttonBox != nullptr) {
        if (QPushButton *ok = ui->buttonBox->button(QDialogButtonBox::Ok)) {
            ok->setText(L(QStringLiteral("OK")));
        }
        if (QPushButton *cancel = ui->buttonBox->button(QDialogButtonBox::Cancel)) {
            cancel->setText(L(QStringLiteral("Cancel")));
        }
    }
}

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------

AppSettings AudioSettingsDialog::settings() const
{
    AppSettings selected = m_settings;

    selected.audioInputName = ui->cmbAudioInput->currentData().toString();
    selected.audioOutputName = ui->cmbAudioOutput->currentData().toString();
    selected.audioSampleRate = ui->cmbSampleRate->currentData().toInt();
    selected.pttPortName = ui->cmbPttPort->currentData().toString();
    selected.pttMethod = ui->cmbPttMethod->currentData().toString();
    selected.hamlibPttEnabled = (selected.pttMethod == QStringLiteral("cat_hamlib"));

    return selected;
}

// -----------------------------------------------------------------------------
// Device enumeration
// -----------------------------------------------------------------------------

void AudioSettingsDialog::refreshDevices()
{
    const QString currentInput = ui->cmbAudioInput->currentData().toString().isEmpty()
                                     ? m_settings.audioInputName
                                     : ui->cmbAudioInput->currentData().toString();

    const QString currentOutput = ui->cmbAudioOutput->currentData().toString().isEmpty()
                                      ? m_settings.audioOutputName
                                      : ui->cmbAudioOutput->currentData().toString();

    const QString currentPtt = ui->cmbPttPort->currentData().toString().isEmpty()
                                   ? m_settings.pttPortName
                                   : ui->cmbPttPort->currentData().toString();

    ui->cmbAudioInput->clear();
    ui->cmbAudioOutput->clear();
    ui->cmbPttPort->clear();

    QStringList seenInputs;
    QStringList seenOutputs;

    addUniqueDeviceItem(ui->cmbAudioInput, seenInputs, "Default", "default");
    addUniqueDeviceItem(ui->cmbAudioOutput, seenOutputs, "Default", "default");

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)

    const QList<QAudioDevice> inputDevices = QMediaDevices::audioInputs();

    for (const QAudioDevice &device : inputDevices) {
        const QString backendName = device.description();
        addUniqueDeviceItem(ui->cmbAudioInput,
                            seenInputs,
                            friendlyAudioName(backendName),
                            backendName);
    }

    const QList<QAudioDevice> outputDevices = QMediaDevices::audioOutputs();

    for (const QAudioDevice &device : outputDevices) {
        const QString backendName = device.description();
        addUniqueDeviceItem(ui->cmbAudioOutput,
                            seenOutputs,
                            friendlyAudioName(backendName),
                            backendName);
    }

#else

    const QList<QAudioDeviceInfo> inputDevices =
        QAudioDeviceInfo::availableDevices(QAudio::AudioInput);

    for (const QAudioDeviceInfo &device : inputDevices) {
        const QString backendName = device.deviceName();
        addUniqueDeviceItem(ui->cmbAudioInput,
                            seenInputs,
                            friendlyAudioName(backendName),
                            backendName);
    }

    const QList<QAudioDeviceInfo> outputDevices =
        QAudioDeviceInfo::availableDevices(QAudio::AudioOutput);

    for (const QAudioDeviceInfo &device : outputDevices) {
        const QString backendName = device.deviceName();
        addUniqueDeviceItem(ui->cmbAudioOutput,
                            seenOutputs,
                            friendlyAudioName(backendName),
                            backendName);
    }

#endif

    const QList<QSerialPortInfo> serialPorts = QSerialPortInfo::availablePorts();

    for (const QSerialPortInfo &port : serialPorts) {
        QString label = port.portName();

        if (!port.description().isEmpty()) {
            label += " - " + port.description();
        }

        ui->cmbPttPort->addItem(label, port.portName());
    }

    if (ui->cmbPttPort->count() == 0) {
        ui->cmbPttPort->addItem("No serial port found", QString());
    }

    selectByBackendName(ui->cmbAudioInput, currentInput);
    selectByBackendName(ui->cmbAudioOutput, currentOutput);
    selectByBackendName(ui->cmbPttPort, currentPtt);
    updatePttSerialUi();
}

void AudioSettingsDialog::updatePttSerialUi()
{
    if (ui == nullptr || ui->cmbPttMethod == nullptr || ui->cmbPttPort == nullptr) {
        return;
    }
    const QString method = ui->cmbPttMethod->currentData().toString();
    const bool serialPtt = (method == QStringLiteral("serial_rts") || method == QStringLiteral("serial_dtr"));
    ui->cmbPttPort->setEnabled(serialPtt);
    if (ui->lblPttPort != nullptr) {
        ui->lblPttPort->setEnabled(serialPtt);
    }
    const QString tip = serialPtt
        ? L(QStringLiteral("Dedicated serial port used only for RTS/DTR PTT."))
        : L(QStringLiteral("Disabled because PTT is handled by CAT/Hamlib or audio-only mode; serial rotator ports are not reserved here."));
    ui->cmbPttPort->setToolTip(tip);
    if (ui->lblPttPort != nullptr) {
        ui->lblPttPort->setToolTip(tip);
    }
}

void AudioSettingsDialog::addUniqueDeviceItem(QComboBox *combo,
                                              QStringList &seenBackendNames,
                                              const QString &displayName,
                                              const QString &backendName)
{
    if (combo == nullptr) {
        return;
    }

    if (backendName.trimmed().isEmpty()) {
        return;
    }

    if (seenBackendNames.contains(backendName)) {
        return;
    }

    seenBackendNames.append(backendName);
    combo->addItem(displayName, backendName);
}

void AudioSettingsDialog::selectByBackendName(QComboBox *combo, const QString &backendName)
{
    if (combo == nullptr) {
        return;
    }

    const int index = combo->findData(backendName);

    if (index >= 0) {
        combo->setCurrentIndex(index);
        return;
    }

    if (combo->count() > 0) {
        combo->setCurrentIndex(0);
    }
}

// -----------------------------------------------------------------------------
// Display helpers
// -----------------------------------------------------------------------------

QString AudioSettingsDialog::friendlyAudioName(const QString &backendName) const
{
    if (backendName.trimmed().isEmpty()) {
        return "Unknown audio device";
    }

    if (backendName == "default") {
        return "Default";
    }

    QString name = backendName;

    name.replace("alsa_input.", "Input: ");
    name.replace("alsa_output.", "Output: ");
    name.replace("pci-0000_00_1f.3-platform-", "");
    name.replace("platform-", "");
    name.replace("__", " ");
    name.replace("_", " ");
    name.replace(".", " ");
    name.replace("source", "");
    name.replace("sink", "");
    name.replace("hw ", "hw:");

    name = name.simplified();

    if (name.length() > 78) {
        name = name.left(75) + "...";
    }

    return name;
}
