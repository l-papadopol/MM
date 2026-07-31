#ifndef RADIOTELESCOPEHEATMAPWIDGET_H
#define RADIOTELESCOPEHEATMAPWIDGET_H

#include <QWidget>
#include <QVector>
#include <QString>

class RadioTelescopeHeatmapWidget : public QWidget
{
    Q_OBJECT
public:
    struct TileSample
    {
        double azimuthDeg = 0.0;
        double elevationDeg = 0.0;
        double beamWidthDeg = 15.0;
        double noiseDb = -120.0;
        double skyX = 0.0;
        double skyY = 0.0;
        bool hasSkyPosition = false;
        bool valid = false;
    };

    explicit RadioTelescopeHeatmapWidget(QWidget *parent = nullptr);

    void setTiles(const QVector<TileSample> &tiles, bool useElevationAxis, bool interpolateGradient);
    void setCurrentTarget(double azimuthDeg, double elevationDeg, bool active);
    void setOverlayText(const QString &text);
    void setScanActive(bool active);
    void clear();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVector<TileSample> m_tiles;
    bool m_useElevationAxis = true;
    bool m_interpolateGradient = true;
    bool m_hasCurrentTarget = false;
    bool m_scanActive = false;
    double m_currentAzimuthDeg = 0.0;
    double m_currentElevationDeg = 0.0;
    QString m_overlayText;

    double tileDisplayNoiseDb(int index) const;
};

#endif // RADIOTELESCOPEHEATMAPWIDGET_H
