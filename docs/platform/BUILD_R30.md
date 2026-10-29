# R30 source build dependencies

The native FT8/FT4 recovery target requires the double-precision FFTW3 library. Qt5 and Qt6 remain supported; Python is required for regression runners. The adapted recovery target requires compiler support for `-ftrivial-auto-var-init=zero` (the GitHub GCC/Clang toolchains provide it); configuration stops if this initialization safeguard is unavailable. Existing Hamlib, Qt Multimedia/SerialPort and optional Location/Help dependencies remain in place.

- Ubuntu: `libfftw3-dev`, alongside the existing Qt/Hamlib development packages.
- Fedora: `fftw-devel`, alongside `hamlib-devel` and Qt5 development packages.
- MSYS2 UCRT64: `mingw-w64-ucrt-x86_64-fftw`; use the distribution workflow's UCRT64 environment.
- Homebrew: `brew install cmake ninja pkg-config qt qtlocation hamlib fftw`.

For a local Unix Makefiles verification build:

```sh
cmake -S . -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
python3 tools/run_ft_reference_regression.py --build build --sanitize
```

`--sanitize` instruments the new recovery adapter, decoder, numeric helpers and FFT implementation with ASan/UBSan. LeakSanitizer can be requested with `--leaks` where `/proc` thread inspection is available; it is separate from the memory/bounds and undefined-behavior checks. The 6500 ms product cap remains active under instrumentation, so an interrupted request intentionally does not populate the completed-input cache.

`tools/run_ft_drift_probes.py build` additionally requires NumPy/SciPy. `tools/run_ft_replay_probe.py --build build --baseline` compiles a baseline disabling just the two additive hooks; its counterpart without `--baseline` tests R30. `PROBE_GATE=1 PROBE_GATE_FINAL=1` exercises early and final publication; `PROBE_OFFLINE=1` removes live UTC deadlines while retaining the offline recovery cap. For the tester WAV use `PROBE_OFFSET_SEC=5.5`. The original WAV is retained in the work checkpoint, outside the source release.

The distribution workflow installs FFTW on all four packaging environments. Runtime dependency scanning is recursive; standalone packages must carry the linked FFTW runtime and its required licenses/corresponding-source information. Native Windows and macOS validation must be run on the release commit.
