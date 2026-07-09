#include "Ft8Transmitter.h"

#include "../Ft8Mode.h"
#include "../../../third_party/mshv_gpl/port/HvGenFt8/gen_ft8.h"
#include "../../../third_party/mshv_gpl/port/HvGenFt4/gen_ft4.h"

#include <QColor>
#include <QPainter>
#include <QtGlobal>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <memory>

namespace {

constexpr double kTwoPi = 6.28318530717958647692;
constexpr int kFtSampleRate = 48000;
constexpr int kMaxMshvSamples = 1000000;
constexpr double kMshvFullScale = 8380000.0;

QString cleanFtMessage(QString message)
{
    message = message.trimmed().toUpper();
    message.replace('\t', ' ');
    while (message.contains("  ")) {
        message.replace("  ", " ");
    }
    return message.left(37);
}

QString cleanModeName(const QString &modeName)
{
    const QString key = modeName.trimmed().toUpper();
    return (key == QStringLiteral("FT4")) ? QStringLiteral("FT4") : QStringLiteral("FT8");
}


} // namespace

Ft8Transmitter::Ft8Transmitter(const QString &message,
                               int sampleRate,
                               double frequencyHz)
    : Ft8Transmitter(QStringLiteral("FT8"), message, sampleRate, frequencyHz)
{
}

Ft8Transmitter::Ft8Transmitter(const QString &modeName,
                               const QString &message,
                               int sampleRate,
                               double frequencyHz,
                               int leadingSilenceMs)
    : m_sampleRate(kFtSampleRate),
      m_frequencyHz(qBound(100.0, frequencyHz, 3000.0)),
      m_modeName(cleanModeName(modeName)),
      m_message(cleanFtMessage(message))
{
    Q_UNUSED(sampleRate)
    buildMessageWaveform(m_message, m_frequencyHz);
    if (m_ok && leadingSilenceMs > 0) {
        prependSilence(leadingSilenceMs);
    }
}

Ft8Transmitter::Ft8Transmitter(int sampleRate,
                               double frequencyHz,
                               double durationSeconds,
                               bool tuneMode)
    : Ft8Transmitter(QStringLiteral("FT8"), sampleRate, frequencyHz, durationSeconds, tuneMode)
{
}

Ft8Transmitter::Ft8Transmitter(const QString &modeName,
                               int sampleRate,
                               double frequencyHz,
                               double durationSeconds,
                               bool tuneMode)
    : m_sampleRate(kFtSampleRate),
      m_frequencyHz(qBound(100.0, frequencyHz, 3000.0)),
      m_modeName(cleanModeName(modeName)),
      m_message(QStringLiteral("TUNE")),
      m_tuneMode(tuneMode)
{
    Q_UNUSED(sampleRate)
    buildTuneWaveform(m_frequencyHz, durationSeconds);
}

int Ft8Transmitter::sampleRate() const
{
    return m_sampleRate;
}

int Ft8Transmitter::generate(float *output, int sampleCount)
{
    if (output == nullptr || sampleCount <= 0 || isFinished()) {
        return 0;
    }

    const int remaining = m_samples.size() - m_position;
    const int n = qBound(0, sampleCount, remaining);

    if (n <= 0) {
        return 0;
    }

    std::copy(m_samples.constData() + m_position,
              m_samples.constData() + m_position + n,
              output);
    m_position += n;
    return n;
}

bool Ft8Transmitter::isFinished() const
{
    return m_position >= m_samples.size();
}

double Ft8Transmitter::progress() const
{
    if (m_samples.isEmpty()) {
        return isFinished() ? 1.0 : 0.0;
    }

    return qBound(0.0,
                  static_cast<double>(m_position) / static_cast<double>(m_samples.size()),
                  1.0);
}

QImage Ft8Transmitter::previewImage() const
{
    return makePreviewImage();
}

QString Ft8Transmitter::description() const
{
    if (m_tuneMode) {
        return QStringLiteral("%1 Tune: %2 Hz, %3 s")
            .arg(m_modeName)
            .arg(m_frequencyHz, 0, 'f', 0)
            .arg(static_cast<double>(m_samples.size()) / static_cast<double>(m_sampleRate), 0, 'f', 1);
    }

    return QStringLiteral("%1 TX: %2 @ %3 Hz")
        .arg(m_modeName,
             m_unpackedMessage.isEmpty() ? m_message : m_unpackedMessage)
        .arg(m_frequencyHz, 0, 'f', 0);
}

bool Ft8Transmitter::lowLatencyTx() const
{
    return true;
}

int Ft8Transmitter::trailingSilenceSamples() const
{
    return 0;
}

QString Ft8Transmitter::normalizedMessage() const
{
    return m_unpackedMessage.isEmpty() ? m_message : m_unpackedMessage;
}

bool Ft8Transmitter::generationSucceeded() const
{
    return m_ok;
}

