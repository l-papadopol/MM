# MadModem changelog

## 0.6.1 — 2026-10-11

- macOS: fix fresh PKG installs silently skipping the app; preserve portable data before replacing an existing bundle.
- Windows: install and deploy the optional MSYS2 ANGLE runtime required by windeployqt; retain strict DLL dependency checks.

- Bundle Linux Qt5/Hamlib runtime, plugins, QML and TLS dependencies, with isolated-container validation and license provenance.
- Publish portable archives alongside installers on all platforms; keep installation optional.
- Run the startup menu regression with the native Windows backend as well as offscreen.

- Add automated native installer production for Windows, Ubuntu, Fedora and macOS.
- Select Windows CPU variant automatically, including OS support for AVX register state.
- Preserve managed-install settings and logbooks in writable user locations; retain originals during portable-data migration.
- Include author copyright and GNU GPLv3 acceptance in interactive installers.
- Add shared window-edge resizing, preserve menus and embedded pages, and remove the SSTV editor's oversized minimum.
- Clean obsolete release reports and verification artifacts from the source distribution.
- Update version metadata, help and GitHub homepage.

The source distribution contains the current changelog and release notes. Earlier per-revision reports and generated test logs are no longer shipped.
