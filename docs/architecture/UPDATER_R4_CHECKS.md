# Promoted build updater — source R4

The changed UpdateController.cpp was compiled with GCC and Qt 5.15 on Linux.
A small executable called the actual newerStableVersion method: 31 checks passed.

- v0.6.3, 0.6.3 and build-0.6.3-111 are recognized from a 0.6.2 client.
- All three formats reject drafts, prereleases, equal versions and downgrades.
- Changing the title does not change the result.
- 0.6.10 compares newer than 0.6.9.
- build-0.6.4-1 is newer than 0.6.3; build-0.6.3-999 is equal to 0.6.3.
- Empty, arbitrary, incomplete, nonnumeric, release-candidate, extended and
  newline-suffixed tags are rejected.

The existing release guard suite passed all 7 checks: macOS portability, operator
copy, localization, documentation, RTTY rules, CW rules and version consistency.
Help text and its generator were updated together for all six languages.

Validation scope: version discovery and compilation of the changed translation
unit. No new full application build, native Windows/macOS build, remote release
mutation or end-to-end installation was performed for R4. Download, checksum,
package matching and installation code are byte-identical to R3. CW engines,
FT clock layout, packaging workflow and Hamlib source are unchanged from R3.
