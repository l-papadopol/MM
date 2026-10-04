#ifndef SSTVMODEDEFINITION_H
#define SSTVMODEDEFINITION_H

#include "../../dsp/FrequencyMarker.h"

#include <QString>
#include <QVector>

/**
 * @brief Provides metadata for the future SSTV decoder.
 *
 * Purpose:
 * - Keep SSTV-specific waterfall markers outside MainWindow.
 * - Allow the UI to show correct SSTV tone references before the decoder exists.
 */
class SstvModeDefinition
{
public:
    /**
     * @brief Returns the user-visible mode name.
     */
    static QString modeName();

    /**
     * @brief Returns the waterfall markers used by common analog SSTV modes.
     */
    static QVector<FrequencyMarker> frequencyMarkers();
};

#endif // SSTVMODEDEFINITION_H
