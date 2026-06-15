#include "WaterfallWidget.h"

#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QtGlobal>
#include <QtMath>

#include <cstring>

// -----------------------------------------------------------------------------
// Construction
// -----------------------------------------------------------------------------

WaterfallWidget::WaterfallWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
    setMinimumHeight(160);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setUpdateBehavior(QOpenGLWidget::PartialUpdate);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_NoSystemBackground, true);

    // Coalesce waterfall repaints to one GL-backed presentation per display
    // frame.  FFT/DSP lines may arrive faster than an old GPU/CPU can repaint;
    // drawing every line immediately causes UI lag without improving decode.
    m_repaintTimer.setSingleShot(true);
    connect(&m_repaintTimer, &QTimer::timeout, this, [this]() {
        m_repaintQueued = false;
        update();
    });

    buildColorTable();
    clear();
}

// -----------------------------------------------------------------------------
// Public slots
// -----------------------------------------------------------------------------

void WaterfallWidget::addLine(const QVector<quint8> &line, double minHz, double maxHz)
{
    if (line.isEmpty()) {
        return;
    }

    m_minHz = minHz;
    m_maxHz = maxHz;

    ensureImage();

    if (m_image.isNull()) {
        return;
    }

    const int imageWidth = m_image.width();
    const int imageHeight = m_image.height();

    if (imageWidth <= 0 || imageHeight <= 0) {
        return;
    }

    const int lineLast = line.size() - 1;

    auto sampleLine = [&line, lineLast](double ratio) -> int {
        if (lineLast <= 0) {
            return static_cast<int>(line.first());
        }
        const double sourcePosition = qBound(0.0, ratio, 1.0) * static_cast<double>(lineLast);
        const int i0 = qBound(0, static_cast<int>(sourcePosition), lineLast);
        const int i1 = qBound(0, i0 + 1, lineLast);
        const double frac = qBound(0.0, sourcePosition - static_cast<double>(i0), 1.0);
        const int interpolated = static_cast<int>(
            ((1.0 - frac) * static_cast<double>(line[i0])) +
            (frac * static_cast<double>(line[i1]))
            );
        return interpolated;
    };

    if (m_scrollDirection == ScrollDirection::Right) {
        if (imageWidth > 1) {
            for (int y = 0; y < imageHeight; ++y) {
                QRgb *row = reinterpret_cast<QRgb *>(m_image.scanLine(y));
                std::memmove(row, row + 1, static_cast<size_t>(imageWidth - 1) * sizeof(QRgb));
            }
        }

        for (int y = 0; y < imageHeight; ++y) {
            const double ratio = 1.0 - (static_cast<double>(y) / qMax(1, imageHeight - 1));
            const int value = static_cast<int>(sampleLine(ratio));
            QRgb *row = reinterpret_cast<QRgb *>(m_image.scanLine(y));
            row[imageWidth - 1] = colorForIntensity(static_cast<quint8>(qBound(0, value, 255)));
        }

        requestRepaint();
        return;
    }

    const int bytesPerLine = m_image.bytesPerLine();

    if (imageHeight > 1) {
        std::memmove(
            m_image.bits(),
            m_image.bits() + bytesPerLine,
            static_cast<size_t>(bytesPerLine) * static_cast<size_t>(imageHeight - 1)
            );
    }

    QRgb *dst = reinterpret_cast<QRgb *>(m_image.scanLine(imageHeight - 1));

    for (int x = 0; x < imageWidth; ++x) {
        const double ratio = static_cast<double>(x) / qMax(1, imageWidth - 1);
        const int value = static_cast<int>(sampleLine(ratio));
        dst[x] = colorForIntensity(static_cast<quint8>(qBound(0, value, 255)));
    }

    requestRepaint();
}

void WaterfallWidget::clear()
{
    ensureImage();

    if (!m_image.isNull()) {
        m_image.fill(QColor(4, 6, 8));
    }

    requestRepaint();
}

void WaterfallWidget::setMarkers(const QVector<FrequencyMarker> &markers)
{
    m_markers = markers;
    requestRepaint();
}

void WaterfallWidget::setTextOverlays(const QVector<WaterfallTextOverlay> &overlays)
{
    m_textOverlays = overlays;
    requestRepaint();
}

