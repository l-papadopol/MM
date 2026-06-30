#pragma once

// Public application version.
//
// MadModem uses Semantic Versioning for user-visible releases:
//   MAJOR.MINOR.PATCH[-pre.release][+build.metadata]
// 0.5.76s changes only GitHub release packaging: Windows uses MXE static
// legacy+AVX2 packages from GitHub, macOS verifies bundled Qt frameworks, and
// Linux emits one tar.gz. It does not change modem/DSP/CAT runtime logic.
#define MADMODEM_VERSION_MAJOR 0
#define MADMODEM_VERSION_MINOR 5
#define MADMODEM_VERSION_PATCH 76
#define MADMODEM_VERSION_PRERELEASE "s"
#define MADMODEM_VERSION_STRING "0.5.76s"
#define MADMODEM_VERSION_DISPLAY "MadModem 0.5.76s"
#define MADMODEM_LEGACY_SNAPSHOT "0.5.0-alpha26-ft-osd-gf2-order1-full"
