#include "TextMacroSettingsDialog.h"
#include "../utils/UiScale.h"
#include "../utils/RuntimeI18n.h"

#include <QDialogButtonBox>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QTabWidget>
#include <QVBoxLayout>

namespace
{

QString L18n(const QString &source)
{
    return MadModemI18n::text(source);
}
QString P18n(const QString &source)
{
    return MadModemI18n::placeholder(source);
}
constexpr int kDialogMinWidth = 860;
constexpr int kDialogMinHeight = 560;
constexpr int kLabelMinWidth = 145;
constexpr int kTokenMinWidth = 74;
constexpr int kFieldMinHeight = 28;

QLabel *makeFieldLabel(const QString &text, QWidget *parent)
{
    QLabel *label = new QLabel(text, parent);
    label->setMinimumWidth(kLabelMinWidth);
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    label->setWordWrap(false);
    return label;
}

QLabel *makeTokenLabel(const QString &text, QWidget *parent)
{
    QLabel *label = new QLabel(text, parent);
    label->setMinimumWidth(kTokenMinWidth);
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setStyleSheet("color: #555555;");
    return label;
}

void addVariableRow(QGridLayout *grid,
                    int row,
                    const QString &labelText,
                    const QString &tokenText,
                    QLineEdit *editor,
                    QWidget *parent)
{
    grid->addWidget(makeFieldLabel(labelText, parent), row, 0);
    grid->addWidget(makeTokenLabel(tokenText, parent), row, 1);
    grid->addWidget(editor, row, 2);
    grid->setRowMinimumHeight(row, kFieldMinHeight + 4);
}

QScrollArea *makeScrollPage(QWidget *content)
{
    QScrollArea *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(content);
    return scroll;
}
}

// -----------------------------------------------------------------------------
// Construction
// -----------------------------------------------------------------------------

TextMacroSettingsDialog::TextMacroSettingsDialog(const AppSettings &settings, QWidget *parent)
    : QDialog(parent),
      m_settings(settings)
{
    setWindowTitle(L18n(QStringLiteral("User/QTH")));
    resize(780, 500);
    setMinimumSize(0, 0);
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 8);
    mainLayout->setSpacing(8);

    QLabel *intro = new QLabel(L18n(QStringLiteral("Configure persistent user, station and QTH data plus the shared text-mode macro buttons. The callsign and locator set here are used by FT4/FT8 standard messages and as the home position for QSO maps.")),
        this);
    intro->setWordWrap(true);
    intro->setMinimumHeight(34);
    mainLayout->addWidget(intro);

    QTabWidget *tabs = new QTabWidget(this);
    mainLayout->addWidget(tabs, 1);

    // ---------------------------------------------------------------------
    // Variables page
    // ---------------------------------------------------------------------

    QWidget *variablesContent = new QWidget;
    QVBoxLayout *variablesLayout = new QVBoxLayout(variablesContent);
    variablesLayout->setContentsMargins(8, 8, 8, 8);
    variablesLayout->setSpacing(8);

    QGroupBox *myStationGroup = new QGroupBox(L18n(QStringLiteral("User / station / QTH")), variablesContent);
    QGridLayout *myStationGrid = new QGridLayout(myStationGroup);
    myStationGrid->setContentsMargins(10, 12, 10, 10);
    myStationGrid->setHorizontalSpacing(8);
    myStationGrid->setVerticalSpacing(5);
    myStationGrid->setColumnStretch(2, 1);

    m_editMyCallsign = makeLineEdit(QStringLiteral("your callsign"));
    m_editMyName = makeLineEdit(QStringLiteral("operator name"));
    m_editMyQth = makeLineEdit(QStringLiteral("town / city"));
    m_editMyLocator = makeLineEdit(QStringLiteral("Maidenhead locator, e.g. AA00aa"));
    m_editRig = makeLineEdit(QStringLiteral("radio / SDR / transceiver"));
    m_editAntenna = makeLineEdit(QStringLiteral("antenna description"));
    m_editPower = makeLineEdit(QStringLiteral("power in watts"));

    addVariableRow(myStationGrid, 0, L18n(QStringLiteral("My callsign")), "{MYCALL}", m_editMyCallsign, myStationGroup);
    addVariableRow(myStationGrid, 1, L18n(QStringLiteral("My name")), "{MYNAME}", m_editMyName, myStationGroup);
    addVariableRow(myStationGrid, 2, L18n(QStringLiteral("My QTH")), "{MYQTH}", m_editMyQth, myStationGroup);
    addVariableRow(myStationGrid, 3, L18n(QStringLiteral("Locator")), "{LOC}", m_editMyLocator, myStationGroup);
    addVariableRow(myStationGrid, 4, L18n(QStringLiteral("Rig")), "{RIG}", m_editRig, myStationGroup);
    addVariableRow(myStationGrid, 5, L18n(QStringLiteral("Antenna")), "{ANT}", m_editAntenna, myStationGroup);
    addVariableRow(myStationGrid, 6, L18n(QStringLiteral("Power")), "{PWR}", m_editPower, myStationGroup);

    QGroupBox *tokensGroup = new QGroupBox(L18n(QStringLiteral("Available tokens")), variablesContent);
    QVBoxLayout *tokensLayout = new QVBoxLayout(tokensGroup);
    tokensLayout->setContentsMargins(10, 12, 10, 10);
    tokensLayout->setSpacing(5);
    QLabel *tokens = new QLabel(L18n(QStringLiteral("{MYCALL}, {MYNAME}, {MYQTH}, {LOC}, {CALL}, {NAME}, {QTH}, {RST}, {RIG}, {ANT}, {PWR}, {MODE}, {DATE}, {TIME}, {UTC}, {NL}.\n{NL} inserts a new line. Token names are case-insensitive.")),
        tokensGroup);
    tokens->setWordWrap(true);
    tokens->setTextInteractionFlags(Qt::TextSelectableByMouse);
    tokensLayout->addWidget(tokens);

    variablesLayout->addWidget(myStationGroup);
    variablesLayout->addWidget(tokensGroup);
    variablesLayout->addStretch(1);
    tabs->addTab(makeScrollPage(variablesContent), L18n(QStringLiteral("User / QTH")));

    // ---------------------------------------------------------------------
    // Macros page
    // ---------------------------------------------------------------------

    QWidget *macrosContent = new QWidget;
    QVBoxLayout *macrosLayout = new QVBoxLayout(macrosContent);
    macrosLayout->setContentsMargins(8, 8, 8, 8);
    macrosLayout->setSpacing(8);

    for (int i = 0; i < 6; ++i) {
        QGroupBox *macroGroup = new QGroupBox(L18n(QStringLiteral("Macro %1")).arg(i + 1), macrosContent);
        QGridLayout *grid = new QGridLayout(macroGroup);
        grid->setContentsMargins(10, 12, 10, 10);
        grid->setHorizontalSpacing(8);
        grid->setVerticalSpacing(5);
        grid->setColumnStretch(1, 1);

        QLineEdit *labelEdit = makeLineEdit(QString("button label %1").arg(i + 1));
        QPlainTextEdit *textEdit = new QPlainTextEdit(macroGroup);
        textEdit->setMinimumHeight(62);
        textEdit->setPlaceholderText(P18n(QStringLiteral("Macro text with tokens, e.g. CQ CQ CQ de {MYCALL} {MYCALL} pse k")));

        m_macroLabelEdits.append(labelEdit);
        m_macroTextEdits.append(textEdit);

        QLabel *buttonLabel = makeFieldLabel(L18n(QStringLiteral("Button label")), macroGroup);
        QLabel *textLabel = makeFieldLabel(L18n(QStringLiteral("Macro text")), macroGroup);
        textLabel->setAlignment(Qt::AlignRight | Qt::AlignTop);

        grid->addWidget(buttonLabel, 0, 0);
        grid->addWidget(labelEdit, 0, 1);
        grid->addWidget(textLabel, 1, 0);
        grid->addWidget(textEdit, 1, 1);

        macrosLayout->addWidget(macroGroup);
    }

    macrosLayout->addStretch(1);
    tabs->addTab(makeScrollPage(macrosContent), L18n(QStringLiteral("Macros")));

    QDialogButtonBox *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        this);
    connect(buttons, &QDialogButtonBox::accepted, this, &TextMacroSettingsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &TextMacroSettingsDialog::reject);
    mainLayout->addWidget(buttons);

    loadFromSettings(settings);
    // Keep this dialog native-sized; global scaling made it too tall on 1920x1080 Windows.
}

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------

