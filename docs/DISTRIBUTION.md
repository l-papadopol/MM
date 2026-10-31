# GitHub distribution — MadModem 0.6.5

Copy the complete source directory contents into the repository root, including
`.github`. To clean up documents left after overlaying an older source tree,
optionally run `python3 scripts/clean_legacy_docs.py --apply` before committing.
The script removes only explicitly listed retired documents and preserves current
manuals, source code, upstream notices and unrelated files. Replacing files alone
does not remove old tracked documents from GitHub. Documentation cleanup is a
repository maintenance operation and does not block compilation or packaging.

**Build installers** runs on main/master pushes, version tags and manual dispatch.
It builds Windows UCRT64, Ubuntu 24.04, Fedora 43 and both macOS architectures.
After all jobs succeed, it publishes portable archives, EXE/RUN/DEB/RPM/PKG
installers and SHA256SUMS. Native Linux packages are the same payloads wrapped by RUN.

A branch build creates a numbered prerelease (`build-0.6.5-N`). Tag `v0.6.5` creates
a version release. The tag must match all source metadata; the source-check job
rejects mixed versions. Run the workflow on the new commit, not an older run.
Only the publish job requests `contents: write`; the normal GitHub token is used.
Repository policy must allow release writes. Immutable releases require a new version.

`github.run_number` becomes the compiled build number on every platform; the source
commit is embedded too. Updating a release preserves operator notes and refreshes
its machine-readable build/commit identity. Build numbers are not reset per version.

The separate **Full regression suite (manual)** runs the extended Qt5/Qt6 checks.
Native installer/runtime checks belong to the distribution jobs. Signing with
Windows/Apple publisher credentials is not configured.

## Distribution repositories

GitHub DEB/RPM downloads support direct installation. PPA Launchpad and Fedora Copr
are options for additional update repositories. Official Debian/Fedora inclusion
requires rebuildable source packages, packaging review and ongoing maintenance;
a binary DEB or RPM alone is not a submission. Source packaging for those repositories
is separate work. No external repository is configured by these sources.
