#include "SstvTransmitter.h"

#include <QColor>
#include <QPainter>
#include <QtMath>

#include <cmath>

namespace {

constexpr double kTwoPi = 6.28318530717958647692;
constexpr double kSyncHz = 1200.0;
constexpr double kPorchHz = 1500.0;
constexpr double kBlackHz = 1500.0;
constexpr double kWhiteHz = 2300.0;
constexpr int kSegmentSync = 0;
constexpr int kSegmentPorch = 1;
constexpr int kSegmentVideo = 2;
constexpr int kSegmentTone = 3;
constexpr int kRed = 0;
constexpr int kGreen = 1;
constexpr int kBlue = 2;
constexpr int kY0 = 3;
constexpr int kY1 = 4;
constexpr int kRedChroma = 5;
constexpr int kBlueChroma = 6;
constexpr int kRobot36Chroma = 7;

constexpr int kEncodingRgb = 0;
constexpr int kEncodingYuvSingle = 1;
constexpr int kEncodingYuvPair = 2;
constexpr int kEncodingRobot36 = 3;

int componentValue(const QImage &image, int x, int y, int channel)
{
    const QColor color(image.pixel(qBound(0, x, image.width() - 1),
                                   qBound(0, y, image.height() - 1)));

    switch (channel) {
    case kRed:
        return color.red();
    case kGreen:
        return color.green();
    case kBlue:
        return color.blue();
    default:
        break;
    }

    return qGray(color.rgb());
}

} // namespace

// -----------------------------------------------------------------------------
// Construction
// -----------------------------------------------------------------------------

SstvTransmitter::SstvTransmitter(const QImage &sourceImage,
                                 const QString &modeName,
                                 int sampleRate)
    : m_mode(modeByName(modeName)),
      m_image(prepareImage(sourceImage, modeName)),
      m_sampleRate(qMax(8000, sampleRate))
{
    if (m_image.isNull()) {
        m_image = QImage(m_mode.width, m_mode.height, QImage::Format_RGB32);
        m_image.fill(Qt::black);
    }

    m_lineSamples = (m_mode.lineMs * static_cast<double>(m_sampleRate)) / 1000.0;
    m_leadSamples = static_cast<qint64>(qRound(1.0 * static_cast<double>(m_sampleRate)));
    m_imageSamples = static_cast<qint64>(qCeil(m_lineSamples * static_cast<double>(m_mode.transmittedLines)));
    m_tailSamples = static_cast<qint64>(qRound(0.7 * static_cast<double>(m_sampleRate)));
    m_totalSamples = m_leadSamples + m_imageSamples + m_tailSamples;
}

// -----------------------------------------------------------------------------
// Static helpers
// -----------------------------------------------------------------------------

