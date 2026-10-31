#pragma once
#include "../dsp/text/SkimmerSpot.h"
#include <QHash>
#include <QRect>
class QPainter;

class SkimmerOverlay {
public:
    void setSpots(const QVector<SkimmerSpot>& spots, bool cqOnly);
    void clear();
    void draw(QPainter& painter, const QRect& plot, double minHz, double maxHz,
              const QVector<double>& markerFrequencies);
private:
    QVector<SkimmerSpot> m_spots;
    QHash<quint64, int> m_columns;
    bool m_cqOnly = false;
    int m_width = 0;
    double m_minHz = 0, m_maxHz = 0;
};
