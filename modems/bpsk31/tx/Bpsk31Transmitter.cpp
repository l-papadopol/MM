#include "Bpsk31Transmitter.h"

#include <QColor>
#include <QFont>
#include <QHash>
#include <QImage>
#include <QPainter>
#include <QTextOption>
#include <QtMath>

namespace {

constexpr double kTwoPi = 2.0 * M_PI;

QHash<QChar, QString> varicodeEncodeTable()
{
    return {
        {QChar('\t'), "11101111"}, {QChar('\n'), "11101"}, {QChar('\r'), "11111"},
        {QChar(' '), "1"}, {QChar('!'), "111111111"}, {QChar('"'), "101011111"},
        {QChar('#'), "111110101"}, {QChar('$'), "111011011"}, {QChar('%'), "1011010101"},
        {QChar('&'), "1010111011"}, {QChar('\''), "101111111"}, {QChar('('), "11111011"},
        {QChar(')'), "11110111"}, {QChar('*'), "101101111"}, {QChar('+'), "111011111"},
        {QChar(','), "1110101"}, {QChar('-'), "110101"}, {QChar('.'), "1010111"},
        {QChar('/'), "110101111"}, {QChar('0'), "10110111"}, {QChar('1'), "10111101"},
        {QChar('2'), "11101101"}, {QChar('3'), "11111111"}, {QChar('4'), "101110111"},
        {QChar('5'), "101011011"}, {QChar('6'), "101101011"}, {QChar('7'), "110101101"},
        {QChar('8'), "110101011"}, {QChar('9'), "110110111"}, {QChar(':'), "11110101"},
        {QChar(';'), "110111101"}, {QChar('<'), "111101101"}, {QChar('='), "1010101"},
        {QChar('>'), "111010111"}, {QChar('?'), "1010101111"}, {QChar('@'), "1010111101"},
        {QChar('A'), "1111101"}, {QChar('B'), "11101011"}, {QChar('C'), "10101101"},
        {QChar('D'), "10110101"}, {QChar('E'), "1110111"}, {QChar('F'), "11011011"},
        {QChar('G'), "11111101"}, {QChar('H'), "101010101"}, {QChar('I'), "1111111"},
        {QChar('J'), "111111101"}, {QChar('K'), "101111101"}, {QChar('L'), "11010111"},
        {QChar('M'), "10111011"}, {QChar('N'), "11011101"}, {QChar('O'), "10101011"},
        {QChar('P'), "11010101"}, {QChar('Q'), "111011101"}, {QChar('R'), "10101111"},
        {QChar('S'), "1101111"}, {QChar('T'), "1101101"}, {QChar('U'), "101010111"},
        {QChar('V'), "110110101"}, {QChar('W'), "101011101"}, {QChar('X'), "101110101"},
        {QChar('Y'), "101111011"}, {QChar('Z'), "1010101101"}, {QChar('['), "111110111"},
        {QChar('\\'), "111101111"}, {QChar(']'), "111111011"}, {QChar('^'), "1010111111"},
        {QChar('_'), "101101101"}, {QChar('`'), "1011011111"}, {QChar('a'), "1011"},
        {QChar('b'), "1011111"}, {QChar('c'), "101111"}, {QChar('d'), "101101"},
        {QChar('e'), "11"}, {QChar('f'), "111101"}, {QChar('g'), "1011011"},
        {QChar('h'), "101011"}, {QChar('i'), "1101"}, {QChar('j'), "111101011"},
        {QChar('k'), "10111111"}, {QChar('l'), "11011"}, {QChar('m'), "111011"},
        {QChar('n'), "1111"}, {QChar('o'), "111"}, {QChar('p'), "111111"},
        {QChar('q'), "110111111"}, {QChar('r'), "10101"}, {QChar('s'), "10111"},
        {QChar('t'), "101"}, {QChar('u'), "110111"}, {QChar('v'), "1111011"},
        {QChar('w'), "1101011"}, {QChar('x'), "11011111"}, {QChar('y'), "1011101"},
        {QChar('z'), "111010101"}, {QChar('{'), "1010110111"}, {QChar('|'), "110111011"},
        {QChar('}'), "1010110101"}, {QChar('~'), "1011010111"}
    };
}

} // namespace