void WaterfallWidget::setColorScalePercent(int percent)
{
    const int clamped = qBound(0, percent, 100);
    if (m_colorScalePercent == clamped) {
        return;
    }

    m_colorScalePercent = clamped;
    requestRepaint();
}


void WaterfallWidget::setPaletteName(const QString &name)
{
    QString normalized = name.trimmed().toLower();
    if (normalized == QStringLiteral("default") ||
        normalized == QStringLiteral("wsjt-x") ||
        normalized == QStringLiteral("wsjtx")) {
        normalized = QStringLiteral("wsjtx");
    }

    if (normalized != QStringLiteral("wsjtx") &&
        normalized != QStringLiteral("mshv") &&
        normalized != QStringLiteral("fldigi") &&
        normalized != QStringLiteral("raptor") &&
        normalized != QStringLiteral("grayscale") &&
        normalized != QStringLiteral("madmodem")) {
        normalized = QStringLiteral("madmodem");
    }

    if (m_paletteName == normalized) {
        return;
    }

    m_paletteName = normalized;
    buildColorTable();
    requestRepaint();
}


void WaterfallWidget::setScrollDirection(ScrollDirection direction)
{
    if (m_scrollDirection == direction) {
        return;
    }

    m_scrollDirection = direction;
    clear();
}

// -----------------------------------------------------------------------------
// QOpenGLWidget events
// -----------------------------------------------------------------------------

void WaterfallWidget::initializeGL()
{
    // QPainter clears the GL-backed surface in paintGL().
}

void WaterfallWidget::paintGL()
{
    ensureImage();

    QPainter painter(this);
    painter.fillRect(rect(), QColor(4, 6, 8));

    if (!m_image.isNull()) {
        painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
        painter.drawImage(rect(), m_image);
    }

    painter.setRenderHint(QPainter::Antialiasing, false);

    drawFrequencyScale(painter);
    drawMarkers(painter);
    drawTextOverlays(painter);

    painter.setPen(QColor(130, 150, 160));
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
}


void WaterfallWidget::mousePressEvent(QMouseEvent *event)
{
    if (event == nullptr) {
        return;
    }

    const Qt::MouseButton button = event->button();
    if (button != Qt::LeftButton && button != Qt::RightButton) {
        QOpenGLWidget::mousePressEvent(event);
        return;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const int clickX = static_cast<int>(event->position().x());
#else
    const int clickX = event->pos().x();
#endif

    double frequencyHz = 0.0;
    if (m_scrollDirection == ScrollDirection::Right) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        const int clickY = static_cast<int>(event->position().y());
#else
        const int clickY = event->pos().y();
#endif
        frequencyHz = yToFrequency(clickY);
    } else {
        frequencyHz = xToFrequency(clickX);
    }

    emit frequencyClicked(frequencyHz, button);
    event->accept();
}

void WaterfallWidget::resizeGL(int width, int height)
{
    if (width <= 0 || height <= 0) {
        m_image = QImage();
        return;
    }

    m_image = QImage(width, height, QImage::Format_RGB32);
    m_image.fill(QColor(4, 6, 8));
}

// -----------------------------------------------------------------------------
// Drawing helpers
// -----------------------------------------------------------------------------

