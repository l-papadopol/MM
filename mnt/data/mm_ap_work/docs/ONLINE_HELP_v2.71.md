# MM inline help notes - v2.71

MM now has a compact built-in help browser inspired by old desktop IDE help panels:

- Help > Help contents opens a two-pane help dialog.
- Help > What's This? or Shift+F1 enters context-help mode.
- Existing tooltips are also exposed as status tips and What's This text.

The current help content is intentionally embedded in the application, so it works without internet access and without shipping an external Qt Assistant collection. It can later be expanded into a real indexed manual if needed.

Scope of this patch:

- quick start;
- audio, CAT and PTT;
- waterfall and markers;
- text modes;
- CW/Morse;
- FT4/FT8;
- Evil mode/automation;
- logbook and map;
- troubleshooting.

No DSP, CAT/PTT, sequencer or decoder behavior is changed in this patch.
