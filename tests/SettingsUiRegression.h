#pragma once
inline int runSettingsUiRegression(QApplication &app) {
        bool ok = true;
        QTextStream out(stdout);
        QWidget parent;
        parent.resize(800, 600);
        parent.show();
        AppSettings settings;
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
                            out << "CLIPPED\t" << language << "\t" << page << "\t" << button->text() << "\n";
                            languageOk = false;
                        }
                    }
                    const QString captureDir = qEnvironmentVariable("MADMODEM_UI_CAPTURE_DIR");
                    if (!captureDir.isEmpty()) {
                        QDir().mkpath(captureDir);
                        dialog.grab().save(QDir(captureDir).filePath(QStringLiteral("settings_%1_%2.png").arg(language).arg(page)));
                    }
                }
            } else languageOk = false;
            out << (languageOk ? "PASS" : "FAIL") << " Settings geometry/buttons " << language << "\n";
            ok &= languageOk;
            dialog.close();
        }
        return ok ? 0 : 1;
    }

