#include "CockpitTheme.h"

#include <QApplication>
#include <QAbstractItemView>
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
#include <QMap>
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

QString g_activeThemeKey = QStringLiteral("avionica");

struct ThemeSpec
{
    QColor window;
    QColor windowText;
    QColor base;
    QColor alternateBase;
    QColor button;
    QColor buttonText;
    QColor border;
    QColor darkBorder;
    QColor highlight;
    QColor highlightedText;
    QColor disabledText;
    QColor tooltipBase;
    QColor tooltipText;
    QColor accent;
    QColor accentText;
    QColor positive;
    QColor negative;
    QColor warning;
    QColor mutedText;
    QColor rxPrimary;
    QColor rxSecondary;
    QColor mapBackdrop;
    QColor mapText;
    QColor mapMutedText;
    QColor mapBorder;
    bool highContrast = false;
};

ThemeSpec themeSpec(const QString &requestedKey)
{
    const QString key = requestedKey.trimmed().toLower();
    if (key == QStringLiteral("qt_default")) {
        return {
            QColor("#F0F0F0"), QColor("#202020"), QColor("#FFFFFF"), QColor("#F7F7F7"),
            QColor("#E7E7E7"), QColor("#202020"), QColor("#9A9A9A"), QColor("#666666"),
            QColor("#2B579A"), QColor("#FFFFFF"), QColor("#777777"), QColor("#FFFFDC"),
            QColor("#101010"), QColor("#2B579A"), QColor("#FFFFFF"), QColor("#14743A"),
            QColor("#B4232F"), QColor("#9A5B00"), QColor("#5E6670"), QColor("#08783E"),
            QColor("#1E63A8"), QColor("#E8EDF2"), QColor("#18212B"), QColor("#556270"),
            QColor("#8794A2"), false
        };
    }
    if (key == QStringLiteral("hacker_green")) {
        return {
            QColor("#001008"), QColor("#71FF91"), QColor("#000603"), QColor("#001A0D"),
            QColor("#002313"), QColor("#83FFA0"), QColor("#167A3A"), QColor("#0B5127"),
            QColor("#2BEA65"), QColor("#001006"), QColor("#3F8453"), QColor("#001A0D"),
            QColor("#B5FFC5"), QColor("#2BEA65"), QColor("#001006"), QColor("#45F47A"),
            QColor("#FF6675"), QColor("#FFD45C"), QColor("#68A879"), QColor("#45F47A"),
            QColor("#58D7FF"), QColor("#00150B"), QColor("#9DFFB2"), QColor("#68A879"),
            QColor("#167A3A"), false
        };
    }
    if (key == QStringLiteral("classic_dark")) {
        return {
            QColor("#24272D"), QColor("#E8EBEF"), QColor("#15171B"), QColor("#2B2E35"),
            QColor("#343840"), QColor("#F0F2F5"), QColor("#626A76"), QColor("#404650"),
            QColor("#6FA8FF"), QColor("#0E1622"), QColor("#7D838C"), QColor("#30343B"),
            QColor("#FFFFFF"), QColor("#6FA8FF"), QColor("#0E1622"), QColor("#61D68A"),
            QColor("#FF737D"), QColor("#FFC45C"), QColor("#A9B0BA"), QColor("#61D68A"),
            QColor("#66B8FF"), QColor("#1C222A"), QColor("#EDF3F8"), QColor("#A9B0BA"),
            QColor("#596573"), false
        };
    }
    if (key == QStringLiteral("high_contrast")) {
        return {
            QColor("#000000"), QColor("#FFFFFF"), QColor("#000000"), QColor("#161616"),
            QColor("#090909"), QColor("#FFFFFF"), QColor("#FFFFFF"), QColor("#FFFFFF"),
            QColor("#FFD800"), QColor("#000000"), QColor("#AFAFAF"), QColor("#000000"),
            QColor("#FFFFFF"), QColor("#FFD800"), QColor("#000000"), QColor("#57FF76"),
            QColor("#FF6B78"), QColor("#FFD800"), QColor("#D0D0D0"), QColor("#57FF76"),
            QColor("#57C7FF"), QColor("#000000"), QColor("#FFFFFF"), QColor("#D0D0D0"),
            QColor("#FFFFFF"), true
        };
    }

    return {
        QColor("#070707"), QColor("#FFB13E"), QColor("#020202"), QColor("#0D0D0D"),
        QColor("#101010"), QColor("#FFB240"), QColor("#514331"), QColor("#302820"),
        QColor("#FF9416"), QColor("#050505"), QColor("#665E52"), QColor("#080808"),
        QColor("#FFC55B"), QColor("#FF9A20"), QColor("#050505"), QColor("#53FF70"),
        QColor("#FF6262"), QColor("#FFC35C"), QColor("#9D8C78"), QColor("#53FF70"),
        QColor("#55AAFF"), QColor("#050607"), QColor("#FFB35A"), QColor("#8394A4"),
        QColor("#323C46"), false
    };
}

