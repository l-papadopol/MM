# MadModem 0.6.3 source release

Source revision: `0.6.3-source-r3-hamlib-windows-native-r12-neural-r13`.

## Delivered behavior

The CW RX selector contains exactly **Native R12** (default) and **Neural R13**
(experimental). Native timing, acquisition and tracking algorithms are unchanged
from R12. The sole neural model is `mm-cw-r13-search`: the R13 weights, causal
20% envelope floor and bounded acoustic CTC prefix search are preserved.

The old `neural_ctc` and `neural_envelope` preferences migrate to `neural_search`
when settings are loaded. Unknown preferences fall back to Native. The band
skimmer remains native; CW audio and serial KEY transmission remain independent
of the receive engine.

The FT clock is fixed outside the settings scroll area, above QRZ. Its nested
sequencer group and duplicated period/next-message labels have been removed.
Detailed sequencer state remains in the clock tooltip. The live TX banner and
selected message row continue to update; FT timing and sequencing are unchanged.

## Verification on Linux

| Check | Result |
|---|---|
| Full Release application build | Passed with Qt 5.15 / Hamlib 4.5.5; executable reports `MadModem 0.6.3` |
| Native R12 source preservation | 19 skimmer/TX source files byte-identical to R12 |
| R13 model preservation | Calibrated checkpoint byte-identical; header differs only in its namespace |
| R13 inference parity | All 67,200 tested logits bit-identical to frozen R13 |
| R13 streaming parity | Same text for chunks of 1, 64, 257, 997 and 4,096 samples |
| Actual application route | Native and Neural emitted text/payloads match the preceding R13 package |
| Native regression executable | All carrier-gated relative-timing checks passed |
| CTest CW routing, integration and CTC-prefix checks | 3/3 passed |
| GUI engine selector and settings | Two choices, old-preference migration, mode switch, save/reopen and return to Native passed |
| GUI clock | FT4/FT8 checked at 1280×900, 1600×1000 and 1024×768, including parent clipping |
| Release suite | 7/7 checks passed: localization, documentation, version, operator copy, rules and macOS portability preflight |

The complete QSO streaming fixture decodes as:

```text
CQ CQ CQ DE IZ6NNH IZ6NNH PSE K UR 599 599 BK
```

Native's existing worker thread can change the polling block at which a callback
is observed. This check compares its emitted payloads rather than requiring
identical wall-clock scheduling. Neural sample-driven emission events match R13.

The R13 checkpoint SHA-256 is
`16d5602b7d66b71306777ac3fa8d3c2f60baf08149577736aebc1a1ff3e2b494`.
The model card records the updated header hash after namespace consolidation.

Historical training checkpoints, rejected candidates and the older runtime
model are excluded. The active R13 training checkpoint, reproducible tooling
and historical benchmark reports remain. Generated build products, caches,
local settings and test screenshots are excluded from the source archive.

## Scope

This release packages the requested R12/R13 engines; it does not introduce new
training or claim a new accuracy/speed improvement. The historical R13 report
retains the measured advantages and limitations. Native Windows/macOS builds
and tests with a physical radio have not been run for this release. The Linux
GUI check uses the offscreen Qt backend; waterfall OpenGL rendering is outside
that check. Qt Location/Qt Help were unavailable in this build environment;
the existing offline-map and embedded-HTML fallbacks were used.

## Windows packaging revision R2

The previous GitHub job failed during MSYS2 package installation with
`target not found: mingw-w64-x86_64-libusb`, before compiling the application.
The live MINGW64 repository no longer contains that package. The UCRT64
repository contains libusb and all the Qt5/ANGLE/OpenSSL dependencies required
by this project.

R2 changes the workflow to `msystem: UCRT64` and uses
`mingw-w64-ucrt-x86_64-*` packages throughout. Build and deployment scripts
resolve tools and runtime DLLs from the selected prefix, defaulting to `/ucrt64`.
Hamlib's build and install directories are specific to that prefix; the DLL
search no longer falls back to a MINGW64 installation. Qt5 is explicitly
selected. Both CPU variants and published asset names remain unchanged.

Checks: both edited shell scripts pass `bash -n`; workflow YAML parses; every
one of its 20 requested package/group/provider entries exists in the current
MSYS2 repository databases. The consolidated release suite passes 7/7. All
application C/C++ sources and neural model files remain byte-identical to the
preceding 0.6.3 source archive. No decoder retraining or Linux rebuild is needed
for this packaging-only revision. The Windows compilation and installer tests
must be rerun on GitHub; they have not been executed in this Linux environment.

References, checked 2026-10-18:

- https://www.msys2.org/news/#2026-03-15-deprecating-the-mingw64-environment
- https://www.msys2.org/docs/environments/
- https://packages.msys2.org/packages/mingw-w64-ucrt-x86_64-libusb
- https://repo.msys2.org/mingw/ucrt64/ucrt64.db
- https://repo.msys2.org/mingw/mingw64/mingw64.db
- https://repo.msys2.org/msys/x86_64/msys.db

## Windows Hamlib revision R3

The next CI failure was in bundled Hamlib, before the MadModem build:
`precise_time.c` called `assert()` without including its header. GCC also ignored
MSVC's `allocate` attribute on the startup initializer, leaving the intended
frequency initialization unregistered. R3 includes `assert.h`, registers the
initializer with GCC's constructor attribute, and retains the MSVC CRT path.
The existing Windows conversion scale is preserved; no CAT or CW timing policy
is changed. Builds with assertions disabled remain warning-free.

The two Autoconf feature lists now use literal whitespace-separated names,
without shell-style backslashes inside quoted M4 arguments. Two concatenated
BSD header names are separated. This corrects the malformed probes and warnings
shown in the same CI log.

Checks already completed: the affected Windows branch compiles and links into
PE executables with real MinGW-w64 Windows headers, both with and without
`NDEBUG`, under `-Wall -Wextra -Werror`. The object contains the GCC constructor
section and the executables import the two performance-counter APIs. An
isolated host test with Windows API doubles verifies initialization before
`main`, finite values and forward progress in debug/release. The unchanged
POSIX branch compiles with warnings treated as errors. Windows executables have
not been run in this Linux environment. Full native UCRT64/Qt installer checks
remain the responsibility of the GitHub Windows job.

The complete bundled Hamlib source also cross-builds successfully for Windows
with GCC 13.2 / MinGW-w64, including its C++ binding, `rigctl.exe`, `rigctld.exe`
and the remaining command-line programs. The build uses
`-Werror=implicit-function-declaration`. Regenerated configuration recognizes
`snprintf` correctly and no longer reports the malformed-literal warnings.
This is a Linux-hosted cross-build, separate from the native UCRT64/Qt GitHub job.
