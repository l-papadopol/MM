# MadModem 0.5.77.experimental - shutdown/fullscreen/popup regression fix

This patch repairs a regression introduced by the Windows popup hardening pass.

Changes:

- Removed the invalid global `disconnect(nullptr, nullptr, this, nullptr)` call from shutdown.
  Qt printed `QObject::disconnect: Unexpected nullptr parameter` and the call did not do
  what the shutdown path intended.
- Main-window close now explicitly accepts the event and posts `QCoreApplication::quit()`
  after `shutdownRuntime()` completes.  This avoids a hidden popup/dialog helper keeping
  the Qt event loop alive after the runtime has already stopped.
- Main startup now enforces fullscreen immediately and again after the first event-loop
  turn.  This repairs cases where the custom cockpit frame is shown normal before the
  compositor applies fullscreen.
- The popup hardening pass no longer rewrites every `Qt::Popup` globally. Combo-box
  item views are styled but remain inside Qt's private popup container; the real
  container is raised safely after it is shown, without assigning on-top flags to the view.

Decoder code is untouched in this patch.
