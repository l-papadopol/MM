#include "DdspPanelWidget.h"

#include "../utils/RuntimeI18n.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QFileInfo>
#include <QGridLayout>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QProgressBar>
#include <QPainter>
#include <QPixmap>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPushButton>
#include <QPolygonF>
#include <QRadialGradient>
#include <QSpinBox>
#include <QSizePolicy>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <cmath>

namespace {
QString T(const QString &s) { return MadModemI18n::text(s); }
}

class NeuralMatrixWidget : public QWidget
{
public:
    explicit NeuralMatrixWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumHeight(92);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setToolTip(T(QStringLiteral("Live MIND neural activity. The displayed layer geometry follows the selected MIND profile.")));
    }

    void setProfile(const QString &profileName, const QVector<int> &layers)
    {
        m_profileName = profileName;
        m_layers = layers;
        update();
    }

    void setActivity(const QVector<float> &activity)
    {
        m_activity = activity;
        update();
    }

    void setIdleText(const QString &text)
    {
        m_idleText = text;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.fillRect(rect(), QColor(18, 22, 26));
        p.setPen(QColor(80, 90, 100));
        p.drawRect(rect().adjusted(0, 0, -1, -1));

        if (m_layers.isEmpty()) {
            p.setPen(QColor(190, 195, 200));
            p.drawText(rect(), Qt::AlignCenter, T(QStringLiteral("No MIND profile selected")));
            return;
        }

        const int cell = 3;
        const int gap = 1;
        const int stride = cell + gap;
        const int left = 6;
        const int right = 6;
        const int top = 18;
        const int bottom = 8;
        const int layerCount = m_layers.size();
        const int totalGap = 8 * (layerCount - 1);
        const int availW = qMax(1, width() - left - right - totalGap);
        const int layerW = qMax(18, availW / layerCount);
        const int availH = qMax(1, height() - top - bottom);

        int activityOffset = 0;
        for (int l = 0; l < layerCount; ++l) {
            const int neurons = qMax(1, m_layers[l]);
            const int x0 = left + l * (layerW + 8);
            const int cols = qMax(1, layerW / stride);
            const int rows = qMax(1, availH / stride);
            const int drawable = qMax(1, cols * rows);
            const int cellsToDraw = qMin(neurons, drawable);

            p.setPen(QColor(160, 166, 172));
            const QString layerText = QString::number(neurons);
            p.drawText(QRect(x0, 2, layerW, 14), Qt::AlignCenter, layerText);

            for (int i = 0; i < cellsToDraw; ++i) {
                const int srcIdx = activityOffset + static_cast<int>((static_cast<qint64>(i) * neurons) / cellsToDraw);
                float v = -1.0f;
                if (srcIdx >= 0 && srcIdx < m_activity.size()) {
                    v = qBound(0.0f, m_activity[srcIdx], 1.0f);
                }
                const int x = x0 + (i % cols) * stride;
                const int y = top + (i / cols) * stride;
                QColor c;
                if (v < 0.0f) {
                    c = QColor(42, 48, 54);
                } else if (v < 0.33f) {
                    c = QColor(20, 70 + static_cast<int>(v * 180.0f), 170);
                } else if (v < 0.66f) {
                    c = QColor(20 + static_cast<int>((v - 0.33f) * 300.0f), 180, 80);
                } else {
                    c = QColor(220, 130 + static_cast<int>((v - 0.66f) * 250.0f), 40);
                }
                p.fillRect(QRect(x, y, cell, cell), c);
            }
            activityOffset += neurons;
        }

        if (m_activity.isEmpty()) {
            p.setPen(QColor(190, 195, 200));
            p.drawText(rect().adjusted(0, 14, 0, 0), Qt::AlignCenter,
                       m_idleText.isEmpty() ? T(QStringLiteral("Waiting for profile activity")) : m_idleText);
        }
    }

private:
    QString m_profileName;
    QString m_idleText;
    QVector<int> m_layers;
    QVector<float> m_activity;
};

class MindNixieGaugeWidget : public QWidget
{
public:
    enum VisualState
    {
        Disabled,
        Learning,
        Assist
    };

