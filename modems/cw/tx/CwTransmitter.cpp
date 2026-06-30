#include "CwTransmitter.h"

#include <QColor>
#include <QFont>
#include <QHash>
#include <QImage>
#include <QPainter>
#include <QTextOption>
#include <QtMath>

namespace {
constexpr double kTwoPi = 6.283185307179586476925286766559;
constexpr double kPi = 3.1415926535897932384626433832795;
constexpr double kDefaultCwFilterBandwidthHz = 110.0;
constexpr double kCwTxLevel = 0.42;

QHash<QChar, QString> morseEncodeTable()
{
    return {
        {'A', ".-"}, {'B', "-..."}, {'C', "-.-."}, {'D', "-.."}, {'E', "."},
        {'F', "..-."}, {'G', "--."}, {'H', "...."}, {'I', ".."}, {'J', ".---"},
        {'K', "-.-"}, {'L', ".-.."}, {'M', "--"}, {'N', "-."}, {'O', "---"},
        {'P', ".--."}, {'Q', "--.-"}, {'R', ".-."}, {'S', "..."}, {'T', "-"},
        {'U', "..-"}, {'V', "...-"}, {'W', ".--"}, {'X', "-..-"}, {'Y', "-.--"},
        {'Z', "--.."},
        {'1', ".----"}, {'2', "..---"}, {'3', "...--"}, {'4', "....-"}, {'5', "....."},
        {'6', "-...."}, {'7', "--..."}, {'8', "---.."}, {'9', "----."}, {'0', "-----"},
        {'.', ".-.-.-"}, {',', "--..--"}, {'?', "..--.."}, {'/', "-..-."},
        {'=', "-...-"}, {'+', ".-.-."}, {'-', "-....-"}, {'(', "-.--."},
        {')', "-.--.-"}, {'@', ".--.-."}, {'!', "-.-.--"}, {'&', ".-..."},
        {':', "---..."}, {';', "-.-.-."}, {'\'', ".----."}, {'\"', ".-..-."}
    };
}
}

void CwTransmitter::Biquad::configureBandPass(double sampleRate, double centerHz, double bandwidthHz)
{
    const double safeSampleRate = qMax(8000.0, sampleRate);
    const double safeCenter = qBound(100.0, centerHz, (safeSampleRate * 0.45));
    const double safeBandwidth = qBound(40.0, bandwidthHz, 300.0);
    const double q = qBound(0.707, safeCenter / safeBandwidth, 30.0);
    const double w0 = kTwoPi * safeCenter / safeSampleRate;
    const double cosW0 = qCos(w0);
    const double sinW0 = qSin(w0);
    const double alpha = sinW0 / (2.0 * q);

    const double a0 = 1.0 + alpha;
    b0 = alpha / a0;
    b1 = 0.0;
    b2 = -alpha / a0;
    a1 = (-2.0 * cosW0) / a0;
    a2 = (1.0 - alpha) / a0;
    reset();
}

void CwTransmitter::Biquad::reset()
{
    z1 = 0.0;
    z2 = 0.0;
}

double CwTransmitter::Biquad::process(double x)
{
    const double y = (b0 * x) + z1;
    z1 = (b1 * x) - (a1 * y) + z2;
    z2 = (b2 * x) - (a2 * y);
    return y;
}

CwTransmitter::CwTransmitter(const QString &text,
                             int sampleRate,
                             double toneHz,
                             double wpm)
    : m_text(text),
      m_sampleRate(qBound(8000, sampleRate, 192000)),
      m_toneHz(qBound(250.0, toneHz, 3500.0)),
      m_wpm(qBound(5.0, wpm, 60.0))
{
    m_dotSamples = (1.2 / m_wpm) * static_cast<double>(m_sampleRate);

    /* 8 ms raised-cosine shaping is fast enough for ordinary CW speeds but
     * prevents the hard transitions that make the TX waterfall fill with comb
     * lines and harmonics.  Keep it shorter than half a dot at high WPM.
     */
    const qint64 preferredEdge = qRound(0.008 * static_cast<double>(m_sampleRate));
    const qint64 maxEdgeForSpeed = qMax<qint64>(1, qRound(0.35 * m_dotSamples));
    m_edgeSamples = qMax<qint64>(1, qMin(preferredEdge, maxEdgeForSpeed));

    configureOutputFilter();
    buildSegments(text);
}

int CwTransmitter::sampleRate() const
{
    return m_sampleRate;
}

int CwTransmitter::generate(float *output, int sampleCount)
{
    if (output == nullptr || sampleCount <= 0 || isFinished()) {
        return 0;
    }

    int generated = 0;

    while (generated < sampleCount && !isFinished()) {
        const Segment &seg = m_segments.at(m_segmentIndex);
        const double envelope = shapedEnvelope(seg);
        output[generated] = static_cast<float>(filteredCarrierSample(envelope));

        ++generated;
        ++m_generatedSamples;
        ++m_segmentSampleIndex;

        while (m_segmentIndex < m_segments.size() &&
               m_segmentSampleIndex >= m_segments.at(m_segmentIndex).samples) {
            m_segmentSampleIndex -= m_segments.at(m_segmentIndex).samples;
            ++m_segmentIndex;
            if (m_segmentIndex >= m_segments.size()) {
                break;
            }
        }
    }

    return generated;
}

