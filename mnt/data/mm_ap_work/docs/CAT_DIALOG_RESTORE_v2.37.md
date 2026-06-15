# CAT / Rig control dialog restore — v2.37

The v2.36 CAT dialog repair attempted to wrap the Hamlib form in a scrollable panel. On the user's Linux test this caused the Hamlib configuration area to render as a large empty rectangle, while only the test buttons/log area remained visible.

For v2.37, the CAT/Rig dialog was restored from the known-good older source supplied by the user:

- `dialogs/RigControlSettingsDialog.cpp`
- `dialogs/RigControlSettingsDialog.h`

Only this dialog was restored. The rest of the project remains based on the latest line, including ADIF full compatibility.
