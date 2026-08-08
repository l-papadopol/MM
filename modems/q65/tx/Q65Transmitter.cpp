#include "Q65Transmitter.h"

#include "../../../third_party/mshv_gpl/port/HvGenQ65/gen_q65.h"

#include <QColor>
#include <QPainter>
#include <QtGlobal>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <memory>

namespace {
constexpr int kMshvRate = 48000;
constexpr double kMshvFullScale = 8380000.0;

QString cleanMessage(QString msg)
{
    msg = msg.trimmed().toUpper();
    msg.replace('\t', ' ');
    while (msg.contains(QStringLiteral("  "))) msg.replace(QStringLiteral("  "), QStringLiteral(" "));
    return msg.left(37);
}
}

Q65Transmitter::Q65Transmitter(const QString &message,
                               int sampleRate,
                               int periodSeconds,
                               Q65Mode::Submode submode,
                               double txFrequencyHz)
    : m_message(cleanMessage(message)),
      m_sampleRate(qBound(8000, sampleRate, 96000)),
      m_periodSeconds((periodSeconds == 15 || periodSeconds == 30 || periodSeconds == 60 || periodSeconds == 120) ? periodSeconds : 60),
      m_submode(submode),
      m_txFrequencyHz(qBound(300.0, txFrequencyHz, 2700.0))
{
    buildWaveform();
}

int Q65Transmitter::sampleRate() const { return m_sampleRate; }

int Q65Transmitter::generate(float *output, int sampleCount)
{
    if (output == nullptr || sampleCount <= 0 || isFinished()) return 0;
    const int remaining = m_samples.size() - m_position;
    const int n = qBound(0, sampleCount, remaining);
    if (n <= 0) return 0;
    std::copy(m_samples.constData() + m_position, m_samples.constData() + m_position + n, output);
    m_position += n;
    return n;
}

bool Q65Transmitter::isFinished() const { return m_position >= m_samples.size(); }

double Q65Transmitter::progress() const
{
    if (m_samples.isEmpty()) return isFinished() ? 1.0 : 0.0;
    return qBound(0.0, static_cast<double>(m_position) / static_cast<double>(m_samples.size()), 1.0);
}

QImage Q65Transmitter::previewImage() const { return makePreviewImage(); }

QString Q65Transmitter::description() const
{
    return QStringLiteral("%1 TX: %2, %3 s")
        .arg(Q65Mode::modeName(m_submode), m_unpackedMessage.isEmpty() ? m_message : m_unpackedMessage)
        .arg(m_periodSeconds);
}

bool Q65Transmitter::lowLatencyTx() const { return false; }
int Q65Transmitter::trailingSilenceSamples() const { return 0; }
bool Q65Transmitter::generationSucceeded() const { return m_ok; }
QString Q65Transmitter::generationError() const { return m_error; }
QString Q65Transmitter::normalizedMessage() const { return m_unpackedMessage.isEmpty() ? m_message : m_unpackedMessage; }

void Q65Transmitter::buildWaveform()
{
    m_samples.clear();
    m_position = 0;
    m_ok = false;
    m_error.clear();

    if (m_message.isEmpty()) {
        m_error = QStringLiteral("Empty Q65 message.");
        return;
    }

    const int internalSamples = qMax(1, m_periodSeconds * kMshvRate);
    std::unique_ptr<int[]> wave(new int[internalSamples + kMshvRate]);
    std::fill(wave.get(), wave.get() + internalSamples + kMshvRate, 0);

    GenQ65 generator(false);
    const int produced = generator.genq65(m_message,
                                          wave.get(),
                                          static_cast<double>(kMshvRate),
                                          m_txFrequencyHz,
                                          Q65Mode::mshvToneSpacingMultiplier(m_submode),
                                          m_periodSeconds);
    if (produced <= 0 || produced > internalSamples + kMshvRate) {
        m_error = QStringLiteral("MSHV Q65 generator returned an invalid waveform length; TX was inhibited.");
        return;
    }

    m_unpackedMessage = cleanMessage(generator.GetUnpackMsg());
    if (m_unpackedMessage.isEmpty()) m_unpackedMessage = m_message;

    const int outSamples = qMax(1, m_periodSeconds * m_sampleRate);
    m_samples.resize(outSamples);
    const double internalPerOutput = static_cast<double>(kMshvRate) / static_cast<double>(m_sampleRate);
    for (int i = 0; i < outSamples; ++i) {
        const double src = qMin(static_cast<double>(produced - 1), static_cast<double>(i) * internalPerOutput);
        const int i0 = qBound(0, static_cast<int>(std::floor(src)), produced - 1);
        const int i1 = qBound(0, i0 + 1, produced - 1);
        const double frac = src - static_cast<double>(i0);
        const double v0 = static_cast<double>(wave[i0]) / kMshvFullScale;
        const double v1 = static_cast<double>(wave[i1]) / kMshvFullScale;
        const double v = (1.0 - frac) * v0 + frac * v1;
        m_samples[i] = static_cast<float>(qBound(-0.92, 0.58 * v, 0.92));
    }
    m_ok = !m_samples.isEmpty();
}

QImage Q65Transmitter::makePreviewImage() const
{
    QImage image(480, 120, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(8, 10, 12));
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QColor(255, 176, 43));
    p.drawText(QRect(12, 10, image.width() - 24, 28), Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("%1 %2 s TX").arg(Q65Mode::modeName(m_submode)).arg(m_periodSeconds));
    p.setPen(QColor(210, 230, 255));
    p.drawText(QRect(12, 42, image.width() - 24, 32), Qt::AlignLeft | Qt::AlignVCenter,
               m_unpackedMessage.isEmpty() ? m_message : m_unpackedMessage);
    p.setPen(QColor(90, 160, 255));
    p.drawText(QRect(12, 78, image.width() - 24, 26), Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("MSHV GenQ65: 85 symbols, Q65A/B/C/D spacing, repeated period waveform"));
    return image;
}
