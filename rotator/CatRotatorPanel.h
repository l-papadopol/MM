#ifndef CATROTATORPANEL_H
#define CATROTATORPANEL_H

#include "CatRotatorController.h"

#include <QWidget>

class QLabel;
class QPushButton;
class QDoubleSpinBox;
class QLineEdit;
class QComboBox;
class QProgressBar;
class QRadioButton;
class QButtonGroup;

namespace mm {

class NavballWidget;

class CatRotatorPanel final : public QWidget
{
    Q_OBJECT
public:
    explicit CatRotatorPanel(CatRotatorController *controller, QWidget *parent = nullptr);

signals:
    void requestDirectTarget(const QString &text);

public slots:
    void applyConfig(const CatRotatorController::Config &config);
    void updateQsoTarget(const CatRotatorController::QsoTarget &target);
    void refreshState();

private:
    void buildUi();
    QString azElText(double az, double el) const;
    QString friendlyStatusText() const;
    void updateStatusLabel();
    void updateTrackingModeControls();

    CatRotatorController *m_controller = nullptr;
    CatRotatorController::Config m_config;
    CatRotatorController::QsoTarget m_qsoTarget;

    QLabel *m_lblConnection = nullptr;
    QLabel *m_lblCurrentAz = nullptr;
    QLabel *m_lblCurrentEl = nullptr;
    NavballWidget *m_navball = nullptr;
    QLabel *m_lblTarget = nullptr;
    QLabel *m_lblQso = nullptr;
    QLabel *m_lblEta = nullptr;
    QLabel *m_lblMoon = nullptr;
    QLabel *m_lblStatus = nullptr;
    QButtonGroup *m_trackingModeGroup = nullptr;
    QRadioButton *m_radioManual = nullptr;
    QRadioButton *m_radioQso = nullptr;
    QRadioButton *m_radioMoon = nullptr;
    QProgressBar *m_calibrationProgress = nullptr;
    QLineEdit *m_editDirectTarget = nullptr;
    QPushButton *m_btnDirectTarget = nullptr;
    QDoubleSpinBox *m_spinAz = nullptr;
    QDoubleSpinBox *m_spinEl = nullptr;
    QLineEdit *m_editStep = nullptr;
    QPushButton *m_btnConnect = nullptr;
    QPushButton *m_btnStop = nullptr;
    QPushButton *m_btnGo = nullptr;
    QPushButton *m_btnTrack = nullptr;
    QPushButton *m_btnPark = nullptr;
};

} // namespace mm

#endif // CATROTATORPANEL_H
