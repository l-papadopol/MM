#pragma once
#include <QStyleFactory>
#include <QFont>
#include <QScrollArea>
#include <QScrollBar>
#include "../dialogs/AudioSettingsDialog.h"
#include "../dialogs/RigControlSettingsDialog.h"
inline bool runPttSettingsRegression(QApplication &app, QWidget &parent, QTextStream &out)
{
    bool ok = true;
    auto check = [&](bool value, const char *name) {
        out << (value ? "PASS " : "FAIL ") << name << "\n";
        ok &= value;
    };
    for (const QString &method : {QStringLiteral("none"), QStringLiteral("cat_hamlib"),
                                  QStringLiteral("serial_rts"), QStringLiteral("serial_dtr")}) {
        AppSettings initial;
        initial.pttMethod = method;
        initial.hamlibPttEnabled = method == QStringLiteral("cat_hamlib");
        initial.hamlibCatEnabled = false;
        initial.pttPortName = QStringLiteral("COM237"); // A disconnected saved interface.
        AppSettingsDialog dialog(initial, QString(), QString(), {}, {}, {},
                                 AppSettingsDialog::InitialPage::AudioPtt, &parent);
        dialog.show();
        app.processEvents();
        auto *audio = dialog.findChild<AudioSettingsDialog *>();
        auto *rig = dialog.findChild<RigControlSettingsDialog *>();
        auto *ptt = dialog.findChild<QComboBox *>(QStringLiteral("cmbPttMethod"));
        auto *port = dialog.findChild<QComboBox *>(QStringLiteral("cmbPttPort"));
        auto *catPort = dialog.findChild<QComboBox *>(QStringLiteral("catSerialPort"));
        auto *catPtt = dialog.findChild<QComboBox *>(QStringLiteral("catPttMethod"));
        auto *maker = dialog.findChild<QComboBox *>(QStringLiteral("catManufacturer"));
        check(audio && rig && ptt && port && catPort && catPtt && maker, "PTT controls available");
        if (!audio || !rig || !ptt || !port || !catPort || !catPtt || !maker) continue;
        check(catPort->isEnabled() && catPort->isEditable() && maker->isEnabled(),
              "CAT configuration editable with polling off");
        check(catPtt->isHidden() && catPtt->currentData().toString() == method,
              "one visible PTT method and synchronized CAT test configuration");
        check(audio->settings().pttPortName == QStringLiteral("COM237"), "retain disconnected PTT port");
        // Simulate typing over a detected item; itemData must not override edited text.
        catPort->addItem(QStringLiteral("COM11 - USB adapter"), QStringLiteral("COM11"));
        catPort->setCurrentIndex(catPort->count() - 1);
        catPort->setEditText(QStringLiteral("COM12"));
        ptt->setCurrentIndex(ptt->findData(QStringLiteral("serial_rts")));
        port->addItem(QStringLiteral("COM11 - USB adapter"), QStringLiteral("COM11"));
        port->setCurrentIndex(port->count() - 1);
        port->setEditText(QStringLiteral("COM238"));
        QMetaObject::invokeMethod(audio, "refreshDevices", Qt::DirectConnection);
        check(audio->settings().pttPortName == QStringLiteral("COM238"), "manual PTT port survives refresh");
        ptt->setCurrentIndex(ptt->findData(method));
        QMetaObject::invokeMethod(rig, "applyToSettings", Qt::DirectConnection);
        check(rig->settings().pttMethod == method && rig->settings().pttPortName == QStringLiteral("COM238"),
              "CAT tests follow the current PTT method and dedicated port");
        check((method == QStringLiteral("serial_rts") || method == QStringLiteral("serial_dtr"))
              ? port->isVisible() && port->isEnabled() : port->isHidden(), "only serial PTT displays a dedicated port");
        QMetaObject::invokeMethod(&dialog, "accept", Qt::DirectConnection);
        const AppSettings saved = dialog.settings();
        check(saved.pttMethod == method && saved.hamlibPttEnabled == (method == QStringLiteral("cat_hamlib")),
              "save preserves the unified PTT selection");
        check(saved.hamlibSerialPath == QStringLiteral("COM12") && saved.pttPortName == QStringLiteral("COM238"),
              "save preserves typed CAT and dedicated PTT ports independently");
    }
    return ok;
}

