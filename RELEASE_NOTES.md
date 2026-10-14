# MadModem 0.6.2

- RTTY: added an optional band skimmer with 2–16 independent Mark/Space receivers, separate Baudot letters/figures states and shared anti-aliased audio resampling.
- RTTY skimmer: follows the selected baud, shift and polarity; tracks each pair while preserving tone spacing, rejects transient detections and suppresses duplicate sideband receivers.
- CW: retained simultaneous decoding of up to 16 independent carriers alongside manual RX A and RX B.
- Waterfall: both skimmers offer vertically scrolling station text or confirmed CQ callsigns. Columns avoid neighbouring text and traces; a zoom indicator identifies columns that cannot fit.
- CQ recognition: shared committed-token processing requires two matching callsigns in CQ context, including repeated CQ cycles, and expires old identifications.
- RX: bounded channel counts, one-second acquisition history, limited text history and batched display updates. Tracking resets after missing audio, retuning, format changes or local transmission.
- UI: restored visible RTTY skimmer controls and retained the optional selected-station text trail when the skimmer is disabled.
- Documentation: refreshed all 72 help pages in six languages, the GitHub README, build/distribution instructions and developer references.
- Sources: removed obsolete verification logs, screenshots and revision patches; consolidated skimmer documentation and removed unused RTTY controls and legacy callout code.
- Updated application metadata, Windows resources, translations and help namespaces to 0.6.2.

Installer and portable packages remain available on Windows, Linux and macOS. Windows retains automatic CPU selection in the installer. Linux portable packages include Qt5 and Hamlib. macOS packages use ad-hoc signing, without Developer ID notarization; Windows installers are unsigned.

Skimmers process the audio actually received from the radio, between 100 and 3500 Hz. Narrow radio filters, overlapping signals and QRM limit coverage and decoding. Skimmer output does not start TX, move an antenna or save a QSO. Native package builds and hardware/on-air verification remain necessary on the target systems.
