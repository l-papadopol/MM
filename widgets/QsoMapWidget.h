#ifndef QSOMAPWIDGET_H
#define QSOMAPWIDGET_H

#include "../logbook/AdifLogbook.h"

#include <QDate>
#include <QHash>
#include <QMap>
#include <QPointF>
#include <QPixmap>
#include <QVariantList>
#include <QVariantMap>
#include <QSet>
#include <QVector>
#include <functional>
#include <QWidget>

class QHideEvent;
class QNetworkAccessManager;
class QPainter;
class QShowEvent;
class QPrinter;

#ifdef MADMODEM_WITH_QT_LOCATION
class QQuickView;
#endif

/**
 * @brief Interactive current-day QSO world map for text and FT modes.
 *
 * The widget uses Qt Location / OSM as the default interactive map when those
 * Qt modules and online tiles are available.  It automatically falls back to
 * the bundled offline planet_map.jpg renderer, converts Maidenhead grid squares
 * to latitude/longitude, and overlays current-day ADIF QSOs filtered by mode.
 * It supports mouse drag, wheel zoom, marker tooltips, optional home-to-QSO
 * lines, and print/export helpers.
 */
class QsoMapWidget : public QWidget
{
    Q_OBJECT

public:
    explicit QsoMapWidget(QWidget *parent = nullptr);
    ~QsoMapWidget() override;

    void setRecords(const QVector<LogbookEntry> &records);
    void addHeardStation(const LogbookEntry &entry);
    void clearHeardStations();
    void setModeFilter(const QString &modeFilter);
    void setHomeGrid(const QString &grid);
    void setShowPaths(bool enabled);
    void setShowMaidenheadGrid(bool enabled);
    void setTextTranslator(std::function<QString(const QString &)> translator);

    QString modeFilter() const;
    bool showPaths() const;
    bool showMaidenheadGrid() const;

    // Public geometry helpers used by the integrated rotator and map modules.
    static bool maidenheadToLonLat(const QString &grid, QPointF *lonLat);
    static QString maidenheadGrid4(const QString &grid);
    static QString maidenheadGrid4FromLonLat(double lon, double lat);
    static double distanceKm(const QPointF &aLonLat, const QPointF &bLonLat);
    static double bearingDeg(const QPointF &aLonLat, const QPointF &bLonLat);

    enum class DisplayBehavior { LogbookQsos, HeardToday, WorkedDxcc };
    Q_ENUM(DisplayBehavior)
    void setDisplayBehavior(DisplayBehavior behavior);
    DisplayBehavior displayBehavior() const;
    QString displayBehaviorName() const;

public slots:
    void resetView();
    void saveMap();
    void printMap();
    bool configureLayerSettings();

private slots:
    void fallBackToStaticMap(const QString &reason);

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    struct MapPoint
    {
        LogbookEntry entry;
        QPointF lonLat;
        QPoint screen;
        double distanceKm = 0.0;
        double bearingDeg = 0.0;
    };

    struct TileSpec
    {
        QString key;
        int z = 0;
        int x = 0;
        int y = 0;
        QRectF screenRect;
    };

    QVector<LogbookEntry> filteredMapRecords() const;
    QVector<LogbookEntry> sourceRecordsForBehavior() const;
    QVector<MapPoint> visiblePoints(const QRect &targetRect) const;
    QRect mapRectForTarget(const QRect &targetRect) const;
    QRectF projectedMapRect(const QRect &mapRect) const;
    QRectF offlineMapRect(const QRect &mapRect) const;
    QPointF lonLatToScreen(const QPointF &lonLat, const QRect &mapRect) const;
    QPointF screenToLonLat(const QPointF &screen, const QRect &mapRect) const;
    void drawMap(QPainter *painter, const QRect &targetRect) const;
    void drawOsmTiles(QPainter *painter, const QRect &mapRect) const;
    QVector<TileSpec> visibleOsmTiles(const QRect &mapRect) const;
    void requestOsmTile(const TileSpec &tile) const;
    void drawLand(QPainter *painter, const QRect &mapRect) const;
    void drawGrid(QPainter *painter, const QRect &mapRect) const;
    void drawMaidenheadGrid(QPainter *painter, const QRect &mapRect) const;
    void drawOverlays(QPainter *painter, const QRect &targetRect) const;
    void showMarkerTooltip(const QPoint &pos);
    bool quickMapActive() const;
    QImage renderedImage(const QSize &size) const;
    QImage renderedExportImage(const QSize &size) const;
    void drawImageToPrinterPage(QPainter *painter, const QPrinter &printer, const QImage &image) const;
    QVariantList qmlMarkerList() const;
    QVariantMap qmlHome() const;
    QVariantMap qmlWorkedGridMap() const;
    QString mapTitleText() const;
    QString mapStatusText() const;
    QString L(const QString &source) const;
    void updateQuickMapModel();
    void ensureQuickMapCreated();
    void releaseQuickMapForShutdown();
    QString osmDiagnosticsText() const;
    QString osmShortStatusText() const;
    void logOsmDiagnostics(const QString &event = QString()) const;
    void loadDisplaySettings();
    void saveDisplaySettings() const;
    bool dateMatchesDisplayScope(const LogbookEntry &entry) const;

    QMap<QString, int> workedMaidenheadGridCounts() const;
    QMap<QString, int> workedDxccCounts() const;

    static QString normalizedMode(const QString &mode);
    static bool modeMatches(const QString &entryMode, const QString &filterMode);

    QVector<LogbookEntry> m_records;
    QVector<LogbookEntry> m_heardRecords;
    QString m_modeFilter;
    QString m_homeGrid;
    bool m_showPaths = false;
    bool m_showMaidenheadGrid = false;
    DisplayBehavior m_displayBehavior = DisplayBehavior::LogbookQsos;
    bool m_mapUseModeFilter = true;
    QString m_mapBandFilter;
    QString m_mapDateScope = QStringLiteral("today"); // today, last7, last30, all
    bool m_mapLatestPerGrid = true;
    int m_mapMaxMarkers = 1000;
    double m_zoom = 1.0;
    QPointF m_pan;
    bool m_dragging = false;
    QPoint m_lastMousePos;
    QPoint m_pressPos;
    QPixmap m_worldMap;
    QNetworkAccessManager *m_tileNetwork = nullptr;
    mutable QHash<QString, QPixmap> m_osmTileCache;
    mutable QSet<QString> m_osmTilePending;
    mutable bool m_osmTileRequestBlocked = false;
    mutable bool m_osmUseHttpFallback = false;
    mutable int m_osmTileFailureCount = 0;
    mutable int m_osmTileProviderIndex = 0;
    mutable QString m_osmLastTileUrl;
    mutable QString m_osmLastNetworkError;
    mutable QString m_osmLastRedirectTarget;
    mutable QString m_osmLastHttpStatus;
    mutable QString m_osmLastSslError;
    mutable bool m_osmDiagnosticsLogged = false;
    mutable bool m_exportRendering = false;
    bool m_onlineMapFailed = false;
    QString m_onlineMapFailureReason;
    std::function<QString(const QString &)> m_textTranslator;
#ifdef MADMODEM_WITH_QT_LOCATION
    QQuickView *m_quickView = nullptr;
    QWidget *m_quickContainer = nullptr;
    bool m_quickSourceSet = false;
    bool m_quickReleasedForShutdown = false;
#endif
};

#endif // QSOMAPWIDGET_H
