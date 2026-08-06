#include "RttyTransmitter.h"

#include <QColor>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QTextOption>
#include <QtMath>

namespace {

constexpr double kTwoPi = 2.0 * M_PI;

QHash<QChar, int> lettersTable()
{
    return {
        {'E', 1}, {'\n', 2}, {'A', 3}, {' ', 4}, {'S', 5},
        {'I', 6}, {'U', 7}, {'\r', 8}, {'D', 9}, {'R', 10},
        {'J', 11}, {'N', 12}, {'F', 13}, {'C', 14}, {'K', 15},
        {'T', 16}, {'Z', 17}, {'L', 18}, {'W', 19}, {'H', 20},
        {'Y', 21}, {'P', 22}, {'Q', 23}, {'O', 24}, {'B', 25},
        {'G', 26}, {'M', 28}, {'X', 29}, {'V', 30}
    };
}

QHash<QChar, int> figuresTable()
{
    return {
        {'3', 1}, {'\n', 2}, {'-', 3}, {' ', 4}, {'\'', 5},
        {'8', 6}, {'7', 7}, {'\r', 8}, {'$', 9}, {'4', 10},
        {'\a', 11}, {',', 12}, {'!', 13}, {':', 14}, {'(', 15},
        {'5', 16}, {'"', 17}, {')', 18}, {'2', 19}, {'#', 20},
        {'6', 21}, {'0', 22}, {'1', 23}, {'9', 24}, {'?', 25},
        {'&', 26}, {'.', 28}, {'/', 29}, {';', 30}
    };
}

} // namespace

RttyTransmitter::RttyTransmitter(const QString &text,
                                 int sampleRate,
                                 double baudRate,
                                 double markHz,
                                 double spaceHz,
                                 bool reverse)
    : m_text(text),
      m_sampleRate(qBound(8000, sampleRate, 192000)),
      m_baudRate(qBound(10.0, baudRate, 300.0)),
      m_markHz(qBound(300.0, markHz, 3500.0)),
      m_spaceHz(qBound(300.0, spaceHz, 3500.0)),
      m_reverse(reverse)
{
    m_symbolSamples = static_cast<double>(m_sampleRate) / m_baudRate;
    buildSegments(text);

    if (!m_segments.isEmpty()) {
        m_segmentRemaining = m_segments.first().samples;
    }
}

int RttyTransmitter::sampleRate() const
{
    return m_sampleRate;
}

int RttyTransmitter::generate(float *output, int sampleCount)
{
    if (output == nullptr || sampleCount <= 0 || isFinished()) {
        return 0;
    }

    int generated = 0;

    while (generated < sampleCount && !isFinished()) {
        if (m_segmentRemaining <= 0) {
            ++m_segmentIndex;
            if (m_segmentIndex >= m_segments.size()) {
                break;
            }
            m_segmentRemaining = m_segments[m_segmentIndex].samples;
        }

        const bool markState = currentMarkState();
        m_scopeMarkState.store(markState, std::memory_order_relaxed);
        const double frequency = markState ? m_markHz : m_spaceHz;
        const double phaseInc = kTwoPi * frequency / static_cast<double>(m_sampleRate);
        output[generated] = static_cast<float>(0.62 * qSin(m_phase));
        m_phase += phaseInc;
        if (m_phase >= kTwoPi) {
            m_phase -= kTwoPi;
        }

        --m_segmentRemaining;
        ++m_generatedSamples;
        ++generated;
    }

    return generated;
}

bool RttyTransmitter::isFinished() const
{
    return m_segmentIndex >= m_segments.size();
}

double RttyTransmitter::progress() const
{
    if (m_totalSamples <= 0) {
        return 1.0;
    }

    return qBound(0.0, static_cast<double>(m_generatedSamples) / static_cast<double>(m_totalSamples), 1.0);
}

QImage RttyTransmitter::previewImage() const
{
    return previewTextImage(m_text);
}

QString RttyTransmitter::description() const
{
    return QString("RTTY TX: %1 baud, mark %2 Hz, space %3 Hz, %4 char(s)")
        .arg(m_baudRate, 0, 'f', 2)
        .arg(m_markHz, 0, 'f', 0)
        .arg(m_spaceHz, 0, 'f', 0)
        .arg(m_text.size());
}

bool RttyTransmitter::rttyToneState(bool *markOut) const
{
    if (markOut == nullptr) {
        return false;
    }

    *markOut = m_scopeMarkState.load(std::memory_order_relaxed);
    return true;
}

QImage RttyTransmitter::previewTextImage(const QString &text)
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
                     text.isEmpty() ? QString("RTTY TX text is empty") : text,
                     option);

    return image;
}

void RttyTransmitter::buildSegments(const QString &text)
{
    m_segments.clear();
    m_totalSamples = 0;
    m_segmentIndex = 0;
    m_segmentRemaining = 0;
    m_figuresShift = false;

    // Lead-in idle mark lets a receiving decoder settle before the start bit.
    appendBit(true, 20.0);
    ensureShift(false);

    QString normalized = text;
    normalized.replace("\r\n", "\n");
    normalized.replace('\r', '\n');
    normalized = normalized.toUpper();

    for (QChar ch : normalized) {
        appendCharacter(ch);
    }

    appendBit(true, 20.0);
}

void RttyTransmitter::appendBit(bool mark, double bitCount)
{
    const int samples = qMax(1, static_cast<int>(qRound(m_symbolSamples * bitCount)));

    if (!m_segments.isEmpty() && m_segments.last().mark == mark) {
        Segment &last = m_segments.last();
        last.samples += samples;
        m_totalSamples += samples;
        return;
    }

    Segment segment;
    segment.mark = mark;
    segment.samples = samples;
    m_segments.append(segment);
    m_totalSamples += samples;
}

void RttyTransmitter::appendCharacter(QChar ch)
{
    int code = 0;
    bool figures = false;

    if (!findIta2Code(ch, code, figures)) {
        ch = ' ';
        if (!findIta2Code(ch, code, figures)) {
            return;
        }
    }

    ensureShift(figures);
    appendCode(code);

    if (ch == '\n') {
        appendCode(8); // CR after LF improves compatibility with old terminals.
    }
}

void RttyTransmitter::appendCode(int code)
{
    appendBit(false); // start bit = space

    for (int bit = 0; bit < 5; ++bit) {
        appendBit((code & (1 << bit)) != 0);
    }

    appendBit(true, 1.5); // 1.5 stop bits, common 5N1.5 RTTY framing.
}

bool RttyTransmitter::findIta2Code(QChar ch, int &code, bool &figures) const
{
    static const QHash<QChar, int> letters = lettersTable();
    static const QHash<QChar, int> figs = figuresTable();

    if (letters.contains(ch)) {
        code = letters.value(ch);
        figures = false;
        return true;
    }

    if (figs.contains(ch)) {
        code = figs.value(ch);
        figures = true;
        return true;
    }

    return false;
}

void RttyTransmitter::ensureShift(bool figures)
{
    if (m_figuresShift == figures) {
        return;
    }

    appendCode(figures ? 27 : 31);
    m_figuresShift = figures;
}

bool RttyTransmitter::currentMarkState() const
{
    if (m_segmentIndex >= m_segments.size()) {
        return true;
    }

    bool mark = m_segments[m_segmentIndex].mark;
    if (m_reverse) {
        mark = !mark;
    }

    return mark;
}

double RttyTransmitter::currentFrequency() const
{
    return currentMarkState() ? m_markHz : m_spaceHz;
}