QPalette paletteForTheme(const ThemeSpec &spec)
{
    QPalette pal;
    pal.setColor(QPalette::Window, spec.window);
    pal.setColor(QPalette::WindowText, spec.windowText);
    pal.setColor(QPalette::Base, spec.base);
    pal.setColor(QPalette::AlternateBase, spec.alternateBase);
    pal.setColor(QPalette::ToolTipBase, spec.tooltipBase);
    pal.setColor(QPalette::ToolTipText, spec.tooltipText);
    pal.setColor(QPalette::Text, spec.windowText);
    pal.setColor(QPalette::Button, spec.button);
    pal.setColor(QPalette::ButtonText, spec.buttonText);
    pal.setColor(QPalette::BrightText, spec.negative);
    pal.setColor(QPalette::Highlight, spec.highlight);
    pal.setColor(QPalette::HighlightedText, spec.highlightedText);
    pal.setColor(QPalette::Light, spec.window.lighter(118));
    pal.setColor(QPalette::Midlight, spec.window.lighter(108));
    pal.setColor(QPalette::Mid, spec.border);
    pal.setColor(QPalette::Dark, spec.darkBorder);
    pal.setColor(QPalette::Shadow, spec.darkBorder.darker(135));
    pal.setColor(QPalette::Disabled, QPalette::WindowText, spec.disabledText);
    pal.setColor(QPalette::Disabled, QPalette::Text, spec.disabledText);
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, spec.disabledText);
    pal.setColor(QPalette::Disabled, QPalette::Highlight, spec.border);
    pal.setColor(QPalette::Disabled, QPalette::HighlightedText, spec.disabledText);
    return pal;
}

