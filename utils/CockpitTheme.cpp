#include "CockpitTheme.h"

#include <QApplication>
#include <QColor>
#include <QDialog>
#include <QHBoxLayout>
#include <QIcon>
#include <QComboBox>
#include <QEvent>
#include <QFrame>
#include <QGroupBox>
#include <QLabel>
#include <QLayout>
#include <QMainWindow>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPalette>
#include <QPointer>
#include <QPushButton>
#include <QStyle>
#include <QTimer>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QPlainTextEdit>
#include <QWidget>

namespace {


class CockpitTitleBar final : public QFrame
{
public:
    explicit CockpitTitleBar(QWidget *owner, bool mainWindowButtons = false)
        : QFrame(owner)
        , m_owner(owner)
        , m_mainWindowButtons(mainWindowButtons)
    {
        setObjectName(mainWindowButtons ? QStringLiteral("cockpitMainTitleBar")
                                        : QStringLiteral("cockpitTitleBar"));
        setAttribute(Qt::WA_StyledBackground, true);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setMinimumHeight(mainWindowButtons ? 24 : 24);
        setMaximumHeight(mainWindowButtons ? 27 : 28);
        if (m_owner != nullptr) {
            m_owner->installEventFilter(this);
        }

        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(7, 1, 5, 1);
        layout->setSpacing(5);


        m_title = new QLabel(owner != nullptr ? owner->windowTitle() : QString(), this);
        m_title->setObjectName(QStringLiteral("cockpitTitleLabel"));
        m_title->setAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
        m_title->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        layout->addWidget(m_title, 1);

        if (mainWindowButtons) {
            m_minimizeButton = new QPushButton(this);
            setupTitleButton(m_minimizeButton,
                             QStringLiteral("cockpitMinimizeButton"),
                             QStyle::SP_TitleBarMinButton,
                             QStringLiteral("−"),
                             QStringLiteral("Minimize"));
            layout->addWidget(m_minimizeButton, 0, Qt::AlignRight | Qt::AlignVCenter);
            QObject::connect(m_minimizeButton, &QPushButton::clicked, this, [this]() {
                if (m_owner != nullptr) {
                    m_owner->showMinimized();
                }
            });

            m_maximizeButton = new QPushButton(this);
            setupTitleButton(m_maximizeButton,
                             QStringLiteral("cockpitMaximizeButton"),
                             QStyle::SP_TitleBarMaxButton,
                             QStringLiteral("□"),
                             QStringLiteral("Maximize"));
            layout->addWidget(m_maximizeButton, 0, Qt::AlignRight | Qt::AlignVCenter);
            QObject::connect(m_maximizeButton, &QPushButton::clicked, this, [this]() {
                toggleMainWindowFullScreen();
            });
            updateMaximizeButtonIcon();
        }

        m_closeButton = new QPushButton(this);
        setupTitleButton(m_closeButton,
                         QStringLiteral("cockpitCloseButton"),
                         QStyle::SP_TitleBarCloseButton,
                         QStringLiteral("×"),
                         QStringLiteral("Close"));
        layout->addWidget(m_closeButton, 0, Qt::AlignRight | Qt::AlignVCenter);
        QObject::connect(m_closeButton, &QPushButton::clicked, this, [this]() {
            if (m_owner != nullptr) {
                m_owner->close();
            }
        });
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == m_owner) {
            if (event->type() == QEvent::WindowTitleChange && m_title != nullptr) {
                m_title->setText(m_owner != nullptr ? m_owner->windowTitle() : QString());
            } else if (event->type() == QEvent::WindowStateChange) {
                updateMaximizeButtonIcon();
            }
        }
        return QFrame::eventFilter(watched, event);
    }

    void mouseDoubleClickEvent(QMouseEvent *event) override
    {
        if (m_mainWindowButtons && event->button() == Qt::LeftButton && m_owner != nullptr) {
            toggleMainWindowFullScreen();
            event->accept();
            return;
        }
        QFrame::mouseDoubleClickEvent(event);
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && m_owner != nullptr) {
            m_dragging = true;
            m_dragOffset = event->globalPos() - m_owner->frameGeometry().topLeft();
            event->accept();
            return;
        }
        QFrame::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (m_dragging && m_owner != nullptr) {
            if (m_owner->isFullScreen()) {
                event->accept();
                return;
            }
            if (m_owner->isMaximized()) {
                m_owner->showNormal();
                m_dragOffset = QPoint(m_owner->width() / 2, height() / 2);
                updateMaximizeButtonIcon();
            }
            m_owner->move(event->globalPos() - m_dragOffset);
            event->accept();
            return;
        }
        QFrame::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            m_dragging = false;
            event->accept();
            return;
        }
        QFrame::mouseReleaseEvent(event);
    }

