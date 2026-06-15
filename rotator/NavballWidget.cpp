#include "NavballWidget.h"

#include <QFont>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QPaintEvent>
#include <QPen>
#include <QSizePolicy>
#include <QtMath>
#include <algorithm>
#include <cmath>

namespace mm {

NavballWidget::NavballWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
    setMinimumSize(190, 190);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAutoFillBackground(false);
    setUpdateBehavior(QOpenGLWidget::NoPartialUpdate);
}

void NavballWidget::set_talt(double talt)
{
    m_talt = clampElevation(talt);
    update();
}

void NavballWidget::set_taz(double taz)
{
    m_taz = taz;
    update();
}

void NavballWidget::set_alt(double alt)
{
    m_alt = clampElevation(alt);
    update();
}

void NavballWidget::set_az(double az)
{
    m_az = az;
    update();
}

void NavballWidget::setTargetVisible(bool visible)
{
    m_hasTarget = visible;
    update();
}

void NavballWidget::setMoonAzEl(double az, double el)
{
    m_moonAz = az;
    m_moonEl = clampElevation(el);
    update();
}

void NavballWidget::setMoonVisible(bool visible)
{
    m_hasMoon = visible;
    update();
}

void NavballWidget::setSunAzEl(double az, double el)
{
    m_sunAz = az;
    m_sunEl = clampElevation(el);
    update();
}

void NavballWidget::setSunVisible(bool visible)
{
    m_hasSun = visible;
    update();
}

void NavballWidget::set_x_size(int w)
{
    m_xSize = std::max(120, w);
    setMinimumWidth(std::min(m_xSize, 240));
    updateGeometry();
    update();
}

void NavballWidget::set_y_size(int h)
{
    m_ySize = std::max(120, h);
    setMinimumHeight(std::min(m_ySize, 240));
    updateGeometry();
    update();
}

void NavballWidget::refresh()
{
    update();
}

QSize NavballWidget::sizeHint() const
{
    return QSize(m_xSize, m_ySize);
}

QSize NavballWidget::minimumSizeHint() const
{
    return QSize(160, 160);
}

double NavballWidget::normalize360(double value)
{
    double az = std::fmod(value, 360.0);
    if (az < 0.0) az += 360.0;
    if (az >= 360.0) az -= 360.0;
    return az;
}

double NavballWidget::clampElevation(double value)
{
    return std::max(-90.0, std::min(180.0, value));
}

NavballWidget::Vector3D NavballWidget::spherePoint(double azDeg, double elDeg) const
{
    const double radAz = qDegreesToRadians(normalize360(azDeg));
    const double radEl = qDegreesToRadians(clampElevation(elDeg));
    return {
        qCos(radEl) * qSin(radAz),
        qSin(radEl),
        qCos(radEl) * qCos(radAz)
    };
}

bool NavballWidget::project3D(double pointAzDeg, double pointElDeg, QPointF &outScreenPoint, double *outDepth) const
{
    const double radius = qMin(width(), height()) * 0.40;
    const QPointF center(width() / 2.0, height() / 2.0);

    const Vector3D v = spherePoint(pointAzDeg, pointElDeg);
    const double curAz = qDegreesToRadians(normalize360(m_az));
    const double curEl = qDegreesToRadians(clampElevation(m_alt));

    const double x1 = v.x * qCos(curAz) - v.z * qSin(curAz);
    const double y1 = v.y;
    const double z1 = v.x * qSin(curAz) + v.z * qCos(curAz);

    const double x2 = x1;
    const double y2 = y1 * qCos(curEl) - z1 * qSin(curEl);
    const double z2 = y1 * qSin(curEl) + z1 * qCos(curEl);

    if (outDepth != nullptr) *outDepth = z2;
    if (z2 < 0.0) return false;

    outScreenPoint.setX(center.x() + x2 * radius);
    outScreenPoint.setY(center.y() - y2 * radius);
    return true;
}

