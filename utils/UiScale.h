#ifndef MADMODEM_UISCALE_H
#define MADMODEM_UISCALE_H

#include <QApplication>
#include <QByteArray>
#include <QGuiApplication>
#include <QLayout>
#include <QMainWindow>
#include <QScreen>
#include <QSize>
#include <QWidget>
#include <QtGlobal>

namespace MadModemUi {

inline double scaleFactor()
{
    const QScreen *screen = QGuiApplication::primaryScreen();
    if (screen == nullptr) {
        return 1.0;
    }
    const QSize available = screen->availableGeometry().size();
    const double sx = static_cast<double>(available.width()) / 2880.0;
    const double sy = static_cast<double>(available.height()) / 1800.0;
    const double s = qMin(sx, sy);
    // Keep the 2880x1800 Linux layout as the design reference.  On smaller or
    // lower-DPI Windows displays, shrink hard-coded hints instead of letting
    // dialogs and side panes overflow/deform.
    return qBound(0.58, s, 1.08);
}

inline int px(int value)
{
    return qMax(0, qRound(static_cast<double>(value) * scaleFactor()));
}

inline QSize size(int w, int h)
{
    return QSize(px(w), px(h));
}

inline void applyLayoutScale(QLayout *layout)
{
    if (layout == nullptr) {
        return;
    }
    // QMainWindow owns an internal QMainWindowLayout. Querying itemAt/count on
    // that private layout can print "QMainWindowLayout::count: ?" on Qt5.
    // Scale the central widget/dialog layouts instead and leave that private
    // layout untouched.
    if (QByteArray(layout->metaObject()->className()).contains("QMainWindowLayout")) {
        return;
    }
    const double s = scaleFactor();
    int l = 0, t = 0, r = 0, b = 0;
    layout->getContentsMargins(&l, &t, &r, &b);
    layout->setContentsMargins(qRound(l * s), qRound(t * s), qRound(r * s), qRound(b * s));
    if (layout->spacing() >= 0) {
        layout->setSpacing(qRound(layout->spacing() * s));
    }
    const int count = layout->count();
    for (int i = 0; i < count; ++i) {
        QLayoutItem *item = layout->itemAt(i);
        if (item != nullptr) {
            applyLayoutScale(item->layout());
        }
    }
}

inline void markScaled(QWidget *w)
{
    if (w != nullptr) {
        w->setProperty("madmodemUiScaled", true);
    }
}

inline bool wasScaled(QWidget *w)
{
    return w != nullptr && w->property("madmodemUiScaled").toBool();
}

inline void scaleWidgetTree(QWidget *root)
{
    if (root == nullptr || wasScaled(root)) {
        return;
    }
    const double s = scaleFactor();
    const QList<QWidget *> widgets = root->findChildren<QWidget *>();
    QList<QWidget *> all;
    all << root;
    all << widgets;
    for (QWidget *w : all) {
        if (w == nullptr || wasScaled(w)) {
            continue;
        }
        const QSize min = w->minimumSize();
        if (!min.isEmpty()) {
            w->setMinimumSize(QSize(qRound(min.width() * s), qRound(min.height() * s)));
        }
        const QSize max = w->maximumSize();
        if (max.width() < QWIDGETSIZE_MAX || max.height() < QWIDGETSIZE_MAX) {
            w->setMaximumSize(QSize(max.width() < QWIDGETSIZE_MAX ? qRound(max.width() * s) : QWIDGETSIZE_MAX,
                                    max.height() < QWIDGETSIZE_MAX ? qRound(max.height() * s) : QWIDGETSIZE_MAX));
        }
        const QSize base = w->baseSize();
        if (!base.isEmpty()) {
            w->setBaseSize(QSize(qRound(base.width() * s), qRound(base.height() * s)));
        }
        markScaled(w);
    }
    if (qobject_cast<QMainWindow *>(root) != nullptr) {
        if (QMainWindow *mw = qobject_cast<QMainWindow *>(root)) {
            applyLayoutScale(mw->centralWidget() != nullptr ? mw->centralWidget()->layout() : nullptr);
        }
    } else {
        applyLayoutScale(root->layout());
    }
}

} // namespace MadModemUi

#endif // MADMODEM_UISCALE_H