private:
    void setupTitleButton(QPushButton *button,
                          const QString &objectName,
                          QStyle::StandardPixmap standardIcon,
                          const QString &fallbackText,
                          const QString &toolTip)
    {
        if (button == nullptr) {
            return;
        }
        button->setObjectName(objectName);
        button->setFixedSize(25, 19);
        button->setMinimumSize(25, 19);
        button->setMaximumSize(25, 19);
        button->setFocusPolicy(Qt::NoFocus);
        button->setToolTip(toolTip);
        button->setAccessibleName(toolTip);
        button->setText(QString());
        button->setIconSize(QSize(12, 12));
        QStyle *s = (m_owner != nullptr && m_owner->style() != nullptr) ? m_owner->style() : QApplication::style();
        const QIcon icon = s != nullptr ? s->standardIcon(standardIcon, nullptr, m_owner) : QIcon();
        if (!icon.isNull()) {
            button->setIcon(icon);
        } else {
            // Fallback is intentionally narrow and the button is wide enough, so Qt
            // does not elide it to "..." on styles with large internal margins.
            button->setText(fallbackText);
        }
    }

    void setButtonIconOrFallback(QPushButton *button,
                                 QStyle::StandardPixmap standardIcon,
                                 const QString &fallbackText,
                                 const QString &toolTip)
    {
        if (button == nullptr) {
            return;
        }
        button->setToolTip(toolTip);
        button->setAccessibleName(toolTip);
        button->setText(QString());
        button->setIcon(QIcon());
        QStyle *s = (m_owner != nullptr && m_owner->style() != nullptr) ? m_owner->style() : QApplication::style();
        const QIcon icon = s != nullptr ? s->standardIcon(standardIcon, nullptr, m_owner) : QIcon();
        if (!icon.isNull()) {
            button->setIcon(icon);
        } else {
            button->setText(fallbackText);
        }
    }

    void updateMaximizeButtonIcon()
    {
        if (m_maximizeButton == nullptr || m_owner == nullptr) {
            return;
        }
        if (m_owner->isFullScreen() || m_owner->isMaximized()) {
            setButtonIconOrFallback(m_maximizeButton,
                                    QStyle::SP_TitleBarNormalButton,
                                    QStringLiteral("▣"),
                                    QStringLiteral("Restore"));
        } else {
            setButtonIconOrFallback(m_maximizeButton,
                                    QStyle::SP_TitleBarMaxButton,
                                    QStringLiteral("□"),
                                    QStringLiteral("Maximize"));
        }
    }

    void toggleMainWindowFullScreen()
    {
        if (m_owner == nullptr) {
            return;
        }
        if (m_owner->isFullScreen() || m_owner->isMaximized()) {
            m_owner->showNormal();
        } else {
            m_owner->setWindowState((m_owner->windowState() & ~Qt::WindowMinimized) | Qt::WindowFullScreen);
            m_owner->showFullScreen();
        }
        QTimer::singleShot(0, this, [this]() { updateMaximizeButtonIcon(); });
    }

    QWidget *m_owner = nullptr;
    QLabel *m_title = nullptr;
    QPushButton *m_minimizeButton = nullptr;
    QPushButton *m_maximizeButton = nullptr;
    QPushButton *m_closeButton = nullptr;
    QPoint m_dragOffset;
    bool m_dragging = false;
    bool m_mainWindowButtons = false;
};