void WaterfallWidget::drawFrequencyScale(QPainter &painter)
{
    if (m_scrollDirection == ScrollDirection::Right) {
        const QColor gridColor(35, 50, 55, 145);
        const QColor bandBackground(0, 0, 0, 95);
        const QColor labelBackground(235, 235, 235, 225);
        const QColor labelBorder(80, 80, 80, 210);
        const QColor labelText(15, 15, 15);
        const int divisions = 7;
        const int labelPaddingX = 4;
        const int labelPaddingY = 1;
        QFont labelFont = painter.font();
        labelFont.setBold(true);
        labelFont.setPointSize(7);
        painter.setFont(labelFont);
        const QFontMetrics fm(labelFont);
        const int sideWidth = 58;
        const int sideLeft = qMax(0, width() - sideWidth);
        painter.fillRect(QRect(sideLeft, 0, sideWidth, height()), bandBackground);
        for (int i = 0; i <= divisions; ++i) {
            const double ratio = static_cast<double>(i) / static_cast<double>(divisions);
            const double freq = m_minHz + ratio * (m_maxHz - m_minHz);
            const int y = frequencyToY(freq);
            painter.setPen(gridColor);
            painter.drawLine(0, y, width(), y);
            const QString label = QString::number(static_cast<int>(freq)) + " Hz";
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
            const int textWidth = fm.horizontalAdvance(label);
#else
            const int textWidth = fm.width(label);
#endif
            const int labelHeight = fm.height() + (labelPaddingY * 2);
            QRect labelRect(qMax(2, width() - textWidth - (labelPaddingX * 2) - 3),
                            qBound(2, y - labelHeight / 2, qMax(2, height() - labelHeight - 2)),
                            textWidth + (labelPaddingX * 2),
                            labelHeight);
            painter.fillRect(labelRect, labelBackground);
            painter.setPen(labelBorder);
            painter.drawRect(labelRect.adjusted(0, 0, -1, -1));
            painter.setPen(labelText);
            painter.drawText(labelRect.adjusted(labelPaddingX, labelPaddingY, -labelPaddingX, -labelPaddingY),
                             Qt::AlignVCenter | Qt::AlignLeft,
                             label);
        }
        return;
    }
    const QColor gridColor(35, 50, 55, 145);
    const QColor bandBackground(0, 0, 0, 95);
    const QColor labelBackground(235, 235, 235, 225);
    const QColor labelBorder(80, 80, 80, 210);
    const QColor labelText(15, 15, 15);

    const int divisions = 9;
    const int labelPaddingX = 4;
    const int labelPaddingY = 1;

    QFont labelFont = painter.font();
    labelFont.setBold(true);
    labelFont.setPointSize(7);
    painter.setFont(labelFont);

    const QFontMetrics fm(labelFont);
    const int labelHeight = fm.height() + (labelPaddingY * 2);
    const int bandHeight = 24;
    const int bandTop = qMax(0, height() - bandHeight);

    painter.fillRect(QRect(0, bandTop, width(), bandHeight), bandBackground);

    for (int i = 0; i <= divisions; ++i) {
        const double ratio = static_cast<double>(i) / static_cast<double>(divisions);
        const int x = static_cast<int>(ratio * static_cast<double>(width() - 1));

        painter.setPen(gridColor);
        painter.drawLine(x, 0, x, height());

        const double freq = m_minHz + ratio * (m_maxHz - m_minHz);
        const QString label = QString::number(static_cast<int>(freq)) + " Hz";

#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
        const int textWidth = fm.horizontalAdvance(label);
#else
        const int textWidth = fm.width(label);
#endif

        int labelLeft = x + 3;

        if (labelLeft + textWidth + (labelPaddingX * 2) > width()) {
            labelLeft = width() - textWidth - (labelPaddingX * 2) - 3;
        }

        if (labelLeft < 2) {
            labelLeft = 2;
        }

        const QRect labelRect(
            labelLeft,
            height() - labelHeight - 3,
            textWidth + (labelPaddingX * 2),
            labelHeight
            );

        painter.fillRect(labelRect, labelBackground);
        painter.setPen(labelBorder);
        painter.drawRect(labelRect.adjusted(0, 0, -1, -1));

        painter.setPen(labelText);
        painter.drawText(
            labelRect.adjusted(labelPaddingX, labelPaddingY, -labelPaddingX, -labelPaddingY),
            Qt::AlignVCenter | Qt::AlignLeft,
            label
            );
    }
}

