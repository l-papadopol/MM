#ifndef WATERFALLWIDGET_H
#define WATERFALLWIDGET_H

#include "../dsp/FrequencyMarker.h"

#include <QByteArray>
#include <QColor>
#include <QHash>
#include <QImage>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QElapsedTimer>
#include <QQueue>
#include <memory>
#include <QPoint>
#include <QScrollBar>
#include <QMouseEvent>
#include <QRgb>
#include <QString>
#include <QTimer>
#include <QWheelEvent>
#include <QVector>

/**
 * @brief Short text annotation drawn directly over the waterfall.
 *
 * These overlays are intentionally separate from the permanent frequency
 * markers. FT8 uses static callouts; live text modes can use a vertical trail
 * whose characters remain aligned with their audio-frequency stream.
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
     * the top edge. This is intended for live text such as RTTY between its
     * Mark and Space markers.
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
 * - Downward scrolling uses a persistent circular OpenGL texture and uploads
 *   only the newest FFT row. A QImage path remains for compatibility fallback
 *   and rightward scrolling.
 * - The DSP engine limits the diagnostic spectrum to 3 kHz.
 */
class WaterfallWidget : public QOpenGLWidget, protected QOpenGLFunctions
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
    ~WaterfallWidget() override;

signals:
    /**
     * @brief Emits the audio frequency selected by clicking the waterfall.
     */
    void frequencyClicked(double frequencyHz, Qt::MouseButton button);
    void runtimeDiagnostic(const QString &message);

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
    void clearTextOverlayStream(const QString &streamId);
    void clearVerticalTextTrails();

    /**
     * @brief Sets the display gain for waterfall intensity-to-color mapping.
     *
     * The saved default (80%) is unity gain.  The control follows the
     * exponential WSJT-X Wide Graph gain law; the DSP/decoder data are
     * unchanged.
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
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

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
    void ensureImage(int preferredFrequencyBins = -1);
    double visibleMinHz() const;
    double visibleMaxHz() const;
    void resetFrequencyZoom();
    void setVisibleRange(double minHz, double maxHz);
    void zoomAt(double anchorHz, double factor);
    void panFrequency(double deltaHz);
    void updateFrequencyScrollBar();
    QRectF waterfallSourceRect() const;

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
    void initializeGpuRenderer();
    void destroyGpuRenderer();
    void ensureGpuTexture();
    void clearGpuTexture();
    void uploadPendingGpuRows();
    QByteArray rgbaRowForLine(const QVector<quint8> &line) const;
    void drawGpuWaterfall();

private:
    QImage m_image;
    QVector<QRgb> m_colorTable;
    int m_colorScalePercent = 100;
    QString m_paletteName = QStringLiteral("madmodem");
    ScrollDirection m_scrollDirection = ScrollDirection::Down;

    double m_minHz = 100.0;
    double m_maxHz = 3000.0;
    double m_viewMinHz = 100.0;
    double m_viewMaxHz = 3000.0;
    bool m_viewInitialized = false;
    int m_frequencyBins = 0;
    QScrollBar *m_frequencyScrollBar = nullptr;
    bool m_updatingFrequencyScrollBar = false;
    bool m_frequencyPanning = false;
    QPoint m_panStartPos;
    double m_panStartMinHz = 100.0;
    double m_panStartMaxHz = 3000.0;

    struct VerticalTextGlyph
    {
        double frequencyHz = 0.0;
        QString text;
        QColor textColor = QColor(255, 235, 80);
        QColor backgroundColor = QColor(0, 0, 0, 185);
        QString streamId;
        int ageRows = 0;
        int sequenceIndex = 0;
    };

    QVector<FrequencyMarker> m_markers;
    QVector<WaterfallTextOverlay> m_textOverlays;
    QVector<VerticalTextGlyph> m_verticalTextGlyphs;
    QHash<QString, QString> m_verticalTrailLastLabelByStream;
    QTimer m_repaintTimer;
    bool m_repaintQueued = false;

    // Downward waterfalls use a persistent OpenGL circular texture. Only one
    // newly computed FFT row is uploaded; a shader applies the ring offset and
    // frequency zoom. The QImage path remains solely as a compatibility
    // fallback and for rightward scrolling.
    std::unique_ptr<QOpenGLShaderProgram> m_gpuProgram;
    QOpenGLBuffer m_gpuVertexBuffer {QOpenGLBuffer::VertexBuffer};
    QOpenGLVertexArrayObject m_gpuVertexArray;
    GLuint m_gpuTexture = 0;
    bool m_gpuReady = false;
    bool m_gpuFailed = false;
    bool m_gpuTextureNeedsRecreate = true;
    bool m_gpuClearPending = true;
    int m_gpuTextureWidth = 0;
    int m_gpuTextureHeight = 0;
    int m_gpuWriteRow = 0;
    QQueue<QByteArray> m_pendingGpuRows;
    int m_droppedGpuRows = 0;
    QElapsedTimer m_gpuDiagnosticClock;
    QElapsedTimer m_repaintLatencyClock;

    // A minimized/hidden top-level window cannot present QOpenGLWidget frames.
    // Do not queue seconds of display-only FFT rows and replay them at high speed
    // when the window is restored: that compresses time and produces the
    // characteristic comb/squiggle artefact.
    bool m_presentationSuspended = false;
};

#endif // WATERFALLWIDGET_H