bool CwTransmitter::isFinished() const
{
    return m_segmentIndex >= m_segments.size();
}

double CwTransmitter::progress() const
{
    if (m_totalSamples <= 0) {
        return 1.0;
    }
    return qBound(0.0,
                  static_cast<double>(m_generatedSamples) / static_cast<double>(m_totalSamples),
                  1.0);
}

QImage CwTransmitter::previewImage() const
{
    return previewTextImage(m_text);
}

QString CwTransmitter::description() const
{
    return QString("CW TX: %1 Hz sine, %2 WPM, %3 Hz BPF, %4 char(s)")
        .arg(m_toneHz, 0, 'f', 0)
        .arg(m_wpm, 0, 'f', 0)
        .arg(kDefaultCwFilterBandwidthHz, 0, 'f', 0)
        .arg(m_text.size());
}

QImage CwTransmitter::previewTextImage(const QString &text)
{
    QImage image(800, 520, QImage::Format_RGB32);
    image.fill(QColor(10, 14, 12));

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(image.rect(), QColor(10, 14, 12));
    painter.setPen(QColor(140, 255, 120));
    painter.setFont(QFont("Monospace", 20));

    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    painter.drawText(QRect(24, 24, image.width() - 48, image.height() - 48),
                     text.trimmed().isEmpty() ? QString("CW TX text is empty") : text.toUpper(),
                     option);
    return image;
}

void CwTransmitter::buildSegments(const QString &text)
{
    m_segments.clear();
    m_segmentIndex = 0;
    m_segmentSampleIndex = 0;
    m_generatedSamples = 0;
    m_totalSamples = 0;

    const QString normalized = text.toUpper().simplified();
    bool previousWasCharacter = false;

    for (QChar ch : normalized) {
        if (ch.isSpace()) {
            if (!m_segments.isEmpty()) {
                appendKey(false, 7.0);
            }
            previousWasCharacter = false;
            continue;
        }

        const QString code = morseForChar(ch);
        if (code.isEmpty()) {
            continue;
        }

        if (previousWasCharacter) {
            appendKey(false, 3.0);
        }

        for (int i = 0; i < code.size(); ++i) {
            appendKey(true, code.at(i) == '-' ? 3.0 : 1.0);
            if (i + 1 < code.size()) {
                appendKey(false, 1.0);
            }
        }
        previousWasCharacter = true;
    }

    /* Give the cascaded band-pass a real zero-input tail so the final edge
     * decays cleanly before TxAudioEngine adds its generic backend silence.
     */
    appendKey(false, 4.0);
}

void CwTransmitter::appendKey(bool down, double dotUnits)
{
    const qint64 samples = qMax<qint64>(1, qRound(dotUnits * m_dotSamples));
    if (!m_segments.isEmpty() && m_segments.last().keyDown == down) {
        m_segments.last().samples += samples;
    } else {
        Segment segment;
        segment.keyDown = down;
        segment.samples = samples;
        m_segments.append(segment);
    }
    m_totalSamples += samples;
}

QString CwTransmitter::morseForChar(QChar ch) const
{
    static const QHash<QChar, QString> table = morseEncodeTable();
    return table.value(ch.toUpper(), QString());
}

void CwTransmitter::configureOutputFilter()
{
    m_bandPass1.configureBandPass(static_cast<double>(m_sampleRate), m_toneHz, kDefaultCwFilterBandwidthHz);
    m_bandPass2.configureBandPass(static_cast<double>(m_sampleRate), m_toneHz, kDefaultCwFilterBandwidthHz);
    m_bandPass3.configureBandPass(static_cast<double>(m_sampleRate), m_toneHz, kDefaultCwFilterBandwidthHz);
}

double CwTransmitter::shapedEnvelope(const Segment &seg) const
{
    if (!seg.keyDown) {
        return 0.0;
    }

    const qint64 edge = qMax<qint64>(1, qMin(m_edgeSamples, seg.samples / 2));
    if (edge <= 1) {
        return 1.0;
    }

    double envelope = 1.0;

    if (m_segmentSampleIndex < edge) {
        const double x = qBound(0.0, static_cast<double>(m_segmentSampleIndex) / static_cast<double>(edge), 1.0);
        envelope = qMin(envelope, 0.5 - (0.5 * qCos(kPi * x)));
    }

    const qint64 remaining = qMax<qint64>(0, seg.samples - m_segmentSampleIndex);
    if (remaining < edge) {
        const double x = qBound(0.0, static_cast<double>(remaining) / static_cast<double>(edge), 1.0);
        envelope = qMin(envelope, 0.5 - (0.5 * qCos(kPi * x)));
    }

    return qBound(0.0, envelope, 1.0);
}

double CwTransmitter::filteredCarrierSample(double envelope)
{
    const double phaseInc = kTwoPi * m_toneHz / static_cast<double>(m_sampleRate);
    const double raw = envelope * qSin(m_phase);

    m_phase += phaseInc;
    while (m_phase >= kTwoPi) {
        m_phase -= kTwoPi;
    }

    double y = m_bandPass1.process(raw);
    y = m_bandPass2.process(y);
    y = m_bandPass3.process(y);

    return qBound(-0.95, kCwTxLevel * y, 0.95);
}
