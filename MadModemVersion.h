#pragma once

// Public application version.
//
// MadModem uses Semantic Versioning for user-visible releases:
//   MAJOR.MINOR.PATCH[-pre.release][+build.metadata]
// 0.5.76d moves MSK144 period decode off the UI path, removes transient MSK
// waterfall labels, and cleans duplicate MSK144/Q65 mode-panel buttons.
#define MADMODEM_VERSION_MAJOR 0
#define MADMODEM_VERSION_MINOR 5
#define MADMODEM_VERSION_PATCH 76
#define MADMODEM_VERSION_PRERELEASE "d"
#define MADMODEM_VERSION_STRING "0.5.76d"
#define MADMODEM_VERSION_DISPLAY "MadModem 0.5.76d"
#define MADMODEM_LEGACY_SNAPSHOT "0.5.0-alpha26-ft-osd-gf2-order1-full"
