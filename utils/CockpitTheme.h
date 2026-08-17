#pragma once

class QApplication;
class QMainWindow;
class QWidget;

namespace MadModemUi {

void applyCockpitTheme(QApplication &app);
void installCockpitMainWindowChrome(QMainWindow *window);
void polishCockpitWidgetTree(QWidget *root);

} // namespace MadModemUi