QPointF NavballWidget::edgePointForBearing(double pointAzDeg, double pointElDeg, double radius, const QPointF &center) const
{
    const Vector3D v = spherePoint(pointAzDeg, pointElDeg);
    const double curAz = qDegreesToRadians(normalize360(m_az));
    const double curEl = qDegreesToRadians(clampElevation(m_alt));

    const double x1 = v.x * qCos(curAz) - v.z * qSin(curAz);
    const double y1 = v.y;
    const double z1 = v.x * qSin(curAz) + v.z * qCos(curAz);
    Q_UNUSED(z1)
    const double x2 = x1;
    const double y2 = y1 * qCos(curEl) - z1 * qSin(curEl);

    double len = std::sqrt(x2 * x2 + y2 * y2);
    if (len < 1e-6) len = 1.0;
    return QPointF(center.x() + (x2 / len) * radius * 0.92,
                   center.y() - (y2 / len) * radius * 0.92);
}

void NavballWidget::drawLabel(QPainter &painter, const QPointF &pos, const QString &text, const QColor &colour) const
{
    painter.save();
    QFont f(QStringLiteral("monospace"), 8, QFont::Bold);
    painter.setFont(f);
    const QFontMetrics fm(f);
    QRectF box(pos.x() - fm.horizontalAdvance(text) / 2.0 - 4.0,
               pos.y() - fm.height() / 2.0 - 2.0,
               fm.horizontalAdvance(text) + 8.0,
               fm.height() + 4.0);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 85));
    painter.drawRoundedRect(box, 3, 3);
    painter.setPen(colour);
    painter.drawText(box, Qt::AlignCenter, text);
    painter.restore();
}


void NavballWidget::drawStylizedSunFace(QPainter &painter, const QPointF &pos, qreal scale) const
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    const qreal bodyR = 12.0 * scale;
    const qreal rayOuter = 19.0 * scale;
    const int rayCount = 12;
    QPainterPath rays;
    for (int i = 0; i < rayCount; ++i) {
        const double a = qDegreesToRadians(i * (360.0 / rayCount));
        const double a1 = a - qDegreesToRadians(10.0);
        const double a2 = a + qDegreesToRadians(10.0);
        QPointF p1(pos.x() + std::cos(a1) * (bodyR * 0.92), pos.y() + std::sin(a1) * (bodyR * 0.92));
        QPointF p2(pos.x() + std::cos(a) * rayOuter, pos.y() + std::sin(a) * rayOuter);
        QPointF p3(pos.x() + std::cos(a2) * (bodyR * 0.92), pos.y() + std::sin(a2) * (bodyR * 0.92));
        QPainterPath ray;
        ray.moveTo(p1);
        ray.quadTo(p2, p3);
        ray.closeSubpath();
        rays.addPath(ray);
    }
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 189, 18));
    painter.drawPath(rays);

    QRadialGradient g(pos, bodyR * 1.1, pos - QPointF(4.0 * scale, 5.0 * scale));
    g.setColorAt(0.0, QColor(255, 244, 92));
    g.setColorAt(0.7, QColor(255, 217, 18));
    g.setColorAt(1.0, QColor(245, 170, 0));
    painter.setBrush(g);
    painter.setPen(QPen(QColor(255, 226, 40), 1.5 * scale));
    painter.drawEllipse(pos, bodyR, bodyR);

    // Sunglasses inspired by the reference image.
    const QRectF leftGlass(pos.x() - 10.0 * scale, pos.y() - 2.5 * scale, 9.0 * scale, 6.8 * scale);
    const QRectF rightGlass(pos.x() + 1.0 * scale, pos.y() - 2.5 * scale, 9.0 * scale, 6.8 * scale);
    painter.setBrush(QColor(18, 20, 24));
    painter.setPen(QPen(QColor(8, 8, 10), 1.2 * scale));
    painter.drawRoundedRect(leftGlass, 2.2 * scale, 2.2 * scale);
    painter.drawRoundedRect(rightGlass, 2.2 * scale, 2.2 * scale);
    painter.drawLine(QPointF(leftGlass.right(), leftGlass.center().y()), QPointF(rightGlass.left(), rightGlass.center().y()));
    painter.drawArc(QRectF(pos.x() - 8.0 * scale, pos.y() + 1.2 * scale, 16.0 * scale, 10.0 * scale), 200 * 16, 140 * 16);

    painter.restore();
}