Bpsk31Transmitter::Bpsk31Transmitter(const QString &text,
                                     int sampleRate,
                                     double toneHz,
                                     double symbolRate,
                                     bool invertBits,
                                     bool qpskMode)
    : m_text(text),
      m_sampleRate(qBound(8000, sampleRate, 192000)),
      m_toneHz(qBound(300.0, toneHz, 3500.0)),
      m_invertBits(invertBits),
      m_qpskMode(qpskMode)
{
    if (symbolRate >= 750.0) {
        m_symbolRate = 1000.0;
    } else if (symbolRate >= 375.0) {
        m_symbolRate = 500.0;
    } else if (symbolRate >= 187.5) {
        m_symbolRate = 250.0;
    } else if (symbolRate >= 100.0) {
        m_symbolRate = 125.0;
    } else if (symbolRate >= 50.0) {
        m_symbolRate = 62.5;
    } else {
        m_symbolRate = 31.25;
    }

    m_symbolSamples = static_cast<double>(m_sampleRate) / m_symbolRate;
    buildBits(text);
    const int bitsPerSymbol = m_qpskMode ? 2 : 1;
    const int symbolCount = (m_bits.size() + bitsPerSymbol - 1) / bitsPerSymbol;
    m_totalSamples = static_cast<qint64>(qCeil(static_cast<double>(symbolCount) * m_symbolSamples));
    setupCurrentSymbol();
}

int Bpsk31Transmitter::sampleRate() const
{
    return m_sampleRate;
}

int Bpsk31Transmitter::generate(float *output, int sampleCount)
{
    if (output == nullptr || sampleCount <= 0 || isFinished()) {
        return 0;
    }

    int generated = 0;
    const double carrierInc = kTwoPi * m_toneHz / static_cast<double>(m_sampleRate);

    while (generated < sampleCount && !isFinished()) {
        const double frac = qBound(0.0, m_samplesInSymbol / m_symbolSamples, 1.0);
        const double shaped = 0.5 - (0.5 * qCos(M_PI * frac));
        const double dataPhase = m_symbolStartPhase + ((m_symbolEndPhase - m_symbolStartPhase) * shaped);

        output[generated] = static_cast<float>(0.58 * qSin(m_carrierPhase + dataPhase));

        m_carrierPhase += carrierInc;
        if (m_carrierPhase >= kTwoPi) {
            m_carrierPhase -= kTwoPi;
        }

        m_samplesInSymbol += 1.0;
        ++m_generatedSamples;
        ++generated;

        if (m_samplesInSymbol >= m_symbolSamples) {
            m_dataPhase = m_symbolEndPhase;
            while (m_dataPhase >= kTwoPi) {
                m_dataPhase -= kTwoPi;
            }
            m_samplesInSymbol -= m_symbolSamples;
            m_bitIndex += (m_qpskMode ? 2 : 1);
            setupCurrentSymbol();
        }
    }

    return generated;
}

bool Bpsk31Transmitter::isFinished() const
{
    return m_bitIndex >= m_bits.size();
}

double Bpsk31Transmitter::progress() const
{
    if (m_totalSamples <= 0) {
        return 1.0;
    }

    return qBound(0.0,
                  static_cast<double>(m_generatedSamples) / static_cast<double>(m_totalSamples),
                  1.0);
}

QImage Bpsk31Transmitter::previewImage() const
{
    return previewTextImage(m_text);
}

