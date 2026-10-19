# MadModem 0.6.4

Source R7 checks all published GitHub releases, including pre-releases, instead
of relying on the Latest badge. It compares public versions and automatically
embedded distribution build numbers, so newer builds of the same version are
detected and the installed build is not proposed again.

The update offer identifies pre-releases and shows installed/available builds.
Download and installation require confirmation. Existing package selection,
SHA256 verification, user-data preservation and portable rollback remain active.
Portable startup verification checks the expected build as well as the version.
Automatic checks wait for idle operation if TX or automatic activity is running.

The GitHub distribution workflow embeds the same build identity on every platform
and stamps it into release metadata. Published builds may remain pre-releases;
there is no need to promote them to Latest or rename generated tags.

Install this version once manually if the old 0.6.3 checker cannot detect it.
Subsequent published builds are handled by the new checker. Drafts are excluded.

Retained: experimental FT8/FT4 RX drift recovery, persistent QRZ credentials in
Settings, main QRZ station information, CW Native R12 / Neural R13, pinned FT
clock, R5 response-window fixes and Windows UCRT64/Hamlib packaging fixes.
