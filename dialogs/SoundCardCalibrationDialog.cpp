/**************************************************************************
 * MadModem sound-card calibration dialog.
 *
 * This implementation intentionally follows the calibration model documented
 * by QSSTV: before SSTV work, discipline the PC clock using NTP/timesyncd and
 * then measure how many audio frames the sound device transfers against that
 * monotonic clock.  It is not an audio loopback measurement.
 *
 * QSSTV reference:
 *   Copyright (C) 2000-2019 Johan Maes, ON4QZ
 *   QSSTV is distributed under the GNU General Public License.
 *
 * This MadModem source-level adaptation keeps attribution and the original
 * QSSTV reference files under third_party/qsstv_gpl/.
 **************************************************************************/

#include "SoundCardCalibrationDialog.h"
#include "../utils/UiScale.h"

#include <QApplication>
#include <QByteArray>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QIODevice>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QScrollArea>
#include <QSizePolicy>
#include <QThread>
#include <QVBoxLayout>
#include <QtGlobal>
#include <QtMath>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QAudioSource>
#include <QMediaDevices>
#else
#include <QAudio>
#include <QAudioDeviceInfo>
#include <QAudioFormat>
#include <QAudioInput>
#include <QAudioOutput>
#endif

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

namespace {

constexpr int kLeadInMs = 2500;
constexpr int kDefaultDurationSec = 180;
constexpr int kMinimumDurationSec = 30;
constexpr int kMaximumDurationSec = 900;
constexpr double kPpmClamp = 5000.0;
constexpr double kWarnPpm = 1500.0;
constexpr double kFailPpm = 4900.0;
constexpr double kTwoPi = 6.283185307179586476925286766559;

QString safeDeviceName(const QString &name)
{
    return name.trimmed().isEmpty() ? QStringLiteral("default") : name;
}

double boundedPpm(double ppm)
{
    if (!std::isfinite(ppm)) {
        return 0.0;
    }
    return qBound(-kPpmClamp, ppm, kPpmClamp);
}

QString ppmText(double ppm)
{
    return QStringLiteral("%1 ppm").arg(ppm, 0, 'f', 2);
}

double ppmFromMeasured(double measuredHz, double nominalHz)
{
    if (!std::isfinite(measuredHz) || !std::isfinite(nominalHz) || nominalHz <= 0.0) {
        return 0.0;
    }
    return ((measuredHz / nominalHz) - 1.0) * 1000000.0;
}

qint64 durationNsFromSeconds(int seconds)
{
    return static_cast<qint64>(qBound(kMinimumDurationSec, seconds, kMaximumDurationSec)) * 1000000000LL;
}

void fillTone(QByteArray &buffer, int bytesPerFrame, int channels, int sampleRate, double &phase)
{
    if (bytesPerFrame <= 0 || channels <= 0 || sampleRate <= 0 || buffer.isEmpty()) {
        return;
    }

    const int frameCount = buffer.size() / bytesPerFrame;
    qint16 *pcm = reinterpret_cast<qint16 *>(buffer.data());
    const double inc = kTwoPi * 1000.0 / static_cast<double>(sampleRate);

    for (int i = 0; i < frameCount; ++i) {
        const qint16 value = static_cast<qint16>(qRound(std::sin(phase) * 1800.0));
        for (int ch = 0; ch < channels; ++ch) {
            pcm[i * channels + ch] = value;
        }
        phase += inc;
        if (phase >= kTwoPi) {
            phase -= kTwoPi;
        }
    }
}

QString readProcessText(const QString &program, const QStringList &arguments, int timeoutMs = 1500)
{
    QProcess process;
    process.start(program, arguments);
    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(250);
        return QString();
    }
    return QString::fromLocal8Bit(process.readAllStandardOutput()) +
           QString::fromLocal8Bit(process.readAllStandardError());
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)

QAudioDevice selectInputDevice(const QString &deviceName)
{
    QAudioDevice selected = QMediaDevices::defaultAudioInput();
    const QList<QAudioDevice> devices = QMediaDevices::audioInputs();
    for (const QAudioDevice &device : devices) {
        if (device.description() == deviceName) {
            selected = device;
            break;
        }
    }
    return selected;
}

QAudioDevice selectOutputDevice(const QString &deviceName)
{
    QAudioDevice selected = QMediaDevices::defaultAudioOutput();
    const QList<QAudioDevice> devices = QMediaDevices::audioOutputs();
    for (const QAudioDevice &device : devices) {
        if (device.description() == deviceName) {
            selected = device;
            break;
        }
    }
    return selected;
}

bool makeInt16MonoFormat(const QAudioDevice &device,
                         int requestedSampleRate,
                         QAudioFormat &format,
                         QString &error)
{
    format = QAudioFormat();
    format.setSampleRate(requestedSampleRate);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);

    if (!device.isFormatSupported(format)) {
        QAudioFormat nearest = format;
        QAudioFormat preferred = device.preferredFormat();
        if (preferred.sampleFormat() == QAudioFormat::Int16 && preferred.channelCount() >= 1) {
            nearest = preferred;
        }
        format = nearest;
    }

    if (format.sampleRate() <= 0 || format.channelCount() <= 0 ||
        format.sampleFormat() != QAudioFormat::Int16) {
        error = QStringLiteral("Selected device does not support signed 16-bit PCM calibration.");
        return false;
    }

    return true;
}

