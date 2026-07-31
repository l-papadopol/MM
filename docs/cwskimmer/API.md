# Native CW API

## `SelectedToneCwTracker`

One instance represents one exact-tone receiver. RX A and RX B use separate
instances and therefore separate filters, AFC, timing, pre-roll, text and WPM.

Important inputs:

- selected tone and AFC range;
- requested/automatic bandwidth;
- minimum SNR;
- initial WPM hint and Auto-WPM state;
- normalized mono floating-point audio.

The callback returns only committed text together with tracked frequency, SNR,
confidence, WPM and rolling text.

## `CwSkimmerEngine`

Scans the configured audio passband and reports persistent carrier lanes. It does
not publish Morse characters and cannot inject text into RX A or RX B.

## `CwBayesianDecoder`

Consumes one-millisecond soft observations. It is internal to the selected-tone
tracker and is the only component that assigns Morse timing meanings.