// Lightweight cockpit bezel overlay.
// The first 0.5.53 pass installed this on every QGroupBox/QFrame/QTabWidget;
// that looked heavy in real layouts because screw heads stacked on nested panels
// and consumed visual space.  From 0.5.55 overlays are opt-in only via the
// dynamic property cockpitInstrumentFrame=true.  Normal panels keep the cockpit
// feel through QSS borders without extra widgets or screw clutter.
class CockpitBezelOverlay final : public QWidget
{
public:
    explicit CockpitBezelOverlay(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("cockpitBezelOverlay"));
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
        setAttribute(Qt::WA_TranslucentBackground, true);
        setFocusPolicy(Qt::NoFocus);
        if (parent != nullptr) {
            setGeometry(parent->rect());
            parent->installEventFilter(this);
        }
        raise();
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        QWidget *parentWidget = qobject_cast<QWidget *>(watched);
        if (parentWidget == parentWidgetPointer() && parentWidget != nullptr) {
            if (event->type() == QEvent::Resize || event->type() == QEvent::Show) {
                setGeometry(parentWidget->rect());
                raise();
            }
        }
        return QWidget::eventFilter(watched, event);
    }

    void paintEvent(QPaintEvent *) override
    {
        if (width() < 72 || height() < 56) {
            return;
        }

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const QRectF r = rect().adjusted(2.0, 2.0, -2.0, -2.0);

        QLinearGradient rim(r.topLeft(), r.bottomRight());
        rim.setColorAt(0.0, QColor(105, 88, 68, 125));
        rim.setColorAt(0.50, QColor(18, 18, 18, 80));
        rim.setColorAt(1.0, QColor(112, 70, 32, 105));
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(rim, 1.0));
        p.drawRoundedRect(r, 7, 7);

        // Window/dialog bezel screws removed in 0.5.68: on real layouts they
        // overlapped title bars, tabs and inner borders.  Keep the aircraft-style
        // metal rim only; dedicated instrument widgets may still paint their own
        // internal screws where they do not collide with controls.
    }

private:
    QWidget *parentWidgetPointer() const
    {
        return qobject_cast<QWidget *>(parent());
    }
};

bool isGoodBezelTarget(QWidget *w)
{
    if (w == nullptr) return false;
    if (w->objectName() == QStringLiteral("cockpitBezelOverlay")) return false;
    return w->property("cockpitInstrumentFrame").toBool();
}

void decorate(QWidget *w)
{
    if (!isGoodBezelTarget(w)) return;
    if (w->property("cockpitBezelInstalled").toBool()) return;
    w->setProperty("cockpitBezelInstalled", true);
    auto *overlay = new CockpitBezelOverlay(w);
    overlay->show();
    overlay->raise();
}

void installDialogChrome(QDialog *dlg)
{
    if (dlg == nullptr || dlg->property("cockpitDialogChromeInstalled").toBool()) {
        return;
    }

    // Only real top-level dialogs get the cockpit title bar and close button.
    // Several settings pages reuse QDialog subclasses as embedded widgets inside
    // AppSettingsDialog.  Decorating those child dialogs produced the bogus red
    // close/ellipsis buttons that looked like a group "minimize" control with no
    // way to restore the content.  Embedded dialogs are plain page contents.
    if (dlg->property("cockpitEmbeddedDialog").toBool() || !dlg->isWindow()) {
        return;
    }

    // QMessageBox uses an internal grid layout and was the last place where the
    // native grey window manager title bar leaked through.  Do not try to insert
    // the title widget into that private layout; make it a compact frameless
    // cockpit panel and let its own icon/text/buttons remain intact.
    if (qobject_cast<QMessageBox *>(dlg) != nullptr) {
        dlg->setProperty("cockpitDialogChromeInstalled", true);
        dlg->setProperty("cockpitDialog", true);
        dlg->setProperty("cockpitMessageBox", true);
        dlg->setAttribute(Qt::WA_StyledBackground, true);
        dlg->setWindowFlag(Qt::FramelessWindowHint, true);
        dlg->setWindowFlag(Qt::WindowSystemMenuHint, true);
        if (QLayout *layout = dlg->layout()) {
            layout->setContentsMargins(18, 16, 18, 16);
            layout->setSpacing(10);
        }
        decorate(dlg);
        return;
    }

    QLayout *layout = dlg->layout();
    auto *box = qobject_cast<QBoxLayout *>(layout);
    if (box == nullptr) {
        return;
    }

    dlg->setProperty("cockpitDialogChromeInstalled", true);
    dlg->setProperty("cockpitDialog", true);
    dlg->setProperty("cockpitInstrumentFrame", true);
    dlg->setAttribute(Qt::WA_StyledBackground, true);
    dlg->setWindowFlag(Qt::FramelessWindowHint, true);
    dlg->setWindowFlag(Qt::WindowSystemMenuHint, true);

    box->setContentsMargins(8, 8, 8, 8);
    box->setSpacing(6);
    auto *titleBar = new CockpitTitleBar(dlg);
    box->insertWidget(0, titleBar);
    decorate(dlg);
}