#else

QAudioDeviceInfo selectInputDevice(const QString &deviceName)
{
    QAudioDeviceInfo selected = QAudioDeviceInfo::defaultInputDevice();
    const QList<QAudioDeviceInfo> devices = QAudioDeviceInfo::availableDevices(QAudio::AudioInput);
    for (const QAudioDeviceInfo &device : devices) {
        if (device.deviceName() == deviceName) {
            selected = device;
            break;
        }
    }
    return selected;
}

QAudioDeviceInfo selectOutputDevice(const QString &deviceName)
{
    QAudioDeviceInfo selected = QAudioDeviceInfo::defaultOutputDevice();
    const QList<QAudioDeviceInfo> devices = QAudioDeviceInfo::availableDevices(QAudio::AudioOutput);
    for (const QAudioDeviceInfo &device : devices) {
        if (device.deviceName() == deviceName) {
            selected = device;
            break;
        }
    }
    return selected;
}

bool makeInt16MonoFormat(const QAudioDeviceInfo &device,
                         int requestedSampleRate,
                         QAudioFormat &format,
                         QString &error)
{
    format = QAudioFormat();
    format.setSampleRate(requestedSampleRate);
    format.setChannelCount(1);
    format.setSampleSize(16);
    format.setCodec(QStringLiteral("audio/pcm"));
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setSampleType(QAudioFormat::SignedInt);

    if (!device.isFormatSupported(format)) {
        format = device.nearestFormat(format);
    }

    if (format.sampleRate() <= 0 || format.channelCount() <= 0 ||
        format.sampleSize() != 16 || format.sampleType() != QAudioFormat::SignedInt) {
        error = QStringLiteral("Selected device does not support signed 16-bit PCM calibration.");
        return false;
    }

    return true;
}

#endif

} // namespace

SoundCardCalibrationDialog::SoundCardCalibrationDialog(const AppSettings &settings,
                                                       QWidget *parent)
    : QDialog(parent),
      m_settings(settings)
{
    buildUi();
    refreshLabels();
    checkClockDiscipline();
    // Keep native widget sizes; global scaling made this dialog twice as tall on 1920x1080 Windows.
}

AppSettings SoundCardCalibrationDialog::settings() const
{
    return m_settings;
}


void SoundCardCalibrationDialog::setTextTranslator(std::function<QString(const QString &)> translator)
{
    m_textTranslator = std::move(translator);
    updateStandardButtonText();
    if (m_lblStatus != nullptr) {
        const QString source = m_lblStatus->property("i18nSourceText").toString();
        if (!source.trimmed().isEmpty()) {
            m_lblStatus->setText(L(source));
        }
    }
    checkClockDiscipline();
}

QString SoundCardCalibrationDialog::L(const QString &source) const
{
    if (m_textTranslator) {
        return m_textTranslator(source);
    }
    return source;
}

void SoundCardCalibrationDialog::setStatusText(const QString &text)
{
    if (m_lblStatus != nullptr) {
        m_lblStatus->setText(text);
    }
}

void SoundCardCalibrationDialog::updateStandardButtonText()
{
    if (m_buttonBox == nullptr) {
        return;
    }
    if (QPushButton *ok = m_buttonBox->button(QDialogButtonBox::Ok)) {
        ok->setText(L(QStringLiteral("OK")));
    }
    if (QPushButton *cancel = m_buttonBox->button(QDialogButtonBox::Cancel)) {
        cancel->setText(L(QStringLiteral("Cancel")));
    }
}

