#ifndef RIGCONTROLSETTINGSDIALOG_H
#define RIGCONTROLSETTINGSDIALOG_H

#include "../settings/AppSettings.h"
#include "../rig/HamlibController.h"

#include <QDialog>
#include <QString>

#include <functional>

class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

/**
 * @brief Settings dialog for Hamlib CAT frequency polling and PTT.
 */
class RigControlSettingsDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit RigControlSettingsDialog(const AppSettings &settings,
                                      QWidget *parent = nullptr);

    AppSettings settings() const { return m_settings; }
    void setTextTranslator(std::function<QString(const QString &)> translator);

public slots:
    void setExternalTestStatus(const QString &message, bool ok);

signals:
    void catTestRequested(const HamlibController::Config &config);
    void pttTestRequested(const HamlibController::Config &config, bool enabled);

private slots:
    void applyToSettings();
    void refreshEnabledState();
    void onManufacturerChanged();
    void onModelChanged();
    void onModelFilterChanged(const QString &text);
    void onTestCatClicked();
    void onTestPttOnClicked();
    void onTestPttOffClicked();

private:
    void buildUi();
    void loadFromSettings();
    void refreshLabels();
    void populateManufacturers();
    void populateModelsForManufacturer(const QString &manufacturer, int selectedRigModel);
    void populateSerialPorts(const QString &selectedPath = QString());
    QString selectedSerialPath() const;
    bool rigPathLooksNetwork(const QString &path) const;
    void selectPresetForRigModel(int rigModel);
    int selectedRigModel() const;
    int currentBaudRate() const;
    void setBaudRateValue(int baudRate);
    void refreshBaudSuggestionForRigModel(int rigModel);
    QString L(const QString &source) const;
    HamlibController::Config configFromCurrentUi(bool forceCat, bool forcePtt, bool readOnlyTest = false) const;
    void setTestStatus(const QString &message, bool ok);
    bool runPttTest(bool enabled);

    AppSettings m_settings;
    std::function<QString(const QString &)> m_textTranslator;
    bool m_loading = false;

    QLabel *m_lblDescription = nullptr;
    QLabel *m_lblCompiled = nullptr;
    QLabel *m_lblManufacturer = nullptr;
    QLabel *m_lblModel = nullptr;
    QLabel *m_lblModelFilter = nullptr;
    QLabel *m_lblManualId = nullptr;
    QLabel *m_lblSerialPort = nullptr;
    QLabel *m_lblTcpAddress = nullptr;
    QLabel *m_lblBaud = nullptr;
    QLabel *m_lblDataBits = nullptr;
    QLabel *m_lblStopBits = nullptr;
    QLabel *m_lblHandshake = nullptr;
    QLabel *m_lblForceDtr = nullptr;
    QLabel *m_lblForceRts = nullptr;
    QLabel *m_lblPttMethod = nullptr;
    QLabel *m_lblBaudHint = nullptr;
    QLabel *m_lblPoll = nullptr;
    QLabel *m_lblTxAudioRoute = nullptr;
    QLabel *m_lblTransmitAudioSource = nullptr;
    QLabel *m_lblHint = nullptr;
    QLabel *m_lblTestStatus = nullptr;
    QCheckBox *m_chkCatEnabled = nullptr;
    QCheckBox *m_chkPttEnabled = nullptr;
    QCheckBox *m_chkUpdateFt8Band = nullptr;
    QComboBox *m_cmbManufacturer = nullptr;
    QComboBox *m_cmbRigModel = nullptr;
    QLineEdit *m_editModelFilter = nullptr;
    QSpinBox *m_spinRigModel = nullptr;
    QComboBox *m_cmbTxAudioRoute = nullptr;
    QComboBox *m_cmbTransmitAudioSource = nullptr;
    QComboBox *m_cmbSerialPort = nullptr;
    QLineEdit *m_editTcpAddress = nullptr;
    QComboBox *m_cmbBaud = nullptr;
    QComboBox *m_cmbDataBits = nullptr;
    QComboBox *m_cmbStopBits = nullptr;
    QComboBox *m_cmbHandshake = nullptr;
    QComboBox *m_cmbForceDtr = nullptr;
    QComboBox *m_cmbForceRts = nullptr;
    QComboBox *m_cmbPttMethod = nullptr;
    QSpinBox *m_spinPollMs = nullptr;
    QLabel *m_lblCatLed = nullptr;
    QPushButton *m_btnTestCat = nullptr;
    QPushButton *m_btnTestPttOn = nullptr;
    QPushButton *m_btnTestPttOff = nullptr; // kept for source compatibility; not shown in the compact CAT setup UI
    QDialogButtonBox *m_buttonBox = nullptr;
};

#endif // RIGCONTROLSETTINGSDIALOG_H
