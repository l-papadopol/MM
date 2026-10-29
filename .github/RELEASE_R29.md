## 0.6.4 source R29 — FT8/FT4 coherent recovery and CRC-aware FEC

- Replace degenerate FT8 amplitude-group retries with complex 1/2/3-symbol recovery, including complete tail groups extending into Costas symbols.
- Enable FT4 complex 1/2/4-symbol recovery in normal deep reception; retain the cheap early gate. Add CRC-checked order-1 OSD fallback under the shared resource permit.
- Both LDPC implementations now require parity AND CRC for success; parity-only vectors continue iterating and reach recovery on failure.
- Extra recovery admission is limited by cancellation, 24 ms per candidate and the UTC slot/reply deadline. FT4 OSD statistics are included in session diagnostics.
- Actual-decoder regression checks valid/invalid-CRC codewords, FT4 recovery witnesses, noise/silence and gate/expired slots. GitHub's manual Qt5/Qt6 regression workflow runs this test using Unix Makefiles.
- Public version stays 0.6.4; source revision is R29 (R28 was an investigation, not a source release). Windows and real RF/GPU still require field validation. This is not a JTDX engine replacement or parity claim.
- Measurements and limitations: docs/architecture/RELEASE_0_6_4_R29.md and FT_R29_RESULTS.json.

### Publishing this source revision

Upload the complete source tree, preserving `.github/workflows` and all bundled dependencies. The existing distribution workflow builds the three platforms. The manual **Full regression suite** workflow additionally exercises Qt5 and Qt6 plus the native FT recovery test. Run it on the same commit as the distribution build.

This source revision retains public version 0.6.4. The release-version guard requires a matching numeric tag (`v0.6.4`) when building from a tag; use a branch/manual run for R29 if that tag already identifies another commit. Do not move an existing public tag to publish a source revision. Use the source revision/build number in beta notes.

The changes do not implement JTDX's general history/AP system or replace the SIC filter. Those are architectural differences, not defects silently claimed fixed by this patch. The four diagnosed metric/FEC defects are the scope of R29.