void SoundCardCalibrationDialog::buildUi()
{
    setWindowTitle(QStringLiteral("Soundcard calibration"));
    // Compact tab-friendly layout: keep the QSSTV information readable, but avoid
    // fixed columns/buttons wider than the unified Settings window.
    resize(780, 500);
    setMinimumSize(0, 0);

    QVBoxLayout *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(8, 8, 8, 8);
    outerLayout->setSpacing(6);

    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    QWidget *page = new QWidget(scroll);
    QVBoxLayout *mainLayout = new QVBoxLayout(page);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(6);

    QLabel *intro = new QLabel(
        QStringLiteral("QSSTV-style SSTV clock calibration. It does not need an audio loopback: RX counts captured audio frames and TX counts frames accepted by the playback device, both against the PC monotonic clock. For a meaningful result, synchronize the system clock with NTP/timesyncd and let the measurement run for several minutes."),
        page);
    intro->setWordWrap(true);
    intro->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    mainLayout->addWidget(intro);

    QGroupBox *clockGroup = new QGroupBox(QStringLiteral("Clock reference"), page);
    QGridLayout *clockLayout = new QGridLayout(clockGroup);
    clockLayout->setColumnStretch(0, 1);
    m_lblClockStatus = new QLabel(clockGroup);
    m_lblClockStatus->setWordWrap(true);
    m_lblClockStatus->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_btnCheckClock = new QPushButton(QStringLiteral("Check clock"), clockGroup);
    m_btnCheckClock->setMinimumWidth(140);
    m_btnCheckClock->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    clockLayout->addWidget(m_lblClockStatus, 0, 0);
    clockLayout->addWidget(m_btnCheckClock, 0, 1, Qt::AlignTop);
    mainLayout->addWidget(clockGroup);

    QGroupBox *deviceGroup = new QGroupBox(QStringLiteral("Selected devices"), page);
    QGridLayout *deviceLayout = new QGridLayout(deviceGroup);
    deviceLayout->setColumnMinimumWidth(0, 190);
    deviceLayout->setColumnStretch(1, 1);
    m_lblInput = new QLabel(deviceGroup);
    m_lblOutput = new QLabel(deviceGroup);
    m_lblNominal = new QLabel(deviceGroup);
    for (QLabel *label : {m_lblInput, m_lblOutput, m_lblNominal}) {
        label->setWordWrap(true);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    }
    deviceLayout->addWidget(new QLabel(QStringLiteral("RX input"), deviceGroup), 0, 0);
    deviceLayout->addWidget(m_lblInput, 0, 1);
    deviceLayout->addWidget(new QLabel(QStringLiteral("TX output"), deviceGroup), 1, 0);
    deviceLayout->addWidget(m_lblOutput, 1, 1);
    deviceLayout->addWidget(new QLabel(QStringLiteral("Configured sample rate"), deviceGroup), 2, 0);
    deviceLayout->addWidget(m_lblNominal, 2, 1);
    mainLayout->addWidget(deviceGroup);

    QGroupBox *measureGroup = new QGroupBox(QStringLiteral("Measured clock"), page);
    QGridLayout *grid = new QGridLayout(measureGroup);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(5);
    grid->setColumnMinimumWidth(0, 60);
    grid->setColumnMinimumWidth(1, 145);
    grid->setColumnMinimumWidth(2, 105);
    grid->setColumnMinimumWidth(3, 150);
    grid->setColumnMinimumWidth(4, 180);
    grid->setColumnStretch(4, 1);

    auto header = [measureGroup](const QString &text) {
        QLabel *label = new QLabel(text, measureGroup);
        label->setWordWrap(true);
        label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        return label;
    };

    grid->addWidget(header(QStringLiteral("Path")), 0, 0);
    grid->addWidget(header(QStringLiteral("Measured rate")), 0, 1);
    grid->addWidget(header(QStringLiteral("Correction")), 0, 2);
    grid->addWidget(header(QStringLiteral("Manual rate")), 0, 3);
    grid->addWidget(header(QStringLiteral("Progress")), 0, 4);

    grid->addWidget(new QLabel(QStringLiteral("RX"), measureGroup), 1, 0);
    m_lblRxMeasured = new QLabel(measureGroup);
    m_lblRxPpm = new QLabel(measureGroup);
    m_spinManualRxHz = new QDoubleSpinBox(measureGroup);
    m_spinManualRxHz->setRange(7000.0, 200000.0);
    m_spinManualRxHz->setDecimals(3);
    m_spinManualRxHz->setSingleStep(0.1);
    m_spinManualRxHz->setMinimumWidth(135);
    m_spinManualRxHz->setMaximumWidth(170);
    m_rxProgress = new QProgressBar(measureGroup);
    m_rxProgress->setRange(0, 100);
    m_rxProgress->setMinimumWidth(160);
    m_rxProgress->setMaximumWidth(220);
    grid->addWidget(m_lblRxMeasured, 1, 1);
    grid->addWidget(m_lblRxPpm, 1, 2);
    grid->addWidget(m_spinManualRxHz, 1, 3);
    grid->addWidget(m_rxProgress, 1, 4);

    grid->addWidget(new QLabel(QStringLiteral("TX"), measureGroup), 2, 0);
    m_lblTxMeasured = new QLabel(measureGroup);
    m_lblTxPpm = new QLabel(measureGroup);
    m_spinManualTxHz = new QDoubleSpinBox(measureGroup);
    m_spinManualTxHz->setRange(7000.0, 200000.0);
    m_spinManualTxHz->setDecimals(3);
    m_spinManualTxHz->setSingleStep(0.1);
    m_spinManualTxHz->setMinimumWidth(135);
    m_spinManualTxHz->setMaximumWidth(170);
    m_txProgress = new QProgressBar(measureGroup);
    m_txProgress->setRange(0, 100);
    m_txProgress->setMinimumWidth(160);
    m_txProgress->setMaximumWidth(220);
    grid->addWidget(m_lblTxMeasured, 2, 1);
    grid->addWidget(m_lblTxPpm, 2, 2);
    grid->addWidget(m_spinManualTxHz, 2, 3);
    grid->addWidget(m_txProgress, 2, 4);

    mainLayout->addWidget(measureGroup);

    QGroupBox *controlGroup = new QGroupBox(QStringLiteral("Measurement controls"), page);
    QGridLayout *controlLayout = new QGridLayout(controlGroup);
    controlLayout->setHorizontalSpacing(8);
    controlLayout->setVerticalSpacing(6);
    controlLayout->setColumnMinimumWidth(0, 160);
    controlLayout->setColumnMinimumWidth(1, 110);
    controlLayout->setColumnMinimumWidth(2, 1);
    controlLayout->setColumnStretch(2, 1);

    QLabel *durationLabel = new QLabel(QStringLiteral("Seconds per path"), controlGroup);
    m_spinDurationSec = new QSpinBox(controlGroup);
    m_spinDurationSec->setRange(kMinimumDurationSec, kMaximumDurationSec);
    m_spinDurationSec->setSingleStep(30);
    m_spinDurationSec->setValue(kDefaultDurationSec);
    m_spinDurationSec->setMinimumWidth(110);
    QLabel *durationHint = new QLabel(QStringLiteral("Use 180 s or more for reliable SSTV calibration."), controlGroup);
    durationHint->setWordWrap(true);
    controlLayout->addWidget(durationLabel, 0, 0);
    controlLayout->addWidget(m_spinDurationSec, 0, 1);
    controlLayout->addWidget(durationHint, 0, 2, 1, 3);

    m_btnRx = new QPushButton(QStringLiteral("Calibrate RX"), controlGroup);
    m_btnTx = new QPushButton(QStringLiteral("Calibrate TX"), controlGroup);
    m_btnBoth = new QPushButton(QStringLiteral("Calibrate both"), controlGroup);
    m_btnApplyManual = new QPushButton(QStringLiteral("Apply manual rates"), controlGroup);
    m_btnCancelRun = new QPushButton(QStringLiteral("Stop measurement"), controlGroup);
    m_btnCancelRun->setEnabled(false);
    m_btnRx->setMinimumWidth(120);
    m_btnTx->setMinimumWidth(120);
    m_btnBoth->setMinimumWidth(130);
    m_btnApplyManual->setMinimumWidth(160);
    m_btnCancelRun->setMinimumWidth(150);
    for (QPushButton *button : {m_btnRx, m_btnTx, m_btnBoth, m_btnApplyManual, m_btnCancelRun}) {
        button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    }
    controlLayout->addWidget(m_btnRx, 1, 0);
    controlLayout->addWidget(m_btnTx, 1, 1);
    controlLayout->addWidget(m_btnBoth, 1, 2);
    controlLayout->addWidget(m_btnApplyManual, 2, 0, 1, 2);
    controlLayout->addWidget(m_btnCancelRun, 2, 2);
    mainLayout->addWidget(controlGroup);

    m_lblStatus = new QLabel(QStringLiteral("Ready. Use 180 s or more for reliable SSTV calibration."), page);
    m_lblStatus->setProperty("i18nSourceText", QStringLiteral("Ready. Use 180 s or more for reliable SSTV calibration."));
    m_lblStatus->setWordWrap(true);
    m_lblStatus->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    mainLayout->addWidget(m_lblStatus);
    mainLayout->addStretch(1);

    scroll->setWidget(page);
    outerLayout->addWidget(scroll, 1);

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    outerLayout->addWidget(m_buttonBox);
    updateStandardButtonText();

    connect(m_btnRx, &QPushButton::clicked, this, &SoundCardCalibrationDialog::calibrateRx);
    connect(m_btnTx, &QPushButton::clicked, this, &SoundCardCalibrationDialog::calibrateTx);
    connect(m_btnBoth, &QPushButton::clicked, this, &SoundCardCalibrationDialog::calibrateBoth);
    connect(m_btnApplyManual, &QPushButton::clicked, this, &SoundCardCalibrationDialog::applyManualValues);
    connect(m_btnCancelRun, &QPushButton::clicked, this, &SoundCardCalibrationDialog::cancelMeasurement);
    connect(m_btnCheckClock, &QPushButton::clicked, this, &SoundCardCalibrationDialog::checkClockDiscipline);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, [this]() {
        if (m_busy) {
            cancelMeasurement();
            return;
        }
        reject();
    });
}

