#ifndef FREQUENCYMARKER_H
#define FREQUENCYMARKER_H

#include <QColor>
#include <QString>
#include <QMetaType>

/**
 * @brief Describes one important frequency marker for the waterfall.
 *
 * Purpose:
 * - Mark mode-specific tones.
 * - Keep marker data outside the waterfall drawing code.
 * - Allow each modem/demodulator to expose its own visual hints.
 */
struct FrequencyMarker
{
    double frequencyHz = 0.0;
    QString label;
    QColor color = QColor(220, 0, 0);
    bool dashed = false;
    int width = 2;
};

Q_DECLARE_METATYPE(FrequencyMarker)

#endif // FREQUENCYMARKER_H
