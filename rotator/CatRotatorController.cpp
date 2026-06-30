#include "CatRotatorController.h"

#include <QtGlobal>
#include <QMetaType>
#include <QDateTime>
#include <QVector>
#include <QtMath>
#include <cmath>
#include <algorithm>
#include <limits>

#ifdef MADMODEM_WITH_HAMLIB
#include <hamlib/rotator.h>

#endif

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kDegToRad = kPi / 180.0;
constexpr double kRadToDeg = 180.0 / kPi;
constexpr double kEarthRadiusKm = 6378.137;

struct MoonEphemeris
{
    bool valid = false;
    double azimuthDeg = 0.0;
    double elevationDeg = -90.0;
    double distanceKm = 0.0;
};

struct SunEphemeris
{
    bool valid = false;
    double azimuthDeg = 0.0;
    double elevationDeg = -90.0;
};

double norm360(double degrees)
{
    double v = std::fmod(degrees, 360.0);
    if (v < 0.0) v += 360.0;
    return v;
}

double julianDateUtc(const QDateTime &utc)
{
    const QDateTime t = utc.toUTC();
    const QDate d = t.date();
    const QTime tm = t.time();
    int y = d.year();
    int m = d.month();
    const double day = static_cast<double>(d.day())
        + (tm.hour() + (tm.minute() + (tm.second() + tm.msec() / 1000.0) / 60.0) / 60.0) / 24.0;
    if (m <= 2) {
        y -= 1;
        m += 12;
    }
    const int a = y / 100;
    const int b = 2 - a + a / 4;
    return std::floor(365.25 * (y + 4716))
        + std::floor(30.6001 * (m + 1))
        + day + b - 1524.5;
}

MoonEphemeris moonTopocentricAzEl(const QDateTime &utc, double latitudeDeg, double longitudeDeg, double altitudeM)
{
    MoonEphemeris out;
    const double jd = julianDateUtc(utc);
    const double d = jd - 2451545.0;

    // Low-order Meeus/NOAA-style lunar position. This is intentionally local,
    // deterministic and dependency-free; topocentric parallax is applied below.
    const double L = norm360(218.316 + 13.176396 * d);
    const double M = norm360(134.963 + 13.064993 * d);
    const double F = norm360(93.272 + 13.229350 * d);
    const double D = norm360(297.850 + 12.190749 * d);
    const double Ms = norm360(357.529 + 0.98560028 * d);

    const double lon = norm360(L
        + 6.289 * std::sin(M * kDegToRad)
        + 1.274 * std::sin((2.0 * D - M) * kDegToRad)
        + 0.658 * std::sin((2.0 * D) * kDegToRad)
        + 0.214 * std::sin((2.0 * M) * kDegToRad)
        - 0.186 * std::sin(Ms * kDegToRad)
        - 0.114 * std::sin((2.0 * F) * kDegToRad));
    const double lat = 5.128 * std::sin(F * kDegToRad)
        + 0.280 * std::sin((M + F) * kDegToRad)
        + 0.277 * std::sin((M - F) * kDegToRad)
        + 0.173 * std::sin((2.0 * D - F) * kDegToRad);
    const double distanceKm = 385001.0
        - 20905.0 * std::cos(M * kDegToRad)
        - 3699.0 * std::cos((2.0 * D - M) * kDegToRad)
        - 2956.0 * std::cos((2.0 * D) * kDegToRad);

    const double eps = (23.439291 - 0.00000036 * d) * kDegToRad;
    const double lonRad = lon * kDegToRad;
    const double latRad = lat * kDegToRad;

    const double xEcl = std::cos(latRad) * std::cos(lonRad);
    const double yEcl = std::cos(latRad) * std::sin(lonRad);
    const double zEcl = std::sin(latRad);

    const double xEq = distanceKm * xEcl;
    const double yEq = distanceKm * (yEcl * std::cos(eps) - zEcl * std::sin(eps));
    const double zEq = distanceKm * (yEcl * std::sin(eps) + zEcl * std::cos(eps));

    const double T = (jd - 2451545.0) / 36525.0;
    const double gmst = norm360(280.46061837 + 360.98564736629 * (jd - 2451545.0)
                                + 0.000387933 * T * T - T * T * T / 38710000.0);
    const double lst = norm360(gmst + longitudeDeg) * kDegToRad;
    const double phi = latitudeDeg * kDegToRad;
    const double obsRadiusKm = kEarthRadiusKm + altitudeM / 1000.0;

    const double obsX = obsRadiusKm * std::cos(phi) * std::cos(lst);
    const double obsY = obsRadiusKm * std::cos(phi) * std::sin(lst);
    const double obsZ = obsRadiusKm * std::sin(phi);

    const double topX = xEq - obsX;
    const double topY = yEq - obsY;
    const double topZ = zEq - obsZ;

    const double east = -std::sin(lst) * topX + std::cos(lst) * topY;
    const double north = -std::sin(phi) * std::cos(lst) * topX
                       - std::sin(phi) * std::sin(lst) * topY
                       + std::cos(phi) * topZ;
    const double up = std::cos(phi) * std::cos(lst) * topX
                    + std::cos(phi) * std::sin(lst) * topY
                    + std::sin(phi) * topZ;

    const double range = std::sqrt(east * east + north * north + up * up);
    if (range <= 1.0) return out;

    out.azimuthDeg = norm360(std::atan2(east, north) * kRadToDeg);
    out.elevationDeg = std::asin(qBound(-1.0, up / range, 1.0)) * kRadToDeg;
    out.distanceKm = range;
    out.valid = true;
    return out;
}