void NavballWidget::drawStylizedMoonFace(QPainter &painter, const QPointF &pos, qreal scale) const
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    const qreal r = 13.0 * scale;
    QPainterPath moon;
    moon.addEllipse(pos, r, r);
    QPainterPath cut;
    cut.addEllipse(QPointF(pos.x() + 6.0 * scale, pos.y() - 0.4 * scale), r * 0.92, r * 0.92);
    moon = moon.subtracted(cut);
    painter.setPen(QPen(QColor(35, 35, 40), 1.2 * scale));
    painter.setBrush(QColor(232, 232, 236));
    painter.drawPath(moon);

    // Simple sleepy face inspired by the reference icon.
    painter.setPen(QPen(QColor(45, 45, 50), 1.2 * scale, Qt::SolidLine, Qt::RoundCap));
    painter.drawArc(QRectF(pos.x() - 5.8 * scale, pos.y() - 4.5 * scale, 5.0 * scale, 3.2 * scale), 25 * 16, 120 * 16);
    painter.drawArc(QRectF(pos.x() - 4.2 * scale, pos.y() - 0.8 * scale, 4.6 * scale, 2.8 * scale), 200 * 16, 120 * 16);
    QPainterPath nose;
    nose.moveTo(pos.x() - 0.6 * scale, pos.y() - 1.0 * scale);
    nose.quadTo(QPointF(pos.x() + 1.8 * scale, pos.y() + 0.6 * scale), QPointF(pos.x() - 0.6 * scale, pos.y() + 1.4 * scale));
    painter.drawPath(nose);
    painter.drawArc(QRectF(pos.x() - 2.0 * scale, pos.y() + 2.0 * scale, 5.5 * scale, 2.8 * scale), 190 * 16, 100 * 16);

    painter.restore();
}

void NavballWidget::drawSunMarker(QPainter &painter, const QPointF &pos, bool edgeCue) const
{
    drawStylizedSunFace(painter, pos, edgeCue ? 0.82 : 1.0);
}

void NavballWidget::drawMoonMarker(QPainter &painter, const QPointF &pos, bool edgeCue) const
{
    drawStylizedMoonFace(painter, pos, edgeCue ? 0.82 : 1.0);
}


void NavballWidget::drawInstrumentBezel(QPainter &painter) const
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF panel = rect().adjusted(4.0, 4.0, -4.0, -4.0);
    QLinearGradient panelGradient(panel.topLeft(), panel.bottomRight());
    panelGradient.setColorAt(0.0, QColor(55, 60, 68));
    panelGradient.setColorAt(0.45, QColor(23, 27, 34));
    panelGradient.setColorAt(1.0, QColor(8, 10, 14));

    painter.setPen(QPen(QColor(95, 105, 118), 2.0));
    painter.setBrush(panelGradient);
    painter.drawRoundedRect(panel, 18.0, 18.0);

    const QRectF inner = panel.adjusted(11.0, 11.0, -11.0, -11.0);
    painter.setPen(QPen(QColor(8, 9, 12), 3.0));
    painter.setBrush(QColor(6, 8, 12));
    painter.drawRoundedRect(inner, 12.0, 12.0);
    painter.setPen(QPen(QColor(65, 72, 82), 1.2));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(inner.adjusted(2.0, 2.0, -2.0, -2.0), 10.0, 10.0);

    const qreal screwR = qMax<qreal>(5.0, qMin(width(), height()) * 0.028);
    const qreal margin = 21.0;
    const QPointF screws[] = {
        QPointF(panel.left() + margin, panel.top() + margin),
        QPointF(panel.right() - margin, panel.top() + margin),
        QPointF(panel.left() + margin, panel.bottom() - margin),
        QPointF(panel.right() - margin, panel.bottom() - margin)
    };

    for (const QPointF &c : screws) {
        QRadialGradient sg(c - QPointF(screwR * 0.35, screwR * 0.35), screwR * 1.25);
        sg.setColorAt(0.0, QColor(230, 235, 240));
        sg.setColorAt(0.45, QColor(130, 138, 148));
        sg.setColorAt(1.0, QColor(38, 42, 48));
        painter.setPen(QPen(QColor(10, 12, 16), 1.2));
        painter.setBrush(sg);
        painter.drawEllipse(c, screwR, screwR);

        painter.save();
        painter.translate(c);
        painter.rotate((c.x() < width() / 2.0) ? -22.0 : 22.0);
        painter.setPen(QPen(QColor(25, 28, 34), 1.6, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(-screwR * 0.58, 0.0), QPointF(screwR * 0.58, 0.0));
        painter.restore();
    }

    painter.restore();
}

void NavballWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const int w = width();
    const int h = height();
    const double radius = qMin(w, h) * 0.40;
    const QPointF center(w / 2.0, h / 2.0);

    painter.fillRect(rect(), QColor(7, 9, 13));
    drawInstrumentBezel(painter);

    QPainterPath sphereClip;
    sphereClip.addEllipse(center, radius, radius);

    QRadialGradient outerGlow(center, radius * 1.35);
    outerGlow.setColorAt(0.0, QColor(35, 60, 95, 60));
    outerGlow.setColorAt(0.6, QColor(25, 40, 70, 30));
    outerGlow.setColorAt(1.0, QColor(10, 12, 18, 0));
    painter.setPen(Qt::NoPen);
    painter.setBrush(outerGlow);
    painter.drawEllipse(center, radius * 1.25, radius * 1.25);

    QLinearGradient globe(center.x(), center.y() - radius, center.x(), center.y() + radius);
    globe.setColorAt(0.0, QColor(44, 178, 221));
    globe.setColorAt(0.47, QColor(74, 192, 232));
    globe.setColorAt(0.50, QColor(230, 238, 245));
    globe.setColorAt(0.53, QColor(171, 111, 46));
    globe.setColorAt(1.0, QColor(120, 73, 30));

    painter.setPen(Qt::NoPen);
    painter.setBrush(globe);
    painter.drawEllipse(center, radius, radius);

    painter.save();
    painter.setClipPath(sphereClip);

    auto insetTowardCenter = [&](const QPointF &pt, double factor) -> QPointF {
        return QPointF(center.x() + (pt.x() - center.x()) * factor,
                       center.y() + (pt.y() - center.y()) * factor);
    };

    auto drawCurve = [&](int fixedDeg, bool isElevation, int sampleStep, const QColor &color, qreal width) {
        QPolygonF poly;
        const int loopStart = isElevation ? 0 : -90;
        const int loopEnd = isElevation ? 360 : 180;
        for (int moving = loopStart; moving <= loopEnd; moving += sampleStep) {
            QPointF pt;
            const bool ok = isElevation
                ? project3D(moving, fixedDeg, pt)
                : project3D(fixedDeg, moving, pt);
            if (ok) {
                poly << pt;
            } else if (poly.size() > 1) {
                painter.setPen(QPen(color, width));
                painter.drawPolyline(poly);
                poly.clear();
            }
        }
        if (poly.size() > 1) {
            painter.setPen(QPen(color, width));
            painter.drawPolyline(poly);
        }
    };

    // Dense white avionics-style grid.
    for (int el = -90; el <= 90; el += 10) {
        const bool major = (el % 30) == 0;
        const bool horizon = el == 0;
        QColor c = QColor(255, 255, 255, horizon ? 220 : (major ? 175 : 95));
        qreal widthLine = horizon ? 2.4 : (major ? 1.45 : 0.9);
        drawCurve(el, true, 4, c, widthLine);
    }
    // Extra elevation lines for 100..180 extended hemisphere support.
    for (int el = 100; el <= 180; el += 10) {
        const bool major = (el % 30) == 0;
        QColor c = QColor(255, 255, 255, major ? 105 : 65);
        qreal widthLine = major ? 1.2 : 0.8;
        drawCurve(el, true, 4, c, widthLine);
    }

    for (int az = 0; az < 360; az += 10) {
        const bool major = (az % 30) == 0;
        const bool cardinal = (az % 90) == 0;
        QColor c = QColor(255, 255, 255, cardinal ? 170 : (major ? 115 : 70));
        qreal widthLine = cardinal ? 1.6 : (major ? 1.0 : 0.7);
        drawCurve(az, false, 4, c, widthLine);
    }

    // Sparse elevation labels only: 0/30/60/90 on one side, then 120/150/180 on the
    // opposite side so the operator can immediately see when the elevation axis has
    // passed through zenith and is effectively rotated over itself.
    struct ElevLabel { int elevation; double azimuth; double factor; double yOffset; };
    const ElevLabel elevLabels[] = {
        {0,   315.0, 0.74,  8.0},
        {30,  315.0, 0.80,  0.0},
        {60,  315.0, 0.82, -1.0},
        {90,  315.0, 0.78, -2.0},
        {120,  45.0, 0.82, -1.0},
        {150,  45.0, 0.80,  0.0},
        {180,  45.0, 0.74,  8.0}
    };
    for (const ElevLabel &labelInfo : elevLabels) {
        QPointF pt;
        if (project3D(labelInfo.azimuth, static_cast<double>(labelInfo.elevation), pt)) {
            drawLabel(painter,
                      insetTowardCenter(pt, labelInfo.factor) + QPointF(0.0, labelInfo.yOffset),
                      QString::number(labelInfo.elevation) + QChar(0x00B0),
                      Qt::white);
        }
    }

    // Reduced azimuth label set on the inner equatorial arc.
    struct AzLabel { int az; const char *text; double yOffset; };
    const AzLabel azLabels[] = {
        {315, "315", 8.0},
        {0,   "0",   10.0},
        {45,  "45",  8.0}
    };
    for (const AzLabel &labelInfo : azLabels) {
        QPointF eqPt;
        if (project3D(static_cast<double>(labelInfo.az), 0.0, eqPt)) {
            const QPointF interior = insetTowardCenter(eqPt, labelInfo.az == 0 ? 0.58 : 0.72);
            drawLabel(painter, interior + QPointF(0.0, labelInfo.yOffset), QString::fromLatin1(labelInfo.text), Qt::white);
        }
    }

    // Keep only the most useful horizon/cardinal cue for East/West on the arc.
    struct CardinalLabel { int az; const char *text; QPointF delta; double factor; };
    const CardinalLabel cardLabels[] = {
        {90,  "E", QPointF(12.0, -2.0), 0.66},
        {270, "W", QPointF(-12.0, -2.0), 0.66}
    };
    for (const CardinalLabel &labelInfo : cardLabels) {
        QPointF pt;
        if (project3D(static_cast<double>(labelInfo.az), 0.0, pt)) {
            drawLabel(painter, insetTowardCenter(pt, labelInfo.factor) + labelInfo.delta, QString::fromLatin1(labelInfo.text), Qt::white);
        }
    }

    if (m_hasSun) {
        QPointF sunPt;
        if (project3D(m_sunAz, m_sunEl, sunPt)) {
            drawSunMarker(painter, sunPt, false);
            drawLabel(painter, sunPt + QPointF(0.0, -22.0), QStringLiteral("Sun"), QColor(255, 214, 66));
        } else {
            const QPointF edge = edgePointForBearing(m_sunAz, m_sunEl, radius, center);
            drawSunMarker(painter, edge, true);
            drawLabel(painter, edge + QPointF(0.0, -22.0), QStringLiteral("Sun"), QColor(255, 214, 66));
        }
    }

    if (m_hasMoon) {
        QPointF moonPt;
        if (project3D(m_moonAz, m_moonEl, moonPt)) {
            drawMoonMarker(painter, moonPt, false);
            drawLabel(painter, moonPt + QPointF(0.0, -22.0), QStringLiteral("Moon"), QColor(220, 225, 235));
        } else {
            const QPointF edge = edgePointForBearing(m_moonAz, m_moonEl, radius, center);
            drawMoonMarker(painter, edge, true);
            drawLabel(painter, edge + QPointF(0.0, -22.0), QStringLiteral("Moon"), QColor(220, 225, 235));
        }
    }

    // Target marker. If it is on the far hemisphere, draw an edge cue.
    if (m_hasTarget) {
        QPointF targetPt;
        double depth = 0.0;
        if (project3D(m_taz, m_talt, targetPt, &depth)) {
            painter.setPen(QPen(Qt::black, 1));
            painter.setBrush(QColor(46, 204, 113));
            painter.drawEllipse(targetPt, 6, 6);
            painter.setPen(QPen(QColor(46, 204, 113), 2));
            painter.drawLine(targetPt + QPointF(-9, 0), targetPt + QPointF(-15, 0));
            painter.drawLine(targetPt + QPointF(9, 0), targetPt + QPointF(15, 0));
            painter.drawLine(targetPt + QPointF(0, -9), targetPt + QPointF(0, -15));
            painter.drawLine(targetPt + QPointF(0, 9), targetPt + QPointF(0, 15));
            drawLabel(painter, targetPt + QPointF(0.0, -18.0), QStringLiteral("TG"), QColor(46, 204, 113));
        } else {
            const QPointF edge = edgePointForBearing(m_taz, m_talt, radius, center);
            painter.setPen(QPen(QColor(46, 204, 113), 2));
            painter.setBrush(QColor(46, 204, 113, 110));
            painter.drawEllipse(edge, 5, 5);
            drawLabel(painter, edge + QPointF(0.0, -18.0), QStringLiteral("TG"), QColor(46, 204, 113));
        }
    }

    painter.restore();

    // Outer tick marks/ring.
    painter.save();
    painter.setPen(QPen(QColor(220, 235, 255, 165), 1.6));
    for (int deg = 0; deg < 360; deg += 10) {
        const double r = qDegreesToRadians(static_cast<double>(deg));
        const double outer = radius * 1.03;
        const double inner = radius * ((deg % 30) == 0 ? 0.89 : 0.94);
        const QPointF p1(center.x() + std::sin(r) * inner,
                         center.y() - std::cos(r) * inner);
        const QPointF p2(center.x() + std::sin(r) * outer,
                         center.y() - std::cos(r) * outer);
        painter.drawLine(p1, p2);
    }
    painter.restore();

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(180, 205, 235), 3));
    painter.drawEllipse(center, radius, radius);

    // Fixed central reticle: the antenna current pointing is always here.
    painter.setPen(QPen(QColor(241, 196, 15), 3, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(center.x() - 30, center.y()), QPointF(center.x() - 8, center.y()));
    painter.drawLine(QPointF(center.x() - 8, center.y()), QPointF(center.x() - 8, center.y() + 7));
    painter.drawLine(QPointF(center.x() + 8, center.y()), QPointF(center.x() + 30, center.y()));
    painter.drawLine(QPointF(center.x() + 8, center.y()), QPointF(center.x() + 8, center.y() + 7));
    painter.setBrush(QColor(241, 196, 15));
    painter.drawEllipse(center, 3.0, 3.0);

    painter.setFont(QFont(QStringLiteral("monospace"), 8, QFont::Bold));
    painter.setPen(QColor(230, 230, 230));
    painter.drawText(QRectF(8, h - 30, w - 16, 22), Qt::AlignLeft | Qt::AlignVCenter,
                     QStringLiteral("AZ %1°  EL %2°")
                         .arg(QString::number(m_az, 'f', 0), QString::number(m_alt, 'f', 0)));

    if (m_hasTarget) {
        painter.drawText(QRectF(8, h - 48, w - 16, 18), Qt::AlignLeft | Qt::AlignVCenter,
                         QStringLiteral("TG %1° / %2°")
                             .arg(QString::number(m_taz, 'f', 0), QString::number(m_talt, 'f', 0)));
    }

    if (m_az > 360.0) {
        QRectF overlapRect(12, 12, 138, 24);
        painter.setPen(QPen(QColor(230, 126, 34), 2));
        painter.setBrush(QColor(230, 126, 34, 95));
        painter.drawRoundedRect(overlapRect, 4, 4);
        painter.setPen(Qt::white);
        painter.drawText(overlapRect, Qt::AlignCenter, QStringLiteral("OVERLAP ACTIVE"));
    }
}

} // namespace mm
