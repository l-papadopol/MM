#include "QsoMapWidget.h"
#include "../dxcc/CtyCountryFile.h"

#include <QDateTime>
#include <QCoreApplication>
#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QSettings>
#include <QSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHideEvent>
#include <QLineF>
#include <QMessageBox>
#include <QDir>
#include <QMouseEvent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPainterPath>
#include <QPageLayout>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSslError>
#include <QSslSocket>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QVariantMap>
#include <QVBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include <QPrintDialog>
#include <QRegularExpression>
#include <QPrinter>
#include <QStringList>
#include <QToolTip>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <utility>

#ifdef MADMODEM_WITH_QT_LOCATION
#include <QQmlContext>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickView>
#endif

namespace {

constexpr double kEarthRadiusKm = 6371.0;

constexpr int kOsmTileSize = 256;
constexpr double kMaxMapZoom = 8192.0;

int wrapTileX(int x, int z)
{
    const int n = 1 << z;
    int wrapped = x % n;
    if (wrapped < 0) {
        wrapped += n;
    }
    return wrapped;
}

int clampTileY(int y, int z)
{
    const int n = 1 << z;
    return qBound(0, y, n - 1);
}

double lonToTileX(double lon, int z)
{
    const double n = static_cast<double>(1 << z);
    return (lon + 180.0) / 360.0 * n;
}

double latToTileY(double lat, int z)
{
    const double clampedLat = qBound(-85.05112878, lat, 85.05112878);
    const double latRad = qDegreesToRadians(clampedLat);
    const double n = static_cast<double>(1 << z);
    return (1.0 - qLn(qTan(latRad) + 1.0 / qCos(latRad)) / M_PI) / 2.0 * n;
}

double tileXToLon(double x, int z)
{
    const double n = static_cast<double>(1 << z);
    return x / n * 360.0 - 180.0;
}

double tileYToLat(double y, int z)
{
    const double n = static_cast<double>(1 << z);
    const double a = M_PI * (1.0 - 2.0 * y / n);
    return qRadiansToDegrees(qAtan(std::sinh(a)));
}

double lonToMercatorX01(double lon)
{
    return (lon + 180.0) / 360.0;
}

double latToMercatorY01(double lat)
{
    return latToTileY(lat, 0);
}

double mercatorX01ToLon(double x)
{
    return x * 360.0 - 180.0;
}

double mercatorY01ToLat(double y)
{
    return tileYToLat(y, 0);
}

bool ctyCentroidForCoarseEntry(const LogbookEntry &entry, QPointF *lonLat, QString *mapGrid)
{
    if (lonLat == nullptr) {
        return false;
    }

    const QString originalGrid = entry.grid.trimmed().toUpper();
    if (originalGrid.size() >= 6) {
        return false;
    }

    const CtyCountryFile::LookupResult cty = CtyCountryFile::instance().lookupCallsign(entry.callsign);
    if (!cty.valid || cty.entity.referenceGrid.size() < 6) {
        return false;
    }

    const QString refGrid = cty.entity.referenceGrid.trimmed().toUpper();
    if (!originalGrid.isEmpty() && refGrid.left(4) != originalGrid.left(4)) {
        return false;
    }

    if (!std::isfinite(cty.entity.longitude) || !std::isfinite(cty.entity.latitude)) {
        return false;
    }

    *lonLat = QPointF(cty.entity.longitude, cty.entity.latitude);
    if (mapGrid != nullptr) {
        *mapGrid = refGrid;
    }
    return true;
}

QString osmTileKey(int z, int x, int y)
{
    return QStringLiteral("%1/%2/%3").arg(z).arg(x).arg(y);
}

QString htmlEsc(const QString &s)
{
    QString out = s;
    out.replace('&', "&amp;");
    out.replace('<', "&lt;");
    out.replace('>', "&gt;");
    out.replace('"', "&quot;");
    return out;
}

QStringList configuredOsmTileBaseUrls(bool useHttpFallback)
{
    Q_UNUSED(useHttpFallback);

    const QByteArray env = qgetenv("MADMODEM_OSM_TILE_BASE").trimmed();
    QStringList bases;
    if (!env.isEmpty()) {
        /*
         * Operator override.  This may point to a local tile cache/proxy, for
         * example http://127.0.0.1:8080/osm.  It is intentionally the only way
         * to use plain HTTP in v1.90: the public OSM tile service requires HTTPS
         * and a real application User-Agent.
         */
        const QString envText = QString::fromUtf8(env);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
        bases = envText.split(QRegularExpression(QStringLiteral("[;,\\s]+")), Qt::SkipEmptyParts);
#else
        bases = envText.split(QRegularExpression(QStringLiteral("[;,\\s]+")), QString::SkipEmptyParts);
#endif
    } else {
        bases << QStringLiteral("https://tile.openstreetmap.org");
    }

    for (QString &base : bases) {
        while (base.endsWith(QChar('/'))) {
            base.chop(1);
        }
    }
    bases.removeAll(QString());
    if (bases.isEmpty()) {
        bases << QStringLiteral("https://tile.openstreetmap.org");
    }
    return bases;
}

QString configuredOsmTileBaseUrl(bool useHttpFallback, int providerIndex)
{
    const QStringList bases = configuredOsmTileBaseUrls(useHttpFallback);
    if (bases.isEmpty()) {
        return QStringLiteral("https://tile.openstreetmap.org");
    }
    const int idx = qBound(0, providerIndex, bases.size() - 1);
    return bases.at(idx);
}

int configuredOsmTileProviderCount(bool useHttpFallback)
{
    return qMax(1, configuredOsmTileBaseUrls(useHttpFallback).size());
}

QPolygonF landPoly(const std::initializer_list<QPointF> &pts)
{
    QPolygonF p;
    for (const QPointF &pt : pts) {
        p << pt;
    }
    return p;
}

QVector<QPolygonF> worldLandPolygons()
{
    // Lightweight hand-drawn silhouettes.  They are not a GIS source; they just
    // give the operator enough orientation without adding online map/tile deps.
    return {
        landPoly({{-168, 72}, {-140, 70}, {-126, 55}, {-124, 42}, {-118, 32}, {-105, 23}, {-96, 15}, {-84, 12}, {-78, 25}, {-86, 42}, {-100, 55}, {-125, 68}, {-150, 73}}),
        landPoly({{-82, 13}, {-78, -5}, {-72, -18}, {-64, -35}, {-58, -53}, {-46, -55}, {-38, -40}, {-35, -20}, {-48, -8}, {-60, 6}, {-70, 12}}),
        landPoly({{-18, 36}, {2, 58}, {30, 70}, {70, 72}, {105, 64}, {145, 55}, {170, 45}, {160, 22}, {120, 12}, {90, 8}, {62, 24}, {42, 35}, {30, 30}, {14, 44}, {0, 45}}),
        landPoly({{-18, 34}, {5, 37}, {32, 31}, {50, 12}, {45, -20}, {30, -35}, {18, -34}, {10, -5}, {-5, 5}, {-15, 20}}),
        landPoly({{112, -11}, {154, -10}, {153, -38}, {132, -44}, {116, -33}}),
        landPoly({{-50, 72}, {-20, 78}, {-15, 64}, {-42, 58}}),
        landPoly({{-180, -62}, {-90, -70}, {0, -66}, {90, -70}, {180, -62}, {180, -82}, {-180, -82}})
    };
}

} // namespace

QsoMapWidget::QsoMapWidget(QWidget *parent)
    : QWidget(parent)
{
    loadDisplaySettings();
    setMinimumSize(640, 360);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAutoFillBackground(false);
    m_worldMap = QPixmap(QStringLiteral(":/icons/planet_map.png"));
    if (m_worldMap.isNull()) {
        m_worldMap = QPixmap(QStringLiteral(":/icons/planet_map.jpg"));
    }
    if (m_worldMap.isNull() && QCoreApplication::instance() != nullptr) {
        // Windows static deployments can miss the JPEG image plugin while PNG
        // support is normally built into QtGui.  Try the bundled PNG first,
        // then legacy JPG paths beside the executable.
        const QDir appDir(QCoreApplication::applicationDirPath());
        const QStringList candidates = {
            appDir.filePath(QStringLiteral("planet_map.png")),
            appDir.filePath(QStringLiteral("icons/planet_map.png")),
            appDir.filePath(QStringLiteral("../icons/planet_map.png")),
            appDir.filePath(QStringLiteral("../../icons/planet_map.png")),
            appDir.filePath(QStringLiteral("planet_map.jpg")),
            appDir.filePath(QStringLiteral("icons/planet_map.jpg")),
            appDir.filePath(QStringLiteral("../icons/planet_map.jpg")),
            appDir.filePath(QStringLiteral("../../icons/planet_map.jpg"))
        };
        for (const QString &candidate : candidates) {
            if (QFileInfo::exists(candidate) && m_worldMap.load(candidate)) {
                break;
            }
        }
    }
    m_tileNetwork = new QNetworkAccessManager(this);
    if (!QSslSocket::supportsSsl()) {
        m_onlineMapFailureReason = tr("Qt SSL/OpenSSL is unavailable; online OSM HTTPS tiles will use the offline map unless MADMODEM_OSM_TILE_BASE points to a local HTTP tile proxy/cache.");
    }
    logOsmDiagnostics(QStringLiteral("startup"));

#ifdef MADMODEM_WITH_QT_LOCATION
    if (QCoreApplication::instance() != nullptr) {
        connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit,
                this, &QsoMapWidget::releaseQuickMapForShutdown,
                Qt::DirectConnection);
    }
#endif
}

QsoMapWidget::~QsoMapWidget()
{
#ifdef MADMODEM_WITH_QT_LOCATION
    releaseQuickMapForShutdown();
#endif
}

void QsoMapWidget::setRecords(const QVector<LogbookEntry> &records)
{
    m_records = records;
    updateQuickMapModel();
    update();
}

