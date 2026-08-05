#ifndef RTTYSCOPEWIDGET_H
#define RTTYSCOPEWIDGET_H

#include <QPointF>
#include <QTimer>
#include <QVector>
#include <QWidget>

/**
 * @brief Round CRT-style RTTY crossed-ellipse tuning indicator.
 *
 * The decoder supplies oscilloscope-like X/Y deflection samples derived from
 * live Mark/Space tone energy.  The widget only simulates the CRT face,
 * phosphor persistence and beam intensity; it does not invent a perfect trace
 * when RX is stopped or squelch is closed.
 */
class RttyScopeWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RttyScopeWidget(QWidget *parent = nullptr);

public slots:
    void setTuningMetrics(double markLevel,
                          double spaceLevel,
                          double snrLike,
                          bool locked);
    void setTrace(const QVector<QPointF> &tracePoints,
                  double snrLike,
                  bool locked);

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void advanceAnimation();

private:
    double m_markLevel = 0.0;
    double m_spaceLevel = 0.0;
    double m_snrLike = 0.0;
    bool m_locked = false;
    QVector<QPointF> m_tracePoints;
    int m_framesSinceTraceUpdate = 1000;
    QTimer m_animTimer;
};

#endif // RTTYSCOPEWIDGET_H