QVector<SstvTransmitter::Mode> SstvTransmitter::modes()
{
    QVector<Mode> result;

    const auto makeSegment = [](int type, int channel, double startMs, double durationMs, double toneHz = 0.0) {
        Segment segment;
        segment.type = type;
        segment.channel = channel;
        segment.startMs = startMs;
        segment.durationMs = durationMs;
        segment.toneHz = toneHz;
        return segment;
    };

    const auto addMartin = [&result, &makeSegment](const QString &key,
                                                   const QString &label,
                                                   int width,
                                                   int height,
                                                   double colorMs) {
        const double syncMs = 4.862;
        const double gapMs = 0.572;

        Mode mode;
        mode.key = key;
        mode.label = label;
        mode.width = width;
        mode.height = height;
        mode.transmittedLines = height;
        mode.outputLinesPerTxLine = 1;
        mode.encoding = kEncodingRgb;
        mode.lineMs = syncMs + (4.0 * gapMs) + (3.0 * colorMs);

        double t = 0.0;
        mode.segments.append(makeSegment(kSegmentSync, 0, t, syncMs));
        t += syncMs;
        mode.segments.append(makeSegment(kSegmentPorch, 0, t, gapMs));
        t += gapMs;
        mode.segments.append(makeSegment(kSegmentVideo, kGreen, t, colorMs));
        t += colorMs;
        mode.segments.append(makeSegment(kSegmentPorch, 0, t, gapMs));
        t += gapMs;
        mode.segments.append(makeSegment(kSegmentVideo, kBlue, t, colorMs));
        t += colorMs;
        mode.segments.append(makeSegment(kSegmentPorch, 0, t, gapMs));
        t += gapMs;
        mode.segments.append(makeSegment(kSegmentVideo, kRed, t, colorMs));
        t += colorMs;
        mode.segments.append(makeSegment(kSegmentPorch, 0, t, gapMs));

        result.append(mode);
    };

    const auto addScottie = [&result, &makeSegment](const QString &key,
                                                    const QString &label,
                                                    int width,
                                                    int height,
                                                    double colorMs) {
        const double syncMs = 9.0;
        const double gapMs = 1.5;

        Mode mode;
        mode.key = key;
        mode.label = label;
        mode.width = width;
        mode.height = height;
        mode.transmittedLines = height;
        mode.outputLinesPerTxLine = 1;
        mode.encoding = kEncodingRgb;
        mode.lineMs = syncMs + (3.0 * gapMs) + (3.0 * colorMs);

        double t = 0.0;
        mode.segments.append(makeSegment(kSegmentPorch, 0, t, gapMs));
        t += gapMs;
        mode.segments.append(makeSegment(kSegmentVideo, kGreen, t, colorMs));
        t += colorMs;
        mode.segments.append(makeSegment(kSegmentPorch, 0, t, gapMs));
        t += gapMs;
        mode.segments.append(makeSegment(kSegmentVideo, kBlue, t, colorMs));
        t += colorMs;
        mode.segments.append(makeSegment(kSegmentSync, 0, t, syncMs));
        t += syncMs;
        mode.segments.append(makeSegment(kSegmentPorch, 0, t, gapMs));
        t += gapMs;
        mode.segments.append(makeSegment(kSegmentVideo, kRed, t, colorMs));

        result.append(mode);
    };

    const auto addRobotSingle = [&result, &makeSegment](const QString &key,
                                                        const QString &label,
                                                        int width,
                                                        int height,
                                                        int transmittedLines,
                                                        double imageSeconds,
                                                        double syncMs,
                                                        double frontPorchMs,
                                                        double backPorchMs,
                                                        double blankMs) {
        const double lineMs = (imageSeconds * 1000.0) / static_cast<double>(qMax(1, transmittedLines));
        const double visibleMs = (lineMs - frontPorchMs - backPorchMs - (2.0 * blankMs) - syncMs) / 4.0;

        Mode mode;
        mode.key = key;
        mode.label = label;
        mode.width = width;
        mode.height = height;
        mode.transmittedLines = transmittedLines;
        mode.outputLinesPerTxLine = 1;
        mode.encoding = kEncodingYuvSingle;
        mode.lineMs = lineMs;

        double t = 0.0;
        mode.segments.append(makeSegment(kSegmentPorch, 0, t, backPorchMs));
        t += backPorchMs;
        mode.segments.append(makeSegment(kSegmentVideo, kY0, t, 2.0 * visibleMs));
        t += 2.0 * visibleMs;
        mode.segments.append(makeSegment(kSegmentTone, 0, t, (2.0 * blankMs) / 3.0, kPorchHz));
        t += (2.0 * blankMs) / 3.0;
        mode.segments.append(makeSegment(kSegmentTone, 0, t, blankMs / 3.0, 1900.0));
        t += blankMs / 3.0;
        mode.segments.append(makeSegment(kSegmentVideo, kRedChroma, t, visibleMs));
        t += visibleMs;
        mode.segments.append(makeSegment(kSegmentTone, 0, t, (2.0 * blankMs) / 3.0, kWhiteHz));
        t += (2.0 * blankMs) / 3.0;
        mode.segments.append(makeSegment(kSegmentTone, 0, t, blankMs / 3.0, 1900.0));
        t += blankMs / 3.0;
        mode.segments.append(makeSegment(kSegmentVideo, kBlueChroma, t, visibleMs));
        t += visibleMs;
        mode.segments.append(makeSegment(kSegmentPorch, 0, t, frontPorchMs));
        t += frontPorchMs;
        mode.segments.append(makeSegment(kSegmentSync, 0, t, syncMs));

        result.append(mode);
    };

    const auto addRobot36 = [&result, &makeSegment]() {
        const int width = 320;
        const int height = 240;
        const int transmittedLines = 240;
        const double imageSeconds = 36.00200;
        const double syncMs = 9.0;
        const double frontPorchMs = 0.4;
        const double backPorchMs = 2.5;
        const double blankMs = 7.0;
        const double lineMs = (imageSeconds * 1000.0) / static_cast<double>(transmittedLines);
        const double visibleMs = (lineMs - frontPorchMs - backPorchMs - blankMs - syncMs) / 3.0;

        Mode mode;
        mode.key = QStringLiteral("ROBOT_36");
        mode.label = QStringLiteral("Robot 36");
        mode.width = width;
        mode.height = height;
        mode.transmittedLines = transmittedLines;
        mode.outputLinesPerTxLine = 1;
        mode.encoding = kEncodingRobot36;
        mode.lineMs = lineMs;

        double t = 0.0;
        mode.segments.append(makeSegment(kSegmentPorch, 0, t, backPorchMs));
        t += backPorchMs;
        mode.segments.append(makeSegment(kSegmentVideo, kY0, t, 2.0 * visibleMs));
        t += 2.0 * visibleMs;
        mode.segments.append(makeSegment(kSegmentTone, 0, t, (2.0 * blankMs) / 3.0, -1.0));
        t += (2.0 * blankMs) / 3.0;
        mode.segments.append(makeSegment(kSegmentTone, 0, t, blankMs / 3.0, 1900.0));
        t += blankMs / 3.0;
        mode.segments.append(makeSegment(kSegmentVideo, kRobot36Chroma, t, visibleMs));
        t += visibleMs;
        mode.segments.append(makeSegment(kSegmentPorch, 0, t, frontPorchMs));
        t += frontPorchMs;
        mode.segments.append(makeSegment(kSegmentSync, 0, t, syncMs));

        result.append(mode);
    };

    const auto addPD = [&result, &makeSegment](const QString &key,
                                               const QString &label,
                                               int width,
                                               int height,
                                               int transmittedLines,
                                               double imageSeconds,
                                               double syncMs,
                                               double frontPorchMs,
                                               double backPorchMs) {
        const double lineMs = (imageSeconds * 1000.0) / static_cast<double>(qMax(1, transmittedLines));
        const double visibleMs = (lineMs - frontPorchMs - backPorchMs - syncMs) / 4.0;

        Mode mode;
        mode.key = key;
        mode.label = label;
        mode.width = width;
        mode.height = height;
        mode.transmittedLines = transmittedLines;
        mode.outputLinesPerTxLine = 2;
        mode.encoding = kEncodingYuvPair;
        mode.lineMs = lineMs;

        double t = 0.0;
        mode.segments.append(makeSegment(kSegmentPorch, 0, t, backPorchMs));
        t += backPorchMs;
        mode.segments.append(makeSegment(kSegmentVideo, kY0, t, visibleMs));
        t += visibleMs;
        mode.segments.append(makeSegment(kSegmentVideo, kRedChroma, t, visibleMs));
        t += visibleMs;
        mode.segments.append(makeSegment(kSegmentVideo, kBlueChroma, t, visibleMs));
        t += visibleMs;
        mode.segments.append(makeSegment(kSegmentVideo, kY1, t, visibleMs));
        t += visibleMs;
        mode.segments.append(makeSegment(kSegmentPorch, 0, t, frontPorchMs));
        t += frontPorchMs;
        mode.segments.append(makeSegment(kSegmentSync, 0, t, syncMs));

        result.append(mode);
    };

    addMartin("MARTIN_M1", "Martin M1", 320, 256, 146.432);
    addMartin("MARTIN_M2", "Martin M2", 160, 256, 73.216);
    addMartin("MARTIN_M3", "Martin M3", 320, 128, 146.432);
    addMartin("MARTIN_M4", "Martin M4", 160, 128, 73.216);

    addScottie("SCOTTIE_S1", "Scottie S1", 320, 256, 138.240);
    addScottie("SCOTTIE_S2", "Scottie S2", 160, 256, 88.064);
    addScottie("SCOTTIE_S3", "Scottie S3", 320, 128, 138.240);
    addScottie("SCOTTIE_S4", "Scottie S4", 160, 128, 88.064);
    addScottie("SCOTTIE_DX", "Scottie DX", 320, 256, 345.600);

    addRobotSingle("ROBOT_24", "Robot 24", 160, 120, 120, 24.00150, 6.0, 0.1, 3.0, 4.5);
    addRobot36();
    addRobotSingle("ROBOT_72", "Robot 72", 320, 240, 240, 72.00500, 9.0, 0.4, 3.5, 6.0);

    addPD("PD50", "PD50", 320, 256, 128, 49.68770, 20.0, 0.0, 2.08);
    addPD("PD90", "PD90", 320, 256, 128, 89.99500, 20.0, 0.0, 2.08);
    addPD("PD120", "PD120", 640, 496, 248, 126.11150, 20.0, 0.0, 2.08);
    addPD("PD160", "PD160", 512, 400, 200, 160.89420, 20.0, 0.0, 2.00);
    addPD("PD180", "PD180", 640, 496, 248, 187.06450, 20.0, 0.0, 2.00);
    addPD("PD240", "PD240", 640, 496, 248, 248.01700, 20.0, 2.0, 2.00);
    addPD("PD290", "PD290", 800, 616, 308, 288.70200, 20.0, 0.0, 2.00);

    return result;
}

