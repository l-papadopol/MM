# macOS / Qt 6 MSHV portability fix — 0.5.78

GitHub Actions failed in `scripts/check_macos_portability.sh` because the active MSHV MSK40/MSK144 source set still contained Qt 5-only API names.

Corrections:

- `QRegExp` → `QRegularExpression` in the dormant callsign-validation block retained in both the port and upstream 2766 MSK40 sources.
- `QString::midRef()` → `QString::mid()` in active MSK40 and decoder message-formatting code.
- Both `third_party/mshv_gpl/port/` and the CI-checked `third_party/mshv_gpl/upstream_2766/` mirror were synchronized.

The replacement preserves substring and regular-expression behavior while compiling with Qt 5 and Qt 6.