void QsoMapWidget::addHeardStation(const LogbookEntry &entry)
{
    if (entry.callsign.trimmed().isEmpty() || maidenheadGrid4(entry.grid).isEmpty()) {
        return;
    }

    LogbookEntry normalized = entry;
    normalized.callsign = normalized.callsign.trimmed().toUpper();
    normalized.grid = normalized.grid.trimmed().toUpper();
    if (!normalized.utc.isValid()) {
        normalized.utc = QDateTime::currentDateTimeUtc();
    }

    const QDate today = QDateTime::currentDateTimeUtc().date();
    QVector<LogbookEntry> kept;
    kept.reserve(m_heardRecords.size() + 1);
    for (const LogbookEntry &oldEntry : m_heardRecords) {
        if (oldEntry.utc.isValid() && oldEntry.utc.toUTC().date() != today) {
            continue;
        }
        const bool same = oldEntry.callsign.compare(normalized.callsign, Qt::CaseInsensitive) == 0 &&
                          maidenheadGrid4(oldEntry.grid) == maidenheadGrid4(normalized.grid) &&
                          normalizedMode(oldEntry.mode) == normalizedMode(normalized.mode);
        if (!same) {
            kept.append(oldEntry);
        }
    }
    kept.append(normalized);
    if (kept.size() > 5000) {
        std::sort(kept.begin(), kept.end(), [](const LogbookEntry &a, const LogbookEntry &b) {
            return a.utc.toUTC() > b.utc.toUTC();
        });
        kept.resize(5000);
    }
    m_heardRecords = kept;
    updateQuickMapModel();
    update();
}

void QsoMapWidget::clearHeardStations()
{
    if (m_heardRecords.isEmpty()) {
        return;
    }
    m_heardRecords.clear();
    updateQuickMapModel();
    update();
}

void QsoMapWidget::setDisplayBehavior(DisplayBehavior behavior)
{
    if (m_displayBehavior == behavior) {
        return;
    }
    m_displayBehavior = behavior;
    saveDisplaySettings();
    updateQuickMapModel();
    update();
}

QsoMapWidget::DisplayBehavior QsoMapWidget::displayBehavior() const
{
    return m_displayBehavior;
}

QString QsoMapWidget::displayBehaviorName() const
{
    switch (m_displayBehavior) {
    case DisplayBehavior::HeardToday:
        return QStringLiteral("heard_today");
    case DisplayBehavior::WorkedDxcc:
        return QStringLiteral("worked_dxcc");
    case DisplayBehavior::LogbookQsos:
    default:
        return QStringLiteral("logbook_qsos");
    }
}

void QsoMapWidget::setModeFilter(const QString &modeFilter)
{
    m_modeFilter = normalizedMode(modeFilter);
    updateQuickMapModel();
    update();
}

void QsoMapWidget::setHomeGrid(const QString &grid)
{
    m_homeGrid = grid.trimmed().toUpper();
    updateQuickMapModel();
    update();
}

void QsoMapWidget::setShowPaths(bool enabled)
{
    m_showPaths = enabled;
    updateQuickMapModel();
    update();
}

void QsoMapWidget::setShowMaidenheadGrid(bool enabled)
{
    m_showMaidenheadGrid = enabled;
    updateQuickMapModel();
    update();
}

QString QsoMapWidget::modeFilter() const
{
    return m_modeFilter;
}

bool QsoMapWidget::showPaths() const
{
    return m_showPaths;
}

bool QsoMapWidget::showMaidenheadGrid() const
{
    return m_showMaidenheadGrid;
}

void QsoMapWidget::setTextTranslator(std::function<QString(const QString &)> translator)
{
    m_textTranslator = std::move(translator);
    update();
}

QString QsoMapWidget::L(const QString &source) const
{
    if (m_textTranslator) {
        return m_textTranslator(source);
    }
    return source;
}

void QsoMapWidget::resetView()
{
    m_zoom = 1.0;
    m_pan = QPointF();
    update();
}


void QsoMapWidget::saveMap()
{
    const QString fileName = QFileDialog::getSaveFileName(this,
                                                          L(QStringLiteral("Save QSO map")),
                                                          QStringLiteral("qso-map.pdf"),
                                                          tr("PDF files (*.pdf);;PNG images (*.png);;JPEG images (*.jpg *.jpeg)"));
    if (fileName.isEmpty()) {
        return;
    }

    if (fileName.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive)) {
        QPrinter printer(QPrinter::HighResolution);
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setOutputFileName(fileName);
        printer.setPageOrientation(QPageLayout::Landscape);
        printer.setPageMargins(QMarginsF(10, 10, 10, 10), QPageLayout::Millimeter);

        QPainter painter(&printer);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        const QImage image = renderedExportImage(QSize(3000, 1688));
        drawImageToPrinterPage(&painter, printer, image);
        painter.end();
        return;
    }

    const QImage image = renderedExportImage(QSize(3000, 1688));

    if (!image.save(fileName)) {
        QMessageBox::warning(this, L(QStringLiteral("Save QSO map")), L(QStringLiteral("Unable to save the selected map file.")));
    }
}

void QsoMapWidget::printMap()
{
    QPrinter printer(QPrinter::HighResolution);
    printer.setPageOrientation(QPageLayout::Landscape);
    printer.setPageMargins(QMarginsF(10, 10, 10, 10), QPageLayout::Millimeter);

    QPrintDialog dialog(&printer, this);
    dialog.setWindowTitle(L(QStringLiteral("Print QSO map")));
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QPainter painter(&printer);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    const QImage image = renderedExportImage(QSize(3000, 1688));
    drawImageToPrinterPage(&painter, printer, image);
    painter.end();
}


void QsoMapWidget::loadDisplaySettings()
{
    QSettings settings(QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("settings.mad")), QSettings::IniFormat);
    m_mapUseModeFilter = settings.value(QStringLiteral("QsoMap/useModeFilter"), m_mapUseModeFilter).toBool();
    m_mapBandFilter = settings.value(QStringLiteral("QsoMap/bandFilter"), m_mapBandFilter).toString().trimmed();
    m_mapDateScope = settings.value(QStringLiteral("QsoMap/dateScope"), m_mapDateScope).toString().trimmed().toLower();
    if (m_mapDateScope != QStringLiteral("today") && m_mapDateScope != QStringLiteral("last7") &&
        m_mapDateScope != QStringLiteral("last30") && m_mapDateScope != QStringLiteral("all")) {
        m_mapDateScope = QStringLiteral("today");
    }
    m_mapLatestPerGrid = settings.value(QStringLiteral("QsoMap/latestPerGrid"), m_mapLatestPerGrid).toBool();
    m_mapMaxMarkers = settings.value(QStringLiteral("QsoMap/maxMarkers"), m_mapMaxMarkers).toInt();
    if (m_mapMaxMarkers < 50 || m_mapMaxMarkers > 10000) {
        m_mapMaxMarkers = 1000;
    }
}

void QsoMapWidget::saveDisplaySettings() const
{
    QSettings settings(QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("settings.mad")), QSettings::IniFormat);
    settings.setValue(QStringLiteral("QsoMap/displayBehavior"), displayBehaviorName());
    settings.setValue(QStringLiteral("QsoMap/useModeFilter"), m_mapUseModeFilter);
    settings.setValue(QStringLiteral("QsoMap/bandFilter"), m_mapBandFilter.trimmed());
    settings.setValue(QStringLiteral("QsoMap/dateScope"), m_mapDateScope);
    settings.setValue(QStringLiteral("QsoMap/latestPerGrid"), m_mapLatestPerGrid);
    settings.setValue(QStringLiteral("QsoMap/maxMarkers"), m_mapMaxMarkers);
    settings.sync();
}

bool QsoMapWidget::configureLayerSettings()
{
    QDialog dialog(this);
    dialog.setWindowTitle(L(QStringLiteral("QSO map layers")));
    QVBoxLayout *outer = new QVBoxLayout(&dialog);

    QLabel *hint = new QLabel(L(QStringLiteral("Choose visible QSO-map layers, marker sources and filtering. These settings are saved for the map.")), &dialog);
    hint->setWordWrap(true);
    outer->addWidget(hint);

    QGroupBox *visibleGroup = new QGroupBox(L(QStringLiteral("Visible layers")), &dialog);
    QGridLayout *visible = new QGridLayout(visibleGroup);
    QCheckBox *paths = new QCheckBox(L(QStringLiteral("Home to QSO paths")), visibleGroup);
    paths->setChecked(m_showPaths);
    QCheckBox *grid = new QCheckBox(L(QStringLiteral("Maidenhead grid")), visibleGroup);
    grid->setChecked(m_showMaidenheadGrid);
    QCheckBox *workedSquares = new QCheckBox(L(QStringLiteral("Worked square shading")), visibleGroup);
    workedSquares->setChecked(m_showMaidenheadGrid);
    workedSquares->setToolTip(L(QStringLiteral("Worked-square shading is shown together with the Maidenhead grid.")));
    QCheckBox *markerLabels = new QCheckBox(L(QStringLiteral("Marker tooltips")), visibleGroup);
    markerLabels->setChecked(true);
    markerLabels->setEnabled(false);
    QCheckBox *osmTiles = new QCheckBox(L(QStringLiteral("OpenStreetMap raster tiles")), visibleGroup);
    osmTiles->setChecked(true);
    osmTiles->setEnabled(false);
    osmTiles->setToolTip(L(QStringLiteral("OSM tiles are used automatically when available; the offline map remains the fallback.")));
    visible->addWidget(paths, 0, 0);
    visible->addWidget(grid, 0, 1);
    visible->addWidget(workedSquares, 1, 0);
    visible->addWidget(markerLabels, 1, 1);
    visible->addWidget(osmTiles, 2, 0, 1, 2);
    outer->addWidget(visibleGroup);

    QGroupBox *sourceGroup = new QGroupBox(L(QStringLiteral("Marker source")), &dialog);
    QFormLayout *sourceForm = new QFormLayout(sourceGroup);
    QComboBox *behavior = new QComboBox(sourceGroup);
    behavior->addItem(L(QStringLiteral("Logged QSOs")), QStringLiteral("logbook_qsos"));
    behavior->addItem(L(QStringLiteral("Heard stations today")), QStringLiteral("heard_today"));
    behavior->addItem(L(QStringLiteral("Worked DXCC countries")), QStringLiteral("worked_dxcc"));
    const int behaviorIndex = behavior->findData(displayBehaviorName());
    behavior->setCurrentIndex(behaviorIndex >= 0 ? behaviorIndex : 0);
    sourceForm->addRow(L(QStringLiteral("Source")), behavior);
    outer->addWidget(sourceGroup);

    QGroupBox *filtersGroup = new QGroupBox(L(QStringLiteral("Filters and marker reduction")), &dialog);
    QFormLayout *form = new QFormLayout(filtersGroup);

    QCheckBox *useMode = new QCheckBox(L(QStringLiteral("Use current mode filter")), filtersGroup);
    useMode->setChecked(m_mapUseModeFilter);
    form->addRow(L(QStringLiteral("Mode")), useMode);

    QLineEdit *band = new QLineEdit(m_mapBandFilter, filtersGroup);
    band->setPlaceholderText(L(QStringLiteral("empty = all bands, e.g. 20m")));
    form->addRow(L(QStringLiteral("Band filter")), band);

    QComboBox *dateScope = new QComboBox(filtersGroup);
    dateScope->addItem(L(QStringLiteral("Current UTC day")), QStringLiteral("today"));
    dateScope->addItem(L(QStringLiteral("Last 7 days")), QStringLiteral("last7"));
    dateScope->addItem(L(QStringLiteral("Last 30 days")), QStringLiteral("last30"));
    dateScope->addItem(L(QStringLiteral("All logbook")), QStringLiteral("all"));
    const int scopeIndex = dateScope->findData(m_mapDateScope);
    dateScope->setCurrentIndex(scopeIndex >= 0 ? scopeIndex : 0);
    form->addRow(L(QStringLiteral("Time window")), dateScope);

    QCheckBox *latestPerGrid = new QCheckBox(L(QStringLiteral("Show only latest QSO per Maidenhead square")), filtersGroup);
    latestPerGrid->setChecked(m_mapLatestPerGrid);
    latestPerGrid->setToolTip(L(QStringLiteral("Recommended for large ADIF logs: 50,000 QSOs become one marker per worked square.")));
    form->addRow(L(QStringLiteral("Marker reduction")), latestPerGrid);

    QSpinBox *maxMarkers = new QSpinBox(filtersGroup);
    maxMarkers->setRange(50, 10000);
    maxMarkers->setSingleStep(50);
    maxMarkers->setValue(m_mapMaxMarkers);
    form->addRow(L(QStringLiteral("Maximum markers")), maxMarkers);
    outer->addWidget(filtersGroup);

    QLabel *note = new QLabel(L(QStringLiteral("Available map layers now include OSM/fallback map background, Home→QSO paths, Maidenhead grid with worked-square shading, marker tooltips, logbook QSO markers, heard-today markers and DXCC summary markers.")), &dialog);
    note->setWordWrap(true);
    outer->addWidget(note);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    outer->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    m_showPaths = paths->isChecked();
    m_showMaidenheadGrid = grid->isChecked() || workedSquares->isChecked();
    const QString behaviorValue = behavior->currentData().toString();
    if (behaviorValue == QStringLiteral("heard_today")) {
        m_displayBehavior = DisplayBehavior::HeardToday;
    } else if (behaviorValue == QStringLiteral("worked_dxcc")) {
        m_displayBehavior = DisplayBehavior::WorkedDxcc;
    } else {
        m_displayBehavior = DisplayBehavior::LogbookQsos;
    }
    m_mapUseModeFilter = useMode->isChecked();
    m_mapBandFilter = band->text().trimmed();
    m_mapDateScope = dateScope->currentData().toString();
    m_mapLatestPerGrid = latestPerGrid->isChecked();
    m_mapMaxMarkers = maxMarkers->value();
    saveDisplaySettings();
    updateQuickMapModel();
    update();
    return true;
}

void QsoMapWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    drawMap(&painter, rect());
}

void QsoMapWidget::wheelEvent(QWheelEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    const QPoint angle = event->angleDelta();
#else
    const QPoint angle(0, event->delta());
#endif
    if (angle.y() == 0) {
        return;
    }

    const QRect mapRect = mapRectForTarget(rect());
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    const QPointF cursorPos = event->position();
#else
    const QPointF cursorPos = QPointF(event->pos());
#endif
    const QPointF before = screenToLonLat(cursorPos, mapRect);
    const double factor = angle.y() > 0 ? 1.35 : 1.0 / 1.35;
    m_zoom = qBound(1.0, m_zoom * factor, kMaxMapZoom);

    // Keep the lon/lat under the cursor stable while zooming. This makes
    // wheel zoom behave like an actual pan/zoom map rather than a grid overlay.
    const QPointF afterScreen = lonLatToScreen(before, mapRect);
    m_pan += cursorPos - afterScreen;
    update();
    event->accept();
}

void QsoMapWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_lastMousePos = event->pos();
        m_pressPos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void QsoMapWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging) {
        const QPoint delta = event->pos() - m_lastMousePos;
        m_lastMousePos = event->pos();
        m_pan += QPointF(delta.x(), delta.y());
        update();
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void QsoMapWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        unsetCursor();
        if ((event->pos() - m_pressPos).manhattanLength() < 4) {
            showMarkerTooltip(event->pos());
        }
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void QsoMapWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        resetView();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

QRect QsoMapWidget::mapRectForTarget(const QRect &targetRect) const
{
    // Reserve a title/status band, but do not letterbox the actual map inside the
    // page.  The v1.53 2:1 clamp made the map look like a small picture centered
    // in a large tab; the QWidget OSM backend already preserves Web-Mercator
    // scale internally and can safely use the full available viewport.
    const int topBand = m_onlineMapFailureReason.isEmpty() ? 48 : 82;
    return targetRect.adjusted(14, topBand, -14, -24);
}


QRectF QsoMapWidget::projectedMapRect(const QRect &mapRect) const
{
    // OpenStreetMap slippy tiles use Web Mercator: one world tile plane is
    // square, with identical pixel scale on X and Y.  v1.39 stretched that
    // square world into the old 2:1 equirectangular map rectangle, which made
    // OSM look vertically squashed and also limited practical zoom depth.  Keep
    // a single square world span and simply clip it through the available
    // widget viewport.
    const double worldSpan = static_cast<double>(qMax(1, mapRect.width())) * m_zoom;
    const QPointF center = QPointF(mapRect.center()) + m_pan;
    return QRectF(center.x() - worldSpan / 2.0,
                  center.y() - worldSpan / 2.0,
                  worldSpan,
                  worldSpan);
}

QRectF QsoMapWidget::offlineMapRect(const QRect &mapRect) const
{
    /*
     * The bundled planet_map image is equirectangular 2:1.  Treat it as an
     * actual pannable/zoomable map plane, not as a static background stretched
     * to the widget.  This keeps Maidenhead grid, HOME and QSO markers aligned
     * with the JPG while still allowing mouse wheel zoom and drag pan offline.
     */
    const double aspect = 2.0;
    const double baseWidth = qMax(static_cast<double>(mapRect.width()),
                                  static_cast<double>(mapRect.height()) * aspect);
    const double baseHeight = baseWidth / aspect;
    const double width = baseWidth * m_zoom;
    const double height = baseHeight * m_zoom;
    const QPointF center = QPointF(mapRect.center()) + m_pan;
    return QRectF(center.x() - width / 2.0,
                  center.y() - height / 2.0,
                  width,
                  height);
}

QPointF QsoMapWidget::lonLatToScreen(const QPointF &lonLat, const QRect &mapRect) const
{
    /*
     * Online OSM tiles are Web-Mercator.  The bundled planet_map image is a
     * simple 2:1/equirectangular world map.  v1.58 always used the Mercator
     * transform, so when Windows fell back to the bundled image the Maidenhead
     * overlay and HOME marker were projected onto the wrong map and JN63 could
     * appear down toward Africa.  Use the matching transform for the active
     * renderer.
     */
    const bool offlineBundledMap = (m_osmTileRequestBlocked && m_osmTileCache.isEmpty());
    if (offlineBundledMap) {
        const QRectF projected = offlineMapRect(mapRect);
        const double xNorm = (lonLat.x() + 180.0) / 360.0;
        const double yNorm = (90.0 - lonLat.y()) / 180.0;
        const double x = projected.left() + xNorm * projected.width();
        const double y = projected.top() + yNorm * projected.height();
        return QPointF(x, y);
    }

    const QRectF projected = projectedMapRect(mapRect);
    const double xNorm = lonToMercatorX01(lonLat.x());
    const double yNorm = latToMercatorY01(lonLat.y());
    const double x = projected.left() + xNorm * projected.width();
    const double y = projected.top() + yNorm * projected.height();
    return QPointF(x, y);
}

QPointF QsoMapWidget::screenToLonLat(const QPointF &screen, const QRect &mapRect) const
{
    const bool offlineBundledMap = (m_osmTileRequestBlocked && m_osmTileCache.isEmpty());
    if (offlineBundledMap) {
        const QRectF projected = offlineMapRect(mapRect);
        const double xNorm = (screen.x() - projected.left()) / qMax(1.0, projected.width());
        const double yNorm = (screen.y() - projected.top()) / qMax(1.0, projected.height());
        const double lon = xNorm * 360.0 - 180.0;
        const double lat = 90.0 - yNorm * 180.0;
        return QPointF(lon, lat);
    }

    const QRectF projected = projectedMapRect(mapRect);
    const double xNorm = (screen.x() - projected.left()) / projected.width();
    const double yNorm = (screen.y() - projected.top()) / projected.height();
    const double lon = mercatorX01ToLon(xNorm);
    const double lat = mercatorY01ToLat(yNorm);
    return QPointF(lon, lat);
}

QVector<QsoMapWidget::TileSpec> QsoMapWidget::visibleOsmTiles(const QRect &mapRect) const
{
    QVector<TileSpec> tiles;
    if (mapRect.width() <= 0 || mapRect.height() <= 0) {
        return tiles;
    }

    const QRectF projected = projectedMapRect(mapRect);
    // Select a slippy-map zoom level matching the current square Web-Mercator
    // world span.  Since projected.width() == projected.height(), tile images
    // are painted with square pixels instead of being squashed into the old
    // 2:1 world map rectangle.
    const double tilesAcross = qMax(1.0, projected.width() / static_cast<double>(kOsmTileSize));
    const int z = qBound(1,
                         static_cast<int>(qFloor(qLn(tilesAcross) / qLn(2.0))),
                         16);
    const int n = 1 << z;

    const QPointF nw = screenToLonLat(QPointF(mapRect.left(), mapRect.top()), mapRect);
    const QPointF se = screenToLonLat(QPointF(mapRect.right(), mapRect.bottom()), mapRect);
    const double minLon = qBound(-180.0, qMin(nw.x(), se.x()), 180.0);
    const double maxLon = qBound(-180.0, qMax(nw.x(), se.x()), 180.0);
    const double minLat = qBound(-85.05112878, qMin(nw.y(), se.y()), 85.05112878);
    const double maxLat = qBound(-85.05112878, qMax(nw.y(), se.y()), 85.05112878);

    int x0 = static_cast<int>(qFloor(lonToTileX(minLon, z)));
    int x1 = static_cast<int>(qFloor(lonToTileX(maxLon, z)));
    int y0 = static_cast<int>(qFloor(latToTileY(maxLat, z)));
    int y1 = static_cast<int>(qFloor(latToTileY(minLat, z)));
    x0 = qBound(0, x0, n - 1);
    x1 = qBound(0, x1, n - 1);
    y0 = clampTileY(y0, z);
    y1 = clampTileY(y1, z);
    if (x1 < x0) {
        qSwap(x0, x1);
    }
    if (y1 < y0) {
        qSwap(y0, y1);
    }

    // Keep accidental extreme pans/zooms from queuing thousands of network jobs.
    const int maxTiles = 192;
    for (int ty = y0; ty <= y1; ++ty) {
        for (int tx = x0; tx <= x1; ++tx) {
            if (tiles.size() >= maxTiles) {
                return tiles;
            }
            const int wrappedX = wrapTileX(tx, z);
            const int clampedY = clampTileY(ty, z);
            const double lonLeft = tileXToLon(static_cast<double>(wrappedX), z);
            const double lonRight = tileXToLon(static_cast<double>(wrappedX + 1), z);
            const double latTop = tileYToLat(static_cast<double>(clampedY), z);
            const double latBottom = tileYToLat(static_cast<double>(clampedY + 1), z);
            const QPointF a = lonLatToScreen(QPointF(lonLeft, latTop), mapRect);
            const QPointF b = lonLatToScreen(QPointF(lonRight, latBottom), mapRect);
            QRectF target(QPointF(qMin(a.x(), b.x()), qMin(a.y(), b.y())),
                          QPointF(qMax(a.x(), b.x()), qMax(a.y(), b.y())));
            target = target.normalized();
            if (!target.intersects(QRectF(mapRect))) {
                continue;
            }
            TileSpec spec;
            spec.z = z;
            spec.x = wrappedX;
            spec.y = clampedY;
            spec.key = osmTileKey(spec.z, spec.x, spec.y);
            spec.screenRect = target;
            tiles.append(spec);
        }
    }
    return tiles;
}

void QsoMapWidget::requestOsmTile(const TileSpec &tile) const
{
    if (m_tileNetwork == nullptr || tile.key.isEmpty() ||
        m_osmTileCache.contains(tile.key) || m_osmTilePending.contains(tile.key) ||
        m_osmTileRequestBlocked) {
        return;
    }

    // Avoid creating a large burst of replies if the operator zooms/pans fast.
    if (m_osmTilePending.size() >= 32) {
        return;
    }

    const QString base = configuredOsmTileBaseUrl(m_osmUseHttpFallback, m_osmTileProviderIndex);
    QUrl url(QStringLiteral("%1/%2/%3/%4.png")
                 .arg(base)
                 .arg(tile.z)
                 .arg(tile.x)
                 .arg(tile.y));
    if (url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0 && !QSslSocket::supportsSsl()) {
        QsoMapWidget *self = const_cast<QsoMapWidget *>(this);
        self->m_osmTileRequestBlocked = true;
        self->m_onlineMapFailed = true;
        self->m_osmLastTileUrl = url.toString(QUrl::RemoveUserInfo);
        self->m_osmLastNetworkError = tr("Qt SSL/OpenSSL support is not available");
        self->m_onlineMapFailureReason = tr("OSM HTTPS tiles require Qt SSL/OpenSSL on Windows. Rebuild MXE Qt with OpenSSL or set MADMODEM_OSM_TILE_BASE to a local HTTP tile proxy/cache.");
        self->logOsmDiagnostics(QStringLiteral("ssl-unavailable"));
        self->update();
        return;
    }
    m_osmLastTileUrl = url.toString(QUrl::RemoveUserInfo);
    m_osmLastNetworkError.clear();
    m_osmLastRedirectTarget.clear();
    m_osmLastHttpStatus.clear();
    m_osmLastSslError.clear();

    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "MadModem/2.00 (QSO-map; GPL-3.0; contact=IZ6NNH)");
    request.setRawHeader("Accept", "image/png,image/*;q=0.8,*/*;q=0.5");
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
#elif QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif

    QNetworkReply *reply = m_tileNetwork->get(request);
    m_osmTilePending.insert(tile.key);
    QsoMapWidget *self = const_cast<QsoMapWidget *>(this);
    const QString key = tile.key;
    const QString scheme = url.scheme().toLower();
    QObject::connect(reply, QOverload<const QList<QSslError> &>::of(&QNetworkReply::sslErrors),
                     self, [self, reply](const QList<QSslError> &errors) {
        QStringList messages;
        for (const QSslError &err : errors) {
            messages << err.errorString();
        }
        self->m_osmLastSslError = messages.join(QStringLiteral("; "));
        self->m_onlineMapFailureReason = tr("OSM SSL warning: %1").arg(self->m_osmLastSslError.left(120));
#ifdef Q_OS_WIN
        /*
         * Map tiles are public raster images and Windows 7 installations often
         * have obsolete root stores.  Ignore certificate-chain problems for
         * tile downloads only so a Qt/OpenSSL build that is present but cannot
         * verify old roots can still render OSM.  If OpenSSL is missing
         * entirely, this signal is never enough and the HTTP fallback remains.
         */
        reply->ignoreSslErrors();
#endif
        self->logOsmDiagnostics(QStringLiteral("ssl-error"));
    });
    QObject::connect(reply, &QNetworkReply::finished, self, [self, reply, key, scheme]() {
        self->m_osmTilePending.remove(key);
        self->m_osmLastTileUrl = reply->url().toString(QUrl::RemoveUserInfo);
        const QVariant statusAttr = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        const QVariant reasonAttr = reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute);
        if (statusAttr.isValid()) {
            self->m_osmLastHttpStatus = QStringLiteral("HTTP %1 %2")
                                          .arg(statusAttr.toInt())
                                          .arg(reasonAttr.toString()).trimmed();
        } else {
            self->m_osmLastHttpStatus.clear();
        }

        bool providerShouldAdvance = false;
        const QVariant redirectAttr = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
        if (redirectAttr.isValid()) {
            const QUrl redirectUrl = reply->url().resolved(redirectAttr.toUrl());
            self->m_osmLastRedirectTarget = redirectUrl.toString(QUrl::RemoveUserInfo);
            ++self->m_osmTileFailureCount;
            self->m_onlineMapFailureReason = tr("OSM tile endpoint redirected to %1; trying another configured source")
                                                 .arg(redirectUrl.scheme().toUpper());
            providerShouldAdvance = true;
        } else if (reply->error() == QNetworkReply::NoError) {
            QPixmap pixmap;
            if (pixmap.loadFromData(reply->readAll())) {
                self->m_osmTileCache.insert(key, pixmap);
                self->m_osmTileFailureCount = 0;
                self->m_onlineMapFailureReason.clear();
                self->m_osmLastNetworkError.clear();
                self->m_osmLastRedirectTarget.clear();
                while (self->m_osmTileCache.size() > 640) {
                    self->m_osmTileCache.erase(self->m_osmTileCache.begin());
                }
            } else {
                ++self->m_osmTileFailureCount;
                self->m_osmLastNetworkError = tr("Tile image decode failed");
                self->m_onlineMapFailureReason = tr("OSM tile image decode failed; trying another source");
                providerShouldAdvance = true;
            }
        } else {
            ++self->m_osmTileFailureCount;
            self->m_osmLastNetworkError = reply->errorString();
            self->m_onlineMapFailureReason = reply->errorString();
            if (scheme == QStringLiteral("https") &&
                (reply->error() == QNetworkReply::SslHandshakeFailedError ||
                 reply->error() == QNetworkReply::ProtocolInvalidOperationError ||
                 reply->errorString().contains(QStringLiteral("SSL"), Qt::CaseInsensitive) ||
                 reply->errorString().contains(QStringLiteral("TLS"), Qt::CaseInsensitive))) {
                self->m_onlineMapFailureReason = tr("HTTPS/TLS tile request failed. Verify that the Windows build has Qt SSL/OpenSSL, or set MADMODEM_OSM_TILE_BASE to a local HTTP tile proxy/cache.");
                providerShouldAdvance = true;
            } else if (reply->errorString().contains(QStringLiteral("SSL"), Qt::CaseInsensitive) ||
                       reply->errorString().contains(QStringLiteral("TLS"), Qt::CaseInsensitive) ||
                       reply->errorString().contains(QStringLiteral("redirect"), Qt::CaseInsensitive)) {
                providerShouldAdvance = true;
            }
        }

        if (providerShouldAdvance) {
            const int count = configuredOsmTileProviderCount(self->m_osmUseHttpFallback);
            self->m_osmTileProviderIndex = (self->m_osmTileProviderIndex + 1) % count;
            if (count > 1) {
                self->m_osmTileFailureCount = qMin(self->m_osmTileFailureCount, 2);
            }
        }

        if (self->m_osmTileFailureCount >= 14 && self->m_osmTileCache.isEmpty()) {
            self->m_osmTileRequestBlocked = true;
            self->m_onlineMapFailed = true;
            if (self->m_onlineMapFailureReason.isEmpty()) {
                self->m_onlineMapFailureReason = tr("OSM tiles unavailable");
            }
        }
        if (self->m_osmTileFailureCount > 0 || !self->m_osmLastRedirectTarget.isEmpty() || !self->m_osmLastNetworkError.isEmpty()) {
            self->logOsmDiagnostics(QStringLiteral("tile-result"));
        }
        reply->deleteLater();
        self->update();
    });
}

