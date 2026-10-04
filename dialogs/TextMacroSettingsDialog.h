#ifndef TEXTMACROSETTINGSDIALOG_H
#define TEXTMACROSETTINGSDIALOG_H

#include "../settings/AppSettings.h"

#include <QDialog>
#include <QList>

class QLineEdit;
class QPlainTextEdit;

/**
 * @brief Edits persistent station data and text-mode macro button templates.
 *
 * QSO-specific variables such as {CALL} and {RST} are intentionally not
 * persistent settings: they come from the live QSO strip in the active text
 * mode.
 */
class TextMacroSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Creates the dialog and initializes fields from persistent settings.
     */
    explicit TextMacroSettingsDialog(const AppSettings &settings, QWidget *parent = nullptr);

    /**
     * @brief Returns a copy of settings updated with the dialog fields.
     */
    AppSettings settings() const;

private slots:
    /**
     * @brief Commits UI values before closing with Accepted.
     */
    void accept() override;

private:
    /**
     * @brief Creates a single-line editor with a readable placeholder.
     */
    QLineEdit *makeLineEdit(const QString &placeholder = QString());

    /**
     * @brief Copies persistent settings into UI controls.
     */
    void loadFromSettings(const AppSettings &settings);

    /**
     * @brief Copies UI controls back to the working settings object.
     */
    void storeToSettings();

private:
    AppSettings m_settings;

    QLineEdit *m_editMyCallsign = nullptr;
    QLineEdit *m_editMyName = nullptr;
    QLineEdit *m_editMyQth = nullptr;
    QLineEdit *m_editMyLocator = nullptr;
    QLineEdit *m_editRig = nullptr;
    QLineEdit *m_editAntenna = nullptr;
    QLineEdit *m_editPower = nullptr;

    QList<QLineEdit *> m_macroLabelEdits;
    QList<QPlainTextEdit *> m_macroTextEdits;
};

#endif // TEXTMACROSETTINGSDIALOG_H
