#include "mainwindow.h"
#include "callsign/QrzLookupPanel.h"
#include <QApplication>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <iostream>
#include <stdexcept>

static void check(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
int main(int argc, char **argv)
{
    QApplication app(argc, argv); app.setApplicationName("MadModem");
    QSettings settings(AppSettings::settingsFilePath(), QSettings::IniFormat);
    settings.setValue("Updates/checkAtStartup", false); settings.sync();
    MainWindow window;
    check(!window.m_qrzLookupPanel->findChild<QPushButton *>("qrzAccountButton"), "QRZ account button remained in main UI");
    check(window.m_qrzLookupPanel->m_details != nullptr, "station details removed");
    AppSettings initial;
    initial.qrzUsername = "fixture-user"; initial.qrzPassword = "fixture-password-only";
    AppSettingsDialog dialog(initial, {}, {}, {}, {}, {}, AppSettingsDialog::InitialPage::QrzAccount);
    auto *username = dialog.findChild<QLineEdit *>("qrzUsername");
    auto *password = dialog.findChild<QLineEdit *>("qrzPassword");
    check(username && password && password->echoMode() == QLineEdit::Password, "masked QRZ settings missing");
    dialog.show(); app.processEvents(); dialog.grab().save("qrz-settings.png");
    check(username->text() == initial.qrzUsername && password->text() == initial.qrzPassword, "saved credentials not loaded into UI");
    username->setText(" next-user "); password->setText(" next-password ");
    dialog.setInitialPage(AppSettingsDialog::InitialPage::FtDecoder);
    auto *enabled = dialog.findChild<QCheckBox *>("ftDriftRxEnabled");
    auto *limit = dialog.findChild<QDoubleSpinBox *>("ftDriftMaxHzPerSec");
    check(enabled && limit && !enabled->isChecked() && !limit->isEnabled(), "drift default must be off");
    app.processEvents(); dialog.grab().save("ft-drift-settings.png");
    enabled->setChecked(true); limit->setValue(3.25);
    dialog.accept(); const AppSettings saved = dialog.settings();
    check(saved.qrzUsername == "next-user" && saved.qrzPassword == " next-password ", "credentials not collected verbatim");
    check(saved.ftDriftRxEnabled && saved.ftDriftMaxHzPerSec == 3.25, "drift settings not collected");
    check(saved.save(), "settings write failed");
    AppSettings loaded; loaded.load();
    check(loaded.qrzUsername == saved.qrzUsername && loaded.qrzPassword == saved.qrzPassword, "credentials did not survive profile reload");
    check(loaded.ftDriftRxEnabled && loaded.ftDriftMaxHzPerSec == 3.25, "drift settings did not survive reload");
    QrzLookupPanel restarted;
    check(restarted.m_username == saved.qrzUsername && restarted.m_password == saved.qrzPassword, "QRZ panel did not restore credentials after restart");
    AppSettingsDialog cancelDialog(loaded, {}, {}, {}, {}, {}, AppSettingsDialog::InitialPage::QrzAccount);
    cancelDialog.findChild<QLineEdit *>("qrzPassword")->setText("discard-this"); cancelDialog.reject();
    check(cancelDialog.settings().qrzPassword == saved.qrzPassword, "Cancel changed credentials");
    AppSettingsDialog other(loaded, {}, {}, {}, {}, {}, AppSettingsDialog::InitialPage::FtDecoder);
    other.accept(); check(other.settings().qrzPassword == saved.qrzPassword, "deferred QRZ page erased credentials");
    restarted.m_key="old-key"; QrzResponse cached; cached.valid=true; restarted.m_cache.insert("G0JEI", {cached,QDateTime::currentDateTimeUtc()});
    restarted.setAccountCredentials("new-account", "new-password");
    check(restarted.m_key.isEmpty() && restarted.m_cache.isEmpty(), "account change retained old session/cache");
    loaded.qrzPassword.clear(); check(loaded.save(), "credential deletion failed");
    AppSettings cleared; cleared.load(); check(cleared.qrzPassword.isEmpty(), "password remained after clearing");
    std::cout << "QRZ settings/persistence/restart/Cancel/deferred-page/account-change; FT drift UI PASS\n";
}