void WaterfallWidget::drawMarkers(QPainter &painter)
{
    if (m_markers.isEmpty()) {
        return;
    }

    if (m_scrollDirection == ScrollDirection::Right) {
        QFont markerFont = painter.font();
        markerFont.setBold(true);
        markerFont.setPointSize(7);
        painter.setFont(markerFont);
        const QFontMetrics fm(markerFont);
        const int labelPaddingX = 4;
        const int labelPaddingY = 1;
        const int labelHeight = fm.height() + (labelPaddingY * 2);
        int visibleLabelIndex = 0;
        for (const FrequencyMarker &marker : m_markers) {
            if (marker.frequencyHz < m_minHz || marker.frequencyHz > m_maxHz) {
                continue;
            }
            const int y = frequencyToY(marker.frequencyHz);
            QPen markerPen(marker.color);
            markerPen.setWidth(2);
            painter.setPen(markerPen);
            painter.drawLine(0, y, width(), y);
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
            const int textWidth = fm.horizontalAdvance(marker.label);
#else
            const int textWidth = fm.width(marker.label);
#endif
            const int labelLeft = 4 + ((visibleLabelIndex % 2) * (textWidth + 14));
            QRect labelRect(labelLeft,
                            qBound(2, y - labelHeight - 2, qMax(2, height() - labelHeight - 2)),
                            textWidth + (labelPaddingX * 2),
                            labelHeight);
            painter.fillRect(labelRect, QColor(245, 245, 245, 235));
            painter.setPen(QColor(130, 0, 0));
            painter.drawRect(labelRect.adjusted(0, 0, -1, -1));
            painter.setPen(marker.color);
            painter.drawText(labelRect.adjusted(labelPaddingX, labelPaddingY, -labelPaddingX, -labelPaddingY),
                             Qt::AlignVCenter | Qt::AlignLeft,
                             marker.label);
            ++visibleLabelIndex;
        }
        return;
    }

    QFont markerFont = painter.font();
    markerFont.setBold(true);
    markerFont.setPointSize(7);
    painter.setFont(markerFont);

    const QFontMetrics fm(markerFont);

    const int labelPaddingX = 4;
    const int labelPaddingY = 1;
    const int labelHeight = fm.height() + (labelPaddingY * 2);

    int visibleLabelIndex = 0;

    for (const FrequencyMarker &marker : m_markers) {
        if (marker.frequencyHz < m_minHz || marker.frequencyHz > m_maxHz) {
            continue;
        }

        const int x = frequencyToX(marker.frequencyHz);

        QPen markerPen(marker.color);
        markerPen.setWidth(2);
        painter.setPen(markerPen);
        painter.drawLine(x, 0, x, height());

#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
        const int textWidth = fm.horizontalAdvance(marker.label);
#else
        const int textWidth = fm.width(marker.label);
#endif

        int labelLeft = x + 4;

        if (labelLeft + textWidth + (labelPaddingX * 2) > width()) {
            labelLeft = x - textWidth - (labelPaddingX * 2) - 4;
        }

        if (labelLeft < 2) {
            labelLeft = 2;
        }

        const int labelBottomOffset = 24 + ((visibleLabelIndex % 2) * (labelHeight + 2));
        const int labelTop = qMax(2, height() - labelBottomOffset - labelHeight);

        const QRect labelRect(
            labelLeft,
            labelTop,
            textWidth + (labelPaddingX * 2),
            labelHeight
            );

        painter.fillRect(labelRect, QColor(245, 245, 245, 235));
        painter.setPen(QColor(130, 0, 0));
        painter.drawRect(labelRect.adjusted(0, 0, -1, -1));

        painter.setPen(marker.color);
        painter.drawText(
            labelRect.adjusted(labelPaddingX, labelPaddingY, -labelPaddingX, -labelPaddingY),
            Qt::AlignVCenter | Qt::AlignLeft,
            marker.label
            );

        ++visibleLabelIndex;
    }
}

