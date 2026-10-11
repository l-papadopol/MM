# macOS builds — MadModem 0.6.1

Intel and Apple Silicon jobs are part of the shared
[Build installers workflow](GITHUB_ACTIONS_DISTRIBUTION.md). They build with Qt6
and Homebrew Hamlib, run CTest, deploy the required frameworks and create a native
PKG with GNU GPLv3 acceptance. The current build target is macOS 15 or later.

For a local build on macOS:

```bash
brew install cmake ninja pkg-config qt qtlocation hamlib
export MACOSX_DEPLOYMENT_TARGET=15.0
bash scripts/build_macos.sh
bash scripts/package_macos.sh
bash scripts/package_macos_installer.sh
```

The package scripts validate required runtime files and use ad-hoc signing.
Developer ID signing and notarization require the author's Apple credentials and
are not configured. See [installation instructions](INSTALLERS.md).
