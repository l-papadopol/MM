#pragma once
#include <QString>
#include <QVector>

// One alphabet and timing plan for both audio CW and serial KEY.
struct CwMorseSegment {
    bool down = false;
    double units = 0;
    int textPosition = -1;
};
QVector<CwMorseSegment> cwMorseSequence(const QString& text);