QString Ft8Transmitter::generationError() const
{
    return m_error;
}

QString Ft8Transmitter::modeName() const
{
    return m_modeName;
}



void Ft8Transmitter::skipInitialMilliseconds(int milliseconds)
{
    const int samples = qMax(0, (milliseconds * m_sampleRate) / 1000);
    skipInitialSamples(samples);
}

void Ft8Transmitter::prependLeadingSilenceMilliseconds(int milliseconds)
{
    prependSilence(milliseconds);
}

void Ft8Transmitter::skipInitialSamples(int samples)
{
    if (samples <= 0 || m_samples.isEmpty()) {
        return;
    }
    m_position = qBound(0, samples, m_samples.size());
}

void Ft8Transmitter::prependSilence(int milliseconds)
{
    const int silenceSamples = qBound(0, (milliseconds * m_sampleRate) / 1000, m_sampleRate * 2);
    if (silenceSamples <= 0 || m_samples.isEmpty()) {
        return;
    }

    QVector<float> shifted;
    shifted.resize(silenceSamples + m_samples.size());
    std::fill(shifted.begin(), shifted.begin() + silenceSamples, 0.0f);
    std::copy(m_samples.constBegin(), m_samples.constEnd(), shifted.begin() + silenceSamples);
    m_samples.swap(shifted);
    m_position = 0;
}

void Ft8Transmitter::buildMessageWaveform(const QString &message, double frequencyHz)
{
    if (m_modeName == QStringLiteral("FT4")) {
        buildFt4MessageWaveform(message, frequencyHz);
    } else {
        buildFt8MessageWaveform(message, frequencyHz);
    }
}

void Ft8Transmitter::buildFt8MessageWaveform(const QString &message, double frequencyHz)
{
    m_samples.clear();
    m_position = 0;
    m_ok = false;
    m_error.clear();

    if (message.trimmed().isEmpty()) {
        m_error = QStringLiteral("Empty FT8 message.");
        return;
    }

    std::unique_ptr<int[]> iwave(new int[kMaxMshvSamples]);
    std::fill(iwave.get(), iwave.get() + kMaxMshvSamples, 0);

    GenFt8 generator(false); // f_dec_gen=false: generator-side hash tables.

    const QString neutralOptions = QStringLiteral("0#0#0#0#0#0#0");
    const QString neutralOtp = QStringLiteral("0#0");

    const int generated = generator.genft8(message,
                                           iwave.get(),
                                           static_cast<double>(m_sampleRate),
                                           frequencyHz,
                                           neutralOptions,
                                           neutralOtp);

    if (generated <= 0 || generated > kMaxMshvSamples) {
        m_error = QStringLiteral("MSHV FT8 generator returned an invalid sample count.");
        return;
    }

    m_unpackedMessage = cleanFtMessage(generator.GetUnpackMsg());
    if (m_unpackedMessage.isEmpty()) {
        m_unpackedMessage = message;
    }

    /*
     * MSHV's generator appends several seconds of zero samples after the
     * actual FT8 frame.  That is harmless in a standalone player, but fatal in
     * a slot-timed modem: if we keep those zeros, PTT/audio remain active up
     * to (or beyond) the next opposite period and we miss the start of the
     * correspondent's reply.
     *
     * Trim the generated FT8 buffer to the last non-silent sample plus a very
     * small guard.  A normal FT8 frame is about 12.64 s, so with the WSJT-X-like
     * +500 ms target start it ends around slot+13.14 s, leaving roughly 1.8 s
     * for PTT-off and RX restart before the next 15 s period.
     */
    int lastActive = -1;
    constexpr int kSilenceThreshold = 8;
    for (int i = qMin(generated, kMaxMshvSamples) - 1; i >= 0; --i) {
        if (qAbs(iwave[i]) > kSilenceThreshold) {
            lastActive = i;
            break;
        }
    }

    const int guardSamples = qMax(1, m_sampleRate / 50); // 20 ms quiet guard.
    const int protocolCapSamples = qRound(12.70 * static_cast<double>(m_sampleRate));
    const int trimmedSamples = (lastActive >= 0) ? (lastActive + 1 + guardSamples) : generated;
    const int usableSamples = qBound(1,
                                     qMin(trimmedSamples, protocolCapSamples),
                                     qMin(generated, kMaxMshvSamples));

    m_samples.resize(usableSamples);
    for (int i = 0; i < usableSamples; ++i) {
        const double scaled = 0.62 * static_cast<double>(iwave[i]) / kMshvFullScale;
        m_samples[i] = static_cast<float>(qBound(-0.95, scaled, 0.95));
    }

    m_ok = !m_samples.isEmpty();
}