void WaterfallWidget::drawTextOverlays(QPainter &painter)
{
    if (m_textOverlays.isEmpty()) {
        return;
    }

    QFont overlayFont = painter.font();
    overlayFont.setBold(true);
    overlayFont.setPointSize(9);
    painter.setFont(overlayFont);

    const QFontMetrics fm(overlayFont);
    const int paddingX = 6;
    const int paddingY = 2;
    const int labelHeight = fm.height() + paddingY * 2;
    const int usableBottom = qMax(0, height() - 30);
    QVector<QRect> occupied;

    for (const WaterfallTextOverlay &overlay : m_textOverlays) {
        if (overlay.label.trimmed().isEmpty() ||
            overlay.frequencyHz < m_minHz || overlay.frequencyHz > m_maxHz) {
            continue;
        }

        const int x = frequencyToX(overlay.frequencyHz);

#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
        const int textWidth = fm.horizontalAdvance(overlay.label);
#else
        const int textWidth = fm.width(overlay.label);
#endif

        const int labelWidth = textWidth + paddingX * 2;
        int left = x - labelWidth / 2;
        left = qBound(2, left, qMax(2, width() - labelWidth - 2));

        // Decodes are known only after the 15-second FT8 slot closes.  Draw
        // the callout near the recent trace area, then step upward if several
        // labels would overlap.  This keeps CQ and direct-reply labels readable
        // without disturbing the permanent RX/TX markers.
        int top = qMax(2, usableBottom - labelHeight - 8);
        QRect rect(left, top, labelWidth, labelHeight);
        int guard = 0;
        while (guard < 12) {
            bool overlaps = false;
            for (const QRect &used : occupied) {
                if (rect.adjusted(-3, -2, 3, 2).intersects(used)) {
                    overlaps = true;
                    break;
                }
            }
            if (!overlaps) {
                break;
            }
            rect.moveTop(qMax(2, rect.top() - labelHeight - 4));
            ++guard;
        }
        occupied.append(rect);

        painter.fillRect(rect, overlay.backgroundColor);
        painter.setPen(QColor(255, 255, 255, 120));
        painter.drawRect(rect.adjusted(0, 0, -1, -1));

        painter.setPen(overlay.textColor);
        painter.drawText(rect.adjusted(paddingX, paddingY, -paddingX, -paddingY),
                         Qt::AlignVCenter | Qt::AlignLeft,
                         overlay.label);

        QPen guidePen(overlay.textColor);
        guidePen.setWidth(1);
        guidePen.setStyle(Qt::DashLine);
        painter.setPen(guidePen);
        painter.drawLine(x, rect.bottom(), x, usableBottom);
    }
}

int WaterfallWidget::frequencyToX(double frequencyHz) const
{
    if (m_maxHz <= m_minHz) {
        return 0;
    }

    const double ratio = (frequencyHz - m_minHz) / (m_maxHz - m_minHz);

    return qBound(
        0,
        static_cast<int>(ratio * static_cast<double>(width() - 1)),
        width() - 1
        );
}


int WaterfallWidget::frequencyToY(double frequencyHz) const
{
    if (m_maxHz <= m_minHz) {
        return height() - 1;
    }

    const double ratio = (frequencyHz - m_minHz) / (m_maxHz - m_minHz);
    return qBound(
        0,
        height() - 1 - static_cast<int>(ratio * static_cast<double>(height() - 1)),
        qMax(0, height() - 1)
        );
}

double WaterfallWidget::yToFrequency(int y) const
{
    if (height() <= 1 || m_maxHz <= m_minHz) {
        return m_minHz;
    }

    const double ratio = 1.0 - (static_cast<double>(qBound(0, y, height() - 1)) /
                                static_cast<double>(height() - 1));

    return m_minHz + (ratio * (m_maxHz - m_minHz));
}

double WaterfallWidget::xToFrequency(int x) const
{
    if (width() <= 1 || m_maxHz <= m_minHz) {
        return m_minHz;
    }

    const double ratio = static_cast<double>(qBound(0, x, width() - 1)) /
                         static_cast<double>(width() - 1);

    return m_minHz + (ratio * (m_maxHz - m_minHz));
}

void WaterfallWidget::requestRepaint()
{
    if (m_repaintQueued) {
        return;
    }

    m_repaintQueued = true;
    m_repaintTimer.start(16); // about 60 Hz max, GPU-composited by QOpenGLWidget
}

// -----------------------------------------------------------------------------
// Color mapping
// -----------------------------------------------------------------------------