class CockpitApplicationFilter final : public QObject
{
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::Polish || event->type() == QEvent::Show) {
            if (auto *dlg = qobject_cast<QDialog *>(watched)) {
                if (!dlg->testAttribute(Qt::WA_DontShowOnScreen)) {
                    const bool wasVisible = dlg->isVisible();
                    installDialogChrome(dlg);
                    if (wasVisible && (dlg->windowFlags() & Qt::FramelessWindowHint)) {
                        dlg->show();
                    }
                    MadModemUi::polishCockpitWidgetTree(dlg);
                }
            }
        }
        return QObject::eventFilter(watched, event);
    }
};

} // namespace

namespace MadModemUi {

void applyCockpitTheme(QApplication &app)
{
    QPalette pal;
    pal.setColor(QPalette::Window, QColor(7, 7, 7));
    pal.setColor(QPalette::WindowText, QColor(255, 177, 62));
    pal.setColor(QPalette::Base, QColor(2, 2, 2));
    pal.setColor(QPalette::AlternateBase, QColor(13, 13, 13));
    pal.setColor(QPalette::ToolTipBase, QColor(8, 8, 8));
    pal.setColor(QPalette::ToolTipText, QColor(255, 197, 91));
    pal.setColor(QPalette::Text, QColor(255, 188, 72));
    pal.setColor(QPalette::Button, QColor(16, 16, 16));
    pal.setColor(QPalette::ButtonText, QColor(255, 178, 64));
    pal.setColor(QPalette::BrightText, QColor(255, 80, 60));
    pal.setColor(QPalette::Highlight, QColor(255, 148, 22));
    pal.setColor(QPalette::HighlightedText, QColor(5, 5, 5));
    pal.setColor(QPalette::Disabled, QPalette::WindowText, QColor(102, 94, 82));
    pal.setColor(QPalette::Disabled, QPalette::Text, QColor(102, 94, 82));
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(102, 94, 82));
    app.setPalette(pal);

    static CockpitApplicationFilter *cockpitDialogFilter = nullptr;
    if (cockpitDialogFilter == nullptr) {
        cockpitDialogFilter = new CockpitApplicationFilter(&app);
        app.installEventFilter(cockpitDialogFilter);
    }

