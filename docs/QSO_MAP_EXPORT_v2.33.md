# QSO map export/print fix — v2.33

The previous QSO map export path reused the interactive map renderer directly on the printer/PDF device. That could produce poor graphics because the interactive renderer may request OSM tiles asynchronously during painting, draw placeholder tile rectangles, and stretch the map to the raw printer page rectangle.

v2.33 separates screen rendering from export rendering:

- PDF/print/image export now renders to a fixed high-resolution off-screen image.
- Export rendering is deterministic: it uses the bundled offline map or vector fallback, then overlays Maidenhead grid, HOME marker, QSO markers and paths.
- OSM tile network requests are disabled during export painting, so partial online tile loading cannot corrupt the exported map.
- The rendered image is centered on the PDF/printable page while preserving aspect ratio.

This keeps the interactive QSO map unchanged while making exported PDFs and printed maps visually consistent.
