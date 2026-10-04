#include "MfskTransmitter.h"

#include <QColor>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QTextOption>
#include <QtMath>

namespace {
constexpr double kTwoPi = 6.283185307179586476925286766559;
constexpr int kPoly1 = 0x6d;
constexpr int kPoly2 = 0x4f;

static int parity7(int value)
{
    value ^= value >> 4;
    value ^= value >> 2;
    value ^= value >> 1;
    return value & 1;
}

static int grayEncode(int value)
{
    return value ^ (value >> 1);
}

static const char *kMfskVaricodeAscii[128] = {
    "11101011100","11101100000","11101101000","11101101100","11101110000","11101110100","11101111000","11101111100",
    "10101000","11110000000","11110100000","11110101000","11110101100","10101100","11110110000","11110110100",
    "11110111000","11110111100","11111000000","11111010000","11111010100","11111011000","11111011100","11111100000",
    "11111101000","11111101100","11111110000","11111110100","11111111000","11111111100","100000000000","101000000000",
    "100","111000000","111111100","1011011000","1010101000","1010100000","1000000000","110111100",
    "111110100","111110000","1010110100","111100000","10100000","111011000","111010100","111101000",
    "11100000","11110000","101000000","101010100","101110100","101100000","101101100","110100000",
    "110000000","110101100","111101100","111111000","1011000000","111011100","1010111100","111010000",
    "1010000000","10111100","100000000","11010100","11011100","10111000","11111000","101010000",
    "101011000","11000000","110110100","101111100","11110100","11101000","11111100","11010000",
    "11101100","110110000","11011000","10110100","10110000","101011100","110101000","101101000",
    "101110000","101111000","110111000","1011101000","1011010000","1011101100","1011010100","1010110000",
    "1010101100","10100","1100000","111000","110100","1000","1010000","1011000",
    "110000","11000","10000000","1110000","101100","1000000","11100","10000",
    "1010100","1111000","100000","101000","1100","111100","1101100","1101000",
    "1110100","1011100","1111100","1011011100","1010111000","1011100000","1011110000","101010000000"
};

} // namespace

class MfskTransmitter::DiagonalInterleaver
{
public:
    explicit DiagonalInterleaver(int size)
        : m_size(size), m_table(10 * size * size, 0)
    {
    }

    void interleaveBits(unsigned int &bits)
    {
        QVector<unsigned char> syms(m_size, 0);
        for (int i = 0; i < m_size; ++i) {
            syms[i] = static_cast<unsigned char>((bits >> (m_size - i - 1)) & 1u);
        }
        for (int stage = 0; stage < 10; ++stage) {
            for (int row = 0; row < m_size; ++row) {
                const int base = (stage * m_size * m_size) + (row * m_size);
                for (int col = 0; col < m_size - 1; ++col) {
                    m_table[base + col] = m_table.at(base + col + 1);
                }
                m_table[base + m_size - 1] = syms.at(row);
            }
            QVector<unsigned char> out(m_size, 0);
            for (int row = 0; row < m_size; ++row) {
                const int base = (stage * m_size * m_size) + (row * m_size);
                out[row] = m_table.at(base + (m_size - row - 1)); // INTERLEAVE_FWD
            }
            syms = out;
        }
        bits = 0;
        for (int i = 0; i < m_size; ++i) {
            bits = (bits << 1) | syms.at(i);
        }
    }

private:
    int m_size = 4;
    QVector<unsigned char> m_table;
};



MfskTransmitter::MfskTransmitter(const QString &text,
                                 int sampleRate,
                                 double centerHz,
                                 MfskDecoder::Variant variant)
    : m_text(text),
      m_sampleRate(qBound(8000, sampleRate, 192000)),
      m_centerHz(qBound(300.0, centerHz, 3300.0)),
      m_variant(variant)
{
    m_symbolSamples = static_cast<double>(m_sampleRate) / symbolRate();
    buildTones(text);
    m_totalSamples = static_cast<qint64>(qCeil(static_cast<double>(m_tones.size()) * m_symbolSamples));
}

