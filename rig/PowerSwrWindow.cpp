#include "PowerSwrWindow.h"

/*
 * Power and SWR window (IC8TEM, from MSHV-IC8TEM). See PowerSwrWindow.h.
 * Copyright (C) 2026 Costantino Cerrotta, IC8TEM. GNU GPL v3 or later.
 */

#include "../settings/AppSettings.h"
#include "../utils/RuntimeI18n.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QSlider>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include <cmath>

namespace {

QString uiText(const char *key, const char *fallback)
{
    return MadModemI18n::translate(QString::fromUtf8(key), QString::fromUtf8(fallback));
}

struct Band
{
    const char *name;
    double lowMHz;
    double highMHz;
};

const Band kBands[] = {
    {"160 m", 1.8, 2.0},     {"80 m", 3.5, 4.0},     {"60 m", 5.25, 5.45},  {"40 m", 7.0, 7.3},
    {"30 m", 10.1, 10.15},   {"20 m", 14.0, 14.35},  {"17 m", 18.068, 18.168}, {"15 m", 21.0, 21.45},
    {"12 m", 24.89, 24.99},  {"10 m", 28.0, 29.7},   {"6 m", 50.0, 54.0},   {"4 m", 70.0, 71.0},
    {"2 m", 144.0, 148.0},   {"1.25 m", 222.0, 225.0}, {"70 cm", 420.0, 450.0}, {"23 cm", 1240.0, 1300.0},
};
constexpr int kBandCount = static_cast<int>(sizeof(kBands) / sizeof(kBands[0]));

int bandRow(const QString &band)
{
    for (int i = 0; i < kBandCount; ++i) {
        if (band == QLatin1String(kBands[i].name)) {
            return i;
        }
    }
    return -1;
}

QString settingsPath()
{
    return AppSettings::settingsFilePath();
}

} // namespace

QString PowerSwrWindow::bandName(double frequencyHz)
{
    const double mhz = frequencyHz / 1.0e6;
    for (const Band &b : kBands) {
        if (mhz >= b.lowMHz && mhz <= b.highMHz) {
            return QString::fromLatin1(b.name);
        }
    }
    return QString();
}

PowerSwrWindow::PowerSwrWindow(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(uiText("pwr_title", "Power and SWR"));
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setModal(false);
    buildUi();
    loadSettings();
    resetMeters();
    updateStatus();
}