void QsoMapWidget::drawOsmTiles(QPainter *painter, const QRect &mapRect) const
{
    painter->save();
    painter->setClipRect(mapRect);
    painter->fillRect(mapRect, QColor(219, 229, 236));

    if (m_exportRendering) {
        // v4.13a: export/print should use the same cached OpenStreetMap tiles
        // visible in the widget.  Do not start asynchronous network requests
        // during print/PDF rendering, but draw every tile that is already in the
        // cache; use the bundled map only behind missing tiles.
        if (!m_worldMap.isNull()) {
            const QRectF projected = projectedMapRect(mapRect);
            painter->drawPixmap(projected, m_worldMap, QRectF(m_worldMap.rect()));
        } else {
            drawLand(painter, mapRect);
            drawGrid(painter, mapRect);
        }

        const QVector<TileSpec> tiles = visibleOsmTiles(mapRect);
        int cachedTiles = 0;
        for (const TileSpec &tile : tiles) {
            const auto it = m_osmTileCache.constFind(tile.key);
            if (it != m_osmTileCache.constEnd()) {
                painter->drawPixmap(tile.screenRect, it.value(), QRectF(it.value().rect()));
                ++cachedTiles;
            }
        }

        painter->setClipping(false);
        const QRect attrib(mapRect.left() + 6, mapRect.bottom() - 21, 420, 18);
        painter->fillRect(attrib, QColor(245, 248, 250, 220));
        painter->setPen(QColor(55, 75, 88));
        painter->drawText(attrib.adjusted(5, 0, -5, 0),
                          Qt::AlignVCenter | Qt::AlignLeft,
                          cachedTiles > 0
                              ? L(QStringLiteral("OpenStreetMap cached tiles for export/print"))
                              : L(QStringLiteral("Offline fallback map — OSM tiles not yet cached")));
        painter->restore();
        return;
    }

    // Always paint a complete deterministic base map before drawing network
    // tiles.  Previously the widget painted grey placeholders for missing OSM
    // tile rows while cached rows were drawn normally; with the Maidenhead
    // overlay enabled this could look like a horizontal no-grid/no-map stripe
    // until the last asynchronous tile arrived.  A full base layer keeps the
    // QSO map visually continuous even when OSM loading is partial.
    if (!m_worldMap.isNull()) {
        if (m_osmTileRequestBlocked) {
            painter->drawPixmap(offlineMapRect(mapRect), m_worldMap, QRectF(m_worldMap.rect()));
        } else {
            const QRectF projected = projectedMapRect(mapRect);
            painter->drawPixmap(projected, m_worldMap, QRectF(m_worldMap.rect()));
        }
    } else {
        drawLand(painter, mapRect);
        drawGrid(painter, mapRect);
    }

    if (m_osmTileRequestBlocked && m_osmTileCache.isEmpty()) {
        painter->setClipping(false);
        const QRect attrib(mapRect.left() + 6, mapRect.bottom() - 21, 340, 18);
        painter->fillRect(attrib, QColor(245, 248, 250, 215));
        painter->setPen(QColor(55, 75, 88));
        painter->drawText(attrib.adjusted(5, 0, -5, 0),
                          Qt::AlignVCenter | Qt::AlignLeft,
                          QStringLiteral("Offline bundled map"));
        painter->restore();
        return;
    }

    const QVector<TileSpec> tiles = visibleOsmTiles(mapRect);
    bool drewAnyTile = false;
    for (const TileSpec &tile : tiles) {
        const auto it = m_osmTileCache.constFind(tile.key);
        if (it != m_osmTileCache.constEnd()) {
            painter->drawPixmap(tile.screenRect, it.value(), QRectF(it.value().rect()));
            drewAnyTile = true;
        } else {
            // Do not cover the complete fallback map with blank tile
            // placeholders.  Request the missing tile and draw only a very
            // subtle boundary so partial OSM loads cannot create horizontal
            // blank bands over the Maidenhead overlay.
            painter->setPen(QPen(QColor(205, 214, 222, 80), 0.6));
            painter->drawRect(tile.screenRect.adjusted(0, 0, -0.5, -0.5));
            requestOsmTile(tile);
        }
    }

    if (!drewAnyTile && !m_worldMap.isNull() && tiles.isEmpty()) {
        if (m_osmTileRequestBlocked) {
            painter->drawPixmap(offlineMapRect(mapRect), m_worldMap, QRectF(m_worldMap.rect()));
        } else {
            const QRectF projected = projectedMapRect(mapRect);
            painter->drawPixmap(projected, m_worldMap, QRectF(m_worldMap.rect()));
        }
    }

    painter->setClipping(false);
    const QRect attrib(mapRect.left() + 6, mapRect.bottom() - 21, 260, 18);
    painter->fillRect(attrib, QColor(245, 248, 250, 205));
    painter->setPen(QColor(55, 75, 88));
    painter->drawText(attrib.adjusted(5, 0, -5, 0),
                      Qt::AlignVCenter | Qt::AlignLeft,
                      QStringLiteral("OpenStreetMap raster tiles"));
    painter->restore();
}