MfskTransmitter::~MfskTransmitter() = default;

int MfskTransmitter::sampleRate() const
{
    return m_sampleRate;
}

int MfskTransmitter::generate(float *output, int sampleCount)
{
    if (output == nullptr || sampleCount <= 0 || isFinished()) {
        return 0;
    }

    int generated = 0;
    while (generated < sampleCount && !isFinished()) {
        const int tone = qBound(0, m_tones.at(m_toneIndex), toneCount() - 1);
        const double freq = frequencyForTone(tone);
        const double inc = kTwoPi * freq / static_cast<double>(m_sampleRate);
        const double frac = qBound(0.0, m_samplesInSymbol / m_symbolSamples, 1.0);
        const double edge = 0.08;
        double envelope = 1.0;
        if (frac < edge) {
            envelope = 0.5 - 0.5 * qCos(M_PI * frac / edge);
        } else if (frac > 1.0 - edge) {
            envelope = 0.5 - 0.5 * qCos(M_PI * (1.0 - frac) / edge);
        }
        output[generated] = static_cast<float>(0.52 * envelope * qSin(m_carrierPhase));

        m_carrierPhase += inc;
        while (m_carrierPhase >= kTwoPi) {
            m_carrierPhase -= kTwoPi;
        }

        ++generated;
        ++m_generatedSamples;
        m_samplesInSymbol += 1.0;
        if (m_samplesInSymbol >= m_symbolSamples) {
            m_samplesInSymbol -= m_symbolSamples;
            ++m_toneIndex;
        }
    }

    return generated;
}

bool MfskTransmitter::isFinished() const
{
    return m_toneIndex >= m_tones.size();
}

double MfskTransmitter::progress() const
{
    if (m_totalSamples <= 0) {
        return 1.0;
    }
    return qBound(0.0,
                  static_cast<double>(m_generatedSamples) / static_cast<double>(m_totalSamples),
                  1.0);
}

QImage MfskTransmitter::previewImage() const
{
    return previewTextImage(m_text, m_variant);
}

QString MfskTransmitter::description() const
{
    return QStringLiteral("%1 TX: center %2 Hz, %3 char(s)")
        .arg(MfskDecoder::variantName(m_variant))
        .arg(m_centerHz, 0, 'f', 0)
        .arg(m_text.size());
}

QImage MfskTransmitter::previewTextImage(const QString &text, MfskDecoder::Variant variant)
{
    QImage image(800, 520, QImage::Format_RGB32);
    image.fill(QColor(10, 14, 18));

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(image.rect(), QColor(10, 14, 18));
    painter.setPen(QColor(90, 220, 255));
    painter.setFont(QFont("Monospace", 18));

    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    const QString note = (variant == MfskDecoder::Variant::Mfsk16)
        ? QStringLiteral("MFSK16 standard Varicode/FEC TX")
        : QStringLiteral("MFSK32 legacy experimental TX");
    painter.drawText(QRect(24, 24, image.width() - 48, image.height() - 48),
                     text.isEmpty()
                         ? QStringLiteral("%1\nTX text is empty").arg(note)
                         : note + QStringLiteral("\n\n") + text,
                     option);
    return image;
}

int MfskTransmitter::toneCount() const
{
    return m_variant == MfskDecoder::Variant::Mfsk32 ? 32 : 16;
}

double MfskTransmitter::symbolRate() const
{
    return m_variant == MfskDecoder::Variant::Mfsk32 ? 31.25 : 15.625;
}

double MfskTransmitter::toneSpacingHz() const
{
    return symbolRate();
}

double MfskTransmitter::firstToneHz() const
{
    return m_centerHz - (0.5 * static_cast<double>(toneCount() - 1) * toneSpacingHz());
}

