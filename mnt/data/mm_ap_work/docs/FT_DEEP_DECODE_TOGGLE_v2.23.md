# FT deep decode toggle - v2.23

MM now exposes the MSHV-style extra decode passes as an explicit operator setting.

## UI

Location: FT4/FT8 Mode settings tab.

Checkbox:

```text
Deep decode / triple pass
```

Tooltip:

```text
Enable MSHV-style extra decode passes with subtraction. This can reveal weak/overlapped signals, but costs more CPU; leave off on old PCs or when TX timing is critical.
```

## Runtime behavior

### Checkbox disabled

- FT8: one candidate-search / LDPC decode pass.
- FT4: one decode pass.
- Lowest CPU load.
- Recommended for old PCs, portable machines, and time-critical field testing.

### Checkbox enabled

- FT8: three MSHV-style passes with signal subtraction between passes.
- FT4: second-pass subtraction/rescan path enabled.
- Higher sensitivity in crowded/overlapped conditions.
- Higher CPU load.

## Architecture boundary

The setting is passed only to `Ft8RxDecoder`:

```text
MainWindow/AppSettings -> Ft8RxDecoder::setDeepDecodeEnabled(bool)
```

It does not change:

- UTC slot scheduler;
- PTT control;
- FT TX worker;
- TxPlan;
- QSO sequencer;
- logbook;
- CAT/Hamlib.

This preserves the earlier divide-and-conquer timing model while allowing the RX decoder to be made deeper when the user explicitly asks for it.