void PowerSwrWindow::buildUi()
{
    auto *tabs = new QTabWidget(this);

    // ---------------------------------------------------------------- main page
    auto *main = new QWidget(tabs);
    auto *powerBox = new QGroupBox(uiText("pwr_rf_power", "RF power"), main);
    // Labels with a digit: MadModem's widget translator turns symbol-only texts into "...".
    auto *minus = new QPushButton(QStringLiteral("\u22121"), powerBox);
    auto *plus = new QPushButton(QStringLiteral("+1"), powerBox);
    minus->setToolTip(uiText("pwr_decrease", "Decrease power"));
    plus->setToolTip(uiText("pwr_increase", "Increase power"));
    minus->setMinimumWidth(44);
    plus->setMinimumWidth(44);
    m_slider = new QSlider(Qt::Horizontal, powerBox);
    m_slider->setRange(0, 100);
    m_slider->setPageStep(5);
    m_powerLabel = new QLabel(powerBox);
    m_powerLabel->setMinimumWidth(110);
    auto *sliderRow = new QHBoxLayout();
    sliderRow->addWidget(minus);
    sliderRow->addWidget(m_slider, 1);
    sliderRow->addWidget(plus);
    sliderRow->addWidget(m_powerLabel);
    auto *presetRow = new QHBoxLayout();
    for (int pct : {10, 25, 35, 50, 100}) {
        auto *b = new QPushButton(QStringLiteral("%1 %").arg(pct), powerBox);
        connect(b, &QPushButton::clicked, this, [this, pct]() { requestPercent(pct); });
        presetRow->addWidget(b);
    }
    m_memoryLabel = new QLabel(powerBox);
    auto *save = new QPushButton(uiText("pwr_save_band", "Save for this band"), powerBox);
    auto *read = new QPushButton(uiText("pwr_read", "Read from radio"), powerBox);
    auto *memoryRow = new QHBoxLayout();
    memoryRow->addWidget(m_memoryLabel, 1);
    memoryRow->addWidget(save);
    memoryRow->addWidget(read);
    auto *powerLayout = new QVBoxLayout(powerBox);
    powerLayout->addLayout(sliderRow);
    powerLayout->addLayout(presetRow);
    powerLayout->addLayout(memoryRow);

    auto *meterBox = new QGroupBox(uiText("pwr_meters", "Measurements while transmitting"), main);
    auto makeBar = [meterBox](int max) {
        auto *bar = new QProgressBar(meterBox);
        bar->setRange(0, max);
        bar->setTextVisible(false);
        bar->setFixedHeight(14);
        return bar;
    };
    m_poBar = makeBar(1000);
    m_swrBar = makeBar(1000);
    m_alcBar = makeBar(1000);
    m_poValue = new QLabel(meterBox);
    m_swrValue = new QLabel(meterBox);
    m_alcValue = new QLabel(meterBox);
    for (QLabel *l : {m_poValue, m_swrValue, m_alcValue}) {
        l->setMinimumWidth(80);
    }
    auto *grid = new QGridLayout();
    grid->addWidget(new QLabel(QStringLiteral("Po"), meterBox), 0, 0);
    grid->addWidget(m_poBar, 0, 1);
    grid->addWidget(m_poValue, 0, 2);
    grid->addWidget(new QLabel(uiText("pwr_swr", "SWR"), meterBox), 1, 0);
    grid->addWidget(m_swrBar, 1, 1);
    grid->addWidget(m_swrValue, 1, 2);
    grid->addWidget(new QLabel(QStringLiteral("ALC"), meterBox), 2, 0);
    grid->addWidget(m_alcBar, 2, 1);
    grid->addWidget(m_alcValue, 2, 2);
    grid->setColumnStretch(1, 1);
    m_blockLabel = new QLabel(meterBox);
    m_blockLabel->setWordWrap(true);
    m_blockLabel->setStyleSheet(QStringLiteral("color:#ff5050;font-weight:bold;"));
    m_unlock = new QPushButton(uiText("pwr_unlock", "Unlock"), meterBox);
    m_unlock->setEnabled(false);
    m_warnLabel = new QLabel(meterBox);
    m_warnLabel->setWordWrap(true);
    m_warnLabel->setStyleSheet(QStringLiteral("color:#ffc040;"));
    auto *blockRow = new QHBoxLayout();
    blockRow->addWidget(m_blockLabel, 1);
    blockRow->addWidget(m_unlock);
    auto *meterLayout = new QVBoxLayout(meterBox);
    meterLayout->addLayout(grid);
    meterLayout->addWidget(m_warnLabel);
    meterLayout->addLayout(blockRow);

    auto *mainLayout = new QVBoxLayout(main);
    mainLayout->addWidget(powerBox);
    mainLayout->addWidget(meterBox);
    mainLayout->addStretch(1);
    tabs->addTab(main, uiText("pwr_tab_power", "Power"));

    // ---------------------------------------------------------------- settings page
    auto *settingsPage = new QWidget(tabs);
    auto *bandsBox = new QGroupBox(uiText("pwr_per_band", "Power per band"), settingsPage);
    m_bands = new QTableWidget(kBandCount, 3, bandsBox);
    m_bands->setHorizontalHeaderLabels({uiText("pwr_band", "Band"), uiText("pwr_power_pct", "Power %"),
                                        uiText("pwr_tune_pct", "Tune %")});
    m_bands->verticalHeader()->setVisible(false);
    m_bands->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    for (int i = 0; i < kBandCount; ++i) {
        auto *name = new QTableWidgetItem(QString::fromLatin1(kBands[i].name));
        name->setFlags(Qt::ItemIsEnabled);
        m_bands->setItem(i, 0, name);
        m_bands->setItem(i, 1, new QTableWidgetItem());
        m_bands->setItem(i, 2, new QTableWidgetItem(QStringLiteral("20")));
    }
    m_bands->setMinimumHeight(220);
    m_applyOnBand = new QCheckBox(uiText("pwr_apply_band", "Set the stored power when the band changes"), bandsBox);
    m_perMode = new QCheckBox(uiText("pwr_per_mode", "Separate memory for each mode (e.g. 2 m MSK144 and 2 m FT8)"), bandsBox);
    m_tuneReduced = new QCheckBox(uiText("pwr_tune_reduced", "Tune at the Tune % of the band (FT Tune button)"), bandsBox);
    auto *bandsLayout = new QVBoxLayout(bandsBox);
    bandsLayout->addWidget(m_bands);
    bandsLayout->addWidget(m_applyOnBand);
    bandsLayout->addWidget(m_perMode);
    bandsLayout->addWidget(m_tuneReduced);

    auto *protectBox = new QGroupBox(uiText("pwr_protection", "Protection"), settingsPage);
    m_protect = new QCheckBox(uiText("pwr_stop_swr", "Stop TX if SWR is above"), protectBox);
    m_swrLimit = new QDoubleSpinBox(protectBox);
    m_swrLimit->setRange(1.3, 10.0);
    m_swrLimit->setSingleStep(0.1);
    m_swrLimit->setDecimals(1);
    m_swrSeconds = new QDoubleSpinBox(protectBox);
    m_swrSeconds->setRange(0.2, 10.0);
    m_swrSeconds->setSingleStep(0.1);
    m_swrSeconds->setDecimals(1);
    m_swrSeconds->setSuffix(QStringLiteral(" s"));
    auto *protectRow = new QHBoxLayout();
    protectRow->addWidget(m_protect);
    protectRow->addWidget(m_swrLimit);
    protectRow->addWidget(new QLabel(uiText("pwr_for_more_than", "for more than"), protectBox));
    protectRow->addWidget(m_swrSeconds);
    protectRow->addStretch(1);
    m_alcWarn = new QCheckBox(uiText("pwr_alc_warn", "Warn if ALC is above 50 % (audio too high in digital modes)"), protectBox);
    m_poWarn = new QCheckBox(uiText("pwr_po_warn", "Warn if Po stays below 50 % of the set power"), protectBox);
    auto *transverter = new QLabel(uiText("pwr_transverter", "With a transverter the radio measures the drive power and the SWR towards the transverter, not those at the antenna."), protectBox);
    transverter->setWordWrap(true);
    auto *protectLayout = new QVBoxLayout(protectBox);
    protectLayout->addLayout(protectRow);
    protectLayout->addWidget(m_alcWarn);
    protectLayout->addWidget(m_poWarn);
    protectLayout->addWidget(transverter);

    auto *settingsLayout = new QVBoxLayout(settingsPage);
    settingsLayout->addWidget(bandsBox, 1);
    settingsLayout->addWidget(protectBox);
    tabs->addTab(settingsPage, uiText("pwr_tab_settings", "Settings"));

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    auto *outer = new QVBoxLayout(this);
    outer->addWidget(tabs, 1);
    outer->addWidget(m_statusLabel);
    resize(560, 520);

    connect(minus, &QPushButton::clicked, this, [this]() { requestPercent(m_slider->value() - 1); });
    connect(plus, &QPushButton::clicked, this, [this]() { requestPercent(m_slider->value() + 1); });
    connect(m_slider, &QSlider::valueChanged, this, [this](int v) { showPercent(v); });
    connect(m_slider, &QSlider::sliderReleased, this, [this]() { requestPercent(m_slider->value()); });
    connect(m_slider, &QSlider::actionTriggered, this, [this](int action) {
        if (action != QAbstractSlider::SliderMove) {
            QMetaObject::invokeMethod(this, [this]() { requestPercent(m_slider->value()); }, Qt::QueuedConnection);
        }
    });
    connect(save, &QPushButton::clicked, this, [this]() {
        if (m_band.isEmpty()) {
            return;
        }
        const int pct = m_slider->value();
        if (m_perMode->isChecked() && !m_mode.isEmpty()) {
            m_modeMemory.insert(memoryKey(), pct);
        } else {
            const int row = bandRow(m_band);
            if (row >= 0) {
                m_bands->item(row, 1)->setText(QString::number(pct));
            }
        }
        saveSettings();
        refreshMemoryLabel();
    });
    connect(read, &QPushButton::clicked, this, &PowerSwrWindow::readPowerRequested);
    connect(m_unlock, &QPushButton::clicked, this, [this]() {
        m_blocked = false;
        m_blockReason.clear();
        m_blockLabel->clear();
        m_unlock->setEnabled(false);
        emit unlocked();
    });
    connect(m_bands, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *item) {
        if (m_loading || item == nullptr || item->column() == 0) {
            return;
        }
        bool ok = false;
        const int v = item->text().trimmed().toInt(&ok);
        if (!item->text().trimmed().isEmpty() && (!ok || v < 0 || v > 100)) {
            m_loading = true;
            item->setText(QString());
            m_loading = false;
        }
        saveSettings();
        refreshMemoryLabel();
    });
    for (QCheckBox *box : {m_applyOnBand, m_perMode, m_tuneReduced, m_protect, m_alcWarn, m_poWarn}) {
        connect(box, &QCheckBox::toggled, this, [this]() {
            saveSettings();
            refreshMemoryLabel();
        });
    }
    for (QDoubleSpinBox *box : {m_swrLimit, m_swrSeconds}) {
        connect(box, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this]() { saveSettings(); });
    }
}

