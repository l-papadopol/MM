// -----------------------------------------------------------------------------
// MadModem
#include "MadModemVersion.h"
// Cross-platform amateur radio audio modem
// -----------------------------------------------------------------------------

#include "mainwindow.h"
#include "audio/AudioBlock.h"
#include "settings/AppSettings.h"
#include "utils/CockpitTheme.h"

#include <QApplication>
#include <QCoreApplication>
#include <QMetaType>
#include <QIcon>
#include <QPointF>
#include <QVector>
#include <QProxyStyle>
#include <QStyle>
#include <QSurfaceFormat>
#include <QtGlobal>
#include <QThread>
#include <QTimer>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QTextCodec>
#endif


namespace {

/**
 * @brief Application style proxy that delays tooltips for dense control panels.
 *
 * Purpose:
 * - Keep the UI clean without visible help buttons.
 * - Show standard Qt tooltips only after the mouse rests on a component.
 */
class DelayedToolTipStyle final : public QProxyStyle
{
public:
    /**
     * @brief Creates a proxy over the current application style.
     */
    DelayedToolTipStyle()
        : QProxyStyle()
    {
    }

    /**
     * @brief Overrides the standard tooltip wake-up delay.
     */
    int styleHint(StyleHint hint,
                  const QStyleOption *option = nullptr,
                  const QWidget *widget = nullptr,
                  QStyleHintReturn *returnData = nullptr) const override
    {
        if (hint == QStyle::SH_ToolTip_WakeUpDelay) {
            return 3000;
        }

        return QProxyStyle::styleHint(hint, option, widget, returnData);
    }
};

} // namespace

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
#endif
    QSurfaceFormat glFormat;
    glFormat.setSwapInterval(0);
    QSurfaceFormat::setDefaultFormat(glFormat);

    QApplication app(argc, argv);
    qRegisterMetaType<AudioBlock>("AudioBlock");
    qRegisterMetaType<QVector<quint8>>("QVector<quint8>");
    qRegisterMetaType<QVector<QPointF>>("QVector<QPointF>");
    app.setStyle(new DelayedToolTipStyle());
    QCoreApplication::setApplicationName(QStringLiteral("MadModem"));
    QCoreApplication::setApplicationVersion(QStringLiteral(MADMODEM_VERSION_STRING));
    app.setApplicationDisplayName(QStringLiteral(MADMODEM_VERSION_DISPLAY));
    app.setWindowIcon(QIcon(":/icons/madmodem.png"));

    AppSettings bootSettings;
    bootSettings.load();
    const QString bootTheme = bootSettings.uiTheme.trimmed().toLower();
    if (bootTheme != QStringLiteral("qt_default")) {
        MadModemUi::applyCockpitTheme(app);
    }

    MainWindow window;
    if (bootTheme != QStringLiteral("qt_default")) {
        MadModemUi::installCockpitMainWindowChrome(&window);
    }
    // Cockpit UI is intended to run like a radio console / fullscreen
    // instrument panel.  Keep the custom minimize/maximize/close buttons in
    // the in-app title bar, but hide the OS panel/taskbar.
    auto forceMainWindowFullScreen = [&window]() {
        if (!window.isVisible() || !window.isFullScreen()) {
            window.setWindowState((window.windowState() & ~Qt::WindowMinimized) | Qt::WindowFullScreen);
            window.showFullScreen();
        }
        window.raise();
        window.activateWindow();
    };
    forceMainWindowFullScreen();
    QTimer::singleShot(0, &window, forceMainWindowFullScreen);
    QTimer::singleShot(250, &window, forceMainWindowFullScreen);

    return app.exec();
}
