#include "Q65Transmitter.h"

#include "../../weak_signal/WeakSignalCodecLock.h"
#include "../../../third_party/mshv_gpl/port/HvGenQ65/gen_q65.h"

#include <QColor>
#include <QPainter>
#include <QtGlobal>
#include <QtMath>

#include <algorithm>
#include <array>
#include <cmath>
#include <mutex>

namespace {
constexpr int kProtocolRate = 12000;
constexpr int kSymbols = 85;
constexpr double kTwoPi = 6.283185307179586476925286766559;

QString cleanMessage(QString msg)
{
    msg = msg.trimmed().toUpper();
    msg.replace('\t', ' ');
    while (msg.contains(QStringLiteral("  "))) msg.replace(QStringLiteral("  "), QStringLiteral(" "));
    return msg.left(37);
}

int protocolSymbolSamples(int periodSeconds)
{
    switch (periodSeconds) {
    case 15: return 1800;
    case 30: return 3600;
    case 120: return 16000;
    case 60:
    default: return 7200;
    }
}

double raisedCosineEdgeGain(int sample, int activeSamples, int rampSamples)
{
    if (rampSamples <= 0 || activeSamples <= 1) return 1.0;
    const int fromStart = sample + 1;
    const int fromEnd = activeSamples - sample;
    const int edge = qMin(fromStart, fromEnd);
    if (edge >= rampSamples) return 1.0;
    return 0.5 - 0.5 * std::cos((0.5 * kTwoPi) * static_cast<double>(edge) /
                                static_cast<double>(rampSamples));
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

bool Q65Transmitter::lowLatencyTx() const { return true; }
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

    std::array<int, kSymbols> tones{};
    {
        std::lock_guard<std::mutex> guard(WeakSignalCodecLock::mutex());
        GenQ65 generator(false);
        generator.resetGeneratorHashState();
        generator.genq65itone(m_message, tones.data(), true);
        m_unpackedMessage = cleanMessage(generator.GetUnpackMsg());
    }
    if (m_unpackedMessage.isEmpty()) m_unpackedMessage = m_message;

    const int nsps = protocolSymbolSamples(m_periodSeconds);
    const double baud = static_cast<double>(kProtocolRate) / nsps;
    const int spacing = Q65Mode::mshvToneSpacingMultiplier(m_submode);
    const double highestTone = m_txFrequencyHz + 64.0 * spacing * baud;
    const int minimumBase = Q65Mode::minimumBaseToneHz(m_submode, m_periodSeconds,
                                                       kProtocolRate);
    const int maximumBase = Q65Mode::maximumBaseToneHz(m_submode, m_periodSeconds,
                                                       kProtocolRate);
    if (m_txFrequencyHz < minimumBase || m_txFrequencyHz > maximumBase ||
        highestTone >= 0.48 * m_sampleRate) {
        m_error = QStringLiteral("Q65 base frequency falls outside the complete native RX/TX spectral window; TX was inhibited.");
        return;
    }

    // End before the following UTC period even if the precise timer, CAT/PTT
    // and platform audio stream add bounded startup latency. The useful
    // 85-symbol Q65 waveform already leaves at least 2.25 s at the end of every
    // supported period, so this 300 ms guard removes silence only and never
    // truncates a symbol.
    const int outSamples = qMax(1, m_periodSeconds * m_sampleRate - 3 * m_sampleRate / 10);
    m_samples.fill(0.0f, outSamples);
    const double samplesPerSymbol = static_cast<double>(nsps) * m_sampleRate / kProtocolRate;
    const int transmittedSamples = qMin(outSamples,
        static_cast<int>(std::llround(kSymbols * samplesPerSymbol)));
    const int rampSamples = qMax(1, m_sampleRate / 200); // 5 ms cosine edge.
    double phase = 0.0;
    for (int i = 0; i < transmittedSamples; ++i) {
        const int symbol = qBound(0, static_cast<int>(i / samplesPerSymbol), kSymbols - 1);
        const double frequency = m_txFrequencyHz + spacing * baud * tones[static_cast<std::size_t>(symbol)];
        phase += kTwoPi * frequency / static_cast<double>(m_sampleRate);
        if (phase >= kTwoPi) phase -= kTwoPi;
        const double gain = raisedCosineEdgeGain(i, transmittedSamples, rampSamples);
        m_samples[i] = static_cast<float>(0.58 * gain * std::sin(phase));
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
               QStringLiteral("85 symbols · Q65A/B/C/D · native RX/TX codec"));
    return image;
}
