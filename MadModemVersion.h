#pragma once

// Public application version.
//
// MadModem uses Semantic Versioning for user-visible releases:
//   MAJOR.MINOR.PATCH[-pre.release][+build.metadata]
// 0.5.76z is a Windows standalone runtime-DLL closure hotfix over 0.5.76w.
// It does not change modem/DSP/CAT runtime logic.
#define MADMODEM_VERSION_MAJOR 0
#define MADMODEM_VERSION_MINOR 5
#define MADMODEM_VERSION_PATCH 76
#define MADMODEM_VERSION_PRERELEASE "z"
#define MADMODEM_VERSION_STRING "0.5.76z"
#define MADMODEM_VERSION_DISPLAY "MadModem 0.5.76z"
#define MADMODEM_LEGACY_SNAPSHOT "0.5.0-alpha26-ft-osd-gf2-order1-full"
