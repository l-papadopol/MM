# Priority Decoder Selection

MadModem will skim multiple CW channels but show only two channels prominently in the RX textbox.

## Current library selector

`priorityChannels(2)` sorts channel states by:

```text
confidence * 100 + positive SNR + small rolling-text continuity bonus
```

This is intentionally simple for the first port.

## Recommended MadModem policy

A channel is promoted to the RX textbox only when it matches the user-selected RX A or RX B marker. Automatic channel ranking is diagnostic-only for now. A future Auto-B mode may use these conditions:

- confidence is acceptable;
- SNR is above the noise threshold;
- text is not just one ambiguous partial symbol;
- the channel has produced recent committed text;
- the channel contains useful radio tokens such as CQ, DE, callsign-like words, TEST, K, KN, SK, 599.

Suggested stable policy:

```text
score = 0
score += confidence * 100
score += clamp(SNR dB, 0, 40)
score += committed text freshness bonus
score += callsign/CQ/DE token bonus
score -= rapid channel-flip penalty
```

## Anti-flicker rule

For the current manual A/B mode, the RX textbox never jumps automatically: A follows the green marker, B follows the blue marker when enabled. If Auto-B is reintroduced later, hold the chosen channel for at least 2-4 seconds to avoid jumpy output.

## RX textbox behavior

Recommended:

```text
Priority A -> main RX line / normal text
Priority B -> secondary RX line or tagged text
Other channels -> waterfall OSD only
```

The old CW selected-tone model should be removed after this is validated.
