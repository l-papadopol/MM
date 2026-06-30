#ifndef WATERFALLWIDGET_H
#define WATERFALLWIDGET_H

#include "../dsp/FrequencyMarker.h"

#include <QColor>
#include <QHash>
#include <QImage>
#include <QOpenGLWidget>
#include <QMouseEvent>
#include <QRgb>
#include <QString>
#include <QTimer>
#include <QVector>

/**
 * @brief Short text annotation drawn directly over the waterfall.
 *
 * These overlays are intentionally separate from the permanent frequency
 * markers.  FT8 uses them to show recently decoded CQ calls and direct replies
 * at the decoded audio frequency without moving the operator's RX/TX markers.
 */
struct WaterfallTextOverlay
{
    double frequencyHz = 0.0;
    QString label;
    QColor textColor = QColor(255, 235, 80);
    QColor backgroundColor = QColor(0, 0, 0, 185);

    /**
     * When true the label is not drawn as a static callout.  Instead only new
     * characters are appended to a time-locked vertical trail beside the signal.
     * The trail then moves upward with the waterfall until it naturally leaves
     * the top edge.  This is intended for live CW skimmer text.
     */
    bool verticalTrail = false;

    /**
     * Stable stream key used to detect newly appended characters.  If empty, a
     * key is derived from the frequency bucket.
     */
    QString streamId;
};

/**
 * @brief Displays a scrolling audio waterfall.
 *
 * Purpose:
 * - Render FFT intensity lines as a time/frequency waterfall.
 * - Keep frequency on the horizontal axis.
 * - Keep time scrolling downward.
 * - Display mode-provided frequency markers.
 *
 * Performance note:
 * - The widget stores waterfall pixels in a CPU-side image, but presents it
 *   through QOpenGLWidget so final compositing/scaling is GPU-backed where
 *   the platform supports OpenGL.
 * - The DSP engine now limits the diagnostic spectrum to 3 kHz so this view
 *   stays responsive during fast WAV analysis.
 */
class WaterfallWidget : public QOpenGLWidget
{
    Q_OBJECT

public:
    enum class ScrollDirection
    {
        Down,
        Right
    };

public:
    /**
     * @brief Creates the waterfall widget.
     */
    explicit WaterfallWidget(QWidget *parent = nullptr);

signals:
    /**
     * @brief Emits the audio frequency selected by clicking the waterfall.
     */
    void frequencyClicked(double frequencyHz, Qt::MouseButton button);

public slots:
    /**
     * @brief Adds one new FFT intensity line.
     */
    void addLine(const QVector<quint8> &line, double minHz, double maxHz);

    /**
     * @brief Clears the waterfall image.
     */
    void clear();

    /**
     * @brief Sets the frequency markers supplied by the selected modem.
     */
    void setMarkers(const QVector<FrequencyMarker> &markers);

    /**
     * @brief Sets transient decode/callsign labels drawn over the waterfall.
     */
    void setTextOverlays(const QVector<WaterfallTextOverlay> &overlays);

    /**
     * @brief Sets the display gain for waterfall intensity-to-color mapping.
     *
     * Values below 100 make a hot/yellow noise floor become green/blue.
     * The DSP data are unchanged.
     */
    void setColorScalePercent(int percent);

    /**
     * @brief Selects the intensity palette: madmodem/wsjtx, mshv, fldigi, raptor, or grayscale.
     */
    void setPaletteName(const QString &name);

    /**
     * @brief Selects vertical-time/downward or horizontal-time scrolling.
     */
    void setScrollDirection(ScrollDirection direction);

protected:
    /**
     * @brief Initializes the OpenGL-backed widget surface.
     */
    void initializeGL() override;

    /**
     * @brief Converts a mouse click into an audio-frequency tuning request.
     */
    void mousePressEvent(QMouseEvent *event) override;

    /**
     * @brief Paints the waterfall, scale, and markers on the GL surface.
     */
    void paintGL() override;

    /**
     * @brief Recreates the internal image when the widget is resized.
     */
    void resizeGL(int width, int height) override;

private:
    void buildColorTable();
    QRgb colorForIntensity(quint8 value) const;
    void ensureImage();

    int frequencyToX(double frequencyHz) const;
    int frequencyToY(double frequencyHz) const;
    double xToFrequency(int x) const;
    double yToFrequency(int y) const;

    void drawFrequencyScale(QPainter &painter);
    void drawMarkers(QPainter &painter);
    void drawTextOverlays(QPainter &painter);
    void drawVerticalTextTrails(QPainter &painter);
    void ageVerticalTextTrails();
    void appendVerticalTextTrail(const WaterfallTextOverlay &overlay);
    QString newOverlaySuffix(const QString &key, const QString &currentLabel);
    int bottomScaleBandHeight() const;
    int rightScaleBandWidth() const;
    void requestRepaint();

private:
    QImage m_image;
    QVector<QRgb> m_colorTable;
    int m_colorScalePercent = 100;
    QString m_paletteName = QStringLiteral("madmodem");
    ScrollDirection m_scrollDirection = ScrollDirection::Down;

    double m_minHz = 100.0;
    double m_maxHz = 3000.0;

    struct VerticalTextGlyph
    {
        double frequencyHz = 0.0;
        QString text;
        QColor textColor = QColor(255, 235, 80);
        QColor backgroundColor = QColor(0, 0, 0, 185);
        int ageRows = 0;
        int sequenceIndex = 0;
    };

    QVector<FrequencyMarker> m_markers;
    QVector<WaterfallTextOverlay> m_textOverlays;
    QVector<VerticalTextGlyph> m_verticalTextGlyphs;
    QHash<QString, QString> m_verticalTrailLastLabelByStream;
    QTimer m_repaintTimer;
    bool m_repaintQueued = false;
};

#endif // WATERFALLWIDGET_H
