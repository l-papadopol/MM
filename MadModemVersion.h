#pragma once

// Public application version.
// 0.5.77.experimental is the experimental GitHub release line for the Radio Telescope prototype.
// It is intentionally separated from the normal lab numbering so GitHub releases can publish it
// without promoting the 0.5.79-labXX development branch as stable.
//
// MadModem uses Semantic Versioning for user-visible releases:
//   MAJOR.MINOR.PATCH[-pre.release][+build.metadata]
// This build includes the Radio Telescope receive-only sky/RFI heatmap work from the lab18-lab24 series:
// - rotator-required RX guard for real sky scans;
// - true honeycomb heatmap rendering;
// - rotator-profile antenna beam width;
// - CSV export and side-panel sample table;
// - rotator auto-peak integration hooks.
#define MADMODEM_VERSION_MAJOR 0
#define MADMODEM_VERSION_MINOR 5
#define MADMODEM_VERSION_PATCH 77
#define MADMODEM_VERSION_PRERELEASE "experimental"
#define MADMODEM_VERSION_STRING "0.5.77.experimental"
#define MADMODEM_VERSION_DISPLAY "MadModem 0.5.77.experimental"
#define MADMODEM_LEGACY_SNAPSHOT "0.5.0-alpha26-ft-osd-gf2-order1-full"