void QsoMapWidget::drawMap(QPainter *painter, const QRect &targetRect) const
{
    painter->save();
    painter->fillRect(targetRect, QColor(236, 242, 247));

    const QRect titleLine(targetRect.left() + 12,
                          targetRect.top() + 6,
                          qMax(10, targetRect.width() - 24),
                          24);
    painter->setPen(QPen(QColor(25, 50, 70), 1));
    painter->drawText(titleLine,
                      Qt::AlignLeft | Qt::AlignVCenter,
                      mapTitleText());
    if (!m_onlineMapFailureReason.isEmpty()) {
        const QString reason = L(QStringLiteral("Offline fallback: %1")).arg(m_onlineMapFailureReason);
        const QRect reasonLine(targetRect.left() + 12,
                               targetRect.top() + 32,
                               qMax(10, targetRect.width() - 24),
                               42);
        QFont smallFont = painter->font();
        smallFont.setPointSize(qMax(7, smallFont.pointSize() - 1));
        painter->setFont(smallFont);
        painter->setPen(QPen(QColor(80, 95, 110), 1));
        const QString elided = painter->fontMetrics().elidedText(reason, Qt::ElideRight, reasonLine.width());
        painter->drawText(reasonLine,
                          Qt::AlignLeft | Qt::AlignTop,
                          elided);
    }

    const QRect mapRect = mapRectForTarget(targetRect);
    painter->setClipRect(mapRect);
    drawOsmTiles(painter, mapRect);
    drawMaidenheadGrid(painter, mapRect);
    drawGrid(painter, mapRect);
    drawOverlays(painter, targetRect);
    painter->setClipping(false);
    painter->setPen(QPen(QColor(50, 60, 70), 1));
    painter->drawRect(mapRect.adjusted(-1, -1, 1, 1));
    painter->restore();
}

void QsoMapWidget::drawLand(QPainter *painter, const QRect &mapRect) const
{
    painter->save();
    painter->setClipRect(mapRect);
    painter->setBrush(QColor(211, 219, 197));
    painter->setPen(QPen(QColor(112, 128, 104), 1));
    for (const QPolygonF &poly : worldLandPolygons()) {
        QPolygonF screenPoly;
        for (const QPointF &lonLat : poly) {
            screenPoly << lonLatToScreen(lonLat, mapRect);
        }
        painter->drawPolygon(screenPoly);
    }
    painter->restore();
}

void QsoMapWidget::drawGrid(QPainter *painter, const QRect &mapRect) const
{
    painter->save();
    painter->setClipRect(mapRect);
    painter->setPen(QPen(QColor(115, 145, 170, 150), 1, Qt::DashLine));
    for (int lon = -180; lon <= 180; lon += 30) {
        const QPointF a = lonLatToScreen(QPointF(lon, -90), mapRect);
        const QPointF b = lonLatToScreen(QPointF(lon, 90), mapRect);
        painter->drawLine(a, b);
    }
    for (int lat = -60; lat <= 60; lat += 30) {
        const QPointF a = lonLatToScreen(QPointF(-180, lat), mapRect);
        const QPointF b = lonLatToScreen(QPointF(180, lat), mapRect);
        painter->drawLine(a, b);
    }
    painter->restore();
}

void QsoMapWidget::drawMaidenheadGrid(QPainter *painter, const QRect &mapRect) const
{
    if (!m_showMaidenheadGrid) {
        return;
    }

    const QPointF nw = screenToLonLat(QPointF(mapRect.left(), mapRect.top()), mapRect);
    const QPointF se = screenToLonLat(QPointF(mapRect.right(), mapRect.bottom()), mapRect);
    const double minLon = qBound(-180.0, qMin(nw.x(), se.x()), 180.0);
    const double maxLon = qBound(-180.0, qMax(nw.x(), se.x()), 180.0);
    const double minLat = qBound(-90.0, qMin(nw.y(), se.y()), 90.0);
    const double maxLat = qBound(-90.0, qMax(nw.y(), se.y()), 90.0);

    const QMap<QString, int> worked = workedMaidenheadGridCounts();
    const double firstLon = qMax(-180.0, qFloor(minLon / 2.0) * 2.0);
    const double lastLon = qMin(178.0, qCeil(maxLon / 2.0) * 2.0);
    const double firstLat = qMax(-90.0, static_cast<double>(qFloor(minLat)));
    const double lastLat = qMin(89.0, static_cast<double>(qCeil(maxLat)));

    painter->save();
    painter->setClipRect(mapRect);
    painter->setRenderHint(QPainter::Antialiasing, false);

    for (double lon = firstLon; lon <= lastLon; lon += 2.0) {
        for (double lat = firstLat; lat <= lastLat; lat += 1.0) {
            const QString grid4 = maidenheadGrid4FromLonLat(lon + 1.0, lat + 0.5);
            if (grid4.isEmpty()) {
                continue;
            }

            const QPointF a = lonLatToScreen(QPointF(lon, lat + 1.0), mapRect);
            const QPointF b = lonLatToScreen(QPointF(lon + 2.0, lat), mapRect);
            QRectF cell(QPointF(qMin(a.x(), b.x()), qMin(a.y(), b.y())),
                        QPointF(qMax(a.x(), b.x()), qMax(a.y(), b.y())));
            cell = cell.normalized();
            if (!cell.intersects(QRectF(mapRect)) || cell.width() < 3.0 || cell.height() < 3.0) {
                continue;
            }

            const bool isWorked = worked.contains(grid4);
            const int count = worked.value(grid4, 0);
            const int alpha = isWorked ? qMin(92, 42 + count * 10) : 34;
            painter->fillRect(cell, isWorked ? QColor(0, 150, 70, alpha)
                                             : QColor(190, 30, 30, alpha));
            painter->setPen(QPen(isWorked ? QColor(0, 120, 55, 110)
                                           : QColor(160, 20, 20, 95),
                                 isWorked ? 1.2 : 0.8));
            painter->drawRect(cell);

            if (cell.width() >= 42.0 && cell.height() >= 20.0) {
                QFont f = painter->font();
                f.setBold(isWorked);
                f.setPointSize(qMax(6, f.pointSize() - 2));
                painter->setFont(f);
                painter->setPen(isWorked ? QColor(0, 70, 28, 210) : QColor(120, 0, 0, 185));
                painter->drawText(cell.adjusted(2, 1, -2, -1),
                                  Qt::AlignCenter,
                                  grid4);
            }
        }
    }

    painter->restore();
}

bool QsoMapWidget::dateMatchesDisplayScope(const LogbookEntry &entry) const
{
    if (!entry.utc.isValid()) {
        return false;
    }
    if (m_mapDateScope == QStringLiteral("all")) {
        return true;
    }
    const QDate d = entry.utc.toUTC().date();
    const QDate today = QDateTime::currentDateTimeUtc().date();
    if (m_mapDateScope == QStringLiteral("last30")) {
        return d >= today.addDays(-29) && d <= today;
    }
    if (m_mapDateScope == QStringLiteral("last7")) {
        return d >= today.addDays(-6) && d <= today;
    }
    return d == today;
}

