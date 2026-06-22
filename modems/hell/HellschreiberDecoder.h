#ifndef HELLSCHREIBERDECODER_H
#define HELLSCHREIBERDECODER_H

#include "../../audio/AudioBlock.h"
#include "../../dsp/FrequencyMarker.h"

#include <QImage>
#include <QObject>
#include <QString>
#include <QVector>

/**
 * @brief Feld Hell / Hellschreiber visual receive decoder.
 *
 * Purpose:
 * - Track classic Feld Hell as keyed A1 audio.
 * - Track FSK-Hell 105 as a two-tone frequency-shift visual raster.
 * - Convert the received pixels into a virtual white paper tape.
 * - Print the result column by column, wrapping like a small paper printer.
 *
 * Design note:
 * - Hellschreiber is normally read by eye.  The receiver renders a visual
 *   raster; it intentionally does not OCR the image into ASCII text.
 */
class HellschreiberDecoder : public QObject
{
    Q_OBJECT

public:
    enum class Variant
    {
        FeldHell,
        Fsk105
    };

    /**
     * @brief Creates a decoder with Feld Hell defaults.
     */
    explicit HellschreiberDecoder(QObject *parent = nullptr);

    /**
     * @brief Returns the user-visible modem name.
     */
    static QString modeName();

    /**
     * @brief Returns the user-visible variant name.
     */
    static QString variantName(Variant variant);

    /**
     * @brief Parses a persisted variant key.
     */
    static Variant variantFromKey(const QString &key);

    /**
     * @brief Returns a stable persisted variant key.
     */
    static QString variantKey(Variant variant);

    /**
     * @brief Returns the classic FSK-105 mark/space separation used by fldigi-style FSK Hell.
     */
    static double fsk105ShiftHz();

    /**
     * @brief Returns waterfall markers for the selected Hellschreiber tone.
     */
    static QVector<FrequencyMarker> frequencyMarkers(double toneHz,
                                                     double bandwidthHz,
                                                     Variant variant = Variant::FeldHell,
                                                     double fskShiftHz = 55.0);

    /**
     * @brief Clears DSP state and the received paper image.
     */
    void reset();

    /**
     * @brief Processes one normalized audio block and appends raster pixels.
     */
    void processAudioBlock(const AudioBlock &block);

    /**
     * @brief Selects classic Feld Hell or FSK-105.
     */
    void setVariant(Variant variant);

    /**
     * @brief Sets the center carrier tone in Hz.
     */
    void setToneHz(double toneHz);

    /**
     * @brief Sets the Hellschreiber column rate in columns per second.
     */
    void setColumnRate(double columnRate);

    /**
     * @brief Sets the RX tone/filter bandwidth in Hz.
     */
    void setBandwidthHz(double bandwidthHz);

    /**
     * @brief Sets the visual vertical scale of the Hell paper.
     *
     * This affects only the displayed paper height.  Modem timing remains tied
     * to the 14 logical Hell rows.
     */
    void setVerticalScale(int scale);

    /**
     * @brief Returns the visual vertical scale of the Hell paper.
     */
    int verticalScale() const;

    /**
     * @brief Appends locally transmitted Hell raster pixels to the same paper tape.
     *
     * TX is shown as red ink on the same virtual paper used for RX.  This is
     * a display echo only; it does not change the generated audio waveform.
     */
    void appendTransmitRaster(const QImage &raster);

    /**
     * @brief Sets the FSK-105 mark/space separation in Hz.
     */
    void setFskShiftHz(double shiftHz);

    /**
     * @brief Returns the selected variant.
     */
    Variant variant() const;

    /**
     * @brief Returns the selected center carrier tone in Hz.
     */
    double toneHz() const;

    /**
     * @brief Returns the selected column rate in columns per second.
     */
    double columnRate() const;

    /**
     * @brief Returns the selected detector bandwidth in Hz.
     */
    double bandwidthHz() const;

    /**
     * @brief Returns the selected FSK shift in Hz.
     */
    double fskShiftHz() const;

    /**
     * @brief Returns the current virtual paper image.
     */
    QImage currentImage() const;

signals:
    void imageUpdated(const QImage &image);
    void statusChanged(const QString &status);
    void markersChanged(const QVector<FrequencyMarker> &markers);

private:
    /**
     * @brief Configures oscillator and filter constants for a sample rate.
     */
    void configureForSampleRate(int sampleRate);