void Ft8Transmitter::buildFt4MessageWaveform(const QString &message, double frequencyHz)
{
    m_samples.clear();
    m_position = 0;
    m_ok = false;
    m_error.clear();

    if (message.trimmed().isEmpty()) {
        m_error = QStringLiteral("Empty FT4 message.");
        return;
    }

    std::unique_ptr<int[]> iwave(new int[kMaxMshvSamples]);
    std::fill(iwave.get(), iwave.get() + kMaxMshvSamples, 0);

    // v2.21: use the real MSHV FT4 TX generator instead of MM's local
    // reimplementation.  This keeps FT4 packing, 174/91 encoding, tone mapping,
    // GFSK pulse shaping, ramping and multi-message slot handling aligned with
    // the MSHV source tree just as FT8 already uses GenFt8.
    GenFt4 generator(false); // f_dec_gen=false: generator-side hash tables.
    const int generated = generator.genft4(message,
                                           iwave.get(),
                                           static_cast<double>(m_sampleRate),
                                           frequencyHz);

    if (generated <= 0 || generated > kMaxMshvSamples) {
        m_error = QStringLiteral("MSHV FT4 generator returned an invalid sample count.");
        return;
    }

    m_unpackedMessage = cleanFtMessage(generator.GetUnpackMsg());
    if (m_unpackedMessage.isEmpty()) {
        m_unpackedMessage = message;
    }

    int lastActive = -1;
    constexpr int kSilenceThreshold = 8;
    for (int i = qMin(generated, kMaxMshvSamples) - 1; i >= 0; --i) {
        if (qAbs(iwave[i]) > kSilenceThreshold) {
            lastActive = i;
            break;
        }
    }

    const int guardSamples = qMax(1, m_sampleRate / 50); // 20 ms quiet guard.
    const int protocolCapSamples = qRound(5.15 * static_cast<double>(m_sampleRate));
    const int trimmedSamples = (lastActive >= 0) ? (lastActive + 1 + guardSamples) : generated;
    const int usableSamples = qBound(1,
                                     qMin(trimmedSamples, protocolCapSamples),
                                     qMin(generated, kMaxMshvSamples));

    m_samples.resize(usableSamples);
    for (int i = 0; i < usableSamples; ++i) {
        const double scaled = 0.62 * static_cast<double>(iwave[i]) / kMshvFullScale;
        m_samples[i] = static_cast<float>(qBound(-0.95, scaled, 0.95));
    }

    m_ok = !m_samples.isEmpty();
}

void Ft8Transmitter::buildTuneWaveform(double frequencyHz, double durationSeconds)
{
    m_samples.clear();
    m_position = 0;
    m_ok = false;
    m_error.clear();

    const int totalSamples = qBound(m_sampleRate,
                                    static_cast<int>(qRound(durationSeconds * static_cast<double>(m_sampleRate))),
                                    60 * m_sampleRate);

    m_samples.resize(totalSamples);

    double phase = 0.0;
    const double step = kTwoPi * frequencyHz / static_cast<double>(m_sampleRate);
    const int rampSamples = qMax(1, m_sampleRate / 50); // 20 ms soft edge.

    for (int i = 0; i < totalSamples; ++i) {
        double env = 1.0;
        if (i < rampSamples) {
            env = 0.5 - 0.5 * qCos(kTwoPi * static_cast<double>(i) / static_cast<double>(2 * rampSamples));
        } else if (i > totalSamples - rampSamples) {
            const int k = totalSamples - i;
            env = 0.5 - 0.5 * qCos(kTwoPi * static_cast<double>(k) / static_cast<double>(2 * rampSamples));
        }

        m_samples[i] = static_cast<float>(0.55 * env * qSin(phase));
        phase += step;
        if (phase >= kTwoPi) {
            phase = std::fmod(phase, kTwoPi);
        }
    }

    m_ok = !m_samples.isEmpty();
}

QImage Ft8Transmitter::makePreviewImage() const
{
    QImage image(640, 96, QImage::Format_RGB32);
    image.fill(QColor(15, 15, 15));

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(m_modeName == QStringLiteral("FT4") ? QColor(80, 210, 255) : QColor(0, 220, 80));
    painter.drawRect(image.rect().adjusted(1, 1, -2, -2));
    painter.setPen(Qt::white);
    painter.drawText(QRect(12, 10, image.width() - 24, 24),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     m_tuneMode ? m_modeName + QStringLiteral(" TUNE") : m_modeName + QStringLiteral(" TX"));
    painter.setPen(QColor(255, 230, 80));
    painter.drawText(QRect(12, 40, image.width() - 24, 28),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     m_tuneMode ? QStringLiteral("Tone %1 Hz").arg(m_frequencyHz, 0, 'f', 0)
                                : normalizedMessage());
    painter.setPen(QColor(180, 180, 180));
    painter.drawText(QRect(12, 68, image.width() - 24, 20),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     m_modeName == QStringLiteral("FT4")
                         ? QStringLiteral("MSHV native FT4 TX path, 48000 Hz")
                         : QStringLiteral("MSHV-derived FT8 TX path, 48000 Hz"));
    painter.end();

    return image;
}
