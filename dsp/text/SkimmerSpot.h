#pragma once
#include <QMetaType>
#include <QString>
#include <QVector>

// Immutable, bounded worker-to-display snapshot; no decoder state crosses threads.
struct SkimmerSpot {
    quint64 id = 0;
    double frequencyHz = 0;
    QString text;
    QString cqCall;
    double lowerToneHz = 0;
    double upperToneHz = 0;
};
Q_DECLARE_METATYPE(QVector<SkimmerSpot>)
