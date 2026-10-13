# MadModem 0.6.1

- Releases: automatically publish installers, portable archives and SHA256 checksums after all platform jobs succeed; version tags publish official releases, branch builds publish numbered prereleases.
- CI: run the full regression suite manually; distribution builds skip regression executables and retain package/install verification.

- macOS signing: move data into Resources with compatible relative links, sign nested code before bundle executables, and use the same signing path for portable and installer packages.

- macOS portable: stage DMG contents separately, retry transient resource-busy failures, validate all bundled library paths and require a valid final ad-hoc signature.

- Windows installer verification: prevent modal version dialogs, use the native platform plugin, bound every process wait and retain diagnostic logs.

- macOS installer verification: use the bundled Cocoa platform plugin instead of forcing the absent offscreen plugin; check the installed application version.
- Linux portable: require Qt Location/Positioning at build time and add the missing Ubuntu development dependency; include Fontconfig configuration and validate font rendering.

- macOS: fix fresh PKG installs silently skipping the app; preserve portable data before replacing an existing bundle.
- Windows: install and deploy the optional MSYS2 ANGLE runtime required by windeployqt; retain strict DLL dependency checks.

- Distribution: retain standalone Windows ZIPs, Linux ZIP/TAR.GZ and macOS ZIP/DMG alongside the optional installers. Publish portable and installer downloads separately.
- Windows menu verification: add a native-platform startup/maximize/restore regression to CI alongside offscreen tests; retain the earlier fix that removed forced fullscreen startup.

- Windows: one installer automatically selects the compatible legacy or AVX2/FMA build, creates shortcuts and supports installation updates and uninstall.
- Linux portable: bundle Qt5, Hamlib, Qt audio/display plugins, QML map modules, OpenSSL and their dependencies. Separate Qt/Hamlib installation is no longer required.
- Linux packaging: check runtime dependency closure and verify relocated archives in clean distribution containers without system Qt or Hamlib.
- Linux: distribution-specific installers for Ubuntu 24.04 and Fedora 43 use native package management and dependency resolution.
- macOS: native PKG installers for Intel and Apple Silicon install to Applications.
- Installers display the author's copyright and the unmodified GNU GPLv3; interactive installation requires acceptance.
- Managed installations store preferences and the default logbook in user folders. Existing user files are never replaced by portable-data migration.
- UI: added edge and corner resizing to custom window frames, including the main window, settings and runtime dialogs. Native-frame windows retain native resizing.
- UI: message dialogs no longer snap back to a fixed size after resizing. Embedded settings pages and popup menus retain their normal interaction.
- SSTV editor: removed the oversized window minimum and made the tool area scrollable.
- Sources: removed obsolete verification folders, per-revision reports and outdated release documents; retained automated tests and third-party licensing.
- Updated application metadata, Windows resources, translations, localized help, README and build documentation to 0.6.1.
- Retained the CW acquisition, CAT/PTT, narrow-band waterfall, FT scheduling, RTTY, logbook and contest-export improvements from the preceding release.

Installer production includes native build and package/installation/reinstallation checks on GitHub. The full regression suite is available through the separate manual workflow. Windows/macOS packages remain unsigned; no automatic update-download service is included.
