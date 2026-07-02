#ifndef AUDIOSETTINGSDIALOG_H
#define AUDIOSETTINGSDIALOG_H

#include "../settings/AppSettings.h"

#include <QComboBox>
#include <QDialog>
#include <QStringList>

#include <functional>

QT_BEGIN_NAMESPACE
namespace Ui {
class AudioSettingsDialog;
}
QT_END_NAMESPACE

/**
 * @brief Dialog used to select audio and PTT devices.
 *
 * Purpose:
 * - Enumerate available audio input devices.
 * - Enumerate available audio output devices.
 * - Enumerate available serial ports for RTS PTT.
 * - Return updated persistent settings to MainWindow.
 */
class AudioSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Creates the settings dialog.
     *
     * Details:
     * - Receives the current persistent settings.
     * - Populates audio input, audio output, and PTT serial port lists.
     * - Selects previously saved devices when available.
     */
    explicit AudioSettingsDialog(const AppSettings &initialSettings,
                                 QWidget *parent = nullptr);

    /**
     * @brief Releases UI resources.
     */
    ~AudioSettingsDialog() override;

    /**
     * @brief Returns the settings selected by the user.
     */
    AppSettings settings() const;
    void setTextTranslator(std::function<QString(const QString &)> translator);

private slots:
    /**
     * @brief Refreshes all device lists.
     */
    void refreshDevices();

private:
    /**
     * @brief Adds a combo item only if its backend name is not already present.
     *
     * Details:
     * - Prevents duplicate entries.
     * - Stores the real backend name in itemData().
     */
    void addUniqueDeviceItem(QComboBox *combo,
                             QStringList &seenBackendNames,
                             const QString &displayName,
                             const QString &backendName);

    /**
     * @brief Selects a combo item by backend name.
     */
    void selectByBackendName(QComboBox *combo, const QString &backendName);

    /**
     * @brief Builds a readable label from backend audio device names.
     */
    QString friendlyAudioName(const QString &backendName) const;
    void refreshLabels();
    void updatePttSerialUi();
    QString L(const QString &source) const;

private:
    Ui::AudioSettingsDialog *ui = nullptr;
    AppSettings m_settings;
    std::function<QString(const QString &)> m_textTranslator;
};

#endif // AUDIOSETTINGSDIALOG_H