QString genericThemeStyleSheet(const ThemeSpec &spec)
{
    QString qss = QString::fromLatin1(R"QSS(
* { selection-background-color: @HIGHLIGHT@; selection-color: @HIGHLIGHT_TEXT@; }
QWidget { color: palette(window-text); }
QMainWindow, QDialog { background: palette(window); }
QMainWindow[cockpitMainWindow="true"] { background: palette(window); border:none; }
QDialog[cockpitDialog="true"], QMessageBox[cockpitMessageBox="true"] { background: palette(window); border:none; }
QFrame#cockpitMainHeader { background:palette(button); border:none; border-radius:0; }
QFrame#cockpitMainTitleBar, QFrame#cockpitTitleBar {
    background:palette(button); border:1px solid palette(mid); border-radius:4px;
}
QLabel#cockpitTitleLabel { color: palette(button-text); font-weight: 600; background: transparent; }
QPushButton#cockpitMinimizeButton, QPushButton#cockpitMaximizeButton, QPushButton#cockpitCloseButton {
    min-width:25px; max-width:25px; min-height:19px; max-height:19px; padding:0px;
}
QPushButton#cockpitCloseButton { color: #FFFFFF; background: @CLOSE@; border-color: @CLOSE_BORDER@; }
QMenuBar { background:palette(button); color:palette(button-text); border:none; border-top:1px solid palette(mid); padding:1px 4px; }
QMenuBar::item { background: transparent; padding: 3px 8px; }
QMenuBar::item:selected { background: palette(highlight); color: palette(highlighted-text); }
QMenu { background: palette(base); color: palette(text); border: 1px solid palette(mid); padding: 4px; }
QMenu::item { padding: 5px 24px 5px 20px; }
QMenu::item:selected { background: palette(highlight); color: palette(highlighted-text); }
QMenu::item:disabled { color: @DISABLED@; }
QMenu::separator { height:1px; background:palette(mid); margin:4px 8px; }
QFrame { background: transparent; border: none; }
QFrame[frameShape="1"], QFrame[frameShape="2"], QFrame[frameShape="6"] { background: palette(window); border: 1px solid palette(mid); border-radius: 5px; }
QFrame[frameShape="4"], QFrame[frameShape="5"], QFrame[frameShape="HLine"], QFrame[frameShape="VLine"] { border:none; background:palette(mid); max-height:1px; max-width:1px; }
QGroupBox { border: 1px solid palette(mid); border-radius: 5px; margin-top: 9px; padding: 5px; background: palette(window); }
QGroupBox::title { subcontrol-origin:margin; left:8px; padding:0 5px; color:palette(window-text); background:palette(window); }
QTabWidget::pane { border: 1px solid palette(mid); background: palette(window); top:-1px; }
QTabBar::tab { background:palette(button); color:palette(button-text); border:1px solid palette(mid); border-bottom:0; padding:5px 7px; min-width:72px; margin-right:1px; }
QTabBar::tab:selected { background:palette(base); color:palette(text); border-color:@ACCENT@; }
QTabBar::tab:!selected { color:@MUTED@; }
QPushButton, QToolButton { background:palette(button); color:palette(button-text); border:1px solid palette(mid); border-radius:4px; padding:4px 8px; }
QPushButton:hover, QToolButton:hover { border-color:@ACCENT@; background:@HOVER@; }
QPushButton:pressed, QToolButton:pressed { background:palette(highlight); color:palette(highlighted-text); }
QPushButton:disabled, QToolButton:disabled { color:@DISABLED@; border-color:palette(mid); }
QLineEdit, QTextEdit, QPlainTextEdit, QSpinBox, QDoubleSpinBox, QComboBox, QDateTimeEdit, QTimeEdit, QDateEdit {
    background:palette(base); color:palette(text); border:1px solid palette(mid); border-radius:4px; padding:3px 5px;
}
QTextEdit, QPlainTextEdit { font-family:"DejaVu Sans Mono", "Liberation Mono", "Consolas", monospace; }
QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus, QDateTimeEdit:focus { border-color:@ACCENT@; }
QComboBox::drop-down, QSpinBox::up-button, QSpinBox::down-button, QDoubleSpinBox::up-button, QDoubleSpinBox::down-button { background:palette(button); border-left:1px solid palette(mid); width:18px; }
QCheckBox, QRadioButton { color:palette(window-text); spacing:5px; }
QCheckBox::indicator, QRadioButton::indicator { width:13px; height:13px; border:1px solid palette(mid); background:palette(base); }
QCheckBox::indicator { border-radius:3px; }
QRadioButton::indicator { border-radius:7px; }
QCheckBox::indicator:checked, QRadioButton::indicator:checked { background:@POSITIVE@; border-color:@POSITIVE@; }
QLabel { background:transparent; color:palette(window-text); }
QLCDNumber { background:palette(base); color:@ACCENT@; border:1px solid palette(mid); border-radius:5px; }
QProgressBar { background:palette(base); color:palette(text); border:1px solid palette(mid); border-radius:5px; text-align:center; min-height:18px; }
QProgressBar::chunk { background:palette(highlight); border-radius:3px; }
QSlider::groove:horizontal { height:6px; background:palette(base); border:1px solid palette(mid); border-radius:3px; }
QSlider::handle:horizontal { width:14px; margin:-5px 0; background:@ACCENT@; border:1px solid palette(dark); border-radius:7px; }
QSlider::groove:vertical { width:6px; background:palette(base); border:1px solid palette(mid); border-radius:3px; }
QSlider::handle:vertical { height:14px; margin:0 -5px; background:@ACCENT@; border:1px solid palette(dark); border-radius:7px; }
QHeaderView::section { background:palette(button); color:palette(button-text); border:1px solid palette(mid); padding:3px; }
QTableWidget, QTreeWidget, QListWidget, QAbstractItemView { background:palette(base); alternate-background-color:palette(alternate-base); color:palette(text); gridline-color:palette(mid); border:1px solid palette(mid); }
QAbstractItemView::item:selected { background:palette(highlight); color:palette(highlighted-text); }
QScrollArea { background:palette(window); }
QScrollArea > QWidget > QWidget { background:palette(window); }
QScrollBar:vertical, QScrollBar:horizontal { background:palette(window); border:1px solid palette(mid); width:13px; height:13px; }
QScrollBar::handle:vertical, QScrollBar::handle:horizontal { background:@SCROLL_HANDLE@; border:1px solid palette(dark); border-radius:4px; min-height:24px; min-width:24px; }
QScrollBar::add-line, QScrollBar::sub-line { background:palette(button); border:1px solid palette(mid); }
QSplitter::handle { background:palette(mid); }
QSplitter::handle:horizontal { width:1px; } QSplitter::handle:vertical { height:1px; }
QToolTip { background:palette(tool-tip-base); color:palette(tool-tip-text); border:1px solid palette(mid); padding:4px; }
QStatusBar { background:palette(window); color:palette(window-text); border-top:1px solid palette(mid); }
QWidget[mmRole="positive"] { color:@POSITIVE@; }
QWidget[mmRole="negative"] { color:@NEGATIVE@; }
QWidget[mmRole="warning"] { color:@WARNING@; }
QWidget[mmRole="muted"] { color:@MUTED@; }
QWidget[mmRole="accent"] { color:@ACCENT@; }
QLabel[mmRole="rxPrimary"] { color:@RX_PRIMARY@; }
QLabel[mmRole="rxSecondary"] { color:@RX_SECONDARY@; }
QLabel[ftBannerState="idle"] { border:2px solid @MAP_BORDER@; border-radius:6px; padding:6px; background:@BANNER_IDLE@; color:@BANNER_TEXT@; font-weight:500; font-size:10pt; }
QLabel[ftBannerState="tx"] { border:2px solid @NEGATIVE@; border-radius:6px; padding:6px; background:@BANNER_TX@; color:@BANNER_TEXT@; font-weight:500; font-size:10pt; }
QLabel[ftBannerState="armed"] { border:2px solid @WARNING@; border-radius:6px; padding:6px; background:@BANNER_WARNING@; color:@BANNER_TEXT@; font-weight:500; font-size:10pt; }
QLabel[ftBannerState="ready"], QLabel[ftBannerState="monitor"] { border:2px solid @POSITIVE@; border-radius:6px; padding:6px; background:@BANNER_READY@; color:@BANNER_TEXT@; font-weight:500; font-size:10pt; }
QGroupBox#grpWaterfall { margin-top:1px; padding:0; border:none; background:transparent; }
QGroupBox#grpWaterfall::title { height:0; width:0; margin:0; padding:0; border:none; background:transparent; }
QFrame#frameWaterfall { margin:0; padding:0; border:1px solid @MAP_BORDER@; border-radius:0; background:#020608; }
QFrame#frameWaterfall > QWidget { margin:0; padding:0; border:none; }
)QSS");

    const QColor hover = spec.button.lighter(spec.highContrast ? 125 : 112);
    const QColor close = spec.highContrast ? QColor("#000000") : QColor("#7A2525");
    const QColor closeBorder = spec.highContrast ? QColor("#FFFFFF") : QColor("#D76666");
    const QColor bannerText = spec.highContrast ? spec.windowText : spec.mapText;
    const QColor bannerIdle = spec.highContrast ? QColor("#000000") : spec.mapBackdrop.lighter(112);
    const QColor bannerTx = spec.highContrast ? QColor("#000000") : spec.negative.darker(260);
    const QColor bannerWarning = spec.highContrast ? QColor("#000000") : spec.warning.darker(280);
    const QColor bannerReady = spec.highContrast ? QColor("#000000") : spec.positive.darker(280);
    const QMap<QString, QString> replacements = {
        {QStringLiteral("@HIGHLIGHT@"), spec.highlight.name()},
        {QStringLiteral("@HIGHLIGHT_TEXT@"), spec.highlightedText.name()},
        {QStringLiteral("@CLOSE@"), close.name()},
        {QStringLiteral("@CLOSE_BORDER@"), closeBorder.name()},
        {QStringLiteral("@DISABLED@"), spec.disabledText.name()},
        {QStringLiteral("@ACCENT@"), spec.accent.name()},
        {QStringLiteral("@MUTED@"), spec.mutedText.name()},
        {QStringLiteral("@HOVER@"), hover.name()},
        {QStringLiteral("@POSITIVE@"), spec.positive.name()},
        {QStringLiteral("@NEGATIVE@"), spec.negative.name()},
        {QStringLiteral("@WARNING@"), spec.warning.name()},
        {QStringLiteral("@RX_PRIMARY@"), spec.rxPrimary.name()},
        {QStringLiteral("@RX_SECONDARY@"), spec.rxSecondary.name()},
        {QStringLiteral("@SCROLL_HANDLE@"), spec.border.name()},
        {QStringLiteral("@MAP_BORDER@"), spec.mapBorder.name()},
        {QStringLiteral("@BANNER_IDLE@"), bannerIdle.name()},
        {QStringLiteral("@BANNER_TX@"), bannerTx.name()},
        {QStringLiteral("@BANNER_WARNING@"), bannerWarning.name()},
        {QStringLiteral("@BANNER_READY@"), bannerReady.name()},
        {QStringLiteral("@BANNER_TEXT@"), bannerText.name()}
    };
    for (auto it = replacements.constBegin(); it != replacements.constEnd(); ++it) {
        qss.replace(it.key(), it.value());
    }
    return qss;
}


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

        // Avionica deliberately keeps its aircraft-instrument metal rim. Every
        // other theme uses one quiet palette-derived outline. Painting the
        // three amber rims unconditionally was the source of the apparent
        // double/triple borders in Qt Classic, Classic Dark and High Contrast.
        if (g_activeThemeKey != QStringLiteral("avionica")) {
            const QColor outline = palette().color(QPalette::Mid);
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(outline, g_activeThemeKey == QStringLiteral("high_contrast") ? 2.0 : 1.0));
            p.drawRoundedRect(QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5), 6, 6);
            return;
        }

        // 0.5.78 UI pass: top-level cockpit windows use the same
        // visually detached triple bezel requested for the Logbook dialog.  The
        // overlay is mouse-transparent, so it gives the main window a continuous
        // aircraft-style frame without stealing clicks from menus or controls.
        const QRectF outer = rect().adjusted(2.0, 2.0, -2.0, -2.0);
        const QRectF mid = rect().adjusted(5.0, 5.0, -5.0, -5.0);
        const QRectF inner = rect().adjusted(8.0, 8.0, -8.0, -8.0);

        QLinearGradient outerRim(outer.topLeft(), outer.bottomRight());
        outerRim.setColorAt(0.0, QColor(138, 96, 44, 220));
        outerRim.setColorAt(0.45, QColor(42, 31, 20, 150));
        outerRim.setColorAt(1.0, QColor(188, 114, 39, 210));
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(outerRim, 2.0));
        p.drawRoundedRect(outer, 9, 9);

        QLinearGradient midRim(mid.topLeft(), mid.bottomRight());
        midRim.setColorAt(0.0, QColor(38, 29, 21, 180));
        midRim.setColorAt(0.50, QColor(5, 5, 5, 80));
        midRim.setColorAt(1.0, QColor(78, 50, 25, 170));
        p.setPen(QPen(midRim, 1.0));
        p.drawRoundedRect(mid, 7, 7);

        QLinearGradient innerRim(inner.topLeft(), inner.bottomRight());
        innerRim.setColorAt(0.0, QColor(102, 73, 38, 160));
        innerRim.setColorAt(0.55, QColor(20, 20, 20, 80));
        innerRim.setColorAt(1.0, QColor(124, 72, 27, 150));
        p.setPen(QPen(innerRim, 1.0));
        p.drawRoundedRect(inner, 5, 5);

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

