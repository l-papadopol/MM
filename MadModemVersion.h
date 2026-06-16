#pragma once

// Public application version.
//
// MadModem uses Semantic Versioning for user-visible releases:
//   MAJOR.MINOR.PATCH[-pre.release][+build.metadata]
// This production consolidation is based on the validated 0.5.0
// FT8 GF(2) OSD order-1 decoder baseline. Later beta02..beta08 decoder lab
// experiments were not promoted into this production source tree.
#define MADMODEM_VERSION_MAJOR 0
#define MADMODEM_VERSION_MINOR 5
#define MADMODEM_VERSION_PATCH 0
#define MADMODEM_VERSION_PRERELEASE ""
#define MADMODEM_VERSION_STRING "0.5.0"
#define MADMODEM_VERSION_DISPLAY "MadModem 0.5.0"
#define MADMODEM_LEGACY_SNAPSHOT "0.5.0-alpha26-ft-osd-gf2-order1-full"
