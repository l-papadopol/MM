# MadModem 0.5.77.experimental – Settings layout cleanup

This experimental UI cleanup focuses on Settings usability without changing decoder or radio logic.

## Rotator settings

- Stop point and stall-threshold editors are now compact fixed-width spin boxes instead of expanding across the whole row.
- The per-profile Auto peak search algorithm panel is placed in the same upper profile row, to the right of Connection and Settings, instead of consuming a full-width block below them.
- The common Band → Rotator table remains the shared band assignment point for all three rotators.

## User / QTH / Macros

- The old inner tabs are removed.
- User / station / QTH data and Macros are now visible on the same page using a horizontal split:
  - left side: station/QTH/coordinates/tokens;
  - right side: macro labels and macro text fields.

The package version remains `0.5.77.experimental` for the experimental GitHub release line.
