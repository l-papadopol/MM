# MadModem versioning

MadModem public packages use `MAJOR.MINOR.PATCH` version numbers.

Current release: **0.6.3**.

The version must agree in all release metadata:

- `MADMODEM_VERSION.txt`
- `MadModemVersion.h`
- the CMake `project(... VERSION ...)` declaration
- release/package names
- current README, release notes and localized help metadata

Development-only suffixes such as `.experimental` or `labNN` belong to temporary branches and test artifacts. They are not part of the 0.6.3 release identifier.

Run `scripts/ci_release_version_guard.sh` before packaging. A full source archive should be named with the public version and preserve executable permissions on shell scripts.

Windows resource tuples and the six Qt Help namespaces must match the same version. Source revisions distinguish packaging updates without changing the public version.

## Promoting branch builds

Branch builds are published as prereleases with tags `build-MAJOR.MINOR.PATCH-N`.
To make one available to the updater, clear **Set as a pre-release**, enable
**Set as the latest release** and save it. Starting with 0.6.3 source R4, MM
extracts the public version from that tag, so its name and release title can
remain unchanged. Version tags such as `v0.6.4` remain supported. The public
version must increase for an update to be offered; rebuilding 0.6.3 does not
make it newer than another 0.6.3 installation. Older clients require a numeric
version tag or manual installation once to acquire the new parser.
