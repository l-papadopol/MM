// -----------------------------------------------------------------------------
// MadModem
#include "MadModemVersion.h"
// Cross-platform amateur radio audio modem
// -----------------------------------------------------------------------------

#include "mainwindow.h"
#include "audio/AudioBlock.h"
#include "settings/AppSettings.h"
#include "modems/ft8/Ft8RxDecoder.h"
#include "utils/CockpitTheme.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QFileInfo>
#include <QTextStream>
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
#include <cstdio>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QTextCodec>
#endif


namespace {

constexpr const char *kFtPerfRecord = "FTPERF\t";
constexpr const char *kFtPerfPass = "\tpass=";
constexpr const char *kFtPerfCandidates = "\tcandidates=";
constexpr const char *kFtPerfDecodes = "\tdecodes=";
constexpr const char *kFtPerfSearchMs = "\tsearch_ms=";
constexpr const char *kFtPerfDecodeMs = "\tdecode_ms=";
constexpr const char *kFtPerfSubtractMs = "\tsubtract_ms=";
constexpr const char *kFtPerfTotalMs = "\ttotal_ms=";
constexpr const char *kFtPerfSyncReject = "\tsync_reject=";
constexpr const char *kFtPerfLdpc = "\tldpc=";
constexpr const char *kFtPerfLdpcFail = "\tldpc_fail=";
constexpr const char *kFtPerfOsd = "\tosd=";
constexpr const char *kFtPerfOsdRecovered = "\tosd_recovered=";
constexpr const char *kFtPerfResidual = "\tresidual=";
constexpr const char *kFtPerfResidualRecovered = "\tresidual_recovered=";
constexpr const char *kFtPerfDecision = "\tdecision=";

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

    QCommandLineParser commandLine;
    commandLine.setApplicationDescription(QStringLiteral("MadModem cross-platform amateur-radio modem"));
    commandLine.addHelpOption();
    commandLine.addVersionOption();
    const QCommandLineOption ftRegressionOption(
        QStringList{QStringLiteral("ft-regression")},
        QStringLiteral("Decode positional WAV files without opening the UI and print machine-readable FT regression results."));
    const QCommandLineOption ftModeOption(
        QStringList{QStringLiteral("ft-mode")},
        QStringLiteral("FT mode used by --ft-regression: FT8 or FT4."),
        QStringLiteral("mode"),
        QStringLiteral("FT8"));
    const QCommandLineOption ftDepthOption(
        QStringList{QStringLiteral("ft-depth")},
        QStringLiteral("Decode depth used by --ft-regression: fast, deep or max."),
        QStringLiteral("depth"),
        QStringLiteral("max"));
    commandLine.addOption(ftRegressionOption);
    commandLine.addOption(ftModeOption);
    commandLine.addOption(ftDepthOption);
    commandLine.addPositionalArgument(QStringLiteral("wav"),
                                      QStringLiteral("WAV file(s) for --ft-regression."),
                                      QStringLiteral("[wav ...]"));
    commandLine.process(app);

