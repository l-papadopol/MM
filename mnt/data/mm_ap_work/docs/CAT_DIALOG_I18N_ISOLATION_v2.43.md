# CAT dialog i18n isolation — v2.43

The CAT/Rig Control dialog contains several dynamic combo boxes populated from
Hamlib runtime data. It also builds a scrollable form programmatically.

Starting from the localization cleanup series, MainWindow applied the generic
object-tree translator to every settings dialog. That generic walker is safe for
static labels and simple controls, but it is risky for the CAT dialog because it
can traverse and rewrite dynamic combo contents and scroll-area children.

`RigControlSettingsDialog` already exposes `setTextTranslator()` and handles its
own static labels through `refreshLabels()`. Therefore v2.43 removes the generic
`applyUiLanguageToObjectTree(&dialog)` call only for this dialog.

This is a surgical UI stability change. CAT/Hamlib backend behaviour is not
modified.
