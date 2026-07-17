# Windows QComboBox popup rendering fix — 0.5.77.experimental

This earlier patch made combo popup colors explicit on Windows so text remains readable with the cockpit theme.

The final correction keeps those explicit palette and item colors but does **not** assign top-level window flags to `QComboBox::view()`. The view must remain inside Qt's private popup container. The container is raised safely after it becomes visible.
