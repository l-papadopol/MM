#!/usr/bin/env python3
"""Source and WCAG contrast guard for all MadModem UI themes."""

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
THEME = (ROOT / "utils/CockpitTheme.cpp").read_text(encoding="utf-8")
THEME_H = (ROOT / "utils/CockpitTheme.h").read_text(encoding="utf-8")
MAIN = (ROOT / "main.cpp").read_text(encoding="utf-8")
WINDOW = (ROOT / "mainwindow.cpp").read_text(encoding="utf-8")
APP_SETTINGS = (ROOT / "dialogs/AppSettingsDialog.cpp").read_text(encoding="utf-8")
LOGBOOK = (ROOT / "dialogs/LogbookDialog.cpp").read_text(encoding="utf-8")
MAP = (ROOT / "widgets/QsoMapWidget.cpp").read_text(encoding="utf-8")
WATERFALL = (ROOT / "widgets/WaterfallWidget.cpp").read_text(encoding="utf-8")
UI = (ROOT / "mainwindow.ui").read_text(encoding="utf-8")

THEMES = {
    "avionica": {
        "window": "#070707", "window_text": "#FFB13E", "base": "#020202",
        "button": "#101010", "button_text": "#FFB240",
        "highlight": "#FF9416", "highlighted_text": "#050505",
        "positive": "#53FF70", "negative": "#FF6262", "warning": "#FFC35C",
        "muted": "#9D8C78", "map_backdrop": "#050607", "map_text": "#FFB35A",
        "map_muted": "#8394A4",
    },
    "qt_default": {
        "window": "#F0F0F0", "window_text": "#202020", "base": "#FFFFFF",
        "button": "#E7E7E7", "button_text": "#202020",
        "highlight": "#2B579A", "highlighted_text": "#FFFFFF",
        "positive": "#14743A", "negative": "#B4232F", "warning": "#9A5B00",
        "muted": "#5E6670", "map_backdrop": "#E8EDF2", "map_text": "#18212B",
        "map_muted": "#556270",
    },
    "hacker_green": {
        "window": "#001008", "window_text": "#71FF91", "base": "#000603",
        "button": "#002313", "button_text": "#83FFA0",
        "highlight": "#2BEA65", "highlighted_text": "#001006",
        "positive": "#45F47A", "negative": "#FF6675", "warning": "#FFD45C",
        "muted": "#68A879", "map_backdrop": "#00150B", "map_text": "#9DFFB2",
        "map_muted": "#68A879",
    },
    "classic_dark": {
        "window": "#24272D", "window_text": "#E8EBEF", "base": "#15171B",
        "button": "#343840", "button_text": "#F0F2F5",
        "highlight": "#6FA8FF", "highlighted_text": "#0E1622",
        "positive": "#61D68A", "negative": "#FF737D", "warning": "#FFC45C",
        "muted": "#A9B0BA", "map_backdrop": "#1C222A", "map_text": "#EDF3F8",
        "map_muted": "#A9B0BA",
    },
    "high_contrast": {
        "window": "#000000", "window_text": "#FFFFFF", "base": "#000000",
        "button": "#090909", "button_text": "#FFFFFF",
        "highlight": "#FFD800", "highlighted_text": "#000000",
        "positive": "#57FF76", "negative": "#FF6B78", "warning": "#FFD800",
        "muted": "#D0D0D0", "map_backdrop": "#000000", "map_text": "#FFFFFF",
        "map_muted": "#D0D0D0",
    },
}


def luminance(hex_colour: str) -> float:
    channels = [int(hex_colour[i:i + 2], 16) / 255.0 for i in (1, 3, 5)]
    linear = [c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
              for c in channels]
    return 0.2126 * linear[0] + 0.7152 * linear[1] + 0.0722 * linear[2]


def contrast(a: str, b: str) -> float:
    la, lb = luminance(a), luminance(b)
    return (max(la, lb) + 0.05) / (min(la, lb) + 0.05)


