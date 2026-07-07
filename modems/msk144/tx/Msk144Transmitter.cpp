#include "Msk144Transmitter.h"

#include "../../../third_party/mshv_gpl/port/HvGenMsk/genmesage_msk.h"

#include <QColor>
#include <QPainter>
#include <QtGlobal>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>

namespace {
constexpr int kInternalRate = 12000;
constexpr double kMshvFullScale = 8380000.0;
constexpr double kTwoPi = 6.28318530717958647692;
constexpr int kMaxFrameSamples = 4096;

QString cleanMessage(QString msg)
{
    msg = msg.trimmed().toUpper();
    msg.replace('\t', ' ');
    while (msg.contains("  ")) msg.replace("  ", " ");
    return msg.left(37);
}
}

Msk144Transmitter::Msk144Transmitter(const QString &message,
                                     int sampleRate,
                                     int periodSeconds,
                                     bool shortMessage,
                                     double txFrequencyHz)
    : m_message(cleanMessage(message)),
      m_sampleRate(qBound(8000, sampleRate, 96000)),
      m_periodSeconds(periodSeconds == 30 ? 30 : 15),
      m_shortMessage(shortMessage),
      m_txFrequencyHz(qBound(300.0, txFrequencyHz, 2700.0))
{
    buildWaveform();
}

int Msk144Transmitter::sampleRate() const { return m_sampleRate; }

int Msk144Transmitter::generate(float *output, int sampleCount)
{
    if (output == nullptr || sampleCount <= 0 || isFinished()) return 0;
    const int remaining = m_samples.size() - m_position;
    const int n = qBound(0, sampleCount, remaining);
    if (n <= 0) return 0;
    std::copy(m_samples.constData() + m_position, m_samples.constData() + m_position + n, output);
    m_position += n;
    return n;
}

bool Msk144Transmitter::isFinished() const { return m_position >= m_samples.size(); }

double Msk144Transmitter::progress() const
{
    if (m_samples.isEmpty()) return isFinished() ? 1.0 : 0.0;
    return qBound(0.0, static_cast<double>(m_position) / static_cast<double>(m_samples.size()), 1.0);
}

QImage Msk144Transmitter::previewImage() const { return makePreviewImage(); }

QString Msk144Transmitter::description() const
{
    return QStringLiteral("MSK144 TX: %1, %2 s%3")
        .arg(m_unpackedMessage.isEmpty() ? m_message : m_unpackedMessage)
        .arg(m_periodSeconds)
        .arg(m_shortMessage ? QStringLiteral(", short-message enabled") : QString());
}

bool Msk144Transmitter::lowLatencyTx() const { return true; }
int Msk144Transmitter::trailingSilenceSamples() const { return 0; }
bool Msk144Transmitter::generationSucceeded() const { return m_ok; }
QString Msk144Transmitter::generationError() const { return m_error; }
QString Msk144Transmitter::normalizedMessage() const { return m_unpackedMessage.isEmpty() ? m_message : m_unpackedMessage; }

void Msk144Transmitter::buildWaveform()
{
    m_samples.clear();
    m_position = 0;
    m_ok = false;
    m_error.clear();

    if (m_message.isEmpty()) {
        m_error = QStringLiteral("Empty MSK144 message.");
        return;
    }

    std::unique_ptr<int[]> frame(new int[kMaxFrameSamples]);
    std::unique_ptr<int[]> tones(new int[256]);
    std::fill(frame.get(), frame.get() + kMaxFrameSamples, 0);
    std::fill(tones.get(), tones.get() + 256, 0);

    QByteArray msgBytes = m_message.leftJustified(50, ' ', true).toLatin1();
    GenMsk generator(false);
    const int frameSamples = generator.genmsk(msgBytes.data(),
                                              1.0,
                                              tones.get(),
                                              true,
                                              frame.get(),
                                              static_cast<double>(kInternalRate),
                                              1.0,
                                              0,
                                              0,
                                              false);
    if (frameSamples <= 0 || frameSamples > kMaxFrameSamples) {
        m_error = QStringLiteral("MSHV MSK144 generator returned an invalid frame length; TX was inhibited.");
        return;
    }

    m_unpackedMessage = cleanMessage(generator.GetUnpackMsg());
    if (m_unpackedMessage.isEmpty()) m_unpackedMessage = m_message;

    const int totalSamples = qMax(1, m_periodSeconds * m_sampleRate - m_sampleRate / 20); // leave 50 ms guard
    m_samples.resize(totalSamples);
    const double internalPerOutput = static_cast<double>(kInternalRate) / static_cast<double>(m_sampleRate);
    for (int i = 0; i < totalSamples; ++i) {
        const double src = std::fmod(static_cast<double>(i) * internalPerOutput, static_cast<double>(frameSamples));
        const int i0 = qBound(0, static_cast<int>(std::floor(src)), frameSamples - 1);
        const int i1 = (i0 + 1) % frameSamples;
        const double frac = src - static_cast<double>(i0);
        const double v0 = static_cast<double>(frame[i0]) / kMshvFullScale;
        const double v1 = static_cast<double>(frame[i1]) / kMshvFullScale;
        const double v = (1.0 - frac) * v0 + frac * v1;
        m_samples[i] = static_cast<float>(qBound(-0.92, 0.62 * v, 0.92));
    }
    m_ok = !m_samples.isEmpty();
}

void Msk144Transmitter::buildFallbackMskLikeWaveform()
{
    const int totalSamples = qMax(1, m_periodSeconds * m_sampleRate - m_sampleRate / 20);
    m_samples.resize(totalSamples);
    double phase = 0.0;
    const QByteArray bits = m_message.toLatin1();
    const int symbolSamples = qMax(1, m_sampleRate / 2000);
    for (int i = 0; i < totalSamples; ++i) {
        const int symbol = (i / symbolSamples) % qMax(1, bits.size() * 8);
        const int byteIndex = symbol / 8;
        const int bitIndex = symbol % 8;
        const bool bit = byteIndex < bits.size() ? ((bits.at(byteIndex) >> bitIndex) & 0x01) : false;
        const double f = bit ? 2000.0 : 1000.0;
        phase += kTwoPi * f / static_cast<double>(m_sampleRate);
        if (phase > kTwoPi) phase -= kTwoPi;
        m_samples[i] = static_cast<float>(0.25 * std::sin(phase));
    }
    m_unpackedMessage = m_message;
    m_ok = true;
}

QImage Msk144Transmitter::makePreviewImage() const
{
    QImage image(480, 120, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(8, 10, 12));
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QColor(255, 176, 43));
    p.drawText(QRect(12, 10, image.width() - 24, 28), Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("MSK144 %1 s TX").arg(m_periodSeconds));
    p.setPen(QColor(210, 230, 255));
    p.drawText(QRect(12, 42, image.width() - 24, 32), Qt::AlignLeft | Qt::AlignVCenter,
               m_unpackedMessage.isEmpty() ? m_message : m_unpackedMessage);
    p.setPen(QColor(90, 160, 255));
    p.drawText(QRect(12, 78, image.width() - 24, 26), Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("144-bit frames, repeated across the selected T/R period"));
    return image;
}
