# CAT / Hamlib dialog layout repair — v2.36

The CAT/Hamlib settings dialog contains a long form. Recent UI/localization changes exposed a layout problem where the CAT form could be vertically compressed until only the group frame remained visible, making the Hamlib configuration appear to have disappeared.

The form is now placed inside a bounded `QScrollArea`. This keeps the test/status area and OK/Cancel buttons stable while preserving access to the complete Hamlib configuration.

This is a UI layout-only change. Runtime CAT/Hamlib behavior is intentionally unchanged.
