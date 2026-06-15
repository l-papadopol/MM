# MM Qt Help backend - v2.72

MM now uses Qt Help as the preferred online-help format.

Source files live in `docs/help/` as simple HTML + CSS. The Qt Help project is `docs/help/MM.qhp`. During CMake builds, if `qhelpgenerator` is available, CMake generates `MM.qch` into the build tree under `docs/help/`.

At runtime, the Help dialog looks for `MM.qch` in:

- `help/MM.qch` next to the executable,
- `docs/help/MM.qch` next to the executable/build tree,
- the current working directory help/docs locations.

If QtHelp or `MM.qch` is missing, the same HTML pages are loaded from `resources.qrc` as an embedded fallback, so Help remains usable.

The packaged `MM.zip` includes the HTML help folder and copies the generated `MM.qch` when present.