void SoundCardCalibrationDialog::refreshLabels()
{
    const double nominal = static_cast<double>(m_settings.audioSampleRate);
    const double rxMeasured = nominal * (1.0 + m_settings.audioRxClockPpm / 1000000.0);
    const double txMeasured = nominal * (1.0 + m_settings.audioTxClockPpm / 1000000.0);

    if (m_lblInput != nullptr) {
        m_lblInput->setText(safeDeviceName(m_settings.audioInputName));
    }
    if (m_lblOutput != nullptr) {
        m_lblOutput->setText(safeDeviceName(m_settings.audioOutputName));
    }
    if (m_lblNominal != nullptr) {
        m_lblNominal->setText(QStringLiteral("%1 Hz").arg(m_settings.audioSampleRate));
    }
    if (m_lblRxMeasured != nullptr) {
        m_lblRxMeasured->setText(QStringLiteral("%1 Hz").arg(rxMeasured, 0, 'f', 3));
    }
    if (m_lblTxMeasured != nullptr) {
        m_lblTxMeasured->setText(QStringLiteral("%1 Hz").arg(txMeasured, 0, 'f', 3));
    }
    if (m_lblRxPpm != nullptr) {
        m_lblRxPpm->setText(ppmText(m_settings.audioRxClockPpm));
    }
    if (m_lblTxPpm != nullptr) {
        m_lblTxPpm->setText(ppmText(m_settings.audioTxClockPpm));
    }
    if (m_spinManualRxHz != nullptr && !m_spinManualRxHz->hasFocus()) {
        m_spinManualRxHz->setValue(rxMeasured);
    }
    if (m_spinManualTxHz != nullptr && !m_spinManualTxHz->hasFocus()) {
        m_spinManualTxHz->setValue(txMeasured);
    }
}