void PowerSwrWindow::loadSettings()
{
    m_loading = true;
    QSettings s(settingsPath(), QSettings::IniFormat);
    s.beginGroup(QStringLiteral("PowerSwr"));
    m_applyOnBand->setChecked(s.value(QStringLiteral("applyOnBand"), true).toBool());
    m_perMode->setChecked(s.value(QStringLiteral("perMode"), false).toBool());
    m_tuneReduced->setChecked(s.value(QStringLiteral("tuneReduced"), true).toBool());
    m_protect->setChecked(s.value(QStringLiteral("protect"), true).toBool());
    m_swrLimit->setValue(s.value(QStringLiteral("swrLimit"), 2.5).toDouble());
    m_swrSeconds->setValue(s.value(QStringLiteral("swrSeconds"), 1.0).toDouble());
    m_alcWarn->setChecked(s.value(QStringLiteral("alcWarn"), true).toBool());
    m_poWarn->setChecked(s.value(QStringLiteral("poWarn"), false).toBool());
    for (int i = 0; i < kBandCount; ++i) {
        const QString key = QString::fromLatin1(kBands[i].name).remove(QLatin1Char(' '));
        const QVariant power = s.value(QStringLiteral("band/%1/power").arg(key));
        m_bands->item(i, 1)->setText(power.isValid() ? QString::number(power.toInt()) : QString());
        m_bands->item(i, 2)->setText(QString::number(s.value(QStringLiteral("band/%1/tune").arg(key), 20).toInt()));
    }
    s.beginGroup(QStringLiteral("mode"));
    for (const QString &key : s.childKeys()) {
        m_modeMemory.insert(QString(key).replace(QLatin1Char('_'), QLatin1Char('|')), s.value(key).toInt());
    }
    s.endGroup();
    s.endGroup();
    m_loading = false;
}

