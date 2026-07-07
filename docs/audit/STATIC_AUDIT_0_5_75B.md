# MadModem 0.5.75b static audit after MSK144 MIND domain bug

Date: 2026-06-29
Baseline audited: 0.5.75a
Output branch: 0.5.75b

## Scope

This audit reviewed the current tree, with special attention to recent high-risk areas:

- MIND mode/domain gating for FT8/FT4/MSK144.
- MSK144 MIND candidate ranking integration.
- Q65/MSK144 UI plumbing.
- Q65/MSK144 status strings and developer-facing UI leakage.
- CMake source lists and obvious missing source paths.
- QSO form integration for newly added modes.

A full Qt build could not be executed in the sandbox because Qt development packages are not present here, so this audit is static plus source-list/path checks. The user build environment remains the final compile authority.

## Static checks performed

- Extracted the 0.5.75a package from a clean archive.
- Checked CMake `add_library` / `add_executable` source paths for the active targets:
  - `madmodem_cwskimmer`
  - `madmodem_msk144`
  - `madmodem_q65`
  - `MadModem`
- Checked project quoted includes outside generated Qt files and excluded large upstream reference trees.
- Reviewed MIND profile/status flow from `MainWindow` -> `DeepDspController` -> `DdspPanelWidget`.
- Reviewed MSK144 candidate ranking path in `Msk144Decoder`.
- Reviewed Q65/MSK144 UI/layout/status side effects in `MainWindow` and decoder shells.

## Critical findings fixed in 0.5.75b

### 1. MIND Assist request still used the FT readiness gate

`DeepDspController::setAssistMode()` still calculated readiness using the FT8/FT4 global validation window. This was conceptually the same bug family as the earlier UI issue: MSK144 could appear queued/active according to FT state instead of MSK144 state.

Fixed: `setAssistMode()` now selects the readiness gate from the active profile. MSK144 Assist uses only:

- MSK144 sample count;
- MSK144 validation count;
- MSK144 ranker accuracy;
- MSK144 best ranker accuracy.

### 2. MainWindow enabled MSK144 candidate reordering when Assist was only requested

`MainWindow` passed `assistedRankingEnabled` based on `assistMode == assisted`, not on `status.assistEnabled`. That meant an unready MSK144 Assist request could still reorder candidates by MIND confidence, even while the UI said Assist was queued.

Fixed: decoder candidate reordering is enabled only when `status.assistEnabled` is true.

### 3. MIND profile panel reused active-profile stats for manual profile views

When the user selected a profile view manually, the panel could show stats from the currently active profile rather than the selected one.

Fixed: manual FT8/FT4/MSK144 profile views now show profile-specific sample counts and suppress readiness/accuracy if that profile is not the active validated profile.

### 4. MSK144 samples were counted as FT samples in one path

`submitNativeMsk144Sample()` incremented `m_ftSamples`. This made old aggregate labels misleading.

Fixed: MSK144 increments `m_msk144Samples` and the shared native weak-signal sample count, not the FT-specific counter.

### 5. Loaded mixed replay datasets restored FT sample count incorrectly

`loadGoldDataset()` restored `m_ftSamples` from total native samples, which could include MSK144.

Fixed: `m_ftSamples = loadedFt8 + loadedFt4` after loading a mixed dataset.

### 6. Q65 QSO UTC field did not refresh

The periodic UTC refresh list included MSK144 but not Q65.

Fixed: Q65 QSO form is included in `updateQsoUtcFields()`.

### 7. Developer/status chatter was still visible in runtime status

Q65/MSK144 runtime status strings still exposed implementation notes such as FFTW bridge/waiting/promoted state.

Fixed: cleaned status strings to keep the UI operational and short.

## High-risk findings not fully solved in 0.5.75b

### A. MSK144 RX is not yet a full MSHV decoder port

The current `Msk144Decoder` is a lightweight C++ frame-search/demod/LDPC attempt using MSHV packing/unpacking helpers. It is not yet a direct port of MSHV's full `DecoderMs::msk_144_40_rtd()` / `msk_144_40_decode()` chain.

Consequence: on-air sensitivity and decode parity with MSHV/WSJT-X are not guaranteed. MIND training quality also depends on valid positive samples; without reliable MSK144 decodes, training may collect mostly negatives.

Required next step: promote the actual MSHV MSK144 decoder chain into a stand-alone backend and wire it behind the existing `Msk144Decoder` API.

### B. Q65 RX remains staged/buffered, not fully active

Q65 TX uses the MSHV generator path, but Q65 RX is still a buffered shell until the full FFTW-backed DecoderQ65 bridge is promoted.

Consequence: Q65 must not be described as fully receiving/decoding yet.

Required next step: wire `DecoderQ65` + `decoderpom` + FFTW into `madmodem_q65` behind a worker interface.

### C. MIND still uses one shared neural network for FT8/FT4/MSK144

The UI and readiness gates are now domain-aware, but the underlying model remains shared. This is acceptable for Shadow/experimental training, but it is not ideal if FT and MSK144 features compete.

Recommended next step: add separate per-domain checkpoints or add a domain tag to the input vector before promoting MSK144 Assist beyond experimental.

## 0.5.75b patch summary

- Fixed domain-specific `setAssistMode()` readiness.
- Fixed `MainWindow` integration to use `status.assistEnabled` for assisted ranking.
- Fixed MIND profile view sample/readiness display.
- Fixed MSK144 sample accounting.
- Fixed mixed replay dataset accounting.
- Fixed Q65 UTC refresh.
- Cleaned Q65/MSK144 status strings.

## Build note

CMake project version remains numeric `0.5.75` because CMake `project(VERSION)` does not accept suffixes like `75b`. Application/display version files are set to `0.5.75b`.
