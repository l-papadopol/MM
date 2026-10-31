# MSHV 2.76.7 headless FT reference port

Original algorithms: WSJT development group; C++ rewrite and modifications by Hrisimir Hristov LZ2HV. GPL v3; COPYING.txt is included.

Source: official MSHV_2767_Full_Source_Code.zip, 2026-10-05, SHA256 2cdac0941c5e734e62983eb3782297f10cdd8373888fd09608a3c465987791e8. Only FT8/FT4 and numeric helpers are ported, not the UI or TX scheduling. Original files and their hashes are listed in the R30 audit inventory.

Changes: namespace isolation; plain callback interface instead of QObject signals; instance-owned configuration/history/POD initialization; codec calls use MadModem's TX-priority shared-codec lock; FFTW plans/buffers are instance-owned with serialized plan creation/destruction; cancellation at pass/candidate boundaries; GenFt4 AP helper compatibility uses the existing TX codec unchanged.

Upstream FT8 sync-quality indexing wrote snrsync[21] beyond its 21-element array and left gaps. Blocks now map k to k, k-29, k-58 (all 21 positions exactly once). The original error was reproduced with AddressSanitizer/UBSan; the corrected native repeated replay passed those sanitizers. This fix is in this fork only; no upstream issue or message was sent.

These files provide a recovery path. MadModem retains its existing FT receiver, transmitter, GPU paths and Doppler correction. A larger message count alone does not establish correctness or parity with another receiver.

The FT recovery target is compiled with `-ftrivial-auto-var-init=zero` for remaining upstream automatic scratch storage; configuration requires that compiler capability rather than silently removing this safeguard. This flag is scoped to the recovery target.
