# Published-release updates — 0.6.4 source R7

Baseline: the complete 0.6.3 R6 source package. All modem, audio, runtime,
settings, QRZ-panel and main-window source files remain byte-identical to R6.
Only update discovery, distribution identity, relevant installer handling,
version metadata, help and tests are changed.

## Behavior

- Startup checks the paged `/repos/l-papadopol/MM/releases` API, not `/latest`.
  Published stable releases and pre-releases participate equally; drafts do not.
- Choose the highest public numeric version, then the highest distribution build
  of that version. API ordering and the Latest badge do not decide the result.
  The same/older build and lower public versions are not offered as upgrades.
- GitHub distribution runs embed their run number and source commit on every
  platform through CMake-generated build metadata. The publisher stamps the same
  build identity into release notes, preserving operator text on reruns.
  Generated `build-VERSION-N` tags carry the number directly; version tags use
  the stamped notes. Titles are display text and can be edited freely.
- An unknown local build (0) can acquire an official same-version build once.
  After installing the official build it has a comparable nonzero identity.
- Offers display installed/available version and build, and identify pre-releases.
  Download and installation still require confirmation. Existing SHA256/package
  checks, operation-idle checks, backup and user-data preservation remain active.
- Automatic startup checks defer active operations and retry once idle. The
  existing startup-check switch is respected. A closing application cancels the
  pending automatic check and active requests.
- Portable replacement verifies both expected public version and build. Wrong
  builds trigger rollback. Native DEB/RPM package release numbers use the actual
  compiled distribution build, enabling same-version native package upgrades.
  The macOS component explicitly allows same-public-version bundle replacement;
  Windows already copies its selected payload with ignoreversion.
- Pagination is bounded at 50 pages of 100 releases. A truncated check fails
  rather than declaring the installation current.

## Verification

- Full Release Linux / Qt 5 application build: PASS.
- Native updater regression: PASS. Fixtures cover stable/pre-release discovery,
  Latest independence, same-version newer builds, installed/older builds,
  version downgrades, drafts, unknown local builds, version-tag metadata,
  malformed/contradictory identities, and page merging.
- Actual checker control flow against simulated asynchronous API responses:
  PASS. Busy-operation deferral is exercised, a 100-entry first page is read,
  and the second-page prerelease reaches the offer method. No live request is
  made by this test.
- Real publisher executed with a local gh double: PASS. It creates a numbered
  prerelease without Latest, preserves custom notes and refreshes one machine
  identity on rerun. No GitHub release was created or modified by these tests.
- Portable helper integration: PASS. Matching build installs while preserving
  user state; wrong build restores the previous executable and settings.
- Native package identity: PASS. Compiled build numbers order successive native
  packages; mismatched package/executable identities are rejected, and local
  unnumbered builds retain the source-revision fallback.
- `--version` remains exactly `MadModem 0.6.4`. `--build-info` reports version,
  build and commit. The verification build used fixture number 120; this number
  is NOT hardcoded in sources or future distribution builds.
- Distribution YAML, shell syntax, version consistency and all 7 release guards:
  PASS. Existing 6 FT source guards: PASS. Modem numerical code is unchanged.

Real GitHub download/installation and native Windows/macOS execution were not
performed here. Native GitHub distribution builds retain their platform package
checks. The first installation of 0.6.4 may need to be manual: an already installed
0.6.3 executable continues to use its previous updater rules until replaced.
Subsequent published builds require neither Latest promotion nor tag renaming.

Reproduction:

```
ctest --test-dir /absolute/build -R madmodem_update_regression --output-on-failure
python3 tests/test_update_helpers.py
python3 tests/test_update_publication.py
```