inline int runSettingsUiRegression(QApplication &app) {
        bool ok = true;
        QTextStream out(stdout);
        QWidget parent;
        parent.resize(800, 600);
        parent.show();
        ok &= runPttSettingsRegression(app, parent, out);
        AppSettings settings;
        const QFont originalFont = app.font();
        for (const QString &styleName : QStyleFactory::keys()) {
        app.setStyle(QStyleFactory::create(styleName));
        for (const double pointSize : {9.0, 14.0}) {
        QFont font = originalFont;
        font.setPointSizeF(pointSize);
        app.setFont(font);
        for (const QString &language : {QStringLiteral("en"), QStringLiteral("it"), QStringLiteral("de"),
                                       QStringLiteral("fr"), QStringLiteral("no"), QStringLiteral("cs")}) {
            MadModemI18n::setLanguageCode(language);
            AppSettingsDialog dialog(settings, QString(), QString(), {},
                [](const QString &source) { return MadModemI18n::text(source); }, [](const QString &key, const QString &fallback) { return MadModemI18n::translate(key, fallback); },
                AppSettingsDialog::InitialPage::AudioPtt, &parent);
            dialog.show();
            app.processEvents();
            const QRect available = dialog.screen()->availableGeometry();
            bool languageOk = !dialog.isFullScreen() &&
                dialog.width() <= available.width() && dialog.height() <= available.height();
            if (auto *tabs = dialog.findChild<QTabWidget *>()) {
                for (int page = 0; page < tabs->count(); ++page) {
                    tabs->setCurrentIndex(page);
                    app.processEvents();
                    for (QPushButton *button : dialog.findChildren<QPushButton *>()) {
                        if (!button->isVisible() || button->text().isEmpty()) continue;
                        if (button->width() < button->minimumSizeHint().width() ||
                            button->height() < button->minimumSizeHint().height()) {
                            out << "CLIPPED\t" << styleName << "\t" << pointSize << "pt\t" << button->width() << "x" << button->height() << " required=" << button->minimumSizeHint().width() << "x" << button->minimumSizeHint().height() << "\t" << language << "\t" << page << "\t" << button->text() << "\n";
                            languageOk = false;
                        }
                    }
                    const QString captureDir = qEnvironmentVariable("MADMODEM_UI_CAPTURE_DIR");
                    if (!captureDir.isEmpty()) {
                        QDir().mkpath(captureDir);
                        dialog.grab().save(QDir(captureDir).filePath(QStringLiteral("settings_%1_%2_%3_%4.png").arg(styleName).arg(pointSize).arg(language).arg(page)));
                        // Include controls below the fold, especially the colour buttons.
                        for (QScrollArea *area : tabs->currentWidget()->findChildren<QScrollArea *>()) {
                            area->verticalScrollBar()->setValue(area->verticalScrollBar()->maximum());
                        }
                        if (auto *area = qobject_cast<QScrollArea *>(tabs->currentWidget())) {
                            area->verticalScrollBar()->setValue(area->verticalScrollBar()->maximum());
                        }
                        app.processEvents();
                        dialog.grab().save(QDir(captureDir).filePath(QStringLiteral("settings_%1_%2_%3_%4_bottom.png").arg(styleName).arg(pointSize).arg(language).arg(page)));
                    }
                }
            } else languageOk = false;
            out << (languageOk ? "PASS" : "FAIL") << " Settings geometry/buttons " << styleName << " " << pointSize << "pt " << language << "\n";
            ok &= languageOk;
            dialog.close();
        }
        }
        }
        app.setFont(originalFont);
        return ok ? 0 : 1;
    }