void hardenPopupSurface(QWidget *popup)
{
    if (popup == nullptr) return;
    const Qt::WindowFlags flags = popup->windowFlags();
    if (!(flags & Qt::Popup)) return;

    if (!popup->property("cockpitPopupHardened").toBool()) {
        popup->setProperty("cockpitPopupHardened", true);
        popup->setAttribute(Qt::WA_StyledBackground, true);
        popup->setWindowFlags(flags | Qt::Popup | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    }

    QTimer::singleShot(0, popup, [popup]() {
        if (popup != nullptr && popup->isVisible()) {
            popup->raise();
            popup->activateWindow();
        }
    });
}

class ComboPopupRaiseFilter final : public QObject
{
public:
    explicit ComboPopupRaiseFilter(QAbstractItemView *view)
        : QObject(view)
        , m_view(view)
    {
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == m_view && event != nullptr && event->type() == QEvent::Show) {
            // QComboBox owns a private popup container around the item view.
            // Raising that container is safe; changing the view itself into a
            // top-level Qt::Popup is not.  The latter breaks the private
            // parent/geometry relationship on Windows and produces the exact
            // symptom where the wheel changes items but clicking shows no list.
            QPointer<QAbstractItemView> view(m_view);
            QTimer::singleShot(0, m_view, [view]() {
                if (view == nullptr || !view->isVisible()) return;
                if (QWidget *popup = view->window()) {
                    popup->raise();
                }
            });
        }
        return QObject::eventFilter(watched, event);
    }

private:
    QAbstractItemView *m_view = nullptr;
};