void SoundCardCalibrationDialog::setBusy(bool busy)
{
    m_busy = busy;
    m_cancelRequested = false;
    if (m_btnRx != nullptr) m_btnRx->setEnabled(!busy);
    if (m_btnTx != nullptr) m_btnTx->setEnabled(!busy);
    if (m_btnBoth != nullptr) m_btnBoth->setEnabled(!busy);
    if (m_btnApplyManual != nullptr) m_btnApplyManual->setEnabled(!busy);
    if (m_btnCheckClock != nullptr) m_btnCheckClock->setEnabled(!busy);
    if (m_spinDurationSec != nullptr) m_spinDurationSec->setEnabled(!busy);
    if (m_spinManualRxHz != nullptr) m_spinManualRxHz->setEnabled(!busy);
    if (m_spinManualTxHz != nullptr) m_spinManualTxHz->setEnabled(!busy);
    if (m_btnCancelRun != nullptr) m_btnCancelRun->setEnabled(busy);
    if (m_buttonBox != nullptr) {
        if (QPushButton *ok = m_buttonBox->button(QDialogButtonBox::Ok)) {
            ok->setEnabled(!busy);
        }
    }
}

void SoundCardCalibrationDialog::updateProgress(QProgressBar *bar, int percent)
{
    if (bar != nullptr) {
        bar->setValue(qBound(0, percent, 100));
    }
}

void SoundCardCalibrationDialog::cancelMeasurement()
{
    m_cancelRequested = true;
    setStatusText(L(QStringLiteral("Stopping measurement...")));
}

void SoundCardCalibrationDialog::checkClockDiscipline()
{
    const QString status = systemClockStatusText();
    if (m_lblClockStatus != nullptr) {
        m_lblClockStatus->setText(status + QStringLiteral("\n") +
                                  L(QStringLiteral("Last check")) + QStringLiteral(": ") +
                                  QDateTime::currentDateTime().toString(Qt::ISODate));
    }
    if (m_lblStatus != nullptr && !m_busy) {
        m_lblStatus->setProperty("i18nSourceText", QString());
        m_lblStatus->setText(L(QStringLiteral("Clock reference check completed.")) + QStringLiteral(" ") + status);
    }
}

QString SoundCardCalibrationDialog::systemClockStatusText() const
{
#ifdef Q_OS_LINUX
    QString out = readProcessText(QStringLiteral("timedatectl"),
                                  QStringList() << QStringLiteral("show")
                                                << QStringLiteral("-p") << QStringLiteral("SystemClockSynchronized")
                                                << QStringLiteral("-p") << QStringLiteral("NTPSynchronized")
                                                << QStringLiteral("-p") << QStringLiteral("NTP"));

    if (!out.trimmed().isEmpty()) {
        const QString lower = out.toLower();
        if (lower.contains(QStringLiteral("systemclocksynchronized=yes")) ||
            lower.contains(QStringLiteral("ntpsynchronized=yes"))) {
            return L(QStringLiteral("System clock synchronized: yes. Good reference for QSSTV-style calibration."));
        }
        if (lower.contains(QStringLiteral("systemclocksynchronized=no")) ||
            lower.contains(QStringLiteral("ntpsynchronized=no"))) {
            return L(QStringLiteral("System clock synchronized: no. Enable NTP/timesyncd before trusting SSTV calibration."));
        }
    }

    out = readProcessText(QStringLiteral("timedatectl"), QStringList());
    if (!out.trimmed().isEmpty()) {
        const QString lower = out.toLower();
        if (lower.contains(QStringLiteral("system clock synchronized: yes"))) {
            return L(QStringLiteral("System clock synchronized: yes. Good reference for QSSTV-style calibration."));
        }
        if (lower.contains(QStringLiteral("system clock synchronized: no"))) {
            return L(QStringLiteral("System clock synchronized: no. Enable NTP/timesyncd before trusting SSTV calibration."));
        }
    }

    out = readProcessText(QStringLiteral("ntpq"), QStringList() << QStringLiteral("-p"));
    if (out.contains(QChar('*'))) {
        return L(QStringLiteral("NTP peer selected by ntpq. Good reference for QSSTV-style calibration."));
    }

    return L(QStringLiteral("Clock sync status unknown. Calibration can run, but verify NTP/timesyncd manually."));
#elif defined(Q_OS_WIN)
    const QString w32tm = readProcessText(QStringLiteral("w32tm"),
                                          QStringList() << QStringLiteral("/query") << QStringLiteral("/status"));
    if (!w32tm.trimmed().isEmpty()) {
        if (w32tm.contains(QStringLiteral("Source"), Qt::CaseInsensitive) &&
            !w32tm.contains(QStringLiteral("Local CMOS Clock"), Qt::CaseInsensitive)) {
            return L(QStringLiteral("Windows Time reports an external time source. Good reference for QSSTV-style calibration."));
        }
        return L(QStringLiteral("Windows Time is available, but the source may be local. Verify Internet/NTP time synchronization before trusting calibration."));
    }
    return L(QStringLiteral("Clock sync status unknown. Enable Windows Internet time/NTP before trusting SSTV calibration."));
#else
    return L(QStringLiteral("Clock sync status not checked on this OS. Use an NTP-disciplined system clock for reliable SSTV calibration."));
#endif
}