void PowerSwrWindow::saveSettings() const
{
    if (m_loading) {
        return;
    }
    QSettings s(settingsPath(), QSettings::IniFormat);
    s.beginGroup(QStringLiteral("PowerSwr"));
    s.setValue(QStringLiteral("applyOnBand"), m_applyOnBand->isChecked());
    s.setValue(QStringLiteral("perMode"), m_perMode->isChecked());
    s.setValue(QStringLiteral("tuneReduced"), m_tuneReduced->isChecked());
    s.setValue(QStringLiteral("protect"), m_protect->isChecked());
    s.setValue(QStringLiteral("swrLimit"), m_swrLimit->value());
    s.setValue(QStringLiteral("swrSeconds"), m_swrSeconds->value());
    s.setValue(QStringLiteral("alcWarn"), m_alcWarn->isChecked());
    s.setValue(QStringLiteral("poWarn"), m_poWarn->isChecked());
    for (int i = 0; i < kBandCount; ++i) {
        const QString key = QString::fromLatin1(kBands[i].name).remove(QLatin1Char(' '));
        const QString power = m_bands->item(i, 1)->text().trimmed();
        if (power.isEmpty()) {
            s.remove(QStringLiteral("band/%1/power").arg(key));
        } else {
            s.setValue(QStringLiteral("band/%1/power").arg(key), power.toInt());
        }
        s.setValue(QStringLiteral("band/%1/tune").arg(key), m_bands->item(i, 2)->text().trimmed().toInt());
    }
    s.remove(QStringLiteral("mode"));
    s.beginGroup(QStringLiteral("mode"));
    for (auto it = m_modeMemory.cbegin(); it != m_modeMemory.cend(); ++it) {
        s.setValue(QString(it.key()).replace(QLatin1Char('|'), QLatin1Char('_')).remove(QLatin1Char(' ')), it.value());
    }
    s.endGroup();
    s.endGroup();
}

QString PowerSwrWindow::memoryKey() const
{
    return QString(m_band).remove(QLatin1Char(' ')) + QLatin1Char('|') + m_mode;
}

