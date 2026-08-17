#ifndef NAVBALLWIDGET_H
#define NAVBALLWIDGET_H

#include <QColor>
#include <QPointF>
#include <QSize>
#include <QString>
#include <QOpenGLWidget>

class QPainter;

namespace mm {

class NavballWidget final : public QOpenGLWidget
{
    Q_OBJECT

public:
    explicit NavballWidget(QWidget *parent = nullptr);

    // Public antenna/target API.  Azimuth accepts the configured mechanical
    // range, including 450 degree overlap rotators.  Elevation accepts the
    // extended 0..180 degree satellite/az-el domain used by some controllers.
    void set_talt(double talt);
    void set_taz(double taz);
    void set_alt(double alt);
    void set_az(double az);
    void setTargetVisible(bool visible);
    void setMoonAzEl(double az, double el);
    void setMoonVisible(bool visible);
    void setSunAzEl(double az, double el);
    void setSunVisible(bool visible);

    void set_x_size(int w);
    void set_y_size(int h);
    void refresh();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    struct Vector3D
    {
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
    };

    static double normalize360(double value);
    static double clampElevation(double value);
    static double shortestAngularDelta(double targetDeg, double currentDeg);
    Vector3D spherePoint(double azDeg, double elDeg) const;
    bool project3D(double pointAzDeg, double pointElDeg, QPointF &outScreenPoint, double *outDepth = nullptr) const;
    QPointF edgePointForBearing(double pointAzDeg, double pointElDeg, double radius, const QPointF &center) const;
    void drawLabel(QPainter &painter, const QPointF &pos, const QString &text, const QColor &colour) const;
    void drawSunMarker(QPainter &painter, const QPointF &pos, bool edgeCue) const;
    void drawMoonMarker(QPainter &painter, const QPointF &pos, bool edgeCue) const;
    void drawStylizedSunFace(QPainter &painter, const QPointF &pos, qreal scale) const;
    void drawStylizedMoonFace(QPainter &painter, const QPointF &pos, qreal scale) const;
    void drawInstrumentBezel(QPainter &painter) const;

    double m_talt = 0.0;
    double m_taz = 0.0;
    double m_alt = 0.0;
    double m_az = 0.0;
    bool m_haveFilteredAz = false;
    bool m_haveFilteredAlt = false;
    bool m_overlapActive = false;
    bool m_hasTarget = false;
    double m_moonAz = 0.0;
    double m_moonEl = 0.0;
    bool m_hasMoon = false;
    double m_sunAz = 0.0;
    double m_sunEl = 0.0;
    bool m_hasSun = false;

    int m_xSize = 360;
    int m_ySize = 360;
};

} // namespace mm

#endif // NAVBALLWIDGET_H