SunEphemeris sunTopocentricAzEl(const QDateTime &utc, double latitudeDeg, double longitudeDeg)
{
    SunEphemeris out;
    const double jd = julianDateUtc(utc);
    const double T = (jd - 2451545.0) / 36525.0;
    const double L0 = norm360(280.46646 + T * (36000.76983 + 0.0003032 * T));
    const double M = norm360(357.52911 + T * (35999.05029 - 0.0001537 * T));
    const double C = (1.914602 - T * (0.004817 + 0.000014 * T)) * std::sin(M * kDegToRad)
                   + (0.019993 - 0.000101 * T) * std::sin(2.0 * M * kDegToRad)
                   + 0.000289 * std::sin(3.0 * M * kDegToRad);
    const double trueLong = L0 + C;
    const double omega = 125.04 - 1934.136 * T;
    const double lambda = trueLong - 0.00569 - 0.00478 * std::sin(omega * kDegToRad);
    const double epsilon0 = 23.0 + (26.0 + ((21.448 - T * (46.815 + T * (0.00059 - T * 0.001813))) / 60.0)) / 60.0;
    const double epsilon = epsilon0 + 0.00256 * std::cos(omega * kDegToRad);

    const double lambdaRad = lambda * kDegToRad;
    const double epsilonRad = epsilon * kDegToRad;

    const double alpha = std::atan2(std::cos(epsilonRad) * std::sin(lambdaRad), std::cos(lambdaRad));
    const double delta = std::asin(std::sin(epsilonRad) * std::sin(lambdaRad));

    const double gmst = norm360(280.46061837 + 360.98564736629 * (jd - 2451545.0)
                                + 0.000387933 * T * T - T * T * T / 38710000.0);
    const double lst = norm360(gmst + longitudeDeg) * kDegToRad;
    const double H = lst - alpha;
    const double phi = latitudeDeg * kDegToRad;

    const double sinEl = std::sin(phi) * std::sin(delta) + std::cos(phi) * std::cos(delta) * std::cos(H);
    const double el = std::asin(qBound(-1.0, sinEl, 1.0));
    const double az = std::atan2(std::sin(H), std::cos(H) * std::sin(phi) - std::tan(delta) * std::cos(phi));

    out.azimuthDeg = norm360(az * kRadToDeg + 180.0);
    out.elevationDeg = el * kRadToDeg;
    out.valid = true;
    return out;
}
} // namespace

