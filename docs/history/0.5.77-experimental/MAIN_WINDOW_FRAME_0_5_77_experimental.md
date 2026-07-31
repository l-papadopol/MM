# MadModem 0.5.77.experimental - Main window triple frame

This experimental UI pass extends the Logbook-style cockpit frame to the main
window.

Changes:
- the main `QMainWindow[cockpitMainWindow="true"]` border now uses the same
  stronger amber/dark double bezel styling as the Logbook window;
- the transparent cockpit bezel overlay now draws three nested rounded border
  lines instead of a single rim;
- the overlay remains mouse-transparent, so menus, title-bar buttons, waterfall
  and all controls keep normal interaction;
- no decoder, Radio Telescope, FT4 or MIND logic is changed.
