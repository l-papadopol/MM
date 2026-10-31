# Troubleshooting — MadModem 0.6.5

Include the complete window title or `MadModem --build-info`, operating system,
mode, radio/audio configuration and the time of the problem in a report.

## Evil Auto QSO traces

Open **Runtime log** to inspect or export the ordinary text log. Automation traces
start with `FT automation:` and contain one JSON object per line. They remain
available even when complete session logging is disabled.

Enable **Settings → Diagnostics → complete session log** to also record structured
`ft.automation` events in the compressed `.jsonl.gz` file. Enable it before the test;
it does not reconstruct earlier events. Runtime log has a bounded display buffer;
the complete session log is preferable for longer operation.

| Action | Meaning |
| --- | --- |
| `control` | Evil unlock and Auto QSO enable/disable with policy/limits |
| `candidate.enqueue`, `candidate.refresh`, `candidate.select` | CQ queue, ranking and selected target |
| `candidate.reject`, `candidate.remove` | Blacklist, duplicate/cooldown, ownership, expiry or capacity reason |
| `candidate.wait`, `candidate.lookup` | Logbook query, directed traffic or fresh CQ required |
| `wait` | Active QSO/TX, queued TX, caller/manual priority, missing identity or no CQ |
| `qso.start`, `qso.rx_decision`, `qso.advance` | Selected answer, received message and sequencer transition |
| `tx.arm`, `tx.prepare`, `tx.ptt`, `tx.audio`, `tx.defer` | Plan, waveform, PTT, actual audio start/finish/error and deferral |
| `qso.retry`, `qso.timeout`, `qso.complete`, `qso.log` | Attempts, cooldown, completion and ADIF save result |
| `operator.select`, `sequencer.stop` | Manual takeover or cancellation reason |

Records contain version/build, `trace_id`, `qso_id`, mode/band, state, DX, queue sizes, retry count,
TX message, `tx_plan_id` and UTC slot. CQ/received-decode events include their own
message, slot, SNR or priority evidence. `logged_qso_id` identifies the original
contact when an asynchronous ADIF save finishes after another QSO has started.
A scheduled/armed TX is not proof of transmission: use `tx.audio` reason `started`.
Unchanged repeated waits are suppressed for 30 seconds, then refreshed; state or
payload changes appear immediately. The suppression cache is bounded.

Read a complete log with `python3 tools/read_session_log.py <file.jsonl.gz>`.
For an interrupted gzip stream, add `--recover`; it preserves complete readable
lines and exits with a warning status. Normal closure finalizes gzip integrity.
Logs retain existing password/token redaction.

## Reading RX diagnostics

Early and boundary FT decode phases can share a period; adding their durations
is not the time spent entirely after its boundary. Display-audio drops do not by
themselves imply decoder-audio loss: inspect sequence gaps and missing samples.
An RX-only log cannot prove what TX or Auto QSO did. Decoder sensitivity comparisons
require matching input audio and settings.