void SoundCardCalibrationDialog::calibrateRx()
{
    setBusy(true);
    setStatusText(L(QStringLiteral("Measuring RX clock...")));
    if (m_rxProgress != nullptr) m_rxProgress->setValue(0);
    const MeasurementResult result = measureRxClock();
    storeRxResult(result);
    setBusy(false);
}

void SoundCardCalibrationDialog::calibrateTx()
{
    setBusy(true);
    setStatusText(L(QStringLiteral("Measuring TX clock...")));
    if (m_txProgress != nullptr) m_txProgress->setValue(0);
    const MeasurementResult result = measureTxClock();
    storeTxResult(result);
    setBusy(false);
}

void SoundCardCalibrationDialog::calibrateBoth()
{
    setBusy(true);

    setStatusText(L(QStringLiteral("Measuring RX clock...")));
    if (m_rxProgress != nullptr) m_rxProgress->setValue(0);
    MeasurementResult rx = measureRxClock();
    storeRxResult(rx);

    if (!m_cancelRequested) {
        setStatusText(L(QStringLiteral("Measuring TX clock...")));
        if (m_txProgress != nullptr) m_txProgress->setValue(0);
        MeasurementResult tx = measureTxClock();
        storeTxResult(tx);
    }

    setBusy(false);
}

void SoundCardCalibrationDialog::applyManualValues()
{
    const double nominal = static_cast<double>(m_settings.audioSampleRate);
    if (m_spinManualRxHz != nullptr) {
        m_settings.audioRxClockPpm = boundedPpm(ppmFromMeasured(m_spinManualRxHz->value(), nominal));
    }
    if (m_spinManualTxHz != nullptr) {
        m_settings.audioTxClockPpm = boundedPpm(ppmFromMeasured(m_spinManualTxHz->value(), nominal));
    }
    refreshLabels();
    if (m_lblStatus != nullptr) {
        setStatusText(L(QStringLiteral("Manual clock values applied. Press OK to save them.")));
    }
}

void SoundCardCalibrationDialog::storeRxResult(const MeasurementResult &result)
{
    if (result.ok) {
        m_settings.audioRxClockPpm = boundedPpm(result.ppm);
        if (m_spinManualRxHz != nullptr) {
            m_spinManualRxHz->setValue(result.measuredHz);
        }
        if (m_lblStatus != nullptr) {
            setStatusText(L(QStringLiteral("RX calibration completed. %1")).arg(result.message));
        }
    } else if (m_lblStatus != nullptr) {
        setStatusText(L(QStringLiteral("RX calibration failed/cancelled. %1")).arg(result.message));
    }
    refreshLabels();
}

void SoundCardCalibrationDialog::storeTxResult(const MeasurementResult &result)
{
    if (result.ok) {
        m_settings.audioTxClockPpm = boundedPpm(result.ppm);
        if (m_spinManualTxHz != nullptr) {
            m_spinManualTxHz->setValue(result.measuredHz);
        }
        if (m_lblStatus != nullptr) {
            setStatusText(L(QStringLiteral("TX calibration completed. %1")).arg(result.message));
        }
    } else if (m_lblStatus != nullptr) {
        setStatusText(L(QStringLiteral("TX calibration failed/cancelled. %1")).arg(result.message));
    }
    refreshLabels();
}

