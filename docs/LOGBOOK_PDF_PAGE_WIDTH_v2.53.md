# Logbook PDF page width — v2.53

v2.53 fixes a print/PDF layout issue where the first block of selected logbook fields could occupy only the left portion of the landscape page, leaving a large blank area on the right.

The PDF/print renderer now sets the QTextDocument page size and text width from the actual QPrinter page rectangle before loading the HTML. This allows `width:100%` tables to expand to the printable area.

The field block budget is also slightly increased so each block uses more horizontal space while still avoiding the unreadable ultra-narrow columns that happen when too many ADIF fields are forced onto one table.
