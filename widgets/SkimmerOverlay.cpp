#include "SkimmerOverlay.h"
#include <QPainter>
#include <QFontMetrics>
#include <QSet>
#include <algorithm>
#include <cmath>

void SkimmerOverlay::setSpots(const QVector<SkimmerSpot>& spots, bool cqOnly) {
    m_spots = spots;
    if (m_cqOnly != cqOnly) m_columns.clear();
    m_cqOnly = cqOnly;
    QSet<quint64> live;
    for (const auto& spot : spots) live.insert(spot.id);
    for (auto it = m_columns.begin(); it != m_columns.end();)
        if (!live.contains(it.key())) it = m_columns.erase(it); else ++it;
}
void SkimmerOverlay::clear() { m_spots.clear(); m_columns.clear(); }

void SkimmerOverlay::draw(QPainter& p, const QRect& plot, double minHz, double maxHz,
                          const QVector<double>& markers) {
    if (m_spots.isEmpty() || plot.width() < 30 || plot.height() < 30 || maxHz <= minHz) return;
    if (m_width != plot.width() || m_minHz != minHz || m_maxHz != maxHz) {
        m_columns.clear(); m_width = plot.width(); m_minHz = minHz; m_maxHz = maxHz;
    }
    p.save(); p.setClipRect(plot);
    QFont font = p.font(); font.setBold(true); font.setPointSize(9); p.setFont(font);
    const QFontMetrics fm(font);
    const int step = fm.height() + 3;
    const QRect content = plot.adjusted(0, step + 2, 0, 0);
    if (content.height() < step) { p.restore(); return; }
    const int columnWidth = fm.horizontalAdvance(QLatin1Char('W')) + 4;
    auto xFor = [&](double hz) { return plot.left() + qRound((hz - minHz) * (plot.width() - 1) / (maxHz - minHz)); };
    QVector<int> traces;
    for (const auto& s : m_spots) {
        for (double hz : {s.frequencyHz, s.lowerToneHz, s.upperToneHz})
            if (hz > 0 && hz >= minHz && hz <= maxHz) traces.append(xFor(hz));
    }
    for (double hz : markers) if (hz >= minHz && hz <= maxHz) traces.append(xFor(hz));
    QVector<QRect> occupied;
    int hidden = 0;
    for (const auto& s : m_spots) {
        if (s.frequencyHz < minHz || s.frequencyHz > maxHz) continue;
        QString text = m_cqOnly ? s.cqCall : s.text;
        if (text.trimmed().isEmpty()) continue;
        text.replace(QLatin1Char('\n'), QLatin1Char(' '));
        const int x = xFor(s.frequencyHz);
        QRect box;
        if (!m_cqOnly) {
            auto fits = [&](int left) {
                const QRect candidate(left, content.top(), columnWidth, content.height());
                if (!plot.contains(candidate)) return false;
                for (int trace : traces) if (trace >= left - 2 && trace <= left + columnWidth + 2) return false;
                for (const QRect& used : occupied) if (candidate.adjusted(-2, 0, 2, 0).intersects(used)) return false;
                return true;
            };
            int left = m_columns.value(s.id, -10000);
            if (!fits(left) || std::abs(left - x) > 120) {
                left = -10000;
                // Nearest free column, without drawing over any carrier or
                // splitting one station's letters among several columns.
                for (int d = 4; d <= 120 && left == -10000; ++d) {
                    if (fits(x + d)) left = x + d;
                    else if (fits(x - d - columnWidth)) left = x - d - columnWidth;
                }
            }
            if (left == -10000) { ++hidden; continue; }
            m_columns.insert(s.id, left);
            box = QRect(left, content.top(), columnWidth, content.height());
        } else {
            const int width = fm.horizontalAdvance(text) + 12;
            if (width > plot.width()) { ++hidden; continue; }
            const int left = qBound(plot.left(), x - width / 2, plot.right() - width + 1);
            for (int y = plot.bottom() - step - 6; y >= content.top(); y -= step + 4) {
                const QRect candidate(left, y, width, step);
                bool collision = false;
                for (const auto& used : occupied) if (candidate.adjusted(-3, -2, 3, 2).intersects(used)) { collision = true; break; }
                if (!collision) { box = candidate; break; }
            }
            if (box.isNull()) { ++hidden; continue; }
        }
        occupied.append(box);
        const QColor color = QColor::fromHsv(int((s.id * 47) % 360), 100, 255);
        p.fillRect(box, QColor(0, 0, 0, 190));
        p.setPen(color);
        if (m_cqOnly) {
            p.drawText(box, Qt::AlignCenter, text);
            p.drawLine(x, plot.bottom(), box.center().x(), box.bottom());
        } else {
            const int capacity = qMax(1, (box.height() - 10) / step);
            const QString tail = text.right(capacity);
            const int top = box.bottom() - 10 - tail.size() * step;
            for (int i = 0; i < tail.size(); ++i) {
                const QString glyph = tail.at(i).isSpace() ? QString(QChar(0x00b7)) : QString(tail.at(i));
                p.drawText(QRect(box.left(), top + i * step, box.width(), step), Qt::AlignCenter, glyph);
            }
            p.drawLine(x, plot.bottom(), box.center().x(), box.bottom() - 5);
        }
    }
    if (hidden) {
        const QString label = QStringLiteral("+%1 · zoom").arg(hidden);
        const QRect status(plot.left(), plot.top(), fm.horizontalAdvance(label) + 10, step);
        p.fillRect(status, Qt::black); p.setPen(Qt::white); p.drawText(status, Qt::AlignCenter, label);
    }
    p.restore();
}
