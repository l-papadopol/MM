#ifndef CATROTATORCONTROLLER_H
#define CATROTATORCONTROLLER_H

#include <QObject>
#include <QDateTime>
#include <QTimer>
#include <QString>

namespace mm {

class CatRotatorController final : public QObject
{
    Q_OBJECT

public:
    enum class CalibrationState
    {
        Idle,
        AzToMin,
        AzMeasureToMax,
        AzReturnHome,
        ElToMin,
        ElMeasureToMax,
        ElReturnHome
    };

    struct Config
    {
        bool enabled = false;
        bool stationIdentityOk = false;
        bool autoConnect = false;
        bool showWindowOnStart = false;
        int profileIndex = 0;
        QString label = QStringLiteral("Rotator 1");
        QString bandsCsv;
        int hamlibModel = 1;
        QString path;
        int baudRate = 9600;
        int pollIntervalMs = 750;
        bool useElevation = false;
        bool overlap = false;
        QString azimuthGeometryPreset = QStringLiteral("north-stop-360");
        double azimuthStopDeg = 0.0;
        bool autoReverseOnStall = true;
        int noMovementTimeoutMs = 3000;
        double noMovementThresholdDeg = 2.0;
        double parkAzimuth = 0.0;
        double parkElevation = 0.0;
        bool trackSelectedQso = true;
        bool trackOnlyWhenQsoActive = true;
        int targetToleranceDeg = 3;

        // v4.13u: mechanical inertia / movement timing model.
        // Values are per-rotator profile and are used both by the UI and by the FT TX guard.
        double azimuthMinDeg = 0.0;
        double azimuthMaxDeg = 359.9;
        double elevationMinDeg = 0.0;
        double elevationMaxDeg = 90.0;
        double azimuthMsPerDeg = 0.0;
        double elevationMsPerDeg = 0.0;
        int startupDelayMs = 250;
        int settleDelayMs = 700;
        int txGuardMarginMs = 800;
        QString calibrationStampUtc;

        // Home position used by local astronomical tracking targets.
        // Derived from Settings -> User/QTH locator for now; kept numeric in
        // the controller so Moon tracking does not depend on map widgets.
        bool homeCoordinatesValid = false;
        double homeLatitudeDeg = 0.0;
        double homeLongitudeDeg = 0.0;
        double homeAltitudeM = 0.0;
    };

    enum class TrackingMode
    {
        Manual,
        Qso,
        Moon
    };
    Q_ENUM(TrackingMode)

    struct MoonTarget
    {
        bool valid = false;
        bool aboveHorizon = false;
        double azimuthDeg = -1.0;
        double elevationDeg = -90.0;
        double distanceKm = 0.0;
        QString statusText;
        QDateTime updatedUtc;
    };

    struct SunTarget
    {
        bool valid = false;
        bool aboveHorizon = false;
        double azimuthDeg = -1.0;
        double elevationDeg = -90.0;
        QString statusText;
        QDateTime updatedUtc;
    };

    struct QsoTarget
    {
        QString callsign;
        QString grid;
        QString reason;
        int rxFrequencyHz = 0;
        double bearingDeg = -1.0;
        double distanceKm = -1.0;
        bool qsoActive = false;
        QDateTime updatedUtc;
    };

    explicit CatRotatorController(QObject *parent = nullptr);
    ~CatRotatorController() override;

