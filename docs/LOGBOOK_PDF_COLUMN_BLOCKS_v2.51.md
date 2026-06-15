# Logbook PDF column blocks — v2.51

The previous print/PDF output used a single fixed-width HTML table for every selected ADIF field. With many visible fields this produced unreadable one-character-wide columns.

v2.51 changes the export strategy: if the selected fields exceed the page width, MadModem splits them into consecutive field blocks. Each block is a normal landscape table and contains the same QSO rows. UTC and CALL are repeated in each block when selected so the rows remain identifiable.

Hidden fields are still preserved by the ADIF backend; the field selection dialog controls only table visibility, CSV/PDF/print output and copy/export views.