int PowerSwrWindow::storedPercent() const
{
    if (m_band.isEmpty()) {
        return -1;
    }
    if (m_perMode->isChecked() && !m_mode.isEmpty() && m_modeMemory.contains(memoryKey())) {
        return m_modeMemory.value(memoryKey());
    }
    const int row = bandRow(m_band);
    if (row < 0) {
        return -1;
    }
    bool ok = false;
    const int v = m_bands->item(row, 1)->text().trimmed().toInt(&ok);
    return ok ? v : -1;
}

int PowerSwrWindow::tunePercent() const
{
    if (!m_tuneReduced->isChecked() || m_band.isEmpty()) {
        return -1;
    }
    const int row = bandRow(m_band);
    if (row < 0) {
        return -1;
    }
    bool ok = false;
    const int v = m_bands->item(row, 2)->text().trimmed().toInt(&ok);
    return ok && v > 0 ? qBound(1, v, 100) : -1;
}

void PowerSwrWindow::refreshMemoryLabel()
{
    if (m_band.isEmpty()) {
        m_memoryLabel->setText(uiText("pwr_no_band", "Band: not known (CAT frequency outside the amateur bands)"));
        return;
    }
    const int stored = storedPercent();
    const QString where = m_perMode->isChecked() && !m_mode.isEmpty() ? m_band + QStringLiteral(" ") + m_mode : m_band;
    m_memoryLabel->setText(stored >= 0 ? uiText("pwr_stored", "%1: stored %2 %").arg(where).arg(stored)
                                       : uiText("pwr_not_stored", "%1: nothing stored").arg(where));
}

void PowerSwrWindow::setContext(double frequencyHz, const QString &mode)
{
    const QString band = bandName(frequencyHz);
    const bool changed = band != m_band || mode != m_mode;
    m_band = band;
    m_mode = mode;
    refreshMemoryLabel();
    if (!changed || !m_applyOnBand->isChecked() || !m_canSetPower || m_transmitting) {
        return;
    }
    const int stored = storedPercent();
    if (stored >= 0 && stored != m_rigPercent) {
        requestPercent(stored);
    }
}

void PowerSwrWindow::requestPercent(int percent)
{
    percent = qBound(0, percent, 100);
    showPercent(percent);
    if (!m_canSetPower) {
        updateStatus();
        return;
    }
    emit powerRequested(percent / 100.0);
}

void PowerSwrWindow::showPercent(int percent)
{
    if (m_slider->value() != percent) {
        const bool blocked = m_slider->blockSignals(true);
        m_slider->setValue(percent);
        m_slider->blockSignals(blocked);
    }
    QString text = QStringLiteral("%1 %").arg(percent);
    if (m_fullScaleWatts > 0.0) {
        text += QStringLiteral(" · %1 W").arg(m_fullScaleWatts * percent / 100.0, 0, 'f', m_fullScaleWatts < 20.0 ? 1 : 0);
    }
    m_powerLabel->setText(text);
}

void PowerSwrWindow::setRigPower(double fraction, double fullScaleWatts)
{
    m_rigPercent = static_cast<int>(std::lround(fraction * 100.0));
    m_fullScaleWatts = fullScaleWatts;
    if (!m_slider->isSliderDown()) {
        showPercent(m_rigPercent);
    }
    updateStatus();
}

void PowerSwrWindow::setCapabilities(bool meters, bool powerControl)
{
    m_canMeter = meters;
    m_canSetPower = powerControl;
    m_connected = meters || powerControl || m_connected;
    updateStatus();
}

void PowerSwrWindow::setConnected(bool connected)
{
    m_connected = connected;
    if (!connected) {
        m_canMeter = false;
        m_canSetPower = false;
        m_rigPercent = -1;
    }
    updateStatus();
}

void PowerSwrWindow::updateStatus()
{
    QString text;
    if (!m_connected) {
        text = uiText("pwr_status_off", "CAT not connected: power and meters need the Hamlib CAT connection (Settings › CAT).");
    } else {
        text = uiText("pwr_status_on", "CAT connected. Meters: %1. Power control: %2.")
                   .arg(m_canMeter ? uiText("pwr_yes", "yes") : uiText("pwr_no", "not offered by this radio"),
                        m_canSetPower ? uiText("pwr_yes", "yes") : uiText("pwr_no", "not offered by this radio"));
        if (m_fullScaleWatts > 0.0) {
            text += QStringLiteral(" ") + uiText("pwr_full_scale", "Full scale on this band: %1 W.").arg(m_fullScaleWatts, 0, 'f', 0);
        }
    }
    m_statusLabel->setText(text);
}

