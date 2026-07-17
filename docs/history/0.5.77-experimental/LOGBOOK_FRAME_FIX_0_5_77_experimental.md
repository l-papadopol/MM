# MadModem 0.5.77.experimental - Logbook frame continuity fix

The Logbook dialog previously installed its `QMenuBar` through
`QLayout::setMenuBar()`.  On the cockpit/dark theme this made the menu bar
occupy a top strip outside the regular layout margins, covering the outer
border above `File / Edit / Search / Tools` and making the floating-window frame
look visually interrupted.

The menu bar is now added as a normal child widget inside the dialog layout.
It respects the same inset as the toolbar and search panel, and the Logbook
outer border remains continuous around the whole floating dialog.