    explicit MindNixieGaugeWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumHeight(128);
        setMaximumHeight(150);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setToolTip(T(QStringLiteral("MIND extra decodes for the current FT session.")));
    }

    void setStats(int extraLastSlot,
                  int extraSession,
                  int ldpcSkippedLastSlot,
                  int ldpcSkippedSession,
                  int scoredLastSlot,
                  double avgSuccessPercent,
                  bool active)
    {
        Q_UNUSED(extraLastSlot)
        Q_UNUSED(ldpcSkippedLastSlot)
        Q_UNUSED(ldpcSkippedSession)
        Q_UNUSED(scoredLastSlot)
        Q_UNUSED(avgSuccessPercent)
        m_extraSession = qMax(0, extraSession);
        m_active = active;
        update();
    }

    void setMindVisualState(VisualState state,
                            const QString &profile,
                            const QString &reason,
                            int samples,
                            int validationCount)
    {
        m_visualState = state;
        m_profile = profile.trimmed().toUpper();
        m_readinessReason = reason;
        m_profileSamples = qMax(0, samples);
        m_profileValidationCount = qMax(0, validationCount);
        setToolTip(statusToolTip());
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);

        const QRectF r = rect().adjusted(1, 1, -2, -2);
        QLinearGradient bg(r.topLeft(), r.bottomLeft());
        bg.setColorAt(0.0, QColor(18, 10, 6));
        bg.setColorAt(0.55, QColor(6, 4, 4));
        bg.setColorAt(1.0, QColor(2, 2, 3));
        p.setBrush(bg);
        p.setPen(QPen(QColor(112, 62, 30), 1.2));
        p.drawRoundedRect(r, 8, 8);
        p.setPen(QPen(QColor(236, 130, 48, 42), 3.0));
        p.drawRoundedRect(r.adjusted(1.5, 1.5, -1.5, -1.5), 8, 8);

        QFont titleFont = font();
        titleFont.setBold(true);
        titleFont.setPointSizeF(qMax(7.5, titleFont.pointSizeF() * 0.82));
        p.setFont(titleFont);
        p.setPen(QColor(224, 142, 72));
        p.drawText(r.adjusted(9, 6, -9, 0), Qt::AlignLeft | Qt::AlignTop,
                   T(QStringLiteral("MIND GAIN")));

        const qreal iconSide = qMin<qreal>(96.0, qMax<qreal>(80.0, r.height() - 26.0));
        const QRectF iconArea(r.right() - iconSide - 9.0,
                              r.top() + (r.height() - iconSide) / 2.0,
                              iconSide,
                              iconSide);
        drawStatusIcon(p, iconArea.adjusted(3, 3, -3, -3));

        const QRectF stateArea(r.left() + 12.0,
                               r.bottom() - 28.0,
                               iconArea.left() - r.left() - 18.0,
                               22.0);

        const QString digits = QStringLiteral("%1").arg(qMin(999, m_extraSession), 3, 10, QLatin1Char('0'));
        const QRectF digitArea = QRectF(r.left() + 18.0,
                                        r.top() + 35.0,
                                        qMax<qreal>(60.0, iconArea.left() - r.left() - 30.0),
                                        qMax<qreal>(46.0, stateArea.top() - r.top() - 44.0));
        const qreal gap = qMax<qreal>(8.0, digitArea.width() * 0.034);
        const qreal tileW = qMin((digitArea.width() - 2.0 * gap) / 3.0, digitArea.height() * 0.72);
        const qreal tileH = digitArea.height();
        const qreal totalW = tileW * 3.0 + gap * 2.0;
        qreal x = digitArea.center().x() - totalW / 2.0;
        for (int i = 0; i < 3; ++i) {
            const QRectF tube(x, digitArea.top(), tileW, tileH);
            drawNixieTube(p, tube, digits.at(i), m_active);
            x += tileW + gap;
        }

        QFont stateFont = font();
        stateFont.setBold(true);
        stateFont.setPointSizeF(qMax(8.0, stateFont.pointSizeF() * 0.88));
        p.setFont(stateFont);
        p.setPen(m_visualState == Assist ? QColor(114, 232, 132)
                                         : (m_visualState == Learning ? QColor(255, 205, 96) : QColor(135, 140, 145)));
        p.drawText(stateArea, Qt::AlignLeft | Qt::AlignVCenter, statusShortText());
    }

