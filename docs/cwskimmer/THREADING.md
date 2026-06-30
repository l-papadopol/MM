# Threading Model

The library is deliberately single-threaded.

MadModem should call `processFloatMono()` from the existing audio/DSP worker thread, not from the GUI thread. The callback is invoked synchronously from that same thread.

Recommended MadModem integration:

```text
Audio callback / DSP worker
    -> CwSkimmerEngine::processFloatMono()
    -> callback emits lightweight event
    -> enqueue event into thread-safe MM queue
GUI thread
    -> drain queue on timer
    -> update RX textbox and waterfall OSD
```

Do not update Qt widgets directly from the callback unless the callback is already executed on the GUI thread. In the normal MM architecture, it should not be.

Suggested GUI refresh rate:

- RX textbox: event-driven or 10-20 Hz maximum;
- waterfall OSD: same cadence as waterfall paint;
- priority decoder selection: 5-10 Hz is enough.
