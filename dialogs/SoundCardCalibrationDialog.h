#ifndef SOUNDCARDCALIBRATIONDIALOG_H
#define SOUNDCARDCALIBRATIONDIALOG_H

#include "../settings/AppSettings.h"

#include <QDialog>

#include <functional>

class QLabel;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QDoubleSpinBox;
class QDialogButtonBox;

/**
 * @brief QSSTV-style sound-card clock calibration dialog.
 *
 * This dialog follows QSSTV's real calibration principle: count audio frames
 * delivered by the selected RX device, or accepted by the selected TX device,
 * against a monotonic system clock.  It is not an audio loopback test and it
 * does not inspect the audio content.  A stable NTP/timesyncd-disciplined PC
 * clock is therefore the external reference.
 */
class SoundCardCalibrationDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit SoundCardCalibrationDialog(const AppSettings &settings,
                                        QWidget *parent = nullptr);

    AppSettings settings() const;
    void setTextTranslator(std::function<QString(const QString &)> translator);

private slots:
    void calibrateRx();
    void calibrateTx();
    void calibrateBoth();
    void cancelMeasurement();
    void applyManualValues();
    void checkClockDiscipline();

private:
    struct MeasurementResult
    {
        bool ok = false;
        double nominalHz = 0.0;
        double measuredHz = 0.0;
        double ppm = 0.0;
        double elapsedSec = 0.0;
        qint64 frames = 0;
        QString message;
    };

    MeasurementResult measureRxClock();
    MeasurementResult measureTxClock();

    void buildUi();
    void refreshLabels();
    void setBusy(bool busy);
    void updateProgress(QProgressBar *bar, int percent);
    void storeRxResult(const MeasurementResult &result);
    void storeTxResult(const MeasurementResult &result);
    QString systemClockStatusText() const;
    QString L(const QString &source) const;
    void setStatusText(const QString &text);
    void updateStandardButtonText();

    AppSettings m_settings;
    std::function<QString(const QString &)> m_textTranslator;

    QLabel *m_lblClockStatus = nullptr;
    QLabel *m_lblInput = nullptr;
    QLabel *m_lblOutput = nullptr;
    QLabel *m_lblNominal = nullptr;
    QLabel *m_lblRxMeasured = nullptr;
    QLabel *m_lblTxMeasured = nullptr;
    QLabel *m_lblRxPpm = nullptr;
    QLabel *m_lblTxPpm = nullptr;
    QLabel *m_lblStatus = nullptr;
    QProgressBar *m_rxProgress = nullptr;
    QProgressBar *m_txProgress = nullptr;
    QSpinBox *m_spinDurationSec = nullptr;
    QDoubleSpinBox *m_spinManualRxHz = nullptr;
    QDoubleSpinBox *m_spinManualTxHz = nullptr;
    QPushButton *m_btnRx = nullptr;
    QPushButton *m_btnTx = nullptr;
    QPushButton *m_btnBoth = nullptr;
    QPushButton *m_btnCancelRun = nullptr;
    QPushButton *m_btnApplyManual = nullptr;
    QPushButton *m_btnCheckClock = nullptr;
    QDialogButtonBox *m_buttonBox = nullptr;

    bool m_cancelRequested = false;
    bool m_busy = false;
};

#endif // SOUNDCARDCALIBRATIONDIALOG_H
