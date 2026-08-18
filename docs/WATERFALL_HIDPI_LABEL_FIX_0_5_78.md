# Waterfall HiDPI and label rendering fix — 0.5.78

The OpenGL circular-texture renderer works in physical framebuffer pixels,
while QWidget geometry and QPainter overlays use logical coordinates.

This checkpoint applies the monitor-specific `devicePixelRatioF()` on every
OpenGL frame, including fractional scale factors, and keeps Qt high-DPI support
enabled in `main.cpp` for Qt 5 (Qt 6 enables it by default).

The label ghosting visible in the failing screenshot was not a font-size issue
alone. The new GPU path used a preserved (`PartialUpdate`) framebuffer. When the
GL viewport covered only part of the physical FBO, old QPainter text remained in
the untouched area and accumulated over later frames, producing fuzzy/ghosted
FT callouts.

Corrections:

- use a physical-pixel OpenGL viewport based on `devicePixelRatioF()`;
- use `QOpenGLWidget::NoPartialUpdate`, because every frame is reconstructed;
- disable any stale OpenGL scissor test before the full clear and quad draw;
- restore a full RGBA color mask before clearing;
- explicitly enable QPainter text antialiasing for waterfall labels;
- retain pixel-aligned non-antialiased grid lines and marker geometry.

The FT decoder, resource controller, audio path, sequencer and DSP are unchanged.