QVector<LogbookEntry> QsoMapWidget::sourceRecordsForBehavior() const
{
    if (m_displayBehavior == DisplayBehavior::HeardToday) {
        return m_heardRecords;
    }
    return m_records;
}

QVector<LogbookEntry> QsoMapWidget::filteredMapRecords() const
{
    QVector<LogbookEntry> filtered;
    filtered.reserve(qMin(sourceRecordsForBehavior().size(), qMax(32, m_mapMaxMarkers)));

    QMap<QString, LogbookEntry> latestByGrid;
    QMap<QString, LogbookEntry> latestByDxcc;
    const QVector<LogbookEntry> source = sourceRecordsForBehavior();

    for (const LogbookEntry &entry : source) {
        if (m_displayBehavior == DisplayBehavior::HeardToday) {
            if (!entry.utc.isValid() || entry.utc.toUTC().date() != QDateTime::currentDateTimeUtc().date()) {
                continue;
            }
        } else if (!dateMatchesDisplayScope(entry)) {
            continue;
        }

        if (m_mapUseModeFilter && !modeMatches(entry.mode, m_modeFilter)) {
            continue;
        }
        if (!m_mapBandFilter.trimmed().isEmpty() && entry.band.trimmed().compare(m_mapBandFilter.trimmed(), Qt::CaseInsensitive) != 0) {
            continue;
        }
        const bool hasGrid = !maidenheadGrid4(entry.grid).isEmpty();

        if (m_displayBehavior == DisplayBehavior::WorkedDxcc) {
            const CtyCountryFile::LookupResult cty = CtyCountryFile::instance().lookupCallsign(entry.callsign);
            QString countryKey = entry.adifFields.value(QStringLiteral("DXCC")).trimmed();
            if (countryKey.isEmpty() && cty.valid) {
                countryKey = cty.entity.dxcc;
            }
            if (countryKey.isEmpty()) {
                countryKey = entry.country.trimmed().toUpper();
            }
            if (countryKey.isEmpty()) {
                countryKey = entry.callsign.section(QChar('/'), -1).left(3).toUpper();
            }
            if (countryKey.isEmpty()) {
                continue;
            }

            LogbookEntry enriched = entry;
            if (enriched.country.trimmed().isEmpty() && cty.valid) {
                enriched.country = cty.entity.name;
            }
            if (cty.valid) {
                const QString refined = CtyCountryFile::instance().refinedGridForCallsign(enriched.callsign, enriched.grid, 6);
                if (!refined.isEmpty() && (enriched.grid.trimmed().isEmpty() || enriched.grid.trimmed().size() < 6)) {
                    enriched.grid = refined;
                }
            }
            if (cty.valid) {
                enriched.adifFields.insert(QStringLiteral("DXCC"), cty.entity.dxcc);
                enriched.adifFields.insert(QStringLiteral("CTY_NAME"), cty.entity.name);
                enriched.adifFields.insert(QStringLiteral("CTY_PREFIX"), cty.entity.primaryPrefix);
                enriched.adifFields.insert(QStringLiteral("CTY_CONT"), cty.entity.continent);
                enriched.adifFields.insert(QStringLiteral("CTY_GRID"), cty.entity.referenceGrid);
                enriched.adifFields.insert(QStringLiteral("CTY_LAT"), QString::number(cty.entity.latitude, 'f', 4));
                enriched.adifFields.insert(QStringLiteral("CTY_LON"), QString::number(cty.entity.longitude, 'f', 4));
            }
            if (!hasGrid && (!cty.valid || cty.entity.referenceGrid.isEmpty())) {
                continue;
            }
            if (!latestByDxcc.contains(countryKey) || enriched.utc.toUTC() > latestByDxcc.value(countryKey).utc.toUTC()) {
                latestByDxcc.insert(countryKey, enriched);
            }
            continue;
        }

        if (!hasGrid) {
            continue;
        }

        if (m_mapLatestPerGrid) {
            const QString key = maidenheadGrid4(entry.grid);
            if (!latestByGrid.contains(key) || entry.utc.toUTC() > latestByGrid.value(key).utc.toUTC()) {
                latestByGrid.insert(key, entry);
            }
        } else {
            filtered.append(entry);
        }
    }

    if (m_displayBehavior == DisplayBehavior::WorkedDxcc) {
        filtered = latestByDxcc.values().toVector();
    } else if (m_mapLatestPerGrid) {
        filtered = latestByGrid.values().toVector();
    }

    std::sort(filtered.begin(), filtered.end(), [](const LogbookEntry &a, const LogbookEntry &b) {
        return a.utc.toUTC() > b.utc.toUTC();
    });
    if (m_mapMaxMarkers > 0 && filtered.size() > m_mapMaxMarkers) {
        filtered.resize(m_mapMaxMarkers);
    }
    return filtered;
}

QVector<QsoMapWidget::MapPoint> QsoMapWidget::visiblePoints(const QRect &targetRect) const
{
    const QRect mapRect = mapRectForTarget(targetRect);
    QPointF homeLonLat;
    const bool haveHome = maidenheadToLonLat(m_homeGrid, &homeLonLat);

    QVector<MapPoint> points;
    const QVector<LogbookEntry> entries = filteredMapRecords();
    points.reserve(entries.size());
    for (const LogbookEntry &entry : entries) {
        QPointF lonLat;
        QString mapGrid = CtyCountryFile::instance().refinedGridForCallsign(entry.callsign, entry.grid, 6);
        if (mapGrid.isEmpty()) {
            mapGrid = entry.grid.trimmed().toUpper();
        }
        const bool usedCtyCentroid = ctyCentroidForCoarseEntry(entry, &lonLat, &mapGrid);
        if (!usedCtyCentroid && !maidenheadToLonLat(mapGrid, &lonLat)) {
            bool haveCtyPoint = false;
            if (m_displayBehavior == DisplayBehavior::WorkedDxcc) {
                bool okLat = false;
                bool okLon = false;
                const double lat = entry.adifFields.value(QStringLiteral("CTY_LAT")).toDouble(&okLat);
                const double lon = entry.adifFields.value(QStringLiteral("CTY_LON")).toDouble(&okLon);
                if (okLat && okLon) {
                    lonLat = QPointF(lon, lat);
                    haveCtyPoint = true;
                }
            }
            if (!haveCtyPoint) {
                continue;
            }
        }
        MapPoint p;
        p.entry = entry;
        p.lonLat = lonLat;
        p.screen = lonLatToScreen(lonLat, mapRect).toPoint();
        if (haveHome) {
            p.distanceKm = distanceKm(homeLonLat, lonLat);
            p.bearingDeg = bearingDeg(homeLonLat, lonLat);
        }
        points.append(p);
    }
    return points;
}

void QsoMapWidget::drawOverlays(QPainter *painter, const QRect &targetRect) const
{
    const QRect mapRect = mapRectForTarget(targetRect);
    QPointF homeLonLat;
    const bool haveHome = maidenheadToLonLat(m_homeGrid, &homeLonLat);
    const QPointF homeScreen = haveHome ? lonLatToScreen(homeLonLat, mapRect) : QPointF();
    const QVector<MapPoint> points = visiblePoints(targetRect);

    painter->save();
    painter->setClipRect(mapRect);
    painter->setRenderHint(QPainter::Antialiasing, true);

    if (haveHome && m_showPaths) {
        painter->setPen(QPen(QColor(40, 95, 145, 130), 1.2));
        for (const MapPoint &p : points) {
            painter->drawLine(homeScreen, p.screen);
        }
    }

    if (haveHome) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(255, 255, 255, 210));
        painter->drawEllipse(homeScreen, 18, 18);

        QPainterPath house;
        house.moveTo(homeScreen.x() - 12, homeScreen.y() + 2);
        house.lineTo(homeScreen.x(), homeScreen.y() - 11);
        house.lineTo(homeScreen.x() + 12, homeScreen.y() + 2);
        house.lineTo(homeScreen.x() + 9, homeScreen.y() + 2);
        house.lineTo(homeScreen.x() + 9, homeScreen.y() + 13);
        house.lineTo(homeScreen.x() - 9, homeScreen.y() + 13);
        house.lineTo(homeScreen.x() - 9, homeScreen.y() + 2);
        house.closeSubpath();
        painter->setBrush(QColor(255, 215, 40));
        painter->setPen(QPen(QColor(90, 48, 0), 2.2));
        painter->drawPath(house);

        const QString homeLabel = QStringLiteral("HOME %1").arg(m_homeGrid.left(6));
        QFont labelFont = painter->font();
        labelFont.setBold(true);
        painter->setFont(labelFont);
        const QFontMetrics fm(labelFont);
        const int labelW = qMax(96, fm.horizontalAdvance(homeLabel) + 18);
        const int labelH = fm.height() + 8;
        double labelX = homeScreen.x() + 24.0;
        double labelY = homeScreen.y() - labelH - 6.0;
        if (labelX + labelW > mapRect.right() - 4) {
            labelX = homeScreen.x() - labelW - 24.0;
        }
        if (labelY < mapRect.top() + 4) {
            labelY = homeScreen.y() + 24.0;
        }
        QRectF labelRect(labelX, labelY, labelW, labelH);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(255, 255, 255, 225));
        painter->drawRoundedRect(labelRect, 5, 5);
        painter->setPen(QPen(QColor(60, 65, 70), 1));
        painter->drawRoundedRect(labelRect, 5, 5);
        painter->setPen(QColor(20, 35, 50));
        painter->drawText(labelRect.adjusted(8, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft, homeLabel);
    }

    painter->setFont(QFont(painter->font().family(), 8, QFont::Bold));
    for (const MapPoint &p : points) {
        QColor fill(230, 64, 64);
        QColor outline(80, 0, 0);
        QString label = p.entry.callsign.left(12);
        if (m_displayBehavior == DisplayBehavior::HeardToday) {
            fill = QColor(255, 185, 35);
            outline = QColor(120, 70, 0);
        } else if (m_displayBehavior == DisplayBehavior::WorkedDxcc) {
            fill = QColor(45, 170, 85);
            outline = QColor(0, 90, 35);
            label = p.entry.country.trimmed();
            if (label.isEmpty()) {
                label = p.entry.adifFields.value(QStringLiteral("CTY_NAME"));
            }
            if (label.isEmpty()) {
                label = p.entry.adifFields.value(QStringLiteral("DXCC"));
            }
            if (label.isEmpty()) {
                label = p.entry.callsign.left(12);
            }
            label = label.left(14);
        }
        painter->setBrush(fill);
        painter->setPen(QPen(outline, 1.2));
        painter->drawEllipse(p.screen, 5, 5);
        painter->setPen(QColor(20, 20, 20));
        painter->drawText(p.screen + QPoint(7, -5), label);
    }

    painter->setClipping(false);
    painter->setPen(QColor(30, 50, 65));
    painter->drawText(targetRect.adjusted(12, -24, -12, -6),
                      Qt::AlignRight | Qt::AlignBottom,
                      mapStatusText());
    painter->restore();
}

