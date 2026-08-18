#pragma once

#include <QColor>
#include <QString>

class QApplication;
class QMainWindow;
class QWidget;

namespace MadModemUi {

enum class ThemeColorRole {
    Accent,
    AccentText,
    Positive,
    Negative,
    Warning,
    MutedText,
    RxPrimary,
    RxSecondary,
    MapBackdrop,
    MapText,
    MapMutedText,
    MapBorder
};

QString normalizedThemeKey(const QString &themeKey);
QString activeThemeKey();
QColor themeColor(ThemeColorRole role);
void applyUiTheme(QApplication &app, const QString &themeKey);
void setSemanticRole(QWidget *widget, const QString &role);

void applyCockpitTheme(QApplication &app);
void installCockpitMainWindowChrome(QMainWindow *window);
void polishCockpitWidgetTree(QWidget *root);

} // namespace MadModemUi
