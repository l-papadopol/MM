# OpenGL waterfall HiDPI viewport fix — MadModem 0.5.78

## Observed failure

The circular OpenGL waterfall was visible only in the lower-left part of the waterfall widget, while frequency labels, markers and FT decode callouts continued across the full area. The supplied runtime log reported a healthy OpenGL backend, approximately 1 ms render time, no dropped rows and a 1024×210 texture. This excludes FFT-row loss and queue overload as the primary cause.

The screenshot is consistent with a device-pixel ratio of 2.0: the GL content covers about one half of the logical width and one half of the logical height.

## Root cause

`QOpenGLWidget::width()` and `height()` are logical-pixel dimensions. Its backing framebuffer is allocated in physical device pixels. The new circular renderer called:

```cpp
glViewport(0, 0, width(), height());
```

On a HiDPI display this configured a viewport smaller than the framebuffer. QPainter subsequently drew labels and overlays in the correct logical coordinate system, making the mismatch especially visible.

## Correction

Both OpenGL viewport paths now use:

```cpp
const qreal dpr = devicePixelRatioF();
glViewport(0, 0,
           qMax(1, qRound(width() * dpr)),
           qMax(1, qRound(height() * dpr)));
```

The texture remains 1024 bins wide and one logical widget-height deep. OpenGL scales it to the physical framebuffer; the ring order, row uploads and frequency mapping are unchanged.

## Preserved scope

- FT8/FT4 Costas, LDPC, subtraction and OSD logic unchanged;
- adaptive worker budgets and persistent coordinator unchanged;
- audio capture/timestamps unchanged;
- native CW and all other modem paths unchanged;
- CPU QImage fallback unchanged.

## Target validation

Build and run at desktop scale 100%, 125/150% where available, and 200%. In FT8 downward-scroll mode confirm that spectrum data fills the entire horizontal 100–3000 Hz region and the complete vertical history area. Zoom/pan, RX/TX markers and decode callouts must remain aligned. The runtime log should continue to show zero dropped rows under normal load.