void QsoMapWidget::showMarkerTooltip(const QPoint &pos)
{
    const QVector<MapPoint> points = visiblePoints(rect());
    for (const MapPoint &p : points) {
        if (QLineF(pos, p.screen).length() <= 10.0) {
            QString tip;
            tip += QStringLiteral("<b>%1</b><br>").arg(htmlEsc(p.entry.callsign));
            tip += QStringLiteral("Mode: %1<br>").arg(htmlEsc(p.entry.mode));
            tip += QStringLiteral("Band: %1<br>").arg(htmlEsc(p.entry.band));
            tip += QStringLiteral("Grid: %1<br>").arg(htmlEsc(p.entry.grid));
            if (!p.entry.country.trimmed().isEmpty()) {
                tip += QStringLiteral("Country: %1<br>").arg(htmlEsc(p.entry.country.trimmed()));
            }
            if (!p.entry.adifFields.value(QStringLiteral("DXCC")).trimmed().isEmpty()) {
                tip += QStringLiteral("DXCC: %1<br>").arg(htmlEsc(p.entry.adifFields.value(QStringLiteral("DXCC")).trimmed()));
            }
            tip += QStringLiteral("UTC: %1<br>").arg(htmlEsc(p.entry.utc.toUTC().toString("yyyy-MM-dd HH:mm:ss")));
            if (p.distanceKm > 0.0) {
                tip += QStringLiteral("Distance: %1 km<br>Bearing: %2°")
                           .arg(QString::number(p.distanceKm, 'f', 0),
                                QString::number(p.bearingDeg, 'f', 0));
            }
            if (!p.entry.comment.isEmpty()) {
                tip += QStringLiteral("<br>%1").arg(htmlEsc(p.entry.comment));
            }
            QToolTip::showText(mapToGlobal(pos), tip, this);
            return;
        }
    }
}


void QsoMapWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    update();
}

void QsoMapWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    update();
}

void QsoMapWidget::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
}


bool QsoMapWidget::quickMapActive() const
{
    return false;
}


QImage QsoMapWidget::renderedImage(const QSize &size) const
{
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(245, 248, 250));
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    drawMap(&painter, image.rect());
    painter.end();
    return image;
}

QImage QsoMapWidget::renderedExportImage(const QSize &size) const
{
    const bool oldExportRendering = m_exportRendering;
    m_exportRendering = true;
    const QImage image = renderedImage(size);
    m_exportRendering = oldExportRendering;
    return image;
}

void QsoMapWidget::drawImageToPrinterPage(QPainter *painter,
                                          const QPrinter &printer,
                                          const QImage &image) const
{
    if (painter == nullptr || image.isNull()) {
        return;
    }

    const auto page = printer.pageRect(QPrinter::DevicePixel);
    const QRectF pageRect(page.x(), page.y(), page.width(), page.height());
    painter->fillRect(pageRect, Qt::white);

    const QSizeF imageSize(image.size());
    QSizeF targetSize = imageSize;
    targetSize.scale(pageRect.size(), Qt::KeepAspectRatio);
    const QRectF target(pageRect.center().x() - targetSize.width() / 2.0,
                        pageRect.center().y() - targetSize.height() / 2.0,
                        targetSize.width(),
                        targetSize.height());
    painter->drawImage(target, image);
}

