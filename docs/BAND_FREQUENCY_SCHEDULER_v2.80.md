# Band/Frequency scheduler design notes — v2.80

The scheduler is intentionally separated from the FT sequencer and from the TX/RX state machine.

```text
Scheduler = daily UTC QSY requests
Sequencer = QSO state and next FT message
TX/RX = audio and PTT mutex
```

The scheduler stores a daily UTC plan. Each event contains mode, band, frequency and whether the frequency is the FT standard frequency for the selected band. When an event is due, MM checks whether the scheduler is enabled for the current mode group. If the radio is transmitting, PTT is still active, or an FT QSO is active, the QSY is queued and retried once the condition clears.

The scheduler never disables Evil Mode, Auto CQ, Auto QSO or the FT sequencer. In an FT automation session it simply changes the rig frequency after the current QSO/TX has safely finished; Auto CQ/Auto QSO can then continue on the new band.
