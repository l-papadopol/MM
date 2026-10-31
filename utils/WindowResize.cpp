#include "WindowResize.h"
#include <QDialog>
#include <QEvent>
#include <QLayout>
#include <QMouseEvent>
#include <QMessageBox>
#include <QVariant>
#include <QTimer>
#include <QWidget>
#include <QWindow>
#include <array>

namespace {
class ResizeHandle final : public QWidget {
public:
    ResizeHandle(QWidget *window, Qt::Edges edges) : QWidget(window), m_edges(edges) {
        setObjectName(QStringLiteral("mmResizeHandle_%1").arg(int(edges)));
        setAttribute(Qt::WA_NoSystemBackground);
        setFocusPolicy(Qt::NoFocus);
        const bool horizontal = edges & (Qt::LeftEdge | Qt::RightEdge);
        const bool vertical = edges & (Qt::TopEdge | Qt::BottomEdge);
        setCursor(horizontal && vertical
            ? ((edges == (Qt::LeftEdge | Qt::TopEdge) || edges == (Qt::RightEdge | Qt::BottomEdge))
                ? Qt::SizeFDiagCursor : Qt::SizeBDiagCursor)
            : (horizontal ? Qt::SizeHorCursor : Qt::SizeVerCursor));
    }
protected:
    void mousePressEvent(QMouseEvent *event) override {
        QWidget *owner = parentWidget();
        if (event->button() != Qt::LeftButton || owner->isMaximized() || owner->isFullScreen()) return;
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
        if (event->spontaneous() && owner->windowHandle() && owner->windowHandle()->startSystemResize(m_edges)) {
            event->accept();
            return;
        }
#endif
        m_origin = event->globalPos();
        m_geometry = owner->geometry();
        m_dragging = true;
        grabMouse();
        event->accept();
    }
    void mouseMoveEvent(QMouseEvent *event) override {
        if (!m_dragging) return;
        if (!(event->buttons() & Qt::LeftButton)) { finish(); return; }
        QWidget *owner = parentWidget();
        const QPoint delta = event->globalPos() - m_origin;
        const QSize minimum = owner->minimumSize().expandedTo(owner->minimumSizeHint());
        const QSize maximum = owner->maximumSize();
        QRect rect = m_geometry;
        if (m_edges & Qt::LeftEdge) {
            const int width = qBound(minimum.width(), m_geometry.width() - delta.x(), maximum.width());
            rect.setLeft(m_geometry.right() - width + 1);
        } else if (m_edges & Qt::RightEdge) {
            rect.setWidth(qBound(minimum.width(), m_geometry.width() + delta.x(), maximum.width()));
        }
        if (m_edges & Qt::TopEdge) {
            const int height = qBound(minimum.height(), m_geometry.height() - delta.y(), maximum.height());
            rect.setTop(m_geometry.bottom() - height + 1);
        } else if (m_edges & Qt::BottomEdge) {
            rect.setHeight(qBound(minimum.height(), m_geometry.height() + delta.y(), maximum.height()));
        }
        owner->setGeometry(rect);
        event->accept();
    }
    void mouseReleaseEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) { finish(); event->accept(); }
    }
    bool event(QEvent *event) override {
        if (event->type() == QEvent::UngrabMouse || event->type() == QEvent::Hide) m_dragging = false;
        return QWidget::event(event);
    }
private:
    void finish() { m_dragging = false; if (QWidget::mouseGrabber() == this) releaseMouse(); }
    Qt::Edges m_edges;
    QPoint m_origin;
    QRect m_geometry;
    bool m_dragging = false;
};

class WindowResize final : public QObject {
public:
    explicit WindowResize(QWidget *owner) : QObject(owner), m_owner(owner) {
        owner->installEventFilter(this);
        refresh();
    }
protected:
    bool eventFilter(QObject *, QEvent *event) override {
        if (event->type() == QEvent::LayoutRequest && m_messageBoxReady) {
            // QMessageBox::event(LayoutRequest) calls its private updateSize(),
            // which restores a fixed size even after a successful edge drag.
            // Activate its public layout without that extra fixed-size pass.
            relaxConstraints();
            if (auto *layout = m_owner->layout()) layout->activate();
            refresh();
            return true;
        }
        if (event->type() == QEvent::Resize || event->type() == QEvent::Show ||
            event->type() == QEvent::WindowStateChange) refresh();
        // QMessageBox/QInputDialog may reset their private layout constraints
        // during show or after changing their text. Relax those after layout.
        if ((event->type() == QEvent::Show || event->type() == QEvent::LayoutRequest) && !m_refreshPending) {
            m_refreshPending = true;
            QTimer::singleShot(0, this, [this] {
                m_refreshPending = false;
                relaxConstraints();
                m_messageBoxReady = qobject_cast<QMessageBox *>(m_owner) != nullptr;
                refresh();
            });
        }
        return false;
    }
private:
    void relaxConstraints() {
        if (!m_owner->isWindow()) return;
        if (auto *layout = m_owner->layout()) {
            if (layout->sizeConstraint() == QLayout::SetFixedSize || layout->sizeConstraint() == QLayout::SetMinAndMaxSize)
                layout->setSizeConstraint(QLayout::SetMinimumSize);
        }
        if (m_owner->maximumSize() != QSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX))
            m_owner->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    }
    void refresh() {
        const bool custom = m_owner->isWindow() && (m_owner->windowFlags() & Qt::FramelessWindowHint);
        if (custom && !m_handles[0]) {
            const std::array<Qt::Edges, 8> edges = {{Qt::LeftEdge, Qt::RightEdge, Qt::TopEdge, Qt::BottomEdge,
                Qt::LeftEdge | Qt::TopEdge, Qt::RightEdge | Qt::TopEdge,
                Qt::LeftEdge | Qt::BottomEdge, Qt::RightEdge | Qt::BottomEdge}};
            for (size_t i = 0; i < edges.size(); ++i) m_handles[i] = new ResizeHandle(m_owner, edges[i]);
        }
        if (!m_handles[0]) return;
        const int w = m_owner->width(), h = m_owner->height(), edge = 5, corner = 10;
        const std::array<QRect, 8> rects = {{
            {0, corner, edge, qMax(0, h - 2 * corner)}, {w-edge, corner, edge, qMax(0, h - 2 * corner)},
            {corner, 0, qMax(0, w - 2 * corner), edge}, {corner, h-edge, qMax(0, w - 2 * corner), edge},
            {0, 0, corner, corner}, {w-corner, 0, corner, corner},
            {0, h-corner, corner, corner}, {w-corner, h-corner, corner, corner}}};
        const bool visible = custom && !m_owner->isMaximized() && !m_owner->isFullScreen();
        for (size_t i = 0; i < m_handles.size(); ++i) {
            m_handles[i]->setGeometry(rects[i]);
            m_handles[i]->setVisible(visible);
            m_handles[i]->raise();
        }
    }
    QWidget *m_owner;
    std::array<ResizeHandle *, 8> m_handles{};
    bool m_refreshPending = false;
    bool m_messageBoxReady = false;
};
}

void MadModemUi::enableWindowResize(QWidget *window) {
    if (!window || !window->isWindow() || window->property("cockpitEmbeddedDialog").toBool() ||
        window->property("mmWindowResizeInstalled").toBool()) return;
    const auto type = window->windowType();
    if (type == Qt::Popup || type == Qt::ToolTip || type == Qt::SplashScreen) return;
    window->setProperty("mmWindowResizeInstalled", true);
    window->setWindowFlag(Qt::MSWindowsFixedSizeDialogHint, false);
    new WindowResize(window);
}
