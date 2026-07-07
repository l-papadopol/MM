#include "mainwindow.h"
#include "MadModemVersion.h"
#include "ui_mainwindow.h"

#include "dialogs/HelpDialog.h"
#include "dialogs/LogbookDialog.h"
#include "dialogs/SstvImageEditorDialog.h"
#include "modems/sstv/SstvModeDefinition.h"
#include "modems/ft8/Ft8Mode.h"
#include "modems/ft8/AutoQsoFlowExecutor.h"
#include "flow/MmFlowRuntime.h"
#include "modems/ft8/Ft8RxDecoder.h"
#include "modems/ft8/FtDecodedText.h"
#include "modems/ft8/FtQsoSequencer.h"
#include "modems/ft8/FtStandardMessageSet.h"
#include "modems/sstv/tx/SstvTransmitter.h"
#include "modems/rtty/tx/RttyTransmitter.h"
#include "modems/bpsk31/tx/Bpsk31Transmitter.h"
#include "modems/cw/tx/CwTransmitter.h"
#include "modems/mfsk/tx/MfskTransmitter.h"
#include "modems/hell/tx/HellschreiberTransmitter.h"
#include "modems/ft8/tx/Ft8Transmitter.h"
#include "modems/msk144/Msk144Mode.h"
#include "modems/msk144/tx/Msk144Transmitter.h"
#include "modems/q65/tx/Q65Transmitter.h"
#include "utils/UiScale.h"
#include "utils/RuntimeI18n.h"
#include "utils/CockpitTheme.h"
#include "modems/weatherfax/tx/WeatherFaxTransmitter.h"
#include "widgets/RttyScopeWidget.h"
#include "widgets/DdspPanelWidget.h"
#include "ai/DeepDspController.h"
#include "dxcc/CtyCountryFile.h"
#include "rig/HamlibController.h"
#include "dsp/cpu/CpuFeatures.h"
#include "third_party/decodium_gpl/port/NtpClient.hpp"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QMetaType>
#include <QTabWidget>
#include <QTabBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QRadioButton>
#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAbstractItemModel>
#include <algorithm>
#include <utility>
#include <QByteArray>
#include <QBrush>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDebug>
#include <QIcon>
#include <QColor>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <memory>
#include <QProgressBar>
#include <QPlainTextEdit>
#include <QLabel>
#include <QLCDNumber>
#include <QImageReader>
#include <QInputDialog>
#include <QGroupBox>
#include <QIODevice>
#include <QLineEdit>
#include <QList>
#include <QMessageBox>
#include <QMetaObject>
#include <QMenu>
#include <QMouseEvent>
#include <QEvent>
#include <QProgressDialog>
#include <QProcess>
#include <QPixmap>
#include <QPointer>
#include <QPainter>
#include <QPair>
#include <QPen>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QScreen>
#include <QSerialPortInfo>
#include <QSettings>
#include <cmath>
#include <QSignalBlocker>
#include <QSize>
#include <QSizePolicy>
#include <QSlider>
#include <QSpinBox>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextCharFormat>
#include <QTextStream>
#include <QRegularExpression>
#include <QRandomGenerator>
#include <QStringList>
#include <QSplitter>
#include <QSpacerItem>
#include <QStyledItemDelegate>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTextBrowser>
#include <QWhatsThis>
#include <QKeySequence>
#include <QtMath>

#include <cstring>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QAudioDevice>
#include <QMediaDevices>
#else
#include <QAudio>
#include <QAudioDeviceInfo>
#endif


class LedVuMeterWidget final : public QWidget
{
public:
    explicit LedVuMeterWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumWidth(58);
        setMaximumWidth(66);
        setMinimumHeight(170);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    }

    void setLevelPercent(int percent)
    {
        const int clamped = qBound(0, percent, 100);
        if (m_percent == clamped) {
            return;
        }
        m_percent = clamped;
        update();
    }

    void setDbText(const QString &text)
    {
        if (m_dbText == text) {
            return;
        }
        m_dbText = text;
        update();
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event)

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QRect outer = rect().adjusted(0, 0, -1, -1);
        painter.setPen(QPen(QColor(70, 70, 70), 1));
        painter.setBrush(QColor(12, 12, 12));
        painter.drawRoundedRect(outer, 4, 4);

        QFont dbFont = painter.font();
        dbFont.setPointSizeF(qMax(7.0, dbFont.pointSizeF() - 1.0));
        painter.setFont(dbFont);
        painter.setPen(QColor(225, 225, 225));
        const QRect dbRect = outer.adjusted(3, 3, -3, 0);
        painter.drawText(dbRect, Qt::AlignHCenter | Qt::AlignTop, m_dbText);

        const int labelHeight = qMax(18, QFontMetrics(dbFont).height() + 5);
        const QRect ledArea = outer.adjusted(7, labelHeight + 3, -7, -5);
        constexpr int kSegments = 32;
        constexpr int kGap = 2;
        const int totalGap = (kSegments - 1) * kGap;
        const int segmentHeight = qMax(2, (ledArea.height() - totalGap) / kSegments);
        const int usedHeight = segmentHeight * kSegments + totalGap;
        const int top = ledArea.bottom() - usedHeight + 1;
        const int litSegments = qBound(0, qRound((m_percent / 100.0) * kSegments), kSegments);

        for (int i = 0; i < kSegments; ++i) {
            const bool lit = i < litSegments;
            const int y = top + (kSegments - 1 - i) * (segmentHeight + kGap);
            const QRect seg(ledArea.left(), y, ledArea.width(), segmentHeight);

            QColor color;
            if (i >= 23) {
                color = lit ? QColor(240, 30, 30) : QColor(45, 14, 14);
            } else if (i >= 18) {
                color = lit ? QColor(255, 185, 30) : QColor(48, 35, 10);
            } else {
                color = lit ? QColor(20, 230, 60) : QColor(10, 42, 18);
            }

            painter.setPen(Qt::NoPen);
            painter.setBrush(color);
            painter.drawRoundedRect(seg, 1.5, 1.5);
        }
    }

private:
    int m_percent = 0;
    QString m_dbText = QStringLiteral("-inf dB");
};

namespace {

void hardenPopupMenuForFullscreen(QMenu *menu)
{
    if (menu == nullptr) {
        return;
    }

    // In true frameless fullscreen on some X11/Wayland window managers a
    // top-level QMenu can be created below the fullscreen window or lose the
    // activation race.  Keep the menu as a normal Qt popup, but explicitly keep
    // it above the cockpit window and raise it after Qt has created the native
    // popup surface.  This is deliberately applied to mode/language menus only;
    // it does not change modem routing.
    menu->setWindowFlags(menu->windowFlags() | Qt::Popup | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    QObject::connect(menu, &QMenu::aboutToShow, menu, [menu]() {
        QTimer::singleShot(0, menu, [menu]() {
            if (menu != nullptr) {
                menu->raise();
                menu->activateWindow();
            }
        });
    });
}

constexpr int kFtRowRedOutlineRole = Qt::UserRole + 427;
constexpr int kFtOriginalMessageRole = Qt::UserRole + 428;

class FtDecodeRowDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QStyledItemDelegate::paint(painter, option, index);
        if (!index.data(kFtRowRedOutlineRole).toBool()) {
            return;
        }
        painter->save();
        QPen pen(QColor(220, 0, 0), 2);
        pen.setStyle(Qt::DashLine);
        painter->setPen(pen);
        QRect r = option.rect.adjusted(1, 1, -1, -1);
        painter->drawLine(r.topLeft(), r.topRight());
        painter->drawLine(r.bottomLeft(), r.bottomRight());
        const QAbstractItemModel *model = index.model();
        if (model != nullptr) {
            if (index.column() == 0) {
                painter->drawLine(r.topLeft(), r.bottomLeft());
            }
            if (index.column() == model->columnCount(index.parent()) - 1) {
                painter->drawLine(r.topRight(), r.bottomRight());
            }
        }
        painter->restore();
    }
};

QString normalizeFtFilterCall(QString call)
{
    call = call.trimmed().toUpper();
    call.remove(QRegularExpression(QStringLiteral("[^A-Z0-9/]+")));
    return call;
}

QColor mmColourFromSetting(const QString &name, const QColor &fallback)
{
    const QColor colour(name);
    return colour.isValid() ? colour : fallback;
}

double bpskSymbolRateForVariant(const QString &variant)
{
    const QString v = variant.trimmed().toUpper();
    if (v.endsWith(QStringLiteral("1000"))) {
        return 1000.0;
    }
    if (v.endsWith(QStringLiteral("500"))) {
        return 500.0;
    }
    if (v.endsWith(QStringLiteral("250"))) {
        return 250.0;
    }
    if (v.endsWith(QStringLiteral("125"))) {
        return 125.0;
    }
    if (v.endsWith(QStringLiteral("63"))) {
        return 62.5;
    }
    return 31.25;
}

bool pskVariantIsQpsk(const QString &variant)
{
    return variant.trimmed().toUpper().startsWith(QStringLiteral("QPSK"));
}

int safeDocumentEndPosition(QTextDocument *document)
{
    if (document == nullptr) {
        return 0;
    }
    QTextCursor cursor(document);
    cursor.movePosition(QTextCursor::End);
    return qMax(0, cursor.position());
}

bool moveCursorToDocumentPosition(QTextCursor &cursor, int position)
{
    const int target = qMax(0, position);
    cursor.clearSelection();
    cursor.movePosition(QTextCursor::Start);
    int remaining = target;
    while (remaining > 0) {
        const int step = qMin(remaining, 256);
        if (!cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, step)) {
            return false;
        }
        remaining -= step;
    }
    return true;
}

bool selectDocumentRange(QTextCursor &cursor, int start, int end)
{
    if (end <= start) {
        cursor.clearSelection();
        return false;
    }
    if (!moveCursorToDocumentPosition(cursor, start)) {
        return false;
    }
    int remaining = end - start;
    while (remaining > 0) {
        const int step = qMin(remaining, 256);
        if (!cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, step)) {
            cursor.clearSelection();
            return false;
        }
        remaining -= step;
    }
    return true;
}

QString readShortProcessText(const QString &program,
                             const QStringList &arguments,
                             int timeoutMs = 900)
{
    QProcess process;
    process.start(program, arguments);
    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(100);
        return QString();
    }
    QString out = QString::fromLocal8Bit(process.readAllStandardOutput());
    const QString err = QString::fromLocal8Bit(process.readAllStandardError());
    if (out.trimmed().isEmpty()) {
        out = err;
    }
    return out;
}

QString cachedSystemClockSyncText()
{
    static QDateTime nextRefreshUtc;
    static QString cached = QStringLiteral("unknown");
    const QDateTime now = QDateTime::currentDateTimeUtc();
    if (nextRefreshUtc.isValid() && now < nextRefreshUtc) {
        return cached;
    }
    nextRefreshUtc = now.addSecs(45);

#ifdef Q_OS_LINUX
    QString out = readShortProcessText(QStringLiteral("timedatectl"),
                                       QStringList() << QStringLiteral("show")
                                                     << QStringLiteral("-p") << QStringLiteral("SystemClockSynchronized")
                                                     << QStringLiteral("-p") << QStringLiteral("NTPSynchronized"));
    const QString lower = out.toLower();
    if (lower.contains(QStringLiteral("systemclocksynchronized=yes")) ||
        lower.contains(QStringLiteral("ntpsynchronized=yes"))) {
        cached = QStringLiteral("synced");
        return cached;
    }
    if (lower.contains(QStringLiteral("systemclocksynchronized=no")) ||
        lower.contains(QStringLiteral("ntpsynchronized=no"))) {
        cached = QStringLiteral("not synced");
        return cached;
    }
    out = readShortProcessText(QStringLiteral("ntpq"), QStringList() << QStringLiteral("-p"));
    if (out.contains(QChar('*'))) {
        cached = QStringLiteral("synced");
        return cached;
    }
#elif defined(Q_OS_WIN)
    const QString out = readShortProcessText(QStringLiteral("w32tm"),
                                            QStringList() << QStringLiteral("/query") << QStringLiteral("/status"));
    if (out.contains(QStringLiteral("Source"), Qt::CaseInsensitive) &&
        !out.contains(QStringLiteral("Local CMOS Clock"), Qt::CaseInsensitive)) {
        cached = QStringLiteral("synced");
        return cached;
    }
#endif

    cached = QStringLiteral("unknown");
    return cached;
}

QString ft8StatusColour(const QString &state)
{
    const QString lower = state.toLower();
    if (lower.contains(QStringLiteral("not")) || lower.contains(QStringLiteral("slow")) || lower.contains(QStringLiteral("warning")) || lower.contains(QStringLiteral("unknown"))) {
        return QStringLiteral("#d84a4a");
    }
    if (lower.contains(QStringLiteral("synced")) || lower.contains(QStringLiteral("locked")) || lower.contains(QStringLiteral("ok")) || lower.contains(QStringLiteral("none"))) {
        return QStringLiteral("#2f80ed");
    }
    return QStringLiteral("#c8c8c8");
}

QString htmlEscaped(const QString &text)
{
    QString out = text;
    out.replace(QChar('&'), QStringLiteral("&amp;"));
    out.replace(QChar('<'), QStringLiteral("&lt;"));
    out.replace(QChar('>'), QStringLiteral("&gt;"));
    return out;
}

QIcon makeLogbookPlusIcon()
{
    QIcon icon;
    const QList<int> sizes = {16, 20, 24, 32, 48, 64};
    for (int size : sizes) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);

        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const qreal margin = qMax<qreal>(1.0, size * 0.10);
        QRectF box(margin, margin, size - 2.0 * margin, size - 2.0 * margin);
        painter.setPen(QPen(QColor(30, 95, 48), qMax<qreal>(1.0, size * 0.055)));
        painter.setBrush(QColor(38, 150, 72));
        painter.drawRoundedRect(box, size * 0.18, size * 0.18);

        painter.setPen(QPen(Qt::white, qMax<qreal>(2.0, size * 0.16), Qt::SolidLine, Qt::RoundCap));
        const qreal cx = size * 0.50;
        const qreal cy = size * 0.50;
        const qreal arm = size * 0.24;
        painter.drawLine(QPointF(cx - arm, cy), QPointF(cx + arm, cy));
        painter.drawLine(QPointF(cx, cy - arm), QPointF(cx, cy + arm));
        painter.end();

        icon.addPixmap(pixmap);
    }
    return icon;
}

/**
 * @brief Builds a shorter display name for long PulseAudio/ALSA device names.
 *
 * Purpose:
 * - Keep combo boxes readable.
 * - Preserve the original backend name in itemData().
 */
QString friendlyAudioName(const QString &backendName)
{
    if (backendName.trimmed().isEmpty()) {
        return "Unknown audio device";
    }

    if (backendName == "default") {
        return "Default";
    }

    QString name = backendName;

    name.replace("alsa_input.", "Input: ");
    name.replace("alsa_output.", "Output: ");
    name.replace("pci-0000_00_1f.3-platform-", "");
    name.replace("platform-", "");
    name.replace("__", " ");
    name.replace("_", " ");
    name.replace(".", " ");
    name.replace("source", "");
    name.replace("sink", "");
    name.replace("hw ", "hw:");

    name = name.simplified();

    if (name.length() > 72) {
        name = name.left(69) + "...";
    }

    return name;
}


/**
 * @brief Result of a small AFC tone-energy search around one marker.
 */
struct AfcTonePeak
{
    double frequencyHz = 0.0;
    double power = 0.0;
    double confidence = 0.0;
    bool valid = false;
};

/**
 * @brief Goertzel tone power normalized to roughly sample-power units.
 */
double goertzelPowerAt(const QVector<float> &samples, int sampleRate, double frequencyHz)
{
    const int n = samples.size();
    if (n < 64 || sampleRate <= 0 || frequencyHz <= 0.0 || frequencyHz >= sampleRate * 0.48) {
        return 0.0;
    }

    const double omega = 2.0 * M_PI * frequencyHz / static_cast<double>(sampleRate);
    const double coeff = 2.0 * qCos(omega);
    double s0 = 0.0;
    double s1 = 0.0;
    double s2 = 0.0;

    /* A very light triangular window reduces false pulls from symbol edges and
     * Hell keying clicks without adding allocations. */
    const double half = 0.5 * static_cast<double>(n - 1);
    for (int i = 0; i < n; ++i) {
        const double w = 1.0 - (0.35 * qAbs((static_cast<double>(i) - half) / qMax(1.0, half)));
        s0 = (static_cast<double>(samples.at(i)) * w) + (coeff * s1) - s2;
        s2 = s1;
        s1 = s0;
    }

    const double raw = (s1 * s1) + (s2 * s2) - (coeff * s1 * s2);
    const double norm = static_cast<double>(n) * static_cast<double>(n);
    return qMax(0.0, raw / qMax(1.0, norm));
}

/**
 * @brief Finds the local energy maximum around one selected marker.
 *
 * The search is deliberately narrow.  It is an AFC nudge around an operator
 * marker, not an automatic signal finder across the whole waterfall.
 */
AfcTonePeak estimateAfcTonePeak(const AudioBlock &block,
                                double centerHz,
                                int rangeHz,
                                double stepHz)
{
    AfcTonePeak result;

    if (block.samples.size() < 1024 || block.sampleRate <= 0 || rangeHz <= 0 || stepHz <= 0.0) {
        return result;
    }

    const double minHz = qMax(20.0, centerHz - static_cast<double>(rangeHz));
    const double maxHz = qMin(block.sampleRate * 0.45, centerHz + static_cast<double>(rangeHz));
    if (maxHz <= minHz) {
        return result;
    }

    QVector<double> powers;
    powers.reserve(static_cast<int>((maxHz - minHz) / stepHz) + 2);

    double bestFrequency = centerHz;
    double bestPower = 0.0;
    for (double f = minHz; f <= maxHz + 0.001; f += stepHz) {
        const double p = goertzelPowerAt(block.samples, block.sampleRate, f);
        powers.append(p);
        if (p > bestPower) {
            bestPower = p;
            bestFrequency = f;
        }
    }

    if (powers.size() < 3 || bestPower <= 0.0) {
        return result;
    }

    std::sort(powers.begin(), powers.end());
    const double medianPower = powers.at(powers.size() / 2);
    const double confidence = bestPower / qMax(1.0e-12, medianPower);

    double blockPower = 0.0;
    for (float sample : block.samples) {
        const double v = static_cast<double>(sample);
        blockPower += v * v;
    }
    blockPower /= qMax(1, block.samples.size());

    /* Avoid chasing random noise.  Strong keyed text signals usually produce a
     * very clear local maximum inside ±10..30 Hz. */
    const bool enoughAbsoluteEnergy = bestPower > qMax(1.0e-8, blockPower * 0.010);
    const bool enoughContrast = confidence >= 1.35;

    result.frequencyHz = bestFrequency;
    result.power = bestPower;
    result.confidence = confidence;
    result.valid = enoughAbsoluteEnergy && enoughContrast;
    return result;
}

int nudgedToneValue(int currentHz, double measuredHz, int maxStepHz, int minHz, int maxHz)
{
    const int targetHz = static_cast<int>(qRound(measuredHz));
    const int delta = qBound(-maxStepHz, targetHz - currentHz, maxStepHz);
    return qBound(minHz, currentHz + delta, maxHz);
}

/**
 * @brief Minimal parsed WAV stream metadata.
 */
struct WavStreamFormat
{
    quint16 audioFormat = 0;
    quint16 channels = 0;
    quint32 sampleRate = 0;
    quint16 blockAlign = 0;
    quint16 bitsPerSample = 0;
    qint64 dataStart = 0;
    qint64 dataSize = 0;
};

/**
 * @brief User-facing WEFAX station/preset entry.
 */
struct FaxLinePreset
{
    QString key;
    QString label;
    int lpm = 120;
    int lines = 3000;
    int blackHz = 1500;
    int whiteHz = 2300;
    QString details;
};

QList<FaxLinePreset> weatherFaxLinePresets()
{
    return {
        {
            "CUSTOM",
            "Custom / manual",
            120,
            3000,
            1500,
            2300,
            "Manual station. Set LPM, tone range and RX frequency by hand."
        },
        {
            "DWD_PINNEBERG",
            "Hamburg / Pinneberg - DDH3 DDK3 DDK6",
            120,
            3000,
            1500,
            2300,
            "Germany DWD Pinneberg. Example RF kHz: 3855.0, 7880.0, 13882.5. North Atlantic and Europe weather charts."
        },
        {
            "UK_NORTHWOOD_GYA",
            "Northwood UK - GYA",
            120,
            3000,
            1500,
            2300,
            "UK Northwood. Example RF kHz: 2618.5, 4610.0, 8040.0, 11086.5. Atlantic, British Isles and northern Mediterranean."
        },
        {
            "NOAA_BOSTON_NMF",
            "NOAA / USCG Boston - NMF",
            120,
            3000,
            1500,
            2300,
            "NOAA/USCG Boston. Example RF kHz: 4235.0, 6340.5, 9110.0, 12750.0. Good night-time transatlantic target."
        },
        {
            "NOAA_POINT_REYES_NMC",
            "NOAA / USCG Point Reyes - NMC",
            120,
            3000,
            1500,
            2300,
            "NOAA/USCG Point Reyes. Example RF kHz: 4346.0, 8682.0, 12786.0, 17151.2, 22527.0."
        },
        {
            "JAPAN_JMH",
            "Tokyo Japan - JMH",
            120,
            3000,
            1500,
            2300,
            "Tokyo/JMH. Example RF kHz: 3622.5, 7795.0, 13988.5, 18220.0."
        },
        {
            "AUSTRALIA_VMC",
            "Australia Melbourne - VMC",
            120,
            3000,
            1500,
            2300,
            "Australia VMC. Example RF kHz: 5100.0, 11030.0, 13920.0."
        },
        {
            "AUSTRALIA_VMW",
            "Australia Charleville - VMW",
            120,
            3000,
            1500,
            2300,
            "Australia VMW. Example RF kHz: 5755.0, 7535.0."
        },
        {
            "GENERIC_120LPM",
            "Generic HF WEFAX 120 LPM / IOC 576",
            120,
            3000,
            1500,
            2300,
            "Generic HF radiofax. Decode until end/control tone or end-of-signal timeout."
        },
        {
            "GENERIC_60LPM",
            "Generic slow 60 LPM",
            60,
            3000,
            1500,
            2300,
            "Slow line-rate test preset."
        },
        {
            "GENERIC_240LPM",
            "Generic fast 240 LPM",
            240,
            3000,
            1500,
            2300,
            "Fast line-rate test preset."
        }
    };
}

FaxLinePreset presetByKey(const QString &key)
{
    for (const FaxLinePreset &preset : weatherFaxLinePresets()) {
        if (preset.key == key) {
            return preset;
        }
    }

    return weatherFaxLinePresets().first();
}


/**
 * @brief User-facing RTTY preset entry.
 */
struct RttyPreset
{
    QString key;
    QString label;
    double baud = 45.45;
    int shiftHz = 170;
    int markHz = 2125;
    bool reverse = false;
    QString details;
};

QList<RttyPreset> rttyPresets()
{
    return {
        {
            "HAM_45_170_HIGH",
            "Amateur 45.45 / 170 / high",
            45.45,
            170,
            2125,
            false,
            "Common amateur HF AFSK RTTY: 45.45 baud, 170 Hz shift, high tones 2125/2295 Hz."
        },
        {
            "HAM_45_170_LOW",
            "Amateur 45.45 / 170 / low",
            45.45,
            170,
            1275,
            false,
            "Low-tone AFSK RTTY: 45.45 baud, 170 Hz shift, tones 1275/1445 Hz."
        },
        {
            "EU_50_170_HIGH",
            "50 baud / 170 / high",
            50.0,
            170,
            2125,
            false,
            "50 baud, 170 Hz shift, high tones. Useful for many utility and test recordings."
        },
        {
            "WIDE_50_425_HIGH",
            "50 baud / 425 / high",
            50.0,
            425,
            2125,
            false,
            "50 baud, 425 Hz shift. Wider-shift utility-style RTTY."
        },
        {
            "WIDE_75_425_HIGH",
            "75 baud / 425 / high",
            75.0,
            425,
            2125,
            false,
            "75 baud, 425 Hz shift. Faster RTTY mode for stronger channels."
        },
        {
            "WIDE_100_850_HIGH",
            "100 baud / 850 / high",
            100.0,
            850,
            2125,
            false,
            "100 baud, 850 Hz shift. Wide and fast compatibility/test mode."
        },
        {
            "CUSTOM",
            "Custom / manual",
            45.45,
            170,
            2125,
            false,
            "Manual RTTY settings. Adjust baud, shift, mark tone and reverse polarity by hand."
        }
    };
}

RttyPreset rttyPresetByKey(const QString &key)
{
    for (const RttyPreset &preset : rttyPresets()) {
        if (preset.key == key) {
            return preset;
        }
    }

    return rttyPresets().first();
}

/**
 * @brief Applies the same short tooltip to a widget and status bar hint.
 */
void setHelpText(QWidget *widget, const QString &text)
{
    if (widget == nullptr) {
        return;
    }

    widget->setToolTip(text);
    widget->setStatusTip(text);
    widget->setWhatsThis(text);
}

/**
 * @brief Makes ordinary tooltips available through the old-school What's This help mode too.
 */
void promoteToolTipsToContextHelp(QWidget *root)
{
    if (root == nullptr) {
        return;
    }

    const QList<QWidget *> widgets = root->findChildren<QWidget *>();
    for (QWidget *widget : widgets) {
        if (widget == nullptr) {
            continue;
        }
        const QString tip = widget->toolTip().trimmed();
        if (!tip.isEmpty()) {
            if (widget->statusTip().trimmed().isEmpty()) {
                widget->setStatusTip(tip);
            }
            if (widget->whatsThis().trimmed().isEmpty()) {
                widget->setWhatsThis(tip);
            }
        }
    }
}

quint16 readUInt16Le(const char *data)
{
    return static_cast<quint16>(static_cast<unsigned char>(data[0])) |
           static_cast<quint16>(static_cast<unsigned char>(data[1]) << 8);
}

quint32 readUInt32Le(const char *data)
{
    return static_cast<quint32>(static_cast<unsigned char>(data[0])) |
           (static_cast<quint32>(static_cast<unsigned char>(data[1])) << 8) |
           (static_cast<quint32>(static_cast<unsigned char>(data[2])) << 16) |
           (static_cast<quint32>(static_cast<unsigned char>(data[3])) << 24);
}

qint32 readInt24Le(const char *data)
{
    qint32 value = static_cast<qint32>(static_cast<unsigned char>(data[0])) |
                   (static_cast<qint32>(static_cast<unsigned char>(data[1])) << 8) |
                   (static_cast<qint32>(static_cast<unsigned char>(data[2])) << 16);

    if ((value & 0x00800000) != 0) {
        value |= static_cast<qint32>(0xFF000000);
    }

    return value;
}

bool parseWavHeader(QFile &file, WavStreamFormat &format, QString &errorMessage)
{
    const QByteArray riffHeader = file.read(12);

    if (riffHeader.size() != 12 ||
        riffHeader.mid(0, 4) != "RIFF" ||
        riffHeader.mid(8, 4) != "WAVE") {
        errorMessage = "The selected file is not a RIFF/WAVE file.";
        return false;
    }

    bool hasFormat = false;
    bool hasData = false;

    while (!file.atEnd()) {
        const QByteArray chunkHeader = file.read(8);

        if (chunkHeader.size() == 0) {
            break;
        }

        if (chunkHeader.size() != 8) {
            errorMessage = "Unexpected end of file while reading WAV chunks.";
            return false;
        }

        const QByteArray chunkId = chunkHeader.mid(0, 4);
        const quint32 chunkSize = readUInt32Le(chunkHeader.constData() + 4);
        const qint64 chunkDataStart = file.pos();
        const qint64 nextChunk = chunkDataStart + static_cast<qint64>(chunkSize) +
                                 static_cast<qint64>(chunkSize & 1U);

        if (chunkId == "fmt ") {
            if (chunkSize < 16 || chunkSize > 4096) {
                errorMessage = "Unsupported or corrupted WAV fmt chunk.";
                return false;
            }

            const QByteArray fmt = file.read(static_cast<qint64>(chunkSize));

            if (fmt.size() != static_cast<int>(chunkSize)) {
                errorMessage = "Unexpected end of file while reading WAV format.";
                return false;
            }

            format.audioFormat = readUInt16Le(fmt.constData() + 0);
            format.channels = readUInt16Le(fmt.constData() + 2);
            format.sampleRate = readUInt32Le(fmt.constData() + 4);
            format.blockAlign = readUInt16Le(fmt.constData() + 12);
            format.bitsPerSample = readUInt16Le(fmt.constData() + 14);

            if (format.audioFormat == 0xFFFE && fmt.size() >= 40) {
                format.audioFormat = readUInt16Le(fmt.constData() + 24);
            }

            hasFormat = true;
        } else if (chunkId == "data") {
            format.dataStart = chunkDataStart;
            format.dataSize = static_cast<qint64>(chunkSize);
            hasData = true;
        }

        if (hasFormat && hasData) {
            break;
        }

        if (!file.seek(nextChunk)) {
            errorMessage = "Unable to seek inside WAV file.";
            return false;
        }
    }

    if (!hasFormat) {
        errorMessage = "WAV format chunk not found.";
        return false;
    }

    if (!hasData) {
        errorMessage = "WAV data chunk not found.";
        return false;
    }

    if (format.channels == 0 || format.sampleRate == 0 || format.blockAlign == 0) {
        errorMessage = "Invalid WAV stream parameters.";
        return false;
    }

    const bool supportedPcm =
        format.audioFormat == 1 &&
        (format.bitsPerSample == 8 ||
         format.bitsPerSample == 16 ||
         format.bitsPerSample == 24 ||
         format.bitsPerSample == 32);

    const bool supportedFloat =
        format.audioFormat == 3 &&
        format.bitsPerSample == 32;

    if (!supportedPcm && !supportedFloat) {
        errorMessage = "Unsupported WAV format. Use PCM 8/16/24/32-bit or 32-bit float.";
        return false;
    }

    if (!file.seek(format.dataStart)) {
        errorMessage = "Unable to seek to WAV audio data.";
        return false;
    }

    return true;
}

float decodeWavSample(const char *data, const WavStreamFormat &format)
{
    if (format.audioFormat == 3 && format.bitsPerSample == 32) {
        const quint32 raw = readUInt32Le(data);
        float value = 0.0f;
        std::memcpy(&value, &raw, sizeof(float));
        return qBound(-1.0f, value, 1.0f);
    }

    switch (format.bitsPerSample) {
    case 8: {
        const int value = static_cast<int>(static_cast<unsigned char>(data[0])) - 128;
        return static_cast<float>(value) / 128.0f;
    }
    case 16: {
        const quint16 raw = readUInt16Le(data);
        const qint16 value = static_cast<qint16>(raw);
        return static_cast<float>(value) / 32768.0f;
    }
    case 24:
        return static_cast<float>(readInt24Le(data)) / 8388608.0f;
    case 32: {
        const quint32 raw = readUInt32Le(data);
        const qint32 value = static_cast<qint32>(raw);
        return static_cast<float>(static_cast<double>(value) / 2147483648.0);
    }
    default:
        break;
    }

    return 0.0f;
}

QVector<float> convertWavBytesToMono(const QByteArray &bytes, const WavStreamFormat &format)
{
    QVector<float> samples;

    if (format.blockAlign == 0 || format.channels == 0) {
        return samples;
    }

    const int bytesPerSample = format.bitsPerSample / 8;
    const int frames = bytes.size() / static_cast<int>(format.blockAlign);

    samples.reserve(frames);

    for (int frame = 0; frame < frames; ++frame) {
        const char *frameData = bytes.constData() +
                                frame * static_cast<int>(format.blockAlign);

        double mono = 0.0;

        for (quint16 channel = 0; channel < format.channels; ++channel) {
            const char *sampleData = frameData + channel * bytesPerSample;
            mono += static_cast<double>(decodeWavSample(sampleData, format));
        }

        mono /= static_cast<double>(format.channels);
        samples.append(static_cast<float>(qBound(-1.0, mono, 1.0)));
    }

    return samples;
}


struct ParsedFt8Message
{
    QString text;
    QStringList parts;
    QString firstCall;
    QString secondCall;
    QString senderCall;
    QString grid;
    QString report;
    QString contestExchange;
    bool cq = false;
    bool addressedToMe = false;
    bool final73 = false;
    bool rr73 = false;
    bool rrr = false;
    bool contestExchangeLike = false;
    bool contestAck = false;
};

bool isFt8CallsignToken(const QString &token)
{
    return FtDecodedText::isCallsign(token);
}

bool isFt8AckLikeGridTrap(const QString &token)
{
    return FtDecodedText::isAckLikeGridTrap(token);
}

bool isFt8GridToken(const QString &token)
{
    return FtDecodedText::isGrid(token);
}

bool isFt8ReportToken(const QString &token)
{
    return FtDecodedText::isReport(token);
}

QString cleanFt8Report(QString report)
{
    return FtDecodedText::cleanReport(report);
}

QString formatFt8SignalReport(int snrDb, bool acknowledged)
{
    return FtDecodedText::formatSignalReport(snrDb, acknowledged);
}

ParsedFt8Message parseFt8MessageText(const QString &message, const QString &myCall)
{
    const FtDecodedText::ParsedMessage common = FtDecodedText::parse(message, myCall);

    ParsedFt8Message parsed;
    parsed.text = common.cleanText;
    parsed.parts = common.parts;
    parsed.firstCall = common.firstCall;
    parsed.secondCall = common.secondCall;
    parsed.senderCall = common.senderCall;
    parsed.grid = common.grid;
    parsed.report = common.report;
    parsed.contestExchange = common.contestExchange;
    parsed.cq = common.cq;
    parsed.addressedToMe = common.addressedToMe;
    parsed.final73 = common.final73;
    parsed.rr73 = common.rr73;
    parsed.rrr = common.rrr;
    parsed.contestExchangeLike = common.contestExchangeLike;
    parsed.contestAck = common.contestAck;
    return parsed;
}

bool isFt8CqMessage(const QString &message)
{
    return message.trimmed().toUpper().startsWith(QStringLiteral("CQ "));
}

QString ft8CqCallsignFromMessage(const ParsedFt8Message &parsed)
{
    if (!parsed.cq) {
        return QString();
    }

    for (const QString &part : parsed.parts) {
        if (part == QStringLiteral("CQ") || part.size() <= 2) {
            continue;
        }
        // FT8 allows directed CQs such as "CQ DX", "CQ NA" and "CQ POTA".
        // The first actual callsign after any such qualifier is the station to
        // label on the waterfall.
        if (isFt8CallsignToken(part)) {
            return part;
        }
    }

    return parsed.firstCall;
}

bool isFt8DirectReplyToMyCall(const ParsedFt8Message &parsed, const QString &myCall)
{
    const QString my = myCall.trimmed().toUpper();
    if (my.isEmpty() || parsed.cq || !parsed.addressedToMe || parsed.senderCall.isEmpty()) {
        return false;
    }

    return !FtDecodedText::callMatches(parsed.senderCall, my);
}

struct Ft8DecodeDisplayInfo
{
    QString callsign;
    QString locator;
    QString country;
    QString countryDxcc;
    QString countryTooltip;
};

QString ft8PrimaryCallsignForDisplay(const ParsedFt8Message &parsed, const QString &myCall)
{
    QString call;
    if (parsed.cq) {
        call = ft8CqCallsignFromMessage(parsed);
    } else if (!parsed.senderCall.isEmpty()) {
        call = parsed.senderCall;
    }

    if (call.isEmpty()) {
        const QString my = myCall.trimmed().toUpper();
        for (const QString &part : parsed.parts) {
            if (part == QStringLiteral("CQ") || FtDecodedText::callMatches(part, my)) {
                continue;
            }
            if (isFt8CallsignToken(part)) {
                call = part;
                break;
            }
        }
    }
    return call.trimmed().toUpper();
}

QStringList ft8HighlightCandidateCallsigns(const ParsedFt8Message &parsed, const QString &myCall)
{
    const QString my = myCall.trimmed().toUpper();
    QStringList calls;
    auto addCall = [&](QString call) {
        call = call.trimmed().toUpper();
        if (call.isEmpty() || !isFt8CallsignToken(call) || FtDecodedText::callMatches(call, my)) {
            return;
        }
        for (const QString &existing : calls) {
            if (FtDecodedText::callMatches(existing, call)) {
                return;
            }
        }
        calls << call;
    };

    if (parsed.cq) {
        addCall(ft8CqCallsignFromMessage(parsed));
    }
    addCall(parsed.senderCall);
    addCall(parsed.firstCall);
    addCall(parsed.secondCall);
    addCall(ft8PrimaryCallsignForDisplay(parsed, myCall));
    for (const QString &part : parsed.parts) {
        addCall(part);
    }
    return calls;
}

bool ft8GridIsDisplaySafe(const ParsedFt8Message &parsed)
{
    const QString grid = parsed.grid.trimmed().left(4).toUpper();
    if (!FtDecodedText::isGrid(grid) || FtDecodedText::isAckLikeGridTrap(grid)) {
        return false;
    }
    if (parsed.final73 || parsed.rr73 || parsed.rrr || !parsed.report.isEmpty() || parsed.contestAck || parsed.contestExchangeLike) {
        return false;
    }
    return true;
}


QString ft8MaidenheadGrid4(const QString &grid)
{
    return QsoMapWidget::maidenheadGrid4(grid);
}

bool ft8MaidenheadToLonLat(const QString &grid, QPointF *lonLat)
{
    return QsoMapWidget::maidenheadToLonLat(grid, lonLat);
}

double ft8DistanceKm(const QPointF &aLonLat, const QPointF &bLonLat)
{
    return QsoMapWidget::distanceKm(aLonLat, bLonLat);
}

Ft8DecodeDisplayInfo makeFt8DecodeDisplayInfo(const ParsedFt8Message &parsed,
                                              const QString &myCall,
                                              const QString &knownLocator)
{
    Ft8DecodeDisplayInfo info;
    info.callsign = ft8PrimaryCallsignForDisplay(parsed, myCall);

    if (ft8GridIsDisplaySafe(parsed)) {
        info.locator = parsed.grid.trimmed().left(4).toUpper();
    } else {
        const QString loc = knownLocator.trimmed().left(4).toUpper();
        if (FtDecodedText::isGrid(loc) && !FtDecodedText::isAckLikeGridTrap(loc)) {
            info.locator = loc;
        }
    }

    if (!info.callsign.isEmpty()) {
        const CtyCountryFile::LookupResult cty = CtyCountryFile::instance().lookupCallsign(info.callsign);
        if (cty.valid) {
            info.country = cty.entity.name;
            info.countryDxcc = cty.entity.dxcc;
            info.countryTooltip = QStringLiteral("DXCC %1, %2, prefix %3, CQ %4, ITU %5")
                                      .arg(cty.entity.dxcc,
                                           cty.entity.continent,
                                           cty.matchedToken,
                                           QString::number(cty.entity.cqZone),
                                           QString::number(cty.entity.ituZone));
        }
    }

    return info;
}


} // namespace

/**
 * @brief Compact analogue 30-second FT8 period clock.
 *
 * The dial shows one complete 30-second FT8 cycle. The two 15-second periods
 * are indicated by labels/ticks, while the background stays neutral so it does
 * not look like the selected TX/RX marker colors.
 */
class Ft8SlotClockWidget : public QWidget
{
public:
    explicit Ft8SlotClockWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(176, 176);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

    QSize sizeHint() const override
    {
        return QSize(196, 196);
    }

    void setSlotState(double cycleSeconds,
                      bool firstPeriodNow,
                      bool txWindow,
                      double slotLengthSeconds = 15.0,
                      double cycleLengthSeconds = 30.0,
                      const QString &modeLabel = QStringLiteral("FT8"))
    {
        m_slotLengthSeconds = qMax(0.25, slotLengthSeconds);
        m_cycleLengthSeconds = qMax(m_slotLengthSeconds * 2.0, cycleLengthSeconds);
        double normalized = std::fmod(cycleSeconds, m_cycleLengthSeconds);
        if (normalized < 0.0) {
            normalized += m_cycleLengthSeconds;
        }
        m_cycleSeconds = normalized;
        m_firstPeriodNow = firstPeriodNow;
        m_txWindow = txWindow;
        m_modeLabel = modeLabel.trimmed().isEmpty() ? QStringLiteral("FT") : modeLabel.trimmed().toUpper();
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const int side = qMin(width(), height()) - 8;
        const QRectF rect((width() - side) * 0.5,
                          (height() - side) * 0.5,
                          side,
                          side);
        const QPointF center = rect.center();
        const qreal radius = rect.width() * 0.5;
        const QRectF dialRect = rect.adjusted(5, 5, -5, -5);

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(47, 55, 62));
        painter.drawEllipse(rect.adjusted(1, 1, -1, -1));

        painter.setBrush(QColor(205, 220, 230));
        painter.drawEllipse(dialRect);

        // Draw the two UTC T/R periods explicitly.  For FT8 this is 0-15 / 15-30 s;
        // for FT4 it is 0-7.5 / 7.5-15 s.  The hand and scheduler use the same
        // slot/cycle values, so the analogue dial can no longer stay FT8-centric.
        const qreal startAngle = 90.0 * 16.0;
        const qreal halfSpan = -180.0 * 16.0;
        painter.setBrush(QColor(210, 234, 219, 130));
        painter.drawPie(dialRect.adjusted(2, 2, -2, -2), static_cast<int>(startAngle), static_cast<int>(halfSpan));
        painter.setBrush(QColor(230, 219, 219, 110));
        painter.drawPie(dialRect.adjusted(2, 2, -2, -2), static_cast<int>(startAngle + halfSpan), static_cast<int>(halfSpan));

        painter.setPen(QPen(QColor(105, 119, 128), 2.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(dialRect);
        painter.setPen(QPen(QColor(105, 119, 128), 1.6));
        painter.drawLine(QPointF(center.x(), dialRect.top() + 3), QPointF(center.x(), dialRect.bottom() - 3));

        painter.setPen(QPen(QColor(38, 50, 58), 1.2));
        QFont f = painter.font();
        f.setBold(true);
        f.setPointSize(qMax(7, height() / 19));
        painter.setFont(f);
        painter.drawText(QRectF(center.x() + radius * 0.25, center.y() - radius * 0.12,
                                radius * 0.40, radius * 0.24),
                         Qt::AlignCenter,
                         QStringLiteral("I"));
        painter.drawText(QRectF(center.x() - radius * 0.65, center.y() - radius * 0.12,
                                radius * 0.40, radius * 0.24),
                         Qt::AlignCenter,
                         QStringLiteral("II"));

        QFont small = painter.font();
        small.setBold(true);
        small.setPointSize(qMax(6, height() / 22));
        painter.setFont(small);
        painter.drawText(QRectF(center.x() - radius * 0.45, center.y() - radius * 0.18,
                                radius * 0.90, radius * 0.25),
                         Qt::AlignCenter,
                         m_modeLabel);

        const int tickCount = qBound(15, static_cast<int>(std::lround(m_cycleLengthSeconds * 2.0)), 60);
        for (int i = 0; i < tickCount; ++i) {
            const double fraction = static_cast<double>(i) / static_cast<double>(tickCount);
            const double degrees = -90.0 + fraction * 360.0;
            const double radians = qDegreesToRadians(degrees);
            const bool major = (i == 0) || (std::abs(fraction - 0.5) < (0.51 / tickCount)) || (i % qMax(1, tickCount / 6) == 0);
            const qreal outer = radius * 0.88;
            const qreal inner = major ? radius * 0.73 : radius * 0.81;
            const QPointF a(center.x() + qCos(radians) * outer,
                            center.y() + qSin(radians) * outer);
            const QPointF b(center.x() + qCos(radians) * inner,
                            center.y() + qSin(radians) * inner);
            painter.setPen(QPen(QColor(45, 58, 66, major ? 230 : 145), major ? 1.7 : 1.0));
            painter.drawLine(a, b);
        }

        const double handDegrees = -90.0 + (m_cycleSeconds / qMax(1.0, m_cycleLengthSeconds)) * 360.0;
        const double handRadians = qDegreesToRadians(handDegrees);
        const QPointF handEnd(center.x() + qCos(handRadians) * radius * 0.70,
                              center.y() + qSin(handRadians) * radius * 0.70);
        const QPointF tail(center.x() - qCos(handRadians) * radius * 0.16,
                           center.y() - qSin(handRadians) * radius * 0.16);
        painter.setPen(QPen(m_txWindow ? QColor(220, 60, 60) : QColor(30, 150, 78), 3.0, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(tail, handEnd);
        painter.setPen(QPen(QColor(10, 10, 10), 1.4));
        painter.setBrush(QColor(245, 245, 245));
        painter.drawEllipse(center, 4.5, 4.5);

        painter.setPen(QPen(m_firstPeriodNow ? QColor(30, 150, 78) : QColor(200, 68, 68), 2.5));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(rect.adjusted(1.5, 1.5, -1.5, -1.5));
    }

private:
    double m_cycleSeconds = 0.0;
    double m_slotLengthSeconds = 15.0;
    double m_cycleLengthSeconds = 30.0;
    bool m_firstPeriodNow = true;
    bool m_txWindow = false;
    QString m_modeLabel = QStringLiteral("FT8");
};


// -----------------------------------------------------------------------------
// Construction
// -----------------------------------------------------------------------------

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_audioEngine(new AudioEngine(this)),
    m_txAudioEngine(new TxAudioEngine(this)),
    m_ftTxWorker(new FtTxWorker()),
    m_ftTxThread(new QThread(this)),
    m_dspEngine(new DspEngine()),
    m_dspThread(new QThread(this)),
    m_weatherFaxDecoder(new WeatherFaxDecoder(this)),
    m_sstvDecoder(new SstvDecoder(this)),
    m_rttyDecoder(new RttyDecoder(this)),
    m_rttyMultiDecoder(new RttyMultiDecoder(this)),
    m_bpsk31Decoder(new Bpsk31Decoder(this)),
    m_mfskDecoder(new MfskDecoder(this)),
    m_cwDecoder(new CwDecoder(this)),
    m_cwSecondaryDecoder(nullptr),
    m_hellDecoder(new HellschreiberDecoder(this)),
    m_msk144Decoder(new Msk144Decoder(this)),
    m_ft8RxDecoder(new Ft8RxDecoder()),
    m_ft8RxThread(new QThread(this)),
    m_ftSlotScheduler(new FtSlotScheduler()),
    m_ftSlotThread(new QThread(this)),
    m_ntpClient(new NtpClient(this)),
    m_rigController(new HamlibController()),
    m_rigThread(new QThread(this)),
    m_ddspController(new DeepDspController(this))
{
    ui->setupUi(this);
    setWindowIcon(QIcon(":/icons/madmodem.png"));

    if (m_dspEngine != nullptr && m_dspThread != nullptr) {
        m_dspEngine->moveToThread(m_dspThread);
        connect(m_dspThread, &QThread::finished,
                m_dspEngine, &QObject::deleteLater);
        m_dspThread->start();
    }

    // FT4/FT8 transmit audio is isolated in a dedicated worker thread, MSHV-style.
    // The main/UI thread may arm messages and update controls, but it must not build
    // or run time-critical FT TX audio at the slot edge.
    qRegisterMetaType<TxModulator *>("TxModulator*");
    qRegisterMetaType<QVector<float>>("QVector<float>");
    if (m_ftTxWorker != nullptr && m_ftTxThread != nullptr) {
        m_ftTxWorker->moveToThread(m_ftTxThread);
        connect(m_ftTxThread, &QThread::finished,
                m_ftTxWorker, &QObject::deleteLater);
        m_ftTxThread->start();
    }
    // FT4/FT8 RX decode is isolated from MainWindow, closer to WSJT-X's
    // separate jt9 worker/process role.  Audio blocks are queued into this
    // worker object; decoded messages come back through queued signals.
    if (m_ft8RxDecoder != nullptr && m_ft8RxThread != nullptr) {
        m_ft8RxDecoder->moveToThread(m_ft8RxThread);
        connect(m_ft8RxThread, &QThread::finished,
                m_ft8RxDecoder, &QObject::deleteLater);
        m_ft8RxThread->start();
    }

    // FT4/FT8 slot timing is isolated from the UI.  This scheduler is the
    // single UTC-clock authority for RX/TX period phase and pending TX backend
    // pre-arm; MainWindow only receives thread-safe events and updates widgets.
    if (m_ftSlotScheduler != nullptr && m_ftSlotThread != nullptr) {
        m_ftSlotScheduler->moveToThread(m_ftSlotThread);
        connect(m_ftSlotThread, &QThread::finished,
                m_ftSlotScheduler, &QObject::deleteLater);
        connect(m_ftSlotScheduler, &FtSlotScheduler::slotUpdated,
                this, &MainWindow::handleFtSlotUpdated,
                Qt::QueuedConnection);
        connect(m_ftSlotScheduler, &FtSlotScheduler::txPttPrearmDue,
                this, &MainWindow::handleFtScheduledPttPrearmDue,
                Qt::QueuedConnection);
        connect(m_ftSlotScheduler, &FtSlotScheduler::txAudioStartDue,
                this, &MainWindow::handleFtScheduledTxDue,
                Qt::QueuedConnection);
        connect(m_ftSlotScheduler, &FtSlotScheduler::pendingStateChanged,
                this, &MainWindow::handleFtSchedulerPendingChanged,
                Qt::QueuedConnection);
        m_ftSlotThread->start();
        QMetaObject::invokeMethod(m_ftSlotScheduler, "startClock", Qt::QueuedConnection);
    }


    // CAT/Hamlib is isolated in its own worker thread.  No Hamlib call may run
    // in the UI/audio path; requests are queued and results come back via
    // signals.  This keeps FT/RTTY/SSTV/WEFAX audio timing independent of CAT.
    if (m_rigController != nullptr && m_rigThread != nullptr) {
        m_rigController->moveToThread(m_rigThread);
        connect(m_rigThread, &QThread::finished,
                m_rigController, &QObject::deleteLater);
        m_rigThread->start();
    }

    m_settings.load();
    const QString configuredLogbookPath = m_settings.logbookFilePath.trimmed();
    m_logbook.setFileName(configuredLogbookPath.isEmpty()
                              ? AdifLogbook::defaultPath()
                              : configuredLogbookPath);

    QString logbookError;
    if (!m_logbook.load(&logbookError)) {
        appendLog("Logbook load failed: " + logbookError);
    }

    // Load language before creating dynamic pages/tooltips.
    // A large part of the Mode tab is built in C++ with uiText(), so doing this
    // only after setupUiState() left many labels and tooltips in English until
    // restart or manual refresh.
    loadUiLanguageSetting();
    loadUiTranslationFile(m_uiLanguageCode);
    applyUiAppearanceSettings();

    setupUiState();
    setupCustomWidgets();
    // Do not globally scale the QMainWindow. The v1.45-v1.50 reference-screen
    // scaler was too aggressive for the main workspace: it modified many
    // child minimum/maximum sizes after the UI had already been tuned for
    // Linux 2880x1800, which distorted the FT/side/waterfall proportions on
    // both Linux and Windows. Keep the main window layout native/adaptive;
    // dialog-only scaling remains available for fixed-size utility windows.
    setupHelpTooltips();
    setupProcessingConnections();
    setupUiConnections();
    setupCatRotatorModule();
    setupCatRotatorSideTab();
    if (ui != nullptr && ui->sideTabWidget != nullptr && m_ddspController != nullptr) {
        m_ddspPanelWidget = new DdspPanelWidget(m_ddspController, this);
        ui->sideTabWidget->addTab(m_ddspPanelWidget, uiText("tab_ddsp", "MIND"));
        connect(m_ddspController, &DeepDspController::logMessage,
                this, [this](const QString &message) { appendLog(message); });

        auto syncMindDecoderIntegration = [this](const DeepDspController::Status &status) {
            if (m_ft8RxDecoder == nullptr && m_msk144Decoder == nullptr) {
                return;
            }
            const QString currentMode = (ui != nullptr && ui->cmbMode != nullptr)
                ? ui->cmbMode->currentText()
                : QString();
            const bool mindMode = modeSupportsMind(currentMode);
            const QString assistMode = status.assistMode.trimmed().toLower();
            const bool hardBypass = !mindMode || !status.enabled || assistMode == QStringLiteral("off");
            const bool modelLoaded = status.modelStateText == QStringLiteral("Model loaded");
            const bool assistedRequested = assistMode == QStringLiteral("assisted");
            const bool assistedActive = status.assistEnabled;

            // MIND is exposed only for FT4/FT8 and MSK144 candidate ranking.
            // CW/RTTY/BPSK/etc. remain fully classical and bypassed.
            const bool scoringEnabled = !hardBypass && modelLoaded &&
                                        (assistMode == QStringLiteral("shadow") || assistedRequested);
            const bool sampleExportEnabled = !hardBypass;
            const bool assistedRankingEnabled = !hardBypass && assistedActive && modelLoaded;
            if (m_ft8RxDecoder != nullptr) {
                m_ft8RxDecoder->setMindIntegrationState(!(Ft8Mode::isFamilyMode(currentMode)) || hardBypass,
                                                        scoringEnabled && Ft8Mode::isFamilyMode(currentMode),
                                                        sampleExportEnabled && Ft8Mode::isFamilyMode(currentMode),
                                                        assistedRankingEnabled && Ft8Mode::isFamilyMode(currentMode));
            }
            if (m_msk144Decoder != nullptr) {
                m_msk144Decoder->setMindIntegrationState(!Msk144Mode::isMode(currentMode) || hardBypass,
                                                         scoringEnabled && Msk144Mode::isMode(currentMode),
                                                         sampleExportEnabled && Msk144Mode::isMode(currentMode),
                                                         assistedRankingEnabled && Msk144Mode::isMode(currentMode));
            }
        };
        syncMindDecoderIntegration(m_ddspController->status());
        connect(m_ddspController, &DeepDspController::statusChanged,
                this, syncMindDecoderIntegration,
                Qt::QueuedConnection);

        QPointer<DeepDspController> mindController(m_ddspController);
        if (m_ft8RxDecoder != nullptr) {
            m_ft8RxDecoder->setMindAssistCallback(
                [mindController](const QVector<float> &candidateMagnitudes,
                                 QVector<float> *predictedBits,
                                 double *confidencePercent) {
                    if (mindController.isNull()) {
                        return false;
                    }
                    return mindController->scoreNativeFtCandidate(candidateMagnitudes,
                                                                  predictedBits,
                                                                  confidencePercent);
                });
        }
        if (m_msk144Decoder != nullptr) {
            m_msk144Decoder->setMindScoreCallback(
                [mindController](const QVector<float> &candidateFeatures,
                                 double *confidencePercent,
                                 bool *mayPromote) {
                    if (mindController.isNull()) {
                        return false;
                    }
                    return mindController->scoreMsk144Candidate(candidateFeatures,
                                                               confidencePercent,
                                                               mayPromote);
                });
            m_msk144Decoder->setMindSampleCallback(
                [mindController](const QVector<float> &candidateFeatures,
                                 bool decodeSucceeded,
                                 const QString &message) {
                    if (mindController.isNull()) {
                        return;
                    }
                    mindController->submitNativeMsk144Sample(candidateFeatures,
                                                            decodeSucceeded,
                                                            message);
                });
        }
    }
    updateMindUiForMode(ui != nullptr && ui->cmbMode != nullptr ? ui->cmbMode->currentText() : QString());
    setupModeMenu();
    refreshQsoMaps();
    setupLanguageMenu();
    applyUiLanguage();
    MadModemUi::polishCockpitWidgetTree(this);

    m_pttTestTimer.setSingleShot(true);
    m_ft8FullAutoCqSelectionTimer.setSingleShot(true);
    connect(&m_ft8FullAutoCqSelectionTimer, &QTimer::timeout,
            this, &MainWindow::processFt8FullAutoCqCandidates);
    m_qsoUtcTimer.setInterval(1000);
    connect(&m_qsoUtcTimer, &QTimer::timeout,
            this, &MainWindow::updateQsoUtcFields);
    m_qsoUtcTimer.start();

    m_bandSchedulerTimer.setInterval(1000);
    connect(&m_bandSchedulerTimer, &QTimer::timeout,
            this, &MainWindow::handleBandSchedulerTick);
    m_bandSchedulerTimer.start();

    if (m_ntpClient != nullptr) {
        m_ntpClient->setRefreshInterval(60000);
        m_ntpClient->setMaxRtt(2000.0);
        m_ntpClient->setEnabled(true);
    }
    updateQsoUtcFields();
    updateFt8SlotStatus();

    refreshDevices();
    applyPersistentSettingsToRuntime();
    updateWaterfallMarkers();
    updateTxPreview();

    appendLog("MM started.");
    appendLog("Settings file: " + AppSettings::settingsFilePath());
    appendLog("Receiver is ready. Use Status → ▶ RX to start live decoding, or analyze a WAV test file from the mode panel.");
    updateMainStateButton();
}

MainWindow::~MainWindow()
{
    shutdownRuntime("destructor");
    qWarning().noquote() << QStringLiteral("[shutdown] deleting UI");
    delete ui;
    ui = nullptr;
    qWarning().noquote() << QStringLiteral("[shutdown] MainWindow destructor done");
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    shutdownRuntime("closeEvent");
    QMainWindow::closeEvent(event);
}

void MainWindow::shutdownLog(const QString &message) const
{
    const QString line = QStringLiteral("[shutdown] ") + message;
    qWarning().noquote() << line;
    if (ui != nullptr && ui->txtLog != nullptr) {
        const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"));
        ui->txtLog->appendPlainText(QStringLiteral("[%1] %2").arg(timestamp, line));
    }
}

void MainWindow::shutdownRuntime(const char *reason)
{
    if (m_runtimeShutdownComplete) {
        return;
    }
    if (m_shutdownInProgress) {
        shutdownLog(QStringLiteral("recursive shutdown ignored"));
        return;
    }
    m_shutdownInProgress = true;

    shutdownLog(QStringLiteral("begin (%1)").arg(QString::fromLatin1(reason != nullptr ? reason : "unknown")));

    auto invokeStop = [](QObject *object, const char *method, Qt::ConnectionType queuedType = Qt::BlockingQueuedConnection) {
        if (object == nullptr || method == nullptr) {
            return;
        }
        const Qt::ConnectionType type = (object->thread() == QThread::currentThread())
            ? Qt::DirectConnection
            : queuedType;
        QMetaObject::invokeMethod(object, method, type);
    };
    auto stopThread = [this](QThread *thread, const QString &name) {
        if (thread == nullptr) {
            return;
        }
        shutdownLog(QStringLiteral("stopping thread: %1").arg(name));
        thread->requestInterruption();
        thread->quit();
        if (!thread->wait(3000)) {
            shutdownLog(QStringLiteral("thread did not stop within 3000 ms: %1; forcing terminate").arg(name));
            thread->terminate();
            thread->wait(1000);
        }
    };

    m_pttTestTimer.stop();
    m_ft8FullAutoCqSelectionTimer.stop();
    m_qsoUtcTimer.stop();
    m_bandSchedulerTimer.stop();

    shutdownLog(QStringLiteral("disconnecting queued UI callbacks"));
    disconnect(nullptr, nullptr, this, nullptr);

    if (m_ftSlotScheduler != nullptr) {
        shutdownLog(QStringLiteral("stopping FT slot scheduler"));
        invokeStop(m_ftSlotScheduler, "cancelTransmission", Qt::QueuedConnection);
        invokeStop(m_ftSlotScheduler, "stopClock");
        disconnect(m_ftSlotScheduler, nullptr, nullptr, nullptr);
    }

    if (m_catRotatorController != nullptr) {
        shutdownLog(QStringLiteral("stopping CatRotator"));
        m_catRotatorController->disconnect(this);
        QMetaObject::invokeMethod(m_catRotatorController, "stop", Qt::DirectConnection);
        QMetaObject::invokeMethod(m_catRotatorController, "disconnectRotator", Qt::DirectConnection);
    }

    if (m_hellDecoder != nullptr) disconnect(m_hellDecoder, nullptr, this, nullptr);
    if (m_rttyDecoder != nullptr) disconnect(m_rttyDecoder, nullptr, this, nullptr);
    if (m_rttyMultiDecoder != nullptr) disconnect(m_rttyMultiDecoder, nullptr, this, nullptr);
    if (m_bpsk31Decoder != nullptr) disconnect(m_bpsk31Decoder, nullptr, this, nullptr);
    if (m_mfskDecoder != nullptr) disconnect(m_mfskDecoder, nullptr, this, nullptr);
    if (m_cwDecoder != nullptr) disconnect(m_cwDecoder, nullptr, this, nullptr);
    if (m_cwSecondaryDecoder != nullptr) disconnect(m_cwSecondaryDecoder, nullptr, this, nullptr);
    if (m_msk144Decoder != nullptr) disconnect(m_msk144Decoder, nullptr, this, nullptr);
    if (m_q65Decoder != nullptr) disconnect(m_q65Decoder, nullptr, this, nullptr);
    if (m_ft8RxDecoder != nullptr) {
        disconnect(m_ft8RxDecoder, nullptr, this, nullptr);
        invokeStop(m_ft8RxDecoder, "reset", Qt::QueuedConnection);
    }

    endTextTxHighlight();

    if (m_audioEngine != nullptr) {
        shutdownLog(QStringLiteral("stopping RX audio"));
        disconnect(m_audioEngine, nullptr, this, nullptr);
        m_audioEngine->stopInput();
    }

    if (m_txAudioEngine != nullptr) {
        shutdownLog(QStringLiteral("stopping TX audio"));
        disconnect(m_txAudioEngine, nullptr, this, nullptr);
        m_txAudioEngine->stopOutput();
    }

    if (m_ftTxWorker != nullptr) {
        shutdownLog(QStringLiteral("stopping FT TX worker"));
        disconnect(m_ftTxWorker, nullptr, this, nullptr);
        invokeStop(m_ftTxWorker, "stopOutput");
    }
    stopThread(m_ftTxThread, QStringLiteral("FT TX"));
    m_ftTxThread = nullptr;
    m_ftTxWorker = nullptr;

    if (m_ftSlotScheduler != nullptr) {
        disconnect(m_ftSlotScheduler, nullptr, this, nullptr);
    }
    stopThread(m_ftSlotThread, QStringLiteral("FT slot scheduler"));
    m_ftSlotThread = nullptr;
    m_ftSlotScheduler = nullptr;

    stopThread(m_ft8RxThread, QStringLiteral("FT RX decoder"));
    m_ft8RxThread = nullptr;
    m_ft8RxDecoder = nullptr;

    if (m_pttSerial.isOpen()) {
        shutdownLog(QStringLiteral("closing serial PTT"));
        m_pttSerial.setRequestToSend(false);
        m_pttSerial.setDataTerminalReady(false);
        m_pttSerial.close();
    }

    if (m_ntpClient != nullptr) {
        shutdownLog(QStringLiteral("stopping NTP client"));
        m_ntpClient->setEnabled(false);
        disconnect(m_ntpClient, nullptr, this, nullptr);
    }

    if (m_rigController != nullptr) {
        shutdownLog(QStringLiteral("disconnecting CAT/Hamlib"));
        invokeRigPttBlocking(false);
        invokeStop(m_rigController, "disconnectRig");
        disconnect(m_rigController, nullptr, this, nullptr);
    }
    stopThread(m_rigThread, QStringLiteral("CAT/Hamlib"));
    m_rigThread = nullptr;
    m_rigController = nullptr;

    if (m_ddspController != nullptr) {
        shutdownLog(QStringLiteral("stopping MIND trainer"));
        disconnect(m_ddspController, nullptr, this, nullptr);
        m_ddspController->shutdown();
    }

    savePersistentSettings();
    // Do not rewrite the ADIF logbook on shutdown.  A user may point MM to a
    // large external ADIF file; rewriting it just because the app closes can
    // destroy unsupported fields or shrink the file if the parser has a bug.
    // Append/delete/import operations save explicitly when they actually modify
    // the logbook.

    stopThread(m_dspThread, QStringLiteral("DSP"));
    m_dspThread = nullptr;
    m_dspEngine = nullptr;

    m_runtimeShutdownComplete = true;
    m_shutdownInProgress = false;
    shutdownLog(QStringLiteral("done"));
}

// -----------------------------------------------------------------------------
// UI setup
// -----------------------------------------------------------------------------

void MainWindow::setupUiState()
{
    setWindowTitle(QStringLiteral(MADMODEM_VERSION_DISPLAY));

    if (ui->menubar != nullptr) {
        // Keep menus as in-process Qt widgets. Native/global menu behaviour can
        // break popup stacking in the frameless fullscreen cockpit window on
        // some desktop environments.
        ui->menubar->setNativeMenuBar(false);
    }

    ui->cmbMode->clear();
    // Mode names are operational identifiers used by the modem routing logic.
    // Do not run them through the generic combo-item translator: labels such as
    // SSTV/RTTY/FT8 must stay canonical and must never be replaced by unrelated
    // strings harvested from filenames such as "_sstv".
    ui->cmbMode->setProperty("i18nSkipComboItems", true);
    ui->cmbMode->addItem(WeatherFaxDecoder::modeName());
    ui->cmbMode->addItem(SstvModeDefinition::modeName());
    ui->cmbMode->addItem(RttyDecoder::modeName());
    ui->cmbMode->addItem(Bpsk31Decoder::modeName());
    ui->cmbMode->addItem(MfskDecoder::modeName());
    ui->cmbMode->addItem(CwDecoder::modeName());
    ui->cmbMode->addItem(HellschreiberDecoder::modeName());
    ui->cmbMode->addItem(Msk144Mode::modeName());
    ui->cmbMode->addItem(Q65Mode::familyName());
    for (const QString &ftModeName : Ft8Mode::allModeNames()) {
        ui->cmbMode->addItem(ftModeName);
    }

    if (ui->grpOperation != nullptr) {
        ui->grpOperation->setVisible(false);
        ui->grpOperation->setMinimumHeight(0);
        ui->grpOperation->setMaximumHeight(0);
        ui->grpOperation->setTitle(QString());
    }

    if (ui->lblMode != nullptr) {
        ui->lblMode->setVisible(false);
    }

    if (ui->cmbMode != nullptr) {
        ui->cmbMode->setVisible(false);
    }

    if (ui->lblRxControls != nullptr) {
        ui->lblRxControls->setVisible(false);
    }

    if (ui->lblTxControls != nullptr) {
        ui->lblTxControls->setVisible(false);
    }

    if (ui->btnStopRx != nullptr) {
        ui->btnStopRx->setVisible(false);
    }

    if (ui->btnTxTone != nullptr) {
        ui->btnTxTone->setVisible(false);
    }

    if (ui->btnTestPtt != nullptr) {
        ui->btnTestPtt->setVisible(false);
    }

    if (ui->lblConfigHint != nullptr) {
        ui->lblConfigHint->setVisible(false);
    }

    if (ui->lblAppStatusCaption != nullptr) {
        ui->lblAppStatusCaption->setVisible(false);
    }

    if (ui->lblAppStatus != nullptr) {
        ui->lblAppStatus->setVisible(false);
    }

    if (ui->btnStartRx != nullptr) {
        ui->btnStartRx->setMinimumWidth(0);
        ui->btnStartRx->setMinimumHeight(34);
        ui->btnStartRx->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    if (ui->grpWaterfall != nullptr) {
        ui->grpWaterfall->setMinimumHeight(170);
        ui->grpWaterfall->setMaximumHeight(QWIDGETSIZE_MAX);
        ui->grpWaterfall->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }
    if (ui->grpWaterfall != nullptr) {
        ui->grpWaterfall->setProperty("cockpitUntitled", true);
        ui->grpWaterfall->setContentsMargins(0, 0, 0, 0);
    }
    if (ui->frameWaterfall != nullptr) {
        ui->frameWaterfall->setMinimumHeight(150);
        ui->frameWaterfall->setMaximumHeight(QWIDGETSIZE_MAX);
        ui->frameWaterfall->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        ui->frameWaterfall->setContentsMargins(0, 0, 0, 0);
        ui->frameWaterfall->setFrameShape(QFrame::NoFrame);
    }
    if (ui->waterfallVerticalLayout != nullptr) {
        ui->waterfallVerticalLayout->setContentsMargins(0, 0, 0, 0);
        ui->waterfallVerticalLayout->setSpacing(0);
    }
    if (ui->mainLeftVerticalLayout != nullptr) {
        ui->mainLeftVerticalLayout->setStretch(0, 4);
        ui->mainLeftVerticalLayout->setStretch(1, 1);
        ui->mainLeftVerticalLayout->setSpacing(3);
    }

    ui->progressAudioLevel->setRange(0, 100);
    ui->progressAudioLevel->setValue(0);

    ui->lblAudioLevelDb->setText("-inf dB");
    ui->lblEstimatedFrequency->setText("Freq: -- Hz");
    ui->lblDecoderState->setText("Decoder: idle");
    ui->lblAppStatus->setText("Idle");

    setupBandSchedulerTab();

    populateWeatherFaxLinePresets();
    loadWeatherFaxSettingsToUi();
    loadSstvSettingsToUi();

    // Mode-specific DSP checkboxes live in the dedicated right-side DSP tab.

    if (ui->stkModeSettings != nullptr) {
        ui->stkModeSettings->setCurrentWidget(ui->pageFaxSettings);
    }

    setReceiverRunning(false);
    appendLog(QStringLiteral("CPU acceleration: %1").arg(MadModemCpu::summary()));
}

void MainWindow::setupCustomWidgets()
{
    QHBoxLayout *waterfallLayout = new QHBoxLayout(ui->frameWaterfall);
    waterfallLayout->setContentsMargins(0, 0, 0, 0);
    waterfallLayout->setSpacing(0);

    if (ui->progressAudioLevel != nullptr) {
        // Replaced by a compact LED-style VU meter beside the waterfall.
        ui->progressAudioLevel->setVisible(false);
        ui->progressAudioLevel->setMaximumWidth(0);
    }

    m_lblVuMeterDb = new QLabel(QStringLiteral("-inf dB"), ui->frameWaterfall);
    m_lblVuMeterDb->setVisible(false);
    m_lblVuMeterDb->setMaximumSize(0, 0);

    m_ledVuMeter = new LedVuMeterWidget(ui->frameWaterfall);
    m_ledVuMeter->setToolTip(uiText("audio_vu_meter_tooltip", "Input audio level. Green is safe, yellow is strong, red is near clipping."));
    waterfallLayout->addWidget(m_ledVuMeter, 0);

    m_waterfallWidget = new WaterfallWidget(ui->frameWaterfall);
    waterfallLayout->addWidget(m_waterfallWidget, 1);

    QVBoxLayout *displayLayout = new QVBoxLayout(ui->frameSstvImage);
    displayLayout->setContentsMargins(0, 0, 0, 0);
    displayLayout->setSpacing(0);

    m_mainDisplayStack = new QStackedWidget(ui->frameSstvImage);
    m_mainDisplayStack->setAutoFillBackground(false);
    m_mainDisplayStack->setStyleSheet(QString());
    displayLayout->addWidget(m_mainDisplayStack);

    m_imageDisplayPage = new QWidget(m_mainDisplayStack);
    QVBoxLayout *imageLayout = new QVBoxLayout(m_imageDisplayPage);
    imageLayout->setContentsMargins(0, 0, 0, 0);
    imageLayout->setSpacing(0);

    m_faxImageWidget = new FaxImageWidget(m_imageDisplayPage);
    imageLayout->addWidget(m_faxImageWidget);

    // v1.31/v1.42: SSTV is a QSO-bearing image mode and therefore gets the same
    // Qt Location / offline QSO map tab as the text/FT modes.  WEFAX is not a
    // QSO mode, so updateCentralDisplayForMode() removes the map tab and hides
    // the single-tab bar when MeteoFax/WEFAX is selected.
    m_imageDisplayPage = wrapTextDisplayPageWithMap(m_imageDisplayPage,
                                                    uiText("tab_sstv_image", "SSTV"),
                                                    QStringLiteral("SSTV"),
                                                    &m_sstvQsoMapWidget,
                                                    &m_imageDisplayTabs,
                                                    &m_sstvMapPage);
    setSstvQsoMapVisible(false);
    m_mainDisplayStack->addWidget(m_imageDisplayPage);

    if (ui->modeTabLayout != nullptr &&
        ui->tabModeSettings != nullptr &&
        ui->btnStartRx != nullptr &&
        ui->btnTxTone != nullptr) {
        QWidget *transportBar = new QWidget(ui->tabModeSettings);
        QHBoxLayout *transportLayout = new QHBoxLayout(transportBar);
        transportLayout->setContentsMargins(4, 4, 4, 4);
        transportLayout->setSpacing(6);

        ui->btnStartRx->setParent(transportBar);
        ui->btnStartRx->setVisible(true);
        ui->btnStartRx->setMinimumWidth(0);
        ui->btnStartRx->setMinimumHeight(30);
        ui->btnStartRx->setMaximumHeight(34);
        ui->btnStartRx->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        ui->btnTxTone->setParent(transportBar);
        ui->btnTxTone->setVisible(true);
        ui->btnTxTone->setMinimumWidth(0);
        ui->btnTxTone->setMinimumHeight(30);
        ui->btnTxTone->setMaximumHeight(34);
        ui->btnTxTone->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        transportLayout->addWidget(ui->btnStartRx, 1);
        transportLayout->addWidget(ui->btnTxTone, 1);
        ui->modeTabLayout->insertWidget(0, transportBar, 0);
    }

    if (ui->mainHorizontalSplitter != nullptr &&
        ui->sideTabWidget != nullptr) {
        const int sideIndex = ui->mainHorizontalSplitter->indexOf(ui->sideTabWidget);

        if (sideIndex >= 0) {
            QWidget *sideContainer = new QWidget(ui->mainHorizontalSplitter);
            // v1.18/v4.13ad: keep the side panel compact, but do not reserve a
            // separate global RX bar above the tabs.  RX/TX transport now lives
            // at the top of the Status tab, so all modes gain vertical room.
            // 0.5.1 keeps diagnostic Runtime Log access only in FT Mode.
            sideContainer->setMinimumWidth(280);
            sideContainer->setMaximumWidth(340);
            sideContainer->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

            QVBoxLayout *sideLayout = new QVBoxLayout(sideContainer);
            sideLayout->setContentsMargins(0, 0, 0, 0);
            sideLayout->setSpacing(0);

            ui->sideTabWidget->setParent(nullptr);
            sideLayout->addWidget(ui->sideTabWidget, 1);

            ui->mainHorizontalSplitter->insertWidget(sideIndex, sideContainer);
        }
    }

    setupTextTerminalPages();
    setupRttyPage();
    setupBpsk31Page();
    setupMfskPage();
    setupCwPage();
    setupHellPage();
    setupMsk144Page();
    setupQ65Page();
    // DSP tab removed: useful per-mode conditioning now lives inside Mode pages.
    // Keep the backend helpers for Software AGC and legacy settings migration.
    // setupDspTab();
    setupFt8Page();

    m_grpTxImage = new QGroupBox(QString(), ui->tabModeSettings);
    QGridLayout *txLayout = new QGridLayout(m_grpTxImage);
    txLayout->setContentsMargins(8, 8, 8, 8);
    txLayout->setHorizontalSpacing(6);
    txLayout->setVerticalSpacing(6);

    QLabel *txImageCaption = new QLabel("Image", m_grpTxImage);
    txImageCaption->setVisible(false);
    m_lblTxImageName = new QLabel("No TX image loaded", m_grpTxImage);
    m_lblTxImageName->setWordWrap(true);
    m_lblTxImageName->setVisible(false);

    QLabel *txModeCaption = new QLabel("TX mode", m_grpTxImage);
    txModeCaption->setVisible(false);
    m_lblTxMode = new QLabel("--", m_grpTxImage);
    m_lblTxMode->setWordWrap(true);
    m_lblTxMode->setVisible(false);

    m_lblSstvTxPreview = new QLabel(m_grpTxImage);
    m_lblSstvTxPreview->setMinimumSize(220, 110);
    m_lblSstvTxPreview->setMaximumHeight(145);
    m_lblSstvTxPreview->setFrameShape(QFrame::StyledPanel);
    m_lblSstvTxPreview->setAlignment(Qt::AlignCenter);
    m_lblSstvTxPreview->setText("SSTV preview");

    m_progressTx = new QProgressBar(m_grpTxImage);
    m_progressTx->setRange(0, 100);
    m_progressTx->setValue(0);
    m_progressTx->setTextVisible(true);

    m_btnLoadTxImage = new QPushButton("Load image...", m_grpTxImage);
    m_btnSstvEditor = new QPushButton("Open SSTV editor...", m_grpTxImage);
    m_btnStartImageTx = new QPushButton("Start image TX", m_grpTxImage);
    m_btnStopImageTx = new QPushButton("Stop TX", m_grpTxImage);
    m_btnStopImageTx->setEnabled(false);

    m_editSstvTxCall = new QLineEdit(m_grpTxImage);
    m_editSstvTxName = new QLineEdit(m_grpTxImage);
    m_editSstvTxQth = new QLineEdit(m_grpTxImage);
    m_editSstvTxReport = new QLineEdit(m_grpTxImage);
    m_editSstvTxCall->setPlaceholderText("Callsign");
    m_editSstvTxName->setPlaceholderText("Name");
    m_editSstvTxQth->setPlaceholderText("QTH");
    m_editSstvTxReport->setPlaceholderText("Report / note");

    Q_UNUSED(txImageCaption);
    Q_UNUSED(txModeCaption);
    txLayout->addWidget(m_btnLoadTxImage, 0, 0, 1, 2);
    txLayout->addWidget(m_btnSstvEditor, 0, 2, 1, 1);
    txLayout->addWidget(m_lblSstvTxPreview, 1, 0, 1, 3);
    m_lblSstvTxCall = new QLabel("Call", m_grpTxImage);
    m_lblSstvTxName = new QLabel("Name", m_grpTxImage);
    m_lblSstvTxQth = new QLabel("QTH", m_grpTxImage);
    m_lblSstvTxInfo = new QLabel("Info", m_grpTxImage);
    txLayout->addWidget(m_lblSstvTxCall, 2, 0);
    txLayout->addWidget(m_editSstvTxCall, 2, 1, 1, 2);
    txLayout->addWidget(m_lblSstvTxName, 3, 0);
    txLayout->addWidget(m_editSstvTxName, 3, 1, 1, 2);
    txLayout->addWidget(m_lblSstvTxQth, 4, 0);
    txLayout->addWidget(m_editSstvTxQth, 4, 1, 1, 2);
    txLayout->addWidget(m_lblSstvTxInfo, 5, 0);
    txLayout->addWidget(m_editSstvTxReport, 5, 1, 1, 2);
    txLayout->addWidget(m_progressTx, 6, 0, 1, 3);
    txLayout->addWidget(m_btnStartImageTx, 7, 0, 1, 2);
    txLayout->addWidget(m_btnStopImageTx, 7, 2);
    txLayout->setColumnStretch(1, 1);

    ui->modeTabLayout->addWidget(m_grpTxImage);

    m_btnFaxForceRx = new QPushButton(uiText("button.forceWefaxRx", "Force WEFAX RX now"), ui->grpFaxSettings);
    m_btnFaxForceRx->setMinimumHeight(34);
    m_btnFaxForceRx->setToolTip(uiText("tooltip.forceWefaxRx", "Start live RX and force MeteoFax to receive immediately, bypassing APT start/phasing wait."));
    if (ui->faxSettingsGridLayout != nullptr) {
        ui->faxSettingsGridLayout->addWidget(m_btnFaxForceRx, 24, 0, 1, 2);
    }

    m_btnSstvForceRx = new QPushButton(uiText("button.forceSstvRx", "Force SSTV RX now"), ui->grpSstvSettings);
    m_btnSstvForceRx->setVisible(false);
    m_btnSstvForceRx->setEnabled(false);

    // Keep the right-side tabs compact: Status holds live receiver/waterfall/clock
    // information, Mode contains only mode-specific controls.
    if (ui->grpLog != nullptr) {
        if (ui->statusTabLayout != nullptr) {
            ui->statusTabLayout->removeWidget(ui->grpLog);
        }
        ui->grpLog->setVisible(false);
        ui->grpLog->setMaximumHeight(0);
    }

    if (ui->statusTabLayout != nullptr) {
        ui->statusTabLayout->setAlignment(Qt::AlignTop);
        ui->statusTabLayout->setContentsMargins(8, 8, 8, 8);
        ui->statusTabLayout->setSpacing(12);
    }
    if (ui->grpStatusMeters != nullptr) {
        // The old Status page duplicated information and consumed too much space.
        // Audio level is now the vertical VU meter beside the waterfall.
        ui->grpStatusMeters->setVisible(false);
        ui->grpStatusMeters->setMaximumHeight(0);
    }
    if (ui->statusGridLayout != nullptr) {
        ui->statusGridLayout->setContentsMargins(14, 16, 14, 16);
        ui->statusGridLayout->setHorizontalSpacing(14);
        ui->statusGridLayout->setVerticalSpacing(14);
        for (int r = 0; r < 4; ++r) {
            ui->statusGridLayout->setRowStretch(r, 0);
            ui->statusGridLayout->setRowMinimumHeight(r, 30);
        }
        ui->statusGridLayout->setColumnStretch(0, 0);
        ui->statusGridLayout->setColumnStretch(1, 1);
    }
    const QList<QLabel *> statusLabels = {
        ui->lblAudioLevel,
        ui->lblAudioLevelCaption,
        ui->lblAudioLevelDb,
        ui->lblEstimatedFrequencyCaption,
        ui->lblEstimatedFrequency,
        ui->lblDecoderStateCaption,
        ui->lblDecoderState
    };
    for (QLabel *label : statusLabels) {
        if (label == nullptr) {
            continue;
        }
        label->setMinimumHeight(28);
        label->setAlignment(Qt::AlignVCenter | (label == ui->lblDecoderState ? Qt::AlignLeft : Qt::AlignLeft));
    }
    if (ui->lblDecoderState != nullptr) {
        ui->lblDecoderState->setWordWrap(false);
        ui->lblDecoderState->setMinimumWidth(260);
        ui->lblDecoderState->setToolTip(ui->lblDecoderState->text());
    }
    if (ui->progressAudioLevel != nullptr) {
        ui->progressAudioLevel->setMinimumHeight(120);
        ui->progressAudioLevel->setMaximumHeight(QWIDGETSIZE_MAX);
    }

    if (ui->modeTabLayout != nullptr && ui->stkModeSettings != nullptr && ui->stkModeSettings->parentWidget() != ui->tabModeSettings) {
        ui->stkModeSettings->setParent(ui->tabModeSettings);
        ui->modeTabLayout->insertWidget(0, ui->stkModeSettings, 1);
    }

    if (ui->modeTabLayout != nullptr && m_grpTxImage != nullptr && m_grpTxImage->parentWidget() != ui->tabModeSettings) {
        m_grpTxImage->setParent(ui->tabModeSettings);
        ui->modeTabLayout->addWidget(m_grpTxImage);
    }

    // Status tab cleanup: do not create the old Rig/CAT diagnostic block here.
    // CAT/PTT diagnostics remain in Settings and the FT runtime log when needed.

    if (ui->modeTabLayout != nullptr) {
        m_btnShowRuntimeLog = new QPushButton(uiText("runtime_log_button", "Runtime log"), ui->tabModeSettings);
        m_btnShowRuntimeLog->setToolTip(uiText("runtime_log_ft_tooltip", "Open the FT timing/CAT/PTT diagnostic log."));
        m_btnShowRuntimeLog->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        connect(m_btnShowRuntimeLog, &QPushButton::clicked, this, &MainWindow::showRuntimeLogDialog);
        ui->modeTabLayout->addWidget(m_btnShowRuntimeLog, 0);
        const QString modeName = (ui != nullptr && ui->cmbMode != nullptr) ? ui->cmbMode->currentText() : QString();
        m_btnShowRuntimeLog->setVisible(Ft8Mode::isFamilyMode(modeName) || Msk144Mode::isMode(modeName) || Q65Mode::isFamilyMode(modeName));
    }

    m_settings.waterfallColorScalePercent = 80;
    m_settings.waterfallPalette = QStringLiteral("madmodem");
    if (m_waterfallWidget != nullptr) {
        m_waterfallWidget->setColorScalePercent(80);
        m_waterfallWidget->setPaletteName(QStringLiteral("madmodem"));
    }

    if (ui->statusTabLayout != nullptr) {
        ui->statusTabLayout->addStretch(1);
    }

    if (ui->sideTabWidget != nullptr) {
        ui->sideTabWidget->setMinimumWidth(280);
        ui->sideTabWidget->setMaximumWidth(340);
        ui->sideTabWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        // Keep the native platform tab look.  Expanding tabs make Stato/Modo/Time
        // reach the right edge of the side panel instead of floating at the left.
        ui->sideTabWidget->setStyleSheet(QString());
        if (ui->sideTabWidget->tabBar() != nullptr) {
            ui->sideTabWidget->tabBar()->show();
            ui->sideTabWidget->tabBar()->setUsesScrollButtons(false);
            ui->sideTabWidget->tabBar()->setExpanding(true);
        }
        if (ui->tabModeSettings != nullptr) {
            const int modeTabIndex = ui->sideTabWidget->indexOf(ui->tabModeSettings);
            if (modeTabIndex >= 0) {
                ui->sideTabWidget->setTabText(modeTabIndex, uiText("tab_mode", "Mode"));
                ui->sideTabWidget->setCurrentIndex(modeTabIndex);
            }
        }
        if (ui->tabStatus != nullptr && ui->sideTabWidget->tabBar() != nullptr) {
            const int statusIndex = ui->sideTabWidget->indexOf(ui->tabStatus);
            if (statusIndex >= 0) {
                ui->sideTabWidget->tabBar()->setTabVisible(statusIndex, false);
            }
        }
    }
}



QWidget *MainWindow::wrapTextDisplayPageWithMap(QWidget *mainPage,
                                                const QString &mainTabTitle,
                                                const QString &modeFilter,
                                                QsoMapWidget **mapOut,
                                                QTabWidget **tabsOut,
                                                QWidget **mapPageOut)
{
    if (mainPage == nullptr) {
        return nullptr;
    }

    QTabWidget *tabs = new QTabWidget(m_mainDisplayStack);
    tabs->setDocumentMode(false);
    tabs->setTabPosition(QTabWidget::North);
    // Keep native platform tab styling; the map/image page itself owns its background.
    tabs->setStyleSheet(QString());
    if (tabs->tabBar() != nullptr) {
        tabs->tabBar()->setExpanding(false);
    }

    mainPage->setParent(tabs);
    tabs->addTab(mainPage, mainTabTitle.isEmpty() ? uiText("tab_activity", "Activity") : mainTabTitle);

    QWidget *mapPage = new QWidget(tabs);
    mapPage->setAutoFillBackground(false);
    mapPage->setStyleSheet(QStringLiteral("background-color: #050607; color: #ffb35a;"));
    QVBoxLayout *outer = new QVBoxLayout(mapPage);
    outer->setContentsMargins(8, 8, 8, 8);
    outer->setSpacing(6);

    QsoMapWidget *map = new QsoMapWidget(mapPage);
    map->setAutoFillBackground(false);
    map->setStyleSheet(QStringLiteral("background-color: #050607;"));

    // Keep the map controls inside the map canvas instead of reserving a
    // separate row above it.  The layout is owned by QsoMapWidget, so the
    // buttons sit on the upper edge of the blue map area and the parent page
    // gives all vertical space to the actual map.
    QHBoxLayout *tools = new QHBoxLayout(map);
    tools->setContentsMargins(10, 10, 10, 0);
    tools->setSpacing(6);

    QPushButton *mapLayers = new QPushButton(uiText("qso_map_layers", "Layers..."), map);
    mapLayers->setToolTip(uiText("qso_map_layers_tooltip", "Choose all visible QSO map layers, marker source and map filters."));
    QPushButton *refresh = new QPushButton(uiText("refresh", "Refresh"), map);
    QPushButton *reset = new QPushButton(uiText("reset_view", "Reset view"), map);
    QPushButton *save = new QPushButton(uiText("save_map", "Save map..."), map);
    QPushButton *print = new QPushButton(uiText("print_map", "Print map..."), map);

    tools->addStretch(1);
    tools->addWidget(mapLayers, 0, Qt::AlignTop);
    tools->addWidget(refresh, 0, Qt::AlignTop);
    tools->addWidget(reset, 0, Qt::AlignTop);
    tools->addWidget(save, 0, Qt::AlignTop);
    tools->addWidget(print, 0, Qt::AlignTop);
    tools->addStretch(0);
    map->setTextTranslator([this](const QString &source) {
        return uiTextFromSource(QStringLiteral("text"), source);
    });
    map->setModeFilter(modeFilter);
    map->setHomeGrid(m_settings.textMyLocator);
    map->setRecords(m_logbook.records());
    connect(mapLayers, &QPushButton::clicked, map, &QsoMapWidget::configureLayerSettings);
    connect(refresh, &QPushButton::clicked, this, &MainWindow::refreshQsoMaps);
    connect(reset, &QPushButton::clicked, map, &QsoMapWidget::resetView);
    connect(save, &QPushButton::clicked, map, &QsoMapWidget::saveMap);
    connect(print, &QPushButton::clicked, map, &QsoMapWidget::printMap);

    outer->addWidget(map, 1);
    tabs->addTab(mapPage, uiText("qso_map_tab", "QSO map"));

    m_qsoMapWidgets.append(map);
    if (mapOut != nullptr) {
        *mapOut = map;
    }
    if (tabsOut != nullptr) {
        *tabsOut = tabs;
    }
    if (mapPageOut != nullptr) {
        *mapPageOut = mapPage;
    }
    return tabs;
}

void MainWindow::setSstvQsoMapVisible(bool visible)
{
    if (m_imageDisplayTabs == nullptr || m_sstvMapPage == nullptr) {
        return;
    }

    const int existingIndex = m_imageDisplayTabs->indexOf(m_sstvMapPage);
    if (visible) {
        if (existingIndex < 0) {
            m_imageDisplayTabs->addTab(m_sstvMapPage, uiText("qso_map_tab", "QSO map"));
        }
        if (m_imageDisplayTabs->tabBar() != nullptr) {
            m_imageDisplayTabs->tabBar()->setVisible(true);
        }
        if (m_sstvQsoMapWidget != nullptr) {
            m_sstvQsoMapWidget->setModeFilter(QStringLiteral("SSTV"));
        }
        refreshQsoMaps();
    } else {
        if (existingIndex >= 0) {
            if (m_imageDisplayTabs->currentIndex() == existingIndex) {
                m_imageDisplayTabs->setCurrentIndex(0);
            }
            m_imageDisplayTabs->removeTab(existingIndex);
        }
        if (m_imageDisplayTabs->tabBar() != nullptr) {
            m_imageDisplayTabs->tabBar()->setVisible(false);
        }
    }
}

void MainWindow::refreshQsoMaps()
{
    const QVector<LogbookEntry> records = m_logbook.records();
    QString homeGrid = stationLocator();
    for (QsoMapWidget *map : m_qsoMapWidgets) {
        if (map == nullptr) {
            continue;
        }
        map->setHomeGrid(homeGrid);
        map->setRecords(records);
    }
}

void MainWindow::recordHeardStationForMaps(const QString &callsign,
                                         const QString &grid,
                                         const QString &mode,
                                         const QString &band,
                                         const QString &comment)
{
    const QString call = callsign.trimmed().toUpper();
    const QString locator = grid.trimmed().toUpper();
    if (call.isEmpty() || locator.size() < 4) {
        return;
    }
    if (!isFt8CallsignToken(call) || !isFt8GridToken(locator.left(4))) {
        return;
    }

    LogbookEntry entry;
    entry.callsign = call;
    entry.grid = locator;
    entry.mode = mode.trimmed().isEmpty() ? currentAdifMode() : mode.trimmed().toUpper();
    entry.band = band.trimmed().toLower();
    if (entry.band.isEmpty()) {
        if (ui != nullptr && Ft8Mode::isFamilyMode(ui->cmbMode->currentText()) && m_cmbFt8Band != nullptr) {
            entry.band = m_cmbFt8Band->currentText().trimmed().toLower();
        }
    }
    entry.comment = comment;
    entry.utc = QDateTime::currentDateTimeUtc();

    for (QsoMapWidget *map : m_qsoMapWidgets) {
        if (map != nullptr) {
            map->addHeardStation(entry);
        }
    }
}

void MainWindow::scanTextForHeardStations(QPlainTextEdit *terminal, const QString &newText)
{
    if (terminal == nullptr || newText.trimmed().isEmpty()) {
        return;
    }
    if (terminal != m_txtRttyRx && terminal != m_txtBpsk31Rx) {
        return;
    }

    QsoFormWidgets *form = qsoFormForTerminal(terminal);
    QString mode = QStringLiteral("RTTY");
    if (terminal == m_txtBpsk31Rx) {
        mode = (m_cmbBpsk31Variant != nullptr) ? m_cmbBpsk31Variant->currentData().toString().toUpper() : QStringLiteral("BPSK31");
    }
    const QString band = (form != nullptr && form->band != nullptr) ? form->band->text().trimmed().toLower() : QString();

    QString text = terminal->toPlainText();
    if (text.size() > 1200) {
        text = text.right(1200);
    }
    const QRegularExpression tokenRe(QStringLiteral("\\b([A-Z0-9/]{3,16})\\b"));
    QRegularExpressionMatchIterator it = tokenRe.globalMatch(text.toUpper());
    QStringList tokens;
    while (it.hasNext()) {
        tokens << it.next().captured(1);
    }
    for (int i = 0; i < tokens.size(); ++i) {
        if (!isFt8CallsignToken(tokens.at(i))) {
            continue;
        }
        for (int j = i + 1; j < tokens.size() && j <= i + 6; ++j) {
            if (isFt8GridToken(tokens.at(j))) {
                recordHeardStationForMaps(tokens.at(i), tokens.at(j), mode, band, QStringLiteral("RX text terminal"));
                break;
            }
        }
    }
}

void MainWindow::updateFtQsoMapModeFilter()
{
    if (m_ftQsoMapWidget == nullptr || ui == nullptr || ui->cmbMode == nullptr) {
        return;
    }
    const QString mode = Ft8Mode::profileForMode(ui->cmbMode->currentText()).adifMode;
    m_ftQsoMapWidget->setModeFilter(mode);
    refreshQsoMaps();
}


QWidget *MainWindow::createTextTerminalPage(const QString &title,
                                            QPlainTextEdit **rxTerminal,
                                            QPlainTextEdit **txInput,
                                            QPushButton **clearRxButton,
                                            QPushButton **loadTextButton,
                                            QPushButton **clearInputButton,
                                            QPushButton **sendButton,
                                            QList<QPushButton *> *macroButtons,
                                            QsoFormWidgets **qsoForm)
{
    QWidget *page = new QWidget(m_mainDisplayStack);
    page->setAutoFillBackground(false);
    page->setStyleSheet(
        "QPlainTextEdit {"
        " padding: 4px;"
        " font-family: DejaVu Sans Mono, Consolas, monospace;"
        " font-size: 9pt;"
        "}"
        "QPushButton {"
        " padding: 3px 7px;"
        " min-height: 24px;"
        " font-size: 9pt;"
        "}"
        "QLabel {"
        " font-size: 9pt;"
        "}"
        );

    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(5);

    QLabel *caption = new QLabel(title, page);
    caption->setStyleSheet("font-weight: 500; font-size: 9pt;");

    *rxTerminal = new QPlainTextEdit(page);
    (*rxTerminal)->setReadOnly(true);
    (*rxTerminal)->setPlaceholderText("Received and transmitted text appears here.");
    (*rxTerminal)->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    (*rxTerminal)->setMinimumHeight(190);
    installRxTextContextMenu(*rxTerminal);

    QWidget *qsoPanel = createQsoFormPanel(page, title.section(' ', 0, 0), qsoForm);

    QGridLayout *macroLayout = new QGridLayout();
    macroLayout->setContentsMargins(0, 0, 0, 0);
    macroLayout->setHorizontalSpacing(4);
    macroLayout->setVerticalSpacing(4);

    if (macroButtons != nullptr) {
        macroButtons->clear();
        for (int i = 0; i < 6; ++i) {
            QPushButton *button = new QPushButton(QString("Macro %1").arg(i + 1), page);
            button->setMinimumHeight(26);
            button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            macroButtons->append(button);
            macroLayout->addWidget(button, i / 6, i % 6);
        }
    }

    QHBoxLayout *inputLayout = new QHBoxLayout();
    inputLayout->setContentsMargins(0, 0, 0, 0);
    inputLayout->setSpacing(6);

    *txInput = new QPlainTextEdit(page);
    (*txInput)->setMinimumHeight(50);
    (*txInput)->setMaximumHeight(68);
    (*txInput)->setPlaceholderText("Type text to transmit, then press ➤.");
    (*txInput)->setLineWrapMode(QPlainTextEdit::WidgetWidth);

    *sendButton = new QPushButton(QString::fromUtf8("➤"), page);
    (*sendButton)->setMinimumWidth(52);
    (*sendButton)->setMinimumHeight(50);
    (*sendButton)->setToolTip("Transmit the text typed in the input box.");

    inputLayout->addWidget(*txInput, 1);
    inputLayout->addWidget(*sendButton);

    /*
     * The old text-mode utility row (Clear terminal / Load text... / Clear
     * input) consumed a full strip of vertical space in the central work area.
     * Keep the buttons allocated for older signal/slot code paths and future
     * context-menu actions, but do not add them to the visible layout.
     */
    *clearRxButton = new QPushButton("Clear terminal", page);
    *loadTextButton = new QPushButton("Load text...", page);
    *clearInputButton = new QPushButton("Clear input", page);
    (*clearRxButton)->hide();
    (*loadTextButton)->hide();
    (*clearInputButton)->hide();

    layout->addWidget(caption);
    layout->addWidget(*rxTerminal, 1);
    // QSO logging fields belong in the right-side Mode tab, not inside the
    // central RX/TX work area.  Keep the form object alive here; each mode
    // setup page reparents it into its compact Mode panel.
    qsoPanel->setVisible(false);
    layout->addLayout(macroLayout);
    layout->addLayout(inputLayout);

    return page;
}

void MainWindow::placeQsoFormInModePanel(QVBoxLayout *layout, QsoFormWidgets *form)
{
    if (layout == nullptr || form == nullptr || form->container == nullptr) {
        return;
    }

    QWidget *panel = form->container;
    if (QWidget *oldParent = panel->parentWidget()) {
        if (QLayout *oldLayout = oldParent->layout()) {
            oldLayout->removeWidget(panel);
        }
    }

    panel->setParent(layout->parentWidget());
    panel->setVisible(true);
    panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    panel->setMaximumHeight(QWIDGETSIZE_MAX);
    layout->addWidget(panel);
}

QWidget *MainWindow::createQsoFormPanel(QWidget *parent, const QString &modeLabel, QsoFormWidgets **qsoForm)
{
    QGroupBox *box = new QGroupBox(MadModemI18n::text(QStringLiteral("QSO Info")), parent);
    // v1.55: do not force a too-small maximum height here.  The v1.53 cap made
    // translated labels and the macro row paint over the QSO fields on Linux
    // and Windows.  Let the grid compute its own natural height and keep the
    // density with smaller margins/fields instead of clipping the contents.
    box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    QGridLayout *grid = new QGridLayout(box);
    grid->setContentsMargins(8, 6, 8, 8);
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(3);

    QsoFormWidgets *form = new QsoFormWidgets();
    form->container = box;
    form->callsign = new QLineEdit(box);
    form->rstSent = new QLineEdit(box);
    form->rstReceived = new QLineEdit(box);
    form->band = new QLineEdit(box);
    form->mode = new QLineEdit(box);
    form->grid = new QLineEdit(box);
    form->utc = new QLineEdit(box);
    form->addButton = new QPushButton("+ Add to log", box);
    form->addButton->setIcon(QIcon());

    const QList<QLineEdit *> qsoEdits = { form->callsign, form->band, form->rstSent, form->rstReceived, form->mode, form->grid, form->utc };
    for (QLineEdit *edit : qsoEdits) {
        edit->setMinimumHeight(22);
        edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
    form->addButton->setMinimumHeight(26);
    form->addButton->setMaximumWidth(QWIDGETSIZE_MAX);
    form->addButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    form->callsign->setPlaceholderText("Call");
    form->rstSent->setPlaceholderText("599");
    form->rstReceived->setPlaceholderText("599");
    form->band->setPlaceholderText("20m");
    form->mode->setPlaceholderText(modeLabel);
    form->grid->setPlaceholderText("JN61");
    form->utc->setReadOnly(true);
    form->utc->setToolTip("UTC is written automatically when the QSO is added to the ADIF logbook.");
    form->addButton->setToolTip("Add this QSO to logbook. The UTC time is taken at save time.");

    form->rstSent->setMaxLength(8);
    form->rstReceived->setMaxLength(8);
    form->band->setMaxLength(12);
    form->mode->setMaxLength(16);
    form->grid->setMaxLength(8);

    auto *lblCall = new QLabel("Callsign", box);
    auto *lblBand = new QLabel("Band", box);
    auto *lblSent = new QLabel("RST sent", box);
    auto *lblRecv = new QLabel("RST received", box);
    auto *lblMode = new QLabel("Mode", box);
    auto *lblGrid = new QLabel("Grid", box);
    auto *lblUtc = new QLabel("UTC save time", box);
    const QList<QLabel *> labels = { lblCall, lblBand, lblSent, lblRecv, lblMode, lblGrid, lblUtc };
    for (QLabel *label : labels) {
        label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        label->setWordWrap(false);
    }

    // Compact side-panel layout: these fields now live in the Mode tab, so avoid
    // the old wide horizontal strip that consumed central RX/TX space.
    grid->addWidget(lblCall, 0, 0);
    grid->addWidget(form->callsign, 0, 1, 1, 3);
    grid->addWidget(lblBand, 1, 0);
    grid->addWidget(form->band, 1, 1);
    grid->addWidget(lblMode, 1, 2);
    grid->addWidget(form->mode, 1, 3);
    grid->addWidget(lblSent, 2, 0);
    grid->addWidget(form->rstSent, 2, 1);
    grid->addWidget(lblRecv, 2, 2);
    grid->addWidget(form->rstReceived, 2, 3);
    grid->addWidget(lblGrid, 3, 0);
    grid->addWidget(form->grid, 3, 1, 1, 3);
    grid->addWidget(lblUtc, 4, 0);
    grid->addWidget(form->utc, 4, 1, 1, 3);
    grid->addWidget(form->addButton, 5, 0, 1, 4);

    grid->setColumnMinimumWidth(0, 48);
    grid->setColumnMinimumWidth(1, 58);
    grid->setColumnMinimumWidth(2, 54);
    grid->setColumnMinimumWidth(3, 58);
    grid->setColumnStretch(0, 0);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(2, 0);
    grid->setColumnStretch(3, 1);

    populateQsoFormDefaults(form, ui != nullptr && ui->cmbMode != nullptr ? ui->cmbMode->currentText() : QString());

    connect(form->addButton, &QPushButton::clicked,
            this, [this, form]() {
                addQsoToLogFromForm(form);
            });

    if (qsoForm != nullptr) {
        *qsoForm = form;
    }

    return box;
}


MainWindow::QsoFormWidgets *MainWindow::activeQsoForm() const
{
    if (ui == nullptr || ui->cmbMode == nullptr) {
        return nullptr;
    }

    const QString modeName = ui->cmbMode->currentText();
    if (modeName == RttyDecoder::modeName()) {
        return m_rttyQsoForm;
    }
    if (modeName == Bpsk31Decoder::modeName()) {
        return m_bpsk31QsoForm;
    }
    if (modeName == MfskDecoder::modeName()) {
        return m_mfskQsoForm;
    }
    if (modeName == CwDecoder::modeName()) {
        return m_cwQsoForm;
    }
    if (modeName == HellschreiberDecoder::modeName()) {
        return m_hellQsoForm;
    }
    if (Msk144Mode::isMode(modeName)) {
        return m_msk144QsoForm;
    }
    if (Q65Mode::isFamilyMode(modeName)) {
        return m_q65QsoForm;
    }
    return nullptr;
}

MainWindow::QsoFormWidgets *MainWindow::qsoFormForTerminal(QPlainTextEdit *terminal) const
{
    if (terminal == m_txtRttyRx) {
        return m_rttyQsoForm;
    }
    if (terminal == m_txtBpsk31Rx) {
        return m_bpsk31QsoForm;
    }
    if (terminal == m_txtMfskRx) {
        return m_mfskQsoForm;
    }
    if (terminal == m_txtCwRx) {
        return m_cwQsoForm;
    }
    return activeQsoForm();
}

QString MainWindow::currentAdifMode() const
{
    if (ui == nullptr || ui->cmbMode == nullptr) {
        return QString();
    }

    const QString modeName = ui->cmbMode->currentText();
    if (modeName == RttyDecoder::modeName()) {
        return "RTTY";
    }
    if (modeName == Bpsk31Decoder::modeName()) {
        if (m_cmbBpsk31Variant != nullptr) {
            return m_cmbBpsk31Variant->currentData().toString().toUpper();
        }
        return "BPSK31";
    }
    if (modeName == MfskDecoder::modeName()) {
        if (m_cmbMfskVariant != nullptr) {
            return m_cmbMfskVariant->currentData().toString().toUpper();
        }
        return "MFSK16";
    }
    if (modeName == CwDecoder::modeName()) {
        return "CW";
    }
    if (modeName == HellschreiberDecoder::modeName()) {
        if (m_cmbHellVariant != nullptr && m_cmbHellVariant->currentData().toString() == "FSK105") {
            return "HELL-FSK105";
        }
        return "HELL";
    }
    if (Ft8Mode::isFamilyMode(modeName)) {
        return Ft8Mode::profileForMode(modeName).adifMode;
    }
    if (Msk144Mode::isMode(modeName)) {
        return QStringLiteral("MSK144");
    }

    if (Q65Mode::isFamilyMode(modeName)) {
        return QStringLiteral("Q65");
    }
    return shortModeLabel(modeName).toUpper();
}

void MainWindow::populateQsoFormDefaults(QsoFormWidgets *form, const QString &modeName)
{
    if (form == nullptr) {
        return;
    }

    QString adifMode;
    if (modeName == RttyDecoder::modeName() || modeName.compare("RTTY", Qt::CaseInsensitive) == 0) {
        adifMode = "RTTY";
    } else if (modeName == Bpsk31Decoder::modeName() || modeName.compare("BPSK", Qt::CaseInsensitive) == 0 || modeName.compare("PSK", Qt::CaseInsensitive) == 0) {
        adifMode = (m_cmbBpsk31Variant != nullptr) ? m_cmbBpsk31Variant->currentData().toString().toUpper() : QString("BPSK31");
    } else if (modeName == MfskDecoder::modeName() || modeName.compare("MFSK", Qt::CaseInsensitive) == 0) {
        adifMode = (m_cmbMfskVariant != nullptr) ? m_cmbMfskVariant->currentData().toString().toUpper() : QString("MFSK16");
    } else if (modeName == CwDecoder::modeName() || modeName.compare("CW", Qt::CaseInsensitive) == 0) {
        adifMode = "CW";
    } else if (modeName == HellschreiberDecoder::modeName() || modeName.compare("Hell", Qt::CaseInsensitive) == 0) {
        adifMode = (m_cmbHellVariant != nullptr && m_cmbHellVariant->currentData().toString() == "FSK105") ? QString("HELL-FSK105") : QString("HELL");
    } else if (Msk144Mode::isMode(modeName)) {
        adifMode = QStringLiteral("MSK144");
    } else if (Q65Mode::isFamilyMode(modeName)) {
        adifMode = QStringLiteral("Q65");
    } else {
        adifMode = currentAdifMode().isEmpty() ? shortModeLabel(modeName).toUpper() : currentAdifMode();
    }

    if (form->rstSent != nullptr && form->rstSent->text().trimmed().isEmpty()) {
        form->rstSent->setText("599");
    }
    if (form->rstReceived != nullptr && form->rstReceived->text().trimmed().isEmpty()) {
        form->rstReceived->setText("599");
    }
    if (form->mode != nullptr) {
        form->mode->setText(adifMode);
    }
    // Callsign is intentionally left empty: it is a live per-QSO value, not a saved default.
    if (form->utc != nullptr) {
        form->utc->setText(QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd HH:mm:ss 'UTC'"));
    }
}

void MainWindow::updateQsoUtcFields()
{
    const QString utc = QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd HH:mm:ss 'UTC'");
    const QList<QsoFormWidgets *> forms = {m_rttyQsoForm, m_bpsk31QsoForm, m_mfskQsoForm, m_cwQsoForm, m_hellQsoForm, m_msk144QsoForm, m_q65QsoForm};
    for (QsoFormWidgets *form : forms) {
        if (form != nullptr && form->utc != nullptr) {
            form->utc->setText(utc);
        }
    }
}


bool MainWindow::addQsoToLogFromForm(QsoFormWidgets *form)
{
    if (form == nullptr || form->callsign == nullptr) {
        return false;
    }

    LogbookEntry entry;
    entry.callsign = AdifLogbook::normalizeCallsign(form->callsign->text());
    entry.rstSent = form->rstSent != nullptr ? form->rstSent->text().trimmed().toUpper() : QStringLiteral("599");
    entry.rstReceived = form->rstReceived != nullptr ? form->rstReceived->text().trimmed().toUpper() : QStringLiteral("599");
    entry.band = form->band != nullptr ? form->band->text().trimmed().toLower() : QString();
    entry.mode = form->mode != nullptr ? form->mode->text().trimmed().toUpper() : currentAdifMode();
    entry.grid = form->grid != nullptr ? form->grid->text().trimmed().toUpper() : QString();
    entry.utc = QDateTime::currentDateTimeUtc();

    if (entry.callsign.isEmpty()) {
        QMessageBox::information(this,
                                 uiText("add_qso_to_log", "Add QSO to log"),
                                 uiText("insert_correspondent_callsign", "Insert the correspondent callsign first."));
        return false;
    }

    if (entry.rstSent.isEmpty()) {
        entry.rstSent = QStringLiteral("599");
    }
    if (entry.rstReceived.isEmpty()) {
        entry.rstReceived = QStringLiteral("599");
    }
    if (entry.mode.isEmpty()) {
        entry.mode = currentAdifMode();
    }

    const bool wasKnown = m_logbook.containsCallsign(entry.callsign);
    QString error;
    if (!m_logbook.append(entry, &error)) {
        QMessageBox::warning(this,
                             uiText("add_qso_to_log", "Add QSO to log"),
                             uiText("cannot_save_logbook", "Cannot save logbook: %1").arg(error));
        return false;
    }

    appendLog(QStringLiteral("Logged QSO: %1 %2 %3 %4%5")
                  .arg(entry.callsign,
                       entry.rstSent,
                       entry.rstReceived,
                       entry.mode,
                       wasKnown ? QStringLiteral(" (worked before)") : QString()));
    refreshLogbookHighlights();
    refreshQsoMaps();

    QMessageBox::information(this,
                             uiText("add_qso_to_log", "Add QSO to log"),
                             uiText("qso_saved_to_adif", "QSO with %1 saved to ADIF logbook.%2")
                                 .arg(entry.callsign,
                                      wasKnown ? uiText("qso_already_present_suffix", "\nThis callsign was already present in the log.") : QString()));
    return true;
}

QString MainWindow::extractCallsignFromText(const QString &text) const
{
    const QRegularExpression re(QStringLiteral("\\b[A-Z0-9]{1,3}[0-9][A-Z]{1,4}(?:/[A-Z0-9]{1,4})?\\b"),
                                QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = re.match(text.toUpper());
    if (match.hasMatch()) {
        return AdifLogbook::normalizeCallsign(match.captured(0));
    }

    const QStringList parts = text.simplified().split(QLatin1Char(' '));
    for (const QString &part : parts) {
        const QString normalized = AdifLogbook::normalizeCallsign(part);
        if (!normalized.isEmpty()) {
            return normalized;
        }
    }
    return QString();
}


bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (event != nullptr && event->type() == QEvent::MouseButtonRelease) {
        QWidget *viewport = qobject_cast<QWidget *>(watched);
        QPlainTextEdit *terminal = (viewport != nullptr) ? qobject_cast<QPlainTextEdit *>(viewport->parentWidget()) : nullptr;
        if (terminal == m_txtRttyRx || terminal == m_txtBpsk31Rx ||
            terminal == m_txtMfskRx || terminal == m_txtCwRx) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse != nullptr && mouse->button() == Qt::LeftButton) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                const QPoint pos = mouse->position().toPoint();
#else
                const QPoint pos = mouse->pos();
#endif
                const QTextCursor clickedCursor = terminal->cursorForPosition(pos);
                const int clickPos = clickedCursor.position();
                const QString text = terminal->toPlainText().toUpper();
                const QRegularExpression re(QStringLiteral("\\b[A-Z0-9]{1,3}[0-9][A-Z]{1,4}(?:/[A-Z0-9]{1,4})?\\b"),
                                            QRegularExpression::CaseInsensitiveOption);
                QRegularExpressionMatchIterator it = re.globalMatch(text);
                while (it.hasNext()) {
                    const QRegularExpressionMatch match = it.next();
                    if (clickPos < match.capturedStart() || clickPos >= match.capturedEnd()) {
                        continue;
                    }
                    const QString call = AdifLogbook::normalizeCallsign(match.captured(0));
                    if (call.size() < 3) {
                        break;
                    }
                    QsoFormWidgets *form = qsoFormForTerminal(terminal);
                    if (form != nullptr && form->callsign != nullptr) {
                        form->callsign->setText(call);
                        if (form->mode != nullptr && form->mode->text().trimmed().isEmpty()) {
                            form->mode->setText(currentAdifMode());
                        }
                        appendLog(QStringLiteral("QSO callsign autofill from RX text: %1").arg(call));
                    }
                    event->accept();
                    return true;
                }
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::installRxTextContextMenu(QPlainTextEdit *terminal)
{
    if (terminal == nullptr) {
        return;
    }

    if (terminal->viewport() != nullptr) {
        terminal->viewport()->installEventFilter(this);
        terminal->viewport()->setCursor(Qt::IBeamCursor);
    }

    terminal->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(terminal, &QPlainTextEdit::customContextMenuRequested,
            this, [this, terminal](const QPoint &pos) {
                QMenu *menu = terminal->createStandardContextMenu();
                menu->setStyleSheet(
                    "QMenu { background-color: #ffffff; color: #111111; border: 1px solid #777777; }"
                    "QMenu::item { padding: 6px 26px 6px 26px; }"
                    "QMenu::item:selected { background-color: #2f6fba; color: #ffffff; }"
                    "QMenu::item:disabled { color: #777777; }"
                    "QMenu::separator { height: 1px; background: #c0c0c0; margin: 4px 8px; }"
                    );
                const QString selected = terminal->textCursor().selectedText().trimmed();
                if (!selected.isEmpty()) {
                    menu->addSeparator();
                    QAction *callAction = menu->addAction("Send selection to log: Callsign");
                    QAction *rstSentAction = menu->addAction("Send selection to log: RST sent");
                    QAction *rstReceivedAction = menu->addAction("Send selection to log: RST received");
                    QAction *bandAction = menu->addAction("Send selection to log: Band");
                    QAction *modeAction = menu->addAction("Send selection to log: Mode");
                    QAction *gridAction = menu->addAction("Send selection to log: Grid square");

                    connect(callAction, &QAction::triggered,
                            this, [this, terminal]() { sendSelectedRxTextToQsoField(terminal, "CALL"); });
                    connect(rstSentAction, &QAction::triggered,
                            this, [this, terminal]() { sendSelectedRxTextToQsoField(terminal, "RST_SENT"); });
                    connect(rstReceivedAction, &QAction::triggered,
                            this, [this, terminal]() { sendSelectedRxTextToQsoField(terminal, "RST_RCVD"); });
                    connect(bandAction, &QAction::triggered,
                            this, [this, terminal]() { sendSelectedRxTextToQsoField(terminal, "BAND"); });
                    connect(modeAction, &QAction::triggered,
                            this, [this, terminal]() { sendSelectedRxTextToQsoField(terminal, "MODE"); });
                    connect(gridAction, &QAction::triggered,
                            this, [this, terminal]() { sendSelectedRxTextToQsoField(terminal, "GRID"); });
                }
                menu->exec(terminal->viewport()->mapToGlobal(pos));
                delete menu;
            });
}

void MainWindow::sendSelectedRxTextToQsoField(QPlainTextEdit *terminal, const QString &fieldName)
{
    if (terminal == nullptr) {
        return;
    }

    QsoFormWidgets *form = qsoFormForTerminal(terminal);
    if (form == nullptr) {
        return;
    }

    QString selected = terminal->textCursor().selectedText().trimmed();
    selected.replace(QChar::ParagraphSeparator, ' ');
    selected = selected.simplified();
    if (selected.isEmpty()) {
        return;
    }

    if (fieldName == "CALL" && form->callsign != nullptr) {
        form->callsign->setText(extractCallsignFromText(selected));
    } else if (fieldName == "RST_SENT" && form->rstSent != nullptr) {
        form->rstSent->setText(selected.left(8).toUpper());
    } else if (fieldName == "RST_RCVD" && form->rstReceived != nullptr) {
        form->rstReceived->setText(selected.left(8).toUpper());
    } else if (fieldName == "BAND" && form->band != nullptr) {
        form->band->setText(selected.left(12).toLower());
    } else if (fieldName == "MODE" && form->mode != nullptr) {
        form->mode->setText(selected.left(16).toUpper());
    } else if (fieldName == "GRID" && form->grid != nullptr) {
        form->grid->setText(selected.left(8).toUpper());
    }
}

void MainWindow::highlightCallsignsInTerminal(QPlainTextEdit *terminal)
{
    if (terminal == nullptr || terminal->document() == nullptr) {
        return;
    }

    const QString text = terminal->toPlainText();
    if (text.isEmpty()) {
        return;
    }

    const int cursorPosition = terminal->textCursor().position();
    QSignalBlocker block(terminal);

    QTextCursor cursor(terminal->document());
    QTextCharFormat normal;
    // Cockpit terminals have a black background; resetting the document to
    // nearly-black made CW/BPSK/RTTY receive text unreadable after the callsign
    // highlighter ran.  Keep ordinary RX text amber and only override detected
    // callsigns with green/red formats below.
    normal.setForeground(QColor("#ffb347"));
    normal.setFontUnderline(false);
    normal.setFontStrikeOut(false);
    normal.setUnderlineStyle(QTextCharFormat::NoUnderline);
    cursor.select(QTextCursor::Document);
    cursor.mergeCharFormat(normal);

    const QRegularExpression re(QStringLiteral("\\b[A-Z0-9]{1,3}[0-9][A-Z]{1,4}(?:/[A-Z0-9]{1,4})?\\b"),
                                QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator it = re.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        const QString call = AdifLogbook::normalizeCallsign(match.captured(0));
        if (call.size() < 3) {
            continue;
        }

        QTextCharFormat fmt;
        const bool worked = m_logbook.containsCallsign(call);
        fmt.setForeground(worked ? QColor("#b00020") : QColor("#118a2a"));
        fmt.setFontUnderline(true);
        fmt.setUnderlineColor(worked ? QColor("#b00020") : QColor("#118a2a"));
        fmt.setUnderlineStyle(QTextCharFormat::SingleUnderline);
        if (worked && m_settings.logbookStrikeWorkedCalls) {
            fmt.setFontStrikeOut(true);
        }

        QTextCursor endCursor(terminal->document());
        endCursor.movePosition(QTextCursor::End);
        const int docEnd = qMax(0, endCursor.position());
        const int start = qBound(0, match.capturedStart(), docEnd);
        const int end = qBound(start, match.capturedEnd(), docEnd);
        if (end <= start) {
            continue;
        }

        if (selectDocumentRange(cursor, start, end)) {
            cursor.mergeCharFormat(fmt);
        }
    }

    recolorCwChannelPrefixes(terminal);

    QTextCursor restore(terminal->document());
    const int restoreEnd = safeDocumentEndPosition(terminal->document());
    const int safeCursorPosition = qBound(0, cursorPosition, restoreEnd);
    moveCursorToDocumentPosition(restore, safeCursorPosition);
    terminal->setTextCursor(restore);
    terminal->ensureCursorVisible();
}

void MainWindow::refreshFt8DecodeWorkedHighlights()
{
    if (m_tableFt8Rx == nullptr) {
        return;
    }
    const QString myCall = stationCallsign();
    for (int row = 0; row < m_tableFt8Rx->rowCount(); ++row) {
        QTableWidgetItem *messageItem = m_tableFt8Rx->item(row, 4);
        if (messageItem == nullptr) {
            continue;
        }
        const QString storedMessage = messageItem->data(kFtOriginalMessageRole).toString().trimmed().toUpper();
        const QString visibleMessage = messageItem->text().trimmed().toUpper();
        const ParsedFt8Message parsed = parseFt8MessageText(storedMessage.isEmpty() ? visibleMessage : storedMessage, myCall);
        const QStringList candidateCalls = ft8HighlightCandidateCallsigns(parsed, myCall);
        QString workedCall;
        QString neededCall;
        for (const QString &call : candidateCalls) {
            if (m_logbook.containsCallsign(call)) {
                if (workedCall.isEmpty()) {
                    workedCall = call;
                }
            } else if (neededCall.isEmpty()) {
                neededCall = call;
            }
        }
        const bool worked = !workedCall.isEmpty() && neededCall.isEmpty();
        const bool needed = !neededCall.isEmpty();
        for (int col = 0; col < m_tableFt8Rx->columnCount(); ++col) {
            QTableWidgetItem *it = m_tableFt8Rx->item(row, col);
            if (it == nullptr) {
                continue;
            }
            QFont f = it->font();
            f.setStrikeOut(worked && m_settings.logbookStrikeWorkedCalls);
            it->setFont(f);
            const bool preserveNewCountryOutline = it->data(kFtRowRedOutlineRole).toBool() &&
                                                   it->toolTip().startsWith(QStringLiteral("New DXCC country"));
            // Do not draw the heavy red dashed outline for every merely-not-in-log station.
            // That category is useful for AutoQSO/logbook logic, but red outline is reserved
            // for high-priority new DXCC highlights so the FT table does not become all red.
            it->setData(kFtRowRedOutlineRole, preserveNewCountryOutline);
            if (worked) {
                it->setToolTip(QStringLiteral("Worked before: %1 is already in the ADIF logbook").arg(workedCall));
            } else if (needed) {
                it->setToolTip(QStringLiteral("Needed station: %1 is not in the ADIF logbook yet. You can call it after the current QSO/73, even if this line is not CQ.").arg(neededCall));
            } else if (!preserveNewCountryOutline) {
                it->setToolTip(QString());
            }
        }
    }
}

void MainWindow::refreshLogbookHighlights()
{
    highlightCallsignsInTerminal(m_txtRttyRx);
    highlightCallsignsInTerminal(m_txtBpsk31Rx);
    highlightCallsignsInTerminal(m_txtMfskRx);
    highlightCallsignsInTerminal(m_txtCwRx);
    refreshFt8DecodeWorkedHighlights();
}

void MainWindow::setupTextTerminalPages()
{
    if (m_mainDisplayStack == nullptr) {
        return;
    }

    m_rttyDisplayPage = createTextTerminalPage("RTTY terminal",
                                               &m_txtRttyRx,
                                               &m_txtRttyTx,
                                               &m_btnRttyClearRx,
                                               &m_btnRttyLoadTxText,
                                               &m_btnRttyClearTx,
                                               &m_btnRttySend,
                                               &m_rttyMacroButtons,
                                               &m_rttyQsoForm);
    m_rttyDisplayPage = wrapTextDisplayPageWithMap(m_rttyDisplayPage, uiText("tab_rtty", "RTTY"), QStringLiteral("RTTY"));
    m_mainDisplayStack->addWidget(m_rttyDisplayPage);

    m_bpsk31DisplayPage = createTextTerminalPage("PSK terminal",
                                                 &m_txtBpsk31Rx,
                                                 &m_txtBpsk31Tx,
                                                 &m_btnBpsk31ClearRx,
                                                 &m_btnBpsk31LoadTxText,
                                                 &m_btnBpsk31ClearTx,
                                                 &m_btnBpsk31Send,
                                                 &m_bpsk31MacroButtons,
                                                 &m_bpsk31QsoForm);
    m_bpsk31DisplayPage = wrapTextDisplayPageWithMap(m_bpsk31DisplayPage, uiText("tab_psk", "PSK"), QStringLiteral("PSK"));
    m_mainDisplayStack->addWidget(m_bpsk31DisplayPage);

    m_mfskDisplayPage = createTextTerminalPage("MFSK terminal",
                                               &m_txtMfskRx,
                                               &m_txtMfskTx,
                                               &m_btnMfskClearRx,
                                               &m_btnMfskLoadTxText,
                                               &m_btnMfskClearTx,
                                               &m_btnMfskSend,
                                               &m_mfskMacroButtons,
                                               &m_mfskQsoForm);
    m_mfskDisplayPage = wrapTextDisplayPageWithMap(m_mfskDisplayPage, uiText("tab_mfsk", "MFSK"), QStringLiteral("MFSK"));
    m_mainDisplayStack->addWidget(m_mfskDisplayPage);

    m_cwDisplayPage = createTextTerminalPage("CW terminal",
                                             &m_txtCwRx,
                                             &m_txtCwTx,
                                             &m_btnCwClearRx,
                                             &m_btnCwLoadTxText,
                                             &m_btnCwClearTx,
                                             &m_btnCwSend,
                                             nullptr,
                                             &m_cwQsoForm);

    /*
     * CW uses one skimmer engine under the hood, but operator control stays
     * simple: RX A on the green marker and optional RX B on the blue marker.
     * Left-click sets RX A; right-click sets/enables RX B. Internal skimmer
     * channels are never drawn as waterfall markers. Do not show the generic
     * text-mode macro row or the three utility
     * buttons used by RTTY/PSK/MFSK.
     */
    m_cwMacroButtons.clear();
    if (m_btnCwClearRx != nullptr) {
        m_btnCwClearRx->setVisible(false);
        m_btnCwClearRx->setEnabled(false);
    }
    if (m_btnCwLoadTxText != nullptr) {
        m_btnCwLoadTxText->setVisible(false);
        m_btnCwLoadTxText->setEnabled(false);
    }
    if (m_btnCwClearTx != nullptr) {
        m_btnCwClearTx->setVisible(false);
        m_btnCwClearTx->setEnabled(false);
    }
    if (m_txtCwRx != nullptr) {
        m_txtCwRx->setPlaceholderText("Decoded CW text appears here. RX A is green; RX B is blue and appears as B> lines. TX text is echoed as TX> lines.");
    }
    if (m_txtCwTx != nullptr) {
        m_txtCwTx->setPlaceholderText("Type CW text to transmit, then press ➤.");
    }
    m_cwDisplayPage = wrapTextDisplayPageWithMap(m_cwDisplayPage, uiText("tab_cw", "CW"), QStringLiteral("CW"));
    m_mainDisplayStack->addWidget(m_cwDisplayPage);

    // MSK144 weak-signal meteor-scatter activity page: decoded messages table
    // plus the standard TX message list.  It intentionally follows the compact
    // WSJT/MSHV layout rather than a generic text terminal.
    m_msk144DisplayPage = new QWidget(m_mainDisplayStack);
    QVBoxLayout *mskLayout = new QVBoxLayout(m_msk144DisplayPage);
    mskLayout->setContentsMargins(6, 6, 6, 6);
    mskLayout->setSpacing(5);

    QLabel *mskCaption = new QLabel(uiText("msk144_activity", "MSK144 activity"), m_msk144DisplayPage);
    mskCaption->setStyleSheet(QStringLiteral("font-weight: 600;"));
    m_tableMsk144Rx = new QTableWidget(0, 5, m_msk144DisplayPage);
    m_tableMsk144Rx->setHorizontalHeaderLabels(QStringList() << "UTC" << "dB" << "T" << "DF" << "Message");
    m_tableMsk144Rx->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableMsk144Rx->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableMsk144Rx->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableMsk144Rx->verticalHeader()->setVisible(false);
    m_tableMsk144Rx->horizontalHeader()->setStretchLastSection(true);
    m_tableMsk144Rx->setColumnWidth(0, 58);
    m_tableMsk144Rx->setColumnWidth(1, 42);
    m_tableMsk144Rx->setColumnWidth(2, 42);
    m_tableMsk144Rx->setColumnWidth(3, 52);
    applyDecodeTableVisualSettings(m_tableMsk144Rx);
    connect(m_tableMsk144Rx, &QTableWidget::cellClicked, this, [this](int row, int) {
        if (m_tableMsk144Rx == nullptr || row < 0 || row >= m_tableMsk144Rx->rowCount()) return;
        QTableWidgetItem *msgItem = m_tableMsk144Rx->item(row, 4);
        const QString call = (msgItem != nullptr) ? extractCallsignFromText(msgItem->text()) : QString();
        if (call.isEmpty() || call == stationCallsign()) return;
        if (m_msk144QsoForm != nullptr && m_msk144QsoForm->callsign != nullptr) {
            m_msk144QsoForm->callsign->setText(call);
        }
        if (m_editMsk144DxCall != nullptr) {
            m_editMsk144DxCall->setText(call);
        }
        appendLog(QStringLiteral("MSK144 QSO callsign autofill: %1").arg(call));
    });

    m_tableMsk144TxMessages = new QTableWidget(7, 2, m_msk144DisplayPage);
    m_tableMsk144TxMessages->setHorizontalHeaderLabels(QStringList() << "Tx" << "Message");
    m_tableMsk144TxMessages->verticalHeader()->setVisible(false);
    m_tableMsk144TxMessages->horizontalHeader()->setStretchLastSection(true);
    m_tableMsk144TxMessages->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableMsk144TxMessages->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableMsk144TxMessages->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    m_tableMsk144TxMessages->setMaximumHeight(190);
    m_tableMsk144TxMessages->setColumnWidth(0, 40);
    applyDecodeTableVisualSettings(m_tableMsk144TxMessages);

    m_btnMsk144ClearRx = new QPushButton(uiText("clear_messages", "Clear messages"), m_msk144DisplayPage);
    connect(m_btnMsk144ClearRx, &QPushButton::clicked, this, &MainWindow::clearMsk144RxTable);

    mskLayout->addWidget(mskCaption);
    mskLayout->addWidget(m_tableMsk144Rx, 1);
    mskLayout->addWidget(m_btnMsk144ClearRx, 0, Qt::AlignLeft);
    m_msk144DisplayPage = wrapTextDisplayPageWithMap(m_msk144DisplayPage, uiText("tab_msk144", "MSK144"), QStringLiteral("MSK144"));
    m_mainDisplayStack->addWidget(m_msk144DisplayPage);
    refreshMsk144StandardMessages();

    // Q65 weak-signal activity page.  Q65A/B/C/D share one clean table/QSO
    // layout; the selected mode chooses the MSHV submode multiplier.
    m_q65DisplayPage = new QWidget(m_mainDisplayStack);
    QVBoxLayout *q65Layout = new QVBoxLayout(m_q65DisplayPage);
    q65Layout->setContentsMargins(6, 6, 6, 6);
    q65Layout->setSpacing(5);

    QLabel *q65Caption = new QLabel(uiText("q65_activity", "Q65 activity"), m_q65DisplayPage);
    q65Caption->setStyleSheet(QStringLiteral("font-weight: 600;"));
    m_tableQ65Rx = new QTableWidget(0, 5, m_q65DisplayPage);
    m_tableQ65Rx->setHorizontalHeaderLabels(QStringList() << "UTC" << "dB" << "DT" << "DF" << "Message");
    m_tableQ65Rx->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableQ65Rx->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableQ65Rx->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableQ65Rx->verticalHeader()->setVisible(false);
    m_tableQ65Rx->horizontalHeader()->setStretchLastSection(true);
    m_tableQ65Rx->setColumnWidth(0, 58);
    m_tableQ65Rx->setColumnWidth(1, 42);
    m_tableQ65Rx->setColumnWidth(2, 42);
    m_tableQ65Rx->setColumnWidth(3, 52);
    applyDecodeTableVisualSettings(m_tableQ65Rx);
    connect(m_tableQ65Rx, &QTableWidget::cellClicked, this, [this](int row, int) {
        if (m_tableQ65Rx == nullptr || row < 0 || row >= m_tableQ65Rx->rowCount()) return;
        QTableWidgetItem *msgItem = m_tableQ65Rx->item(row, 4);
        const QString call = (msgItem != nullptr) ? extractCallsignFromText(msgItem->text()) : QString();
        if (call.isEmpty() || call == stationCallsign()) return;
        if (m_q65QsoForm != nullptr && m_q65QsoForm->callsign != nullptr) {
            m_q65QsoForm->callsign->setText(call);
        }
        if (m_editQ65DxCall != nullptr) {
            m_editQ65DxCall->setText(call);
        }
        appendLog(QStringLiteral("Q65 QSO callsign autofill: %1").arg(call));
    });

    m_tableQ65TxMessages = new QTableWidget(7, 2, m_q65DisplayPage);
    m_tableQ65TxMessages->setHorizontalHeaderLabels(QStringList() << "Tx" << "Message");
    m_tableQ65TxMessages->verticalHeader()->setVisible(false);
    m_tableQ65TxMessages->horizontalHeader()->setStretchLastSection(true);
    m_tableQ65TxMessages->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableQ65TxMessages->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableQ65TxMessages->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    m_tableQ65TxMessages->setMaximumHeight(190);
    m_tableQ65TxMessages->setColumnWidth(0, 40);
    applyDecodeTableVisualSettings(m_tableQ65TxMessages);

    m_btnQ65ClearRx = new QPushButton(uiText("clear_messages", "Clear messages"), m_q65DisplayPage);
    connect(m_btnQ65ClearRx, &QPushButton::clicked, this, &MainWindow::clearQ65RxTable);

    q65Layout->addWidget(q65Caption);
    q65Layout->addWidget(m_tableQ65Rx, 1);
    q65Layout->addWidget(m_btnQ65ClearRx, 0, Qt::AlignLeft);
    m_q65DisplayPage = wrapTextDisplayPageWithMap(m_q65DisplayPage, uiText("tab_q65", "Q65"), QStringLiteral("Q65"));
    m_mainDisplayStack->addWidget(m_q65DisplayPage);
    refreshQ65StandardMessages();


    /*
     * Hellschreiber is a text mode, but the received information is read visually
     * from a Hellschreiber paper tape rather than decoded into ASCII.  Keep the
     * same macro/input workflow as RTTY and BPSK, and place the virtual paper
     * where the text terminal normally lives.
     */
    m_hellDisplayPage = new QWidget(m_mainDisplayStack);
    m_hellDisplayPage->setAutoFillBackground(false);
    m_hellDisplayPage->setStyleSheet(
        "QLabel#hellPaperLabel { background: palette(base); color: palette(text); }"
        "QPlainTextEdit { padding: 5px;"
        " font-family: DejaVu Sans Mono, Consolas, monospace; font-size: 9pt; }"
        "QPushButton { padding: 3px 7px; min-height: 24px; font-size: 9pt; }"
        "QLabel { font-size: 9pt; }"
        );

    QVBoxLayout *hellLayout = new QVBoxLayout(m_hellDisplayPage);
    hellLayout->setContentsMargins(6, 6, 6, 6);
    hellLayout->setSpacing(5);

    QLabel *hellCaption = new QLabel(uiText("hellschreiber_paper", "Hellschreiber paper"), m_hellDisplayPage);
    hellCaption->setStyleSheet("font-weight: 500; font-size: 9pt;");

    m_lblHellRaster = new QLabel(m_hellDisplayPage);
    m_lblHellRaster->setObjectName("hellPaperLabel");
    m_lblHellRaster->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_lblHellRaster->setMinimumSize(760, 104);
    m_lblHellRaster->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_lblHellRaster->setScaledContents(false);

    m_scrollHellRaster = new QScrollArea(m_hellDisplayPage);
    m_scrollHellRaster->setWidget(m_lblHellRaster);
    m_scrollHellRaster->setWidgetResizable(false);
    m_scrollHellRaster->setMinimumHeight(88);
    m_scrollHellRaster->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_scrollHellRaster->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollHellRaster->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    QWidget *hellQsoPanel = createQsoFormPanel(m_hellDisplayPage, "Hell", &m_hellQsoForm);

    QGridLayout *hellMacroLayout = new QGridLayout();
    hellMacroLayout->setContentsMargins(0, 0, 0, 0);
    hellMacroLayout->setHorizontalSpacing(4);
    hellMacroLayout->setVerticalSpacing(4);

    m_hellMacroButtons.clear();
    for (int i = 0; i < 6; ++i) {
        QPushButton *button = new QPushButton(QString("Macro %1").arg(i + 1), m_hellDisplayPage);
        button->setMinimumHeight(26);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_hellMacroButtons.append(button);
        hellMacroLayout->addWidget(button, i / 6, i % 6);
    }

    QHBoxLayout *hellInputLayout = new QHBoxLayout();
    hellInputLayout->setContentsMargins(0, 0, 0, 0);
    hellInputLayout->setSpacing(6);

    m_txtHellTx = new QPlainTextEdit(m_hellDisplayPage);
    m_txtHellTx->setMinimumHeight(50);
    m_txtHellTx->setMaximumHeight(68);
    m_txtHellTx->setPlaceholderText(uiText("placeholder.hell_tx", "Type Hellschreiber text to transmit, then press ➤."));
    m_txtHellTx->setLineWrapMode(QPlainTextEdit::WidgetWidth);

    m_btnHellSend = new QPushButton(QString::fromUtf8("➤"), m_hellDisplayPage);
    m_btnHellSend->setMinimumWidth(52);
    m_btnHellSend->setMinimumHeight(50);
    m_btnHellSend->setToolTip(uiText("tooltip.hell_send", "Transmit the Hellschreiber text typed in the input box."));

    hellInputLayout->addWidget(m_txtHellTx, 1);
    hellInputLayout->addWidget(m_btnHellSend);

    QHBoxLayout *hellToolLayout = new QHBoxLayout();
    hellToolLayout->setContentsMargins(0, 0, 0, 0);
    hellToolLayout->setSpacing(6);

    m_btnHellLoadTxText = new QPushButton("Load text...", m_hellDisplayPage);
    m_btnHellClearTx = new QPushButton("Clear input", m_hellDisplayPage);
    m_btnHellResetImage = new QPushButton("Reset RX paper", m_hellDisplayPage);

    hellToolLayout->addWidget(m_btnHellResetImage);
    hellToolLayout->addWidget(m_btnHellLoadTxText);
    hellToolLayout->addWidget(m_btnHellClearTx);
    hellToolLayout->addStretch(1);

    hellLayout->addWidget(hellCaption);
    hellLayout->addWidget(m_scrollHellRaster, 1);
    hellQsoPanel->setVisible(false);
    hellLayout->addLayout(hellMacroLayout);
    hellLayout->addLayout(hellInputLayout);
    hellLayout->addLayout(hellToolLayout);

    m_hellDisplayPage = wrapTextDisplayPageWithMap(m_hellDisplayPage, uiText("tab_hell", "Hell"), QStringLiteral("HELL"));
    m_mainDisplayStack->addWidget(m_hellDisplayPage);

    populateQsoFormDefaults(m_rttyQsoForm, RttyDecoder::modeName());
    populateQsoFormDefaults(m_bpsk31QsoForm, Bpsk31Decoder::modeName());
    populateQsoFormDefaults(m_mfskQsoForm, MfskDecoder::modeName());
    populateQsoFormDefaults(m_cwQsoForm, CwDecoder::modeName());
    populateQsoFormDefaults(m_hellQsoForm, HellschreiberDecoder::modeName());
}

void MainWindow::updateCentralDisplayForMode(const QString &modeName)
{
    if (m_mainDisplayStack == nullptr) {
        return;
    }

    const bool sstvMode = (modeName == SstvModeDefinition::modeName());
    const bool wefaxMode = (modeName == WeatherFaxDecoder::modeName());
    const bool cwMode = (modeName == CwDecoder::modeName());
    const bool textMode = (modeName == RttyDecoder::modeName() ||
                           modeName == Bpsk31Decoder::modeName() ||
                           modeName == MfskDecoder::modeName() ||
                           cwMode ||
                           modeName == HellschreiberDecoder::modeName() ||
                           Ft8Mode::isFamilyMode(modeName) ||
                           Msk144Mode::isMode(modeName) ||
                           Q65Mode::isFamilyMode(modeName));

    if (ui->frameSstvImage != nullptr) {
        // Keep the surrounding SSTV/WEFAX page on the native Qt palette.
        // The image widget itself paints only the actual picture/canvas area dark,
        // so the whole Mode page no longer becomes a black rectangle.
        ui->frameSstvImage->setStyleSheet(QString());
    }

    // Let the waterfall scale proportionally with the main window instead of
    // being trapped by fixed max heights. CW skimmer decoding is active under
    // the hood, but the waterfall remains compact and only shows user A/B
    // markers plus transient decoded-text overlays.
    if (ui->grpWaterfall != nullptr) {
        ui->grpWaterfall->setMinimumHeight(modeName == HellschreiberDecoder::modeName() ? 125 : (cwMode ? 190 : (textMode ? 195 : 155)));
        ui->grpWaterfall->setMaximumHeight(QWIDGETSIZE_MAX);
        ui->grpWaterfall->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        if (ui->waterfallVerticalLayout != nullptr) {
            ui->waterfallVerticalLayout->setContentsMargins(0, 0, 0, 0);
            ui->waterfallVerticalLayout->setSpacing(0);
        }
        ui->grpWaterfall->setContentsMargins(0, 0, 0, 0);
    }
    if (ui->frameWaterfall != nullptr) {
        ui->frameWaterfall->setMinimumHeight(modeName == HellschreiberDecoder::modeName() ? 110 : (cwMode ? 175 : (textMode ? 180 : 145)));
        ui->frameWaterfall->setMaximumHeight(QWIDGETSIZE_MAX);
        ui->frameWaterfall->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        ui->frameWaterfall->setContentsMargins(0, 0, 0, 0);
        ui->frameWaterfall->setFrameShape(QFrame::NoFrame);
    }
    if (ui->mainLeftVerticalLayout != nullptr) {
        ui->mainLeftVerticalLayout->setStretch(0, cwMode ? 5 : 4);
        ui->mainLeftVerticalLayout->setStretch(1, 1);
        ui->mainLeftVerticalLayout->setSpacing(3);
    }

    if (modeName == RttyDecoder::modeName() && m_rttyDisplayPage != nullptr) {
        m_mainDisplayStack->setCurrentWidget(m_rttyDisplayPage);
        return;
    }

    if (modeName == Bpsk31Decoder::modeName() && m_bpsk31DisplayPage != nullptr) {
        m_mainDisplayStack->setCurrentWidget(m_bpsk31DisplayPage);
        return;
    }

    if (modeName == MfskDecoder::modeName() && m_mfskDisplayPage != nullptr) {
        m_mainDisplayStack->setCurrentWidget(m_mfskDisplayPage);
        return;
    }

    if (modeName == CwDecoder::modeName() && m_cwDisplayPage != nullptr) {
        m_mainDisplayStack->setCurrentWidget(m_cwDisplayPage);
        return;
    }

    if (modeName == HellschreiberDecoder::modeName() && m_hellDisplayPage != nullptr) {
        m_mainDisplayStack->setCurrentWidget(m_hellDisplayPage);
        return;
    }

    if (Ft8Mode::isFamilyMode(modeName) && m_ft8DisplayPage != nullptr) {
        updateFtQsoMapModeFilter();
        m_mainDisplayStack->setCurrentWidget(m_ft8DisplayPage);
        return;
    }

    if (Msk144Mode::isMode(modeName) && m_msk144DisplayPage != nullptr) {
        m_mainDisplayStack->setCurrentWidget(m_msk144DisplayPage);
        return;
    }

    if (Q65Mode::isFamilyMode(modeName) && m_q65DisplayPage != nullptr) {
        m_mainDisplayStack->setCurrentWidget(m_q65DisplayPage);
        return;
    }

    if (sstvMode && m_imageDisplayPage != nullptr) {
        setSstvQsoMapVisible(true);
        m_mainDisplayStack->setCurrentWidget(m_imageDisplayPage);
        return;
    }

    if (wefaxMode && m_imageDisplayPage != nullptr) {
        setSstvQsoMapVisible(false);
        m_mainDisplayStack->setCurrentWidget(m_imageDisplayPage);
        return;
    }

    if (m_imageDisplayPage != nullptr) {
        setSstvQsoMapVisible(false);
        m_mainDisplayStack->setCurrentWidget(m_imageDisplayPage);
    }
}

QString MainWindow::shortModeLabel(const QString &modeName) const
{
    if (modeName == WeatherFaxDecoder::modeName()) {
        return "MeteoFax / HF WEFAX";
    }

    if (modeName == SstvModeDefinition::modeName()) {
        return "SSTV";
    }

    if (modeName == RttyDecoder::modeName()) {
        return "RTTY";
    }

    if (modeName == Bpsk31Decoder::modeName()) {
        return "PSK";
    }

    if (modeName == MfskDecoder::modeName()) {
        return "MFSK";
    }

    if (modeName == CwDecoder::modeName()) {
        return "CW Morse";
    }

    if (modeName == HellschreiberDecoder::modeName()) {
        return "Feld Hell";
    }

    if (Msk144Mode::isMode(modeName)) {
        return QStringLiteral("MSK144");
    }
    if (Q65Mode::isFamilyMode(modeName)) {
        if (modeName.trimmed().compare(Q65Mode::familyName(), Qt::CaseInsensitive) == 0) {
            return Q65Mode::familyName();
        }
        return Q65Mode::modeName(Q65Mode::submodeForMode(modeName));
    }

    if (Ft8Mode::isFamilyMode(modeName)) {
        const Ft8Mode::Profile profile = Ft8Mode::profileForMode(modeName);
        return profile.experimental
            ? profile.shortLabel + QStringLiteral(" experimental")
            : profile.shortLabel;
    }

    return modeName;
}

void MainWindow::setupModeMenu()
{
    QMenu *modeMenu = new QMenu("Mode", this);
    hardenPopupMenuForFullscreen(modeMenu);
    QActionGroup *modeGroup = new QActionGroup(modeMenu);
    modeGroup->setExclusive(true);

    auto addModeAction = [this, modeMenu, modeGroup](const QString &modeName) {
        QAction *action = modeMenu->addAction(shortModeLabel(modeName));
        action->setCheckable(true);
        action->setData(modeName);
        modeGroup->addAction(action);

        if (ui->cmbMode != nullptr && ui->cmbMode->currentText() == modeName) {
            action->setChecked(true);
        }

        connect(action, &QAction::triggered, this, [this, modeName]() {
            requestModeChange(modeName);
        });
    };

    addModeAction(WeatherFaxDecoder::modeName());
    addModeAction(SstvModeDefinition::modeName());
    addModeAction(RttyDecoder::modeName());
    addModeAction(Bpsk31Decoder::modeName());
    addModeAction(MfskDecoder::modeName());
    addModeAction(CwDecoder::modeName());
    addModeAction(HellschreiberDecoder::modeName());
    addModeAction(Msk144Mode::modeName());
    addModeAction(Q65Mode::familyName());
    for (const QString &ftModeName : Ft8Mode::allModeNames()) {
        addModeAction(ftModeName);
    }

    connect(ui->cmbMode, &QComboBox::currentTextChanged, this, [modeGroup](const QString &modeName) {
        for (QAction *action : modeGroup->actions()) {
            if (action->data().toString() == modeName) {
                action->setChecked(true);
                break;
            }
        }
    });

    // Settings is now a direct menubar action, not the original QMenu.  Insert
    // Mode before Settings explicitly; otherwise Qt appends Mode at the end
    // after the old menuSettings action has been removed from the menubar.
    QAction *before = m_actionAppSettings != nullptr ? m_actionAppSettings : nullptr;
    if (before == nullptr && ui->menuSettings != nullptr) {
        before = ui->menuSettings->menuAction();
    }
    if (before != nullptr && ui->menubar->actions().contains(before)) {
        ui->menubar->insertMenu(before, modeMenu);
    } else {
        QAction *helpAction = ui->menuHelp != nullptr ? ui->menuHelp->menuAction() : nullptr;
        if (helpAction != nullptr && ui->menubar->actions().contains(helpAction)) {
            ui->menubar->insertMenu(helpAction, modeMenu);
        } else {
            ui->menubar->addMenu(modeMenu);
        }
    }
}

void MainWindow::setupRttyPage()
{
    if (ui->stkModeSettings == nullptr) {
        return;
    }

    m_pageRttySettings = new QWidget(ui->stkModeSettings);
    QVBoxLayout *outerLayout = new QVBoxLayout(m_pageRttySettings);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(8);

    QGroupBox *settingsGroup = new QGroupBox("RTTY settings", m_pageRttySettings);
    settingsGroup->setStyleSheet(QString());
    QGridLayout *grid = new QGridLayout(settingsGroup);
    grid->setContentsMargins(8, 8, 8, 8);
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(6);

    m_cmbRttyPreset = new QComboBox(settingsGroup);
    m_spinRttyBaud = new QDoubleSpinBox(settingsGroup);
    m_spinRttyShiftHz = new QSpinBox(settingsGroup);
    m_spinRttyMarkHz = new QSpinBox(settingsGroup);
    m_chkRttyReverse = new QCheckBox("Reverse polarity", settingsGroup);
    m_chkRttyAutoReverse = new QCheckBox("Auto polarity", settingsGroup);
    m_chkRttyAfc = new QCheckBox("AFC", settingsGroup);
    m_spinRttyAfcRangeHz = new QSpinBox(settingsGroup);

    m_spinRttyBaud->setRange(10.0, 300.0);
    m_spinRttyBaud->setDecimals(2);
    m_spinRttyBaud->setSingleStep(0.01);
    m_spinRttyBaud->setSuffix(" baud");

    m_spinRttyShiftHz->setRange(50, 1200);
    m_spinRttyShiftHz->setSingleStep(5);
    m_spinRttyShiftHz->setSuffix(" Hz");

    m_spinRttyMarkHz->setRange(300, 3500);
    m_spinRttyMarkHz->setSingleStep(5);
    m_spinRttyMarkHz->setSuffix(" Hz");

    m_spinRttyAfcRangeHz->setRange(5, 100);
    m_spinRttyAfcRangeHz->setSingleStep(5);
    m_spinRttyAfcRangeHz->setPrefix(QString::fromUtf8("±"));
    m_spinRttyAfcRangeHz->setSuffix(" Hz");

    grid->addWidget(new QLabel("Preset", settingsGroup), 0, 0);
    grid->addWidget(m_cmbRttyPreset, 0, 1, 1, 2);
    grid->addWidget(new QLabel("Baud", settingsGroup), 1, 0);
    grid->addWidget(m_spinRttyBaud, 1, 1, 1, 2);
    grid->addWidget(new QLabel("Shift", settingsGroup), 2, 0);
    grid->addWidget(m_spinRttyShiftHz, 2, 1, 1, 2);
    grid->addWidget(new QLabel("Mark", settingsGroup), 3, 0);
    grid->addWidget(m_spinRttyMarkHz, 3, 1, 1, 2);
    grid->addWidget(m_chkRttyReverse, 4, 0, 1, 3);
    grid->addWidget(m_chkRttyAutoReverse, 5, 0, 1, 3);
    grid->addWidget(m_chkRttyAfc, 6, 0, 1, 1);
    grid->addWidget(new QLabel("AFC range", settingsGroup), 6, 1);
    grid->addWidget(m_spinRttyAfcRangeHz, 6, 2);
    grid->setColumnStretch(1, 1);

    // RTTY contest/multi-decode DSP controls live in the dedicated DSP tab.

    QGroupBox *scopeGroup = new QGroupBox("RTTY tuning scope", m_pageRttySettings);
    scopeGroup->setStyleSheet(QString());
    QVBoxLayout *scopeLayout = new QVBoxLayout(scopeGroup);
    scopeLayout->setContentsMargins(10, 12, 10, 10);
    scopeLayout->setSpacing(8);

    m_rttyScopeWidget = new RttyScopeWidget(scopeGroup);
    scopeLayout->addWidget(m_rttyScopeWidget, 0, Qt::AlignHCenter);

    outerLayout->addWidget(settingsGroup);
    placeQsoFormInModePanel(outerLayout, m_rttyQsoForm);
    outerLayout->addWidget(scopeGroup);
    outerLayout->addStretch(1);

    ui->stkModeSettings->addWidget(m_pageRttySettings);

    populateRttyPresets();
    loadRttySettingsToUi();
    refreshTextMacroButtons();
}

void MainWindow::populateRttyPresets()
{
    if (m_cmbRttyPreset == nullptr) {
        return;
    }

    const QSignalBlocker block(m_cmbRttyPreset);
    m_cmbRttyPreset->clear();

    for (const RttyPreset &preset : rttyPresets()) {
        m_cmbRttyPreset->addItem(preset.label, preset.key);
    }
}

void MainWindow::loadRttySettingsToUi()
{
    if (m_cmbRttyPreset == nullptr ||
        m_spinRttyBaud == nullptr ||
        m_spinRttyShiftHz == nullptr ||
        m_spinRttyMarkHz == nullptr ||
        m_chkRttyReverse == nullptr ||
        m_chkRttyAutoReverse == nullptr ||
        m_chkRttyAfc == nullptr ||
        m_spinRttyAfcRangeHz == nullptr) {
        return;
    }

    const QSignalBlocker blockPreset(m_cmbRttyPreset);
    const QSignalBlocker blockBaud(m_spinRttyBaud);
    const QSignalBlocker blockShift(m_spinRttyShiftHz);
    const QSignalBlocker blockMark(m_spinRttyMarkHz);
    const QSignalBlocker blockReverse(m_chkRttyReverse);
    const QSignalBlocker blockAutoReverse(m_chkRttyAutoReverse);
    const QSignalBlocker blockAfc(m_chkRttyAfc);
    const QSignalBlocker blockAfcRange(m_spinRttyAfcRangeHz);
    const QSignalBlocker blockMulti(m_chkRttyMultiDecode);
    const QSignalBlocker blockOverlay(m_chkRttyOverlayCallsigns);
    const QSignalBlocker blockEnhanced(m_chkRttyContestEnhanced);
    const QSignalBlocker blockSecond(m_chkRttySecondPass);
    const QSignalBlocker blockMax(m_spinRttyMaxDecoders);

    const int presetIndex = m_cmbRttyPreset->findData(m_settings.rttyPreset);
    m_cmbRttyPreset->setCurrentIndex(qMax(0, presetIndex));

    const RttyPreset preset = rttyPresetByKey(m_cmbRttyPreset->currentData().toString());
    const bool customPreset = (preset.key == "CUSTOM");

    m_spinRttyBaud->setValue(qBound(10.0, customPreset ? m_settings.rttyBaudRate : preset.baud, 300.0));
    m_spinRttyShiftHz->setValue(qBound(50, customPreset ? m_settings.rttyShiftHz : preset.shiftHz, 1200));
    m_spinRttyMarkHz->setValue(qBound(300, preset.markHz, 3500));
    m_chkRttyReverse->setChecked(m_settings.rttyReverse);
    m_chkRttyAutoReverse->setChecked(m_settings.rttyAutoReverseEnabled);
    m_chkRttyAfc->setChecked(m_settings.rttyAfcEnabled);
    m_spinRttyAfcRangeHz->setValue(qBound(5, m_settings.rttyAfcRangeHz, 100));
    if (m_chkRttyMultiDecode != nullptr) m_chkRttyMultiDecode->setChecked(m_settings.rttyMultiDecodeEnabled);
    if (m_chkRttyOverlayCallsigns != nullptr) m_chkRttyOverlayCallsigns->setChecked(m_settings.rttyOverlayCallsignsEnabled);
    if (m_chkRttyContestEnhanced != nullptr) m_chkRttyContestEnhanced->setChecked(m_settings.rttyContestEnhancedEnabled);
    if (m_chkRttySecondPass != nullptr) m_chkRttySecondPass->setChecked(m_settings.rttySecondPassEnabled);
    if (m_spinRttyMaxDecoders != nullptr) m_spinRttyMaxDecoders->setValue(qBound(2, m_settings.rttyMaxParallelDecoders, 32));
    if (m_chkRttyOverlayCallsigns != nullptr) m_chkRttyOverlayCallsigns->setEnabled(m_settings.rttyMultiDecodeEnabled);
    if (m_chkRttyContestEnhanced != nullptr) m_chkRttyContestEnhanced->setEnabled(m_settings.rttyMultiDecodeEnabled);
    if (m_chkRttySecondPass != nullptr) m_chkRttySecondPass->setEnabled(m_settings.rttyMultiDecodeEnabled);
    if (m_spinRttyMaxDecoders != nullptr) m_spinRttyMaxDecoders->setEnabled(m_settings.rttyMultiDecodeEnabled);

    m_cmbRttyPreset->setToolTip(preset.details);
    m_cmbRttyPreset->setStatusTip(preset.details);
}


void MainWindow::setupBpsk31Page()
{
    if (ui->stkModeSettings == nullptr) {
        return;
    }

    m_pageBpsk31Settings = new QWidget(ui->stkModeSettings);
    QVBoxLayout *outerLayout = new QVBoxLayout(m_pageBpsk31Settings);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(8);

    QGroupBox *settingsGroup = new QGroupBox("PSK settings", m_pageBpsk31Settings);
    QGridLayout *grid = new QGridLayout(settingsGroup);
    grid->setContentsMargins(8, 8, 8, 8);
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(6);

    m_cmbBpsk31Variant = new QComboBox(settingsGroup);
    m_cmbBpsk31Variant->addItem("BPSK31 / 31.25 baud", "BPSK31");
    m_cmbBpsk31Variant->addItem("BPSK63 / 62.5 baud", "BPSK63");
    m_cmbBpsk31Variant->addItem("BPSK125 / 125 baud", "BPSK125");
    m_cmbBpsk31Variant->addItem("BPSK250 / 250 baud", "BPSK250");
    m_cmbBpsk31Variant->addItem("BPSK500 / 500 baud", "BPSK500");
    m_cmbBpsk31Variant->addItem("BPSK1000 / 1000 baud", "BPSK1000");
    m_cmbBpsk31Variant->addItem("QPSK31 / 31.25 baud", "QPSK31");
    m_cmbBpsk31Variant->addItem("QPSK63 / 62.5 baud", "QPSK63");
    m_cmbBpsk31Variant->addItem("QPSK125 / 125 baud", "QPSK125");
    m_cmbBpsk31Variant->addItem("QPSK250 / 250 baud", "QPSK250");
    m_cmbBpsk31Variant->addItem("QPSK500 / 500 baud", "QPSK500");

    m_spinBpsk31ToneHz = new QSpinBox(settingsGroup);
    m_spinBpsk31ToneHz->setRange(300, 3500);
    m_spinBpsk31ToneHz->setSingleStep(10);
    m_spinBpsk31ToneHz->setSuffix(" Hz");

    m_chkBpsk31Afc = new QCheckBox("AFC", settingsGroup);
    m_spinBpsk31AfcRangeHz = new QSpinBox(settingsGroup);
    m_chkBpsk31Invert = new QCheckBox("Invert bits", settingsGroup);

    m_spinBpsk31AfcRangeHz->setRange(5, 100);
    m_spinBpsk31AfcRangeHz->setSingleStep(5);
    m_spinBpsk31AfcRangeHz->setPrefix(QString::fromUtf8("±"));
    m_spinBpsk31AfcRangeHz->setSuffix(" Hz");

    grid->addWidget(new QLabel("Variant", settingsGroup), 0, 0);
    grid->addWidget(m_cmbBpsk31Variant, 0, 1, 1, 2);
    grid->addWidget(new QLabel("Tone", settingsGroup), 1, 0);
    grid->addWidget(m_spinBpsk31ToneHz, 1, 1, 1, 2);
    grid->addWidget(m_chkBpsk31Afc, 2, 0, 1, 1);
    grid->addWidget(new QLabel("AFC range", settingsGroup), 2, 1);
    grid->addWidget(m_spinBpsk31AfcRangeHz, 2, 2);
    grid->addWidget(m_chkBpsk31Invert, 3, 0, 1, 3);
    grid->setColumnStretch(1, 1);

    setHelpText(settingsGroup, uiText("psk_settings_tooltip", "Select PSK/QPSK speed, audio tone, AFC and inversion. The RX terminal, TX box and macros are shown in the central text area above the waterfall."));

    outerLayout->addWidget(settingsGroup);
    placeQsoFormInModePanel(outerLayout, m_bpsk31QsoForm);
    outerLayout->addStretch(1);

    ui->stkModeSettings->addWidget(m_pageBpsk31Settings);

    loadBpsk31SettingsToUi();
    refreshTextMacroButtons();
}

void MainWindow::loadBpsk31SettingsToUi()
{
    if (m_cmbBpsk31Variant == nullptr ||
        m_spinBpsk31ToneHz == nullptr ||
        m_chkBpsk31Afc == nullptr ||
        m_spinBpsk31AfcRangeHz == nullptr ||
        m_chkBpsk31Invert == nullptr) {
        return;
    }

    const QSignalBlocker blockVariant(m_cmbBpsk31Variant);
    const QSignalBlocker blockTone(m_spinBpsk31ToneHz);
    const QSignalBlocker blockAfc(m_chkBpsk31Afc);
    const QSignalBlocker blockAfcRange(m_spinBpsk31AfcRangeHz);
    const QSignalBlocker blockInvert(m_chkBpsk31Invert);

    const int variantIndex = m_cmbBpsk31Variant->findData(m_settings.bpsk31Variant);
    m_cmbBpsk31Variant->setCurrentIndex(qMax(0, variantIndex));
    m_spinBpsk31ToneHz->setValue(qBound(300, m_settings.bpsk31ToneHz, 3500));
    m_chkBpsk31Afc->setChecked(m_settings.bpsk31AfcEnabled);
    m_spinBpsk31AfcRangeHz->setValue(qBound(5, m_settings.bpsk31AfcRangeHz, 100));
    m_chkBpsk31Invert->setChecked(m_settings.bpsk31InvertBits);
}


void MainWindow::setupMfskPage()
{
    if (ui->stkModeSettings == nullptr) {
        return;
    }

    m_pageMfskSettings = new QWidget(ui->stkModeSettings);
    QVBoxLayout *outerLayout = new QVBoxLayout(m_pageMfskSettings);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(8);

    QGroupBox *settingsGroup = new QGroupBox("MFSK settings", m_pageMfskSettings);
    QGridLayout *grid = new QGridLayout(settingsGroup);
    grid->setContentsMargins(8, 8, 8, 8);
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(6);

    m_cmbMfskVariant = new QComboBox(settingsGroup);
    m_cmbMfskVariant->addItem("MFSK16 / 15.625 baud", "MFSK16");
    m_cmbMfskVariant->addItem("MFSK32 / 31.25 baud", "MFSK32");

    m_spinMfskCenterHz = new QSpinBox(settingsGroup);
    m_spinMfskCenterHz->setRange(300, 3300);
    m_spinMfskCenterHz->setSingleStep(10);
    m_spinMfskCenterHz->setSuffix(" Hz");

    m_chkMfskAfc = new QCheckBox("AFC", settingsGroup);
    m_chkMfskAfc->setToolTip("Enable slow AFC for the MFSK tone-bank receiver.");
    m_spinMfskAfcRangeHz = new QSpinBox(settingsGroup);
    m_spinMfskAfcRangeHz->setRange(5, 200);
    m_spinMfskAfcRangeHz->setSingleStep(5);
    m_spinMfskAfcRangeHz->setPrefix(QString::fromUtf8("±"));
    m_spinMfskAfcRangeHz->setSuffix(" Hz");

    grid->addWidget(new QLabel("Variant", settingsGroup), 0, 0);
    grid->addWidget(m_cmbMfskVariant, 0, 1, 1, 2);
    grid->addWidget(new QLabel("Center", settingsGroup), 1, 0);
    grid->addWidget(m_spinMfskCenterHz, 1, 1, 1, 2);
    grid->addWidget(m_chkMfskAfc, 2, 0, 1, 1);
    grid->addWidget(new QLabel("AFC range", settingsGroup), 2, 1);
    grid->addWidget(m_spinMfskAfcRangeHz, 2, 2);
    grid->setColumnStretch(1, 1);

    setHelpText(settingsGroup, uiText("mfsk_settings_tooltip", "MFSK16 uses standard Varicode/FEC. MFSK32 is still a legacy experimental mode."));

    outerLayout->addWidget(settingsGroup);
    placeQsoFormInModePanel(outerLayout, m_mfskQsoForm);
    outerLayout->addStretch(1);

    ui->stkModeSettings->addWidget(m_pageMfskSettings);

    loadMfskSettingsToUi();
    refreshTextMacroButtons();
}

void MainWindow::loadMfskSettingsToUi()
{
    if (m_cmbMfskVariant == nullptr ||
        m_spinMfskCenterHz == nullptr ||
        m_chkMfskAfc == nullptr ||
        m_spinMfskAfcRangeHz == nullptr) {
        return;
    }

    const QSignalBlocker blockVariant(m_cmbMfskVariant);
    const QSignalBlocker blockCenter(m_spinMfskCenterHz);
    const QSignalBlocker blockAfc(m_chkMfskAfc);
    const QSignalBlocker blockAfcRange(m_spinMfskAfcRangeHz);

    const int variantIndex = m_cmbMfskVariant->findData(m_settings.mfskVariant);
    m_cmbMfskVariant->setCurrentIndex(qMax(0, variantIndex));
    m_spinMfskCenterHz->setValue(qBound(300, m_settings.mfskCenterHz, 3300));
    m_chkMfskAfc->setChecked(m_settings.mfskAfcEnabled);
    m_spinMfskAfcRangeHz->setValue(qBound(5, m_settings.mfskAfcRangeHz, 200));
}

void MainWindow::setupCwPage()
{
    if (ui->stkModeSettings == nullptr) {
        return;
    }

    m_pageCwSettings = new QWidget(ui->stkModeSettings);
    QVBoxLayout *outerLayout = new QVBoxLayout(m_pageCwSettings);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(8);

    QGroupBox *settingsGroup = new QGroupBox("CW Morse RX/TX settings", m_pageCwSettings);
    QGridLayout *grid = new QGridLayout(settingsGroup);
    grid->setContentsMargins(8, 8, 8, 8);
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(6);

    m_spinCwToneHz = new QSpinBox(settingsGroup);
    m_spinCwToneHz->setRange(250, 3000);
    m_spinCwToneHz->setSingleStep(10);
    m_spinCwToneHz->setSuffix(" Hz");

    m_spinCwWpm = new QSpinBox(settingsGroup);
    m_spinCwWpm->setRange(5, 60);
    m_spinCwWpm->setSingleStep(1);
    m_spinCwWpm->setSuffix(" WPM");
    m_chkCwAutoWpm = new QCheckBox(uiText("cw_auto_wpm", "Auto WPM"), settingsGroup);
    m_lblCwTrackedWpm = new QLabel(uiText("cw_tracked_wpm", "Tracked: -- WPM"), settingsGroup);
    m_lblCwTrackedWpm->setStyleSheet(QStringLiteral("font-weight: 500;"));
    m_lblCwDualRx = new QLabel(uiText("cw_dual_rx_status", "RX A: left click · RX B: right click on waterfall"), settingsGroup);
    m_lblCwDualRx->setWordWrap(true);
    m_lblCwDualRx->setStyleSheet(QStringLiteral("color: #3aa8ff; font-weight: 500;"));

    m_btnCwDisableSecondary = new QPushButton(uiText("cw_disable_rx_b", "Disable RX B"), settingsGroup);
    m_btnCwDisableSecondary->setToolTip(uiText("cw_disable_rx_b_tooltip", "Remove the blue secondary CW marker. RX A keeps running."));
    connect(m_btnCwDisableSecondary, &QPushButton::clicked, this, &MainWindow::disableCwSecondaryRx);

    m_spinCwBandwidthHz = new QSpinBox(settingsGroup);
    m_chkCwAfc = new QCheckBox("AFC", settingsGroup);
    m_spinCwAfcRangeHz = new QSpinBox(settingsGroup);
    m_spinCwBandwidthHz->setRange(40, 500);
    m_spinCwBandwidthHz->setSingleStep(10);
    m_spinCwBandwidthHz->setSuffix(" Hz");

    m_spinCwAfcRangeHz->setRange(5, 100);
    m_spinCwAfcRangeHz->setSingleStep(5);
    m_spinCwAfcRangeHz->setPrefix(QString::fromUtf8("±"));
    m_spinCwAfcRangeHz->setSuffix(" Hz");

    // CW RX is now the assimilated skimmer.  Manual bandwidth/AFC controls are
    // intentionally hidden: the engine owns adaptive per-bin noise, threshold
    // and hysteresis internally.  The old setting objects remain allocated only
    // so older settings files/load paths stay harmless.
    m_chkCwSoftwareAgc = nullptr;
    m_spinCwBandwidthHz->setVisible(false);
    m_chkCwAfc->setVisible(false);
    m_spinCwAfcRangeHz->setVisible(false);

    grid->addWidget(new QLabel("RX A tone", settingsGroup), 0, 0);
    grid->addWidget(m_spinCwToneHz, 0, 1, 1, 2);
    grid->addWidget(new QLabel("Speed hint", settingsGroup), 1, 0);
    grid->addWidget(m_spinCwWpm, 1, 1, 1, 2);
    grid->addWidget(m_chkCwAutoWpm, 2, 0, 1, 1);
    grid->addWidget(m_lblCwTrackedWpm, 2, 1, 1, 2);
    grid->addWidget(m_lblCwDualRx, 3, 0, 1, 2);
    grid->addWidget(m_btnCwDisableSecondary, 3, 2);
    grid->setColumnStretch(1, 1);

    outerLayout->addWidget(settingsGroup);
    placeQsoFormInModePanel(outerLayout, m_cwQsoForm);
    outerLayout->addStretch(1);

    ui->stkModeSettings->addWidget(m_pageCwSettings);

    loadCwSettingsToUi();
}

void MainWindow::loadCwSettingsToUi()
{
    if (m_spinCwToneHz == nullptr ||
        m_spinCwWpm == nullptr ||
        m_chkCwAutoWpm == nullptr ||
        m_spinCwBandwidthHz == nullptr ||
        m_chkCwAfc == nullptr ||
        m_spinCwAfcRangeHz == nullptr) {
        return;
    }

    const QSignalBlocker blockTone(m_spinCwToneHz);
    const QSignalBlocker blockWpm(m_spinCwWpm);
    const QSignalBlocker blockAutoWpm(m_chkCwAutoWpm);
    const QSignalBlocker blockBandwidth(m_spinCwBandwidthHz);
    const QSignalBlocker blockAfc(m_chkCwAfc);
    const QSignalBlocker blockAfcRange(m_spinCwAfcRangeHz);

    m_spinCwToneHz->setValue(qBound(250, m_settings.cwToneHz, 3000));
    m_cwSecondaryEnabled = m_settings.cwSecondaryEnabled;
    m_cwSecondaryToneHz = qBound(250, m_settings.cwSecondaryToneHz, 3000);
    updateCwDualRxStatusLabel();
    m_spinCwWpm->setValue(qBound(5, m_settings.cwWpm, 60));
    m_chkCwAutoWpm->setChecked(m_settings.cwAutoWpm);
    m_spinCwWpm->setEnabled(!m_settings.cwAutoWpm);
    if (m_lblCwTrackedWpm != nullptr) {
        m_lblCwTrackedWpm->setText(uiText("cw_tracked_wpm", "Tracked: -- WPM"));
    }
    m_spinCwBandwidthHz->setValue(qBound(40, m_settings.cwBandwidthHz, 500));
    m_chkCwAfc->setChecked(m_settings.cwAfcEnabled);
    m_spinCwAfcRangeHz->setValue(qBound(5, m_settings.cwAfcRangeHz, 100));
    m_settings.cwAgcEnabled = false;
}


void MainWindow::setupHellPage()
{
    if (ui->stkModeSettings == nullptr) {
        return;
    }

    m_pageHellSettings = new QWidget(ui->stkModeSettings);
    QVBoxLayout *outerLayout = new QVBoxLayout(m_pageHellSettings);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(8);

    QGroupBox *settingsGroup = new QGroupBox("Hellschreiber settings", m_pageHellSettings);
    QGridLayout *grid = new QGridLayout(settingsGroup);
    grid->setContentsMargins(8, 8, 8, 8);
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(6);

    m_cmbHellVariant = new QComboBox(settingsGroup);
    m_cmbHellVariant->addItem("Feld Hell", "FeldHell");
    m_cmbHellVariant->addItem("FSK-105", "FSK105");

    m_spinHellToneHz = new QSpinBox(settingsGroup);
    m_spinHellToneHz->setRange(250, 3500);
    m_spinHellToneHz->setSingleStep(10);
    m_spinHellToneHz->setSuffix(" Hz");

    m_spinHellColumnRate = new QDoubleSpinBox(settingsGroup);
    m_spinHellColumnRate->setRange(2.0, 80.0);
    m_spinHellColumnRate->setDecimals(2);
    m_spinHellColumnRate->setSingleStep(0.5);
    m_spinHellColumnRate->setSuffix(" col/s");

    m_spinHellBandwidthHz = new QSpinBox(settingsGroup);
    m_chkHellAfc = new QCheckBox("AFC", settingsGroup);
    m_spinHellAfcRangeHz = new QSpinBox(settingsGroup);
    m_spinHellBandwidthHz->setRange(40, 800);
    m_spinHellBandwidthHz->setSingleStep(5);
    m_spinHellBandwidthHz->setSuffix(" Hz");

    m_spinHellAfcRangeHz->setRange(5, 100);
    m_spinHellAfcRangeHz->setSingleStep(5);
    m_spinHellAfcRangeHz->setPrefix(QString::fromUtf8("±"));
    m_spinHellAfcRangeHz->setSuffix(" Hz");

    m_sliderHellPaperScale = new QSlider(Qt::Horizontal, settingsGroup);
    m_sliderHellPaperScale->setRange(1, 12);
    m_sliderHellPaperScale->setSingleStep(1);
    m_sliderHellPaperScale->setPageStep(1);
    m_sliderHellPaperScale->setTickPosition(QSlider::TicksBelow);
    m_sliderHellPaperScale->setTickInterval(1);
    m_lblHellPaperScale = new QLabel(settingsGroup);
    m_lblHellPaperScale->setMinimumWidth(46);

    grid->addWidget(new QLabel("Variant", settingsGroup), 0, 0);
    grid->addWidget(m_cmbHellVariant, 0, 1, 1, 2);
    grid->addWidget(new QLabel("Center tone", settingsGroup), 1, 0);
    grid->addWidget(m_spinHellToneHz, 1, 1, 1, 2);
    grid->addWidget(new QLabel("Paper speed", settingsGroup), 2, 0);
    grid->addWidget(m_spinHellColumnRate, 2, 1, 1, 2);
    grid->addWidget(new QLabel("Bandwidth", settingsGroup), 3, 0);
    grid->addWidget(m_spinHellBandwidthHz, 3, 1, 1, 2);
    grid->addWidget(m_chkHellAfc, 4, 0, 1, 1);
    grid->addWidget(new QLabel("AFC range", settingsGroup), 4, 1);
    grid->addWidget(m_spinHellAfcRangeHz, 4, 2);
    grid->addWidget(new QLabel("Paper zoom", settingsGroup), 5, 0);
    grid->addWidget(m_sliderHellPaperScale, 5, 1);
    grid->addWidget(m_lblHellPaperScale, 5, 2);
    grid->setColumnStretch(1, 1);

    outerLayout->addWidget(settingsGroup);
    placeQsoFormInModePanel(outerLayout, m_hellQsoForm);
    outerLayout->addStretch(1);

    ui->stkModeSettings->addWidget(m_pageHellSettings);

    loadHellSettingsToUi();
    refreshTextMacroButtons();
}

void MainWindow::loadHellSettingsToUi()
{
    if (m_cmbHellVariant == nullptr ||
        m_spinHellToneHz == nullptr ||
        m_spinHellColumnRate == nullptr ||
        m_spinHellBandwidthHz == nullptr ||
        m_chkHellAfc == nullptr ||
        m_spinHellAfcRangeHz == nullptr ||
        m_sliderHellPaperScale == nullptr ||
        m_lblHellPaperScale == nullptr) {
        return;
    }

    const QSignalBlocker blockVariant(m_cmbHellVariant);
    const QSignalBlocker blockTone(m_spinHellToneHz);
    const QSignalBlocker blockRate(m_spinHellColumnRate);
    const QSignalBlocker blockBandwidth(m_spinHellBandwidthHz);
    const QSignalBlocker blockAfc(m_chkHellAfc);
    const QSignalBlocker blockAfcRange(m_spinHellAfcRangeHz);
    const QSignalBlocker blockPaperScale(m_sliderHellPaperScale);

    const int variantIndex = m_cmbHellVariant->findData(m_settings.hellVariant);
    m_cmbHellVariant->setCurrentIndex(variantIndex >= 0 ? variantIndex : 0);
    m_spinHellToneHz->setValue(qBound(250, m_settings.hellToneHz, 3500));
    m_spinHellColumnRate->setValue(qBound(2.0, m_settings.hellColumnRate, 80.0));
    m_spinHellBandwidthHz->setValue(qBound(40, m_settings.hellBandwidthHz, 800));
    m_chkHellAfc->setChecked(m_settings.hellAfcEnabled);
    m_spinHellAfcRangeHz->setValue(qBound(5, m_settings.hellAfcRangeHz, 100));
    const int paperScale = qBound(1, m_settings.hellPaperScale, 12);
    m_sliderHellPaperScale->setValue(paperScale);
    m_lblHellPaperScale->setText(QStringLiteral("x%1").arg(paperScale));
}



void MainWindow::setupMsk144Page()
{
    if (ui == nullptr || ui->stkModeSettings == nullptr) {
        return;
    }

    m_pageMsk144Settings = new QWidget(ui->stkModeSettings);
    QVBoxLayout *outer = new QVBoxLayout(m_pageMsk144Settings);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(8);

    QGroupBox *rxGroup = new QGroupBox(uiText("msk144_rx_tx_settings", "MSK144 RX/TX settings"), m_pageMsk144Settings);
    QGridLayout *grid = new QGridLayout(rxGroup);
    grid->setContentsMargins(8, 8, 8, 8);
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(6);

    m_cmbMsk144Period = new QComboBox(rxGroup);
    m_cmbMsk144Period->addItem(QStringLiteral("15 s"), 15);
    m_cmbMsk144Period->addItem(QStringLiteral("30 s"), 30);
    // MSHV exposes MSK/FT/Q65 decode depth from the global Decode menu,
    // not as a bulky per-mode panel control. Keep a hidden combo as the
    // single source of truth for settings/apply logic, but expose it through
    // Decode -> MSK144 decode depth below. Mapping follows MSHV:
    //   1 = Fast   : short-ping decoder only
    //   2 = Normal : short-ping + 4-frame coherent averages
    //   3 = Deep   : short-ping + 4/5/7-frame coherent averages
    m_cmbMsk144DecodeDepth = new QComboBox(rxGroup);
    m_cmbMsk144DecodeDepth->addItem(QStringLiteral("Fast"), 1);
    m_cmbMsk144DecodeDepth->addItem(QStringLiteral("Normal"), 2);
    m_cmbMsk144DecodeDepth->addItem(QStringLiteral("Deep"), 3);
    const int savedMskDepth = qBound(1, QSettings(AppSettings::settingsFilePath(), QSettings::IniFormat)
                                           .value(QStringLiteral("MSK144/decodeDepth"), 2).toInt(), 3);
    const int savedMskDepthIndex = m_cmbMsk144DecodeDepth->findData(savedMskDepth);
    m_cmbMsk144DecodeDepth->setCurrentIndex(savedMskDepthIndex >= 0 ? savedMskDepthIndex : 1);
    m_cmbMsk144DecodeDepth->hide();

    m_spinMsk144RxFreq = new QSpinBox(rxGroup);
    m_spinMsk144RxFreq->setRange(300, 2700);
    m_spinMsk144RxFreq->setValue(1500);
    m_spinMsk144RxFreq->setSuffix(QStringLiteral(" Hz"));
    m_spinMsk144TxFreq = new QSpinBox(rxGroup);
    m_spinMsk144TxFreq->setRange(300, 2700);
    m_spinMsk144TxFreq->setValue(1500);
    m_spinMsk144TxFreq->setSuffix(QStringLiteral(" Hz"));
    m_spinMsk144DfTolerance = new QSpinBox(rxGroup);
    m_spinMsk144DfTolerance->setRange(10, 500);
    m_spinMsk144DfTolerance->setValue(100);
    m_spinMsk144DfTolerance->setPrefix(QString::fromUtf8("±"));
    m_spinMsk144DfTolerance->setSuffix(QStringLiteral(" Hz"));

    m_chkMsk144ShortMessages = new QCheckBox(uiText("msk144_short_messages", "Short messages"), rxGroup);
    m_chkMsk144Swl = new QCheckBox(uiText("msk144_swl", "SWL"), rxGroup);
    m_chkMsk144Contest = new QCheckBox(uiText("msk144_contest", "Contest"), rxGroup);
    m_chkMsk144TxFirst = new QCheckBox(uiText("msk144_tx_first", "TX first period"), rxGroup);

    m_editMsk144DxCall = new QLineEdit(rxGroup);
    m_editMsk144DxCall->setPlaceholderText(uiText("dx_callsign", "DX callsign"));
    m_editMsk144DxGrid = new QLineEdit(rxGroup);
    m_editMsk144DxGrid->setPlaceholderText(uiText("dx_grid", "DX grid"));

    m_btnMsk144GenerateStd = new QPushButton(uiText("generate_standard_messages", "Generate standard messages"), rxGroup);
    m_btnMsk144Rx = new QPushButton(uiText("rx", "RX"), rxGroup);
    m_btnMsk144Tx = new QPushButton(uiText("tx", "TX"), rxGroup);
    m_btnMsk144Stop = new QPushButton(uiText("button.stop", "STOP"), rxGroup);
    m_btnMsk144Tune = new QPushButton(uiText("tune", "Tune"), rxGroup);
    for (QPushButton *btn : {m_btnMsk144GenerateStd, m_btnMsk144Rx, m_btnMsk144Tx, m_btnMsk144Stop, m_btnMsk144Tune}) {
        if (btn != nullptr) btn->hide();
    }
    m_lblMsk144Status = new QLabel(uiText("msk144_status_idle", "MSK144: idle"), rxGroup);
    m_lblMsk144Status->setWordWrap(true);
    m_lblMsk144Status->setStyleSheet(QStringLiteral("font-weight: 500; color: #3aa8ff;"));
    m_lblMsk144PeriodStatus = new QLabel(rxGroup);
    m_lblMsk144PeriodStatus->setWordWrap(true);

    int row = 0;
    grid->addWidget(new QLabel(uiText("period", "Period"), rxGroup), row, 0);
    grid->addWidget(m_cmbMsk144Period, row, 1);
    ++row;
    grid->addWidget(new QLabel(uiText("rx_frequency", "RX freq"), rxGroup), row, 0);
    grid->addWidget(m_spinMsk144RxFreq, row, 1);
    grid->addWidget(new QLabel(uiText("tx_frequency", "TX freq"), rxGroup), row, 2);
    grid->addWidget(m_spinMsk144TxFreq, row, 3);
    ++row;
    grid->addWidget(new QLabel(uiText("df_tolerance", "DF tol"), rxGroup), row, 0);
    grid->addWidget(m_spinMsk144DfTolerance, row, 1);
    grid->addWidget(m_chkMsk144TxFirst, row, 2, 1, 2);
    ++row;
    grid->addWidget(m_chkMsk144ShortMessages, row, 0);
    grid->addWidget(m_chkMsk144Swl, row, 1);
    grid->addWidget(m_chkMsk144Contest, row, 2, 1, 2);
    ++row;
    grid->addWidget(new QLabel(uiText("dx_callsign", "DX callsign"), rxGroup), row, 0);
    grid->addWidget(m_editMsk144DxCall, row, 1);
    grid->addWidget(new QLabel(uiText("dx_grid", "DX grid"), rxGroup), row, 2);
    grid->addWidget(m_editMsk144DxGrid, row, 3);
    ++row;
    grid->addWidget(m_lblMsk144Status, row, 0, 1, 4);
    ++row;
    grid->addWidget(m_lblMsk144PeriodStatus, row, 0, 1, 4);

    QGroupBox *mskSeqGroup = new QGroupBox(uiText("msk144_sequence_status", "MSK144 sequence status"), m_pageMsk144Settings);
    QVBoxLayout *mskSeqLayout = new QVBoxLayout(mskSeqGroup);
    mskSeqLayout->setContentsMargins(8, 8, 8, 8);
    mskSeqLayout->setSpacing(4);
    m_lblMsk144SequencerStatus = new QLabel(uiText("msk144_seq_idle", "Sequencer: idle"), mskSeqGroup);
    m_lblMsk144SequencerStatus->setWordWrap(true);
    m_lblMsk144SequencerStatus->setStyleSheet(QStringLiteral("font-weight: 500;"));
    mskSeqLayout->addWidget(m_lblMsk144SequencerStatus);

    QWidget *mskQsoPanel = createQsoFormPanel(m_pageMsk144Settings, QStringLiteral("MSK144"), &m_msk144QsoForm);

    QGroupBox *mskTxGroup = new QGroupBox(uiText("standard_messages", "Standard messages"), m_pageMsk144Settings);
    QVBoxLayout *mskTxLayout = new QVBoxLayout(mskTxGroup);
    mskTxLayout->setContentsMargins(8, 8, 8, 8);
    mskTxLayout->setSpacing(4);
    if (m_tableMsk144TxMessages != nullptr) {
        m_tableMsk144TxMessages->setParent(mskTxGroup);
        m_tableMsk144TxMessages->setMinimumHeight(160);
        m_tableMsk144TxMessages->setMaximumHeight(220);
        mskTxLayout->addWidget(m_tableMsk144TxMessages);
    }

    outer->addWidget(rxGroup);
    outer->addWidget(mskSeqGroup);
    outer->addWidget(mskQsoPanel);
    outer->addWidget(mskTxGroup);
    outer->addStretch(1);
    ui->stkModeSettings->addWidget(m_pageMsk144Settings);

    auto apply = [this]() { applyMsk144Settings(); };
    connect(m_cmbMsk144Period, QOverload<int>::of(&QComboBox::currentIndexChanged), this, apply);
    connect(m_cmbMsk144DecodeDepth, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        QSettings settings(AppSettings::settingsFilePath(), QSettings::IniFormat);
        settings.setValue(QStringLiteral("MSK144/decodeDepth"),
                          m_cmbMsk144DecodeDepth != nullptr ? m_cmbMsk144DecodeDepth->currentData().toInt() : 2);
        applyMsk144Settings();
    });
    connect(m_spinMsk144RxFreq, QOverload<int>::of(&QSpinBox::valueChanged), this, apply);
    connect(m_spinMsk144TxFreq, QOverload<int>::of(&QSpinBox::valueChanged), this, apply);
    connect(m_spinMsk144DfTolerance, QOverload<int>::of(&QSpinBox::valueChanged), this, apply);
    connect(m_chkMsk144ShortMessages, &QCheckBox::toggled, this, apply);
    connect(m_chkMsk144Swl, &QCheckBox::toggled, this, apply);
    connect(m_chkMsk144Contest, &QCheckBox::toggled, this, apply);
    connect(m_editMsk144DxCall, &QLineEdit::textChanged, this, [this, apply]() { apply(); refreshMsk144StandardMessages(); });
    connect(m_editMsk144DxGrid, &QLineEdit::textChanged, this, &MainWindow::refreshMsk144StandardMessages);
    connect(m_btnMsk144GenerateStd, &QPushButton::clicked, this, &MainWindow::refreshMsk144StandardMessages);
    connect(m_btnMsk144Rx, &QPushButton::clicked, this, &MainWindow::startMsk144RxShell);
    connect(m_btnMsk144Tx, &QPushButton::clicked, this, &MainWindow::startMsk144TxShell);
    connect(m_btnMsk144Stop, &QPushButton::clicked, this, &MainWindow::stopMsk144Shell);
    if (m_btnMsk144Tune != nullptr) {
        m_btnMsk144Tune->setVisible(false); // no non-standard diagnostic tune waveform in MSK144
        m_btnMsk144Tune->setEnabled(false);
    }

    if (ui != nullptr && ui->menubar != nullptr && m_cmbMsk144DecodeDepth != nullptr) {
        QMenu *decodeMenu = new QMenu(uiText("menu_decode", "Decode"), this);
        hardenPopupMenuForFullscreen(decodeMenu);

        QMenu *mskDepthMenu = decodeMenu->addMenu(uiText("msk144_decode_depth", "MSK144 decode depth"));
        hardenPopupMenuForFullscreen(mskDepthMenu);
        QActionGroup *depthGroup = new QActionGroup(mskDepthMenu);
        depthGroup->setExclusive(true);

        auto addDepthAction = [this, mskDepthMenu, depthGroup](const QString &label, int value, const QString &hint) {
            QAction *action = mskDepthMenu->addAction(label);
            action->setCheckable(true);
            action->setData(value);
            action->setToolTip(hint);
            action->setStatusTip(hint);
            depthGroup->addAction(action);
            if (m_cmbMsk144DecodeDepth != nullptr && m_cmbMsk144DecodeDepth->currentData().toInt() == value) {
                action->setChecked(true);
            }
            connect(action, &QAction::triggered, this, [this, value]() {
                if (m_cmbMsk144DecodeDepth == nullptr) {
                    return;
                }
                const int index = m_cmbMsk144DecodeDepth->findData(value);
                if (index >= 0 && m_cmbMsk144DecodeDepth->currentIndex() != index) {
                    m_cmbMsk144DecodeDepth->setCurrentIndex(index);
                } else {
                    QSettings settings(AppSettings::settingsFilePath(), QSettings::IniFormat);
                    settings.setValue(QStringLiteral("MSK144/decodeDepth"), value);
                    applyMsk144Settings();
                }
                appendLog(QStringLiteral("MSK144 decode depth set to %1.")
                              .arg(value <= 1 ? QStringLiteral("Fast") : (value == 2 ? QStringLiteral("Normal") : QStringLiteral("Deep"))));
            });
        };

        addDepthAction(uiText("msk144_depth_fast", "Fast"), 1,
                       uiText("msk144_depth_fast_hint", "MSHV mapping: short-ping decoder only."));
        addDepthAction(uiText("msk144_depth_normal", "Normal"), 2,
                       uiText("msk144_depth_normal_hint", "MSHV default for MSK144: short-ping decoder plus 4-frame coherent averages."));
        addDepthAction(uiText("msk144_depth_deep", "Deep"), 3,
                       uiText("msk144_depth_deep_hint", "MSHV deep MSK144: short-ping decoder plus 4-, 5- and 7-frame coherent averages."));

        connect(m_cmbMsk144DecodeDepth, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [depthGroup, this]() {
            if (m_cmbMsk144DecodeDepth == nullptr) {
                return;
            }
            const int depth = m_cmbMsk144DecodeDepth->currentData().toInt();
            for (QAction *action : depthGroup->actions()) {
                if (action->data().toInt() == depth) {
                    action->setChecked(true);
                    break;
                }
            }
        });

        QAction *insertBefore = m_actionAppSettings != nullptr ? m_actionAppSettings : nullptr;
        if (insertBefore == nullptr && ui->menuHelp != nullptr) {
            insertBefore = ui->menuHelp->menuAction();
        }
        if (insertBefore != nullptr && ui->menubar->actions().contains(insertBefore)) {
            ui->menubar->insertMenu(insertBefore, decodeMenu);
        } else {
            ui->menubar->addMenu(decodeMenu);
        }
    }

    if (m_msk144Decoder != nullptr) {
        connect(m_msk144Decoder, &Msk144Decoder::decoded, this, &MainWindow::handleMsk144DecodeReady, Qt::QueuedConnection);
        connect(m_msk144Decoder, &Msk144Decoder::statusChanged, this, [this](const QString &s) {
            if (m_lblMsk144Status != nullptr) m_lblMsk144Status->setText(s);
            appendLog(s);
        }, Qt::QueuedConnection);
        connect(m_msk144Decoder, &Msk144Decoder::pingDetected, this, &MainWindow::handleMsk144Ping, Qt::QueuedConnection);
        connect(m_msk144Decoder, &Msk144Decoder::periodReady, this, [this](int buffered, int period) {
            if (m_lblMsk144PeriodStatus != nullptr) {
                m_lblMsk144PeriodStatus->setText(QStringLiteral("Last MSK144 period: %1/%2 s buffered").arg(buffered).arg(period));
            }
        }, Qt::QueuedConnection);
        connect(m_msk144Decoder, &Msk144Decoder::mindStats, this, [this](int scored, int promoted, double avgConfidence) {
            appendLog(QStringLiteral("MSK144 MIND Ranker: scored %1, promoted %2, avg candidate %3%")
                          .arg(scored)
                          .arg(promoted)
                          .arg(avgConfidence, 0, 'f', 1));
        }, Qt::QueuedConnection);
    }

    refreshMsk144StandardMessages();
    applyMsk144Settings();
}

void MainWindow::setupQ65Page()
{
    if (ui == nullptr || ui->stkModeSettings == nullptr) {
        return;
    }

    m_pageQ65Settings = new QWidget(ui->stkModeSettings);
    QVBoxLayout *outer = new QVBoxLayout(m_pageQ65Settings);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(8);

    QGroupBox *rxGroup = new QGroupBox(uiText("q65_rx_tx_settings", "Q65 RX/TX settings"), m_pageQ65Settings);
    QGridLayout *grid = new QGridLayout(rxGroup);
    grid->setContentsMargins(8, 8, 8, 8);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(6);

    m_cmbQ65Submode = new QComboBox(rxGroup);
    m_cmbQ65Submode->addItem(QStringLiteral("A"), 1);
    m_cmbQ65Submode->addItem(QStringLiteral("B"), 2);
    m_cmbQ65Submode->addItem(QStringLiteral("C"), 4);
    m_cmbQ65Submode->addItem(QStringLiteral("D"), 8);
    const int savedSubmode = qBound(1, QSettings(AppSettings::settingsFilePath(), QSettings::IniFormat)
                                           .value(QStringLiteral("Q65/submode"), 1).toInt(), 8);
    int submodeIndex = m_cmbQ65Submode->findData(savedSubmode);
    if (submodeIndex < 0) submodeIndex = 0;
    m_cmbQ65Submode->setCurrentIndex(submodeIndex);

    m_cmbQ65Period = new QComboBox(rxGroup);
    m_cmbQ65Period->addItem(QStringLiteral("15 s"), 15);
    m_cmbQ65Period->addItem(QStringLiteral("30 s"), 30);
    m_cmbQ65Period->addItem(QStringLiteral("60 s"), 60);
    m_cmbQ65Period->addItem(QStringLiteral("120 s"), 120);
    const int savedPeriod = qBound(15, QSettings(AppSettings::settingsFilePath(), QSettings::IniFormat)
                                          .value(QStringLiteral("Q65/period"), 60).toInt(), 120);
    int periodIndex = m_cmbQ65Period->findData(savedPeriod);
    if (periodIndex < 0) periodIndex = m_cmbQ65Period->findData(60);
    m_cmbQ65Period->setCurrentIndex(periodIndex >= 0 ? periodIndex : 2);

    m_cmbQ65DecodeDepth = new QComboBox(rxGroup);
    m_cmbQ65DecodeDepth->addItem(QStringLiteral("Fast"), 1);
    m_cmbQ65DecodeDepth->addItem(QStringLiteral("Normal"), 2);
    m_cmbQ65DecodeDepth->addItem(QStringLiteral("Deep"), 3);
    const int savedDepth = qBound(1, QSettings(AppSettings::settingsFilePath(), QSettings::IniFormat)
                                         .value(QStringLiteral("Q65/decodeDepth"), 2).toInt(), 3);
    const int depthIndex = m_cmbQ65DecodeDepth->findData(savedDepth);
    m_cmbQ65DecodeDepth->setCurrentIndex(depthIndex >= 0 ? depthIndex : 1);

    m_spinQ65RxFreq = new QSpinBox(rxGroup);
    m_spinQ65RxFreq->setRange(300, 2700);
    m_spinQ65RxFreq->setValue(1500);
    m_spinQ65RxFreq->setSuffix(QStringLiteral(" Hz"));
    m_spinQ65TxFreq = new QSpinBox(rxGroup);
    m_spinQ65TxFreq->setRange(300, 2700);
    m_spinQ65TxFreq->setValue(1500);
    m_spinQ65TxFreq->setSuffix(QStringLiteral(" Hz"));
    m_spinQ65DfTolerance = new QSpinBox(rxGroup);
    m_spinQ65DfTolerance->setRange(10, 1000);
    m_spinQ65DfTolerance->setValue(100);
    m_spinQ65DfTolerance->setPrefix(QString::fromUtf8("±"));
    m_spinQ65DfTolerance->setSuffix(QStringLiteral(" Hz"));

    m_chkQ65AverageDecode = new QCheckBox(uiText("q65_avg_decode", "Average"), rxGroup);
    m_chkQ65AverageDecode->setChecked(true);
    m_chkQ65AutoClearAvg = new QCheckBox(uiText("q65_auto_clear_avg", "Auto clear AVG"), rxGroup);
    m_chkQ65AutoClearAvg->setChecked(true);
    m_chkQ65SingleDecode = new QCheckBox(uiText("q65_single_decode", "Single decode"), rxGroup);
    m_chkQ65ApDecode = new QCheckBox(uiText("q65_ap_decode", "AP decode"), rxGroup);
    m_chkQ65ApDecode->setChecked(true);
    m_chkQ65MaxDrift = new QCheckBox(uiText("q65_max_drift", "Max drift"), rxGroup);
    m_chkQ65EmeDelay = new QCheckBox(uiText("q65_eme_delay", "EME delay"), rxGroup);

    m_editQ65DxCall = new QLineEdit(rxGroup);
    m_editQ65DxCall->setPlaceholderText(uiText("dx_callsign", "DX callsign"));
    m_editQ65DxGrid = new QLineEdit(rxGroup);
    m_editQ65DxGrid->setPlaceholderText(uiText("dx_grid", "DX grid"));

    m_btnQ65GenerateStd = new QPushButton(uiText("generate_standard_messages", "Generate standard messages"), rxGroup);
    m_btnQ65Rx = new QPushButton(uiText("rx", "RX"), rxGroup);
    m_btnQ65Tx = new QPushButton(uiText("tx", "TX"), rxGroup);
    m_btnQ65Stop = new QPushButton(uiText("button.stop", "STOP"), rxGroup);
    m_btnQ65ClearAvg = new QPushButton(uiText("q65_clear_avg", "Clear AVG"), rxGroup);
    for (QPushButton *btn : {m_btnQ65GenerateStd, m_btnQ65Rx, m_btnQ65Tx, m_btnQ65Stop, m_btnQ65ClearAvg}) {
        if (btn != nullptr) btn->hide();
    }
    m_lblQ65Status = new QLabel(uiText("q65_status_idle", "Q65: idle"), rxGroup);
    m_lblQ65Status->setWordWrap(true);
    m_lblQ65Status->setStyleSheet(QStringLiteral("font-weight: 500; color: #3aa8ff;"));
    m_lblQ65AverageStatus = new QLabel(QStringLiteral("AVG: 0 | 0"), rxGroup);
    m_lblQ65AverageStatus->setWordWrap(true);

    auto compactLabel = [rxGroup](const QString &label) {
        QLabel *l = new QLabel(label, rxGroup);
        l->setWordWrap(false);
        l->setMinimumWidth(46);
        l->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
        return l;
    };
    auto compactInput = [](QWidget *widget) {
        if (widget != nullptr) {
            widget->setMinimumWidth(78);
            widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        }
    };
    for (QWidget *w : {static_cast<QWidget *>(m_cmbQ65Submode), static_cast<QWidget *>(m_cmbQ65Period),
                      static_cast<QWidget *>(m_cmbQ65DecodeDepth), static_cast<QWidget *>(m_spinQ65RxFreq),
                      static_cast<QWidget *>(m_spinQ65TxFreq), static_cast<QWidget *>(m_spinQ65DfTolerance),
                      static_cast<QWidget *>(m_editQ65DxCall), static_cast<QWidget *>(m_editQ65DxGrid)}) {
        compactInput(w);
    }
    m_chkQ65AutoClearAvg->setText(uiText("q65_auto_clear_avg_short", "Auto clear"));
    m_chkQ65SingleDecode->setText(uiText("q65_single_decode_short", "Single"));
    m_chkQ65ApDecode->setText(uiText("q65_ap_decode_short", "AP"));
    m_chkQ65MaxDrift->setText(uiText("q65_max_drift_short", "Drift"));
    m_chkQ65EmeDelay->setText(uiText("q65_eme_delay_short", "EME"));

    int row = 0;
    grid->addWidget(compactLabel(uiText("submode", "Sub")), row, 0);
    grid->addWidget(m_cmbQ65Submode, row, 1);
    grid->addWidget(compactLabel(uiText("period", "Period")), row, 2);
    grid->addWidget(m_cmbQ65Period, row, 3);
    ++row;
    grid->addWidget(compactLabel(uiText("decode_depth", "Decode")), row, 0);
    grid->addWidget(m_cmbQ65DecodeDepth, row, 1);
    grid->addWidget(compactLabel(uiText("df_tolerance", "DF")), row, 2);
    grid->addWidget(m_spinQ65DfTolerance, row, 3);
    ++row;
    grid->addWidget(compactLabel(uiText("rx_frequency", "RX")), row, 0);
    grid->addWidget(m_spinQ65RxFreq, row, 1);
    grid->addWidget(compactLabel(uiText("tx_frequency", "TX")), row, 2);
    grid->addWidget(m_spinQ65TxFreq, row, 3);
    ++row;
    grid->addWidget(m_chkQ65AverageDecode, row, 0, 1, 2);
    grid->addWidget(m_chkQ65AutoClearAvg, row, 2, 1, 2);
    ++row;
    grid->addWidget(m_chkQ65SingleDecode, row, 0, 1, 2);
    grid->addWidget(m_chkQ65ApDecode, row, 2, 1, 2);
    ++row;
    grid->addWidget(m_chkQ65MaxDrift, row, 0, 1, 2);
    grid->addWidget(m_chkQ65EmeDelay, row, 2, 1, 2);
    ++row;
    grid->addWidget(compactLabel(uiText("dx_callsign", "DX")), row, 0);
    grid->addWidget(m_editQ65DxCall, row, 1);
    grid->addWidget(compactLabel(uiText("dx_grid", "Grid")), row, 2);
    grid->addWidget(m_editQ65DxGrid, row, 3);
    ++row;
    grid->addWidget(m_lblQ65Status, row++, 0, 1, 4);
    grid->addWidget(m_lblQ65AverageStatus, row++, 0, 1, 4);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(3, 1);

    QGroupBox *q65SeqGroup = new QGroupBox(uiText("q65_sequence_status", "Q65 sequence status"), m_pageQ65Settings);
    QVBoxLayout *q65SeqLayout = new QVBoxLayout(q65SeqGroup);
    q65SeqLayout->setContentsMargins(8, 8, 8, 8);
    q65SeqLayout->setSpacing(4);
    m_lblQ65SequencerStatus = new QLabel(uiText("q65_seq_idle", "Sequencer: idle"), q65SeqGroup);
    m_lblQ65SequencerStatus->setWordWrap(true);
    m_lblQ65SequencerStatus->setStyleSheet(QStringLiteral("font-weight: 500;"));
    q65SeqLayout->addWidget(m_lblQ65SequencerStatus);

    QWidget *q65QsoPanel = createQsoFormPanel(m_pageQ65Settings, QStringLiteral("Q65"), &m_q65QsoForm);

    QGroupBox *txGroup = new QGroupBox(uiText("standard_messages", "Standard messages"), m_pageQ65Settings);
    QVBoxLayout *txLayout = new QVBoxLayout(txGroup);
    txLayout->setContentsMargins(8, 8, 8, 8);
    txLayout->setSpacing(4);
    if (m_tableQ65TxMessages != nullptr) {
        m_tableQ65TxMessages->setParent(txGroup);
        m_tableQ65TxMessages->setMinimumHeight(160);
        m_tableQ65TxMessages->setMaximumHeight(220);
        txLayout->addWidget(m_tableQ65TxMessages);
    }

    outer->addWidget(rxGroup);
    outer->addWidget(q65SeqGroup);
    outer->addWidget(q65QsoPanel);
    outer->addWidget(txGroup);
    outer->addStretch(1);
    ui->stkModeSettings->addWidget(m_pageQ65Settings);

    auto apply = [this]() { applyQ65Settings(); };
    connect(m_cmbQ65Submode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        QSettings settings(AppSettings::settingsFilePath(), QSettings::IniFormat);
        settings.setValue(QStringLiteral("Q65/submode"), m_cmbQ65Submode != nullptr ? m_cmbQ65Submode->currentData().toInt() : 1);
        applyQ65Settings();
        refreshQ65StandardMessages();
    });
    connect(m_cmbQ65Period, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        QSettings settings(AppSettings::settingsFilePath(), QSettings::IniFormat);
        settings.setValue(QStringLiteral("Q65/period"), m_cmbQ65Period != nullptr ? m_cmbQ65Period->currentData().toInt() : 60);
        applyQ65Settings();
        refreshQ65StandardMessages();
    });
    connect(m_cmbQ65DecodeDepth, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        QSettings settings(AppSettings::settingsFilePath(), QSettings::IniFormat);
        settings.setValue(QStringLiteral("Q65/decodeDepth"), m_cmbQ65DecodeDepth != nullptr ? m_cmbQ65DecodeDepth->currentData().toInt() : 2);
        applyQ65Settings();
    });
    connect(m_spinQ65RxFreq, QOverload<int>::of(&QSpinBox::valueChanged), this, apply);
    connect(m_spinQ65TxFreq, QOverload<int>::of(&QSpinBox::valueChanged), this, apply);
    connect(m_spinQ65DfTolerance, QOverload<int>::of(&QSpinBox::valueChanged), this, apply);
    connect(m_chkQ65AverageDecode, &QCheckBox::toggled, this, apply);
    connect(m_chkQ65AutoClearAvg, &QCheckBox::toggled, this, apply);
    connect(m_chkQ65SingleDecode, &QCheckBox::toggled, this, apply);
    connect(m_chkQ65ApDecode, &QCheckBox::toggled, this, apply);
    connect(m_chkQ65MaxDrift, &QCheckBox::toggled, this, apply);
    connect(m_chkQ65EmeDelay, &QCheckBox::toggled, this, apply);
    connect(m_editQ65DxCall, &QLineEdit::textChanged, this, [this, apply]() { apply(); refreshQ65StandardMessages(); });
    connect(m_editQ65DxGrid, &QLineEdit::textChanged, this, &MainWindow::refreshQ65StandardMessages);
    connect(m_btnQ65GenerateStd, &QPushButton::clicked, this, &MainWindow::refreshQ65StandardMessages);
    connect(m_btnQ65Rx, &QPushButton::clicked, this, &MainWindow::startQ65RxShell);
    connect(m_btnQ65Tx, &QPushButton::clicked, this, &MainWindow::startQ65TxShell);
    connect(m_btnQ65Stop, &QPushButton::clicked, this, &MainWindow::stopQ65Shell);
    connect(m_btnQ65ClearAvg, &QPushButton::clicked, this, [this]() { if (m_q65Decoder != nullptr) m_q65Decoder->clearAverages(); });

    if (m_q65Decoder == nullptr) {
        m_q65Decoder = new Q65Decoder(this);
        connect(m_q65Decoder, &Q65Decoder::decoded, this, &MainWindow::handleQ65DecodeReady, Qt::QueuedConnection);
        connect(m_q65Decoder, &Q65Decoder::statusChanged, this, [this](const QString &s) {
            if (m_lblQ65Status != nullptr) m_lblQ65Status->setText(s);
        }, Qt::QueuedConnection);
        connect(m_q65Decoder, &Q65Decoder::periodReady, this, [this](int buffered, int period) {
            if (m_lblQ65AverageStatus != nullptr) {
                m_lblQ65AverageStatus->setText(QStringLiteral("Last Q65 period: %1/%2 s buffered").arg(buffered).arg(period));
            }
        }, Qt::QueuedConnection);
        connect(m_q65Decoder, &Q65Decoder::averageStatusChanged, this, [this](int usable, int all) {
            if (m_lblQ65AverageStatus != nullptr) {
                m_lblQ65AverageStatus->setText(QStringLiteral("AVG: %1 | %2").arg(usable).arg(all));
            }
        }, Qt::QueuedConnection);
    }

    refreshQ65StandardMessages();
    applyQ65Settings();
}

void MainWindow::setupFt8Page()
{
    if (m_mainDisplayStack == nullptr || ui->stkModeSettings == nullptr) {
        return;
    }

    m_ft8DisplayPage = new QWidget(m_mainDisplayStack);
    QVBoxLayout *mainLayout = new QVBoxLayout(m_ft8DisplayPage);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    QSplitter *splitter = new QSplitter(Qt::Horizontal, m_ft8DisplayPage);

    QGroupBox *rxGroup = new QGroupBox(uiText("received_decodes", "Received decodes"), splitter);
    QVBoxLayout *rxLayout = new QVBoxLayout(rxGroup);
    rxLayout->setContentsMargins(8, 8, 8, 8);

    QHBoxLayout *rxToolsLayout = new QHBoxLayout();
    rxToolsLayout->setContentsMargins(0, 0, 0, 0);
    rxToolsLayout->setSpacing(8);
    m_btnFt8ClearRx = new QPushButton(uiText("ft_clear_decodes", "Clear decodes"), rxGroup);
    m_chkFt8AutoScroll = new QCheckBox(uiText("ft_auto_scroll", "Auto-scroll"), rxGroup);
    m_chkFt8AutoScroll->setChecked(true);
    m_btnFt8ClearRx->setToolTip(uiText("ft_clear_decodes_tooltip", "Clears the visible FT4/FT8 decode list and transient waterfall CQ/reply callouts."));
    m_chkFt8AutoScroll->setToolTip(uiText("ft_auto_scroll_tooltip", "Keep the decode table scrolled to the newest line while receiving."));
    rxToolsLayout->addWidget(m_btnFt8ClearRx);
    rxToolsLayout->addWidget(m_chkFt8AutoScroll);
    rxToolsLayout->addStretch(1);
    rxLayout->addLayout(rxToolsLayout);

    m_tableFt8Rx = new QTableWidget(0, 9, rxGroup);
    m_tableFt8Rx->setHorizontalHeaderLabels(QStringList()
        << "UTC" << uiText("db", "dB") << uiText("dt", "DT") << uiText("frequency", "Freq")
        << uiText("message", "Message") << uiText("callsign", "Call")
        << uiText("locator", "Locator") << uiText("bearing", "Bearing")
        << uiText("country", "Country"));
    m_tableFt8Rx->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableFt8Rx->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableFt8Rx->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableFt8Rx->verticalHeader()->setVisible(false);
    m_tableFt8Rx->horizontalHeader()->setStretchLastSection(true);
    m_tableFt8Rx->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_tableFt8Rx->setColumnWidth(0, 58);  // UTC HHMMSS
    m_tableFt8Rx->setColumnWidth(1, 42);  // dB
    m_tableFt8Rx->setColumnWidth(2, 42);  // DT
    m_tableFt8Rx->setColumnWidth(3, 58);  // Freq
    m_tableFt8Rx->setColumnWidth(4, 260); // Message
    m_tableFt8Rx->setColumnWidth(5, 90);  // Call
    m_tableFt8Rx->setColumnWidth(6, 72);  // Locator
    m_tableFt8Rx->setColumnWidth(7, 66);  // Bearing
    m_tableFt8Rx->setColumnWidth(8, 130); // Country
    m_tableFt8Rx->setAlternatingRowColors(true);
    m_tableFt8Rx->setItemDelegate(new FtDecodeRowDelegate(m_tableFt8Rx));
    applyDecodeTableVisualSettings(m_tableFt8Rx);
    rxLayout->addWidget(m_tableFt8Rx);

    QGroupBox *qsoActivityGroup = new QGroupBox(uiText("ft_qso_activity", "QSO activity / RX frequency"), splitter);
    QVBoxLayout *qsoActivityLayout = new QVBoxLayout(qsoActivityGroup);
    qsoActivityLayout->setContentsMargins(8, 8, 8, 8);
    m_tableFt8QsoHistory = new QTableWidget(0, 6, qsoActivityGroup);
    m_tableFt8QsoHistory->setHorizontalHeaderLabels(QStringList()
        << "UTC" << uiText("direction", "Dir") << uiText("db", "dB/Tag")
        << uiText("dt", "DT") << uiText("frequency", "Freq") << uiText("message", "Message"));
    m_tableFt8QsoHistory->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableFt8QsoHistory->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableFt8QsoHistory->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableFt8QsoHistory->verticalHeader()->setVisible(false);
    m_tableFt8QsoHistory->horizontalHeader()->setStretchLastSection(true);
    m_tableFt8QsoHistory->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_tableFt8QsoHistory->setColumnWidth(0, 58); // UTC HHMMSS
    m_tableFt8QsoHistory->setColumnWidth(1, 34); // Dir
    m_tableFt8QsoHistory->setColumnWidth(2, 54); // dB/Tag
    m_tableFt8QsoHistory->setColumnWidth(3, 42); // DT
    m_tableFt8QsoHistory->setColumnWidth(4, 58); // Freq
    m_tableFt8QsoHistory->setAlternatingRowColors(false);
    applyDecodeTableVisualSettings(m_tableFt8QsoHistory);
    qsoActivityLayout->addWidget(m_tableFt8QsoHistory);

    splitter->addWidget(rxGroup);
    splitter->addWidget(qsoActivityGroup);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    mainLayout->addWidget(splitter, 1);

    QGroupBox *qsoGroup = new QGroupBox(m_ft8DisplayPage);
    QGridLayout *qsoGrid = new QGridLayout(qsoGroup);
    qsoGrid->setContentsMargins(8, 8, 8, 8);
    qsoGrid->setHorizontalSpacing(8);
    qsoGrid->setVerticalSpacing(6);

    // Station identity is not part of the FT activity page.  My Call and My
    // Locator live only in Settings -> User/QTH; TX is blocked until those
    // station settings are explicitly configured.
    m_editFt8MyCall = nullptr;
    m_editFt8MyGrid = nullptr;

    m_editFt8DxCall = new QLineEdit(qsoGroup);
    m_editFt8DxGrid = new QLineEdit(qsoGroup);

    m_cmbFt8Band = new QComboBox(qsoGroup);
    m_cmbFt8Band->addItems(QStringList() << "160m" << "80m" << "60m" << "40m" << "30m"
                                         << "20m" << "17m" << "15m" << "12m" << "10m"
                                         << "6m" << "2m" << "70cm" << "23cm");
    m_cmbFt8Band->setToolTip(uiText("ft_band_tooltip", "Select a band to QSY the CAT-connected rig to the standard FT4/FT8 frequency. Off-slot operation is allowed and shown in red."));
    m_lblFt8BandStatus = new QLabel(qsoGroup);
    m_lblFt8BandStatus->setWordWrap(true);
    m_lblFt8BandStatus->setStyleSheet(QStringLiteral("font-weight: 500;"));

    m_editFt8DxCall->setPlaceholderText(uiText("dx_callsign", "DX callsign"));
    m_editFt8DxGrid->setPlaceholderText(uiText("dx_grid", "DX grid"));

    m_cmbFt8Band->setMinimumContentsLength(8);
    m_cmbFt8Band->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_cmbFt8Band->setMinimumWidth(150);

    qsoGrid->addWidget(new QLabel(uiText("frequency", "Frequency"), qsoGroup), 0, 0);
    qsoGrid->addWidget(m_lblFt8BandStatus, 0, 1);
    qsoGrid->addWidget(new QLabel(uiText("band", "Band"), qsoGroup), 1, 0);
    qsoGrid->addWidget(m_cmbFt8Band, 1, 1);
    qsoGrid->addWidget(new QLabel(uiText("dx_call", "DX Call"), qsoGroup), 2, 0);
    qsoGrid->addWidget(m_editFt8DxCall, 2, 1);
    qsoGrid->addWidget(new QLabel(uiText("dx_grid", "DX Grid"), qsoGroup), 3, 0);
    qsoGrid->addWidget(m_editFt8DxGrid, 3, 1);
    m_btnFt8GenerateStd = new QPushButton(uiText("generate_standard_messages", "Generate standard messages"), qsoGroup);
    qsoGrid->addWidget(m_btnFt8GenerateStd, 4, 0, 1, 2);
    qsoGrid->setColumnStretch(1, 1);

    // The FT QSO fields belong in the right-side Mode tab, not in the central
    // activity area.  They are added to settingsLayout below, replacing the old
    // oversized clock block.

    QGroupBox *controlGroup = new QGroupBox(m_ft8DisplayPage);
    QGridLayout *controlGrid = new QGridLayout(controlGroup);
    controlGrid->setContentsMargins(8, 8, 8, 8);
    controlGrid->setHorizontalSpacing(8);
    controlGrid->setVerticalSpacing(6);

    m_radioFt8TxFirst = new QRadioButton(uiText("tx_first_period", "TX first period"), controlGroup);
    m_radioFt8TxSecond = new QRadioButton(uiText("tx_second_period", "TX second period"), controlGroup);
    m_btnFt8Rx = new QPushButton(uiText("rx", "RX"), controlGroup);
    m_btnFt8Tx = new QPushButton(uiText("tx", "TX"), controlGroup);
    m_btnFt8Stop = new QPushButton(uiText("button.stop", "STOP"), controlGroup);
    m_btnFt8Tune = new QPushButton(uiText("tune", "Tune"), controlGroup);
    m_chkFt8AutoSeq = new QCheckBox(uiText("ft8_auto_sequence", "Auto sequence"), controlGroup);
    // Auto-sequence is the normal FT4/FT8 operating model.  Keep the widget as
    // an internal compatibility setting, but do not expose it as an operator
    // toggle in the hot path.
    m_chkFt8CqRepeat = new QCheckBox(uiText("ft8_cq_retry", "CQ retry"), controlGroup);
    m_chkFt8AutoLog = new QCheckBox(uiText("ft8_auto_log", "Auto log completed QSOs"), controlGroup);
    m_chkFt8FullAutoQso = new QCheckBox(uiText("ft8_auto_qso", "Auto QSO"), controlGroup);
    m_chkFt8FullAutoQso->setToolTip(uiText("ft8_auto_qso_tooltip", "WSJT-Z-style gated automation. Decoded CQs are buffered and prioritized by new country, new grid square, new band, new mode, distance and SNR before answering. Visible only after Evil Mode is explicitly unlocked."));
    m_chkFt8FullAutoQso->setStyleSheet(QStringLiteral("QCheckBox { font-weight: 500; color: #ffb347; }"));
    m_chkFt8HoldTxFreq = new QCheckBox(uiText("hold_tx_frequency", "Hold TX frequency"), controlGroup);
    m_chkFt8HoldTxFreq->setToolTip(uiText("hold_tx_frequency_tooltip", "Legacy shortcut: force the red TX marker to stay fixed. The TX strategy selector in the FT settings page is the primary control."));
    m_cmbFt8TxStrategy = new QComboBox(controlGroup);
    m_cmbFt8TxStrategy->addItem(uiText("ft_tx_strategy_auto_free", "TX auto-free slot"), QStringLiteral("auto_free"));
    m_cmbFt8TxStrategy->addItem(uiText("ft_tx_strategy_fixed", "TX fixed/manual marker"), QStringLiteral("fixed"));
    m_cmbFt8TxStrategy->addItem(uiText("ft_tx_strategy_follow_rx", "TX on correspondent frequency"), QStringLiteral("follow_rx"));
    m_cmbFt8TxStrategy->setToolTip(uiText("ft_tx_strategy_tooltip", "Controls the red FT TX marker. Auto-free chooses a clear audio slot with side guards; fixed uses the manual red marker; follow uses the selected/correspondent RX frequency."));
    m_spinFt8CqTimeoutMin = new QSpinBox(controlGroup);
    m_spinFt8CqTimeoutMin->setRange(1, 99);
    m_spinFt8CqTimeoutMin->setSuffix(QStringLiteral(" x"));
    m_spinFt8CqTimeoutMin->setToolTip(uiText("ft8_cq_retry_count_tooltip", "Number of CQ transmissions when CQ retry is enabled. After the last unanswered CQ, MM returns to RX standby."));

    m_spinFt8NoResponseLimit = new QSpinBox(controlGroup);
    m_spinFt8NoResponseLimit->setRange(1, 12);
    m_spinFt8NoResponseLimit->setSuffix(QStringLiteral(" x"));
    m_spinFt8NoResponseLimit->setToolTip(uiText("ft8_no_response_limit_tooltip", "Maximum number of unanswered transmissions for a started FT QSO, including the first call. If the DX does not reply, MM cancels the action and returns to RX standby."));

    controlGrid->addWidget(m_radioFt8TxFirst, 0, 0);
    controlGrid->addWidget(m_radioFt8TxSecond, 0, 1);
    controlGrid->addWidget(m_btnFt8Rx, 0, 2);
    controlGrid->addWidget(m_btnFt8Tx, 0, 3);
    controlGrid->addWidget(m_btnFt8Stop, 0, 4);
    controlGrid->addWidget(m_btnFt8Tune, 0, 5);
    m_chkFt8AutoSeq->setChecked(true);
    m_chkFt8AutoSeq->hide();
    controlGrid->addWidget(m_chkFt8CqRepeat, 1, 0);
    m_lblFt8CqTimeout = new QLabel(uiText("ft8_cq_retry_count", "CQ retry count"), controlGroup);
    controlGrid->addWidget(m_lblFt8CqTimeout, 1, 1);
    controlGrid->addWidget(m_spinFt8CqTimeoutMin, 1, 2);
    m_lblFt8NoResponseLimit = new QLabel(uiText("ft8_no_response_limit", "No-response limit"), controlGroup);
    m_lblFt8NoResponseLimit->setToolTip(m_spinFt8NoResponseLimit->toolTip());
    controlGrid->addWidget(m_lblFt8NoResponseLimit, 1, 3);
    controlGrid->addWidget(m_spinFt8NoResponseLimit, 1, 4);
    controlGrid->addWidget(m_chkFt8HoldTxFreq, 2, 0);
    controlGrid->addWidget(m_cmbFt8TxStrategy, 2, 1, 1, 2);
    controlGrid->addWidget(m_chkFt8AutoLog, 2, 3);
    controlGrid->addWidget(m_chkFt8FullAutoQso, 2, 4);
    updateFt8EvilModeVisibility();

    m_lblFt8TxBanner = new QLabel(controlGroup);
    m_lblFt8TxBanner->setAlignment(Qt::AlignCenter);
    m_lblFt8TxBanner->setWordWrap(true);
    m_lblFt8TxBanner->setMinimumHeight(58);
    m_lblFt8TxBanner->setMinimumWidth(260);
    m_lblFt8TxBanner->setMaximumWidth(420);
    m_lblFt8TxBanner->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_lblFt8TxBanner->setStyleSheet(QStringLiteral("QLabel { border: 2px solid #33414d; border-radius: 6px; padding: 6px; background: #17202a; color: #d7e0e7; font-weight: 500; font-size: 10pt; }"));
    m_lblFt8TxBanner->setText(uiText("ft_tx_banner_rx", "RX monitor — no FT TX armed"));
    // Keep the live TX banner beside the operator controls, WSJT-X style, so
    // the next/current message is visible without pushing controls downward.
    controlGrid->addWidget(m_lblFt8TxBanner, 0, 6, 3, 1);
    // Keep the operator controls and the live TX banner in a stable 66/33
    // horizontal balance.  Previously only the banner column stretched, so on
    // wide desktops it drifted too far right and became oversized.
    for (int c = 0; c < 6; ++c) {
        controlGrid->setColumnStretch(c, 1);
    }
    controlGrid->setColumnStretch(6, 3);

    mainLayout->addWidget(controlGroup);

    m_ft8DisplayPage = wrapTextDisplayPageWithMap(m_ft8DisplayPage, uiText("tab_ft_activity", "FT activity"), QStringLiteral("FT8"), &m_ftQsoMapWidget);
    m_mainDisplayStack->addWidget(m_ft8DisplayPage);

    m_pageFt8Settings = new QWidget(ui->stkModeSettings);
    QVBoxLayout *settingsLayout = new QVBoxLayout(m_pageFt8Settings);
    settingsLayout->setContentsMargins(0, 0, 0, 0);
    settingsLayout->setSpacing(8);

    QGroupBox *clockGroup = new QGroupBox(QString(), ui->tabStatus);
    m_grpFt8UtcClock = clockGroup;
    QVBoxLayout *clockLayout = new QVBoxLayout(clockGroup);
    clockLayout->setContentsMargins(8, 8, 8, 8);
    clockLayout->setSpacing(4);

    m_ft8SlotClock = new Ft8SlotClockWidget(clockGroup);
    m_lblFt8PeriodStatus = new QLabel(clockGroup);
    m_lblFt8PeriodStatus->setAlignment(Qt::AlignCenter);
    m_lblFt8PeriodStatus->setStyleSheet("font-weight: 500;");
    m_lblFt8PeriodStatus->hide();
    QLabel *utcCaption = new QLabel(uiText("ft8_utc", "UTC"), clockGroup);
    utcCaption->setAlignment(Qt::AlignCenter);
    m_lcdFt8UtcClock = new QLCDNumber(clockGroup);
    m_lcdFt8UtcClock->setDigitCount(8);
    m_lcdFt8UtcClock->setSegmentStyle(QLCDNumber::Filled);
    m_lcdFt8UtcClock->setMinimumHeight(40);
    m_lcdFt8UtcClock->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_lblFt8WindowStatus = new QLabel(uiText("ft8_rx_window", "RX window"), clockGroup);
    m_lblFt8WindowStatus->setAlignment(Qt::AlignCenter);
    m_lblFt8WindowStatus->setStyleSheet("font-weight: 500; font-size: 9pt;");

    clockLayout->addWidget(m_ft8SlotClock, 0, Qt::AlignHCenter);
    clockLayout->addWidget(utcCaption);
    clockLayout->addWidget(m_lcdFt8UtcClock);
    clockLayout->addWidget(m_lblFt8WindowStatus);

    QGroupBox *sequencerGroup = new QGroupBox(uiText("ft8_sequence_status", "FT8 sequence status"), m_pageFt8Settings);
    QVBoxLayout *sequencerLayout = new QVBoxLayout(sequencerGroup);
    sequencerLayout->setContentsMargins(8, 8, 8, 8);
    sequencerLayout->setSpacing(6);
    m_lblFt8SlotStatus = new QLabel(uiText("ft8_slot", "FT8 slot: --"), sequencerGroup);
    m_lblFt8SlotStatus->setWordWrap(true);
    m_lblFt8SequencerStatus = new QLabel(uiText("ft8_seq_idle", "Sequencer: idle"), sequencerGroup);
    m_lblFt8SequencerStatus->setWordWrap(true);
    m_lblFt8SequencerStatus->setStyleSheet(QStringLiteral("font-weight: 500;"));
    sequencerLayout->addWidget(m_lblFt8SlotStatus);
    sequencerLayout->addWidget(m_lblFt8SequencerStatus);

    // The old separate "FT8 signal report" box duplicated information already
    // shown by the decode table and the active sequence status.  Keep the
    // pointers null so updateFt8SignalReportUi() becomes a cheap no-op.
    m_lblFt8LastSnr = nullptr;
    m_lblFt8TxReport = nullptr;

    // Keep the FT UTC slot clock in the Status tab, visible only in FT4/FT8.
    m_pageFt8Time = nullptr;
    // The obsolete FT decode diagnostics tab is fully removed from the UI.
    // Keep only the hidden decode-depth combo used by legacy settings loaders.
    m_tabFtDecodeDiagnostics = nullptr;
    m_cmbFtLiveDecodeDepth = new QComboBox(this);
    m_cmbFtLiveDecodeDepth->addItem(uiText("ft_live_decode_unified", "Unified"), QStringLiteral("adaptive"));
    m_cmbFtLiveDecodeDepth->setCurrentIndex(0);
    m_cmbFtLiveDecodeDepth->hide();
    m_grpFt8DecodePerformance = nullptr;
    m_lblFt8DecodePerformance = nullptr;

    // FT WAV analysis and the bundled benchmark are developer/test tools.
    // They intentionally live under Settings rather than in the always-visible
    // side UI, so normal QSO operation keeps the FT screen clean.
    m_btnFtDecodeAnalyzeWav = nullptr;
    m_btnFtDecodeAutoTest = nullptr;

    if (ui->statusTabLayout != nullptr) {
        const int insertIndex = qMax(0, ui->statusTabLayout->count() - 1);
        ui->statusTabLayout->insertWidget(insertIndex, clockGroup, 0);
    }
    updateFtUtcClockVisibility(ui != nullptr && ui->cmbMode != nullptr
                                   ? ui->cmbMode->currentText()
                                   : QString());
    updateFt8DecodePerformanceUi();

    QGroupBox *settingsGroup = new QGroupBox(m_pageFt8Settings);
    QGridLayout *settingsGrid = new QGridLayout(settingsGroup);
    settingsGrid->setContentsMargins(8, 8, 8, 8);
    settingsGrid->setHorizontalSpacing(6);
    settingsGrid->setVerticalSpacing(6);

    m_spinFt8RxFreq = new QSpinBox(settingsGroup);
    m_spinFt8TxFreq = new QSpinBox(settingsGroup);
    // v4.10: converge to one operator-facing FT receive engine.  Fast/Deep/Deep Max
    // remain internal pipeline stages and diagnostics only, not UI modes.
    m_lblFt8DecodeEngine = new QLabel(uiText("ft_engine_unified_native", "FT unified adaptive decoder"), settingsGroup);
    m_lblFt8DecodeEngine->setToolTip(uiText("ft_engine_tooltip_unified_native", "Single FT decoder path: baseline decode, CRC-valid subtract/residual recovery, and AP/OSD lab attempts on promising candidates."));
    // v2.91: live/offline FT decode stays on the fast single-pass path.  Keep
    // hidden compatibility widgets so older settings/signal code remains safe,
    // but do not expose the former Deep/DSP++ check boxes in the UI.
    m_chkFt8DeepDecode = new QCheckBox(settingsGroup);
    m_chkFt8DeepDecode->setChecked(false);
    m_chkFt8DeepDecode->hide();
    m_chkFt8DspPlusDecode = new QCheckBox(settingsGroup);
    m_chkFt8DspPlusDecode->setChecked(false);
    m_chkFt8DspPlusDecode->hide();
    m_btnFt8AnalyzeWav = nullptr;
    m_chkFt8EvilMode = new QCheckBox(uiText("ft8_evil_mode", "Evil mode"), settingsGroup);
    m_chkFt8EvilMode->setToolTip(uiText("ft8_evil_mode_tooltip", "Shows WSJT-Z-style Auto CQ / Auto QSO controls only after an explicit typed confirmation. This unlock is deliberately session-only."));
    m_chkFt8EvilMode->setStyleSheet(QStringLiteral("QCheckBox { font-weight: 500; color: #ffb347; }"));
    const QList<QSpinBox *> ft8FrequencySpins = {m_spinFt8RxFreq, m_spinFt8TxFreq};
    for (QSpinBox *spin : ft8FrequencySpins) {
        spin->setRange(100, 3000);
        spin->setSingleStep(10);
        spin->setSuffix(" Hz");
    }

    settingsGrid->addWidget(new QLabel(uiText("ft_decode_engine", "FT decoder"), settingsGroup), 0, 0);
    settingsGrid->addWidget(m_lblFt8DecodeEngine, 0, 1);
    settingsGrid->addWidget(new QLabel(uiText("rx_audio_marker", "RX audio marker"), settingsGroup), 1, 0);
    settingsGrid->addWidget(m_spinFt8RxFreq, 1, 1);
    settingsGrid->addWidget(new QLabel(uiText("tx_audio_marker", "TX audio marker"), settingsGroup), 2, 0);
    settingsGrid->addWidget(m_spinFt8TxFreq, 2, 1);
    settingsGrid->addWidget(m_chkFt8EvilMode, 3, 0, 1, 2);
    settingsGrid->setColumnStretch(1, 1);

    QGroupBox *txGroup = new QGroupBox(m_pageFt8Settings);
    QVBoxLayout *txLayout = new QVBoxLayout(txGroup);
    txLayout->setContentsMargins(8, 8, 8, 8);
    txLayout->setSpacing(4);
    m_tableFt8TxMessages = new QTableWidget(6, 3, txGroup);
    QFont ftTxTableFont = m_tableFt8TxMessages->font();
    if (ftTxTableFont.pointSize() > 0) {
        // Autoscale the standard-message table font instead of adding a fixed
        // +2 pt.  That fixed bump looked fine on some Windows DPI settings but
        // became oversized on Linux themes such as Mint/Cinnamon.
#if defined(Q_OS_LINUX)
        const int targetPointSize = qBound(8, QApplication::font().pointSize(), 10);
#else
        const int targetPointSize = qBound(9, QApplication::font().pointSize() + 1, 12);
#endif
        ftTxTableFont.setPointSize(targetPointSize);
        m_tableFt8TxMessages->setFont(ftTxTableFont);
        m_tableFt8TxMessages->horizontalHeader()->setFont(ftTxTableFont);
    }
    m_tableFt8TxMessages->setHorizontalHeaderLabels(QStringList() << QString() << uiText("tx", "Tx") << uiText("message", "Message"));
    m_tableFt8TxMessages->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableFt8TxMessages->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableFt8TxMessages->verticalHeader()->setVisible(false);
    m_tableFt8TxMessages->horizontalHeader()->setStretchLastSection(true);
    m_tableFt8TxMessages->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_tableFt8TxMessages->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_tableFt8TxMessages->setColumnWidth(0, 28);
    m_tableFt8TxMessages->setColumnWidth(1, 34); // v1.89: TX label column reduced by about 50%.
    m_tableFt8TxMessages->setAlternatingRowColors(true);
    m_tableFt8TxMessages->setMinimumHeight(190);
    m_tableFt8TxMessages->setMaximumHeight(260);
    for (int i = 0; i < 6; ++i) {
        QTableWidgetItem *arrow = new QTableWidgetItem(QString());
        arrow->setTextAlignment(Qt::AlignCenter);
        arrow->setFlags(arrow->flags() & ~Qt::ItemIsEditable);
        QTableWidgetItem *label = new QTableWidgetItem(QString("Tx %1").arg(i + 1));
        label->setFlags(label->flags() & ~Qt::ItemIsEditable);
        QTableWidgetItem *message = new QTableWidgetItem(QString());
        m_tableFt8TxMessages->setItem(i, 0, arrow);
        m_tableFt8TxMessages->setItem(i, 1, label);
        m_tableFt8TxMessages->setItem(i, 2, message);
    }
    m_tableFt8TxMessages->setToolTip(uiText("ft_tx_arrow_tooltip", "The arrow marks the standard message currently selected or armed by the FT auto-sequencer."));
    m_tableFt8TxMessages->selectRow(0);
    setFt8ActiveTxRow(0);
    applyDecodeTableVisualSettings(m_tableFt8TxMessages);
    txLayout->addWidget(m_tableFt8TxMessages);

    settingsLayout->addWidget(settingsGroup);
    settingsLayout->addWidget(txGroup);
    // Sequence state is now represented by the live TX banner and activity log;
    // keep the label object for internal status updates but do not show the old
    // bulky separate block in the Mode tab.
    sequencerGroup->hide();
    settingsLayout->addWidget(qsoGroup);
    settingsLayout->addStretch(1);
    ui->stkModeSettings->addWidget(m_pageFt8Settings);

    loadFt8SettingsToUi();
    refreshFt8StandardMessages();
    updateFt8SlotStatus();
    updateFt8SignalReportUi();
    updateFt8TxBannerUi();
}

void MainWindow::loadFt8SettingsToUi()
{
    if (m_editFt8DxCall == nullptr || m_editFt8DxGrid == nullptr ||
        m_cmbFt8Band == nullptr || m_spinFt8RxFreq == nullptr ||
        m_spinFt8TxFreq == nullptr || m_lblFt8DecodeEngine == nullptr ||
        m_radioFt8TxFirst == nullptr ||
        m_radioFt8TxSecond == nullptr || m_chkFt8AutoSeq == nullptr ||
        m_chkFt8CqRepeat == nullptr || m_chkFt8AutoLog == nullptr ||
        m_chkFt8HoldTxFreq == nullptr || m_spinFt8CqTimeoutMin == nullptr ||
        m_spinFt8NoResponseLimit == nullptr ||
        m_cmbFtLiveDecodeDepth == nullptr) {
        return;
    }

    const QSignalBlocker blockDxCall(m_editFt8DxCall);
    const QSignalBlocker blockDxGrid(m_editFt8DxGrid);
    const QSignalBlocker blockBand(m_cmbFt8Band);
    const QSignalBlocker blockRx(m_spinFt8RxFreq);
    const QSignalBlocker blockTx(m_spinFt8TxFreq);
    const QSignalBlocker blockEngine(m_lblFt8DecodeEngine);
    const QSignalBlocker blockFirst(m_radioFt8TxFirst);
    const QSignalBlocker blockSecond(m_radioFt8TxSecond);
    const QSignalBlocker blockAutoSeq(m_chkFt8AutoSeq);
    const QSignalBlocker blockCqRepeat(m_chkFt8CqRepeat);
    const QSignalBlocker blockAutoLog(m_chkFt8AutoLog);
    const QSignalBlocker blockFullAuto(m_chkFt8FullAutoQso);
    const QSignalBlocker blockEvil(m_chkFt8EvilMode);
    const QSignalBlocker blockHoldTx(m_chkFt8HoldTxFreq);
    const QSignalBlocker blockTxStrategy(m_cmbFt8TxStrategy);
    const QSignalBlocker blockDeepDecode(m_chkFt8DeepDecode);
    const QSignalBlocker blockDspPlusDecode(m_chkFt8DspPlusDecode);
    const QSignalBlocker blockCqTimeout(m_spinFt8CqTimeoutMin);
    const QSignalBlocker blockNoResponseLimit(m_spinFt8NoResponseLimit);
    const QSignalBlocker blockLiveDecodeDepth(m_cmbFtLiveDecodeDepth);

    m_editFt8DxCall->setText(m_settings.ft8DxCallsign);
    m_editFt8DxGrid->setText(m_settings.ft8DxGrid);
    const int bandIndex = m_cmbFt8Band->findText(m_settings.ft8Band);
    m_cmbFt8Band->setCurrentIndex(bandIndex >= 0 ? bandIndex : m_cmbFt8Band->findText("20m"));
    m_spinFt8RxFreq->setValue(qBound(100, m_settings.ft8RxFrequencyHz, 3000));
    m_spinFt8TxFreq->setValue(qBound(100, m_settings.ft8TxFrequencyHz, 3000));
    m_settings.ft8DecodeEngine = QStringLiteral("mshv");
    m_lblFt8DecodeEngine->setText(uiText("ft_engine_unified_native", "FT unified adaptive decoder"));
    m_radioFt8TxFirst->setChecked(m_settings.ft8TxFirstPeriod);
    m_radioFt8TxSecond->setChecked(!m_settings.ft8TxFirstPeriod);
    m_settings.ft8AutoSequence = true;
    m_chkFt8AutoSeq->setChecked(true);
    m_chkFt8AutoSeq->hide();
    m_chkFt8CqRepeat->setChecked(m_settings.ft8CqRepeat);
    m_chkFt8AutoLog->setChecked(m_settings.ft8AutoLog);
    if (m_chkFt8FullAutoQso != nullptr) {
        m_chkFt8FullAutoQso->setChecked(false);
    }
    m_ft8EvilModeUnlocked = false;
    if (m_chkFt8EvilMode != nullptr) {
        m_chkFt8EvilMode->setChecked(false);
    }
    updateFt8EvilModeVisibility();
    m_chkFt8HoldTxFreq->setChecked(m_settings.ft8TxFrequencyStrategy == QStringLiteral("fixed") || m_settings.ft8HoldTxFrequency);
    const int strategyIndex = m_cmbFt8TxStrategy->findData(m_settings.ft8TxFrequencyStrategy);
    m_cmbFt8TxStrategy->setCurrentIndex(strategyIndex >= 0 ? strategyIndex : m_cmbFt8TxStrategy->findData(QStringLiteral("auto_free")));
    const QString liveDepth = QStringLiteral("adaptive");
    const int depthIndex = m_cmbFtLiveDecodeDepth->findData(liveDepth);
    m_cmbFtLiveDecodeDepth->setCurrentIndex(depthIndex >= 0 ? depthIndex : 0);
    m_settings.ft8LiveDecodeDepth = liveDepth;
    m_settings.ft8DeepDecode = true;
    m_settings.ft8DspPlusDecode = true;
    if (m_chkFt8DeepDecode != nullptr) {
        m_chkFt8DeepDecode->setChecked(m_settings.ft8DeepDecode);
    }
    if (m_chkFt8DspPlusDecode != nullptr) {
        m_chkFt8DspPlusDecode->setChecked(m_settings.ft8DspPlusDecode);
    }
    m_spinFt8CqTimeoutMin->setValue(qBound(1, m_settings.ft8CqRepeatCount, 99));
    m_spinFt8NoResponseLimit->setValue(qBound(1, m_settings.ft8NoResponseRetryCount, 12));
    updateFtBandFrequencyUi();
    updateFt8SequencerUi();
}


void MainWindow::setupHelpTooltips()
{
    // v1.18: keep the side tabs full-width.  Older helper defaults narrowed the
    // QTabWidget after setupCustomWidgets(), leaving blank space at the right.
    ui->sideTabWidget->setMinimumWidth(300);
    ui->sideTabWidget->setMaximumWidth(360);
    ui->sideTabWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    ui->sideTabWidget->tabBar()->setElideMode(Qt::ElideNone);
    ui->sideTabWidget->tabBar()->setUsesScrollButtons(false);
    ui->mainHorizontalSplitter->setStretchFactor(0, 1);
    ui->mainHorizontalSplitter->setStretchFactor(1, 0);
    ui->mainHorizontalSplitter->setHandleWidth(1);
    ui->mainHorizontalSplitter->setSizes(QList<int>() << 1500 << 320);

    ui->faxSettingsGridLayout->setColumnStretch(0, 0);
    ui->faxSettingsGridLayout->setColumnStretch(1, 1);
    ui->statusGridLayout->setColumnStretch(0, 0);
    ui->statusGridLayout->setColumnStretch(1, 1);

    ui->cmbFaxLpm->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    ui->spinFaxBlackHz->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    ui->spinFaxWhiteHz->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    ui->cmbFaxLinePreset->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    ui->spinFaxImageLines->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    ui->spinFaxEndTimeoutSec->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    ui->spinFaxSlantPpm->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // v46: the old artificial WEFAX image buffer and slant controls are no
    // longer exposed. The received image grows line-by-line and slant
    // correction is intentionally disabled to preserve the known-good timing.
    ui->lblFaxImageLines->setVisible(false);
    ui->spinFaxImageLines->setVisible(false);
    ui->chkFaxAutoSlant->setVisible(false);
    ui->lblFaxSlantPpm->setVisible(false);
    ui->spinFaxSlantPpm->setVisible(false);

    // v50: keep automatic WEFAX saving simple.  The path field looked like a
    // loaded WAV filename and did not add value in the compact mode panel.
    // Auto-save still uses the default WeatherFAX directory internally.
    ui->editFaxOutputFolder->setVisible(false);
    ui->btnFaxBrowseOutputFolder->setVisible(false);
    ui->editFaxOutputFolder->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    ui->lblFaxLinePreset->setText("Station");
    ui->lblFaxImageLines->setText("Max lines");
    ui->lblFaxEndTimeout->setText("Stop wait");
    ui->chkFaxAutoStartPhasing->setText(uiText("wefax_apt_start", "APT start"));
    ui->chkFaxAutoToneTracking->setText(uiText("wefax_auto_tones", "Auto tones"));
    ui->chkFaxInputBandpass->setText(uiText("wefax_bandpass", "Band-pass"));
    ui->chkFaxEndOfSignal->setText("Detect stop/end");
    ui->chkFaxAutoSave->setText("Auto save");
    ui->lblFaxZoomStatus->setText("Zoom: fit");
    ui->lblFaxHint->setVisible(false);
    ui->cmbSstvMode->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    ui->lblSstvHint->setVisible(false);
    if (ui->pageSstvSettings != nullptr) {
        ui->pageSstvSettings->setProperty("cockpitCompactPanel", true);
    }
    if (ui->grpSstvSettings != nullptr) {
        ui->grpSstvSettings->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
        ui->grpSstvSettings->setMaximumHeight(430);
    }
    if (ui->sstvSettingsGridLayout != nullptr) {
        ui->sstvSettingsGridLayout->setContentsMargins(5, 5, 5, 5);
        ui->sstvSettingsGridLayout->setHorizontalSpacing(5);
        ui->sstvSettingsGridLayout->setVerticalSpacing(3);
        for (int r = 0; r < ui->sstvSettingsGridLayout->rowCount(); ++r) {
            ui->sstvSettingsGridLayout->setRowStretch(r, 0);
        }
    }
    const QList<QPushButton *> sstvSideButtons = {
        ui->btnSstvAnalyzeWav, ui->btnSstvResetImage, ui->btnSstvSaveImage,
        ui->btnSstvZoomFit, ui->btnSstvZoom100, ui->btnSstvZoomOut, ui->btnSstvZoomIn
    };
    for (QPushButton *b : sstvSideButtons) {
        if (b != nullptr) {
            b->setMinimumHeight(24);
            b->setMaximumHeight(28);
            b->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        }
    }
    if (ui->lineSstvActionsSection != nullptr) {
        ui->lineSstvActionsSection->setVisible(false);
    }
    if (ui->lineSstvZoomSection != nullptr) {
        ui->lineSstvZoomSection->setVisible(false);
    }
    const QList<QWidget *> sstvAdvancedHidden = {
        ui->lblSstvHorizontalShift,
        ui->spinSstvHorizontalShift,
        ui->lblSstvRedShift,
        ui->spinSstvRedShift,
        ui->lblSstvBlueShift,
        ui->spinSstvBlueShift
    };
    for (QWidget *w : sstvAdvancedHidden) {
        if (w != nullptr) {
            w->setVisible(false);
            w->setEnabled(false);
        }
    }
    if (ui->sstvSettingsLayout != nullptr && ui->sstvSettingsLayout->count() > 1) {
        if (QLayoutItem *item = ui->sstvSettingsLayout->itemAt(1)) {
            if (QSpacerItem *spacer = item->spacerItem()) {
                spacer->changeSize(0, 0, QSizePolicy::Minimum, QSizePolicy::Fixed);
            }
        }
        ui->sstvSettingsLayout->setSpacing(4);
        ui->sstvSettingsLayout->invalidate();
    }
    if (ui->pageSstvSettings != nullptr) {
        ui->pageSstvSettings->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    }

    const QString modeHelp = uiText("tooltip.mode_selector", "Select the active modem mode: MeteoFax/WEFAX, SSTV, RTTY, PSK/QPSK, MFSK, CW Morse, Feld Hell, FT4 or FT8.");
    setHelpText(ui->cmbMode, modeHelp);
    setHelpText(ui->btnStartRx, uiText("tooltip.status_rx", "Start or stop live RX from the Status tab. Ready keeps audio input closed for offline WAV analysis; RX starts live decoding from the selected audio input."));
    setHelpText(ui->btnStopRx, "Compatibility stop control kept hidden; the Status RX button now toggles live RX.");
    ui->btnTxTone->setText(uiText("button.transport_tx", "● TX"));
    setHelpText(ui->btnTxTone, uiText("tooltip.status_tx", "Start or stop TX for the current mode when TX content is ready. FT modes keep their slot-safe TX controls in the Mode tab."));
    setHelpText(ui->btnTestPtt, "Pulse the configured RTS PTT serial line for a short hardware test from Settings/diagnostics.");
    setHelpText(m_grpTxImage, uiText("tooltip.image_tx_group", "Load a PNG/JPEG/BMP image, preview it adapted to WEFAX or SSTV, then transmit it as generated audio."));
    setHelpText(m_btnLoadTxImage, "Load a still image for TX. Supported formats depend on Qt image plugins, typically PNG, JPEG/JPG and BMP.");
    setHelpText(m_btnStartImageTx, "Transmit the loaded image using the active mode settings. Waterfall shows the generated TX audio; the image overlay shows transmitted rows.");
    setHelpText(m_btnStopImageTx, "Abort the active image transmission and release RTS PTT.");
    setHelpText(m_progressTx, "Image transmission progress. The shaded area on the preview shows pixels/lines already transmitted.");

    setHelpText(ui->progressAudioLevel, "Current input audio level. Keep it safely below clipping while still high enough for reliable decoding.");
    setHelpText(ui->lblAudioLevelDb, "RMS input level in dBFS. Very low values indicate no audio or wrong input device.");
    setHelpText(ui->lblEstimatedFrequency, "Dominant audio frequency estimated by the common DSP engine and shown on the waterfall.");
    setHelpText(ui->lblDecoderState, "Current decoder state and diagnostic text from the selected modem.");
    setHelpText(ui->txtLog, "Runtime status log. Use the dedicated runtime log button for longer CAT/FT diagnostics when debugging timing or PTT issues.");
    setHelpText(ui->cmbAudioInput, "Compatibility device selector. The real audio/PTT setup is in Settings > Audio / PTT settings; this field mirrors saved devices for older layouts.");
    setHelpText(ui->cmbAudioOutput, uiText("tooltip.compat_audio_output", "Compatibility output selector. Use Settings > Audio / PTT settings to choose the TX audio device used by text modes, SSTV/WEFAX and FT Tune."));
    setHelpText(ui->cmbPttPort, "Compatibility PTT serial selector. Prefer Settings > Audio / PTT settings or CAT / Rig for normal configuration.");
    setHelpText(ui->btnRefreshDevices, "Refresh visible audio and serial device lists after plugging/unplugging USB sound cards or radio interfaces.");
    if (m_waterfallWidget != nullptr) {
        const QString waterfallHelp = uiText("tooltip.waterfall_widget",
                                            "Live audio waterfall. Click on a signal to tune the active mode marker. In FT4/FT8 green is RX and red is TX; in CW the marker selects the Morse tone.");
        // Do not use a normal hover tooltip on the GL waterfall: on several
        // Linux/Wayland/X11 themes it becomes a large black rectangle over the
        // spectrum.  Keep the help text available through the status bar and
        // What's This? context help only.
        m_waterfallWidget->setToolTip(QString());
        m_waterfallWidget->setStatusTip(waterfallHelp);
        m_waterfallWidget->setWhatsThis(waterfallHelp);
    }
    if (m_faxImageWidget != nullptr) {
        setHelpText(m_faxImageWidget, uiText("tooltip.image_preview", "Decoded image preview for WEFAX/SSTV. Mouse wheel zooms and drag pans when zoomed in."));
    }


    const QString lpmHelp = "WeatherFax line rate in lines per minute. Common HF WEFAX is 120 LPM; some signals use 60, 90, 180 or 240 LPM.";
    const QString blackHelp = uiText("tooltip.wefax_black_tone", "Black tone frequency in Hz. Typical HF WEFAX is near 1500 Hz; auto tone tracking can refine it during reception.");
    const QString whiteHelp = uiText("tooltip.wefax_white_tone", "White tone frequency in Hz. Typical HF WEFAX is near 2300 Hz; the decoder maps black-to-white tone range into grayscale.");
    const QString autoStartHelp = uiText("tooltip.wefax_apt_start", "Enable automatic WEFAX APT start detection and initial phasing. Disable it for manual/free-running test files.");
    const QString autoToneHelp = uiText("tooltip.wefax_auto_tones", "Enable AFC tracking of the black/white tone pair. The marker pair moves together and ignores start/sync guard periods.");
    const QString bandpassHelp = uiText("tooltip.wefax_bandpass", "Apply a decoder-only band-pass around the selected black/white tone range to reject noise outside the useful WEFAX audio band.");
    const QString slantHelp = "Removed in v45/v46: slant correction is disabled to preserve the stable WEFAX line timing path.";
    const QString presetHelp = "Select a station example. The entry sets practical WEFAX defaults and shows example RF frequencies in the tooltip/status text.";
    const QString linesHelp = "Internal safety maximum only. The received image grows line by line and no artificial image buffer is shown.";
    const QString eosHelp = "Complete and optionally auto-save the frame when the decoder sees an APT stop/end tone or when the useful WEFAX signal disappears for the timeout.";
    const QString timeoutHelp = "End-of-signal timeout in seconds. Increase it for noisy HF channels; decrease it for faster unattended saving after the control/end tone or signal loss.";
    const QString saveHelp = "Automatically save a completed WEFAX frame as PNG using WeatherFAX_YYYYMMDD_HHMMSS.png in the selected folder.";
    const QString folderHelp = "Folder used for automatic PNG saving. The folder is created automatically if it does not exist.";
    const QString wavHelp = "Decode a WAV recording directly through the MeteoFax DSP path without playing it to speakers. WAV analysis uses manual/free-running timing from the first sample, then locks the first stable WEFAX sync stripe before showing/saving rows.";
    const QString zoomHelp = "Image zoom controls. Mouse wheel zooms over the image; drag pans when zoomed in.";
    const QString resetHelp = "Clear the current MeteoFax image and restart the decoder start/stop state machine.";
    const QString manualSaveHelp = "Save the currently displayed MeteoFax image as a PNG file selected manually.";

    setHelpText(ui->cmbFaxLpm, lpmHelp);
    setHelpText(ui->spinFaxBlackHz, blackHelp);
    setHelpText(ui->spinFaxWhiteHz, whiteHelp);
    setHelpText(ui->chkFaxAutoStartPhasing, autoStartHelp);
    setHelpText(ui->chkFaxAutoToneTracking, autoToneHelp);
    setHelpText(ui->chkFaxInputBandpass, bandpassHelp);
    setHelpText(ui->chkFaxAutoSlant, slantHelp);
    setHelpText(ui->spinFaxSlantPpm, slantHelp);
    setHelpText(ui->cmbFaxLinePreset, presetHelp);
    setHelpText(ui->spinFaxImageLines, linesHelp);
    setHelpText(ui->chkFaxEndOfSignal, eosHelp);
    setHelpText(ui->spinFaxEndTimeoutSec, timeoutHelp);
    setHelpText(ui->chkFaxAutoSave, saveHelp);
    setHelpText(ui->editFaxOutputFolder, folderHelp);
    setHelpText(ui->btnFaxBrowseOutputFolder, folderHelp);
    setHelpText(ui->btnFaxAnalyzeWav, wavHelp);
    setHelpText(ui->btnFaxZoomFit, zoomHelp);
    setHelpText(ui->btnFaxZoom100, zoomHelp);
    setHelpText(ui->btnFaxZoomOut, zoomHelp);
    setHelpText(ui->btnFaxZoomIn, zoomHelp);
    setHelpText(ui->btnFaxResetImage, resetHelp);
    setHelpText(ui->btnFaxSaveImage, manualSaveHelp);
    if (m_btnFaxForceRx != nullptr) setHelpText(m_btnFaxForceRx, uiText("tooltip.forceWefaxRx", "Start live RX and force MeteoFax to receive immediately, bypassing APT start/phasing wait."));

    setHelpText(ui->cmbSstvMode, uiText("tooltip.sstv_mode", "Select the manual SSTV mode. Martin, Scottie, Robot and PD modes are supported for RX/TX; Robot/PD use classic SSTV YUV timing."));
    setHelpText(ui->chkSstvAutoSync, uiText("tooltip.sstv_auto_sync", "Lock each SSTV line using the 1200 Hz horizontal sync pulse. Disable only for controlled tests with stable timing."));
    setHelpText(ui->spinSstvHorizontalShift, uiText("tooltip.sstv_h_phase", "Manual horizontal phase correction in pixels. Use negative values to roll the image left and positive values to roll it right when the SSTV frame appears cylindrically wrapped."));
    setHelpText(ui->spinSstvRedShift, "Manual red-channel registration in pixels. Leave at 0 unless the whole image phase is correct but red still has a horizontal fringe.");
    setHelpText(ui->spinSstvBlueShift, "Manual blue-channel registration in pixels. Leave at 0 unless the whole image phase is correct but blue still has a horizontal fringe.");
    setHelpText(ui->btnSstvAnalyzeWav, uiText("tooltip.sstv_analyze_wav", "Decode an SSTV WAV recording directly through the active SSTV decoder without playing it to speakers."));
    setHelpText(ui->btnSstvResetImage, uiText("tooltip.sstv_reset_image", "Clear the current SSTV image and restart the selected SSTV decoder."));
    setHelpText(ui->btnSstvSaveImage, uiText("tooltip.sstv_save_png", "Save the currently displayed SSTV image as a PNG file selected manually."));
    setHelpText(ui->btnSstvZoomFit, zoomHelp);
    setHelpText(ui->btnSstvZoom100, zoomHelp);
    setHelpText(ui->btnSstvZoomOut, zoomHelp);
    setHelpText(ui->btnSstvZoomIn, zoomHelp);

    if (m_cmbRttyPreset != nullptr) {
        setHelpText(m_cmbRttyPreset, "Select a common RTTY baud/shift/tone preset. Use Custom for manual values.");
        setHelpText(m_spinRttyBaud, "RTTY symbol rate. Common amateur value is 45.45 baud; 50, 75 and 100 baud are also available for compatibility tests.");
        setHelpText(m_spinRttyShiftHz, "Frequency shift between mark and space tones. Common amateur narrow shift is 170 Hz; utility modes often use 425 or 850 Hz.");
        setHelpText(m_spinRttyMarkHz, "Audio mark tone in Hz. High-tone AFSK commonly uses 2125 Hz; low-tone AFSK often uses 1275 Hz. Waterfall click tuning sets this marker for the current session.");
        setHelpText(m_chkRttyReverse, "Invert mark/space logic. Enable when decoded text is garbage but the signal is otherwise strong and centered.");
        if (m_chkRttyAutoReverse != nullptr) {
            setHelpText(m_chkRttyAutoReverse, "Let the RTTY decoder flip Reverse polarity automatically when framing repeatedly looks inverted. Manual Reverse polarity remains available.");
        }
        setHelpText(m_chkRttyAfc, "Enable narrow AFC around the current RTTY markers. Mark and Space are searched independently so each carrier can settle on its own local energy peak.");
        setHelpText(m_spinRttyAfcRangeHz, "Maximum AFC search window around each RTTY marker. Start with ±20 Hz; use smaller values for crowded contest bands.");
        setHelpText(m_chkRttyMultiDecode, "Run lightweight parallel RTTY shadow decoders over the waterfall while the main terminal remains tuned to the selected signal.");
        setHelpText(m_chkRttyOverlayCallsigns, "Show detected CQ/callsign labels above candidate RTTY signals on the waterfall.");
        setHelpText(m_chkRttyContestEnhanced, "Use a denser signal scan and more stable tracking for crowded RTTY contest bands.");
        setHelpText(m_chkRttySecondPass, "Run a guarded second scan after the strongest RTTY candidates are marked, to find weaker nearby signals.");
        setHelpText(m_spinRttyMaxDecoders, "Maximum number of parallel RTTY shadow decoders. Lower this on older PCs.");
        if (m_rttyScopeWidget != nullptr) {
            setHelpText(m_rttyScopeWidget, "CRT-style RTTY tuning scope. A brighter, more stable centered trace indicates stronger and better-balanced Mark/Space reception.");
        }
        setHelpText(m_txtRttyRx, "Decoded RTTY text. Uses ITA2/Baudot letters/figures shift.");
        setHelpText(m_txtRttyTx, "Text to transmit as ITA2/Baudot AFSK RTTY. Unsupported characters are converted to spaces.");
        setHelpText(m_btnRttyClearRx, "Clear the received RTTY text window.");
        setHelpText(m_btnRttyLoadTxText, "Load plain text for RTTY transmission.");
        setHelpText(m_btnRttyClearTx, "Clear the RTTY transmission text.");
    }

    if (m_spinBpsk31ToneHz != nullptr) {
        if (m_cmbBpsk31Variant != nullptr) {
            setHelpText(m_cmbBpsk31Variant, "Select BPSK/QPSK symbol rate. Higher rates need stronger, cleaner and wider signals; QPSK variants in this build are experimental.");
        }
        setHelpText(m_spinBpsk31ToneHz, "PSK audio carrier tone. 1000 Hz is a safe default for sound-card testing; center the signal on this marker in the waterfall.");
        setHelpText(m_chkBpsk31Afc, "Enable narrow AFC around the selected PSK tone. Useful for small audio/radio drift; disable for exact loopback tests.");
        setHelpText(m_spinBpsk31AfcRangeHz, "Maximum AFC search window around the PSK marker. Start with ±20 Hz; use smaller values in crowded bands.");
        setHelpText(m_chkBpsk31Invert, "Invert decoded/transmitted bits for diagnostics. Normally leave OFF for standard PSK.");
        setHelpText(m_txtBpsk31Rx, "Decoded PSK text using the Varicode pipeline.");
        setHelpText(m_txtBpsk31Tx, "Text to transmit as BPSK/QPSK. The transmitter uses Varicode and shaped phase changes.");
        setHelpText(m_btnBpsk31ClearRx, "Clear the received PSK text window.");
        setHelpText(m_btnBpsk31LoadTxText, "Load plain text for PSK transmission.");
        setHelpText(m_btnBpsk31ClearTx, "Clear the PSK transmission text.");
    }

    if (m_spinMfskCenterHz != nullptr) {
        if (m_cmbMfskVariant != nullptr) {
            setHelpText(m_cmbMfskVariant, "Select standard MFSK16 Varicode/FEC or legacy experimental MFSK32.");
        }
        setHelpText(m_spinMfskCenterHz, "Center frequency of the MFSK tone set. Click the waterfall to retune it directly.");
        setHelpText(m_chkMfskAfc, "Enable slow AFC on the MFSK tone bank to follow small audio-frequency offsets.");
        setHelpText(m_spinMfskAfcRangeHz, "Maximum MFSK tone-bank AFC correction range around the selected center frequency.");
        setHelpText(m_txtMfskRx, "Received MFSK text. MFSK16 uses standard Varicode/FEC; MFSK32 remains experimental.");
        setHelpText(m_txtMfskTx, "Text to transmit. MFSK16 uses standard Varicode/FEC; MFSK32 uses the legacy experimental path.");
        setHelpText(m_btnMfskClearRx, "Clear the received MFSK text window.");
        setHelpText(m_btnMfskLoadTxText, "Load plain text for MFSK transmission.");
        setHelpText(m_btnMfskClearTx, "Clear the MFSK transmission text.");
    }

    if (m_spinCwToneHz != nullptr) {
        setHelpText(m_spinCwToneHz, "CW receive tone in Hz. Click the waterfall to retune this marker directly.");
        setHelpText(m_spinCwWpm, "Approximate Morse speed. Disable Auto WPM and set this manually when the estimator is hunting on noisy hand-sent CW.");
        setHelpText(m_chkCwAutoWpm, "Let the CW decoder estimate WPM from dot/dash timing. Turn it off if the estimate jumps too much and enter a manual speed.");
        setHelpText(m_spinCwToneHz, "Green RX A CW marker. Left click the waterfall to move it to the signal you want in the main RX text.");
        setHelpText(m_spinCwWpm, "Manual WPM hint for the skimmer timing classifier when Auto WPM is disabled.");
        setHelpText(m_chkCwAutoWpm, "Let the skimmer estimate WPM from the selected A/B streams.");
        setHelpText(m_txtCwRx, "Decoded Morse text for the selected CW signal. Click the waterfall to move the green RX marker to the tone you want to copy.");
        setHelpText(m_txtCwTx, "Type CW text to transmit. Unsupported characters are skipped or spaced by the CW transmitter.");
        setHelpText(m_btnCwSend, "Transmit the CW text, or stop the current CW transmission when already sending.");
        setHelpText(m_btnCwClearRx, "Clear the decoded CW text window and reset the CW timing state.");
    }

    if (m_spinHellToneHz != nullptr) {
        setHelpText(m_cmbHellVariant, "Select classic keyed-amplitude Feld Hell or two-tone FSK-105 Hellschreiber.");
        setHelpText(m_spinHellToneHz, "Hellschreiber center carrier tone in Hz. Click the waterfall to retune this session-only marker directly.");
        setHelpText(m_spinHellColumnRate, "Paper speed / Hellschreiber column rate. Use this if received letters look horizontally compressed or stretched. Standard Feld Hell and FSK-105 use 17.5 columns per second.");
        setHelpText(m_spinHellBandwidthHz, "RX tone detector bandwidth for the visual Hell paper. FSK-105 uses two tones around the center marker.");
        setHelpText(m_chkHellAfc, "Enable narrow AFC around the Hellschreiber center marker. The paper follows the strongest local keyed-tone energy peak.");
        setHelpText(m_spinHellAfcRangeHz, "Maximum AFC search window around the Hellschreiber marker. ±10 to ±20 Hz is a good starting point.");
        setHelpText(m_sliderHellPaperScale, "Zoom the Hellschreiber paper on both axes. Existing RX/TX pixels are preserved; only the display scale changes.");
        setHelpText(m_txtHellTx, "Text to transmit as Hellschreiber. Local TX is printed in red on the same paper tape.");
        setHelpText(m_btnHellResetImage, "Clear the Hellschreiber virtual receive paper.");
        setHelpText(m_btnHellLoadTxText, "Load plain text for Hellschreiber transmission.");
        setHelpText(m_btnHellClearTx, "Clear the Hellschreiber transmission text.");
    }

    promoteToolTipsToContextHelp(this);

}

void MainWindow::setupProcessingConnections()
{
    connect(m_audioEngine, &AudioEngine::audioBlockReady,
            this, [this](const AudioBlock &block) {
                if (!m_rxRunning || m_txRunning || m_offlineAnalysisActive || m_dspEngine == nullptr) {
                    return;
                }

                const AudioBlock waterfallBlock = conditionAudioForWaterfall(block);

                QMetaObject::invokeMethod(
                    m_dspEngine,
                    "processAudioBlock",
                    Qt::QueuedConnection,
                    Q_ARG(AudioBlock, waterfallBlock)
                    );
            },
            Qt::QueuedConnection);

    connect(m_audioEngine, &AudioEngine::audioBlockReady,
            this, &MainWindow::handleRxAudioBlock);

    connect(m_txAudioEngine, &TxAudioEngine::audioBlockReady,
            this, [this](const AudioBlock &block) {
                if (!m_txRunning) {
                    return;
                }

                /*
                 * Do not feed Hell TX monitor audio back through the live
                 * Hell RX decoder.  That created a heavy UI feedback path and
                 * made the local paper speed differ from the actual TX raster.
                 * Hell TX now paints the exact generated raster directly when
                 * transmission starts; the copied PCM below is only for the
                 * waterfall/spectrum path.
                 */

                if (m_dspEngine == nullptr) {
                    return;
                }

                QMetaObject::invokeMethod(
                    m_dspEngine,
                    "processAudioBlock",
                    Qt::QueuedConnection,
                    Q_ARG(AudioBlock, block)
                    );
            },
            Qt::QueuedConnection);

    connect(m_txAudioEngine, &TxAudioEngine::rttyToneStateChanged,
            this, [this](bool mark, double progress) {
                Q_UNUSED(mark)
                Q_UNUSED(progress)

                /*
                 * The RTTY tuning scope is an RX instrument.  Do not draw a
                 * synthetic TX crossed ellipse: it looks like a perfect signal
                 * even when it says nothing about reception or decoding.
                 */
                if (m_rttyScopeWidget != nullptr && m_txRunning) {
                    m_rttyScopeWidget->setTrace(QVector<QPointF>(), 0.0, false);
                }
            },
            Qt::QueuedConnection);

    connect(m_txAudioEngine, &TxAudioEngine::started,
            this, &MainWindow::handleTxStarted);

    connect(m_txAudioEngine, &TxAudioEngine::stopped,
            this, &MainWindow::handleTxStopped);

    connect(m_txAudioEngine, &TxAudioEngine::finished,
            this, &MainWindow::handleTxFinished);

    connect(m_txAudioEngine, &TxAudioEngine::errorOccurred,
            this, &MainWindow::handleTxError);

    connect(m_txAudioEngine, &TxAudioEngine::progressChanged,
            this, &MainWindow::handleTxProgress);

    if (m_ftTxWorker != nullptr) {
        connect(m_ftTxWorker, &FtTxWorker::audioBlockReady,
                this, [this](const AudioBlock &block) {
                    if (!m_txRunning || m_dspEngine == nullptr) {
                        return;
                    }
                    QMetaObject::invokeMethod(
                        m_dspEngine,
                        "processAudioBlock",
                        Qt::QueuedConnection,
                        Q_ARG(AudioBlock, block)
                        );
                },
                Qt::QueuedConnection);
        connect(m_ftTxWorker, &FtTxWorker::rttyToneStateChanged,
                this, [this](bool mark, double progress) {
                    Q_UNUSED(mark)
                    Q_UNUSED(progress)
                    if (m_rttyScopeWidget != nullptr && m_txRunning) {
                        m_rttyScopeWidget->setTrace(QVector<QPointF>(), 0.0, false);
                    }
                },
                Qt::QueuedConnection);
        connect(m_ftTxWorker, &FtTxWorker::started,
                this, [this]() {
                    m_ftTxWorkerRunning = true;
                    handleTxStarted();
                },
                Qt::QueuedConnection);
        connect(m_ftTxWorker, &FtTxWorker::stopped,
                this, [this]() {
                    m_ftTxWorkerRunning = false;
                    handleTxStopped();
                },
                Qt::QueuedConnection);
        connect(m_ftTxWorker, &FtTxWorker::finished,
                this, &MainWindow::handleTxFinished,
                Qt::QueuedConnection);
        connect(m_ftTxWorker, &FtTxWorker::errorOccurred,
                this, [this](const QString &message) {
                    m_ftTxWorkerRunning = false;
                    handleTxError(message);
                },
                Qt::QueuedConnection);
        connect(m_ftTxWorker, &FtTxWorker::progressChanged,
                this, &MainWindow::handleTxProgress,
                Qt::QueuedConnection);
        connect(m_ftTxWorker, &FtTxWorker::logMessage,
                this, [this](const QString &message) { appendLog(message); },
                Qt::QueuedConnection);
    }

    connect(m_dspEngine, &DspEngine::waterfallLineReady,
            m_waterfallWidget, &WaterfallWidget::addLine,
            Qt::QueuedConnection);

    connect(m_dspEngine, &DspEngine::dominantFrequencyChanged,
            this, &MainWindow::handleDominantFrequency,
            Qt::QueuedConnection);

    connect(m_waterfallWidget, &WaterfallWidget::frequencyClicked,
            this, &MainWindow::handleWaterfallFrequencyClicked);

    if (m_ntpClient != nullptr) {
        m_ntpClient->setEnabled(false);
    }

    if (m_rigController != nullptr) {
        connect(m_rigController, &HamlibController::frequencyChanged,
                this, &MainWindow::handleRigFrequencyChanged,
                Qt::QueuedConnection);
        connect(m_rigController, &HamlibController::statusChanged,
                this, [this](const QString &status) {
                    m_rigStatusMirror = status;
                    m_rigCatConnected = status.contains(QStringLiteral("connected"), Qt::CaseInsensitive);
                    handleRigStatusChanged(status);
                    appendLog(QStringLiteral("CAT: ") + status);
                },
                Qt::QueuedConnection);
        connect(m_rigController, &HamlibController::pttChanged,
                this, [this](bool enabled) {
                    m_lastRigPttOn = enabled;
                    updateRigControlStatusUi();
                    appendLog(enabled ? QStringLiteral("PTT: ON") : QStringLiteral("PTT: OFF"));
                },
                Qt::QueuedConnection);
        connect(m_rigController, &HamlibController::errorOccurred,
                this, [this](const QString &message) {
                    m_rigCatConnected = false;
                    appendLog(QStringLiteral("CAT ERROR: ") + message);
                    updateRigControlStatusUi();
                },
                Qt::QueuedConnection);
    }

    connect(m_weatherFaxDecoder, &WeatherFaxDecoder::imageUpdated,
            m_faxImageWidget, &FaxImageWidget::setImage);

    connect(m_weatherFaxDecoder, &WeatherFaxDecoder::statusChanged,
            this, &MainWindow::handleWeatherFaxStatus);

    connect(m_weatherFaxDecoder, &WeatherFaxDecoder::markersChanged,
            this, [this](const QVector<FrequencyMarker> &markers) {
                if (m_waterfallWidget != nullptr && ui != nullptr && ui->cmbMode != nullptr &&
                    ui->cmbMode->currentText() == WeatherFaxDecoder::modeName()) {
                    m_waterfallWidget->setMarkers(markers);
                }
            });

    connect(m_weatherFaxDecoder, &WeatherFaxDecoder::toneRangeUpdated,
            this, &MainWindow::handleWeatherFaxToneRangeUpdated);

    connect(m_weatherFaxDecoder, &WeatherFaxDecoder::imageCompleted,
            this, &MainWindow::handleWeatherFaxImageCompleted);

    connect(m_sstvDecoder, &SstvDecoder::imageUpdated,
            m_faxImageWidget, &FaxImageWidget::setImage);

    connect(m_sstvDecoder, &SstvDecoder::statusChanged,
            this, &MainWindow::handleWeatherFaxStatus);

    connect(m_sstvDecoder, &SstvDecoder::imageCompleted,
            this, &MainWindow::handleSstvImageCompleted);

    connect(m_rttyDecoder, &RttyDecoder::characterReceived,
            this, &MainWindow::handleRttyTextUpdated,
            Qt::QueuedConnection);
    // MIND is not part of the RTTY live chain.  RTTY remains a classical
    // matched-filter/Baudot decoder; no RTTY text or bit samples are sent to MIND.

    connect(m_rttyDecoder, &RttyDecoder::statusChanged,
            this, &MainWindow::handleWeatherFaxStatus);

    connect(m_rttyDecoder, &RttyDecoder::reversePolarityRequested,
            this, [this](bool reverse) {
                if (m_chkRttyAutoReverse == nullptr || !m_chkRttyAutoReverse->isChecked() ||
                    m_chkRttyReverse == nullptr || m_chkRttyReverse->isChecked() == reverse) {
                    return;
                }
                {
                    const QSignalBlocker blockReverse(m_chkRttyReverse);
                    m_chkRttyReverse->setChecked(reverse);
                }
                applyRttySettings();
                handleWeatherFaxStatus(reverse
                                           ? QStringLiteral("RTTY: auto polarity enabled Reverse polarity")
                                           : QStringLiteral("RTTY: auto polarity disabled Reverse polarity"));
            },
            Qt::QueuedConnection);

    connect(m_rttyDecoder, &RttyDecoder::tuningScopeChanged,
            this, [this](double markLevel, double spaceLevel, double snrLike, bool locked) {
                if (m_rttyScopeWidget == nullptr) {
                    return;
                }

                const bool rttyRxVisible =
                    m_rxRunning && ui->cmbMode->currentText() == RttyDecoder::modeName();

                if (!rttyRxVisible) {
                    m_rttyScopeWidget->setTrace(QVector<QPointF>(), 0.0, false);
                    return;
                }

                m_rttyScopeWidget->setTuningMetrics(markLevel, spaceLevel, snrLike, locked);
            });

    connect(m_rttyDecoder, &RttyDecoder::tuningScopeTraceChanged,
            this, [this](const QVector<QPointF> &tracePoints, double snrLike, bool locked) {
                if (m_rttyScopeWidget == nullptr) {
                    return;
                }

                const bool rttyRxVisible =
                    m_rxRunning && ui->cmbMode->currentText() == RttyDecoder::modeName();

                if (!rttyRxVisible) {
                    m_rttyScopeWidget->setTrace(QVector<QPointF>(), 0.0, false);
                    return;
                }

                m_rttyScopeWidget->setTrace(tracePoints, snrLike, locked);
            });

    connect(m_rttyDecoder, &RttyDecoder::markersChanged,
            this, [this](const QVector<FrequencyMarker> &markers) {
                if (m_waterfallWidget != nullptr && ui != nullptr && ui->cmbMode != nullptr &&
                    ui->cmbMode->currentText() == RttyDecoder::modeName()) {
                    m_waterfallWidget->setMarkers(markers);
                }
            });

    if (m_rttyMultiDecoder != nullptr) {
        connect(m_rttyMultiDecoder, &RttyMultiDecoder::calloutsChanged,
                this, [this](const QVector<RttyMultiDecoder::Callout> &callouts) {
                    m_rttyWaterfallCallouts = callouts;
                    if (m_waterfallWidget == nullptr || ui == nullptr || ui->cmbMode == nullptr ||
                        ui->cmbMode->currentText() != RttyDecoder::modeName()) {
                        return;
                    }
                    QVector<WaterfallTextOverlay> overlays;
                    for (const RttyMultiDecoder::Callout &callout : callouts) {
                        WaterfallTextOverlay overlay;
                        overlay.frequencyHz = static_cast<double>(callout.markHz);
                        overlay.label = callout.label;
                        overlay.textColor = callout.cq ? QColor(255, 255, 180) : QColor(180, 230, 255);
                        overlay.backgroundColor = callout.cq ? QColor(0, 95, 0, 210) : QColor(0, 0, 0, 190);
                        overlays.append(overlay);
                    }
                    m_waterfallWidget->setTextOverlays(overlays);
                });
        connect(m_rttyMultiDecoder, &RttyMultiDecoder::statusChanged,
                this, &MainWindow::handleWeatherFaxStatus);
    }

    connect(m_bpsk31Decoder, &Bpsk31Decoder::characterReceived,
            this, &MainWindow::handleBpsk31TextUpdated,
            Qt::QueuedConnection);

    connect(m_bpsk31Decoder, &Bpsk31Decoder::statusChanged,
            this, &MainWindow::handleWeatherFaxStatus);

    connect(m_bpsk31Decoder, &Bpsk31Decoder::markersChanged,
            this, [this](const QVector<FrequencyMarker> &markers) {
                if (m_waterfallWidget != nullptr && ui != nullptr && ui->cmbMode != nullptr &&
                    ui->cmbMode->currentText() == Bpsk31Decoder::modeName()) {
                    m_waterfallWidget->setMarkers(markers);
                }
            });

    connect(m_mfskDecoder, &MfskDecoder::characterReceived,
            this, &MainWindow::handleMfskTextUpdated,
            Qt::QueuedConnection);

    connect(m_mfskDecoder, &MfskDecoder::statusChanged,
            this, &MainWindow::handleWeatherFaxStatus);

    connect(m_mfskDecoder, &MfskDecoder::markersChanged,
            this, [this](const QVector<FrequencyMarker> &markers) {
                if (m_waterfallWidget != nullptr && ui != nullptr && ui->cmbMode != nullptr &&
                    ui->cmbMode->currentText() == MfskDecoder::modeName()) {
                    m_waterfallWidget->setMarkers(markers);
                }
            });

    connect(m_cwDecoder, &CwDecoder::characterReceived,
            this, &MainWindow::handleCwTextUpdated,
            Qt::QueuedConnection);
    // CW RX is produced by the assimilated multi-channel skimmer.  The operator
    // still controls RX A/RX B with the green/blue waterfall markers; only those
    // two selected streams are promoted to the main RX terminal.
    connect(m_cwDecoder, &CwDecoder::priorityTextReceived,
            this, [this](int rank, const QString &text) {
                if (rank <= 0) {
                    appendCwDecoderText(QStringLiteral("A"), text, QColor("#118a2a"), &m_cwPrimaryLineOpen);
                } else if (rank == 1) {
                    appendCwDecoderText(QStringLiteral("B"), text, QColor("#0069d9"), &m_cwSecondaryLineOpen);
                }
            },
            Qt::QueuedConnection);

    connect(m_cwDecoder, &CwDecoder::skimmerOverlaysChanged,
            this, [this](const QStringList &labels, const QVector<double> &frequenciesHz, const QVector<float> &confidences) {
                if (m_waterfallWidget == nullptr || ui == nullptr || ui->cmbMode == nullptr ||
                    ui->cmbMode->currentText() != CwDecoder::modeName()) {
                    return;
                }
                QVector<WaterfallTextOverlay> overlays;
                const int count = qMin(labels.size(), qMin(frequenciesHz.size(), confidences.size()));
                overlays.reserve(count);
                static const QRegularExpression priorityPrefixRe(QStringLiteral("^([AB])\\s+\\d+Hz\\s+"));
                static const QRegularExpression frequencyPrefixRe(QStringLiteral("^\\d+Hz\\s+"));
                for (int i = 0; i < count; ++i) {
                    WaterfallTextOverlay overlay;
                    overlay.frequencyHz = frequenciesHz.at(i);

                    QString label = labels.at(i).simplified();
                    const QRegularExpressionMatch priorityMatch = priorityPrefixRe.match(label);
                    const bool priorityA = priorityMatch.hasMatch() && priorityMatch.captured(1) == QStringLiteral("A");
                    const bool priorityB = priorityMatch.hasMatch() && priorityMatch.captured(1) == QStringLiteral("B");
                    if (priorityMatch.hasMatch()) {
                        label.remove(priorityPrefixRe);
                    } else {
                        label.remove(frequencyPrefixRe);
                    }
                    label = label.simplified();
                    if (label.isEmpty()) {
                        continue;
                    }

                    overlay.label = label;
                    overlay.streamId = priorityA ? QStringLiteral("CW_A")
                                     : priorityB ? QStringLiteral("CW_B")
                                                 : QStringLiteral("CW_%1").arg(qRound(overlay.frequencyHz));
                    overlay.verticalTrail = true;
                    overlay.textColor = priorityA ? QColor(230, 255, 230)
                                      : priorityB ? QColor(220, 238, 255)
                                                  : QColor(255, 244, 170);
                    overlay.backgroundColor = priorityA ? QColor(0, 72, 28, 232)
                                             : priorityB ? QColor(0, 38, 95, 232)
                                                         : QColor(20, 14, 0, 220);
                    overlays.append(overlay);
                }
                m_waterfallWidget->setTextOverlays(overlays);
            });

    connect(m_cwDecoder, &CwDecoder::statusChanged,
            this, &MainWindow::handleWeatherFaxStatus);

    connect(m_cwDecoder, &CwDecoder::markersChanged,
            this, [this](const QVector<FrequencyMarker> &) {
                if (m_waterfallWidget != nullptr && ui != nullptr && ui->cmbMode != nullptr &&
                    ui->cmbMode->currentText() == CwDecoder::modeName()) {
                    updateWaterfallMarkers();
                    updateCwDualRxStatusLabel();
                }
            });

    connect(m_cwDecoder, &CwDecoder::speedEstimateChanged,
            this, [this](double wpm) {
                m_cwTrackedWpmA = wpm;
                if (m_lblCwTrackedWpm != nullptr) {
                    const QString text = m_cwSecondaryEnabled
                                             ? uiText("cw_tracked_ab", "Tracked A: %1 WPM · B: %2 WPM")
                                                   .arg(m_cwTrackedWpmA, 0, 'f', 1)
                                                   .arg(m_cwTrackedWpmB > 0.1 ? QString::number(m_cwTrackedWpmB, 'f', 1) : QStringLiteral("--"))
                                             : QString("%1 %2 WPM").arg(uiText("tracked", "Tracked:")).arg(wpm, 0, 'f', 1);
                    m_lblCwTrackedWpm->setText(text);
                }
                if (m_chkCwAutoWpm != nullptr && m_chkCwAutoWpm->isChecked() && m_spinCwWpm != nullptr) {
                    const int rounded = qBound(5, static_cast<int>(std::lround(wpm)), 60);
                    const QSignalBlocker block(m_spinCwWpm);
                    m_spinCwWpm->setValue(rounded);
                    m_settings.cwWpm = rounded;
                }
                updateCwDualRxStatusLabel();
                updateWaterfallMarkers();
            });

    // RX B is no longer a second decoder object. It is a user-selected marker
    // inside the single skimmer engine, so there are no secondary decoder
    // signal connections here.

    connect(m_hellDecoder, &HellschreiberDecoder::imageUpdated,
            this, [this](const QImage &image) {
                if (m_lblHellRaster == nullptr) {
                    return;
                }

                updateHellRasterDisplay(image);
            });

    connect(m_hellDecoder, &HellschreiberDecoder::statusChanged,
            this, &MainWindow::handleWeatherFaxStatus);

    connect(m_hellDecoder, &HellschreiberDecoder::markersChanged,
            this, [this](const QVector<FrequencyMarker> &markers) {
                if (m_waterfallWidget != nullptr && ui != nullptr && ui->cmbMode != nullptr &&
                    ui->cmbMode->currentText() == HellschreiberDecoder::modeName()) {
                    m_waterfallWidget->setMarkers(markers);
                }
            });

    connect(m_ft8RxDecoder, &Ft8RxDecoder::decodeReady,
            this, &MainWindow::handleFt8DecodeReady,
            Qt::QueuedConnection);
    if (m_ddspController != nullptr) {
        connect(m_ft8RxDecoder, &Ft8RxDecoder::nativeTrainingSampleReady,
                this, [this](const QString &mode, const QVector<float> &candidateMagnitudes,
                             const QVector<float> &targetBits, const QString &message) {
                    m_ddspController->submitNativeFtSample(mode, candidateMagnitudes, targetBits, message);
                },
                Qt::QueuedConnection);
    }

    connect(m_ft8RxDecoder, &Ft8RxDecoder::statusChanged,
            this, &MainWindow::handleWeatherFaxStatus,
            Qt::QueuedConnection);

    connect(m_ft8RxDecoder, &Ft8RxDecoder::performanceUpdated,
            this, &MainWindow::handleFt8DecodePerformance,
            Qt::QueuedConnection);

    connect(m_ft8RxDecoder, &Ft8RxDecoder::offlineAnalysisFinished,
            this, &MainWindow::handleFtOfflineAnalysisFinished,
            Qt::QueuedConnection);

    if (m_ntpClient != nullptr) {
        connect(m_ntpClient, &NtpClient::syncStatusChanged,
                this, &MainWindow::handleFt8NtpStatus,
                Qt::QueuedConnection);
        connect(m_ntpClient, &NtpClient::offsetUpdated,
                this, &MainWindow::handleFt8NtpOffset,
                Qt::QueuedConnection);
    }

    connect(m_faxImageWidget, &FaxImageWidget::zoomChanged,
            this, &MainWindow::handleFaxImageZoomChanged);
}


void MainWindow::setupLanguageMenu()
{
    if (ui->menubar == nullptr || m_menuLanguage != nullptr) {
        return;
    }

    m_menuLanguage = new QMenu("Language", this);
    hardenPopupMenuForFullscreen(m_menuLanguage);
    m_languageActionGroup = new QActionGroup(this);
    m_languageActionGroup->setExclusive(true);

    struct LanguageEntry { QString code; QString label; QString iconPath; };
    const QList<LanguageEntry> languages = {
        {QStringLiteral("en"), QStringLiteral("English"), QStringLiteral(":/icons/flag_en.png")},
        {QStringLiteral("it"), QStringLiteral("Italiano"), QStringLiteral(":/icons/flag_it.png")},
        {QStringLiteral("fr"), QStringLiteral("Français"), QStringLiteral(":/icons/flag_fr.png")},
        {QStringLiteral("de"), QStringLiteral("Deutsch"), QStringLiteral(":/icons/flag_de.png")},
        {QStringLiteral("no"), QStringLiteral("Norsk"), QStringLiteral(":/icons/flag_no.png")},
        {QStringLiteral("cs"), QStringLiteral("Cecoslovacco"), QStringLiteral(":/icons/flag_cs.png")}
    };

    for (const LanguageEntry &entry : languages) {
        QAction *action = new QAction(QIcon(entry.iconPath), entry.label, this);
        action->setCheckable(true);
        action->setData(entry.code);
        m_languageActionGroup->addAction(action);
        m_menuLanguage->addAction(action);
    }

    connect(m_languageActionGroup, &QActionGroup::triggered,
            this, &MainWindow::setUiLanguageFromAction);

    ui->menubar->insertMenu(ui->menuHelp->menuAction(), m_menuLanguage);
}



void MainWindow::loadUiLanguageSetting()
{
    QSettings settings(AppSettings::settingsFilePath(), QSettings::IniFormat);
    m_uiLanguageCode = settings.value(QStringLiteral("UI/language"), QStringLiteral("en")).toString();
    if (m_uiLanguageCode.trimmed().isEmpty()) {
        m_uiLanguageCode = QStringLiteral("en");
    }
}

void MainWindow::saveUiLanguageSetting() const
{
    QSettings settings(AppSettings::settingsFilePath(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("UI/language"), m_uiLanguageCode);
}

void MainWindow::loadUiTranslationFile(const QString &languageCode)
{
    MadModemI18n::setLanguageCode(languageCode);
    m_uiTranslations.clear();

    const QString resourcePath = QStringLiteral(":/translations/ui_%1.ini").arg(languageCode);
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    QTextStream stream(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    stream.setCodec("UTF-8");
#endif
    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#') || line.startsWith('[')) {
            continue;
        }
        const int eq = line.indexOf('=');
        if (eq <= 0) {
            continue;
        }
        const QString key = line.left(eq).trimmed();
        QString value = line.mid(eq + 1).trimmed();
        value.replace(QStringLiteral("\\n"), QStringLiteral("\n"));
        m_uiTranslations.insert(key, value);
    }
}

QString MainWindow::uiText(const QString &key, const QString &fallback) const
{
    return m_uiTranslations.value(key, fallback);
}

QString MainWindow::uiTextFromSource(const QString &prefix, const QString &source) const
{
    QString normalized;
    normalized.reserve(source.size());

    for (const QChar ch : source) {
        if (ch.isLetterOrNumber()) {
            normalized.append(ch.toLower());
        } else {
            if (!normalized.endsWith(QLatin1Char('_'))) {
                normalized.append(QLatin1Char('_'));
            }
        }
    }

    while (normalized.startsWith(QLatin1Char('_'))) normalized.remove(0, 1);
    while (normalized.endsWith(QLatin1Char('_'))) normalized.chop(1);
    if (normalized.isEmpty()) normalized = QStringLiteral("empty");
    if (normalized.size() > 90) normalized = normalized.left(90);

    // Some widgets are created from explicit uiText("key", "fallback") calls and
    // are then visited by the generic object-tree translator.  Older generated
    // dictionaries contained entries such as text.reset_view=reset_view, which
    // made visible labels regress to marker names.  Resolve direct keys first and
    // refuse marker-like generated values.
    const QString trimmed = source.trimmed();
    if (m_uiTranslations.contains(trimmed)) {
        return m_uiTranslations.value(trimmed);
    }
    if (m_uiTranslations.contains(normalized)) {
        return m_uiTranslations.value(normalized);
    }

    const QString generatedKey = prefix + QStringLiteral(".") + normalized;
    const QString translated = m_uiTranslations.value(generatedKey, QString());
    if (!translated.isEmpty()) {
        const QString t = translated.trimmed();
        const bool looksLikeMarker =
            (t == normalized) ||
            (t == trimmed && t.contains(QLatin1Char('_'))) ||
            (t.contains(QLatin1Char('_')) && !t.contains(QLatin1Char(' ')) && t.toLower() == t);
        const bool looksLikeSourceFile =
            t.endsWith(QStringLiteral(".h"), Qt::CaseInsensitive) ||
            t.endsWith(QStringLiteral(".cpp"), Qt::CaseInsensitive) ||
            t.endsWith(QStringLiteral(".moc"), Qt::CaseInsensitive) ||
            t.contains(QStringLiteral("../")) ||
            t.contains(QStringLiteral("/"));
        if (!looksLikeMarker && !looksLikeSourceFile) {
            return translated;
        }
    }

    return source;
}

QString MainWindow::localizedRuntimeText(const QString &prefix, const QString &message) const
{
    const QString source = message.trimmed();
    if (source.isEmpty()) {
        return message;
    }

    // 1) Exact message match. This covers static appendLog()/QMessageBox text
    //    without forcing every call site to be rewritten immediately.
    QString translated = uiTextFromSource(prefix, source);
    if (translated != source) {
        return translated;
    }

    // 2) Common runtime log pattern: "Caption: value".  Translate only the
    //    caption and preserve the dynamic value verbatim (device names, paths,
    //    callsigns, frequencies, error strings).
    const int colon = source.indexOf(QLatin1Char(':'));
    if (colon > 0 && colon <= 64) {
        const QString caption = source.left(colon).trimmed();
        const QString translatedCaption = uiTextFromSource(QStringLiteral("text"), caption);
        if (translatedCaption != caption) {
            QString out = message;
            const int start = message.indexOf(caption);
            if (start >= 0) {
                out.replace(start, caption.size(), translatedCaption);
                return out;
            }
        }
    }

    // 3) Common runtime log pattern: "Prefix — details".  Translate only the
    //    stable prefix and keep the details/path intact.
    const int dash = source.indexOf(QStringLiteral(" — "));
    if (dash > 0 && dash <= 96) {
        const QString caption = source.left(dash).trimmed();
        const QString translatedCaption = uiTextFromSource(prefix, caption);
        if (translatedCaption != caption) {
            QString out = message;
            const int start = message.indexOf(caption);
            if (start >= 0) {
                out.replace(start, caption.size(), translatedCaption);
                return out;
            }
        }
    }

    return message;
}

void MainWindow::translateObjectTree(QObject *object)
{
    if (object == nullptr) {
        return;
    }

    if (QAction *action = qobject_cast<QAction *>(object)) {
        const QString source = action->property("i18nSourceText").toString().isEmpty()
                                   ? action->text()
                                   : action->property("i18nSourceText").toString();
        if (!source.trimmed().isEmpty()) {
            action->setProperty("i18nSourceText", source);
            action->setText(uiTextFromSource("text", source));
        }

        const QString toolSource = action->property("i18nSourceToolTip").toString().isEmpty()
                                      ? action->toolTip()
                                      : action->property("i18nSourceToolTip").toString();
        if (!toolSource.trimmed().isEmpty()) {
            action->setProperty("i18nSourceToolTip", toolSource);
            action->setToolTip(uiTextFromSource("text", toolSource));
        }

        const QString statusSource = action->property("i18nSourceStatusTip").toString().isEmpty()
                                        ? action->statusTip()
                                        : action->property("i18nSourceStatusTip").toString();
        if (!statusSource.trimmed().isEmpty()) {
            action->setProperty("i18nSourceStatusTip", statusSource);
            action->setStatusTip(uiTextFromSource("text", statusSource));
        }
    }

    if (QWidget *widget = qobject_cast<QWidget *>(object)) {
        // The main application title is the product/version identifier, not UI copy.
        // It must never be harvested or translated; otherwise language switching can
        // corrupt or duplicate it in localized builds.  Dialog window titles are
        // still translated below.
        if (widget != this) {
            const QString source = widget->property("i18nSourceWindowTitle").toString().isEmpty()
                                       ? widget->windowTitle()
                                       : widget->property("i18nSourceWindowTitle").toString();
            if (!source.trimmed().isEmpty()) {
                widget->setProperty("i18nSourceWindowTitle", source);
                widget->setWindowTitle(uiTextFromSource("text", source));
            }
        }

        const QString toolSource = widget->property("i18nSourceToolTip").toString().isEmpty()
                                      ? widget->toolTip()
                                      : widget->property("i18nSourceToolTip").toString();
        if (!toolSource.trimmed().isEmpty()) {
            widget->setProperty("i18nSourceToolTip", toolSource);
            widget->setToolTip(uiTextFromSource("text", toolSource));
        }

        const QString statusSource = widget->property("i18nSourceStatusTip").toString().isEmpty()
                                        ? widget->statusTip()
                                        : widget->property("i18nSourceStatusTip").toString();
        if (!statusSource.trimmed().isEmpty()) {
            widget->setProperty("i18nSourceStatusTip", statusSource);
            widget->setStatusTip(uiTextFromSource("text", statusSource));
        }
    }

    if (QMenu *menu = qobject_cast<QMenu *>(object)) {
        const QString source = menu->property("i18nSourceTitle").toString().isEmpty()
                                   ? menu->title()
                                   : menu->property("i18nSourceTitle").toString();
        if (!source.trimmed().isEmpty()) {
            menu->setProperty("i18nSourceTitle", source);
            menu->setTitle(uiTextFromSource("text", source));
        }
    } else if (QGroupBox *group = qobject_cast<QGroupBox *>(object)) {
        const QString source = group->property("i18nSourceTitle").toString().isEmpty()
                                   ? group->title()
                                   : group->property("i18nSourceTitle").toString();
        if (!source.trimmed().isEmpty()) {
            group->setProperty("i18nSourceTitle", source);
            group->setTitle(uiTextFromSource("text", source));
        }
    } else if (QLabel *label = qobject_cast<QLabel *>(object)) {
        const QString source = label->property("i18nSourceText").toString().isEmpty()
                                   ? label->text()
                                   : label->property("i18nSourceText").toString();
        if (!source.trimmed().isEmpty() && !source.startsWith("<")) {
            label->setProperty("i18nSourceText", source);
            label->setText(uiTextFromSource("text", source));
        }
    } else if (QAbstractButton *button = qobject_cast<QAbstractButton *>(object)) {
        const QString source = button->property("i18nSourceText").toString().isEmpty()
                                   ? button->text()
                                   : button->property("i18nSourceText").toString();
        if (!source.trimmed().isEmpty()) {
            button->setProperty("i18nSourceText", source);
            button->setText(uiTextFromSource("text", source));
        }
    } else if (QLineEdit *edit = qobject_cast<QLineEdit *>(object)) {
        const QString source = edit->property("i18nSourcePlaceholder").toString().isEmpty()
                                   ? edit->placeholderText()
                                   : edit->property("i18nSourcePlaceholder").toString();
        if (!source.trimmed().isEmpty()) {
            edit->setProperty("i18nSourcePlaceholder", source);
            edit->setPlaceholderText(uiTextFromSource("placeholder", source));
        }
    } else if (QPlainTextEdit *edit = qobject_cast<QPlainTextEdit *>(object)) {
        const QString source = edit->property("i18nSourcePlaceholder").toString().isEmpty()
                                   ? edit->placeholderText()
                                   : edit->property("i18nSourcePlaceholder").toString();
        if (!source.trimmed().isEmpty()) {
            edit->setProperty("i18nSourcePlaceholder", source);
            edit->setPlaceholderText(uiTextFromSource("placeholder", source));
        }
    }

    if (QTabWidget *tabs = qobject_cast<QTabWidget *>(object)) {
        for (int i = 0; i < tabs->count(); ++i) {
            const QString propertyName = QStringLiteral("i18nTab%1").arg(i);
            const QString source = tabs->property(propertyName.toUtf8().constData()).toString().isEmpty()
                                       ? tabs->tabText(i)
                                       : tabs->property(propertyName.toUtf8().constData()).toString();
            if (!source.trimmed().isEmpty()) {
                tabs->setProperty(propertyName.toUtf8().constData(), source);
                tabs->setTabText(i, uiTextFromSource("text", source));
            }
        }
    }

    if (QComboBox *combo = qobject_cast<QComboBox *>(object)) {
        if (combo->property("i18nSkipComboItems").toBool()) {
            return;
        }
        constexpr int SourceTextRole = Qt::UserRole + 913;
        for (int i = 0; i < combo->count(); ++i) {
            const QString source = combo->itemData(i, SourceTextRole).toString().isEmpty()
                                       ? combo->itemText(i)
                                       : combo->itemData(i, SourceTextRole).toString();
            if (!source.trimmed().isEmpty()) {
                combo->setItemData(i, source, SourceTextRole);
                combo->setItemText(i, uiTextFromSource("text", source));
            }
        }
    }

    if (QTableWidget *table = qobject_cast<QTableWidget *>(object)) {
        constexpr int SourceTextRole = Qt::UserRole + 914;
        for (int c = 0; c < table->columnCount(); ++c) {
            if (QTableWidgetItem *item = table->horizontalHeaderItem(c)) {
                const QString source = item->data(SourceTextRole).toString().isEmpty()
                                           ? item->text()
                                           : item->data(SourceTextRole).toString();
                if (!source.trimmed().isEmpty()) {
                    item->setData(SourceTextRole, source);
                    item->setText(uiTextFromSource("text", source));
                }
            }
        }
        for (int r = 0; r < table->rowCount(); ++r) {
            if (QTableWidgetItem *item = table->verticalHeaderItem(r)) {
                const QString source = item->data(SourceTextRole).toString().isEmpty()
                                           ? item->text()
                                           : item->data(SourceTextRole).toString();
                if (!source.trimmed().isEmpty()) {
                    item->setData(SourceTextRole, source);
                    item->setText(uiTextFromSource("text", source));
                }
            }
        }
    }

    const QList<QObject *> children = object->children();
    for (QObject *child : children) {
        translateObjectTree(child);
    }
}

void MainWindow::applyUiLanguageToObjectTree(QObject *root)
{
    translateObjectTree(root);
}

void MainWindow::applyUiLanguage()
{
    if (m_languageActionGroup != nullptr) {
        for (QAction *action : m_languageActionGroup->actions()) {
            action->setChecked(action->data().toString() == m_uiLanguageCode);
        }
    }

    // First update the generic object tree using the canonical source text
    // captured on the first pass.  Explicitly managed menus/actions/tabs are
    // updated below afterwards.  In v1.69 this call ran at the end: when the
    // user changed language, the generic translator could capture/restore the
    // previous translated caption as if it were the source text, so menu and tab
    // labels appeared to change only after restarting the application.
    applyUiLanguageToObjectTree(this);
    setWindowTitle(QStringLiteral(MADMODEM_VERSION_DISPLAY));

    if (ui->menuFile != nullptr) ui->menuFile->setTitle(uiText("menu.file", "File"));
    if (ui->menuSettings != nullptr) ui->menuSettings->setTitle(uiText("menu.settings", "Settings"));
    if (ui->menuHelp != nullptr) ui->menuHelp->setTitle(uiText("menu.help", "Help"));
    if (m_menuLanguage != nullptr) m_menuLanguage->setTitle(uiText("menu.language", "Language"));
    // Startup motto removed in v2.42: keep the menu bar clean.

    if (ui->actionExit != nullptr) {
        ui->actionExit->setText(uiText("action.exit", "Exit"));
        ui->actionExit->setToolTip(uiText("action.exit.safe", "Safely close MadModem."));
        ui->actionExit->setStatusTip(ui->actionExit->toolTip());
        ui->actionExit->setShortcut(QKeySequence(QStringLiteral("Ctrl+Q")));
    }
    if (m_actionAppSettings != nullptr) {
        // 0.5.63: Settings is a direct menubar action, not a drop-down menu.
        m_actionAppSettings->setText(uiText("menu.settings", "Settings"));
        m_actionAppSettings->setToolTip(uiText("action.appSettings", "Settings..."));
        m_actionAppSettings->setStatusTip(m_actionAppSettings->toolTip());
    }
    if (ui->actionOpenWeatherFaxWav != nullptr) ui->actionOpenWeatherFaxWav->setText(uiText("action.openWefaxWav", "Analyze WEFAX WAV..."));
    if (ui->actionAboutMadModem != nullptr) ui->actionAboutMadModem->setText(uiText("action.about", "About MM"));
    if (m_actionHelpContents != nullptr) {
        const QString helpTip = uiText("help.contents.tooltip", "Open the built-in MM help browser.");
        m_actionHelpContents->setText(uiText("action.helpContents", "Help contents..."));
        m_actionHelpContents->setStatusTip(helpTip);
        m_actionHelpContents->setToolTip(helpTip);
        m_actionHelpContents->setWhatsThis(helpTip);
    }
    if (m_actionWhatsThisMode != nullptr) {
        const QString whatsTip = uiText("help.whatsThis.tooltip", "Enter context-help mode. Click a control to see its explanation.");
        m_actionWhatsThisMode->setText(uiText("action.whatsThis", "What's This?"));
        m_actionWhatsThisMode->setStatusTip(whatsTip);
        m_actionWhatsThisMode->setToolTip(whatsTip);
        m_actionWhatsThisMode->setWhatsThis(whatsTip);
    }
    if (m_actionLogbook != nullptr) m_actionLogbook->setText(uiText("action.logbook", "+ Logbook..."));
    if (m_actionSstvEditor != nullptr) m_actionSstvEditor->setText(uiText("action.sstvEditor", "SSTV image editor..."));

    if (m_btnSstvEditor != nullptr) m_btnSstvEditor->setText(uiText("button.openSstvEditor", "Open SSTV editor..."));
    if (m_btnFaxForceRx != nullptr) m_btnFaxForceRx->setText(uiText("button.forceWefaxRx", "Force WEFAX RX now"));
    if (m_btnSstvForceRx != nullptr) m_btnSstvForceRx->setText(uiText("button.forceSstvRx", "Force SSTV RX now"));
    if (m_btnStartImageTx != nullptr && ui->cmbMode != nullptr && ui->cmbMode->currentText() == SstvDecoder::modeName()) {
        m_btnStartImageTx->setText(uiText("button.sendSstv", "Send SSTV"));
    }
    if (m_btnLoadTxImage != nullptr) m_btnLoadTxImage->setText(uiText("button.loadImage", "Load image..."));
    if (m_btnStopImageTx != nullptr) m_btnStopImageTx->setText(uiText("button.stopTx", "Stop TX"));

    if (m_lblSstvTxCall != nullptr) m_lblSstvTxCall->setText(uiText("label.call", "Call"));
    if (m_lblSstvTxName != nullptr) m_lblSstvTxName->setText(uiText("label.name", "Name"));
    if (m_lblSstvTxQth != nullptr) m_lblSstvTxQth->setText(uiText("label.qth", "QTH"));
    if (m_lblSstvTxInfo != nullptr) m_lblSstvTxInfo->setText(uiText("label.info", "Info"));
    if (m_grpWaterfallDisplay != nullptr) {
        m_grpWaterfallDisplay->setTitle(QString());
    }
    if (ui->grpStatusMeters != nullptr) {
        ui->grpStatusMeters->setTitle(QString());
    }
    if (m_lblWaterfallScale != nullptr) {
        m_lblWaterfallScale->setText(uiText("waterfall_color_scale", "Colour scale") +
                                     QStringLiteral(": %1%").arg(qBound(0, m_settings.waterfallColorScalePercent, 100)));
    }
    if (m_sliderWaterfallScale != nullptr) {
        m_sliderWaterfallScale->setToolTip(uiText("waterfall_scale_tooltip", "Display contrast/gamma for the waterfall. Lower values push the noise floor down while strong signals still reach the hot colours."));
    }
    if (m_cmbWaterfallPalette != nullptr) {
        const QSignalBlocker blocker(m_cmbWaterfallPalette);
        if (m_cmbWaterfallPalette->count() >= 6) {
            m_cmbWaterfallPalette->setItemText(0, uiText("waterfall_palette_madmodem", "MadModem / WSJT-X default"));
            m_cmbWaterfallPalette->setItemText(1, uiText("waterfall_palette_wsjtx", "WSJT-X default"));
            m_cmbWaterfallPalette->setItemText(2, uiText("waterfall_palette_mshv", "MSHV contrast"));
            m_cmbWaterfallPalette->setItemText(3, uiText("waterfall_palette_fldigi", "fldigi colours"));
            m_cmbWaterfallPalette->setItemText(4, uiText("waterfall_palette_raptor", "Raptor green"));
            m_cmbWaterfallPalette->setItemText(5, uiText("waterfall_palette_grayscale", "Grayscale"));
        }
        m_cmbWaterfallPalette->setToolTip(uiText("waterfall_palette_tooltip", "Selects the waterfall colour palette. The default palette follows WSJT-X-style weak-signal contrast."));
    }
    if (ui->sideTabWidget != nullptr) {
        if (ui->tabStatus != nullptr && ui->sideTabWidget->tabBar() != nullptr) {
            const int i = ui->sideTabWidget->indexOf(ui->tabStatus);
            if (i >= 0) ui->sideTabWidget->tabBar()->setTabVisible(i, false);
        }
        if (ui->tabModeSettings != nullptr) {
            const int i = ui->sideTabWidget->indexOf(ui->tabModeSettings);
            if (i >= 0) ui->sideTabWidget->setTabText(i, uiText("tab_mode", "Mode"));
        }
        if (m_tabDspSettings != nullptr) {
            const int i = ui->sideTabWidget->indexOf(m_tabDspSettings);
            if (i >= 0) ui->sideTabWidget->setTabText(i, uiText("tab_dsp", "DSP"));
        }
        if (m_tabCatRotator != nullptr) {
            const int i = ui->sideTabWidget->indexOf(m_tabCatRotator);
            if (i >= 0) ui->sideTabWidget->setTabText(i, uiText("tab_rotator", "Rotator"));
        }
        if (m_ddspPanelWidget != nullptr) {
            const int i = ui->sideTabWidget->indexOf(m_ddspPanelWidget);
            if (i >= 0) ui->sideTabWidget->setTabText(i, uiText("tab_ddsp", "MIND"));
        }
    }
    updateMindUiForMode(ui != nullptr && ui->cmbMode != nullptr ? ui->cmbMode->currentText() : QString());
    updateBandSchedulerTabForMode(ui != nullptr && ui->cmbMode != nullptr ? ui->cmbMode->currentText() : QString());
    updateDspTabForMode(ui != nullptr && ui->cmbMode != nullptr ? ui->cmbMode->currentText() : QString());
    updateFt8DecodePerformanceUi();

    updateRigControlStatusUi();
    updateMainStateButton();
    updateTxControlState();
}

void MainWindow::setUiLanguageFromAction(QAction *action)
{
    if (action == nullptr) {
        return;
    }
    const QString code = action->data().toString();
    if (code.isEmpty()) {
        return;
    }
    m_uiLanguageCode = code;
    loadUiTranslationFile(m_uiLanguageCode);
    saveUiLanguageSetting();
    applyUiLanguage();
    appendLog(uiText("log.languageChanged", "Language changed.") + " " + action->text());
}

void MainWindow::setupUiConnections()
{
    m_actionSstvEditor = new QAction("SSTV image editor...", this);
    m_actionLogbook = new QAction("+ Logbook...", this);
    m_actionAppSettings = new QAction("Settings...", this);
    m_actionHelpContents = new QAction("Help contents...", this);
    m_actionHelpContents->setShortcut(QKeySequence(QKeySequence::HelpContents));
    m_actionHelpContents->setStatusTip("Open MM inline help.");
    m_actionHelpContents->setToolTip("Open MM inline help.");
    m_actionWhatsThisMode = new QAction("What's This?", this);
    m_actionWhatsThisMode->setShortcut(QKeySequence(QStringLiteral("Shift+F1")));
    m_actionWhatsThisMode->setStatusTip("Click a control to read its context help.");
    m_actionWhatsThisMode->setToolTip("Click a control to read its context help.");
    if (ui->menuHelp != nullptr && ui->actionAboutMadModem != nullptr) {
        ui->menuHelp->insertAction(ui->actionAboutMadModem, m_actionHelpContents);
        ui->menuHelp->insertAction(ui->actionAboutMadModem, m_actionWhatsThisMode);
        ui->menuHelp->insertSeparator(ui->actionAboutMadModem);
    }
    if (ui->menuFile != nullptr) {
        ui->menuFile->insertAction(ui->actionExit, m_actionSstvEditor);
        ui->menuFile->insertAction(ui->actionExit, m_actionLogbook);
        ui->menuFile->insertSeparator(ui->actionExit);
    }
    connect(m_actionSstvEditor, &QAction::triggered,
            this, &MainWindow::openSstvImageEditor);
    connect(m_actionLogbook, &QAction::triggered,
            this, &MainWindow::showLogbookDialog);

    if (ui->menuSettings != nullptr && ui->menubar != nullptr) {
        // 0.5.63: the Settings menubar item opens the unified settings dialog
        // directly.  The old Settings drop-down only contained Settings + FT
        // developer WAV tools, which made the menu feel like a junk drawer.
        // FT WAV analysis / auto-test remain available from the dedicated
        // Settings dialog pages and developer controls, but not as entries in
        // the top-level Settings menubar item.
        QAction *oldMenuAction = ui->menuSettings->menuAction();
        QAction *insertBefore = nullptr;
        const QList<QAction *> menuActions = ui->menubar->actions();
        const int oldIndex = menuActions.indexOf(oldMenuAction);
        if (oldIndex >= 0 && oldIndex + 1 < menuActions.size()) {
            insertBefore = menuActions.at(oldIndex + 1);
        }
        ui->menuSettings->clear();
        ui->menubar->removeAction(oldMenuAction);
        ui->menubar->insertAction(insertBefore, m_actionAppSettings);
        m_actionAppSettings->setText(uiText("menu.settings", "Settings"));
        m_actionAppSettings->setToolTip(uiText("action.appSettings", "Settings..."));
        m_actionAppSettings->setStatusTip(m_actionAppSettings->toolTip());
    }


    // v2.42: rotating startup mottos removed; menu bar corner remains empty.

    connect(m_actionAppSettings, &QAction::triggered,
            this, &MainWindow::showAppSettingsDialog);
    connect(ui->actionExit, &QAction::triggered,
            this, &MainWindow::close);

    connect(ui->actionOpenWeatherFaxWav, &QAction::triggered,
            this, &MainWindow::openWeatherFaxWavFile);

    connect(ui->actionAboutMadModem, &QAction::triggered,
            this, &MainWindow::showAboutMadModem);

    if (m_actionHelpContents != nullptr) {
        connect(m_actionHelpContents, &QAction::triggered,
                this, &MainWindow::showOnlineHelp);
    }
    if (m_actionWhatsThisMode != nullptr) {
        connect(m_actionWhatsThisMode, &QAction::triggered,
                this, &MainWindow::enterWhatsThisMode);
    }

    if (m_sliderWaterfallScale != nullptr) {
        connect(m_sliderWaterfallScale, &QSlider::valueChanged, this, [this](int value) {
            const int clamped = qBound(0, value, 100);
            m_settings.waterfallColorScalePercent = clamped;
            if (m_waterfallWidget != nullptr) {
                m_waterfallWidget->setColorScalePercent(clamped);
            }
            if (m_lblWaterfallScale != nullptr) {
                m_lblWaterfallScale->setText(uiText("waterfall_color_scale", "Colour scale") +
                                             QStringLiteral(": %1%").arg(clamped));
            }
            m_settings.save();
        });
    }

    if (m_cmbWaterfallPalette != nullptr) {
        connect(m_cmbWaterfallPalette, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
            QString palette = m_cmbWaterfallPalette->itemData(index).toString().trimmed().toLower();
            if (palette != QStringLiteral("raptor") && palette != QStringLiteral("grayscale")) {
                palette = QStringLiteral("madmodem");
            }
            m_settings.waterfallPalette = palette;
            if (m_waterfallWidget != nullptr) {
                m_waterfallWidget->setPaletteName(m_settings.waterfallPalette);
            }
            m_settings.save();
        });
    }

    connect(ui->btnRefreshDevices, &QPushButton::clicked,
            this, &MainWindow::refreshDevices);

    connect(ui->btnStartRx, &QPushButton::clicked,
            this, &MainWindow::toggleRxReady);

    connect(ui->btnStopRx, &QPushButton::clicked,
            this, &MainWindow::stopRx);

    connect(ui->btnTestPtt, &QPushButton::clicked,
            this, &MainWindow::testPtt);

    connect(ui->btnTxTone, &QPushButton::clicked,
            this, &MainWindow::txToneTest);

    connect(m_btnLoadTxImage, &QPushButton::clicked,
            this, &MainWindow::loadTxImage);

    connect(m_btnStartImageTx, &QPushButton::clicked,
            this, &MainWindow::startImageTx);

    connect(m_btnStopImageTx, &QPushButton::clicked,
            this, &MainWindow::stopImageTx);

    connect(m_btnSstvEditor, &QPushButton::clicked,
            this, &MainWindow::openSstvImageEditor);
    connect(m_editSstvTxCall, &QLineEdit::textChanged,
            this, [this](const QString &) { updateSstvTxPreparedImage(); });
    connect(m_editSstvTxName, &QLineEdit::textChanged,
            this, [this](const QString &) { updateSstvTxPreparedImage(); });
    connect(m_editSstvTxQth, &QLineEdit::textChanged,
            this, [this](const QString &) { updateSstvTxPreparedImage(); });
    connect(m_editSstvTxReport, &QLineEdit::textChanged,
            this, [this](const QString &) { updateSstvTxPreparedImage(); });

    connect(ui->cmbMode, &QComboBox::currentTextChanged,
            this, &MainWindow::handleModeChanged);

    connect(ui->cmbSstvMode, &QComboBox::currentTextChanged,
            this, [this](const QString &) {
                applySstvSettings();
                updateSstvTxPreparedImage();
            });

    connect(ui->chkSstvAutoSync, &QCheckBox::toggled,
            this, [this](bool) {
                applySstvSettings();
            });

    connect(ui->spinSstvHorizontalShift, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) {
                applySstvSettings();
            });

    connect(ui->spinSstvRedShift, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) {
                applySstvSettings();
            });

    connect(ui->spinSstvBlueShift, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) {
                applySstvSettings();
            });

    connect(ui->btnSstvAnalyzeWav, &QPushButton::clicked,
            this, &MainWindow::openSstvWavFile);

    if (m_btnSstvForceRx != nullptr) {
        connect(m_btnSstvForceRx, &QPushButton::clicked,
                this, &MainWindow::forceSstvManualRx);
    }

    connect(ui->btnSstvResetImage, &QPushButton::clicked,
            this, &MainWindow::resetSstvImage);

    connect(ui->btnSstvSaveImage, &QPushButton::clicked,
            this, &MainWindow::saveSstvImage);

    connect(ui->btnSstvZoomFit, &QPushButton::clicked,
            m_faxImageWidget, &FaxImageWidget::setFitToWindow);

    connect(ui->btnSstvZoom100, &QPushButton::clicked,
            m_faxImageWidget, &FaxImageWidget::setActualSize);

    connect(ui->btnSstvZoomOut, &QPushButton::clicked,
            m_faxImageWidget, &FaxImageWidget::zoomOut);

    connect(ui->btnSstvZoomIn, &QPushButton::clicked,
            m_faxImageWidget, &FaxImageWidget::zoomIn);

    connect(ui->cmbFaxLpm, &QComboBox::currentTextChanged,
            this, [this](const QString &) {
                applyWeatherFaxSettings();
            });

    connect(ui->spinFaxBlackHz, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) {
                applyWeatherFaxSettings();
            });

    connect(ui->spinFaxWhiteHz, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) {
                applyWeatherFaxSettings();
            });

    connect(ui->chkFaxAutoStartPhasing, &QCheckBox::toggled,
            this, [this](bool) {
                applyWeatherFaxSettings();
            });

    connect(ui->chkFaxAutoToneTracking, &QCheckBox::toggled,
            this, [this](bool) {
                applyWeatherFaxSettings();
            });

    connect(ui->chkFaxInputBandpass, &QCheckBox::toggled,
            this, [this](bool) {
                applyWeatherFaxSettings();
            });

    connect(ui->chkFaxAutoSlant, &QCheckBox::toggled,
            this, [this](bool) {
                applyWeatherFaxSettings();
            });

    connect(ui->spinFaxSlantPpm, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double) {
                applyWeatherFaxSettings();
            });

    connect(ui->cmbFaxLinePreset, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::handleWeatherFaxLinePresetChanged);

    connect(ui->spinFaxImageLines, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) {
                applyWeatherFaxSettings();
            });

    connect(ui->chkFaxEndOfSignal, &QCheckBox::toggled,
            this, [this](bool) {
                applyWeatherFaxSettings();
            });

    connect(ui->spinFaxEndTimeoutSec, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) {
                applyWeatherFaxSettings();
            });

    connect(ui->chkFaxAutoSave, &QCheckBox::toggled,
            this, [this](bool) {
                applyWeatherFaxSettings();
            });

    connect(ui->editFaxOutputFolder, &QLineEdit::editingFinished,
            this, [this]() {
                applyWeatherFaxSettings();
            });

    connect(ui->btnFaxBrowseOutputFolder, &QPushButton::clicked,
            this, &MainWindow::browseWeatherFaxOutputFolder);

    connect(ui->btnFaxZoomFit, &QPushButton::clicked,
            m_faxImageWidget, &FaxImageWidget::setFitToWindow);

    connect(ui->btnFaxZoom100, &QPushButton::clicked,
            m_faxImageWidget, &FaxImageWidget::setActualSize);

    connect(ui->btnFaxZoomOut, &QPushButton::clicked,
            m_faxImageWidget, &FaxImageWidget::zoomOut);

    connect(ui->btnFaxZoomIn, &QPushButton::clicked,
            m_faxImageWidget, &FaxImageWidget::zoomIn);

    connect(ui->btnFaxAnalyzeWav, &QPushButton::clicked,
            this, &MainWindow::openWeatherFaxWavFile);

    if (m_btnFaxForceRx != nullptr) {
        connect(m_btnFaxForceRx, &QPushButton::clicked,
                this, &MainWindow::forceWeatherFaxManualRx);
    }

    connect(ui->btnFaxResetImage, &QPushButton::clicked,
            this, &MainWindow::resetWeatherFaxImage);

    connect(ui->btnFaxSaveImage, &QPushButton::clicked,
            this, &MainWindow::saveWeatherFaxImage);

    if (m_cmbRttyPreset != nullptr) {
        connect(m_cmbRttyPreset, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &MainWindow::handleRttyPresetChanged);
    }

    if (m_spinRttyBaud != nullptr) {
        connect(m_spinRttyBaud, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this](double) { applyRttySettings(); });
    }

    if (m_spinRttyShiftHz != nullptr) {
        connect(m_spinRttyShiftHz, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int) { applyRttySettings(); });
    }

    if (m_spinRttyMarkHz != nullptr) {
        connect(m_spinRttyMarkHz, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int) { applyRttySettings(); });
    }

    if (m_chkRttyReverse != nullptr) {
        connect(m_chkRttyReverse, &QCheckBox::toggled,
                this, [this](bool) { applyRttySettings(); });
    }

    if (m_chkRttyAutoReverse != nullptr) {
        connect(m_chkRttyAutoReverse, &QCheckBox::toggled,
                this, [this](bool) { applyRttySettings(); });
    }

    if (m_chkRttyAfc != nullptr) {
        connect(m_chkRttyAfc, &QCheckBox::toggled,
                this, [this](bool) { applyRttySettings(); });
    }

    if (m_spinRttyAfcRangeHz != nullptr) {
        connect(m_spinRttyAfcRangeHz, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int) { applyRttySettings(); });
    }

    auto connectRttyMonitorCheck = [this](QCheckBox *check) {
        if (check != nullptr) {
            connect(check, &QCheckBox::toggled, this, [this](bool) { applyRttySettings(); });
        }
    };
    connectRttyMonitorCheck(m_chkRttyMultiDecode);
    connectRttyMonitorCheck(m_chkRttyOverlayCallsigns);
    connectRttyMonitorCheck(m_chkRttyContestEnhanced);
    connectRttyMonitorCheck(m_chkRttySecondPass);
    if (m_spinRttyMaxDecoders != nullptr) {
        connect(m_spinRttyMaxDecoders, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int) { applyRttySettings(); });
    }

    if (m_btnRttyClearRx != nullptr) {
        connect(m_btnRttyClearRx, &QPushButton::clicked,
                this, &MainWindow::clearRttyRxText);
    }

    if (m_btnRttyLoadTxText != nullptr) {
        connect(m_btnRttyLoadTxText, &QPushButton::clicked,
                this, &MainWindow::loadRttyTxTextFile);
    }

    if (m_btnRttyClearTx != nullptr) {
        connect(m_btnRttyClearTx, &QPushButton::clicked,
                this, &MainWindow::clearRttyTxText);
    }

    if (m_btnRttySend != nullptr) {
        connect(m_btnRttySend, &QPushButton::clicked,
                this, &MainWindow::sendRttyTxText);
    }

    for (int i = 0; i < m_rttyMacroButtons.size(); ++i) {
        QPushButton *button = m_rttyMacroButtons.at(i);
        connect(button, &QPushButton::clicked,
                this, [this, i]() { sendTextMacro(i); });
    }

    if (m_txtRttyTx != nullptr) {
        connect(m_txtRttyTx, &QPlainTextEdit::textChanged,
                this, &MainWindow::updateTxPreview);
    }

    if (m_cmbBpsk31Variant != nullptr) {
        connect(m_cmbBpsk31Variant, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int) { applyBpsk31Settings(); populateQsoFormDefaults(m_bpsk31QsoForm, Bpsk31Decoder::modeName()); });
    }

    if (m_spinBpsk31ToneHz != nullptr) {
        connect(m_spinBpsk31ToneHz, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int) { applyBpsk31Settings(); });
    }

    if (m_chkBpsk31Afc != nullptr) {
        connect(m_chkBpsk31Afc, &QCheckBox::toggled,
                this, [this](bool) { applyBpsk31Settings(); });
    }

    if (m_spinBpsk31AfcRangeHz != nullptr) {
        connect(m_spinBpsk31AfcRangeHz, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int) { applyBpsk31Settings(); });
    }

    if (m_chkBpsk31Invert != nullptr) {
        connect(m_chkBpsk31Invert, &QCheckBox::toggled,
                this, [this](bool) { applyBpsk31Settings(); });
    }

    if (m_btnBpsk31ClearRx != nullptr) {
        connect(m_btnBpsk31ClearRx, &QPushButton::clicked,
                this, &MainWindow::clearBpsk31RxText);
    }

    if (m_btnBpsk31LoadTxText != nullptr) {
        connect(m_btnBpsk31LoadTxText, &QPushButton::clicked,
                this, &MainWindow::loadBpsk31TxTextFile);
    }

    if (m_btnBpsk31ClearTx != nullptr) {
        connect(m_btnBpsk31ClearTx, &QPushButton::clicked,
                this, &MainWindow::clearBpsk31TxText);
    }

    if (m_btnBpsk31Send != nullptr) {
        connect(m_btnBpsk31Send, &QPushButton::clicked,
                this, &MainWindow::sendBpsk31TxText);
    }

    for (int i = 0; i < m_bpsk31MacroButtons.size(); ++i) {
        QPushButton *button = m_bpsk31MacroButtons.at(i);
        connect(button, &QPushButton::clicked,
                this, [this, i]() { sendTextMacro(i); });
    }

    if (m_txtBpsk31Tx != nullptr) {
        connect(m_txtBpsk31Tx, &QPlainTextEdit::textChanged,
                this, &MainWindow::updateTxPreview);
    }

    if (m_cmbMfskVariant != nullptr) {
        connect(m_cmbMfskVariant, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int) { applyMfskSettings(); populateQsoFormDefaults(m_mfskQsoForm, MfskDecoder::modeName()); });
    }

    if (m_spinMfskCenterHz != nullptr) {
        connect(m_spinMfskCenterHz, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int) { applyMfskSettings(); });
    }

    if (m_chkMfskAfc != nullptr) {
        connect(m_chkMfskAfc, &QCheckBox::toggled,
                this, [this](bool) { applyMfskSettings(); });
    }

    if (m_spinMfskAfcRangeHz != nullptr) {
        connect(m_spinMfskAfcRangeHz, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int) { applyMfskSettings(); });
    }

    if (m_btnMfskClearRx != nullptr) {
        connect(m_btnMfskClearRx, &QPushButton::clicked,
                this, &MainWindow::clearMfskRxText);
    }

    if (m_btnMfskLoadTxText != nullptr) {
        connect(m_btnMfskLoadTxText, &QPushButton::clicked,
                this, &MainWindow::loadMfskTxTextFile);
    }

    if (m_btnMfskClearTx != nullptr) {
        connect(m_btnMfskClearTx, &QPushButton::clicked,
                this, &MainWindow::clearMfskTxText);
    }

    if (m_btnMfskSend != nullptr) {
        connect(m_btnMfskSend, &QPushButton::clicked,
                this, &MainWindow::sendMfskTxText);
    }

    for (int i = 0; i < m_mfskMacroButtons.size(); ++i) {
        QPushButton *button = m_mfskMacroButtons.at(i);
        connect(button, &QPushButton::clicked,
                this, [this, i]() { sendTextMacro(i); });
    }

    if (m_txtMfskTx != nullptr) {
        connect(m_txtMfskTx, &QPlainTextEdit::textChanged,
                this, &MainWindow::updateTxPreview);
    }

    if (m_spinCwToneHz != nullptr) {
        connect(m_spinCwToneHz, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int) { applyCwSettings(); });
    }

    if (m_spinCwWpm != nullptr) {
        connect(m_spinCwWpm, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int) { applyCwSettings(); });
    }

    if (m_chkCwAutoWpm != nullptr) {
        connect(m_chkCwAutoWpm, &QCheckBox::toggled,
                this, [this](bool checked) {
                    if (m_spinCwWpm != nullptr) {
                        m_spinCwWpm->setEnabled(!checked);
                    }
                    applyCwSettings();
                });
    }

    if (m_spinCwBandwidthHz != nullptr) {
        connect(m_spinCwBandwidthHz, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int) { applyCwSettings(); });
    }

    if (m_chkCwAfc != nullptr) {
        connect(m_chkCwAfc, &QCheckBox::toggled,
                this, [this](bool) { applyCwSettings(); });
    }

    if (m_spinCwAfcRangeHz != nullptr) {
        connect(m_spinCwAfcRangeHz, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int) { applyCwSettings(); });
    }

    if (m_chkCwSoftwareAgc != nullptr) {
        connect(m_chkCwSoftwareAgc, &QCheckBox::toggled,
                this, [this](bool) { applyCwSettings(); });
    }

    if (m_btnCwClearRx != nullptr) {
        connect(m_btnCwClearRx, &QPushButton::clicked,
                this, &MainWindow::clearCwRxText);
    }
    if (m_btnCwLoadTxText != nullptr) {
        connect(m_btnCwLoadTxText, &QPushButton::clicked,
                this, &MainWindow::loadCwTxTextFile);
    }
    if (m_btnCwClearTx != nullptr) {
        connect(m_btnCwClearTx, &QPushButton::clicked,
                this, &MainWindow::clearCwTxText);
    }
    if (m_btnCwSend != nullptr) {
        connect(m_btnCwSend, &QPushButton::clicked,
                this, &MainWindow::sendCwTxText);
    }
    for (int i = 0; i < m_cwMacroButtons.size(); ++i) {
        QPushButton *button = m_cwMacroButtons.at(i);
        connect(button, &QPushButton::clicked,
                this, [this, i]() { sendTextMacro(i); });
    }
    if (m_txtCwTx != nullptr) {
        connect(m_txtCwTx, &QPlainTextEdit::textChanged,
                this, &MainWindow::updateTxPreview);
    }

    if (m_cmbHellVariant != nullptr) {
        connect(m_cmbHellVariant, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int) {
                    const bool fsk105 = (m_cmbHellVariant != nullptr &&
                                         m_cmbHellVariant->currentData().toString() == "FSK105");
                    if (fsk105 && m_spinHellColumnRate != nullptr && qAbs(m_spinHellColumnRate->value() - 17.5) > 0.01) {
                        const QSignalBlocker blockRate(m_spinHellColumnRate);
                        m_spinHellColumnRate->setValue(17.5);
                    }
                    if (fsk105 && m_spinHellBandwidthHz != nullptr && m_spinHellBandwidthHz->value() == 245) {
                        const QSignalBlocker blockBandwidth(m_spinHellBandwidthHz);
                        m_spinHellBandwidthHz->setValue(220);
                    }
                    applyHellSettings();
                    populateQsoFormDefaults(m_hellQsoForm, HellschreiberDecoder::modeName());
                });
    }

    if (m_spinHellToneHz != nullptr) {
        connect(m_spinHellToneHz, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int) { applyHellSettings(); });
    }

    if (m_spinHellColumnRate != nullptr) {
        connect(m_spinHellColumnRate, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this](double) { applyHellSettings(); });
    }

    if (m_spinHellBandwidthHz != nullptr) {
        connect(m_spinHellBandwidthHz, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int) { applyHellSettings(); });
    }

    if (m_chkHellAfc != nullptr) {
        connect(m_chkHellAfc, &QCheckBox::toggled,
                this, [this](bool) { applyHellSettings(); });
    }

    if (m_spinHellAfcRangeHz != nullptr) {
        connect(m_spinHellAfcRangeHz, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int) { applyHellSettings(); });
    }

    if (m_sliderHellPaperScale != nullptr) {
        connect(m_sliderHellPaperScale, &QSlider::valueChanged,
                this, [this](int) { applyHellSettings(); });
    }

    if (m_btnHellLoadTxText != nullptr) {
        connect(m_btnHellLoadTxText, &QPushButton::clicked,
                this, &MainWindow::loadHellTxTextFile);
    }

    if (m_btnHellClearTx != nullptr) {
        connect(m_btnHellClearTx, &QPushButton::clicked,
                this, &MainWindow::clearHellTxText);
    }

    if (m_btnHellResetImage != nullptr) {
        connect(m_btnHellResetImage, &QPushButton::clicked,
                this, [this]() {
                    if (m_hellDecoder != nullptr) {
                        m_hellDecoder->reset();
                    }
                });
    }

    if (m_btnHellSend != nullptr) {
        connect(m_btnHellSend, &QPushButton::clicked,
                this, &MainWindow::sendHellTxText);
    }

    for (int i = 0; i < m_hellMacroButtons.size(); ++i) {
        QPushButton *button = m_hellMacroButtons.at(i);
        connect(button, &QPushButton::clicked,
                this, [this, i]() { sendTextMacro(i); });
    }

    if (m_txtHellTx != nullptr) {
        connect(m_txtHellTx, &QPlainTextEdit::textChanged,
                this, &MainWindow::updateTxPreview);
    }



    if (m_editFt8DxCall != nullptr) {
        connect(m_editFt8DxCall, &QLineEdit::textChanged,
                this, [this](const QString &) { applyFt8Settings(); });
    }
    if (m_editFt8DxGrid != nullptr) {
        connect(m_editFt8DxGrid, &QLineEdit::textChanged,
                this, [this](const QString &) { applyFt8Settings(); });
    }
    if (m_cmbFt8Band != nullptr) {
        connect(m_cmbFt8Band, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int) {
                    clearFt8DecodeList();
                    applyFt8Settings();
                    qsyRigToSelectedFtBand();
                    updateFtBandFrequencyUi();
                    applyCatRotatorSettings();
                });
    }
    if (m_spinFt8RxFreq != nullptr) {
        connect(m_spinFt8RxFreq, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int) { applyFt8Settings(); });
    }
    if (m_spinFt8TxFreq != nullptr) {
        connect(m_spinFt8TxFreq, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int) { applyFt8Settings(); });
    }
    // v2.19: no FT decoder-engine combo. The receive path is fixed;
    // operators only choose the Fast or Deep decode mode.
    if (m_radioFt8TxFirst != nullptr) {
        connect(m_radioFt8TxFirst, &QRadioButton::toggled,
                this, [this](bool checked) { if (checked) applyFt8Settings(); });
    }
    if (m_radioFt8TxSecond != nullptr) {
        connect(m_radioFt8TxSecond, &QRadioButton::toggled,
                this, [this](bool checked) { if (checked) applyFt8Settings(); });
    }
    if (m_btnFt8GenerateStd != nullptr) {
        connect(m_btnFt8GenerateStd, &QPushButton::clicked,
                this, &MainWindow::refreshFt8StandardMessages);
    }
    if (m_tableFt8Rx != nullptr) {
        connect(m_tableFt8Rx, &QTableWidget::itemDoubleClicked,
                this, &MainWindow::handleFt8DecodeDoubleClicked);
    }
    if (m_tableFt8QsoHistory != nullptr) {
        m_tableFt8QsoHistory->setToolTip(uiText("ft_history_activation_tooltip",
            "Double-click a valid FT QSO activity row to select/arm the corresponding transmission."));
        connect(m_tableFt8QsoHistory, &QTableWidget::itemDoubleClicked,
                this, &MainWindow::handleFt8QsoHistoryDoubleClicked);
    }
    if (m_tableFt8TxMessages != nullptr) {
        connect(m_tableFt8TxMessages, &QTableWidget::itemSelectionChanged,
                this, [this]() {
                    if (m_tableFt8TxMessages == nullptr) {
                        return;
                    }
                    const QList<QTableWidgetItem *> selected = m_tableFt8TxMessages->selectedItems();
                    if (!selected.isEmpty() && !m_ft8PendingTxArmed && !m_txRunning) {
                        setFt8ActiveTxRow(selected.first()->row());
                        updateFt8SequencerUi();
                    }
                });
    }
    if (m_btnFt8ClearRx != nullptr) {
        connect(m_btnFt8ClearRx, &QPushButton::clicked,
                this, &MainWindow::clearFt8DecodeList);
    }
    if (m_btnFt8Rx != nullptr) {
        connect(m_btnFt8Rx, &QPushButton::clicked,
                this, &MainWindow::startFt8RxShell);
    }
    if (m_btnFt8Tx != nullptr) {
        connect(m_btnFt8Tx, &QPushButton::clicked,
                this, &MainWindow::startFt8TxShell);
    }
    if (m_btnFt8Stop != nullptr) {
        connect(m_btnFt8Stop, &QPushButton::clicked,
                this, &MainWindow::stopFt8Shell);
    }
    if (m_btnFt8Tune != nullptr) {
        connect(m_btnFt8Tune, &QPushButton::clicked,
                this, &MainWindow::tuneFt8Shell);
    }
    if (m_chkFt8AutoSeq != nullptr) {
        connect(m_chkFt8AutoSeq, &QCheckBox::toggled,
                this, [this](bool) { applyFt8Settings(); });
    }
    if (m_chkFt8CqRepeat != nullptr) {
        connect(m_chkFt8CqRepeat, &QCheckBox::toggled,
                this, &MainWindow::handleFt8AutoCqToggled);
    }
    if (m_chkFt8AutoLog != nullptr) {
        connect(m_chkFt8AutoLog, &QCheckBox::toggled,
                this, [this](bool) { applyFt8Settings(); });
    }
    if (m_chkFt8FullAutoQso != nullptr) {
        connect(m_chkFt8FullAutoQso, &QCheckBox::toggled,
                this, &MainWindow::handleFt8FullAutoQsoToggled);
    }
    if (m_chkFt8EvilMode != nullptr) {
        connect(m_chkFt8EvilMode, &QCheckBox::toggled,
                this, &MainWindow::handleFt8EvilModeToggled);
    }
    if (m_chkFt8HoldTxFreq != nullptr) {
        connect(m_chkFt8HoldTxFreq, &QCheckBox::toggled,
                this, [this](bool checked) {
                    if (checked && m_cmbFt8TxStrategy != nullptr) {
                        const int idx = m_cmbFt8TxStrategy->findData(QStringLiteral("fixed"));
                        if (idx >= 0) {
                            const QSignalBlocker block(m_cmbFt8TxStrategy);
                            m_cmbFt8TxStrategy->setCurrentIndex(idx);
                        }
                    }
                    applyFt8Settings();
                });
    }
    if (m_cmbFt8TxStrategy != nullptr) {
        connect(m_cmbFt8TxStrategy, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int) {
                    const bool fixed = ft8TxFrequencyStrategyKey() == QStringLiteral("fixed");
                    if (m_chkFt8HoldTxFreq != nullptr && m_chkFt8HoldTxFreq->isChecked() != fixed) {
                        const QSignalBlocker block(m_chkFt8HoldTxFreq);
                        m_chkFt8HoldTxFreq->setChecked(fixed);
                    }
                    applyFt8Settings();
                });
    }
    if (m_chkFt8DeepDecode != nullptr) {
        connect(m_chkFt8DeepDecode, &QCheckBox::toggled,
                this, [this](bool enabled) {
                    // Deprecated compatibility checkbox.  The visible UI now
                    // exposes only Fast and Adaptive decode depth.
                    if (enabled && m_chkFt8DspPlusDecode != nullptr && m_chkFt8DspPlusDecode->isChecked()) {
                        const QSignalBlocker block(m_chkFt8DspPlusDecode);
                        m_chkFt8DspPlusDecode->setChecked(false);
                    }
                    applyFt8Settings();
                });
    }
    if (m_chkFt8DspPlusDecode != nullptr) {
        connect(m_chkFt8DspPlusDecode, &QCheckBox::toggled,
                this, [this](bool enabled) {
                    // Deprecated compatibility checkbox.  v3.25 no longer
                    // exposes Deep/DSP++ modes.
                    if (enabled && m_chkFt8DeepDecode != nullptr && m_chkFt8DeepDecode->isChecked()) {
                        const QSignalBlocker block(m_chkFt8DeepDecode);
                        m_chkFt8DeepDecode->setChecked(false);
                    }
                    applyFt8Settings();
                });
    }
    if (m_cmbFtLiveDecodeDepth != nullptr) {
        connect(m_cmbFtLiveDecodeDepth, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int) { applyFt8Settings(); });
    }
    if (m_btnFt8AnalyzeWav != nullptr) {
        connect(m_btnFt8AnalyzeWav, &QPushButton::clicked,
                this, &MainWindow::openFtWavFile);
    }
    if (m_btnFtDecodeAnalyzeWav != nullptr) {
        connect(m_btnFtDecodeAnalyzeWav, &QPushButton::clicked,
                this, &MainWindow::openFtWavFile);
    }
    if (m_btnFtDecodeAutoTest != nullptr) {
        connect(m_btnFtDecodeAutoTest, &QPushButton::clicked,
                this, &MainWindow::runFtAutoTest);
    }
    if (m_spinFt8CqTimeoutMin != nullptr) {
        connect(m_spinFt8CqTimeoutMin, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int) { applyFt8Settings(); });
    }
    if (m_spinFt8NoResponseLimit != nullptr) {
        connect(m_spinFt8NoResponseLimit, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int) { applyFt8Settings(); });
    }

    connect(&m_pttTestTimer, &QTimer::timeout,
            this, &MainWindow::finishPttTest);

    connect(m_audioEngine, &AudioEngine::levelChanged,
            this, &MainWindow::handleAudioLevel);

    connect(m_audioEngine, &AudioEngine::started,
            this, &MainWindow::handleAudioStarted);

    connect(m_audioEngine, &AudioEngine::stopped,
            this, &MainWindow::handleAudioStopped);

    connect(m_audioEngine, &AudioEngine::errorOccurred,
            this, &MainWindow::handleAudioError);
}

void MainWindow::populateWeatherFaxLinePresets()
{
    const QSignalBlocker blockPreset(ui->cmbFaxLinePreset);

    ui->cmbFaxLinePreset->clear();

    for (const FaxLinePreset &preset : weatherFaxLinePresets()) {
        ui->cmbFaxLinePreset->addItem(preset.label, preset.key);
    }
}

void MainWindow::loadWeatherFaxSettingsToUi()
{
    const QSignalBlocker blockLpm(ui->cmbFaxLpm);
    const QSignalBlocker blockBlack(ui->spinFaxBlackHz);
    const QSignalBlocker blockWhite(ui->spinFaxWhiteHz);
    const QSignalBlocker blockAutoStart(ui->chkFaxAutoStartPhasing);
    const QSignalBlocker blockAutoTone(ui->chkFaxAutoToneTracking);
    const QSignalBlocker blockInputBandpass(ui->chkFaxInputBandpass);
    const QSignalBlocker blockAutoSlant(ui->chkFaxAutoSlant);
    const QSignalBlocker blockSlantPpm(ui->spinFaxSlantPpm);
    const QSignalBlocker blockLinePreset(ui->cmbFaxLinePreset);
    const QSignalBlocker blockImageLines(ui->spinFaxImageLines);
    const QSignalBlocker blockEndOfSignal(ui->chkFaxEndOfSignal);
    const QSignalBlocker blockEndTimeout(ui->spinFaxEndTimeoutSec);
    const QSignalBlocker blockAutoSave(ui->chkFaxAutoSave);
    const QSignalBlocker blockOutputFolder(ui->editFaxOutputFolder);

    const QString lpmText = QString::number(m_settings.weatherFaxLpm);
    const int lpmIndex = ui->cmbFaxLpm->findText(lpmText);

    if (lpmIndex >= 0) {
        ui->cmbFaxLpm->setCurrentIndex(lpmIndex);
    } else {
        ui->cmbFaxLpm->setCurrentIndex(ui->cmbFaxLpm->findText("120"));
        m_settings.weatherFaxLpm = 120;
    }

    ui->spinFaxBlackHz->setValue(m_settings.weatherFaxBlackHz);
    ui->spinFaxWhiteHz->setValue(m_settings.weatherFaxWhiteHz);
    ui->chkFaxAutoStartPhasing->setChecked(m_settings.weatherFaxAutoStartPhasing);
    ui->chkFaxAutoToneTracking->setChecked(m_settings.weatherFaxAutoToneTracking);
    ui->chkFaxInputBandpass->setChecked(m_settings.weatherFaxInputBandpass);
    m_settings.weatherFaxAutoSlantCorrection = false;
    m_settings.weatherFaxManualSlantPpm = 0.0;
    ui->chkFaxAutoSlant->setChecked(false);
    ui->spinFaxSlantPpm->setValue(0.0);

    const int presetIndex = ui->cmbFaxLinePreset->findData(m_settings.weatherFaxLinePreset);
    ui->cmbFaxLinePreset->setCurrentIndex(qMax(0, presetIndex));
    const FaxLinePreset selectedPreset = presetByKey(ui->cmbFaxLinePreset->currentData().toString());
    ui->cmbFaxLinePreset->setToolTip(selectedPreset.details);
    ui->cmbFaxLinePreset->setStatusTip(selectedPreset.details);
    ui->spinFaxImageLines->setValue(qBound(120, m_settings.weatherFaxImageLines, 8000));
    ui->chkFaxEndOfSignal->setChecked(m_settings.weatherFaxEndOfSignal);
    ui->spinFaxEndTimeoutSec->setValue(qBound(3, m_settings.weatherFaxEndOfSignalTimeoutSec, 180));
    ui->chkFaxAutoSave->setChecked(m_settings.weatherFaxAutoSave);

    if (m_settings.weatherFaxOutputFolder.trimmed().isEmpty()) {
        m_settings.weatherFaxOutputFolder = defaultWeatherFaxOutputFolder();
    }

    ui->editFaxOutputFolder->setText(m_settings.weatherFaxOutputFolder);
}

void MainWindow::loadSstvSettingsToUi()
{
    const QSignalBlocker blockMode(ui->cmbSstvMode);
    const QSignalBlocker blockSync(ui->chkSstvAutoSync);
    const QSignalBlocker blockShift(ui->spinSstvHorizontalShift);
    const QSignalBlocker blockRedShift(ui->spinSstvRedShift);
    const QSignalBlocker blockBlueShift(ui->spinSstvBlueShift);

    ui->cmbSstvMode->clear();

    for (const QString &modeName : SstvDecoder::availableModeNames()) {
        ui->cmbSstvMode->addItem(modeName);
    }

    int modeIndex = ui->cmbSstvMode->findText(m_settings.sstvMode);

    if (modeIndex < 0) {
        modeIndex = ui->cmbSstvMode->findText("Martin M1");
    }

    ui->cmbSstvMode->setCurrentIndex(qMax(0, modeIndex));
    ui->chkSstvAutoSync->setChecked(m_settings.sstvAutoSync);
    ui->spinSstvHorizontalShift->setValue(m_settings.sstvHorizontalShiftPixels);
    ui->spinSstvRedShift->setValue(m_settings.sstvRedShiftPixels);
    ui->spinSstvBlueShift->setValue(m_settings.sstvBlueShiftPixels);
}


void MainWindow::invokeRigConfigureFromSettings()
{
    if (m_rigController == nullptr) {
        return;
    }
    const AppSettings settingsCopy = m_settings;
    QMetaObject::invokeMethod(m_rigController, [controller = m_rigController, settingsCopy]() {
        controller->configureFromSettings(settingsCopy);
    }, Qt::QueuedConnection);
    appendLog(QStringLiteral("CAT: configure request queued."));
}

bool MainWindow::invokeRigPttBlocking(bool enabled)
{
    if (m_rigController == nullptr) {
        return false;
    }
    bool ok = false;
    if (QThread::currentThread() == m_rigController->thread()) {
        ok = m_rigController->setPtt(enabled);
    } else {
        QMetaObject::invokeMethod(m_rigController, [controller = m_rigController, enabled, &ok]() {
            ok = controller->setPtt(enabled);
        }, Qt::BlockingQueuedConnection);
    }
    return ok;
}

void MainWindow::invokeRigSetFrequency(double frequencyHz)
{
    if (m_rigController == nullptr || frequencyHz <= 0.0) {
        return;
    }
    QMetaObject::invokeMethod(m_rigController, [controller = m_rigController, frequencyHz]() {
        controller->setFrequencyHz(frequencyHz);
    }, Qt::QueuedConnection);
}



void MainWindow::applyUiAppearanceSettings()
{
    QApplication *app = qobject_cast<QApplication *>(QCoreApplication::instance());
    if (app == nullptr) {
        return;
    }

    QString theme = m_settings.uiTheme.trimmed().toLower();
    if (theme.isEmpty()) theme = QStringLiteral("avionica");
    if (theme != QStringLiteral("avionica") && theme != QStringLiteral("qt_default") &&
        theme != QStringLiteral("hacker_green") && theme != QStringLiteral("classic_dark") &&
        theme != QStringLiteral("high_contrast")) {
        theme = QStringLiteral("avionica");
    }

    auto recommendedFont = [&]() -> QFont {
        if (theme == QStringLiteral("hacker_green")) {
            QFont f = QFontDatabase::systemFont(QFontDatabase::FixedFont);
            if (f.pointSize() <= 0) f.setPointSize(10);
            return f;
        }
        QFont f = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
        if (f.pointSize() <= 0) f.setPointSize(theme == QStringLiteral("high_contrast") ? 12 : 10);
        if (theme == QStringLiteral("high_contrast") && f.pointSize() < 12) f.setPointSize(12);
        return f;
    };

    QFont font = recommendedFont();
    if (!m_settings.uiUseThemeFont) {
        const QString family = m_settings.uiFontFamily.trimmed();
        if (!family.isEmpty()) {
            font.setFamily(family);
        }
        font.setPointSize(qBound(8, m_settings.uiFontPointSize, 24));
    }
    app->setFont(font);

    if (theme == QStringLiteral("qt_default")) {
        app->setPalette(app->style() != nullptr ? app->style()->standardPalette() : QPalette());
        app->setStyleSheet(QString());
        return;
    }

    if (theme == QStringLiteral("avionica")) {
        MadModemUi::applyCockpitTheme(*app);
        app->setFont(font);
        return;
    }

    QPalette pal;
    if (theme == QStringLiteral("hacker_green")) {
        pal.setColor(QPalette::Window, QColor(0, 8, 0));
        pal.setColor(QPalette::WindowText, QColor(80, 255, 80));
        pal.setColor(QPalette::Base, QColor(0, 0, 0));
        pal.setColor(QPalette::AlternateBase, QColor(0, 20, 0));
        pal.setColor(QPalette::Text, QColor(96, 255, 96));
        pal.setColor(QPalette::Button, QColor(0, 24, 0));
        pal.setColor(QPalette::ButtonText, QColor(96, 255, 96));
        pal.setColor(QPalette::Highlight, QColor(40, 180, 40));
        pal.setColor(QPalette::HighlightedText, QColor(0, 0, 0));
        app->setPalette(pal);
        app->setStyleSheet(QStringLiteral(R"QSS(
QWidget { background-color: #000800; color: #60ff60; }
QMainWindow, QDialog { background-color: #000800; }
QGroupBox { border: 1px solid #1b8f1b; border-radius: 6px; margin-top: 8px; padding: 6px; }
QGroupBox::title { color: #7cff7c; subcontrol-origin: margin; left: 8px; padding: 0 4px; }
QPushButton, QToolButton { background-color: #001800; color: #7cff7c; border: 1px solid #2ad62a; border-radius: 5px; padding: 4px 8px; min-height: 24px; }
QPushButton:hover, QToolButton:hover { background-color: #003000; }
QLineEdit, QTextEdit, QPlainTextEdit, QComboBox, QSpinBox, QDoubleSpinBox { background-color: #000000; color: #60ff60; border: 1px solid #1b8f1b; selection-background-color: #2ad62a; selection-color: #000000; }
QTabBar::tab { background: #001000; color: #60ff60; border: 1px solid #1b8f1b; padding: 5px 10px; }
QTabBar::tab:selected { background: #002800; color: #a0ffa0; }
QHeaderView::section { background: #001000; color: #7cff7c; border: 1px solid #1b8f1b; }
QTableWidget, QTreeWidget, QListWidget { background-color: #000000; alternate-background-color: #001400; color: #60ff60; gridline-color: #1b8f1b; }
QScrollBar { background: #000800; }
QScrollBar::handle { background: #136d13; border-radius: 4px; }
QToolTip { background-color: #001000; color: #a0ffa0; border: 1px solid #2ad62a; }
)QSS"));
        return;
    }

    const bool high = (theme == QStringLiteral("high_contrast"));
    pal.setColor(QPalette::Window, high ? QColor(0, 0, 0) : QColor(32, 34, 38));
    pal.setColor(QPalette::WindowText, high ? QColor(255, 255, 255) : QColor(230, 232, 235));
    pal.setColor(QPalette::Base, high ? QColor(0, 0, 0) : QColor(22, 24, 28));
    pal.setColor(QPalette::AlternateBase, high ? QColor(28, 28, 28) : QColor(42, 44, 48));
    pal.setColor(QPalette::Text, high ? QColor(255, 255, 255) : QColor(230, 232, 235));
    pal.setColor(QPalette::Button, high ? QColor(20, 20, 20) : QColor(44, 47, 53));
    pal.setColor(QPalette::ButtonText, high ? QColor(255, 255, 255) : QColor(235, 238, 242));
    pal.setColor(QPalette::Highlight, high ? QColor(255, 214, 0) : QColor(86, 132, 214));
    pal.setColor(QPalette::HighlightedText, high ? QColor(0, 0, 0) : QColor(255, 255, 255));
    app->setPalette(pal);
    app->setStyleSheet(high ? QStringLiteral(R"QSS(
QWidget { background-color: #000000; color: #ffffff; }
QGroupBox, QFrame { border-color: #ffffff; }
QPushButton, QToolButton { background-color: #101010; color: #ffffff; border: 2px solid #ffffff; border-radius: 5px; padding: 5px 10px; min-height: 28px; }
QLineEdit, QTextEdit, QPlainTextEdit, QComboBox, QSpinBox, QDoubleSpinBox, QTableWidget, QListWidget, QTreeWidget { background-color: #000000; color: #ffffff; border: 2px solid #ffffff; selection-background-color: #ffd600; selection-color: #000000; }
QHeaderView::section { background: #000000; color: #ffffff; border: 1px solid #ffffff; padding: 4px; }
QTabBar::tab { background: #000000; color: #ffffff; border: 2px solid #ffffff; padding: 6px 12px; }
QTabBar::tab:selected { background: #ffd600; color: #000000; }
)QSS") : QStringLiteral(R"QSS(
QWidget { background-color: #202226; color: #e6e8eb; }
QGroupBox { border: 1px solid #555b66; border-radius: 6px; margin-top: 8px; padding: 6px; }
QGroupBox::title { color: #d8dee9; subcontrol-origin: margin; left: 8px; padding: 0 4px; }
QPushButton, QToolButton { background-color: #2c2f35; color: #eceff4; border: 1px solid #666d78; border-radius: 5px; padding: 4px 8px; min-height: 24px; }
QPushButton:hover, QToolButton:hover { background-color: #3a3f48; }
QLineEdit, QTextEdit, QPlainTextEdit, QComboBox, QSpinBox, QDoubleSpinBox { background-color: #16181c; color: #e6e8eb; border: 1px solid #555b66; selection-background-color: #5684d6; }
QHeaderView::section { background: #2c2f35; color: #e6e8eb; border: 1px solid #555b66; padding: 3px; }
QTableWidget, QTreeWidget, QListWidget { background-color: #16181c; alternate-background-color: #22252b; color: #e6e8eb; gridline-color: #555b66; }
QTabBar::tab { background: #2c2f35; color: #e6e8eb; border: 1px solid #555b66; padding: 5px 10px; }
QTabBar::tab:selected { background: #3a3f48; color: #ffffff; }
QToolTip { background-color: #202226; color: #ffffff; border: 1px solid #666d78; }
)QSS"));
}

void MainWindow::applyDecodeTableVisualSettings(QTableWidget *table)
{
    if (table == nullptr) {
        return;
    }

    const int fontPt = qBound(8, m_settings.decodeTableFontPointSize, 18);
    const int rowPx = qBound(16, m_settings.decodeTableRowHeightPx, 48);

    QFont f = table->font();
    f.setPointSize(fontPt);
    table->setFont(f);
    if (table->horizontalHeader() != nullptr) {
        table->horizontalHeader()->setFont(f);
    }
    if (table->verticalHeader() != nullptr) {
        table->verticalHeader()->setDefaultSectionSize(rowPx);
        table->verticalHeader()->setMinimumSectionSize(qMin(rowPx, 16));
    }
    for (int r = 0; r < table->rowCount(); ++r) {
        table->setRowHeight(r, rowPx);
    }
}

void MainWindow::applyDecodeTableVisualSettings()
{
    applyDecodeTableVisualSettings(m_tableFt8Rx);
    applyDecodeTableVisualSettings(m_tableFt8QsoHistory);
    applyDecodeTableVisualSettings(m_tableMsk144Rx);
    applyDecodeTableVisualSettings(m_tableQ65Rx);
    // Keep the standard-message tables readable but compact as well; they are
    // decode/QSO workflow tables, not free-form editors.
    applyDecodeTableVisualSettings(m_tableFt8TxMessages);
    applyDecodeTableVisualSettings(m_tableMsk144TxMessages);
    applyDecodeTableVisualSettings(m_tableQ65TxMessages);
}


void MainWindow::applyPersistentSettingsToRuntime()
{
    applyUiAppearanceSettings();
    applyDecodeTableVisualSettings();

    if (m_audioEngine != nullptr) {
        m_audioEngine->setClockCorrectionPpm(m_settings.audioRxClockPpm);
    }

    if (m_ntpClient != nullptr) {
        m_ntpClient->setEnabled(false);
    }

    invokeRigConfigureFromSettings();
    updateRigControlStatusUi();
    applyCatRotatorSettings();

    m_weatherFaxDecoder->setLpm(m_settings.weatherFaxLpm);
    m_weatherFaxDecoder->setToneRange(
        static_cast<double>(m_settings.weatherFaxBlackHz),
        static_cast<double>(m_settings.weatherFaxWhiteHz)
        );
    m_weatherFaxDecoder->setAutoStartEnabled(m_settings.weatherFaxAutoStartPhasing);
    m_weatherFaxDecoder->setAutoToneTrackingEnabled(m_settings.weatherFaxAutoToneTracking);
    m_weatherFaxDecoder->setInputBandpassEnabled(m_settings.weatherFaxInputBandpass);
    m_weatherFaxDecoder->setAutoSlantCorrectionEnabled(false);
    m_weatherFaxDecoder->setManualSlantPpm(0.0);
    m_weatherFaxDecoder->setTargetImageLines(m_settings.weatherFaxImageLines);
    m_weatherFaxDecoder->setEndOfSignalCompletionEnabled(m_settings.weatherFaxEndOfSignal);
    m_weatherFaxDecoder->setEndOfSignalTimeoutSec(m_settings.weatherFaxEndOfSignalTimeoutSec);

    m_sstvDecoder->setModeName(m_settings.sstvMode);
    m_sstvDecoder->setAutoSyncEnabled(m_settings.sstvAutoSync);
    m_sstvDecoder->setHorizontalShiftPixels(m_settings.sstvHorizontalShiftPixels);
    m_sstvDecoder->setColorShiftPixels(m_settings.sstvRedShiftPixels,
                                       m_settings.sstvBlueShiftPixels);

    m_rttyDecoder->setBaudRate(m_settings.rttyBaudRate);
    m_rttyDecoder->setTones(static_cast<double>(m_settings.rttyMarkHz),
                            static_cast<double>(m_settings.rttyMarkHz + m_settings.rttyShiftHz));
    m_rttyDecoder->setReverse(m_settings.rttyReverse);
    m_rttyDecoder->setAutoReverseEnabled(m_settings.rttyAutoReverseEnabled);

    m_bpsk31Decoder->setSymbolRate(bpskSymbolRateForVariant(m_settings.bpsk31Variant));
    m_bpsk31Decoder->setQpskMode(pskVariantIsQpsk(m_settings.bpsk31Variant));
    m_bpsk31Decoder->setToneHz(static_cast<double>(m_settings.bpsk31ToneHz));
    m_bpsk31Decoder->setAfcEnabled(m_settings.bpsk31AfcEnabled);
    m_bpsk31Decoder->setAfcRangeHz(static_cast<double>(m_settings.bpsk31AfcRangeHz));
    m_bpsk31Decoder->setInvertBits(m_settings.bpsk31InvertBits);

    if (m_mfskDecoder != nullptr) {
        m_mfskDecoder->setVariant(MfskDecoder::variantFromKey(m_settings.mfskVariant));
        m_mfskDecoder->setCenterHz(static_cast<double>(m_settings.mfskCenterHz));
        m_mfskDecoder->setAfcEnabled(m_settings.mfskAfcEnabled);
        m_mfskDecoder->setAfcRangeHz(static_cast<double>(m_settings.mfskAfcRangeHz));
    }

    m_cwSecondaryEnabled = m_settings.cwSecondaryEnabled;
    m_cwSecondaryToneHz = qBound(250, m_settings.cwSecondaryToneHz, 3000);
    m_cwDecoder->setToneHz(static_cast<double>(m_settings.cwToneHz));
    m_cwDecoder->setSecondaryToneHz(static_cast<double>(m_cwSecondaryToneHz));
    m_cwDecoder->setSecondaryEnabled(m_cwSecondaryEnabled);
    m_cwDecoder->setAutoWpm(m_settings.cwAutoWpm);
    m_cwDecoder->setWpm(static_cast<double>(m_settings.cwWpm));
    m_cwDecoder->setBandwidthHz(static_cast<double>(m_settings.cwBandwidthHz));
    if (m_cwSecondaryDecoder != nullptr) {
        m_cwSecondaryEnabled = m_settings.cwSecondaryEnabled;
        m_cwSecondaryToneHz = m_settings.cwSecondaryToneHz;
        m_cwSecondaryDecoder->setToneHz(static_cast<double>(m_cwSecondaryToneHz));
        m_cwSecondaryDecoder->setAutoWpm(m_settings.cwAutoWpm);
        m_cwSecondaryDecoder->setWpm(static_cast<double>(m_settings.cwWpm));
        m_cwSecondaryDecoder->setBandwidthHz(static_cast<double>(m_settings.cwBandwidthHz));
    }
    m_hellDecoder->setVariant(HellschreiberDecoder::variantFromKey(m_settings.hellVariant));
    m_hellDecoder->setToneHz(static_cast<double>(m_settings.hellToneHz));
    m_hellDecoder->setColumnRate(m_settings.hellColumnRate);
    m_hellDecoder->setBandwidthHz(static_cast<double>(m_settings.hellBandwidthHz));
    m_hellDecoder->setVerticalScale(m_settings.hellPaperScale);
    m_hellDecoder->setFskShiftHz(HellschreiberDecoder::fsk105ShiftHz());

    if (m_ft8RxDecoder != nullptr) {
        const QString activeModeName = (ui != nullptr && ui->cmbMode != nullptr) ? ui->cmbMode->currentText() : QStringLiteral("FT8");
        QMetaObject::invokeMethod(m_ft8RxDecoder, "setModeName", Qt::QueuedConnection, Q_ARG(QString, activeModeName));
        QMetaObject::invokeMethod(m_ft8RxDecoder, "setDecodeEngine", Qt::QueuedConnection, Q_ARG(QString, QStringLiteral("mshv")));
        QMetaObject::invokeMethod(m_ft8RxDecoder, "setDeepDecodeEnabled", Qt::QueuedConnection, Q_ARG(bool, m_settings.ft8DeepDecode));
        QMetaObject::invokeMethod(m_ft8RxDecoder, "setDspPlusDecodeEnabled", Qt::QueuedConnection, Q_ARG(bool, m_settings.ft8DspPlusDecode));
        QMetaObject::invokeMethod(m_ft8RxDecoder, "setRxMarkerHz", Qt::QueuedConnection, Q_ARG(int, m_settings.ft8RxFrequencyHz));
        QMetaObject::invokeMethod(m_ft8RxDecoder, "setMyCall", Qt::QueuedConnection, Q_ARG(QString, stationCallsign()));
        QMetaObject::invokeMethod(m_ft8RxDecoder, "setDxCall", Qt::QueuedConnection, Q_ARG(QString, m_settings.ft8DxCallsign));
    }

    if (m_waterfallWidget != nullptr) {
        m_waterfallWidget->setColorScalePercent(m_settings.waterfallColorScalePercent);
        m_waterfallWidget->setPaletteName(m_settings.waterfallPalette);
    }
    if (m_cmbWaterfallPalette != nullptr) {
        const int paletteIndex = m_cmbWaterfallPalette->findData(m_settings.waterfallPalette);
        m_cmbWaterfallPalette->setCurrentIndex(paletteIndex >= 0 ? paletteIndex : 0);
    }

    selectComboByBackendName(ui->cmbAudioInput, m_settings.audioInputName);
    selectComboByBackendName(ui->cmbAudioOutput, m_settings.audioOutputName);
    selectComboByBackendName(ui->cmbPttPort, m_settings.pttPortName);
}


void MainWindow::updateRigControlStatusUi()
{
    const bool hamlibBuilt = HamlibController::isCompiledWithHamlib();
    const bool catEnabled = m_settings.hamlibCatEnabled;
    const bool pttEnabled = m_settings.hamlibPttEnabled;
    const bool connected = m_rigCatConnected;

    if (m_grpRigStatus != nullptr) {
        m_grpRigStatus->setTitle(uiText("rig_status_group", "Rig / CAT"));
        m_grpRigStatus->setVisible(catEnabled || pttEnabled || m_lastRigFrequencyHz > 0.0);
    }

    if (m_lblRigCatStatus != nullptr) {
        QString text;
        if (!catEnabled && !pttEnabled) {
            text = uiText("rig_cat_disabled", "CAT: disabled");
        } else if (!hamlibBuilt) {
            text = uiText("rig_hamlib_not_built", "Hamlib: not compiled in");
        } else {
            const QString status = m_rigStatusMirror;
            text = uiText("rig_cat_status", "CAT status") + QStringLiteral(": ") +
                   (status.isEmpty() ? (connected ? uiText("connected", "connected") : uiText("not_connected", "not connected")) : status);
        }
        m_lblRigCatStatus->setText(text);
    }

    if (m_lblRigFrequency != nullptr) {
        const QString freqText = m_lastRigFrequencyHz > 0.0
            ? formatRigFrequency(m_lastRigFrequencyHz)
            : QStringLiteral("--");
        QString text = uiText("rig_frequency", "Frequency") + QStringLiteral(": ") + freqText;
        const QString band = bandFromFrequencyHz(m_lastRigFrequencyHz);
        if (!band.isEmpty()) {
            text += QStringLiteral("  ") + uiText("band", "Band") + QStringLiteral(": ") + band;
        }
        m_lblRigFrequency->setText(text);
    }

    if (m_lblRigPttStatus != nullptr) {
        QString pttText;
        if (!pttEnabled) {
            pttText = uiText("rig_ptt_serial_or_none", "PTT: serial RTS / none");
        } else {
            pttText = uiText("rig_ptt", "PTT") + QStringLiteral(": ") +
                      (m_lastRigPttOn ? uiText("on", "ON") : uiText("off", "OFF"));
        }
        m_lblRigPttStatus->setText(pttText);
        m_lblRigPttStatus->setStyleSheet(m_lastRigPttOn
            ? QStringLiteral("font-weight: 500; color: #c82020;")
            : QStringLiteral("font-weight: 500; color: #15803d;"));
    }
}

void MainWindow::handleRigFrequencyChanged(double frequencyHz)
{
    m_lastRigFrequencyHz = frequencyHz;

    const QString band = ftBandFromFrequencyHz(frequencyHz);
    if (m_settings.hamlibUpdateFt8Band && !band.isEmpty()) {
        if (m_cmbFt8Band != nullptr && m_cmbFt8Band->currentText() != band) {
            const int idx = m_cmbFt8Band->findText(band);
            if (idx >= 0) {
                const QSignalBlocker blockBand(m_cmbFt8Band);
                m_cmbFt8Band->setCurrentIndex(idx);
                m_settings.ft8Band = band;
                refreshFt8StandardMessages();
            }
        }
    }

    updateFtBandFrequencyUi();
    updateRigControlStatusUi();
}

void MainWindow::handleRigStatusChanged(const QString &status)
{
    Q_UNUSED(status)
    updateRigControlStatusUi();
}


QString MainWindow::stationCallsign() const
{
    return m_settings.textMyCallsign.trimmed().toUpper();
}

QString MainWindow::stationLocator() const
{
    return m_settings.textMyLocator.trimmed().toUpper();
}

bool MainWindow::stationIdentityReady(QString *reason) const
{
    const QString call = stationCallsign();
    const QString locator = stationLocator();
    if (call.isEmpty()) {
        if (reason != nullptr) {
            *reason = uiText("station_identity_missing_call", "Set My Call in Settings -> User/QTH before using TX/PTT or moving the rotator.");
        }
        return false;
    }
    if (locator.isEmpty()) {
        if (reason != nullptr) {
            *reason = uiText("station_identity_missing_locator", "Set My Locator in Settings -> User/QTH before using TX/PTT or moving the rotator.");
        }
        return false;
    }
    QPointF homeLonLat;
    if (!QsoMapWidget::maidenheadToLonLat(locator, &homeLonLat)) {
        if (reason != nullptr) {
            *reason = uiText("station_identity_invalid_locator", "My Locator in Settings -> User/QTH is not a valid Maidenhead locator.");
        }
        return false;
    }
    return true;
}

bool MainWindow::ensureStationIdentityForTx(const QString &modeLabel)
{
    QString reason;
    if (stationIdentityReady(&reason)) {
        return true;
    }

    appendLog(QStringLiteral("%1 blocked: %2").arg(modeLabel, reason));
    showTxBlockedWarning(modeLabel,
                         reason,
                         AppSettingsDialog::InitialPage::UserQthMacros,
                         uiText("open_user_qth_settings", "Open User/QTH settings"));
    return false;
}

void MainWindow::showTxBlockedWarning(const QString &modeLabel,
                                      const QString &reason,
                                      AppSettingsDialog::InitialPage settingsPage,
                                      const QString &openSettingsText)
{
    const QString cleanMode = modeLabel.trimmed().isEmpty() ? QStringLiteral("TX/PTT") : modeLabel.trimmed();
    const QString cleanReason = reason.trimmed().isEmpty()
        ? uiText("tx_blocked_unknown_reason", "MadModem blocked TX/PTT for safety, but no detailed reason was returned.")
        : reason.trimmed();

    QMessageBox box(this);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(uiText("tx_blocked_title", "TX/PTT blocked"));
    box.setText(uiText("tx_blocked_message", "%1 cannot transmit because a required safety/configuration check failed.").arg(cleanMode));
    box.setInformativeText(cleanReason + QStringLiteral("\n\n") +
                           uiText("tx_blocked_fix_hint",
                                  "Open Settings, correct the highlighted station/PTT/CAT configuration, then try TX again."));

    QPushButton *openButton = box.addButton(openSettingsText.trimmed().isEmpty()
                                                ? uiText("open_settings", "Open Settings")
                                                : openSettingsText.trimmed(),
                                            QMessageBox::ActionRole);
    box.addButton(uiText("cancel_tx", "Cancel TX"), QMessageBox::RejectRole);
    box.setDefaultButton(openButton);
    box.exec();

    if (box.clickedButton() == openButton) {
        showAppSettingsDialogPage(settingsPage);
    }
}

QString MainWindow::bandFromFrequencyHz(double frequencyHz) const
{
    if (frequencyHz <= 0.0) {
        return QString();
    }
    const double mhz = frequencyHz / 1000000.0;
    struct BandRange { double low; double high; const char *name; };
    static const BandRange ranges[] = {
        {1.8, 2.0, "160m"},
        {3.5, 4.0, "80m"},
        {5.0, 5.5, "60m"},
        {7.0, 7.3, "40m"},
        {10.1, 10.15, "30m"},
        {14.0, 14.35, "20m"},
        {18.068, 18.168, "17m"},
        {21.0, 21.45, "15m"},
        {24.89, 24.99, "12m"},
        {28.0, 29.7, "10m"},
        {50.0, 54.0, "6m"},
        {144.0, 148.0, "2m"},
        {430.0, 440.0, "70cm"},
        {1240.0, 1300.0, "23cm"}
    };
    for (const BandRange &range : ranges) {
        if (mhz >= range.low && mhz <= range.high) {
            return QString::fromLatin1(range.name);
        }
    }
    return QString();
}

double MainWindow::ftStandardFrequencyHz(const QString &modeName, const QString &band) const
{
    const bool ft4 = Ft8Mode::profileForMode(modeName).shortLabel.compare(QStringLiteral("FT4"), Qt::CaseInsensitive) == 0 ||
                     modeName.compare(QStringLiteral("FT4"), Qt::CaseInsensitive) == 0;
    struct FtFreq { const char *band; double ft8Hz; double ft4Hz; };
    static const FtFreq table[] = {
        {"160m", 1840000.0, 0.0},
        {"80m", 3573000.0, 3575000.0},
        {"60m", 5357000.0, 0.0},
        {"40m", 7074000.0, 7047500.0},
        {"30m", 10136000.0, 10140000.0},
        {"20m", 14074000.0, 14080000.0},
        {"17m", 18100000.0, 18104000.0},
        {"15m", 21074000.0, 21140000.0},
        {"12m", 24915000.0, 24919000.0},
        {"10m", 28074000.0, 28180000.0},
        {"6m", 50313000.0, 50318000.0},
        {"2m", 144174000.0, 144170000.0},
        {"70cm", 432174000.0, 432174000.0},
        {"23cm", 1296174000.0, 1296174000.0}
    };
    const QString wanted = band.trimmed();
    for (const FtFreq &entry : table) {
        if (wanted.compare(QString::fromLatin1(entry.band), Qt::CaseInsensitive) == 0) {
            const double hz = ft4 && entry.ft4Hz > 0.0 ? entry.ft4Hz : entry.ft8Hz;
            return hz;
        }
    }
    return 0.0;
}

QString MainWindow::ftBandFromFrequencyHz(double frequencyHz) const
{
    return bandFromFrequencyHz(frequencyHz);
}

bool MainWindow::isFtFrequencyOnStandardSlot(double frequencyHz, const QString &modeName, const QString &band) const
{
    const double standard = ftStandardFrequencyHz(modeName, band);
    if (frequencyHz <= 0.0 || standard <= 0.0) {
        return false;
    }
    return qAbs(frequencyHz - standard) <= 1000.0;
}

void MainWindow::updateFtBandFrequencyUi()
{
    if (m_cmbFt8Band == nullptr) {
        return;
    }
    const QString modeName = (ui != nullptr && ui->cmbMode != nullptr) ? ui->cmbMode->currentText() : QStringLiteral("FT8");
    const QString band = m_cmbFt8Band->currentText();
    const double standard = ftStandardFrequencyHz(modeName, band);
    const bool haveCat = m_lastRigFrequencyHz > 0.0;
    const bool onSlot = haveCat && isFtFrequencyOnStandardSlot(m_lastRigFrequencyHz, modeName, band);

    if (m_lblFt8BandStatus != nullptr) {
        const QString stdText = standard > 0.0 ? formatRigFrequency(standard) : QStringLiteral("--");
        QString text = uiText("ft_standard_frequency", "Standard FT frequency") + QStringLiteral(": ") + stdText;
        if (haveCat) {
            text += QStringLiteral(" — CAT: ") + formatRigFrequency(m_lastRigFrequencyHz);
            if (!onSlot) {
                text += QStringLiteral(" — ") + uiText("off_ft_standard_frequency", "off standard FT frequency");
            }
        }
        m_lblFt8BandStatus->setText(text);
        m_lblFt8BandStatus->setStyleSheet(onSlot || !haveCat
            ? QStringLiteral("font-weight: 500; color: #15803d;")
            : QStringLiteral("font-weight: 500; color: #b00020;"));
    }
    m_cmbFt8Band->setStyleSheet(haveCat && !onSlot ? QStringLiteral("color: #b00020; font-weight: 500;") : QString());
}

void MainWindow::qsyRigToSelectedFtBand()
{
    if (m_cmbFt8Band == nullptr || m_rigController == nullptr) {
        return;
    }
    const QString modeName = (ui != nullptr && ui->cmbMode != nullptr) ? ui->cmbMode->currentText() : QStringLiteral("FT8");
    const QString band = m_cmbFt8Band->currentText();
    const double targetHz = ftStandardFrequencyHz(modeName, band);
    if (targetHz <= 0.0) {
        appendLog(QStringLiteral("%1: no standard FT frequency configured for %2.").arg(modeName, band));
        return;
    }
    if (!m_settings.hamlibCatEnabled) {
        appendLog(QStringLiteral("%1 band %2 selected; CAT is disabled, rig not tuned.").arg(modeName, band));
        return;
    }
    invokeRigSetFrequency(targetHz);
    appendLog(QStringLiteral("%1 QSY requested: %2 -> %3.").arg(modeName, band, formatRigFrequency(targetHz)));
}



void MainWindow::setupBandSchedulerTab()
{
    if (ui == nullptr || ui->tabModeSettings == nullptr || ui->modeTabLayout == nullptr ||
        m_tabBandScheduler != nullptr) {
        return;
    }

    QWidget *schedulerBox = new QWidget(ui->tabModeSettings);
    m_tabBandScheduler = schedulerBox;

    QVBoxLayout *layout = new QVBoxLayout(schedulerBox);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(4);

    m_lblBandSchedulerMode = new QLabel(schedulerBox);
    m_lblBandSchedulerMode->setWordWrap(true);
    layout->addWidget(m_lblBandSchedulerMode);

    QHBoxLayout *controls = new QHBoxLayout();
    controls->setContentsMargins(0, 0, 0, 0);
    controls->setSpacing(6);

    m_chkBandSchedulerEnabled = new QCheckBox(schedulerBox);
    controls->addWidget(m_chkBandSchedulerEnabled, 1);

    m_btnBandSchedulerSettings = new QPushButton(schedulerBox);
    m_btnBandSchedulerSettings->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    controls->addWidget(m_btnBandSchedulerSettings, 0);

    layout->addLayout(controls);

    m_lblBandSchedulerHint = nullptr;

    ui->modeTabLayout->addWidget(m_tabBandScheduler, 0);

    connect(m_chkBandSchedulerEnabled, &QCheckBox::toggled, this, [this](bool checked) {
        setBandSchedulerEnabledForMode(activeSchedulerModeGroup(), checked);
    });
    connect(m_btnBandSchedulerSettings, &QPushButton::clicked, this, &MainWindow::showBandSchedulerDialog);

    const QString modeName = (ui->cmbMode != nullptr) ? ui->cmbMode->currentText() : QString();
    updateBandSchedulerTabForMode(modeName);
}

void MainWindow::updateBandSchedulerTabForMode(const QString &modeName)
{
    const QString currentMode = modeName.trimmed().isEmpty() && ui != nullptr && ui->cmbMode != nullptr
                                ? ui->cmbMode->currentText()
                                : modeName.trimmed();
    const QString group = schedulerModeGroup(currentMode);
    if (m_tabBandScheduler != nullptr) {
        m_tabBandScheduler->setVisible(!group.isEmpty());
    }
    if (group.isEmpty()) {
        return;
    }
    const QString display = group == QStringLiteral("WEFAX")
                            ? uiText("scheduler_wefax_group", "MeteoFax / HF WEFAX RX")
                            : group;

    const bool enabledForMode = isBandSchedulerEnabledForMode(group);
    QString statusText;
    if (group.isEmpty()) {
        statusText = uiText("scheduler_status_no_mode", "Scheduler: no valid mode selected.");
    } else if (m_hasPendingScheduledQsy) {
        statusText = uiText("scheduler_status_pending", "Scheduler: pending QSY to %1.")
                         .arg(scheduledQsyDescription(m_pendingScheduledQsy));
    } else if (enabledForMode) {
        statusText = uiText("scheduler_status_enabled", "Scheduler: enabled for %1.").arg(display);
    } else {
        statusText = uiText("scheduler_status_disabled", "Scheduler: disabled for %1.").arg(display);
    }

    if (m_lblBandSchedulerMode != nullptr) {
        m_lblBandSchedulerMode->setText(statusText);
    }
    if (m_chkBandSchedulerEnabled != nullptr) {
        const QSignalBlocker blocker(m_chkBandSchedulerEnabled);
        m_chkBandSchedulerEnabled->setText(uiText("scheduler_enable_for_mode", "Enable for current mode"));
        m_chkBandSchedulerEnabled->setToolTip(uiText("scheduler_enable_tooltip", "When enabled, MM applies the daily UTC QSY plan for this mode. It changes only rig frequency/band and never starts TX."));
        m_chkBandSchedulerEnabled->setChecked(enabledForMode);
        m_chkBandSchedulerEnabled->setEnabled(!group.isEmpty());
    }
    if (m_btnBandSchedulerSettings != nullptr) {
        m_btnBandSchedulerSettings->setText(uiText("scheduler_settings_button", "Settings..."));
        m_btnBandSchedulerSettings->setToolTip(uiText("scheduler_settings_tooltip", "Open the daily UTC band/frequency plan. FT4/FT8 entries can use the standard frequencies for the selected band."));
    }
    if (m_lblBandSchedulerHint != nullptr) {
        m_lblBandSchedulerHint->setText(uiText("scheduler_safe_qsy_hint", "Scheduled QSY waits for TX/QSO to finish. Evil Auto CQ and Auto QSO remain enabled."));
    }
}


void MainWindow::setupDspTab()
{
    // Removed by design: the generic DSP tab exposed mostly ineffective controls.
    // Keep useful RX conditioning in the relevant Mode pages instead.
}


void MainWindow::updateDspTabForMode(const QString &modeName)
{
    if (m_tabDspSettings == nullptr || ui == nullptr || ui->sideTabWidget == nullptr) {
        return;
    }

    const QString key = dspModeKeyForModeName(modeName);
    const bool ftMode = Ft8Mode::isFamilyMode(modeName);
    const bool showTab = !ftMode && !key.isEmpty();
    const int tabIndex = ui->sideTabWidget->indexOf(m_tabDspSettings);
    if (tabIndex >= 0) {
        ui->sideTabWidget->tabBar()->setTabVisible(tabIndex, showTab);
        if (!showTab && ui->sideTabWidget->currentIndex() == tabIndex) {
            const int modeIndex = ui->tabModeSettings != nullptr
                ? ui->sideTabWidget->indexOf(ui->tabModeSettings)
                : -1;
            ui->sideTabWidget->setCurrentIndex(modeIndex >= 0 ? modeIndex : 0);
        }
    }
    if (!showTab) {
        return;
    }

    if (m_grpDspCore != nullptr) m_grpDspCore->setTitle(uiText("dsp_core_group", "Common RX conditioning"));
    if (m_grpDspRtty != nullptr) m_grpDspRtty->setTitle(uiText("dsp_rtty_group", "RTTY contest DSP"));
    if (m_grpDspBpsk != nullptr) m_grpDspBpsk->setTitle(uiText("dsp_psk_group", "PSK coherent tracking"));
    if (m_grpDspImage != nullptr) m_grpDspImage->setTitle(uiText("dsp_image_group", "Image-mode denoise"));
    if (m_chkDspSoftwareAgc != nullptr) m_chkDspSoftwareAgc->setText(uiText("dsp_software_agc", "Software AGC"));
    if (m_chkDspNoiseReduction != nullptr) m_chkDspNoiseReduction->setText(uiText("dsp_noise_reduction", "Noise reduction"));
    if (m_chkDspAdaptiveLineEnhancer != nullptr) m_chkDspAdaptiveLineEnhancer->setText(uiText("dsp_adaptive_line_enhancer", "Adaptive line enhancer (LMS)"));
    if (m_chkDspRttyMatchedFilter != nullptr) m_chkDspRttyMatchedFilter->setText(uiText("dsp_rtty_matched_filter", "Matched Mark/Space filters"));
    if (m_chkDspRttyMarkSpaceEnhancer != nullptr) m_chkDspRttyMarkSpaceEnhancer->setText(uiText("dsp_rtty_mark_space_enhancer", "Adaptive Mark/Space enhancer"));
    if (m_chkRttyMultiDecode != nullptr) m_chkRttyMultiDecode->setText(uiText("rtty_multi_decode", "Multi-decode RTTY waterfall"));
    if (m_chkRttyOverlayCallsigns != nullptr) m_chkRttyOverlayCallsigns->setText(uiText("rtty_overlay_callsigns", "Show CQ/callsign labels on waterfall"));
    if (m_chkRttyContestEnhanced != nullptr) m_chkRttyContestEnhanced->setText(uiText("rtty_contest_enhanced", "Contest enhanced RX"));
    if (m_chkRttySecondPass != nullptr) m_chkRttySecondPass->setText(uiText("rtty_second_pass", "Second-pass strong-signal scan"));
    if (m_chkDspBpskCoherentTracking != nullptr) m_chkDspBpskCoherentTracking->setText(uiText("dsp_bpsk_coherent_tracking", "Costas + timing tracking"));
    if (m_chkDspImageWaveletDenoise != nullptr) m_chkDspImageWaveletDenoise->setText(uiText("dsp_wavelet_denoise", "Wavelet-style soft denoise"));

    const bool rttyMode = key == QStringLiteral("RTTY");
    const bool bpskMode = key == QStringLiteral("BPSK31");
    const bool cwMode = key == QStringLiteral("CW");
    const bool imageMode = key == QStringLiteral("WEFAX") || key == QStringLiteral("SSTV");
    const bool aleAvailable = cwMode || rttyMode;

    if (m_lblDspMode != nullptr) {
        m_lblDspMode->setText(uiText("dsp_current_mode", "DSP options for: %1").arg(modeName));
    }
    if (m_grpDspRtty != nullptr) m_grpDspRtty->setVisible(rttyMode);
    if (m_grpDspBpsk != nullptr) m_grpDspBpsk->setVisible(bpskMode);
    if (m_grpDspImage != nullptr) m_grpDspImage->setVisible(imageMode);
    if (m_chkDspAdaptiveLineEnhancer != nullptr) m_chkDspAdaptiveLineEnhancer->setVisible(aleAvailable);

    const QSignalBlocker b0(m_chkDspSoftwareAgc);
    const QSignalBlocker b1(m_chkDspNoiseReduction);
    const QSignalBlocker b2(m_chkDspAdaptiveLineEnhancer);
    const QSignalBlocker b3(m_chkDspRttyMatchedFilter);
    const QSignalBlocker b4(m_chkDspRttyMarkSpaceEnhancer);
    const QSignalBlocker b5(m_chkRttyMultiDecode);
    const QSignalBlocker b6(m_chkRttyOverlayCallsigns);
    const QSignalBlocker b7(m_chkRttyContestEnhanced);
    const QSignalBlocker b8(m_chkRttySecondPass);
    const QSignalBlocker b9(m_spinRttyMaxDecoders);
    const QSignalBlocker b10(m_chkDspBpskCoherentTracking);
    const QSignalBlocker b11(m_chkDspImageWaveletDenoise);

    if (m_chkDspSoftwareAgc != nullptr) m_chkDspSoftwareAgc->setChecked(dspAgcEnabledForModeKey(key));
    if (m_chkDspNoiseReduction != nullptr) m_chkDspNoiseReduction->setChecked(dspNoiseReductionEnabledForModeKey(key));
    if (m_chkDspAdaptiveLineEnhancer != nullptr) {
        m_chkDspAdaptiveLineEnhancer->setChecked(cwMode ? m_settings.cwAdaptiveLineEnhancerEnabled
                                                         : (rttyMode ? m_settings.rttyAdaptiveLineEnhancerEnabled : false));
    }
    if (m_chkDspRttyMatchedFilter != nullptr) m_chkDspRttyMatchedFilter->setChecked(m_settings.rttyMatchedFilterEnabled);
    if (m_chkDspRttyMarkSpaceEnhancer != nullptr) m_chkDspRttyMarkSpaceEnhancer->setChecked(m_settings.rttyMarkSpaceEnhancerEnabled);
    if (m_chkRttyMultiDecode != nullptr) m_chkRttyMultiDecode->setChecked(m_settings.rttyMultiDecodeEnabled);
    if (m_chkRttyOverlayCallsigns != nullptr) m_chkRttyOverlayCallsigns->setChecked(m_settings.rttyOverlayCallsignsEnabled);
    if (m_chkRttyContestEnhanced != nullptr) m_chkRttyContestEnhanced->setChecked(m_settings.rttyContestEnhancedEnabled);
    if (m_chkRttySecondPass != nullptr) m_chkRttySecondPass->setChecked(m_settings.rttySecondPassEnabled);
    if (m_spinRttyMaxDecoders != nullptr) m_spinRttyMaxDecoders->setValue(qBound(2, m_settings.rttyMaxParallelDecoders, 32));
    if (m_chkDspBpskCoherentTracking != nullptr) m_chkDspBpskCoherentTracking->setChecked(m_settings.bpsk31CoherentTrackingEnabled);
    if (m_chkDspImageWaveletDenoise != nullptr) {
        m_chkDspImageWaveletDenoise->setChecked(key == QStringLiteral("WEFAX") ? m_settings.weatherFaxWaveletDenoiseEnabled
                                                                               : m_settings.sstvWaveletDenoiseEnabled);
    }

    const bool multi = m_settings.rttyMultiDecodeEnabled;
    if (m_chkRttyOverlayCallsigns != nullptr) m_chkRttyOverlayCallsigns->setEnabled(multi);
    if (m_chkRttyContestEnhanced != nullptr) m_chkRttyContestEnhanced->setEnabled(multi);
    if (m_chkRttySecondPass != nullptr) m_chkRttySecondPass->setEnabled(multi);
    if (m_spinRttyMaxDecoders != nullptr) m_spinRttyMaxDecoders->setEnabled(multi);
}


QString MainWindow::dspModeKeyForModeName(const QString &modeName) const
{
    const QString mode = modeName.trimmed();
    if (mode == WeatherFaxDecoder::modeName()) return QStringLiteral("WEFAX");
    if (mode == SstvModeDefinition::modeName() || mode == SstvDecoder::modeName()) return QStringLiteral("SSTV");
    if (mode == RttyDecoder::modeName()) return QStringLiteral("RTTY");
    if (mode == Bpsk31Decoder::modeName()) return QStringLiteral("BPSK31");
    if (mode == MfskDecoder::modeName()) return QStringLiteral("MFSK");
    if (mode == CwDecoder::modeName()) return QStringLiteral("CW");
    if (mode == HellschreiberDecoder::modeName()) return QStringLiteral("HELL");
    return QString();
}

bool MainWindow::dspNoiseReductionEnabledForModeKey(const QString &modeKey) const
{
    const QString key = modeKey.trimmed().toUpper();
    if (key == QStringLiteral("WEFAX")) return m_settings.weatherFaxNoiseReductionEnabled;
    if (key == QStringLiteral("SSTV")) return m_settings.sstvNoiseReductionEnabled;
    if (key == QStringLiteral("RTTY")) return m_settings.rttyNoiseReductionEnabled;
    if (key == QStringLiteral("BPSK31")) return m_settings.bpsk31NoiseReductionEnabled;
    if (key == QStringLiteral("MFSK")) return m_settings.mfskNoiseReductionEnabled;
    if (key == QStringLiteral("CW")) return m_settings.cwNoiseReductionEnabled;
    if (key == QStringLiteral("HELL")) return m_settings.hellNoiseReductionEnabled;
    return false;
}

bool MainWindow::dspAgcEnabledForModeKey(const QString &modeKey) const
{
    const QString key = modeKey.trimmed().toUpper();
    if (key == QStringLiteral("WEFAX")) return m_settings.weatherFaxAgcEnabled;
    if (key == QStringLiteral("SSTV")) return m_settings.sstvAgcEnabled;
    if (key == QStringLiteral("RTTY")) return m_settings.rttyAgcEnabled;
    if (key == QStringLiteral("BPSK31")) return m_settings.bpsk31AgcEnabled;
    if (key == QStringLiteral("MFSK")) return m_settings.mfskAgcEnabled;
    if (key == QStringLiteral("CW")) return m_settings.cwAgcEnabled;
    if (key == QStringLiteral("HELL")) return m_settings.hellAgcEnabled;
    return false;
}


void MainWindow::setDspNoiseReductionEnabledForModeKey(const QString &modeKey, bool enabled)
{
    const QString key = modeKey.trimmed().toUpper();
    if (key == QStringLiteral("WEFAX")) m_settings.weatherFaxNoiseReductionEnabled = enabled;
    else if (key == QStringLiteral("SSTV")) m_settings.sstvNoiseReductionEnabled = enabled;
    else if (key == QStringLiteral("RTTY")) m_settings.rttyNoiseReductionEnabled = enabled;
    else if (key == QStringLiteral("BPSK31")) m_settings.bpsk31NoiseReductionEnabled = enabled;
    else if (key == QStringLiteral("MFSK")) m_settings.mfskNoiseReductionEnabled = enabled;
    else if (key == QStringLiteral("CW")) m_settings.cwNoiseReductionEnabled = enabled;
    else if (key == QStringLiteral("HELL")) m_settings.hellNoiseReductionEnabled = enabled;

    for (QCheckBox *check : m_dspNoiseReductionChecks.value(key)) {
        if (check != nullptr && check->isChecked() != enabled) {
            const QSignalBlocker blocker(check);
            check->setChecked(enabled);
        }
    }
}

void MainWindow::setDspAgcEnabledForModeKey(const QString &modeKey, bool enabled)
{
    const QString key = modeKey.trimmed().toUpper();
    if (key == QStringLiteral("WEFAX")) m_settings.weatherFaxAgcEnabled = enabled;
    else if (key == QStringLiteral("SSTV")) m_settings.sstvAgcEnabled = enabled;
    else if (key == QStringLiteral("RTTY")) m_settings.rttyAgcEnabled = enabled;
    else if (key == QStringLiteral("BPSK31")) m_settings.bpsk31AgcEnabled = enabled;
    else if (key == QStringLiteral("MFSK")) m_settings.mfskAgcEnabled = enabled;
    else if (key == QStringLiteral("CW")) m_settings.cwAgcEnabled = enabled;
    else if (key == QStringLiteral("HELL")) m_settings.hellAgcEnabled = enabled;

    for (QCheckBox *check : m_dspAgcChecks.value(key)) {
        if (check != nullptr && check->isChecked() != enabled) {
            const QSignalBlocker blocker(check);
            check->setChecked(enabled);
        }
    }
}

QString MainWindow::schedulerModeGroup(const QString &modeName) const
{
    const QString mode = modeName.trimmed();
    if (mode.compare(QStringLiteral("WEFAX"), Qt::CaseInsensitive) == 0 ||
        mode.compare(QStringLiteral("MeteoFax / HF WEFAX"), Qt::CaseInsensitive) == 0 ||
        mode.compare(QStringLiteral("MeteoFax / HF WEFAX RX"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("WEFAX");
    }
    if (mode.compare(QStringLiteral("RTTY"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("RTTY");
    }
    return QString();
}

QString MainWindow::activeSchedulerModeGroup() const
{
    if (ui == nullptr || ui->cmbMode == nullptr) {
        return QString();
    }
    return schedulerModeGroup(ui->cmbMode->currentText());
}

bool MainWindow::isBandSchedulerEnabledForMode(const QString &modeName) const
{
    const QString group = schedulerModeGroup(modeName);
    for (const QString &enabled : m_settings.schedulerQsyEnabledModes) {
        if (schedulerModeGroup(enabled).compare(group, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

void MainWindow::setBandSchedulerEnabledForMode(const QString &modeName, bool enabled)
{
    const QString group = schedulerModeGroup(modeName);
    QStringList normalized;
    for (const QString &m : m_settings.schedulerQsyEnabledModes) {
        const QString g = schedulerModeGroup(m);
        if (!g.isEmpty() && g.compare(group, Qt::CaseInsensitive) != 0 && !normalized.contains(g)) {
            normalized.append(g);
        }
    }
    if (enabled && !group.isEmpty() && !normalized.contains(group)) {
        normalized.append(group);
    }
    m_settings.schedulerQsyEnabledModes = normalized;
    savePersistentSettings();

    if (m_chkBandSchedulerEnabled != nullptr && activeSchedulerModeGroup().compare(group, Qt::CaseInsensitive) == 0) {
        const QSignalBlocker block(m_chkBandSchedulerEnabled);
        m_chkBandSchedulerEnabled->setChecked(enabled);
    }

    updateBandSchedulerTabForMode(ui != nullptr && ui->cmbMode != nullptr ? ui->cmbMode->currentText() : QString());
    appendLog(QStringLiteral("Scheduler %1 for %2.").arg(enabled ? QStringLiteral("enabled") : QStringLiteral("disabled"), group));
}

QList<ScheduledQsyEntry> MainWindow::schedulerEntries() const
{
    return BandSchedulerDialog::deserializeEntries(m_settings.schedulerQsyPlanJson);
}

bool MainWindow::shouldRunScheduledQsyForMode(const QString &modeName) const
{
    const QString entryGroup = schedulerModeGroup(modeName);
    const QString currentGroup = activeSchedulerModeGroup();
    if (entryGroup.isEmpty() || currentGroup.isEmpty() ||
        entryGroup.compare(currentGroup, Qt::CaseInsensitive) != 0) {
        return false;
    }
    return isBandSchedulerEnabledForMode(entryGroup);
}

QString MainWindow::scheduledQsyDescription(const ScheduledQsyEntry &entry) const
{
    return QStringLiteral("%1 %2 %3").arg(entry.mode.trimmed(),
                                          entry.band.trimmed(),
                                          formatRigFrequency(entry.frequencyHz));
}

bool MainWindow::canApplyScheduledQsyNow() const
{
    if (m_txRunning || m_ftTxWorkerRunning || m_lastRigPttOn || m_ft8PendingTxArmed || m_pendingFt8PttKeyed) {
        return false;
    }
    if (m_ftSession.qsoActive) {
        return false;
    }
    return true;
}

void MainWindow::requestScheduledQsy(const ScheduledQsyEntry &entry, const QString &reason)
{
    if (entry.frequencyHz <= 0.0) {
        return;
    }
    if (!m_settings.hamlibCatEnabled) {
        appendLog(QStringLiteral("Scheduled QSY skipped: CAT is disabled (%1).").arg(scheduledQsyDescription(entry)));
        return;
    }

    m_pendingScheduledQsy = entry;
    m_hasPendingScheduledQsy = true;
    m_pendingScheduledQsyReason = reason;

    if (canApplyScheduledQsyNow()) {
        applyScheduledQsy(m_pendingScheduledQsy);
        m_hasPendingScheduledQsy = false;
        m_pendingScheduledQsyReason.clear();
    } else {
        appendLog(QStringLiteral("Scheduled QSY pending: %1 — waiting for TX/QSO end.").arg(scheduledQsyDescription(entry)));
    }
}

void MainWindow::applyScheduledQsy(const ScheduledQsyEntry &entry)
{
    const QString targetMode = entry.mode.trimmed();
    if (ui != nullptr && ui->cmbMode != nullptr && !targetMode.isEmpty() &&
        ui->cmbMode->findText(targetMode) >= 0 && ui->cmbMode->currentText() != targetMode) {
        ui->cmbMode->setCurrentText(targetMode);
    }

    if (Ft8Mode::isFamilyMode(targetMode) && m_cmbFt8Band != nullptr && !entry.band.isEmpty()) {
        const int idx = m_cmbFt8Band->findText(entry.band, Qt::MatchFixedString);
        if (idx >= 0) {
            const QSignalBlocker block(m_cmbFt8Band);
            m_cmbFt8Band->setCurrentIndex(idx);
            m_settings.ft8Band = entry.band;
            refreshFt8StandardMessages();
            updateFtBandFrequencyUi();
        }
    }

    invokeRigSetFrequency(entry.frequencyHz);
    appendLog(QStringLiteral("Scheduled QSY applied: %1.").arg(scheduledQsyDescription(entry)));
}

void MainWindow::handleBandSchedulerTick()
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const QDate today = now.date();

    if (m_hasPendingScheduledQsy) {
        if (canApplyScheduledQsyNow()) {
            applyScheduledQsy(m_pendingScheduledQsy);
            m_hasPendingScheduledQsy = false;
            m_pendingScheduledQsyReason.clear();
        }
        updateBandSchedulerTabForMode(ui != nullptr && ui->cmbMode != nullptr ? ui->cmbMode->currentText() : QString());
        return;
    }

    // Check the whole current minute. The per-day trigger key prevents repeats,
    // while this avoids missing a scheduled QSY if the UI thread is briefly busy
    // at second 0.
    const QTime minute(now.time().hour(), now.time().minute());
    const QList<ScheduledQsyEntry> entries = schedulerEntries();
    for (const ScheduledQsyEntry &entry : entries) {
        if (!entry.enabled || !entry.timeUtc.isValid() || entry.timeUtc != minute) {
            continue;
        }
        if (!shouldRunScheduledQsyForMode(entry.mode)) {
            continue;
        }
        const QString key = entry.triggerKey(today);
        if (m_bandSchedulerTriggeredKeys.contains(key)) {
            continue;
        }
        m_bandSchedulerTriggeredKeys.insert(key);
        requestScheduledQsy(entry, QStringLiteral("daily UTC plan"));
        break;
    }

    updateBandSchedulerTabForMode(ui != nullptr && ui->cmbMode != nullptr ? ui->cmbMode->currentText() : QString());

    if (now.time().hour() == 0 && now.time().minute() == 0 && now.time().second() < 5) {
        const QString todayPrefix = today.toString(Qt::ISODate);
        QSet<QString> kept;
        for (const QString &key : m_bandSchedulerTriggeredKeys) {
            if (key.startsWith(todayPrefix)) {
                kept.insert(key);
            }
        }
        m_bandSchedulerTriggeredKeys = kept;
    }
}

void MainWindow::showBandSchedulerDialog()
{
    showAppSettingsDialogPage(AppSettingsDialog::InitialPage::Scheduler);
}

QString MainWindow::formatRigFrequency(double frequencyHz) const
{
    if (frequencyHz <= 0.0) {
        return QStringLiteral("--");
    }
    if (frequencyHz >= 1000000.0) {
        return QStringLiteral("%1 MHz").arg(frequencyHz / 1000000.0, 0, 'f', 6);
    }
    if (frequencyHz >= 1000.0) {
        return QStringLiteral("%1 kHz").arg(frequencyHz / 1000.0, 0, 'f', 3);
    }
    return QStringLiteral("%1 Hz").arg(frequencyHz, 0, 'f', 0);
}


void MainWindow::savePersistentSettings()
{
    if (!m_settings.save()) {
        appendLog("Settings save failed: " + AppSettings::settingsFilePath());
    }
}

QString MainWindow::defaultWeatherFaxOutputFolder() const
{
    return QDir::home().filePath("WeatherFAX");
}

QString MainWindow::makeWeatherFaxAutoSaveFileName() const
{
    const QString folder = ui->editFaxOutputFolder->text().trimmed().isEmpty()
                               ? defaultWeatherFaxOutputFolder()
                               : ui->editFaxOutputFolder->text().trimmed();

    const QString baseName =
        "WeatherFAX_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");

    QDir dir(folder);
    QString fileName = dir.filePath(baseName + ".png");

    int suffix = 1;
    while (QFile::exists(fileName)) {
        fileName = dir.filePath(QString("%1_%2.png").arg(baseName).arg(suffix));
        ++suffix;
    }

    return fileName;
}

bool MainWindow::saveWeatherFaxImageToFile(const QImage &image, const QString &fileName)
{
    if (image.isNull()) {
        appendLog("Save PNG failed: no MeteoFax image available.");
        return false;
    }

    QFileInfo fileInfo(fileName);
    QDir dir = fileInfo.dir();

    if (!dir.exists() && !dir.mkpath(".")) {
        appendLog("Save PNG failed: unable to create folder " + dir.absolutePath());
        return false;
    }

    if (!image.save(fileName, "PNG")) {
        appendLog("Save PNG failed: " + fileName);
        return false;
    }

    appendLog("MeteoFax image saved: " + fileName);
    return true;
}

void MainWindow::updateLinePresetSelectionFromCurrentValues()
{
    const QString currentKey = ui->cmbFaxLinePreset->currentData().toString();

    if (currentKey == "CUSTOM") {
        return;
    }

    const FaxLinePreset preset = presetByKey(currentKey);

    if (preset.key == "CUSTOM") {
        return;
    }

    const int currentLpm = ui->cmbFaxLpm->currentText().toInt();
    const int currentLines = ui->spinFaxImageLines->value();
    const int currentBlackHz = ui->spinFaxBlackHz->value();
    const int currentWhiteHz = ui->spinFaxWhiteHz->value();

    if (preset.lpm == currentLpm &&
        preset.lines == currentLines &&
        preset.blackHz == currentBlackHz &&
        preset.whiteHz == currentWhiteHz) {
        return;
    }

    const QSignalBlocker blockPreset(ui->cmbFaxLinePreset);
    ui->cmbFaxLinePreset->setCurrentIndex(ui->cmbFaxLinePreset->findData("CUSTOM"));
}

void MainWindow::selectComboByBackendName(QComboBox *combo, const QString &backendName)
{
    if (combo == nullptr || backendName.isEmpty()) {
        return;
    }

    const int index = combo->findData(backendName);

    if (index >= 0) {
        combo->setCurrentIndex(index);
    }
}

void MainWindow::setReceiverRunning(bool running)
{
    m_rxRunning = running;

    if (!m_rxRunning && !m_txRunning && m_rttyScopeWidget != nullptr) {
        m_rttyScopeWidget->setTrace(QVector<QPointF>(), 0.0, false);
    }

    const bool idle = !m_rxRunning && !m_txRunning && !m_offlineAnalysisActive;
    const bool configurable = !m_txRunning && !m_offlineAnalysisActive;

    ui->btnStartRx->setEnabled(!m_txRunning && !m_offlineAnalysisActive);
    ui->btnStopRx->setEnabled(m_rxRunning);

    ui->cmbMode->setEnabled(idle);

    ui->btnTxTone->setEnabled(idle);
    ui->btnTestPtt->setEnabled(idle);

    ui->cmbFaxLpm->setEnabled(configurable);
    ui->spinFaxBlackHz->setEnabled(configurable);
    ui->spinFaxWhiteHz->setEnabled(configurable);
    ui->chkFaxAutoStartPhasing->setEnabled(configurable);
    ui->chkFaxAutoToneTracking->setEnabled(configurable);
    ui->chkFaxInputBandpass->setEnabled(configurable);
    ui->chkFaxAutoSlant->setEnabled(false);
    ui->spinFaxSlantPpm->setEnabled(false);
    ui->cmbFaxLinePreset->setEnabled(configurable);
    ui->spinFaxImageLines->setEnabled(false);
    ui->chkFaxEndOfSignal->setEnabled(configurable);
    ui->spinFaxEndTimeoutSec->setEnabled(configurable);
    ui->chkFaxAutoSave->setEnabled(configurable);
    ui->editFaxOutputFolder->setEnabled(configurable);
    ui->btnFaxBrowseOutputFolder->setEnabled(configurable);
    ui->btnFaxAnalyzeWav->setEnabled(!m_txRunning && !m_offlineAnalysisActive);
    if (m_btnFaxForceRx != nullptr) m_btnFaxForceRx->setEnabled(!m_txRunning && !m_offlineAnalysisActive);
    ui->actionOpenWeatherFaxWav->setEnabled(!m_txRunning && !m_offlineAnalysisActive);
    ui->cmbSstvMode->setEnabled(configurable);
    ui->chkSstvAutoSync->setEnabled(configurable);
    ui->spinSstvHorizontalShift->setEnabled(configurable);
    ui->spinSstvRedShift->setEnabled(configurable);
    ui->spinSstvBlueShift->setEnabled(configurable);
    ui->btnSstvAnalyzeWav->setEnabled(!m_txRunning && !m_offlineAnalysisActive);
    if (m_btnSstvForceRx != nullptr) m_btnSstvForceRx->setEnabled(!m_txRunning && !m_offlineAnalysisActive);
    ui->btnSstvResetImage->setEnabled(configurable);
    ui->btnSstvSaveImage->setEnabled(configurable);
    ui->btnSstvZoomFit->setEnabled(true);
    ui->btnSstvZoom100->setEnabled(true);
    ui->btnSstvZoomOut->setEnabled(true);
    ui->btnSstvZoomIn->setEnabled(true);

    if (m_cmbRttyPreset != nullptr) {
        m_cmbRttyPreset->setEnabled(configurable);
        m_spinRttyBaud->setEnabled(configurable);
        m_spinRttyShiftHz->setEnabled(configurable);
        m_spinRttyMarkHz->setEnabled(configurable);
        m_chkRttyReverse->setEnabled(configurable);
        m_chkRttyAfc->setEnabled(!m_txRunning && !m_offlineAnalysisActive);
        m_spinRttyAfcRangeHz->setEnabled(!m_txRunning && !m_offlineAnalysisActive);
        if (m_chkRttyMultiDecode != nullptr) {
            const bool monitorConfigurable = !m_txRunning && !m_offlineAnalysisActive;
            const bool monitorEnabled = monitorConfigurable && m_chkRttyMultiDecode->isChecked();
            m_chkRttyMultiDecode->setEnabled(monitorConfigurable);
            if (m_chkRttyOverlayCallsigns != nullptr) m_chkRttyOverlayCallsigns->setEnabled(monitorEnabled);
            if (m_chkRttyContestEnhanced != nullptr) m_chkRttyContestEnhanced->setEnabled(monitorEnabled);
            if (m_chkRttySecondPass != nullptr) m_chkRttySecondPass->setEnabled(monitorEnabled);
            if (m_spinRttyMaxDecoders != nullptr) m_spinRttyMaxDecoders->setEnabled(monitorEnabled);
        }
        m_txtRttyTx->setEnabled(!m_offlineAnalysisActive);
        m_txtRttyTx->setReadOnly(m_txRunning || m_offlineAnalysisActive);
        m_btnRttyLoadTxText->setEnabled(!m_txRunning && !m_offlineAnalysisActive);
        m_btnRttyClearTx->setEnabled(!m_txRunning && !m_offlineAnalysisActive);
        m_btnRttyClearRx->setEnabled(!m_txRunning && !m_offlineAnalysisActive);
    }

    if (m_spinBpsk31ToneHz != nullptr) {
        if (m_cmbBpsk31Variant != nullptr) {
            m_cmbBpsk31Variant->setEnabled(configurable);
        }
        m_spinBpsk31ToneHz->setEnabled(configurable);
        m_chkBpsk31Afc->setEnabled(!m_txRunning && !m_offlineAnalysisActive);
        m_spinBpsk31AfcRangeHz->setEnabled(!m_txRunning && !m_offlineAnalysisActive);
        m_chkBpsk31Invert->setEnabled(configurable);
        m_txtBpsk31Tx->setEnabled(!m_offlineAnalysisActive);
        m_txtBpsk31Tx->setReadOnly(m_txRunning || m_offlineAnalysisActive);
        m_btnBpsk31LoadTxText->setEnabled(!m_txRunning && !m_offlineAnalysisActive);
        m_btnBpsk31ClearTx->setEnabled(!m_txRunning && !m_offlineAnalysisActive);
        m_btnBpsk31ClearRx->setEnabled(!m_txRunning && !m_offlineAnalysisActive);
    }

    if (m_spinMfskCenterHz != nullptr) {
        if (m_cmbMfskVariant != nullptr) {
            m_cmbMfskVariant->setEnabled(configurable);
        }
        m_spinMfskCenterHz->setEnabled(configurable);
        m_chkMfskAfc->setEnabled(!m_txRunning && !m_offlineAnalysisActive);
        m_spinMfskAfcRangeHz->setEnabled(!m_txRunning && !m_offlineAnalysisActive);
        m_txtMfskTx->setEnabled(!m_offlineAnalysisActive);
        m_txtMfskTx->setReadOnly(m_txRunning || m_offlineAnalysisActive);
        m_btnMfskLoadTxText->setEnabled(!m_txRunning && !m_offlineAnalysisActive);
        m_btnMfskClearTx->setEnabled(!m_txRunning && !m_offlineAnalysisActive);
        m_btnMfskClearRx->setEnabled(!m_txRunning && !m_offlineAnalysisActive);
    }

    if (m_spinCwToneHz != nullptr) {
        m_spinCwToneHz->setEnabled(configurable);
        m_spinCwWpm->setEnabled(configurable);
        m_spinCwBandwidthHz->setEnabled(configurable);
        m_chkCwAfc->setEnabled(!m_txRunning && !m_offlineAnalysisActive);
        m_spinCwAfcRangeHz->setEnabled(!m_txRunning && !m_offlineAnalysisActive);
        if (m_chkCwSoftwareAgc != nullptr) m_chkCwSoftwareAgc->setEnabled(!m_txRunning && !m_offlineAnalysisActive);
        m_btnCwClearRx->setEnabled(!m_txRunning && !m_offlineAnalysisActive);
        if (m_txtCwTx != nullptr) {
            m_txtCwTx->setEnabled(!m_offlineAnalysisActive);
            m_txtCwTx->setReadOnly(m_txRunning || m_offlineAnalysisActive);
        }
        if (m_btnCwLoadTxText != nullptr) m_btnCwLoadTxText->setEnabled(!m_txRunning && !m_offlineAnalysisActive);
        if (m_btnCwClearTx != nullptr) m_btnCwClearTx->setEnabled(!m_txRunning && !m_offlineAnalysisActive);
    }

    if (m_spinHellToneHz != nullptr) {
        if (m_cmbHellVariant != nullptr) {
            m_cmbHellVariant->setEnabled(configurable);
        }
        m_spinHellToneHz->setEnabled(configurable);
        m_spinHellColumnRate->setEnabled(configurable);
        m_spinHellBandwidthHz->setEnabled(configurable);
        if (m_sliderHellPaperScale != nullptr) {
            m_sliderHellPaperScale->setEnabled(!m_offlineAnalysisActive);
        }
        m_chkHellAfc->setEnabled(!m_txRunning && !m_offlineAnalysisActive);
        m_spinHellAfcRangeHz->setEnabled(!m_txRunning && !m_offlineAnalysisActive);
        m_txtHellTx->setEnabled(!m_offlineAnalysisActive);
        m_txtHellTx->setReadOnly(m_txRunning || m_offlineAnalysisActive);
        m_btnHellLoadTxText->setEnabled(!m_txRunning && !m_offlineAnalysisActive);
        m_btnHellClearTx->setEnabled(!m_txRunning && !m_offlineAnalysisActive);
        m_btnHellResetImage->setEnabled(!m_txRunning && !m_offlineAnalysisActive);
    }



    if (m_spinFt8RxFreq != nullptr) {
        const bool ft8Pending = m_ft8PendingTxArmed;
        const bool ft8TuneActive = m_pendingFt8Tune ||
                                   (m_ftSession.lastTxWasTune && (m_txRunning || m_ftTxWorkerRunning)) ||
                                   (m_pendingFt8PttKeyed && !m_txRunning && !m_ftTxWorkerRunning);
        m_spinFt8RxFreq->setEnabled(configurable && !ft8Pending);
        m_spinFt8TxFreq->setEnabled(configurable && !ft8Pending);
        if (m_editFt8DxCall != nullptr) m_editFt8DxCall->setEnabled(!m_offlineAnalysisActive && !ft8Pending);
        if (m_editFt8DxGrid != nullptr) m_editFt8DxGrid->setEnabled(!m_offlineAnalysisActive && !ft8Pending);
        if (m_cmbFt8Band != nullptr) m_cmbFt8Band->setEnabled(!m_offlineAnalysisActive && !ft8Pending);
        if (m_chkFt8HoldTxFreq != nullptr) m_chkFt8HoldTxFreq->setEnabled(!m_offlineAnalysisActive && !ft8Pending);
        if (m_cmbFt8TxStrategy != nullptr) m_cmbFt8TxStrategy->setEnabled(!m_offlineAnalysisActive && !ft8Pending);
        if (m_radioFt8TxFirst != nullptr) m_radioFt8TxFirst->setEnabled(!m_txRunning && !m_offlineAnalysisActive && !ft8Pending);
        if (m_radioFt8TxSecond != nullptr) m_radioFt8TxSecond->setEnabled(!m_txRunning && !m_offlineAnalysisActive && !ft8Pending);
        if (m_btnFt8GenerateStd != nullptr) m_btnFt8GenerateStd->setEnabled(!m_offlineAnalysisActive && !ft8Pending);
        if (m_btnFt8Rx != nullptr) m_btnFt8Rx->setEnabled(!m_txRunning && !m_offlineAnalysisActive && !ft8Pending);
        if (m_btnFt8Tx != nullptr) m_btnFt8Tx->setEnabled(!m_txRunning && !m_offlineAnalysisActive && !ft8Pending);
        if (m_btnFt8Stop != nullptr) m_btnFt8Stop->setEnabled((m_rxRunning || m_txRunning || ft8Pending || ft8TuneActive || m_ftSession.cqRepeatActive || m_ftSession.qsoActive) && !m_offlineAnalysisActive);
        if (m_btnFt8Tune != nullptr) {
            m_btnFt8Tune->setText(ft8TuneActive ? uiText("button.stopTune", "Stop Tune") : uiText("tune", "Tune"));
            m_btnFt8Tune->setEnabled(!m_offlineAnalysisActive && (!ft8Pending || ft8TuneActive));
        }
        if (m_chkFt8AutoSeq != nullptr) m_chkFt8AutoSeq->setEnabled(!m_offlineAnalysisActive);
        if (m_chkFt8CqRepeat != nullptr) m_chkFt8CqRepeat->setEnabled(!m_offlineAnalysisActive);
        if (m_chkFt8AutoLog != nullptr) m_chkFt8AutoLog->setEnabled(!m_offlineAnalysisActive);
        if (m_chkFt8FullAutoQso != nullptr) m_chkFt8FullAutoQso->setEnabled(!m_offlineAnalysisActive && m_ft8EvilModeUnlocked);
        if (m_chkFt8EvilMode != nullptr) m_chkFt8EvilMode->setEnabled(!m_offlineAnalysisActive);
        if (m_spinFt8CqTimeoutMin != nullptr) m_spinFt8CqTimeoutMin->setEnabled(!m_offlineAnalysisActive);
        if (m_spinFt8NoResponseLimit != nullptr) m_spinFt8NoResponseLimit->setEnabled(!m_offlineAnalysisActive);
        if (m_btnFt8AnalyzeWav != nullptr) m_btnFt8AnalyzeWav->setEnabled(!m_txRunning && !m_offlineAnalysisActive && !m_ftAutoTestRunning);
        if (m_btnFtDecodeAnalyzeWav != nullptr) m_btnFtDecodeAnalyzeWav->setEnabled(!m_txRunning && !m_offlineAnalysisActive && !m_ftAutoTestRunning);
        if (m_btnFtDecodeAutoTest != nullptr) m_btnFtDecodeAutoTest->setEnabled(!m_txRunning && !m_offlineAnalysisActive && !m_ftAutoTestRunning);
    }

    ui->btnFaxResetImage->setEnabled(configurable);
    ui->btnFaxSaveImage->setEnabled(configurable);
    ui->btnFaxZoomFit->setEnabled(true);
    ui->btnFaxZoom100->setEnabled(true);
    ui->btnFaxZoomOut->setEnabled(true);
    ui->btnFaxZoomIn->setEnabled(true);

    if (m_txRunning) {
        ui->lblAppStatus->setText("TX active");
        updateMainStateButton();
        updateTxControlState();
        return;
    }

    if (m_offlineAnalysisActive) {
        ui->lblAppStatus->setText("WAV analysis");
        updateMainStateButton();
        updateTxControlState();
        return;
    }

    ui->lblAppStatus->setText(m_rxRunning ? "RX running" : "Ready");
    updateMainStateButton();
    updateTxControlState();
}

void MainWindow::updateMainStateButton()
{
    if (ui->btnStartRx == nullptr) {
        return;
    }

    if (m_txRunning) {
        ui->btnStartRx->setText(uiText("button.transport_tx_busy", "● TX"));
        ui->btnStartRx->setEnabled(false);
        return;
    }

    if (m_offlineAnalysisActive) {
        ui->btnStartRx->setText(uiText("button.transport_wav", "… WAV"));
        ui->btnStartRx->setEnabled(false);
        return;
    }

    if (m_rxRunning) {
        ui->btnStartRx->setText(uiText("button.transport_rx_stop", "■ RX"));
        ui->btnStartRx->setEnabled(true);
        return;
    }

    ui->btnStartRx->setText(uiText("button.transport_rx", "▶ RX"));
    ui->btnStartRx->setEnabled(true);
}

bool MainWindow::prepareForOfflineAnalysis(const QString &label)
{
    if (m_txRunning || (m_txAudioEngine != nullptr && m_txAudioEngine->isRunning())) {
        QMessageBox::information(
            this,
            label,
            "Stop TX before analyzing a WAV test file."
            );
        return false;
    }

    if (m_audioEngine != nullptr && m_audioEngine->isRunning()) {
        appendLog(label + ": stopping live RX first.");
        m_audioEngine->stopInput();
    }

    m_rxRunning = false;
    m_offlineAnalysisActive = true;
    setReceiverRunning(false);
    return true;
}



void MainWindow::requestModeChange(const QString &modeName)
{
    if (ui->cmbMode == nullptr || modeName.isEmpty()) {
        return;
    }

    const QString currentMode = ui->cmbMode->currentText();
    const bool txActive = m_txRunning ||
                          (m_txAudioEngine != nullptr && m_txAudioEngine->isRunning());
    const bool rxActive = m_rxRunning ||
                          (m_audioEngine != nullptr && m_audioEngine->isRunning());

    if (m_offlineAnalysisActive) {
        appendLog("Mode change blocked: WAV analysis is active.");
        QMessageBox::information(
            this,
            "Change mode",
            "Wait for WAV analysis to finish before changing modem mode."
            );
        return;
    }

    if (!txActive && !rxActive) {
        if (currentMode != modeName) {
            ui->cmbMode->setCurrentText(modeName);
            appendLog("Mode changed to " + shortModeLabel(modeName) + ".");
        }
        return;
    }

    if (currentMode == modeName && rxActive && !txActive) {
        return;
    }

    m_pendingModeName = modeName;
    m_pendingModeRestartRx = true;

    appendLog(QString("Changing mode to %1: stopping active audio first.")
                  .arg(shortModeLabel(modeName)));

    if (txActive) {
        m_returnToRxAfterTx = false;
        m_txFinishedNaturally = false;

        if (m_txAudioEngine != nullptr) {
            m_txAudioEngine->stopOutput();
        } else {
            m_txRunning = false;
            finishPendingModeChange();
        }
        return;
    }

    if (rxActive && m_audioEngine != nullptr) {
        m_audioEngine->stopInput();
        return;
    }

    finishPendingModeChange();
}

void MainWindow::finishPendingModeChange()
{
    if (m_pendingModeName.isEmpty() || ui->cmbMode == nullptr) {
        return;
    }

    const QString targetMode = m_pendingModeName;
    const bool restartRx = m_pendingModeRestartRx;

    m_pendingModeName.clear();
    m_pendingModeRestartRx = false;

    if (ui->cmbMode->currentText() != targetMode) {
        ui->cmbMode->setCurrentText(targetMode);
    }

    setReceiverRunning(false);
    updateWaterfallMarkers();
    updateTxPreview();

    appendLog("Mode changed to " + shortModeLabel(targetMode) + ".");

    if (!restartRx) {
        return;
    }

    QTimer::singleShot(250, this, [this]() {
        if (m_pendingModeName.isEmpty() &&
            !m_txRunning &&
            !m_offlineAnalysisActive &&
            (m_audioEngine == nullptr || !m_audioEngine->isRunning())) {
            appendLog("Restarting RX after mode change.");
            startRx();
        }
    });
}


void MainWindow::handleModeChanged(const QString &modeName)
{
    updateMindUiForMode(modeName);
    updateCentralDisplayForMode(modeName);
    populateQsoFormDefaults(activeQsoForm(), modeName);
    updateBandSchedulerTabForMode(modeName);
    updateDspTabForMode(modeName);

    updateFtUtcClockVisibility(modeName);

    if (modeName == WeatherFaxDecoder::modeName()) {
        ui->stkModeSettings->setCurrentWidget(ui->pageFaxSettings);
        ui->lblDecoderState->setText("Decoder: WEFAX ready");

        if (m_faxImageWidget != nullptr) {
            m_faxImageWidget->setImage(m_weatherFaxDecoder->currentImage());
        }
    } else if (modeName == SstvDecoder::modeName()) {
        ui->stkModeSettings->setCurrentWidget(ui->pageSstvSettings);
        applySstvSettings();
        ui->lblDecoderState->setText("Decoder: SSTV ready");

        if (m_faxImageWidget != nullptr) {
            m_faxImageWidget->setImage(m_sstvDecoder->currentImage());
        }
    } else if (modeName == RttyDecoder::modeName()) {
        if (m_pageRttySettings != nullptr) {
            ui->stkModeSettings->setCurrentWidget(m_pageRttySettings);
        }
        applyRttySettings();
        ui->lblDecoderState->setText("Decoder: RTTY ready");
    } else if (modeName == Bpsk31Decoder::modeName()) {
        if (m_pageBpsk31Settings != nullptr) {
            ui->stkModeSettings->setCurrentWidget(m_pageBpsk31Settings);
        }
        applyBpsk31Settings();
        ui->lblDecoderState->setText("Decoder: PSK/QPSK ready");
    } else if (modeName == MfskDecoder::modeName()) {
        if (m_pageMfskSettings != nullptr) {
            ui->stkModeSettings->setCurrentWidget(m_pageMfskSettings);
        }
        applyMfskSettings();
        ui->lblDecoderState->setText("Decoder: MFSK ready");
    } else if (modeName == CwDecoder::modeName()) {
        if (m_pageCwSettings != nullptr) {
            ui->stkModeSettings->setCurrentWidget(m_pageCwSettings);
        }
        applyCwSettings();
        ui->lblDecoderState->setText("Decoder: CW Morse ready");
    } else if (modeName == HellschreiberDecoder::modeName()) {
        if (m_pageHellSettings != nullptr) {
            ui->stkModeSettings->setCurrentWidget(m_pageHellSettings);
        }
        applyHellSettings();
        ui->lblDecoderState->setText("Decoder: Hellschreiber ready");

        if (m_lblHellRaster != nullptr) {
            const QImage image = m_hellDecoder->currentImage();
            updateHellRasterDisplay(image);
        }
    } else if (Msk144Mode::isMode(modeName)) {
        if (m_pageMsk144Settings != nullptr) {
            ui->stkModeSettings->setCurrentWidget(m_pageMsk144Settings);
        }
        applyMsk144Settings();
        ui->lblDecoderState->setText("Decoder: MSK144 ready");
    } else if (Q65Mode::isFamilyMode(modeName)) {
        if (m_pageQ65Settings != nullptr) {
            ui->stkModeSettings->setCurrentWidget(m_pageQ65Settings);
        }
        applyQ65Settings();
        ui->lblDecoderState->setText(QStringLiteral("Decoder: %1 ready").arg(Q65Mode::modeName(currentQ65Submode())));
    } else if (Ft8Mode::isFamilyMode(modeName)) {
        if (m_pageFt8Settings != nullptr) {
            ui->stkModeSettings->setCurrentWidget(m_pageFt8Settings);
        }
        applyFt8Settings();
        qsyRigToSelectedFtBand();
        const Ft8Mode::Profile profile = Ft8Mode::profileForMode(modeName);
        ui->lblDecoderState->setText(profile.interoperableCoreAvailable
            ? QString("Decoder: %1 TX/RX core ready").arg(profile.shortLabel)
            : QString("Decoder: %1 core unavailable").arg(profile.shortLabel));
        updateFt8SlotStatus();
    }

    if (m_btnShowRuntimeLog != nullptr) {
        m_btnShowRuntimeLog->setVisible(Ft8Mode::isFamilyMode(modeName) || Msk144Mode::isMode(modeName) || Q65Mode::isFamilyMode(modeName));
    }

    updateWaterfallMarkers();
    updateTxPreview();
}


bool MainWindow::modeSupportsMind(const QString &modeName) const
{
    return Ft8Mode::isFamilyMode(modeName) || Msk144Mode::isMode(modeName);
}

void MainWindow::updateMindUiForMode(const QString &modeName)
{
    const QString currentMode = modeName.trimmed().isEmpty() && ui != nullptr && ui->cmbMode != nullptr
        ? ui->cmbMode->currentText()
        : modeName.trimmed();
    const bool mindMode = modeSupportsMind(currentMode);

    if (m_ddspController != nullptr) {
        // MIND is now operator-fixed in Assist-requested mode.  Non-FT modes
        // still hard-bypass decoder integration below, but the autonomous
        // trainer remains alive on the persistent FT replay dataset instead of
        // being turned off every time the operator leaves FT8/FT4.
        m_ddspController->setAssistMode(QStringLiteral("assisted"));
        if (mindMode) {
            m_ddspController->setRuntimeMode(currentMode);
        }
    }

    if (m_ft8RxDecoder != nullptr && !Ft8Mode::isFamilyMode(currentMode)) {
        m_ft8RxDecoder->setMindIntegrationState(true, false, false, false);
    }
    if (m_msk144Decoder != nullptr && !Msk144Mode::isMode(currentMode)) {
        m_msk144Decoder->setMindIntegrationState(true, false, false, false);
    }
    if (ui == nullptr || ui->sideTabWidget == nullptr || m_ddspPanelWidget == nullptr ||
        ui->sideTabWidget->tabBar() == nullptr) {
        return;
    }

    const int tabIndex = ui->sideTabWidget->indexOf(m_ddspPanelWidget);
    if (tabIndex < 0) {
        return;
    }

    ui->sideTabWidget->tabBar()->setTabVisible(tabIndex, mindMode);
    m_ddspPanelWidget->setVisible(mindMode);
    if (!mindMode && ui->sideTabWidget->currentIndex() == tabIndex) {
        const int modeIndex = ui->tabModeSettings != nullptr
            ? ui->sideTabWidget->indexOf(ui->tabModeSettings)
            : -1;
        ui->sideTabWidget->setCurrentIndex(modeIndex >= 0 ? modeIndex : 0);
    }
}

void MainWindow::updateWaterfallMarkers()
{
    if (m_waterfallWidget == nullptr) {
        return;
    }

    const QString mode = ui->cmbMode->currentText();
    const bool cwMode = (mode == CwDecoder::modeName());

    // CW keeps operator markers simple: A green, optional B blue.  The skimmer
    // runs underneath but its internal FFT channels are never drawn as markers.
    if (ui->frameWaterfall != nullptr) {
        ui->frameWaterfall->setMinimumHeight(cwMode ? 200 : 210);
        ui->frameWaterfall->setContentsMargins(0, 0, 0, 0);
    }
    if (ui->waterfallVerticalLayout != nullptr) {
        ui->waterfallVerticalLayout->setContentsMargins(0, 0, 0, 0);
        ui->waterfallVerticalLayout->setSpacing(0);
    }
    if (ui->frameSstvImage != nullptr) {
        ui->frameSstvImage->setMinimumHeight(cwMode ? 360 : 360);
    }
    if (m_txtCwRx != nullptr) {
        m_txtCwRx->setMinimumHeight(cwMode ? 165 : 190);
        m_txtCwRx->setMaximumHeight(cwMode ? QWIDGETSIZE_MAX : QWIDGETSIZE_MAX);
    }

    m_waterfallWidget->setScrollDirection(WaterfallWidget::ScrollDirection::Down);
    if (!Ft8Mode::isFamilyMode(mode) &&
        mode != RttyDecoder::modeName() &&
        mode != CwDecoder::modeName()) {
        m_waterfallWidget->setTextOverlays({});
    }

    if (mode == WeatherFaxDecoder::modeName()) {
        m_waterfallWidget->setMarkers(m_weatherFaxDecoder->currentFrequencyMarkers());
        return;
    }

    if (mode == SstvDecoder::modeName()) {
        m_waterfallWidget->setMarkers(SstvDecoder::frequencyMarkers());
        return;
    }

    if (mode == RttyDecoder::modeName()) {
        const double mark = (m_spinRttyMarkHz != nullptr) ? m_spinRttyMarkHz->value() : 2125.0;
        const double shift = (m_spinRttyShiftHz != nullptr) ? m_spinRttyShiftHz->value() : 170.0;
        const bool reverse = (m_chkRttyReverse != nullptr) ? m_chkRttyReverse->isChecked() : false;
        m_waterfallWidget->setMarkers(RttyDecoder::frequencyMarkers(mark, mark + shift, reverse));
        QVector<WaterfallTextOverlay> overlays;
        if (m_settings.rttyOverlayCallsignsEnabled) {
            for (const RttyMultiDecoder::Callout &callout : std::as_const(m_rttyWaterfallCallouts)) {
                WaterfallTextOverlay overlay;
                overlay.frequencyHz = static_cast<double>(callout.markHz);
                overlay.label = callout.label;
                overlay.textColor = callout.cq ? QColor(255, 255, 180) : QColor(180, 230, 255);
                overlay.backgroundColor = callout.cq ? QColor(0, 95, 0, 210) : QColor(0, 0, 0, 190);
                overlays.append(overlay);
            }
        }
        m_waterfallWidget->setTextOverlays(overlays);
        return;
    }

    if (mode == Bpsk31Decoder::modeName()) {
        const double tone = (m_spinBpsk31ToneHz != nullptr) ? m_spinBpsk31ToneHz->value() : 1000.0;
        const QString variant = (m_cmbBpsk31Variant != nullptr)
                                    ? m_cmbBpsk31Variant->currentData().toString()
                                    : QStringLiteral("BPSK31");
        const double symbolRate = bpskSymbolRateForVariant(variant);
        m_waterfallWidget->setMarkers(Bpsk31Decoder::frequencyMarkers(tone, symbolRate, pskVariantIsQpsk(variant)));
        return;
    }

    if (mode == MfskDecoder::modeName()) {
        const double center = (m_spinMfskCenterHz != nullptr) ? m_spinMfskCenterHz->value() : 1000.0;
        const QString variantKey = (m_cmbMfskVariant != nullptr)
                                       ? m_cmbMfskVariant->currentData().toString()
                                       : QStringLiteral("MFSK16");
        m_waterfallWidget->setMarkers(MfskDecoder::frequencyMarkers(center, MfskDecoder::variantFromKey(variantKey)));
        return;
    }

    if (mode == CwDecoder::modeName()) {
        QVector<FrequencyMarker> markers;
        FrequencyMarker rxA;
        rxA.frequencyHz = (m_spinCwToneHz != nullptr) ? m_spinCwToneHz->value() : m_settings.cwToneHz;
        rxA.label = QStringLiteral("A");
        rxA.color = QColor(80, 255, 120);
        rxA.width = 2;
        rxA.dashed = false;
        markers.append(rxA);

        if (m_cwSecondaryEnabled) {
            FrequencyMarker rxB;
            rxB.frequencyHz = m_cwSecondaryToneHz;
            rxB.label = QStringLiteral("B");
            rxB.color = QColor(80, 170, 255);
            rxB.width = 2;
            rxB.dashed = false;
            markers.append(rxB);
        }

        m_waterfallWidget->setMarkers(markers);
        return;
    }

    if (mode == HellschreiberDecoder::modeName()) {
        const double tone = (m_spinHellToneHz != nullptr) ? m_spinHellToneHz->value() : 1000.0;
        const double bandwidth = (m_spinHellBandwidthHz != nullptr) ? m_spinHellBandwidthHz->value() : 245.0;
        const HellschreiberDecoder::Variant variant = (m_cmbHellVariant != nullptr)
                                                         ? HellschreiberDecoder::variantFromKey(m_cmbHellVariant->currentData().toString())
                                                         : HellschreiberDecoder::Variant::FeldHell;
        m_waterfallWidget->setMarkers(HellschreiberDecoder::frequencyMarkers(tone,
                                                                              bandwidth,
                                                                              variant,
                                                                              HellschreiberDecoder::fsk105ShiftHz()));
        return;
    }

    if (Msk144Mode::isMode(mode)) {
        QVector<FrequencyMarker> markers;
        FrequencyMarker rx;
        rx.frequencyHz = (m_spinMsk144RxFreq != nullptr) ? m_spinMsk144RxFreq->value() : 1500.0;
        rx.label = QStringLiteral("RX");
        rx.color = QColor(80, 255, 120);
        rx.width = 2;
        rx.dashed = false;
        markers.append(rx);
        FrequencyMarker tx;
        tx.frequencyHz = 1500.0;
        tx.label = QStringLiteral("TX");
        tx.color = QColor(255, 80, 80);
        tx.width = 2;
        tx.dashed = true;
        markers.append(tx);
        m_waterfallWidget->setMarkers(markers);
        m_waterfallWidget->setTextOverlays({});
        return;
    }

    if (Q65Mode::isFamilyMode(mode)) {
        QVector<FrequencyMarker> markers;
        FrequencyMarker rx;
        rx.frequencyHz = (m_spinQ65RxFreq != nullptr) ? m_spinQ65RxFreq->value() : 1500.0;
        rx.label = QStringLiteral("RX");
        rx.color = QColor(80, 255, 120);
        rx.width = 2;
        rx.dashed = false;
        markers.append(rx);
        FrequencyMarker tx;
        tx.frequencyHz = (m_spinQ65TxFreq != nullptr) ? m_spinQ65TxFreq->value() : 1500.0;
        tx.label = QStringLiteral("TX");
        tx.color = QColor(255, 80, 80);
        tx.width = 2;
        tx.dashed = true;
        markers.append(tx);
        m_waterfallWidget->setMarkers(markers);
        return;
    }

    if (Ft8Mode::isFamilyMode(mode)) {
        const int rxHz = (m_spinFt8RxFreq != nullptr) ? m_spinFt8RxFreq->value() : 1500;
        const int txHz = (m_spinFt8TxFreq != nullptr) ? m_spinFt8TxFreq->value() : 1500;
        m_waterfallWidget->setMarkers(Ft8Mode::frequencyMarkers(rxHz, txHz, mode));
        updateFt8WaterfallOverlays();
        return;
    }

    m_waterfallWidget->setMarkers({});
    m_waterfallWidget->setTextOverlays({});
}

void MainWindow::handleWaterfallFrequencyClicked(double frequencyHz, Qt::MouseButton button)
{
    if (m_txRunning || m_offlineAnalysisActive) {
        return;
    }

    const int roundedHz = qBound(100, static_cast<int>(qRound(frequencyHz)), 3500);
    const QString mode = ui->cmbMode->currentText();

    if (Msk144Mode::isMode(mode) && m_spinMsk144RxFreq != nullptr) {
        const int hz = qBound(m_spinMsk144RxFreq->minimum(), roundedHz, m_spinMsk144RxFreq->maximum());
        m_spinMsk144RxFreq->setValue(hz);
        applyMsk144Settings();
        appendLog(QStringLiteral("MSK144 RX frequency from waterfall: %1 Hz.").arg(hz));
        return;
    }

    if (Q65Mode::isFamilyMode(mode) && m_spinQ65RxFreq != nullptr) {
        const int hz = qBound(m_spinQ65RxFreq->minimum(), roundedHz, m_spinQ65RxFreq->maximum());
        m_spinQ65RxFreq->setValue(hz);
        applyQ65Settings();
        appendLog(QStringLiteral("Q65 RX frequency from waterfall: %1 Hz.").arg(hz));
        return;
    }

    if (mode == RttyDecoder::modeName() &&
        m_spinRttyMarkHz != nullptr &&
        m_spinRttyShiftHz != nullptr &&
        m_cmbRttyPreset != nullptr) {
        const int shiftHz = m_spinRttyShiftHz->value();
        const int currentMark = m_spinRttyMarkHz->value();
        const int currentSpace = currentMark + shiftHz;
        const bool clickedSpace = qAbs(roundedHz - currentSpace) < qAbs(roundedHz - currentMark);

        int newMark = clickedSpace ? (roundedHz - shiftHz) : roundedHz;
        newMark = qBound(m_spinRttyMarkHz->minimum(), newMark, m_spinRttyMarkHz->maximum());

        if (newMark + shiftHz > 3500) {
            newMark = 3500 - shiftHz;
        }

        const int customIndex = m_cmbRttyPreset->findData(QStringLiteral("CUSTOM"));
        {
            const QSignalBlocker blockPreset(m_cmbRttyPreset);
            const QSignalBlocker blockMark(m_spinRttyMarkHz);
            if (customIndex >= 0) {
                m_cmbRttyPreset->setCurrentIndex(customIndex);
            }
            m_spinRttyMarkHz->setValue(newMark);
        }

        applyRttySettings();
        appendLog(QString("RTTY tones from waterfall: Mark %1 Hz, Space %2 Hz.")
                      .arg(newMark)
                      .arg(newMark + shiftHz));
        return;
    }

    if (mode == Bpsk31Decoder::modeName() && m_spinBpsk31ToneHz != nullptr) {
        const int newTone = qBound(m_spinBpsk31ToneHz->minimum(),
                                   roundedHz,
                                   m_spinBpsk31ToneHz->maximum());
        {
            const QSignalBlocker blockTone(m_spinBpsk31ToneHz);
            m_spinBpsk31ToneHz->setValue(newTone);
        }

        applyBpsk31Settings();
        appendLog(QString("PSK tone from waterfall: %1 Hz.").arg(newTone));
        return;
    }

    if (mode == MfskDecoder::modeName() && m_spinMfskCenterHz != nullptr) {
        const int newCenter = qBound(m_spinMfskCenterHz->minimum(),
                                     roundedHz,
                                     m_spinMfskCenterHz->maximum());
        {
            const QSignalBlocker blockCenter(m_spinMfskCenterHz);
            m_spinMfskCenterHz->setValue(newCenter);
        }

        applyMfskSettings();
        appendLog(QString("MFSK center from waterfall: %1 Hz.").arg(newCenter));
        return;
    }

    if (mode == CwDecoder::modeName() && m_spinCwToneHz != nullptr) {
        const int newTone = qBound(m_spinCwToneHz->minimum(),
                                   roundedHz,
                                   m_spinCwToneHz->maximum());
        if (button == Qt::RightButton) {
            m_cwSecondaryEnabled = true;
            m_cwSecondaryToneHz = newTone;
            m_settings.cwSecondaryEnabled = true;
            m_settings.cwSecondaryToneHz = newTone;
            if (m_cwDecoder != nullptr) {
                m_cwDecoder->setSecondaryToneHz(static_cast<double>(newTone));
                m_cwDecoder->setSecondaryEnabled(true);
            }
            if (m_cwSecondaryDecoder != nullptr) {
                m_cwSecondaryDecoder->setToneHz(static_cast<double>(newTone));
                m_cwSecondaryDecoder->reset();
            }
            m_cwTrackedWpmB = 0.0;
            updateCwDualRxStatusLabel();
            savePersistentSettings();
            updateWaterfallMarkers();
            appendLog(QString("CW RX B from waterfall: %1 Hz.").arg(newTone));
            return;
        }

        {
            const QSignalBlocker blockTone(m_spinCwToneHz);
            m_spinCwToneHz->setValue(newTone);
        }

        applyCwSettings();
        updateCwDualRxStatusLabel();
        appendLog(QString("CW RX A from waterfall: %1 Hz.").arg(newTone));
        return;
    }

    if (mode == HellschreiberDecoder::modeName() && m_spinHellToneHz != nullptr) {
        const int newTone = qBound(m_spinHellToneHz->minimum(),
                                   roundedHz,
                                   m_spinHellToneHz->maximum());
        {
            const QSignalBlocker blockTone(m_spinHellToneHz);
            m_spinHellToneHz->setValue(newTone);
        }

        applyHellSettings();
        appendLog(QString("Hellschreiber center tone from waterfall: %1 Hz.").arg(newTone));
        return;
    }



    if (Ft8Mode::isFamilyMode(mode) && m_spinFt8RxFreq != nullptr && m_spinFt8TxFreq != nullptr) {
        if (button == Qt::RightButton) {
            const int tx = qBound(m_spinFt8TxFreq->minimum(), roundedHz, m_spinFt8TxFreq->maximum());
            const QSignalBlocker blockTx(m_spinFt8TxFreq);
            m_spinFt8TxFreq->setValue(tx);
            m_settings.ft8TxFrequencyHz = tx;
            if (m_cmbFt8TxStrategy != nullptr) {
                const int idx = m_cmbFt8TxStrategy->findData(QStringLiteral("fixed"));
                if (idx >= 0) {
                    const QSignalBlocker blockStrategy(m_cmbFt8TxStrategy);
                    m_cmbFt8TxStrategy->setCurrentIndex(idx);
                }
            }
            if (m_chkFt8HoldTxFreq != nullptr) {
                const QSignalBlocker blockHold(m_chkFt8HoldTxFreq);
                m_chkFt8HoldTxFreq->setChecked(true);
            }
            m_settings.ft8TxFrequencyStrategy = QStringLiteral("fixed");
            m_settings.ft8HoldTxFrequency = true;
            appendLog(QString("%1 TX marker from waterfall: %2 Hz; strategy set to fixed/manual.")
                          .arg(Ft8Mode::profileForMode(mode).shortLabel)
                          .arg(m_spinFt8TxFreq->value()));
            updateWaterfallMarkers();
            savePersistentSettings();
        } else {
            applyFt8RuntimeFrequencySelection(roundedHz,
                                              QStringLiteral("manual RX focus marker from waterfall"),
                                              true);
            savePersistentSettings();
        }
        return;
    }
    if (mode == WeatherFaxDecoder::modeName()) {
        const int blackHz = ui->spinFaxBlackHz->value();
        const int whiteHz = ui->spinFaxWhiteHz->value();
        const int centerHz = (blackHz + whiteHz) / 2;
        const int halfSpan = qMax(50, qAbs(whiteHz - blackHz) / 2);

        const int dBlack = qAbs(roundedHz - blackHz);
        const int dWhite = qAbs(roundedHz - whiteHz);
        const int dCenter = qAbs(roundedHz - centerHz);

        int newBlack = blackHz;
        int newWhite = whiteHz;

        if (dCenter <= dBlack && dCenter <= dWhite) {
            newBlack = roundedHz - halfSpan;
            newWhite = roundedHz + halfSpan;
        } else if (dBlack <= dWhite) {
            newBlack = roundedHz;
        } else {
            newWhite = roundedHz;
        }

        newBlack = qBound(ui->spinFaxBlackHz->minimum(), newBlack, ui->spinFaxBlackHz->maximum());
        newWhite = qBound(ui->spinFaxWhiteHz->minimum(), newWhite, ui->spinFaxWhiteHz->maximum());

        if (qAbs(newWhite - newBlack) < 50) {
            if (newWhite >= newBlack) {
                newWhite = qMin(ui->spinFaxWhiteHz->maximum(), newBlack + 50);
            } else {
                newBlack = qMin(ui->spinFaxBlackHz->maximum(), newWhite + 50);
            }
        }

        {
            const QSignalBlocker blockBlack(ui->spinFaxBlackHz);
            const QSignalBlocker blockWhite(ui->spinFaxWhiteHz);
            ui->spinFaxBlackHz->setValue(newBlack);
            ui->spinFaxWhiteHz->setValue(newWhite);
        }

        applyWeatherFaxSettings();
        appendLog(QString("WEFAX tones from waterfall: black %1 Hz, white %2 Hz.")
                      .arg(newBlack)
                      .arg(newWhite));
    }
}


void MainWindow::retuneRttyFromAfc(int markHz, int spaceHz)
{
    if (m_rttyDecoder == nullptr || m_spinRttyMarkHz == nullptr || m_spinRttyShiftHz == nullptr) {
        return;
    }

    markHz = qBound(m_spinRttyMarkHz->minimum(), markHz, m_spinRttyMarkHz->maximum());
    spaceHz = qBound(300, spaceHz, 3500);
    const int shiftHz = spaceHz - markHz;
    if (shiftHz < m_spinRttyShiftHz->minimum() || shiftHz > m_spinRttyShiftHz->maximum()) {
        return;
    }

    const QSignalBlocker blockMark(m_spinRttyMarkHz);
    const QSignalBlocker blockShift(m_spinRttyShiftHz);
    m_spinRttyMarkHz->setValue(markHz);
    m_spinRttyShiftHz->setValue(shiftHz);

    m_rttyDecoder->retuneTones(static_cast<double>(markHz), static_cast<double>(spaceHz));
    updateWaterfallMarkers();
    updateTxPreview();
}

void MainWindow::updateTextModeAfc(const AudioBlock &block)
{
    if (!m_rxRunning || m_txRunning || m_offlineAnalysisActive || block.samples.isEmpty() || block.sampleRate <= 0) {
        return;
    }

    const QString modeName = ui->cmbMode->currentText();
    const bool textMode = modeName == RttyDecoder::modeName() ||
                          modeName == Bpsk31Decoder::modeName() ||
                          modeName == MfskDecoder::modeName() ||
                          modeName == HellschreiberDecoder::modeName();
    if (!textMode) {
        m_textAfcSamplesSinceUpdate = 0;
        return;
    }

    m_textAfcSamplesSinceUpdate += block.samples.size();
    const int updateInterval = qMax(2048, block.sampleRate / 4);
    if (m_textAfcSamplesSinceUpdate < updateInterval) {
        return;
    }
    m_textAfcSamplesSinceUpdate = 0;

    constexpr double searchStepHz = 1.0;
    constexpr int maxStepHz = 3;

    if (modeName == RttyDecoder::modeName()) {
        if (m_chkRttyAfc == nullptr || !m_chkRttyAfc->isChecked() ||
            m_spinRttyAfcRangeHz == nullptr ||
            m_spinRttyMarkHz == nullptr || m_spinRttyShiftHz == nullptr) {
            return;
        }

        const int rangeHz = m_spinRttyAfcRangeHz->value();
        const int oldMark = m_spinRttyMarkHz->value();
        const int oldSpace = oldMark + m_spinRttyShiftHz->value();

        const AfcTonePeak markPeak = estimateAfcTonePeak(block, static_cast<double>(oldMark), rangeHz, searchStepHz);
        const AfcTonePeak spacePeak = estimateAfcTonePeak(block, static_cast<double>(oldSpace), rangeHz, searchStepHz);

        int newMark = oldMark;
        int newSpace = oldSpace;
        if (markPeak.valid) {
            newMark = nudgedToneValue(oldMark, markPeak.frequencyHz, maxStepHz,
                                      m_spinRttyMarkHz->minimum(), m_spinRttyMarkHz->maximum());
        }
        if (spacePeak.valid) {
            newSpace = nudgedToneValue(oldSpace, spacePeak.frequencyHz, maxStepHz, 300, 3500);
        }

        if ((newMark != oldMark || newSpace != oldSpace) && newSpace > newMark + 20) {
            retuneRttyFromAfc(newMark, newSpace);
        }
        return;
    }

    if (modeName == Bpsk31Decoder::modeName()) {
        if (m_bpsk31Decoder == nullptr ||
            m_chkBpsk31Afc == nullptr || !m_chkBpsk31Afc->isChecked() ||
            m_spinBpsk31AfcRangeHz == nullptr || m_spinBpsk31ToneHz == nullptr) {
            return;
        }

        const int oldTone = m_spinBpsk31ToneHz->value();
        const AfcTonePeak peak = estimateAfcTonePeak(block, static_cast<double>(oldTone),
                                                     m_spinBpsk31AfcRangeHz->value(), searchStepHz);
        if (!peak.valid) {
            return;
        }

        const int newTone = nudgedToneValue(oldTone, peak.frequencyHz, maxStepHz,
                                            m_spinBpsk31ToneHz->minimum(), m_spinBpsk31ToneHz->maximum());
        if (newTone != oldTone) {
            const QSignalBlocker blockTone(m_spinBpsk31ToneHz);
            m_spinBpsk31ToneHz->setValue(newTone);
            m_bpsk31Decoder->setToneHz(static_cast<double>(newTone));
            m_bpsk31Decoder->setAfcEnabled(true);
            m_bpsk31Decoder->setAfcRangeHz(static_cast<double>(m_spinBpsk31AfcRangeHz->value()));
            updateWaterfallMarkers();
            updateTxPreview();
        }
        return;
    }

    if (modeName == CwDecoder::modeName()) {
        if (m_cwDecoder == nullptr ||
            m_chkCwAfc == nullptr || !m_chkCwAfc->isChecked() ||
            m_spinCwAfcRangeHz == nullptr || m_spinCwToneHz == nullptr) {
            return;
        }

        const int oldTone = m_spinCwToneHz->value();
        const AfcTonePeak peak = estimateAfcTonePeak(block, static_cast<double>(oldTone),
                                                     m_spinCwAfcRangeHz->value(), searchStepHz);
        if (!peak.valid) {
            return;
        }

        const int newTone = nudgedToneValue(oldTone, peak.frequencyHz, maxStepHz,
                                            m_spinCwToneHz->minimum(), m_spinCwToneHz->maximum());
        if (newTone != oldTone) {
            const QSignalBlocker blockTone(m_spinCwToneHz);
            m_spinCwToneHz->setValue(newTone);
            m_cwDecoder->setToneHz(static_cast<double>(newTone));
            updateWaterfallMarkers();
            updateTxPreview();
        }
        return;
    }

    if (modeName == HellschreiberDecoder::modeName()) {
        if (m_hellDecoder == nullptr ||
            m_chkHellAfc == nullptr || !m_chkHellAfc->isChecked() ||
            m_spinHellAfcRangeHz == nullptr || m_spinHellToneHz == nullptr) {
            return;
        }

        const int oldTone = m_spinHellToneHz->value();
        const bool fsk105 = (m_cmbHellVariant != nullptr && m_cmbHellVariant->currentData().toString() == "FSK105");
        double measuredCenterHz = 0.0;
        bool measuredValid = false;

        if (fsk105) {
            const double halfShift = HellschreiberDecoder::fsk105ShiftHz() * 0.5;
            const int rangeHz = m_spinHellAfcRangeHz->value();
            const AfcTonePeak lowPeak = estimateAfcTonePeak(block,
                                                            static_cast<double>(oldTone) - halfShift,
                                                            rangeHz,
                                                            searchStepHz);
            const AfcTonePeak highPeak = estimateAfcTonePeak(block,
                                                             static_cast<double>(oldTone) + halfShift,
                                                             rangeHz,
                                                             searchStepHz);

            if (lowPeak.valid && highPeak.valid) {
                measuredCenterHz = (lowPeak.frequencyHz + highPeak.frequencyHz) * 0.5;
                measuredValid = true;
            } else if (lowPeak.valid) {
                measuredCenterHz = lowPeak.frequencyHz + halfShift;
                measuredValid = true;
            } else if (highPeak.valid) {
                measuredCenterHz = highPeak.frequencyHz - halfShift;
                measuredValid = true;
            }
        } else {
            const AfcTonePeak peak = estimateAfcTonePeak(block, static_cast<double>(oldTone),
                                                         m_spinHellAfcRangeHz->value(), searchStepHz);
            if (peak.valid) {
                measuredCenterHz = peak.frequencyHz;
                measuredValid = true;
            }
        }

        if (!measuredValid) {
            return;
        }

        const int newTone = nudgedToneValue(oldTone, measuredCenterHz, maxStepHz,
                                            m_spinHellToneHz->minimum(), m_spinHellToneHz->maximum());
        if (newTone != oldTone) {
            const QSignalBlocker blockTone(m_spinHellToneHz);
            m_spinHellToneHz->setValue(newTone);
            m_hellDecoder->setToneHz(static_cast<double>(newTone));
            updateWaterfallMarkers();
            updateTxPreview();
        }
    }
}

void MainWindow::handleRxAudioBlock(const AudioBlock &block)
{
    if (!m_rxRunning || m_txRunning || m_offlineAnalysisActive) {
        return;
    }

    const QString modeName = ui->cmbMode->currentText();

    if (Ft8Mode::isFamilyMode(modeName)) {
        if (m_ddspController != nullptr) {
            m_ddspController->observeFtActivity(modeName);
        }
        if (Ft8Mode::profileForMode(modeName).interoperableCoreAvailable && m_ft8RxDecoder != nullptr) {
            // FT weak-signal RX follows the WSJT-X divide-et-impera pattern:
            // do not run per-block conditioner/AFC work in MainWindow.  Queue
            // the raw audio block to the FT decoder worker and let that worker
            // own resampling/slot collection/decode timing.
            QMetaObject::invokeMethod(m_ft8RxDecoder, "processAudioBlock", Qt::QueuedConnection, Q_ARG(AudioBlock, block));
        }
        return;
    }

    if (Msk144Mode::isMode(modeName)) {
        if (m_ddspController != nullptr) {
            m_ddspController->observeMsk144Activity();
        }
        if (m_msk144Decoder != nullptr) {
            m_msk144Decoder->processAudioBlock(block);
        }
        return;
    }

    if (Q65Mode::isFamilyMode(modeName)) {
        if (m_q65Decoder != nullptr) {
            m_q65Decoder->processAudioBlock(block);
        }
        return;
    }

    updateTextModeAfc(block);
    const AudioBlock conditionedBlock = conditionAudioForActiveMode(block);

    if (modeName == WeatherFaxDecoder::modeName()) {
        m_weatherFaxDecoder->processAudioBlock(conditionedBlock);
        return;
    }

    if (modeName == SstvDecoder::modeName()) {
        m_sstvDecoder->processAudioBlock(conditionedBlock);
        return;
    }

    if (modeName == RttyDecoder::modeName()) {
        if (m_rttyMultiDecoder != nullptr && m_settings.rttyMultiDecodeEnabled) {
            // Parallel monitor must see the wide passband, not the selected
            // RTTY tone bandpass used by the main terminal decoder.
            m_rttyMultiDecoder->processAudioBlock(block);
        }
        m_rttyDecoder->processAudioBlock(conditionedBlock);
        return;
    }

    if (modeName == Bpsk31Decoder::modeName()) {
        m_bpsk31Decoder->processAudioBlock(conditionedBlock);
        return;
    }

    if (modeName == MfskDecoder::modeName()) {
        m_mfskDecoder->processAudioBlock(conditionedBlock);
        return;
    }

    if (modeName == CwDecoder::modeName()) {
        // CW uses one full-passband skimmer engine.  The UI-selected A/B
        // markers decide which decoded streams are promoted to the RX textbox.
        if (m_cwDecoder != nullptr) {
            m_cwDecoder->processAudioBlock(block);
        }
        return;
    }

    if (modeName == HellschreiberDecoder::modeName()) {
        m_hellDecoder->processAudioBlock(conditionedBlock);
        return;
    }

}


void MainWindow::applyMsk144Settings()
{
    if (m_msk144Decoder == nullptr) {
        return;
    }
    const int period = (m_cmbMsk144Period != nullptr) ? m_cmbMsk144Period->currentData().toInt() : 15;
    const int depth = (m_cmbMsk144DecodeDepth != nullptr) ? m_cmbMsk144DecodeDepth->currentData().toInt() : 2;
    const int rxHz = (m_spinMsk144RxFreq != nullptr) ? m_spinMsk144RxFreq->value() : 1500;
    const int dfTol = (m_spinMsk144DfTolerance != nullptr) ? m_spinMsk144DfTolerance->value() : 100;
    m_msk144Decoder->setPeriodSeconds(period);
    m_msk144Decoder->setDecodeDepth(depth);
    m_msk144Decoder->setRxFrequencyHz(rxHz);
    m_msk144Decoder->setDfToleranceHz(dfTol);
    m_msk144Decoder->setShortMessagesEnabled(m_chkMsk144ShortMessages != nullptr && m_chkMsk144ShortMessages->isChecked());
    m_msk144Decoder->setSwlEnabled(m_chkMsk144Swl != nullptr && m_chkMsk144Swl->isChecked());
    m_msk144Decoder->setContestModeEnabled(m_chkMsk144Contest != nullptr && m_chkMsk144Contest->isChecked());
    m_msk144Decoder->setMyCall(stationCallsign());
    m_msk144Decoder->setDxCall(m_editMsk144DxCall != nullptr ? m_editMsk144DxCall->text() : QString());
    if (m_lblMsk144Status != nullptr) {
        m_lblMsk144Status->setText(QStringLiteral("MSK144 RX: %1 s, %2, RX %3 Hz, DF ±%4 Hz; TX center 1500 Hz")
                                       .arg(period)
                                       .arg(depth <= 1 ? QStringLiteral("Fast") : (depth == 2 ? QStringLiteral("Normal") : QStringLiteral("Deep")))
                                       .arg(rxHz)
                                       .arg(dfTol));
    }
    updateWaterfallMarkers();
}

void MainWindow::refreshMsk144StandardMessages()
{
    if (m_tableMsk144TxMessages == nullptr) {
        return;
    }
    const QString myCall = stationCallsign().isEmpty() ? QStringLiteral("MYCALL") : stationCallsign();
    const QString myGrid = stationLocator().left(4).toUpper();
    const QString dxCall = (m_editMsk144DxCall != nullptr) ? m_editMsk144DxCall->text().trimmed().toUpper() : QString();
    const QString dxGrid = (m_editMsk144DxGrid != nullptr) ? m_editMsk144DxGrid->text().trimmed().left(4).toUpper() : QString();
    const bool contest = m_chkMsk144Contest != nullptr && m_chkMsk144Contest->isChecked();
    const bool sh = m_chkMsk144ShortMessages != nullptr && m_chkMsk144ShortMessages->isChecked();

    QStringList msgs;
    msgs << QStringLiteral("CQ %1 %2").arg(myCall, myGrid.isEmpty() ? QStringLiteral("JN61") : myGrid);
    if (!dxCall.isEmpty()) {
        msgs << QStringLiteral("%1 %2 %3").arg(dxCall, myCall, myGrid.isEmpty() ? QStringLiteral("JN61") : myGrid);
        if (contest) {
            msgs << QStringLiteral("%1 %2 R %3").arg(dxCall, myCall, dxGrid.isEmpty() ? QStringLiteral("JN61") : dxGrid);
            msgs << QStringLiteral("%1 %2 RRR").arg(dxCall, myCall);
            msgs << QStringLiteral("CQ %1 %2").arg(myCall, myGrid.isEmpty() ? QStringLiteral("JN61") : myGrid);
        } else if (sh) {
            msgs << QStringLiteral("%1 %2 -01").arg(dxCall, myCall);
            msgs << QStringLiteral("<%1 %2> R+00").arg(dxCall, myCall);
            msgs << QStringLiteral("<%1 %2> RRR").arg(dxCall, myCall);
            msgs << QStringLiteral("<%1 %2> 73").arg(dxCall, myCall);
        } else {
            msgs << QStringLiteral("%1 %2 -01").arg(dxCall, myCall);
            msgs << QStringLiteral("%1 %2 R+00").arg(dxCall, myCall);
            msgs << QStringLiteral("%1 %2 RRR").arg(dxCall, myCall);
            msgs << QStringLiteral("%1 %2 73").arg(dxCall, myCall);
        }
    } else {
        msgs << QStringLiteral("CQ %1 %2").arg(myCall, myGrid.isEmpty() ? QStringLiteral("JN61") : myGrid);
        msgs << QStringLiteral("%1 TEST").arg(myCall);
        msgs << QStringLiteral("%1 73").arg(myCall);
    }
    while (msgs.size() < 7) msgs << QString();

    const int oldRow = m_tableMsk144TxMessages->currentRow();
    m_tableMsk144TxMessages->setRowCount(7);
    for (int r = 0; r < 7; ++r) {
        QTableWidgetItem *txItem = new QTableWidgetItem(QStringLiteral("Tx%1").arg(r + 1));
        txItem->setFlags(txItem->flags() & ~Qt::ItemIsEditable);
        m_tableMsk144TxMessages->setItem(r, 0, txItem);
        m_tableMsk144TxMessages->setItem(r, 1, new QTableWidgetItem(msgs.value(r).trimmed()));
    }
    m_tableMsk144TxMessages->selectRow(qBound(0, oldRow < 0 ? 0 : oldRow, 6));
    updateTxPreview();
}

void MainWindow::startMsk144RxShell()
{
    if (ui != nullptr && ui->cmbMode != nullptr && !Msk144Mode::isMode(ui->cmbMode->currentText())) {
        ui->cmbMode->setCurrentText(Msk144Mode::modeName());
    }
    applyMsk144Settings();
    startRx();
}

void MainWindow::startMsk144TxShell()
{
    if (ui != nullptr && ui->cmbMode != nullptr && !Msk144Mode::isMode(ui->cmbMode->currentText())) {
        ui->cmbMode->setCurrentText(Msk144Mode::modeName());
    }
    refreshMsk144StandardMessages();
    applyMsk144Settings();
    startImageTx();
}

void MainWindow::stopMsk144Shell()
{
    if (m_txRunning) {
        stopImageTx();
        return;
    }
    if (m_rxRunning) {
        stopRx();
    }
    if (m_msk144Decoder != nullptr) {
        m_msk144Decoder->flushPeriod();
    }
}

void MainWindow::handleMsk144DecodeReady(const Msk144Decode &decode)
{
    if (m_tableMsk144Rx != nullptr) {
        const int row = m_tableMsk144Rx->rowCount();
        m_tableMsk144Rx->insertRow(row);
        m_tableMsk144Rx->setItem(row, 0, new QTableWidgetItem(decode.utc.time().toString(QStringLiteral("HHmmss"))));
        m_tableMsk144Rx->setItem(row, 1, new QTableWidgetItem(QString::number(decode.snrDb)));
        m_tableMsk144Rx->setItem(row, 2, new QTableWidgetItem(QString::number(decode.tSeconds, 'f', 1)));
        m_tableMsk144Rx->setItem(row, 3, new QTableWidgetItem(QString::number(decode.dfHz)));
        m_tableMsk144Rx->setItem(row, 4, new QTableWidgetItem(decode.message));
        m_tableMsk144Rx->scrollToBottom();
    }

    appendLog(QStringLiteral("MSK144 decode: %1 dB T %2 DF %3: %4")
                  .arg(decode.snrDb)
                  .arg(decode.tSeconds, 0, 'f', 1)
                  .arg(decode.dfHz)
                  .arg(decode.message));

    const QString call = extractCallsignFromText(decode.message);
    if (!call.isEmpty()) {
        if (m_editMsk144DxCall != nullptr && m_editMsk144DxCall->text().trimmed().isEmpty() && call != stationCallsign()) {
            m_editMsk144DxCall->setText(call);
        }
        if (m_msk144QsoForm != nullptr && m_msk144QsoForm->callsign != nullptr &&
            m_msk144QsoForm->callsign->text().trimmed().isEmpty() && call != stationCallsign()) {
            m_msk144QsoForm->callsign->setText(call);
        }
    }
    refreshMsk144StandardMessages();
    updateMsk144SequencerFromDecode(decode.message, decode.snrDb);
}


void MainWindow::updateMsk144SequencerFromDecode(const QString &message, int snrDb)
{
    if (m_tableMsk144TxMessages == nullptr || m_lblMsk144SequencerStatus == nullptr) {
        return;
    }
    const QString my = stationCallsign().trimmed().toUpper();
    const QString msg = message.trimmed().toUpper();
    if (my.isEmpty() || msg.isEmpty() || !msg.contains(my)) {
        return;
    }

    const QString dx = extractCallsignFromText(msg);
    int row = -1;
    QString reason;
    if (msg.contains(QStringLiteral(" RR73")) || msg.contains(QStringLiteral(" RRR")) || msg.endsWith(QStringLiteral(" 73"))) {
        row = 5; // courtesy/final 73
        reason = uiText("seq_completed", "QSO completed / send 73 if needed");
    } else if (msg.contains(QRegularExpression(QStringLiteral("\\bR[+-]?[0-9]{2}\\b")))) {
        row = 4; // acknowledge with RR73
        reason = uiText("seq_send_rr73", "received R-report; selected RR73");
    } else if (msg.contains(QRegularExpression(QStringLiteral("\\b[+-]?[0-9]{2}\\b")))) {
        row = 3; // reply with R-report
        reason = uiText("seq_send_r_report", "received report; selected R-report");
    } else if (!dx.isEmpty()) {
        row = 2; // send report after calls/grid exchange
        reason = uiText("seq_send_report", "received directed call/grid; selected report");
    }

    if (row >= 0) {
        row = qBound(0, row, m_tableMsk144TxMessages->rowCount() - 1);
        m_tableMsk144TxMessages->selectRow(row);
        QTableWidgetItem *item = m_tableMsk144TxMessages->item(row, 1);
        const QString selected = item != nullptr ? item->text().trimmed() : QString();
        m_lblMsk144SequencerStatus->setText(QStringLiteral("Sequencer: %1 | Tx%2 %3 | SNR %4 dB")
                                                .arg(reason)
                                                .arg(row + 1)
                                                .arg(selected)
                                                .arg(snrDb));
        updateTxPreview();
    }
}

void MainWindow::handleMsk144Ping(double frequencyHz, int snrDb, double tSeconds)
{
    // Keep ping detection as a textual/status hint only.  The waterfall must not
    // accumulate transient MSK dB labels: they flicker, hide real traces, and are
    // not operator-selectable markers.  Decoded messages remain in the RX table.
    if (m_lblMsk144PeriodStatus != nullptr) {
        m_lblMsk144PeriodStatus->setText(QStringLiteral("MSK144 ping near %1 Hz: %2 dB at T=%3 s")
                                             .arg(qRound(frequencyHz))
                                             .arg(snrDb)
                                             .arg(tSeconds, 0, 'f', 1));
    }
}

void MainWindow::clearMsk144RxTable()
{
    if (m_tableMsk144Rx != nullptr) {
        m_tableMsk144Rx->setRowCount(0);
    }
    m_msk144PingOverlays.clear();
    updateWaterfallMarkers();
}

Q65Mode::Submode MainWindow::currentQ65Submode() const
{
    if (m_cmbQ65Submode != nullptr) {
        const int value = m_cmbQ65Submode->currentData().toInt();
        switch (value) {
        case 2: return Q65Mode::Submode::B;
        case 4: return Q65Mode::Submode::C;
        case 8: return Q65Mode::Submode::D;
        default: return Q65Mode::Submode::A;
        }
    }
    if (ui == nullptr || ui->cmbMode == nullptr) return Q65Mode::Submode::A;
    return Q65Mode::submodeForMode(ui->cmbMode->currentText());
}

void MainWindow::applyQ65Settings()
{
    if (m_q65Decoder == nullptr) return;
    const int period = (m_cmbQ65Period != nullptr) ? m_cmbQ65Period->currentData().toInt() : 60;
    const int depth = (m_cmbQ65DecodeDepth != nullptr) ? m_cmbQ65DecodeDepth->currentData().toInt() : 2;
    const int rxHz = (m_spinQ65RxFreq != nullptr) ? m_spinQ65RxFreq->value() : 1500;
    const int dfTol = (m_spinQ65DfTolerance != nullptr) ? m_spinQ65DfTolerance->value() : 100;
    const Q65Mode::Submode submode = currentQ65Submode();

    m_q65Decoder->setSubmode(submode);
    m_q65Decoder->setPeriodSeconds(period);
    m_q65Decoder->setDecodeDepth(depth);
    m_q65Decoder->setRxFrequencyHz(rxHz);
    m_q65Decoder->setDfToleranceHz(dfTol);
    m_q65Decoder->setAveragingEnabled(m_chkQ65AverageDecode != nullptr && m_chkQ65AverageDecode->isChecked());
    m_q65Decoder->setAutoClearAverages(m_chkQ65AutoClearAvg != nullptr && m_chkQ65AutoClearAvg->isChecked());
    m_q65Decoder->setSingleDecode(m_chkQ65SingleDecode != nullptr && m_chkQ65SingleDecode->isChecked());
    m_q65Decoder->setApDecodeEnabled(m_chkQ65ApDecode != nullptr && m_chkQ65ApDecode->isChecked());
    m_q65Decoder->setMaxDriftEnabled(m_chkQ65MaxDrift != nullptr && m_chkQ65MaxDrift->isChecked());
    m_q65Decoder->setEmeDelayEnabled(m_chkQ65EmeDelay != nullptr && m_chkQ65EmeDelay->isChecked());
    m_q65Decoder->setMyCall(stationCallsign());
    m_q65Decoder->setDxCall(m_editQ65DxCall != nullptr ? m_editQ65DxCall->text() : QString());
    m_q65Decoder->setDxGrid(m_editQ65DxGrid != nullptr ? m_editQ65DxGrid->text() : QString());

    if (m_lblQ65Status != nullptr) {
        m_lblQ65Status->setText(QStringLiteral("%1 RX: %2 s, %3, RX %4 Hz, DF ±%5 Hz; TX %6 Hz")
                                    .arg(Q65Mode::modeName(submode))
                                    .arg(period)
                                    .arg(depth <= 1 ? QStringLiteral("Fast") : (depth == 2 ? QStringLiteral("Normal") : QStringLiteral("Deep")))
                                    .arg(rxHz)
                                    .arg(dfTol)
                                    .arg(m_spinQ65TxFreq != nullptr ? m_spinQ65TxFreq->value() : 1500));
    }
    updateWaterfallMarkers();
}

void MainWindow::refreshQ65StandardMessages()
{
    if (m_tableQ65TxMessages == nullptr) return;
    const QString myCall = stationCallsign().isEmpty() ? QStringLiteral("MYCALL") : stationCallsign();
    const QString myGrid = stationLocator().left(4).toUpper();
    const QString dxCall = (m_editQ65DxCall != nullptr) ? m_editQ65DxCall->text().trimmed().toUpper() : QString();
    const QString dxGrid = (m_editQ65DxGrid != nullptr) ? m_editQ65DxGrid->text().trimmed().left(4).toUpper() : QString();

    QStringList msgs;
    msgs << QStringLiteral("CQ %1 %2").arg(myCall, myGrid.isEmpty() ? QStringLiteral("JN61") : myGrid);
    if (!dxCall.isEmpty()) {
        msgs << QStringLiteral("%1 %2 %3").arg(dxCall, myCall, myGrid.isEmpty() ? QStringLiteral("JN61") : myGrid);
        msgs << QStringLiteral("%1 %2 -10").arg(dxCall, myCall);
        msgs << QStringLiteral("%1 %2 R-10").arg(dxCall, myCall);
        msgs << QStringLiteral("%1 %2 RR73").arg(dxCall, myCall);
        msgs << QStringLiteral("%1 %2 73").arg(dxCall, myCall);
        if (!dxGrid.isEmpty()) msgs << QStringLiteral("%1 %2 %3").arg(dxCall, myCall, dxGrid);
    } else {
        msgs << QStringLiteral("CQ %1 %2").arg(myCall, myGrid.isEmpty() ? QStringLiteral("JN61") : myGrid);
        msgs << QStringLiteral("%1 TEST").arg(myCall);
        msgs << QStringLiteral("%1 73").arg(myCall);
    }
    while (msgs.size() < 7) msgs << QString();

    const int oldRow = m_tableQ65TxMessages->currentRow();
    m_tableQ65TxMessages->setRowCount(7);
    for (int r = 0; r < 7; ++r) {
        QTableWidgetItem *txItem = new QTableWidgetItem(QStringLiteral("Tx%1").arg(r + 1));
        txItem->setFlags(txItem->flags() & ~Qt::ItemIsEditable);
        m_tableQ65TxMessages->setItem(r, 0, txItem);
        m_tableQ65TxMessages->setItem(r, 1, new QTableWidgetItem(msgs.value(r).trimmed()));
    }
    m_tableQ65TxMessages->selectRow(qBound(0, oldRow < 0 ? 0 : oldRow, 6));
    updateTxPreview();
}

void MainWindow::startQ65RxShell()
{
    if (ui != nullptr && ui->cmbMode != nullptr && !Q65Mode::isFamilyMode(ui->cmbMode->currentText())) {
        ui->cmbMode->setCurrentText(Q65Mode::familyName());
    }
    applyQ65Settings();
    startRx();
}

void MainWindow::startQ65TxShell()
{
    if (ui != nullptr && ui->cmbMode != nullptr && !Q65Mode::isFamilyMode(ui->cmbMode->currentText())) {
        ui->cmbMode->setCurrentText(Q65Mode::familyName());
    }
    refreshQ65StandardMessages();
    applyQ65Settings();
    startImageTx();
}

void MainWindow::stopQ65Shell()
{
    if (m_txRunning) { stopImageTx(); return; }
    if (m_rxRunning) stopRx();
    if (m_q65Decoder != nullptr) m_q65Decoder->flushPeriod();
}

void MainWindow::handleQ65DecodeReady(const Q65Decode &decode)
{
    if (m_tableQ65Rx != nullptr) {
        const int row = m_tableQ65Rx->rowCount();
        m_tableQ65Rx->insertRow(row);
        m_tableQ65Rx->setItem(row, 0, new QTableWidgetItem(decode.utc.time().toString(QStringLiteral("HHmmss"))));
        m_tableQ65Rx->setItem(row, 1, new QTableWidgetItem(QString::number(decode.snrDb)));
        m_tableQ65Rx->setItem(row, 2, new QTableWidgetItem(QString::number(decode.dtSeconds, 'f', 1)));
        m_tableQ65Rx->setItem(row, 3, new QTableWidgetItem(QString::number(decode.dfHz)));
        m_tableQ65Rx->setItem(row, 4, new QTableWidgetItem(decode.message));
        m_tableQ65Rx->scrollToBottom();
    }
    appendLog(QStringLiteral("Q65 decode: %1 dB DT %2 DF %3: %4")
                  .arg(decode.snrDb)
                  .arg(decode.dtSeconds, 0, 'f', 1)
                  .arg(decode.dfHz)
                  .arg(decode.message));
    const QString call = extractCallsignFromText(decode.message);
    if (!call.isEmpty() && call != stationCallsign()) {
        if (m_editQ65DxCall != nullptr && m_editQ65DxCall->text().trimmed().isEmpty()) m_editQ65DxCall->setText(call);
        if (m_q65QsoForm != nullptr && m_q65QsoForm->callsign != nullptr && m_q65QsoForm->callsign->text().trimmed().isEmpty()) {
            m_q65QsoForm->callsign->setText(call);
        }
    }
    refreshQ65StandardMessages();
    updateQ65SequencerFromDecode(decode.message, decode.snrDb);
}


void MainWindow::updateQ65SequencerFromDecode(const QString &message, int snrDb)
{
    if (m_tableQ65TxMessages == nullptr || m_lblQ65SequencerStatus == nullptr) {
        return;
    }
    const QString my = stationCallsign().trimmed().toUpper();
    const QString msg = message.trimmed().toUpper();
    if (my.isEmpty() || msg.isEmpty() || !msg.contains(my)) {
        return;
    }

    const QString dx = extractCallsignFromText(msg);
    int row = -1;
    QString reason;
    if (msg.contains(QStringLiteral(" RR73")) || msg.contains(QStringLiteral(" RRR")) || msg.endsWith(QStringLiteral(" 73"))) {
        row = 5;
        reason = uiText("seq_completed", "QSO completed / send 73 if needed");
    } else if (msg.contains(QRegularExpression(QStringLiteral("\\bR[+-]?[0-9]{2}\\b")))) {
        row = 4;
        reason = uiText("seq_send_rr73", "received R-report; selected RR73");
    } else if (msg.contains(QRegularExpression(QStringLiteral("\\b[+-]?[0-9]{2}\\b")))) {
        row = 3;
        reason = uiText("seq_send_r_report", "received report; selected R-report");
    } else if (!dx.isEmpty()) {
        row = 2;
        reason = uiText("seq_send_report", "received directed call/grid; selected report");
    }

    if (row >= 0) {
        row = qBound(0, row, m_tableQ65TxMessages->rowCount() - 1);
        m_tableQ65TxMessages->selectRow(row);
        QTableWidgetItem *item = m_tableQ65TxMessages->item(row, 1);
        const QString selected = item != nullptr ? item->text().trimmed() : QString();
        m_lblQ65SequencerStatus->setText(QStringLiteral("Sequencer: %1 | Tx%2 %3 | SNR %4 dB")
                                             .arg(reason)
                                             .arg(row + 1)
                                             .arg(selected)
                                             .arg(snrDb));
        updateTxPreview();
    }
}

void MainWindow::clearQ65RxTable()
{
    if (m_tableQ65Rx != nullptr) m_tableQ65Rx->setRowCount(0);
    if (m_q65Decoder != nullptr) m_q65Decoder->clearAverages();
}

void MainWindow::applyWeatherFaxSettings()
{
    if (m_weatherFaxDecoder == nullptr) {
        return;
    }

    const int lpm = ui->cmbFaxLpm->currentText().toInt();
    const int blackHz = ui->spinFaxBlackHz->value();
    const int whiteHz = ui->spinFaxWhiteHz->value();
    const bool autoStartPhasing = ui->chkFaxAutoStartPhasing->isChecked();
    const bool autoToneTracking = ui->chkFaxAutoToneTracking->isChecked();
    const bool inputBandpass = ui->chkFaxInputBandpass->isChecked();
    const bool autoSlant = false;
    const double manualSlantPpm = 0.0;
    const QString linePresetKey = ui->cmbFaxLinePreset->currentData().toString();
    const int imageLines = qBound(256, m_settings.weatherFaxImageLines, 12000);
    const bool endOfSignal = ui->chkFaxEndOfSignal->isChecked();
    const int endTimeoutSec = ui->spinFaxEndTimeoutSec->value();
    const bool autoSave = ui->chkFaxAutoSave->isChecked();
    const QString outputFolder = ui->editFaxOutputFolder->text().trimmed().isEmpty()
                                     ? defaultWeatherFaxOutputFolder()
                                     : ui->editFaxOutputFolder->text().trimmed();

    m_weatherFaxDecoder->setLpm(lpm);
    m_weatherFaxDecoder->setAutoStartEnabled(autoStartPhasing);
    m_weatherFaxDecoder->setAutoToneTrackingEnabled(autoToneTracking);
    m_weatherFaxDecoder->setInputBandpassEnabled(inputBandpass);
    m_weatherFaxDecoder->setAutoSlantCorrectionEnabled(false);
    m_weatherFaxDecoder->setManualSlantPpm(0.0);
    m_weatherFaxDecoder->setTargetImageLines(imageLines);
    m_weatherFaxDecoder->setEndOfSignalCompletionEnabled(endOfSignal);
    m_weatherFaxDecoder->setEndOfSignalTimeoutSec(endTimeoutSec);

    if (whiteHz <= blackHz + 50) {
        m_weatherFaxDecoder->setToneRange(static_cast<double>(blackHz),
                                          static_cast<double>(whiteHz));
        return;
    }

    m_weatherFaxDecoder->setToneRange(static_cast<double>(blackHz),
                                      static_cast<double>(whiteHz));

    m_settings.weatherFaxLpm = lpm;
    m_settings.weatherFaxBlackHz = blackHz;
    m_settings.weatherFaxWhiteHz = whiteHz;
    m_settings.weatherFaxAutoStartPhasing = autoStartPhasing;
    m_settings.weatherFaxAutoToneTracking = autoToneTracking;
    m_settings.weatherFaxInputBandpass = inputBandpass;
    m_settings.weatherFaxAutoSlantCorrection = autoSlant;
    m_settings.weatherFaxManualSlantPpm = manualSlantPpm;
    m_settings.weatherFaxLinePreset = linePresetKey;
    m_settings.weatherFaxImageLines = imageLines;
    m_settings.weatherFaxAutoSave = autoSave;
    m_settings.weatherFaxOutputFolder = outputFolder;
    m_settings.weatherFaxEndOfSignal = endOfSignal;
    m_settings.weatherFaxEndOfSignalTimeoutSec = endTimeoutSec;

    updateLinePresetSelectionFromCurrentValues();
    m_settings.weatherFaxLinePreset = ui->cmbFaxLinePreset->currentData().toString();
    savePersistentSettings();
    updateCwDualRxStatusLabel();
    updateWaterfallMarkers();
    updateTxPreview();
}

void MainWindow::applySstvSettings()
{
    if (m_sstvDecoder == nullptr) {
        return;
    }

    const QString modeName = ui->cmbSstvMode->currentText().trimmed().isEmpty()
                                 ? QString("Martin M1")
                                 : ui->cmbSstvMode->currentText().trimmed();
    const bool autoSync = ui->chkSstvAutoSync->isChecked();
    const int horizontalShiftPixels = ui->spinSstvHorizontalShift->value();
    const int redShiftPixels = ui->spinSstvRedShift->value();
    const int blueShiftPixels = ui->spinSstvBlueShift->value();

    m_sstvDecoder->setModeName(modeName);
    m_sstvDecoder->setAutoSyncEnabled(autoSync);
    m_sstvDecoder->setHorizontalShiftPixels(horizontalShiftPixels);
    m_sstvDecoder->setColorShiftPixels(redShiftPixels, blueShiftPixels);

    m_settings.sstvMode = modeName;
    m_settings.sstvAutoSync = autoSync;
    m_settings.sstvHorizontalShiftPixels = horizontalShiftPixels;
    m_settings.sstvRedShiftPixels = redShiftPixels;
    m_settings.sstvBlueShiftPixels = blueShiftPixels;

    savePersistentSettings();
    updateCwDualRxStatusLabel();
    updateWaterfallMarkers();
    updateTxPreview();
}


void MainWindow::applyRttySettings()
{
    if (m_rttyDecoder == nullptr ||
        m_spinRttyBaud == nullptr ||
        m_spinRttyShiftHz == nullptr ||
        m_spinRttyMarkHz == nullptr ||
        m_chkRttyReverse == nullptr ||
        m_chkRttyAutoReverse == nullptr ||
        m_chkRttyAfc == nullptr ||
        m_spinRttyAfcRangeHz == nullptr) {
        return;
    }

    const QString presetKey = (m_cmbRttyPreset != nullptr)
                                  ? m_cmbRttyPreset->currentData().toString()
                                  : QString("CUSTOM");
    const double baud = m_spinRttyBaud->value();
    const int shiftHz = m_spinRttyShiftHz->value();
    const int markHz = m_spinRttyMarkHz->value();
    const int spaceHz = markHz + shiftHz;
    const bool reverse = m_chkRttyReverse->isChecked();
    const bool autoReverse = (m_chkRttyAutoReverse != nullptr) ? m_chkRttyAutoReverse->isChecked() : true;
    const bool afc = (m_chkRttyAfc != nullptr) ? m_chkRttyAfc->isChecked() : true;
    const int afcRangeHz = (m_spinRttyAfcRangeHz != nullptr) ? m_spinRttyAfcRangeHz->value() : 20;
    const bool multiDecode = (m_chkRttyMultiDecode != nullptr)
                                 ? m_chkRttyMultiDecode->isChecked()
                                 : m_settings.rttyMultiDecodeEnabled;
    const bool overlayCallsigns = (m_chkRttyOverlayCallsigns != nullptr)
                                      ? m_chkRttyOverlayCallsigns->isChecked()
                                      : m_settings.rttyOverlayCallsignsEnabled;
    const bool contestEnhanced = (m_chkRttyContestEnhanced != nullptr)
                                     ? m_chkRttyContestEnhanced->isChecked()
                                     : m_settings.rttyContestEnhancedEnabled;
    const bool secondPass = (m_chkRttySecondPass != nullptr)
                                ? m_chkRttySecondPass->isChecked()
                                : m_settings.rttySecondPassEnabled;
    const int maxDecoders = (m_spinRttyMaxDecoders != nullptr)
                                ? m_spinRttyMaxDecoders->value()
                                : m_settings.rttyMaxParallelDecoders;

    m_rttyDecoder->setBaudRate(baud);
    m_rttyDecoder->setTones(static_cast<double>(markHz), static_cast<double>(spaceHz));
    m_rttyDecoder->setReverse(reverse);
    m_rttyDecoder->setAutoReverseEnabled(autoReverse);

    m_settings.rttyPreset = presetKey;
    m_settings.rttyBaudRate = baud;
    m_settings.rttyShiftHz = shiftHz;
    m_settings.rttyMarkHz = markHz;
    m_settings.rttyReverse = reverse;
    m_settings.rttyAutoReverseEnabled = autoReverse;
    m_settings.rttyAfcEnabled = afc;
    m_settings.rttyAfcRangeHz = afcRangeHz;
    m_settings.rttyMultiDecodeEnabled = multiDecode;
    m_settings.rttyOverlayCallsignsEnabled = overlayCallsigns;
    m_settings.rttyContestEnhancedEnabled = contestEnhanced;
    m_settings.rttySecondPassEnabled = secondPass;
    m_settings.rttyMaxParallelDecoders = maxDecoders;

    if (m_rttyMultiDecoder != nullptr) {
        m_rttyMultiDecoder->configure(baud, shiftHz, reverse, multiDecode, overlayCallsigns, contestEnhanced, secondPass, maxDecoders);
    }
    if (m_chkRttyOverlayCallsigns != nullptr) m_chkRttyOverlayCallsigns->setEnabled(multiDecode);
    if (m_chkRttyContestEnhanced != nullptr) m_chkRttyContestEnhanced->setEnabled(multiDecode);
    if (m_chkRttySecondPass != nullptr) m_chkRttySecondPass->setEnabled(multiDecode);
    if (m_spinRttyMaxDecoders != nullptr) m_spinRttyMaxDecoders->setEnabled(multiDecode);

    savePersistentSettings();
    updateCwDualRxStatusLabel();
    updateWaterfallMarkers();
    updateTxPreview();
}

void MainWindow::clearRttyRxText()
{
    if (m_rttyDecoder != nullptr) {
        m_rttyDecoder->reset();
    }
    if (m_rttyMultiDecoder != nullptr) {
        m_rttyMultiDecoder->reset();
    }
    m_rttyWaterfallCallouts.clear();
    if (m_waterfallWidget != nullptr && ui != nullptr && ui->cmbMode != nullptr &&
        ui->cmbMode->currentText() == RttyDecoder::modeName()) {
        m_waterfallWidget->setTextOverlays({});
    }

    m_lastRttyDecodedText.clear();
    m_rttyPendingRxLineBreak = false;

    if (m_txtRttyRx != nullptr) {
        m_txtRttyRx->clear();
    }
}

void MainWindow::clearRttyTxText()
{
    if (m_txtRttyTx != nullptr) {
        if (m_activeTextTxEditor == m_txtRttyTx) {
            endTextTxHighlight();
        }
        m_txtRttyTx->clear();
    }

    updateTxPreview();
}

void MainWindow::loadRttyTxTextFile()
{
    const QString fileName = QFileDialog::getOpenFileName(
        this,
        "Load RTTY TX text",
        QDir::homePath(),
        "Text files (*.txt *.log);;All files (*)"
        );

    if (fileName.isEmpty()) {
        return;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this,
                             "Load RTTY text failed",
                             "Unable to open the selected text file.");
        appendLog("RTTY text load failed: " + file.errorString());
        return;
    }

    QTextStream stream(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    stream.setCodec("UTF-8");
#endif
    const QString text = stream.readAll();

    if (m_txtRttyTx != nullptr) {
        m_txtRttyTx->setPlainText(text);
    }

    appendLog("RTTY TX text loaded: " + fileName);
    updateTxPreview();
}

void MainWindow::handleRttyTextUpdated(const QString &text)
{
    if (m_txtRttyRx == nullptr || text.isEmpty()) {
        return;
    }

    /*
     * The decoder now emits incremental characters.  RTTY still carries CR
     * and LF as separate Baudot control codes, and noisy start-bit recovery can
     * occasionally decode a lone LF between printable letters.  Require a CR/LF
     * pair for an actual terminal newline; a lone CR or LF is discarded instead
     * of turning every received character into a vertical column.
     */
    appendRxTextTerminal(m_txtRttyRx, text, true, &m_rttyPendingRxLineBreak);
}

void MainWindow::handleRttyPresetChanged(int index)
{
    Q_UNUSED(index)

    if (m_cmbRttyPreset == nullptr ||
        m_spinRttyBaud == nullptr ||
        m_spinRttyShiftHz == nullptr ||
        m_spinRttyMarkHz == nullptr ||
        m_chkRttyReverse == nullptr ||
        m_chkRttyAutoReverse == nullptr ||
        m_chkRttyAfc == nullptr ||
        m_spinRttyAfcRangeHz == nullptr) {
        return;
    }

    const RttyPreset preset = rttyPresetByKey(m_cmbRttyPreset->currentData().toString());
    m_cmbRttyPreset->setToolTip(preset.details);
    m_cmbRttyPreset->setStatusTip(preset.details);

    if (preset.key != "CUSTOM") {
        const QSignalBlocker blockBaud(m_spinRttyBaud);
        const QSignalBlocker blockShift(m_spinRttyShiftHz);
        const QSignalBlocker blockMark(m_spinRttyMarkHz);
        const QSignalBlocker blockReverse(m_chkRttyReverse);
        const QSignalBlocker blockAfc(m_chkRttyAfc);
        const QSignalBlocker blockAfcRange(m_spinRttyAfcRangeHz);
        m_spinRttyBaud->setValue(preset.baud);
        m_spinRttyShiftHz->setValue(preset.shiftHz);
        m_spinRttyMarkHz->setValue(preset.markHz);
        m_chkRttyReverse->setChecked(preset.reverse);
    }

    applyRttySettings();
}


void MainWindow::applyBpsk31Settings()
{
    if (m_bpsk31Decoder == nullptr ||
        m_cmbBpsk31Variant == nullptr ||
        m_spinBpsk31ToneHz == nullptr ||
        m_chkBpsk31Afc == nullptr ||
        m_spinBpsk31AfcRangeHz == nullptr ||
        m_chkBpsk31Invert == nullptr) {
        return;
    }

    const QString variant = m_cmbBpsk31Variant->currentData().toString();
    const double symbolRate = bpskSymbolRateForVariant(variant);
    const int toneHz = m_spinBpsk31ToneHz->value();
    const bool afc = m_chkBpsk31Afc->isChecked();
    const int afcRangeHz = m_spinBpsk31AfcRangeHz->value();
    const bool invert = m_chkBpsk31Invert->isChecked();
    const bool coherentTracking = (m_chkDspBpskCoherentTracking != nullptr)
                                      ? m_chkDspBpskCoherentTracking->isChecked()
                                      : m_settings.bpsk31CoherentTrackingEnabled;

    m_bpsk31Decoder->setSymbolRate(symbolRate);
    m_bpsk31Decoder->setQpskMode(pskVariantIsQpsk(variant));
    m_bpsk31Decoder->setToneHz(static_cast<double>(toneHz));
    m_bpsk31Decoder->setAfcEnabled(afc);
    m_bpsk31Decoder->setAfcRangeHz(static_cast<double>(afcRangeHz));
    m_bpsk31Decoder->setInvertBits(invert);
    m_bpsk31Decoder->setCoherentTrackingEnabled(coherentTracking);

    m_settings.bpsk31Variant = variant.isEmpty() ? QString("BPSK31") : variant;
    m_settings.bpsk31ToneHz = toneHz;
    m_settings.bpsk31AfcEnabled = afc;
    m_settings.bpsk31AfcRangeHz = afcRangeHz;
    m_settings.bpsk31InvertBits = invert;
    m_settings.bpsk31CoherentTrackingEnabled = coherentTracking;

    savePersistentSettings();
    updateCwDualRxStatusLabel();
    updateWaterfallMarkers();
    updateTxPreview();
}

void MainWindow::clearBpsk31RxText()
{
    if (m_bpsk31Decoder != nullptr) {
        m_bpsk31Decoder->reset();
    }

    m_lastBpsk31DecodedText.clear();
    m_bpsk31PendingRxLineBreak = false;

    if (m_txtBpsk31Rx != nullptr) {
        m_txtBpsk31Rx->clear();
    }
}

void MainWindow::clearBpsk31TxText()
{
    if (m_txtBpsk31Tx != nullptr) {
        if (m_activeTextTxEditor == m_txtBpsk31Tx) {
            endTextTxHighlight();
        }
        m_txtBpsk31Tx->clear();
    }

    updateTxPreview();
}

void MainWindow::loadBpsk31TxTextFile()
{
    const QString fileName = QFileDialog::getOpenFileName(
        this,
        "Load PSK TX text",
        QDir::homePath(),
        "Text files (*.txt *.log);;All files (*)"
        );

    if (fileName.isEmpty()) {
        return;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this,
                             "Load PSK text failed",
                             "Unable to open the selected text file.");
        appendLog("PSK text load failed: " + file.errorString());
        return;
    }

    QTextStream stream(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    stream.setCodec("UTF-8");
#endif
    const QString text = stream.readAll();

    if (m_txtBpsk31Tx != nullptr) {
        m_txtBpsk31Tx->setPlainText(text);
    }

    appendLog("PSK TX text loaded: " + fileName);
    updateTxPreview();
}

void MainWindow::handleBpsk31TextUpdated(const QString &text)
{
    if (m_txtBpsk31Rx == nullptr || text.isEmpty()) {
        return;
    }

    /*
     * BPSK Varicode commonly emits printable characters one at a time. Feed
     * them to the terminal as a stream and only honour explicit line controls.
     */
    appendRxTextTerminal(m_txtBpsk31Rx, text, false, &m_bpsk31PendingRxLineBreak);
}


void MainWindow::applyMfskSettings()
{
    if (m_mfskDecoder == nullptr ||
        m_cmbMfskVariant == nullptr ||
        m_spinMfskCenterHz == nullptr ||
        m_chkMfskAfc == nullptr ||
        m_spinMfskAfcRangeHz == nullptr) {
        return;
    }

    const QString variantKey = m_cmbMfskVariant->currentData().toString().toUpper();
    const MfskDecoder::Variant variant = MfskDecoder::variantFromKey(variantKey);
    const int centerHz = m_spinMfskCenterHz->value();
    const bool afc = m_chkMfskAfc->isChecked();
    const int afcRangeHz = m_spinMfskAfcRangeHz->value();

    m_mfskDecoder->setVariant(variant);
    m_mfskDecoder->setCenterHz(static_cast<double>(centerHz));
    m_mfskDecoder->setAfcEnabled(afc);
    m_mfskDecoder->setAfcRangeHz(static_cast<double>(afcRangeHz));

    m_settings.mfskVariant = variantKey.isEmpty() ? QString("MFSK16") : variantKey;
    m_settings.mfskCenterHz = centerHz;
    m_settings.mfskAfcEnabled = afc;
    m_settings.mfskAfcRangeHz = afcRangeHz;

    savePersistentSettings();
    updateCwDualRxStatusLabel();
    updateWaterfallMarkers();
    updateTxPreview();
}

void MainWindow::clearMfskRxText()
{
    if (m_mfskDecoder != nullptr) {
        m_mfskDecoder->reset();
    }

    m_mfskPendingRxLineBreak = false;

    if (m_txtMfskRx != nullptr) {
        m_txtMfskRx->clear();
    }
}

void MainWindow::clearMfskTxText()
{
    if (m_txtMfskTx != nullptr) {
        if (m_activeTextTxEditor == m_txtMfskTx) {
            endTextTxHighlight();
        }
        m_txtMfskTx->clear();
    }

    updateTxPreview();
}

void MainWindow::loadMfskTxTextFile()
{
    const QString fileName = QFileDialog::getOpenFileName(
        this,
        "Load MFSK TX text",
        QDir::homePath(),
        "Text files (*.txt *.log);;All files (*)"
        );

    if (fileName.isEmpty()) {
        return;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this,
                             "Load MFSK text failed",
                             "Unable to open the selected text file.");
        appendLog("MFSK text load failed: " + file.errorString());
        return;
    }

    QTextStream stream(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    stream.setCodec("UTF-8");
#endif
    const QString text = stream.readAll();

    if (m_txtMfskTx != nullptr) {
        m_txtMfskTx->setPlainText(text);
    }

    appendLog("MFSK TX text loaded: " + fileName);
    updateTxPreview();
}

void MainWindow::handleMfskTextUpdated(const QString &text)
{
    if (m_txtMfskRx == nullptr || text.isEmpty()) {
        return;
    }

    appendRxTextTerminal(m_txtMfskRx, text, false, &m_mfskPendingRxLineBreak);
}

void MainWindow::applyCwSettings()
{
    if (m_cwDecoder == nullptr ||
        m_spinCwToneHz == nullptr ||
        m_spinCwWpm == nullptr ||
        m_chkCwAutoWpm == nullptr ||
        m_spinCwBandwidthHz == nullptr ||
        m_chkCwAfc == nullptr ||
        m_spinCwAfcRangeHz == nullptr) {
        return;
    }

    const int toneHz = m_spinCwToneHz->value();
    const int wpm = m_spinCwWpm->value();
    const bool autoWpm = m_chkCwAutoWpm->isChecked();
    const int bandwidthHz = m_spinCwBandwidthHz->value();
    const bool afc = m_chkCwAfc->isChecked();
    const int afcRangeHz = m_spinCwAfcRangeHz->value();
    const bool softwareAgc = false;

    m_cwDecoder->setToneHz(static_cast<double>(toneHz));
    m_cwDecoder->setSecondaryToneHz(static_cast<double>(m_cwSecondaryToneHz));
    m_cwDecoder->setSecondaryEnabled(m_cwSecondaryEnabled);
    m_cwDecoder->setAutoWpm(autoWpm);
    m_cwDecoder->setWpm(static_cast<double>(wpm));
    m_cwDecoder->setBandwidthHz(static_cast<double>(bandwidthHz));
    m_cwDecoder->setAfcEnabled(afc);
    m_cwDecoder->setAfcRangeHz(static_cast<double>(afcRangeHz));
    if (m_cwSecondaryDecoder != nullptr) {
        m_cwSecondaryDecoder->setToneHz(static_cast<double>(m_cwSecondaryToneHz));
        m_cwSecondaryDecoder->setAutoWpm(autoWpm);
        m_cwSecondaryDecoder->setWpm(static_cast<double>(wpm));
        m_cwSecondaryDecoder->setBandwidthHz(static_cast<double>(bandwidthHz));
        m_cwSecondaryDecoder->setAfcEnabled(afc);
        m_cwSecondaryDecoder->setAfcRangeHz(static_cast<double>(afcRangeHz));
    }
    m_settings.cwToneHz = toneHz;
    m_settings.cwSecondaryEnabled = m_cwSecondaryEnabled;
    m_settings.cwSecondaryToneHz = m_cwSecondaryToneHz;
    m_settings.cwWpm = wpm;
    m_settings.cwAutoWpm = autoWpm;
    m_settings.cwBandwidthHz = bandwidthHz;
    m_settings.cwAfcEnabled = afc;
    m_settings.cwAfcRangeHz = afcRangeHz;
    if (m_settings.cwAgcEnabled != softwareAgc) {
        m_settings.cwAgcEnabled = softwareAgc;
        m_decoderConditioner.reset();
    }

    savePersistentSettings();
    updateCwDualRxStatusLabel();
    updateWaterfallMarkers();
    updateTxPreview();
}

void MainWindow::clearCwRxText()
{
    if (m_cwDecoder != nullptr) {
        m_cwDecoder->reset();
    }
    if (m_cwSecondaryDecoder != nullptr) {
        m_cwSecondaryDecoder->reset();
    }
    m_cwPrimaryLineOpen = false;
    m_cwSecondaryLineOpen = false;
    m_cwCurrentLineChannel.clear();
    if (m_txtCwRx != nullptr) {
        m_txtCwRx->clear();
    }
}

void MainWindow::clearCwTxText()
{
    if (m_txtCwTx != nullptr) {
        if (m_activeTextTxEditor == m_txtCwTx) {
            endTextTxHighlight();
        }
        m_txtCwTx->clear();
    }

    updateTxPreview();
}

void MainWindow::loadCwTxTextFile()
{
    const QString fileName = QFileDialog::getOpenFileName(
        this,
        "Load CW TX text",
        QDir::homePath(),
        "Text files (*.txt *.log);;All files (*)"
        );

    if (fileName.isEmpty()) {
        return;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this,
                             "Load CW text failed",
                             "Unable to open the selected text file.");
        appendLog("CW text load failed: " + file.errorString());
        return;
    }

    QTextStream stream(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    stream.setCodec("UTF-8");
#endif
    const QString text = stream.readAll();
    if (m_txtCwTx != nullptr) {
        m_txtCwTx->setPlainText(text);
    }
    updateTxPreview();
}

void MainWindow::sendCwTxText()
{
    if (m_txRunning) {
        stopImageTx();
        return;
    }

    const QString text = (m_txtCwTx != nullptr) ? m_txtCwTx->toPlainText() : QString();
    startTextModeTx(text);
}

void MainWindow::handleCwTextUpdated(const QString &text)
{
    appendCwDecoderText(QStringLiteral("A"), text, QColor("#118a2a"), &m_cwPrimaryLineOpen);
}

void MainWindow::appendCwDecoderText(const QString &channel, const QString &text, const QColor &color, bool *lineOpen)
{
    if (m_txtCwRx == nullptr || text.isEmpty()) {
        return;
    }

    QString normalized = text;
    normalized.replace("\r\n", "\n");
    normalized.replace('\r', '\n');
    normalized.remove(QChar('\a'));
    if (normalized.isEmpty()) {
        return;
    }

    QTextCursor cursor = m_txtCwRx->textCursor();
    cursor.movePosition(QTextCursor::End);

    QTextCharFormat prefixFormat;
    prefixFormat.setForeground(color);
    prefixFormat.setFontWeight(QFont::Bold);

    QTextCharFormat bodyFormat;
    bodyFormat.setForeground(QColor("#111111"));

    bool open = (lineOpen != nullptr) ? *lineOpen : false;
    auto ensurePrefix = [&]() {
        if (open && m_cwCurrentLineChannel == channel) {
            return;
        }
        const QString existing = m_txtCwRx->toPlainText();
        if (!existing.isEmpty() && !existing.endsWith('\n')) {
            cursor.insertText(QStringLiteral("\n"), bodyFormat);
        }
        cursor.insertText(channel + QStringLiteral("> "), prefixFormat);
        open = true;
        m_cwCurrentLineChannel = channel;
    };

    for (QChar ch : normalized) {
        if (ch == QChar('\n')) {
            cursor.insertText(QStringLiteral("\n"), bodyFormat);
            open = false;
            if (m_cwCurrentLineChannel == channel) {
                m_cwCurrentLineChannel.clear();
            }
            continue;
        }
        ensurePrefix();
        cursor.insertText(QString(ch), bodyFormat);
    }

    if (lineOpen != nullptr) {
        *lineOpen = open;
    }

    m_txtCwRx->setTextCursor(cursor);
    m_txtCwRx->ensureCursorVisible();
    highlightCallsignsInTerminal(m_txtCwRx);
}

void MainWindow::recolorCwChannelPrefixes(QPlainTextEdit *terminal)
{
    if (terminal != m_txtCwRx || terminal == nullptr || terminal->document() == nullptr) {
        return;
    }

    const QString text = terminal->toPlainText();
    if (text.isEmpty()) {
        return;
    }

    QTextCursor cursor(terminal->document());
    const QRegularExpression prefixRe(QStringLiteral("(^|\n)([AB]> )"));
    QRegularExpressionMatchIterator it = prefixRe.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        const QString channel = match.captured(2).left(1);
        const int start = match.capturedStart(2);
        const int end = match.capturedEnd(2);
        if (!selectDocumentRange(cursor, start, end)) {
            continue;
        }
        QTextCharFormat fmt;
        fmt.setForeground(channel == QLatin1String("A") ? QColor("#118a2a") : QColor("#0069d9"));
        fmt.setFontWeight(QFont::Bold);
        fmt.setFontUnderline(false);
        fmt.setUnderlineStyle(QTextCharFormat::NoUnderline);
        cursor.mergeCharFormat(fmt);
    }
}

void MainWindow::updateCwDualRxStatusLabel()
{
    if (m_lblCwDualRx == nullptr) {
        return;
    }

    const QString wpmA = (m_cwTrackedWpmA > 0.1)
                             ? QString::number(m_cwTrackedWpmA, 'f', 1)
                             : QStringLiteral("--");

    const int toneA = (m_spinCwToneHz != nullptr) ? m_spinCwToneHz->value() : m_settings.cwToneHz;
    const QString bText = m_cwSecondaryEnabled
                              ? QStringLiteral(" · RX B %1 Hz").arg(m_cwSecondaryToneHz)
                              : QStringLiteral(" · RX B off");
    m_lblCwDualRx->setText(uiText("cw_skimmer_status",
                                  "CW skimmer: RX A %1 Hz%2 · tracked %3 WPM")
                               .arg(toneA)
                               .arg(bText)
                               .arg(wpmA));

    if (m_btnCwDisableSecondary != nullptr) {
        m_btnCwDisableSecondary->setEnabled(m_cwSecondaryEnabled);
    }
}

void MainWindow::disableCwSecondaryRx()
{
    m_cwSecondaryEnabled = false;
    m_settings.cwSecondaryEnabled = false;
    m_cwSecondaryLineOpen = false;
    if (m_cwDecoder != nullptr) {
        m_cwDecoder->setSecondaryEnabled(false);
    }
    savePersistentSettings();
    updateCwDualRxStatusLabel();
    updateWaterfallMarkers();
    appendLog(uiText("log.cw_rx_b_disabled", "CW RX B disabled."));
}


int MainWindow::hellPaperScale() const
{
    if (m_sliderHellPaperScale != nullptr) {
        return qBound(1, m_sliderHellPaperScale->value(), 12);
    }
    return qBound(1, m_settings.hellPaperScale, 12);
}

void MainWindow::updateHellRasterDisplay(const QImage &image)
{
    if (m_lblHellRaster == nullptr || image.isNull()) {
        return;
    }

    const QSize previousSize = m_lblHellRaster->size();

    m_lblHellRaster->setPixmap(QPixmap::fromImage(image));
    m_lblHellRaster->setMinimumSize(image.size());
    m_lblHellRaster->resize(image.size());

    /* Paper scale is a zoom, not a reset.  When the image changes size keep
     * the visible tape anchored at the top/left instead of letting the scroll
     * area appear to slide the text underneath the viewport.
     */
    if (m_scrollHellRaster != nullptr && !previousSize.isEmpty() && previousSize != image.size()) {
        if (QScrollBar *bar = m_scrollHellRaster->verticalScrollBar()) {
            bar->setValue(0);
        }
        if (QScrollBar *bar = m_scrollHellRaster->horizontalScrollBar()) {
            bar->setValue(0);
        }
    }
}

void MainWindow::applyHellSettings()
{
    if (m_hellDecoder == nullptr ||
        m_cmbHellVariant == nullptr ||
        m_spinHellToneHz == nullptr ||
        m_spinHellColumnRate == nullptr ||
        m_spinHellBandwidthHz == nullptr ||
        m_chkHellAfc == nullptr ||
        m_spinHellAfcRangeHz == nullptr ||
        m_sliderHellPaperScale == nullptr ||
        m_lblHellPaperScale == nullptr) {
        return;
    }

    const HellschreiberDecoder::Variant variant = HellschreiberDecoder::variantFromKey(m_cmbHellVariant->currentData().toString());
    const int toneHz = m_spinHellToneHz->value();
    const double columnRate = m_spinHellColumnRate->value();
    const int bandwidthHz = m_spinHellBandwidthHz->value();
    const bool afc = m_chkHellAfc->isChecked();
    const int afcRangeHz = m_spinHellAfcRangeHz->value();
    const int paperScale = qBound(1, m_sliderHellPaperScale->value(), 12);

    m_hellDecoder->setVariant(variant);
    m_hellDecoder->setToneHz(static_cast<double>(toneHz));
    m_hellDecoder->setColumnRate(columnRate);
    m_hellDecoder->setBandwidthHz(static_cast<double>(bandwidthHz));
    m_hellDecoder->setVerticalScale(paperScale);
    m_hellDecoder->setFskShiftHz(HellschreiberDecoder::fsk105ShiftHz());

    /* Tone is a waterfall marker and remains session-only per v0.50 policy. */
    m_settings.hellVariant = HellschreiberDecoder::variantKey(variant);
    m_settings.hellColumnRate = columnRate;
    m_settings.hellBandwidthHz = bandwidthHz;
    m_settings.hellAfcEnabled = afc;
    m_settings.hellAfcRangeHz = afcRangeHz;
    m_settings.hellPaperScale = paperScale;
    m_lblHellPaperScale->setText(QStringLiteral("x%1").arg(paperScale));

    savePersistentSettings();
    updateCwDualRxStatusLabel();
    updateWaterfallMarkers();
    updateTxPreview();
}

void MainWindow::clearHellTxText()
{
    if (m_txtHellTx != nullptr) {
        if (m_activeTextTxEditor == m_txtHellTx) {
            endTextTxHighlight();
        }
        m_txtHellTx->clear();
    }

    updateTxPreview();
}

void MainWindow::loadHellTxTextFile()
{
    const QString fileName = QFileDialog::getOpenFileName(
        this,
        "Load Hellschreiber TX text",
        QDir::homePath(),
        "Text files (*.txt *.log);;All files (*)"
        );

    if (fileName.isEmpty()) {
        return;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this,
                             "Load Hellschreiber text failed",
                             "Unable to open the selected text file.");
        appendLog("Hellschreiber text load failed: " + file.errorString());
        return;
    }

    QTextStream stream(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    stream.setCodec("UTF-8");
#endif
    const QString text = stream.readAll();

    if (m_txtHellTx != nullptr) {
        m_txtHellTx->setPlainText(text);
    }

    appendLog("Hellschreiber TX text loaded: " + fileName);
    updateTxPreview();
}

void MainWindow::sendHellTxText()
{
    if (m_txRunning) {
        stopImageTx();
        return;
    }

    const QString text = (m_txtHellTx != nullptr) ? m_txtHellTx->toPlainText() : QString();
    startTextModeTx(text);
}


void MainWindow::sendRttyTxText()
{
    if (m_txRunning) {
        stopImageTx();
        return;
    }

    const QString text = (m_txtRttyTx != nullptr) ? m_txtRttyTx->toPlainText() : QString();
    startTextModeTx(text);
}

void MainWindow::sendBpsk31TxText()
{
    if (m_txRunning) {
        stopImageTx();
        return;
    }

    const QString text = (m_txtBpsk31Tx != nullptr) ? m_txtBpsk31Tx->toPlainText() : QString();
    startTextModeTx(text);
}

void MainWindow::sendMfskTxText()
{
    if (m_txRunning) {
        stopImageTx();
        return;
    }

    const QString text = (m_txtMfskTx != nullptr) ? m_txtMfskTx->toPlainText() : QString();
    startTextModeTx(text);
}


void MainWindow::applyFt8Settings()
{
    if (m_editFt8DxCall == nullptr || m_editFt8DxGrid == nullptr ||
        m_cmbFt8Band == nullptr || m_spinFt8RxFreq == nullptr ||
        m_spinFt8TxFreq == nullptr || m_lblFt8DecodeEngine == nullptr ||
        m_radioFt8TxFirst == nullptr ||
        m_chkFt8AutoSeq == nullptr || m_chkFt8CqRepeat == nullptr ||
        m_chkFt8AutoLog == nullptr || m_chkFt8HoldTxFreq == nullptr || m_cmbFt8TxStrategy == nullptr ||
        m_chkFt8DeepDecode == nullptr || m_chkFt8DspPlusDecode == nullptr ||
        m_spinFt8CqTimeoutMin == nullptr || m_spinFt8NoResponseLimit == nullptr ||
        m_cmbFtLiveDecodeDepth == nullptr) {
        return;
    }

    m_settings.ft8MyCallsign = stationCallsign();
    m_settings.ft8MyGrid = stationLocator();
    m_settings.ft8DxCallsign = m_editFt8DxCall->text().trimmed().toUpper();
    m_settings.ft8DxGrid = m_editFt8DxGrid->text().trimmed().toUpper();
    m_settings.ft8Band = m_cmbFt8Band->currentText();
    m_settings.ft8RxFrequencyHz = m_spinFt8RxFreq->value();
    m_settings.ft8TxFrequencyHz = m_spinFt8TxFreq->value();
    // v2.19: single FT RX pipeline.  Keep the setting key only for backwards
    // profile compatibility; do not expose or persist pseudo-engine choices.
    m_settings.ft8DecodeEngine = QStringLiteral("mshv");
    m_settings.ft8TxFirstPeriod = m_radioFt8TxFirst->isChecked();
    m_settings.ft8AutoSequence = true;
    m_settings.ft8CqRepeat = m_chkFt8CqRepeat->isChecked();
    m_settings.ft8AutoLog = m_chkFt8AutoLog->isChecked();
    m_settings.ft8TxFrequencyStrategy = ft8TxFrequencyStrategyKey();
    m_settings.ft8HoldTxFrequency = (m_settings.ft8TxFrequencyStrategy == QStringLiteral("fixed"));
    if (m_chkFt8HoldTxFreq->isChecked() != m_settings.ft8HoldTxFrequency) {
        const QSignalBlocker blockHold(m_chkFt8HoldTxFreq);
        m_chkFt8HoldTxFreq->setChecked(m_settings.ft8HoldTxFrequency);
    }
    const QString liveDepth = QStringLiteral("adaptive");
    m_settings.ft8LiveDecodeDepth = liveDepth;
    m_settings.ft8DeepDecode = true;
    m_settings.ft8DspPlusDecode = true;
    if (m_chkFt8DeepDecode != nullptr) {
        const QSignalBlocker blockDeep(m_chkFt8DeepDecode);
        m_chkFt8DeepDecode->setChecked(m_settings.ft8DeepDecode);
    }
    if (m_chkFt8DspPlusDecode != nullptr) {
        const QSignalBlocker blockDsp(m_chkFt8DspPlusDecode);
        m_chkFt8DspPlusDecode->setChecked(m_settings.ft8DspPlusDecode);
    }
    m_settings.ft8CqRepeatCount = m_spinFt8CqTimeoutMin->value();
    m_settings.ft8NoResponseRetryCount = m_spinFt8NoResponseLimit->value();

    const QString activeModeName = (ui != nullptr && ui->cmbMode != nullptr) ? ui->cmbMode->currentText() : QStringLiteral("FT8");
    if (m_ft8RxDecoder != nullptr) {
        QMetaObject::invokeMethod(m_ft8RxDecoder, "setModeName", Qt::QueuedConnection, Q_ARG(QString, activeModeName));
        QMetaObject::invokeMethod(m_ft8RxDecoder, "setDecodeEngine", Qt::QueuedConnection, Q_ARG(QString, QStringLiteral("mshv")));
        QMetaObject::invokeMethod(m_ft8RxDecoder, "setDeepDecodeEnabled", Qt::QueuedConnection, Q_ARG(bool, m_settings.ft8DeepDecode));
        QMetaObject::invokeMethod(m_ft8RxDecoder, "setDspPlusDecodeEnabled", Qt::QueuedConnection, Q_ARG(bool, m_settings.ft8DspPlusDecode));
        QMetaObject::invokeMethod(m_ft8RxDecoder, "setRxMarkerHz", Qt::QueuedConnection, Q_ARG(int, m_settings.ft8RxFrequencyHz));
        QMetaObject::invokeMethod(m_ft8RxDecoder, "setMyCall", Qt::QueuedConnection, Q_ARG(QString, stationCallsign()));
        QMetaObject::invokeMethod(m_ft8RxDecoder, "setDxCall", Qt::QueuedConnection, Q_ARG(QString, m_settings.ft8DxCallsign));
    }
    if (m_ftSlotScheduler != nullptr) {
        QMetaObject::invokeMethod(m_ftSlotScheduler,
                                  "configure",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, activeModeName),
                                  Q_ARG(bool, m_settings.ft8TxFirstPeriod));
    }

    refreshFt8StandardMessages();
    updateFt8SlotStatus();
    updateFtBandFrequencyUi();
    updateWaterfallMarkers();
    updateFt8SequencerUi();
    savePersistentSettings();
}


QString MainWindow::ft8TxFrequencyStrategyKey() const
{
    QString key;
    if (m_cmbFt8TxStrategy != nullptr) {
        key = m_cmbFt8TxStrategy->currentData().toString().trimmed().toLower();
    }
    if (key.isEmpty()) {
        key = m_settings.ft8TxFrequencyStrategy.trimmed().toLower();
    }
    if (key != QStringLiteral("fixed") && key != QStringLiteral("auto_free") && key != QStringLiteral("follow_rx")) {
        key = (m_chkFt8HoldTxFreq != nullptr && m_chkFt8HoldTxFreq->isChecked())
            ? QStringLiteral("fixed")
            : QStringLiteral("auto_free");
    }
    return key;
}

void MainWindow::pruneFt8AudioActivity()
{
    const qint64 nowMs = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
    const QString modeName = (ui != nullptr && ui->cmbMode != nullptr) ? ui->cmbMode->currentText() : QStringLiteral("FT8");
    const Ft8Mode::Profile profile = Ft8Mode::profileForMode(modeName);
    const qint64 keepMs = static_cast<qint64>(qMax(profile.slotMs * 4, 45000));
    QVector<Ft8AudioActivity> kept;
    kept.reserve(m_ft8RecentAudioActivity.size());
    for (const Ft8AudioActivity &a : std::as_const(m_ft8RecentAudioActivity)) {
        const qint64 t = a.heardUtc.isValid() ? a.heardUtc.toMSecsSinceEpoch() : a.slotStartUtcMs;
        if (a.frequencyHz >= 100 && a.frequencyHz <= 3000 && t > 0 && nowMs - t <= keepMs) {
            kept.append(a);
        }
    }
    if (kept.size() > 300) {
        kept = kept.mid(kept.size() - 300);
    }
    m_ft8RecentAudioActivity = kept;
}

void MainWindow::rememberFt8AudioActivity(const Ft8RxDecoder::Decode &decode, const QString &callHint)
{
    if (decode.frequencyHz < 100 || decode.frequencyHz > 3000) {
        return;
    }
    Ft8AudioActivity a;
    a.frequencyHz = decode.frequencyHz;
    a.slotStartUtcMs = decode.slotStartUtcMs;
    a.heardUtc = QDateTime::currentDateTimeUtc();
    a.call = callHint.trimmed().toUpper();
    m_ft8RecentAudioActivity.append(a);
    pruneFt8AudioActivity();
}

int MainWindow::chooseFt8AutoFreeTxFrequency(int wantedHz, QString *reason) const
{
    const int low = 300;
    const int high = 2700;
    const int preferredLow = 900;
    const int preferredHigh = 2100;
    const int stepHz = 10;
    const int guardHz = qBound(60, m_settings.ft8TxGuardHz, 220);

    QVector<int> occupied;
    occupied.reserve(m_ft8RecentAudioActivity.size() + 8);
    const qint64 nowMs = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
    const QString modeName = (ui != nullptr && ui->cmbMode != nullptr) ? ui->cmbMode->currentText() : QStringLiteral("FT8");
    const Ft8Mode::Profile profile = Ft8Mode::profileForMode(modeName);
    const qint64 keepMs = static_cast<qint64>(qMax(profile.slotMs * 4, 45000));
    for (const Ft8AudioActivity &a : m_ft8RecentAudioActivity) {
        const qint64 t = a.heardUtc.isValid() ? a.heardUtc.toMSecsSinceEpoch() : a.slotStartUtcMs;
        if (a.frequencyHz >= 100 && a.frequencyHz <= 3000 && t > 0 && nowMs - t <= keepMs) {
            occupied.append(a.frequencyHz);
        }
    }
    if (m_spinFt8RxFreq != nullptr) {
        occupied.append(m_spinFt8RxFreq->value());
    }
    if (wantedHz >= 100 && wantedHz <= 3000) {
        occupied.append(wantedHz);
    }

    auto clearAt = [&](int hz) -> bool {
        if (hz < low || hz > high) {
            return false;
        }
        for (int f : occupied) {
            if (qAbs(f - hz) <= guardHz) {
                return false;
            }
        }
        return true;
    };

    int bestHz = 0;
    int bestScore = -1000000;
    for (int hz = low; hz <= high; hz += stepHz) {
        if (!clearAt(hz)) {
            continue;
        }
        int score = 10000;
        if (hz >= preferredLow && hz <= preferredHigh) {
            score += 2500;
        }
        // Prefer the middle of the audio passband, but avoid gratuitous jumps
        // if the current TX marker is already clear.
        score -= qAbs(hz - 1500) / 2;
        if (m_spinFt8TxFreq != nullptr) {
            score -= qAbs(hz - m_spinFt8TxFreq->value()) / 8;
        }
        if (score > bestScore) {
            bestScore = score;
            bestHz = hz;
        }
    }

    if (bestHz <= 0) {
        bestHz = qBound(low, wantedHz > 0 ? wantedHz : 1500, high);
        if (reason != nullptr) {
            *reason = QStringLiteral("no clear slot found; using bounded fallback %1 Hz").arg(bestHz);
        }
        return bestHz;
    }

    if (reason != nullptr) {
        *reason = QStringLiteral("auto-free %1 Hz, guard ±%2 Hz from recent decodes").arg(bestHz).arg(guardHz);
    }
    return bestHz;
}

int MainWindow::resolveFt8TxFrequencyForStrategy(int correspondentHz, QString *reason) const
{
    const QString strategy = ft8TxFrequencyStrategyKey();
    if (strategy == QStringLiteral("fixed")) {
        const int tx = (m_spinFt8TxFreq != nullptr) ? m_spinFt8TxFreq->value() : m_settings.ft8TxFrequencyHz;
        if (reason != nullptr) {
            *reason = QStringLiteral("fixed/manual TX marker %1 Hz").arg(tx);
        }
        return qBound(100, tx, 3000);
    }
    if (strategy == QStringLiteral("follow_rx")) {
        const int tx = qBound(100, correspondentHz > 0 ? correspondentHz : ((m_spinFt8RxFreq != nullptr) ? m_spinFt8RxFreq->value() : 1500), 3000);
        if (reason != nullptr) {
            *reason = QStringLiteral("following correspondent/RX frequency %1 Hz").arg(tx);
        }
        return tx;
    }
    return chooseFt8AutoFreeTxFrequency(correspondentHz, reason);
}

void MainWindow::applyFt8RuntimeFrequencySelection(int rxHz, const QString &source, bool updateTx)
{
    const int boundedRx = qBound(100, rxHz, 3000);
    const int oldRx = (m_spinFt8RxFreq != nullptr) ? m_spinFt8RxFreq->value() : m_settings.ft8RxFrequencyHz;
    const int oldTx = (m_spinFt8TxFreq != nullptr) ? m_spinFt8TxFreq->value() : m_settings.ft8TxFrequencyHz;
    if (m_spinFt8RxFreq != nullptr) {
        const QSignalBlocker blockRx(m_spinFt8RxFreq);
        m_spinFt8RxFreq->setValue(boundedRx);
        m_settings.ft8RxFrequencyHz = boundedRx;
    }
    if (m_ft8RxDecoder != nullptr) {
        QMetaObject::invokeMethod(m_ft8RxDecoder, "setRxMarkerHz", Qt::QueuedConnection, Q_ARG(int, boundedRx));
    }

    QString why;
    if (updateTx && m_spinFt8TxFreq != nullptr) {
        const int txHz = resolveFt8TxFrequencyForStrategy(boundedRx, &why);
        const QSignalBlocker blockTx(m_spinFt8TxFreq);
        m_spinFt8TxFreq->setValue(qBound(m_spinFt8TxFreq->minimum(), txHz, m_spinFt8TxFreq->maximum()));
        m_settings.ft8TxFrequencyHz = m_spinFt8TxFreq->value();
    }

    const bool important = source.contains(QStringLiteral("manual"), Qt::CaseInsensitive) ||
                           source.contains(QStringLiteral("AutoQSO"), Qt::CaseInsensitive) ||
                           source.contains(QStringLiteral("reclaim"), Qt::CaseInsensitive);
    if (important || qAbs(oldRx - m_settings.ft8RxFrequencyHz) >= 20 ||
        (updateTx && qAbs(oldTx - m_settings.ft8TxFrequencyHz) >= 20)) {
        appendLog(QString("FT frequency: RX focus %1 Hz, TX marker %2 Hz (%3) — %4.")
                      .arg(m_settings.ft8RxFrequencyHz)
                      .arg(m_settings.ft8TxFrequencyHz)
                      .arg(ft8TxFrequencyStrategyKey())
                      .arg(source.isEmpty() ? why : source + (why.isEmpty() ? QString() : QStringLiteral("; ") + why)));
    }
    updateWaterfallMarkers();
}

bool MainWindow::reclaimFt8ActiveQsoAwayFromQrm(const Ft8RxDecoder::Decode &decode,
                                                const FtQsoSequencer::ParsedMessage &parsed,
                                                const QString &reason)
{
    if (!m_ftSession.qsoActive || m_ftSession.dxCall.trimmed().isEmpty() ||
        !FtDecodedText::callMatches(parsed.senderCall, m_ftSession.dxCall)) {
        return false;
    }

    // Keep the QSO target; only move our TX away from the QRM/correspondent's
    // current exchange and re-arm the current retry message.
    applyFt8RuntimeFrequencySelection(decode.frequencyHz,
                                      QStringLiteral("AutoQSO reclaim: target %1 heard while working another station")
                                          .arg(parsed.senderCall),
                                      true);

    if (m_ftSlotScheduler != nullptr) {
        QMetaObject::invokeMethod(m_ftSlotScheduler, "cancelTransmission", Qt::QueuedConnection);
    }
    if (m_pendingFt8PttKeyed && !m_txRunning && !m_ftTxWorkerRunning) {
        unkeyPttAfterTx();
    }
    m_ft8PendingTxArmed = false;
    m_ft8PendingTxToken.clear();
    m_pendingFt8TxMessage.clear();
    m_pendingFt8TxTag.clear();
    m_pendingFt8Tune = false;
    m_pendingFt8PreparedModulator.reset();
    m_pendingFt8TxPlan = FtTxPlan();

    QString msg = m_ftSession.retryMessage.trimmed().toUpper();
    if (msg.isEmpty() && m_ftSession.activeTxRow >= 0) {
        msg = selectFt8TxRow(m_ftSession.activeTxRow);
    }
    if (msg.isEmpty()) {
        // Conservative fallback from the current state.
        switch (m_ftSession.state) {
        case Ft8SequencerState::WaitingRReport:
        case Ft8SequencerState::SendingReport:
            msg = selectFt8TxRow(2);
            break;
        case Ft8SequencerState::WaitingFinal73:
        case Ft8SequencerState::SendingRReport:
            msg = selectFt8TxRow(3);
            break;
        case Ft8SequencerState::SendingRr73:
            msg = selectFt8TxRow(4);
            break;
        default:
            msg = selectFt8TxRow(1);
            break;
        }
    }
    if (msg.trimmed().isEmpty()) {
        appendLog(QString("FT AutoQSO reclaim: %1 but no retry message is available; staying RX.")
                      .arg(reason.isEmpty() ? QStringLiteral("target moved/QRM seen") : reason));
        updateFt8SequencerUi();
        return true;
    }

    m_ftSession.retryMessage = msg;
    m_ftSession.retryTag = QStringLiteral("RETRY");
    if (m_ftSession.retryRemaining <= 0) {
        m_ftSession.retryRemaining = qBound(1, m_settings.ft8NoResponseRetryCount, 12);
    }
    appendLog(QString("FT AutoQSO reclaim: %1; re-arming %2 on clear TX marker.")
                  .arg(reason.isEmpty() ? QStringLiteral("target heard off our exchange") : reason,
                       msg));
    scheduleFt8SequencerMessage(msg, QStringLiteral("RETRY"));
    updateFt8SequencerUi();
    return true;
}

void MainWindow::refreshFt8StandardMessages()
{
    if (m_tableFt8TxMessages == nullptr) {
        return;
    }

    const QString myCall = stationCallsign();
    const QString myGrid = stationLocator();
    const QString dxCall = (m_editFt8DxCall != nullptr) ? m_editFt8DxCall->text().trimmed().toUpper() : QString();
    const QString dxGrid = (m_editFt8DxGrid != nullptr) ? m_editFt8DxGrid->text().trimmed().toUpper() : QString();

    if (myCall.isEmpty() || myGrid.isEmpty()) {
        m_ftStandardMessages = FtStandardMessageSet();
        const QString warning = uiText("station_identity_required_short", "Set My Call and My Locator in Settings -> User/QTH");
        for (int row = 0; row < FtStandardMessageSet::RowCount; ++row) {
            QTableWidgetItem *item = m_tableFt8TxMessages->item(row, 2);
            if (item == nullptr) {
                item = new QTableWidgetItem();
                m_tableFt8TxMessages->setItem(row, 2, item);
            }
            item->setText(row == 0 ? warning : QString());
        }
        updateFt8SequencerUi();
        return;
    }

    const int reportDb = m_ftSession.haveLastSnr ? m_ftSession.lastSnrDb : -10;
    QString report = formatFt8SignalReport(reportDb, false);
    QString rReport = formatFt8SignalReport(reportDb, true);

    // The QSO session owns the active reports once a QSO is underway.  The
    // standard-message table is rebuilt from the same object used by the
    // sequencer and FtTxPlan, so Tx3/Tx4 cannot silently drift apart from the
    // message actually armed for the next slot.
    const QString seqReport = m_ftSession.reportSent.trimmed().toUpper();
    if (!seqReport.isEmpty()) {
        if (seqReport.startsWith(QLatin1Char('R'))) {
            rReport = seqReport;
        } else {
            report = seqReport;
        }
    }

    FtStandardMessageSet::Inputs inputs;
    inputs.myCall = myCall;
    inputs.myGrid = myGrid;
    inputs.dxCall = dxCall;
    inputs.dxGrid = dxGrid;
    inputs.report = report;
    inputs.rReport = rReport;
    inputs.audioFrequencyHz = (m_spinFt8TxFreq != nullptr) ? m_spinFt8TxFreq->value() : m_settings.ft8TxFrequencyHz;
    m_ftStandardMessages.rebuild(inputs);

    for (int row = 0; row < FtStandardMessageSet::RowCount; ++row) {
        QTableWidgetItem *item = m_tableFt8TxMessages->item(row, 2);
        if (item == nullptr) {
            item = new QTableWidgetItem();
            m_tableFt8TxMessages->setItem(row, 2, item);
        }
        item->setText(m_ftStandardMessages.message(row));
    }
}

void MainWindow::updateFtUtcClockVisibility(const QString &modeName)
{
    const QString activeModeName = !modeName.isEmpty()
        ? modeName
        : ((ui != nullptr && ui->cmbMode != nullptr) ? ui->cmbMode->currentText() : QString());
    const bool showFtClock = Ft8Mode::isFamilyMode(activeModeName);

    if (m_grpFt8UtcClock != nullptr) {
        m_grpFt8UtcClock->setVisible(showFtClock);
    }
}


void MainWindow::updateFt8SlotStatus()
{
    const QString modeName = (ui != nullptr && ui->cmbMode != nullptr) ? ui->cmbMode->currentText() : QStringLiteral("FT8");
    const Ft8Mode::Profile profile = Ft8Mode::profileForMode(modeName);
    const int slotMs = qMax(1000, profile.slotMs);
    const int cycleMs = qMax(slotMs * 2, profile.cycleMs);

    const QDateTime now = QDateTime::currentDateTimeUtc();
    const QTime t = now.time();
    const int msOfDay = (((t.hour() * 60 + t.minute()) * 60) + t.second()) * 1000 + t.msec();

    // Use one shared UTC phase model for the dial, labels and scheduler:
    // FT8  = 30 s cycle split into 15 + 15 s periods.
    // FT4  = 15 s cycle split into 7.5 + 7.5 s periods.
    // The previous code was mathematically close, but it still let the clock
    // widget look FT8-centric and made FT4 half-second boundaries hard to see.
    const int cyclePosMs = msOfDay % cycleMs;
    const int slotIndexInCycle = qBound(0, cyclePosMs / slotMs, 1);
    const int slotElapsedMs = cyclePosMs - slotIndexInCycle * slotMs;
    const bool firstPeriodNow = (slotIndexInCycle == 0);
    const bool txFirst = (m_radioFt8TxFirst != nullptr) ? m_radioFt8TxFirst->isChecked() : true;
    const bool txWindow = (firstPeriodNow == txFirst);
    const int remainMs = qMax(0, slotMs - slotElapsedMs);
    const int remainSec = qMax(0, (remainMs + 999) / 1000);
    const double remainExactSec = static_cast<double>(remainMs) / 1000.0;
    const double cycleSeconds = static_cast<double>(cyclePosMs) / 1000.0;

    const QString first = uiText("first_period", "first period");
    const QString second = uiText("second_period", "second period");
    const QString selectedPeriod = txFirst ? first : second;
    const QString window = txWindow ? uiText("ft8_tx_window", "TX window")
                                    : uiText("ft8_rx_window", "RX window");

    if (m_ft8SlotClock != nullptr) {
        m_ft8SlotClock->setSlotState(cycleSeconds,
                                      firstPeriodNow,
                                      txWindow,
                                      static_cast<double>(slotMs) / 1000.0,
                                      static_cast<double>(cycleMs) / 1000.0,
                                      profile.shortLabel);
    }
    if (m_lblFt8PeriodStatus != nullptr) {
        m_lblFt8PeriodStatus->clear();
    }
    if (m_lcdFt8UtcClock != nullptr) {
        m_lcdFt8UtcClock->display(t.toString("HH:mm:ss"));
    }
    if (m_lblFt8WindowStatus != nullptr) {
        m_lblFt8WindowStatus->setText(window);
        m_lblFt8WindowStatus->setStyleSheet(txWindow
            ? QStringLiteral("font-weight: 500; font-size: 9pt; color: #c82020;")
            : QStringLiteral("font-weight: 500; font-size: 9pt; color: #15803d;"));
    }
    if (m_lblFt8SlotStatus != nullptr) {
        const int nextTxSeconds = (millisecondsToNextFt8TxPeriod() + 999) / 1000;
        const QString nextTxText = (slotMs % 1000 == 0)
            ? QString::number(nextTxSeconds)
            : QString::number(static_cast<double>(millisecondsToNextFt8TxPeriod()) / 1000.0, 'f', 1);
        const QString remainWindowText = (slotMs % 1000 == 0)
            ? QString::number(remainSec)
            : QString::number(remainExactSec, 'f', 1);
        const QString detail = txWindow
            ? uiText("ft8_tx_window_ends_in", "TX window ends in %1 s").arg(remainWindowText)
            : uiText("ft8_next_tx_in", "next TX in %1 s").arg(nextTxText);
        QString text = profile.shortLabel + QStringLiteral(" | ") +
                       uiText("ft8_selected_tx_period", "Selected TX period") +
                       QStringLiteral(": ") + selectedPeriod +
                       QStringLiteral(" | slot ") + QString::number(static_cast<double>(slotMs) / 1000.0, 'f', slotMs % 1000 == 0 ? 0 : 1) +
                       QStringLiteral(" s | cycle ") + QString::number(static_cast<double>(cycleMs) / 1000.0, 'f', cycleMs % 1000 == 0 ? 0 : 1) +
                       QStringLiteral(" s | ") + detail;
        if (!profile.interoperableCoreAvailable) {
            text += QStringLiteral(" | core unavailable");
        }
        m_lblFt8SlotStatus->setText(text);
    }
    updateFt8SequencerUi();
    updateFt8WaterfallOverlays();
    updateFt8DecodePerformanceUi();
}


void MainWindow::handleFtSlotUpdated(const QString &modeLabel,
                                     int slotMs,
                                     int cycleMs,
                                     bool firstPeriodNow,
                                     bool txWindow,
                                     int cyclePosMs,
                                     int slotElapsedMs,
                                     int remainMs,
                                     qint64 nowUtcMs)
{
    const int remainSec = qMax(0, (remainMs + 999) / 1000);
    const double remainExactSec = static_cast<double>(remainMs) / 1000.0;
    const double cycleSeconds = static_cast<double>(cyclePosMs) / 1000.0;
    const bool txFirst = (m_radioFt8TxFirst != nullptr) ? m_radioFt8TxFirst->isChecked() : true;

    const QString first = uiText("first_period", "first period");
    const QString second = uiText("second_period", "second period");
    const QString selectedPeriod = txFirst ? first : second;
    const QString window = txWindow ? uiText("ft8_tx_window", "TX window")
                                    : uiText("ft8_rx_window", "RX window");

    if (m_ft8SlotClock != nullptr) {
        m_ft8SlotClock->setSlotState(cycleSeconds,
                                      firstPeriodNow,
                                      txWindow,
                                      static_cast<double>(slotMs) / 1000.0,
                                      static_cast<double>(cycleMs) / 1000.0,
                                      modeLabel);
    }
    if (m_lblFt8PeriodStatus != nullptr) {
        m_lblFt8PeriodStatus->clear();
    }
    if (m_lcdFt8UtcClock != nullptr) {
        m_lcdFt8UtcClock->display(QDateTime::fromMSecsSinceEpoch(nowUtcMs, Qt::UTC).time().toString("HH:mm:ss"));
    }
    if (m_lblFt8WindowStatus != nullptr) {
        m_lblFt8WindowStatus->setText(window);
        m_lblFt8WindowStatus->setStyleSheet(txWindow
            ? QStringLiteral("font-weight: 500; font-size: 9pt; color: #c82020;")
            : QStringLiteral("font-weight: 500; font-size: 9pt; color: #15803d;"));
    }
    if (m_lblFt8SlotStatus != nullptr) {
        const int nextTxSeconds = (millisecondsToNextFt8TxPeriod() + 999) / 1000;
        const QString nextTxText = (slotMs % 1000 == 0)
            ? QString::number(nextTxSeconds)
            : QString::number(static_cast<double>(millisecondsToNextFt8TxPeriod()) / 1000.0, 'f', 1);
        const QString remainWindowText = (slotMs % 1000 == 0)
            ? QString::number(remainSec)
            : QString::number(remainExactSec, 'f', 1);
        const QString detail = txWindow
            ? uiText("ft8_tx_window_ends_in", "TX window ends in %1 s").arg(remainWindowText)
            : uiText("ft8_next_tx_in", "next TX in %1 s").arg(nextTxText);
        QString text = modeLabel + QStringLiteral(" | ") +
                       uiText("ft8_selected_tx_period", "Selected TX period") +
                       QStringLiteral(": ") + selectedPeriod +
                       QStringLiteral(" | slot ") + QString::number(static_cast<double>(slotMs) / 1000.0, 'f', slotMs % 1000 == 0 ? 0 : 1) +
                       QStringLiteral(" s | cycle ") + QString::number(static_cast<double>(cycleMs) / 1000.0, 'f', cycleMs % 1000 == 0 ? 0 : 1) +
                       QStringLiteral(" s | ") + detail;
        if (m_ft8PendingTxArmed) {
            text += QStringLiteral(" | TX armed by UTC scheduler");
        } else if (!m_pendingFt8TxMessage.trimmed().isEmpty() && !m_pendingFt8Tune) {
            text += QStringLiteral(" | sequencer ready; RX still decoding");
        }
        m_lblFt8SlotStatus->setText(text);
    }
    // v4.13a midnight/rollover watchdog: AutoQSO is a persistent operator
    // intent, not a per-date state. If RX was stopped by a date/slot rollover
    // while AutoQSO remains checked, restart FT RX without touching active TX/QSO.
    if (m_ft8EvilModeUnlocked && m_chkFt8FullAutoQso != nullptr && m_chkFt8FullAutoQso->isChecked() &&
        ui != nullptr && ui->cmbMode != nullptr && Ft8Mode::isFamilyMode(ui->cmbMode->currentText()) &&
        !m_offlineAnalysisActive && !m_txRunning && !m_ftTxWorkerRunning &&
        !m_ft8PendingTxArmed && !m_pendingFt8PttKeyed &&
        !m_rxRunning && m_audioEngine != nullptr && !m_audioEngine->isRunning()) {
        appendLog(QStringLiteral("FT AutoQSO watchdog: AutoQSO is still enabled; restarting FT RX after slot/date rollover."));
        startFt8RxShell();
    }

    updateFt8WaterfallOverlays();
    updateFt8DecodePerformanceUi();
}

void MainWindow::handleFtScheduledPttPrearmDue(const QString &token,
                                                qint64 slotBoundaryUtcMs,
                                                int audioTargetDelayMs,
                                                int pttLeadMs,
                                                qint64 nowUtcMs)
{
    Q_UNUSED(nowUtcMs);
    if (!m_ft8PendingTxArmed || token != m_ft8PendingTxToken) {
        appendLog("FT timing: stale scheduler PTT pre-arm event ignored.");
        return;
    }
    m_pendingFt8SlotBoundaryUtcMs = slotBoundaryUtcMs;
    m_pendingFt8AudioTargetDelayMs = qMax(0, audioTargetDelayMs);
    m_pendingFt8PttLeadMs = qMax(0, pttLeadMs);
    m_pendingFt8TxPlan.slotBoundaryUtcMs = slotBoundaryUtcMs;
    m_pendingFt8TxPlan.audioTargetDelayMs = m_pendingFt8AudioTargetDelayMs;
    m_pendingFt8TxPlan.pttLeadMs = m_pendingFt8PttLeadMs;
    beginScheduledFt8Transmit();
}

void MainWindow::handleFtScheduledTxDue(const QString &token,
                                        qint64 slotBoundaryUtcMs,
                                        int audioTargetDelayMs,
                                        int pttLeadMs,
                                        qint64 nowUtcMs)
{
    Q_UNUSED(nowUtcMs);

    const bool havePendingMessage = !m_pendingFt8TxMessage.trimmed().isEmpty();
    const bool exactTokenMatch = (token == m_ft8PendingTxToken);
    const bool boundaryMatch = (slotBoundaryUtcMs > 0 &&
                                m_pendingFt8SlotBoundaryUtcMs > 0 &&
                                qAbs(slotBoundaryUtcMs - m_pendingFt8SlotBoundaryUtcMs) <= 50);

    if (!havePendingMessage || (!exactTokenMatch && !boundaryMatch)) {
        appendLog(QString("FT timing: stale scheduler audio-start event ignored; token match=%1 boundary match=%2 pending message=%3.")
                      .arg(exactTokenMatch ? QStringLiteral("yes") : QStringLiteral("no"),
                           boundaryMatch ? QStringLiteral("yes") : QStringLiteral("no"),
                           havePendingMessage ? QStringLiteral("yes") : QStringLiteral("no")));
        return;
    }

    if (!exactTokenMatch && boundaryMatch) {
        appendLog("FT timing: audio-start token was already cleared, but slot boundary still matches; starting pending FT TX as recovery.");
    }

    QString rotatorWaitReason;
    int rotatorEtaMs = 0;
    if (!ftRotatorReadyForPendingTx(&rotatorWaitReason, &rotatorEtaMs)) {
        deferPendingFtTxForRotator(rotatorEtaMs, rotatorWaitReason);
        return;
    }

    m_ft8PendingTxArmed = false;
    m_pendingFt8SlotBoundaryUtcMs = slotBoundaryUtcMs;
    m_pendingFt8AudioTargetDelayMs = qMax(0, audioTargetDelayMs);
    m_pendingFt8PttLeadMs = qMax(0, pttLeadMs);
    m_pendingFt8TxPlan.slotBoundaryUtcMs = slotBoundaryUtcMs;
    m_pendingFt8TxPlan.audioTargetDelayMs = m_pendingFt8AudioTargetDelayMs;
    m_pendingFt8TxPlan.pttLeadMs = m_pendingFt8PttLeadMs;
    startFtPreparedSlotTransmit();
}

void MainWindow::handleFtSchedulerPendingChanged(bool pending, qint64 slotBoundaryUtcMs)
{
    m_ft8PendingTxArmed = pending;
    if (!pending) {
        // Do not clear m_ft8PendingTxToken here while a message is still waiting
        // for the same slot.  The scheduler emits pending=false immediately after
        // txAudioStartDue; across queued threads this signal may be processed
        // before the audio-start handler on some systems, making a valid TX look
        // stale.  The token is cleared when the TX starts/stops or when the plan
        // is explicitly cancelled/replaced.
        if (m_pendingFt8TxMessage.trimmed().isEmpty() || m_txRunning || m_ftTxWorkerRunning) {
            m_ft8PendingTxToken.clear();
        }
    }
    if (pending && slotBoundaryUtcMs > 0) {
        m_pendingFt8SlotBoundaryUtcMs = slotBoundaryUtcMs;
    }
    updateTxControlState();
    updateFt8SequencerUi();
}


void MainWindow::updateFt8SequencerUi()
{
    if (m_lblFt8SequencerStatus == nullptr) {
        return;
    }

    QString stateText;
    switch (m_ftSession.state) {
    case Ft8SequencerState::Idle:
        stateText = uiText("ft8_seq_idle", "Sequencer: idle");
        break;
    case Ft8SequencerState::CallingCq:
        stateText = uiText("ft8_seq_calling_cq", "Sequencer: calling CQ");
        break;
    case Ft8SequencerState::WaitingDxReply:
        stateText = uiText("ft8_seq_waiting_reply", "Sequencer: waiting for reply");
        break;
    case Ft8SequencerState::SendingLocator:
        stateText = uiText("ft8_seq_sending_locator", "Sequencer: sending locator");
        break;
    case Ft8SequencerState::WaitingReport:
        stateText = uiText("ft8_seq_waiting_report", "Sequencer: waiting for report");
        break;
    case Ft8SequencerState::SendingReport:
        stateText = uiText("ft8_seq_sending_report", "Sequencer: sending report");
        break;
    case Ft8SequencerState::WaitingRReport:
        stateText = uiText("ft8_seq_waiting_r_report", "Sequencer: waiting for R-report");
        break;
    case Ft8SequencerState::SendingRReport:
        stateText = uiText("ft8_seq_sending_r_report", "Sequencer: sending R-report");
        break;
    case Ft8SequencerState::WaitingFinal73:
        stateText = uiText("ft8_seq_waiting_73", "Sequencer: waiting for 73/RR73");
        break;
    case Ft8SequencerState::SendingRr73:
        stateText = uiText("ft8_seq_sending_rr73", "Sequencer: sending RR73/73");
        break;
    case Ft8SequencerState::Completed:
        stateText = uiText("ft8_seq_completed", "Sequencer: QSO completed");
        break;
    }

    QStringList details;
    if (m_ft8EvilModeUnlocked && m_chkFt8FullAutoQso != nullptr && m_chkFt8FullAutoQso->isChecked() && !m_ftSession.qsoActive) {
        details << uiText("ft8_auto_qso_waiting", "Auto QSO: waiting for CQ");
    }
    if (m_ftSession.cqRepeatActive) {
        if (m_ftSession.cqRepeatRemaining > 0) {
            details << uiText("ft8_cq_retry_remaining", "CQ retry: %1 transmission(s) remaining").arg(m_ftSession.cqRepeatRemaining);
        } else if (m_ftSession.cqRepeatDeadlineUtc.isValid()) {
            const qint64 remaining = qMax<qint64>(qint64{0}, QDateTime::currentDateTimeUtc().secsTo(m_ftSession.cqRepeatDeadlineUtc));
            details << uiText("ft8_cq_repeat_remaining", "CQ repeat: %1 s remaining").arg(remaining);
        }
    }
    if (m_ftSession.qsoActive || !m_ftSession.dxCall.isEmpty()) {
        details << uiText("ft8_seq_dx", "DX: %1 %2").arg(m_ftSession.dxCall.isEmpty() ? QStringLiteral("--") : m_ftSession.dxCall,
                                                           m_ftSession.dxGrid.isEmpty() ? QStringLiteral("--") : m_ftSession.dxGrid);
    }
    if (!m_ftSession.reportSent.isEmpty() || !m_ftSession.reportReceived.isEmpty()) {
        details << uiText("ft8_seq_reports", "Sent %1 / Rcvd %2").arg(m_ftSession.reportSent.isEmpty() ? QStringLiteral("--") : m_ftSession.reportSent,
                                                                        m_ftSession.reportReceived.isEmpty() ? QStringLiteral("--") : m_ftSession.reportReceived);
    }

    if (m_ftSession.activeTxRow >= 0 && m_tableFt8TxMessages != nullptr) {
        QTableWidgetItem *messageItem = m_tableFt8TxMessages->item(m_ftSession.activeTxRow, 2);
        if (messageItem != nullptr) {
            details << uiText("ft8_next_standard_message", "Next TX%1: %2")
                           .arg(m_ftSession.activeTxRow + 1)
                           .arg(messageItem->text().trimmed().isEmpty() ? QStringLiteral("--") : messageItem->text().trimmed());
        }
    }

    m_lblFt8SequencerStatus->setText(details.isEmpty() ? stateText : stateText + QStringLiteral(" | ") + details.join(QStringLiteral(" | ")));
    setFt8ActiveTxRow(m_ftSession.activeTxRow);
    updateFt8TxBannerUi();
}

void MainWindow::setFt8ActiveTxRow(int row, bool clearWhenIdle)
{
    if (m_tableFt8TxMessages == nullptr) {
        m_ftSession.activeTxRow = row;
        return;
    }

    const QSignalBlocker blockTxTable(m_tableFt8TxMessages);

    if (clearWhenIdle) {
        row = -1;
    }
    if (row >= m_tableFt8TxMessages->rowCount()) {
        row = -1;
    }
    m_ftSession.activeTxRow = row;

    for (int r = 0; r < m_tableFt8TxMessages->rowCount(); ++r) {
        QTableWidgetItem *arrow = m_tableFt8TxMessages->item(r, 0);
        if (arrow == nullptr) {
            arrow = new QTableWidgetItem();
            arrow->setTextAlignment(Qt::AlignCenter);
            arrow->setFlags(arrow->flags() & ~Qt::ItemIsEditable);
            m_tableFt8TxMessages->setItem(r, 0, arrow);
        }
        arrow->setText(r == row ? QStringLiteral("▶") : QString());
        QFont f = arrow->font();
        f.setBold(r == row);
        arrow->setFont(f);
        for (int c = 1; c < m_tableFt8TxMessages->columnCount(); ++c) {
            QTableWidgetItem *it = m_tableFt8TxMessages->item(r, c);
            if (it == nullptr) {
                continue;
            }
            QFont itemFont = it->font();
            itemFont.setBold(r == row);
            it->setFont(itemFont);
        }
    }
    if (row >= 0) {
        m_tableFt8TxMessages->selectRow(row);
        QTableWidgetItem *arrow = m_tableFt8TxMessages->item(row, 0);
        if (arrow != nullptr) {
            m_tableFt8TxMessages->scrollToItem(arrow, QAbstractItemView::PositionAtCenter);
        }
    }
}

void MainWindow::updateFt8TxBannerUi()
{
    if (m_lblFt8TxBanner == nullptr) {
        return;
    }

    const QString activeModeName = (ui != nullptr && ui->cmbMode != nullptr)
        ? ui->cmbMode->currentText()
        : QString();
    const bool ftMode = Ft8Mode::isFamilyMode(activeModeName);
    m_lblFt8TxBanner->setVisible(ftMode);
    if (!ftMode) {
        return;
    }

    auto rowLabelForMessage = [this](const QString &message) -> QString {
        const QString wanted = message.trimmed().toUpper();
        if (wanted.isEmpty() || m_tableFt8TxMessages == nullptr) {
            return QString();
        }
        for (int r = 0; r < m_tableFt8TxMessages->rowCount(); ++r) {
            QTableWidgetItem *msgItem = m_tableFt8TxMessages->item(r, 2);
            if (msgItem != nullptr && msgItem->text().trimmed().toUpper() == wanted) {
                QTableWidgetItem *labelItem = m_tableFt8TxMessages->item(r, 1);
                const QString label = labelItem != nullptr ? labelItem->text().trimmed() : QString();
                return label.isEmpty() ? QStringLiteral("TX%1").arg(r + 1) : label;
            }
        }
        return QString();
    };

    auto utcTextForBoundary = [](qint64 boundaryMs) -> QString {
        if (boundaryMs <= 0) {
            return QStringLiteral("--");
        }
        return QDateTime::fromMSecsSinceEpoch(boundaryMs, Qt::UTC)
            .toString(QStringLiteral("HH:mm:ss 'UTC'"));
    };

    const bool onAir = (m_txRunning || m_ftTxWorkerRunning) && !m_ftSession.lastTxWasTune && !m_ftSession.lastTxMessage.trimmed().isEmpty();
    const bool schedulerArmed = m_ft8PendingTxArmed && !m_pendingFt8TxMessage.trimmed().isEmpty() && !m_pendingFt8Tune;
    const bool sequencerReady = !schedulerArmed && !m_pendingFt8TxMessage.trimmed().isEmpty() && !m_pendingFt8Tune;

    QString text;
    QString style;
    QString toolTip;

    if (onAir) {
        const QString row = rowLabelForMessage(m_ftSession.lastTxMessage);
        const QString tag = m_ftSession.lastTxTag.trimmed().isEmpty() ? QStringLiteral("TX") : m_ftSession.lastTxTag.trimmed().toUpper();
        text = QStringLiteral("<b>%1</b> — %2%3<br><span style='font-size:15pt;'>%4</span>")
                   .arg(htmlEscaped(uiText("ft_tx_banner_on_air", "ON AIR NOW")),
                        htmlEscaped(tag),
                        row.isEmpty() ? QString() : QStringLiteral(" / ") + htmlEscaped(row),
                        htmlEscaped(m_ftSession.lastTxMessage));
        style = QStringLiteral("QLabel { border: 2px solid #8a1f1f; border-radius: 6px; padding: 6px; background: #3b1010; color: #ffe5e5; font-weight: 500; font-size: 10pt; }");
        toolTip = uiText("ft_tx_banner_on_air_tip", "The FT transmitter is on air now. This is the exact message currently being sent.");
    } else if (schedulerArmed) {
        const QString row = rowLabelForMessage(m_pendingFt8TxMessage);
        const QString tag = m_pendingFt8TxTag.trimmed().isEmpty() ? QStringLiteral("TX") : m_pendingFt8TxTag.trimmed().toUpper();
        const QString when = utcTextForBoundary(m_pendingFt8SlotBoundaryUtcMs);
        text = QStringLiteral("<b>%1</b> — %2%3 — %4<br><span style='font-size:15pt;'>%5</span>")
                   .arg(htmlEscaped(uiText("ft_tx_banner_armed", "TX SCHEDULER ARMED")),
                        htmlEscaped(tag),
                        row.isEmpty() ? QString() : QStringLiteral(" / ") + htmlEscaped(row),
                        htmlEscaped(when),
                        htmlEscaped(m_pendingFt8TxMessage));
        style = QStringLiteral("QLabel { border: 2px solid #a06400; border-radius: 6px; padding: 6px; background: #332100; color: #fff0cc; font-weight: 500; font-size: 10pt; }");
        toolTip = uiText("ft_tx_banner_armed_tip", "The TX scheduler is armed: PTT/audio will become mutex with RX only at the selected UTC transmit boundary.");
    } else if (sequencerReady) {
        const QString row = rowLabelForMessage(m_pendingFt8TxMessage);
        const QString tag = m_pendingFt8TxTag.trimmed().isEmpty() ? QStringLiteral("TX") : m_pendingFt8TxTag.trimmed().toUpper();
        const QString when = utcTextForBoundary(m_pendingFt8SlotBoundaryUtcMs);
        text = QStringLiteral("<b>%1</b> — %2%3 — %4<br><span style='font-size:15pt;'>%5</span>")
                   .arg(htmlEscaped(uiText("ft_tx_banner_seq_ready", "SEQUENCER READY — RX STILL OPEN")),
                        htmlEscaped(tag),
                        row.isEmpty() ? QString() : QStringLiteral(" / ") + htmlEscaped(row),
                        htmlEscaped(when),
                        htmlEscaped(m_pendingFt8TxMessage));
        style = QStringLiteral("QLabel { border: 2px solid #1f6b4a; border-radius: 6px; padding: 6px; background: #10251b; color: #d9ffe9; font-weight: 500; font-size: 10pt; }");
        toolTip = uiText("ft_tx_banner_seq_ready_tip", "The QSO sequencer has selected the next message, but the TX scheduler is intentionally not armed yet; RX continues to collect and decode the current slot.");
    } else if (!m_ftSession.lastTxMessage.trimmed().isEmpty() && !m_ftSession.lastTxWasTune) {
        const QString row = rowLabelForMessage(m_ftSession.lastTxMessage);
        text = QStringLiteral("<b>%1</b> — %2%3<br><span style='font-size:13pt;'>%4</span>")
                   .arg(htmlEscaped(uiText("ft_tx_banner_rx_monitor", "RX MONITOR")),
                        htmlEscaped(uiText("ft_tx_banner_last_tx", "last TX")),
                        row.isEmpty() ? QString() : QStringLiteral(" / ") + htmlEscaped(row),
                        htmlEscaped(m_ftSession.lastTxMessage));
        style = QStringLiteral("QLabel { border: 2px solid #245a36; border-radius: 6px; padding: 6px; background: #102218; color: #d8ffe2; font-weight: 500; font-size: 10pt; }");
        toolTip = uiText("ft_tx_banner_rx_last_tip", "Receiver is active. The banner shows the last completed FT transmission.");
    } else {
        text = QStringLiteral("<b>%1</b>").arg(htmlEscaped(uiText("ft_tx_banner_rx", "RX monitor — no FT TX armed")));
        style = QStringLiteral("QLabel { border: 2px solid #33414d; border-radius: 6px; padding: 6px; background: #17202a; color: #d7e0e7; font-weight: 500; font-size: 10pt; }");
        toolTip = uiText("ft_tx_banner_idle_tip", "FT receiver monitor is active and no TX message is currently armed.");
    }

    m_lblFt8TxBanner->setText(text);
    m_lblFt8TxBanner->setStyleSheet(style);
    m_lblFt8TxBanner->setToolTip(toolTip);
}

void MainWindow::updateFt8SignalReportUi()
{
    if (m_lblFt8LastSnr == nullptr && m_lblFt8TxReport == nullptr) {
        return;
    }

    if (!m_ftSession.haveLastSnr) {
        if (m_lblFt8LastSnr != nullptr) {
            m_lblFt8LastSnr->setText(uiText("ft8_last_snr", "Last decoded SNR") + QStringLiteral(": -- dB"));
        }
        if (m_lblFt8TxReport != nullptr) {
            m_lblFt8TxReport->setText(uiText("ft8_tx_report", "TX report") + QStringLiteral(": --"));
        }
        return;
    }

    const QString baseReport = formatFt8SignalReport(m_ftSession.lastSnrDb, false);
    const QString acknowledgedReport = formatFt8SignalReport(m_ftSession.lastSnrDb, true);

    if (m_lblFt8LastSnr != nullptr) {
        QString text = uiText("ft8_last_snr", "Last decoded SNR") + QStringLiteral(": ") +
                       baseReport + QStringLiteral(" dB");
        if (!m_ftSession.lastSnrMessage.trimmed().isEmpty()) {
            text += QStringLiteral("  —  ") + m_ftSession.lastSnrMessage.trimmed().left(48);
        }
        m_lblFt8LastSnr->setText(text);
    }

    if (m_lblFt8TxReport != nullptr) {
        m_lblFt8TxReport->setText(uiText("ft8_tx_report", "TX report") +
                                  QStringLiteral(": ") + baseReport +
                                  QStringLiteral("  /  ") + acknowledgedReport);
        m_lblFt8TxReport->setToolTip(uiText("ft8_tx_report_tooltip",
                                            "FT8 reports are SNR in dB. Send the plain report first; send the R-prefixed report when acknowledging the correspondent's report."));
    }
}


void MainWindow::handleFt8DecodePerformance(const Ft8RxDecoder::PerfStats &stats)
{
    m_lastFt8PerfStats = stats;
    m_haveFt8PerfStats = true;

    if (!stats.offline) {
        const QString slotKey = stats.slotUtc.trimmed().isEmpty() ? QStringLiteral("--") : stats.slotUtc.trimmed();
        int existingSlot = -1;
        for (int i = 0; i < m_ft8RecentLiveDecodeSlots.size(); ++i) {
            if (m_ft8RecentLiveDecodeSlots.at(i) == slotKey) {
                existingSlot = i;
                break;
            }
        }
        if (existingSlot >= 0) {
            // A live slot can have an early gate pass plus a boundary pass.
            // Store total CPU time for the slot, not one entry per pass.
            m_ft8RecentLiveDecodeMs[existingSlot] += stats.totalMs;
        } else {
            m_ft8RecentLiveDecodeSlots.append(slotKey);
            m_ft8RecentLiveDecodeMs.append(stats.totalMs);
        }
        while (m_ft8RecentLiveDecodeMs.size() > 10) {
            m_ft8RecentLiveDecodeMs.removeFirst();
            m_ft8RecentLiveDecodeSlots.removeFirst();
        }
    }

    double liveAvgMs = 0.0;
    double liveMaxMs = 0.0;
    for (double value : m_ft8RecentLiveDecodeMs) {
        liveAvgMs += value;
        liveMaxMs = qMax(liveMaxMs, value);
    }
    if (!m_ft8RecentLiveDecodeMs.isEmpty()) {
        liveAvgMs /= static_cast<double>(m_ft8RecentLiveDecodeMs.size());
    }

    const QString phase = stats.phase.trimmed().isEmpty()
        ? (stats.offline ? QStringLiteral("offline") : QStringLiteral("live"))
        : stats.phase.trimmed();

    // Keep the decode stopwatch in the runtime log and in the dedicated
    // FT decode tab.  The field log must always show per-slot latency because
    // multi-second FT8 decodes are a real QSO-timing regression.
    appendLog(QStringLiteral("FT decode profiler: %1, slot %2, %3 pass(es), %4 candidate(s), %5 decode(s), total %6 ms [search %7 ms, LDPC %8 ms, subtract %9 ms, sync-gate %10, LDPC tried %11, workers %12]")
                  .arg(phase)
                  .arg(stats.slotUtc.trimmed().isEmpty() ? QStringLiteral("--") : stats.slotUtc)
                  .arg(stats.passCount)
                  .arg(stats.candidateCount)
                  .arg(stats.decodeCount)
                  .arg(stats.totalMs, 0, 'f', 0)
                  .arg(stats.candidateSearchMs, 0, 'f', 0)
                  .arg(stats.candidateDecodeMs, 0, 'f', 0)
                  .arg(stats.subtractionMs, 0, 'f', 0)
                  .arg(stats.syncGateRejects)
                  .arg(stats.ldpcTried)
                  .arg(stats.workerCount));

    if (stats.osdGf2Tried > 0 || stats.osdGf2Recovered > 0) {
        appendLog(QStringLiteral("FT OSD GF2 lab: tried %1, recovered %2, rank-fail %3, pivot-skip %4, order0/1/2 %5/%6/%7, post-CRC reject %8, budget-skip %9, %10 ms")
                      .arg(stats.osdGf2Tried)
                      .arg(stats.osdGf2Recovered)
                      .arg(stats.osdGf2RankFails)
                      .arg(stats.osdGf2PivotSkips)
                      .arg(stats.osdGf2Order0Hits)
                      .arg(stats.osdGf2Order1Hits)
                      .arg(stats.osdGf2Order2Hits)
                      .arg(stats.osdGf2PostCrcRejects)
                      .arg(stats.osdGf2BudgetSkips)
                      .arg(stats.osdGf2TotalMs, 0, 'f', 1));
    }

    if (m_ddspController != nullptr && !stats.offline) {
        m_ddspController->updateFtMindGainStats(stats.mindAssistExtraDecodes,
                                                stats.mindAssistRecovered,
                                                stats.mindAssistTried,
                                                stats.mindAssistAvgConfidence);
    }

    if (stats.mindAssistTried > 0 || stats.mindAssistRecovered > 0 ||
        stats.mindAssistExtraDecodes > 0 || stats.mindAssistUnavailable > 0) {
        appendLog(QStringLiteral("FT MIND Ranker: scored %1, pruned %2, extra %3, unavailable %4, avg success %5%")
                      .arg(stats.mindAssistTried)
                      .arg(stats.mindAssistRecovered)
                      .arg(stats.mindAssistExtraDecodes)
                      .arg(stats.mindAssistUnavailable)
                      .arg(stats.mindAssistAvgConfidence, 0, 'f', 1));
    }

    if (!stats.offline && !m_ft8RecentLiveDecodeMs.isEmpty()) {
        appendLog(QStringLiteral("FT live decode rolling: last %1 slot(s), avg %2 ms, max %3 ms")
                      .arg(m_ft8RecentLiveDecodeMs.size())
                      .arg(liveAvgMs, 0, 'f', 0)
                      .arg(liveMaxMs, 0, 'f', 0));
    }

    if (!stats.offline && stats.passCount > 1 && stats.totalMs > 1000.0) {
        appendLog(uiText("ft_decode_warning_adaptive_slow",
                         "WARNING: FT unified live decode exceeded 1 second; reduce FT workload or check CPU load if the PC starts missing slots."));
    }

    if (stats.timeBudgetHit && !stats.earlyStopReason.trimmed().isEmpty()) {
        appendLog(QStringLiteral("FT decode profiler: ") + stats.earlyStopReason.trimmed());
    }

    updateFt8DecodePerformanceUi();
}

void MainWindow::handleFt8NtpStatus(bool synced, const QString &statusText)
{
    m_ft8NtpSynced = synced;
    m_ft8NtpStatusText = statusText;
    if (m_ntpClient != nullptr) {
        m_ft8NtpServers = m_ntpClient->lastServerCount();
        m_ft8NtpRttMs = m_ntpClient->lastRttMs();
        m_ft8NtpLastSyncUtc = m_ntpClient->lastSyncUtc();
    }
    updateFt8DecodePerformanceUi();
}

void MainWindow::handleFt8NtpOffset(double offsetMs)
{
    m_ft8NtpOffsetMs = offsetMs;
    if (m_ntpClient != nullptr) {
        m_ft8NtpSynced = m_ntpClient->isSynced();
        m_ft8NtpStatusText = m_ntpClient->lastStatusText();
        m_ft8NtpServers = m_ntpClient->lastServerCount();
        m_ft8NtpRttMs = m_ntpClient->lastRttMs();
        m_ft8NtpLastSyncUtc = m_ntpClient->lastSyncUtc();
    }
    updateFt8DecodePerformanceUi();
}

void MainWindow::updateFt8DecodePerformanceUi()
{
    if (m_grpFt8DecodePerformance != nullptr) {
        m_grpFt8DecodePerformance->setTitle(QString());
    }
    if (m_lblFt8DecodePerformance == nullptr) {
        return;
    }

    const QString systemClockState = cachedSystemClockSyncText();
    QString ntpState = m_ft8NtpStatusText.trimmed().isEmpty()
        ? systemClockState
        : m_ft8NtpStatusText.trimmed();
    bool effectiveTimeOk = m_ft8NtpSynced;
    if (!m_ft8NtpSynced && systemClockState == QStringLiteral("synced")) {
        effectiveTimeOk = true;
        if (ntpState.contains(QStringLiteral("no UDP response"), Qt::CaseInsensitive) ||
            ntpState.contains(QStringLiteral("no response"), Qt::CaseInsensitive) ||
            ntpState.contains(QStringLiteral("no reply"), Qt::CaseInsensitive) ||
            ntpState.contains(QStringLiteral("trying next"), Qt::CaseInsensitive)) {
            ntpState = uiText("ft8_diag_ntp_udp_blocked_system_ok", "System clock synced; UDP NTP still retrying");
        }
    }
    const double rxPpm = m_settings.audioRxClockPpm;

    double avgDt = 0.0;
    for (double dt : m_ft8RecentDtSeconds) {
        avgDt += dt;
    }
    if (!m_ft8RecentDtSeconds.isEmpty()) {
        avgDt /= static_cast<double>(m_ft8RecentDtSeconds.size());
    }
    const double dtCorrectionMs = -avgDt * 1000.0;

    QStringList lastDt;
    const int firstDt = qMax(0, m_ft8RecentDtSeconds.size() - 6);
    for (int i = firstDt; i < m_ft8RecentDtSeconds.size(); ++i) {
        lastDt << QString::number(m_ft8RecentDtSeconds.at(i), 'f', 2);
    }

    QString convergence = uiText("ft8_diag_waiting", "WAITING");
    if (m_ft8RecentDtSeconds.size() >= 5 && qAbs(avgDt) <= 0.35) {
        convergence = uiText("ft8_diag_locked", "LOCKED");
    } else if (m_ft8RecentDtSeconds.size() >= 3) {
        convergence = uiText("ft8_diag_tracking", "TRACKING");
    }

    double liveAvgForWarningMs = 0.0;
    for (double value : m_ft8RecentLiveDecodeMs) {
        liveAvgForWarningMs += value;
    }
    if (!m_ft8RecentLiveDecodeMs.isEmpty()) {
        liveAvgForWarningMs /= static_cast<double>(m_ft8RecentLiveDecodeMs.size());
    }

    QString warning = uiText("ft8_diag_none", "None");
    if (!effectiveTimeOk && systemClockState == QStringLiteral("not synced")) {
        warning = uiText("ft8_diag_warning_clock", "System clock is not NTP-synchronized");
    } else if (!effectiveTimeOk && systemClockState == QStringLiteral("unknown")) {
        warning = uiText("ft8_diag_warning_ntp_unknown", "NTP status unknown; verify PC clock before FT8");
    } else if (!m_ft8RecentLiveDecodeMs.isEmpty() && liveAvgForWarningMs > 1000.0) {
        warning = uiText("ft8_diag_warning_live_slow", "Unified live decode average is above 1 s; check CPU load on slow PCs");
    } else if (m_haveFt8PerfStats && m_lastFt8PerfStats.totalMs > 9000.0) {
        warning = uiText("ft8_diag_warning_slow", "Decode is close to the next FT8 slot");
    } else if (qAbs(avgDt) > 1.0 && m_ft8RecentDtSeconds.size() >= 3) {
        warning = uiText("ft8_diag_warning_dt", "Average DT is large; check PC clock or audio timing");
    }

    const QString latencyText = m_haveFt8PerfStats
        ? QStringLiteral("%1 ms").arg(m_lastFt8PerfStats.totalMs, 0, 'f', 0)
        : QStringLiteral("--");
    const QString latencyColour = (m_haveFt8PerfStats && m_lastFt8PerfStats.totalMs > 3000.0)
        ? QStringLiteral("#d84a4a")
        : QStringLiteral("#2f80ed");

    double liveAvgMs = 0.0;
    double liveMaxMs = 0.0;
    for (double value : m_ft8RecentLiveDecodeMs) {
        liveAvgMs += value;
        liveMaxMs = qMax(liveMaxMs, value);
    }
    if (!m_ft8RecentLiveDecodeMs.isEmpty()) {
        liveAvgMs /= static_cast<double>(m_ft8RecentLiveDecodeMs.size());
    }
    const QString liveAvgText = m_ft8RecentLiveDecodeMs.isEmpty()
        ? QStringLiteral("--")
        : QStringLiteral("%1 ms").arg(liveAvgMs, 0, 'f', 0);
    const QString liveMaxText = m_ft8RecentLiveDecodeMs.isEmpty()
        ? QStringLiteral("--")
        : QStringLiteral("%1 ms").arg(liveMaxMs, 0, 'f', 0);
    const QString phaseText = (m_haveFt8PerfStats && !m_lastFt8PerfStats.phase.trimmed().isEmpty())
        ? m_lastFt8PerfStats.phase.trimmed()
        : QStringLiteral("--");

    QString html;
    QTextStream out(&html);
    auto row = [&out](const QString &label, const QString &value) {
        out << "<tr><td>" << htmlEscaped(label) << ":</td><td>" << value << "</td></tr>";
    };

    out << "<div style='font-family:monospace;'>";
    out << "<table width='100%' cellspacing='3' cellpadding='1'>";

    out << "<tr><td colspan='2'><b>" << htmlEscaped(uiText("ft8_diag_decode_timing", "Decode timing")) << "</b></td></tr>";
    row(uiText("ft8_diag_avg_dt", "Avg DT"),
        QStringLiteral("<span style='color:#2f80ed;'>%1 s</span>").arg(avgDt, 0, 'f', 3));
    row(uiText("ft8_diag_dt_correction", "DT correction"),
        htmlEscaped(QStringLiteral("%1 ms").arg(dtCorrectionMs, 0, 'f', 1)));
    row(uiText("ft8_decode_perf_decodes", "Decodes used"),
        htmlEscaped(QString::number(m_ft8RecentDtSeconds.size())));
    row(uiText("ft8_decode_perf_total", "Decode latency"),
        QStringLiteral("<span style='color:%1;'>%2</span>").arg(latencyColour, htmlEscaped(latencyText)));
    row(uiText("ft8_decode_perf_phase", "Last phase"),
        htmlEscaped(phaseText));
    row(uiText("ft8_decode_perf_live_avg", "Live avg 10"),
        htmlEscaped(liveAvgText));
    row(uiText("ft8_decode_perf_live_max", "Live max 10"),
        htmlEscaped(liveMaxText));
    row(uiText("ft8_diag_last_dts", "Last DTs"),
        htmlEscaped(lastDt.isEmpty() ? QStringLiteral("--") : lastDt.join(QStringLiteral(", "))));
    row(uiText("ft8_diag_convergence", "Convergence"),
        QStringLiteral("<span style='color:#2f80ed; font-weight:bold;'>%1</span>").arg(htmlEscaped(convergence)));
    row(uiText("ft8_diag_sc_drift_comp", "SC drift comp"),
        htmlEscaped(QStringLiteral("%1 ppm").arg(rxPpm, 0, 'f', 1)));

    row(uiText("ft8_diag_warning", "Warning"),
        QStringLiteral("<span style='color:%1;'>%2</span>").arg(ft8StatusColour(warning), htmlEscaped(warning)));
    out << "</table></div>";

    m_lblFt8DecodePerformance->setText(html);
}


QString MainWindow::selectFt8TxRow(int row)
{
    if (m_tableFt8TxMessages == nullptr) {
        return QString();
    }
    row = qBound(0, row, m_tableFt8TxMessages->rowCount() - 1);
    setFt8ActiveTxRow(row);

    const QString generated = m_ftStandardMessages.message(row);
    if (!generated.trimmed().isEmpty()) {
        return generated.trimmed().toUpper();
    }

    QTableWidgetItem *item = m_tableFt8TxMessages->item(row, 2);
    return item != nullptr ? item->text().trimmed().toUpper() : QString();
}


void MainWindow::scheduleFt8SequencerMessage(const QString &message, const QString &tag)
{
    const QString txMessage = message.trimmed().toUpper();
    if (txMessage.isEmpty()) {
        appendLog("FT8 sequencer: empty message not scheduled.");
        return;
    }

    if (m_spinFt8TxFreq != nullptr) {
        const int correspondentHz = (m_ftSession.audioFreqHz > 0)
            ? m_ftSession.audioFreqHz
            : ((m_spinFt8RxFreq != nullptr) ? m_spinFt8RxFreq->value() : m_settings.ft8RxFrequencyHz);
        QString why;
        const int txHz = resolveFt8TxFrequencyForStrategy(correspondentHz, &why);
        if (txHz >= m_spinFt8TxFreq->minimum() && txHz <= m_spinFt8TxFreq->maximum() &&
            m_spinFt8TxFreq->value() != txHz) {
            const QSignalBlocker blockTx(m_spinFt8TxFreq);
            m_spinFt8TxFreq->setValue(txHz);
            m_settings.ft8TxFrequencyHz = txHz;
            updateWaterfallMarkers();
            appendLog(QString("FT frequency: TX marker prepared at %1 Hz (%2).").arg(txHz).arg(why));
        }
    }

    if (m_txRunning || (m_txAudioEngine != nullptr && m_txAudioEngine->isRunning()) || m_ftTxWorkerRunning) {
        // WSJT-X can change the selected next message while a transmission is
        // already in progress; the current audio cannot be replaced safely, but
        // the next slot must not be lost.  Keep a deferred TX plan and arm it as
        // soon as the active FT worker stops, before any old retry logic runs.
        m_hasDeferredFt8TxPlan = true;
        m_deferredFt8TxMessage = txMessage;
        m_deferredFt8TxTag = tag.trimmed().isEmpty() ? QStringLiteral("TX") : tag.trimmed().toUpper();
        m_deferredFt8TxPlan = m_ftStandardMessages.makePlanForMessage(txMessage, m_deferredFt8TxTag);
        if (m_deferredFt8TxPlan.audioFrequencyHz <= 0) {
            m_deferredFt8TxPlan.audioFrequencyHz = (m_spinFt8TxFreq != nullptr) ? m_spinFt8TxFreq->value() : m_settings.ft8TxFrequencyHz;
        }
        m_ftSession.deferredState = m_ftSession.state;
        appendLog("FT sequencer: TX is active; queued next-slot message: " + txMessage);
        updateFt8TxBannerUi();
        return;
    }

    if (m_ftSlotScheduler != nullptr) {
        QMetaObject::invokeMethod(m_ftSlotScheduler, "cancelTransmission", Qt::QueuedConnection);
    }
    if (m_pendingFt8PttKeyed && !m_txRunning && !m_ftTxWorkerRunning) {
        unkeyPttAfterTx();
    }
    m_ft8PendingTxArmed = false;
    m_ft8PendingTxToken.clear();
    m_pendingFt8PttPrearmed = false;
    m_pendingFt8PttKeyed = false;

    m_pendingFt8Tune = false;
    m_pendingFt8PreSilenceMs = 0;
    m_pendingFt8TxMessage = txMessage;
    m_pendingFt8TxTag = tag.trimmed().isEmpty() ? QStringLiteral("TX") : tag.trimmed().toUpper();
    m_pendingFt8TxPlan = m_ftStandardMessages.makePlanForMessage(txMessage, m_pendingFt8TxTag);
    if (m_pendingFt8TxPlan.audioFrequencyHz <= 0) {
        m_pendingFt8TxPlan.audioFrequencyHz = (m_spinFt8TxFreq != nullptr) ? m_spinFt8TxFreq->value() : m_settings.ft8TxFrequencyHz;
    }

    if (m_tableFt8TxMessages != nullptr) {
        int matchedRow = m_ftStandardMessages.findRow(txMessage);
        if (matchedRow < 0) {
            for (int r = 0; r < m_tableFt8TxMessages->rowCount(); ++r) {
                QTableWidgetItem *msgItem = m_tableFt8TxMessages->item(r, 2);
                if (msgItem != nullptr && msgItem->text().trimmed().toUpper() == txMessage) {
                    matchedRow = r;
                    break;
                }
            }
        }
        if (matchedRow >= 0) {
            m_pendingFt8TxPlan.row = matchedRow;
            setFt8ActiveTxRow(matchedRow);
        }
    }

    if (m_pendingFt8TxTag != QStringLiteral("RETRY") &&
        m_pendingFt8TxTag != QStringLiteral("CQ") &&
        m_pendingFt8TxTag != QStringLiteral("TUNE")) {
        // Keep the current exchange sticky, but not forever.  Count the first
        // transmission plus automatic retries; if the DX never answers, MM must
        // return to RX standby instead of calling the same station indefinitely.
        m_ftSession.retryMessage = txMessage;
        m_ftSession.retryTag = m_pendingFt8TxTag;
        m_ftSession.retryRemaining = qBound(1, m_settings.ft8NoResponseRetryCount, 12);
    }

    const int delayMs = millisecondsToNextFt8TxPeriod();
    const QString modeName = (ui != nullptr && ui->cmbMode != nullptr) ? ui->cmbMode->currentText() : QStringLiteral("FT8");
    const Ft8Mode::Profile profile = Ft8Mode::profileForMode(modeName);

    m_pendingFt8SlotBoundaryUtcMs = selectedFt8TxSlotBoundaryUtcMs();
    m_pendingFt8TxPlan.slotBoundaryUtcMs = m_pendingFt8SlotBoundaryUtcMs;
    // Keep PTT/audio backend armed before the selected UTC slot, then prepend
    // or skip samples so useful FT tones are anchored to the protocol start
    // offset.  This gives the decoder/sequencer the short WSJT-X-style post-RX
    // window while avoiding the old UI-timer/startImageTx delay.
    const bool ft4Timing = (profile.shortLabel.compare(QStringLiteral("FT4"), Qt::CaseInsensitive) == 0);
    m_pendingFt8AudioTargetDelayMs = ft4Timing ? 300 : 500;
    // Do not key PTT before the RX slot has fully closed.  The FT waveform
    // already carries WSJT-X-style leading silence (FT8 500 ms, FT4 300 ms),
    // so PTT can be asserted at the UTC boundary without losing the last
    // second of RX audio/decode gating.
    m_pendingFt8PttLeadMs = 0;
    m_pendingFt8TxPlan.audioTargetDelayMs = m_pendingFt8AudioTargetDelayMs;
    m_pendingFt8TxPlan.pttLeadMs = m_pendingFt8PttLeadMs;

    m_pendingFt8PreparedModulator.reset();
    if (!m_pendingFt8Tune) {
        const double txHz = (m_spinFt8TxFreq != nullptr)
                                ? static_cast<double>(m_spinFt8TxFreq->value())
                                : 1500.0;
        std::unique_ptr<Ft8Transmitter> prepared(new Ft8Transmitter(
            profile.modeName,
            m_pendingFt8TxMessage,
            48000,
            txHz,
            0));
        if (prepared->generationSucceeded()) {
            m_pendingFt8PreparedModulator = std::move(prepared);
            appendLog(QString("FT timing: %1 waveform prebuilt for UTC-slot TX.").arg(profile.shortLabel));
        } else {
            appendLog(QString("FT timing: %1 waveform prebuild failed: %2")
                          .arg(profile.shortLabel, prepared->generationError()));
        }
    }

    m_ft8PendingTxToken = QStringLiteral("%1:%2:%3")
        .arg(m_pendingFt8SlotBoundaryUtcMs)
        .arg(m_pendingFt8TxTag)
        .arg(QString::number(qHash(m_pendingFt8TxMessage)));
    m_ft8PendingTxArmed = true;

    if (m_ftSlotScheduler != nullptr) {
        QMetaObject::invokeMethod(m_ftSlotScheduler,
                                  "configure",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, profile.modeName),
                                  Q_ARG(bool, m_settings.ft8TxFirstPeriod));
        QMetaObject::invokeMethod(m_ftSlotScheduler,
                                  "armTransmission",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, m_ft8PendingTxToken),
                                  Q_ARG(qint64, m_pendingFt8SlotBoundaryUtcMs),
                                  Q_ARG(int, m_pendingFt8AudioTargetDelayMs),
                                  Q_ARG(int, m_pendingFt8PttLeadMs));
        appendLog(QString("FT timing: %1 %2 armed by UTC scheduler; next selected slot in %3 ms, PTT/audio pre-arm %4 ms before boundary: %5")
                      .arg(profile.shortLabel, m_pendingFt8TxTag)
                      .arg(delayMs)
                      .arg(m_pendingFt8PttLeadMs)
                      .arg(txMessage));

        const QString watchdogToken = m_ft8PendingTxToken;
        const qint64 watchdogBoundary = m_pendingFt8SlotBoundaryUtcMs;
        const int watchdogAudioDelay = m_pendingFt8AudioTargetDelayMs;
        const int watchdogPttLead = m_pendingFt8PttLeadMs;
        const int watchdogDelay = qBound(0, delayMs + 250, 60000);
        QTimer::singleShot(watchdogDelay, this, [this, watchdogToken, watchdogBoundary, watchdogAudioDelay, watchdogPttLead]() {
            if (m_txRunning || m_ftTxWorkerRunning) {
                return;
            }
            if (m_pendingFt8TxMessage.trimmed().isEmpty()) {
                return;
            }
            if (!m_ft8PendingTxArmed) {
                // A hard guard such as rotator pointing may have intentionally
                // disarmed this UTC slot while leaving the pending message around
                // for the next valid slot.  The watchdog must not resurrect it.
                return;
            }
            if (m_ft8PendingTxToken != watchdogToken || m_pendingFt8SlotBoundaryUtcMs != watchdogBoundary) {
                return;
            }
            const qint64 nowMs = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
            if (watchdogBoundary > 0 && nowMs + 25 < watchdogBoundary) {
                return;
            }
            QString watchdogRotatorReason;
            int watchdogRotatorEtaMs = 0;
            if (!ftRotatorReadyForPendingTx(&watchdogRotatorReason, &watchdogRotatorEtaMs)) {
                appendLog("FT timing watchdog: TX remains blocked by rotator guard; not forcing PTT/audio.");
                deferPendingFtTxForRotator(watchdogRotatorEtaMs, watchdogRotatorReason);
                return;
            }
            appendLog("FT timing watchdog: scheduler audio-start was not observed; forcing pending FT TX for the armed UTC slot.");
            m_ft8PendingTxArmed = false;
            m_pendingFt8AudioTargetDelayMs = qMax(0, watchdogAudioDelay);
            m_pendingFt8PttLeadMs = qMax(0, watchdogPttLead);
            m_pendingFt8TxPlan.audioTargetDelayMs = m_pendingFt8AudioTargetDelayMs;
            m_pendingFt8TxPlan.pttLeadMs = m_pendingFt8PttLeadMs;
            startFtPreparedSlotTransmit();
        });
    } else {
        appendLog("FT timing: scheduler unavailable, starting immediately as fallback.");
        beginScheduledFt8Transmit();
        QTimer::singleShot(0, this, [this]() {
            if (!m_txRunning && !m_ftTxWorkerRunning && !m_pendingFt8TxMessage.trimmed().isEmpty()) {
                startFtPreparedSlotTransmit();
            }
        });
    }

    updateTxControlState();
    updateFt8SequencerUi();
}

void MainWindow::updateFt8EvilModeVisibility()
{
    const bool visible = m_ft8EvilModeUnlocked;

    if (m_chkFt8CqRepeat != nullptr) {
        m_chkFt8CqRepeat->setVisible(true);
        m_chkFt8CqRepeat->setEnabled(!m_offlineAnalysisActive);
    }
    if (m_lblFt8CqTimeout != nullptr) {
        m_lblFt8CqTimeout->setVisible(true);
    }
    if (m_spinFt8CqTimeoutMin != nullptr) {
        m_spinFt8CqTimeoutMin->setVisible(true);
        m_spinFt8CqTimeoutMin->setEnabled(!m_offlineAnalysisActive);
    }
    if (m_lblFt8NoResponseLimit != nullptr) {
        m_lblFt8NoResponseLimit->setVisible(true);
    }
    if (m_spinFt8NoResponseLimit != nullptr) {
        m_spinFt8NoResponseLimit->setVisible(true);
        m_spinFt8NoResponseLimit->setEnabled(!m_offlineAnalysisActive);
    }
    if (m_chkFt8FullAutoQso != nullptr) {
        m_chkFt8FullAutoQso->setVisible(visible);
        m_chkFt8FullAutoQso->setEnabled(visible && !m_offlineAnalysisActive);
        if (!visible && m_chkFt8FullAutoQso->isChecked()) {
            const QSignalBlocker block(m_chkFt8FullAutoQso);
            m_chkFt8FullAutoQso->setChecked(false);
        }
    }

    if (!visible) {
        m_settings.ft8CqRepeat = false;
    }
    updateFt8SequencerUi();
}

void MainWindow::handleFt8EvilModeToggled(bool enabled)
{
    if (m_chkFt8EvilMode == nullptr) {
        return;
    }

    if (!enabled) {
        m_ft8EvilModeUnlocked = false;
        if (m_chkFt8FullAutoQso != nullptr && m_chkFt8FullAutoQso->isChecked()) {
            m_chkFt8FullAutoQso->setChecked(false);
        }
        updateFt8EvilModeVisibility();
        appendLog("FT Evil mode disabled: Auto QSO hidden. CQ retry remains a normal operator control.");
        return;
    }

    bool ok = false;
    const QString phrase = QInputDialog::getText(
        this,
        uiText("ft8_evil_mode_unlock_title", "Unlock Evil mode"),
        uiText("ft8_evil_mode_unlock_prompt", "Enter the unlock phrase to enable automatic FT helpers:"),
        QLineEdit::Normal,
        QString(),
        &ok).trimmed();

    const QString unlockPhrase = QStringLiteral("I AM ") + QStringLiteral("EVIL");
    if (!ok || phrase != unlockPhrase) {
        const QSignalBlocker block(m_chkFt8EvilMode);
        m_chkFt8EvilMode->setChecked(false);
        m_ft8EvilModeUnlocked = false;
        updateFt8EvilModeVisibility();
        QMessageBox::information(this,
                                 uiText("ft8_evil_mode_locked_title", "Evil mode locked"),
                                 uiText("ft8_evil_mode_locked_text", "Auto CQ / Auto QSO remain hidden. The unlock phrase was not accepted."));
        return;
    }

    m_ft8EvilModeUnlocked = true;
    updateFt8EvilModeVisibility();
    appendLog("FT Evil mode unlocked: WSJT-Z-style Auto CQ / Auto QSO controls are now visible for this session.");
}

void MainWindow::handleFt8AutoCqToggled(bool enabled)
{
    if (m_chkFt8CqRepeat == nullptr) {
        return;
    }

    applyFt8Settings();

    if (!enabled) {
        if (m_ftSession.cqRepeatActive && !m_ftSession.qsoActive) {
            stopFt8Sequencer(QStringLiteral("Auto CQ disabled"));
        }
        appendLog("FT CQ retry disabled.");
        updateFt8SequencerUi();
        return;
    }

    if (m_chkFt8FullAutoQso != nullptr && m_chkFt8FullAutoQso->isChecked()) {
        const QSignalBlocker blockAutoQso(m_chkFt8FullAutoQso);
        m_chkFt8FullAutoQso->setChecked(false);
    }

    appendLog("FT CQ retry enabled: MM will repeat the selected CQ for the configured count, then stay in RX.");

    if (ui != nullptr && ui->cmbMode != nullptr && Ft8Mode::isFamilyMode(ui->cmbMode->currentText()) &&
        !m_ftSession.qsoActive && !m_txRunning && !m_ftTxWorkerRunning && !m_ft8PendingTxArmed) {
        QTimer::singleShot(0, this, [this]() {
            if (m_chkFt8CqRepeat != nullptr && m_chkFt8CqRepeat->isChecked() &&
                !m_ftSession.qsoActive && !m_txRunning && !m_ftTxWorkerRunning && !m_ft8PendingTxArmed) {
                startFt8CqRepeat();
            }
        });
    }

    updateFt8SequencerUi();
}

void MainWindow::handleFt8FullAutoQsoToggled(bool enabled)
{
    if (m_chkFt8FullAutoQso == nullptr) {
        return;
    }

    if (enabled && !m_ft8EvilModeUnlocked) {
        const QSignalBlocker block(m_chkFt8FullAutoQso);
        m_chkFt8FullAutoQso->setChecked(false);
        updateFt8EvilModeVisibility();
        return;
    }

    if (enabled) {
        m_ft8FullAutoCqCandidates.clear();
        m_ft8FullAutoCqSelectionTimer.stop();
        if (m_chkFt8CqRepeat != nullptr && m_chkFt8CqRepeat->isChecked()) {
            const QSignalBlocker blockAutoCq(m_chkFt8CqRepeat);
            m_chkFt8CqRepeat->setChecked(false);
            m_settings.ft8CqRepeat = false;
        }
        if (m_chkFt8AutoLog != nullptr && !m_chkFt8AutoLog->isChecked()) {
            const QSignalBlocker blockAutoLog(m_chkFt8AutoLog);
            m_chkFt8AutoLog->setChecked(true);
            m_settings.ft8AutoLog = true;
        }
        if (m_ftSession.cqRepeatActive) {
            m_ftSession.cqRepeatActive = false;
            m_ftSession.resumeCqAfterQso = false;
        }
        if (!m_rxRunning && !m_txRunning && m_audioEngine != nullptr && !m_audioEngine->isRunning()) {
            startFt8RxShell();
        }
        appendLog("FT Auto QSO enabled: decoded CQs are buffered briefly and ranked by new country, new grid square, new band, new mode, distance, then SNR before answering. STOP disables it.");
        if (m_settings.ftAutoQsoFlowShadowMode) {
            appendLog("FT AutoQSO Flow shadow mode enabled: [Flow][shadow] traces will be written for relevant live decodes; no extra PTT/audio/CAT action is performed.");
        }
    } else {
        m_ft8FullAutoCqCandidates.clear();
        m_ft8FullAutoCqSelectionTimer.stop();
        appendLog("FT Auto QSO disabled: no new CQ will be answered automatically.");
    }

    updateFt8SequencerUi();
    updateTxControlState();
}

void MainWindow::runFtAutoQsoFlowShadowForDecode(const Ft8RxDecoder::Decode &decode,
                                                 bool blacklistedDecode,
                                                 bool watchedDecode)
{
    if (!m_settings.ftAutoQsoFlowShadowMode) {
        return;
    }
    const bool autoArmed = m_ft8EvilModeUnlocked &&
                           m_chkFt8FullAutoQso != nullptr &&
                           m_chkFt8FullAutoQso->isChecked();
    if (!autoArmed || m_offlineAnalysisActive) {
        return;
    }

    const QString myCall = stationCallsign();
    const ParsedFt8Message parsed = parseFt8MessageText(decode.message, myCall);
    const bool directReplyToMe = isFt8DirectReplyToMyCall(parsed, myCall);
    const QString cqCall = parsed.cq ? ft8CqCallsignFromMessage(parsed).trimmed().toUpper() : QString();
    const QString activeDx = m_ftSession.dxCall.trimmed().toUpper();

    bool activeTarget = false;
    if (m_ftSession.qsoActive && !activeDx.isEmpty()) {
        activeTarget = FtDecodedText::callMatches(parsed.senderCall, activeDx) ||
                       FtDecodedText::callMatches(parsed.firstCall, activeDx) ||
                       FtDecodedText::callMatches(parsed.secondCall, activeDx) ||
                       FtDecodedText::callMatches(cqCall, activeDx);
    }
    const bool qsoComplete = activeTarget && (parsed.final73 || parsed.rr73 || parsed.rrr);

    // Keep the runtime log useful: shadow-trace only messages that may affect
    // AutoQSO decisions, target reclaim, blacklist/drop policy, or QSO finish.
    if (!blacklistedDecode && !parsed.cq && !activeTarget && !directReplyToMe && !qsoComplete) {
        return;
    }

    Ft8FullAutoCqCandidate candidate;
    if (parsed.cq) {
        candidate = buildFt8FullAutoCqCandidate(decode);
    }

    QString targetCall = candidate.call;
    if (targetCall.isEmpty()) {
        targetCall = cqCall;
    }
    if (targetCall.isEmpty() && activeTarget) {
        targetCall = activeDx;
    }
    if (targetCall.isEmpty()) {
        targetCall = parsed.senderCall.trimmed().toUpper();
    }

    QString targetGrid = candidate.grid;
    if (targetGrid.isEmpty() && activeTarget) {
        targetGrid = m_ftSession.dxGrid.left(4).toUpper();
    }
    if (targetGrid.isEmpty() && !parsed.grid.isEmpty() && !FtDecodedText::isAckLikeGridTrap(parsed.grid)) {
        targetGrid = parsed.grid.left(4).toUpper();
    }

    QString txReason;
    const int suggestedTxHz = resolveFt8TxFrequencyForStrategy(decode.frequencyHz, &txReason);

    FtStandardMessageSet::Inputs inputs;
    inputs.myCall = myCall;
    inputs.myGrid = stationLocator();
    inputs.dxCall = targetCall;
    inputs.dxGrid = targetGrid;
    inputs.report = formatFt8SignalReport(decode.snrDb, false);
    inputs.rReport = formatFt8SignalReport(decode.snrDb, true);
    inputs.audioFrequencyHz = suggestedTxHz;
    const FtStandardMessageSet previewSet(inputs);

    FtQsoSequencer::Decode shadowSeqDecode;
    shadowSeqDecode.message = decode.message;
    shadowSeqDecode.snrDb = decode.snrDb;
    shadowSeqDecode.frequencyHz = decode.frequencyHz;
    shadowSeqDecode.slotStartUtcMs = decode.slotStartUtcMs;
    shadowSeqDecode.slotPeriodMs = decode.slotPeriodMs;

    FtQsoSequencer::Context shadowSeqContext = m_ftSession.makeContext(
        myCall,
        (m_spinFt8RxFreq != nullptr) ? m_spinFt8RxFreq->value() : m_settings.ft8RxFrequencyHz,
        suggestedTxHz > 0 ? suggestedTxHz : ((m_spinFt8TxFreq != nullptr) ? m_spinFt8TxFreq->value() : m_settings.ft8TxFrequencyHz));
    shadowSeqContext.startToleranceHz = 25;
    shadowSeqContext.stopToleranceHz = 50;
    const FtQsoSequencer::Decision shadowDecision = FtQsoSequencer::evaluateDecode(shadowSeqDecode, shadowSeqContext);

    QString suggestedMessage;
    bool shadowCompleteWithoutFurtherTx = false;
    switch (shadowDecision.action) {
    case FtQsoSequencer::Action::ArmRow:
        suggestedMessage = previewSet.message(shadowDecision.txRow);
        break;
    case FtQsoSequencer::Action::RetryCurrent:
        suggestedMessage = !m_ftSession.retryMessage.trimmed().isEmpty()
            ? m_ftSession.retryMessage.trimmed().toUpper()
            : (!m_ftSession.lastTxMessage.trimmed().isEmpty()
                   ? m_ftSession.lastTxMessage.trimmed().toUpper()
                   : QStringLiteral("<retry current sequencer message>"));
        break;
    case FtQsoSequencer::Action::Complete:
        shadowCompleteWithoutFurtherTx = true;
        suggestedMessage = QStringLiteral("<QSO complete: no further TX required>");
        break;
    case FtQsoSequencer::Action::StopTx:
        suggestedMessage = QStringLiteral("<stop pending TX to avoid QRM>");
        break;
    case FtQsoSequencer::Action::Ignore:
    case FtQsoSequencer::Action::None:
        break;
    }

    if (suggestedMessage.isEmpty()) {
        if (parsed.cq && !activeTarget) {
            suggestedMessage = previewSet.message(FtStandardMessageSet::Tx2Locator);
        } else if (activeTarget && directReplyToMe) {
            if (parsed.rr73 || parsed.rrr || parsed.final73) {
                suggestedMessage = previewSet.message(FtStandardMessageSet::Tx6Final73);
            } else if (!parsed.report.isEmpty()) {
                suggestedMessage = parsed.report.trimmed().toUpper().startsWith(QLatin1Char('R'))
                    ? previewSet.message(FtStandardMessageSet::Tx5Rr73)
                    : previewSet.message(FtStandardMessageSet::Tx4RReport);
            }
        } else if (activeTarget) {
            suggestedMessage = !m_ftSession.retryMessage.trimmed().isEmpty()
                ? m_ftSession.retryMessage.trimmed().toUpper()
                : QStringLiteral("<keep current retry/reclaim plan>");
        }
    }

    AutoQsoFlowExecutor::Context ctx;
    ctx.modeName = (ui != nullptr && ui->cmbMode != nullptr) ? ui->cmbMode->currentText() : QStringLiteral("FT8");
    ctx.decodedMessage = decode.message.trimmed().toUpper();
    ctx.myCall = myCall;
    ctx.targetCall = targetCall;
    ctx.targetGrid = targetGrid;
    ctx.country = candidate.country;
    ctx.priorityText = candidate.priorityText;
    ctx.rejectReason = candidate.priorityText;
    ctx.suggestedTxMessage = suggestedMessage;
    ctx.txStrategy = ft8TxFrequencyStrategyKey();
    ctx.txReason = txReason;
    ctx.snrDb = decode.snrDb;
    ctx.dt = decode.dt;
    ctx.rxHz = decode.frequencyHz;
    ctx.suggestedTxHz = suggestedTxHz;
    ctx.evilModeUnlocked = m_ft8EvilModeUnlocked;
    ctx.autoQsoArmed = autoArmed;
    ctx.txAllowedByRuntime = canStartFt8FullAutoQsoNow();
    ctx.qsoActive = m_ftSession.qsoActive;
    ctx.activeTarget = activeTarget;
    ctx.directReplyToMe = directReplyToMe;
    ctx.cqCandidate = parsed.cq && !targetCall.isEmpty();
    ctx.blacklisted = blacklistedDecode || (parsed.cq && !candidate.valid && candidate.priorityText == QStringLiteral("blacklisted"));
    ctx.watched = watchedDecode;
    ctx.workedBefore = !targetCall.isEmpty() && m_logbook.containsCallsign(targetCall);
    ctx.duplicateRejected = parsed.cq && !candidate.valid && !ctx.blacklisted;
    ctx.candidateValid = candidate.valid;
    ctx.retryAvailable = activeTarget && !shadowCompleteWithoutFurtherTx;
    ctx.qsoComplete = shadowCompleteWithoutFurtherTx;

    mm::MmFlowRuntime::FtContext runtimeCtx;
    runtimeCtx.autoQso = ctx;
    runtimeCtx.mode = mm::MmFlowRuntime::ExecutionMode::Shadow;
    runtimeCtx.hardwareArmed = false;
    runtimeCtx.schedulerArmed = false;
    if (m_catRotatorController != nullptr) {
        const mm::CatRotatorController::Config rotCfg = m_catRotatorController->config();
        runtimeCtx.rotatorConfigured = rotCfg.enabled;
        runtimeCtx.rotatorConnected = m_catRotatorController->isConnected();
        runtimeCtx.rotatorMoving = m_catRotatorController->isMoving();
        const mm::CatRotatorController::QsoTarget rotTarget = m_catRotatorController->qsoTarget();
        runtimeCtx.rotatorAzimuthDeg = m_catRotatorController->currentAzimuth();
        runtimeCtx.rotatorElevationDeg = m_catRotatorController->currentElevation();
        runtimeCtx.rotatorTargetBearingDeg = rotTarget.bearingDeg;
        runtimeCtx.rotatorEtaMs = rotTarget.bearingDeg >= 0.0 ? m_catRotatorController->estimatePointingTimeMs(rotTarget.bearingDeg, 0.0) : 0;
        runtimeCtx.rotatorReady = rotTarget.bearingDeg >= 0.0
            ? m_catRotatorController->isReadyForTarget(rotTarget.bearingDeg, 0.0)
            : (runtimeCtx.rotatorConfigured && runtimeCtx.rotatorConnected && !runtimeCtx.rotatorMoving);
        runtimeCtx.ftTxGuardActive = runtimeCtx.rotatorConfigured && runtimeCtx.rotatorConnected && !runtimeCtx.rotatorReady;
    }

    const mm::MmFlowRuntime::Result flowResult = mm::MmFlowRuntime::runFtDecode(m_settings.ftAutoQsoFlowJson, runtimeCtx);
    for (const QString &line : flowResult.lines) {
        appendLog(line);
    }
}

void MainWindow::selectFt8OppositePeriodFromDecode(const Ft8RxDecoder::Decode &decode)
{
    if (m_radioFt8TxFirst == nullptr || m_radioFt8TxSecond == nullptr) {
        return;
    }

    const QString modeName = (ui != nullptr && ui->cmbMode != nullptr) ? ui->cmbMode->currentText() : QStringLiteral("FT8");
    const Ft8Mode::Profile profile = Ft8Mode::profileForMode(modeName);
    const int slotMs = qMax(1000, profile.slotMs);
    const int cycleMs = qMax(slotMs * 2, profile.cycleMs);

    bool haveSlot = false;
    int cyclePosMs = 0;
    if (decode.slotStartUtcMs > 0) {
        const QTime decodedTime = QDateTime::fromMSecsSinceEpoch(decode.slotStartUtcMs, Qt::UTC).time();
        const int msOfDay = (((decodedTime.hour() * 60 + decodedTime.minute()) * 60) + decodedTime.second()) * 1000 + decodedTime.msec();
        cyclePosMs = msOfDay % cycleMs;
        haveSlot = true;
    } else if (!decode.utc.trimmed().isEmpty()) {
        QTime decodedTime = QTime::fromString(decode.utc.trimmed().left(6), QStringLiteral("HHmmss"));
        if (decodedTime.isValid()) {
            const int msOfDay = (((decodedTime.hour() * 60 + decodedTime.minute()) * 60) + decodedTime.second()) * 1000 + decodedTime.msec();
            cyclePosMs = msOfDay % cycleMs;
            haveSlot = true;
        }
    }

    if (!haveSlot) {
        return;
    }

    const bool decodedWasFirstPeriod = cyclePosMs < slotMs;
    const QSignalBlocker blockFirst(m_radioFt8TxFirst);
    const QSignalBlocker blockSecond(m_radioFt8TxSecond);
    m_radioFt8TxFirst->setChecked(!decodedWasFirstPeriod);
    m_radioFt8TxSecond->setChecked(decodedWasFirstPeriod);
    m_settings.ft8TxFirstPeriod = m_radioFt8TxFirst->isChecked();

    if (m_ftSlotScheduler != nullptr) {
        QMetaObject::invokeMethod(m_ftSlotScheduler,
                                  "configure",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, profile.modeName),
                                  Q_ARG(bool, m_settings.ft8TxFirstPeriod));
    }
    updateFt8SlotStatus();
}

bool MainWindow::canStartFt8FullAutoQsoNow() const
{
    if (!m_ft8EvilModeUnlocked || m_chkFt8FullAutoQso == nullptr || !m_chkFt8FullAutoQso->isChecked()) {
        return false;
    }
    if (m_offlineAnalysisActive || ui == nullptr || ui->cmbMode == nullptr ||
        !Ft8Mode::isFamilyMode(ui->cmbMode->currentText())) {
        return false;
    }
    if (m_ftSession.qsoActive || m_ft8PendingTxArmed || m_hasDeferredFt8TxPlan ||
        m_txRunning || m_ftTxWorkerRunning ||
        (m_txAudioEngine != nullptr && m_txAudioEngine->isRunning()) ||
        m_pendingFt8Tune || m_pendingFt8PttKeyed) {
        return false;
    }

    const QString myCall = stationCallsign();
    const QString myGrid = stationLocator();
    return !myCall.trimmed().isEmpty() && !myGrid.trimmed().isEmpty();
}

Ft8FullAutoCqCandidate MainWindow::buildFt8FullAutoCqCandidate(const Ft8RxDecoder::Decode &decode) const
{
    Ft8FullAutoCqCandidate candidate;
    candidate.decode = decode;
    candidate.queuedUtc = QDateTime::currentDateTimeUtc();
    candidate.band = (m_cmbFt8Band != nullptr) ? m_cmbFt8Band->currentText().trimmed().toLower()
                                             : m_settings.ft8Band.trimmed().toLower();
    candidate.mode = currentAdifMode().trimmed().toUpper();

    const QString myCall = stationCallsign();
    const ParsedFt8Message parsed = parseFt8MessageText(decode.message, myCall);
    if (!parsed.cq) {
        return candidate;
    }

    candidate.call = ft8CqCallsignFromMessage(parsed).trimmed().toUpper();
    if (candidate.call.isEmpty() || FtDecodedText::callMatches(candidate.call, myCall)) {
        return candidate;
    }

    if (isFtCallBlacklisted(candidate.call)) {
        candidate.priorityText = QStringLiteral("blacklisted");
        return candidate;
    }

    const auto cooldownIt = m_ft8AutoNoResponseCooldown.constFind(candidate.call);
    if (cooldownIt != m_ft8AutoNoResponseCooldown.constEnd() &&
        cooldownIt.value().isValid() && cooldownIt.value() > QDateTime::currentDateTimeUtc()) {
        candidate.priorityText = QStringLiteral("no-response cooldown");
        return candidate;
    }

    if (m_settings.ftAutoQsoDuplicatePolicy == QStringLiteral("never_worked") &&
        m_logbook.containsCallsign(candidate.call)) {
        candidate.priorityText = QStringLiteral("already in log");
        return candidate;
    }
    if (m_settings.ftAutoQsoDuplicatePolicy == QStringLiteral("recent") &&
        ftCallWorkedWithinHours(candidate.call, qBound(1, m_settings.ftAutoQsoRecentHours, 168))) {
        candidate.priorityText = QStringLiteral("recently worked");
        return candidate;
    }

    if (!m_lastCompletedFt8Call.isEmpty() && FtDecodedText::callMatches(candidate.call, m_lastCompletedFt8Call) &&
        m_lastCompletedFt8Utc.isValid() &&
        qAbs(m_lastCompletedFt8Utc.secsTo(QDateTime::currentDateTimeUtc())) <= 120) {
        candidate.priorityText = QStringLiteral("fresh duplicate after completed QSO");
        return candidate;
    }

    QString grid = parsed.grid.left(4).toUpper();
    if (!FtDecodedText::isGrid(grid) || FtDecodedText::isAckLikeGridTrap(grid)) {
        grid = m_ftSession.knownGridFor(candidate.call).left(4).toUpper();
    }
    if (FtDecodedText::isGrid(grid) && !FtDecodedText::isAckLikeGridTrap(grid)) {
        candidate.grid = grid;
        candidate.distanceGrid = grid;
    }

    const CtyCountryFile::LookupResult cty = CtyCountryFile::instance().lookupCallsign(candidate.call);
    if (cty.valid) {
        candidate.country = cty.entity.name;
        candidate.dxcc = cty.entity.dxcc;
        if (candidate.distanceGrid.isEmpty() && FtDecodedText::isGrid(cty.entity.referenceGrid.left(4))) {
            candidate.distanceGrid = cty.entity.referenceGrid.left(4).toUpper();
        }
    }

    QPointF homeLonLat;
    QPointF dxLonLat;
    if (ft8MaidenheadToLonLat(stationLocator(), &homeLonLat) &&
        ft8MaidenheadToLonLat(candidate.distanceGrid, &dxLonLat)) {
        candidate.distanceKm = ft8DistanceKm(homeLonLat, dxLonLat);
    }

    candidate.workedCall = m_logbook.containsCallsign(candidate.call);

    bool countryWorkedAny = false;
    bool countryWorkedBand = false;
    bool countryWorkedMode = false;
    bool gridWorkedAny = false;
    bool gridWorkedBand = false;
    bool gridWorkedMode = false;

    const QString targetDxcc = candidate.dxcc.trimmed();
    const QString targetGrid = ft8MaidenheadGrid4(candidate.grid);
    const QString currentBand = candidate.band.trimmed().toLower();
    const QString currentMode = candidate.mode.trimmed().toUpper();

    const QVector<LogbookEntry> records = m_logbook.records();
    for (const LogbookEntry &entry : records) {
        const QString entryBand = entry.band.trimmed().toLower();
        const QString entryMode = entry.mode.trimmed().toUpper();

        if (!targetDxcc.isEmpty()) {
            QString entryDxcc = entry.adifFields.value(QStringLiteral("DXCC")).trimmed();
            if (entryDxcc.isEmpty()) {
                const CtyCountryFile::LookupResult entryCty = CtyCountryFile::instance().lookupCallsign(entry.callsign);
                if (entryCty.valid) {
                    entryDxcc = entryCty.entity.dxcc.trimmed();
                }
            }
            if (!entryDxcc.isEmpty() && entryDxcc == targetDxcc) {
                countryWorkedAny = true;
                if (!currentBand.isEmpty() && entryBand == currentBand) {
                    countryWorkedBand = true;
                }
                if (!currentMode.isEmpty() && entryMode == currentMode) {
                    countryWorkedMode = true;
                }
            }
        }

        if (!targetGrid.isEmpty()) {
            const QString entryGrid = ft8MaidenheadGrid4(entry.grid);
            if (!entryGrid.isEmpty() && entryGrid == targetGrid) {
                gridWorkedAny = true;
                if (!currentBand.isEmpty() && entryBand == currentBand) {
                    gridWorkedBand = true;
                }
                if (!currentMode.isEmpty() && entryMode == currentMode) {
                    gridWorkedMode = true;
                }
            }
        }
    }

    candidate.newCountry = !targetDxcc.isEmpty() && !countryWorkedAny;
    candidate.newGrid = !targetGrid.isEmpty() && !gridWorkedAny;
    candidate.newBand = (!targetDxcc.isEmpty() && !countryWorkedBand) ||
                        (!targetGrid.isEmpty() && !gridWorkedBand);
    candidate.newMode = (!targetDxcc.isEmpty() && !countryWorkedMode) ||
                        (!targetGrid.isEmpty() && !gridWorkedMode);

    QStringList flags;
    if (candidate.newCountry) flags << QStringLiteral("new country");
    if (candidate.newGrid) flags << QStringLiteral("new grid");
    if (candidate.newBand) flags << QStringLiteral("new band");
    if (candidate.newMode) flags << QStringLiteral("new mode");
    if (!candidate.country.isEmpty()) flags << candidate.country;
    if (candidate.distanceKm >= 0.0) flags << QStringLiteral("%1 km").arg(QString::number(candidate.distanceKm, 'f', 0));
    if (flags.isEmpty()) flags << (candidate.workedCall ? QStringLiteral("worked before") : QStringLiteral("new call"));
    candidate.priorityText = flags.join(QStringLiteral(", "));
    candidate.valid = true;
    return candidate;
}

bool MainWindow::queueFt8FullAutoCqCandidate(const Ft8RxDecoder::Decode &decode)
{
    if (!canStartFt8FullAutoQsoNow()) {
        return false;
    }

    Ft8FullAutoCqCandidate candidate = buildFt8FullAutoCqCandidate(decode);
    if (!candidate.valid) {
        if (!candidate.call.isEmpty() && candidate.priorityText == QStringLiteral("fresh duplicate after completed QSO")) {
            appendLog(QString("FT Auto QSO priority: ignoring fresh duplicate CQ from %1 after completed QSO.")
                          .arg(candidate.call));
        } else if (!candidate.call.isEmpty() && !candidate.priorityText.isEmpty()) {
            appendLog(QString("FT Auto QSO priority: ignoring CQ from %1 (%2).").arg(candidate.call, candidate.priorityText));
        }
        return false;
    }

    const qint64 candidateSlot = candidate.decode.slotStartUtcMs;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    for (int i = m_ft8FullAutoCqCandidates.size() - 1; i >= 0; --i) {
        const Ft8FullAutoCqCandidate &old = m_ft8FullAutoCqCandidates.at(i);
        if ((candidateSlot > 0 && old.decode.slotStartUtcMs > 0 && old.decode.slotStartUtcMs != candidateSlot) ||
            (old.queuedUtc.isValid() && old.queuedUtc.secsTo(now) > 4)) {
            m_ft8FullAutoCqCandidates.removeAt(i);
        }
    }

    auto betterCandidate = [](const Ft8FullAutoCqCandidate &a, const Ft8FullAutoCqCandidate &b) {
        if (a.newCountry != b.newCountry) return a.newCountry;
        if (a.newGrid != b.newGrid) return a.newGrid;
        if (a.newBand != b.newBand) return a.newBand;
        if (a.newMode != b.newMode) return a.newMode;
        if (a.workedCall != b.workedCall) return !a.workedCall;
        if (qAbs(a.distanceKm - b.distanceKm) > 1.0) return a.distanceKm > b.distanceKm;
        if (a.decode.snrDb != b.decode.snrDb) return a.decode.snrDb > b.decode.snrDb;
        return a.decode.frequencyHz < b.decode.frequencyHz;
    };

    bool replaced = false;
    for (Ft8FullAutoCqCandidate &old : m_ft8FullAutoCqCandidates) {
        if (FtDecodedText::callMatches(old.call, candidate.call)) {
            if (betterCandidate(candidate, old)) {
                old = candidate;
            }
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        m_ft8FullAutoCqCandidates.append(candidate);
    }

    const QString modeName = (ui != nullptr && ui->cmbMode != nullptr) ? ui->cmbMode->currentText() : QStringLiteral("FT8");
    const Ft8Mode::Profile profile = Ft8Mode::profileForMode(modeName);
    const int delayMs = (profile.slotMs <= 8000) ? 280 : 420;
    m_ft8FullAutoCqSelectionTimer.start(delayMs);
    return true;
}

void MainWindow::processFt8FullAutoCqCandidates()
{
    if (m_ft8FullAutoCqCandidates.isEmpty()) {
        return;
    }
    if (!canStartFt8FullAutoQsoNow()) {
        m_ft8FullAutoCqCandidates.clear();
        return;
    }

    auto betterCandidate = [](const Ft8FullAutoCqCandidate &a, const Ft8FullAutoCqCandidate &b) {
        if (a.newCountry != b.newCountry) return a.newCountry;
        if (a.newGrid != b.newGrid) return a.newGrid;
        if (a.newBand != b.newBand) return a.newBand;
        if (a.newMode != b.newMode) return a.newMode;
        if (a.workedCall != b.workedCall) return !a.workedCall;
        if (qAbs(a.distanceKm - b.distanceKm) > 1.0) return a.distanceKm > b.distanceKm;
        if (a.decode.snrDb != b.decode.snrDb) return a.decode.snrDb > b.decode.snrDb;
        return a.decode.frequencyHz < b.decode.frequencyHz;
    };

    std::sort(m_ft8FullAutoCqCandidates.begin(), m_ft8FullAutoCqCandidates.end(), betterCandidate);
    const Ft8FullAutoCqCandidate best = m_ft8FullAutoCqCandidates.first();

    QStringList preview;
    const int previewCount = qMin(3, m_ft8FullAutoCqCandidates.size());
    for (int i = 0; i < previewCount; ++i) {
        const Ft8FullAutoCqCandidate &c = m_ft8FullAutoCqCandidates.at(i);
        preview << QStringLiteral("%1(%2)").arg(c.call, c.priorityText);
    }
    appendLog(QString("FT Auto QSO priority: %1 CQ candidate(s), selected %2 [%3]. %4")
                  .arg(m_ft8FullAutoCqCandidates.size())
                  .arg(best.call, best.priorityText, preview.join(QStringLiteral(" | "))));

    m_ft8FullAutoCqCandidates.clear();
    startFt8FullAutoQsoFromCandidate(best);
}

bool MainWindow::tryStartFt8FullAutoQso(const Ft8RxDecoder::Decode &decode)
{
    // Evil/Auto QSO is a CQ responder only.  Do not ever start an unattended
    // sequence from ordinary directed traffic, reports, 73/RR73, or stations
    // that are already working somebody else.
    const ParsedFt8Message parsed = parseFt8MessageText(decode.message, stationCallsign());
    if (!parsed.cq) {
        return false;
    }
    return queueFt8FullAutoCqCandidate(decode);
}

bool MainWindow::startFt8FullAutoQsoFromCandidate(const Ft8FullAutoCqCandidate &candidate)
{
    if (!candidate.valid || !canStartFt8FullAutoQsoNow()) {
        return false;
    }

    const QString call = candidate.call.trimmed().toUpper();
    const QString grid = candidate.grid.trimmed().left(4).toUpper();
    if (call.isEmpty() || FtDecodedText::callMatches(call, stationCallsign())) {
        return false;
    }

    if (m_chkFt8AutoLog != nullptr && !m_chkFt8AutoLog->isChecked()) {
        const QSignalBlocker blockAutoLog(m_chkFt8AutoLog);
        m_chkFt8AutoLog->setChecked(true);
        m_settings.ft8AutoLog = true;
    }

    selectFt8OppositePeriodFromDecode(candidate.decode);

    if (m_editFt8DxCall != nullptr) {
        const QSignalBlocker blockDxCall(m_editFt8DxCall);
        m_editFt8DxCall->setText(call);
        m_settings.ft8DxCallsign = call;
    }
    if (m_editFt8DxGrid != nullptr) {
        const QSignalBlocker blockDxGrid(m_editFt8DxGrid);
        m_editFt8DxGrid->setText(grid);
        m_settings.ft8DxGrid = grid;
    }
    if (candidate.decode.frequencyHz >= 100 && candidate.decode.frequencyHz <= 3000) {
        const int correspondentHz = qBound(100, candidate.decode.frequencyHz, 3000);
        if (m_spinFt8TxFreq != nullptr) {
            const QSignalBlocker blockTx(m_spinFt8TxFreq);
            m_spinFt8TxFreq->setValue(qBound(m_spinFt8TxFreq->minimum(), correspondentHz, m_spinFt8TxFreq->maximum()));
            m_settings.ft8TxFrequencyHz = m_spinFt8TxFreq->value();
        } else {
            m_settings.ft8TxFrequencyHz = correspondentHz;
        }
        applyFt8RuntimeFrequencySelection(correspondentHz,
                                          QStringLiteral("AutoQSO selected CQ from %1; TX follows correspondent").arg(call),
                                          false);
        appendLog(QStringLiteral("FT AutoQSO frequency: TX marker moved to correspondent %1 at %2 Hz.")
                      .arg(call)
                      .arg(m_settings.ft8TxFrequencyHz));
    }

    m_ftSession.clearQso();
    m_ftSession.cqRepeatActive = false;
    m_ftSession.resumeCqAfterQso = false;
    m_ftSession.startQso(call, grid, candidate.decode.frequencyHz);
    updateCatRotatorQsoTarget(QStringLiteral("AutoQSO selected CQ from %1").arg(call));
    m_ftSession.state = Ft8SequencerState::SendingLocator;
    m_ftSession.haveLastSnr = true;
    m_ftSession.lastSnrDb = candidate.decode.snrDb;
    m_ftSession.lastSnrMessage = candidate.decode.message.trimmed().toUpper();

    refreshFt8StandardMessages();
    const QString answer = selectFt8TxRow(1);
    if (answer.trimmed().isEmpty()) {
        appendLog("FT FULL AUTO QSO: CQ selected but no Tx2 answer could be generated; check My Call/My Locator.");
        m_ftSession.clearQso();
        updateFt8SequencerUi();
        return false;
    }

    if (!m_rxRunning && !m_txRunning && m_audioEngine != nullptr && !m_audioEngine->isRunning()) {
        startFt8RxShell();
    }

    appendLog(QString("FT FULL AUTO QSO: answering prioritized CQ from %1 %2 at %3 Hz with %4 [%5].")
                  .arg(call, grid.isEmpty() ? QStringLiteral("--") : grid)
                  .arg(candidate.decode.frequencyHz)
                  .arg(answer, candidate.priorityText));
    scheduleFt8SequencerMessage(answer, QStringLiteral("AUTO"));
    return true;
}

void MainWindow::startFt8CqRepeat()
{
    if (m_chkFt8CqRepeat == nullptr || !m_chkFt8CqRepeat->isChecked()) {
        appendLog("FT CQ retry ignored: CQ retry is disabled.");
        return;
    }
    applyFt8Settings();
    if (m_editFt8DxCall != nullptr) {
        m_editFt8DxCall->clear();
    }
    if (m_editFt8DxGrid != nullptr) {
        m_editFt8DxGrid->clear();
    }
    applyFt8Settings();
    refreshFt8StandardMessages();

    const QString cq = selectFt8TxRow(0);
    if (!isFt8CqMessage(cq)) {
        QMessageBox::information(this,
                                 "FT8 CQ repeat",
                                 uiText("ft8_cq_message_missing", "Generate/select a CQ message before starting CQ repeat."));
        return;
    }

    const int repeatCount = (m_spinFt8CqTimeoutMin != nullptr) ? m_spinFt8CqTimeoutMin->value() : m_settings.ft8CqRepeatCount;
    m_ftSession.resetForCqRepeatCount(stationCallsign(), stationLocator(), qBound(1, repeatCount, 99));
    // CQ repeat is only an unanswered-CQ helper.  Once a QSO starts or
    // completes, MM must not silently go back to calling CQ unless the
    // operator explicitly starts CQ repeat again.
    m_ftSession.resumeCqAfterQso = false;

    if (!m_rxRunning && !m_txRunning && m_audioEngine != nullptr && !m_audioEngine->isRunning()) {
        startFt8RxShell();
    }

    appendLog(QString("FT8 CQ retry started: %1 CQ transmission(s).").arg(qBound(1, repeatCount, 99)));
    scheduleFt8SequencerMessage(cq, QStringLiteral("CQ"));
}

void MainWindow::stopFt8Sequencer(const QString &reason)
{
    m_ftSession.resetAll();
    updateCatRotatorQsoTarget(reason);
    if (m_ftSlotScheduler != nullptr) {
        QMetaObject::invokeMethod(m_ftSlotScheduler, "cancelTransmission", Qt::QueuedConnection);
    }
    if (m_pendingFt8PttKeyed && !m_txRunning && !m_ftTxWorkerRunning) {
        unkeyPttAfterTx();
    }
    m_ft8PendingTxArmed = false;
    m_ft8PendingTxToken.clear();
    m_pendingFt8TxMessage.clear();
    m_pendingFt8TxTag.clear();
    m_pendingFt8PreSilenceMs = 0;
    m_pendingFt8PreparedModulator.reset();
    m_pendingFt8TxPlan = FtTxPlan();
    m_pendingFt8PttPrearmed = false;
    m_pendingFt8PttKeyed = false;
    m_ftSession.activeTxRow = -1;
    m_hasDeferredFt8TxPlan = false;
    m_deferredFt8TxMessage.clear();
    m_deferredFt8TxTag.clear();
    m_deferredFt8TxPlan = FtTxPlan();
    m_ftSession.deferredState = Ft8SequencerState::Idle;
    m_ft8FullAutoCqCandidates.clear();
    m_ft8FullAutoCqSelectionTimer.stop();
    if (!reason.trimmed().isEmpty()) {
        appendLog("FT8 sequencer stopped: " + reason.trimmed());
    }
    updateFt8SequencerUi();
}


bool MainWindow::shouldParkFt8LateReply(const Ft8RxDecoder::Decode &decode,
                                       const FtQsoSequencer::ParsedMessage &parsed) const
{
    Q_UNUSED(decode)

    const QString sender = parsed.senderCall.trimmed().toUpper();
    if (sender.isEmpty() || !parsed.addressedToMe) {
        return false;
    }

    const auto cooldownIt = m_ft8AutoNoResponseCooldown.constFind(sender);
    const bool wasNoResponseTarget = cooldownIt != m_ft8AutoNoResponseCooldown.constEnd() &&
                                     cooldownIt.value().isValid() &&
                                     cooldownIt.value() > QDateTime::currentDateTimeUtc();
    if (!wasNoResponseTarget) {
        return false;
    }

    // If we are idle, the normal sequencer path can resume this late answer
    // immediately.  Only park it when acting on it now would steal focus from
    // an active/pending QSO or an already armed FT transmission.
    const bool workingAnotherCall = m_ftSession.qsoActive &&
                                    !m_ftSession.dxCall.trimmed().isEmpty() &&
                                    !FtDecodedText::callMatches(sender, m_ftSession.dxCall);
    const bool ftBusy = m_txRunning || m_ftTxWorkerRunning ||
                        (m_txAudioEngine != nullptr && m_txAudioEngine->isRunning()) ||
                        m_ft8PendingTxArmed || m_hasDeferredFt8TxPlan ||
                        !m_pendingFt8TxMessage.trimmed().isEmpty();
    return workingAnotherCall || ftBusy;
}

void MainWindow::parkFt8LateReply(const Ft8RxDecoder::Decode &decode,
                                  const FtQsoSequencer::ParsedMessage &parsed)
{
    const QString sender = parsed.senderCall.trimmed().toUpper();
    if (sender.isEmpty()) {
        return;
    }

    Ft8ParkedLateReply parked;
    parked.decode = decode;
    parked.senderCall = sender;
    parked.heardUtc = QDateTime::currentDateTimeUtc();

    for (int i = 0; i < m_ft8ParkedLateReplies.size(); ++i) {
        if (FtDecodedText::callMatches(m_ft8ParkedLateReplies.at(i).senderCall, sender)) {
            m_ft8ParkedLateReplies[i] = parked;
            appendLog(QString("FT sequencer: updated parked late reply from %1; current QSO/armed TX is left untouched.")
                          .arg(sender));
            return;
        }
    }

    m_ft8ParkedLateReplies.append(parked);
    while (m_ft8ParkedLateReplies.size() > 8) {
        m_ft8ParkedLateReplies.removeFirst();
    }
    appendLog(QString("FT sequencer: parked late reply from %1; current QSO/armed TX is left untouched.")
                  .arg(sender));
}

void MainWindow::processParkedFt8LateReplies()
{
    if (m_ft8ParkedLateReplies.isEmpty()) {
        return;
    }

    const bool busy = m_ftSession.qsoActive || m_txRunning || m_ftTxWorkerRunning ||
                      (m_txAudioEngine != nullptr && m_txAudioEngine->isRunning()) ||
                      m_ft8PendingTxArmed || m_hasDeferredFt8TxPlan ||
                      !m_pendingFt8TxMessage.trimmed().isEmpty();
    if (busy) {
        return;
    }

    const QDateTime now = QDateTime::currentDateTimeUtc();
    for (int i = m_ft8ParkedLateReplies.size() - 1; i >= 0; --i) {
        if (!m_ft8ParkedLateReplies.at(i).heardUtc.isValid() ||
            m_ft8ParkedLateReplies.at(i).heardUtc.secsTo(now) > 120) {
            appendLog(QString("FT sequencer: expired parked late reply from %1; too old to resume safely.")
                          .arg(m_ft8ParkedLateReplies.at(i).senderCall));
            m_ft8ParkedLateReplies.removeAt(i);
        }
    }
    if (m_ft8ParkedLateReplies.isEmpty()) {
        return;
    }

    // Resume the newest late answer first.  This mirrors WSJT-X-like behaviour:
    // act only once the current exchange is free, and do not pre-empt the QSO
    // that was already in progress when the late answer arrived.
    int bestIndex = 0;
    for (int i = 1; i < m_ft8ParkedLateReplies.size(); ++i) {
        if (m_ft8ParkedLateReplies.at(i).heardUtc > m_ft8ParkedLateReplies.at(bestIndex).heardUtc) {
            bestIndex = i;
        }
    }

    const Ft8ParkedLateReply parked = m_ft8ParkedLateReplies.takeAt(bestIndex);
    m_ft8AutoNoResponseCooldown.remove(parked.senderCall);
    appendLog(QString("FT sequencer: resuming parked late reply from %1 after current activity finished.")
                  .arg(parked.senderCall));
    processFt8SequencerDecode(parked.decode);
}

void MainWindow::processFt8SequencerDecode(const Ft8RxDecoder::Decode &decode)
{
    const QString myCall = stationCallsign();
    if (myCall.isEmpty()) {
        return;
    }

    const bool autoSeq = true;

    FtQsoSequencer::Decode seqDecode;
    seqDecode.message = decode.message;
    seqDecode.snrDb = decode.snrDb;
    seqDecode.frequencyHz = decode.frequencyHz;
    seqDecode.slotStartUtcMs = decode.slotStartUtcMs;
    seqDecode.slotPeriodMs = decode.slotPeriodMs;

    FtQsoSequencer::Context seqContext = m_ftSession.makeContext(
        myCall,
        (m_spinFt8RxFreq != nullptr) ? m_spinFt8RxFreq->value() : m_settings.ft8RxFrequencyHz,
        (m_spinFt8TxFreq != nullptr) ? m_spinFt8TxFreq->value() : m_settings.ft8TxFrequencyHz);
    seqContext.startToleranceHz = 25;
    seqContext.stopToleranceHz = 50;

    const FtQsoSequencer::Decision decision = FtQsoSequencer::evaluateDecode(seqDecode, seqContext);
    const FtQsoSequencer::ParsedMessage &parsed = decision.parsed;

    const bool sameAsRecentlyCompleted = !m_lastCompletedFt8Call.isEmpty() &&
        !parsed.senderCall.isEmpty() &&
        FtDecodedText::callMatches(parsed.senderCall, m_lastCompletedFt8Call) &&
        m_lastCompletedFt8Utc.isValid() &&
        qAbs(m_lastCompletedFt8Utc.secsTo(QDateTime::currentDateTimeUtc())) <= 120;
    if (!m_ftSession.qsoActive && sameAsRecentlyCompleted &&
        (parsed.final73 || parsed.rr73 || parsed.rrr || parsed.addressedToMe)) {
        appendLog(QString("FT sequencer: ignoring late/repeated post-QSO decode from %1; QSO was already completed.")
                      .arg(parsed.senderCall));
        return;
    }

    if (decision.action == FtQsoSequencer::Action::StopTx) {
        if (!decision.logLine.isEmpty()) {
            appendLog(decision.logLine);
        }
        if (reclaimFt8ActiveQsoAwayFromQrm(decode, parsed, decision.logLine)) {
            return;
        }
        if (m_ftSlotScheduler != nullptr) {
            QMetaObject::invokeMethod(m_ftSlotScheduler, "cancelTransmission", Qt::QueuedConnection);
        }
        m_ft8PendingTxArmed = false;
        m_ft8PendingTxToken.clear();
        m_pendingFt8TxMessage.clear();
        m_pendingFt8TxTag.clear();
        m_pendingFt8PreparedModulator.reset();
        m_pendingFt8TxPlan = FtTxPlan();
        if (m_txRunning || m_ftTxWorkerRunning || (m_txAudioEngine != nullptr && m_txAudioEngine->isRunning())) {
            stopImageTx();
        }
        updateFt8SequencerUi();
        return;
    }

    if (decision.action == FtQsoSequencer::Action::Ignore ||
        !parsed.addressedToMe || parsed.senderCall.isEmpty() || FtDecodedText::callMatches(parsed.senderCall, myCall)) {
        return;
    }

    if (shouldParkFt8LateReply(decode, parsed)) {
        parkFt8LateReply(decode, parsed);
        return;
    }

    if (m_ftSession.qsoActive && !m_ftSession.dxCall.isEmpty() && !FtDecodedText::callMatches(parsed.senderCall, m_ftSession.dxCall)) {
        // WSJT-X style: while a QSO is active, ignore third-party callers for the
        // auto-sequencer.  They still remain visible in the decode/history table.
        return;
    }

    const QString parsedSenderKey = parsed.senderCall.trimmed().toUpper();
    if (!parsedSenderKey.isEmpty() && m_ft8AutoNoResponseCooldown.contains(parsedSenderKey)) {
        m_ft8AutoNoResponseCooldown.remove(parsedSenderKey);
        appendLog(QString("FT sequencer: late reply from %1 received; no-response cooldown cleared and QSO resumed.")
                      .arg(parsedSenderKey));
    }

    if (m_editFt8DxCall != nullptr) {
        const QSignalBlocker blockDxCall(m_editFt8DxCall);
        m_editFt8DxCall->setText(parsed.senderCall);
        m_settings.ft8DxCallsign = parsed.senderCall;
    }
    const bool safeDecodedGrid = !parsed.grid.isEmpty() &&
                                 !parsed.final73 && !parsed.rr73 && !parsed.rrr &&
                                 parsed.report.isEmpty() &&
                                 !isFt8AckLikeGridTrap(parsed.grid);
    if (safeDecodedGrid && m_editFt8DxGrid != nullptr) {
        const QSignalBlocker blockDxGrid(m_editFt8DxGrid);
        m_editFt8DxGrid->setText(parsed.grid.left(4));
        m_settings.ft8DxGrid = parsed.grid.left(4);
    }
    if (decode.frequencyHz >= 100 && decode.frequencyHz <= 3000) {
        // Runtime-only marker tracking: RX focus follows the active target and
        // TX follows the selected TX-frequency strategy without rewriting the
        // full settings profile on every decode.
        applyFt8RuntimeFrequencySelection(decode.frequencyHz,
                                          QStringLiteral("sequencer target %1").arg(parsed.senderCall),
                                          true);
    }

    m_ftSession.haveLastSnr = true;
    m_ftSession.lastSnrDb = decode.snrDb;
    m_ftSession.lastSnrMessage = decode.message.trimmed().toUpper();
    updateFt8SignalReportUi();

    // No applyFt8Settings() here: it writes persistent settings and rebuilds UI.
    // Decode-driven QSO advancement must remain a lightweight runtime path.

    if (!autoSeq) {
        refreshFt8StandardMessages();
        updateFt8SequencerUi();
        return;
    }

    const bool wasQsoActive = m_ftSession.qsoActive;
    if (decision.startsQso && !wasQsoActive) {
        // A decoded reply has turned an unanswered CQ into a real QSO.
        // Stop CQ-repeat state here; after the QSO MM must remain in RX and
        // wait for an explicit operator action, not restart CQ automatically.
        m_ftSession.cqRepeatActive = false;
        m_ftSession.resumeCqAfterQso = false;
    }
    m_ftSession.applyDecision(decision);
    if (!decision.startsQso && m_ftSession.dxCall.isEmpty()) {
        m_ftSession.startQso(parsed.senderCall, parsed.grid, decision.audioFreqHz);
    }
    if (m_ftSession.qsoActive || decision.startsQso) {
        updateCatRotatorQsoTarget(QStringLiteral("FT sequencer target %1").arg(m_ftSession.dxCall));
    }
    if (!decision.logLine.isEmpty()) {
        appendLog(decision.logLine);
    }

    if (decision.refreshStandardMessages) {
        refreshFt8StandardMessages();
    } else {
        refreshFt8StandardMessages();
    }

    const auto armRow = [this](int row, const QString &tag, Ft8SequencerState nextState) {
        const QString msg = selectFt8TxRow(row);
        if (msg.trimmed().isEmpty()) {
            return;
        }
        if (m_tableFt8TxMessages != nullptr && m_tableFt8TxMessages->item(row, 0) != nullptr) {
            m_tableFt8TxMessages->scrollToItem(m_tableFt8TxMessages->item(row, 0), QAbstractItemView::PositionAtCenter);
        }
        m_ftSession.state = nextState;
        scheduleFt8SequencerMessage(msg, tag);
    };

    switch (decision.action) {
    case FtQsoSequencer::Action::ArmRow:
        armRow(decision.txRow, decision.tag, decision.nextState);
        return;
    case FtQsoSequencer::Action::RetryCurrent:
        m_ftSession.state = decision.nextState;
        scheduleFt8RetryIfNeeded();
        updateFt8SequencerUi();
        return;
    case FtQsoSequencer::Action::Complete:
        m_ftSession.state = decision.nextState;
        completeFt8Qso(decision.completeReason);
        return;
    case FtQsoSequencer::Action::StopTx:
        if (m_ftSlotScheduler != nullptr) {
            QMetaObject::invokeMethod(m_ftSlotScheduler, "cancelTransmission", Qt::QueuedConnection);
        }
        m_ft8PendingTxArmed = false;
        m_ft8PendingTxToken.clear();
        m_pendingFt8TxMessage.clear();
        m_pendingFt8TxTag.clear();
        m_pendingFt8PreparedModulator.reset();
        m_pendingFt8TxPlan = FtTxPlan();
        if (m_txRunning || m_ftTxWorkerRunning || (m_txAudioEngine != nullptr && m_txAudioEngine->isRunning())) {
            stopImageTx();
        }
        updateFt8SequencerUi();
        return;
    case FtQsoSequencer::Action::None:
    case FtQsoSequencer::Action::Ignore:
        break;
    }

    updateFt8SequencerUi();
}

void MainWindow::handleFt8TxCompleted()
{
    if (m_ftSession.lastTxWasTune) {
        return;
    }

    const QString completedTag = m_ftSession.lastTxTag.trimmed().toUpper();
    const QDateTime now = QDateTime::currentDateTimeUtc();

    if (completedTag == QStringLiteral("CQ") && m_ftSession.cqRepeatActive && !m_ftSession.qsoActive) {
        if (m_ftSession.cqRepeatRemaining > 0) {
            --m_ftSession.cqRepeatRemaining;
        }
        if (m_ftSession.cqRepeatRemaining <= 0 ||
            (m_ftSession.cqRepeatDeadlineUtc.isValid() && now >= m_ftSession.cqRepeatDeadlineUtc)) {
            m_ftSession.cqRepeatActive = false;
            m_ftSession.state = Ft8SequencerState::Idle;
            appendLog("FT8 CQ retry count reached; staying in RX.");
            updateFt8SequencerUi();
            return;
        }
        m_ftSession.state = Ft8SequencerState::CallingCq;
        scheduleFt8SequencerMessage(selectFt8TxRow(0), QStringLiteral("CQ"));
        return;
    }

    if (!m_ftSession.qsoActive) {
        updateFt8SequencerUi();
        return;
    }

    const FtQsoSequencer::TxCompleteDecision decision = FtQsoSequencer::onTxCompleted(m_ftSession.state);
    m_ftSession.state = decision.nextState;

    if (!decision.logLine.isEmpty()) {
        appendLog(decision.logLine);
    }

    switch (decision.action) {
    case FtQsoSequencer::Action::RetryCurrent:
        scheduleFt8RetryIfNeeded();
        break;
    case FtQsoSequencer::Action::Complete:
        completeFt8Qso(decision.completeReason);
        return;
    case FtQsoSequencer::Action::None:
    case FtQsoSequencer::Action::Ignore:
    case FtQsoSequencer::Action::ArmRow:
    case FtQsoSequencer::Action::StopTx:
        break;
    }
    updateFt8SequencerUi();
}

void MainWindow::completeFt8Qso(const QString &reason)
{
    if (!m_ftSession.qsoActive && m_ftSession.dxCall.isEmpty()) {
        return;
    }

    m_ftSession.state = Ft8SequencerState::Completed;
    m_ftSession.retryMessage.clear();
    m_ftSession.retryTag.clear();
    m_ftSession.retryRemaining = 0;
    if (m_ftSlotScheduler != nullptr) {
        QMetaObject::invokeMethod(m_ftSlotScheduler, "cancelTransmission", Qt::QueuedConnection);
    }
    if (m_pendingFt8PttKeyed && !m_txRunning && !m_ftTxWorkerRunning) {
        unkeyPttAfterTx();
    }
    m_ft8PendingTxArmed = false;
    m_ft8PendingTxToken.clear();
    m_pendingFt8TxMessage.clear();
    m_pendingFt8TxTag.clear();
    m_pendingFt8PreSilenceMs = 0;
    m_pendingFt8PreparedModulator.reset();
    m_pendingFt8TxPlan = FtTxPlan();
    m_pendingFt8PttPrearmed = false;
    m_pendingFt8PttKeyed = false;
    m_ftSession.activeTxRow = -1;
    const QString completedCall = m_ftSession.dxCall.trimmed().toUpper();
    appendLog(currentAdifMode() + " QSO completed: " + (completedCall.isEmpty() ? QStringLiteral("--") : completedCall) + " (" + reason + ")");
    autoLogFt8Qso(reason);

    m_lastCompletedFt8Call = completedCall;
    m_lastCompletedFt8Utc = QDateTime::currentDateTimeUtc();
    m_lastCompletedFt8Reason = reason;

    m_ftSession.qsoActive = false;
    updateCatRotatorQsoTarget(QStringLiteral("QSO completed: %1").arg(reason));
    m_ftSession.cqRepeatActive = false;
    m_ftSession.resumeCqAfterQso = false;
    m_hasDeferredFt8TxPlan = false;
    m_deferredFt8TxMessage.clear();
    m_deferredFt8TxTag.clear();
    m_deferredFt8TxPlan = FtTxPlan();
    m_ftSession.deferredState = Ft8SequencerState::Idle;
    const bool evilAutoQso = m_ft8EvilModeUnlocked && m_chkFt8FullAutoQso != nullptr && m_chkFt8FullAutoQso->isChecked();
    if (evilAutoQso) {
        appendLog(currentAdifMode() + " QSO complete: Evil Auto QSO remains armed and will answer the next decoded CQ subject to blacklist/duplicate policy.");
    } else {
        appendLog(currentAdifMode() + " QSO complete: CQ auto-resume disabled; staying in RX.");
    }
    updateFt8SequencerUi();
    processParkedFt8LateReplies();
}

void MainWindow::autoLogFt8Qso(const QString &reason)
{
    const bool autoLog = (m_chkFt8AutoLog != nullptr) ? m_chkFt8AutoLog->isChecked() : m_settings.ft8AutoLog;
    if (!autoLog || m_ftSession.autoLogDone || m_ftSession.dxCall.isEmpty()) {
        return;
    }

    LogbookEntry entry;
    entry.callsign = AdifLogbook::normalizeCallsign(m_ftSession.dxCall);
    entry.rstSent = m_ftSession.reportSent.isEmpty() ? QStringLiteral("-10") : cleanFt8Report(m_ftSession.reportSent);
    entry.rstReceived = m_ftSession.reportReceived.isEmpty() ? QStringLiteral("-10") : cleanFt8Report(m_ftSession.reportReceived);
    entry.band = (m_cmbFt8Band != nullptr) ? m_cmbFt8Band->currentText().trimmed().toLower() : m_settings.ft8Band.toLower();
    entry.mode = Ft8Mode::profileForMode(ui->cmbMode != nullptr ? ui->cmbMode->currentText() : QStringLiteral("FT8")).adifMode;
    QString qsoGrid = m_ftSession.dxGrid.trimmed().toUpper();
    if (qsoGrid.isEmpty() || !FtDecodedText::isGrid(qsoGrid.left(4))) {
        qsoGrid = m_ftSession.knownGridFor(m_ftSession.dxCall).trimmed().toUpper();
    }
    qsoGrid = CtyCountryFile::instance().refinedGridForCallsign(m_ftSession.dxCall, qsoGrid, 6);
    entry.grid = FtDecodedText::isGrid(qsoGrid.left(4)) ? qsoGrid.left(qsoGrid.size() >= 6 ? 6 : 4) : QString();
    const CtyCountryFile::LookupResult qsoCty = CtyCountryFile::instance().lookupCallsign(m_ftSession.dxCall);
    if (qsoCty.valid) {
        entry.country = qsoCty.entity.name;
        entry.adifFields.insert(QStringLiteral("DXCC"), qsoCty.entity.dxcc);
        entry.adifFields.insert(QStringLiteral("CTY_NAME"), qsoCty.entity.name);
        entry.adifFields.insert(QStringLiteral("CTY_PREFIX"), qsoCty.entity.primaryPrefix);
        entry.adifFields.insert(QStringLiteral("CTY_CONT"), qsoCty.entity.continent);
        entry.adifFields.insert(QStringLiteral("CTY_GRID"), qsoCty.entity.referenceGrid);
        entry.adifFields.insert(QStringLiteral("CTY_LAT"), QString::number(qsoCty.entity.latitude, 'f', 4));
        entry.adifFields.insert(QStringLiteral("CTY_LON"), QString::number(qsoCty.entity.longitude, 'f', 4));
    }
    entry.utc = m_ftSession.qsoStartUtc.isValid() ? m_ftSession.qsoStartUtc : QDateTime::currentDateTimeUtc();
    entry.utcEnd = QDateTime::currentDateTimeUtc();
    entry.comment = QString("%1 auto-sequence: %2; RX audio %3 Hz; TX audio %4 Hz; final %5")
                        .arg(entry.mode,
                             reason,
                             QString::number(m_ftSession.audioFreqHz > 0 ? m_ftSession.audioFreqHz : (m_spinFt8RxFreq != nullptr ? m_spinFt8RxFreq->value() : 0)),
                             QString::number(m_spinFt8TxFreq != nullptr ? m_spinFt8TxFreq->value() : m_settings.ft8TxFrequencyHz),
                             m_ftSession.lastTxMessage.isEmpty() ? QStringLiteral("--") : m_ftSession.lastTxMessage);

    if (entry.callsign.isEmpty()) {
        return;
    }

    // Guard against duplicate auto-log paths.  A completed FT QSO can be
    // noticed both by the decode-driven sequencer and by the TX-finished path
    // around the same slot boundary.  WSJT-X avoids double logging with a
    // single QSO lifecycle; keep the same practical behaviour here.
    const QVector<LogbookEntry> existingRecords = m_logbook.records();
    for (const LogbookEntry &existing : existingRecords) {
        if (AdifLogbook::normalizeCallsign(existing.callsign) != entry.callsign) {
            continue;
        }
        if (existing.band.trimmed().compare(entry.band, Qt::CaseInsensitive) != 0 ||
            existing.mode.trimmed().compare(entry.mode, Qt::CaseInsensitive) != 0) {
            continue;
        }
        const qint64 dt = qAbs(existing.utc.toUTC().secsTo(entry.utc.toUTC()));
        if (dt <= 10 * 60) {
            m_ftSession.autoLogDone = true;
            appendLog(QString("FT8 auto-log skipped duplicate QSO: %1 %2 %3 already in logbook within 10 minutes.")
                          .arg(entry.callsign, entry.band, entry.mode));
            return;
        }
    }

    const bool wasKnown = m_logbook.containsCallsign(entry.callsign);
    QString error;
    if (!m_logbook.append(entry, &error)) {
        appendLog("FT8 auto-log failed: " + error);
        return;
    }

    m_ftSession.autoLogDone = true;
    refreshLogbookHighlights();
    refreshQsoMaps();
    appendLog(QString("FT8 auto-logged QSO: %1 %2/%3 %4%5")
                  .arg(entry.callsign,
                       entry.rstSent,
                       entry.rstReceived,
                       entry.band,
                       wasKnown ? QStringLiteral(" (worked before)") : QString()));
}



void MainWindow::handleFt8QsoHistoryDoubleClicked(QTableWidgetItem *item)
{
    if (item == nullptr || m_tableFt8QsoHistory == nullptr) {
        return;
    }
    const int row = item->row();
    QTableWidgetItem *utcItem = m_tableFt8QsoHistory->item(row, 0);
    QTableWidgetItem *dirItem = m_tableFt8QsoHistory->item(row, 1);
    QTableWidgetItem *dbItem = m_tableFt8QsoHistory->item(row, 2);
    QTableWidgetItem *dtItem = m_tableFt8QsoHistory->item(row, 3);
    QTableWidgetItem *freqItem = m_tableFt8QsoHistory->item(row, 4);
    QTableWidgetItem *msgItem = m_tableFt8QsoHistory->item(row, 5);
    if (msgItem == nullptr) {
        return;
    }
    const QString message = msgItem->text().trimmed().toUpper();
    if (message.isEmpty() || message.startsWith(QChar(0x2500))) {
        return;
    }

    const QString direction = dirItem != nullptr ? dirItem->text().trimmed().toUpper() : QString();
    const QString utc = utcItem != nullptr ? utcItem->text().trimmed() : QString();
    const QString freq = freqItem != nullptr ? freqItem->text().trimmed() : QString();

    // Prefer the normal RX-table activation path when the history row mirrors a
    // visible received decode.  This keeps CQ/report/RR73 row selection identical
    // to double-clicking the left decode table and avoids maintaining two parsers.
    if (direction != QStringLiteral("TX") && m_tableFt8Rx != nullptr) {
        for (int r = m_tableFt8Rx->rowCount() - 1; r >= 0; --r) {
            QTableWidgetItem *rxUtc = m_tableFt8Rx->item(r, 0);
            QTableWidgetItem *rxFreq = m_tableFt8Rx->item(r, 3);
            QTableWidgetItem *rxMsg = m_tableFt8Rx->item(r, 4);
            if (rxMsg == nullptr || rxMsg->text().trimmed().toUpper() != message) {
                continue;
            }
            const bool utcMatches = utc.isEmpty() || (rxUtc != nullptr && rxUtc->text().trimmed() == utc);
            const bool freqMatches = freq.isEmpty() || (rxFreq != nullptr && rxFreq->text().trimmed() == freq);
            if (utcMatches && freqMatches) {
                m_tableFt8Rx->selectRow(r);
                handleFt8DecodeDoubleClicked(rxMsg);
                return;
            }
        }
    }

    // A TX history row is safe to repeat only when it is a normal FT message
    // involving the operator callsign.  Do not arm arbitrary text from history.
    const QString myCall = stationCallsign();
    const ParsedFt8Message parsed = parseFt8MessageText(message, myCall);
    const bool containsMe = !myCall.trimmed().isEmpty() && message.contains(myCall.trimmed().toUpper());
    const bool plausibleFtMessage = containsMe && (parsed.parts.size() >= 2 || !parsed.senderCall.isEmpty());
    if (!plausibleFtMessage) {
        QMessageBox::information(this,
                                 uiText("ft_history_not_transmittable_title", "FT QSO activity"),
                                 uiText("ft_history_not_transmittable", "This history row is not a valid transmittable FT message for this station."));
        return;
    }

    bool freqOk = false;
    const int audioHz = freq.toInt(&freqOk);
    if (freqOk && audioHz >= 100 && audioHz <= 3000) {
        applyFt8RuntimeFrequencySelection(audioHz,
                                          QStringLiteral("QSO activity table selection"),
                                          true);
    }

    QString call;
    QString grid;
    const QStringList parts = message.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    if (parts.size() >= 2) {
        if (FtDecodedText::callMatches(parts.value(0), myCall)) {
            call = parts.value(1).trimmed().toUpper();
        } else if (FtDecodedText::callMatches(parts.value(1), myCall)) {
            call = parts.value(0).trimmed().toUpper();
        }
    }
    if (parts.size() >= 3 && QRegularExpression(QStringLiteral("^[A-R]{2}[0-9]{2}([A-X]{2})?$"), QRegularExpression::CaseInsensitiveOption).match(parts.value(2)).hasMatch()) {
        grid = parts.value(2).left(4).toUpper();
    }
    if (!call.isEmpty() && m_editFt8DxCall != nullptr) {
        m_editFt8DxCall->setText(call);
    }
    if (!grid.isEmpty() && m_editFt8DxGrid != nullptr) {
        m_editFt8DxGrid->setText(grid);
    }
    applyFt8Settings();

    if (!call.isEmpty()) {
        m_ftSession.cqRepeatActive = false;
        m_ftSession.qsoActive = true;
        m_ftSession.resumeCqAfterQso = false;
        m_ftSession.autoLogDone = false;
        if (m_ftSession.qsoStartUtc.isNull()) {
            m_ftSession.qsoStartUtc = QDateTime::currentDateTimeUtc();
        }
        m_ftSession.dxCall = call;
        if (!grid.isEmpty()) m_ftSession.dxGrid = grid;
        m_ftSession.audioFreqHz = (m_spinFt8RxFreq != nullptr) ? m_spinFt8RxFreq->value() : audioHz;
        m_ftSession.retryMessage = message;
        m_ftSession.state = Ft8SequencerState::SendingLocator;
        updateCatRotatorQsoTarget(QStringLiteral("FT history target %1").arg(call));
    }

    appendLog(QStringLiteral("FT QSO activity selected for TX: %1").arg(message));
    if (!m_rxRunning && !m_txRunning && m_audioEngine != nullptr && !m_audioEngine->isRunning()) {
        startFt8RxShell();
    }
    scheduleFt8SequencerMessage(message, direction == QStringLiteral("TX") ? QStringLiteral("RETRY") : QStringLiteral("SEQ"));
}

void MainWindow::handleFt8DecodeDoubleClicked(QTableWidgetItem *item)
{
    if (item == nullptr || m_tableFt8Rx == nullptr) {
        return;
    }

    const int row = item->row();
    QTableWidgetItem *messageItem = m_tableFt8Rx->item(row, 4);
    if (messageItem == nullptr) {
        return;
    }

    const QString message = messageItem->text().trimmed().toUpper();
    const QString myCall = stationCallsign();
    const ParsedFt8Message parsed = parseFt8MessageText(message, myCall);

    QTableWidgetItem *snrItem = m_tableFt8Rx->item(row, 1);
    bool snrOk = false;
    const int selectedSnr = snrItem != nullptr ? snrItem->text().trimmed().toInt(&snrOk) : 0;
    if (snrOk) {
        m_ftSession.haveLastSnr = true;
        m_ftSession.lastSnrDb = selectedSnr;
        m_ftSession.lastSnrMessage = message;
        updateFt8SignalReportUi();
    }

    // When answering a received FT4/FT8 decode, transmit in the opposite UTC
    // period from the correspondent.  FT8 uses 15 s slots within a 30 s cycle;
    // FT4 uses 7.5 s slots within a 15 s cycle.  The decode table UTC column is
    // HHmmss, so this is exact for FT8 and conservative around FT4 half-second
    // boundaries.
    QString txPeriodNote;
    if (m_radioFt8TxFirst != nullptr && m_radioFt8TxSecond != nullptr) {
        QTableWidgetItem *utcItem = m_tableFt8Rx->item(row, 0);
        const QString utcText = utcItem != nullptr ? utcItem->text().trimmed() : QString();
        QTime decodedTime;
        if (utcText.size() >= 6) {
            decodedTime = QTime::fromString(utcText.left(6), QStringLiteral("HHmmss"));
        }
        if (decodedTime.isValid()) {
            const QString modeName = (ui != nullptr && ui->cmbMode != nullptr) ? ui->cmbMode->currentText() : QStringLiteral("FT8");
            const Ft8Mode::Profile profile = Ft8Mode::profileForMode(modeName);
            const int slotMs = qMax(1000, profile.slotMs);
            const int cycleMs = qMax(slotMs * 2, profile.cycleMs);
            const int msOfDay = (((decodedTime.hour() * 60 + decodedTime.minute()) * 60) + decodedTime.second()) * 1000 + decodedTime.msec();
            const int cyclePosMs = msOfDay % cycleMs;
            const bool decodedWasFirstPeriod = cyclePosMs < slotMs;
            const QSignalBlocker blockFirst(m_radioFt8TxFirst);
            const QSignalBlocker blockSecond(m_radioFt8TxSecond);
            m_radioFt8TxFirst->setChecked(!decodedWasFirstPeriod);
            m_radioFt8TxSecond->setChecked(decodedWasFirstPeriod);
            m_settings.ft8TxFirstPeriod = m_radioFt8TxFirst->isChecked();
            txPeriodNote = decodedWasFirstPeriod
                ? QStringLiteral("; correspondent first/even period -> TX second/odd")
                : QStringLiteral("; correspondent second/odd period -> TX first/even");
            updateFt8SlotStatus();
            updateFt8SequencerUi();
            savePersistentSettings();
        }
    }

    QString call;
    QString grid = parsed.grid.left(4);
    int preferredTxRow = 0; // default: own CQ

    if (parsed.cq) {
        call = ft8CqCallsignFromMessage(parsed);
        preferredTxRow = 1; // DX MY GRID: normal answer to CQ
    } else if (!parsed.senderCall.isEmpty()) {
        call = parsed.senderCall;
        if (!parsed.addressedToMe) {
            // A double-click on a third-party exchange, especially a worked-before
            // / strikethrough line, must not start our QSO at Tx3/Tx4 just because
            // that other QSO contains a report.  Start a fresh call instead.
            preferredTxRow = 1; // DX MY GRID
        } else if (parsed.rr73 || parsed.final73) {
            preferredTxRow = 5; // DX MY 73
        } else if (parsed.rrr || parsed.contestAck || (parsed.report.startsWith('R') && !parsed.report.isEmpty())) {
            preferredTxRow = 4; // DX MY RR73
        } else if (!parsed.report.isEmpty()) {
            preferredTxRow = 3; // DX MY R-report
        } else {
            preferredTxRow = 2; // DX MY report / first contest-exchange reply
        }
    } else {
        // Generic third-party line: choose the first real callsign that is not ours.
        for (const QString &part : parsed.parts) {
            if (FtDecodedText::callMatches(part, myCall)) {
                continue;
            }
            if (isFt8CallsignToken(part)) {
                call = part;
                break;
            }
        }
        preferredTxRow = 2;
    }

    const QStringList rawParts = message.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    const bool rawIsMyRReportToDx = rawParts.size() >= 3 &&
                                    FtDecodedText::callMatches(rawParts.at(0), myCall) &&
                                    !FtDecodedText::callMatches(rawParts.at(1), myCall) &&
                                    QRegularExpression(QStringLiteral("^R[+-]\\d{2}$")).match(rawParts.at(2)).hasMatch();
    if (rawIsMyRReportToDx) {
        // The DX has acknowledged our report with R-xx.  Reply with RR73/73
        // (Tx5/Tx6 family), never with another R-report.  This protects manual
        // reselection and compound-call cases even if the generic parser returns
        // the report token in a reduced form.
        call = rawParts.at(1).trimmed().toUpper();
        preferredTxRow = 4; // DX MY RR73
    }

    if (!call.isEmpty()) {
        const QString previousDx = m_ftSession.dxCall.trimmed().toUpper();
        const bool changingCorrespondent = !previousDx.isEmpty() &&
                                          !FtDecodedText::callMatches(call, previousDx);
        const bool hadArmedOrRetry = m_ft8PendingTxArmed || m_hasDeferredFt8TxPlan ||
                                     !m_ftSession.retryMessage.trimmed().isEmpty();
        if (changingCorrespondent) {
            appendLog(QString("FT sequencer: correspondent changed %1 -> %2; clearing old QSO/retry/TX plan.")
                          .arg(previousDx, call));
            m_ftSession.clearQso();
            m_ftSession.cqRepeatActive = false;
            m_ftSession.resumeCqAfterQso = false;
            m_ftSession.deferredState = Ft8SequencerState::Idle;
            m_hasDeferredFt8TxPlan = false;
            m_deferredFt8TxMessage.clear();
            m_deferredFt8TxTag.clear();
            m_deferredFt8TxPlan = FtTxPlan();
            if (!m_txRunning && !m_ftTxWorkerRunning &&
                !(m_txAudioEngine != nullptr && m_txAudioEngine->isRunning())) {
                if (m_ftSlotScheduler != nullptr) {
                    QMetaObject::invokeMethod(m_ftSlotScheduler, "cancelTransmission", Qt::QueuedConnection);
                }
                if (m_pendingFt8PttKeyed) {
                    unkeyPttAfterTx();
                }
                m_ft8PendingTxArmed = false;
                m_ft8PendingTxToken.clear();
                m_pendingFt8TxMessage.clear();
                m_pendingFt8TxTag.clear();
                m_pendingFt8Tune = false;
                m_pendingFt8PreSilenceMs = 0;
                m_pendingFt8SlotBoundaryUtcMs = 0;
                m_pendingFt8AudioTargetDelayMs = 0;
                m_pendingFt8PttLeadMs = 0;
                m_pendingFt8PreparedModulator.reset();
                m_pendingFt8TxPlan = FtTxPlan();
                m_pendingFt8PttPrearmed = false;
                m_pendingFt8PttKeyed = false;
            }
        } else if (!m_ftSession.qsoActive && hadArmedOrRetry) {
            // Manual reselection of the same visible correspondent after STOP or an
            // aborted sequence must not inherit a stale retry/deferred plan.
            m_ftSession.clearRetry();
            m_hasDeferredFt8TxPlan = false;
            m_deferredFt8TxMessage.clear();
            m_deferredFt8TxTag.clear();
            m_deferredFt8TxPlan = FtTxPlan();
            m_ftSession.deferredState = Ft8SequencerState::Idle;
        }
    }

    if (!call.isEmpty() && m_editFt8DxCall != nullptr) {
        m_editFt8DxCall->setText(call);
    }
    if (!grid.isEmpty() && m_editFt8DxGrid != nullptr) {
        m_editFt8DxGrid->setText(grid);
    }

    // WSJT-X-like frequency handling: selecting/double-clicking a decode moves
    // RX to the correspondent and, by default, TX follows it.  Hold TX
    // Frequency is the explicit exception for split/clear-slot operation.
    QTableWidgetItem *freqItem = m_tableFt8Rx->item(row, 3);
    bool freqOk = false;
    const int decodeAudioHz = freqItem != nullptr ? freqItem->text().trimmed().toInt(&freqOk) : 0;
    if (freqOk && decodeAudioHz >= 100 && decodeAudioHz <= 3000) {
        applyFt8RuntimeFrequencySelection(decodeAudioHz,
                                          QStringLiteral("manual decode selection"),
                                          true);
    }

    applyFt8Settings();

    QString autoTxMessage;
    if (m_tableFt8TxMessages != nullptr) {
        preferredTxRow = qBound(0, preferredTxRow, m_tableFt8TxMessages->rowCount() - 1);
        autoTxMessage = selectFt8TxRow(preferredTxRow);

        const bool selectedShouldBeFinalAck = rawIsMyRReportToDx ||
                                              (parsed.addressedToMe &&
                                               (parsed.rrr || parsed.contestAck ||
                                                (!parsed.report.isEmpty() && parsed.report.trimmed().toUpper().startsWith('R'))));
        if (selectedShouldBeFinalAck &&
            QRegularExpression(QStringLiteral("\\sR[+-]\\d{2}$")).match(autoTxMessage.trimmed().toUpper()).hasMatch()) {
            const int rr73Row = qMin(4, m_tableFt8TxMessages->rowCount() - 1);
            const QString rr73Message = selectFt8TxRow(rr73Row);
            if (!rr73Message.trimmed().isEmpty() && !QRegularExpression(QStringLiteral("\\sR[+-]\\d{2}$")).match(rr73Message.trimmed().toUpper()).hasMatch()) {
                preferredTxRow = rr73Row;
                autoTxMessage = rr73Message;
                appendLog("FT sequencer safety: R-report reply corrected to RR73/73 row.");
            }
        }

        m_tableFt8TxMessages->scrollToItem(m_tableFt8TxMessages->item(preferredTxRow, 0), QAbstractItemView::PositionAtCenter);
    }

    appendLog(QString("%1 correspondent selected: %2 %3; TX%4 queued.")
                  .arg(Ft8Mode::profileForMode(ui->cmbMode->currentText()).shortLabel,
                       call.isEmpty() ? QString("--") : call,
                       grid.isEmpty() ? QString("--") : grid)
                  .arg(preferredTxRow + 1) + txPeriodNote);

    if (!autoTxMessage.trimmed().isEmpty() && !call.isEmpty()) {
        m_ftSession.cqRepeatActive = false;
        m_ftSession.qsoActive = true;
        m_ftSession.resumeCqAfterQso = false;
        m_ftSession.autoLogDone = false;
        m_ftSession.qsoStartUtc = QDateTime::currentDateTimeUtc();
        m_ftSession.dxCall = call;
        m_ftSession.dxGrid = grid;
        m_ftSession.audioFreqHz = (m_spinFt8RxFreq != nullptr) ? m_spinFt8RxFreq->value() : 0;
        if (preferredTxRow == 0) {
            m_ftSession.state = Ft8SequencerState::CallingCq;
        } else if (preferredTxRow == 1) {
            m_ftSession.state = Ft8SequencerState::SendingLocator;
        } else if (preferredTxRow == 2) {
            m_ftSession.state = Ft8SequencerState::SendingReport;
        } else if (preferredTxRow == 3) {
            m_ftSession.state = Ft8SequencerState::SendingRReport;
        } else {
            m_ftSession.state = Ft8SequencerState::SendingRr73;
        }
        updateCatRotatorQsoTarget(QStringLiteral("FT sequencer target %1").arg(call));
        if (!m_rxRunning && !m_txRunning && m_audioEngine != nullptr && !m_audioEngine->isRunning()) {
            startFt8RxShell();
        }
        scheduleFt8SequencerMessage(autoTxMessage, QStringLiteral("SEQ"));
    }
}

void MainWindow::clearFt8DecodeList()
{
    if (m_tableFt8Rx != nullptr) {
        m_tableFt8Rx->setRowCount(0);
    }
    if (m_tableFt8QsoHistory != nullptr) {
        m_tableFt8QsoHistory->setRowCount(0);
    }
    m_lastFt8RxTableUtc.clear();
    m_ft8RecentLiveDecodeMs.clear();
    m_ft8RecentLiveDecodeSlots.clear();
    m_ft8WaterfallCallouts.clear();
    updateFt8WaterfallOverlays();
    appendLog(uiText("ft_decodes_cleared", "FT decode list cleared."));
}

QString MainWindow::selectedFt8TxMessage() const
{
    if (m_tableFt8TxMessages == nullptr) {
        return QString();
    }

    int row = -1;
    const QList<QTableWidgetItem *> selected = m_tableFt8TxMessages->selectedItems();
    if (!selected.isEmpty()) {
        row = selected.first()->row();
    }
    if (row < 0) {
        row = 0;
    }

    QTableWidgetItem *item = m_tableFt8TxMessages->item(row, 2);
    return item != nullptr ? item->text().trimmed().toUpper() : QString();
}

int MainWindow::millisecondsToNextFt8TxPeriod() const
{
    const QString modeName = (ui != nullptr && ui->cmbMode != nullptr) ? ui->cmbMode->currentText() : QStringLiteral("FT8");
    const Ft8Mode::Profile profile = Ft8Mode::profileForMode(modeName);
    const int slotMs = qMax(1000, profile.slotMs);
    const int cycleMs = qMax(slotMs * 2, profile.cycleMs);
    const bool txFirst = (m_radioFt8TxFirst != nullptr) ? m_radioFt8TxFirst->isChecked() : true;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const QTime t = now.time();
    const int msOfDay = (((t.hour() * 60 + t.minute()) * 60) + t.second()) * 1000 + t.msec();
    const int cyclePosMs = msOfDay % cycleMs;
    const int selectedStartMs = txFirst ? 0 : slotMs;
    const int selectedEndMs = selectedStartMs + slotMs;
    const bool insideSelectedWindow = (cyclePosMs >= selectedStartMs && cyclePosMs < selectedEndMs);

    if (insideSelectedWindow) {
        const int elapsedInSelectedMs = cyclePosMs - selectedStartMs;
        const int lateStartCutoffMs = qMax(1, (slotMs * 3) / 4);
        if (elapsedInSelectedMs < lateStartCutoffMs) {
            // WSJT-X guiUpdate() allows late starts while fTR < 0.75.
            // Start immediately and let the FT modulator skip the elapsed
            // samples so the symbols remain UTC-slot aligned.
            return 0;
        }
        return qMax(0, cycleMs - cyclePosMs + selectedStartMs);
    }

    if (cyclePosMs < selectedStartMs) {
        return qMax(0, selectedStartMs - cyclePosMs);
    }
    return qMax(0, cycleMs - cyclePosMs + selectedStartMs);
}

qint64 MainWindow::selectedFt8TxSlotBoundaryUtcMs() const
{
    const QString modeName = (ui != nullptr && ui->cmbMode != nullptr) ? ui->cmbMode->currentText() : QStringLiteral("FT8");
    const Ft8Mode::Profile profile = Ft8Mode::profileForMode(modeName);
    const int slotMs = qMax(1000, profile.slotMs);
    const int cycleMs = qMax(slotMs * 2, profile.cycleMs);
    const bool txFirst = (m_radioFt8TxFirst != nullptr) ? m_radioFt8TxFirst->isChecked() : true;

    const QDateTime now = QDateTime::currentDateTimeUtc();
    const QTime t = now.time();
    const int msOfDay = (((t.hour() * 60 + t.minute()) * 60) + t.second()) * 1000 + t.msec();
    const int cyclePosMs = msOfDay % cycleMs;
    const int selectedStartMs = txFirst ? 0 : slotMs;
    const int selectedEndMs = selectedStartMs + slotMs;
    const bool insideSelectedWindow = (cyclePosMs >= selectedStartMs && cyclePosMs < selectedEndMs);

    int deltaToBoundaryMs = 0;
    if (insideSelectedWindow) {
        const int elapsedInSelectedMs = cyclePosMs - selectedStartMs;
        const int lateStartCutoffMs = qMax(1, (slotMs * 3) / 4);
        if (elapsedInSelectedMs < lateStartCutoffMs) {
            // Boundary is in the past for a WSJT-X-style late start.
            // buildCurrentTxModulator() skips elapsed samples instead of
            // delaying the whole FT frame by another period.
            deltaToBoundaryMs = -elapsedInSelectedMs;
        } else {
            deltaToBoundaryMs = cycleMs - cyclePosMs + selectedStartMs;
        }
    } else if (cyclePosMs < selectedStartMs) {
        deltaToBoundaryMs = selectedStartMs - cyclePosMs;
    } else {
        deltaToBoundaryMs = cycleMs - cyclePosMs + selectedStartMs;
    }

    return now.toMSecsSinceEpoch() + static_cast<qint64>(deltaToBoundaryMs);
}

void MainWindow::appendFt8LocalTxRow(const QString &message, int frequencyHz, const QString &tag)
{
    if (m_tableFt8Rx == nullptr) {
        return;
    }

    const QString utc = QDateTime::currentDateTimeUtc().time().toString("HHmmss");
    if (!m_lastFt8RxTableUtc.isEmpty() && m_lastFt8RxTableUtc != utc) {
        const int sepRow = m_tableFt8Rx->rowCount();
        m_tableFt8Rx->insertRow(sepRow);
        for (int col = 0; col < m_tableFt8Rx->columnCount(); ++col) {
            QTableWidgetItem *sep = new QTableWidgetItem(col == 0 ? QStringLiteral("──── %1 ────").arg(utc) : QString());
            sep->setFlags(sep->flags() & ~Qt::ItemIsEditable & ~Qt::ItemIsSelectable);
            sep->setBackground(QBrush(QColor(230, 230, 230)));
            sep->setForeground(QBrush(QColor(90, 90, 90)));
            QFont f = sep->font();
            f.setBold(true);
            sep->setFont(f);
            m_tableFt8Rx->setItem(sepRow, col, sep);
        }
    }
    m_lastFt8RxTableUtc = utc;

    const int row = m_tableFt8Rx->rowCount();
    m_tableFt8Rx->insertRow(row);
    QList<QTableWidgetItem *> items;
    items << new QTableWidgetItem(utc)
          << new QTableWidgetItem(tag)
          << new QTableWidgetItem(QStringLiteral("0.0"))
          << new QTableWidgetItem(QString::number(frequencyHz))
          << new QTableWidgetItem(message)
          << new QTableWidgetItem(QString())
          << new QTableWidgetItem(QString())
          << new QTableWidgetItem(QString())
          << new QTableWidgetItem(QString());
    const QColor txBackground = mmColourFromSetting(m_settings.ftHighlightTxBackground, QColor(255, 247, 95));
    const QColor txForeground = mmColourFromSetting(m_settings.ftHighlightTxForeground, QColor(0, 0, 0));
    for (QTableWidgetItem *it : items) {
        it->setBackground(QBrush(txBackground));
        it->setForeground(QBrush(txForeground));
        QFont f = it->font();
        f.setBold(true);
        it->setFont(f);
    }
    for (int col = 0; col < items.size(); ++col) {
        m_tableFt8Rx->setItem(row, col, items.at(col));
    }
    m_tableFt8Rx->scrollToBottom();

    appendFt8QsoHistoryRow(utc,
                           QStringLiteral("TX"),
                           tag,
                           QStringLiteral("0.0"),
                           frequencyHz,
                           message,
                           txBackground,
                           txForeground);
}

void MainWindow::appendFt8QsoHistoryRow(const QString &utc,
                                        const QString &direction,
                                        const QString &dbOrTag,
                                        const QString &dt,
                                        int frequencyHz,
                                        const QString &message,
                                        const QColor &background,
                                        const QColor &foreground)
{
    if (m_tableFt8QsoHistory == nullptr) {
        return;
    }

    const int row = m_tableFt8QsoHistory->rowCount();
    m_tableFt8QsoHistory->insertRow(row);
    QList<QTableWidgetItem *> items;
    items << new QTableWidgetItem(utc)
          << new QTableWidgetItem(direction)
          << new QTableWidgetItem(dbOrTag)
          << new QTableWidgetItem(dt)
          << new QTableWidgetItem(QString::number(frequencyHz))
          << new QTableWidgetItem(message);
    for (QTableWidgetItem *it : items) {
        it->setBackground(QBrush(background));
        it->setForeground(QBrush(foreground));
        if (direction.trimmed().toUpper() == QStringLiteral("TX")) {
            QFont f = it->font();
            f.setBold(true);
            it->setFont(f);
        }
    }
    for (int col = 0; col < items.size(); ++col) {
        m_tableFt8QsoHistory->setItem(row, col, items.at(col));
    }
    while (m_tableFt8QsoHistory->rowCount() > 250) {
        m_tableFt8QsoHistory->removeRow(0);
    }
    m_tableFt8QsoHistory->scrollToBottom();
}


void MainWindow::scheduleFt8RetryIfNeeded()
{
    const QString retryMessage = m_ftSession.retryMessage.trimmed().toUpper();
    if (!m_ftSession.qsoActive || retryMessage.isEmpty()) {
        return;
    }
    if (m_txRunning || m_ftTxWorkerRunning || (m_txAudioEngine != nullptr && m_txAudioEngine->isRunning())) {
        return;
    }
    if (m_ft8PendingTxArmed && m_pendingFt8TxMessage.trimmed().toUpper() == retryMessage) {
        updateFt8SequencerUi();
        return;
    }

    // scheduleFt8RetryIfNeeded() is called after a TX slot completed.  First
    // consume that completed unanswered attempt; only arm another retry if the
    // operator-configured no-response limit still leaves room.
    if (m_ftSession.retryRemaining > 0) {
        --m_ftSession.retryRemaining;
    }
    if (m_ftSession.retryRemaining <= 0) {
        const QString dx = m_ftSession.dxCall.trimmed().toUpper();
        const bool fullAutoEnabled = m_ft8EvilModeUnlocked &&
                                     m_chkFt8FullAutoQso != nullptr &&
                                     m_chkFt8FullAutoQso->isChecked();
        if (fullAutoEnabled && !dx.isEmpty()) {
            m_ft8AutoNoResponseCooldown.insert(dx, QDateTime::currentDateTimeUtc().addSecs(15 * 60));
            appendLog(QString("FT Auto QSO: %1 gave no reply; skipping this call for 15 minutes and waiting for another CQ.")
                          .arg(dx));
        }
        stopFt8Sequencer(QString("no response after %1 unanswered transmission(s)%2")
                             .arg(qBound(1, m_settings.ft8NoResponseRetryCount, 12))
                             .arg(dx.isEmpty() ? QString() : QStringLiteral(" from ") + dx));
        if (!m_rxRunning && !m_txRunning && m_audioEngine != nullptr && !m_audioEngine->isRunning()) {
            startFt8RxShell();
        }
        return;
    }

    appendLog(QString("FT8 retry armed: %1 unanswered transmission(s) left for %2")
                  .arg(m_ftSession.retryRemaining)
                  .arg(retryMessage));
    scheduleFt8SequencerMessage(retryMessage, QStringLiteral("RETRY"));
}

void MainWindow::startFt8RxShell()
{
    applyFt8Settings();
    const QString modeName = (ui != nullptr && ui->cmbMode != nullptr) ? ui->cmbMode->currentText() : QStringLiteral("FT8");
    const Ft8Mode::Profile profile = Ft8Mode::profileForMode(modeName);

    if (!profile.interoperableCoreAvailable) {
        appendLog(QString("%1 RX not started: %2").arg(profile.shortLabel, profile.note));
        QMessageBox::information(this,
                                 profile.shortLabel + QStringLiteral(" RX"),
                                 profile.note + QStringLiteral("\n\nOnly FT8 and FT4 have active interoperable cores in this build."));
        return;
    }

    if (m_ft8RxDecoder != nullptr) {
        QMetaObject::invokeMethod(m_ft8RxDecoder, "setModeName", Qt::QueuedConnection, Q_ARG(QString, profile.modeName));
        QMetaObject::invokeMethod(m_ft8RxDecoder, "reset", Qt::QueuedConnection);
        QMetaObject::invokeMethod(m_ft8RxDecoder, "setRxMarkerHz", Qt::QueuedConnection, Q_ARG(int, m_spinFt8RxFreq != nullptr ? m_spinFt8RxFreq->value() : m_settings.ft8RxFrequencyHz));
        QMetaObject::invokeMethod(m_ft8RxDecoder, "setMyCall", Qt::QueuedConnection, Q_ARG(QString, stationCallsign()));
        QMetaObject::invokeMethod(m_ft8RxDecoder, "setDxCall", Qt::QueuedConnection, Q_ARG(QString, m_editFt8DxCall != nullptr ? m_editFt8DxCall->text() : m_settings.ft8DxCallsign));
    }
    appendLog(QString("%1 RX decoder started: whole-passband multi-decode, UTC %2 s slots, Costas candidates, LDPC/CRC decoding, CQ/reply waterfall callouts.")
                  .arg(profile.shortLabel)
                  .arg(static_cast<double>(profile.slotMs) / 1000.0, 0, 'f', profile.slotMs % 1000 == 0 ? 0 : 2));
    if (!m_rxRunning) {
        startRx();
    }
}

bool MainWindow::isFtCallBlacklisted(const QString &call) const
{
    const QString normalized = normalizeFtFilterCall(call);
    if (normalized.isEmpty()) {
        return false;
    }
    for (const QString &entry : m_settings.ftBlacklistCalls) {
        const QString item = normalizeFtFilterCall(entry);
        if (!item.isEmpty() && (FtDecodedText::callMatches(normalized, item) || normalized == item)) {
            return true;
        }
    }
    return false;
}

bool MainWindow::isFtCallWatched(const QString &call) const
{
    const QString normalized = normalizeFtFilterCall(call);
    if (normalized.isEmpty()) {
        return false;
    }
    for (const QString &entry : m_settings.ftWatchListCalls) {
        const QString item = normalizeFtFilterCall(entry);
        if (!item.isEmpty() && (FtDecodedText::callMatches(normalized, item) || normalized == item)) {
            return true;
        }
    }
    return false;
}

bool MainWindow::ftCountryAlreadyWorked(const QString &dxcc, const QString &countryName) const
{
    const QString targetDxcc = dxcc.trimmed();
    const QString targetCountry = countryName.trimmed().toUpper();
    if (targetDxcc.isEmpty() && targetCountry.isEmpty()) {
        return false;
    }
    const QVector<LogbookEntry> records = m_logbook.records();
    for (const LogbookEntry &entry : records) {
        QString entryDxcc = entry.adifFields.value(QStringLiteral("DXCC")).trimmed();
        QString entryCountry = entry.country.trimmed().toUpper();
        if ((entryDxcc.isEmpty() || entryCountry.isEmpty()) && !entry.callsign.trimmed().isEmpty()) {
            const CtyCountryFile::LookupResult cty = CtyCountryFile::instance().lookupCallsign(entry.callsign);
            if (cty.valid) {
                if (entryDxcc.isEmpty()) entryDxcc = cty.entity.dxcc.trimmed();
                if (entryCountry.isEmpty()) entryCountry = cty.entity.name.trimmed().toUpper();
            }
        }
        if (!targetDxcc.isEmpty() && !entryDxcc.isEmpty() && entryDxcc == targetDxcc) {
            return true;
        }
        if (!targetCountry.isEmpty() && !entryCountry.isEmpty() && entryCountry == targetCountry) {
            return true;
        }
    }
    return false;
}

bool MainWindow::ftCallWorkedWithinHours(const QString &call, int hours) const
{
    const QString normalized = AdifLogbook::normalizeCallsign(call);
    if (normalized.isEmpty()) {
        return false;
    }
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const qint64 windowSecs = static_cast<qint64>(qBound(1, hours, 168)) * 3600;
    const QVector<LogbookEntry> records = m_logbook.records();
    for (const LogbookEntry &entry : records) {
        if (AdifLogbook::normalizeCallsign(entry.callsign) != normalized) {
            continue;
        }
        const QDateTime t = entry.utc.isValid() ? entry.utc.toUTC() : QDateTime();
        if (t.isValid() && qAbs(t.secsTo(now)) <= windowSecs) {
            return true;
        }
    }
    return false;
}

void MainWindow::handleFt8DecodeReady(const Ft8RxDecoder::Decode &decode)
{
    if (m_tableFt8Rx == nullptr) {
        return;
    }

    if (m_ftAutoTestRunning) {
        m_ftAutoTestStepUnresolvedHashCount += ftUnresolvedHashMarkerCount(decode.message);
        m_ftAutoTestStepVisibleAngleCallCount += ftVisibleAngleCallCount(decode.message);
    }

    const QString myCall = stationCallsign();
    const ParsedFt8Message parsed = parseFt8MessageText(decode.message, myCall);
    const bool directReplyToMe = isFt8DirectReplyToMyCall(parsed, myCall);
    const bool cqMessage = parsed.cq;
    QString workedCall = parsed.senderCall;
    if (cqMessage) {
        const QString cqCall = ft8CqCallsignFromMessage(parsed);
        if (!cqCall.isEmpty()) {
            workedCall = cqCall;
        }
    }
    const QStringList highlightCandidateCalls = ft8HighlightCandidateCallsigns(parsed, myCall);
    if (workedCall.isEmpty() && !highlightCandidateCalls.isEmpty()) {
        workedCall = highlightCandidateCalls.constFirst();
    }
    QString neededCall;
    for (const QString &call : highlightCandidateCalls) {
        if (!m_logbook.containsCallsign(call)) {
            neededCall = call;
            break;
        }
    }
    const bool workedBefore = !workedCall.isEmpty() && m_logbook.containsCallsign(workedCall) && neededCall.isEmpty();
    const QString primaryCallForDisplay = ft8PrimaryCallsignForDisplay(parsed, myCall);
    const Ft8DecodeDisplayInfo displayInfo = makeFt8DecodeDisplayInfo(parsed, myCall,
                                                                       primaryCallForDisplay.isEmpty() ? QString() : m_ftSession.knownGridFor(primaryCallForDisplay));

    QStringList callsForFilters = highlightCandidateCalls;
    if (!parsed.senderCall.isEmpty()) callsForFilters << parsed.senderCall;
    if (!workedCall.isEmpty()) callsForFilters << workedCall;
    if (!primaryCallForDisplay.isEmpty()) callsForFilters << primaryCallForDisplay;
    bool blacklistedDecode = false;
    bool watchedDecode = false;
    for (const QString &call : callsForFilters) {
        if (isFtCallBlacklisted(call)) {
            blacklistedDecode = true;
        }
        if (isFtCallWatched(call)) {
            watchedDecode = true;
        }
    }
    const bool neededStationForLog = !blacklistedDecode && !neededCall.isEmpty() && !m_logbook.containsCallsign(neededCall);
    runFtAutoQsoFlowShadowForDecode(decode, blacklistedDecode, watchedDecode);

    if (blacklistedDecode) {
        appendLog(QStringLiteral("%1 blacklist: decoded %2 at %3 Hz; marked with skull and ignored by AutoQSO/sequencer.")
                      .arg(Ft8Mode::profileForMode(ui->cmbMode->currentText()).shortLabel,
                           decode.message.trimmed(),
                           QString::number(decode.frequencyHz)));
    }

    const QString dxccForDisplay = displayInfo.countryDxcc.trimmed();
    const bool newCountryForLog = !blacklistedDecode &&
                                  m_settings.ftHighlightNewCountryEnabled &&
                                  !dxccForDisplay.isEmpty() &&
                                  !ftCountryAlreadyWorked(dxccForDisplay, displayInfo.country);

    QString bearingText = QStringLiteral("--");
    {
        QPointF homeLonLat;
        QPointF dxLonLat;
        const QString homeGrid = stationLocator().trimmed().toUpper();
        QString bearingGrid = displayInfo.locator.trimmed().left(displayInfo.locator.size() >= 6 ? 6 : 4).toUpper();
        if ((bearingGrid.isEmpty() || !QsoMapWidget::maidenheadToLonLat(bearingGrid, &dxLonLat)) &&
            !displayInfo.callsign.trimmed().isEmpty()) {
            const CtyCountryFile::LookupResult cty = CtyCountryFile::instance().lookupCallsign(displayInfo.callsign);
            if (cty.valid && QsoMapWidget::maidenheadToLonLat(cty.entity.referenceGrid.left(4), &dxLonLat)) {
                bearingGrid = cty.entity.referenceGrid.left(4).toUpper();
            }
        }
        if (!homeGrid.isEmpty() && !bearingGrid.isEmpty() &&
            QsoMapWidget::maidenheadToLonLat(homeGrid, &homeLonLat) &&
            QsoMapWidget::maidenheadToLonLat(bearingGrid, &dxLonLat)) {
            const double b = QsoMapWidget::bearingDeg(homeLonLat, dxLonLat);
            bearingText = QStringLiteral("%1°").arg(QString::number(b, 'f', 0));
        }
    }

    if (!blacklistedDecode) {
        rememberFt8AudioActivity(decode, primaryCallForDisplay);
    }

    const bool fullAutoStartedFromThisDecode = !blacklistedDecode && tryStartFt8FullAutoQso(decode);
    Q_UNUSED(fullAutoStartedFromThisDecode);

    QString displayMessage = decode.message;
    if (blacklistedDecode) {
        displayMessage = QString::fromUtf8("☠ ") + displayMessage;
    }
    if (m_settings.ftWatchListIconEnabled && watchedDecode) {
        displayMessage = QString::fromUtf8("🚨 ") + displayMessage;
    }
    const QString upperMessage = decode.message.trimmed().toUpper();
    if (upperMessage.contains(QStringLiteral("IC8TEM")) || upperMessage.contains(QStringLiteral("IC8FAX"))) {
        displayMessage += QString::fromUtf8("  🐣📡✨");
    }

    // Maintain a lightweight QSO-context grid source from actual decoded FT
    // messages.  The active QSO's DX grid remains the primary logbook source;
    // this observed map is only used to fill the context when the same station
    // was decoded with a locator earlier/later in the session.
    if (!parsed.senderCall.isEmpty() && !parsed.grid.isEmpty() &&
        !parsed.final73 && !parsed.rr73 && !parsed.rrr && parsed.report.isEmpty() &&
        !FtDecodedText::isAckLikeGridTrap(parsed.grid)) {
        m_ftSession.rememberGrid(parsed.senderCall, parsed.grid);
        if (!m_ftSession.dxCall.isEmpty() && FtDecodedText::callMatches(parsed.senderCall, m_ftSession.dxCall) &&
            (m_ftSession.dxGrid.isEmpty() || !FtDecodedText::isGrid(m_ftSession.dxGrid))) {
            m_ftSession.dxGrid = parsed.grid.left(4).toUpper();
        }
    }

    QString heardCall = parsed.senderCall;
    if (parsed.cq) {
        const QString cqCall = ft8CqCallsignFromMessage(parsed);
        if (!cqCall.isEmpty()) {
            heardCall = cqCall;
        }
    }
    if (!blacklistedDecode &&
        !heardCall.isEmpty() && !parsed.grid.isEmpty() && !FtDecodedText::isAckLikeGridTrap(parsed.grid)) {
        recordHeardStationForMaps(heardCall,
                                  parsed.grid,
                                  Ft8Mode::profileForMode(ui->cmbMode->currentText()).adifMode,
                                  m_cmbFt8Band != nullptr ? m_cmbFt8Band->currentText().trimmed().toLower() : QString(),
                                  decode.message);
    }

    // Decodium/Raptor-style table hygiene: keep the best copy of the same
    // decode within the same UTC slot/frequency neighborhood instead of
    // flooding the operator with near-identical rows from multiple candidates
    // or the second subtraction pass.
    for (int row = qMax(0, m_tableFt8Rx->rowCount() - 80); row < m_tableFt8Rx->rowCount(); ++row) {
        QTableWidgetItem *utcItem = m_tableFt8Rx->item(row, 0);
        QTableWidgetItem *snrItem = m_tableFt8Rx->item(row, 1);
        QTableWidgetItem *dtItem = m_tableFt8Rx->item(row, 2);
        QTableWidgetItem *freqItem = m_tableFt8Rx->item(row, 3);
        QTableWidgetItem *msgItem = m_tableFt8Rx->item(row, 4);
        if (utcItem == nullptr || snrItem == nullptr || dtItem == nullptr || freqItem == nullptr || msgItem == nullptr) {
            continue;
        }
        const QString storedMessage = msgItem->data(kFtOriginalMessageRole).toString().trimmed().toUpper();
        const QString compareMessage = storedMessage.isEmpty() ? msgItem->text().trimmed().toUpper() : storedMessage;
        if (utcItem->text() == decode.utc &&
            compareMessage == decode.message.trimmed().toUpper() &&
            qAbs(freqItem->text().toInt() - decode.frequencyHz) <= 12) {
            if (decode.snrDb > snrItem->text().toInt()) {
                snrItem->setText(QString::number(decode.snrDb));
                dtItem->setText(QString::number(decode.dt, 'f', 1));
                freqItem->setText(QString::number(decode.frequencyHz));
            }
            msgItem->setText(displayMessage);
            if (QTableWidgetItem *callItem = m_tableFt8Rx->item(row, 5)) {
                callItem->setText(displayInfo.callsign);
            }
            if (QTableWidgetItem *locItem = m_tableFt8Rx->item(row, 6)) {
                locItem->setText(displayInfo.locator);
            }
            if (QTableWidgetItem *bearingItem = m_tableFt8Rx->item(row, 7)) {
                bearingItem->setText(bearingText);
                bearingItem->setToolTip(uiText("ft_bearing_tooltip", "Bearing from your configured home locator to the decoded station or DXCC reference grid."));
            }
            if (QTableWidgetItem *countryItem = m_tableFt8Rx->item(row, 8)) {
                countryItem->setText(displayInfo.country);
                countryItem->setToolTip(displayInfo.countryTooltip);
            }
            // Do not let visual de-duplication suppress auto-sequencing.
            // WSJT-X may receive multiple candidate copies of the same message;
            // the UI table should keep one row, but a valid direct report/RR73
            // must still be offered to the sequencer immediately.
            if (!blacklistedDecode && directReplyToMe) {
                processFt8SequencerDecode(decode);
            }
            return;
        }
    }

    const int row = m_tableFt8Rx->rowCount();
    m_tableFt8Rx->insertRow(row);
    QList<QTableWidgetItem *> items;
    items << new QTableWidgetItem(decode.utc)
          << new QTableWidgetItem(QString::number(decode.snrDb))
          << new QTableWidgetItem(QString::number(decode.dt, 'f', 1))
          << new QTableWidgetItem(QString::number(decode.frequencyHz))
          << new QTableWidgetItem(displayMessage)
          << new QTableWidgetItem(displayInfo.callsign)
          << new QTableWidgetItem(displayInfo.locator)
          << new QTableWidgetItem(bearingText)
          << new QTableWidgetItem(displayInfo.country);
    if (!items.isEmpty()) {
        if (QTableWidgetItem *messageItem = items.value(4, nullptr)) {
            messageItem->setData(kFtOriginalMessageRole, decode.message.trimmed().toUpper());
        }
        if (newCountryForLog) {
            const QString neededTip = QStringLiteral("New DXCC country not present in logbook: %1").arg(displayInfo.country);
            for (QTableWidgetItem *item : items) {
                item->setData(kFtRowRedOutlineRole, true);
                item->setToolTip(neededTip);
            }
        } else if (neededStationForLog) {
            const QString neededTip = QStringLiteral("Needed station: %1 is not in the ADIF logbook yet. You can call it after the current QSO/73, even if this line is not CQ.").arg(neededCall);
            for (QTableWidgetItem *item : items) {
                item->setToolTip(neededTip);
            }
        }
        if (QTableWidgetItem *bearingItem = items.value(7, nullptr)) {
            bearingItem->setTextAlignment(Qt::AlignCenter);
            bearingItem->setToolTip(uiText("ft_bearing_tooltip", "Bearing from your configured home locator to the decoded station or DXCC reference grid."));
        }
        if (QTableWidgetItem *countryItem = items.value(8, nullptr)) {
            countryItem->setToolTip(displayInfo.countryTooltip);
        }
        if (QTableWidgetItem *locItem = items.value(6, nullptr)) {
            locItem->setToolTip(displayInfo.locator.isEmpty()
                                    ? uiText("ft_locator_missing_tooltip", "No reliable locator was decoded for this line.")
                                    : uiText("ft_locator_tooltip", "Decoded or previously observed Maidenhead locator for this callsign."));
        }
        if (QTableWidgetItem *callItem = items.value(5, nullptr)) {
            callItem->setToolTip(uiText("ft_call_tooltip", "Primary callsign extracted from the decoded FT message."));
        }
    }

    if (blacklistedDecode) {
        const QColor bg(82, 0, 0);
        const QColor fg(255, 92, 92);
        for (QTableWidgetItem *item : items) {
            item->setForeground(QBrush(fg));
            item->setBackground(QBrush(bg));
            QFont font = item->font();
            font.setBold(true);
            item->setFont(font);
            item->setToolTip(uiText("ft_blacklisted_decode_tooltip",
                                    "Blacklisted call: decoded and shown with a skull, but ignored by AutoQSO and the sequencer."));
        }
    } else if (directReplyToMe) {
        const QColor bg = mmColourFromSetting(m_settings.ftHighlightMyCallBackground, QColor(255, 235, 235));
        const QColor fg = mmColourFromSetting(m_settings.ftHighlightMyCallForeground, QColor(220, 0, 0));
        for (QTableWidgetItem *item : items) {
            item->setForeground(QBrush(fg));
            item->setBackground(QBrush(bg));
            QFont font = item->font();
            font.setBold(true);
            item->setFont(font);
        }
    } else if (cqMessage) {
        const QColor bg = mmColourFromSetting(m_settings.ftHighlightCqBackground, QColor(232, 255, 232));
        const QColor fg = mmColourFromSetting(m_settings.ftHighlightCqForeground, QColor(20, 80, 20));
        for (QTableWidgetItem *item : items) {
            item->setForeground(QBrush(fg));
            item->setBackground(QBrush(bg));
        }
    } else if (workedBefore) {
        const QColor bg = mmColourFromSetting(m_settings.ftHighlightWorkedBackground, QColor(240, 240, 240));
        const QColor fg = mmColourFromSetting(m_settings.ftHighlightWorkedForeground, QColor(119, 119, 119));
        for (QTableWidgetItem *item : items) {
            item->setForeground(QBrush(fg));
            item->setBackground(QBrush(bg));
        }
    }
    if (workedBefore) {
        for (QTableWidgetItem *item : items) {
            QFont font = item->font();
            if (m_settings.logbookStrikeWorkedCalls) {
                font.setStrikeOut(true);
            }
            item->setFont(font);
            item->setToolTip(QStringLiteral("Worked before: %1 is already in the ADIF logbook").arg(workedCall));
        }
    } else if (neededStationForLog) {
        for (QTableWidgetItem *item : items) {
            if (item->toolTip().trimmed().isEmpty()) {
                item->setToolTip(QStringLiteral("Needed station: %1 is not in the ADIF logbook yet. You can call it after the current QSO/73, even if this line is not CQ.").arg(neededCall));
            }
        }
    }

    for (int col = 0; col < items.size(); ++col) {
        m_tableFt8Rx->setItem(row, col, items.at(col));
    }
    if (directReplyToMe) {
        appendFt8QsoHistoryRow(decode.utc,
                               QStringLiteral("RX"),
                               QString::number(decode.snrDb),
                               QString::number(decode.dt, 'f', 1),
                               decode.frequencyHz,
                               decode.message,
                               mmColourFromSetting(m_settings.ftHighlightMyCallBackground, QColor(255, 205, 205)),
                               mmColourFromSetting(m_settings.ftHighlightMyCallForeground, QColor(120, 0, 0)));
    }
    while (m_tableFt8Rx->rowCount() > 600) {
        m_tableFt8Rx->removeRow(0);
    }
    if (m_chkFt8AutoScroll == nullptr || m_chkFt8AutoScroll->isChecked()) {
        m_tableFt8Rx->scrollToBottom();
    }

    addFt8WaterfallOverlayForDecode(decode, blacklistedDecode);

    m_ftSession.haveLastSnr = true;
    m_ftSession.lastSnrDb = decode.snrDb;
    m_ftSession.lastSnrMessage = decode.message;
    m_ft8RecentDtSeconds.append(decode.dt);
    while (m_ft8RecentDtSeconds.size() > 12) {
        m_ft8RecentDtSeconds.remove(0);
    }
    updateFt8SignalReportUi();
    updateFt8DecodePerformanceUi();

    appendLog(QString("%1 decode: %2 dB DT %3 Freq %4 Hz: %5").arg(Ft8Mode::profileForMode(ui->cmbMode->currentText()).shortLabel)
                  .arg(decode.snrDb)
                  .arg(decode.dt, 0, 'f', 1)
                  .arg(decode.frequencyHz)
                  .arg(decode.message));

    if (!blacklistedDecode) {
        processFt8SequencerDecode(decode);
    }
}

void MainWindow::addFt8WaterfallOverlayForDecode(const Ft8RxDecoder::Decode &decode, bool blacklistedDecode)
{
    const QString myCall = stationCallsign();
    const ParsedFt8Message parsed = parseFt8MessageText(decode.message, myCall);

    QString label;
    QColor color = mmColourFromSetting(m_settings.ftHighlightCqForeground, QColor(255, 235, 80));
    bool directReplyToMe = false;
    bool blacklistedCallout = false;

    if (blacklistedDecode) {
        QString call = ft8PrimaryCallsignForDisplay(parsed, myCall).trimmed().toUpper();
        if (parsed.cq) {
            const QString cqCall = ft8CqCallsignFromMessage(parsed).trimmed().toUpper();
            if (!cqCall.isEmpty()) call = cqCall;
        }
        if (call.isEmpty()) call = QStringLiteral("BLACKLIST");
        label = QString::fromUtf8("☠ ") + call;
        color = QColor(255, 68, 68);
        blacklistedCallout = true;
    } else if (parsed.cq) {
        const QString cqCall = ft8CqCallsignFromMessage(parsed);
        if (cqCall.isEmpty()) {
            return;
        }
        label = QStringLiteral("CQ %1").arg(cqCall);
        color = mmColourFromSetting(m_settings.ftHighlightCqForeground, QColor(255, 235, 80));
    } else if (isFt8DirectReplyToMyCall(parsed, myCall)) {
        directReplyToMe = true;
        label = QStringLiteral("%1 → %2").arg(parsed.senderCall, myCall.trimmed().toUpper());
        if (!parsed.report.isEmpty()) {
            label += QStringLiteral(" ") + parsed.report;
        } else if (!parsed.grid.isEmpty()) {
            label += QStringLiteral(" ") + parsed.grid;
        }
        color = mmColourFromSetting(m_settings.ftHighlightMyCallForeground, QColor(255, 60, 60));
    } else {
        return;
    }

    Ft8WaterfallCallout callout;
    callout.frequencyHz = qBound(100, decode.frequencyHz, 3000);
    callout.label = label;
    callout.color = color;
    callout.expiresUtc = QDateTime::currentDateTimeUtc().addSecs(directReplyToMe ? 45 : 32);
    callout.directReplyToMe = directReplyToMe;
    callout.blacklisted = blacklistedCallout;

    // Replace existing label for the same station/frequency bucket instead of
    // piling up duplicates over consecutive FT8 slots.
    const int bucket = callout.frequencyHz / 10;
    for (Ft8WaterfallCallout &existing : m_ft8WaterfallCallouts) {
        if (existing.label == callout.label && (existing.frequencyHz / 10) == bucket) {
            existing = callout;
            updateFt8WaterfallOverlays();
            return;
        }
    }

    m_ft8WaterfallCallouts.append(callout);
    updateFt8WaterfallOverlays();
}

void MainWindow::updateFt8WaterfallOverlays()
{
    if (m_waterfallWidget == nullptr) {
        return;
    }

    QVector<WaterfallTextOverlay> overlays;
    if (ui->cmbMode != nullptr && Ft8Mode::isFamilyMode(ui->cmbMode->currentText())) {
        const QDateTime now = QDateTime::currentDateTimeUtc();
        QVector<Ft8WaterfallCallout> kept;
        kept.reserve(m_ft8WaterfallCallouts.size());
        for (const Ft8WaterfallCallout &callout : m_ft8WaterfallCallouts) {
            if (callout.expiresUtc.isValid() && callout.expiresUtc <= now) {
                continue;
            }
            kept.append(callout);

            WaterfallTextOverlay overlay;
            overlay.frequencyHz = static_cast<double>(callout.frequencyHz);
            overlay.label = callout.label;
            overlay.textColor = callout.color;
            overlay.backgroundColor = callout.blacklisted
                                          ? QColor(90, 0, 0, 225)
                                          : (callout.directReplyToMe
                                                ? QColor(70, 0, 0, 220)
                                                : QColor(0, 0, 0, 190));
            overlays.append(overlay);
        }
        m_ft8WaterfallCallouts = kept;
    } else {
        m_ft8WaterfallCallouts.clear();
    }

    m_waterfallWidget->setTextOverlays(overlays);
}

void MainWindow::startFt8TxShell()
{
    applyFt8Settings();
    const QString modeName = (ui != nullptr && ui->cmbMode != nullptr) ? ui->cmbMode->currentText() : QStringLiteral("FT8");
    const Ft8Mode::Profile profile = Ft8Mode::profileForMode(modeName);
    if (!profile.interoperableCoreAvailable) {
        appendLog(QString("%1 TX blocked: %2").arg(profile.shortLabel, profile.note));
        QMessageBox::information(this,
                                 profile.shortLabel + QStringLiteral(" TX"),
                                 profile.note + QStringLiteral("\n\nOnly FT8 and FT4 have active interoperable TX cores in this build."));
        return;
    }

    if (m_txRunning || (m_txAudioEngine != nullptr && m_txAudioEngine->isRunning())) {
        appendLog("FT8 TX blocked: another TX is active.");
        return;
    }

    if (m_offlineAnalysisActive) {
        appendLog("FT8 TX blocked: WAV analysis is active.");
        return;
    }

    if (!ensureStationIdentityForTx(profile.shortLabel)) {
        return;
    }

    const QString message = selectedFt8TxMessage().trimmed().toUpper();
    if (message.isEmpty()) {
        QMessageBox::information(this,
                                 profile.shortLabel + QStringLiteral(" TX"),
                                 uiText("ft8_select_message_before_tx", "Select or generate an FT8 message before starting TX."));
        return;
    }

    if (m_tableFt8TxMessages != nullptr) {
        const QList<QTableWidgetItem *> selected = m_tableFt8TxMessages->selectedItems();
        if (!selected.isEmpty()) {
            setFt8ActiveTxRow(selected.first()->row());
        }
    }

    const bool repeatCq = (m_chkFt8CqRepeat != nullptr) && m_chkFt8CqRepeat->isChecked() && isFt8CqMessage(message);
    if (repeatCq) {
        startFt8CqRepeat();
        return;
    }

    m_ftSession.cqRepeatActive = false;
    if (m_ftSession.state == Ft8SequencerState::CallingCq) {
        m_ftSession.state = Ft8SequencerState::Idle;
    }
    scheduleFt8SequencerMessage(message, QStringLiteral("TX"));
}

void MainWindow::beginScheduledFt8Transmit()
{
    const Ft8Mode::Profile activeFtProfile = Ft8Mode::profileForMode(ui->cmbMode->currentText());
    if (!ensureStationIdentityForTx(activeFtProfile.shortLabel)) {
        m_pendingFt8TxMessage.clear();
        m_pendingFt8TxTag.clear();
        m_pendingFt8Tune = false;
        m_pendingFt8PreSilenceMs = 0;
        m_pendingFt8SlotBoundaryUtcMs = 0;
        m_pendingFt8AudioTargetDelayMs = 0;
        m_pendingFt8PttLeadMs = 0;
        m_pendingFt8PreparedModulator.reset();
        m_pendingFt8TxPlan = FtTxPlan();
        m_pendingFt8PttPrearmed = false;
        m_pendingFt8PttKeyed = false;
        m_ft8PendingTxArmed = false;
        m_ft8PendingTxToken.clear();
        updateFt8TxBannerUi();
        return;
    }
    if (!Ft8Mode::isFamilyMode(ui->cmbMode->currentText()) || (!m_pendingFt8Tune && !activeFtProfile.interoperableCoreAvailable)) {
        appendLog("Pending FT digital TX cancelled: selected mode has no active interoperable TX core.");
        m_pendingFt8TxMessage.clear();
        m_pendingFt8TxTag.clear();
        m_pendingFt8Tune = false;
        m_pendingFt8PreSilenceMs = 0;
        m_pendingFt8SlotBoundaryUtcMs = 0;
        m_pendingFt8AudioTargetDelayMs = 0;
        m_pendingFt8PttLeadMs = 0;
        m_pendingFt8PreparedModulator.reset();
        m_pendingFt8TxPlan = FtTxPlan();
        m_pendingFt8PttPrearmed = false;
        m_pendingFt8PttKeyed = false;
        m_ft8PendingTxArmed = false;
        m_ft8PendingTxToken.clear();
        updateFt8TxBannerUi();
        updateTxControlState();
        return;
    }

    if (m_pendingFt8TxTag == QStringLiteral("CQ") &&
        m_ftSession.cqRepeatActive &&
        m_ftSession.cqRepeatDeadlineUtc.isValid() &&
        QDateTime::currentDateTimeUtc() >= m_ftSession.cqRepeatDeadlineUtc) {
        m_pendingFt8TxMessage.clear();
        m_pendingFt8TxTag.clear();
        m_pendingFt8Tune = false;
        m_ftSession.cqRepeatActive = false;
        m_ftSession.state = Ft8SequencerState::Idle;
        m_ft8PendingTxArmed = false;
        m_ft8PendingTxToken.clear();
        appendLog(activeFtProfile.shortLabel + " CQ repeat timeout reached before next TX; staying in RX.");
        updateFt8SequencerUi();
        updateTxControlState();
        return;
    }

    QString rotatorWaitReason;
    int rotatorEtaMs = 0;
    if (!ftRotatorReadyForPendingTx(&rotatorWaitReason, &rotatorEtaMs)) {
        deferPendingFtTxForRotator(rotatorEtaMs, rotatorWaitReason);
        return;
    }

    m_ftSession.lastTxWasTune = m_pendingFt8Tune;
    m_ftSession.lastTxMessage = m_pendingFt8TxMessage;
    m_ftSession.lastTxTag = m_pendingFt8TxTag.isEmpty() ? QStringLiteral("TX") : m_pendingFt8TxTag;
    m_lastFt8TxPlan = m_pendingFt8TxPlan;
    m_lastFt8TxPlan.message = m_ftSession.lastTxMessage;
    m_lastFt8TxPlan.tag = m_ftSession.lastTxTag;
    m_lastFt8TxPlan.tune = m_ftSession.lastTxWasTune;
    updateFt8TxBannerUi();

    if (m_pendingFt8Tune) {
        appendLog(QString("Starting %1 tune at %2 Hz.")
                      .arg(activeFtProfile.shortLabel)
                      .arg(m_spinFt8TxFreq != nullptr ? m_spinFt8TxFreq->value() : 1500));
    } else {
        appendFt8LocalTxRow(m_pendingFt8TxMessage,
                            m_spinFt8TxFreq != nullptr ? m_spinFt8TxFreq->value() : 1500,
                            m_ftSession.lastTxTag);
        const QString timingSuffix = QStringLiteral(" (UTC slot boundary target, PTT/audio lead %1 ms)")
            .arg(m_pendingFt8PttLeadMs);
        appendLog(QString("Starting %1 %2 slot TX: %3%4")
                      .arg(activeFtProfile.shortLabel,
                           m_ftSession.lastTxTag,
                           m_pendingFt8TxMessage,
                           timingSuffix));
    }

    prearmFtPreparedSlotTransmit();
}

void MainWindow::stopFt8Shell()
{
    const bool txWasActive = (m_txRunning ||
                              m_ftTxWorkerRunning ||
                              (m_txAudioEngine != nullptr && m_txAudioEngine->isRunning()));

    if (m_ftSlotScheduler != nullptr) {
        QMetaObject::invokeMethod(m_ftSlotScheduler, "cancelTransmission", Qt::QueuedConnection);
    }

    if (m_pendingFt8PttKeyed && !txWasActive) {
        unkeyPttAfterTx();
    }

    m_ft8PendingTxArmed = false;
    m_ft8PendingTxToken.clear();
    m_pendingFt8TxMessage.clear();
    m_pendingFt8TxTag.clear();
    m_pendingFt8Tune = false;
    m_pendingFt8PreSilenceMs = 0;
    m_pendingFt8SlotBoundaryUtcMs = 0;
    m_pendingFt8AudioTargetDelayMs = 0;
    m_pendingFt8PttLeadMs = 0;
    m_pendingFt8PreparedModulator.reset();
    m_pendingFt8TxPlan = FtTxPlan();
    m_pendingFt8PttPrearmed = false;
    m_pendingFt8PttKeyed = false;
    m_hasDeferredFt8TxPlan = false;
    m_deferredFt8TxMessage.clear();
    m_deferredFt8TxTag.clear();
    m_deferredFt8TxPlan = FtTxPlan();
    m_ftSession.deferredState = Ft8SequencerState::Idle;

    // FT STOP must immediately cancel the armed state that disables the FT RX/TX
    // controls.  The previous code cleared the state, but in some paths the UI
    // refresh only happened later, after the worker stopped.  If the worker signal
    // was delayed or lost, RX/TX stayed visually locked.
    appendLog("Pending FT digital TX cancelled.");

    // FT STOP has one deterministic meaning: cancel active/pending TX and stop
    // the automatic sequence.  Do not start RX in parallel while the FT TX
    // worker is still unwinding; handleTxStopped() will return to RX cleanly.
    if (txWasActive) {
        stopImageTx();

        // Safety net for the dedicated FT worker/audio engine: if STOP was pressed
        // and no stopped() signal reaches MainWindow, force the logical UI state
        // back to idle/RX-safe after a short grace period.  This avoids locked RX/TX
        // buttons while still giving the worker the first chance to shut down cleanly.
        QTimer::singleShot(900, this, [this]() {
            if (!Ft8Mode::isFamilyMode(ui->cmbMode->currentText())) {
                return;
            }
            if (m_txRunning || m_ftTxWorkerRunning ||
                (m_txAudioEngine != nullptr && m_txAudioEngine->isRunning())) {
                appendLog("FT STOP watchdog: TX worker did not confirm stop; forcing UI/audio state unlock.");
                if (m_ftTxWorker != nullptr) {
                    QMetaObject::invokeMethod(m_ftTxWorker, "stopOutput", Qt::QueuedConnection);
                }
                if (m_txAudioEngine != nullptr) {
                    m_txAudioEngine->stopOutput();
                }
                unkeyPttAfterTx();
                m_ftTxWorkerRunning = false;
                m_txRunning = false;
                m_returnToRxAfterTx = false;
                m_txFinishedNaturally = false;
                m_currentTxIsTextMode = false;
                setReceiverRunning(false);
                updateTxPreview();
                updateTxControlState();
                startFt8RxShell();
            }
        });
    }

    if (m_chkFt8FullAutoQso != nullptr && m_chkFt8FullAutoQso->isChecked()) {
        m_chkFt8FullAutoQso->setChecked(false);
    }
    if (m_chkFt8CqRepeat != nullptr && m_chkFt8CqRepeat->isChecked()) {
        m_chkFt8CqRepeat->setChecked(false);
    }

    stopFt8Sequencer(QStringLiteral("STOP requested"));
    setReceiverRunning(m_rxRunning && !m_txRunning);
    updateTxControlState();
    updateFt8TxBannerUi();

    if (!txWasActive && !m_rxRunning && (m_audioEngine == nullptr || !m_audioEngine->isRunning())) {
        startFt8RxShell();
    }

    appendLog(txWasActive
              ? QStringLiteral("FT digital STOP requested: active TX stopping, auto-sequence cancelled; RX will resume after TX worker stops.")
              : QStringLiteral("FT digital STOP requested: pending TX/auto-sequence cancelled, RX monitor active."));
}

void MainWindow::tuneFt8Shell()
{
    applyFt8Settings();

    const bool tuneActive = m_pendingFt8Tune ||
                            (m_ftSession.lastTxWasTune && (m_txRunning || m_ftTxWorkerRunning ||
                                (m_txAudioEngine != nullptr && m_txAudioEngine->isRunning()))) ||
                            (m_pendingFt8PttKeyed && !m_txRunning && !m_ftTxWorkerRunning);
    if (tuneActive) {
        appendLog("FT digital Tune toggle: stopping tune/PTT and returning to RX.");
        stopFt8Shell();
        return;
    }

    if (m_txRunning || m_ftTxWorkerRunning || (m_txAudioEngine != nullptr && m_txAudioEngine->isRunning())) {
        appendLog("FT digital Tune blocked: another TX is active.");
        return;
    }

    if (m_ftSlotScheduler != nullptr) {
        QMetaObject::invokeMethod(m_ftSlotScheduler, "cancelTransmission", Qt::QueuedConnection);
    }

    // Tune is a manual carrier/action, not part of CQ repeat or the QSO
    // sequencer.  Cancel any armed auto-CQ/deferred slot so Tune cannot be
    // followed by an unexpected CQ restart when the user stops or the tone ends.
    m_ftSession.cqRepeatActive = false;
    if (m_ftSession.state == Ft8SequencerState::CallingCq) {
        m_ftSession.state = Ft8SequencerState::Idle;
    }
    m_hasDeferredFt8TxPlan = false;
    m_deferredFt8TxMessage.clear();
    m_deferredFt8TxTag.clear();
    m_deferredFt8TxPlan = FtTxPlan();
    m_ftSession.deferredState = Ft8SequencerState::Idle;

    if (m_pendingFt8PttKeyed) {
        unkeyPttAfterTx();
    }

    m_ft8PendingTxArmed = false;
    m_ft8PendingTxToken.clear();
    m_pendingFt8PreparedModulator.reset();
    m_pendingFt8PttPrearmed = false;
    m_pendingFt8PttKeyed = false;
    m_pendingFt8PreSilenceMs = 0;
    m_pendingFt8SlotBoundaryUtcMs = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
    m_pendingFt8AudioTargetDelayMs = 0;
    m_pendingFt8PttLeadMs = 0;
    m_pendingFt8TxPlan = FtTxPlan();
    m_pendingFt8TxPlan.tune = true;
    m_pendingFt8TxPlan.audioFrequencyHz = (m_spinFt8TxFreq != nullptr) ? m_spinFt8TxFreq->value() : m_settings.ft8TxFrequencyHz;
    m_pendingFt8TxPlan.slotBoundaryUtcMs = m_pendingFt8SlotBoundaryUtcMs;
    m_pendingFt8TxPlan.audioTargetDelayMs = 0;
    m_pendingFt8TxPlan.pttLeadMs = 0;

    m_pendingFt8TxTag = QStringLiteral("TUNE");
    m_pendingFt8Tune = true;
    m_pendingFt8TxMessage = QStringLiteral("TUNE");

    beginScheduledFt8Transmit();
    startFtPreparedSlotTransmit();
}


void MainWindow::refreshTextMacroButtons()
{
    auto applyLabels = [this](const QList<QPushButton *> &buttons) {
        for (int i = 0; i < buttons.size(); ++i) {
            QPushButton *button = buttons.at(i);
            if (button == nullptr) {
                continue;
            }

            const QString label = m_settings.textMacroLabels.value(i, QString("Macro %1").arg(i + 1));
            const QString text = m_settings.textMacroTexts.value(i);
            button->setText(label);
            button->setToolTip(QString("Transmit macro: %1\n\n%2").arg(label, text));
            button->setStatusTip(button->toolTip());
        }
    };

    applyLabels(m_rttyMacroButtons);
    applyLabels(m_bpsk31MacroButtons);
    applyLabels(m_mfskMacroButtons);
    applyLabels(m_cwMacroButtons);
    applyLabels(m_hellMacroButtons);
}

QString MainWindow::expandTextTemplate(const QString &source) const
{
    QString expanded = source;
    const QDateTime now = QDateTime::currentDateTime();
    const QDateTime utc = now.toUTC();

    auto replaceToken = [&expanded](const QString &token, const QString &value) {
        expanded.replace("{" + token + "}", value, Qt::CaseInsensitive);
    };

    replaceToken("MYCALL", m_settings.textMyCallsign);
    replaceToken("MYNAME", m_settings.textMyName);
    replaceToken("MYQTH", m_settings.textMyQth);
    replaceToken("LOC", m_settings.textMyLocator);
    QsoFormWidgets *form = activeQsoForm();
    const QString qsoCall = (form != nullptr && form->callsign != nullptr)
                                ? form->callsign->text().trimmed().toUpper()
                                : QString();
    const QString qsoRstSent = (form != nullptr && form->rstSent != nullptr && !form->rstSent->text().trimmed().isEmpty())
                                   ? form->rstSent->text().trimmed().toUpper()
                                   : QString("599");
    const QString qsoRstRcvd = (form != nullptr && form->rstReceived != nullptr && !form->rstReceived->text().trimmed().isEmpty())
                                   ? form->rstReceived->text().trimmed().toUpper()
                                   : QString("599");
    const QString qsoBand = (form != nullptr && form->band != nullptr)
                                ? form->band->text().trimmed()
                                : QString();
    const QString qsoMode = (form != nullptr && form->mode != nullptr && !form->mode->text().trimmed().isEmpty())
                                ? form->mode->text().trimmed().toUpper()
                                : currentAdifMode();

    replaceToken("CALL", qsoCall);
    replaceToken("NAME", QString());
    replaceToken("QTH", QString());
    replaceToken("RST", qsoRstSent);
    replaceToken("RST_SENT", qsoRstSent);
    replaceToken("RST_RCVD", qsoRstRcvd);
    replaceToken("BAND", qsoBand);
    replaceToken("RIG", m_settings.textRig);
    replaceToken("ANT", m_settings.textAntenna);
    replaceToken("PWR", m_settings.textPower);
    replaceToken("MODE", qsoMode);
    replaceToken("DATE", now.date().toString("yyyy-MM-dd"));
    replaceToken("TIME", now.time().toString("HH:mm"));
    replaceToken("UTC", utc.toString("yyyy-MM-dd HH:mm 'UTC'"));
    replaceToken("NL", "\n");
    replaceToken("CRLF", "\n");

    return expanded;
}

void MainWindow::appendRxTextTerminal(QPlainTextEdit *terminal,
                                      const QString &text,
                                      bool requireLineBreakPair,
                                      bool *pendingLineBreak)
{
    if (terminal == nullptr || text.isEmpty()) {
        return;
    }

    /*
     * Decoder signals may arrive very quickly, one character at a time.  Build
     * the text chunk first and insert it once through QPlainTextEdit.  Avoid
     * keeping and re-applying QTextCursor instances after clear()/mode changes:
     * that could make Qt print "QTextCursor::setPosition: Position ... out of
     * range" on the terminal even though the received text was otherwise fine.
     */
    QString chunk;
    chunk.reserve(text.size());

    bool pending = pendingLineBreak != nullptr ? *pendingLineBreak : false;

    for (QChar ch : text) {
        if (ch == QChar('\a') || ch.isNull()) {
            continue;
        }

        const bool isLineControl = (ch == QChar('\r') || ch == QChar('\n'));
        if (isLineControl) {
            if (requireLineBreakPair) {
                if (pending) {
                    chunk.append('\n');
                    pending = false;
                } else {
                    pending = true;
                }
            } else {
                if (!pending) {
                    chunk.append('\n');
                }
                pending = true;
            }
            continue;
        }

        if (pending) {
            /*
             * For RTTY, a single CR or LF without its companion is usually a
             * false control character from marginal demodulation.  Drop it so
             * normal text keeps flowing horizontally.  For BPSK31 the newline
             * was already inserted above, and this just closes the CR/LF pair
             * suppression window.
             */
            pending = false;
        }

        chunk.append(ch);
    }

    if (pendingLineBreak != nullptr) {
        *pendingLineBreak = pending;
    }

    if (chunk.isEmpty()) {
        return;
    }

    terminal->moveCursor(QTextCursor::End);
    terminal->insertPlainText(chunk);
    terminal->moveCursor(QTextCursor::End);
    terminal->ensureCursorVisible();
    highlightCallsignsInTerminal(terminal);
}

void MainWindow::appendTextTerminal(QPlainTextEdit *terminal, const QString &prefix, const QString &text)
{
    if (terminal == nullptr || text.isEmpty()) {
        return;
    }

    QString normalized = text;
    normalized.replace("\r\n", "\n");
    normalized.replace('\r', '\n');
    normalized.remove(QChar('\a'));

    if (normalized.isEmpty()) {
        return;
    }

    QString chunk;

    if (prefix.isEmpty()) {
        /*
         * RX decoders often emit one character at a time.  Do not use
         * appendPlainText() and do not insert an artificial newline for
         * each update, otherwise BPSK31/RTTY/CW text appears as a vertical
         * column.  Let real CR/LF characters from the modem stream control
         * the terminal formatting.
         */
        chunk = normalized;
    } else {
        if (!terminal->toPlainText().isEmpty()) {
            chunk.append('\n');
        }

        const QStringList lines = normalized.split('\n');
        for (int i = 0; i < lines.size(); ++i) {
            if (i > 0) {
                chunk.append('\n');
            }
            chunk.append(prefix);
            chunk.append(lines.at(i));
        }

        if (!normalized.endsWith('\n')) {
            chunk.append('\n');
        }
    }

    if (chunk.isEmpty()) {
        return;
    }

    terminal->moveCursor(QTextCursor::End);
    terminal->insertPlainText(chunk);
    terminal->moveCursor(QTextCursor::End);
    terminal->ensureCursorVisible();
    if (terminal == m_txtRttyRx || terminal == m_txtBpsk31Rx || terminal == m_txtMfskRx || terminal == m_txtCwRx) {
        highlightCallsignsInTerminal(terminal);
    }
    if (terminal == m_txtRttyRx || terminal == m_txtBpsk31Rx) {
        scanTextForHeardStations(terminal, chunk);
    }
}

/**
 * @brief Prepares one text TX editor for live progress highlighting.
 *
 * Purpose:
 * - Reset old color/underline formatting from previous transmissions.
 * - Remember the editor that belongs to the currently active text TX.
 * - Let TX progress paint transmitted characters without moving user text.
 */
void MainWindow::beginTextTxHighlight(QPlainTextEdit *editor)
{
    m_activeTextTxEditor = editor;
    m_activeTextTxLength = (editor != nullptr) ? editor->toPlainText().size() : 0;
    m_textTxHighlightedChars = -1;

    resetTextTxHighlight(editor);
    updateTextTxHighlight(0.0);
}

/**
 * @brief Clears character formatting in one text TX editor.
 *
 * Purpose:
 * - Restore normal black, non-underlined input text before a new TX.
 * - Use bounded document positions to avoid QTextCursor range warnings.
 */
void MainWindow::resetTextTxHighlight(QPlainTextEdit *editor)
{
    if (editor == nullptr || editor->document() == nullptr) {
        return;
    }

    const int documentEnd = safeDocumentEndPosition(editor->document());
    if (documentEnd <= 0) {
        return;
    }

    QSignalBlocker block(editor);

    QTextCharFormat normalFormat;
    normalFormat.setForeground(QColor("#111111"));
    normalFormat.setFontUnderline(false);
    normalFormat.setUnderlineStyle(QTextCharFormat::NoUnderline);

    QTextCursor cursor(editor->document());
    if (selectDocumentRange(cursor, 0, documentEnd)) {
        cursor.mergeCharFormat(normalFormat);
    }
}

/**
 * @brief Paints already-transmitted text characters in green.
 *
 * Purpose:
 * - Provide a contest-style visual TX progress cue for RTTY/BPSK.
 * - Avoid clearing the text box, so the operator can see what has been sent.
 * - Keep formatting updates idempotent and bounded for Qt cursor safety.
 */
void MainWindow::updateTextTxHighlight(double progress)
{
    if (m_activeTextTxEditor == nullptr ||
        m_activeTextTxEditor->document() == nullptr ||
        m_activeTextTxLength <= 0) {
        return;
    }

    const int documentEnd = safeDocumentEndPosition(m_activeTextTxEditor->document());
    const int safeLength = qMin(m_activeTextTxLength, documentEnd);
    if (safeLength <= 0) {
        return;
    }

    const int wanted = qBound(0, static_cast<int>(qFloor(progress * safeLength + 0.5)), safeLength);

    if (wanted == m_textTxHighlightedChars) {
        return;
    }

    QSignalBlocker block(m_activeTextTxEditor);

    QTextCharFormat pendingFormat;
    pendingFormat.setForeground(QColor("#111111"));
    pendingFormat.setFontUnderline(false);
    pendingFormat.setUnderlineStyle(QTextCharFormat::NoUnderline);

    QTextCharFormat sentFormat;
    sentFormat.setForeground(QColor("#118a2a"));
    sentFormat.setFontUnderline(false);
    sentFormat.setUnderlineStyle(QTextCharFormat::NoUnderline);

    QTextCharFormat currentFormat;
    currentFormat.setForeground(QColor("#003f9e"));
    currentFormat.setFontUnderline(true);
    currentFormat.setUnderlineColor(QColor("#003f9e"));
    currentFormat.setUnderlineStyle(QTextCharFormat::SingleUnderline);

    QTextCursor cursor(m_activeTextTxEditor->document());

    if (selectDocumentRange(cursor, 0, safeLength)) {
        cursor.mergeCharFormat(pendingFormat);
    }

    if (wanted > 0 && selectDocumentRange(cursor, 0, wanted)) {
        cursor.mergeCharFormat(sentFormat);
    }

    if (wanted < safeLength && selectDocumentRange(cursor, wanted, wanted + 1)) {
        cursor.mergeCharFormat(currentFormat);
    }

    m_textTxHighlightedChars = wanted;
}

/**
 * @brief Clears the active text TX highlighter state.
 *
 * Purpose:
 * - Detach progress updates after TX stops or errors.
 * - Leave the already-painted text visible for operator review.
 */
void MainWindow::endTextTxHighlight()
{
    if (m_activeTextTxEditor != nullptr) {
        resetTextTxHighlight(m_activeTextTxEditor);
    }

    m_activeTextTxEditor = nullptr;
    m_activeTextTxLength = 0;
    m_textTxHighlightedChars = -1;
}

bool MainWindow::startTextModeTx(const QString &text)
{
    if (m_txRunning) {
        appendLog("Text TX blocked: another TX is active.");
        return false;
    }

    /*
     * Text modes are contest/QSO modes: RX is normally left running.  The
     * actual transition to TX is handled by startImageTx(), which pauses live
     * RX for the duration of the transmission and returns to RX afterwards.
     */

    if (m_offlineAnalysisActive) {
        appendLog("Text TX blocked: WAV analysis is active.");
        return false;
    }

    const QString modeLabel = (ui != nullptr && ui->cmbMode != nullptr) ? ui->cmbMode->currentText() : QStringLiteral("Text");
    if (!ensureStationIdentityForTx(modeLabel)) {
        return false;
    }

    const QString expanded = expandTextTemplate(text).trimmed();

    if (expanded.isEmpty()) {
        appendLog("Text TX blocked: empty text.");
        return false;
    }

    const QString modeName = ui->cmbMode->currentText();

    if (modeName == RttyDecoder::modeName()) {
        if (m_txtRttyTx == nullptr) {
            return false;
        }

        m_txtRttyTx->setPlainText(expanded);
        beginTextTxHighlight(m_txtRttyTx);
        appendTextTerminal(m_txtRttyRx, "TX> ", expanded);
        startImageTx();

        if (m_txRunning) {
            updateTxPreview();
            return true;
        }

        resetTextTxHighlight(m_txtRttyTx);
        endTextTxHighlight();
        return false;
    }

    if (modeName == Bpsk31Decoder::modeName()) {
        if (m_txtBpsk31Tx == nullptr) {
            return false;
        }

        m_txtBpsk31Tx->setPlainText(expanded);
        beginTextTxHighlight(m_txtBpsk31Tx);
        appendTextTerminal(m_txtBpsk31Rx, "TX> ", expanded);
        startImageTx();

        if (m_txRunning) {
            updateTxPreview();
            return true;
        }

        resetTextTxHighlight(m_txtBpsk31Tx);
        endTextTxHighlight();
        return false;
    }

    if (modeName == MfskDecoder::modeName()) {
        if (m_txtMfskTx == nullptr) {
            return false;
        }

        m_txtMfskTx->setPlainText(expanded);
        beginTextTxHighlight(m_txtMfskTx);
        appendTextTerminal(m_txtMfskRx, "TX> ", expanded);
        startImageTx();

        if (m_txRunning) {
            updateTxPreview();
            return true;
        }

        resetTextTxHighlight(m_txtMfskTx);
        endTextTxHighlight();
        return false;
    }

    if (modeName == CwDecoder::modeName()) {
        if (m_txtCwTx == nullptr) {
            return false;
        }

        m_txtCwTx->setPlainText(expanded);
        /* CW now uses a pre-rendered clean sine/BPF audio stream.  The TX text
         * editor is only an operator note here; do not run per-character
         * QTextCursor progress highlighting on it.  On short CW strings such as
         * "ciao", QPlainTextEdit/QTextCursor can print harmless but noisy
         * "Position ... out of range" warnings while the audio worker reports
         * progress.  Keeping CW text static avoids those terminal warnings and
         * does not affect the generated Morse audio.
         */
        m_activeTextTxEditor = nullptr;
        m_activeTextTxLength = 0;
        m_textTxHighlightedChars = -1;
        appendTextTerminal(m_txtCwRx, "TX> ", expanded);
        startImageTx();

        if (m_txRunning) {
            updateTxPreview();
            return true;
        }

        endTextTxHighlight();
        return false;
    }

    if (modeName == HellschreiberDecoder::modeName()) {
        if (m_txtHellTx == nullptr) {
            return false;
        }

        m_txtHellTx->setPlainText(expanded);
        /* Hell TX audio is now fully pre-rendered before playback, so the editor
         * can safely show text progress again.  The highlighter uses cursor
         * movement instead of absolute QTextCursor::setPosition() calls, which
         * avoids Qt out-of-range warnings when the document is edited or rebuilt.
         */
        beginTextTxHighlight(m_txtHellTx);
        const HellschreiberDecoder::Variant variant = (m_cmbHellVariant != nullptr)
                                                         ? HellschreiberDecoder::variantFromKey(m_cmbHellVariant->currentData().toString())
                                                         : HellschreiberDecoder::Variant::FeldHell;
        appendLog(HellschreiberDecoder::variantName(variant) + " TX> " + expanded.left(96));
        startImageTx();

        if (m_txRunning) {
            updateTxPreview();
            return true;
        }

        endTextTxHighlight();
        return false;
    }

    appendLog("Text TX blocked: select RTTY, PSK, MFSK, CW or Hellschreiber mode.");
    return false;
}

void MainWindow::sendTextMacro(int index)
{
    if (index < 0 || index >= m_settings.textMacroTexts.size()) {
        return;
    }

    startTextModeTx(m_settings.textMacroTexts.at(index));
}

AudioBlock MainWindow::conditionAudioForWaterfall(const AudioBlock &block)
{
    DspConditioner::Config config;
    config.enabled = true;
    config.profile = DspConditioner::Profile::DisplayWide;
    config.humNotchEnabled = true;
    config.impulseBlankerEnabled = false;
    config.modeBandpassEnabled = true;
    config.noiseReductionEnabled = false;
    config.agcEnabled = false;
    config.blackHz = 65.0;
    config.whiteHz = 3400.0;

    m_waterfallConditioner.setConfig(config);
    return m_waterfallConditioner.processBlock(block);
}

AudioBlock MainWindow::conditionAudioForActiveMode(const AudioBlock &block)
{
    DspConditioner::Config config;
    const QString modeName = ui->cmbMode->currentText();

    config.enabled = true;
    config.humNotchEnabled = true;
    config.impulseBlankerEnabled = false;
    config.noiseReductionEnabled = false;
    config.agcEnabled = false;

    if (modeName == WeatherFaxDecoder::modeName()) {
        config.profile = DspConditioner::Profile::WeatherFax;
        config.modeBandpassEnabled = ui->chkFaxInputBandpass->isChecked();
        config.noiseReductionEnabled = m_settings.weatherFaxNoiseReductionEnabled;
        config.agcEnabled = m_settings.weatherFaxAgcEnabled;
        config.imageWaveletDenoiseEnabled = m_settings.weatherFaxWaveletDenoiseEnabled;
        config.blackHz = static_cast<double>(ui->spinFaxBlackHz->value());
        config.whiteHz = static_cast<double>(ui->spinFaxWhiteHz->value());
    } else if (modeName == SstvDecoder::modeName()) {
        config.profile = DspConditioner::Profile::Sstv;
        config.modeBandpassEnabled = true;
        config.noiseReductionEnabled = m_settings.sstvNoiseReductionEnabled;
        config.agcEnabled = m_settings.sstvAgcEnabled;
        config.imageWaveletDenoiseEnabled = m_settings.sstvWaveletDenoiseEnabled;
        config.blackHz = 1500.0;
        config.whiteHz = 2300.0;
    } else if (modeName == RttyDecoder::modeName()) {
        config.profile = DspConditioner::Profile::Rtty;
        config.modeBandpassEnabled = true;
        config.noiseReductionEnabled = m_settings.rttyNoiseReductionEnabled;
        config.agcEnabled = m_settings.rttyAgcEnabled;
        config.adaptiveLineEnhancerEnabled = m_settings.rttyAdaptiveLineEnhancerEnabled;
        config.rttyMatchedFilterEnabled = m_settings.rttyMatchedFilterEnabled;
        config.rttyMarkSpaceEnhancerEnabled = m_settings.rttyMarkSpaceEnhancerEnabled;
        const double mark = (m_spinRttyMarkHz != nullptr) ? m_spinRttyMarkHz->value() : 2125.0;
        const double shift = (m_spinRttyShiftHz != nullptr) ? m_spinRttyShiftHz->value() : 170.0;
        config.blackHz = mark;
        config.whiteHz = mark + shift;
    } else if (modeName == Bpsk31Decoder::modeName()) {
        config.profile = DspConditioner::Profile::Bpsk31;
        config.modeBandpassEnabled = true;
        config.noiseReductionEnabled = m_settings.bpsk31NoiseReductionEnabled;
        config.agcEnabled = m_settings.bpsk31AgcEnabled;
        config.bpskCoherentTrackingEnabled = m_settings.bpsk31CoherentTrackingEnabled;
        const double tone = (m_spinBpsk31ToneHz != nullptr) ? m_spinBpsk31ToneHz->value() : 1000.0;
        config.blackHz = tone;
        config.whiteHz = tone;
    } else if (modeName == MfskDecoder::modeName()) {
        config.profile = DspConditioner::Profile::Mfsk;
        config.modeBandpassEnabled = true;
        config.noiseReductionEnabled = m_settings.mfskNoiseReductionEnabled;
        config.agcEnabled = m_settings.mfskAgcEnabled;
        const double center = (m_spinMfskCenterHz != nullptr) ? m_spinMfskCenterHz->value() : 1000.0;
        const QString variantKey = (m_cmbMfskVariant != nullptr) ? m_cmbMfskVariant->currentData().toString() : QStringLiteral("MFSK16");
        const bool mfsk32 = (MfskDecoder::variantFromKey(variantKey) == MfskDecoder::Variant::Mfsk32);
        const double span = mfsk32 ? (31.25 * 34.0) : (15.625 * 18.0);
        config.blackHz = center - span * 0.5;
        config.whiteHz = center + span * 0.5;
    } else if (modeName == HellschreiberDecoder::modeName()) {
        config.profile = DspConditioner::Profile::Hell;
        config.modeBandpassEnabled = true;
        config.noiseReductionEnabled = m_settings.hellNoiseReductionEnabled;
        config.agcEnabled = m_settings.hellAgcEnabled;
        const double tone = (m_spinHellToneHz != nullptr) ? m_spinHellToneHz->value() : 1000.0;
        const bool fsk105 = (m_cmbHellVariant != nullptr && m_cmbHellVariant->currentData().toString() == "FSK105");
        const double halfShift = fsk105 ? (HellschreiberDecoder::fsk105ShiftHz() * 0.5) : 0.0;
        config.blackHz = tone - halfShift;
        config.whiteHz = tone + halfShift;
    } else if (Ft8Mode::isFamilyMode(modeName)) {
        config.profile = DspConditioner::Profile::FtWeakSignal;
        config.modeBandpassEnabled = true;
        config.noiseReductionEnabled = false;
        config.agcEnabled = false;
        config.blackHz = 100.0;
        config.whiteHz = 3000.0;
    } else if (modeName == CwDecoder::modeName()) {
        config.profile = DspConditioner::Profile::Cw;
        config.modeBandpassEnabled = true;
        config.noiseReductionEnabled = m_settings.cwNoiseReductionEnabled;
        config.agcEnabled = false;
        config.adaptiveLineEnhancerEnabled = m_settings.cwAdaptiveLineEnhancerEnabled;
        const double tone = (m_spinCwToneHz != nullptr) ? m_spinCwToneHz->value() : 700.0;
        config.blackHz = tone;
        config.whiteHz = tone;
    } else {
        config.profile = DspConditioner::Profile::General;
        config.modeBandpassEnabled = true;
    }

    m_decoderConditioner.setConfig(config);
    return m_decoderConditioner.processBlock(block);
}

void MainWindow::resetDspEngine()
{
    m_wavWaterfallDecimationCounter = 0;
    m_waterfallConditioner.reset();

    if (m_dspEngine == nullptr) {
        return;
    }

    if (m_dspEngine->thread() == QThread::currentThread()) {
        m_dspEngine->reset();
        return;
    }

    QMetaObject::invokeMethod(
        m_dspEngine,
        "reset",
        Qt::BlockingQueuedConnection
        );
}

void MainWindow::processDspAudioBlockForWav(const AudioBlock &block)
{
    if (m_dspEngine == nullptr) {
        return;
    }

    /*
     * Offline WAV analysis may run tens or hundreds of times faster than real
     * time.  Feeding every block into the diagnostic waterfall makes the UI
     * queue thousands of FFT/rendering updates while the actual decoder is
     * already finished.  Keep only a light preview stream for the waterfall;
     * the modem decoder still receives every audio sample through its normal
     * path, so no decoded information is lost.
     */
    ++m_wavWaterfallDecimationCounter;

    if ((m_wavWaterfallDecimationCounter % 8) != 0) {
        return;
    }

    if (m_dspEngine->thread() == QThread::currentThread()) {
        m_dspEngine->processAudioBlock(block);
        return;
    }

    QMetaObject::invokeMethod(
        m_dspEngine,
        "processAudioBlock",
        Qt::QueuedConnection,
        Q_ARG(AudioBlock, block)
        );
}



QString MainWindow::findFtAutoTestWavDirectory() const
{
    const QStringList requiredFiles = {
        QStringLiteral("websdr_test6.wav"),
        QStringLiteral("test_21.wav"),
        QStringLiteral("test_18.wav"),
        QStringLiteral("test_05.wav")
    };

    QStringList roots;
    roots << QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("tests/wav"));
    roots << QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../tests/wav"));
    roots << QDir::current().filePath(QStringLiteral("tests/wav"));
#ifdef MADMODEM_SOURCE_DIR
    roots << QDir(QString::fromUtf8(MADMODEM_SOURCE_DIR)).filePath(QStringLiteral("tests/wav"));
#endif

    QSet<QString> seen;
    for (const QString &rootPath : roots) {
        const QString absolute = QDir(rootPath).absolutePath();
        if (seen.contains(absolute)) {
            continue;
        }
        seen.insert(absolute);
        QDir dir(absolute);
        if (!dir.exists()) {
            continue;
        }
        bool complete = true;
        for (const QString &fileName : requiredFiles) {
            if (!QFileInfo::exists(dir.filePath(fileName))) {
                complete = false;
                break;
            }
        }
        if (complete) {
            return dir.absolutePath();
        }
    }

    return QString();
}

int MainWindow::ftAutoTestExpectedDecodes(const QString &fileName) const
{
    Q_UNUSED(fileName)
    // v4.10: the old hard-coded Expected values were never a verified WSJT-X
    // reference list.  Keep the column for parser compatibility but do not
    // pretend to know the true number of signals in a WAV.
    return 0;
}

QString MainWindow::ftAutoTestResultLabel(bool ok, int decodeCount, int expected) const
{
    if (!ok) {
        return QStringLiteral("FAIL");
    }
    if (expected > 0 && decodeCount >= expected) {
        return QStringLiteral("PASS");
    }
    if (decodeCount > 0) {
        return expected > 0 ? QStringLiteral("PARTIAL") : QStringLiteral("OK");
    }
    return QStringLiteral("FAIL");
}

int MainWindow::ftUnresolvedHashMarkerCount(const QString &message)
{
    return message.toUpper().count(QStringLiteral("<...>"));
}

int MainWindow::ftVisibleAngleCallCount(const QString &message)
{
    int count = 0;
    static const QRegularExpression angleCallRe(QStringLiteral(R"(<(?!\.\.\.>)[A-Z0-9/]{3,13}>)"));
    QRegularExpressionMatchIterator it = angleCallRe.globalMatch(message.toUpper());
    while (it.hasNext()) {
        it.next();
        ++count;
    }
    return count;
}

void MainWindow::runFtAutoTest()
{
    if (m_ftAutoTestRunning) {
        return;
    }

    if (m_txRunning || (m_txAudioEngine != nullptr && m_txAudioEngine->isRunning())) {
        QMessageBox::information(this,
                                 uiText("ft_auto_test", "Auto test"),
                                 uiText("ft_auto_test_stop_tx", "Stop TX before running the FT auto test."));
        return;
    }

    const QString wavDir = findFtAutoTestWavDirectory();
    if (wavDir.isEmpty()) {
        QMessageBox::warning(this,
                             uiText("ft_auto_test", "Auto test"),
                             uiText("ft_auto_test_missing_wav", "Bundled FT8 test WAV files were not found. Expected tests/wav beside the executable or in the source tree."));
        return;
    }

    m_ftAutoTestPreviousMode = (ui != nullptr && ui->cmbMode != nullptr) ? ui->cmbMode->currentText() : QString();
    if (ui != nullptr && ui->cmbMode != nullptr) {
        const int ft8Index = ui->cmbMode->findText(QStringLiteral("FT8"), Qt::MatchFixedString);
        if (ft8Index >= 0 && ui->cmbMode->currentIndex() != ft8Index && !m_rxRunning && !m_txRunning) {
            ui->cmbMode->setCurrentIndex(ft8Index);
        }
    }

    m_ftAutoTestRunning = true;
    m_ftAutoTestQueue.clear();
    m_ftAutoTestReportLines.clear();
    m_ftAutoTestIndex = 0;
    m_ftAutoTestPreviousDepth = m_settings.ft8LiveDecodeDepth.trimmed().toLower();
    if (m_ftAutoTestPreviousDepth == QStringLiteral("deep")) {
        m_ftAutoTestPreviousDepth = QStringLiteral("adaptive");
    }
    if (m_ftAutoTestPreviousDepth != QStringLiteral("fast") &&
        m_ftAutoTestPreviousDepth != QStringLiteral("adaptive")) {
        m_ftAutoTestPreviousDepth = QStringLiteral("adaptive");
    }
    m_ftAutoTestStartedUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    const QStringList wavFiles = {
        QStringLiteral("websdr_test6.wav"),
        QStringLiteral("test_21.wav"),
        QStringLiteral("test_18.wav"),
        QStringLiteral("test_05.wav")
    };

    for (const QString &fileName : wavFiles) {
        FtAutoTestItem unified;
        unified.filePath = QDir(wavDir).filePath(fileName);
        unified.fileName = fileName;
        unified.depthKey = QStringLiteral("unified");
        unified.depthLabel = QStringLiteral("Unified");
        m_ftAutoTestQueue.append(unified);
    }

    const DeepDspController::Status mindStatusForReport = m_ddspController != nullptr
        ? m_ddspController->status()
        : DeepDspController::Status();
    const QString mindAssistModeForReport = mindStatusForReport.assistMode.trimmed().toLower();
    const QString mindAssistDisplayForReport = (mindAssistModeForReport == QStringLiteral("shadow"))
        ? QStringLiteral("training")
        : (mindAssistModeForReport == QStringLiteral("assisted") ? QStringLiteral("active") : mindAssistModeForReport);
    const QString mindBenchmarkStatus = (mindAssistModeForReport == QStringLiteral("off") || !mindStatusForReport.enabled)
        ? QStringLiteral("bypassed")
        : (mindStatusForReport.modelStateText == QStringLiteral("Model loaded")
               ? QStringLiteral("model loaded")
               : QStringLiteral("model missing (data collection)"));
    m_ftAutoTestReportLines << QStringLiteral("MadModem FT8 Auto test report")
                            << QStringLiteral("UTC: %1").arg(m_ftAutoTestStartedUtc)
                            << QStringLiteral("WAV directory: %1").arg(wavDir)
                            << QStringLiteral("MIND Assist: %1").arg(mindAssistDisplayForReport)
                            << QStringLiteral("MIND status: %1").arg(mindBenchmarkStatus)
                            << QStringLiteral("MIND deferral: native candidate cooldown, no AutoTest-only freeze")
                            << QStringLiteral("MIND ultra-deep: Active opens wider weak/overlap candidate gates; Off remains bypassed")
                            << QStringLiteral("MIND model: %1").arg(mindStatusForReport.modelStateText)
                            << QStringLiteral("MIND ranker samples pos/neg: %1/%2")
                                   .arg(mindStatusForReport.rankerPositiveSamples)
                                   .arg(mindStatusForReport.rankerNegativeSamples)
                            << QStringLiteral("")
                            << QStringLiteral("File	Mode	OK	Reference	Decodes	Delta	Result	Candidates	Passes	Search ms	LDPC ms	Subtract ms	Total ms	Attempted	Sync gate	LDPC tried	LDPC fail	LDPC fail %	CRC fail	Unpack fail	Unpack fail %	Msg reject	Dedup drop	Unresolved hash	Visible hash calls	OK sync	LDPC-fail sync	OK hard	LDPC-fail hard	OK LLR	LDPC-fail LLR	OSD GF2 tried	OSD GF2 recovered	OSD GF2 rank fail	OSD GF2 pivot skips	OSD GF2 O0	OSD GF2 O1	OSD GF2 O2	OSD GF2 postCRC	OSD GF2 budget skip	OSD GF2 ms	MIND scored	MIND pruned	MIND extra	MIND unavailable	MIND avg success %	Summary");

    appendLog(QStringLiteral("FT Auto test started: %1 file(s), Unified adaptive engine, WAV directory %2")
                  .arg(wavFiles.size())
                  .arg(wavDir));

    if (m_ddspController != nullptr) {
        m_ddspController->setRuntimeMode(QStringLiteral("FT8"));
        // Do not freeze MIND specifically for AutoTest.  The benchmark must
        // exercise the normal native candidate-driven cooldown/deferral path.
    }

    runNextFtAutoTestStep();
}

void MainWindow::runNextFtAutoTestStep()
{
    if (!m_ftAutoTestRunning) {
        return;
    }

    if (m_ftAutoTestIndex >= m_ftAutoTestQueue.size()) {
        finishFtAutoTest();
        return;
    }

    const FtAutoTestItem item = m_ftAutoTestQueue.at(m_ftAutoTestIndex);
    if (!prepareForOfflineAnalysis(uiText("ft_auto_test", "Auto test"))) {
        m_ftAutoTestReportLines << QStringLiteral("ABORTED: unable to enter offline analysis mode");
        finishFtAutoTest();
        return;
    }

    if (m_btnFtDecodeAutoTest != nullptr) {
        m_btnFtDecodeAutoTest->setEnabled(false);
    }

    clearFt8DecodeList();
    resetDspEngine();
    if (m_waterfallWidget != nullptr) {
        m_waterfallWidget->clear();
    }

    m_settings.ft8LiveDecodeDepth = QStringLiteral("adaptive");
    m_settings.ft8DeepDecode = true;
    m_settings.ft8DspPlusDecode = true;
    if (m_cmbFtLiveDecodeDepth != nullptr) {
        const QSignalBlocker blockDepth(m_cmbFtLiveDecodeDepth);
        const int depthIndex = m_cmbFtLiveDecodeDepth->findData(QStringLiteral("adaptive"));
        if (depthIndex >= 0) {
            m_cmbFtLiveDecodeDepth->setCurrentIndex(depthIndex);
        }
    }
    applyFt8Settings();
    if (m_ft8RxDecoder != nullptr) {
        QMetaObject::invokeMethod(m_ft8RxDecoder, "setModeName", Qt::QueuedConnection, Q_ARG(QString, QStringLiteral("FT8")));
        QMetaObject::invokeMethod(m_ft8RxDecoder, "setDeepDecodeEnabled", Qt::QueuedConnection, Q_ARG(bool, true));
        QMetaObject::invokeMethod(m_ft8RxDecoder, "setDspPlusDecodeEnabled", Qt::QueuedConnection, Q_ARG(bool, true));
    }

    m_haveFt8PerfStats = false;
    m_ftAutoTestStepUnresolvedHashCount = 0;
    m_ftAutoTestStepVisibleAngleCallCount = 0;
    ui->lblAudioLevelDb->setText(QStringLiteral("WAV"));
    ui->lblEstimatedFrequency->setText(QStringLiteral("Freq: -- Hz"));
    ui->lblAppStatus->setText(uiText("ft_auto_test_running", "FT Auto test"));

    appendLog(QStringLiteral("FT Auto test %1/%2: %3 [%4]")
                  .arg(m_ftAutoTestIndex + 1)
                  .arg(m_ftAutoTestQueue.size())
                  .arg(item.fileName)
                  .arg(item.depthLabel));

    // MIND v2 learns from native FT candidate matrices emitted by Ft8RxDecoder.
    // No global WAV/audio fingerprint priming is used.

    if (m_ft8RxDecoder != nullptr) {
        QMetaObject::invokeMethod(m_ft8RxDecoder,
                                  "analyzeAudioFile",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, item.filePath));
    } else {
        m_ftAutoTestReportLines << QStringLiteral("%1\t%2\tNO\t%3\t0\t%4\tFAIL\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0.0\t0\t0\t0.0\t0\t0\t0\t0\t0.00\t0.00\t0.0\t0.0\t0.00\t0.00\t0\t0\t0\t0\t0\t0\t0\t0.0\tFT decoder unavailable")
                                   .arg(item.fileName)
                                   .arg(item.depthLabel)
                                   .arg(ftAutoTestExpectedDecodes(item.fileName))
                                   .arg(-ftAutoTestExpectedDecodes(item.fileName));
        ++m_ftAutoTestIndex;
        m_offlineAnalysisActive = false;
        setReceiverRunning(false);
        QTimer::singleShot(0, this, &MainWindow::runNextFtAutoTestStep);
    }
}

void MainWindow::finishFtAutoTest()
{
    const bool wasRunning = m_ftAutoTestRunning;
    m_ftAutoTestRunning = false;
    m_offlineAnalysisActive = false;

    if (!m_ftAutoTestPreviousDepth.isEmpty()) {
        m_settings.ft8LiveDecodeDepth = QStringLiteral("adaptive");
        m_settings.ft8DeepDecode = true;
        m_settings.ft8DspPlusDecode = true;
        if (m_cmbFtLiveDecodeDepth != nullptr) {
            const QSignalBlocker blockDepth(m_cmbFtLiveDecodeDepth);
            const int depthIndex = m_cmbFtLiveDecodeDepth->findData(QStringLiteral("adaptive"));
            if (depthIndex >= 0) {
                m_cmbFtLiveDecodeDepth->setCurrentIndex(depthIndex);
            }
        }
    }
    if (ui != nullptr && ui->cmbMode != nullptr && !m_ftAutoTestPreviousMode.isEmpty()) {
        const int previousModeIndex = ui->cmbMode->findText(m_ftAutoTestPreviousMode, Qt::MatchFixedString);
        if (previousModeIndex >= 0) {
            ui->cmbMode->setCurrentIndex(previousModeIndex);
        }
    }
    applyFt8Settings();

    setReceiverRunning(false);

    if (!wasRunning) {
        return;
    }

    QStringList reportTextLines;
    reportTextLines.reserve(m_ftAutoTestReportLines.size());
    for (const QString &line : m_ftAutoTestReportLines) {
        reportTextLines << line;
    }
    const QString reportText = reportTextLines.join(QStringLiteral("\n"));
    const QString safeStamp = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    const QString reportPath = QDir::home().filePath(QStringLiteral("MadModem_FT_AutoTest_%1.txt").arg(safeStamp));
    QFile reportFile(reportPath);
    if (reportFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QTextStream stream(&reportFile);
        stream << reportText << '\n';
        reportFile.close();
        appendLog(QStringLiteral("FT Auto test report saved: %1").arg(reportPath));
    } else {
        appendLog(QStringLiteral("FT Auto test report save failed: %1").arg(reportFile.errorString()));
    }

    appendLog(QStringLiteral("FT Auto test completed:"));
    for (const QString &line : m_ftAutoTestReportLines) {
        appendLog(QStringLiteral("  %1").arg(line));
    }

    QMessageBox::information(this,
                             uiText("ft_auto_test_done", "FT Auto test completed"),
                             uiText("ft_auto_test_done_message", "FT Auto test completed. The textual report was saved to:\n%1").arg(reportPath));
}

void MainWindow::openFtWavFile()
{
    if (!Ft8Mode::isFamilyMode(ui->cmbMode->currentText())) {
        QMessageBox::information(this,
                                 uiText("ft_wav_analysis", "FT WAV analysis"),
                                 uiText("ft_wav_select_ft_mode", "Select FT8 or FT4 before analyzing an FT WAV file."));
        return;
    }

    if (!prepareForOfflineAnalysis(uiText("ft_wav_analysis", "FT WAV analysis"))) {
        return;
    }

    const QString fileName = QFileDialog::getOpenFileName(
        this,
        uiText("ft_open_wav", "Open FT8/FT4 WAV test file"),
        QDir::homePath(),
        uiText("wav_audio_filter", "WAV audio (*.wav *.wave);;All files (*)")
        );

    if (fileName.isEmpty()) {
        m_offlineAnalysisActive = false;
        setReceiverRunning(false);
        return;
    }

    clearFt8DecodeList();
    resetDspEngine();
    if (m_waterfallWidget != nullptr) {
        m_waterfallWidget->clear();
    }

    applyFt8Settings();
    ui->lblAudioLevelDb->setText("WAV");
    ui->lblEstimatedFrequency->setText("Freq: -- Hz");
    ui->lblAppStatus->setText(uiText("ft_wav_analysis", "FT WAV analysis"));
    const Ft8Mode::Profile wavProfile = Ft8Mode::profileForMode(ui->cmbMode->currentText());
    QString wavDecodeMode = QStringLiteral("Unified adaptive");
    m_settings.ft8LiveDecodeDepth = QStringLiteral("adaptive");
    m_settings.ft8DeepDecode = true;
    m_settings.ft8DspPlusDecode = true;
    Q_UNUSED(wavProfile);
    appendLog(QStringLiteral("FT WAV analysis: %1 (%2, %3 decode)")
                  .arg(fileName)
                  .arg(ui->cmbMode->currentText())
                  .arg(wavDecodeMode));
    appendLog(QStringLiteral("FT WAV analysis is RX-only: PTT, slot scheduler and TX worker are not armed."));

    // MIND v2 learns from native FT candidate matrices emitted by Ft8RxDecoder.
    // No global WAV/audio fingerprint priming is used.
    if (m_ddspController != nullptr) {
        m_ddspController->setRuntimeMode(QStringLiteral("FT8"));
    }

    if (m_ft8RxDecoder != nullptr) {
        QMetaObject::invokeMethod(m_ft8RxDecoder,
                                  "analyzeAudioFile",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, fileName));
    } else {
        m_offlineAnalysisActive = false;
        setReceiverRunning(false);
    }
}

void MainWindow::handleFtOfflineAnalysisFinished(const QString &filePath, bool ok, int decodeCount, const QString &message)
{
    if (m_ftAutoTestRunning) {
        FtAutoTestItem item;
        if (m_ftAutoTestIndex >= 0 && m_ftAutoTestIndex < m_ftAutoTestQueue.size()) {
            item = m_ftAutoTestQueue.at(m_ftAutoTestIndex);
        } else {
            item.filePath = filePath;
            item.fileName = QFileInfo(filePath).fileName();
            item.depthLabel = QStringLiteral("?");
        }

        const bool haveStats = m_haveFt8PerfStats && m_lastFt8PerfStats.offline;
        const int candidates = haveStats ? m_lastFt8PerfStats.candidateCount : -1;
        const int passes = haveStats ? m_lastFt8PerfStats.passCount : 0;
        const double searchMs = haveStats ? m_lastFt8PerfStats.candidateSearchMs : 0.0;
        const double ldpcMs = haveStats ? m_lastFt8PerfStats.candidateDecodeMs : 0.0;
        const double subtractMs = haveStats ? m_lastFt8PerfStats.subtractionMs : 0.0;
        const double totalMs = haveStats ? m_lastFt8PerfStats.totalMs : 0.0;
        const int attempted = haveStats ? m_lastFt8PerfStats.attemptedCandidates : 0;
        const int syncGate = haveStats ? m_lastFt8PerfStats.syncGateRejects : 0;
        const int ldpcTried = haveStats ? m_lastFt8PerfStats.ldpcTried : 0;
        const int ldpcFail = haveStats ? m_lastFt8PerfStats.ldpcFailures : 0;
        const int crcFail = haveStats ? m_lastFt8PerfStats.crcFailures : 0;
        const int unpackFail = haveStats ? m_lastFt8PerfStats.unpackFailures : 0;
        const int messageReject = haveStats ? m_lastFt8PerfStats.messageRejects : 0;
        const int dedupDrop = haveStats ? m_lastFt8PerfStats.dedupDropped : 0;
        const double ldpcFailPct = ldpcTried > 0 ? (100.0 * static_cast<double>(ldpcFail) / static_cast<double>(ldpcTried)) : 0.0;
        const double unpackFailPct = attempted > 0 ? (100.0 * static_cast<double>(unpackFail) / static_cast<double>(attempted)) : 0.0;
        const double okSync = haveStats ? m_lastFt8PerfStats.decodedAvgSyncScore : 0.0;
        const double failSync = haveStats ? m_lastFt8PerfStats.ldpcFailureAvgSyncScore : 0.0;
        const double okHard = haveStats ? m_lastFt8PerfStats.decodedAvgHardSync : 0.0;
        const double failHard = haveStats ? m_lastFt8PerfStats.ldpcFailureAvgHardSync : 0.0;
        const double okLlr = haveStats ? m_lastFt8PerfStats.decodedAvgLlrAbs : 0.0;
        const double failLlr = haveStats ? m_lastFt8PerfStats.ldpcFailureAvgLlrAbs : 0.0;
        const int osdGf2Tried = haveStats ? m_lastFt8PerfStats.osdGf2Tried : 0;
        const int osdGf2Recovered = haveStats ? m_lastFt8PerfStats.osdGf2Recovered : 0;
        const int osdGf2RankFails = haveStats ? m_lastFt8PerfStats.osdGf2RankFails : 0;
        const int osdGf2PivotSkips = haveStats ? m_lastFt8PerfStats.osdGf2PivotSkips : 0;
        const int osdGf2Order0 = haveStats ? m_lastFt8PerfStats.osdGf2Order0Hits : 0;
        const int osdGf2Order1 = haveStats ? m_lastFt8PerfStats.osdGf2Order1Hits : 0;
        const int osdGf2Order2 = haveStats ? m_lastFt8PerfStats.osdGf2Order2Hits : 0;
        const int osdGf2PostCrcRejects = haveStats ? m_lastFt8PerfStats.osdGf2PostCrcRejects : 0;
        const int osdGf2BudgetSkips = haveStats ? m_lastFt8PerfStats.osdGf2BudgetSkips : 0;
        const double osdGf2Ms = haveStats ? m_lastFt8PerfStats.osdGf2TotalMs : 0.0;
        const int mindScored = haveStats ? m_lastFt8PerfStats.mindAssistTried : 0;
        const int mindPruned = haveStats ? m_lastFt8PerfStats.mindAssistRecovered : 0;
        const int mindExtra = haveStats ? m_lastFt8PerfStats.mindAssistExtraDecodes : 0;
        const int mindUnavailable = haveStats ? m_lastFt8PerfStats.mindAssistUnavailable : 0;
        const double mindAvg = haveStats ? m_lastFt8PerfStats.mindAssistAvgConfidence : 0.0;
        const int expected = ftAutoTestExpectedDecodes(item.fileName);
        const int delta = (expected > 0) ? (decodeCount - expected) : 0;
        const QString result = ftAutoTestResultLabel(ok, decodeCount, expected);
        const QString compactMessage = message.simplified();
        const QString line = QStringLiteral("%1	%2	%3	%4	%5	%6	%7	%8	%9	%10	%11	%12	%13	%14	%15	%16	%17	%18	%19	%20	%21	%22	%23	%24	%25	%26	%27	%28	%29	%30	%31	%32	%33	%34	%35	%36	%37	%38	%39	%40	%41	%42	%43	%44	%45	%46	%47")
                                 .arg(item.fileName)
                                 .arg(item.depthLabel)
                                 .arg(ok ? QStringLiteral("YES") : QStringLiteral("NO"))
                                 .arg(expected)
                                 .arg(decodeCount)
                                 .arg(delta)
                                 .arg(result)
                                 .arg(candidates)
                                 .arg(passes)
                                 .arg(QString::number(searchMs, 'f', 0))
                                 .arg(QString::number(ldpcMs, 'f', 0))
                                 .arg(QString::number(subtractMs, 'f', 0))
                                 .arg(QString::number(totalMs, 'f', 0))
                                 .arg(attempted)
                                 .arg(syncGate)
                                 .arg(ldpcTried)
                                 .arg(ldpcFail)
                                 .arg(QString::number(ldpcFailPct, 'f', 1))
                                 .arg(crcFail)
                                 .arg(unpackFail)
                                 .arg(QString::number(unpackFailPct, 'f', 1))
                                 .arg(messageReject)
                                 .arg(dedupDrop)
                                 .arg(m_ftAutoTestStepUnresolvedHashCount)
                                 .arg(m_ftAutoTestStepVisibleAngleCallCount)
                                 .arg(QString::number(okSync, 'f', 2))
                                 .arg(QString::number(failSync, 'f', 2))
                                 .arg(QString::number(okHard, 'f', 1))
                                 .arg(QString::number(failHard, 'f', 1))
                                 .arg(QString::number(okLlr, 'f', 2))
                                 .arg(QString::number(failLlr, 'f', 2))
                                 .arg(osdGf2Tried)
                                 .arg(osdGf2Recovered)
                                 .arg(osdGf2RankFails)
                                 .arg(osdGf2PivotSkips)
                                 .arg(osdGf2Order0)
                                 .arg(osdGf2Order1)
                                 .arg(osdGf2Order2)
                                 .arg(osdGf2PostCrcRejects)
                                  .arg(osdGf2BudgetSkips)
                                  .arg(QString::number(osdGf2Ms, 'f', 1))
                                  .arg(mindScored)
                                  .arg(mindPruned)
                                  .arg(mindExtra)
                                  .arg(mindUnavailable)
                                  .arg(QString::number(mindAvg, 'f', 1))
                                  .arg(compactMessage);
        m_ftAutoTestReportLines << line;
        appendLog(QStringLiteral("FT Auto test result: %1").arg(line));

        m_offlineAnalysisActive = false;
        setReceiverRunning(false);
        ++m_ftAutoTestIndex;
        QTimer::singleShot(0, this, &MainWindow::runNextFtAutoTestStep);
        return;
    }

    Q_UNUSED(decodeCount)
    m_offlineAnalysisActive = false;
    setReceiverRunning(false);
    ui->lblAppStatus->setText(ok ? uiText("ready", "Ready") : uiText("ft_wav_failed", "FT WAV failed"));

    if (ok) {
        appendLog(QStringLiteral("FT WAV analysis completed: %1 — %2").arg(filePath, message));
    } else {
        appendLog(QStringLiteral("FT WAV analysis failed: %1 — %2").arg(filePath, message));
        QMessageBox::warning(this,
                             uiText("ft_wav_failed", "FT WAV analysis failed"),
                             message);
    }
}

void MainWindow::openWeatherFaxWavFile()
{
    if (!prepareForOfflineAnalysis("WEFAX WAV analysis")) {
        return;
    }

    const QString fileName = QFileDialog::getOpenFileName(
        this,
        "Open WEFAX WAV test file",
        QDir::homePath(),
        "WAV audio (*.wav *.wave);;All files (*)"
        );

    if (fileName.isEmpty()) {
        m_offlineAnalysisActive = false;
        setReceiverRunning(false);
        return;
    }

    if (!analyzeWeatherFaxWavFile(fileName)) {
        QMessageBox::warning(
            this,
            "Analyze WEFAX WAV failed",
            "Unable to analyze the selected WAV file. See the log panel for details."
            );
    }
}

bool MainWindow::analyzeWeatherFaxWavFile(const QString &fileName)
{
    QFile file(fileName);

    if (!file.open(QIODevice::ReadOnly)) {
        appendLog("WAV open failed: " + file.errorString());
        m_offlineAnalysisActive = false;
        setReceiverRunning(false);
        return false;
    }

    WavStreamFormat wav;
    QString errorMessage;

    if (!parseWavHeader(file, wav, errorMessage)) {
        appendLog("WAV parse failed: " + errorMessage);
        m_offlineAnalysisActive = false;
        setReceiverRunning(false);
        return false;
    }

    const bool restoreAutoStartAfterWav = ui->chkFaxAutoStartPhasing->isChecked();

    applyWeatherFaxSettings();

    /*
     * v45/v46 offline policy: a WAV test file is usually already cut at the
     * wanted image start.  Waiting for a guessed image/control pattern and
     * then clearing the decoder buffer starts reception mid-line, which causes
     * the visible wrap/fold reported in v43.  For WAV analysis we therefore
     * decode free-running from sample zero while preserving the live-RX APT
     * setting in the UI.
     */
    m_weatherFaxDecoder->setAutoStartEnabled(false);

    resetDspEngine();
    m_decoderConditioner.reset();

    if (m_waterfallWidget != nullptr) {
        m_waterfallWidget->clear();
    }

    if (m_faxImageWidget != nullptr) {
        m_faxImageWidget->clear();
    }

    m_weatherFaxDecoder->reset();

    if (ui->progressAudioLevel != nullptr) ui->progressAudioLevel->setValue(0);
    if (m_ledVuMeter != nullptr) {
        m_ledVuMeter->setLevelPercent(0);
        m_ledVuMeter->setDbText(QStringLiteral("WAV"));
    }
    if (ui->lblAudioLevelDb != nullptr) ui->lblAudioLevelDb->setText("WAV");
    if (m_lblVuMeterDb != nullptr) m_lblVuMeterDb->setText("WAV");
    ui->lblEstimatedFrequency->setText("Freq: -- Hz");
    ui->lblAppStatus->setText("WAV analysis");

    appendLog("Analyzing WEFAX WAV: " + fileName);
    appendLog(QString("WAV format: %1 Hz, %2 channel(s), %3-bit, %4 byte(s).")
                  .arg(wav.sampleRate)
                  .arg(wav.channels)
                  .arg(wav.bitsPerSample)
                  .arg(wav.dataSize));
    appendLog("WEFAX WAV timing: manual/free-run from first sample, with early line-sync stripe lock; APT start is used only for live RX.");

    const int framesPerChunk = 4096;
    const qint64 preferredBytes =
        static_cast<qint64>(framesPerChunk) * static_cast<qint64>(wav.blockAlign);

    qint64 remainingBytes = wav.dataSize;
    qint64 firstSampleIndex = 0;
    int chunkCounter = 0;

    QProgressDialog progress(
        "Analyzing WEFAX WAV...",
        "Cancel",
        0,
        100,
        this
        );
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setMinimumDuration(300);
    progress.setValue(0);

    while (remainingBytes > 0) {
        qint64 bytesToRead = qMin(remainingBytes, preferredBytes);
        bytesToRead -= bytesToRead % static_cast<qint64>(wav.blockAlign);

        if (bytesToRead <= 0) {
            break;
        }

        const QByteArray raw = file.read(bytesToRead);

        if (raw.isEmpty()) {
            appendLog("WAV analysis stopped: unexpected end of file.");
            m_weatherFaxDecoder->setAutoStartEnabled(restoreAutoStartAfterWav);
            ui->lblAppStatus->setText("Ready");
            m_offlineAnalysisActive = false;
            setReceiverRunning(false);
            return false;
        }

        const QVector<float> samples = convertWavBytesToMono(raw, wav);

        if (!samples.isEmpty()) {
            AudioBlock block;
            block.samples = samples;
            block.sampleRate = static_cast<int>(wav.sampleRate);
            block.firstSampleIndex = firstSampleIndex;

            processDspAudioBlockForWav(block);
            const AudioBlock conditionedBlock = conditionAudioForActiveMode(block);
            m_weatherFaxDecoder->processAudioBlock(conditionedBlock);

            firstSampleIndex += samples.size();
        }

        remainingBytes -= raw.size();
        ++chunkCounter;

        if ((chunkCounter % 16) == 0 || remainingBytes == 0) {
            const int percent = static_cast<int>(
                qBound<qint64>(
                    qint64{0},
                    ((wav.dataSize - remainingBytes) * qint64{100}) / qMax<qint64>(qint64{1}, wav.dataSize),
                    qint64{100}
                    )
                );

            progress.setValue(percent);
            QCoreApplication::processEvents();

            if (progress.wasCanceled()) {
                appendLog("WAV analysis cancelled by user.");
                m_weatherFaxDecoder->setAutoStartEnabled(restoreAutoStartAfterWav);
                ui->lblAppStatus->setText("Ready");
                m_offlineAnalysisActive = false;
                setReceiverRunning(false);
                return false;
            }
        }
    }

    progress.setValue(100);

    m_weatherFaxDecoder->finishCurrentImage("end of WAV file");
    m_weatherFaxDecoder->setAutoStartEnabled(restoreAutoStartAfterWav);

    if (m_faxImageWidget != nullptr) {
        m_faxImageWidget->setImage(m_weatherFaxDecoder->currentImage());
    }

    m_offlineAnalysisActive = false;
    setReceiverRunning(false);

    appendLog(QString("WAV analysis completed: %1 sample(s) processed.")
                  .arg(firstSampleIndex));

    return true;
}

void MainWindow::openSstvWavFile()
{
    if (!prepareForOfflineAnalysis("SSTV WAV analysis")) {
        return;
    }

    const QString fileName = QFileDialog::getOpenFileName(
        this,
        "Open SSTV WAV test file",
        QDir::homePath(),
        "WAV audio (*.wav *.wave);;All files (*)"
        );

    if (fileName.isEmpty()) {
        m_offlineAnalysisActive = false;
        setReceiverRunning(false);
        return;
    }

    if (!analyzeSstvWavFile(fileName)) {
        QMessageBox::warning(
            this,
            "Analyze SSTV WAV failed",
            "Unable to analyze the selected WAV file. See the log panel for details."
            );
    }
}

bool MainWindow::analyzeSstvWavFile(const QString &fileName)
{
    QFile file(fileName);

    if (!file.open(QIODevice::ReadOnly)) {
        appendLog("SSTV WAV open failed: " + file.errorString());
        m_offlineAnalysisActive = false;
        setReceiverRunning(false);
        return false;
    }

    WavStreamFormat wav;
    QString errorMessage;

    if (!parseWavHeader(file, wav, errorMessage)) {
        appendLog("SSTV WAV parse failed: " + errorMessage);
        m_offlineAnalysisActive = false;
        setReceiverRunning(false);
        return false;
    }

    applySstvSettings();

    resetDspEngine();
    m_decoderConditioner.reset();

    if (m_waterfallWidget != nullptr) {
        m_waterfallWidget->clear();
    }

    if (m_faxImageWidget != nullptr) {
        m_faxImageWidget->clear();
    }

    m_sstvDecoder->reset();

    if (ui->progressAudioLevel != nullptr) ui->progressAudioLevel->setValue(0);
    if (m_ledVuMeter != nullptr) {
        m_ledVuMeter->setLevelPercent(0);
        m_ledVuMeter->setDbText(QStringLiteral("WAV"));
    }
    if (ui->lblAudioLevelDb != nullptr) ui->lblAudioLevelDb->setText("WAV");
    if (m_lblVuMeterDb != nullptr) m_lblVuMeterDb->setText("WAV");
    ui->lblEstimatedFrequency->setText("Freq: -- Hz");
    ui->lblAppStatus->setText("SSTV WAV analysis");

    appendLog("Analyzing SSTV WAV: " + fileName);
    appendLog(QString("WAV format: %1 Hz, %2 channel(s), %3-bit, %4 byte(s).")
                  .arg(wav.sampleRate)
                  .arg(wav.channels)
                  .arg(wav.bitsPerSample)
                  .arg(wav.dataSize));

    const int framesPerChunk = 4096;
    const qint64 preferredBytes =
        static_cast<qint64>(framesPerChunk) * static_cast<qint64>(wav.blockAlign);

    qint64 remainingBytes = wav.dataSize;
    qint64 firstSampleIndex = 0;
    int chunkCounter = 0;

    QProgressDialog progress(
        "Analyzing SSTV WAV...",
        "Cancel",
        0,
        100,
        this
        );
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setMinimumDuration(300);
    progress.setValue(0);

    while (remainingBytes > 0) {
        qint64 bytesToRead = qMin(remainingBytes, preferredBytes);
        bytesToRead -= bytesToRead % static_cast<qint64>(wav.blockAlign);

        if (bytesToRead <= 0) {
            break;
        }

        const QByteArray raw = file.read(bytesToRead);

        if (raw.isEmpty()) {
            appendLog("SSTV WAV analysis stopped: unexpected end of file.");
            ui->lblAppStatus->setText("Ready");
            m_offlineAnalysisActive = false;
            setReceiverRunning(false);
            return false;
        }

        const QVector<float> samples = convertWavBytesToMono(raw, wav);

        if (!samples.isEmpty()) {
            AudioBlock block;
            block.samples = samples;
            block.sampleRate = static_cast<int>(wav.sampleRate);
            block.firstSampleIndex = firstSampleIndex;

            processDspAudioBlockForWav(block);
            const AudioBlock conditionedBlock = conditionAudioForActiveMode(block);
            m_sstvDecoder->processAudioBlock(conditionedBlock);

            firstSampleIndex += samples.size();
        }

        remainingBytes -= raw.size();
        ++chunkCounter;

        if ((chunkCounter % 16) == 0 || remainingBytes == 0) {
            const int percent = static_cast<int>(
                qBound<qint64>(
                    qint64{0},
                    ((wav.dataSize - remainingBytes) * qint64{100}) / qMax<qint64>(qint64{1}, wav.dataSize),
                    qint64{100}
                    )
                );

            progress.setValue(percent);
            QCoreApplication::processEvents();

            if (progress.wasCanceled()) {
                appendLog("SSTV WAV analysis cancelled by user.");
                ui->lblAppStatus->setText("Ready");
                m_offlineAnalysisActive = false;
                setReceiverRunning(false);
                return false;
            }
        }
    }

    progress.setValue(100);

    if (m_faxImageWidget != nullptr) {
        m_faxImageWidget->setImage(m_sstvDecoder->currentImage());
    }

    m_offlineAnalysisActive = false;
    setReceiverRunning(false);

    appendLog(QString("SSTV WAV analysis completed: %1 sample(s) processed.")
                  .arg(firstSampleIndex));

    return true;
}

void MainWindow::forceSstvManualRx()
{
    if (m_sstvDecoder == nullptr || m_txRunning || m_offlineAnalysisActive) {
        return;
    }

    if (ui->cmbMode != nullptr && ui->cmbMode->currentText() != SstvDecoder::modeName()) {
        requestModeChange(SstvDecoder::modeName());
    }

    if (ui->chkSstvAutoSync != nullptr) {
        ui->chkSstvAutoSync->setChecked(false);
    }
    applySstvSettings();
    m_sstvDecoder->setAutoSyncEnabled(false);
    m_sstvDecoder->reset();
    if (m_faxImageWidget != nullptr) {
        m_faxImageWidget->setImage(m_sstvDecoder->currentImage());
    }

    appendLog(uiText("log.forceSstvRx", "Forced SSTV manual RX: sync wait bypassed."));
    if (ui->lblAppStatus != nullptr) {
        ui->lblAppStatus->setText(uiText("status.forceSstvRx", "SSTV manual RX forced"));
    }

    if (!m_rxRunning && m_audioEngine != nullptr && !m_audioEngine->isRunning()) {
        startRx();
    }
}

void MainWindow::resetSstvImage()
{
    if (m_sstvDecoder == nullptr) {
        return;
    }

    m_sstvDecoder->reset();

    if (m_faxImageWidget != nullptr) {
        m_faxImageWidget->setImage(m_sstvDecoder->currentImage());
    }

    appendLog("SSTV image reset.");
}

void MainWindow::saveSstvImage()
{
    if (m_sstvDecoder == nullptr) {
        return;
    }

    const QImage image = m_sstvDecoder->currentImage();

    if (image.isNull()) {
        appendLog("Save PNG failed: no SSTV image available.");
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(
        this,
        "Save SSTV image",
        QDir::home().filePath("SSTV.png"),
        "PNG image (*.png)"
        );

    if (fileName.isEmpty()) {
        return;
    }

    if (!fileName.endsWith(".png", Qt::CaseInsensitive)) {
        fileName += ".png";
    }

    if (!image.save(fileName, "PNG")) {
        QMessageBox::warning(this,
                             "Save PNG failed",
                             "Unable to save the SSTV image.");
        return;
    }

    appendLog("SSTV image saved: " + fileName);
}

void MainWindow::forceWeatherFaxManualRx()
{
    if (m_weatherFaxDecoder == nullptr || m_txRunning || m_offlineAnalysisActive) {
        return;
    }

    if (ui->cmbMode != nullptr && ui->cmbMode->currentText() != WeatherFaxDecoder::modeName()) {
        requestModeChange(WeatherFaxDecoder::modeName());
    }

    if (ui->chkFaxAutoStartPhasing != nullptr) {
        ui->chkFaxAutoStartPhasing->setChecked(false);
    }
    applyWeatherFaxSettings();
    m_weatherFaxDecoder->setAutoStartEnabled(false);
    m_weatherFaxDecoder->reset();
    if (m_faxImageWidget != nullptr) {
        m_faxImageWidget->clear();
    }

    appendLog(uiText("log.forceWefaxRx", "Forced WEFAX manual RX: APT start wait bypassed."));
    if (ui->lblAppStatus != nullptr) {
        ui->lblAppStatus->setText(uiText("status.forceWefaxRx", "WEFAX manual RX forced"));
    }

    if (!m_rxRunning && m_audioEngine != nullptr && !m_audioEngine->isRunning()) {
        startRx();
    }
}

void MainWindow::resetWeatherFaxImage()
{
    if (m_weatherFaxDecoder == nullptr) {
        return;
    }

    m_weatherFaxDecoder->reset();

    if (m_faxImageWidget != nullptr) {
        m_faxImageWidget->setImage(m_weatherFaxDecoder->currentImage());
    }

    appendLog("MeteoFax image reset.");
}

void MainWindow::saveWeatherFaxImage()
{
    if (m_weatherFaxDecoder == nullptr) {
        return;
    }

    const QImage image = m_weatherFaxDecoder->currentImage();

    if (image.isNull()) {
        appendLog("Save PNG failed: no MeteoFax image available.");
        return;
    }

    const QString defaultName = makeWeatherFaxAutoSaveFileName();

    QString fileName = QFileDialog::getSaveFileName(
        this,
        "Save MeteoFax image",
        defaultName,
        "PNG image (*.png)"
        );

    if (fileName.isEmpty()) {
        return;
    }

    if (!fileName.endsWith(".png", Qt::CaseInsensitive)) {
        fileName += ".png";
    }

    if (!saveWeatherFaxImageToFile(image, fileName)) {
        QMessageBox::warning(this,
                             "Save PNG failed",
                             "Unable to save the MeteoFax image.");
    }
}



mm::CatRotatorController::Config MainWindow::catRotatorConfigFromSettings() const
{
    auto normalizeBand = [](QString band) {
        return band.trimmed().toLower().remove(QChar(' '));
    };
    auto profileMatchesBand = [&](const AppSettings::RotatorProfileSettings &profile, const QString &band) {
        const QString wanted = normalizeBand(band);
        if (wanted.isEmpty()) {
            return false;
        }
        const QStringList parts = profile.bandsCsv.split(QRegularExpression(QStringLiteral("[\\s,;]+")), Qt::SkipEmptyParts);
        for (QString part : parts) {
            part = normalizeBand(part);
            if (part == wanted) return true;
        }
        return false;
    };

    QString currentBand;
    if (ui != nullptr && ui->cmbMode != nullptr && Ft8Mode::isFamilyMode(ui->cmbMode->currentText()) && m_cmbFt8Band != nullptr) {
        currentBand = m_cmbFt8Band->currentText();
    }
    if (currentBand.trimmed().isEmpty() && m_lastRigFrequencyHz > 0.0) {
        currentBand = bandFromFrequencyHz(m_lastRigFrequencyHz);
    }
    if (currentBand.trimmed().isEmpty()) {
        currentBand = m_settings.ft8Band;
    }

    const bool currentBandKnown = !normalizeBand(currentBand).isEmpty();
    bool profileMatchedCurrentBand = false;
    int selected = -1;
    for (int i = 0; i < 3; ++i) {
        if (profileMatchesBand(m_settings.rotatorProfiles[i], currentBand)) {
            selected = i;
            profileMatchedCurrentBand = true;
            break;
        }
    }

    // Do not silently fall back to the active profile when the current band is
    // known but no rotator profile is assigned to it. Example: a 2 m rotator
    // must not move when the radio/app is on 20 m. Manual Connect remains
    // blocked too; the panel shows cfg.disabledReason as a clear warning.
    const bool rotatorAllowedForCurrentBand = !currentBandKnown || profileMatchedCurrentBand;
    if (selected < 0) {
        selected = qBound(0, m_settings.rotatorActiveProfile, 2);
    }

    const AppSettings::RotatorProfileSettings &profile = m_settings.rotatorProfiles[selected];
    mm::CatRotatorController::Config cfg;
    cfg.enabled = m_settings.rotatorEnabled && rotatorAllowedForCurrentBand;
    if (m_settings.rotatorEnabled && !rotatorAllowedForCurrentBand) {
        cfg.disabledReason = uiText("rotator_disabled_for_current_band",
                                    "Rotator: disabled for current band %1")
                                 .arg(currentBand.trimmed().isEmpty() ? QStringLiteral("--") : currentBand.trimmed());
    }
    cfg.stationIdentityOk = stationIdentityReady(nullptr);
    cfg.autoConnect = m_settings.rotatorAutoConnect;
    cfg.showWindowOnStart = false;
    cfg.profileIndex = selected;
    cfg.label = profile.label.trimmed().isEmpty() ? QStringLiteral("Rotator %1").arg(selected + 1) : profile.label.trimmed();
    cfg.bandsCsv = profile.bandsCsv;
    cfg.hamlibModel = qMax(1, profile.hamlibModel);
    cfg.path = profile.path;
    cfg.baudRate = qBound(300, profile.baudRate, 1000000);
    cfg.pollIntervalMs = qBound(250, profile.pollIntervalMs, 10000);
    cfg.useElevation = profile.useElevation;
    cfg.overlap = profile.overlap;
    cfg.azimuthGeometryPreset = profile.azimuthGeometryPreset;
    cfg.azimuthStopDeg = profile.azimuthStopDeg;
    cfg.autoReverseOnStall = profile.autoReverseOnStall;
    cfg.noMovementTimeoutMs = profile.noMovementTimeoutMs;
    cfg.noMovementThresholdDeg = profile.noMovementThresholdDeg;
    cfg.parkAzimuth = qBound(0.0, profile.parkAzimuth, 359.9);
    cfg.parkElevation = qBound(-10.0, profile.parkElevation, 180.0);
    cfg.trackSelectedQso = m_settings.rotatorTrackSelectedQso;
    cfg.trackOnlyWhenQsoActive = m_settings.rotatorTrackOnlyWhenQsoActive;
    cfg.targetToleranceDeg = qBound(0, profile.targetToleranceDeg, 45);
    cfg.azimuthMinDeg = profile.azimuthMinDeg;
    cfg.azimuthMaxDeg = profile.azimuthMaxDeg;
    cfg.elevationMinDeg = profile.elevationMinDeg;
    cfg.elevationMaxDeg = profile.elevationMaxDeg;
    cfg.azimuthMsPerDeg = profile.azimuthMsPerDeg;
    cfg.elevationMsPerDeg = profile.elevationMsPerDeg;
    cfg.startupDelayMs = profile.startupDelayMs;
    cfg.settleDelayMs = profile.settleDelayMs;
    cfg.txGuardMarginMs = profile.txGuardMarginMs;
    cfg.calibrationStampUtc = profile.calibrationStampUtc;

    QPointF homeLonLat;
    if (QsoMapWidget::maidenheadToLonLat(stationLocator(), &homeLonLat)) {
        cfg.homeCoordinatesValid = true;
        cfg.homeLongitudeDeg = homeLonLat.x();
        cfg.homeLatitudeDeg = homeLonLat.y();
        cfg.homeAltitudeM = 0.0;
    }
    return cfg;
}
void MainWindow::setupCatRotatorModule()
{
    if (m_catRotatorController == nullptr) {
        m_catRotatorController = new mm::CatRotatorController(this);
        connect(m_catRotatorController, &mm::CatRotatorController::statusChanged, this, [this](const QString &text) {
            if (!text.trimmed().isEmpty()) {
                appendLog(QStringLiteral("CatRotator: ") + text.trimmed());
            }
        });
        connect(m_catRotatorController, &mm::CatRotatorController::connectionChanged,
                this, [this](bool connected) {
            if (m_settings.ftAutoQsoFlowShadowMode) {
                appendLog(QStringLiteral("[Flow][eventbus] Event: rotator.connection -> %1")
                    .arg(connected ? QStringLiteral("connected") : QStringLiteral("disconnected")));
            }
        });
        connect(m_catRotatorController, &mm::CatRotatorController::positionChanged,
                this, [this](double azimuthDeg, double elevationDeg) {
            if (m_settings.ftAutoQsoFlowShadowMode) {
                static qint64 s_lastRotatorPositionLogMs = 0;
                static double s_lastRotatorPositionLogAz = -9999.0;
                static double s_lastRotatorPositionLogEl = -9999.0;
                const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
                const bool first = s_lastRotatorPositionLogMs <= 0;
                const bool changedEnough = qAbs(azimuthDeg - s_lastRotatorPositionLogAz) >= 10.0 ||
                                           qAbs(elevationDeg - s_lastRotatorPositionLogEl) >= 5.0;
                const bool timeDue = (nowMs - s_lastRotatorPositionLogMs) >= 10000;
                if (first || changedEnough || timeDue) {
                    s_lastRotatorPositionLogMs = nowMs;
                    s_lastRotatorPositionLogAz = azimuthDeg;
                    s_lastRotatorPositionLogEl = elevationDeg;
                    appendLog(QStringLiteral("[Flow][eventbus] Event: rotator.position -> az %1°, el %2°")
                        .arg(QString::number(azimuthDeg, 'f', 1), QString::number(elevationDeg, 'f', 1)));
                }
            }
        });
        connect(m_catRotatorController, &mm::CatRotatorController::targetChanged,
                this, [this](double azimuthDeg, double elevationDeg, const QString &reason) {
            if (m_settings.ftAutoQsoFlowShadowMode) {
                appendLog(QStringLiteral("[Flow][eventbus] Event: rotator.target -> az %1°, el %2°%3")
                    .arg(QString::number(azimuthDeg, 'f', 1),
                         QString::number(elevationDeg, 'f', 1),
                         reason.trimmed().isEmpty() ? QString() : QStringLiteral(" — ") + reason.trimmed()));
            }
        });
        connect(m_catRotatorController, &mm::CatRotatorController::motionChanged,
                this, [this](bool moving) {
            if (m_settings.ftAutoQsoFlowShadowMode) {
                appendLog(QStringLiteral("[Flow][eventbus] Event: rotator.motion -> %1")
                    .arg(moving ? QStringLiteral("moving") : QStringLiteral("stopped/ready")));
            }
        });
        connect(m_catRotatorController, &mm::CatRotatorController::calibrationProgress,
                this, [this](int percent, const QString &message) {
            if (!message.trimmed().isEmpty()) {
                appendLog(QStringLiteral("CatRotator calibration %1%: %2").arg(percent).arg(message.trimmed()));
            }
        });
        connect(m_catRotatorController, &mm::CatRotatorController::calibrationFinished,
                this, [this](const mm::CatRotatorController::Config &cfg, const QString &message) {
            const int profile = qBound(0, cfg.profileIndex, 2);
            AppSettings::RotatorProfileSettings &rp = m_settings.rotatorProfiles[profile];
            rp.azimuthMsPerDeg = cfg.azimuthMsPerDeg;
            rp.elevationMsPerDeg = cfg.elevationMsPerDeg;
            rp.startupDelayMs = cfg.startupDelayMs;
            rp.settleDelayMs = cfg.settleDelayMs;
            rp.txGuardMarginMs = cfg.txGuardMarginMs;
            rp.calibrationStampUtc = cfg.calibrationStampUtc;
            savePersistentSettings();
            if (m_catRotatorSidePanel != nullptr) m_catRotatorSidePanel->applyConfig(cfg);
            if (!message.trimmed().isEmpty()) appendLog(QStringLiteral("CatRotator: ") + message.trimmed());
        });
    }
    applyCatRotatorSettings();
}

void MainWindow::setupCatRotatorSideTab()
{
    if (ui == nullptr || ui->sideTabWidget == nullptr || m_tabCatRotator != nullptr) {
        return;
    }
    if (m_catRotatorController == nullptr) {
        setupCatRotatorModule();
    }

    m_tabCatRotator = new QWidget(ui->sideTabWidget);
    QVBoxLayout *layout = new QVBoxLayout(m_tabCatRotator);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    m_catRotatorSidePanel = new mm::CatRotatorPanel(m_catRotatorController, m_tabCatRotator);
    m_catRotatorSidePanel->applyConfig(catRotatorConfigFromSettings());
    connect(m_catRotatorSidePanel, &mm::CatRotatorPanel::requestDirectTarget,
            this, &MainWindow::pointCatRotatorToDirectTarget);
    layout->addWidget(m_catRotatorSidePanel, 2);

    ui->sideTabWidget->insertTab(ui->sideTabWidget->count(), m_tabCatRotator, uiText("tab_rotator", "Rotator"));
    if (ui->sideTabWidget->tabBar() != nullptr) {
        const int idx = ui->sideTabWidget->indexOf(m_tabCatRotator);
        if (idx >= 0) {
            ui->sideTabWidget->tabBar()->setTabVisible(idx, true);
        }
    }
}


void MainWindow::applyCatRotatorSettings()
{
    if (m_catRotatorController == nullptr) {
        return;
    }
    const mm::CatRotatorController::Config cfg = catRotatorConfigFromSettings();
    m_catRotatorController->configure(cfg);
    m_catRotatorController->setTrackingQsoTarget(cfg.trackSelectedQso);
    if (m_catRotatorSidePanel != nullptr) {
        m_catRotatorSidePanel->applyConfig(cfg);
    }
    if (ui != nullptr && ui->sideTabWidget != nullptr && m_tabCatRotator != nullptr && ui->sideTabWidget->tabBar() != nullptr) {
        const int idx = ui->sideTabWidget->indexOf(m_tabCatRotator);
        if (idx >= 0) ui->sideTabWidget->tabBar()->setTabVisible(idx, true);
    }
    updateCatRotatorQsoTarget(QStringLiteral("settings applied"));
}


void MainWindow::pointCatRotatorToDirectTarget(const QString &query)
{
    if (m_catRotatorController == nullptr) {
        setupCatRotatorModule();
    }
    if (m_catRotatorController == nullptr) {
        return;
    }

    QString identityReason;
    if (!stationIdentityReady(&identityReason)) {
        appendLog(uiText("rotator_identity_required_log",
                         "Rotator command blocked: set a valid My Call and My Locator in Settings -> User/QTH first."));
        QMessageBox::warning(this,
                             uiText("station_identity_required", "Station identity required"),
                             uiText("rotator_identity_required_text",
                                    "Set a valid My Call and My Locator in Settings -> User/QTH before moving the rotator.") + QStringLiteral("\n\n") + identityReason);
        return;
    }

    const QString homeGrid = stationLocator();
    QPointF homeLonLat;
    QsoMapWidget::maidenheadToLonLat(homeGrid, &homeLonLat);

    QString q = query.trimmed();
    if (q.isEmpty()) {
        return;
    }

    QPointF targetLonLat;
    QString label = q;
    QString targetGrid;
    bool ok = false;

    if (QsoMapWidget::maidenheadToLonLat(q, &targetLonLat)) {
        ok = true;
        targetGrid = q.trimmed().toUpper();
        label = targetGrid;
    } else {
        const CtyCountryFile::LookupResult cty = CtyCountryFile::instance().lookupEntityNameOrPrefix(q);
        if (cty.valid) {
            targetLonLat = QPointF(cty.entity.longitude, cty.entity.latitude);
            targetGrid = cty.entity.referenceGrid.left(6).toUpper();
            label = QStringLiteral("%1 (%2)").arg(cty.entity.name, cty.entity.primaryPrefix);
            ok = true;
        }
    }

    if (!ok) {
        QMessageBox::warning(this,
                             uiText("rotator_target_not_found", "Target not found"),
                             uiText("rotator_target_not_found_text",
                                    "Enter a Maidenhead locator, a DXCC/country name, or a callsign/prefix known in cty.csv."));
        return;
    }

    const double bearing = QsoMapWidget::bearingDeg(homeLonLat, targetLonLat);
    const double distance = QsoMapWidget::distanceKm(homeLonLat, targetLonLat);

    mm::CatRotatorController::QsoTarget target;
    target.callsign = label.toUpper();
    target.grid = targetGrid;
    target.reason = uiText("rotator_manual_locator_country", "manual locator/country target");
    target.bearingDeg = bearing;
    target.distanceKm = distance;
    target.qsoActive = false;
    target.updatedUtc = QDateTime::currentDateTimeUtc();
    m_catRotatorController->setTrackingMode(mm::CatRotatorController::TrackingMode::Manual);
    m_catRotatorController->setQsoTarget(target);
    m_catRotatorController->setAzEl(bearing, 0.0, uiText("rotator_manual_locator_country", "manual locator/country target"));

    appendLog(QStringLiteral("CatRotator direct target: %1 -> %2° (%3 km)")
              .arg(label, QString::number(bearing, 'f', 1), QString::number(distance, 'f', 0)));
}

void MainWindow::updateCatRotatorQsoTarget(const QString &reason)
{
    if (m_catRotatorController == nullptr) {
        return;
    }
    mm::CatRotatorController::QsoTarget target;
    target.callsign = m_ftSession.dxCall.trimmed().toUpper();
    target.grid = m_ftSession.dxGrid.trimmed().left(6).toUpper();
    if (target.grid.isEmpty() && !target.callsign.isEmpty()) {
        target.grid = m_ftSession.knownGridFor(target.callsign).trimmed().left(6).toUpper();
    }
    target.rxFrequencyHz = m_ftSession.audioFreqHz > 0 ? m_ftSession.audioFreqHz :
        ((m_spinFt8RxFreq != nullptr) ? m_spinFt8RxFreq->value() : m_settings.ft8RxFrequencyHz);
    target.qsoActive = m_ftSession.qsoActive;
    target.reason = reason;
    target.updatedUtc = QDateTime::currentDateTimeUtc();

    QPointF homeLonLat;
    QPointF dxLonLat;
    const QString homeGrid = stationLocator();
    QString bearingGrid = target.grid.left(target.grid.size() >= 6 ? 6 : 4);
    if (!target.callsign.isEmpty() && (bearingGrid.isEmpty() || !QsoMapWidget::maidenheadToLonLat(bearingGrid, &dxLonLat))) {
        const CtyCountryFile::LookupResult cty = CtyCountryFile::instance().lookupCallsign(target.callsign);
        if (cty.valid && QsoMapWidget::maidenheadToLonLat(cty.entity.referenceGrid.left(4), &dxLonLat)) {
            bearingGrid = cty.entity.referenceGrid.left(4).toUpper();
            if (target.grid.isEmpty()) {
                target.grid = bearingGrid;
            }
        }
    }
    if (!homeGrid.trimmed().isEmpty() && !bearingGrid.trimmed().isEmpty() &&
        QsoMapWidget::maidenheadToLonLat(homeGrid, &homeLonLat) &&
        QsoMapWidget::maidenheadToLonLat(bearingGrid, &dxLonLat)) {
        target.bearingDeg = QsoMapWidget::bearingDeg(homeLonLat, dxLonLat);
        target.distanceKm = QsoMapWidget::distanceKm(homeLonLat, dxLonLat);
    }

    if (target.callsign.isEmpty()) {
        m_catRotatorController->clearQsoTarget();
        if (m_catRotatorSidePanel != nullptr) {
            m_catRotatorSidePanel->updateQsoTarget(mm::CatRotatorController::QsoTarget());
        }
    } else {
        m_catRotatorController->setQsoTarget(target);
        if (m_catRotatorSidePanel != nullptr) {
            m_catRotatorSidePanel->updateQsoTarget(target);
        }
    }
}


bool MainWindow::ftRotatorReadyForPendingTx(QString *reason, int *etaMs) const
{
    if (reason != nullptr) reason->clear();
    if (etaMs != nullptr) *etaMs = 0;
    if (m_pendingFt8Tune) {
        return true;
    }
    if (m_catRotatorController == nullptr) {
        return true;
    }
    const mm::CatRotatorController::Config cfg = m_catRotatorController->config();
    if (!cfg.enabled) {
        return true;
    }
    if (!m_settings.rotatorBlockFtTxUntilReady) {
        if (reason != nullptr) {
            *reason = uiText("ft_rotator_guard_disabled", "rotator TX guard disabled by settings");
        }
        return true;
    }
    // A configured but disconnected rotator must not make the FT sequencer drop
    // into RX monitor forever.  Use the rotator guard only when the backend is
    // actually connected and can report/move toward the current QSO target.
    if (!m_catRotatorController->isConnected()) {
        return true;
    }
    const mm::CatRotatorController::QsoTarget target = m_catRotatorController->qsoTarget();
    if (target.callsign.trimmed().isEmpty() || target.bearingDeg < 0.0) {
        return true;
    }
    const int eta = m_catRotatorController->estimatePointingTimeMs(target.bearingDeg, 0.0);
    if (etaMs != nullptr) *etaMs = eta;
    if (m_catRotatorController->isReadyForTarget(target.bearingDeg, 0.0)) {
        return true;
    }
    if (reason != nullptr) {
        *reason = uiText("ft_rotator_waiting_pointing", "waiting for rotator pointing: %1 %2°, ETA %3 s")
            .arg(target.callsign,
                 QString::number(target.bearingDeg, 'f', 1),
                 QString::number(static_cast<double>(eta) / 1000.0, 'f', 1));
    }
    return false;
}

void MainWindow::deferPendingFtTxForRotator(int etaMs, const QString &reason)
{
    const QString txMessage = m_pendingFt8TxMessage.trimmed().toUpper();
    const QString txTag = m_pendingFt8TxTag.trimmed().isEmpty() ? QStringLiteral("TX") : m_pendingFt8TxTag.trimmed().toUpper();
    if (txMessage.isEmpty()) {
        return;
    }
    if (m_ftSlotScheduler != nullptr) {
        QMetaObject::invokeMethod(m_ftSlotScheduler, "cancelTransmission", Qt::QueuedConnection);
    }
    if (m_pendingFt8PttKeyed && !m_txRunning && !m_ftTxWorkerRunning) {
        unkeyPttAfterTx();
    }
    m_ft8PendingTxArmed = false;
    m_pendingFt8PttPrearmed = false;
    m_pendingFt8PttKeyed = false;
    const int delay = qBound(1000, etaMs + 1000, 30000);
    const QString cleanReason = reason.trimmed().isEmpty()
        ? uiText("ft_rotator_waiting_pointing_short", "waiting for rotator pointing")
        : reason.trimmed();
    static qint64 s_lastFtRotatorDeferLogMs = 0;
    static QString s_lastFtRotatorDeferReason;
    const qint64 nowLogMs = QDateTime::currentMSecsSinceEpoch();
    if (s_lastFtRotatorDeferReason != cleanReason || nowLogMs - s_lastFtRotatorDeferLogMs > 5000) {
        appendLog(QStringLiteral("FT rotator guard: %1. TX inhibited and deferred to the next valid slot.").arg(cleanReason));
        s_lastFtRotatorDeferLogMs = nowLogMs;
        s_lastFtRotatorDeferReason = cleanReason;
    }
    if (m_lblFt8SlotStatus != nullptr) {
        m_lblFt8SlotStatus->setText(uiText("ft_rotator_waiting_countdown", "Rotator pointing wait: %1 s; FT TX inhibited")
            .arg(QString::number(static_cast<double>(etaMs) / 1000.0, 'f', 1)));
    }
    QTimer::singleShot(delay, this, [this, txMessage, txTag]() {
        if (m_txRunning || m_ftTxWorkerRunning) {
            return;
        }
        if (!Ft8Mode::isFamilyMode(ui != nullptr && ui->cmbMode != nullptr ? ui->cmbMode->currentText() : QString())) {
            return;
        }
        const QString currentMessage = m_pendingFt8TxMessage.trimmed().toUpper();
        const QString currentTag = m_pendingFt8TxTag.trimmed().isEmpty() ? QStringLiteral("TX") : m_pendingFt8TxTag.trimmed().toUpper();
        if (!txMessage.isEmpty() && currentMessage == txMessage && currentTag == txTag) {
            scheduleFt8SequencerMessage(txMessage, txTag);
        }
    });
    updateFt8TxBannerUi();
    updateTxControlState();
}

void MainWindow::showAppSettingsDialog()
{
    showAppSettingsDialogPage(AppSettingsDialog::InitialPage::AudioPtt);
}

void MainWindow::showAppSettingsDialogPage(AppSettingsDialog::InitialPage page)
{
    if (m_txRunning) {
        QMessageBox::information(
            this,
            uiTextFromSource("text", "Settings"),
            uiTextFromSource("text", "Stop TX before changing settings.")
            );
        return;
    }

    QString currentPath = m_logbook.fileName().trimmed();
    if (currentPath.isEmpty()) {
        currentPath = AdifLogbook::defaultPath();
    }

    AppSettingsDialog dialog(
        m_settings,
        currentPath,
        AdifLogbook::defaultPath(),
        schedulerEntries(),
        [this](const QString &source) { return uiTextFromSource("text", source); },
        [this](const QString &key, const QString &fallback) { return uiText(key, fallback); },
        page,
        this);

    // Also translate late-created editor widgets such as the visual MM/AutoQSO
    // flow editor.  Widgets remember their original source text via properties
    // so repeated language passes do not corrupt labels.
    applyUiLanguageToObjectTree(&dialog);

    QPointer<AppSettingsDialog> dialogPtr(&dialog);
    connect(&dialog, &AppSettingsDialog::catTestRequested, this, [this, dialogPtr](const HamlibController::Config &cfg) {
        appendLog(QStringLiteral("CAT TEST: queued on isolated worker."));
        if (m_rigController == nullptr) {
            if (dialogPtr) dialogPtr->setExternalCatTestStatus(QStringLiteral("TEST FAILED — no CAT controller"), false);
            return;
        }
        QMetaObject::invokeMethod(m_rigController, [controller = m_rigController, cfg, dialogPtr]() {
            controller->configure(cfg);
            const bool okOpen = controller->connectRig();
            if (okOpen) {
                controller->pollNow();
            }
            const double hz = controller->lastFrequencyHz();
            const QString message = (okOpen && hz > 0.0)
                ? QStringLiteral("TEST OK — frequency: %1 MHz").arg(hz / 1000000.0, 0, 'f', 6)
                : QStringLiteral("TEST FAILED — %1").arg(controller->lastStatus().isEmpty() ? QStringLiteral("no valid frequency returned") : controller->lastStatus());
            QMetaObject::invokeMethod(qApp, [dialogPtr, message, ok = (okOpen && hz > 0.0)]() {
                if (dialogPtr) dialogPtr->setExternalCatTestStatus(message, ok);
            }, Qt::QueuedConnection);
        }, Qt::QueuedConnection);
    });
    connect(&dialog, &AppSettingsDialog::pttTestRequested, this, [this, dialogPtr](const HamlibController::Config &cfg, bool enabled) {
        appendLog(enabled ? QStringLiteral("PTT TEST: ON queued on isolated worker.")
                          : QStringLiteral("PTT TEST: OFF queued on isolated worker."));
        if (enabled && !ensureStationIdentityForTx(QStringLiteral("PTT TEST"))) {
            if (dialogPtr) dialogPtr->setExternalCatTestStatus(uiText("station_identity_required", "Station identity required"), false);
            return;
        }
        if (m_rigController == nullptr) {
            if (dialogPtr) dialogPtr->setExternalCatTestStatus(QStringLiteral("PTT TEST FAILED — no CAT controller"), false);
            return;
        }
        QMetaObject::invokeMethod(m_rigController, [controller = m_rigController, cfg, enabled, dialogPtr]() {
            controller->configure(cfg);
            const bool ok = controller->setPtt(enabled);
            const QString message = ok
                ? (enabled ? QStringLiteral("PTT TEST OK — TX requested") : QStringLiteral("PTT TEST OK — RX requested"))
                : QStringLiteral("PTT TEST FAILED — %1").arg(controller->lastStatus().isEmpty() ? QStringLiteral("no detailed error returned") : controller->lastStatus());
            QMetaObject::invokeMethod(qApp, [dialogPtr, message, ok]() {
                if (dialogPtr) dialogPtr->setExternalCatTestStatus(message, ok);
            }, Qt::QueuedConnection);
        }, Qt::QueuedConnection);
    });

    if (m_catRotatorController != nullptr) {
        connect(m_catRotatorController, &mm::CatRotatorController::calibrationFinished,
                &dialog, [dialogPtr](const mm::CatRotatorController::Config &cfg, const QString &message) {
            if (dialogPtr) {
                dialogPtr->setRotatorCalibrationResult(cfg.profileIndex,
                                                       cfg.azimuthMsPerDeg,
                                                       cfg.elevationMsPerDeg,
                                                       cfg.calibrationStampUtc,
                                                       message);
            }
        });
    }

    connect(&dialog, &AppSettingsDialog::rotatorCalibrationRequested,
            this, [this, dialogPtr](int profileIndex, bool elevationAxis) {
        if (!dialogPtr) return;
        if (m_txRunning) {
            QMessageBox::warning(this,
                                 uiText("rotator_calibration", "Rotator calibration"),
                                 uiText("rotator_calibration_stop_tx", "Stop TX before starting rotator calibration."));
            return;
        }
        m_settings = dialogPtr->settings();
        m_settings.rotatorActiveProfile = qBound(0, profileIndex, 2);
        savePersistentSettings();
        applyCatRotatorSettings();
        if (m_catRotatorController == nullptr) return;
        if (!m_catRotatorController->isConnected()) {
            appendLog(uiText("rotator_calibration_connect_first", "CatRotator calibration requested; connect the selected rotator first."));
        }
        if (elevationAxis) {
            m_catRotatorController->startElevationCalibration();
        } else {
            m_catRotatorController->startAzimuthCalibration();
        }
    });

    // Force Settings to be a real full-screen workbench before exec() enters
    // the modal loop.  Relying only on showEvent/showMaximized was not robust
    // with the custom frameless cockpit chrome on all Linux WMs.
    dialog.setWindowFlag(Qt::Window, true);
    dialog.setWindowFlag(Qt::FramelessWindowHint, true);
    dialog.setWindowModality(Qt::ApplicationModal);
    if (QScreen *screen = this->screen()) {
        dialog.setGeometry(screen->geometry());
    } else if (QScreen *screen = QGuiApplication::primaryScreen()) {
        dialog.setGeometry(screen->geometry());
    }
    dialog.showFullScreen();

    if (m_ddspController != nullptr) {
        m_ddspController->setActivityHint(QStringLiteral("settings"));
    }
    const int settingsResult = dialog.exec();
    if (m_ddspController != nullptr) {
        m_ddspController->setActivityHint(QStringLiteral("normal"));
    }
    if (settingsResult != QDialog::Accepted) {
        appendLog("Settings cancelled; restoring runtime configuration.");
        applyPersistentSettingsToRuntime();
        return;
    }

    const AppSettings oldSettings = m_settings;
    const QString oldLogbookPath = currentPath;
    m_settings = dialog.settings();
    m_settings.schedulerQsyPlanJson = BandSchedulerDialog::serializeEntries(dialog.schedulerEntries());
    savePersistentSettings();

    const QString newLogbookPath = dialog.selectedLogbookPath().trimmed();
    if (!newLogbookPath.isEmpty() &&
        QFileInfo(newLogbookPath).absoluteFilePath() != QFileInfo(oldLogbookPath).absoluteFilePath()) {
        m_logbook.setFileName(newLogbookPath);
        QString loadError;
        if (!m_logbook.load(&loadError)) {
            QMessageBox::warning(this,
                                 uiText("logbook", "Logbook"),
                                 uiText("cannot_load_logbook", "Cannot load logbook:") + " " + loadError);
        } else {
            appendLog(uiText("logbook_file_changed", "Logbook file changed:") + " " + QDir::toNativeSeparators(newLogbookPath));
        }
    }

    refreshDevices();
    loadFt8SettingsToUi();
    applyPersistentSettingsToRuntime();
    refreshTextMacroButtons();
    refreshFt8StandardMessages();
    updateBandSchedulerTabForMode(ui != nullptr && ui->cmbMode != nullptr ? ui->cmbMode->currentText() : QString());
    refreshLogbookHighlights();
    refreshQsoMaps();
    m_bandSchedulerTriggeredKeys.clear();

    appendLog(QStringLiteral("Settings updated."));
    if (oldSettings.audioInputName != m_settings.audioInputName ||
        oldSettings.audioOutputName != m_settings.audioOutputName ||
        oldSettings.audioSampleRate != m_settings.audioSampleRate) {
        appendLog("Audio input: " + selectedAudioInputLabel());
        appendLog(QString("Audio sample rate: %1 Hz").arg(m_settings.audioSampleRate));
    }
    if (oldSettings.hamlibCatEnabled != m_settings.hamlibCatEnabled ||
        oldSettings.hamlibRigModel != m_settings.hamlibRigModel ||
        oldSettings.hamlibRigPath != m_settings.hamlibRigPath ||
        oldSettings.hamlibPttEnabled != m_settings.hamlibPttEnabled) {
        appendLog(QString("CAT/Rig updated: CAT %1, CAT PTT %2, model %3, path %4.")
                      .arg(m_settings.hamlibCatEnabled ? QStringLiteral("ON") : QStringLiteral("OFF"),
                           m_settings.hamlibPttEnabled ? QStringLiteral("ON") : QStringLiteral("OFF"))
                      .arg(m_settings.hamlibRigModel)
                      .arg(m_settings.hamlibRigPath.isEmpty() ? QStringLiteral("default") : m_settings.hamlibRigPath));
    }
}


void MainWindow::openSstvImageEditor()
{
    if (m_txRunning) {
        QMessageBox::information(this,
                                 uiTextFromSource("text", "SSTV editor"),
                                 uiTextFromSource("text", "Stop TX before opening the SSTV editor."));
        return;
    }

    SstvImageEditorDialog dialog(this);
    applyUiLanguageToObjectTree(&dialog);
    const bool sstvMode = (ui->cmbMode != nullptr && ui->cmbMode->currentText() == SstvDecoder::modeName());
    if (!m_sstvTxBaseImage.isNull()) {
        dialog.setBackgroundImage(m_sstvTxBaseImage, m_txImageFileName);
    } else if (sstvMode && !m_txSourceImage.isNull()) {
        dialog.setBackgroundImage(m_txSourceImage, m_txImageFileName);
    }

    connect(&dialog, &SstvImageEditorDialog::imageReadyForTx,
            this, [this](const QImage &image, const QString &suggestedFileName) {
                if (image.isNull()) {
                    return;
                }
                m_sstvTxBaseImage = image.convertToFormat(QImage::Format_RGB32);
                m_txImageOwnerMode = SstvDecoder::modeName();
                m_txImageFileName = QFileInfo(suggestedFileName).fileName();
                if (m_txImageFileName.trimmed().isEmpty()) {
                    m_txImageFileName = QStringLiteral("SSTV_QSO_card.png");
                }
                if (ui->cmbMode != nullptr) {
                    const int idx = ui->cmbMode->findText(SstvDecoder::modeName());
                    if (idx >= 0) {
                        ui->cmbMode->setCurrentIndex(idx);
                    }
                }
                updateSstvTxPreparedImage();
                appendLog("SSTV QSO image updated from editor.");
            });
    dialog.exec();
}

void MainWindow::updateSstvTxPreparedImage()
{
    if (m_lblSstvTxPreview == nullptr) {
        return;
    }

    const bool sstvMode = (ui->cmbMode != nullptr && ui->cmbMode->currentText() == SstvDecoder::modeName());
    const QString call = m_editSstvTxCall != nullptr ? m_editSstvTxCall->text().trimmed() : QString();
    const QString name = m_editSstvTxName != nullptr ? m_editSstvTxName->text().trimmed() : QString();
    const QString qth = m_editSstvTxQth != nullptr ? m_editSstvTxQth->text().trimmed() : QString();
    const QString info = m_editSstvTxReport != nullptr ? m_editSstvTxReport->text().trimmed() : QString();
    const bool hasQsoText = !call.isEmpty() || !name.isEmpty() || !qth.isEmpty() || !info.isEmpty();

    if (!sstvMode) {
        return;
    }

    QImage base = m_sstvTxBaseImage;
    if (base.isNull() && !m_txSourceImage.isNull()) {
        base = m_txSourceImage;
    }

    if (base.isNull() && !hasQsoText) {
        m_lblSstvTxPreview->setPixmap(QPixmap());
        m_lblSstvTxPreview->setText(uiText("preview.noSstv", "No SSTV TX image"));
        m_txSourceImage = QImage();
        m_txPreparedImage = QImage();
        updateTxControlState();
        return;
    }

    if (base.isNull()) {
        base = QImage(960, 744, QImage::Format_RGB32);
        base.fill(Qt::white);
    }

    if (base.format() != QImage::Format_RGB32) {
        base = base.convertToFormat(QImage::Format_RGB32);
    }

    QImage composed = base.copy();
    QPainter painter(&composed);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    painter.setPen(QPen(Qt::black, qMax(4, composed.width() / 120)));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(14, 14, composed.width() - 28, composed.height() - 28);

    painter.setPen(QPen(Qt::black, qMax(2, composed.width() / 360)));
    painter.drawRect(38, 38, composed.width() - 76, composed.height() - 170);
    painter.drawRect(38, composed.height() - 136, composed.width() - 76, 72);

    QFont titleFont = font();
    titleFont.setBold(true);
    titleFont.setPointSize(qMax(20, composed.height() / 28));
    QFont infoFont = font();
    infoFont.setPointSize(qMax(16, composed.height() / 42));

    painter.setPen(Qt::black);
    if (!call.isEmpty()) {
        painter.setFont(titleFont);
        painter.drawText(QPoint(55, composed.height() - 92), QString("Call: %1").arg(call));
    }
    painter.setFont(infoFont);
    if (!name.isEmpty()) {
        painter.drawText(QPoint(55, composed.height() - 52), QString("Name: %1").arg(name));
    }
    if (!qth.isEmpty()) {
        painter.drawText(QPoint(composed.width() / 2, composed.height() - 92), QString("QTH: %1").arg(qth));
    }
    if (!info.isEmpty()) {
        painter.drawText(QPoint(composed.width() / 2, composed.height() - 52), QString("Info: %1").arg(info));
    }

    painter.end();

    m_txSourceImage = composed;
    m_txImageOwnerMode = SstvDecoder::modeName();
    m_txPreparedImage = SstvTransmitter::prepareImage(m_txSourceImage,
                                                      ui->cmbSstvMode != nullptr ? ui->cmbSstvMode->currentText() : QString());
    const QPixmap px = QPixmap::fromImage(m_txPreparedImage.isNull() ? composed : m_txPreparedImage)
                           .scaled(m_lblSstvTxPreview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_lblSstvTxPreview->setPixmap(px);
    m_lblSstvTxPreview->setText(QString());

    updateTxControlState();
}



void MainWindow::showLogbookDialog()
{
    QString error;
    if (!m_logbook.load(&error)) {
        QMessageBox::warning(this,
                             uiTextFromSource("text", "Logbook"),
                             uiTextFromSource("text", "Cannot load logbook:") + " " + error);
    }

    LogbookDialog dialog(&m_logbook, &m_settings, this);
    dialog.setTextTranslator([this](const QString &source) {
        return uiTextFromSource("text", source);
    });
    applyUiLanguageToObjectTree(&dialog);
    connect(&dialog, &LogbookDialog::logbookChanged,
            this, [this]() {
                refreshLogbookHighlights();
                refreshQsoMaps();
                appendLog(QString("Logbook updated: %1 QSOs.").arg(m_logbook.count()));
            });
    if (m_ddspController != nullptr) {
        m_ddspController->setActivityHint(QStringLiteral("logbook"));
    }
    dialog.exec();
    if (m_ddspController != nullptr) {
        m_ddspController->setActivityHint(QStringLiteral("normal"));
    }
    refreshLogbookHighlights();
    refreshQsoMaps();
}


void MainWindow::enterWhatsThisMode()
{
    QWhatsThis::enterWhatsThisMode();
}

void MainWindow::showOnlineHelp()
{
    HelpDialog dialog(this);
    applyUiLanguageToObjectTree(&dialog);
    dialog.exec();
}

void MainWindow::showAboutMadModem()
{
    QDialog dialog(this);
    dialog.setWindowTitle(uiText("action.about", "About MM"));
    dialog.resize(560, 360);
    dialog.setMinimumSize(520, 320);

    QVBoxLayout *outer = new QVBoxLayout(&dialog);
    outer->setContentsMargins(18, 18, 18, 18);
    outer->setSpacing(12);

    QHBoxLayout *header = new QHBoxLayout();
    header->setSpacing(14);

    QLabel *icon = new QLabel(&dialog);
    icon->setPixmap(QIcon(QStringLiteral(":/icons/madmodem.png")).pixmap(48, 48));
    icon->setFixedSize(54, 54);
    header->addWidget(icon, 0, Qt::AlignTop);

    QLabel *title = new QLabel(QStringLiteral(
        "<b>MadModem 0.5.10</b><br>"
        "Amateur radio digital modem for HF RX/TX."), &dialog);
    title->setTextFormat(Qt::RichText);
    title->setWordWrap(true);
    header->addWidget(title, 1);
    outer->addLayout(header);

    QLabel *body = new QLabel(&dialog);
    body->setTextFormat(Qt::RichText);
    body->setWordWrap(true);
    body->setText(QStringLiteral(
        "<p><b>Author:</b> Papadopol Lucian-Ioan, IZ6NNH<br>"
        "<b>Website:</b> www.madexp.it<br>"
        "<b>License:</b> GPLv3</p>"
        "<p><b>Modes:</b> FT8/FT4, CW, RTTY, BPSK/QPSK, MFSK, "
        "SSTV, WEFAX, Feld Hell / FSK-105.</p>"
        "<p><b>Includes:</b> CAT/PTT via Hamlib/HRD, ADIF logbook, "
        "QSO map and bundled third-party open-source components.</p>"
        "<p style='color:#666;'>See <tt>LICENSE</tt> and "
        "<tt>THIRD_PARTY_NOTICES.md</tt> for full legal notices.</p>"));
    outer->addWidget(body, 1);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    outer->addWidget(buttons);

    dialog.exec();
}


void MainWindow::showRuntimeLogDialog()
{
    if (m_runtimeLogDialog == nullptr) {
        m_runtimeLogDialog = new QDialog(this);
        m_runtimeLogDialog->setWindowTitle(uiText("runtime_log", "Runtime log"));
        m_runtimeLogDialog->resize(980, 520);
        QVBoxLayout *layout = new QVBoxLayout(m_runtimeLogDialog);
        layout->setContentsMargins(8, 8, 8, 8);
        layout->setSpacing(6);
        m_runtimeLogText = new QPlainTextEdit(m_runtimeLogDialog);
        m_runtimeLogText->setReadOnly(true);
        m_runtimeLogText->setMaximumBlockCount(5000);
        m_runtimeLogText->setLineWrapMode(QPlainTextEdit::NoWrap);
        layout->addWidget(m_runtimeLogText, 1);
        QHBoxLayout *buttons = new QHBoxLayout();
        QPushButton *clear = new QPushButton(uiText("clear", "Clear"), m_runtimeLogDialog);
        QPushButton *close = new QPushButton(uiText("close", "Close"), m_runtimeLogDialog);
        buttons->addWidget(clear);
        buttons->addStretch(1);
        buttons->addWidget(close);
        layout->addLayout(buttons);
        connect(clear, &QPushButton::clicked, m_runtimeLogText, &QPlainTextEdit::clear);
        connect(close, &QPushButton::clicked, this, [this]() {
            if (m_runtimeLogDialog != nullptr) m_runtimeLogDialog->hide();
            if (m_ddspController != nullptr) m_ddspController->setActivityHint(QStringLiteral("normal"));
        });
        connect(m_runtimeLogDialog, &QDialog::finished, this, [this](int) {
            if (m_ddspController != nullptr) m_ddspController->setActivityHint(QStringLiteral("normal"));
        });
    }
    if (m_ddspController != nullptr) {
        m_ddspController->setActivityHint(QStringLiteral("logbook"));
    }
    m_runtimeLogDialog->show();
    m_runtimeLogDialog->raise();
    m_runtimeLogDialog->activateWindow();
}

void MainWindow::appendRuntimeLogLine(const QString &line)
{
    if (m_runtimeLogText == nullptr) {
        return;
    }
    m_runtimeLogText->appendPlainText(line);
}


// -----------------------------------------------------------------------------
// Log
// -----------------------------------------------------------------------------

void MainWindow::appendLog(const QString &message)
{
    const QString localized = localizedRuntimeText(QStringLiteral("log"), message);
    const QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    const QString line = QString("[%1] %2").arg(timestamp, localized);
    ui->txtLog->appendPlainText(line);
    appendRuntimeLogLine(line);
}

// -----------------------------------------------------------------------------
// Device enumeration
// -----------------------------------------------------------------------------

void MainWindow::refreshDevices()
{
    populateAudioInputs();
    populateAudioOutputs();
    populateSerialPorts();

    selectComboByBackendName(ui->cmbAudioInput, m_settings.audioInputName);
    selectComboByBackendName(ui->cmbAudioOutput, m_settings.audioOutputName);
    selectComboByBackendName(ui->cmbPttPort, m_settings.pttPortName);

    appendLog("Device list refreshed.");
}

void MainWindow::populateAudioInputs()
{
    ui->cmbAudioInput->clear();
    ui->cmbAudioInput->addItem("Default", "default");

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)

    const QList<QAudioDevice> devices = QMediaDevices::audioInputs();

    for (const QAudioDevice &device : devices) {
        const QString backendName = device.description();
        ui->cmbAudioInput->addItem(friendlyAudioName(backendName), backendName);
    }

#else

    const QList<QAudioDeviceInfo> devices =
        QAudioDeviceInfo::availableDevices(QAudio::AudioInput);

    for (const QAudioDeviceInfo &device : devices) {
        const QString backendName = device.deviceName();
        ui->cmbAudioInput->addItem(friendlyAudioName(backendName), backendName);
    }

#endif

    if (ui->cmbAudioInput->count() == 0) {
        ui->cmbAudioInput->addItem("No audio input found", QString());
    }
}

void MainWindow::populateAudioOutputs()
{
    ui->cmbAudioOutput->clear();
    ui->cmbAudioOutput->addItem("Default", "default");

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)

    const QList<QAudioDevice> devices = QMediaDevices::audioOutputs();

    for (const QAudioDevice &device : devices) {
        const QString backendName = device.description();
        ui->cmbAudioOutput->addItem(friendlyAudioName(backendName), backendName);
    }

#else

    const QList<QAudioDeviceInfo> devices =
        QAudioDeviceInfo::availableDevices(QAudio::AudioOutput);

    for (const QAudioDeviceInfo &device : devices) {
        const QString backendName = device.deviceName();
        ui->cmbAudioOutput->addItem(friendlyAudioName(backendName), backendName);
    }

#endif

    if (ui->cmbAudioOutput->count() == 0) {
        ui->cmbAudioOutput->addItem("No audio output found", QString());
    }
}

void MainWindow::populateSerialPorts()
{
    ui->cmbPttPort->clear();

    const QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();

    for (const QSerialPortInfo &port : ports) {
        QString label = port.portName();

        if (!port.description().isEmpty()) {
            label += " - " + port.description();
        }

        ui->cmbPttPort->addItem(label, port.portName());
    }

    if (ui->cmbPttPort->count() == 0) {
        ui->cmbPttPort->addItem("No serial port found", QString());
    }
}

QString MainWindow::selectedAudioInputName() const
{
    if (!m_settings.audioInputName.trimmed().isEmpty()) {
        return m_settings.audioInputName;
    }

    const QString backendName = ui->cmbAudioInput->currentData().toString();

    if (!backendName.isEmpty()) {
        return backendName;
    }

    return "default";
}

QString MainWindow::selectedAudioInputLabel() const
{
    const QString backendName = selectedAudioInputName();
    const int index = ui->cmbAudioInput->findData(backendName);

    if (index >= 0) {
        return ui->cmbAudioInput->itemText(index);
    }

    if (backendName == "default") {
        return "Default";
    }

    return backendName;
}

QString MainWindow::selectedPttPort() const
{
    if (!m_settings.pttPortName.trimmed().isEmpty()) {
        return m_settings.pttPortName;
    }

    return ui->cmbPttPort->currentData().toString();
}

// -----------------------------------------------------------------------------
// RX control
// -----------------------------------------------------------------------------

void MainWindow::toggleRxReady()
{
    if (m_shutdownInProgress || m_runtimeShutdownComplete) {
        return;
    }

    if (m_txRunning || m_offlineAnalysisActive) {
        return;
    }

    if (m_rxRunning || (m_audioEngine != nullptr && m_audioEngine->isRunning())) {
        stopRx();
        return;
    }

    startRx();
}

void MainWindow::startRx()
{
    if (m_shutdownInProgress || m_runtimeShutdownComplete) {
        return;
    }

    if (m_offlineAnalysisActive) {
        appendLog("RX start blocked: offline analysis is active.");
        return;
    }

    if (m_txRunning) {
        appendLog("RX start blocked: TX/PTT is active.");
        return;
    }

    if (m_audioEngine->isRunning()) {
        appendLog("RX is already running.");
        return;
    }

    const QString modeName = ui->cmbMode->currentText();

    if (modeName == WeatherFaxDecoder::modeName()) {
        applyWeatherFaxSettings();
    } else if (modeName == SstvDecoder::modeName()) {
        applySstvSettings();
    } else if (modeName == RttyDecoder::modeName()) {
        applyRttySettings();
    } else if (modeName == Bpsk31Decoder::modeName()) {
        applyBpsk31Settings();
    } else if (modeName == MfskDecoder::modeName()) {
        applyMfskSettings();
    } else if (modeName == CwDecoder::modeName()) {
        applyCwSettings();
    } else if (modeName == HellschreiberDecoder::modeName()) {
        applyHellSettings();
    } else if (Ft8Mode::isFamilyMode(modeName)) {
        applyFt8Settings();
    } else if (Msk144Mode::isMode(modeName)) {
        applyMsk144Settings();
    } else if (Q65Mode::isFamilyMode(modeName)) {
        applyQ65Settings();
    } else {
        appendLog(modeName + " is not implemented yet.");
        return;
    }

    const QString inputName = selectedAudioInputName();
    const QString inputLabel = selectedAudioInputLabel();

    if (inputName.isEmpty() || inputLabel.contains("No audio input", Qt::CaseInsensitive)) {
        appendLog("RX start failed: no valid audio input selected.");
        return;
    }

    resetDspEngine();
    m_decoderConditioner.reset();

    if (m_waterfallWidget != nullptr) {
        m_waterfallWidget->clear();
    }

    if (m_faxImageWidget != nullptr) {
        m_faxImageWidget->clear();
    }

    const bool preserveTextTerminal = m_preserveTextTerminalOnNextRx;
    m_preserveTextTerminalOnNextRx = false;

    if (modeName == WeatherFaxDecoder::modeName()) {
        m_weatherFaxDecoder->reset();
    } else if (modeName == SstvDecoder::modeName()) {
        m_sstvDecoder->reset();
    } else if (modeName == RttyDecoder::modeName()) {
        m_lastRttyDecodedText.clear();
        m_rttyDecoder->reset();
        if (!preserveTextTerminal && m_txtRttyRx != nullptr) {
            m_txtRttyRx->clear();
        }
    } else if (modeName == Bpsk31Decoder::modeName()) {
        m_lastBpsk31DecodedText.clear();
        m_bpsk31Decoder->reset();
        if (!preserveTextTerminal && m_txtBpsk31Rx != nullptr) {
            m_txtBpsk31Rx->clear();
        }
    } else if (modeName == MfskDecoder::modeName()) {
        m_mfskDecoder->reset();
        m_mfskPendingRxLineBreak = false;
        if (!preserveTextTerminal && m_txtMfskRx != nullptr) {
            m_txtMfskRx->clear();
        }
    } else if (modeName == CwDecoder::modeName()) {
        m_cwDecoder->reset();
        if (m_cwSecondaryDecoder != nullptr) {
            m_cwSecondaryDecoder->reset();
        }
        m_cwPrimaryLineOpen = false;
        m_cwSecondaryLineOpen = false;
        m_cwCurrentLineChannel.clear();
        if (!preserveTextTerminal && m_txtCwRx != nullptr) {
            m_txtCwRx->clear();
        }
    } else if (modeName == HellschreiberDecoder::modeName()) {
        m_hellDecoder->reset();
    } else if (Msk144Mode::isMode(modeName)) {
        if (m_msk144Decoder != nullptr) {
            m_msk144Decoder->reset();
        }
        if (m_tableMsk144Rx != nullptr && !preserveTextTerminal) {
            m_tableMsk144Rx->setRowCount(0);
        }
        if (m_lblMsk144SequencerStatus != nullptr) {
            m_lblMsk144SequencerStatus->setText(uiText("msk144_seq_idle", "Sequencer: idle"));
        }
        m_msk144PingOverlays.clear();
    } else if (Q65Mode::isFamilyMode(modeName)) {
        if (m_q65Decoder != nullptr) {
            m_q65Decoder->reset();
        }
        if (m_tableQ65Rx != nullptr && !preserveTextTerminal) {
            m_tableQ65Rx->setRowCount(0);
        }
        if (m_lblQ65SequencerStatus != nullptr) {
            m_lblQ65SequencerStatus->setText(uiText("q65_seq_idle", "Sequencer: idle"));
        }
    } else if (Ft8Mode::isFamilyMode(modeName)) {
        if (m_tableFt8Rx != nullptr && !preserveTextTerminal) {
            m_tableFt8Rx->setRowCount(0);
        }
    }

    ui->lblEstimatedFrequency->setText("Freq: -- Hz");

    appendLog("Starting RX...");
    appendLog("Audio input: " + inputLabel);
    appendLog("Backend name: " + inputName);
    appendLog("Mode: " + modeName);
    if (modeName == WeatherFaxDecoder::modeName()) {
        appendLog(QString("MeteoFax settings: %1 LPM, black %2 Hz, white %3 Hz, APT start %4, auto tones %5, band-pass %6, stop/end detector %7, auto-save %8.")
                      .arg(ui->cmbFaxLpm->currentText())
                      .arg(ui->spinFaxBlackHz->value())
                      .arg(ui->spinFaxWhiteHz->value())
                      .arg(ui->chkFaxAutoStartPhasing->isChecked() ? "ON" : "OFF")
                      .arg(ui->chkFaxAutoToneTracking->isChecked() ? "ON" : "OFF")
                      .arg(ui->chkFaxInputBandpass->isChecked() ? "ON" : "OFF")
                      .arg(ui->chkFaxEndOfSignal->isChecked() ? "ON" : "OFF")
                      .arg(ui->chkFaxAutoSave->isChecked() ? "ON" : "OFF"));
    } else if (modeName == SstvDecoder::modeName()) {
        appendLog(QString("SSTV settings: %1, auto sync %2.")
                      .arg(ui->cmbSstvMode->currentText())
                      .arg(ui->chkSstvAutoSync->isChecked() ? "ON" : "OFF"));
    } else if (modeName == RttyDecoder::modeName()) {
        appendLog(QString("RTTY settings: %1 baud, mark %2 Hz, space %3 Hz, reverse %4, AFC %5 ±%6 Hz.")
                      .arg(m_spinRttyBaud != nullptr ? m_spinRttyBaud->value() : 45.45, 0, 'f', 2)
                      .arg(m_spinRttyMarkHz != nullptr ? m_spinRttyMarkHz->value() : 2125)
                      .arg((m_spinRttyMarkHz != nullptr ? m_spinRttyMarkHz->value() : 2125) +
                           (m_spinRttyShiftHz != nullptr ? m_spinRttyShiftHz->value() : 170))
                      .arg((m_chkRttyReverse != nullptr && m_chkRttyReverse->isChecked()) ? "ON" : "OFF")
                      .arg((m_chkRttyAfc != nullptr && m_chkRttyAfc->isChecked()) ? "ON" : "OFF")
                      .arg(m_spinRttyAfcRangeHz != nullptr ? m_spinRttyAfcRangeHz->value() : 20));
    } else if (modeName == Bpsk31Decoder::modeName()) {
        const QString variant = (m_cmbBpsk31Variant != nullptr) ? m_cmbBpsk31Variant->currentData().toString() : QString("BPSK31");
        appendLog(QString("%1 settings: tone %2 Hz, AFC %3 ±%4 Hz, invert %5.")
                      .arg(variant.isEmpty() ? QString("BPSK31") : variant)
                      .arg(m_spinBpsk31ToneHz != nullptr ? m_spinBpsk31ToneHz->value() : 1000)
                      .arg((m_chkBpsk31Afc != nullptr && m_chkBpsk31Afc->isChecked()) ? "ON" : "OFF")
                      .arg(m_spinBpsk31AfcRangeHz != nullptr ? m_spinBpsk31AfcRangeHz->value() : 20)
                      .arg((m_chkBpsk31Invert != nullptr && m_chkBpsk31Invert->isChecked()) ? "ON" : "OFF"));
    } else if (modeName == MfskDecoder::modeName()) {
        const QString variant = (m_cmbMfskVariant != nullptr) ? m_cmbMfskVariant->currentData().toString() : QString("MFSK16");
        appendLog(QString("%1 settings: center %2 Hz, AFC %3 ±%4 Hz.")
                      .arg(variant.isEmpty() ? QString("MFSK16") : variant)
                      .arg(m_spinMfskCenterHz != nullptr ? m_spinMfskCenterHz->value() : 1000)
                      .arg((m_chkMfskAfc != nullptr && m_chkMfskAfc->isChecked()) ? "ON" : "OFF")
                      .arg(m_spinMfskAfcRangeHz != nullptr ? m_spinMfskAfcRangeHz->value() : 50));
    } else if (modeName == CwDecoder::modeName()) {
        appendLog(QString("CW skimmer settings: RX A %1 Hz, RX B %2, speed hint %3 WPM, Auto WPM %4.")
                      .arg(m_spinCwToneHz != nullptr ? m_spinCwToneHz->value() : 700)
                      .arg(m_cwSecondaryEnabled ? QString::number(m_cwSecondaryToneHz) + QStringLiteral(" Hz") : QStringLiteral("off"))
                      .arg(m_spinCwWpm != nullptr ? m_spinCwWpm->value() : 20)
                      .arg((m_chkCwAutoWpm != nullptr && m_chkCwAutoWpm->isChecked()) ? "ON" : "OFF"));
    } else if (modeName == HellschreiberDecoder::modeName()) {
        const HellschreiberDecoder::Variant variant = (m_cmbHellVariant != nullptr)
                                                         ? HellschreiberDecoder::variantFromKey(m_cmbHellVariant->currentData().toString())
                                                         : HellschreiberDecoder::Variant::FeldHell;
        appendLog(QString("%1 settings: center %2 Hz, column rate %3 col/s, bandwidth %4 Hz, AFC %5 ±%6 Hz.")
                      .arg(HellschreiberDecoder::variantName(variant))
                      .arg(m_spinHellToneHz != nullptr ? m_spinHellToneHz->value() : 1000)
                      .arg(m_spinHellColumnRate != nullptr ? m_spinHellColumnRate->value() : 17.5, 0, 'f', 2)
                      .arg(m_spinHellBandwidthHz != nullptr ? m_spinHellBandwidthHz->value() : 245)
                      .arg((m_chkHellAfc != nullptr && m_chkHellAfc->isChecked()) ? "ON" : "OFF")
                      .arg(m_spinHellAfcRangeHz != nullptr ? m_spinHellAfcRangeHz->value() : 20));
    } else if (Msk144Mode::isMode(modeName)) {
        const int period = (m_cmbMsk144Period != nullptr) ? m_cmbMsk144Period->currentData().toInt() : 15;
        const int rx = (m_spinMsk144RxFreq != nullptr) ? m_spinMsk144RxFreq->value() : 1500;
        const int dfTol = (m_spinMsk144DfTolerance != nullptr) ? m_spinMsk144DfTolerance->value() : 100;
        const int depth = (m_cmbMsk144DecodeDepth != nullptr) ? m_cmbMsk144DecodeDepth->currentData().toInt() : 2;
        const QString depthName = depth <= 1 ? QStringLiteral("Fast") : (depth == 2 ? QStringLiteral("Normal") : QStringLiteral("Deep"));
        appendLog(QStringLiteral("MSK144 settings: RX %1 Hz, DF ±%2 Hz, period %3 s, decode %4, TX center 1500 Hz.")
                      .arg(rx).arg(dfTol).arg(period).arg(depthName));
    } else if (Q65Mode::isFamilyMode(modeName)) {
        const int period = (m_cmbQ65Period != nullptr) ? m_cmbQ65Period->currentData().toInt() : 60;
        const int rx = (m_spinQ65RxFreq != nullptr) ? m_spinQ65RxFreq->value() : 1500;
        const int tx = (m_spinQ65TxFreq != nullptr) ? m_spinQ65TxFreq->value() : 1500;
        const int dfTol = (m_spinQ65DfTolerance != nullptr) ? m_spinQ65DfTolerance->value() : 100;
        const int depth = (m_cmbQ65DecodeDepth != nullptr) ? m_cmbQ65DecodeDepth->currentData().toInt() : 2;
        const QString depthName = depth <= 1 ? QStringLiteral("Fast") : (depth == 2 ? QStringLiteral("Normal") : QStringLiteral("Deep"));
        appendLog(QStringLiteral("%1 settings: RX %2 Hz, TX %3 Hz, DF ±%4 Hz, period %5 s, decode %6.")
                      .arg(Q65Mode::modeName(currentQ65Submode())).arg(rx).arg(tx).arg(dfTol).arg(period).arg(depthName));
    } else if (Ft8Mode::isFamilyMode(modeName)) {
        const Ft8Mode::Profile profile = Ft8Mode::profileForMode(modeName);
        appendLog(QString("%1 settings: RX marker %2 Hz, TX marker %3 Hz, band %4, slot %5 s. %6")
                      .arg(profile.shortLabel)
                      .arg(m_spinFt8RxFreq != nullptr ? m_spinFt8RxFreq->value() : 1500)
                      .arg(m_spinFt8TxFreq != nullptr ? m_spinFt8TxFreq->value() : 1500)
                      .arg(m_cmbFt8Band != nullptr ? m_cmbFt8Band->currentText() : QString("20m"))
                      .arg(static_cast<double>(profile.slotMs) / 1000.0, 0, 'f', profile.slotMs % 1000 == 0 ? 0 : 2)
                      .arg(profile.interoperableCoreAvailable ? QStringLiteral("Live Costas/LDPC decoder active.") : profile.note));
    }

    if (!m_audioEngine->startInput(inputName, m_settings.audioSampleRate)) {
        setReceiverRunning(false);
        appendLog("RX start failed.");
        return;
    }
}

void MainWindow::stopRx()
{
    if (!m_audioEngine->isRunning()) {
        setReceiverRunning(false);
        appendLog("RX already stopped.");
        return;
    }

    m_audioEngine->stopInput();
}

void MainWindow::handleAudioStarted()
{
    m_offlineAnalysisActive = false;
    setReceiverRunning(true);

    appendLog(QString("Audio capture started at %1 Hz.")
                  .arg(m_audioEngine->sampleRate()));
}

void MainWindow::handleAudioStopped()
{
    if (!m_offlineAnalysisActive) {
        setReceiverRunning(false);
    } else {
        m_rxRunning = false;
        updateMainStateButton();
    }

    if (ui->progressAudioLevel != nullptr) ui->progressAudioLevel->setValue(0);
    if (m_ledVuMeter != nullptr) {
        m_ledVuMeter->setLevelPercent(0);
        m_ledVuMeter->setDbText(QStringLiteral("-inf dB"));
    }
    if (ui->lblAudioLevelDb != nullptr) ui->lblAudioLevelDb->setText("-inf dB");
    if (m_lblVuMeterDb != nullptr) m_lblVuMeterDb->setText("-inf dB");
    ui->lblEstimatedFrequency->setText("Freq: -- Hz");

    appendLog("RX stopped.");

    if (!m_pendingModeName.isEmpty()) {
        finishPendingModeChange();
    }
}

void MainWindow::handleAudioError(const QString &message)
{
    setReceiverRunning(false);
    appendLog("Audio error: " + message);
}

void MainWindow::handleAudioLevel(int percent, double db, double rms)
{
    Q_UNUSED(rms)

    if (ui->progressAudioLevel != nullptr) {
        ui->progressAudioLevel->setValue(percent);
    }
    const QString dbText = db <= -119.0
                           ? QStringLiteral("-inf dB")
                           : QStringLiteral("%1 dB").arg(db, 0, 'f', 1);
    if (m_ledVuMeter != nullptr) {
        m_ledVuMeter->setLevelPercent(percent);
        m_ledVuMeter->setDbText(dbText);
    }

    if (ui->lblAudioLevelDb != nullptr) {
        ui->lblAudioLevelDb->setText(dbText);
    }
    if (m_lblVuMeterDb != nullptr) {
        m_lblVuMeterDb->setText(dbText);
    }
}

void MainWindow::handleDominantFrequency(double frequencyHz, double levelDb)
{
    if (frequencyHz <= 0.0) {
        ui->lblEstimatedFrequency->setText("Freq: -- Hz");
        return;
    }

    ui->lblEstimatedFrequency->setText(
        QString("Freq: %1 Hz / %2 dB")
            .arg(frequencyHz, 0, 'f', 0)
            .arg(levelDb, 0, 'f', 1)
        );
}

void MainWindow::handleWeatherFaxStatus(const QString &status)
{
    if (ui->lblDecoderState == nullptr) {
        return;
    }
    ui->lblDecoderState->setText(status);
    ui->lblDecoderState->setToolTip(status);
}

void MainWindow::handleWeatherFaxToneRangeUpdated(double blackHz, double whiteHz)
{
    const int blackValue = qBound(
        ui->spinFaxBlackHz->minimum(),
        static_cast<int>(qRound(blackHz)),
        ui->spinFaxBlackHz->maximum()
        );
    const int whiteValue = qBound(
        ui->spinFaxWhiteHz->minimum(),
        static_cast<int>(qRound(whiteHz)),
        ui->spinFaxWhiteHz->maximum()
        );

    const QSignalBlocker blockBlack(ui->spinFaxBlackHz);
    const QSignalBlocker blockWhite(ui->spinFaxWhiteHz);

    ui->spinFaxBlackHz->setValue(blackValue);
    ui->spinFaxWhiteHz->setValue(whiteValue);

    m_settings.weatherFaxBlackHz = blackValue;
    m_settings.weatherFaxWhiteHz = whiteValue;

    updateWaterfallMarkers();
}

void MainWindow::handleWeatherFaxImageCompleted(const QImage &image, const QString &reason)
{
    appendLog("MeteoFax image complete: " + reason);

    if (!ui->chkFaxAutoSave->isChecked()) {
        appendLog("Auto-save disabled; completed image kept on screen.");
        return;
    }

    applyWeatherFaxSettings();

    const QString fileName = makeWeatherFaxAutoSaveFileName();

    if (!saveWeatherFaxImageToFile(image, fileName)) {
        QMessageBox::warning(this,
                             "Auto-save PNG failed",
                             "Unable to auto-save the completed MeteoFax image.");
        return;
    }

    appendLog("MeteoFax auto-save complete; decoder is waiting for the next start tone.");
}

void MainWindow::handleSstvImageCompleted(const QImage &image, const QString &reason)
{
    Q_UNUSED(image)

    appendLog("SSTV image complete: " + reason);
}

void MainWindow::handleWeatherFaxLinePresetChanged(int index)
{
    if (index < 0) {
        return;
    }

    const QString key = ui->cmbFaxLinePreset->itemData(index).toString();
    const FaxLinePreset preset = presetByKey(key);

    if (preset.key == "CUSTOM") {
        applyWeatherFaxSettings();
        return;
    }

    const QSignalBlocker blockLpm(ui->cmbFaxLpm);
    const QSignalBlocker blockBlack(ui->spinFaxBlackHz);
    const QSignalBlocker blockWhite(ui->spinFaxWhiteHz);
    const QSignalBlocker blockLines(ui->spinFaxImageLines);

    const int lpmIndex = ui->cmbFaxLpm->findText(QString::number(preset.lpm));

    if (lpmIndex >= 0) {
        ui->cmbFaxLpm->setCurrentIndex(lpmIndex);
    }

    ui->spinFaxBlackHz->setValue(preset.blackHz);
    ui->spinFaxWhiteHz->setValue(preset.whiteHz);
    m_settings.weatherFaxImageLines = preset.lines;
    ui->spinFaxImageLines->setValue(preset.lines);
    ui->cmbFaxLinePreset->setToolTip(preset.details);
    ui->cmbFaxLinePreset->setStatusTip(preset.details);

    appendLog("MeteoFax station preset: " + preset.label + " - " + preset.details);

    applyWeatherFaxSettings();
}

void MainWindow::browseWeatherFaxOutputFolder()
{
    const QString currentFolder = ui->editFaxOutputFolder->text().trimmed().isEmpty()
                                      ? defaultWeatherFaxOutputFolder()
                                      : ui->editFaxOutputFolder->text().trimmed();

    const QString folder = QFileDialog::getExistingDirectory(
        this,
        "Select MeteoFax output folder",
        currentFolder
        );

    if (folder.isEmpty()) {
        return;
    }

    ui->editFaxOutputFolder->setText(folder);
    applyWeatherFaxSettings();
}

void MainWindow::handleFaxImageZoomChanged(int percent, bool fitMode)
{
    if (fitMode) {
        ui->lblFaxZoomStatus->setText("Zoom: fit. Mouse wheel zooms, drag pans.");
        return;
    }

    ui->lblFaxZoomStatus->setText(
        QString("Zoom: %1%. Mouse wheel zooms, drag pans.").arg(percent)
        );
}


// -----------------------------------------------------------------------------
// TX image preparation
// -----------------------------------------------------------------------------

std::unique_ptr<TxModulator> MainWindow::buildCurrentTxModulator()
{
    const QString modeName = ui->cmbMode->currentText();
    const int txSampleRate = (m_settings.audioSampleRate == 44100 ||
                              m_settings.audioSampleRate == 48000 ||
                              m_settings.audioSampleRate == 96000)
                                 ? m_settings.audioSampleRate
                                 : 48000;

    const bool textMode = modeName == RttyDecoder::modeName() ||
                          modeName == Bpsk31Decoder::modeName() ||
                          modeName == MfskDecoder::modeName() ||
                          modeName == CwDecoder::modeName() ||
                          modeName == HellschreiberDecoder::modeName() ||
                          Ft8Mode::isFamilyMode(modeName) ||
                          Msk144Mode::isMode(modeName) ||
                          Q65Mode::isFamilyMode(modeName);

    if (!textMode && (m_txSourceImage.isNull() || m_txImageOwnerMode != modeName)) {
        return nullptr;
    }

    if (modeName == WeatherFaxDecoder::modeName()) {
        return std::unique_ptr<TxModulator>(new WeatherFaxTransmitter(
            m_txSourceImage,
            txSampleRate,
            ui->cmbFaxLpm->currentText().toInt(),
            static_cast<double>(ui->spinFaxBlackHz->value()),
            static_cast<double>(ui->spinFaxWhiteHz->value()),
            800
            ));
    }

    if (modeName == SstvDecoder::modeName()) {
        return std::unique_ptr<TxModulator>(new SstvTransmitter(
            m_txSourceImage,
            ui->cmbSstvMode->currentText(),
            txSampleRate
            ));
    }

    if (modeName == RttyDecoder::modeName()) {
        const QString text = (m_txtRttyTx != nullptr)
                                 ? m_txtRttyTx->toPlainText()
                                 : QString();
        return std::unique_ptr<TxModulator>(new RttyTransmitter(
            text,
            txSampleRate,
            m_spinRttyBaud != nullptr ? m_spinRttyBaud->value() : 45.45,
            m_spinRttyMarkHz != nullptr ? static_cast<double>(m_spinRttyMarkHz->value()) : 2125.0,
            (m_spinRttyMarkHz != nullptr ? static_cast<double>(m_spinRttyMarkHz->value()) : 2125.0) +
                (m_spinRttyShiftHz != nullptr ? static_cast<double>(m_spinRttyShiftHz->value()) : 170.0),
            m_chkRttyReverse != nullptr && m_chkRttyReverse->isChecked()
            ));
    }

    if (modeName == Bpsk31Decoder::modeName()) {
        const QString text = (m_txtBpsk31Tx != nullptr)
                                 ? m_txtBpsk31Tx->toPlainText()
                                 : QString();
        const QString variant = (m_cmbBpsk31Variant != nullptr) ? m_cmbBpsk31Variant->currentData().toString() : QString("BPSK31");
        const double symbolRate = bpskSymbolRateForVariant(variant);
        return std::unique_ptr<TxModulator>(new Bpsk31Transmitter(
            text,
            txSampleRate,
            m_spinBpsk31ToneHz != nullptr ? static_cast<double>(m_spinBpsk31ToneHz->value()) : 1000.0,
            symbolRate,
            m_chkBpsk31Invert != nullptr && m_chkBpsk31Invert->isChecked(),
            pskVariantIsQpsk(variant)
            ));
    }

    if (modeName == MfskDecoder::modeName()) {
        const QString text = (m_txtMfskTx != nullptr)
                                 ? m_txtMfskTx->toPlainText()
                                 : QString();
        const QString variantKey = (m_cmbMfskVariant != nullptr) ? m_cmbMfskVariant->currentData().toString() : QString("MFSK16");
        return std::unique_ptr<TxModulator>(new MfskTransmitter(
            text,
            txSampleRate,
            m_spinMfskCenterHz != nullptr ? static_cast<double>(m_spinMfskCenterHz->value()) : 1000.0,
            MfskDecoder::variantFromKey(variantKey)
            ));
    }

    if (modeName == CwDecoder::modeName()) {
        const QString text = (m_txtCwTx != nullptr)
                                 ? m_txtCwTx->toPlainText()
                                 : QString();
        return std::unique_ptr<TxModulator>(new CwTransmitter(
            text,
            txSampleRate,
            m_spinCwToneHz != nullptr ? static_cast<double>(m_spinCwToneHz->value()) : 700.0,
            m_spinCwWpm != nullptr ? static_cast<double>(m_spinCwWpm->value()) : 20.0
            ));
    }

    if (modeName == HellschreiberDecoder::modeName()) {
        const QString text = (m_txtHellTx != nullptr)
                                 ? m_txtHellTx->toPlainText()
                                 : QString();
        const HellschreiberDecoder::Variant variant = (m_cmbHellVariant != nullptr)
                                                         ? HellschreiberDecoder::variantFromKey(m_cmbHellVariant->currentData().toString())
                                                         : HellschreiberDecoder::Variant::FeldHell;
        return std::unique_ptr<TxModulator>(new HellschreiberTransmitter(
            text,
            txSampleRate,
            m_spinHellToneHz != nullptr ? static_cast<double>(m_spinHellToneHz->value()) : 1000.0,
            m_spinHellColumnRate != nullptr ? m_spinHellColumnRate->value() : 17.5,
            variant,
            HellschreiberDecoder::fsk105ShiftHz(),
            hellPaperScale()
            ));
    }

    if (Msk144Mode::isMode(modeName)) {
        QString msg;
        int row = (m_tableMsk144TxMessages != nullptr) ? m_tableMsk144TxMessages->currentRow() : -1;
        if (row < 0) row = 0;
        if (m_tableMsk144TxMessages != nullptr && row >= 0 && row < m_tableMsk144TxMessages->rowCount()) {
            QTableWidgetItem *item = m_tableMsk144TxMessages->item(row, 1);
            if (item != nullptr) msg = item->text().trimmed();
        }
        if (msg.isEmpty()) {
            msg = QStringLiteral("CQ %1 %2").arg(stationCallsign(), stationLocator().left(4)).trimmed();
        }
        const int period = (m_cmbMsk144Period != nullptr) ? m_cmbMsk144Period->currentData().toInt() : 15;
        Msk144Transmitter *msk = new Msk144Transmitter(msg, txSampleRate, period, false, 1500.0);
        if (!msk->generationSucceeded()) {
            appendLog(QStringLiteral("MSK144 TX generator failed: %1").arg(msk->generationError()));
            delete msk;
            return nullptr;
        }
        return std::unique_ptr<TxModulator>(msk);
    }

    if (Q65Mode::isFamilyMode(modeName)) {
        QString msg;
        int row = (m_tableQ65TxMessages != nullptr) ? m_tableQ65TxMessages->currentRow() : -1;
        if (row < 0) row = 0;
        if (m_tableQ65TxMessages != nullptr && row >= 0 && row < m_tableQ65TxMessages->rowCount()) {
            QTableWidgetItem *item = m_tableQ65TxMessages->item(row, 1);
            if (item != nullptr) msg = item->text().trimmed();
        }
        if (msg.isEmpty()) {
            msg = QStringLiteral("CQ %1 %2").arg(stationCallsign(), stationLocator().left(4)).trimmed();
        }
        const int period = (m_cmbQ65Period != nullptr) ? m_cmbQ65Period->currentData().toInt() : 60;
        const double txHz = (m_spinQ65TxFreq != nullptr) ? static_cast<double>(m_spinQ65TxFreq->value()) : 1500.0;
        Q65Transmitter *q65 = new Q65Transmitter(msg, txSampleRate, period, currentQ65Submode(), txHz);
        if (!q65->generationSucceeded()) {
            appendLog(QStringLiteral("Q65 TX generator failed: %1").arg(q65->generationError()));
            delete q65;
            return nullptr;
        }
        return std::unique_ptr<TxModulator>(q65);
    }

    if (Ft8Mode::isFamilyMode(modeName)) {
        const Ft8Mode::Profile profile = Ft8Mode::profileForMode(modeName);
        const double txHz = (m_spinFt8TxFreq != nullptr)
                                ? static_cast<double>(m_spinFt8TxFreq->value())
                                : 1500.0;
        if (m_pendingFt8Tune) {
            Ft8Transmitter *tone = new Ft8Transmitter(profile.modeName, 48000, txHz, static_cast<double>(profile.slotMs) / 1000.0, true);
            if (!tone->generationSucceeded()) {
                delete tone;
                return nullptr;
            }
            return std::unique_ptr<TxModulator>(tone);
        }

        if (!profile.interoperableCoreAvailable) {
            return nullptr;
        }

        int leadingSilenceMs = m_pendingFt8PreSilenceMs;
        int skipMs = 0;
        if (m_pendingFt8SlotBoundaryUtcMs > 0 && m_pendingFt8AudioTargetDelayMs >= 0) {
            const qint64 nowMs = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
            const qint64 targetMs = m_pendingFt8SlotBoundaryUtcMs + m_pendingFt8AudioTargetDelayMs;
            const qint64 deltaMs = targetMs - nowMs;
            if (deltaMs >= 0) {
                leadingSilenceMs = qBound(0, static_cast<int>(deltaMs), 2000);
            } else {
                leadingSilenceMs = 0;
                skipMs = qBound(0, static_cast<int>(-deltaMs), profile.slotMs);
            }
        }

        std::unique_ptr<TxModulator> modulator;
        if (m_pendingFt8PreparedModulator) {
            modulator = std::move(m_pendingFt8PreparedModulator);
            appendLog(QString("FT timing: using prebuilt %1 waveform.").arg(profile.shortLabel));
        } else {
            Ft8Transmitter *ft8 = new Ft8Transmitter(
                profile.modeName,
                m_pendingFt8TxMessage,
                48000,
                txHz,
                0
                );
            if (!ft8->generationSucceeded()) {
                delete ft8;
                return nullptr;
            }
            modulator.reset(ft8);
        }

        Ft8Transmitter *ft8 = dynamic_cast<Ft8Transmitter *>(modulator.get());
        if (ft8 != nullptr) {
            if (skipMs > 0) {
                ft8->skipInitialMilliseconds(skipMs);
            }
            if (leadingSilenceMs > 0) {
                ft8->prependLeadingSilenceMilliseconds(leadingSilenceMs);
            }
            appendLog(QString("FT timing: target=%1 ms, lead-in silence=%2 ms, skipped=%3 ms.")
                          .arg(m_pendingFt8AudioTargetDelayMs)
                          .arg(leadingSilenceMs)
                          .arg(skipMs));
        }
        return modulator;
    }

    return nullptr;
}

void MainWindow::updateTxPreview()
{
    if (m_lblTxMode == nullptr || m_lblTxImageName == nullptr) {
        return;
    }

    const QString modeName = ui->cmbMode->currentText();

    if (modeName == WeatherFaxDecoder::modeName()) {
        m_lblTxMode->setText(QString("WEFAX TX: %1 LPM, %2/%3 Hz")
                                 .arg(ui->cmbFaxLpm->currentText())
                                 .arg(ui->spinFaxBlackHz->value())
                                 .arg(ui->spinFaxWhiteHz->value()));

        if (!m_txSourceImage.isNull() && m_txImageOwnerMode == modeName) {
            m_txPreparedImage = WeatherFaxTransmitter::prepareImage(m_txSourceImage, 800);
        } else {
            m_txPreparedImage = QImage();
        }
    } else if (modeName == SstvDecoder::modeName()) {
        m_lblTxMode->setText("SSTV TX: " + ui->cmbSstvMode->currentText());

        if (!m_txSourceImage.isNull() && m_txImageOwnerMode == modeName) {
            m_txPreparedImage = SstvTransmitter::prepareImage(m_txSourceImage,
                                                              ui->cmbSstvMode->currentText());
            if (m_lblSstvTxPreview != nullptr) {
                m_lblSstvTxPreview->setPixmap(QPixmap::fromImage(m_txPreparedImage).scaled(
                                                    m_lblSstvTxPreview->size(),
                                                    Qt::KeepAspectRatio,
                                                    Qt::SmoothTransformation));
                m_lblSstvTxPreview->setText(QString());
            }
        }
    } else if (modeName == RttyDecoder::modeName()) {
        const int mark = m_spinRttyMarkHz != nullptr ? m_spinRttyMarkHz->value() : 2125;
        const int shift = m_spinRttyShiftHz != nullptr ? m_spinRttyShiftHz->value() : 170;
        const double baud = m_spinRttyBaud != nullptr ? m_spinRttyBaud->value() : 45.45;
        m_lblTxMode->setText(QString("RTTY TX: %1 baud, %2/%3 Hz")
                                 .arg(baud, 0, 'f', 2)
                                 .arg(mark)
                                 .arg(mark + shift));
        m_txPreparedImage = QImage();
    } else if (modeName == Bpsk31Decoder::modeName()) {
        const int tone = m_spinBpsk31ToneHz != nullptr ? m_spinBpsk31ToneHz->value() : 1000;
        const QString variant = (m_cmbBpsk31Variant != nullptr) ? m_cmbBpsk31Variant->currentData().toString() : QString("BPSK31");
        m_lblTxMode->setText(QString("%1 TX: %2 Hz").arg(variant.isEmpty() ? QString("BPSK31") : variant).arg(tone));
        m_txPreparedImage = QImage();
    } else if (modeName == MfskDecoder::modeName()) {
        const int center = m_spinMfskCenterHz != nullptr ? m_spinMfskCenterHz->value() : 1000;
        const QString variant = (m_cmbMfskVariant != nullptr) ? m_cmbMfskVariant->currentData().toString() : QString("MFSK16");
        m_lblTxMode->setText(QString("%1 TX: center %2 Hz").arg(variant.isEmpty() ? QString("MFSK16") : variant).arg(center));
        const QString text = (m_txtMfskTx != nullptr) ? m_txtMfskTx->toPlainText() : QString();
        m_txPreparedImage = MfskTransmitter::previewTextImage(text, MfskDecoder::variantFromKey(variant));
    } else if (modeName == HellschreiberDecoder::modeName()) {
        const int tone = m_spinHellToneHz != nullptr ? m_spinHellToneHz->value() : 1000;
        const double columnRate = m_spinHellColumnRate != nullptr ? m_spinHellColumnRate->value() : 17.5;
        const HellschreiberDecoder::Variant variant = (m_cmbHellVariant != nullptr)
                                                         ? HellschreiberDecoder::variantFromKey(m_cmbHellVariant->currentData().toString())
                                                         : HellschreiberDecoder::Variant::FeldHell;
        m_lblTxMode->setText(QString("%1 TX: %2 Hz, %3 col/s")
                                 .arg(HellschreiberDecoder::variantName(variant))
                                 .arg(tone)
                                 .arg(columnRate, 0, 'f', 2));
        /* Hell TX no longer opens a separate preview page.  The actual local TX
         * echo is appended in red to the same paper when TX starts.
         */
        m_txPreparedImage = QImage();
    } else if (modeName == CwDecoder::modeName()) {
        const int tone = m_spinCwToneHz != nullptr ? m_spinCwToneHz->value() : 700;
        const int wpm = m_spinCwWpm != nullptr ? m_spinCwWpm->value() : 20;
        m_lblTxMode->setText(QString("CW TX/RX: %1 Hz, %2 WPM").arg(tone).arg(wpm));
        const QString text = (m_txtCwTx != nullptr) ? m_txtCwTx->toPlainText() : QString();
        m_txPreparedImage = CwTransmitter::previewTextImage(text);
    } else if (Msk144Mode::isMode(modeName)) {
        const int period = (m_cmbMsk144Period != nullptr) ? m_cmbMsk144Period->currentData().toInt() : 15;
        const int rx = (m_spinMsk144RxFreq != nullptr) ? m_spinMsk144RxFreq->value() : 1500;
        m_lblTxMode->setText(QStringLiteral("MSK144: RX %1 Hz, TX center 1500 Hz, %2 s").arg(rx).arg(period));
        m_txPreparedImage = QImage();
    } else if (Q65Mode::isFamilyMode(modeName)) {
        const int period = (m_cmbQ65Period != nullptr) ? m_cmbQ65Period->currentData().toInt() : 60;
        const int rx = (m_spinQ65RxFreq != nullptr) ? m_spinQ65RxFreq->value() : 1500;
        const int tx = (m_spinQ65TxFreq != nullptr) ? m_spinQ65TxFreq->value() : 1500;
        m_lblTxMode->setText(QStringLiteral("%1: RX %2 Hz, TX %3 Hz, %4 s").arg(Q65Mode::modeName(currentQ65Submode())).arg(rx).arg(tx).arg(period));
        m_txPreparedImage = QImage();
    } else if (Ft8Mode::isFamilyMode(modeName)) {
        const Ft8Mode::Profile profile = Ft8Mode::profileForMode(modeName);
        const int rx = m_spinFt8RxFreq != nullptr ? m_spinFt8RxFreq->value() : 1500;
        const int tx = m_spinFt8TxFreq != nullptr ? m_spinFt8TxFreq->value() : 1500;
        m_lblTxMode->setText(QString("%1: RX %2 Hz, TX %3 Hz, %4")
                                 .arg(profile.shortLabel)
                                 .arg(rx)
                                 .arg(tx)
                                 .arg(profile.interoperableCoreAvailable ? QStringLiteral("MSHV TX/RX core") : QStringLiteral("core unavailable")));
        m_txPreparedImage = QImage();
    } else {
        m_lblTxMode->setText("TX mode unavailable");
        m_txPreparedImage = QImage();
    }

    if (modeName == RttyDecoder::modeName()) {
        const int chars = (m_txtRttyTx != nullptr) ? m_txtRttyTx->toPlainText().size() : 0;
        m_lblTxImageName->setText(QString("RTTY text: %1 char(s)").arg(chars));
    } else if (modeName == Bpsk31Decoder::modeName()) {
        const int chars = (m_txtBpsk31Tx != nullptr) ? m_txtBpsk31Tx->toPlainText().size() : 0;
        m_lblTxImageName->setText(QString("PSK text: %1 char(s)").arg(chars));
    } else if (modeName == MfskDecoder::modeName()) {
        const int chars = (m_txtMfskTx != nullptr) ? m_txtMfskTx->toPlainText().size() : 0;
        const QString variant = (m_cmbMfskVariant != nullptr) ? m_cmbMfskVariant->currentData().toString() : QString("MFSK16");
        m_lblTxImageName->setText(QString("%1 text: %2 char(s)").arg(variant.isEmpty() ? QString("MFSK16") : variant).arg(chars));
    } else if (modeName == HellschreiberDecoder::modeName()) {
        const int chars = (m_txtHellTx != nullptr) ? m_txtHellTx->toPlainText().size() : 0;
        const HellschreiberDecoder::Variant variant = (m_cmbHellVariant != nullptr)
                                                         ? HellschreiberDecoder::variantFromKey(m_cmbHellVariant->currentData().toString())
                                                         : HellschreiberDecoder::Variant::FeldHell;
        m_lblTxImageName->setText(QString("%1 text: %2 char(s)")
                                      .arg(HellschreiberDecoder::variantName(variant))
                                      .arg(chars));
    } else if (modeName == CwDecoder::modeName()) {
        const int chars = (m_txtCwTx != nullptr) ? m_txtCwTx->toPlainText().size() : 0;
        m_lblTxImageName->setText(QString("CW text: %1 char(s)").arg(chars));
    } else if (Msk144Mode::isMode(modeName)) {
        QString msg;
        const int row = (m_tableMsk144TxMessages != nullptr) ? qMax(0, m_tableMsk144TxMessages->currentRow()) : 0;
        if (m_tableMsk144TxMessages != nullptr && row < m_tableMsk144TxMessages->rowCount() && m_tableMsk144TxMessages->item(row, 1) != nullptr) {
            msg = m_tableMsk144TxMessages->item(row, 1)->text();
        }
        m_lblTxImageName->setText(msg.trimmed().isEmpty() ? QStringLiteral("MSK144: no TX message selected")
                                                          : QStringLiteral("MSK144 selected: %1").arg(msg.left(48)));
    } else if (Q65Mode::isFamilyMode(modeName)) {
        QString msg;
        const int row = (m_tableQ65TxMessages != nullptr) ? qMax(0, m_tableQ65TxMessages->currentRow()) : 0;
        if (m_tableQ65TxMessages != nullptr && row < m_tableQ65TxMessages->rowCount() && m_tableQ65TxMessages->item(row, 1) != nullptr) {
            msg = m_tableQ65TxMessages->item(row, 1)->text();
        }
        m_lblTxImageName->setText(msg.trimmed().isEmpty() ? QStringLiteral("Q65: no TX message selected")
                                                          : QStringLiteral("%1 selected: %2").arg(Q65Mode::modeName(currentQ65Submode()), msg.left(48)));
    } else if (Ft8Mode::isFamilyMode(modeName)) {
        const Ft8Mode::Profile profile = Ft8Mode::profileForMode(modeName);
        const QString msg = selectedFt8TxMessage();
        m_lblTxImageName->setText(msg.isEmpty() ? QString("%1: no TX message selected").arg(profile.shortLabel)
                                                 : QString("%1 selected: %2").arg(profile.shortLabel, msg.left(48)));
    } else if (m_txSourceImage.isNull() || m_txImageOwnerMode != modeName) {
        m_lblTxImageName->setText("No TX image loaded");
        if (!m_rxRunning && !m_txRunning && m_faxImageWidget != nullptr) {
            m_faxImageWidget->clearTransmitProgress();
        }
    } else {
        QFileInfo info(m_txImageFileName);
        m_lblTxImageName->setText(QString("%1 → %2x%3")
                                      .arg(info.fileName())
                                      .arg(m_txPreparedImage.width())
                                      .arg(m_txPreparedImage.height()));

        if (!m_rxRunning && !m_txRunning && m_faxImageWidget != nullptr) {
            m_faxImageWidget->clearTransmitProgress();
            m_faxImageWidget->setImage(m_txPreparedImage);
        }
    }

    updateTxControlState();
}

void MainWindow::updateTxControlState()
{
    if (m_btnLoadTxImage == nullptr ||
        m_btnStartImageTx == nullptr ||
        m_btnStopImageTx == nullptr ||
        m_progressTx == nullptr) {
        return;
    }

    const QString modeName = ui->cmbMode->currentText();
    const bool rttyMode = modeName == RttyDecoder::modeName();
    const bool bpskMode = modeName == Bpsk31Decoder::modeName();
    const bool mfskMode = modeName == MfskDecoder::modeName();
    const bool hellMode = modeName == HellschreiberDecoder::modeName();
    const bool cwMode = modeName == CwDecoder::modeName();
    const bool sstvMode = modeName == SstvDecoder::modeName();
    const bool ft8Mode = Ft8Mode::isFamilyMode(modeName);
    const bool msk144Mode = Msk144Mode::isMode(modeName);
    const bool q65Mode = Q65Mode::isFamilyMode(modeName);
    const bool textMode = rttyMode || bpskMode || mfskMode || cwMode || hellMode || msk144Mode || q65Mode;
    const bool rxOnlyTextMode = ft8Mode;
    const bool hasImage = !m_txSourceImage.isNull() && m_txImageOwnerMode == modeName;
    const bool hasRttyText = rttyMode && m_txtRttyTx != nullptr && !m_txtRttyTx->toPlainText().trimmed().isEmpty();
    const bool hasBpskText = bpskMode && m_txtBpsk31Tx != nullptr && !m_txtBpsk31Tx->toPlainText().trimmed().isEmpty();
    const bool hasMfskText = mfskMode && m_txtMfskTx != nullptr && !m_txtMfskTx->toPlainText().trimmed().isEmpty();
    const bool hasCwText = cwMode && m_txtCwTx != nullptr && !m_txtCwTx->toPlainText().trimmed().isEmpty();
    const bool hasHellText = hellMode && m_txtHellTx != nullptr && !m_txtHellTx->toPlainText().trimmed().isEmpty();
    const bool hasMskMessage = msk144Mode && m_tableMsk144TxMessages != nullptr && m_tableMsk144TxMessages->rowCount() > 0;
    const bool hasQ65Message = q65Mode && m_tableQ65TxMessages != nullptr && m_tableQ65TxMessages->rowCount() > 0;
    const bool hasSource = textMode ? (hasRttyText || hasBpskText || hasMfskText || hasCwText || hasHellText || hasMskMessage || hasQ65Message) : hasImage;
    const bool rxBusy = m_rxRunning || (m_audioEngine != nullptr && m_audioEngine->isRunning());
    const bool canStartTx = textMode
                                ? (hasSource && !m_txRunning && !m_offlineAnalysisActive)
                                : (hasSource && !m_txRunning && !rxBusy && !m_offlineAnalysisActive);

    if (m_grpTxImage != nullptr) {
        // Keep the image-TX panel visually compact; the surrounding Mode tab
        // already identifies the current modem, so no extra group title is needed.
        m_grpTxImage->setTitle(QString());
        m_grpTxImage->setVisible(!textMode && !rxOnlyTextMode);
    }
    if (m_btnShowRuntimeLog != nullptr) {
        m_btnShowRuntimeLog->setVisible(ft8Mode || msk144Mode || q65Mode);
    }

    m_btnLoadTxImage->setVisible(!textMode && !rxOnlyTextMode);
    m_btnLoadTxImage->setEnabled(!m_txRunning && !textMode && !rxOnlyTextMode);
    if (m_btnSstvEditor != nullptr) {
        m_btnSstvEditor->setVisible(sstvMode);
        m_btnSstvEditor->setEnabled(sstvMode && !m_txRunning);
    }
    if (m_lblSstvTxPreview != nullptr) {
        m_lblSstvTxPreview->setVisible(sstvMode && hasImage);
    }
    if (m_editSstvTxCall != nullptr) {
        m_editSstvTxCall->setVisible(sstvMode);
        m_editSstvTxName->setVisible(sstvMode);
        m_editSstvTxQth->setVisible(sstvMode);
        m_editSstvTxReport->setVisible(sstvMode);
        if (m_lblSstvTxCall != nullptr) m_lblSstvTxCall->setVisible(sstvMode);
        if (m_lblSstvTxName != nullptr) m_lblSstvTxName->setVisible(sstvMode);
        if (m_lblSstvTxQth != nullptr) m_lblSstvTxQth->setVisible(sstvMode);
        if (m_lblSstvTxInfo != nullptr) m_lblSstvTxInfo->setVisible(sstvMode);
    }
    m_btnStartImageTx->setVisible(!textMode && !rxOnlyTextMode);
    m_btnStartImageTx->setText(sstvMode ? uiText("button.sendSstv", "Send SSTV") : uiText("button.startImageTx", "Start image TX"));
    m_btnStartImageTx->setEnabled(!textMode && !rxOnlyTextMode && canStartTx);
    m_btnStopImageTx->setVisible(!textMode && !rxOnlyTextMode);
    m_btnStopImageTx->setEnabled(!textMode && !rxOnlyTextMode && m_txRunning);
    if (ui->btnTxTone != nullptr) {
        ui->btnTxTone->setText(m_txRunning ? uiText("button.transport_tx_stop", "■ TX")
                                           : uiText("button.transport_tx", "● TX"));
        ui->btnTxTone->setEnabled(!rxOnlyTextMode && (m_txRunning || canStartTx));
    }

    if (m_btnRttySend != nullptr) {
        m_btnRttySend->setText(m_txRunning && rttyMode ? QString::fromUtf8("■") : QString::fromUtf8("➤"));
        m_btnRttySend->setToolTip(m_txRunning && rttyMode ? "Stop the current RTTY transmission." : "Transmit the text typed in the input box.");
        m_btnRttySend->setEnabled(rttyMode && (m_txRunning || (hasRttyText && !m_offlineAnalysisActive)));
    }

    if (m_btnBpsk31Send != nullptr) {
        m_btnBpsk31Send->setText(m_txRunning && bpskMode ? QString::fromUtf8("■") : QString::fromUtf8("➤"));
        m_btnBpsk31Send->setToolTip(m_txRunning && bpskMode ? "Stop the current PSK transmission." : "Transmit the text typed in the input box.");
        m_btnBpsk31Send->setEnabled(bpskMode && (m_txRunning || (hasBpskText && !m_offlineAnalysisActive)));
    }

    if (m_btnMfskSend != nullptr) {
        m_btnMfskSend->setText(m_txRunning && mfskMode ? QString::fromUtf8("■") : QString::fromUtf8("➤"));
        m_btnMfskSend->setToolTip(m_txRunning && mfskMode ? "Stop the current MFSK transmission." : "Transmit the MFSK text typed in the input box.");
        m_btnMfskSend->setEnabled(mfskMode && (m_txRunning || (hasMfskText && !m_offlineAnalysisActive)));
    }

    if (m_btnCwSend != nullptr) {
        m_btnCwSend->setText(m_txRunning && cwMode ? QString::fromUtf8("■") : QString::fromUtf8("➤"));
        m_btnCwSend->setToolTip(m_txRunning && cwMode ? "Stop the current CW transmission." : "Transmit the CW text typed in the input box.");
        m_btnCwSend->setEnabled(cwMode && (m_txRunning || (hasCwText && !m_offlineAnalysisActive)));
    }

    if (m_btnHellSend != nullptr) {
        m_btnHellSend->setText(m_txRunning && hellMode ? QString::fromUtf8("■") : QString::fromUtf8("➤"));
        m_btnHellSend->setToolTip(m_txRunning && hellMode ? "Stop the current Hellschreiber transmission." : "Transmit the Hellschreiber text typed in the input box.");
        m_btnHellSend->setEnabled(hellMode && (m_txRunning || (hasHellText && !m_offlineAnalysisActive)));
    }

    for (QPushButton *button : m_rttyMacroButtons) {
        if (button != nullptr) {
            button->setEnabled(rttyMode && !m_txRunning && !m_offlineAnalysisActive);
        }
    }

    for (QPushButton *button : m_bpsk31MacroButtons) {
        if (button != nullptr) {
            button->setEnabled(bpskMode && !m_txRunning && !m_offlineAnalysisActive);
        }
    }

    for (QPushButton *button : m_mfskMacroButtons) {
        if (button != nullptr) {
            button->setEnabled(mfskMode && !m_txRunning && !m_offlineAnalysisActive);
        }
    }

    for (QPushButton *button : m_cwMacroButtons) {
        if (button != nullptr) {
            button->setEnabled(cwMode && !m_txRunning && !m_offlineAnalysisActive);
        }
    }

    for (QPushButton *button : m_hellMacroButtons) {
        if (button != nullptr) {
            button->setEnabled(hellMode && !m_txRunning && !m_offlineAnalysisActive);
        }
    }

    if (!m_txRunning && m_progressTx->value() == 100) {
        updateFt8TxBannerUi();
        return;
    }

    if (!m_txRunning && !hasSource) {
        m_progressTx->setValue(0);
    }
    updateFt8TxBannerUi();
}

bool MainWindow::keyPttForTx()
{
    if (!ensureStationIdentityForTx(QStringLiteral("TX/PTT"))) {
        return false;
    }

    const QString pttMethod = m_settings.pttMethod.trimmed().toLower();
    const QString pttPort = selectedPttPort().trimmed();
    const bool pttUsesCatPort = pttPort.isEmpty() ||
                                pttPort.compare(QStringLiteral("CAT"), Qt::CaseInsensitive) == 0 ||
                                pttPort.compare(m_settings.hamlibSerialPath.trimmed(), Qt::CaseInsensitive) == 0;
    const bool pttViaRigController = (pttMethod == QStringLiteral("cat_hamlib")) ||
                                     ((pttMethod == QStringLiteral("serial_rts") || pttMethod == QStringLiteral("serial_dtr")) &&
                                      m_settings.hamlibCatEnabled && pttUsesCatPort);
    if (pttViaRigController) {
        if (m_rigController == nullptr) {
            const QString reason = uiText("tx_ptt_hamlib_controller_missing",
                                          "CAT/Hamlib PTT is selected, but the Hamlib controller is not available. Check Settings -> Audio/PTT + CAT, radio model, CAT port and PTT method.");
            appendLog("TX PTT failed: " + reason);
            showTxBlockedWarning(QStringLiteral("TX/PTT"),
                                 reason,
                                 AppSettingsDialog::InitialPage::RadioCat,
                                 uiText("open_audio_cat_settings", "Open Audio/PTT + CAT settings"));
            return false;
        }
        if (!invokeRigPttBlocking(true)) {
            const QString detail = m_rigController->lastStatus().trimmed();
            const QString reason = detail.isEmpty()
                ? uiText("tx_ptt_hamlib_key_failed",
                         "CAT/Hamlib PTT is selected, but the radio did not accept the TX/PTT command. Check the rig model, serial port, baud rate, CAT connection and PTT method.")
                : uiText("tx_ptt_hamlib_key_failed_detail",
                         "CAT/Hamlib PTT is selected, but the radio did not accept the TX/PTT command: %1").arg(detail);
            appendLog("TX PTT failed: " + reason);
            showTxBlockedWarning(QStringLiteral("TX/PTT"),
                                 reason,
                                 AppSettingsDialog::InitialPage::RadioCat,
                                 uiText("open_audio_cat_settings", "Open Audio/PTT + CAT settings"));
            return false;
        }
        appendLog(QString("Hamlib CAT PTT ON (%1).").arg(m_settings.hamlibTxAudioRoute.isEmpty() ? QStringLiteral("default") : m_settings.hamlibTxAudioRoute));
        return true;
    }

    if (pttMethod == QStringLiteral("none")) {
        appendLog("TX PTT disabled by Audio/PTT settings; transmitting audio only.");
        return true;
    }

    const bool useDtr = (pttMethod == QStringLiteral("serial_dtr"));
    const QString portName = pttPort;

    if (portName.isEmpty()) {
        const QString reason = uiText("tx_ptt_serial_port_missing",
                                      "Serial PTT is selected, but no RTS/DTR serial port is configured. Select a serial PTT port in Settings, or set PTT method to None only if you intentionally want audio-only TX.");
        appendLog("TX PTT failed: " + reason);
        showTxBlockedWarning(QStringLiteral("TX/PTT"),
                             reason,
                             AppSettingsDialog::InitialPage::AudioPtt,
                             uiText("open_audio_ptt_settings", "Open Audio/PTT settings"));
        return false;
    }

    if (m_pttSerial.isOpen()) {
        m_pttSerial.setRequestToSend(false);
        m_pttSerial.setDataTerminalReady(false);
        m_pttSerial.close();
    }

    m_pttSerial.setPortName(portName);
    m_pttSerial.setBaudRate(QSerialPort::Baud9600);
    m_pttSerial.setDataBits(QSerialPort::Data8);
    m_pttSerial.setParity(QSerialPort::NoParity);
    m_pttSerial.setStopBits(QSerialPort::OneStop);
    m_pttSerial.setFlowControl(QSerialPort::NoFlowControl);

    if (!m_pttSerial.open(QIODevice::ReadWrite)) {
        const QString reason = uiText("tx_ptt_serial_open_failed",
                                      "Serial PTT could not open %1: %2").arg(portName, m_pttSerial.errorString());
        appendLog("TX PTT failed: " + reason);
        showTxBlockedWarning(QStringLiteral("TX/PTT"),
                             reason,
                             AppSettingsDialog::InitialPage::AudioPtt,
                             uiText("open_audio_ptt_settings", "Open Audio/PTT settings"));
        return false;
    }

    const bool ok = useDtr ? m_pttSerial.setDataTerminalReady(true)
                           : m_pttSerial.setRequestToSend(true);
    if (!ok) {
        const QString reason = uiText("tx_ptt_serial_line_failed",
                                      "Serial PTT could not assert %1 on %2: %3")
            .arg(useDtr ? QStringLiteral("DTR") : QStringLiteral("RTS"), portName, m_pttSerial.errorString());
        appendLog("TX PTT failed: " + reason);
        showTxBlockedWarning(QStringLiteral("TX/PTT"),
                             reason,
                             AppSettingsDialog::InitialPage::AudioPtt,
                             uiText("open_audio_ptt_settings", "Open Audio/PTT settings"));
        m_pttSerial.close();
        return false;
    }

    appendLog(QString("TX %1 ON on %2.").arg(useDtr ? QStringLiteral("DTR") : QStringLiteral("RTS"), portName));
    return true;
}

void MainWindow::unkeyPttAfterTx()
{
    const QString pttMethod = m_settings.pttMethod.trimmed().toLower();
    const QString pttPort = selectedPttPort().trimmed();
    const bool pttUsesCatPort = pttPort.isEmpty() ||
                                pttPort.compare(QStringLiteral("CAT"), Qt::CaseInsensitive) == 0 ||
                                pttPort.compare(m_settings.hamlibSerialPath.trimmed(), Qt::CaseInsensitive) == 0;
    const bool pttViaRigController = (pttMethod == QStringLiteral("cat_hamlib")) ||
                                     ((pttMethod == QStringLiteral("serial_rts") || pttMethod == QStringLiteral("serial_dtr")) &&
                                      m_settings.hamlibCatEnabled && pttUsesCatPort);
    if (pttViaRigController && m_rigController != nullptr) {
        invokeRigPttBlocking(false);
        appendLog("Hamlib CAT PTT OFF.");
    }

    if (!m_pttSerial.isOpen()) {
        return;
    }

    m_pttSerial.setRequestToSend(false);
    m_pttSerial.setDataTerminalReady(false);
    m_pttSerial.close();
    appendLog("TX serial PTT OFF.");
}

QString MainWindow::selectedAudioOutputName() const
{
    if (!m_settings.audioOutputName.trimmed().isEmpty()) {
        return m_settings.audioOutputName;
    }

    const QString backendName = ui->cmbAudioOutput->currentData().toString();

    if (!backendName.isEmpty()) {
        return backendName;
    }

    return "default";
}

QString MainWindow::selectedAudioOutputLabel() const
{
    const QString backendName = selectedAudioOutputName();
    const int index = ui->cmbAudioOutput->findData(backendName);

    if (index >= 0) {
        return ui->cmbAudioOutput->itemText(index);
    }

    if (backendName == "default") {
        return "Default";
    }

    return backendName;
}

void MainWindow::loadTxImage()
{
    if (m_txRunning) {
        appendLog("Load TX image blocked: TX is active.");
        return;
    }

    const QString fileName = QFileDialog::getOpenFileName(
        this,
        "Load image for TX",
        QDir::homePath(),
        "Images (*.png *.jpg *.jpeg *.bmp);;All files (*)"
        );

    if (fileName.isEmpty()) {
        return;
    }

    QImageReader reader(fileName);
    reader.setAutoTransform(true);

    const QImage image = reader.read();

    if (image.isNull()) {
        QMessageBox::warning(this,
                             "Load TX image failed",
                             "Unable to load the selected image file.");
        appendLog("TX image load failed: " + reader.errorString());
        return;
    }

    m_txSourceImage = image.convertToFormat(QImage::Format_RGB32);
    m_txImageOwnerMode = (ui->cmbMode != nullptr) ? ui->cmbMode->currentText() : QString();
    if (ui->cmbMode != nullptr && ui->cmbMode->currentText() == SstvDecoder::modeName()) {
        m_sstvTxBaseImage = m_txSourceImage;
    }
    m_txImageFileName = fileName;
    m_progressTx->setValue(0);

    if (ui->cmbMode != nullptr && ui->cmbMode->currentText() == SstvDecoder::modeName()) {
        updateSstvTxPreparedImage();
    } else {
        updateTxPreview();
    }
    appendLog("TX image loaded: " + fileName);
}

void MainWindow::startImageTx()
{
    if (m_shutdownInProgress || m_runtimeShutdownComplete) {
        return;
    }

    if (m_txRunning) {
        appendLog("TX already running.");
        return;
    }

    if (m_offlineAnalysisActive) {
        appendLog("TX blocked: WAV analysis is active.");
        return;
    }

    const QString activeModeName = ui->cmbMode->currentText();
    if (!ensureStationIdentityForTx(activeModeName.trimmed().isEmpty() ? QStringLiteral("TX") : activeModeName)) {
        return;
    }

    const bool rttyMode = activeModeName == RttyDecoder::modeName();
    const bool bpskMode = activeModeName == Bpsk31Decoder::modeName();
    const bool mfskMode = activeModeName == MfskDecoder::modeName();
    const bool hellMode = activeModeName == HellschreiberDecoder::modeName();
    const bool cwMode = activeModeName == CwDecoder::modeName();
    const bool ft8Mode = Ft8Mode::isFamilyMode(activeModeName);
    const bool msk144Mode = Msk144Mode::isMode(activeModeName);
    const bool q65Mode = Q65Mode::isFamilyMode(activeModeName);
    const bool textMode = rttyMode || bpskMode || mfskMode || cwMode || hellMode || ft8Mode || msk144Mode || q65Mode;

    const bool liveRxRunning = m_rxRunning ||
                               (m_audioEngine != nullptr && m_audioEngine->isRunning());

    if (liveRxRunning && !textMode) {
        appendLog("TX blocked: stop RX before image/video transmission.");
        QMessageBox::information(this,
                                 "TX blocked",
                                 "Stop RX before starting WEFAX/SSTV image TX. Text modes can pause and resume RX automatically.");
        return;
    }

    if (rttyMode) {
        resetRttyTxScopeState();
    }

    if (!textMode && m_txSourceImage.isNull()) {
        QMessageBox::information(this,
                                 "Image TX",
                                 "Load an image before starting TX.");
        return;
    }

    if (rttyMode && (m_txtRttyTx == nullptr || m_txtRttyTx->toPlainText().trimmed().isEmpty())) {
        QMessageBox::information(this,
                                 "RTTY TX",
                                 "Enter or load text before starting RTTY TX.");
        return;
    }

    if (bpskMode && (m_txtBpsk31Tx == nullptr || m_txtBpsk31Tx->toPlainText().trimmed().isEmpty())) {
        QMessageBox::information(this,
                                 "PSK TX",
                                 "Enter or load text before starting PSK TX.");
        return;
    }

    if (mfskMode && (m_txtMfskTx == nullptr || m_txtMfskTx->toPlainText().trimmed().isEmpty())) {
        QMessageBox::information(this,
                                 "MFSK TX",
                                 "Enter or load text before starting MFSK TX.");
        return;
    }

    if (cwMode && (m_txtCwTx == nullptr || m_txtCwTx->toPlainText().trimmed().isEmpty())) {
        QMessageBox::information(this,
                                 "CW TX",
                                 "Enter or load text before starting CW TX.");
        return;
    }

    if (hellMode && (m_txtHellTx == nullptr || m_txtHellTx->toPlainText().trimmed().isEmpty())) {
        QMessageBox::information(this,
                                 "Hellschreiber TX",
                                 "Enter or load text before starting Hellschreiber TX.");
        return;
    }

    if (ft8Mode && !m_pendingFt8Tune && m_pendingFt8TxMessage.trimmed().isEmpty()) {
        const Ft8Mode::Profile profile = Ft8Mode::profileForMode(activeModeName);
        QMessageBox::information(this,
                                 profile.shortLabel + QStringLiteral(" TX"),
                                 uiText("ft8_select_message_before_tx", "Select or generate an FT8 message before starting TX."));
        return;
    }

    if (msk144Mode) {
        refreshMsk144StandardMessages();
        if (m_tableMsk144TxMessages == nullptr || m_tableMsk144TxMessages->rowCount() == 0) {
            QMessageBox::information(this, QStringLiteral("MSK144 TX"), QStringLiteral("Generate or select an MSK144 message before starting TX."));
            return;
        }
    }
    if (q65Mode) {
        refreshQ65StandardMessages();
        if (m_tableQ65TxMessages == nullptr || m_tableQ65TxMessages->rowCount() == 0) {
            QMessageBox::information(this, QStringLiteral("Q65 TX"), QStringLiteral("Generate or select a Q65 message before starting TX."));
            return;
        }
    }

    m_currentTxIsTextMode = textMode;
    m_returnToRxAfterTx = textMode;
    m_txFinishedNaturally = false;

    if (liveRxRunning && m_audioEngine != nullptr) {
        m_preserveTextTerminalOnNextRx = true;
        appendLog("Pausing RX for text TX.");
        m_audioEngine->stopInput();
    }

    if (activeModeName == WeatherFaxDecoder::modeName()) {
        applyWeatherFaxSettings();
    } else if (activeModeName == SstvDecoder::modeName()) {
        applySstvSettings();
    } else if (activeModeName == RttyDecoder::modeName()) {
        applyRttySettings();
    } else if (activeModeName == Bpsk31Decoder::modeName()) {
        applyBpsk31Settings();
    } else if (activeModeName == MfskDecoder::modeName()) {
        applyMfskSettings();
    } else if (activeModeName == CwDecoder::modeName()) {
        applyCwSettings();
    } else if (activeModeName == HellschreiberDecoder::modeName()) {
        applyHellSettings();
    } else if (Ft8Mode::isFamilyMode(activeModeName)) {
        applyFt8Settings();
    } else if (Msk144Mode::isMode(activeModeName)) {
        applyMsk144Settings();
    } else if (Q65Mode::isFamilyMode(activeModeName)) {
        applyQ65Settings();
    }

    std::unique_ptr<TxModulator> modulator = buildCurrentTxModulator();

    if (!modulator) {
        QMessageBox::warning(this,
                             "TX",
                             "Unable to create a transmitter for the active mode.");
        const bool restartRx = m_returnToRxAfterTx;
        m_returnToRxAfterTx = false;
        m_currentTxIsTextMode = false;
        if (restartRx) {
            QTimer::singleShot(250, this, [this]() { startRx(); });
        }
        return;
    }

    m_txPreparedImage = modulator->previewImage();

    /* Hellschreiber uses one continuous paper tape.  Do not replace the RX
     * paper with a separate TX preview; successful local TX is appended in red
     * on the same paper after the TX audio path starts.
     */
    if (hellMode) {
        m_txPreparedImage = QImage();
    }

    if (!textMode && m_faxImageWidget != nullptr) {
        m_faxImageWidget->setImage(m_txPreparedImage);
        m_faxImageWidget->setTransmitProgress(0.0);
    }

    resetDspEngine();

    if (m_waterfallWidget != nullptr) {
        m_waterfallWidget->clear();
    }

    updateWaterfallMarkers();

    const bool pttKeyed = keyPttForTx();
    if (!pttKeyed) {
        appendLog("TX aborted: PTT/safety gate did not allow transmission; no audio TX will be generated.");
        const bool restartRx = m_returnToRxAfterTx;
        m_returnToRxAfterTx = false;
        m_currentTxIsTextMode = false;
        if (restartRx) {
            QTimer::singleShot(250, this, [this]() { startRx(); });
        }
        return;
    }

    const QString description = modulator->description();
    const QString outputName = selectedAudioOutputName();
    const QString outputLabel = selectedAudioOutputLabel();

    appendLog("Starting TX: " + description);
    appendLog("Audio output: " + outputLabel);

    m_offlineAnalysisActive = false;
    m_txRunning = true;
    setReceiverRunning(false);
    m_progressTx->setValue(0);

    if (ft8Mode && m_ftTxWorker != nullptr) {
        m_ftTxWorkerRunning = true;
        appendLog("FT timing: handing prepared waveform to dedicated FT TX worker thread.");
        QMetaObject::invokeMethod(m_ftTxWorker,
                                  "startOutput",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, outputName),
                                  Q_ARG(TxModulator *, modulator.release()));
        return;
    }

    if (!m_txAudioEngine->startOutput(outputName, std::move(modulator))) {
        unkeyPttAfterTx();
        const bool restartRx = m_returnToRxAfterTx;
        m_txRunning = false;
        m_returnToRxAfterTx = false;
        m_currentTxIsTextMode = false;
        setReceiverRunning(false);
        const QString reason = uiText("tx_audio_start_failed",
                                      "TX audio output could not be started. Check Settings -> Audio/PTT, the selected TX audio device and operating-system audio permissions.");
        appendLog("TX start failed: " + reason);
        showTxBlockedWarning(QStringLiteral("TX"),
                             reason,
                             AppSettingsDialog::InitialPage::AudioPtt,
                             uiText("open_audio_ptt_settings", "Open Audio/PTT settings"));
        if (restartRx) {
            QTimer::singleShot(250, this, [this]() { startRx(); });
        }
    } else if (hellMode && m_hellDecoder != nullptr && m_txtHellTx != nullptr) {
        const HellschreiberDecoder::Variant variant = (m_cmbHellVariant != nullptr)
                                                         ? HellschreiberDecoder::variantFromKey(m_cmbHellVariant->currentData().toString())
                                                         : HellschreiberDecoder::Variant::FeldHell;
        const QImage txRaster = HellschreiberTransmitter::transmitRasterImage(m_txtHellTx->toPlainText(), variant);
        m_hellDecoder->appendTransmitRaster(txRaster);
    }
}

void MainWindow::prearmFtPreparedSlotTransmit()
{
    if (m_txRunning || m_ftTxWorkerRunning) {
        appendLog("FT TX pre-arm skipped: TX already running.");
        return;
    }

    if (m_offlineAnalysisActive) {
        appendLog("FT TX pre-arm blocked: WAV analysis is active.");
        return;
    }

    const QString activeModeName = (ui != nullptr && ui->cmbMode != nullptr)
        ? ui->cmbMode->currentText()
        : QStringLiteral("FT8");
    if (!Ft8Mode::isFamilyMode(activeModeName)) {
        appendLog("FT TX pre-arm cancelled: active mode is no longer FT4/FT8.");
        return;
    }

    const Ft8Mode::Profile profile = Ft8Mode::profileForMode(activeModeName);
    if (!m_pendingFt8Tune && !profile.interoperableCoreAvailable) {
        appendLog("FT TX pre-arm cancelled: interoperable TX core unavailable for " + profile.shortLabel + ".");
        return;
    }
    if (!m_pendingFt8Tune && m_pendingFt8TxMessage.trimmed().isEmpty()) {
        appendLog("FT TX pre-arm cancelled: no pending FT message.");
        return;
    }

    m_currentTxIsTextMode = true;
    m_returnToRxAfterTx = true;
    m_txFinishedNaturally = false;
    m_preserveTextTerminalOnNextRx = true;

    // Keep RX audio alive until the exact TX boundary.  Stopping it here, at
    // pre-arm time, cut the last part of the RX slot and prevented the WSJT-X
    // gated live decode from launching when a TX was armed.

    if (!m_pendingFt8PttKeyed) {
        m_pendingFt8PttKeyed = keyPttForTx();
        if (!m_pendingFt8PttKeyed) {
            appendLog("FT TX aborted: PTT/safety gate did not allow transmission; no audio TX will be generated.");
            m_ft8PendingTxArmed = false;
            m_pendingFt8PttPrearmed = false;
            m_pendingFt8TxMessage.clear();
            m_pendingFt8TxTag.clear();
            updateFt8TxBannerUi();
            updateTxControlState();
            return;
        }
    }

    m_pendingFt8PttPrearmed = true;
    appendLog(QString("FT timing: PTT pre-armed for %1 slot; boundary=%2, audio target=%3 ms, PTT lead=%4 ms.")
                  .arg(profile.shortLabel)
                  .arg(m_pendingFt8SlotBoundaryUtcMs)
                  .arg(m_pendingFt8AudioTargetDelayMs)
                  .arg(m_pendingFt8PttLeadMs));
    updateTxControlState();
    updateFt8TxBannerUi();
}

void MainWindow::startFtPreparedSlotTransmit()
{
    if (m_shutdownInProgress || m_runtimeShutdownComplete) {
        return;
    }

    if (m_txRunning || m_ftTxWorkerRunning) {
        appendLog("FT TX already running.");
        return;
    }

    if (m_offlineAnalysisActive) {
        appendLog("FT TX blocked: WAV analysis is active.");
        return;
    }

    const QString activeModeName = (ui != nullptr && ui->cmbMode != nullptr)
        ? ui->cmbMode->currentText()
        : QStringLiteral("FT8");
    if (!Ft8Mode::isFamilyMode(activeModeName)) {
        appendLog("FT TX cancelled: active mode is no longer FT4/FT8.");
        return;
    }

    const Ft8Mode::Profile profile = Ft8Mode::profileForMode(activeModeName);
    if (!m_pendingFt8Tune && !profile.interoperableCoreAvailable) {
        appendLog("FT TX cancelled: interoperable TX core unavailable for " + profile.shortLabel + ".");
        return;
    }
    if (!m_pendingFt8Tune && m_pendingFt8TxMessage.trimmed().isEmpty()) {
        appendLog("FT TX cancelled: no pending FT message.");
        return;
    }

    QString rotatorWaitReason;
    int rotatorEtaMs = 0;
    if (!ftRotatorReadyForPendingTx(&rotatorWaitReason, &rotatorEtaMs)) {
        appendLog("FT TX blocked by rotator guard at audio-start; deferring to the next valid slot.");
        deferPendingFtTxForRotator(rotatorEtaMs, rotatorWaitReason);
        return;
    }

    if (!m_pendingFt8PttPrearmed) {
        appendLog("FT timing: audio-start event arrived before PTT pre-arm; pre-arming immediately.");
        prearmFtPreparedSlotTransmit();
    }

    const bool liveRxRunning = m_rxRunning ||
                               (m_audioEngine != nullptr && m_audioEngine->isRunning());
    if (liveRxRunning) {
        if (m_ft8RxDecoder != nullptr) {
            const Qt::ConnectionType finishConnection =
                (m_ft8RxDecoder->thread() == QThread::currentThread())
                    ? Qt::DirectConnection
                    : Qt::BlockingQueuedConnection;
            QMetaObject::invokeMethod(m_ft8RxDecoder,
                                      "noteTransmitStarting",
                                      finishConnection,
                                      Q_ARG(qint64, m_pendingFt8SlotBoundaryUtcMs));
        }
        if (m_audioEngine != nullptr) {
            appendLog("FT timing: RX audio stop requested at UTC boundary for FT TX.");
            m_audioEngine->stopInput();
        }
    }

    std::unique_ptr<TxModulator> modulator = buildCurrentTxModulator();
    if (!modulator) {
        appendLog("FT TX cancelled: unable to create prepared modulator.");
        const bool restartRx = m_returnToRxAfterTx;
        m_returnToRxAfterTx = false;
        m_currentTxIsTextMode = false;
        if (restartRx) {
            QTimer::singleShot(0, this, [this]() { startRx(); });
        }
        return;
    }

    m_txPreparedImage = modulator->previewImage();

    const QString outputName = selectedAudioOutputName();
    const QString outputLabel = selectedAudioOutputLabel();
    appendLog(QString("FT timing: UTC-scheduled %1 audio start; boundary=%2, PTT lead=%3 ms, target=%4 ms.")
                  .arg(profile.shortLabel)
                  .arg(m_pendingFt8SlotBoundaryUtcMs)
                  .arg(m_pendingFt8PttLeadMs)
                  .arg(m_pendingFt8AudioTargetDelayMs));
    appendLog("Audio output: " + outputLabel);

    m_offlineAnalysisActive = false;
    m_txRunning = true;
    setReceiverRunning(false);
    updateFt8TxBannerUi();
    if (m_progressTx != nullptr) {
        m_progressTx->setValue(0);
    }

    if (m_ftTxWorker != nullptr) {
        m_ftTxWorkerRunning = true;
        QMetaObject::invokeMethod(m_ftTxWorker,
                                  "startOutput",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, outputName),
                                  Q_ARG(TxModulator *, modulator.release()));
        updateTxControlState();
        return;
    }

    if (m_txAudioEngine == nullptr || !m_txAudioEngine->startOutput(outputName, std::move(modulator))) {
        unkeyPttAfterTx();
        const bool restartRx = m_returnToRxAfterTx;
        m_txRunning = false;
        m_returnToRxAfterTx = false;
        m_currentTxIsTextMode = false;
        m_pendingFt8PttPrearmed = false;
        m_pendingFt8PttKeyed = false;
        setReceiverRunning(false);
        const QString reason = uiText("ft_tx_audio_start_failed",
                                      "FT TX audio output could not be started. Check Settings -> Audio/PTT, the selected TX audio device and operating-system audio permissions.");
        appendLog("FT TX start failed: " + reason);
        showTxBlockedWarning(profile.shortLabel,
                             reason,
                             AppSettingsDialog::InitialPage::AudioPtt,
                             uiText("open_audio_ptt_settings", "Open Audio/PTT settings"));
        if (restartRx) {
            QTimer::singleShot(0, this, [this]() { startRx(); });
        }
    }
}


void MainWindow::stopImageTx()
{
    if (!m_txRunning &&
        !m_ftTxWorkerRunning &&
        (m_txAudioEngine == nullptr || !m_txAudioEngine->isRunning())) {
        appendLog("TX already stopped.");
        return;
    }

    m_returnToRxAfterTx = m_currentTxIsTextMode;
    m_txFinishedNaturally = false;

    if (m_ftTxWorkerRunning && m_ftTxWorker != nullptr) {
        QMetaObject::invokeMethod(m_ftTxWorker, "stopOutput", Qt::QueuedConnection);
        return;
    }

    if (m_txAudioEngine != nullptr) {
        m_txAudioEngine->stopOutput();
    }
}

void MainWindow::handleTxStarted()
{
    const bool ftTx = (ui != nullptr && ui->cmbMode != nullptr && Ft8Mode::isFamilyMode(ui->cmbMode->currentText()));
    const int txSampleRate = (ftTx && m_ftTxWorkerRunning) ? 48000 : (m_txAudioEngine != nullptr ? m_txAudioEngine->sampleRate() : 48000);
    appendLog(QString("TX audio started at %1 Hz%2.")
                  .arg(txSampleRate)
                  .arg(ftTx ? QStringLiteral(" (FT dedicated worker, slot-aligned low-latency path)") : QString()));
    if (ftTx) {
        appendLog(QString("FT timing: audio engine started; target delay=%1 ms, PTT lead=%2 ms.")
                      .arg(m_pendingFt8AudioTargetDelayMs)
                      .arg(m_pendingFt8PttLeadMs));
    }
    setReceiverRunning(false);
    updateTxControlState();
}

void MainWindow::handleTxStopped()
{
    m_ftTxWorkerRunning = false;
    unkeyPttAfterTx();

    const bool ftModeAtStop = Ft8Mode::isFamilyMode(ui->cmbMode->currentText());
    const qint64 completedFtTxSlotBoundaryMs = ftModeAtStop ? m_pendingFt8SlotBoundaryUtcMs : 0;
    const bool restartRx = m_returnToRxAfterTx;
    const bool naturalFinish = m_txFinishedNaturally;
    const bool textTx = m_currentTxIsTextMode;
    const bool completedFt8Tx = (ftModeAtStop &&
                                  naturalFinish && textTx &&
                                  !m_ftSession.lastTxWasTune &&
                                  !m_ftSession.lastTxMessage.trimmed().isEmpty());

    if (naturalFinish && textTx) {
        updateTextTxHighlight(1.0);
    }

    endTextTxHighlight();

    m_txRunning = false;
    m_returnToRxAfterTx = false;
    m_txFinishedNaturally = false;
    m_currentTxIsTextMode = false;

    if (naturalFinish) {
        appendLog("TX completed.");
    } else {
        appendLog("TX stopped.");
    }

    resetDspEngine();

    if (m_faxImageWidget != nullptr) {
        m_faxImageWidget->clearTransmitProgress();
    }

    if (Ft8Mode::isFamilyMode(ui->cmbMode->currentText())) {
        m_ft8PendingTxArmed = false;
        m_ft8PendingTxToken.clear();
        m_pendingFt8TxMessage.clear();
        m_pendingFt8TxTag.clear();
        m_pendingFt8Tune = false;
        m_pendingFt8PreSilenceMs = 0;
        m_pendingFt8SlotBoundaryUtcMs = 0;
        m_pendingFt8AudioTargetDelayMs = 0;
        m_pendingFt8PttLeadMs = 0;
        m_pendingFt8PreparedModulator.reset();
        m_pendingFt8TxPlan = FtTxPlan();
        m_pendingFt8PttPrearmed = false;
        m_pendingFt8PttKeyed = false;
    }

    if (ftModeAtStop && m_ft8RxDecoder != nullptr) {
        QMetaObject::invokeMethod(m_ft8RxDecoder,
                                  "noteTransmitEnded",
                                  Qt::QueuedConnection,
                                  Q_ARG(qint64, completedFtTxSlotBoundaryMs));
    }

    setReceiverRunning(false);
    updateTxPreview();
    updateTxControlState();

    if (Ft8Mode::isFamilyMode(ui->cmbMode->currentText()) && m_hasDeferredFt8TxPlan) {
        const QString deferredMessage = m_deferredFt8TxMessage.trimmed().toUpper();
        const QString deferredTag = m_deferredFt8TxTag.trimmed().isEmpty() ? QStringLiteral("SEQ") : m_deferredFt8TxTag.trimmed().toUpper();
        const Ft8SequencerState deferredState = m_ftSession.deferredState;
        m_hasDeferredFt8TxPlan = false;
        m_deferredFt8TxMessage.clear();
        m_deferredFt8TxTag.clear();
        m_deferredFt8TxPlan = FtTxPlan();
        m_ftSession.deferredState = Ft8SequencerState::Idle;
        if (!deferredMessage.isEmpty()) {
            m_ftSession.state = deferredState;
            appendLog("FT sequencer: arming deferred next-slot message after TX stop: " + deferredMessage);
            scheduleFt8SequencerMessage(deferredMessage, deferredTag);
        }
    } else if (completedFt8Tx) {
        handleFt8TxCompleted();
    }

    if (!m_pendingModeName.isEmpty()) {
        finishPendingModeChange();
        return;
    }

    if (restartRx) {
        if (textTx) {
            m_preserveTextTerminalOnNextRx = true;
            appendLog("Returning to RX after text TX.");
        } else {
            appendLog("Restarting RX after image TX.");
        }

        const bool ftLowLatencyReturn = Ft8Mode::isFamilyMode(ui->cmbMode->currentText());
        const int rxRestartDelayMs = ftLowLatencyReturn ? 0 : 250;
        if (ftLowLatencyReturn) {
            appendLog("FT timing: TX complete, PTT off requested, immediate RX restart requested.");
        }
        QTimer::singleShot(rxRestartDelayMs, this, [this]() {
            if (!m_txRunning && !m_offlineAnalysisActive) {
                startRx();
            }
        });
    }
}

void MainWindow::handleTxFinished()
{
    m_txFinishedNaturally = true;
    handleTxProgress(1.0);
}

void MainWindow::handleTxError(const QString &message)
{
    m_ftTxWorkerRunning = false;
    const bool ftModeAtError = Ft8Mode::isFamilyMode(ui->cmbMode->currentText());
    const qint64 erroredFtTxSlotBoundaryMs = ftModeAtError ? m_pendingFt8SlotBoundaryUtcMs : 0;
    appendLog("TX audio error: " + message);
    unkeyPttAfterTx();
    const bool restartRx = m_returnToRxAfterTx;
    endTextTxHighlight();
    m_txRunning = false;
    m_returnToRxAfterTx = false;
    m_currentTxIsTextMode = false;
    if (Ft8Mode::isFamilyMode(ui->cmbMode->currentText())) {
        m_ft8PendingTxArmed = false;
        m_ft8PendingTxToken.clear();
        m_pendingFt8TxMessage.clear();
        m_pendingFt8TxTag.clear();
        m_pendingFt8Tune = false;
        m_pendingFt8PreSilenceMs = 0;
        m_pendingFt8SlotBoundaryUtcMs = 0;
        m_pendingFt8AudioTargetDelayMs = 0;
        m_pendingFt8PttLeadMs = 0;
        m_pendingFt8PreparedModulator.reset();
        m_pendingFt8TxPlan = FtTxPlan();
        m_pendingFt8PttPrearmed = false;
        m_pendingFt8PttKeyed = false;
    }
    if (ftModeAtError && m_ft8RxDecoder != nullptr) {
        QMetaObject::invokeMethod(m_ft8RxDecoder,
                                  "noteTransmitEnded",
                                  Qt::QueuedConnection,
                                  Q_ARG(qint64, erroredFtTxSlotBoundaryMs));
    }
    resetDspEngine();
    if (m_faxImageWidget != nullptr) {
        m_faxImageWidget->clearTransmitProgress();
    }
    setReceiverRunning(false);
    updateTxPreview();
    updateTxControlState();

    if (!m_pendingModeName.isEmpty()) {
        finishPendingModeChange();
        return;
    }

    if (restartRx) {
        const bool ftLowLatencyReturn = Ft8Mode::isFamilyMode(ui->cmbMode->currentText());
        QTimer::singleShot(ftLowLatencyReturn ? 0 : 250, this, [this]() { startRx(); });
    }
}

void MainWindow::handleTxProgress(double progress)
{
    const int percent = qBound(0, static_cast<int>(qRound(progress * 100.0)), 100);

    if (m_progressTx != nullptr) {
        m_progressTx->setValue(percent);
    }

    if (m_currentTxIsTextMode) {
        updateTextTxHighlight(progress);
    }

    if (!m_currentTxIsTextMode && m_faxImageWidget != nullptr) {
        m_faxImageWidget->setTransmitProgress(progress);
    }
}


void MainWindow::resetRttyTxScopeState()
{
    // RTTY tuning scope is an RX-only instrument.  On TX start, just clear the
    // RX trace so a stale crossed ellipse is not mistaken for a live decode.
    if (m_rttyScopeWidget != nullptr) {
        m_rttyScopeWidget->setTrace(QVector<QPointF>(), 0.0, false);
    }
}


void MainWindow::testPtt()
{
    if (m_pttTestTimer.isActive()) {
        m_pttTestTimer.stop();
        finishPttTest();
        return;
    }

    if (m_rxRunning) {
        const QString reason = uiText("ptt_test_rx_running", "PTT test is blocked while RX is running. Stop RX first, then try the PTT test again.");
        appendLog(QStringLiteral("PTT test blocked: ") + reason);
        showTxBlockedWarning(QStringLiteral("PTT TEST"),
                             reason,
                             AppSettingsDialog::InitialPage::AudioPtt,
                             uiText("open_audio_ptt_settings", "Open Audio/PTT settings"));
        return;
    }

    if (m_txRunning ||
        m_ftTxWorkerRunning ||
        (m_txAudioEngine != nullptr && m_txAudioEngine->isRunning())) {
        const QString reason = uiText("ptt_test_tx_running", "PTT test is blocked because TX/PTT is already active.");
        appendLog(QStringLiteral("PTT test blocked: ") + reason);
        showTxBlockedWarning(QStringLiteral("PTT TEST"),
                             reason,
                             AppSettingsDialog::InitialPage::AudioPtt,
                             uiText("open_audio_ptt_settings", "Open Audio/PTT settings"));
        return;
    }

    if (!keyPttForTx()) {
        return;
    }

    m_offlineAnalysisActive = false;
    m_txRunning = true;
    setReceiverRunning(false);
    appendLog(uiText("ptt_test_on_one_second", "PTT test ON for 1 second."));
    m_pttTestTimer.start(1000);
}


void MainWindow::finishPttTest()
{
    const QString pttMethod = m_settings.pttMethod.trimmed().toLower();
    const QString pttPort = selectedPttPort().trimmed();
    const bool pttUsesCatPort = pttPort.isEmpty() ||
                                pttPort.compare(QStringLiteral("CAT"), Qt::CaseInsensitive) == 0 ||
                                pttPort.compare(m_settings.hamlibSerialPath.trimmed(), Qt::CaseInsensitive) == 0;
    const bool pttViaRigController = (pttMethod == QStringLiteral("cat_hamlib")) ||
                                     ((pttMethod == QStringLiteral("serial_rts") || pttMethod == QStringLiteral("serial_dtr")) &&
                                      m_settings.hamlibCatEnabled && pttUsesCatPort);
    if (pttViaRigController && m_rigController != nullptr) {
        invokeRigPttBlocking(false);
    }

    if (m_pttSerial.isOpen()) {
        m_pttSerial.setRequestToSend(false);
        m_pttSerial.setDataTerminalReady(false);
        m_pttSerial.close();
    }

    m_txRunning = false;
    setReceiverRunning(m_rxRunning);

    appendLog(pttViaRigController ? QStringLiteral("Hamlib/CAT-port PTT OFF.") : QStringLiteral("PTT serial line OFF."));
}

void MainWindow::txToneTest()
{
    if (m_pttTestTimer.isActive()) {
        m_pttTestTimer.stop();
        finishPttTest();
        return;
    }

    if (m_txRunning ||
        m_ftTxWorkerRunning ||
        (m_txAudioEngine != nullptr && m_txAudioEngine->isRunning())) {
        stopImageTx();
        return;
    }

    startImageTx();
}
