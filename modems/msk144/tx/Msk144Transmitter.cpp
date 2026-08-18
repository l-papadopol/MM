#include "Msk144Transmitter.h"

#include "../../weak_signal/WeakSignalCodecLock.h"
#include "../../../third_party/mshv_gpl/port/HvGenMsk/genmesage_msk.h"

#include <QColor>
#include <QPainter>
#include <QtGlobal>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <memory>
#include <mutex>

namespace {
constexpr int kInternalRate = 12000;
constexpr int kMaxFrameSamples = 4096;
constexpr double kTwoPi = 6.283185307179586476925286766559;

QString cleanMessage(QString msg)
{
    msg = msg.trimmed().toUpper();
    msg.replace('\t', ' ');
    while (msg.contains("  ")) msg.replace("  ", " ");
    return msg.left(37);
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

Msk144Transmitter::Msk144Transmitter(const QString &message,
                                     int sampleRate,
                                     int periodSeconds,
                                     bool shortMessage,
                                     double txFrequencyHz)
    : m_message(cleanMessage(message)),
      m_sampleRate(qBound(8000, sampleRate, 96000)),
      m_periodSeconds(periodSeconds == 30 ? 30 : 15),
      m_shortMessage(shortMessage),
      m_txFrequencyHz(qBound(600.0, txFrequencyHz, 2700.0))
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
        .arg(m_generatedShortMessage ? QStringLiteral(", MSK40 short frame") : QString());
}

bool Msk144Transmitter::lowLatencyTx() const { return true; }
int Msk144Transmitter::trailingSilenceSamples() const { return 0; }
bool Msk144Transmitter::generationSucceeded() const { return m_ok; }
QString Msk144Transmitter::generationError() const { return m_error; }
QString Msk144Transmitter::normalizedMessage() const { return m_unpackedMessage.isEmpty() ? m_message : m_unpackedMessage; }
bool Msk144Transmitter::generatedShortMessage() const { return m_generatedShortMessage; }

void Msk144Transmitter::buildWaveform()
{
    m_samples.clear();
    m_position = 0;
    m_ok = false;
    m_generatedShortMessage = false;
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
    int frameSamples = 0;
    {
        std::lock_guard<std::mutex> guard(WeakSignalCodecLock::mutex());
        GenMsk generator(false);
        frameSamples = generator.genmsk(msgBytes.data(),
                                        1.0,
                                        tones.get(),
                                        true,
                                        frame.get(),
                                        static_cast<double>(kInternalRate),
                                        1.0,
                                        0,
                                        0,
                                        false);
        m_unpackedMessage = cleanMessage(generator.GetUnpackMsg());
    }
    if (frameSamples <= 0 || frameSamples > kMaxFrameSamples) {
        m_error = QStringLiteral("MSK144 codec returned an invalid frame length; TX was inhibited.");
        return;
    }
    const int symbolCount = frameSamples / 6;
    if (frameSamples % 6 != 0 || (symbolCount != 144 && symbolCount != 40)) {
        m_error = QStringLiteral("MSK144 codec returned invalid symbol geometry; TX was inhibited.");
        return;
    }
    m_generatedShortMessage = symbolCount == 40;
    if (m_generatedShortMessage && !m_shortMessage) {
        m_error = QStringLiteral("MSK40 message selected while short messages are disabled; TX was inhibited.");
        return;
    }
    if (m_unpackedMessage.isEmpty()) m_unpackedMessage = m_message;

    // The protocol generator supplies the exact 144/40 coded tone sequence.
    // MadModem owns waveform synthesis so the selected audio centre is real:
    // the two tones are always centre +/-500 Hz.  Synthesize the whole period
    // at the actual output rate instead of looping a sampled frame.  Looping a
    // frame reset the carrier phase whenever centre*72 ms was not an integer
    // number of cycles, producing clicks and breaking coherent averaging.
    const double lowToneHz = m_txFrequencyHz - 500.0;
    const double highToneHz = m_txFrequencyHz + 500.0;
    if (lowToneHz <= 0.0 || highToneHz >= 0.48 * m_sampleRate) {
        m_error = QStringLiteral("MSK144 TX tones fall outside the selected audio passband; TX was inhibited.");
        return;
    }
    // Leave at least 300 ms at the period tail for the bounded scheduler wakeup
    // plus CAT/PTT and audio-device startup latency. Quantize the active time to
    // a whole number of 72 ms (or 20 ms) protocol frames: the last repeat must
    // not be cut halfway through merely because the output rate is not 12 kHz.
    const int maximumOutputSamples = qMax(1,
        m_periodSeconds * m_sampleRate - 3 * m_sampleRate / 10);
    const qint64 maximumProtocolSamples =
        (static_cast<qint64>(maximumOutputSamples) * kInternalRate) / m_sampleRate;
    const qint64 completeProtocolSamples =
        qMax<qint64>(frameSamples,
                     (maximumProtocolSamples / frameSamples) * frameSamples);
    const int totalSamples = static_cast<int>(
        (completeProtocolSamples * m_sampleRate) / kInternalRate);
    m_samples.resize(totalSamples);
    const double protocolSamplesPerOutputSample =
        static_cast<double>(kInternalRate) / static_cast<double>(m_sampleRate);
    const int rampSamples = qMax(1, m_sampleRate / 200); // 5 ms cosine edge.
    double phase = 0.0;
    for (int i = 0; i < totalSamples; ++i) {
        const double framePosition = std::fmod(
            static_cast<double>(i) * protocolSamplesPerOutputSample,
            static_cast<double>(frameSamples));
        const int symbol = qBound(0, static_cast<int>(framePosition / 6.0), symbolCount - 1);
        const double frequency = tones[symbol] != 0 ? highToneHz : lowToneHz;
        phase += kTwoPi * frequency / static_cast<double>(m_sampleRate);
        if (phase >= kTwoPi) phase -= kTwoPi;
        const double gain = raisedCosineEdgeGain(i, totalSamples, rampSamples);
        m_samples[i] = static_cast<float>(0.62 * gain * std::sin(phase));
    }
    m_ok = !m_samples.isEmpty();
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
               m_generatedShortMessage
                   ? QStringLiteral("40-symbol hashed short frames at the selected audio centre")
                   : QStringLiteral("144-symbol frames at the selected audio centre"));
    return image;
}