    app.setStyleSheet(QString::fromLatin1(R"QSS(
/* MadModem cockpit / avionics theme.  Embedded in code for Linux and MXE static builds. */
* {
    selection-background-color: #ff9a20;
    selection-color: #050505;
}
QWidget {
    background-color: #070707;
    color: #ffb347;
    font-family: "DejaVu Sans", "Liberation Sans", "Segoe UI", sans-serif;
    font-size: 8.6pt;
}
QMainWindow, QDialog {
    background-color: #060606;
}
QMainWindow[cockpitMainWindow="true"] {
    background: #030303;
    border: 2px solid #5d4630;
    border-radius: 10px;
}
QFrame#cockpitMainHeader {
    background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #151515, stop:0.50 #070707, stop:1 #020202);
    border: 1px solid #5d4630;
    border-radius: 6px;
}
QFrame#cockpitMainTitleBar {
    background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #181818, stop:0.50 #090909, stop:1 #020202);
    border: 1px solid #6f4b25;
    border-radius: 5px;
}
QDialog[cockpitDialog="true"] {
    background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #111111, stop:0.08 #050505, stop:1 #020202);
    border: 2px solid #5d4630;
    border-radius: 10px;
}
QMessageBox[cockpitMessageBox="true"] {
    background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #14110d, stop:0.12 #070707, stop:1 #020202);
    border: 2px solid #7a4d20;
    border-radius: 11px;
}
QMessageBox[cockpitMessageBox="true"] QLabel {
    color: #ffbd58;
    background: transparent;
    font-size: 9.0pt;
}
QMessageBox[cockpitMessageBox="true"] QPushButton {
    min-width: 128px;
    min-height: 24px;
}
QFrame#cockpitTitleBar {
    background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #181818, stop:0.50 #090909, stop:1 #020202);
    border: 1px solid #6f4b25;
    border-radius: 7px;
}
QLabel#cockpitTitleLabel {
    color: #ffbd55;
    font-size: 8.5pt;
    font-weight: 500;
    letter-spacing: 0.2px;
}
QPushButton#cockpitMinimizeButton, QPushButton#cockpitMaximizeButton {
    color: #ffc261;
    min-width: 25px;
    max-width: 25px;
    min-height: 19px;
    max-height: 19px;
    background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #1f1f1f, stop:1 #050505);
    border: 1px solid #6a401e;
    border-radius: 4px;
    padding: 0px;
    font-size: 8pt;
}
QPushButton#cockpitMinimizeButton:hover, QPushButton#cockpitMaximizeButton:hover {
    background: #2a1a09;
    border-color: #ff9a25;
}
QPushButton#cockpitCloseButton {
    color: #ffebe0;
    min-width: 25px;
    max-width: 25px;
    min-height: 19px;
    max-height: 19px;
    background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #7f2018, stop:1 #220403);
    border: 1px solid #d15a35;
    border-radius: 8px;
    padding: 0px;
    font-size: 9pt;
}
QPushButton#cockpitCloseButton:hover {
    background: #a42b20;
    border-color: #ff8a5a;
}
QMenuBar {
    background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #1a1a1a, stop:0.45 #0a0a0a, stop:1 #030303);
    color: #ffad38;
    border: 1px solid #2f271f;
    border-radius: 4px;
    spacing: 6px;
    padding: 1px 5px;
}
QMenuBar::item { background: transparent; padding: 2px 7px; border-radius: 3px; font-weight: 400; }
QMenuBar::item:selected { color: #ffe08a; background: #251608; border: 1px solid #8c521d; }
QMenu { background-color: #080808; color: #ffc06b; border: 1px solid #6a401e; padding: 5px; }
QMenu::item { padding: 5px 24px 5px 20px; }
QMenu::item:selected { background-color: #3a210b; color: #ffe0a0; }
QFrame {
    background: transparent;
    border: none;
}

QFrame[frameShape="4"], QFrame[frameShape="5"], QFrame[frameShape="HLine"], QFrame[frameShape="VLine"] {
    border: none;
    background: #241b13;
    max-height: 1px;
    max-width: 1px;
}
QGroupBox, QTabWidget::pane {
    background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #111111, stop:0.12 #090909, stop:1 #030303);
    border: 1px solid #3b3026;
    border-radius: 7px;
}
QFrame[frameShape="1"], QFrame[frameShape="2"], QFrame[frameShape="6"] {
    background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #101010, stop:1 #030303);
    border: 1px solid #302820;
    border-radius: 6px;
}
QGroupBox {
    margin-top: 9px;
    padding: 4px 5px 5px 5px;
    color: #ffae35;
    font-weight: 500;
}
QGroupBox[cockpitUntitled="true"] {
    margin-top: 2px;
    padding: 3px;
}
QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    left: 10px;
    padding: 0px 7px;
    color: #ffbd56;
    background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #0b0b0b, stop:1 #1b1006);
    border: 1px solid #70441d;
    border-radius: 5px;
}
QTabWidget::pane { top: -1px; padding: 1px; }
QTabBar::tab {
    background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #1f1f1f, stop:1 #070707);
    color: #cfa268;
    border: 1px solid #453727;
    border-bottom: 0px;
    border-top-left-radius: 8px;
    border-top-right-radius: 8px;
    min-width: 62px;
    padding: 4px 9px;
    margin-right: 1px;
    font-weight: 500;
}
QTabBar::tab:selected {
    color: #ffbf55;
    background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #342009, stop:1 #111111);
    border-color: #a86626;
}
QTabBar::tab:!selected { color: #9d8c78; }
QPushButton, QToolButton {
    background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #252525, stop:0.45 #101010, stop:1 #050505);
    color: #ffb448;
    border: 1px solid #514331;
    border-radius: 6px;
    padding: 3px 7px;
    font-weight: 400;
}
QPushButton:hover, QToolButton:hover {
    border-color: #d1802d;
    color: #ffd589;
    background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #2d1c0a, stop:1 #0b0b0b);
}
QPushButton:pressed, QToolButton:pressed { background: #3a2108; border-color: #ff9b28; }
QPushButton:disabled, QToolButton:disabled { color: #6e6254; border-color: #2d2822; background: #0d0d0d; }
QLineEdit, QTextEdit, QPlainTextEdit, QSpinBox, QDoubleSpinBox, QComboBox, QDateTimeEdit {
    background-color: #020202;
    color: #ffb347;
    border: 1px solid #493a2b;
    border-radius: 5px;
    padding: 2px 4px;
    selection-background-color: #ff9a20;
    selection-color: #050505;
}
QTextEdit, QPlainTextEdit { font-family: "DejaVu Sans Mono", "Liberation Mono", "Consolas", monospace; }
QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus, QDateTimeEdit:focus {
    border-color: #d88631;
    color: #ffd48a;
}
QComboBox::drop-down {
    background-color: #17110b;
    border-left: 1px solid #463728;
    width: 18px;
    subcontrol-origin: padding;
    subcontrol-position: top right;
    border-top-right-radius: 4px;
    border-bottom-right-radius: 4px;
}
QComboBox::down-arrow {
    image: none;
    width: 0px;
    height: 0px;
    border-left: 5px solid transparent;
    border-right: 5px solid transparent;
    border-top: 6px solid #ffb347;
    margin-right: 4px;
}
QSpinBox::up-button, QSpinBox::down-button,
QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {
    background-color: #17110b;
    border-left: 1px solid #463728;
    width: 18px;
}
QSpinBox::up-arrow, QDoubleSpinBox::up-arrow {
    image: none;
    width: 0px;
    height: 0px;
    border-left: 4px solid transparent;
    border-right: 4px solid transparent;
    border-bottom: 5px solid #ffb347;
}
QSpinBox::down-arrow, QDoubleSpinBox::down-arrow {
    image: none;
    width: 0px;
    height: 0px;
    border-left: 4px solid transparent;
    border-right: 4px solid transparent;
    border-top: 5px solid #ffb347;
}
QCheckBox, QRadioButton { color: #ffb347; spacing: 5px; font-weight: 400; }
QCheckBox::indicator, QRadioButton::indicator { width: 13px; height: 13px; border: 1px solid #60472c; background: #030303; border-radius: 3px; }
QCheckBox::indicator:checked:enabled {
    background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #5dff78, stop:1 #1fd845);
    border-color: #b8ffc5;
}
QCheckBox::indicator:disabled {
    background: #050505;
    border-color: #3a3025;
}
QCheckBox::indicator:checked:disabled {
    background: #151515;
    border-color: #504432;
}
QRadioButton::indicator:checked:enabled { background: #32ff5c; border-color: #b2ffc2; }
QRadioButton::indicator:disabled { background: #050505; border-color: #3a3025; }
QRadioButton::indicator:checked:disabled { background: #151515; border-color: #504432; }
QLabel { background: transparent; color: #ffb347; }
QLCDNumber { background-color: #020202; color: #ff9b24; border: 1px solid #53351c; border-radius: 6px; }
QProgressBar { background-color: #020202; color: #ffe0a0; border: 1px solid #463728; border-radius: 6px; text-align: center; min-height: 18px; }
QProgressBar::chunk { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #78420f, stop:0.55 #ff9d24, stop:1 #ffdb77); border-radius: 4px; }
QHeaderView::section { background-color: #111111; color: #ffb347; border: 1px solid #3e3328; padding: 3px; font-weight: 500; }
QTableWidget, QTreeWidget, QListWidget { background-color: #020202; alternate-background-color: #0d0d0d; color: #ffb347; gridline-color: #3e3328; border: 1px solid #3e3328; border-radius: 6px; }
QScrollBar:vertical, QScrollBar:horizontal { background: #050505; border: 1px solid #29251f; width: 13px; height: 13px; }
QScrollBar::handle:vertical, QScrollBar::handle:horizontal { background: #62411e; border: 1px solid #a06223; border-radius: 5px; min-height: 24px; min-width: 24px; }
QScrollBar::add-line, QScrollBar::sub-line { background: #111111; border: 1px solid #2d2822; }
QSplitter::handle { background: transparent; border: none; }
QSplitter::handle:horizontal { width: 1px; }
QSplitter::handle:vertical { height: 1px; }
QToolTip { background-color: #050505; color: #ffd182; border: 1px solid #8e5a27; padding: 4px; }
QStatusBar { background: #050505; color: #ffb347; border-top: 1px solid #3e3328; }
QLabel[role="rx"], QPushButton[role="rx"] { color: #53ff70; }
QLabel[role="tx"], QPushButton[role="tx"] { color: #ff5656; }
QPushButton[txActive="true"] { color: #ff5757; border-color: #b33a32; }
QPushButton[rxActive="true"] { color: #5cff78; border-color: #2d8f42; }

/* Density pass 0.5.56: readable cockpit, no oversized bold chrome. */
QLabel, QCheckBox, QRadioButton, QTabBar::tab, QPushButton, QToolButton, QGroupBox {
    font-weight: 400;
}
QGroupBox::title {
    font-weight: 500;
    font-size: 8.8pt;
}
QTabBar::tab:selected {
    font-weight: 400;
}
QTabBar::tab:!selected {
    font-weight: 400;
}

/* 0.5.57: cockpit density repair from real screenshots. */
QWidget[cockpitCompactPanel="true"] QGroupBox {
    margin-top: 6px;
    padding: 3px 4px 4px 4px;
}
QWidget[cockpitCompactPanel="true"] QPushButton {
    min-height: 22px;
    padding-top: 2px;
    padding-bottom: 2px;
}
QComboBox, QSpinBox, QDoubleSpinBox {
    min-height: 22px;
}
QTabBar::tab {
    font-size: 8.6pt;
    min-width: 92px;
    padding-left: 10px;
    padding-right: 10px;
}

/* 0.5.58: Waterfall is the instrument surface.  Do not wrap the GL widget in
   nested chrome that creates a fake bottom margin or double side rails. */
QGroupBox#grpWaterfall {
    margin-top: 1px;
    padding: 0px;
    border: none;
    border-radius: 0px;
    background: transparent;
}
QGroupBox#grpWaterfall::title {
    height: 0px;
    width: 0px;
    margin: 0px;
    padding: 0px;
    border: none;
    background: transparent;
}
QFrame#frameWaterfall {
    margin: 0px;
    padding: 0px;
    border: 1px solid #3a4548;
    border-radius: 0px;
    background: #020608;
}
QFrame#frameWaterfall > QWidget {
    margin: 0px;
    padding: 0px;
    border: none;
}

)QSS"));
}


void installCockpitMainWindowChrome(QMainWindow *window)
{
    if (window == nullptr || window->property("cockpitMainChromeInstalled").toBool()) {
        return;
    }

    window->setProperty("cockpitMainChromeInstalled", true);
    window->setProperty("cockpitMainWindow", true);
    window->setProperty("cockpitInstrumentFrame", true);
    window->setAttribute(Qt::WA_StyledBackground, true);
    window->setWindowFlag(Qt::FramelessWindowHint, true);
    window->setWindowFlag(Qt::WindowSystemMenuHint, true);
    window->setWindowFlag(Qt::WindowMinMaxButtonsHint, true);

    QMenuBar *bar = window->menuBar();
    auto *header = new QFrame(window);
    header->setObjectName(QStringLiteral("cockpitMainHeader"));
    header->setAttribute(Qt::WA_StyledBackground, true);
    header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto *layout = new QVBoxLayout(header);
    layout->setContentsMargins(2, 2, 2, 1);
    layout->setSpacing(1);
    auto *titleBar = new CockpitTitleBar(window, true);
    layout->addWidget(titleBar);
    if (bar != nullptr) {
        bar->setParent(header);
        layout->addWidget(bar);
    }
    window->setMenuWidget(header);
    decorate(window);
    polishCockpitWidgetTree(window);
}

void polishCockpitWidgetTree(QWidget *root)
{
    if (root == nullptr) {
        return;
    }
    root->setProperty("cockpitRoot", true);

    QList<QWidget *> widgets;
    widgets << root;
    widgets << root->findChildren<QWidget *>();
    for (QWidget *w : widgets) {
        if (w == nullptr) continue;
        if (qobject_cast<QGroupBox *>(w) != nullptr) {
            w->setProperty("cockpitPanel", true);
            if (auto *gb = qobject_cast<QGroupBox *>(w)) {
                if (gb->title().trimmed().isEmpty()) {
                    w->setProperty("cockpitUntitled", true);
                }
            }
            decorate(w);
        } else if (qobject_cast<QFrame *>(w) != nullptr || qobject_cast<QTabWidget *>(w) != nullptr) {
            decorate(w);
        }
        if (auto *tb = qobject_cast<QTextEdit *>(w)) {
            tb->setProperty("cockpitTerminal", true);
        }
        if (auto *pb = qobject_cast<QPlainTextEdit *>(w)) {
            pb->setProperty("cockpitTerminal", true);
        }
    }
}

} // namespace MadModemUi
