# MadModem versioning

MadModem public packages use `MAJOR.MINOR.PATCH` version numbers.

Current release: **0.6.4**.

The version must agree in all release metadata:

- `MADMODEM_VERSION.txt`
- `MadModemVersion.h`
- the CMake `project(... VERSION ...)` declaration
- release/package names
- current README, release notes and localized help metadata

Development-only suffixes such as `.experimental` or `labNN` belong to temporary branches and test artifacts. They are not part of the 0.6.4 release identifier.

Run `scripts/ci_release_version_guard.sh` before packaging. A full source archive should be named with the public version and preserve executable permissions on shell scripts.

Windows resource tuples and the six Qt Help namespaces must match the same version. Source revisions distinguish packaging updates without changing the public version.

## Distribution build identity and update discovery

Every distribution workflow run embeds the same positive build number and source
commit in every platform binary. CMake generates `MadModemBuildInfo.h` from the
workflow environment; local builds without an identity use 0 (unknown).
`MadModem --build-info` prints version, build and commit without opening the UI.
`--version` retains its existing numeric-version response.

MM 0.6.4 queries the paged published-release list. Published pre-releases and
stable releases both participate, regardless of the Latest badge; drafts do not.
The highest public version wins, then the highest build number of that version.
A newer build of the same version can therefore be offered without changing the
public version. Installed/older builds and public-version downgrades are skipped.

Generated tags use `build-MAJOR.MINOR.PATCH-N`. Numeric version tags such as
`v0.6.4` obtain their build number from machine metadata added by the publisher
to release notes, while preserving operator text. Release titles remain display
text. There is no need to rename generated tags or promote pre-releases to Latest.

The native Linux package release field uses the compiled distribution number,
so same-public-version builds are genuine DEB/RPM upgrades. Portable verification
checks the expected version and build before accepting a replacement. Windows
payload copying and the macOS component allow explicit same-version replacement.

Older 0.6.3 clients keep their old detection rules until 0.6.4 is installed once.
Install manually if necessary; all subsequent published builds use the new checker.
An unidentified local build may be offered an official build of the same version
once, acquiring a comparable build identity on installation.
