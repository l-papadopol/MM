# QSO map behavior modes — v2.54

The QSO map now supports three operator-selectable behaviors.

## Logged QSOs

This is the previous behavior: the map shows ADIF logbook records filtered by mode, band, date scope and marker-reduction settings.

## Heard stations today

This mode does not use already-worked stations as the primary source. It shows stations decoded during the current UTC day when the decoded text contains a valid callsign and Maidenhead locator. This is intended as a propagation map: it answers “what am I hearing now/today?” rather than “what have I worked?”

Sources currently feeding the heard cache:

- FT4/FT8 standard decodes with a valid locator;
- RTTY text terminal CALL + LOCATOR patterns;
- BPSK text terminal CALL + LOCATOR patterns.

The cache is in-memory and current-UTC-day oriented; it is not written to the ADIF logbook.

## Worked DXCC countries

This mode groups the ADIF logbook by country/DXCC and plots one latest representative record per country when a locator is available. It is intended as a first DXCC coverage view. It deliberately does not modify ADIF storage and does not require drawing political polygons.

## Performance

Large ADIF logs are still reduced by the existing filters and maximum-marker limit. Heard-today mode is naturally small because it only contains stations decoded during the current run/day.
