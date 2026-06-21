#pragma once

// Public application version.
//
// MadModem uses Semantic Versioning for user-visible releases:
//   MAJOR.MINOR.PATCH[-pre.release][+build.metadata]
// This 0.5.27 line keeps the fldigi text-mode core pass and adds explicit
// TX/PTT preflight popups so missing User/QTH, CAT/Hamlib or serial PTT
// configuration can never fail silently.
#define MADMODEM_VERSION_MAJOR 0
#define MADMODEM_VERSION_MINOR 5
#define MADMODEM_VERSION_PATCH 27
#define MADMODEM_VERSION_PRERELEASE ""
#define MADMODEM_VERSION_STRING "0.5.27"
#define MADMODEM_VERSION_DISPLAY "MadModem 0.5.27"
#define MADMODEM_LEGACY_SNAPSHOT "0.5.0-alpha26-ft-osd-gf2-order1-full"
