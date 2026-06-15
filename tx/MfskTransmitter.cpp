#include "MfskTransmitter.h"

#include <QColor>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QTextOption>
#include <QtMath>

namespace {
constexpr double kTwoPi = 6.283185307179586476925286766559;
}

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
        const double envelope = qSin(M_PI * frac);
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
    painter.drawText(QRect(24, 24, image.width() - 48, image.height() - 48),
                     text.isEmpty()
                         ? QStringLiteral("%1 TX text is empty").arg(MfskDecoder::variantName(variant))
                         : text,
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

    const int idleTone = 0;
    for (int i = 0; i < 16; ++i) {
        m_tones.append(idleTone);
    }

    QString normalized = text;
    normalized.replace("\r\n", "\n");
    normalized.replace('\r', '\n');
    for (QChar ch : normalized) {
        appendCharacter(ch);
    }

    for (int i = 0; i < 8; ++i) {
        m_tones.append(idleTone);
    }
}

void MfskTransmitter::appendCharacter(QChar ch)
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

double MfskTransmitter::frequencyForTone(int toneIndex) const
{
    return firstToneHz() + toneSpacingHz() * static_cast<double>(toneIndex);
}
