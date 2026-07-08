# MadModem 0.5.77.experimental — GitHub release tagging note

The experimental Radio Telescope source version is stored in `MADMODEM_VERSION.txt`
and in `MadModemVersion.h` as `0.5.77.experimental`.

GitHub Actions must build from the tag commit, not from a later `main` commit.
If `main` contains the experimental source but the selected tag still points to an
older commit, CI will build the older plain `0.5.77` tree.

Recommended tag:

```bash
git tag -d v0.5.77.experimental 2>/dev/null || true
git push origin :refs/tags/v0.5.77.experimental 2>/dev/null || true
git tag -a v0.5.77.experimental -m "MadModem 0.5.77.experimental"
git push origin main --tags
```

The distribution workflow now runs `scripts/ci_release_version_guard.sh` in every
job. On a tag build it strips the optional leading `v` and verifies that the tag
matches `MADMODEM_VERSION.txt`. If they do not match, the workflow fails before
building stale packages.

Note: CMake's `project(VERSION 0.5.77)` must remain numeric. The full package and
application identity is `MADMODEM_PACKAGE_VERSION`, read from `MADMODEM_VERSION.txt`.