private:
    QString statusShortText() const
    {
        switch (m_visualState) {
        case Assist:
            return T(QStringLiteral("Assist")) + QStringLiteral(" · ") + (m_profile.isEmpty() ? QStringLiteral("FT") : m_profile);
        case Learning:
            return T(QStringLiteral("Learning")) + QStringLiteral(" · ") + (m_profile.isEmpty() ? QStringLiteral("FT") : m_profile);
        case Disabled:
        default:
            return T(QStringLiteral("Disabled"));
        }
    }

    QString statusToolTip() const
    {
        QString title;
        QString assist;
        if (m_visualState == Assist) {
            title = T(QStringLiteral("MIND assist active"));
            assist = T(QStringLiteral("ready"));
        } else if (m_visualState == Learning) {
            title = T(QStringLiteral("MIND learning"));
            assist = T(QStringLiteral("not ready yet"));
        } else {
            title = T(QStringLiteral("MIND disabled"));
            assist = T(QStringLiteral("unavailable"));
        }

        QString tip = title + QStringLiteral("\n") +
                      T(QStringLiteral("Profile")) + QStringLiteral(": ") + (m_profile.isEmpty() ? QStringLiteral("--") : m_profile) + QStringLiteral("\n") +
                      T(QStringLiteral("Assist")) + QStringLiteral(": ") + assist + QStringLiteral("\n") +
                      T(QStringLiteral("Samples")) + QStringLiteral(": ") + QString::number(m_profileSamples) + QStringLiteral(" · ") +
                      T(QStringLiteral("Val")) + QStringLiteral(": ") + QString::number(m_profileValidationCount);
        if (!m_readinessReason.trimmed().isEmpty()) {
            tip += QStringLiteral("\n") + T(QStringLiteral("Reason")) + QStringLiteral(": ") + T(m_readinessReason);
        }
        return tip;
    }

    void drawStatusIcon(QPainter &p, const QRectF &r) const
    {
        p.save();
        p.setRenderHint(QPainter::Antialiasing, true);
        QRadialGradient halo(r.center(), r.width() * 0.75);
        if (m_visualState == Assist) {
            halo.setColorAt(0.0, QColor(80, 255, 120, 60));
            halo.setColorAt(1.0, QColor(80, 255, 120, 0));
        } else if (m_visualState == Learning) {
            halo.setColorAt(0.0, QColor(255, 170, 45, 70));
            halo.setColorAt(1.0, QColor(255, 170, 45, 0));
        } else {
            halo.setColorAt(0.0, QColor(160, 170, 180, 38));
            halo.setColorAt(1.0, QColor(160, 170, 180, 0));
        }
        p.setBrush(halo);
        p.setPen(Qt::NoPen);
        p.drawEllipse(r.adjusted(-4, -4, 4, 4));

        QString resourcePath;
        if (m_visualState == Assist) {
            resourcePath = QStringLiteral(":/icons/mind_assist_cap.png");
        } else if (m_visualState == Learning) {
            resourcePath = QStringLiteral(":/icons/mind_learning_brain.png");
        }

        if (!resourcePath.isEmpty()) {
            const QPixmap icon(resourcePath);
            if (!icon.isNull()) {
                const QSizeF scaled = icon.size().scaled(r.size().toSize(), Qt::KeepAspectRatio);
                const QRectF target(r.center().x() - scaled.width() / 2.0,
                                    r.center().y() - scaled.height() / 2.0,
                                    scaled.width(),
                                    scaled.height());
                p.drawPixmap(target, icon, QRectF(QPointF(0, 0), icon.size()));
                p.restore();
                return;
            }
        }

        if (m_visualState == Assist) {
            drawGraduateCapIcon(p, r);
        } else if (m_visualState == Learning) {
            drawBrainIcon(p, r);
        } else {
            drawMindOffIcon(p, r);
        }
        p.restore();
    }

    static void drawBrainIcon(QPainter &p, const QRectF &r)
    {
        p.save();
        const QColor fill(255, 158, 68);
        const QColor dark(120, 48, 18);
        const QColor glow(255, 220, 92, 130);
        QPen glowPen(glow, qMax<qreal>(2.0, r.width() * 0.045), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        QPen linePen(dark, qMax<qreal>(2.0, r.width() * 0.038), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        QRectF head = r.adjusted(r.width()*0.11, r.height()*0.16, -r.width()*0.11, -r.height()*0.12);
        QPainterPath brain;
        brain.addEllipse(QRectF(head.left(), head.top()+head.height()*0.10, head.width()*0.56, head.height()*0.66));
        brain.addEllipse(QRectF(head.left()+head.width()*0.30, head.top(), head.width()*0.46, head.height()*0.62));
        brain.addEllipse(QRectF(head.left()+head.width()*0.50, head.top()+head.height()*0.18, head.width()*0.44, head.height()*0.62));
        brain.addEllipse(QRectF(head.left()+head.width()*0.32, head.top()+head.height()*0.38, head.width()*0.48, head.height()*0.54));
        p.setBrush(fill);
        p.setPen(QPen(dark, qMax<qreal>(2.2, r.width()*0.035)));
        p.drawPath(brain.simplified());

        QPainterPath folds;
        folds.moveTo(head.left()+head.width()*0.24, head.top()+head.height()*0.32);
        folds.cubicTo(head.left()+head.width()*0.40, head.top()+head.height()*0.18, head.left()+head.width()*0.46, head.top()+head.height()*0.42, head.left()+head.width()*0.34, head.top()+head.height()*0.52);
        folds.moveTo(head.left()+head.width()*0.50, head.top()+head.height()*0.18);
        folds.cubicTo(head.left()+head.width()*0.66, head.top()+head.height()*0.30, head.left()+head.width()*0.54, head.top()+head.height()*0.52, head.left()+head.width()*0.70, head.top()+head.height()*0.66);
        folds.moveTo(head.left()+head.width()*0.20, head.top()+head.height()*0.62);
        folds.cubicTo(head.left()+head.width()*0.38, head.top()+head.height()*0.72, head.left()+head.width()*0.54, head.top()+head.height()*0.60, head.left()+head.width()*0.80, head.top()+head.height()*0.72);
        p.setPen(glowPen);
        p.drawPath(folds);
        p.setPen(linePen);
        p.drawPath(folds);
        p.restore();
    }

    static void drawGraduateCapIcon(QPainter &p, const QRectF &r)
    {
        p.save();
        const QColor top(40, 54, 66);
        const QColor edge(12, 18, 24);
        const QColor accent(255, 190, 62);
        const qreal lw = qMax<qreal>(2.0, r.width()*0.035);
        QPolygonF board;
        board << QPointF(r.center().x(), r.top()+r.height()*0.12)
              << QPointF(r.right()-r.width()*0.06, r.top()+r.height()*0.34)
              << QPointF(r.center().x(), r.top()+r.height()*0.56)
              << QPointF(r.left()+r.width()*0.06, r.top()+r.height()*0.34);
        p.setBrush(top);
        p.setPen(QPen(edge, lw, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPolygon(board);
        QRectF band(r.left()+r.width()*0.24, r.top()+r.height()*0.45, r.width()*0.52, r.height()*0.25);
        p.setBrush(QColor(58, 76, 92));
        p.drawRoundedRect(band, r.width()*0.08, r.width()*0.08);
        p.setPen(QPen(accent, qMax<qreal>(2.0, r.width()*0.045), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        const QPointF tasselStart(r.center().x()+r.width()*0.18, r.top()+r.height()*0.30);
        const QPointF tasselEnd(r.right()-r.width()*0.18, r.bottom()-r.height()*0.18);
        p.drawLine(tasselStart, tasselEnd);
        p.setBrush(accent);
        p.setPen(Qt::NoPen);
        p.drawEllipse(tasselEnd, r.width()*0.055, r.width()*0.055);
        p.restore();
    }

    static void drawMindOffIcon(QPainter &p, const QRectF &r)
    {
        p.save();
        QRectF box = r.adjusted(r.width()*0.13, r.height()*0.13, -r.width()*0.13, -r.height()*0.13);
        p.setBrush(QColor(80, 86, 92));
        p.setPen(QPen(QColor(40, 45, 50), qMax<qreal>(2.0, r.width()*0.035)));
        p.drawRoundedRect(box, r.width()*0.12, r.width()*0.12);
        p.setPen(QPen(QColor(170, 178, 186), qMax<qreal>(5.0, r.width()*0.075), Qt::SolidLine, Qt::RoundCap));
        p.drawLine(box.topLeft()+QPointF(box.width()*0.18, box.height()*0.18),
                   box.bottomRight()-QPointF(box.width()*0.18, box.height()*0.18));
        p.restore();
    }

    static void drawHoneycomb(QPainter &p, const QRectF &r)
    {
        p.save();
        p.setClipRect(r.adjusted(1, 1, -1, -1));
        p.setPen(QPen(QColor(168, 62, 20, 56), 0.7));
        const qreal cell = qMax<qreal>(5.0, r.width() / 12.0);
        const qreal h = cell * 0.8660254;
        for (qreal y = r.top() - h; y < r.bottom() + h; y += h) {
            const int row = static_cast<int>((y - r.top()) / h);
            for (qreal x = r.left() - cell; x < r.right() + cell; x += cell * 1.5) {
                const qreal ox = (row & 1) ? cell * 0.75 : 0.0;
                QPolygonF poly;
                for (int k = 0; k < 6; ++k) {
                    const double a = 3.14159265358979323846 / 6.0 + k * 3.14159265358979323846 / 3.0;
                    poly << QPointF(x + ox + cell * 0.48 * std::cos(a), y + cell * 0.48 * std::sin(a));
                }
                p.drawPolygon(poly);
            }
        }
        p.restore();
    }

    static QPainterPath sevenSegmentPath(const QRectF &r, int mask)
    {
        auto seg = [](const QRectF &sr, bool horizontal) {
            QPainterPath path;
            const qreal radius = qMin(sr.width(), sr.height()) * 0.45;
            if (horizontal) {
                QPolygonF poly;
                poly << QPointF(sr.left() + radius, sr.top())
                     << QPointF(sr.right() - radius, sr.top())
                     << QPointF(sr.right(), sr.center().y())
                     << QPointF(sr.right() - radius, sr.bottom())
                     << QPointF(sr.left() + radius, sr.bottom())
                     << QPointF(sr.left(), sr.center().y());
                path.addPolygon(poly);
                path.closeSubpath();
            } else {
                QPolygonF poly;
                poly << QPointF(sr.center().x(), sr.top())
                     << QPointF(sr.right(), sr.top() + radius)
                     << QPointF(sr.right(), sr.bottom() - radius)
                     << QPointF(sr.center().x(), sr.bottom())
                     << QPointF(sr.left(), sr.bottom() - radius)
                     << QPointF(sr.left(), sr.top() + radius);
                path.addPolygon(poly);
                path.closeSubpath();
            }
            return path;
        };

        const qreal t = qMax<qreal>(3.0, qMin(r.width(), r.height()) * 0.13);
        const qreal x0 = r.left();
        const qreal x1 = r.right();
        const qreal y0 = r.top();
        const qreal y1 = r.bottom();
        const qreal ym = r.center().y();
        const qreal inset = t * 0.58;
        QPainterPath out;
        if (mask & 0x01) out.addPath(seg(QRectF(x0 + inset, y0, r.width() - 2 * inset, t), true));
        if (mask & 0x02) out.addPath(seg(QRectF(x1 - t, y0 + inset, t, ym - y0 - inset), false));
        if (mask & 0x04) out.addPath(seg(QRectF(x1 - t, ym, t, y1 - ym - inset), false));
        if (mask & 0x08) out.addPath(seg(QRectF(x0 + inset, y1 - t, r.width() - 2 * inset, t), true));
        if (mask & 0x10) out.addPath(seg(QRectF(x0, ym, t, y1 - ym - inset), false));
        if (mask & 0x20) out.addPath(seg(QRectF(x0, y0 + inset, t, ym - y0 - inset), false));
        if (mask & 0x40) out.addPath(seg(QRectF(x0 + inset, ym - t / 2.0, r.width() - 2 * inset, t), true));
        return out;
    }

    static int digitMask(QChar ch)
    {
        switch (ch.toLatin1()) {
        case '0': return 0x3f; // a b c d e f
        case '1': return 0x06;
        case '2': return 0x5b;
        case '3': return 0x4f;
        case '4': return 0x66;
        case '5': return 0x6d;
        case '6': return 0x7d;
        case '7': return 0x07;
        case '8': return 0x7f;
        case '9': return 0x6f;
        default: return 0x00;
        }
    }

    static void drawNixieTube(QPainter &p, const QRectF &tubeRect, QChar digit, bool active)
    {
        p.save();
        const QColor rim(118, 46, 20, active ? 150 : 70);
        const QColor glass(28, 12, 5, active ? 190 : 130);
        const QRectF glassRect = tubeRect.adjusted(1.0, 1.0, -1.0, -1.0);
        QLinearGradient tubeBg(glassRect.topLeft(), glassRect.bottomRight());
        tubeBg.setColorAt(0.0, QColor(32, 14, 7));
        tubeBg.setColorAt(1.0, QColor(2, 2, 3));
        p.setBrush(tubeBg);
        p.setPen(QPen(rim, 1.1));
        p.drawRoundedRect(glassRect, 12, 12);
        p.setPen(QPen(QColor(235, 95, 22, active ? 42 : 22), 3.0));
        p.drawRoundedRect(glassRect.adjusted(1, 1, -1, -1), 12, 12);

        drawHoneycomb(p, glassRect.adjusted(4, 4, -4, -4));

        // Nixie internal supports/wires behind the active digit.
        p.setPen(QPen(QColor(145, 54, 22, active ? 75 : 36), 0.8));
        const qreal cx = glassRect.center().x();
        p.drawLine(QPointF(cx, glassRect.top() + 5), QPointF(cx, glassRect.bottom() - 5));
        p.drawLine(QPointF(cx - glassRect.width() * 0.20, glassRect.top() + 8),
                   QPointF(cx + glassRect.width() * 0.15, glassRect.bottom() - 8));
        p.drawLine(QPointF(cx + glassRect.width() * 0.20, glassRect.top() + 8),
                   QPointF(cx - glassRect.width() * 0.15, glassRect.bottom() - 8));

        const QRectF d = glassRect.adjusted(glassRect.width() * 0.18,
                                            glassRect.height() * 0.12,
                                            -glassRect.width() * 0.18,
                                            -glassRect.height() * 0.12);
        QPainterPath path = sevenSegmentPath(d, digitMask(digit));

        // Dark red inactive digit skeleton.
        p.setBrush(QColor(100, 22, 8, 80));
        p.setPen(Qt::NoPen);
        p.drawPath(sevenSegmentPath(d, 0x7f));

        if (active) {
            for (int glow = 10; glow >= 2; glow -= 2) {
                p.setPen(QPen(QColor(255, 58, 8, 12 + glow * 3), glow, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                p.drawPath(path);
            }
            p.fillPath(path, QColor(255, 178, 34));
            p.setPen(QPen(QColor(255, 225, 64), 1.0));
            p.drawPath(path);
        } else {
            p.fillPath(path, QColor(105, 54, 25));
        }

        if (digit == QLatin1Char('0')) {
            const qreal dotR = qMax<qreal>(2.0, qMin(d.width(), d.height()) * 0.08);
            const QPointF dotCenter = glassRect.center();
            if (active) {
                QRadialGradient g(dotCenter, dotR * 4.5);
                g.setColorAt(0.0, QColor(255, 235, 70, 255));
                g.setColorAt(0.35, QColor(255, 130, 20, 190));
                g.setColorAt(1.0, QColor(255, 70, 5, 0));
                p.setBrush(g);
                p.setPen(Qt::NoPen);
                p.drawEllipse(dotCenter, dotR * 4.5, dotR * 4.5);
                p.setBrush(QColor(255, 212, 44));
                p.drawEllipse(dotCenter, dotR, dotR);
            } else {
                p.setBrush(QColor(100, 52, 22));
                p.setPen(Qt::NoPen);
                p.drawEllipse(dotCenter, dotR, dotR);
            }
        }
        p.restore();
    }

    int m_extraSession = 0;
    bool m_active = false;
    VisualState m_visualState = Disabled;
    QString m_profile;
    QString m_readinessReason;
    int m_profileSamples = 0;
    int m_profileValidationCount = 0;
};

DdspPanelWidget::DdspPanelWidget(DeepDspController *controller, QWidget *parent)
    : QWidget(parent),
      m_controller(controller)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(6, 6, 6, 6);
    outer->setSpacing(6);

    auto *grpStatus = new QGroupBox(this);
    auto *statusLayout = new QGridLayout(grpStatus);
    statusLayout->setContentsMargins(8, 8, 8, 8);
    statusLayout->setHorizontalSpacing(8);
    statusLayout->setVerticalSpacing(5);

    m_lblState = new QLabel(T(QStringLiteral("Autonomous training")), grpStatus);
    QFont stateFont = m_lblState->font();
    stateFont.setBold(true);
    m_lblState->setFont(stateFont);

    m_lblSamples = new QLabel(T(QStringLiteral("Samples: 0")), grpStatus);
    m_lblSamples->setToolTip(T(QStringLiteral("Weak-signal candidate ranker samples.")));
    m_lblModeMeaning = new QLabel(grpStatus);
    m_lblModeMeaning->setVisible(false);
    m_lblProfileMeaning = new QLabel(grpStatus);
    m_lblProfileMeaning->setVisible(false);
    m_lblBreakdown = new QLabel(QStringLiteral("FT8 0 · FT4 0 · MSK144 0"), grpStatus);
    m_lblArchitecture = new QLabel(QStringLiteral("Net --"), grpStatus);
    m_lblBackend = new QLabel(T(QStringLiteral("Backend: CPU Eigen")), grpStatus);
    m_lblModelState = new QLabel(T(QStringLiteral("Model")) + QStringLiteral(": --"), grpStatus);
    m_lblTrainingCompletion = new QLabel(T(QStringLiteral("Ranker: -- · Best: --")), grpStatus);
    m_lblLoss = new QLabel(QStringLiteral("Replay 0 · Loss --"), grpStatus);

    for (QLabel *lbl : {m_lblState, m_lblSamples, m_lblModeMeaning, m_lblProfileMeaning,
                        m_lblBreakdown, m_lblArchitecture,
                        m_lblBackend, m_lblModelState, m_lblTrainingCompletion, m_lblLoss}) {
        lbl->setWordWrap(true);
        lbl->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    }

    m_progressAccuracy = new QProgressBar(grpStatus);
    m_progressAccuracy->setRange(0, 100);
    m_progressAccuracy->setValue(0);
    m_progressAccuracy->setFormat(T(QStringLiteral("MIND %p%")));
    m_progressAccuracy->setTextVisible(true);

    m_nixieGauge = new MindNixieGaugeWidget(grpStatus);

    statusLayout->addWidget(m_lblState, 0, 0);
    statusLayout->addWidget(m_progressAccuracy, 1, 0);
    statusLayout->addWidget(m_nixieGauge, 2, 0);
    statusLayout->addWidget(m_lblModeMeaning, 3, 0);
    statusLayout->addWidget(m_lblSamples, 4, 0);
    statusLayout->addWidget(m_lblTrainingCompletion, 5, 0);
    statusLayout->addWidget(m_lblProfileMeaning, 6, 0);
    statusLayout->addWidget(m_lblBreakdown, 7, 0);
    statusLayout->addWidget(m_lblArchitecture, 8, 0);
    statusLayout->addWidget(m_lblBackend, 9, 0);
    statusLayout->addWidget(m_lblModelState, 10, 0);
    statusLayout->addWidget(m_lblLoss, 11, 0);
    m_lblArchitecture->setVisible(false);
    m_lblBackend->setVisible(false);
    m_lblModelState->setVisible(false);
    m_lblLoss->setVisible(false);

    auto *grpMatrix = new QGroupBox(this);
    auto *matrixLayout = new QVBoxLayout(grpMatrix);
    matrixLayout->setContentsMargins(8, 8, 8, 8);
    matrixLayout->setSpacing(4);
    auto *profileRow = new QHBoxLayout();
    profileRow->setSpacing(4);
    auto *profileLabel = new QLabel(T(QStringLiteral("Profile view")), grpMatrix);
    m_cmbProfileView = new QComboBox(grpMatrix);
    m_cmbProfileView->addItem(T(QStringLiteral("Auto")), QStringLiteral("AUTO"));
    m_cmbProfileView->addItem(QStringLiteral("FT8"), QStringLiteral("FT8"));
    m_cmbProfileView->addItem(QStringLiteral("FT4"), QStringLiteral("FT4"));
    m_cmbProfileView->addItem(QStringLiteral("MSK144"), QStringLiteral("MSK144"));
    m_cmbProfileView->setToolTip(T(QStringLiteral("FT/MSK144 ranker activity view.")));
    profileRow->addWidget(profileLabel);
    profileRow->addWidget(m_cmbProfileView, 1);
    m_matrixWidget = new NeuralMatrixWidget(grpMatrix);
    matrixLayout->addLayout(profileRow);
    matrixLayout->addWidget(m_matrixWidget);

    // MIND mode is deliberately fixed to Assist-requested.  The old selector
    // made it too easy to leave the ranker off or in a misleading state; the
    // controller now keeps training continuously and only enables assisted
    // ranking when the readiness gate says the model is mature enough.

    outer->addWidget(grpStatus);
    outer->addWidget(grpMatrix);
    outer->addStretch(1);

    if (m_controller != nullptr) {
        m_controller->setAssistMode(QStringLiteral("assisted"));
        connect(m_cmbProfileView, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &DdspPanelWidget::profileSelectionChanged);
        connect(m_controller, &DeepDspController::statusChanged,
                this, &DdspPanelWidget::updateStatus);
        updateStatus(m_controller->status());
    }
}




void DdspPanelWidget::profileSelectionChanged(int)
{
    if (m_controller != nullptr) {
        updateStatus(m_controller->status());
    }
}

QString DdspPanelWidget::effectiveProfile(const DeepDspController::Status &status) const
{
    QString profile = QStringLiteral("AUTO");
    if (m_cmbProfileView != nullptr) {
        profile = m_cmbProfileView->currentData().toString().trimmed().toUpper();
    }
    if (profile == QStringLiteral("AUTO") || profile.isEmpty()) {
        profile = status.activeProfile.trimmed().toUpper();
    }
    if (profile != QStringLiteral("FT8") && profile != QStringLiteral("FT4") &&
        profile != QStringLiteral("MSK144")) {
        profile = QStringLiteral("FT8");
    }
    return profile;
}

void DdspPanelWidget::applyProfileToMatrix(const QString &profile, const DeepDspController::Status &status)
{
    if (m_matrixWidget == nullptr) return;

    QVector<int> layers = QVector<int>{464, 8, 24, 1};
    QVector<float> activity;
    if (status.ftActivity.size() == 464 + 8 + 24 + 1) {
        activity = status.ftActivity;
    } else if (status.neuralActivity.size() == 464 + 8 + 24 + 1) {
        activity = status.neuralActivity;
    }
    m_matrixWidget->setProfile(profile, layers);
    if (activity.isEmpty()) {
        const QString idle = status.modelStateText.contains(QStringLiteral("loaded"), Qt::CaseInsensitive)
            ? T(QStringLiteral("Model loaded")) + QStringLiteral(" · ") + T(QStringLiteral("waiting for %1 activity")).arg(profile)
            : T(QStringLiteral("Model missing")) + QStringLiteral(" · ") + T(QStringLiteral("collecting %1 samples")).arg(profile);
        m_matrixWidget->setIdleText(idle);
    } else {
        m_matrixWidget->setIdleText(QString());
    }
    m_matrixWidget->setActivity(activity);
}

void DdspPanelWidget::updateStatus(const DeepDspController::Status &status)
{
    if (m_lblState != nullptr) {
        m_lblState->setText(T(status.stateText));
    }
    const QString assist = status.assistMode.trimmed().toLower();
    if (m_nixieGauge != nullptr) {
        const bool active = status.enabled && assist != QStringLiteral("off");
        m_nixieGauge->setStats(status.mindExtraLastSlot,
                               status.mindExtraSession,
                               status.mindLdpcSkippedLastSlot,
                               status.mindLdpcSkippedSession,
                               status.mindScoredLastSlot,
                               status.mindAvgSuccessLastSlot,
                               active);
    }
    if (m_lblModeMeaning != nullptr) {
        m_lblModeMeaning->clear();
        m_lblModeMeaning->setVisible(false);
    }
    const QString activeProfileForUi = status.activeProfile.trimmed().toUpper();
    if (m_cmbProfileView != nullptr &&
        (activeProfileForUi == QStringLiteral("FT8") || activeProfileForUi == QStringLiteral("FT4") || activeProfileForUi == QStringLiteral("MSK144"))) {
        const int activeIdx = m_cmbProfileView->findData(activeProfileForUi);
        if (activeIdx >= 0 && m_cmbProfileView->currentIndex() != activeIdx) {
            const QSignalBlocker block(m_cmbProfileView);
            m_cmbProfileView->setCurrentIndex(activeIdx);
        }
    }
    const QString profile = effectiveProfile(status);
    if (m_lblProfileMeaning != nullptr) {
        m_lblProfileMeaning->clear();
        m_lblProfileMeaning->setVisible(false);
    }
    if (m_lblSamples != nullptr) {
        int shownSamples = status.sampleCount;
        if (profile == QStringLiteral("FT8")) shownSamples = status.ft8Samples;
        else if (profile == QStringLiteral("FT4")) shownSamples = status.ft4Samples;
        else if (profile == QStringLiteral("MSK144")) shownSamples = status.msk144Samples;
        m_lblSamples->setText(T(QStringLiteral("Training data")) + QStringLiteral(": %1 %2").arg(profile).arg(shownSamples));
    }
    if (m_lblBreakdown != nullptr) {
        m_lblBreakdown->setText(T(QStringLiteral("Data split")) + QStringLiteral(": FT8 %1 · FT4 %2 · MSK144 %3")
                                    .arg(status.ft8Samples)
                                    .arg(status.ft4Samples)
                                    .arg(status.msk144Samples));
    }
    if (m_lblLoss != nullptr) {
        const QString lossText = T(QStringLiteral("Replay")) + QStringLiteral(" %1 · ") +
                                 T(QStringLiteral("Auto")) + QStringLiteral(" %2 ms/%3 · %4 ") +
                                 T(QStringLiteral("samp/s")) + QStringLiteral(" · ") +
                                 T(QStringLiteral("Loss")) + QStringLiteral(" %5");
        m_lblLoss->setText(lossText.arg(status.replayBufferSamples)
                                   .arg(status.adaptiveTrainingBudgetMs)
                                   .arg(status.adaptiveBatchSize)
                                   .arg(status.trainSamplesPerSecond, 0, 'f', 0)
                                   .arg(status.lastLoss, 0, 'f', 5));
    }
    if (m_lblArchitecture != nullptr) {
        QString arch = status.architectureText;
        if (profile == QStringLiteral("FT4")) arch = QStringLiteral("58×8 → Conv2D → sigmoid");
        else if (profile == QStringLiteral("FT8")) arch = QStringLiteral("58×8 → Conv2D → sigmoid");
        else if (profile == QStringLiteral("MSK144")) arch = QStringLiteral("58×8 ping/chunk → Conv2D → sigmoid");
        m_lblArchitecture->setText(T(QStringLiteral("Net")) + QStringLiteral(" ") + profile + QStringLiteral(": ") + arch);
    }
    if (m_lblBackend != nullptr) {
        m_lblBackend->setText(T(QStringLiteral("Backend")) + QStringLiteral(": ") + status.backendText);
        m_lblBackend->setToolTip(T(QStringLiteral("Low-priority autonomous FT/MSK144 ranker training.")));
    }
    if (m_lblModelState != nullptr) {
        const QString modelText = T(status.modelStateText);
        m_lblModelState->setText(T(QStringLiteral("Model")) + QStringLiteral(": ") + modelText +
                                 QStringLiteral(" · ") + T(QStringLiteral("Dataset")) + QStringLiteral(" ") + QString::number(status.loadedGoldSamples) +
                                 QStringLiteral(" · ") + status.lastCheckpointText);
        m_lblModelState->setToolTip(T(QStringLiteral("Checkpoint")) + QStringLiteral(": ") + status.checkpointPath +
                                    QStringLiteral("\n") + T(QStringLiteral("Stats")) + QStringLiteral(": ") + status.statsPath +
                                    QStringLiteral("\n") + T(QStringLiteral("Gold dataset")) + QStringLiteral(": ") + status.goldDatasetPath);
    }
    constexpr int kUiMinValidation = 200;
    const bool profileIsMsk = (profile == QStringLiteral("MSK144"));
    int profileSamples = status.sampleCount;
    if (profile == QStringLiteral("FT8")) profileSamples = status.ft8Samples;
    else if (profile == QStringLiteral("FT4")) profileSamples = status.ft4Samples;
    else if (profile == QStringLiteral("MSK144")) profileSamples = status.msk144Samples;
    const QString activeProfile = status.activeProfile.trimmed().toUpper();
    const bool showingActiveProfile = (profile == activeProfile);
    const int profileValidation = showingActiveProfile ? status.activeProfileValidationCount : 0;
    const double profileScore = showingActiveProfile ? status.activeProfileRankerAccuracy : 0.0;
    const double profileBest = showingActiveProfile ? status.activeProfileBestRankerAccuracy : 0.0;
    const bool warmup = !showingActiveProfile || profileValidation < kUiMinValidation || profileSamples <= 0;
    if (m_nixieGauge != nullptr) {
        MindNixieGaugeWidget::VisualState visualState = MindNixieGaugeWidget::Disabled;
        const QString assistMode = status.assistMode.trimmed().toLower();
        const bool profileSupported = (profile == QStringLiteral("FT8") || profile == QStringLiteral("FT4") || profile == QStringLiteral("MSK144"));
        if (status.enabled && assistMode != QStringLiteral("off") && profileSupported) {
            visualState = (showingActiveProfile && status.activeProfileAssistReady && status.assistEnabled)
                ? MindNixieGaugeWidget::Assist
                : MindNixieGaugeWidget::Learning;
        }
        m_nixieGauge->setMindVisualState(visualState,
                                         profile,
                                         showingActiveProfile ? status.activeProfileReadinessReason : T(QStringLiteral("profile view is not the active decoder profile")),
                                         profileSamples,
                                         profileValidation);
        m_nixieGauge->setVisible(status.enabled && assistMode != QStringLiteral("off") && profileSupported);
    }
    if (m_lblTrainingCompletion != nullptr) {
        const QString valPart = warmup
            ? QStringLiteral("--")
            : QString::number(profileValidation);
        const QString scorePart = (!warmup && profileScore > 0.0)
            ? QString::number(profileScore, 'f', 1) + QStringLiteral(" %")
            : QStringLiteral("--");
        const QString bestPart = (!warmup && profileBest > 0.0)
            ? QString::number(profileBest, 'f', 1) + QStringLiteral(" %")
            : QStringLiteral("--");
        m_lblTrainingCompletion->setText(T(QStringLiteral("Ranker")) + QStringLiteral(": ") + scorePart + QStringLiteral(" · ") +
                                         T(QStringLiteral("Best")) + QStringLiteral(": ") + bestPart + QStringLiteral(" · ") +
                                         T(QStringLiteral("Val")) + QStringLiteral(": ") + valPart +
                                         QStringLiteral(" · ") + T(QStringLiteral("State")) + QStringLiteral(": ") + T(status.activeProfileLifecycleState) +
                                         QStringLiteral(" · ") + T(QStringLiteral("Pos/Neg")) + QStringLiteral(": ") + QString::number(status.activeProfilePositiveSamples) +
                                         QStringLiteral("/") + QString::number(status.activeProfileNegativeSamples));
        m_lblTrainingCompletion->setToolTip(profileIsMsk
            ? T(QStringLiteral("MSK144 ranker stats are shown only after real MSK144 candidate samples exist."))
            : T(QStringLiteral("FT candidate priority.")));
    }
    if (m_progressAccuracy != nullptr) {
        const int progress = warmup ? 0 : qBound(0, static_cast<int>(status.trainingCompletionPercent + 0.5), 100);
        m_progressAccuracy->setValue(progress);
        m_progressAccuracy->setFormat(profileIsMsk && status.msk144Samples <= 0
            ? T(QStringLiteral("MIND MSK144 0%"))
            : T(QStringLiteral("MIND %p%")));
        m_progressAccuracy->setToolTip(profileIsMsk
            ? T(QStringLiteral("MSK144 training starts only from real MSK144 ping/chunk candidates."))
            : T(QStringLiteral("FT candidate priority.")));
    }
    applyProfileToMatrix(profile, status);
    if (m_lblCheckpoint != nullptr) {
        const QString modelName = status.checkpointPath.isEmpty()
                                  ? QStringLiteral("--")
                                  : QFileInfo(status.checkpointPath).fileName();
        m_lblCheckpoint->setText(T(QStringLiteral("Model")) + QStringLiteral(": ") + modelName);
        m_lblCheckpoint->setToolTip(status.checkpointPath);
    }
    if (m_lblLastCheckpoint != nullptr) {
        m_lblLastCheckpoint->setText(T(QStringLiteral("Checkpoint")) + QStringLiteral(": ") + status.lastCheckpointText);
        m_lblLastCheckpoint->setToolTip(T(QStringLiteral("Stats")) + QStringLiteral(": ") + status.statsPath +
                                        QStringLiteral("\n") + T(QStringLiteral("Gold dataset")) + QStringLiteral(": ") + status.goldDatasetPath);
    }
}
