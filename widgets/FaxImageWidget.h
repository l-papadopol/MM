#ifndef FAXIMAGEWIDGET_H
#define FAXIMAGEWIDGET_H

#include <QImage>
#include <QPoint>
#include <QWidget>

/**
 * @brief Displays the progressively decoded fax/SSTV image with zoom and pan.
 *
 * Purpose:
 * - Show the current decoded image.
 * - Keep the image aspect ratio in fit mode.
 * - Allow wheel zoom for detailed WEFAX inspection.
 * - Allow mouse panning while zoomed.
 * - Provide a reusable image display area for WEFAX and SSTV.
 */
class FaxImageWidget : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Creates the image display widget.
     */
    explicit FaxImageWidget(QWidget *parent = nullptr);

    /**
     * @brief Returns true when the widget is in fit-to-window mode.
     */
    bool fitToWindow() const;

    /**
     * @brief Returns the current fixed zoom percentage.
     */
    int zoomPercent() const;

public slots:
    /**
     * @brief Updates the displayed image.
     */
    void setImage(const QImage &image);

    /**
     * @brief Clears the displayed image.
     */
    void clear();

    /**
     * @brief Enables fit-to-window display mode.
     */
    void setFitToWindow();

    /**
     * @brief Sets the image to 100% pixel zoom.
     */
    void setActualSize();

    /**
     * @brief Increases image zoom.
     */
    void zoomIn();

    /**
     * @brief Decreases image zoom.
     */
    void zoomOut();

    /**
     * @brief Sets a fixed image zoom percentage.
     */
    void setZoomPercent(int percent);

    /**
     * @brief Enables or updates the TX progress overlay.
     */
    void setTransmitProgress(double progress);

    /**
     * @brief Disables the TX progress overlay.
     */
    void clearTransmitProgress();

signals:
    /**
     * @brief Emits when zoom mode or zoom value changes.
     */
    void zoomChanged(int percent, bool fitMode);

protected:
    /**
     * @brief Paints the image area.
     */
    void paintEvent(QPaintEvent *event) override;

    /**
     * @brief Handles mouse-wheel zoom.
     */
    void wheelEvent(QWheelEvent *event) override;

    /**
     * @brief Starts panning when zoomed.
     */
    void mousePressEvent(QMouseEvent *event) override;

    /**
     * @brief Updates the pan offset while dragging.
     */
    void mouseMoveEvent(QMouseEvent *event) override;

    /**
     * @brief Ends panning.
     */
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    /**
     * @brief Returns the current target rectangle for painting the image.
     */
    QRect imageTargetRect() const;

    /**
     * @brief Clamps pan offset so the image cannot disappear completely.
     */
    void clampPanOffset();

private:
    QImage m_image;
    bool m_fitToWindow = true;
    int m_zoomPercent = 100;
    QPoint m_panOffset;
    QPoint m_lastMousePos;
    bool m_panning = false;
    bool m_txProgressVisible = false;
    double m_txProgress = 0.0;
};

#endif // FAXIMAGEWIDGET_H