namespace mm {

CatRotatorController::CatRotatorController(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<CatRotatorController::Config>("mm::CatRotatorController::Config");
    qRegisterMetaType<CatRotatorController::QsoTarget>("mm::CatRotatorController::QsoTarget");
    qRegisterMetaType<CatRotatorController::TrackingMode>("mm::CatRotatorController::TrackingMode");
    qRegisterMetaType<CatRotatorController::MoonTarget>("mm::CatRotatorController::MoonTarget");
    qRegisterMetaType<CatRotatorController::SunTarget>("mm::CatRotatorController::SunTarget");
    m_pollTimer.setSingleShot(false);
    connect(&m_pollTimer, &QTimer::timeout, this, &CatRotatorController::pollNow);
    m_calibrationTimer.setSingleShot(false);
    connect(&m_calibrationTimer, &QTimer::timeout, this, &CatRotatorController::advanceCalibration);
    m_moonTimer.setSingleShot(false);
    m_moonTimer.setInterval(1000);
    connect(&m_moonTimer, &QTimer::timeout, this, &CatRotatorController::updateMoonTargetNow);
    setStatus(QStringLiteral("CatRotator module disabled."));
}

CatRotatorController::~CatRotatorController()
{
    closeBackend();
}

void CatRotatorController::configure(const Config &config)
{
    const bool wasConnected = m_connected;
    const bool backendChanged = config.hamlibModel != m_config.hamlibModel ||
                                config.path != m_config.path ||
                                config.baudRate != m_config.baudRate ||
                                config.enabled != m_config.enabled;
    m_config = config;
    if (m_config.pollIntervalMs < 250) m_config.pollIntervalMs = 250;
    if (m_config.pollIntervalMs > 10000) m_config.pollIntervalMs = 10000;
    if (m_config.targetToleranceDeg < 0) m_config.targetToleranceDeg = 0;
    if (m_config.targetToleranceDeg > 45) m_config.targetToleranceDeg = 45;

    m_config.azimuthGeometryPreset = m_config.azimuthGeometryPreset.trimmed();
    if (m_config.azimuthGeometryPreset.isEmpty()) {
        m_config.azimuthGeometryPreset = m_config.overlap ? QStringLiteral("yaesu-450") : QStringLiteral("north-stop-360");
    }
    if (m_config.azimuthGeometryPreset == QStringLiteral("yaesu-450")) {
        m_config.overlap = true;
        m_config.azimuthMinDeg = 0.0;
        m_config.azimuthMaxDeg = 450.0;
        m_config.azimuthStopDeg = 450.0;
    } else if (m_config.azimuthGeometryPreset == QStringLiteral("south-stop-360")) {
        m_config.overlap = false;
        m_config.azimuthMinDeg = -180.0;
        m_config.azimuthMaxDeg = 180.0;
        m_config.azimuthStopDeg = 180.0;
    } else if (m_config.azimuthGeometryPreset == QStringLiteral("north-stop-360")) {
        m_config.overlap = false;
        m_config.azimuthMinDeg = 0.0;
        m_config.azimuthMaxDeg = 359.9;
        m_config.azimuthStopDeg = 0.0;
    } else {
        m_config.azimuthGeometryPreset = QStringLiteral("custom");
        m_config.azimuthMinDeg = qBound(-360.0, m_config.azimuthMinDeg, 540.0);
        m_config.azimuthMaxDeg = qBound(-360.0, m_config.azimuthMaxDeg, 540.0);
        if (m_config.azimuthMaxDeg < m_config.azimuthMinDeg) qSwap(m_config.azimuthMinDeg, m_config.azimuthMaxDeg);
        if (m_config.azimuthMaxDeg - m_config.azimuthMinDeg < 1.0) {
            m_config.azimuthMinDeg = 0.0;
            m_config.azimuthMaxDeg = m_config.overlap ? 450.0 : 359.9;
        }
        m_config.azimuthStopDeg = qBound(-360.0, m_config.azimuthStopDeg, 540.0);
    }
    m_config.noMovementTimeoutMs = qBound(500, m_config.noMovementTimeoutMs, 30000);
    m_config.noMovementThresholdDeg = qBound(0.1, m_config.noMovementThresholdDeg, 30.0);

    m_config.elevationMinDeg = clampElevation(m_config.elevationMinDeg);
    m_config.elevationMaxDeg = clampElevation(m_config.elevationMaxDeg);
    if (m_config.elevationMaxDeg < m_config.elevationMinDeg) qSwap(m_config.elevationMinDeg, m_config.elevationMaxDeg);
    if (m_config.azimuthMsPerDeg < 0.0) m_config.azimuthMsPerDeg = 0.0;
    if (m_config.elevationMsPerDeg < 0.0) m_config.elevationMsPerDeg = 0.0;
    m_config.startupDelayMs = qBound(0, m_config.startupDelayMs, 30000);
    m_config.settleDelayMs = qBound(0, m_config.settleDelayMs, 30000);
    m_config.txGuardMarginMs = qBound(0, m_config.txGuardMarginMs, 30000);

    if (!m_config.enabled) {
        m_moonTimer.stop();
        disconnectRotator();
        const QString reason = m_config.disabledReason.trimmed();
        setStatus(reason.isEmpty()
            ? QStringLiteral("CatRotator disabled in MM settings.")
            : reason);
        return;
    }

    m_moonTimer.start(1000);
    updateMoonTargetNow();

    if (wasConnected && backendChanged) {
        disconnectRotator();
    }

    if (m_config.autoConnect && !m_connected) {
        connectRotator();
    } else if (!m_connected) {
        setStatus(QStringLiteral("CatRotator ready. Configure an independent rotator port/rotctld endpoint and press Connect."));
    }

    if (m_connected) {
        m_pollTimer.start(m_config.pollIntervalMs);
    }
}

void CatRotatorController::connectRotator()
{
    if (!m_config.enabled) {
        setStatus(QStringLiteral("CatRotator is disabled in settings."));
        return;
    }
    if (m_connected) {
        pollNow();
        return;
    }

#ifdef MADMODEM_WITH_HAMLIB
    closeBackend();
    rot_model_t model = static_cast<rot_model_t>(m_config.hamlibModel > 0 ? m_config.hamlibModel : 1);
    ROT *rot = rot_init(model);
    if (rot == nullptr) {
        setStatus(QStringLiteral("Hamlib rot_init failed for model %1.").arg(m_config.hamlibModel));
        emit connectionChanged(false);
        return;
    }

    auto setConf = [rot](const char *name, const QString &value) {
        if (value.trimmed().isEmpty()) {
            return;
        }
        const hamlib_token_t token = rot_token_lookup(rot, name);
        if (token != RIG_CONF_END) {
            rot_set_conf(rot, token, value.toUtf8().constData());
        }
    };

    setConf("rot_pathname", m_config.path);
    setConf("serial_speed", QString::number(qMax(1, m_config.baudRate)));

    const int rc = rot_open(rot);
    if (rc != RIG_OK) {
        const QString message = QStringLiteral("Hamlib rot_open failed: %1").arg(QString::fromLocal8Bit(rigerror(rc)));
        rot_cleanup(rot);
        setStatus(message);
        emit connectionChanged(false);
        return;
    }

    m_rot = rot;
    m_connected = true;
    emit connectionChanged(true);
    setStatus(QStringLiteral("%1 connected via independent Hamlib rotator backend, model %2%3.")
                  .arg(m_config.label.trimmed().isEmpty() ? QStringLiteral("CatRotator") : m_config.label.trimmed())
                  .arg(m_config.hamlibModel)
                  .arg(m_config.path.trimmed().isEmpty() ? QString() : QStringLiteral(" on %1").arg(m_config.path.trimmed())));
    m_pollTimer.start(m_config.pollIntervalMs);
    pollNow();
#else
    m_connected = false;
    setStatus(QStringLiteral("CatRotator cannot connect: MadModem was built without Hamlib rotator support."));
    emit connectionChanged(false);
#endif
}

void CatRotatorController::disconnectRotator()
{
    closeBackend();
    setStatus(m_config.enabled ? QStringLiteral("CatRotator disconnected.") : QStringLiteral("CatRotator disabled."));
}

void CatRotatorController::closeBackend()
{
    cancelCalibration();
    setMotionActive(false);
    m_pollTimer.stop();
#ifdef MADMODEM_WITH_HAMLIB
    if (m_rot != nullptr) {
        ROT *rot = static_cast<ROT *>(m_rot);
        rot_close(rot);
        rot_cleanup(rot);
        m_rot = nullptr;
    }
#endif
    if (m_connected) {
        m_connected = false;
        emit connectionChanged(false);
    }
}

void CatRotatorController::pollNow()
{
    if (!m_connected) {
        return;
    }
#ifdef MADMODEM_WITH_HAMLIB
    if (m_rot == nullptr) {
        return;
    }
    azimuth_t az = 0;
    elevation_t el = 0;
    const int rc = rot_get_position(static_cast<ROT *>(m_rot), &az, &el);
    if (rc == RIG_OK) {
        m_currentAz = clampConfiguredAzimuth(static_cast<double>(az));
        m_currentEl = clampElevation(el);
        updateMotionState();
        checkNoMovementStall();
        emit positionChanged(m_currentAz, m_currentEl);
    } else {
        setStatus(QStringLiteral("CatRotator poll failed: %1").arg(QString::fromLocal8Bit(rigerror(rc))));
    }
#endif
}

void CatRotatorController::setAzimuth(double azimuthDeg, const QString &reason)
{
    setAzEl(azimuthDeg, m_config.useElevation ? m_targetEl : 0.0, reason);
}

void CatRotatorController::setAzEl(double azimuthDeg, double elevationDeg, const QString &reason)
{
    if (!m_config.enabled) {
        setStatus(QStringLiteral("CatRotator target ignored: module disabled."));
        return;
    }
    if (!m_config.stationIdentityOk) {
        setStatus(QStringLiteral("CatRotator movement blocked: set My Call and My Locator in Settings -> User/QTH first."));
        return;
    }
    const double az = logicalAzimuthForTarget(azimuthDeg);
    const double el = m_config.useElevation ? clampElevation(elevationDeg) : 0.0;
    m_lastRawTargetAz = normalizeAzimuth(azimuthDeg);
    m_autoReverseTried = false;
    m_targetAz = az;
    m_targetEl = el;
    emit targetChanged(m_targetAz, m_targetEl, reason);

    if (!m_connected) {
        setStatus(QStringLiteral("CatRotator target staged: %1°%2%3.")
                      .arg(QString::number(m_targetAz, 'f', 1),
                           m_config.useElevation ? QStringLiteral(" / %1°").arg(QString::number(m_targetEl, 'f', 1)) : QString(),
                           reason.trimmed().isEmpty() ? QString() : QStringLiteral(" — %1").arg(reason.trimmed())));
        return;
    }

moveBackend(az, el, reason);
}

void CatRotatorController::stop()
{
#ifdef MADMODEM_WITH_HAMLIB
    if (m_connected && m_rot != nullptr) {
        const int rc = rot_stop(static_cast<ROT *>(m_rot));
        if (rc != RIG_OK) {
            setStatus(QStringLiteral("CatRotator stop failed: %1").arg(QString::fromLocal8Bit(rigerror(rc))));
            return;
        }
    }
#endif
    setMotionActive(false);
    setStatus(QStringLiteral("CatRotator stop requested."));
}

void CatRotatorController::park()
{
    if (!m_config.enabled) {
        return;
    }
#ifdef MADMODEM_WITH_HAMLIB
    if (m_connected && m_rot != nullptr) {
        const int rc = rot_park(static_cast<ROT *>(m_rot));
        if (rc == RIG_OK) {
            setStatus(QStringLiteral("CatRotator park requested via backend."));
            return;
        }
        // Many Hamlib backends do not implement native park; fall back to stored park coordinates.
    }
#endif
    setAzEl(m_config.parkAzimuth, m_config.parkElevation, QStringLiteral("park"));
}

void CatRotatorController::setQsoTarget(const QsoTarget &target)
{
    m_qsoTarget = target;
    m_qsoTarget.callsign = m_qsoTarget.callsign.trimmed().toUpper();
    m_qsoTarget.grid = m_qsoTarget.grid.trimmed().toUpper();
    if (!m_qsoTarget.updatedUtc.isValid()) {
        m_qsoTarget.updatedUtc = QDateTime::currentDateTimeUtc();
    }
    emit qsoTargetChanged(m_qsoTarget);
    if (shouldTrackCurrentQsoTarget()) {
        trackQsoTargetNow(QStringLiteral("selected QSO target %1").arg(m_qsoTarget.callsign));
    }
}

void CatRotatorController::clearQsoTarget()
{
    m_qsoTarget = QsoTarget();
    emit qsoTargetChanged(m_qsoTarget);
}

void CatRotatorController::trackQsoTargetNow(const QString &reason)
{
    if (!shouldTrackCurrentQsoTarget()) {
        setStatus(QStringLiteral("CatRotator QSO tracking skipped: no valid target/bearing or tracking disabled."));
        return;
    }
    setAzEl(m_qsoTarget.bearingDeg, 0.0, reason.trimmed().isEmpty() ? QStringLiteral("QSO target") : reason.trimmed());
}

void CatRotatorController::setTrackingQsoTarget(bool enabled)
{
    m_trackingQsoTarget = enabled;
    if (enabled && m_trackingMode == TrackingMode::Qso) {
        trackQsoTargetNow(QStringLiteral("QSO tracking enabled"));
    } else if (!enabled && m_trackingMode == TrackingMode::Qso) {
        setStatus(QStringLiteral("CatRotator QSO tracking disabled by operator."));
    }
}

void CatRotatorController::setTrackingMode(TrackingMode mode)
{
    if (m_trackingMode == mode) {
        if (mode == TrackingMode::Moon) updateMoonTargetNow();
        return;
    }
    m_trackingMode = mode;
    emit trackingModeChanged(m_trackingMode);

    if (m_trackingMode == TrackingMode::Moon) {
        m_lastMoonCommandMs = 0;
        m_moonTimer.start(1000);
        updateMoonTargetNow();
        return;
    }

    m_moonTimer.stop();
    if (m_trackingMode == TrackingMode::Qso) {
        m_trackingQsoTarget = true;
        trackQsoTargetNow(QStringLiteral("QSO tracking selected"));
    } else {
        setStatus(QStringLiteral("CatRotator manual tracking selected."));
    }
}

void CatRotatorController::updateMoonTargetNow()
{
    MoonTarget moon;
    SunTarget sun;
    const QDateTime nowUtc = QDateTime::currentDateTimeUtc();
    moon.updatedUtc = nowUtc;
    sun.updatedUtc = nowUtc;

    if (!m_config.enabled) {
        moon.statusText = QStringLiteral("Moon tracking unavailable: rotator disabled in settings.");
        sun.statusText = QStringLiteral("Sun tracking unavailable: rotator disabled in settings.");
        m_moonTarget = moon;
        m_sunTarget = sun;
        emit moonTargetChanged(m_moonTarget);
        emit sunTargetChanged(m_sunTarget);
        return;
    }
    if (!m_config.homeCoordinatesValid) {
        moon.statusText = QStringLiteral("Moon tracking unavailable: set My Locator in Settings -> User/QTH first.");
        sun.statusText = QStringLiteral("Sun tracking unavailable: set My Locator in Settings -> User/QTH first.");
        m_moonTarget = moon;
        m_sunTarget = sun;
        emit moonTargetChanged(m_moonTarget);
        emit sunTargetChanged(m_sunTarget);
        if (m_trackingMode == TrackingMode::Moon) setStatus(moon.statusText);
        return;
    }

    const MoonEphemeris e = moonTopocentricAzEl(nowUtc,
                                                m_config.homeLatitudeDeg,
                                                m_config.homeLongitudeDeg,
                                                m_config.homeAltitudeM);
    moon.valid = e.valid;
    moon.azimuthDeg = e.azimuthDeg;
    moon.elevationDeg = e.elevationDeg;
    moon.distanceKm = e.distanceKm;
    moon.aboveHorizon = e.valid && e.elevationDeg >= 0.0;
    if (moon.valid) {
        moon.statusText = moon.aboveHorizon
            ? QStringLiteral("Moon: Az %1° / El %2° | EME tracking target")
                  .arg(QString::number(moon.azimuthDeg, 'f', 1), QString::number(moon.elevationDeg, 'f', 1))
            : QStringLiteral("Moon: below horizon | Az %1° / El %2°")
                  .arg(QString::number(moon.azimuthDeg, 'f', 1), QString::number(moon.elevationDeg, 'f', 1));
    } else {
        moon.statusText = QStringLiteral("Moon tracking unavailable: ephemeris calculation failed.");
    }
    m_moonTarget = moon;
    emit moonTargetChanged(m_moonTarget);

    const SunEphemeris se = sunTopocentricAzEl(nowUtc,
                                               m_config.homeLatitudeDeg,
                                               m_config.homeLongitudeDeg);
    sun.valid = se.valid;
    sun.azimuthDeg = se.azimuthDeg;
    sun.elevationDeg = se.elevationDeg;
    sun.aboveHorizon = se.valid && se.elevationDeg >= 0.0;
    if (sun.valid) {
        sun.statusText = sun.aboveHorizon
            ? QStringLiteral("Sun: Az %1° / El %2°")
                  .arg(QString::number(sun.azimuthDeg, 'f', 1), QString::number(sun.elevationDeg, 'f', 1))
            : QStringLiteral("Sun: below horizon | Az %1° / El %2°")
                  .arg(QString::number(sun.azimuthDeg, 'f', 1), QString::number(sun.elevationDeg, 'f', 1));
    } else {
        sun.statusText = QStringLiteral("Sun tracking unavailable: ephemeris calculation failed.");
    }
    m_sunTarget = sun;
    emit sunTargetChanged(m_sunTarget);

    if (m_trackingMode != TrackingMode::Moon) {
        return;
    }
    if (!moon.valid) {
        setStatus(moon.statusText);
        return;
    }

    if (!moon.aboveHorizon) {
        m_targetAz = logicalAzimuthForTarget(moon.azimuthDeg);
        m_targetEl = m_config.useElevation ? qMax(m_config.elevationMinDeg, 0.0) : 0.0;
        emit targetChanged(m_targetAz, m_targetEl, QStringLiteral("Moon / EME tracking below horizon"));
        setStatus(moon.statusText);
        return;
    }

    const double targetAz = moon.azimuthDeg;
    const double targetEl = qBound(m_config.elevationMinDeg, moon.elevationDeg, m_config.elevationMaxDeg);
    m_targetAz = logicalAzimuthForTarget(targetAz);
    m_targetEl = m_config.useElevation ? targetEl : 0.0;
    emit targetChanged(m_targetAz, m_targetEl, QStringLiteral("Moon / EME tracking"));

    if (!m_connected) {
        setStatus(QStringLiteral("Moon / EME target staged: Az %1° / El %2°; rotator disconnected.")
                      .arg(QString::number(targetAz, 'f', 1), QString::number(targetEl, 'f', 1)));
        return;
    }
    if (!m_config.stationIdentityOk) {
        setStatus(QStringLiteral("Moon tracking blocked: set My Call and My Locator in Settings -> User/QTH first."));
        return;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const bool firstCommand = m_lastMoonCommandMs <= 0;
    const bool targetMoved = angularDifferenceDeg(m_lastMoonCommandAz, targetAz) > 0.20
                          || qAbs(m_lastMoonCommandEl - targetEl) > 0.20;
    const bool refreshDue = now - m_lastMoonCommandMs > 60000;
    if (firstCommand || targetMoved || refreshDue) {
        m_lastMoonCommandMs = now;
        m_lastMoonCommandAz = targetAz;
        m_lastMoonCommandEl = targetEl;
        setAzEl(targetAz, targetEl, QStringLiteral("Moon / EME tracking"));
    }
}

bool CatRotatorController::hasValidAzimuthCalibration() const
{
    return m_config.azimuthMsPerDeg > 0.1;
}

bool CatRotatorController::hasValidElevationCalibration() const
{
    return !m_config.useElevation || m_config.elevationMsPerDeg > 0.1;
}

int CatRotatorController::estimatePointingTimeMs(double targetAzimuthDeg, double targetElevationDeg) const
{
    if (!m_config.enabled) {
        return 0;
    }
    const double azDelta = configuredAzimuthDifferenceDeg(m_currentAz, targetAzimuthDeg);
    const double azMsPerDeg = hasValidAzimuthCalibration() ? m_config.azimuthMsPerDeg : 120.0;
    double ms = static_cast<double>(m_config.startupDelayMs + m_config.settleDelayMs + m_config.txGuardMarginMs) + azDelta * azMsPerDeg;
    if (m_config.useElevation) {
        const double elDelta = qAbs(m_currentEl - clampElevation(targetElevationDeg));
        const double elMsPerDeg = hasValidElevationCalibration() ? m_config.elevationMsPerDeg : azMsPerDeg;
        ms += elDelta * elMsPerDeg;
    }
    return qMax(0, static_cast<int>(std::ceil(ms)));
}

bool CatRotatorController::isReadyForTarget(double targetAzimuthDeg, double targetElevationDeg) const
{
    if (!m_config.enabled) {
        return true;
    }
    if (!m_connected || m_motionActive || isCalibrating()) {
        return false;
    }
    const double azDelta = configuredAzimuthDifferenceDeg(m_currentAz, targetAzimuthDeg);
    const double elDelta = qAbs(m_currentEl - clampElevation(targetElevationDeg));
    return azDelta <= qMax(1.0, static_cast<double>(m_config.targetToleranceDeg)) &&
           (!m_config.useElevation || elDelta <= qMax(1.0, static_cast<double>(m_config.targetToleranceDeg)));
}

void CatRotatorController::moveBackend(double azimuthDeg, double elevationDeg, const QString &reason)
{
    m_targetAz = azimuthDeg;
    m_targetEl = elevationDeg;
    emit targetChanged(m_targetAz, m_targetEl, reason);
#ifdef MADMODEM_WITH_HAMLIB
    if (m_rot != nullptr) {
        const int rc = rot_set_position(static_cast<ROT *>(m_rot), azimuthDeg, elevationDeg);
        if (rc == RIG_OK) {
            m_motionCommandStartMs = QDateTime::currentMSecsSinceEpoch();
            m_lastMotionProgressMs = m_motionCommandStartMs;
            m_lastMotionProgressAz = m_currentAz;
            m_lastMotionProgressEl = m_currentEl;
            setMotionActive(true);
            setStatus(QStringLiteral("CatRotator moving to %1°%2%3.")
                          .arg(QString::number(azimuthDeg, 'f', 1),
                               m_config.useElevation ? QStringLiteral(" / %1°").arg(QString::number(elevationDeg, 'f', 1)) : QString(),
                               reason.trimmed().isEmpty() ? QString() : QStringLiteral(" — %1").arg(reason.trimmed())));
            pollNow();
        } else {
            setStatus(QStringLiteral("CatRotator set position failed: %1").arg(QString::fromLocal8Bit(rigerror(rc))));
        }
    }
#else
    Q_UNUSED(azimuthDeg)
    Q_UNUSED(elevationDeg)
    Q_UNUSED(reason)
#endif
}

void CatRotatorController::startAzimuthCalibration()
{
    beginCalibration(CalibrationState::AzToMin, QStringLiteral("azimuth"));
}

void CatRotatorController::startElevationCalibration()
{
    if (!m_config.useElevation) {
        setStatus(QStringLiteral("Elevation calibration skipped: this rotator profile has no elevation axis."));
        return;
    }
    beginCalibration(CalibrationState::ElToMin, QStringLiteral("elevation"));
}

void CatRotatorController::cancelCalibration()
{
    if (m_calibrationState == CalibrationState::Idle) {
        return;
    }
    m_calibrationTimer.stop();
    m_calibrationState = CalibrationState::Idle;
    emit calibrationProgress(0, QStringLiteral("Calibration cancelled."));
    setStatus(QStringLiteral("CatRotator calibration cancelled."));
}

void CatRotatorController::beginCalibration(CalibrationState initialState, const QString &axisLabel)
{
    if (!m_config.enabled) {
        setStatus(QStringLiteral("Calibration refused: CatRotator is disabled."));
        return;
    }
    if (!m_config.stationIdentityOk) {
        setStatus(QStringLiteral("Calibration refused: set My Call and My Locator in Settings -> User/QTH first."));
        return;
    }
    if (!m_connected) {
        setStatus(QStringLiteral("Calibration refused: connect the rotator first."));
        return;
    }
    if (m_calibrationState != CalibrationState::Idle) {
        setStatus(QStringLiteral("Calibration already running."));
        return;
    }

    pollNow();
    m_calibrationHomeAz = m_currentAz;
    m_calibrationHomeEl = m_currentEl;
    m_calibrationTargetAz = m_currentAz;
    m_calibrationTargetEl = m_currentEl;
    m_calibrationTravelDeg = 0.0;
    m_calibrationAxisLabel = axisLabel;

    const bool azimuthCalibration = (initialState == CalibrationState::AzToMin);
    if (azimuthCalibration) {
        constexpr double kAzCalibrationTravelDeg = 45.0;
        const double minAz = m_config.azimuthMinDeg;
        const double maxAz = m_config.azimuthMaxDeg;
        const double current = m_currentAz;

        double forwardTarget = current + kAzCalibrationTravelDeg;
        double backwardTarget = current - kAzCalibrationTravelDeg;
        if (m_config.overlap) {
            forwardTarget = qMin(maxAz, forwardTarget);
            backwardTarget = qMax(minAz, backwardTarget);
        } else {
            forwardTarget = normalizeAzimuth(forwardTarget);
            backwardTarget = normalizeAzimuth(backwardTarget);
        }

        const double forwardTravel = m_config.overlap ? qAbs(forwardTarget - current)
                                                      : configuredAzimuthDifferenceDeg(forwardTarget, current);
        const double backwardTravel = m_config.overlap ? qAbs(current - backwardTarget)
                                                       : configuredAzimuthDifferenceDeg(backwardTarget, current);

        if (forwardTravel >= kAzCalibrationTravelDeg - 0.5) {
            m_calibrationTargetAz = forwardTarget;
            m_calibrationTravelDeg = forwardTravel;
        } else if (backwardTravel >= kAzCalibrationTravelDeg - 0.5) {
            m_calibrationTargetAz = backwardTarget;
            m_calibrationTravelDeg = backwardTravel;
        } else if (forwardTravel >= backwardTravel && forwardTravel >= 5.0) {
            m_calibrationTargetAz = forwardTarget;
            m_calibrationTravelDeg = forwardTravel;
        } else if (backwardTravel >= 5.0) {
            m_calibrationTargetAz = backwardTarget;
            m_calibrationTravelDeg = backwardTravel;
        } else {
            setStatus(QStringLiteral("Azimuth calibration refused: available mechanical travel is too small."));
            emit calibrationProgress(0, QStringLiteral("Azimuth calibration refused: available mechanical travel is too small."));
            return;
        }
        m_calibrationTargetEl = m_currentEl;
    } else {
        constexpr double kPreferredElCalibrationTravelDeg = 20.0;
        constexpr double kMinimumElCalibrationTravelDeg = 10.0;
        const double minEl = m_config.elevationMinDeg;
        const double maxEl = m_config.elevationMaxDeg;
        const double current = m_currentEl;
        const double upTravel = qMax(0.0, qMin(kPreferredElCalibrationTravelDeg, maxEl - current));
        const double downTravel = qMax(0.0, qMin(kPreferredElCalibrationTravelDeg, current - minEl));

        if (upTravel >= kMinimumElCalibrationTravelDeg) {
            m_calibrationTargetEl = current + upTravel;
            m_calibrationTravelDeg = upTravel;
        } else if (downTravel >= kMinimumElCalibrationTravelDeg) {
            m_calibrationTargetEl = current - downTravel;
            m_calibrationTravelDeg = downTravel;
        } else {
            const double fullSpan = qMax(0.0, maxEl - minEl);
            if (fullSpan >= kMinimumElCalibrationTravelDeg && qAbs(current - minEl) <= 2.0) {
                m_calibrationTargetEl = qMin(maxEl, current + kMinimumElCalibrationTravelDeg);
                m_calibrationTravelDeg = qAbs(m_calibrationTargetEl - current);
            } else if (fullSpan >= kMinimumElCalibrationTravelDeg && qAbs(current - maxEl) <= 2.0) {
                m_calibrationTargetEl = qMax(minEl, current - kMinimumElCalibrationTravelDeg);
                m_calibrationTravelDeg = qAbs(m_calibrationTargetEl - current);
            } else {
                setStatus(QStringLiteral("Elevation calibration refused: no usable elevation travel. Check profile elevation min/max and enable elevation axis."));
                emit calibrationProgress(0, QStringLiteral("Elevation calibration refused: no usable elevation travel."));
                return;
            }
        }
        m_calibrationTargetAz = m_currentAz;
    }

    m_calibrationState = initialState;
    m_calibrationLegStartMs = QDateTime::currentMSecsSinceEpoch();
    emit calibrationProgress(1, QStringLiteral("Starting %1 auto-calibration over %2° travel. TX must remain inhibited.")
                                  .arg(axisLabel, QString::number(m_calibrationTravelDeg, 'f', 1)));
    setStatus(QStringLiteral("CatRotator %1 auto-calibration started: moving %2° then returning to initial position.")
                  .arg(axisLabel, QString::number(m_calibrationTravelDeg, 'f', 1)));
    m_calibrationTimer.start(qMax(250, m_config.pollIntervalMs));

    if (azimuthCalibration) {
        moveBackend(m_calibrationTargetAz, m_currentEl, QStringLiteral("auto-calibration azimuth sample travel"));
    } else {
        moveBackend(m_currentAz, m_calibrationTargetEl, QStringLiteral("auto-calibration elevation sample travel"));
    }
}

bool CatRotatorController::calibrationLegTimedOut(qint64 maxMs) const
{
    return QDateTime::currentMSecsSinceEpoch() - m_calibrationLegStartMs > maxMs;
}

void CatRotatorController::advanceCalibration()
{
    if (m_calibrationState == CalibrationState::Idle) {
        m_calibrationTimer.stop();
        return;
    }
    pollNow();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 legMs = qMax<qint64>(qint64{1}, now - m_calibrationLegStartMs);

    const double nominalAzMsPerDeg = hasValidAzimuthCalibration() ? m_config.azimuthMsPerDeg : 1000.0;
    const double nominalElMsPerDeg = hasValidElevationCalibration() ? m_config.elevationMsPerDeg : nominalAzMsPerDeg;
    const qint64 maxAzLegMs = qBound<qint64>(qint64{15000},
                                             static_cast<qint64>((qMax(1.0, m_calibrationTravelDeg) * nominalAzMsPerDeg * 4.0) + 10000.0),
                                             qint64{180000});
    const qint64 maxElLegMs = qBound<qint64>(qint64{15000},
                                             static_cast<qint64>((qMax(1.0, m_calibrationTravelDeg) * nominalElMsPerDeg * 4.0) + 10000.0),
                                             qint64{180000});

    auto finish = [&](const QString &message) {
        m_calibrationState = CalibrationState::Idle;
        m_calibrationTimer.stop();
        m_config.calibrationStampUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        emit calibrationProgress(100, message);
        emit calibrationFinished(m_config, message);
        setStatus(message);
    };

    auto failAndReturn = [&](const QString &message) {
        emit calibrationProgress(85, message + QStringLiteral(" Returning to initial position."));
        setStatus(message);
        m_calibrationState = (m_calibrationAxisLabel == QStringLiteral("azimuth"))
            ? CalibrationState::AzReturnHome
            : CalibrationState::ElReturnHome;
        m_calibrationLegStartMs = now;
        moveBackend(m_calibrationHomeAz, m_calibrationHomeEl, QStringLiteral("auto-calibration return home after failed sample"));
    };

    switch (m_calibrationState) {
    case CalibrationState::AzToMin:
        emit calibrationProgress(35, QStringLiteral("Azimuth calibration: measuring %1° sample rotation.")
                                      .arg(QString::number(m_calibrationTravelDeg, 'f', 1)));
        if (!m_motionActive || calibrationLegTimedOut(maxAzLegMs)) {
            const double actualTravel = configuredAzimuthDifferenceDeg(m_currentAz, m_calibrationHomeAz);
            if (actualTravel < qMin(10.0, m_calibrationTravelDeg * 0.5)) {
                failAndReturn(QStringLiteral("Azimuth calibration failed: no measurable movement detected."));
                break;
            }
            const double measuredTravel = qMax(actualTravel, m_calibrationTravelDeg);
            m_config.azimuthMsPerDeg = static_cast<double>(legMs) / measuredTravel;
            m_calibrationState = CalibrationState::AzReturnHome;
            m_calibrationLegStartMs = now;
            moveBackend(m_calibrationHomeAz, m_calibrationHomeEl, QStringLiteral("auto-calibration return home"));
        }
        break;
    case CalibrationState::AzMeasureToMax:
        m_calibrationState = CalibrationState::AzReturnHome;
        m_calibrationLegStartMs = now;
        moveBackend(m_calibrationHomeAz, m_calibrationHomeEl, QStringLiteral("auto-calibration return home"));
        break;
    case CalibrationState::AzReturnHome:
        emit calibrationProgress(85, QStringLiteral("Azimuth calibration: returning to initial position."));
        if (!m_motionActive || calibrationLegTimedOut(maxAzLegMs)) {
            finish(QStringLiteral("Azimuth calibration complete: %1 ms/degree saved over %2° sample.")
                       .arg(QString::number(m_config.azimuthMsPerDeg, 'f', 1),
                            QString::number(m_calibrationTravelDeg, 'f', 1)));
        }
        break;
    case CalibrationState::ElToMin:
        emit calibrationProgress(35, QStringLiteral("Elevation calibration: measuring %1° sample movement.")
                                      .arg(QString::number(m_calibrationTravelDeg, 'f', 1)));
        if (!m_motionActive || calibrationLegTimedOut(maxElLegMs)) {
            const double actualTravel = qAbs(m_currentEl - m_calibrationHomeEl);
            if (actualTravel < qMin(3.0, m_calibrationTravelDeg * 0.5)) {
                failAndReturn(QStringLiteral("Elevation calibration failed: no measurable movement detected. Check elevation axis/profile and park position."));
                break;
            }
            const double measuredTravel = qMax(actualTravel, m_calibrationTravelDeg);
            m_config.elevationMsPerDeg = static_cast<double>(legMs) / measuredTravel;
            m_calibrationState = CalibrationState::ElReturnHome;
            m_calibrationLegStartMs = now;
            moveBackend(m_calibrationHomeAz, m_calibrationHomeEl, QStringLiteral("auto-calibration return home"));
        }
        break;
    case CalibrationState::ElMeasureToMax:
        m_calibrationState = CalibrationState::ElReturnHome;
        m_calibrationLegStartMs = now;
        moveBackend(m_calibrationHomeAz, m_calibrationHomeEl, QStringLiteral("auto-calibration return home"));
        break;
    case CalibrationState::ElReturnHome:
        emit calibrationProgress(85, QStringLiteral("Elevation calibration: returning to initial position."));
        if (!m_motionActive || calibrationLegTimedOut(maxElLegMs)) {
            finish(QStringLiteral("Elevation calibration complete: %1 ms/degree saved over %2° sample.")
                       .arg(QString::number(m_config.elevationMsPerDeg, 'f', 1),
                            QString::number(m_calibrationTravelDeg, 'f', 1)));
        }
        break;
    case CalibrationState::Idle:
        break;
    }
}

bool CatRotatorController::shouldTrackCurrentQsoTarget() const
{
    return m_trackingMode == TrackingMode::Qso &&
           m_config.enabled && m_config.trackSelectedQso && m_trackingQsoTarget &&
           !m_qsoTarget.callsign.trimmed().isEmpty() && m_qsoTarget.bearingDeg >= 0.0 &&
           (!m_config.trackOnlyWhenQsoActive || m_qsoTarget.qsoActive);
}

void CatRotatorController::setStatus(const QString &text)
{
    const QString clean = text.trimmed();
    if (clean == m_status) {
        return;
    }
    m_status = clean;
    emit statusChanged(m_status);
}


void CatRotatorController::setMotionActive(bool moving)
{
    if (m_motionActive == moving) {
        return;
    }
    m_motionActive = moving;
    emit motionChanged(m_motionActive);
}

void CatRotatorController::updateMotionState()
{
    if (!m_config.enabled || !m_connected) {
        setMotionActive(false);
        return;
    }
    if (!m_motionActive) {
        return;
    }
    const double azDelta = configuredAzimuthDifferenceDeg(m_currentAz, m_targetAz);
    const double elDelta = qAbs(m_currentEl - m_targetEl);
    const bool stillMoving = azDelta > qMax(1.0, static_cast<double>(m_config.targetToleranceDeg)) ||
                             (m_config.useElevation && elDelta > 2.0);
    setMotionActive(stillMoving);
}

void CatRotatorController::checkNoMovementStall()
{
    if (!m_config.enabled || !m_connected || !m_motionActive || isCalibrating()) {
        return;
    }
    if (!m_config.autoReverseOnStall || m_autoReverseTried) {
        return;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const double azProgress = std::fabs(m_currentAz - m_lastMotionProgressAz);
    const double elProgress = qAbs(m_currentEl - m_lastMotionProgressEl);
    const double progress = m_config.useElevation ? qMax(azProgress, elProgress) : azProgress;
    if (progress >= m_config.noMovementThresholdDeg) {
        m_lastMotionProgressMs = now;
        m_lastMotionProgressAz = m_currentAz;
        m_lastMotionProgressEl = m_currentEl;
        return;
    }

    if (now - m_lastMotionProgressMs < m_config.noMovementTimeoutMs) {
        return;
    }

    const double alternateAz = logicalAzimuthForTarget(m_lastRawTargetAz, true);
    if (std::fabs(alternateAz - m_targetAz) > 0.5) {
        m_autoReverseTried = true;
        setStatus(QStringLiteral("CatRotator end-stop suspected: no movement after %1 ms; reversing path to %2°.")
                      .arg(m_config.noMovementTimeoutMs)
                      .arg(QString::number(alternateAz, 'f', 1)));
        moveBackend(alternateAz, m_targetEl, QStringLiteral("auto-reverse after end-stop detection"));
        return;
    }

    m_autoReverseTried = true;
    setMotionActive(false);
    setStatus(QStringLiteral("CatRotator end-stop suspected: no movement after %1 ms and no alternate mechanical path is available.")
                  .arg(m_config.noMovementTimeoutMs));
}

double CatRotatorController::signedAngularSpanDeg(double start, double stop)
{
    double span = stop - start;
    if (span < 0.0) span += 360.0;
    if (span <= 0.0) span = 360.0;
    return span;
}

double CatRotatorController::clampConfiguredAzimuth(double value) const
{
    if ((value < 0.0 || value >= 360.0) && value >= m_config.azimuthMinDeg && value <= m_config.azimuthMaxDeg) {
        return value;
    }
    const double display = normalizeAzimuth(value);
    QVector<double> candidates;
    for (int k = -2; k <= 2; ++k) {
        const double c = display + 360.0 * static_cast<double>(k);
        if (c >= m_config.azimuthMinDeg && c <= m_config.azimuthMaxDeg) {
            candidates << c;
        }
    }
    if (!candidates.isEmpty()) {
        double best = candidates.first();
        double bestDelta = std::numeric_limits<double>::max();
        for (double c : candidates) {
            const double d = std::fabs(m_currentAz - c);
            if (d < bestDelta) {
                bestDelta = d;
                best = c;
            }
        }
        return best;
    }
    return qBound(m_config.azimuthMinDeg, value, m_config.azimuthMaxDeg);
}

double CatRotatorController::logicalAzimuthForTarget(double targetAzimuthDeg) const
{
    return logicalAzimuthForTarget(targetAzimuthDeg, false);
}

double CatRotatorController::logicalAzimuthForTarget(double targetAzimuthDeg, bool preferAlternate) const
{
    const double display = normalizeAzimuth(targetAzimuthDeg);
    QVector<double> candidates;
    if ((targetAzimuthDeg < 0.0 || targetAzimuthDeg >= 360.0) &&
        targetAzimuthDeg >= m_config.azimuthMinDeg && targetAzimuthDeg <= m_config.azimuthMaxDeg) {
        candidates << targetAzimuthDeg;
    }
    for (int k = -2; k <= 2; ++k) {
        const double c = display + 360.0 * static_cast<double>(k);
        if (c >= m_config.azimuthMinDeg && c <= m_config.azimuthMaxDeg) {
            candidates << c;
        }
    }
    if (candidates.isEmpty()) {
        return qBound(m_config.azimuthMinDeg, display, m_config.azimuthMaxDeg);
    }

    std::sort(candidates.begin(), candidates.end(), [this](double a, double b) {
        return std::fabs(m_currentAz - a) < std::fabs(m_currentAz - b);
    });
    if (preferAlternate && candidates.size() > 1) {
        return candidates.at(1);
    }
    return candidates.first();
}

double CatRotatorController::configuredAzimuthDifferenceDeg(double currentAzimuthDeg, double targetAzimuthDeg) const
{
    return std::fabs(clampConfiguredAzimuth(currentAzimuthDeg) - logicalAzimuthForTarget(targetAzimuthDeg));
}

double CatRotatorController::angularDifferenceDeg(double a, double b)
{
    double d = std::fabs(normalizeAzimuth(a) - normalizeAzimuth(b));
    if (d > 180.0) d = 360.0 - d;
    return d;
}

double CatRotatorController::normalizeAzimuth(double value)
{
    double az = std::fmod(value, 360.0);
    if (az < 0.0) az += 360.0;
    if (az >= 360.0) az -= 360.0;
    return az;
}

double CatRotatorController::clampElevation(double value)
{
    return qBound(-10.0, value, 180.0);
}

} // namespace mm
