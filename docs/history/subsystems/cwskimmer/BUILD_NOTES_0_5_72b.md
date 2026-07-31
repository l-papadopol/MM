# MadModem 0.5.72b — CW skimmer waterfall OSD cleanup

## Purpose

This patch keeps the 0.5.71 fullscreen/cockpit UI and the 0.5.72a CW skimmer
engine, but fixes the waterfall presentation reported during live testing.

## Changes

- CW skimmer decode text no longer appears as a static horizontal callout that
  flashes/repositions near the bottom of the waterfall.
- CW decoded characters are now appended to a persistent vertical trail beside
  the decoded tone.  New glyphs are born near the current waterfall line and
  then move upward with the waterfall until they naturally leave the top edge.
- CW OSD uses stream hysteresis/delta tracking: repeated rolling text does not
  redraw the same label and therefore does not blink.
- Frequency labels at the bottom of the waterfall now have a dedicated protected
  scale band.  Grid and marker lines stop above the band, so labels are not cut
  or overwritten.  The same fix applies to all modes using WaterfallWidget.
- Right-scrolling waterfall scale labels also get a protected side band.

## UI model retained

- RX A is still the user-selected green marker.
- RX B is still the optional user-selected blue marker.
- The skimmer may decode additional streams for OSD, but it does not create
  permanent CW1/CW2/CW3 marker lines.
