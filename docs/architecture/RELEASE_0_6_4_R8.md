# MadModem 0.6.4 source R8 — release consistency audit

## Findings

The canonical R7 archive has SHA-256
`41c9e38277e689f316766660de10d1770177c6d84344cb22b178de50331ea33a`.
Its 2,001 entries pass ZIP CRC validation. Its version file, revision prefix,
CMake project, public version header and Windows resources all specify 0.6.4.
The supplied GitHub logs instead show a 0.6.3 version file, then a 0.6.3 header
in the build checkout. Those metadata bytes differ from the canonical archive.
The remote tree was not available for a complete comparison.

R7 did retain the Designer title and corresponding dictionary entries at 0.6.3.
These are aligned to 0.6.4 in R8. Current platform installation documentation is
also aligned, including removal of an obsolete sentence denying updater support.
Historical changelog, baseline reports and tests intentionally retain older
version numbers where they describe earlier versions or downgrade fixtures.

## Changes

- The shell version guard delegates to a standard-library Python validator.
- One invocation reports all mismatches: revision prefix; header string,
  display, prerelease and numeric macros; CMake project; Windows numeric/string
  resources; Designer title; all six Qt Help namespaces, filters and start pages.
- All four distribution jobs depend on one shared `source-check` job before
  platform setup. The macOS matrix covers both Intel and Apple Silicon.
- The manual source workflow also checks metadata before dependency setup.
- Nine regression tests execute the real shell guard with isolated temporary
  source trees, including both reported mixed-version conditions, corrupt or
  missing files, wrong version tags, numeric-field errors and CRLF metadata.
- R7 update discovery, distribution build identity, native/portable installers
  and all modem, audio, runtime, settings, QRZ and sequencer implementation files
  remain byte-identical to the R7 archive. Only metadata, CI validation, the UI
  version title/dictionaries and relevant documentation change.

## Validation

- Consolidated release suite: PASS, 8/8 checks, including 9 guard tests.
- Consolidated FT source guards: PASS, 6/6 checks.
- Actual publisher with a local simulated gh executable: PASS; no remote writes.
- Portable helper matching-build installation/state preservation and wrong-build
  rollback: PASS. Release-note refresh and native package identity checks: PASS.
- Both workflow YAML documents parse; every distribution platform depends on
  `source-check`; automatic workflow build number/commit remain configured.
- R8 is a source-only validation/metadata change. No new native application build
  was run locally for R8; native Windows/macOS build and installation verification
  remain the responsibility of the GitHub platform jobs. The R7 Linux build
  evidence is recorded separately in UPDATES_0_6_4_R7.md.

## Upload

Extract the archive. Upload the complete contents of
`MadModem_0_6_4_FULL_SOURCE/` into the repository root, including `.github`.
The root must directly contain `CMakeLists.txt`, `MadModemVersion.h` and
`MADMODEM_VERSION.txt`; do not create another nested source directory.
Commit all files together and use the new commit's workflow run. Re-running
an older workflow uses its original commit.
