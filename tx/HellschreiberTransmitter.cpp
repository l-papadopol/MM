#include "HellschreiberTransmitter.h"

#include <QColor>
#include <QFont>
#include <QPainter>
#include <QtMath>

#include <algorithm>
#include <array>
#include <limits>

namespace {

constexpr double kTwoPi = 6.283185307179586476925286766559;

QString normalizedHellText(const QString &text)
{
    QString normalized = text;
    normalized.replace("\r\n", " ");
    normalized.replace('\r', ' ');
    normalized.replace('\n', ' ');
    normalized.replace('\t', ' ');
    normalized = normalized.trimmed();

    /* Hellschreiber alphabets are small.  Keep the transmitter predictable by
     * folding accented European letters to their ASCII base form instead of
     * silently transmitting blank columns for characters such as è/à/ò.  Any
     * other printable unsupported character is rendered as '?'.
     */
    QString folded;
    folded.reserve(normalized.size());
    const QString decomposed = normalized.normalized(QString::NormalizationForm_D);
    for (QChar ch : decomposed) {
        if (ch.category() == QChar::Mark_NonSpacing ||
            ch.category() == QChar::Mark_SpacingCombining ||
            ch.category() == QChar::Mark_Enclosing) {
            continue;
        }
        folded.append(ch);
    }

    return folded.isEmpty() ? QString(" ") : folded;
}

using HellGlyph = std::array<quint8, 7>;

HellGlyph questionGlyph()
{
    return {{0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04}};
}

HellGlyph hellGlyphForChar(QChar ch)
{
    const ushort u = ch.toUpper().unicode();
    switch (u) {
    case ' ': return {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
    case 'A': return {{0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}};
    case 'B': return {{0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}};
    case 'C': return {{0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}};
    case 'D': return {{0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E}};
    case 'E': return {{0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}};
    case 'F': return {{0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}};
    case 'G': return {{0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F}};
    case 'H': return {{0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}};
    case 'I': return {{0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}};
    case 'J': return {{0x07, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0C}};
    case 'K': return {{0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}};
    case 'L': return {{0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}};
    case 'M': return {{0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}};
    case 'N': return {{0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}};
    case 'O': return {{0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}};
    case 'P': return {{0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}};
    case 'Q': return {{0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}};
    case 'R': return {{0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}};
    case 'S': return {{0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}};
    case 'T': return {{0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}};
    case 'U': return {{0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}};
    case 'V': return {{0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04}};
    case 'W': return {{0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A}};
    case 'X': return {{0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}};
    case 'Y': return {{0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}};
    case 'Z': return {{0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}};
    case '0': return {{0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}};
    case '1': return {{0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}};
    case '2': return {{0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}};
    case '3': return {{0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E}};
    case '4': return {{0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}};
    case '5': return {{0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E}};
    case '6': return {{0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}};
    case '7': return {{0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}};
    case '8': return {{0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}};
    case '9': return {{0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C}};
    case '.': return {{0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C}};
    case ',': return {{0x00, 0x00, 0x00, 0x00, 0x0C, 0x04, 0x08}};
    case '?': return questionGlyph();
    case '!': return {{0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04}};
    case '/': return {{0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10}};
    case '-': return {{0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}};
    case '+': return {{0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00}};
    case '=': return {{0x00, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00}};
    case ':': return {{0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00}};
    case ';': return {{0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x04, 0x08}};
    case '\'': return {{0x04, 0x04, 0x08, 0x00, 0x00, 0x00, 0x00}};
    case '"': return {{0x0A, 0x0A, 0x14, 0x00, 0x00, 0x00, 0x00}};
    case '(': return {{0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02}};
    case ')': return {{0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08}};
    case '@': return {{0x0E, 0x11, 0x17, 0x15, 0x17, 0x10, 0x0E}};
    default:
        if (ch.isSpace()) {
            return {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
        }
        return questionGlyph();
    }
}

QImage buildHellRasterImage(const QString &normalized,
                            HellschreiberDecoder::Variant variant,
                            int rasterHeight)
{
    Q_UNUSED(variant);

    /* Feld Hell is a hard-keyed visual mode: black ink is carrier on, white
     * paper is carrier off.  The raster is therefore binary only.  Keep lead
     * and tail paper short; long all-white margins made the UI show progress
     * while the transmitter was correctly silent, which looked like lost audio.
     */
    const int glyphColumns = 7;
    const int charColumns = glyphColumns + 2;
    const int leadingColumns = 7;
    const int trailingColumns = 14;
    const int width = qMax(32, (normalized.size() * charColumns) + leadingColumns + trailingColumns);

    QImage raster(width, rasterHeight, QImage::Format_Grayscale8);
    raster.fill(255);

    for (int i = 0; i < normalized.size(); ++i) {
        const HellGlyph glyph = hellGlyphForChar(normalized.at(i));
        const int x0 = leadingColumns + (i * charColumns) + 1;
        for (int glyphRow = 0; glyphRow < 7; ++glyphRow) {
            const quint8 rowBits = glyph[static_cast<size_t>(glyphRow)];
            for (int glyphCol = 0; glyphCol < 5; ++glyphCol) {
                if ((rowBits & (1u << (4 - glyphCol))) == 0) {
                    continue;
                }

                /* Centre the 5-column glyph in a 7-column Hell character cell. */
                const int x = x0 + glyphCol + 1;
                const int yA = glyphRow * 2;
                const int yB = yA + 1;
                if (x >= 0 && x < raster.width()) {
                    if (yA >= 0 && yA < raster.height()) {
                        raster.scanLine(yA)[x] = 0;
                    }
                    if (yB >= 0 && yB < raster.height()) {
                        raster.scanLine(yB)[x] = 0;
                    }
                }
            }
        }
    }

    return raster;
}

QVector<float> buildHellWaveform(const QImage &raster,
                                 int sampleRate,
                                 double toneHz,
                                 double columnRate,
                                 HellschreiberDecoder::Variant variant,
                                 double fskShiftHz,
                                 int rasterHeight,
                                 double *samplesPerPixelOut)
{
    const double pixelRate = qMax(1.0, columnRate * static_cast<double>(rasterHeight));
    const double samplesPerPixel = static_cast<double>(sampleRate) / pixelRate;
    if (samplesPerPixelOut != nullptr) {
        *samplesPerPixelOut = samplesPerPixel;
    }

    const qint64 totalPixels = static_cast<qint64>(raster.width()) * rasterHeight;
    const qint64 totalSamples64 = qMax<qint64>(1, static_cast<qint64>(qCeil(static_cast<double>(totalPixels) * samplesPerPixel)));
    const int totalSamples = static_cast<int>(qMin<qint64>(totalSamples64, std::numeric_limits<int>::max()));

    QVector<float> waveform(totalSamples, 0.0f);
    double phase = 0.0;
    double envelope = 0.0;

    for (int n = 0; n < totalSamples; ++n) {
        const qint64 pixelIndex = qMin<qint64>(totalPixels - 1,
                                               static_cast<qint64>(qFloor(static_cast<double>(n) / samplesPerPixel)));
        const int column = static_cast<int>(pixelIndex / rasterHeight);
        const int row = static_cast<int>(pixelIndex % rasterHeight);
        const bool keyed = (column >= 0 && column < raster.width() && row >= 0 && row < raster.height())
                               ? (raster.constScanLine(row)[column] < 128)
                               : false;

        if (variant == HellschreiberDecoder::Variant::Fsk105) {
            const double halfShift = fskShiftHz * 0.5;
            const double tone = toneHz + (keyed ? -halfShift : halfShift);
            phase += kTwoPi * tone / static_cast<double>(sampleRate);
            envelope += 0.030 * (0.56 - envelope);
        } else {
            phase += kTwoPi * toneHz / static_cast<double>(sampleRate);
            const double targetEnvelope = keyed ? 0.72 : 0.0;
            /* Edge shaping only, not grey-level modulation. */
            envelope += 0.040 * (targetEnvelope - envelope);
        }

        while (phase >= kTwoPi) {
            phase -= kTwoPi;
        }
        waveform[n] = static_cast<float>(envelope * qSin(phase));
    }

    return waveform;
}

} // namespace

HellschreiberTransmitter::HellschreiberTransmitter(const QString &text,
                                                   int sampleRate,
                                                   double toneHz,
                                                   double columnRate,
                                                   HellschreiberDecoder::Variant variant,
                                                   double fskShiftHz,
                                                   int displayScale)
    : m_text(normalizedHellText(text)),
      m_sampleRate(qBound(8000, sampleRate, 192000)),
      m_toneHz(qBound(250.0, toneHz, 3500.0)),
      m_columnRate(qBound(2.0, columnRate, 80.0)),
      m_fskShiftHz(qBound(20.0, fskShiftHz, 300.0)),
      m_variant(variant),
      m_displayScale(qBound(1, displayScale, 12))
{
    m_pixelRate = m_columnRate * static_cast<double>(kRasterHeight);
    m_raster = buildTransmitRaster(m_text);
    m_waveform = buildHellWaveform(m_raster,
                                   m_sampleRate,
                                   m_toneHz,
                                   m_columnRate,
                                   m_variant,
                                   m_fskShiftHz,
                                   kRasterHeight,
                                   &m_samplesPerPixel);
    m_preview = previewTextImage(m_text, m_variant, m_columnRate, m_displayScale);
    m_totalSamples = m_waveform.size();
}

int HellschreiberTransmitter::sampleRate() const
{
    return m_sampleRate;
}

int HellschreiberTransmitter::generate(float *output, int sampleCount)
{
    if (output == nullptr || sampleCount <= 0 || isFinished()) {
        return 0;
    }

    const int available = static_cast<int>(qMin<qint64>(sampleCount, m_totalSamples - m_generatedSamples));
    if (available <= 0) {
        return 0;
    }

    std::copy(m_waveform.constBegin() + static_cast<int>(m_generatedSamples),
              m_waveform.constBegin() + static_cast<int>(m_generatedSamples) + available,
              output);
    m_generatedSamples += available;
    return available;
}

bool HellschreiberTransmitter::isFinished() const
{
    return m_generatedSamples >= m_totalSamples;
}

double HellschreiberTransmitter::progress() const
{
    if (m_totalSamples <= 0) {
        return 1.0;
    }

    return qBound(0.0,
                  static_cast<double>(m_generatedSamples) / static_cast<double>(m_totalSamples),
                  1.0);
}

QImage HellschreiberTransmitter::previewImage() const
{
    return m_preview;
}

QString HellschreiberTransmitter::description() const
{
    if (m_variant == HellschreiberDecoder::Variant::Fsk105) {
        return QString("FSK-105 TX: center %1 Hz, shift %2 Hz, %3 columns/s, %4 char(s)")
            .arg(m_toneHz, 0, 'f', 0)
            .arg(m_fskShiftHz, 0, 'f', 0)
            .arg(m_columnRate, 0, 'f', 2)
            .arg(m_text.size());
    }

    return QString("Feld Hell TX: %1 Hz, %2 columns/s, %3 char(s)")
        .arg(m_toneHz, 0, 'f', 0)
        .arg(m_columnRate, 0, 'f', 2)
        .arg(m_text.size());
}

QImage HellschreiberTransmitter::transmitRasterImage(const QString &text,
                                                     HellschreiberDecoder::Variant variant)
{
    const QString normalized = normalizedHellText(text);
    return buildHellRasterImage(normalized, variant, kRasterHeight);
}

QImage HellschreiberTransmitter::previewTextImage(const QString &text,
                                                  HellschreiberDecoder::Variant variant,
                                                  double columnRate,
                                                  int displayScale)
{
    const QImage raster = transmitRasterImage(text, variant);

    const int xScale = 6;
    const int yScale = qBound(1, displayScale, 12);
    const int leftMargin = 18;
    const int rightMargin = 18;
    const int topMargin = 18;
    const int bottomMargin = 46;
    const int paperWidth = qMax(1100, raster.width() * xScale + leftMargin + rightMargin);
    const int paperHeight = topMargin + (kRasterHeight * yScale) + bottomMargin;

    QImage image(paperWidth, paperHeight, QImage::Format_RGB32);
    image.fill(Qt::white);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.fillRect(image.rect(), Qt::white);

    QRect paperRect(leftMargin, topMargin, raster.width() * xScale, kRasterHeight * yScale);
    painter.fillRect(paperRect.adjusted(-1, -1, 1, 1), QColor(248, 248, 248));
    painter.setPen(QColor(230, 230, 230));
    for (int row = 0; row <= kRasterHeight; ++row) {
        const int y = topMargin + row * yScale;
        painter.drawLine(leftMargin, y, leftMargin + raster.width() * xScale, y);
    }

    for (int x = 0; x < raster.width(); ++x) {
        for (int y = 0; y < raster.height(); ++y) {
            if (raster.constScanLine(y)[x] < 128) {
                painter.fillRect(leftMargin + x * xScale,
                                 topMargin + y * yScale,
                                 xScale,
                                 yScale,
                                 Qt::black);
            }
        }
    }

    painter.setPen(QColor(90, 90, 90));
    painter.setFont(QFont("DejaVu Sans Mono", 10));
    const QString label = (variant == HellschreiberDecoder::Variant::Fsk105)
                              ? QString("FSK-105 TX raster preview: %1 columns at %2 col/s")
                              : QString("Feld Hell TX raster preview: %1 columns at %2 col/s");
    painter.drawText(QRect(leftMargin, image.height() - 34, image.width() - leftMargin - rightMargin, 22),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     label.arg(raster.width()).arg(columnRate, 0, 'f', 2));
    return image;
}

QImage HellschreiberTransmitter::buildTransmitRaster(const QString &text) const
{
    return transmitRasterImage(text, m_variant);
}

bool HellschreiberTransmitter::pixelOn(int column, int row) const
{
    if (m_raster.isNull() ||
        column < 0 ||
        column >= m_raster.width() ||
        row < 0 ||
        row >= m_raster.height()) {
        return false;
    }

    return m_raster.constScanLine(row)[column] < 128;
}