void PowerSwrWindow::resetMeters()
{
    m_poBar->setValue(0);
    m_swrBar->setValue(0);
    m_alcBar->setValue(0);
    const QString dash; // empty: a lone symbol would be translated to "..."
    m_poValue->setText(dash);
    m_swrValue->setText(dash);
    m_alcValue->setText(dash);
}

void PowerSwrWindow::setTransmitting(bool on)
{
    if (on == m_transmitting) {
        return;
    }
    m_transmitting = on;
    m_swrHigh.invalidate();
    m_lowPoSeconds = 0.0;
    if (on) {
        m_txClock.start();
        m_warnLabel->clear();
    } else {
        resetMeters();
    }
}

void PowerSwrWindow::setMeters(double watts, double powerFraction, double swr, double alc)
{
    if (!m_transmitting) {
        resetMeters();
        return;
    }
    // Po: watts when the backend converts the meter, otherwise the 0..1 meter.
    if (std::isfinite(watts) && watts >= 0.0) {
        const double scale = m_fullScaleWatts > 0.0 ? m_fullScaleWatts : 100.0;
        m_poBar->setValue(qBound(0, static_cast<int>(1000.0 * watts / scale), 1000));
        m_poValue->setText(QStringLiteral("%1 W").arg(watts, 0, 'f', watts < 20.0 ? 1 : 0));
    } else if (std::isfinite(powerFraction)) {
        m_poBar->setValue(qBound(0, static_cast<int>(1000.0 * powerFraction), 1000));
        m_poValue->setText(QStringLiteral("%1 %").arg(100.0 * powerFraction, 0, 'f', 0));
    }
    if (std::isfinite(swr) && swr >= 1.0) {
        // 1.0 .. 5.0 on the bar
        m_swrBar->setValue(qBound(0, static_cast<int>(1000.0 * (swr - 1.0) / 4.0), 1000));
        m_swrValue->setText(QStringLiteral("%1").arg(swr, 0, 'f', 2));
    }
    if (std::isfinite(alc) && alc >= 0.0) {
        m_alcBar->setValue(qBound(0, static_cast<int>(1000.0 * alc), 1000));
        m_alcValue->setText(QStringLiteral("%1 %").arg(100.0 * alc, 0, 'f', 0));
    }

    QStringList warnings;
    if (m_alcWarn->isChecked() && std::isfinite(alc) && alc > 0.5) {
        warnings << uiText("pwr_alc_high", "ALC above 50 %: lower the audio level for a clean digital signal.");
    }
    if (m_poWarn->isChecked() && m_rigPercent > 0 && m_txClock.isValid() && m_txClock.elapsed() > 1500) {
        const double setWatts = m_fullScaleWatts * m_rigPercent / 100.0;
        const bool low = std::isfinite(watts) && setWatts > 0.0 ? watts < 0.5 * setWatts
                       : std::isfinite(powerFraction) && powerFraction < 0.5 * m_rigPercent / 100.0;
        m_lowPoSeconds = low ? m_lowPoSeconds + 0.3 : 0.0;
        if (m_lowPoSeconds >= 1.5) {
            warnings << uiText("pwr_po_low", "Po below 50 % of the set power: check the audio level and the ALC.");
        }
    }
    m_warnLabel->setText(warnings.join(QLatin1Char('\n')));

    // SWR protection: above the limit for longer than the set time.
    if (!m_protect->isChecked() || m_blocked || !std::isfinite(swr)) {
        return;
    }
    if (swr > m_swrLimit->value()) {
        if (!m_swrHigh.isValid()) {
            m_swrHigh.start();
        }
        if (m_swrHigh.elapsed() >= static_cast<qint64>(1000.0 * m_swrSeconds->value())) {
            m_blocked = true;
            m_blockReason = uiText("pwr_blocked", "TX stopped and blocked: SWR %1 above %2. Check the antenna, then press Unlock.")
                                .arg(swr, 0, 'f', 2).arg(m_swrLimit->value(), 0, 'f', 1);
            m_blockLabel->setText(m_blockReason);
            m_unlock->setEnabled(true);
            emit swrTripped(swr, m_blockReason);
        }
    } else {
        m_swrHigh.invalidate();
    }
}