SstvTransmitter::Mode SstvTransmitter::modeByName(const QString &modeName)
{
    const QString wanted = modeName.trimmed();

    for (const Mode &mode : modes()) {
        if (mode.label.compare(wanted, Qt::CaseInsensitive) == 0 ||
            mode.key.compare(wanted, Qt::CaseInsensitive) == 0) {
            return mode;
        }
    }

    return modes().first();
}

QImage SstvTransmitter::prepareImage(const QImage &sourceImage, const QString &modeName)
{
    if (sourceImage.isNull()) {
        return QImage();
    }

    const Mode mode = modeByName(modeName);
    QImage canvas(mode.width, mode.height, QImage::Format_RGB32);
    canvas.fill(Qt::black);

    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QSize fitted = sourceImage.size().scaled(canvas.size(), Qt::KeepAspectRatio);
    const QRect target((canvas.width() - fitted.width()) / 2,
                       (canvas.height() - fitted.height()) / 2,
                       fitted.width(),
                       fitted.height());

    painter.drawImage(target, sourceImage.convertToFormat(QImage::Format_RGB32));
    painter.end();

    return canvas;
}

bool SstvTransmitter::isSupportedMode(const QString &modeName)
{
    const QString wanted = modeName.trimmed();

    for (const Mode &mode : modes()) {
        if (mode.label.compare(wanted, Qt::CaseInsensitive) == 0 ||
            mode.key.compare(wanted, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }

    return false;
}

// -----------------------------------------------------------------------------
// TxModulator API
// -----------------------------------------------------------------------------

int SstvTransmitter::sampleRate() const
{
    return m_sampleRate;
}

int SstvTransmitter::generate(float *output, int sampleCount)
{
    if (output == nullptr || sampleCount <= 0 || isFinished()) {
        return 0;
    }

    int generated = 0;

    while (generated < sampleCount && m_position < m_totalSamples) {
        output[generated] = nextSample(frequencyAt(m_position));
        ++m_position;
        ++generated;
    }

    return generated;
}

bool SstvTransmitter::isFinished() const
{
    return m_position >= m_totalSamples;
}

double SstvTransmitter::progress() const
{
    if (m_imageSamples <= 0) {
        return isFinished() ? 1.0 : 0.0;
    }

    if (m_position <= m_leadSamples) {
        return 0.0;
    }

    const qint64 imagePos = qBound<qint64>(qint64{0},
                                           m_position - m_leadSamples,
                                           m_imageSamples);
    return qBound(0.0, static_cast<double>(imagePos) / static_cast<double>(m_imageSamples), 1.0);
}

QImage SstvTransmitter::previewImage() const
{
    return m_image;
}

QString SstvTransmitter::description() const
{
    return QString("SSTV TX %1, %2x%3")
        .arg(m_mode.label)
        .arg(m_mode.width)
        .arg(m_mode.height);
}

// -----------------------------------------------------------------------------
// Modulation helpers
// -----------------------------------------------------------------------------

double SstvTransmitter::frequencyAt(qint64 sampleIndex) const
{
    if (sampleIndex < m_leadSamples) {
        return kPorchHz;
    }

    qint64 pos = sampleIndex - m_leadSamples;

    if (pos < m_imageSamples) {
        const int txLine = qBound(0,
                                  static_cast<int>(qFloor(static_cast<double>(pos) / m_lineSamples)),
                                  qMax(0, m_mode.transmittedLines - 1));
        const double linePositionMs =
            (std::fmod(static_cast<double>(pos), m_lineSamples) * 1000.0) /
            static_cast<double>(m_sampleRate);
        return lineFrequency(linePositionMs, txLine);
    }

    return kPorchHz;
}

double SstvTransmitter::lineFrequency(double linePositionMs, int txLine) const
{
    for (const Segment &segment : m_mode.segments) {
        if (linePositionMs < segment.startMs ||
            linePositionMs >= segment.startMs + segment.durationMs) {
            continue;
        }

        if (segment.type == kSegmentSync) {
            return kSyncHz;
        }

        if (segment.type == kSegmentPorch) {
            return kPorchHz;
        }

        if (segment.type == kSegmentTone) {
            if (m_mode.encoding == kEncodingRobot36 && segment.toneHz < 0.0) {
                return (txLine % 2 == 0) ? kPorchHz : kWhiteHz;
            }
            return segment.toneHz > 0.0 ? segment.toneHz : kPorchHz;
        }

        const double t = (linePositionMs - segment.startMs) /
                         qMax(0.001, segment.durationMs);
        const int x = qBound(0,
                             static_cast<int>(qFloor(t * static_cast<double>(m_mode.width))),
                             m_mode.width - 1);
        return videoFrequency(x, txLine, segment.channel);
    }

    return kPorchHz;
}

double SstvTransmitter::videoFrequency(int x, int txLine, int channel) const
{
    const int value = (m_mode.encoding == kEncodingRgb)
        ? componentValue(m_image, x, txLine, channel)
        : yuvComponentValue(x, txLine, channel);
    return kBlackHz + (static_cast<double>(value) / 255.0) * (kWhiteHz - kBlackHz);
}

int SstvTransmitter::yuvComponentValue(int x, int txLine, int channel) const
{
    if (m_mode.encoding == kEncodingYuvPair) {
        const int lineA = qBound(0, txLine * 2, m_image.height() - 1);
        const int lineB = qBound(0, lineA + 1, m_image.height() - 1);

        if (channel == kY0) {
            return lumaValue(x, lineA);
        }
        if (channel == kY1) {
            return lumaValue(x, lineB);
        }
        if (channel == kRedChroma) {
            return redChromaValue(x, lineA, lineB);
        }
        if (channel == kBlueChroma) {
            return blueChromaValue(x, lineA, lineB);
        }
    }

    if (m_mode.encoding == kEncodingRobot36) {
        const int displayLine = qBound(0, txLine, m_image.height() - 1);

        if (channel == kY0) {
            return lumaValue(x, displayLine);
        }

        if (channel == kRobot36Chroma) {
            const int lineA = (displayLine % 2 == 0) ? displayLine : qMax(0, displayLine - 1);
            const int lineB = qMin(m_image.height() - 1, lineA + 1);
            return (displayLine % 2 == 0)
                ? redChromaValue(x, lineA, lineB)
                : blueChromaValue(x, lineA, lineB);
        }
    }

    const int displayLine = qBound(0, txLine, m_image.height() - 1);

    if (channel == kY0 || channel == kY1) {
        return lumaValue(x, displayLine);
    }
    if (channel == kRedChroma) {
        return redChromaValue(x, displayLine, displayLine);
    }
    if (channel == kBlueChroma) {
        return blueChromaValue(x, displayLine, displayLine);
    }

    return lumaValue(x, displayLine);
}

int SstvTransmitter::lumaValue(int x, int displayLine) const
{
    const QColor color(m_image.pixel(qBound(0, x, m_image.width() - 1),
                                     qBound(0, displayLine, m_image.height() - 1)));
    const int y = (59 * color.green() + 30 * color.red() + 11 * color.blue()) / 100;
    return qBound(0, y, 255);
}

int SstvTransmitter::redChromaValue(int x, int displayLineA, int displayLineB) const
{
    const QColor a(m_image.pixel(qBound(0, x, m_image.width() - 1),
                                 qBound(0, displayLineA, m_image.height() - 1)));
    const QColor b(m_image.pixel(qBound(0, x, m_image.width() - 1),
                                 qBound(0, displayLineB, m_image.height() - 1)));
    const int ya = (59 * a.green() + 30 * a.red() + 11 * a.blue()) / 100;
    const int yb = (59 * b.green() + 30 * b.red() + 11 * b.blue()) / 100;
    const int r = (a.red() + b.red()) / 2;
    const int chroma = ((10 * r) - (5 * (ya + yb)) + (7 * 255)) / 14;
    return qBound(0, chroma, 255);
}

int SstvTransmitter::blueChromaValue(int x, int displayLineA, int displayLineB) const
{
    const QColor a(m_image.pixel(qBound(0, x, m_image.width() - 1),
                                 qBound(0, displayLineA, m_image.height() - 1)));
    const QColor b(m_image.pixel(qBound(0, x, m_image.width() - 1),
                                 qBound(0, displayLineB, m_image.height() - 1)));
    const int ya = (59 * a.green() + 30 * a.red() + 11 * a.blue()) / 100;
    const int yb = (59 * b.green() + 30 * b.red() + 11 * b.blue()) / 100;
    const int blue = (a.blue() + b.blue()) / 2;
    const int chroma = ((100 * blue) - (50 * (ya + yb)) + (89 * 255)) / 178;
    return qBound(0, chroma, 255);
}

float SstvTransmitter::nextSample(double frequencyHz)
{
    const double amplitude = 0.55;
    const float sample = static_cast<float>(amplitude * qSin(m_phase));

    m_phase += kTwoPi * frequencyHz / static_cast<double>(m_sampleRate);

    if (m_phase >= kTwoPi) {
        m_phase = std::fmod(m_phase, kTwoPi);
    }

    return sample;
}
