# CAT / Rig control dialog layout fix — v2.44

The CAT/Hamlib controls were not missing. They were being compressed vertically by the dialog layout when the window opened with insufficient height. This made labels and comboboxes look like a blank or striped panel.

The fix is layout-only:

- the CAT/Hamlib form is placed in a dedicated `QScrollArea`;
- the form group has a real minimum height;
- the dialog opens taller and has a larger minimum size;
- the scroll area can scroll instead of compressing rows.

The backend and settings logic are unchanged.
