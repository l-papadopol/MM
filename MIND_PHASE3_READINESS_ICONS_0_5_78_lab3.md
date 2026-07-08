# MIND Phase 3 UI readiness indicators — 0.5.78-lab3

Scope: UI/status only. This lab keeps the MIND Phase 2 mode-aware FT8/FT4 ranker path and adds a visible readiness indicator to the MIND gain gauge.

## Implemented

- MIND gain gauge now shows a 64x64 status icon.
- Learning state: stylized brain icon.
- Assist state: stylized graduation-cap icon.
- Disabled/off state: grey inactive icon.
- Tooltip is localized through the existing MadModem runtime i18n system.
- Tooltip reports profile, assist readiness, samples, validation count and readiness reason.
- FT8/FT4/MSK144 profile switching continues to follow the active decoder profile.

## Meaning

- Learning: MIND is collecting samples/training for the current profile and is not yet assist-ready.
- Assist: the current profile has passed its readiness gate and MIND assist is active.
- Disabled: MIND is off or not available for the current view.

## Safety

No LDPC, CRC, decoder, QSB-combining, erasure-like weighting or multi-period combining logic was changed in this lab.
