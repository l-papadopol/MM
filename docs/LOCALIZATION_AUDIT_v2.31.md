# Localization audit — v2.31

## Goal

Identify and reduce hardcoded visible text in MM after the FT v2.30 work, then refresh the multilingual runtime dictionaries.

## Languages

The runtime dictionaries currently cover:

- `en` — English
- `it` — Italian
- `fr` — French
- `de` — German
- `no` — Norwegian
- `cs` — Czech

All language files are kept with the same key set by `tools/update_ui_translations.py`.

## Runtime dictionary mechanism

MM does not currently rely on Qt `.ts/.qm` files. It uses lightweight INI dictionaries in `translations/ui_*.ini` loaded at runtime. The main translator paths are:

- explicit `uiText("key", "English fallback")` calls;
- source-text normalization through `uiTextFromSource("text", source)`;
- object-tree translation for widgets/actions/menus;
- dialog-specific `setTextTranslator(...)` in dialogs that support it;
- v2.31 log fallback through `appendLog()` for static messages and simple `Caption: value` patterns.

## What v2.31 harvests

The update helper now harvests these user-visible sources:

- explicit `uiText()` keys;
- Qt Designer `.ui` labels, placeholders and tooltips;
- common programmatic widget/action constructors;
- common `setText`, `setToolTip`, `setStatusTip`, `setWindowTitle` calls;
- simple combo-box `addItem()` visible labels;
- simple message-box title/body literals;
- simple static runtime log literals.

The helper deliberately avoids a blind C++ string-literal scrape because that previously polluted dictionaries with file names, settings keys, object names, protocol constants and internal marker strings.

## Remaining work

Some dynamic log lines are still better handled with explicit translation keys, especially strings assembled with several `.arg()` values or operator `+`. The preferred future pattern is:

```cpp
appendLog(uiText("log.audioInput", "Audio input:") + " " + selectedAudioInputLabel());
```

or, for message boxes:

```cpp
QMessageBox::information(this,
                         uiText("title.userQth", "User/QTH"),
                         uiText("msg.stopTxBeforeUserQth", "Stop TX before changing User/QTH."));
```

The v2.31 helper improves coverage substantially, but manual review remains required for exact style and idiomatic phrasing in every language.