void WaterfallWidget::buildColorTable()
{
    m_colorTable.resize(256);

    auto interpolateStops = [](const QVector<QColor> &stops, int v) -> QColor {
        if (stops.isEmpty()) {
            return QColor(v, v, v);
        }
        if (stops.size() == 1) {
            return stops.first();
        }

        const double pos = (static_cast<double>(qBound(0, v, 255)) / 255.0) *
                           static_cast<double>(stops.size() - 1);
        const int i0 = qBound(0, static_cast<int>(qFloor(pos)), stops.size() - 1);
        const int i1 = qBound(0, i0 + 1, stops.size() - 1);
        const double f = qBound(0.0, pos - static_cast<double>(i0), 1.0);

        const QColor a = stops[i0];
        const QColor b = stops[i1];
        return QColor(qRound(a.red()   + (b.red()   - a.red())   * f),
                      qRound(a.green() + (b.green() - a.green()) * f),
                      qRound(a.blue()  + (b.blue()  - a.blue())  * f));
    };

    QVector<QColor> stops;
    if (m_paletteName == QStringLiteral("grayscale")) {
        stops = {QColor(0, 0, 0), QColor(255, 255, 255)};
    } else if (m_paletteName == QStringLiteral("fldigi")) {
        // WSJT-X ships a palette named "Fldigi"; the stop colours below follow
        // that file, interpolated to 256 colours here.
        stops = {
            QColor(0, 0, 0), QColor(0, 0, 177), QColor(3, 110, 227),
            QColor(0, 204, 204), QColor(223, 223, 223), QColor(0, 234, 0),
            QColor(244, 244, 0), QColor(250, 126, 0), QColor(244, 0, 0)
        };
    } else if (m_paletteName == QStringLiteral("mshv")) {
        // MSHV-like high-contrast blue/green/yellow display: black background,
        // cold weak traces, then bright green/yellow for readable FT streaks.
        stops = {
            QColor(0, 0, 0), QColor(0, 8, 44), QColor(0, 28, 105),
            QColor(0, 78, 150), QColor(0, 145, 95), QColor(70, 205, 40),
            QColor(210, 225, 45), QColor(255, 156, 24), QColor(255, 255, 230)
        };
    } else if (m_paletteName == QStringLiteral("raptor")) {
        stops = {
            QColor(0, 6, 0), QColor(0, 28, 8), QColor(16, 70, 18),
            QColor(42, 118, 30), QColor(92, 164, 46), QColor(164, 210, 74),
            QColor(230, 238, 130)
        };
    } else {
        // v1.59 default: darker noise floor, fast transition through blue/cyan,
        // then green/yellow/orange for actual traces.  This is deliberately
        // non-linear looking and closer to the practical contrast operators
        // expect from WSJT-X/MSHV/fldigi waterfalls.
        stops = {
            QColor(0, 0, 0), QColor(0, 3, 34), QColor(0, 18, 96),
            QColor(0, 55, 150), QColor(0, 130, 155), QColor(20, 185, 70),
            QColor(170, 222, 40), QColor(255, 238, 40), QColor(255, 126, 0),
            QColor(255, 250, 215)
        };
    }

    for (int v = 0; v < 256; ++v) {
        m_colorTable[v] = interpolateStops(stops, v).rgb();
    }
}

QRgb WaterfallWidget::colorForIntensity(quint8 value) const
{
    const double input = static_cast<double>(value) / 255.0;
    const double percent = static_cast<double>(qBound(5, m_colorScalePercent, 100));

    /*
     * Older builds simply multiplied intensity by the colour-scale slider.
     * At 60-70% that capped strong traces below the hot palette colours, so
     * FT8 lines stopped popping out.  Treat the slider as contrast/gamma
     * instead: reducing it pushes the noise floor down while full-strength
     * signals can still reach yellow/orange/white.
     */
    // Slider is now a contrast curve around the already logarithmic DSP
    // intensity.  Around 80% gives the practical weak-signal pop requested by
    // testing; lower values darken the floor more aggressively.
    const double gamma = 1.0 + ((100.0 - percent) / 115.0);
    const int scaled = qBound(0, static_cast<int>(qRound(255.0 * qPow(input, gamma))), 255);

    if (m_colorTable.size() == 256) {
        return m_colorTable[scaled];
    }

    return QColor(scaled, scaled, scaled).rgb();
}

void WaterfallWidget::ensureImage()
{
    if (width() <= 0 || height() <= 0) {
        return;
    }

    if (m_image.size() == size() && !m_image.isNull()) {
        return;
    }

    m_image = QImage(size(), QImage::Format_RGB32);
    m_image.fill(QColor(4, 6, 8));
}
