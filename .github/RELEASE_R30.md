## 0.6.4 source R30 — integrated native FT8/FT4 recovery

- Complete the headless MSHV 2.76.7 FT8/FT4 recovery path alongside the existing receiver. Preserve ordinary decode, incremental publication, GPU options, TX/CAT and Doppler recovery.
- Isolate decoder configuration/history and FFTW plans per instance; share the TX-priority message-codec lock. Cancel native candidate/OSD work and stop admission at the live UTC deadlines.
- Correct an upstream FT8 sync-quality array overrun; retain protocol/CRC checks and reject uncertain `?` recovery rows. Keep early recoveries through the final slot callback and invalidate stale generation/QSO/band context.
- Tester WAV, three paired runs: gate plus final 41 -> 52 messages, offline 36 -> 60, no baseline messages lost. Of the 60 offline messages, 55 match the tester's 66-row JTDX list; five extra rows remain unverified. A boundary-only job retained 41 with no reliable extra recovery time.
- Linux Qt5 and clean Qt6 full builds: 53/53 tests passed in each; independent Doppler and native sanitizer results are in the R30 validation report. Native Windows/macOS, physical GPU/radio/PTT and crowded real FT4 sensitivity remain unverified here.
- Add FFTW3 to Linux/Windows/macOS workflow dependencies. Public version remains 0.6.4; source revision is `0.6.4-source-r30-reference-ft-recovery`.
- Details: [R30 release and verification](docs/architecture/RELEASE_0_6_4_R30.md), [three-source audit](docs/architecture/FT_DECODER_COMPARISON_R30.md).

### GitHub build and validation

Commit the complete tree, including dot-prefixed `.github` and bundled licenses. Run **Full regression suite (manual)** for Qt5 and Qt6 on that commit and run **Build installers**. The distribution workflow installs FFTW on Ubuntu/Fedora, MSYS2 UCRT64 and Homebrew; recursive package dependency scanners must retain the FFTW runtime. No GitHub push, tag, or publication was performed in this verification environment.

Use a branch/manual numbered prerelease when `v0.6.4` already belongs to another commit. Existing public tags should retain their original commit. Native Windows/macOS compilation and actual radio/PTT/GPU behavior are not certified by the Linux results.
