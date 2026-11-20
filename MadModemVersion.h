#pragma once

// Public application version for the 0.6.5 release.
// CMake keeps a numeric project version while this header is the source of
// truth for the in-application title, package metadata and release guard.
#define MADMODEM_VERSION_MAJOR 0
#define MADMODEM_VERSION_MINOR 6
#define MADMODEM_VERSION_PATCH 5
#define MADMODEM_VERSION_PRERELEASE ""
#define MADMODEM_VERSION_STRING "0.6.5"
#define MADMODEM_VERSION_DISPLAY "MadModem 0.6.5"
#define MADMODEM_LEGACY_SNAPSHOT "0.5.0-alpha26-ft-osd-gf2-order1-full"

// Edition of this source tree, shown after the version in the title and the
// About box. Empty in the original MadModem by IZ6NNH.
#define MADMODEM_EDITION "IC8TEM"
// GitHub repository (owner/name) whose releases the update check offers.
#define MADMODEM_UPDATE_REPOSITORY "ic8tem/MM"
