#include "RxNoiseBlankerMenu.h"

/*
 * Decode menu entries for the RX noise blanker and the MSK144 carrier notch (IC8TEM).
 * Copyright (C) 2026 Costantino Cerrotta, IC8TEM. GNU GPL v3 or later.
 */

#include "../dsp/common/RxNoiseBlanker.h"
#include "../settings/AppSettings.h"
#include "../utils/RuntimeI18n.h"

#include <QAction>
#include <QActionGroup>
#include <QMenu>
#include <QSettings>

namespace MadModemNoise {
namespace {

QString uiText(const char *key, const char *fallback)
{
    return MadModemI18n::translate(QString::fromUtf8(key), QString::fromUtf8(fallback));
}

void save(const Settings &s)
{
    QSettings settings(AppSettings::settingsFilePath(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("NoiseBlanker/mode"), static_cast<int>(s.mode));
    settings.setValue(QStringLiteral("NoiseBlanker/level"), static_cast<int>(s.level));
    settings.setValue(QStringLiteral("NoiseBlanker/mskCarrierNotch"), s.mskCarrierNotch);
}

QString describe(const Settings &s)
{
    QString text;
    switch (s.mode) {
    case BlankerMode::Nb2: text = QStringLiteral("NB2 (NR0V)"); break;
    case BlankerMode::Auto: text = modeName(s.mode); break;
    case BlankerMode::Off: text = QStringLiteral("off"); break;
    }
    if (s.mode != BlankerMode::Off) {
        const char *level = s.level == BlankerLevel::Sensitive ? "sensitive"
                          : s.level == BlankerLevel::StrongOnly ? "strong impulses only" : "normal";
        text += QStringLiteral(", threshold %1").arg(QString::fromLatin1(level));
    }
    text += s.mskCarrierNotch ? QStringLiteral("; MSK144 carrier notch on") : QStringLiteral("; MSK144 carrier notch off");
    return text;
}

} // namespace

void loadSettings()
{
    QSettings settings(AppSettings::settingsFilePath(), QSettings::IniFormat);
    Settings s;
    const int mode = settings.value(QStringLiteral("NoiseBlanker/mode"), 0).toInt();
    constexpr int legacyLpcMode = 2;
    if (mode == static_cast<int>(BlankerMode::Auto)) {
        s.mode = BlankerMode::Auto;
    } else if (mode == static_cast<int>(BlankerMode::Nb2) || mode == legacyLpcMode) {
        s.mode = BlankerMode::Nb2;
    }
    s.level = static_cast<BlankerLevel>(qBound(0, settings.value(QStringLiteral("NoiseBlanker/level"), 1).toInt(), 2));
    s.mskCarrierNotch = settings.value(QStringLiteral("NoiseBlanker/mskCarrierNotch"), false).toBool();
    if (mode == legacyLpcMode) {
        s.level = BlankerLevel::Normal;
        save(s);
    }
    setSettings(s);
}

void installMenu(QMenu *decodeMenu, const std::function<void(const QString &)> &log)
{
    if (decodeMenu == nullptr) {
        return;
    }
    decodeMenu->addSeparator();
    QMenu *menu = decodeMenu->addMenu(uiText("nb_menu", "Noise Blanker"));
    const QString help = uiText("nb_help", "Reduce short impulsive noise before decoding FT8, FT4, CW, RTTY and other digital modes. CW and RTTY skimmers also use the filtered audio. Adds 48 ms of audio delay, compensated in decode timestamps.");
    menu->setToolTipsVisible(true);
    menu->menuAction()->setToolTip(help);
    menu->menuAction()->setStatusTip(help);

    auto *modes = new QActionGroup(menu);
    modes->setExclusive(true);
    const Settings current = settings();
    auto addMode = [&](BlankerMode mode, const QString &text, const QString &tip) {
        QAction *action = menu->addAction(text);
        action->setCheckable(true);
        action->setChecked(current.mode == mode);
        action->setToolTip(tip);
        action->setStatusTip(tip);
        modes->addAction(action);
        QObject::connect(action, &QAction::triggered, menu, [mode, log]() {
            Settings s = settings();
            s.mode = mode;
            setSettings(s);
            save(s);
            if (log) {
                log(QStringLiteral("Noise blanker: %1.").arg(describe(s)));
            }
        });
    };
    addMode(BlankerMode::Off, uiText("nb_off", "Off"), uiText("nb_off_tip", "No processing, no delay."));
    addMode(BlankerMode::Auto, uiText("nb_auto", "NB2 Auto"),
            uiText("nb_auto_tip", "Run NB2 only when the audio contains a possible impulse. The audio delay stays constant."));
    addMode(BlankerMode::Nb2, uiText("nb_nb2", "NB2 (NR0V)"),
            uiText("nb_nb2_tip", "Impulses found on the signal envelope and blanked with soft edges, like the NB2 noise blanker of WDSP by NR0V."));

    menu->addSeparator();
    QMenu *levelMenu = menu->addMenu(uiText("nb_threshold", "Threshold"));
    auto *levels = new QActionGroup(levelMenu);
    levels->setExclusive(true);
    auto addLevel = [&](BlankerLevel level, const QString &text) {
        QAction *action = levelMenu->addAction(text);
        action->setCheckable(true);
        action->setChecked(current.level == level);
        levels->addAction(action);
        QObject::connect(action, &QAction::triggered, levelMenu, [level, log]() {
            Settings s = settings();
            s.level = level;
            setSettings(s);
            save(s);
            if (log) {
                log(QStringLiteral("Noise blanker: %1.").arg(describe(s)));
            }
        });
    };
    addLevel(BlankerLevel::Sensitive, uiText("nb_level_sensitive", "Sensitive (weaker impulses too)"));
    addLevel(BlankerLevel::Normal, uiText("nb_level_normal", "Normal (recommended)"));
    addLevel(BlankerLevel::StrongOnly, uiText("nb_level_strong", "Strong impulses only"));

    menu->addSeparator();
    QAction *count = menu->addAction(QString());
    count->setEnabled(false);
    QObject::connect(menu, &QMenu::aboutToShow, menu, [count]() {
        count->setText(uiText("nb_count", "Impulses blanked since start: %1").arg(static_cast<qulonglong>(totalEvents())));
    });

    QAction *zap = decodeMenu->addAction(uiText("nb_msk_zap", "MSK144 Carrier Notch (ZAP)"));
    zap->setCheckable(true);
    zap->setChecked(current.mskCarrierNotch);
    const QString zapHelp = uiText("nb_msk_zap_tip", "MSK144: steady carriers and birdies are removed with 10 Hz notches, found on the last 5 s of audio. A continuous MSK signal is never notched.");
    zap->setToolTip(zapHelp);
    zap->setStatusTip(zapHelp);
    decodeMenu->setToolTipsVisible(true);
    QObject::connect(zap, &QAction::toggled, decodeMenu, [log](bool on) {
        Settings s = settings();
        s.mskCarrierNotch = on;
        setSettings(s);
        save(s);
        if (log) {
            log(QStringLiteral("Noise blanker: %1.").arg(describe(s)));
        }
    });
}

} // namespace MadModemNoise
