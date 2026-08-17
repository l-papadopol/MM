#include "SstvModeDefinition.h"

// -----------------------------------------------------------------------------
// Static metadata
// -----------------------------------------------------------------------------

QString SstvModeDefinition::modeName()
{
    return "SSTV RX";
}

QVector<FrequencyMarker> SstvModeDefinition::frequencyMarkers()
{
    return {
        {1200.0, "SSTV sync", QColor(220, 0, 0)},
        {1500.0, "SSTV black", QColor(220, 0, 0)},
        {1900.0, "SSTV gray", QColor(220, 0, 0)},
        {2300.0, "SSTV white", QColor(220, 0, 0)}
    };
}
