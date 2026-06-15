# CAT / Rig control compact layout — v2.45

The v2.44 fix correctly identified the root cause: the CAT/Hamlib controls were present but vertically compressed.
However, making the dialog very tall is not the right UI solution.

This revision rationalizes the form:

- radio/model settings and connection/PTT settings are placed in two logical columns;
- the dialog starts smaller and has a lower minimum size;
- a scroll area remains only as protection for small displays or unusual font scaling;
- no backend CAT/Hamlib behavior is changed.