void hardenComboPopup(QComboBox *combo)
{
    if (combo == nullptr) return;
    combo->setProperty("cockpitComboPopupHardened", true);
    if (QAbstractItemView *view = combo->view()) {
        if (!view->property("cockpitComboPopupRaiseFilter").toBool()) {
            view->setProperty("cockpitComboPopupRaiseFilter", true);
            view->installEventFilter(new ComboPopupRaiseFilter(view));
        }
        view->setAttribute(Qt::WA_StyledBackground, true);
        view->setAutoFillBackground(true);
        if (QWidget *vp = view->viewport()) {
            vp->setAttribute(Qt::WA_StyledBackground, true);
            vp->setAutoFillBackground(true);
        }

        // Copy the current application palette every time this tree is
        // polished. A combo can outlive a runtime theme switch; retaining the
        // old Avionica palette produced orange text on the white Qt Classic
        // popup and made the selector effectively unreadable.
        const QPalette pal = qApp != nullptr ? qApp->palette() : combo->palette();
        // Do not call setWindowFlags() on combo->view().  The view is a child
        // of QComboBox's private popup container; promoting it to a top-level
        // popup detaches/breaks that container on Windows.
        view->setStyleSheet(QStringLiteral(
            "QListView, QAbstractItemView { background:palette(base); color:palette(text); "
            "border:1px solid palette(mid); selection-background-color:palette(highlight); "
            "selection-color:palette(highlighted-text); outline:0; }"
            "QListView::item, QAbstractItemView::item { color:palette(text); background:transparent; "
            "min-height:24px; padding:4px 10px; }"
            "QListView::item:selected, QAbstractItemView::item:selected, "
            "QListView::item:hover, QAbstractItemView::item:hover { "
            "background:palette(highlight); color:palette(highlighted-text); }"));
        view->setPalette(pal);
        if (QWidget *vp = view->viewport()) {
            vp->setPalette(pal);
        }
    }
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
            // Do not globally rewrite every Qt::Popup window.  On Windows this can
            // leave hidden native popup helpers alive after the main window closes
            // and it can also fight fullscreen window activation.  Combo boxes are
            // handled explicitly in polishCockpitWidgetTree().
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

void ensureThemeApplicationFilter(QApplication &app)
{
    static CockpitApplicationFilter *themeDialogFilter = nullptr;
    if (themeDialogFilter == nullptr) {
        themeDialogFilter = new CockpitApplicationFilter(&app);
        app.installEventFilter(themeDialogFilter);
    }
}

} // namespace

