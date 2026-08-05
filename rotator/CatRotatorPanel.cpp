#include "CatRotatorPanel.h"
#include "../utils/RuntimeI18n.h"
#include "NavballWidget.h"

#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QFrame>
#include <QGroupBox>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QLineEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QRadioButton>
#include <QButtonGroup>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QSizePolicy>
#include <QtGlobal>



namespace mm {

CatRotatorPanel::CatRotatorPanel(CatRotatorController *controller, QWidget *parent)
    : QWidget(parent), m_controller(controller)
{
    buildUi();
    if (m_controller != nullptr) {
        connect(m_controller, &CatRotatorController::statusChanged,
                this, [this](const QString &) {
            updateStatusLabel();
        });
        connect(m_controller, &CatRotatorController::connectionChanged,
                this, [this](bool) { refreshState(); });
        connect(m_controller, &CatRotatorController::positionChanged,
                this, [this](double, double) { refreshState(); });
        connect(m_controller, &CatRotatorController::targetChanged,
                this, [this](double, double, const QString &) { refreshState(); });
        connect(m_controller, &CatRotatorController::qsoTargetChanged,
                this, &CatRotatorPanel::updateQsoTarget);
        connect(m_controller, &CatRotatorController::trackingModeChanged,
                this, [this](CatRotatorController::TrackingMode) { refreshState(); });
        connect(m_controller, &CatRotatorController::moonTargetChanged,
                this, [this](const CatRotatorController::MoonTarget &) { refreshState(); });
        connect(m_controller, &CatRotatorController::sunTargetChanged,
                this, [this](const CatRotatorController::SunTarget &) { refreshState(); });
        connect(m_controller, &CatRotatorController::motionChanged, this, [this](bool) { refreshState(); });
        connect(m_controller, &CatRotatorController::calibrationProgress, this, [this](int percent, const QString &message) {
            if (m_calibrationProgress != nullptr) {
                m_calibrationProgress->setValue(qBound(0, percent, 100));
                m_calibrationProgress->setVisible(percent > 0 && percent < 100);
            }
            Q_UNUSED(message)
            updateStatusLabel();
            refreshState();
        });
        connect(m_controller, &CatRotatorController::calibrationFinished, this, [this](const CatRotatorController::Config &, const QString &message) {
            if (m_calibrationProgress != nullptr) {
                m_calibrationProgress->setValue(100);
                m_calibrationProgress->setVisible(false);
            }
            Q_UNUSED(message)
            updateStatusLabel();
            refreshState();
        });
    }
    refreshState();
}

void CatRotatorPanel::buildUi()
{
    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(8, 8, 8, 8);
    outer->setSpacing(7);

    // Connection state is shown in the compact status line at the bottom.
    // Do not consume vertical space with a duplicate top label.
    m_lblConnection = nullptr;

    QGridLayout *readout = new QGridLayout();
    readout->setHorizontalSpacing(6);
    readout->setVerticalSpacing(4);
    m_lblCurrentAz = new QLabel(QStringLiteral("--"), this);
    m_lblCurrentEl = new QLabel(QStringLiteral("--"), this);
    QFont big = m_lblCurrentAz->font();
    big.setPointSize(qMax(14, big.pointSize() + 6));
    big.setBold(true);
    m_lblCurrentAz->setFont(big);
    m_lblCurrentEl->setFont(big);
    m_lblCurrentAz->setFrameShape(QFrame::StyledPanel);
    m_lblCurrentEl->setFrameShape(QFrame::StyledPanel);
    m_lblCurrentAz->setAlignment(Qt::AlignCenter);
    m_lblCurrentEl->setAlignment(Qt::AlignCenter);
    readout->addWidget(new QLabel(MadModemI18n::text(QStringLiteral("Az")), this), 0, 0);
    readout->addWidget(new QLabel(MadModemI18n::text(QStringLiteral("El")), this), 0, 1);
    readout->addWidget(m_lblCurrentAz, 1, 0);
    readout->addWidget(m_lblCurrentEl, 1, 1);
    outer->addLayout(readout);

    m_navball = new NavballWidget(this);
    m_navball->setToolTip(MadModemI18n::text(QStringLiteral("Rotator navball: current antenna pointing is centered; TG is the active target. Sun and Moon markers update from local ephemeris when QTH is configured.")));
    m_navball->set_x_size(330);
    m_navball->set_y_size(330);
    outer->addWidget(m_navball, 1);

    QGridLayout *manual = new QGridLayout();
    manual->setHorizontalSpacing(6);
    manual->setVerticalSpacing(6);
    manual->setColumnStretch(0, 0);
    manual->setColumnStretch(1, 1);
    manual->setColumnStretch(2, 0);

    QLabel *lblSetAz = new QLabel(MadModemI18n::text(QStringLiteral("Set Az")), this);
    lblSetAz->setToolTip(MadModemI18n::text(QStringLiteral("Manual azimuth setpoint for the rotator, in degrees.")));
    QLabel *lblSetEl = new QLabel(MadModemI18n::text(QStringLiteral("Set El")), this);
    lblSetEl->setToolTip(MadModemI18n::text(QStringLiteral("Manual elevation setpoint for the rotator, in degrees.")));

    m_spinAz = new QDoubleSpinBox(this);
    m_spinAz->setRange(0.0, 359.9);
    m_spinAz->setDecimals(1);
    m_spinAz->setSuffix(QStringLiteral("°"));
    m_spinAz->setMinimumWidth(118);
    m_spinAz->setMaximumWidth(150);
    m_spinAz->setToolTip(MadModemI18n::text(QStringLiteral("Manual azimuth setpoint. Press Go to command this value.")));

    m_spinEl = new QDoubleSpinBox(this);
    m_spinEl->setRange(-10.0, 180.0);
    m_spinEl->setDecimals(1);
    m_spinEl->setSuffix(QStringLiteral("°"));
    m_spinEl->setMinimumWidth(118);
    m_spinEl->setMaximumWidth(150);
    m_spinEl->setToolTip(MadModemI18n::text(QStringLiteral("Manual elevation setpoint. Press Go to command this value.")));

    m_btnConnect = new QPushButton(MadModemI18n::text(QStringLiteral("Connect")), this);
    m_btnConnect->setToolTip(MadModemI18n::text(QStringLiteral("Connect or disconnect the configured rotator backend.")));
    m_btnStop = new QPushButton(MadModemI18n::text(QStringLiteral("STOP")), this);
    m_btnStop->setToolTip(MadModemI18n::text(QStringLiteral("Stop the current rotator movement immediately.")));
    m_btnStop->setStyleSheet(QStringLiteral("QPushButton { color: #ff3b55; font-weight: 500; }"));

    m_btnGo = new QPushButton(MadModemI18n::text(QStringLiteral("Go")), this);
    m_btnGo->setToolTip(MadModemI18n::text(QStringLiteral("Move the rotator to the manual Set Az / Set El values.")));
    m_btnTrack = new QPushButton(MadModemI18n::text(QStringLiteral("Track QSO")), this);
    m_btnTrack->setToolTip(MadModemI18n::text(QStringLiteral("Track the current QSO/correspondent locator target.")));
    m_btnMoonTrack = new QPushButton(MadModemI18n::text(QStringLiteral("Track Moon / EME")), this);
    m_btnMoonTrack->setToolTip(MadModemI18n::text(QStringLiteral("Switch to Moon / EME tracking and point using local lunar ephemeris.")));
    m_btnPark = new QPushButton(MadModemI18n::text(QStringLiteral("Park")), this);
    m_btnPark->setToolTip(MadModemI18n::text(QStringLiteral("Move the rotator to the configured park position.")));

    manual->addWidget(lblSetAz, 0, 0);
    manual->addWidget(m_spinAz, 0, 1, Qt::AlignLeft);
    manual->addWidget(m_btnConnect, 0, 2);
    manual->addWidget(lblSetEl, 1, 0);
    manual->addWidget(m_spinEl, 1, 1, Qt::AlignLeft);
    manual->addWidget(m_btnStop, 1, 2);
    manual->addWidget(m_btnGo, 2, 0, 1, 2);
    manual->addWidget(m_btnPark, 2, 2);
    manual->addWidget(m_btnTrack, 3, 0, 1, 2);
    manual->addWidget(m_btnMoonTrack, 3, 2);
    outer->addLayout(manual);

    QGroupBox *trackingBox = new QGroupBox(this);
    QVBoxLayout *trackingLayout = new QVBoxLayout(trackingBox);
    trackingLayout->setContentsMargins(8, 8, 8, 8);
    trackingLayout->setSpacing(4);
    m_trackingModeGroup = new QButtonGroup(trackingBox);
    m_radioQso = new QRadioButton(MadModemI18n::text(QStringLiteral("QSO / correspondent locator")), trackingBox);
    m_radioQso->setToolTip(MadModemI18n::text(QStringLiteral("Use the current QSO or selected correspondent locator as rotator target.")));
    m_radioMoon = new QRadioButton(MadModemI18n::text(QStringLiteral("Moon / EME")), trackingBox);
    m_radioMoon->setToolTip(MadModemI18n::text(QStringLiteral("Bypass the QSO locator and track the Moon for EME operation.")));
    m_radioManual = new QRadioButton(MadModemI18n::text(QStringLiteral("Manual az/el")), trackingBox);
    m_radioManual->setToolTip(MadModemI18n::text(QStringLiteral("Use manual azimuth/elevation values instead of automatic tracking.")));
    m_trackingModeGroup->addButton(m_radioQso, 0);
    m_trackingModeGroup->addButton(m_radioMoon, 1);
    m_trackingModeGroup->addButton(m_radioManual, 2);
    trackingLayout->addWidget(m_radioQso);
    trackingLayout->addWidget(m_radioMoon);
    trackingLayout->addWidget(m_radioManual);
    m_lblMoon = new QLabel(MadModemI18n::text(QStringLiteral("Moon: --")), trackingBox);
    m_lblMoon->setWordWrap(true);
    trackingLayout->addWidget(m_lblMoon);
    outer->addWidget(trackingBox);

    QGroupBox *directBox = new QGroupBox(this);
    QGridLayout *direct = new QGridLayout(directBox);
    m_editDirectTarget = new QLineEdit(directBox);
    m_editDirectTarget->setPlaceholderText(MadModemI18n::text(QStringLiteral("Locator, country, DXCC or prefix...")));
    m_editDirectTarget->setToolTip(MadModemI18n::text(QStringLiteral("Type a Maidenhead locator, country, DXCC name or callsign prefix to compute a bearing target.")));
    m_btnDirectTarget = new QPushButton(MadModemI18n::text(QStringLiteral("Point")), directBox);
    m_btnDirectTarget->setToolTip(MadModemI18n::text(QStringLiteral("Resolve the typed target and point the rotator to its bearing.")));
    direct->addWidget(new QLabel(MadModemI18n::text(QStringLiteral("Target")), directBox), 0, 0);
    direct->addWidget(m_editDirectTarget, 0, 1);
    direct->addWidget(m_btnDirectTarget, 1, 0, 1, 2);
    outer->addWidget(directBox);

    m_lblTarget = new QLabel(MadModemI18n::text(QStringLiteral("Target: --")), this);
    m_lblTarget->setWordWrap(true);
    m_lblQso = new QLabel(MadModemI18n::text(QStringLiteral("QSO: --")), this);
    m_lblQso->setWordWrap(true);
    m_lblEta = new QLabel(MadModemI18n::text(QStringLiteral("ETA: --")), this);
    m_lblEta->setWordWrap(true);
    m_lblStatus = new QLabel(MadModemI18n::text(QStringLiteral("Rotator: not yet configured")), this);
    m_lblStatus->setWordWrap(true);
    m_lblStatus->setStyleSheet(QStringLiteral("QLabel { font-weight: 500; }"));
    outer->addWidget(m_lblTarget);
    outer->addWidget(m_lblQso);
    outer->addWidget(m_lblEta);
    m_calibrationProgress = new QProgressBar(this);
    m_calibrationProgress->setRange(0, 100);
    m_calibrationProgress->setValue(0);
    m_calibrationProgress->setVisible(false);
    outer->addWidget(m_calibrationProgress);
    outer->addWidget(m_lblStatus);
    outer->addStretch(1);

    connect(m_radioQso, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked && m_controller != nullptr) {
            m_controller->setTrackingMode(CatRotatorController::TrackingMode::Qso);
        }
    });
    connect(m_radioMoon, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked && m_controller != nullptr) {
            m_controller->setTrackingMode(CatRotatorController::TrackingMode::Moon);
        }
    });
    connect(m_radioManual, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked && m_controller != nullptr) {
            m_controller->setTrackingMode(CatRotatorController::TrackingMode::Manual);
        }
    });

    connect(m_btnConnect, &QPushButton::clicked, this, [this]() {
        if (m_controller == nullptr) return;
        if (m_controller->isConnected()) {
            m_controller->disconnectRotator();
            return;
        }
        const CatRotatorController::Config cfg = m_controller->config();
        if (!cfg.enabled) {
            const QString reason = cfg.disabledReason.trimmed().isEmpty()
                ? MadModemI18n::text(QStringLiteral("Rotator: disabled in settings"))
                : cfg.disabledReason.trimmed();
            QMessageBox::warning(this,
                                 MadModemI18n::text(QStringLiteral("Rotator unavailable")),
                                 reason);
            updateStatusLabel();
            return;
        }
        m_controller->connectRotator();
    });
    connect(m_btnStop, &QPushButton::clicked, this, [this]() {
        if (m_controller != nullptr) m_controller->stop();
    });
    connect(m_btnGo, &QPushButton::clicked, this, [this]() {
        if (m_controller != nullptr) {
            m_controller->setTrackingMode(CatRotatorController::TrackingMode::Manual);
            m_controller->setAzEl(m_spinAz->value(), m_spinEl->value(), MadModemI18n::text(QStringLiteral("side Rotator tab manual command")));
        }
    });
    connect(m_btnTrack, &QPushButton::clicked, this, [this]() {
        if (m_controller != nullptr) {
            m_controller->setTrackingMode(CatRotatorController::TrackingMode::Qso);
            m_controller->setTrackingQsoTarget(true);
            m_controller->trackQsoTargetNow(MadModemI18n::text(QStringLiteral("side Rotator tab QSO tracking")));
        }
    });
    connect(m_btnMoonTrack, &QPushButton::clicked, this, [this]() {
        if (m_controller != nullptr) {
            m_controller->setTrackingMode(CatRotatorController::TrackingMode::Moon);
            m_controller->updateMoonTargetNow();
        }
    });
    connect(m_btnPark, &QPushButton::clicked, this, [this]() {
        if (m_controller != nullptr) {
            m_controller->setTrackingMode(CatRotatorController::TrackingMode::Manual);
            m_controller->park();
        }
    });
    connect(m_btnDirectTarget, &QPushButton::clicked, this, [this]() {
        if (m_controller != nullptr) m_controller->setTrackingMode(CatRotatorController::TrackingMode::Manual);
        if (m_editDirectTarget != nullptr) emit requestDirectTarget(m_editDirectTarget->text().trimmed());
    });
    connect(m_editDirectTarget, &QLineEdit::returnPressed, this, [this]() {
        if (m_controller != nullptr) m_controller->setTrackingMode(CatRotatorController::TrackingMode::Manual);
        if (m_editDirectTarget != nullptr) emit requestDirectTarget(m_editDirectTarget->text().trimmed());
    });
}