for key, colours in THEMES.items():
    assert f'QStringLiteral("{key}")' in THEME, key
    checks = {
        "window text": contrast(colours["window_text"], colours["window"]),
        "editor text": contrast(colours["window_text"], colours["base"]),
        "button text": contrast(colours["button_text"], colours["button"]),
        "selection text": contrast(colours["highlighted_text"], colours["highlight"]),
        "positive state": contrast(colours["positive"], colours["window"]),
        "negative state": contrast(colours["negative"], colours["window"]),
        "warning state": contrast(colours["warning"], colours["window"]),
        "muted text": contrast(colours["muted"], colours["window"]),
        "map text": contrast(colours["map_text"], colours["map_backdrop"]),
        "map muted text": contrast(colours["map_muted"], colours["map_backdrop"]),
    }
    for surface, ratio in checks.items():
        assert ratio >= 4.5, f"{key} {surface}: contrast {ratio:.2f} < 4.5"
        print(f"PASS contrast {key} {surface}: {ratio:.2f}:1")

# The generic stylesheet must replace every named colour token.
generic = THEME[THEME.index("QString genericThemeStyleSheet"):THEME.index("class CockpitTitleBar")]
raw_part, replacement_part = generic.split("const QMap<QString, QString> replacements", 1)
raw_tokens = set(re.findall(r"@[A-Z_]+@", raw_part))
replacement_tokens = set(re.findall(r'QStringLiteral\("(@[A-Z_]+@)"\)', replacement_part))
assert raw_tokens == replacement_tokens, (raw_tokens - replacement_tokens, replacement_tokens - raw_tokens)

for role in ("Accent", "Positive", "Negative", "Warning", "MutedText",
             "RxPrimary", "RxSecondary", "MapBackdrop", "MapText", "MapBorder"):
    assert role in THEME_H and f"ThemeColorRole::{role}" in THEME, role

assert "MadModemUi::applyUiTheme(app, bootTheme);" in MAIN
assert "MadModemUi::installCockpitMainWindowChrome(&window);" in MAIN
assert "if (bootTheme != QStringLiteral(\"qt_default\"))" not in MAIN
assert 'g_activeThemeKey != QStringLiteral("avionica")' in THEME
assert 'QStringLiteral("3px")' not in THEME[THEME.index("QString genericThemeStyleSheet"):THEME.index("class CockpitTitleBar")]

# Theme-independent popup surfaces and logbook chrome may not retain Avionica
# colours. LEDs are intentionally allowed to use physical status colours.
assert "background:#121212" not in APP_SETTINGS
assert "color:#ffb347" not in APP_SETTINGS.lower()
assert "3px double" not in LOGBOOK
assert "#ffb343" not in LOGBOOK.lower()
assert "background-color: #ffffff" not in WINDOW.lower()
assert "background-color: #050607" not in WINDOW.lower()
assert "background-color: rgb(18, 18, 18)" not in UI.lower()
assert "background-color: rgb(4, 6, 8)" not in UI.lower()

for role in ("MapBackdrop", "MapText", "MapMutedText", "MapBorder"):
    assert f"ThemeColorRole::{role}" in MAP, role
assert "background:palette(window)" in WATERFALL
assert "background:palette(highlight)" in WATERFALL

# FT/CW semantic states must be recoloured by the active theme, not by local
# orange/green/blue literals that become unreadable on Qt Classic.
for marker in ('mmRole="positive"', 'mmRole="negative"', 'mmRole="warning"',
               'mmRole="muted"', 'mmRole="rxPrimary"', 'mmRole="rxSecondary"',
               'ftBannerState="idle"', 'ftBannerState="tx"'):
    assert marker in THEME, marker
assert "m_lblFt8TxBanner->setStyleSheet(style)" not in WINDOW

print("UI theme integrity audit: PASS (5/5 complete themes)")
