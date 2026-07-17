# Windows QComboBox dropdown fix — 0.5.77.experimental

## Reported symptom

On Windows, clicking a selector such as MeteoFax **Station** did not display the dropdown list. The mouse wheel still changed the selected item, proving that the model and entries were present. Linux was unaffected.

## Root cause

The Windows popup hardening code called `setWindowFlags(Qt::Popup | ...)` on `QComboBox::view()` itself. That item view belongs inside Qt's private combo popup container. Promoting the child view to a separate top-level popup breaks the private parent/geometry relationship on Windows, so the list is not shown.

## Correction

- The combo item view is no longer assigned top-level window flags.
- Palette and item styling remain explicit and high-contrast.
- A lightweight event filter raises the actual private popup container after it is shown, without changing ownership or window flags.
- The duplicate Settings-page popup code was corrected in the same way.
- Decoder, CAT, rotator, scheduler and selection logic are unchanged.
