#ifndef APPSETTINGSDIALOG_H
#define APPSETTINGSDIALOG_H

#include "../settings/AppSettings.h"
#include "../rig/HamlibController.h"
#include "BandSchedulerDialog.h"

#include <QColor>
#include <QDialog>
#include <QList>
#include <QRect>
#include <QSize>
#include <QString>

#include <functional>

class AudioSettingsDialog;
class LogbookSettingsDialog;
class RigControlSettingsDialog;
class SoundCardCalibrationDialog;
class TextMacroSettingsDialog;
class QDialogButtonBox;
class QPushButton;
class QTabWidget;
class QWidget;
class QCheckBox;
class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QLineEdit;
class QPlainTextEdit;
class QLabel;
class QShowEvent;
class AutoQsoFlowEditorWidget;

/**
 * @brief Unified application settings window.
 *
 * This dialog embeds the existing settings editors as tab pages so MadModem
 * exposes one WSJT-X-like Settings entry instead of many separate menu items.
 * The old editor classes remain reusable, but their local OK/Cancel buttons are
 * hidden and the outer dialog performs one final commit.
 */
class AppSettingsDialog final : public QDialog
{
    Q_OBJECT

public:
    using TextTranslator = std::function<QString(const QString &)>;
    using SchedulerTranslator = BandSchedulerDialog::Translator;

    enum class InitialPage
    {
        AudioPtt,
        RadioCat,
        UserQthMacros,
        Logbook,
        Rotator,
        Scheduler,
        AutoQsoFlow,
        SoundCardCalibration
    };

    explicit AppSettingsDialog(const AppSettings &settings,
                               const QString &currentLogbookPath,
                               const QString &defaultLogbookPath,
                               const QList<ScheduledQsyEntry> &schedulerEntries,
                               TextTranslator textTranslator,
                               SchedulerTranslator schedulerTranslator,
                               InitialPage initialPage = InitialPage::AudioPtt,
                               QWidget *parent = nullptr);

    AppSettings settings() const { return m_resultSettings; }
    QString selectedLogbookPath() const { return m_selectedLogbookPath; }
    QList<ScheduledQsyEntry> schedulerEntries() const { return m_resultSchedulerEntries; }

public slots:
    void setExternalCatTestStatus(const QString &message, bool ok);
    void setRotatorCalibrationResult(int profileIndex, double azimuthMsPerDeg, double elevationMsPerDeg, const QString &stampUtc, const QString &message);

signals:
    void catTestRequested(const HamlibController::Config &config);
    void pttTestRequested(const HamlibController::Config &config, bool enabled);
    void rotatorCalibrationRequested(int profileIndex, bool elevationAxis);

protected:
    void accept() override;
    void showEvent(QShowEvent *event) override;

private:
    struct ColourButton
    {
        QPushButton *button = nullptr;
        QString title;
        QString value;
    };

    QWidget *embedDialogPage(QDialog *dialog);
    QWidget *makeUserQthMacrosPage();
    QWidget *makeAudioCatPage();
    QWidget *makeLogbookPage();
    QWidget *makeFtColourEditor();
    QWidget *makeAutoQsoFlowPage();
    QWidget *makeRotatorPage();
    void prepareEmbeddedDialog(QDialog *dialog);
    void setInitialPage(InitialPage page);
    void updateAutoQsoFlowWindowMode(int index);
    void updateRotatorEndpointWarning();
    bool rotatorEndpointConflictRequiresConfirmation() const;
    void expandForAutoQsoFlow();
    void restoreAfterAutoQsoFlow();
    QString L(const QString &source) const;
    void collectSettings();
    void setColourButton(QPushButton *button, const QColor &colour);
    QColor colourFromString(const QString &name, const QColor &fallback) const;
    void chooseColour(ColourButton *entry);