void CatRotatorPanel::applyConfig(const CatRotatorController::Config &config)
{
    m_config = config;
    refreshState();
}

void CatRotatorPanel::updateQsoTarget(const CatRotatorController::QsoTarget &target)
{
    m_qsoTarget = target;
    refreshState();
}

QString CatRotatorPanel::azElText(double az, double el) const
{
    return MadModemI18n::text(QStringLiteral("Az %1° / El %2°")).arg(QString::number(az, 'f', 1), QString::number(el, 'f', 1));
}

void CatRotatorPanel::updateTrackingModeControls()
{
    if (m_controller == nullptr) {
        return;
    }
    QSignalBlocker b1(m_radioQso);
    QSignalBlocker b2(m_radioMoon);
    QSignalBlocker b3(m_radioManual);
    switch (m_controller->trackingMode()) {
    case CatRotatorController::TrackingMode::Moon:
        if (m_radioMoon != nullptr) m_radioMoon->setChecked(true);
        break;
    case CatRotatorController::TrackingMode::Manual:
        if (m_radioManual != nullptr) m_radioManual->setChecked(true);
        break;
    case CatRotatorController::TrackingMode::Qso:
    default:
        if (m_radioQso != nullptr) m_radioQso->setChecked(true);
        break;
    }
}

void CatRotatorPanel::refreshState()
{
    const bool connected = (m_controller != nullptr && m_controller->isConnected());
    if (m_lblConnection != nullptr) m_lblConnection->setText(connected ? MadModemI18n::text(QStringLiteral("Connected")) : MadModemI18n::text(QStringLiteral("Disconnected")));
    if (m_btnConnect != nullptr) m_btnConnect->setText(connected ? MadModemI18n::text(QStringLiteral("Disconnect")) : MadModemI18n::text(QStringLiteral("Connect")));
    if (m_lblCurrentAz != nullptr) {
        m_lblCurrentAz->setText(m_controller != nullptr ? QString::number(m_controller->currentAzimuth(), 'f', 0) : QStringLiteral("--"));
    }
    if (m_lblCurrentEl != nullptr) {
        m_lblCurrentEl->setText(m_controller != nullptr ? QString::number(m_controller->currentElevation(), 'f', 0) : QStringLiteral("--"));
    }
    if (m_lblTarget != nullptr) {
        m_lblTarget->setText(m_controller != nullptr ? MadModemI18n::text(QStringLiteral("Target: %1")).arg(azElText(m_controller->targetAzimuth(), m_controller->targetElevation())) : MadModemI18n::text(QStringLiteral("Target: --")));
    }
    if (m_navball != nullptr && m_controller != nullptr) {
        m_navball->set_az(m_controller->currentAzimuth());
        m_navball->set_alt(m_controller->currentElevation());
        m_navball->set_taz(m_controller->targetAzimuth());
        m_navball->set_talt(m_controller->targetElevation());
        m_navball->setTargetVisible(m_controller->targetAzimuth() >= 0.0);
        const CatRotatorController::MoonTarget moon = m_controller->moonTarget();
        m_navball->setMoonVisible(moon.valid && moon.aboveHorizon);
        if (moon.valid) m_navball->setMoonAzEl(moon.azimuthDeg, moon.elevationDeg);
        const CatRotatorController::SunTarget sun = m_controller->sunTarget();
        m_navball->setSunVisible(sun.valid && sun.aboveHorizon);
        if (sun.valid) m_navball->setSunAzEl(sun.azimuthDeg, sun.elevationDeg);
        m_navball->refresh();
    }
    updateTrackingModeControls();
    if (m_lblMoon != nullptr) {
        QString moonText = MadModemI18n::text(QStringLiteral("Moon: --"));
        if (m_controller != nullptr) {
            const CatRotatorController::MoonTarget moon = m_controller->moonTarget();
            if (!moon.statusText.trimmed().isEmpty()) {
                moonText = moon.statusText;
            }
        }
        m_lblMoon->setText(moonText);
    }
    if (m_lblQso != nullptr) {
        QString qso = MadModemI18n::text(QStringLiteral("QSO: --"));
        if (!m_qsoTarget.callsign.trimmed().isEmpty()) {
            qso = MadModemI18n::text(QStringLiteral("QSO: %1 %2 — %3° — %4 km"))
                      .arg(m_qsoTarget.callsign,
                           m_qsoTarget.grid.isEmpty() ? QStringLiteral("--") : m_qsoTarget.grid,
                           m_qsoTarget.bearingDeg >= 0.0 ? QString::number(m_qsoTarget.bearingDeg, 'f', 1) : QStringLiteral("--"),
                           m_qsoTarget.distanceKm >= 0.0 ? QString::number(m_qsoTarget.distanceKm, 'f', 0) : QStringLiteral("--"));
        }
        m_lblQso->setText(qso);
    }
    if (m_lblEta != nullptr) {
        QString etaText = MadModemI18n::text(QStringLiteral("ETA: --"));
        if (m_controller != nullptr && !m_qsoTarget.callsign.trimmed().isEmpty() && m_qsoTarget.bearingDeg >= 0.0) {
            const int eta = m_controller->estimatePointingTimeMs(m_qsoTarget.bearingDeg, 0.0);
            const bool ready = m_controller->isReadyForTarget(m_qsoTarget.bearingDeg, 0.0);
            etaText = ready
                ? MadModemI18n::text(QStringLiteral("ETA: ready"))
                : MadModemI18n::text(QStringLiteral("ETA pointing: %1 s — TX inhibited until ready")).arg(QString::number(static_cast<double>(eta) / 1000.0, 'f', 1));
        }
        m_lblEta->setText(etaText);
    }
    updateStatusLabel();
}

QString CatRotatorPanel::friendlyStatusText() const
{
    if (m_controller == nullptr) {
        return MadModemI18n::text(QStringLiteral("Rotator: not yet configured"));
    }

    const CatRotatorController::Config cfg = m_controller->config();
    if (!cfg.enabled) {
        const QString reason = cfg.disabledReason.trimmed();
        return reason.isEmpty() ? MadModemI18n::text(QStringLiteral("Rotator: disabled in settings")) : reason;
    }
    if (cfg.hamlibModel <= 0 || cfg.path.trimmed().isEmpty()) {
        return MadModemI18n::text(QStringLiteral("Rotator: not yet configured"));
    }
    return m_controller->isConnected()
        ? MadModemI18n::text(QStringLiteral("Rotator: connected"))
        : MadModemI18n::text(QStringLiteral("Rotator: disconnected"));
}

void CatRotatorPanel::updateStatusLabel()
{
    if (m_lblStatus != nullptr) {
        m_lblStatus->setText(friendlyStatusText());
    }
}

} // namespace mm