    /**
     * @brief Processes one audio sample through the selected detector.
     */
    void processSample(double sample);

    /**
     * @brief Processes one sample as keyed-amplitude Feld Hell.
     */
    double processFeldHellSample(double sample);

    /**
     * @brief Processes one sample as two-tone FSK-105 Hell.
     */
    double processFsk105Sample(double sample);

    /**
     * @brief Updates one quadrature tone detector and returns its envelope.
     */
    double processToneDetector(double sample,
                               double phaseInc,
                               double &phase,
                               double &iState,
                               double &qState,
                               double &envelopeState);

    /**
     * @brief Updates slow signal/noise estimates and returns a confidence gate.
     */
    double updateSignalGate(double observedLevel);

    /**
     * @brief Emits one paper pixel when the raster clock advances.
     */
    void writePixel(double level);

    /**
     * @brief Advances to the next Hell raster column.
     */
    void finishColumn();

    /**
     * @brief Wraps to a new virtual paper row, scrolling when the page is full.
     */
    void finishPaperRow();

    /**
     * @brief Returns the display zoom applied to both X and Y paper pixels.
     */
    int paperZoom() const;

    /**
     * @brief Returns the displayed paper width.
     */
    int paperWidth() const;

    /**
     * @brief Returns the displayed paper height.
     */
    int paperHeight() const;

    /**
     * @brief Returns the displayed paper Y coordinate for one logical raster row.
     */
    int displayYForLogicalRow(int paperRow, int logicalRow) const;

    /**
     * @brief Returns the top Y coordinate of the current unscaled paper row.
     */
    int currentLogicalPaperTop() const;

    /**
     * @brief Returns the unscaled paper height.
     */
    int logicalPaperHeight() const;

    /**
     * @brief Writes one pixel to the unscaled paper and the scaled display image.
     */
    void setPaperPixel(int column, int paperRow, int logicalRow, const QColor &color);

    /**
     * @brief Rebuilds the scaled display image from the unscaled paper image.
     */
    void rebuildDisplayImageFromLogical();

    /**
     * @brief Redraws separator lines on the unscaled paper background.
     */
    void drawLogicalSeparators();

    /**
     * @brief Clears and redraws the virtual paper background.
     */
    void resetPaperImage();

    /**
     * @brief Emits a periodic receive status string.
     */
    void maybeEmitStatus();

    /**
     * @brief Emits markers for current settings.
     */
    void emitCurrentMarkers();

private:
    /*
     * Hellschreiber timing stays tied to logical raster rows.  The decoder
     * keeps an unscaled paper image as the source of truth, then renders it
     * using the selected paper zoom on both axes.  Changing zoom must never
     * clear or re-time already received pixels.
     */
    static constexpr int kLogicalRasterHeight = 14;
    static constexpr int kLineGap = 4;
    static constexpr int kPaperWidth = 1120;
    static constexpr int kVisibleRows = 6;

    int m_verticalScale = 4;

    Variant m_variant = Variant::FeldHell;
    double m_toneHz = 1000.0;
    double m_columnRate = 17.5;
    double m_bandwidthHz = 245.0;
    double m_fskShiftHz = 55.0;

    int m_sampleRate = 0;
    double m_phase = 0.0;
    double m_phaseInc = 0.0;
    double m_i = 0.0;
    double m_q = 0.0;
    double m_lpAlpha = 0.010;
    double m_fskLpAlpha = 0.006;
    double m_pixelPhase = 0.0;
    double m_pixelsPerSample = 0.0;

    double m_envelope = 0.0;
    double m_noise = 0.004;
    double m_peak = 0.040;

    double m_lowPhase = 0.0;
    double m_highPhase = 0.0;
    double m_lowPhaseInc = 0.0;
    double m_highPhaseInc = 0.0;
    double m_lowI = 0.0;
    double m_lowQ = 0.0;
    double m_highI = 0.0;
    double m_highQ = 0.0;
    double m_lowEnvelope = 0.0;
    double m_highEnvelope = 0.0;

    QImage m_logicalImage;
    QImage m_image;
    int m_column = 0;
    int m_row = 0;
    int m_paperRow = 0;
    int m_columnsWritten = 0;
    int m_paperRowsWritten = 0;
    qint64 m_samplesProcessed = 0;
    int m_statusCounter = 0;
};

#endif // HELLSCHREIBERDECODER_H