QString Bpsk31Transmitter::description() const
{
    QString variant = m_qpskMode ? "QPSK31" : "BPSK31";
    if (m_symbolRate >= 750.0) {
        variant = m_qpskMode ? "QPSK1000" : "BPSK1000";
    } else if (m_symbolRate >= 375.0) {
        variant = m_qpskMode ? "QPSK500" : "BPSK500";
    } else if (m_symbolRate >= 187.5) {
        variant = m_qpskMode ? "QPSK250" : "BPSK250";
    } else if (m_symbolRate >= 100.0) {
        variant = m_qpskMode ? "QPSK125" : "BPSK125";
    } else if (m_symbolRate >= 50.0) {
        variant = m_qpskMode ? "QPSK63" : "BPSK63";
    }

    return QString("%1 TX: %2 Hz, %3 char(s)")
        .arg(variant)
        .arg(m_toneHz, 0, 'f', 0)
        .arg(m_text.size());
}

QImage Bpsk31Transmitter::previewTextImage(const QString &text)
{
    QImage image(800, 520, QImage::Format_RGB32);
    image.fill(QColor(12, 16, 14));

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(image.rect(), QColor(12, 16, 14));
    painter.setPen(QColor(80, 255, 140));
    painter.setFont(QFont("Monospace", 18));

    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    painter.drawText(QRect(24, 24, image.width() - 48, image.height() - 48),
                     text.isEmpty() ? QString("BPSK TX text is empty") : text,
                     option);

    return image;
}

void Bpsk31Transmitter::buildBits(const QString &text)
{
    m_bits.clear();
    m_bitIndex = 0;
    m_samplesInSymbol = 0.0;
    m_generatedSamples = 0;
    m_dataPhase = 0.0;
    m_symbolStartPhase = 0.0;
    m_symbolEndPhase = 0.0;

    // Continuous zero bits create a standard BPSK31 idle/preamble and give RX a chance to settle.
    for (int i = 0; i < 64; ++i) {
        appendBit(false);
    }

    QString normalized = text;
    normalized.replace("\r\n", "\n");
    normalized.replace('\r', '\n');

    for (QChar ch : normalized) {
        appendCharacter(ch);
    }

    for (int i = 0; i < 64; ++i) {
        appendBit(false);
    }
}

void Bpsk31Transmitter::appendBit(bool bitOne)
{
    if (m_invertBits) {
        bitOne = !bitOne;
    }
    m_bits.append(bitOne);
}

void Bpsk31Transmitter::appendCharacter(QChar ch)
{
    const QString bits = varicodeForChar(ch);
    if (bits.isEmpty()) {
        appendCharacter(QChar(' '));
        return;
    }

    for (QChar bit : bits) {
        appendBit(bit == QChar('1'));
    }

    appendBit(false);
    appendBit(false);
}

QString Bpsk31Transmitter::varicodeForChar(QChar ch) const
{
    static const QHash<QChar, QString> table = varicodeEncodeTable();

    if (table.contains(ch)) {
        return table.value(ch);
    }

    const QChar lower = ch.toLower();
    if (table.contains(lower)) {
        return table.value(lower);
    }

    return table.value(QChar(' '));
}

void Bpsk31Transmitter::setupCurrentSymbol()
{
    if (isFinished()) {
        return;
    }

    m_symbolStartPhase = m_dataPhase;

    if (m_qpskMode) {
        const bool first = m_bits.value(m_bitIndex, true);
        const bool second = m_bits.value(m_bitIndex + 1, true);
        double delta = 0.0;
        if (first && second) {
            delta = 0.0;
        } else if (!first && second) {
            delta = (M_PI / 2.0);
        } else if (first && !second) {
            delta = -(M_PI / 2.0);
        } else {
            delta = M_PI;
        }
        m_symbolEndPhase = m_dataPhase + delta;
        return;
    }

    const bool bitOne = m_bits.at(m_bitIndex);
    m_symbolEndPhase = bitOne ? m_dataPhase : (m_dataPhase + M_PI);
}
