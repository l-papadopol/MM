# MadModem 0.6.4

FT periods and UTC are now at the bottom of Mode; the QRZ tab contains station information. Select CQ rows with Ctrl/Shift and use Add selected CQs to prepare a worklist. Start works one selected station at a time after a fresh CQ check; Pause lets the current QSO finish, and STOP stops TX and pauses the list. The separate incoming caller queue remembers stations calling your callsign.

FT capture profile reads use an atomic period snapshot so they can continue while the decoder owns its configuration lock. Search thresholds, passes, FEC and TX timing limits are preserved. Live logs now identify the source/build and report prepared FFT/tone reuse.

The QRZ tab between Mode and Rotator contains information for the selected station. QRZ account settings and saved
credentials remain in Settings. The drift help explains how to select a signal
with the RX marker and what frequency changes the option can recover.

FT8/FT4 prepare reusable synchronization calculations while receiving audio.
Message validation and the existing decode passes are preserved. Recorded FT8
checks show earlier results; the detailed comparison also records output
variations and does not claim increased sensitivity. See
`docs/architecture/RELEASE_0_6_4_R12.md` for measurements and limits.

The Linux DEB installer checks for unfinished dpkg operations before starting
APT and shows the repair command `sudo dpkg --configure -a` when required.
Portable updates identify the existing Ubuntu24.04/Fedora43 bundle before the
host distribution, accept matching TAR.GZ and ZIP downloads, and provide clearer
messages when a package or its checksums are unavailable. User settings,
logbooks, backup, SHA-256 verification and rollback are preserved.

The updater checks all published GitHub releases, including pre-releases,
independently of Latest. It compares the public version and distribution build.
Download and installation require confirmation and wait for idle operation.
If an old executable cannot select the matching Linux package, install this
revision manually once. Future checks use its corrected package selection.

Retained: experimental FT8/FT4 RX drift recovery, CW Native R12 / Neural R13,
compact FT periods, table font controls, response-window fixes and consistent
release metadata across the platform builds.