AppSettings TextMacroSettingsDialog::settings() const
{
    return m_settings;
}

// -----------------------------------------------------------------------------
// Slots
// -----------------------------------------------------------------------------

void TextMacroSettingsDialog::accept()
{
    storeToSettings();
    QDialog::accept();
}

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

QLineEdit *TextMacroSettingsDialog::makeLineEdit(const QString &placeholder)
{
    QLineEdit *edit = new QLineEdit(this);
    edit->setMinimumHeight(kFieldMinHeight);
    edit->setPlaceholderText(P18n(placeholder));
    return edit;
}

void TextMacroSettingsDialog::loadFromSettings(const AppSettings &settings)
{
    m_editMyCallsign->setText(!settings.textMyCallsign.trimmed().isEmpty()
                                  ? settings.textMyCallsign
                                  : settings.ft8MyCallsign);
    m_editMyName->setText(settings.textMyName);
    m_editMyQth->setText(settings.textMyQth);
    m_editMyLocator->setText(!settings.textMyLocator.trimmed().isEmpty()
                                 ? settings.textMyLocator
                                 : settings.ft8MyGrid);
    m_editRig->setText(settings.textRig);
    m_editAntenna->setText(settings.textAntenna);
    m_editPower->setText(settings.textPower);

    for (int i = 0; i < m_macroLabelEdits.size(); ++i) {
        m_macroLabelEdits[i]->setText(settings.textMacroLabels.value(i, L18n(QStringLiteral("Macro %1")).arg(i + 1)));
        m_macroTextEdits[i]->setPlainText(settings.textMacroTexts.value(i));
    }
}

void TextMacroSettingsDialog::storeToSettings()
{
    m_settings.textMyCallsign = m_editMyCallsign->text().trimmed().toUpper();
    m_settings.textMyName = m_editMyName->text().trimmed();
    m_settings.textMyQth = m_editMyQth->text().trimmed();
    m_settings.textMyLocator = m_editMyLocator->text().trimmed().toUpper();
    m_settings.textRig = m_editRig->text().trimmed();
    m_settings.textAntenna = m_editAntenna->text().trimmed();
    m_settings.textPower = m_editPower->text().trimmed();

    // Keep the legacy FT settings synchronized, but the editable source of truth
    // is the station identity above.
    m_settings.ft8MyCallsign = m_settings.textMyCallsign;
    m_settings.ft8MyGrid = m_settings.textMyLocator;

    m_settings.textMacroLabels.clear();
    m_settings.textMacroTexts.clear();

    for (int i = 0; i < m_macroLabelEdits.size(); ++i) {
        QString label = m_macroLabelEdits[i]->text().trimmed();
        if (label.isEmpty()) {
            label = L18n(QStringLiteral("Macro %1")).arg(i + 1);
        }

        m_settings.textMacroLabels.append(label);
        m_settings.textMacroTexts.append(m_macroTextEdits[i]->toPlainText());
    }
}