QVariantList QsoMapWidget::qmlMarkerList() const
{
    QPointF homeLonLat;
    const bool haveHome = maidenheadToLonLat(m_homeGrid, &homeLonLat);

    QVariantList list;
    const QVector<LogbookEntry> entries = filteredMapRecords();
    for (const LogbookEntry &entry : entries) {
        QPointF lonLat;
        QString mapGrid = CtyCountryFile::instance().refinedGridForCallsign(entry.callsign, entry.grid, 6);
        if (mapGrid.isEmpty()) {
            mapGrid = entry.grid.trimmed().toUpper();
        }
        const bool usedCtyCentroid = ctyCentroidForCoarseEntry(entry, &lonLat, &mapGrid);
        if (!usedCtyCentroid && !maidenheadToLonLat(mapGrid, &lonLat)) {
            continue;
        }

        QVariantMap m;
        m.insert(QStringLiteral("callsign"), entry.callsign.left(12));
        m.insert(QStringLiteral("mode"), entry.mode);
        m.insert(QStringLiteral("band"), entry.band);
        m.insert(QStringLiteral("grid"), mapGrid.toUpper());
        m.insert(QStringLiteral("grid4"), maidenheadGrid4(mapGrid));
        m.insert(QStringLiteral("lat"), lonLat.y());
        m.insert(QStringLiteral("lon"), lonLat.x());
        m.insert(QStringLiteral("utc"), entry.utc.toUTC().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
        if (haveHome) {
            m.insert(QStringLiteral("distanceKm"), distanceKm(homeLonLat, lonLat));
            m.insert(QStringLiteral("bearingDeg"), bearingDeg(homeLonLat, lonLat));
        } else {
            m.insert(QStringLiteral("distanceKm"), -1.0);
            m.insert(QStringLiteral("bearingDeg"), -1.0);
        }
        m.insert(QStringLiteral("comment"), entry.comment);
        list.append(m);
    }
    return list;
}

QVariantMap QsoMapWidget::qmlHome() const
{
    QVariantMap home;
    QPointF lonLat;
    if (maidenheadToLonLat(m_homeGrid, &lonLat)) {
        home.insert(QStringLiteral("valid"), true);
        home.insert(QStringLiteral("grid"), m_homeGrid.left(6));
        home.insert(QStringLiteral("lat"), lonLat.y());
        home.insert(QStringLiteral("lon"), lonLat.x());
    } else {
        home.insert(QStringLiteral("valid"), false);
        home.insert(QStringLiteral("grid"), QString());
        home.insert(QStringLiteral("lat"), 0.0);
        home.insert(QStringLiteral("lon"), 0.0);
    }
    return home;
}

QVariantMap QsoMapWidget::qmlWorkedGridMap() const
{
    QVariantMap out;
    const QMap<QString, int> counts = workedMaidenheadGridCounts();
    for (auto it = counts.constBegin(); it != counts.constEnd(); ++it) {
        out.insert(it.key(), it.value());
    }
    return out;
}

QString QsoMapWidget::mapTitleText() const
{
    const QString scope = (m_displayBehavior == DisplayBehavior::HeardToday) ? L(QStringLiteral("current UTC day"))
                        : (m_mapDateScope == QStringLiteral("all")) ? L(QStringLiteral("all dates"))
                        : (m_mapDateScope == QStringLiteral("last30")) ? L(QStringLiteral("last 30 days"))
                        : (m_mapDateScope == QStringLiteral("last7")) ? L(QStringLiteral("last 7 days"))
                        : L(QStringLiteral("current UTC day"));
    const QString behavior = (m_displayBehavior == DisplayBehavior::HeardToday) ? L(QStringLiteral("heard stations"))
                           : (m_displayBehavior == DisplayBehavior::WorkedDxcc) ? L(QStringLiteral("worked DXCC countries"))
                           : L(QStringLiteral("logged QSOs"));
    return L(QStringLiteral("QSO map — %1 — %2 — %3"))
        .arg(behavior, (!m_mapUseModeFilter || m_modeFilter.isEmpty()) ? L(QStringLiteral("all modes")) : m_modeFilter, scope);
}

QString QsoMapWidget::mapStatusText() const
{
    const int count = qmlMarkerList().size();
    QPointF homeLonLat;
    const bool haveHome = maidenheadToLonLat(m_homeGrid, &homeLonLat);
    QString extras;
    if (m_mapLatestPerGrid) {
        extras += L(QStringLiteral(" | latest per grid"));
    }
    if (!m_mapBandFilter.trimmed().isEmpty()) {
        extras += L(QStringLiteral(" | band %1")).arg(m_mapBandFilter.trimmed());
    }
    const int sourceCount = (m_displayBehavior == DisplayBehavior::HeardToday) ? m_heardRecords.size() : m_records.size();
    QString sourceLabel = L(QStringLiteral("QSO records"));
    if (m_displayBehavior == DisplayBehavior::HeardToday) {
        sourceLabel = L(QStringLiteral("heard stations today"));
    } else if (m_displayBehavior == DisplayBehavior::WorkedDxcc) {
        sourceLabel = L(QStringLiteral("logbook records grouped by DXCC"));
    }
    return L(QStringLiteral("%1 plotted marker%2 from %3 %4%5%6  |  %7"))
        .arg(count)
        .arg(count == 1 ? QString() : QStringLiteral("s"))
        .arg(sourceCount)
        .arg(sourceLabel)
        .arg(extras)
        .arg(haveHome ? L(QStringLiteral("  |  Home: %1")).arg(m_homeGrid.left(6))
                      : L(QStringLiteral("  |  Home grid not set")))
        .arg(osmShortStatusText());
}


QString QsoMapWidget::osmDiagnosticsText() const
{
    QStringList lines;
    lines << tr("Qt SSL support: %1").arg(QSslSocket::supportsSsl() ? L(QStringLiteral("YES")) : L(QStringLiteral("NO")));
#if QT_VERSION >= QT_VERSION_CHECK(5, 4, 0)
    lines << tr("OpenSSL build: %1").arg(QSslSocket::sslLibraryBuildVersionString().isEmpty()
                                          ? L(QStringLiteral("unknown"))
                                          : QSslSocket::sslLibraryBuildVersionString());
    lines << tr("OpenSSL runtime: %1").arg(QSslSocket::sslLibraryVersionString().isEmpty()
                                            ? L(QStringLiteral("not loaded"))
                                            : QSslSocket::sslLibraryVersionString());
#endif
    lines << tr("HTTP fallback: %1").arg(m_osmUseHttpFallback ? L(QStringLiteral("ON")) : L(QStringLiteral("OFF")));
    lines << tr("Tile providers: %1").arg(configuredOsmTileBaseUrls(m_osmUseHttpFallback).join(QStringLiteral(" | ")));
    if (!m_osmLastTileUrl.isEmpty()) {
        lines << tr("Last tile: %1").arg(m_osmLastTileUrl);
    }
    if (!m_osmLastHttpStatus.isEmpty()) {
        lines << tr("Last HTTP status: %1").arg(m_osmLastHttpStatus);
    }
    if (!m_osmLastRedirectTarget.isEmpty()) {
        lines << tr("Last redirect: %1").arg(m_osmLastRedirectTarget);
    }
    if (!m_osmLastNetworkError.isEmpty()) {
        lines << tr("Last network error: %1").arg(m_osmLastNetworkError);
    }
    if (!m_osmLastSslError.isEmpty()) {
        lines << tr("Last SSL error: %1").arg(m_osmLastSslError);
    }
    if (!m_onlineMapFailureReason.isEmpty()) {
        lines << tr("Fallback reason: %1").arg(m_onlineMapFailureReason);
    }
    return lines.join(QStringLiteral("\n"));
}

QString QsoMapWidget::osmShortStatusText() const
{
    QStringList parts;
    parts << L(QStringLiteral("OSM %1")).arg(m_osmUseHttpFallback ? L(QStringLiteral("HTTP")) : L(QStringLiteral("HTTPS")));
    parts << L(QStringLiteral("SSL:%1")).arg(QSslSocket::supportsSsl() ? L(QStringLiteral("yes")) : L(QStringLiteral("no")));
    if (!m_osmLastHttpStatus.isEmpty()) {
        parts << m_osmLastHttpStatus;
    }
    if (!m_osmLastNetworkError.isEmpty()) {
        parts << m_osmLastNetworkError.left(48);
    }
    return parts.join(QStringLiteral(" | "));
}

void QsoMapWidget::logOsmDiagnostics(const QString &event) const
{
    if (m_osmDiagnosticsLogged && event == QStringLiteral("startup")) {
        return;
    }
    m_osmDiagnosticsLogged = true;
    qInfo().noquote() << QStringLiteral("[QSO map] OSM diagnostics%1\n%2")
                         .arg(event.isEmpty() ? QString() : QStringLiteral(" (") + event + QStringLiteral(")"),
                              osmDiagnosticsText());
}

void QsoMapWidget::updateQuickMapModel()
{
}


void QsoMapWidget::ensureQuickMapCreated()
{
}


void QsoMapWidget::releaseQuickMapForShutdown()
{
}


void QsoMapWidget::fallBackToStaticMap(const QString &reason)
{
    if (m_onlineMapFailed) {
        return;
    }
    m_onlineMapFailed = true;
    m_onlineMapFailureReason = reason.trimmed().isEmpty()
        ? L(QStringLiteral("Qt Location / OSM tiles unavailable"))
        : reason.trimmed();
#ifdef MADMODEM_WITH_QT_LOCATION
    if (m_quickContainer != nullptr) {
        m_quickContainer->hide();
    }
#endif
    update();
}

QMap<QString, int> QsoMapWidget::workedMaidenheadGridCounts() const
{
    QMap<QString, int> counts;
    const QVector<LogbookEntry> entries = filteredMapRecords();
    for (const LogbookEntry &entry : entries) {
        const QString grid4 = maidenheadGrid4(entry.grid);
        if (!grid4.isEmpty()) {
            counts[grid4] += 1;
        }
    }
    return counts;
}

QMap<QString, int> QsoMapWidget::workedDxccCounts() const
{
    QMap<QString, int> counts;
    for (const LogbookEntry &entry : m_records) {
        QString key = entry.adifFields.value(QStringLiteral("DXCC")).trimmed();
        if (key.isEmpty()) {
            const CtyCountryFile::LookupResult cty = CtyCountryFile::instance().lookupCallsign(entry.callsign);
            if (cty.valid) {
                key = cty.entity.dxcc;
            }
        }
        if (key.isEmpty()) {
            key = entry.country.trimmed();
        }
        if (!key.isEmpty()) {
            counts[key.toUpper()] += 1;
        }
    }
    return counts;
}

bool QsoMapWidget::maidenheadToLonLat(const QString &grid, QPointF *lonLat)
{
    QString g = grid.trimmed().toUpper();
    g.remove(QRegularExpression(QStringLiteral("[^A-R0-9A-X]")));
    if (g.size() < 4) {
        return false;
    }
    const QChar a = g.at(0);
    const QChar b = g.at(1);
    const QChar c = g.at(2);
    const QChar d = g.at(3);
    if (a < 'A' || a > 'R' || b < 'A' || b > 'R' || !c.isDigit() || !d.isDigit()) {
        return false;
    }

    double lon = -180.0 + (a.unicode() - 'A') * 20.0 + c.digitValue() * 2.0;
    double lat = -90.0 + (b.unicode() - 'A') * 10.0 + d.digitValue() * 1.0;
    double lonSize = 2.0;
    double latSize = 1.0;

    if (g.size() >= 6) {
        const QChar e = g.at(4);
        const QChar f = g.at(5);
        if (e >= 'A' && e <= 'X' && f >= 'A' && f <= 'X') {
            lon += (e.unicode() - 'A') * (5.0 / 60.0);
            lat += (f.unicode() - 'A') * (2.5 / 60.0);
            lonSize = 5.0 / 60.0;
            latSize = 2.5 / 60.0;
        }
    }

    lon += lonSize / 2.0;
    lat += latSize / 2.0;
    if (lonLat != nullptr) {
        *lonLat = QPointF(lon, lat);
    }
    return true;
}

QString QsoMapWidget::maidenheadGrid4(const QString &grid)
{
    QString g = grid.trimmed().toUpper();
    g.remove(QRegularExpression(QStringLiteral("[^A-R0-9A-X]")));
    if (g.size() < 4) {
        return QString();
    }
    const QChar a = g.at(0);
    const QChar b = g.at(1);
    const QChar c = g.at(2);
    const QChar d = g.at(3);
    if (a < 'A' || a > 'R' || b < 'A' || b > 'R' || !c.isDigit() || !d.isDigit()) {
        return QString();
    }
    return QStringLiteral("%1%2%3%4").arg(a).arg(b).arg(c).arg(d);
}

QString QsoMapWidget::maidenheadGrid4FromLonLat(double lon, double lat)
{
    if (lon < -180.0 || lon >= 180.0 || lat < -90.0 || lat >= 90.0) {
        return QString();
    }

    const double x = lon + 180.0;
    const double y = lat + 90.0;
    const int lonField = qBound(0, static_cast<int>(qFloor(x / 20.0)), 17);
    const int latField = qBound(0, static_cast<int>(qFloor(y / 10.0)), 17);
    const int lonSquare = qBound(0, static_cast<int>(qFloor((x - lonField * 20.0) / 2.0)), 9);
    const int latSquare = qBound(0, static_cast<int>(qFloor(y - latField * 10.0)), 9);

    QString out;
    out.reserve(4);
    out += QChar(static_cast<ushort>('A' + lonField));
    out += QChar(static_cast<ushort>('A' + latField));
    out += QChar(static_cast<ushort>('0' + lonSquare));
    out += QChar(static_cast<ushort>('0' + latSquare));
    return out;
}

double QsoMapWidget::distanceKm(const QPointF &aLonLat, const QPointF &bLonLat)
{
    const double lat1 = qDegreesToRadians(aLonLat.y());
    const double lat2 = qDegreesToRadians(bLonLat.y());
    const double dLat = qDegreesToRadians(bLonLat.y() - aLonLat.y());
    const double dLon = qDegreesToRadians(bLonLat.x() - aLonLat.x());
    const double h = qSin(dLat / 2.0) * qSin(dLat / 2.0) +
                     qCos(lat1) * qCos(lat2) * qSin(dLon / 2.0) * qSin(dLon / 2.0);
    return 2.0 * kEarthRadiusKm * qAtan2(qSqrt(h), qSqrt(qMax(0.0, 1.0 - h)));
}

double QsoMapWidget::bearingDeg(const QPointF &aLonLat, const QPointF &bLonLat)
{
    const double lat1 = qDegreesToRadians(aLonLat.y());
    const double lat2 = qDegreesToRadians(bLonLat.y());
    const double dLon = qDegreesToRadians(bLonLat.x() - aLonLat.x());
    const double y = qSin(dLon) * qCos(lat2);
    const double x = qCos(lat1) * qSin(lat2) - qSin(lat1) * qCos(lat2) * qCos(dLon);
    double brg = qRadiansToDegrees(qAtan2(y, x));
    while (brg < 0.0) {
        brg += 360.0;
    }
    while (brg >= 360.0) {
        brg -= 360.0;
    }
    return brg;
}

QString QsoMapWidget::normalizedMode(const QString &mode)
{
    QString m = mode.trimmed().toUpper();
    m.replace(QStringLiteral(" "), QString());
    if (m == QStringLiteral("PSK") || m == QStringLiteral("PSKTEXT") ||
        m == QStringLiteral("BPSK") || m.startsWith(QStringLiteral("BPSK")) ||
        m == QStringLiteral("QPSK") || m.startsWith(QStringLiteral("QPSK"))) {
        return QStringLiteral("PSK");
    }
    if (m == QStringLiteral("MFSK") || m == QStringLiteral("MFSKTEXT") ||
        m.startsWith(QStringLiteral("MFSK"))) {
        return QStringLiteral("MFSK");
    }
    if (m == QStringLiteral("HELLSCHREIBER") || m == QStringLiteral("FELDHELL")) {
        return QStringLiteral("HELL");
    }
    if (m == QStringLiteral("SSTVRX") || m.startsWith(QStringLiteral("SSTV"))) {
        return QStringLiteral("SSTV");
    }
    return m;
}

bool QsoMapWidget::modeMatches(const QString &entryMode, const QString &filterMode)
{
    const QString entry = normalizedMode(entryMode);
    const QString filter = normalizedMode(filterMode);
    if (filter.isEmpty()) {
        return entry == QStringLiteral("RTTY") || entry == QStringLiteral("PSK") ||
               entry == QStringLiteral("MFSK") || entry == QStringLiteral("CW") ||
               entry.startsWith(QStringLiteral("HELL")) || entry == QStringLiteral("SSTV") ||
               entry == QStringLiteral("FT8") || entry == QStringLiteral("FT4");
    }
    if (filter == QStringLiteral("PSK")) {
        return entry == QStringLiteral("PSK");
    }
    if (filter == QStringLiteral("MFSK")) {
        return entry == QStringLiteral("MFSK");
    }
    if (filter == QStringLiteral("HELL")) {
        return entry.startsWith(QStringLiteral("HELL"));
    }
    return entry == filter;
}