    if (commandLine.isSet(ftRegressionOption)) {
        QTextStream out(stdout);
        QTextStream err(stderr);
        const QString mode = commandLine.value(ftModeOption).trimmed().toUpper();
        const QString depth = commandLine.value(ftDepthOption).trimmed().toLower();
        const QStringList files = commandLine.positionalArguments();
        if (mode != QStringLiteral("FT8") && mode != QStringLiteral("FT4")) {
            err << "ERROR\tinvalid FT mode: " << mode << "\n";
            return 2;
        }
        if (depth != QStringLiteral("fast") && depth != QStringLiteral("deep") && depth != QStringLiteral("max")) {
            err << "ERROR\tinvalid FT depth: " << depth << "\n";
            return 2;
        }
        if (files.isEmpty()) {
            err << "ERROR\t--ft-regression requires at least one WAV path\n";
            return 2;
        }

        bool allOk = true;
        for (const QString &filePath : files) {
            Ft8RxDecoder decoder;
            decoder.setModeName(mode);
            decoder.setDeepDecodeEnabled(depth != QStringLiteral("fast"));
            decoder.setDspPlusDecodeEnabled(depth == QStringLiteral("max"));

            QStringList messages;
            QVector<Ft8RxDecoder::PerfStats> perfRows;
            bool finished = false;
            bool ok = false;
            int decodeCount = 0;
            QString summary;
            QObject::connect(&decoder, &Ft8RxDecoder::decodeReady, &decoder,
                             [&messages](const Ft8RxDecoder::Decode &decode) {
                                 messages.append(QStringLiteral("%1Hz:%2")
                                                     .arg(decode.frequencyHz)
                                                     .arg(decode.message.simplified()));
                             });
            QObject::connect(&decoder, &Ft8RxDecoder::performanceUpdated, &decoder,
                             [&perfRows](const Ft8RxDecoder::PerfStats &stats) {
                                 perfRows.append(stats);
                             });
            QObject::connect(&decoder, &Ft8RxDecoder::offlineAnalysisFinished, &decoder,
                             [&finished, &ok, &decodeCount, &summary](const QString &, bool resultOk, int count, const QString &message) {
                                 finished = true;
                                 ok = resultOk;
                                 decodeCount = count;
                                 summary = message;
                             });

            decoder.analyzeAudioFile(filePath);
            if (!finished) {
                ok = false;
                summary = QStringLiteral("offline decoder did not emit completion");
            }
            allOk = allOk && ok;
            summary.replace(QLatin1Char('\t'), QLatin1Char(' '));
            summary.replace(QLatin1Char('\n'), QLatin1Char(' '));
            out << "FTREG\t" << mode
                << "\t" << depth
                << "\t" << QFileInfo(filePath).fileName()
                << "\t" << (ok ? QStringLiteral("OK") : QStringLiteral("FAIL"))
                << "\t" << decodeCount
                << "\t" << summary
                << "\n";
            for (const QString &message : messages) {
                out << "FTMSG\t" << QFileInfo(filePath).fileName() << "\t" << message << "\n";
            }
            for (const Ft8RxDecoder::PerfStats &stats : perfRows) {
                QString decision = stats.adaptiveDecision;
                if (!stats.adaptiveReasons.isEmpty()) {
                    if (!decision.isEmpty()) decision += QLatin1Char(':');
                    decision += stats.adaptiveReasons.join(QLatin1Char(','));
                }
                if (!stats.earlyStopReason.isEmpty()) {
                    if (!decision.isEmpty()) decision += QLatin1Char(':');
                    decision += stats.earlyStopReason;
                }
                decision.replace(QLatin1Char('\t'), QLatin1Char(' '));
                decision.replace(QLatin1Char('\n'), QLatin1Char(' '));
                out << kFtPerfRecord << QFileInfo(filePath).fileName()
                    << "\t" << stats.phase
                    << kFtPerfPass << stats.passCount
                    << kFtPerfCandidates << stats.candidateCount
                    << kFtPerfDecodes << stats.decodeCount
                    << kFtPerfSearchMs << QString::number(stats.candidateSearchMs, 'f', 1)
                    << kFtPerfDecodeMs << QString::number(stats.candidateDecodeMs, 'f', 1)
                    << kFtPerfSubtractMs << QString::number(stats.subtractionMs, 'f', 1)
                    << kFtPerfTotalMs << QString::number(stats.totalMs, 'f', 1)
                    << kFtPerfSyncReject << stats.syncGateRejects
                    << kFtPerfLdpc << stats.ldpcTried
                    << kFtPerfLdpcFail << stats.ldpcFailures
                    << kFtPerfOsd << stats.osdGf2Tried
                    << kFtPerfOsdRecovered << stats.osdGf2Recovered
                    << kFtPerfResidual << stats.residualSelectedCandidates
                    << kFtPerfResidualRecovered << stats.residualAcceptedDecodes
                    << kFtPerfDecision << decision
                    << "\n";
            }
            out.flush();
        }
        return allOk ? 0 : 3;
    }

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
