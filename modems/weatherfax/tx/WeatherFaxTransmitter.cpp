#include "WeatherFaxTransmitter.h"

#include <QColor>
#include <QPainter>
#include <QtMath>

#include <cmath>

namespace {

constexpr double kTwoPi = 6.28318530717958647692;
constexpr int kDefaultWidth = 800;

int grayAt(const QImage &image, int x, int y)
{
    const QRgb pixel = image.pixel(qBound(0, x, image.width() - 1),
                                   qBound(0, y, image.height() - 1));
    return qGray(pixel);
}

} // namespace

// -----------------------------------------------------------------------------
// Construction
// -----------------------------------------------------------------------------

WeatherFaxTransmitter::WeatherFaxTransmitter(const QImage &sourceImage,
                                             int sampleRate,
                                             int lpm,
                                             double blackHz,
                                             double whiteHz,
                                             int targetWidth)
    : m_image(prepareImage(sourceImage, targetWidth)),
      m_sampleRate(qMax(8000, sampleRate)),
      m_lpm(qMax(1, lpm)),
      m_blackHz(blackHz),
      m_whiteHz(whiteHz)
{
    if (m_image.isNull()) {
        m_image = QImage(kDefaultWidth, 1, QImage::Format_Grayscale8);
        m_image.fill(255);
    }

    if (m_whiteHz <= m_blackHz + 10.0) {
        m_blackHz = 1500.0;
        m_whiteHz = 2300.0;
    }

    m_centerHz = 0.5 * (m_blackHz + m_whiteHz);
    m_lineSamples = (static_cast<double>(m_sampleRate) * 60.0) /
                    static_cast<double>(m_lpm);

    m_leadSamples = static_cast<qint64>(qRound(2.0 * static_cast<double>(m_sampleRate)));
    m_phasingSamples = static_cast<qint64>(qRound(4.0 * m_lineSamples));
    m_imageSamples = static_cast<qint64>(qCeil(m_lineSamples * static_cast<double>(m_image.height())));
    m_tailSamples = static_cast<qint64>(qRound(1.5 * static_cast<double>(m_sampleRate)));
    m_totalSamples = m_leadSamples + m_phasingSamples + m_imageSamples + m_tailSamples;
}

// -----------------------------------------------------------------------------
// Static helpers
// -----------------------------------------------------------------------------

QImage WeatherFaxTransmitter::prepareImage(const QImage &sourceImage, int targetWidth)
{
    if (sourceImage.isNull()) {
        return QImage();
    }

    const int width = qBound(160, targetWidth, 1600);
    const double aspect = static_cast<double>(sourceImage.height()) /
                          qMax(1.0, static_cast<double>(sourceImage.width()));
    const int height = qBound(1, static_cast<int>(qRound(aspect * static_cast<double>(width))), 12000);

    QImage canvas(width, height, QImage::Format_RGB32);
    canvas.fill(Qt::white);

    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QSize fitted = sourceImage.size().scaled(canvas.size(), Qt::KeepAspectRatio);
    const QRect target((width - fitted.width()) / 2,
                       (height - fitted.height()) / 2,
                       fitted.width(),
                       fitted.height());

    painter.drawImage(target, sourceImage.convertToFormat(QImage::Format_RGB32));
    painter.end();

    return canvas.convertToFormat(QImage::Format_Grayscale8);
}

// -----------------------------------------------------------------------------
// TxModulator API
// -----------------------------------------------------------------------------

int WeatherFaxTransmitter::sampleRate() const
{
    return m_sampleRate;
}

int WeatherFaxTransmitter::generate(float *output, int sampleCount)
{
    if (output == nullptr || sampleCount <= 0 || isFinished()) {
        return 0;
    }

    int generated = 0;

    while (generated < sampleCount && m_position < m_totalSamples) {
        output[generated] = nextSample(frequencyAt(m_position));
        ++m_position;
        ++generated;
    }

    return generated;
}

bool WeatherFaxTransmitter::isFinished() const
{
    return m_position >= m_totalSamples;
}

double WeatherFaxTransmitter::progress() const
{
    if (m_imageSamples <= 0) {
        return isFinished() ? 1.0 : 0.0;
    }

    if (m_position <= m_leadSamples + m_phasingSamples) {
        return 0.0;
    }

    const qint64 imagePos = qBound<qint64>(0,
                                           m_position - m_leadSamples - m_phasingSamples,
                                           m_imageSamples);
    return qBound(0.0, static_cast<double>(imagePos) / static_cast<double>(m_imageSamples), 1.0);
}

QImage WeatherFaxTransmitter::previewImage() const
{
    return m_image;
}

QString WeatherFaxTransmitter::description() const
{
    return QString("WEFAX TX %1 LPM, %2x%3, black %4 Hz, white %5 Hz")
        .arg(m_lpm)
        .arg(m_image.width())
        .arg(m_image.height())
        .arg(m_blackHz, 0, 'f', 0)
        .arg(m_whiteHz, 0, 'f', 0);
}

// -----------------------------------------------------------------------------
// Modulation helpers
// -----------------------------------------------------------------------------

double WeatherFaxTransmitter::frequencyAt(qint64 sampleIndex) const
{
    if (sampleIndex < m_leadSamples) {
        return m_centerHz;
    }

    qint64 pos = sampleIndex - m_leadSamples;

    if (pos < m_phasingSamples) {
        const double linePos = std::fmod(static_cast<double>(pos), m_lineSamples) /
                               qMax(1.0, m_lineSamples);
        return (linePos < 0.08) ? m_blackHz : m_whiteHz;
    }

    pos -= m_phasingSamples;

    if (pos < m_imageSamples) {
        const int y = qBound(0,
                             static_cast<int>(qFloor(static_cast<double>(pos) / m_lineSamples)),
                             m_image.height() - 1);
        const double insideLine = std::fmod(static_cast<double>(pos), m_lineSamples) /
                                  qMax(1.0, m_lineSamples);
        const int x = qBound(0,
                             static_cast<int>(qFloor(insideLine * static_cast<double>(m_image.width()))),
                             m_image.width() - 1);
        return pixelFrequency(x, y);
    }

    return m_centerHz;
}

double WeatherFaxTransmitter::pixelFrequency(int x, int y) const
{
    const int gray = grayAt(m_image, x, y);
    const double t = static_cast<double>(gray) / 255.0;
    return m_blackHz + t * (m_whiteHz - m_blackHz);
}

float WeatherFaxTransmitter::nextSample(double frequencyHz)
{
    const double amplitude = 0.55;
    const float sample = static_cast<float>(amplitude * qSin(m_phase));

    m_phase += kTwoPi * frequencyHz / static_cast<double>(m_sampleRate);

    if (m_phase >= kTwoPi) {
        m_phase = std::fmod(m_phase, kTwoPi);
    }

    return sample;
}
