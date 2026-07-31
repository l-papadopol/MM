# MSK144 decode-depth UI correction — 0.5.73a

MadModem keeps MSK144 decode depth compatible with the MSHV semantics:

- `1 = Fast`: short-ping decoder only.
- `2 = Normal`: short-ping decoder plus 4-frame coherent averages.
- `3 = Deep`: short-ping decoder plus 4-, 5- and 7-frame coherent averages.

The default is `Normal`, matching MSHV's MSK144 default.

## UI placement

The decode depth is no longer exposed as a wide combo box in the MSK144 mode panel. The mode panel is for operational items such as period, RX frequency, DF tolerance, short messages, SWL, contest, and TX period selection.

The decode depth now lives in the global menu:

```text
Decode -> MSK144 decode depth -> Fast / Normal / Deep
```

This mirrors the MSHV concept more closely: decode depth is a decoder policy, not a per-QSO field.

## Persistence

The selected depth is stored in the existing INI settings file as:

```text
MSK144/decodeDepth = 1 | 2 | 3
```

If the key is missing or invalid, MadModem falls back to `2 = Normal`.

## Runtime behavior

Changing the menu item immediately calls `applyMsk144Settings()`, updates the MSK144 decoder backend, and refreshes the MSK144 status label/log string.