namespace MadModemUi {

void applyCockpitTheme(QApplication &app)
{
    g_activeThemeKey = QStringLiteral("avionica");
    app.setPalette(paletteForTheme(themeSpec(g_activeThemeKey)));
    ensureThemeApplicationFilter(app);

    app.setStyleSheet(QString::fromLatin1(R"QSS(
/* MadModem cockpit / avionics theme.  Embedded in code for Linux and MXE static builds. */
* {
    selection-background-color: #ff9a20;
    selection-color: #050505;
}
QWidget {
    background-color: #070707;
    color: #ffb347;
}
QMainWindow, QDialog {
    background-color: #060606;
}
QMainWindow[cockpitMainWindow="true"] {
    background: #030303;
    border: 3px double #a56725;
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
    min-width: 58px;
    padding: 4px 6px;
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
    background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #5dff78, stop:1 #1fd845);
    border-color: #b8ffc5;
}
QRadioButton::indicator:checked:enabled { background: #32ff5c; border-color: #b2ffc2; }
QRadioButton::indicator:disabled { background: #050505; border-color: #3a3025; }
QRadioButton::indicator:checked:disabled { background: #32ff5c; border-color: #b2ffc2; }
QLabel { background: transparent; color: #ffb347; }
QLCDNumber { background-color: #020202; color: #ff9b24; border: 1px solid #53351c; border-radius: 6px; }
QProgressBar { background-color: #020202; color: #ffe0a0; border: 1px solid #463728; border-radius: 6px; text-align: center; min-height: 18px; }
QProgressBar::chunk { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #78420f, stop:0.55 #ff9d24, stop:1 #ffdb77); border-radius: 4px; }
QSlider::groove:horizontal { height:6px; background:#020202; border:1px solid #493a2b; border-radius:3px; }
QSlider::handle:horizontal { width:14px; margin:-5px 0; background:#ff9a20; border:1px solid #ffd079; border-radius:7px; }
QSlider::groove:vertical { width:6px; background:#020202; border:1px solid #493a2b; border-radius:3px; }
QSlider::handle:vertical { height:14px; margin:0 -5px; background:#ff9a20; border:1px solid #ffd079; border-radius:7px; }
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
    min-width: 72px;
    padding-left: 5px;
    padding-right: 5px;
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

/* Semantic colours are properties, not widget-owned hard-coded styles. */
QWidget[mmRole="positive"] { color: #53ff70; }
QWidget[mmRole="negative"] { color: #ff6262; }
QWidget[mmRole="warning"] { color: #ffc35c; }
QWidget[mmRole="muted"] { color: #9d8c78; }
QWidget[mmRole="accent"] { color: #ff9a20; }
QLabel[mmRole="rxPrimary"] { color: #53ff70; }
QLabel[mmRole="rxSecondary"] { color: #55aaff; }
QLabel[ftBannerState="idle"] { border:2px solid #33414d; border-radius:6px; padding:6px; background:#17202a; color:#d7e0e7; font-weight:500; font-size:10pt; }
QLabel[ftBannerState="tx"] { border:2px solid #b33a32; border-radius:6px; padding:6px; background:#33100d; color:#ffe3df; font-weight:500; font-size:10pt; }
QLabel[ftBannerState="armed"] { border:2px solid #a06400; border-radius:6px; padding:6px; background:#332100; color:#fff0cc; font-weight:500; font-size:10pt; }
QLabel[ftBannerState="ready"], QLabel[ftBannerState="monitor"] { border:2px solid #1f6b4a; border-radius:6px; padding:6px; background:#10251b; color:#d9ffe9; font-weight:500; font-size:10pt; }

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
        if (auto *combo = qobject_cast<QComboBox *>(w)) {
            hardenComboPopup(combo);
        }
        if (auto *tb = qobject_cast<QTextEdit *>(w)) {
            tb->setProperty("cockpitTerminal", true);
        }
        if (auto *pb = qobject_cast<QPlainTextEdit *>(w)) {
            pb->setProperty("cockpitTerminal", true);
        }
    }
}

QString normalizedThemeKey(const QString &themeKey)
{
    const QString key = themeKey.trimmed().toLower();
    if (key == QStringLiteral("avionica") || key == QStringLiteral("qt_default") ||
        key == QStringLiteral("hacker_green") || key == QStringLiteral("classic_dark") ||
        key == QStringLiteral("high_contrast")) {
        return key;
    }
    return QStringLiteral("avionica");
}

QString activeThemeKey()
{
    return g_activeThemeKey;
}

QColor themeColor(ThemeColorRole role)
{
    const ThemeSpec spec = themeSpec(g_activeThemeKey);
    switch (role) {
    case ThemeColorRole::Accent: return spec.accent;
    case ThemeColorRole::AccentText: return spec.accentText;
    case ThemeColorRole::Positive: return spec.positive;
    case ThemeColorRole::Negative: return spec.negative;
    case ThemeColorRole::Warning: return spec.warning;
    case ThemeColorRole::MutedText: return spec.mutedText;
    case ThemeColorRole::RxPrimary: return spec.rxPrimary;
    case ThemeColorRole::RxSecondary: return spec.rxSecondary;
    case ThemeColorRole::MapBackdrop: return spec.mapBackdrop;
    case ThemeColorRole::MapText: return spec.mapText;
    case ThemeColorRole::MapMutedText: return spec.mapMutedText;
    case ThemeColorRole::MapBorder: return spec.mapBorder;
    }
    return spec.windowText;
}

void setSemanticRole(QWidget *widget, const QString &role)
{
    if (widget == nullptr || widget->property("mmRole").toString() == role) {
        return;
    }
    widget->setProperty("mmRole", role);
    if (widget->style() != nullptr) {
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
    }
    widget->update();
}

void applyUiTheme(QApplication &app, const QString &themeKey)
{
    const QString key = normalizedThemeKey(themeKey);
    g_activeThemeKey = key;
    ensureThemeApplicationFilter(app);

    if (key == QStringLiteral("avionica")) {
        applyCockpitTheme(app);
    } else {
        const ThemeSpec spec = themeSpec(key);
        app.setPalette(paletteForTheme(spec));
        app.setStyleSheet(genericThemeStyleSheet(spec));
    }

    // Existing dialogs and combo popups may survive a live theme switch. Apply
    // the new palette/QSS to those objects too, instead of requiring a restart
    // or leaving fragments of the previous theme behind.
    const QWidgetList windows = app.topLevelWidgets();
    for (QWidget *window : windows) {
        if (window == nullptr) continue;
        polishCockpitWidgetTree(window);
        if (window->style() != nullptr) {
            window->style()->unpolish(window);
            window->style()->polish(window);
        }
        const QList<QWidget *> children = window->findChildren<QWidget *>();
        for (QWidget *child : children) {
            if (child != nullptr) child->update();
        }
        window->update();
    }
}

} // namespace MadModemUi
