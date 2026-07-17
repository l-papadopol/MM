# MadModem versioning

MadModem public packages use `MAJOR.MINOR.PATCH` version numbers.

Current public release: **0.5.78**.

The version must agree in all release metadata:

- `MADMODEM_VERSION.txt`
- `MadModemVersion.h`
- the CMake `project(... VERSION ...)` declaration
- release/package names
- current README, release notes and localized help metadata

Development-only suffixes such as `.experimental` or `labNN` belong to temporary branches and test artifacts. They are not part of the 0.5.78 public release identifier.

Run `scripts/ci_release_version_guard.sh` before packaging. A full source archive should be named with the public version and preserve executable permissions on shell scripts.