SoundCardCalibrationDialog::MeasurementResult SoundCardCalibrationDialog::measureRxClock()
{
    MeasurementResult result;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QAudioDevice audioDevice = selectInputDevice(m_settings.audioInputName);
#else
    QAudioDeviceInfo audioDevice = selectInputDevice(m_settings.audioInputName);
#endif

    QAudioFormat format;
    QString error;
    if (!makeInt16MonoFormat(audioDevice, m_settings.audioSampleRate, format, error)) {
        result.message = error;
        return result;
    }

    const int bytesPerFrame = qMax(1, format.channelCount() * static_cast<int>(sizeof(qint16)));
    const int bufferFrames = qMax(1024, format.sampleRate() / 10);

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QAudioSource audioInput(audioDevice, format, this);
    audioInput.setBufferSize(bufferFrames * bytesPerFrame);
    QIODevice *device = audioInput.start();
#else
    QAudioInput audioInput(audioDevice, format, this);
    audioInput.setBufferSize(bufferFrames * bytesPerFrame);
    QIODevice *device = audioInput.start();
#endif

    if (device == nullptr) {
        result.message = L(QStringLiteral("Unable to start selected RX input device."));
        return result;
    }

    const qint64 durationNs = durationNsFromSeconds(m_spinDurationSec != nullptr ? m_spinDurationSec->value() : kDefaultDurationSec);
    const qint64 halfNs = durationNs / 2;
    qint64 measuredBytes = 0;
    qint64 midBytes = 0;
    qint64 midNs = 0;
    bool midCaptured = false;
    bool counting = false;
    QElapsedTimer timer;
    timer.start();
    qint64 countStartNs = 0;
    qint64 countEndNs = 0;

    while (!m_cancelRequested) {
        QApplication::processEvents(QEventLoop::AllEvents, 20);
        const qint64 elapsedNs = timer.nsecsElapsed();
        const QByteArray bytes = device->readAll();

        if (!counting && elapsedNs >= static_cast<qint64>(kLeadInMs) * 1000000LL) {
            counting = true;
            countStartNs = timer.nsecsElapsed();
            countEndNs = countStartNs;
            measuredBytes = 0;
            midCaptured = false;
        }

        if (counting) {
            measuredBytes += bytes.size();
            countEndNs = timer.nsecsElapsed();
            const qint64 sinceStartNs = countEndNs - countStartNs;
            if (!midCaptured && sinceStartNs >= halfNs) {
                midCaptured = true;
                midBytes = measuredBytes;
                midNs = sinceStartNs;
            }
            updateProgress(m_rxProgress, static_cast<int>((100.0 * static_cast<double>(sinceStartNs)) /
                                                          static_cast<double>(durationNs)));
            if (sinceStartNs >= durationNs) {
                break;
            }
        }

        QThread::msleep(4);
    }

    audioInput.stop();

    if (m_cancelRequested) {
        result.message = L(QStringLiteral("Cancelled by user."));
        return result;
    }

    result.nominalHz = static_cast<double>(format.sampleRate());
    result.elapsedSec = static_cast<double>(countEndNs - countStartNs) / 1000000000.0;
    result.frames = measuredBytes / bytesPerFrame;

    if (result.elapsedSec <= 0.0 || result.frames <= 0) {
        result.message = L(QStringLiteral("No RX frames were captured."));
        return result;
    }

    result.measuredHz = static_cast<double>(result.frames) / result.elapsedSec;
    result.ppm = ppmFromMeasured(result.measuredHz, result.nominalHz);

    QString stability;
    if (midCaptured && midNs > 0 && (countEndNs - countStartNs - midNs) > 0) {
        const double firstRate = static_cast<double>(midBytes / bytesPerFrame) /
                                 (static_cast<double>(midNs) / 1000000000.0);
        const double secondRate = static_cast<double>((measuredBytes - midBytes) / bytesPerFrame) /
                                  (static_cast<double>(countEndNs - countStartNs - midNs) / 1000000000.0);
        const double deltaPpm = qAbs(ppmFromMeasured(secondRate, firstRate));
        if (std::isfinite(deltaPpm) && deltaPpm > 100.0) {
            stability = L(QStringLiteral(" Stability warning: first/second half differ by %1 ppm.")).arg(deltaPpm, 0, 'f', 1);
        }
    }

    if (qAbs(result.ppm) >= kFailPpm) {
        result.message = L(QStringLiteral("Measured %1 Hz, %2, but this is near the safety limit; check audio device/sample-rate selection.%3"))
                             .arg(result.measuredHz, 0, 'f', 3)
                             .arg(ppmText(result.ppm))
                             .arg(stability);
        return result;
    }

    result.ok = true;
    QString warning;
    if (qAbs(result.ppm) > kWarnPpm) {
        warning = L(QStringLiteral(" Large correction; repeat with a longer run to confirm."));
    }
    result.message = L(QStringLiteral("Measured %1 Hz over %2 s, %3.%4%5"))
                         .arg(result.measuredHz, 0, 'f', 3)
                         .arg(result.elapsedSec, 0, 'f', 1)
                         .arg(ppmText(result.ppm))
                         .arg(warning)
                         .arg(stability);
    updateProgress(m_rxProgress, 100);
    return result;
}

