# Settings open latency fix — 0.5.77.experimental

This experimental patch reduces the perceived delay when opening the unified Settings workbench.

## Problem

The Settings dialog was constructing all pages before becoming visible, including heavy pages such as:

- Rotator profile/setup page
- MM Flow Studio editor
- Scheduler editor
- Soundcard calibration page

On Windows this could add about 1–2 seconds before the Settings window appeared.

## Change

The everyday pages are still constructed immediately:

- User / QTH / Macros
- Audio / PTT / CAT
- Logbook / FT colours

Heavy pages are now lazy-loaded only when selected:

- Rotator
- MM Flow Studio
- Scheduler
- Soundcard calibration

If a deferred page is never opened, `collectSettings()` preserves the previous settings for that page and does not overwrite them with defaults.

## Diagnostics

The main log now reports:

```text
Settings opened in N ms (construction M ms; heavy pages lazy-loaded).
```

The existing close-side diagnostic remains:

```text
Settings updated in N ms.
```

## Rotator state

Rotator connection LED/status events received before the deferred Rotator tab is built are cached and applied when the Rotator tab is opened.
