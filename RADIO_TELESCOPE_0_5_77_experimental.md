# MadModem 0.5.77.experimental — Experimental Radio Telescope release

This source package is intended for a dedicated experimental GitHub release.

It keeps the production 0.5.77 release identity while enabling the experimental receive-only Radio Telescope work from the lab18-lab24 line.

Highlights:

- Radio Telescope receive-only mode for sky/RFI noise heatmaps.
- Rotator is required for real scans; bench mode remains explicit and labelled as non-sky-map testing.
- Audio noise is measured inside a configurable waterfall slice, defaulting to a 1000 Hz span.
- Step-and-stare scanning uses rotator timing/calibration data plus an additional settle time.
- Honeycomb heatmap rendering with transparent unmeasured tiles and measured heat colours.
- Side-panel sample table with CSV export.
- Antenna beam width is stored with the active rotator/antenna profile and drives scan tile spacing.
- Rotator auto-peak integration hooks are wired to the existing rotator peak-search profile logic.

Tag suggestion for GitHub Actions:

```text
v0.5.77.experimental
```

