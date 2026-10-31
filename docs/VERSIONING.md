# Version identity — MadModem 0.6.5

`MADMODEM_VERSION.txt`, `MadModemVersion.h`, the CMake project version, Windows
resources, the UI default title and six Qt Help projects carry the public version.
`scripts/ci_release_version_guard.sh` validates them together before packaging.

CMake generates `MadModemBuildInfo.h` from the distribution workflow number and
commit. The main window displays `MadModem 0.6.5 — build N`, including after a
language change. Local builds use build 0. `--version` remains numeric for installer
checks; `--build-info` additionally reports build and commit.

The source revision identifies the source package, independently of the public
version and GitHub distribution build number. Never invent a GitHub build number
for a source-only ZIP. See [distribution](DISTRIBUTION.md).
