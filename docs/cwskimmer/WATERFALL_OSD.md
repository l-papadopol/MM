# Waterfall OSD Plan

## Goal

CW mode behaves like a controlled skimmer: decoded text may appear near the corresponding audio-frequency lane on the waterfall, while the main RX textbox is reserved for the operator-selected RX A and optional RX B markers.

## OSD data source

Use `CwSkimmerEngine::channelStates()` for every active channel:

```cpp
for (const auto& st : engine.channelStates()) {
    st.audioFrequencyHz;
    st.rollingText;
    st.confidence;
    st.snrDb;
}
```

## Frequency mapping

The library reports audio frequency in Hz. MadModem already maps audio-frequency offsets to waterfall X/Y coordinates. The integration layer should convert:

```text
audioFrequencyHz -> waterfall pixel coordinate
```

depending on the current waterfall orientation.

## Rendering policy

Recommended first implementation:

```text
confidence < 0.25: do not draw
0.25..0.55: draw faint/short rolling label
> 0.55: draw normal OSD label
RX A/B: draw stronger and also send to RX textbox
```

## Vertical/scroller text

User requirement: text should scroll vertically on the waterfall.

Practical rendering model:

```text
channel lane fixed by audio frequency
new decoded chars append to channel rolling text
waterfall paint draws the latest N chars along the lane
older chars move with waterfall history
```

Implementation detail for Qt:

- keep per-channel OSD ring buffer with timestamped fragments;
- when waterfall scrolls, old text positions follow the same scroll transform;
- newly committed text starts at the current waterfall head line;
- RX A/B can use larger or clearer font.

This avoids drawing one huge static string and matches the visual behavior of a real-time skimmer.
