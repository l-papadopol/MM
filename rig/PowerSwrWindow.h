#pragma once
/*
 * Power and SWR window (IC8TEM, from MSHV-IC8TEM): RF power with per-band and
 * per-mode memory, tune power, Po / SWR / ALC meters while transmitting and an
 * SWR protection that stops and blocks the transmission. Everything goes through
 * Hamlib (HamlibController), so it works with every radio whose Hamlib backend
 * reads these meters and sets RFPOWER.
 *
 * Copyright (C) 2026 Costantino Cerrotta, IC8TEM. GNU GPL v3 or later.
 */
#include <QDialog>
#include <QElapsedTimer>
#include <QHash>
#include <QString>

class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QSlider;
class QTableWidget;

class PowerSwrWindow : public QDialog
{
    Q_OBJECT
public:
    explicit PowerSwrWindow(QWidget *parent = nullptr);

    /** Band and mode in use: applies the stored power when they change. */
    void setContext(double frequencyHz, const QString &mode);
    /** True after an SWR trip, until Unlock. */
    bool txBlocked() const { return m_blocked; }
    QString blockReason() const { return m_blockReason; }
    /** Tune power of the current band in %, or -1 when tune at reduced power is off. */
    int tunePercent() const;
    /** Last power read from (or sent to) the radio, 0..100, -1 when unknown. */
    int currentPercent() const { return m_rigPercent; }
    bool powerControlAvailable() const { return m_canSetPower; }

    static QString bandName(double frequencyHz);

public slots:
    void setCapabilities(bool meters, bool powerControl);
    void setConnected(bool connected);
    void setRigPower(double fraction, double fullScaleWatts);
    void setMeters(double watts, double powerFraction, double swr, double alc);
    void setTransmitting(bool on);

signals:
    void powerRequested(double fraction);
    void readPowerRequested();
    /** SWR above the limit for longer than the set time while transmitting. */
    void swrTripped(double swr, const QString &reason);
    void unlocked();

private:
    void buildUi();
    void loadSettings();
    void saveSettings() const;
    void requestPercent(int percent);
    void showPercent(int percent);
    void updateStatus();
    QString memoryKey() const;
    int storedPercent() const;
    void refreshMemoryLabel();
    void resetMeters();

    QSlider *m_slider = nullptr;
    QLabel *m_powerLabel = nullptr;
    QLabel *m_memoryLabel = nullptr;
    QProgressBar *m_poBar = nullptr;
    QProgressBar *m_swrBar = nullptr;
    QProgressBar *m_alcBar = nullptr;
    QLabel *m_poValue = nullptr;
    QLabel *m_swrValue = nullptr;
    QLabel *m_alcValue = nullptr;
    QLabel *m_blockLabel = nullptr;
    QLabel *m_warnLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_unlock = nullptr;
    QTableWidget *m_bands = nullptr;
    QCheckBox *m_applyOnBand = nullptr;
    QCheckBox *m_perMode = nullptr;
    QCheckBox *m_tuneReduced = nullptr;
    QCheckBox *m_protect = nullptr;
    QDoubleSpinBox *m_swrLimit = nullptr;
    QDoubleSpinBox *m_swrSeconds = nullptr;
    QCheckBox *m_alcWarn = nullptr;
    QCheckBox *m_poWarn = nullptr;

    bool m_connected = false;
    bool m_canMeter = false;
    bool m_canSetPower = false;
    bool m_transmitting = false;
    bool m_blocked = false;
    QString m_blockReason;
    int m_rigPercent = -1;
    double m_fullScaleWatts = 0.0;
    QString m_band;
    QString m_mode;
    QHash<QString, int> m_modeMemory; // "band|mode" -> %
    QElapsedTimer m_swrHigh;
    QElapsedTimer m_txClock;
    double m_lowPoSeconds = 0.0;
    bool m_loading = false;
};
