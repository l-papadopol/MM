# QSO map and localization pass v2.48

## QSO map

The intermittent horizontal band in the map was caused by partial OSM tile loading: missing tile rows were being painted as blank grey placeholders while other rows already had raster tiles.  The Maidenhead overlay was then visually inconsistent depending on which tiles were present.

v2.48 paints a full deterministic base map first and overlays OSM raster tiles only where they are already cached.  Missing tiles still get requested, but they no longer paint opaque placeholder bands over the map.

## Localization

This pass also tightens runtime dictionaries for map, logbook, CAT/settings and common menu/dialog labels.  The dictionaries remain INI based and are still maintained by `tools/update_ui_translations.py`; further exact wording can be refined without changing code.
