# cty.csv support

MadModem now loads the standard AD1C/K1EA `cty.csv` country file from the executable directory:

```text
<directory containing MadModem>/cty.csv
```

This is intentionally not compiled into the executable.  Users can update DXCC/prefix data by downloading a fresh `cty.csv` from country-files.com and overwriting the file next to the executable.

## What MM uses it for

- DXCC/entity inference from callsigns.
- Exact-call exceptions beginning with `=`.
- Longest-prefix matching for normal prefixes.
- Country/entity name.
- DXCC numeric identifier.
- Continent, CQ zone and ITU zone metadata.
- Reference latitude/longitude.
- Reference Maidenhead grid generated from the CTY coordinates.

## QSO map

The `Worked DXCC` map mode uses `cty.csv` so the map can group logbook records by real DXCC entity even when the ADIF record lacks a locator.  The marker is placed at the CTY reference coordinate for the entity when no QSO grid is available.

The existing ADIF-preserving backend is unchanged: CTY-derived fields are used for display/map inference and do not destroy or normalize the user's original ADIF records.
