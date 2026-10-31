# Architecture — MadModem 0.6.5

MadModem is a C++17 Qt Widgets application. Audio capture and bounded decoder
queues run independently of GUI rendering. CAT/PTT, waveform preparation and
logbook persistence use workers; the GUI owns FT session decisions and UI state.

FT8/FT4 decoders live in `modems/ft8/`, with shared protocol helpers and the adapted
FT reference sources under `third_party/`. RX publishes early results and boundary
recovery results. Doppler/drift support remains in the FT RX/TX paths. FFTW planner
access is synchronized. `FtQsoSequencer` is the deterministic decision core;
`FtSlotScheduler` and the audio worker handle UTC TX execution.

`FtAutomationLog.cpp` observes existing GUI automation decisions. It writes the
same context to Runtime log and optional structured session diagnostics. Its
bounded suppression state never gates candidates, sequencing, TX or retries.

`runtime/SessionDiagnostics` writes bounded-queue compressed JSONL on its own
thread, with redaction, periodic flushes, loss markers and shutdown drain.
`utils/InstallationPaths.h` separates managed per-user data from portable data.
`updates/UpdateController` checks published versions/builds and verifies downloads.

CW native/neural receivers, CW/RTTY skimmers, Q65 and MSK144 are independent modem
subsystems. Other audio/text/image modes retain their own workers and state.
See [MSK144](msk144/README.md), [Q65](q65/README.md) and [skimmers](skimmers/README.md).
Legal attribution and pinned source inventories are linked from
[third-party notices](../THIRD_PARTY_NOTICES.md).
