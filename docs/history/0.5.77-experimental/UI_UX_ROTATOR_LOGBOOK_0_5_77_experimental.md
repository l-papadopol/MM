# MadModem 0.5.77.experimental — Logbook / Rotator UI cleanup

This experimental patch cleans up the cockpit UI without changing FT decode logic.

## Logbook

- The floating logbook dialog now has a stronger cockpit frame so it separates visually from the main window.
- The logbook table no longer falls back to a bright white spreadsheet style; it uses the dark/amber MadModem palette.
- The toolbar now uses icon + text buttons instead of icon-only buttons.
- Every toolbar action has a descriptive tooltip and status tip.

## Rotator settings

The old per-rotator band assignment lists have been replaced by one compact common table:

```text
Band | Rotator | Az ± | El ± | Auto peak
```

Each band appears once. The operator selects Manual, Rotator 1, Rotator 2 or Rotator 3 in the same row, with the peak-search span beside it. Internally this still writes the existing per-profile band settings, so older runtime code continues to read the same data model.

## FT table visual cleanup

- CQ rows use a stronger cockpit highlight by default.
- Non-CQ traffic between other stations is kept in a calmer grey/brown row.
- The red dashed new-DXCC outline is now also cleared when the option is disabled, not only when new rows are inserted.
