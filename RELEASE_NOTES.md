# MadModem 0.6.1

- Distribution: retain standalone Windows ZIPs, Linux ZIP/TAR.GZ and macOS ZIP/DMG alongside the optional installers. Publish portable and installer downloads separately.
- Windows menu verification: add a native-platform startup/maximize/restore regression to CI alongside offscreen tests; retain the earlier fix that removed forced fullscreen startup.

- Windows: one installer automatically selects the compatible legacy or AVX2/FMA build, creates shortcuts and supports installation updates and uninstall.
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

Installer production includes native build, CTest and installation/reinstallation checks on GitHub. Windows/macOS packages remain unsigned; no automatic update-download service is included.