    Config config() const { return m_config; }
    bool isConnected() const { return m_connected; }
    bool isTrackingQsoTarget() const { return m_trackingQsoTarget; }
    TrackingMode trackingMode() const { return m_trackingMode; }
    MoonTarget moonTarget() const { return m_moonTarget; }
    SunTarget sunTarget() const { return m_sunTarget; }
    bool isMoving() const { return m_motionActive; }
    bool isCalibrating() const { return m_calibrationState != CalibrationState::Idle; }
    double currentAzimuth() const { return m_currentAz; }
    double currentElevation() const { return m_currentEl; }
    double targetAzimuth() const { return m_targetAz; }
    double targetElevation() const { return m_targetEl; }
    QString statusText() const { return m_status; }
    QsoTarget qsoTarget() const { return m_qsoTarget; }
    bool hasValidAzimuthCalibration() const;
    bool hasValidElevationCalibration() const;
    int estimatePointingTimeMs(double targetAzimuthDeg, double targetElevationDeg = 0.0) const;
    bool isReadyForTarget(double targetAzimuthDeg, double targetElevationDeg = 0.0) const;

public slots:
    void configure(const Config &config);
    void connectRotator();
    void disconnectRotator();
    void pollNow();
    void setAzEl(double azimuthDeg, double elevationDeg = 0.0, const QString &reason = QString());
    void setAzimuth(double azimuthDeg, const QString &reason = QString());
    void stop();
    void park();
    void setQsoTarget(const QsoTarget &target);
    void clearQsoTarget();
    void trackQsoTargetNow(const QString &reason = QString());
    void setTrackingQsoTarget(bool enabled);
    void setTrackingMode(TrackingMode mode);
    void updateMoonTargetNow();
    void startAzimuthCalibration();
    void startElevationCalibration();
    void cancelCalibration();

signals:
    void statusChanged(const QString &text);
    void connectionChanged(bool connected);
    void positionChanged(double azimuthDeg, double elevationDeg);
    void targetChanged(double azimuthDeg, double elevationDeg, const QString &reason);
    void qsoTargetChanged(const mm::CatRotatorController::QsoTarget &target);
    void trackingModeChanged(mm::CatRotatorController::TrackingMode mode);
    void moonTargetChanged(const mm::CatRotatorController::MoonTarget &target);
    void sunTargetChanged(const mm::CatRotatorController::SunTarget &target);
    void motionChanged(bool moving);
    void calibrationProgress(int percent, const QString &message);
    void calibrationFinished(const mm::CatRotatorController::Config &updatedConfig, const QString &message);

private:
    void setStatus(const QString &text);
    static double normalizeAzimuth(double value);
    static double clampElevation(double value);
    double clampConfiguredAzimuth(double value) const;
    double logicalAzimuthForTarget(double targetAzimuthDeg) const;
    double logicalAzimuthForTarget(double targetAzimuthDeg, bool preferAlternate) const;
    double configuredAzimuthDifferenceDeg(double currentAzimuthDeg, double targetAzimuthDeg) const;
    void checkNoMovementStall();
    bool shouldTrackCurrentQsoTarget() const;
    void closeBackend();
    void setMotionActive(bool moving);
    void updateMotionState();
    static double angularDifferenceDeg(double a, double b);
    static double signedAngularSpanDeg(double start, double stop);
    void moveBackend(double azimuthDeg, double elevationDeg, const QString &reason);
    void beginCalibration(CalibrationState initialState, const QString &axisLabel);
    void advanceCalibration();
    bool calibrationLegTimedOut(qint64 maxMs) const;

    Config m_config;
    QTimer m_pollTimer;
    QTimer m_calibrationTimer;
    QTimer m_moonTimer;
    bool m_connected = false;
    bool m_trackingQsoTarget = false;
    TrackingMode m_trackingMode = TrackingMode::Qso;
    bool m_motionActive = false;
    double m_currentAz = 0.0;
    double m_currentEl = 0.0;
    double m_targetAz = 0.0;
    double m_targetEl = 0.0;
    QString m_status;
    QsoTarget m_qsoTarget;
    MoonTarget m_moonTarget;
    SunTarget m_sunTarget;
    qint64 m_lastMoonCommandMs = 0;
    double m_lastMoonCommandAz = -999.0;
    double m_lastMoonCommandEl = -999.0;
    void *m_rot = nullptr;

    CalibrationState m_calibrationState = CalibrationState::Idle;
    qint64 m_calibrationLegStartMs = 0;
    qint64 m_motionCommandStartMs = 0;
    qint64 m_lastMotionProgressMs = 0;
    double m_lastMotionProgressAz = 0.0;
    double m_lastMotionProgressEl = 0.0;
    double m_lastRawTargetAz = 0.0;
    bool m_autoReverseTried = false;
    double m_calibrationHomeAz = 0.0;
    double m_calibrationHomeEl = 0.0;
    QString m_calibrationAxisLabel;
};

} // namespace mm

Q_DECLARE_METATYPE(mm::CatRotatorController::Config)
Q_DECLARE_METATYPE(mm::CatRotatorController::QsoTarget)
Q_DECLARE_METATYPE(mm::CatRotatorController::TrackingMode)
Q_DECLARE_METATYPE(mm::CatRotatorController::MoonTarget)
Q_DECLARE_METATYPE(mm::CatRotatorController::SunTarget)

#endif // CATROTATORCONTROLLER_H