void MfskTransmitter::buildTones(const QString &text)
{
    m_tones.clear();
    m_toneIndex = 0;
    m_samplesInSymbol = 0.0;
    m_generatedSamples = 0;
    m_carrierPhase = 0.0;

    if (m_variant == MfskDecoder::Variant::Mfsk16) {
        buildStandardMfsk16Tones(text);
        return;
    }

    const int idleTone = 0;
    for (int i = 0; i < 16; ++i) {
        m_tones.append(idleTone);
    }

    QString normalized = text;
    normalized.replace("\r\n", "\n");
    normalized.replace('\r', '\n');
    for (QChar ch : normalized) {
        appendLegacyCharacter(ch);
    }

    for (int i = 0; i < 8; ++i) {
        m_tones.append(idleTone);
    }
}

void MfskTransmitter::buildStandardMfsk16Tones(const QString &text)
{
    m_convState = 0;
    m_bitState = 0;
    m_bitShiftRegister = 0;
    m_interleaver.reset(new DiagonalInterleaver(4));

    // MFSK16 preamble: lowest tone / zero bit stream to allow tuning and FEC lock.
    for (int i = 0; i < 32; ++i) {
        appendStandardBit(0);
    }

    appendStandardCharacter('\r');
    appendStandardCharacter(QChar(char16_t{2})); // STX, as gMFSK/fldigi-style text start marker
    appendStandardCharacter('\r');

    QString normalized = text;
    normalized.replace("\r\n", "\n");
    normalized.replace('\r', '\n');
    for (QChar ch : normalized) {
        appendStandardCharacter(ch);
    }

    appendStandardCharacter('\r');
    appendStandardCharacter(QChar(char16_t{4})); // EOT
    appendStandardCharacter('\r');

    // Flush Varicode decoder, convolutional encoder and interleaver.
    appendStandardBit(1);
    for (int i = 0; i < 107; ++i) {
        appendStandardBit(0);
    }
    m_bitState = 0;
    m_bitShiftRegister = 0;
}

void MfskTransmitter::appendStandardCharacter(QChar ch)
{
    int code = ch.unicode();
    if (code == '\n') {
        code = '\r';
    }
    if (code < 0 || code >= 128 || kMfskVaricodeAscii[code] == nullptr) {
        code = ' ';
    }
    const char *bits = kMfskVaricodeAscii[code];
    while (*bits) {
        appendStandardBit((*bits++ == '1') ? 1 : 0);
    }
}

void MfskTransmitter::appendStandardBit(int bit)
{
    const int reg = ((m_convState << 1) | (bit ? 1 : 0)) & 0x7f;
    const int outA = parity7(reg & kPoly1);
    const int outB = parity7(reg & kPoly2);
    m_convState = reg & 0x3f;

    const int encoded[2] = {outA, outB};
    for (int value : encoded) {
        m_bitShiftRegister = (m_bitShiftRegister << 1) | (value & 1);
        ++m_bitState;
        if (m_bitState == 4) {
            unsigned int packed = static_cast<unsigned int>(m_bitShiftRegister & 0x0f);
            if (m_interleaver) {
                m_interleaver->interleaveBits(packed);
            }
            appendTone(grayEncode(static_cast<int>(packed)) & 0x0f);
            m_bitState = 0;
            m_bitShiftRegister = 0;
        }
    }
}

void MfskTransmitter::appendLegacyCharacter(QChar ch)
{
    int code = ch.unicode() & 0x7F;
    if (code == '\r') {
        code = '\n';
    }
    if (!(code == '\n' || code == '\t' || (code >= 32 && code <= 126))) {
        code = ' ';
    }

    const int startTone = toneCount() - 1;
    m_tones.append(startTone);
    if (m_variant == MfskDecoder::Variant::Mfsk32) {
        m_tones.append((code >> 3) & 0x1F);
        m_tones.append(code & 0x07);
    } else {
        m_tones.append((code >> 4) & 0x0F);
        m_tones.append(code & 0x0F);
    }
}

void MfskTransmitter::appendTone(int toneIndex)
{
    m_tones.append(qBound(0, toneIndex, toneCount() - 1));
}

double MfskTransmitter::frequencyForTone(int toneIndex) const
{
    return firstToneHz() + toneSpacingHz() * static_cast<double>(toneIndex);
}
