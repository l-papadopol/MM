# 0.5.77.experimental cleanup — auto-tests, bundled media, MM Flow Studio

Removed from the production source/package:

- FT8/FT4/MSK144 bundled auto-test actions and result popup code.
- Bundled WAV/JPG regression media under `tests/wav`.
- Package/install script logic that copied `tests/wav` into Linux/Windows/macOS packages.
- MM Flow Studio visual editor, flow runtime/shadow logging, Settings tab and Help pages.

Kept:

- Manual FT WAV analysis from an operator-selected file.
- Built-in FT AutoQSO/sequencer logic.
- FT8/FT4/MSK144 decoder/runtime code.
