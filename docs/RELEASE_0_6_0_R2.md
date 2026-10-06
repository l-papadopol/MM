# 0.6.0 R2 — contest export

Use **Logbook → File → Export Cabrillo…**. The wizard scans the complete logbook,
not just the current search results, and lists observed editions with UTC dates,
station callsign and QSO count. Review the station/category page, then inspect
the QSO table. Only checked rows are exported; invalid checked rows block saving.
The second preview tab shows the generated Cabrillo. Neither recognition nor
export writes inferred tags or edited metadata back to ADIF.

CQ WW RTTY editions use the last full September weekend and combine restarts
and internal sessions. Records explicitly tagged as another contest are excluded.
Untagged candidates require RTTY, the UTC contest weekend and recorded sent/received
CQ-zone exchanges; their inferred membership is marked for operator review.
No CQ zone is invented from country lookup. Missing fields remain errors.

Other contests use explicit contest/rule tags and observed date clusters, separated
by gaps of at least seven days. Their labels show observed dates rather than an
assumed official calendar. Generic Cabrillo exchange fields remain configurable;
contest-specific formats other than CQ WW RTTY require operator verification.

The CQ WW writer now accepts standard ADIF CQZ/MY_CQ_ZONE and STATE/MY_STATE,
and unambiguous STX_STRING/SRX_STRING exchanges, without requiring an internal
MadModem session identifier. It still rejects mixed years or station callsigns.

The wizard uses the application palette for pages and headers. The translation
harvester now distinguishes a two-argument translator call from adjacent strings
in a list, preventing bogus column translations such as Export=UTC.

Validation: dedicated catalog/export/GUI regression plus the full CTest suite;
headless palette checks and screenshots for classic_dark, qt_default and avionica.
Evidence is in verification-060-r2. Local execution is Linux/Qt5; Windows/macOS
and the operator's actual ADIF log have not been tested here.

CQ WW calendar and file-format references:
- https://cqwwrtty.com/
- https://cqwwrtty.com/cabrillo.htm