    AppSettings m_initialSettings;
    AppSettings m_resultSettings;
    QString m_currentLogbookPath;
    QString m_defaultLogbookPath;
    QString m_selectedLogbookPath;
    QList<ScheduledQsyEntry> m_initialSchedulerEntries;
    QList<ScheduledQsyEntry> m_resultSchedulerEntries;
    TextTranslator m_textTranslator;
    SchedulerTranslator m_schedulerTranslator;

    QTabWidget *m_tabs = nullptr;
    QDialogButtonBox *m_buttons = nullptr;

    AudioSettingsDialog *m_audioPage = nullptr;
    RigControlSettingsDialog *m_rigPage = nullptr;
    TextMacroSettingsDialog *m_textMacroPage = nullptr;
    QLineEdit *m_editMyCallsign = nullptr;
    QLineEdit *m_editMyName = nullptr;
    QLineEdit *m_editMyQth = nullptr;
    QLineEdit *m_editMyLocator = nullptr;
    QLineEdit *m_editRig = nullptr;
    QLineEdit *m_editAntenna = nullptr;
    QLineEdit *m_editPower = nullptr;
    QList<QLineEdit *> m_macroLabelEdits;
    QList<QPlainTextEdit *> m_macroTextEdits;
    LogbookSettingsDialog *m_logbookPage = nullptr;
    BandSchedulerDialog *m_schedulerPage = nullptr;
    SoundCardCalibrationDialog *m_calibrationPage = nullptr;
    QWidget *m_rotatorPage = nullptr;
    AutoQsoFlowEditorWidget *m_autoQsoFlowPage = nullptr;
    int m_autoQsoFlowTabIndex = -1;
    bool m_autoQsoFlowWindowExpanded = false;
    bool m_settingsFullscreenApplied = false;
    QRect m_preAutoQsoFlowGeometry;
    Qt::WindowStates m_preAutoQsoFlowWindowState = Qt::WindowNoState;
    QSize m_preAutoQsoFlowMinimumSize;

    ColourButton m_colourMyCallBg;
    ColourButton m_colourMyCallFg;
    ColourButton m_colourCqBg;
    ColourButton m_colourCqFg;
    ColourButton m_colourWorkedBg;
    ColourButton m_colourWorkedFg;
    ColourButton m_colourTxBg;
    ColourButton m_colourTxFg;

    QCheckBox *m_chkHighlightNewCountry = nullptr;
    QCheckBox *m_chkWatchListIcon = nullptr;
    QCheckBox *m_chkLogbookStrikeWorkedCalls = nullptr;
    QSpinBox *m_spinDecodeTableFontSize = nullptr;
    QSpinBox *m_spinDecodeTableRowHeight = nullptr;
    QPlainTextEdit *m_editFtBlacklist = nullptr;
    QPlainTextEdit *m_editFtWatchList = nullptr;
    QComboBox *m_cmbAutoQsoDuplicatePolicy = nullptr;
    QSpinBox *m_spinAutoQsoRecentHours = nullptr;
    QSpinBox *m_spinCqRepeatCount = nullptr;
    QCheckBox *m_chkAutoQsoFlowShadowMode = nullptr;

    QCheckBox *m_chkRotatorEnabled = nullptr;
    QCheckBox *m_chkRotatorAutoConnect = nullptr;
    QComboBox *m_comboRotatorModel = nullptr;
    QComboBox *m_comboRotatorPath = nullptr;
    QLabel *m_lblRotatorEndpointWarning = nullptr;
    QSpinBox *m_spinRotatorBaud = nullptr;
    QSpinBox *m_spinRotatorPollMs = nullptr;
    QCheckBox *m_chkRotatorUseElevation = nullptr;
    QDoubleSpinBox *m_spinRotatorParkAz = nullptr;
    QDoubleSpinBox *m_spinRotatorParkEl = nullptr;
    QCheckBox *m_chkRotatorTrackSelectedQso = nullptr;
    QCheckBox *m_chkRotatorTrackOnlyQso = nullptr;
    QCheckBox *m_chkRotatorBlockFtTxUntilReady = nullptr;
    QSpinBox *m_spinRotatorTolerance = nullptr;
};

#endif // APPSETTINGSDIALOG_H