SoundCardCalibrationDialog::MeasurementResult SoundCardCalibrationDialog::measureTxClock()
{
    MeasurementResult result;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QAudioDevice audioDevice = selectOutputDevice(m_settings.audioOutputName);
#else
    QAudioDeviceInfo audioDevice = selectOutputDevice(m_settings.audioOutputName);
#endif

    QAudioFormat format;
    QString error;
    if (!makeInt16MonoFormat(audioDevice, m_settings.audioSampleRate, format, error)) {
        result.message = error;
        return result;
    }

    const int bytesPerFrame = qMax(1, format.channelCount() * static_cast<int>(sizeof(qint16)));
    const int channels = qMax(1, format.channelCount());
    const int bufferFrames = qMax(1024, format.sampleRate() / 10);
    const int periodFrames = qMax(256, format.sampleRate() / 50);

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QAudioSink audioOutput(audioDevice, format, this);
    audioOutput.setBufferSize(bufferFrames * bytesPerFrame);
    QIODevice *device = audioOutput.start();
#else
    QAudioOutput audioOutput(audioDevice, format, this);
    audioOutput.setBufferSize(bufferFrames * bytesPerFrame);
    QIODevice *device = audioOutput.start();
#endif

    if (device == nullptr) {
        result.message = L(QStringLiteral("Unable to start selected TX output device."));
        return result;
    }

    const qint64 durationNs = durationNsFromSeconds(m_spinDurationSec != nullptr ? m_spinDurationSec->value() : kDefaultDurationSec);
    const qint64 halfNs = durationNs / 2;
    qint64 writtenBytes = 0;
    qint64 midBytes = 0;
    qint64 midNs = 0;
    bool midCaptured = false;
    bool counting = false;
    QElapsedTimer timer;
    timer.start();
    qint64 countStartNs = 0;
    qint64 countEndNs = 0;
    double phase = 0.0;
    QByteArray chunk(periodFrames * bytesPerFrame, char(0));

    while (!m_cancelRequested) {
        QApplication::processEvents(QEventLoop::AllEvents, 10);
        const qint64 elapsedNs = timer.nsecsElapsed();

        const int freeBytes = audioOutput.bytesFree();
        if (freeBytes >= bytesPerFrame) {
            const int wanted = qMin(chunk.size(), freeBytes - (freeBytes % bytesPerFrame));
            if (wanted > 0) {
                if (chunk.size() != wanted) {
                    chunk.resize(wanted);
                }
                fillTone(chunk, bytesPerFrame, channels, format.sampleRate(), phase);
                const qint64 wrote = device->write(chunk.constData(), chunk.size());
                if (counting && wrote > 0) {
                    writtenBytes += wrote - (wrote % bytesPerFrame);
                }
            }
        }

        if (!counting && elapsedNs >= static_cast<qint64>(kLeadInMs) * 1000000LL) {
            counting = true;
            countStartNs = timer.nsecsElapsed();
            countEndNs = countStartNs;
            writtenBytes = 0;
            midCaptured = false;
        }

        if (counting) {
            countEndNs = timer.nsecsElapsed();
            const qint64 sinceStartNs = countEndNs - countStartNs;
            if (!midCaptured && sinceStartNs >= halfNs) {
                midCaptured = true;
                midBytes = writtenBytes;
                midNs = sinceStartNs;
            }
            updateProgress(m_txProgress, static_cast<int>((100.0 * static_cast<double>(sinceStartNs)) /
                                                          static_cast<double>(durationNs)));
            if (sinceStartNs >= durationNs) {
                break;
            }
        }

        QThread::msleep(2);
    }

    audioOutput.stop();

    if (m_cancelRequested) {
        result.message = L(QStringLiteral("Cancelled by user."));
        return result;
    }

    result.nominalHz = static_cast<double>(format.sampleRate());
    result.elapsedSec = static_cast<double>(countEndNs - countStartNs) / 1000000000.0;
    result.frames = writtenBytes / bytesPerFrame;

    if (result.elapsedSec <= 0.0 || result.frames <= 0) {
        result.message = L(QStringLiteral("No TX frames were accepted by the audio backend."));
        return result;
    }

    result.measuredHz = static_cast<double>(result.frames) / result.elapsedSec;
    result.ppm = ppmFromMeasured(result.measuredHz, result.nominalHz);

    QString stability;
    if (midCaptured && midNs > 0 && (countEndNs - countStartNs - midNs) > 0) {
        const double firstRate = static_cast<double>(midBytes / bytesPerFrame) /
                                 (static_cast<double>(midNs) / 1000000000.0);
        const double secondRate = static_cast<double>((writtenBytes - midBytes) / bytesPerFrame) /
                                  (static_cast<double>(countEndNs - countStartNs - midNs) / 1000000000.0);
        const double deltaPpm = qAbs(ppmFromMeasured(secondRate, firstRate));
        if (std::isfinite(deltaPpm) && deltaPpm > 100.0) {
            stability = L(QStringLiteral(" Stability warning: first/second half differ by %1 ppm.")).arg(deltaPpm, 0, 'f', 1);
        }
    }

    if (qAbs(result.ppm) >= kFailPpm) {
        result.message = L(QStringLiteral("Measured %1 Hz, %2, but this is near the safety limit; the Qt backend may be buffering instead of pacing. Repeat with a longer run or enter manually.%3"))
                             .arg(result.measuredHz, 0, 'f', 3)
                             .arg(ppmText(result.ppm))
                             .arg(stability);
        return result;
    }

    result.ok = true;
    QString warning;
    if (qAbs(result.ppm) > kWarnPpm) {
        warning = L(QStringLiteral(" Large correction; repeat with a longer run to confirm."));
    }
    result.message = L(QStringLiteral("Measured %1 Hz over %2 s, %3.%4%5"))
                         .arg(result.measuredHz, 0, 'f', 3)
                         .arg(result.elapsedSec, 0, 'f', 1)
                         .arg(ppmText(result.ppm))
                         .arg(warning)
                         .arg(stability);
    updateProgress(m_txProgress, 100);
    return result;
}
