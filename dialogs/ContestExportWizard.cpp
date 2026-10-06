#include "ContestExportWizard.h"
#include "../utils/RuntimeI18n.h"
#include <QAbstractButton>
#include <QApplication>
#include <QComboBox>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDir>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTableWidget>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QScreen>

namespace {
QString L(const QString& text) { return MadModemI18n::text(text); }
QString validationText(const QString& error) {
    const QRegularExpression fieldError("(TX|RX) ([A-Z0-9_]+): missing or invalid recorded value$");
    const auto match=fieldError.match(error);
    if(match.hasMatch()) return error.left(match.capturedStart())+
        L("Missing or invalid recorded exchange field: %1").arg(match.captured(1)+' '+match.captured(2));
    const QMap<QString, QString> messages{
        {"Select QSOs from one contest edition and station", L("Select QSOs from one contest edition and station")},
        {"band not allowed by contest profile", L("Band not allowed by contest profile")},
        {"mode does not match contest profile", L("Mode does not match contest profile")},
        {"Callsign", L("Invalid or missing station/contact callsign")},
        {"TX CQ zone", L("Sent CQ zone missing or invalid")},
        {"RX CQ zone", L("Received CQ zone missing or invalid")},
        {"State", L("US state missing or invalid")},
        {"Province", L("Canadian province missing or invalid")},
        {"Mode", L("RTTY mode required")},
        {"Band", L("Band not allowed for CQ WW RTTY")},
        {"RST", L("RST missing or invalid")},
        {"Frequency missing or inconsistent with band", L("Frequency missing or inconsistent with band")},
        {"No QSOs selected", L("No QSOs selected")},
        {"STATION_CALLSIGN is missing", L("Station callsign missing")}
    };
    if (messages.contains(error)) return messages.value(error);
    const int separator = error.indexOf(QStringLiteral(": "));
    if (separator >= 0 && messages.contains(error.mid(separator+2)))
        return error.left(separator+2)+messages.value(error.mid(separator+2));
    return error;
}
QComboBox* choices(QWidget* parent, const QStringList& items, const char* name) {
    auto* combo = new QComboBox(parent);
    combo->setObjectName(QString::fromLatin1(name));
    combo->addItems(items);
    return combo;
}
}
class ContestExportPage : public QWizardPage {
public:
    using QWizardPage::QWizardPage;
    bool isComplete() const override { return m_complete; }
    void setComplete(bool value) { if (value != m_complete) { m_complete = value; emit completeChanged(); } }
private:
    bool m_complete = false;
};

ContestExportWizard::ContestExportWizard(const QVector<LogbookEntry>& records, QWidget* parent)
    : QWizard(parent)
{
    setObjectName("ContestExportWizard");
    setWindowTitle(L("Export contest QSOs to Cabrillo"));
    // Native Aero/Mac wizard banners use platform colors outside the app theme.
    // Classic pages plus palette roles keep headers, pages and buttons coherent.
    setWizardStyle(QWizard::ClassicStyle);
    setStyleSheet(QStringLiteral("QWizard, QWizardPage { background-color: palette(window); color: palette(window-text); }"));
    setOption(QWizard::NoBackButtonOnStartPage);
    setButtonText(QWizard::BackButton, L("Back"));
    setButtonText(QWizard::NextButton, L("Next"));
    setButtonText(QWizard::CancelButton, L("Cancel"));
    setButtonText(QWizard::FinishButton, L("Export Cabrillo..."));
    resize(1100, 680);
    if (auto* screen = QGuiApplication::primaryScreen())
        resize(size().boundedTo(screen->availableGeometry().size() * 0.9));
    const QDir dir(QCoreApplication::applicationDirPath());
    m_profiles = ContestExport::loadProfiles({dir.filePath("rtty_rules"), dir.filePath("cw_rules")});
    m_catalog = ContestExport::discover(records, m_profiles);

    m_selectionPage = new ContestExportPage(this);
    m_selectionPage->setTitle(L("1. Select the contest edition"));
    auto* selectionLayout = new QVBoxLayout(m_selectionPage);
    auto* explanation = new QLabel(L("Editions found in the complete logbook, independent of the current search filter. Dates are UTC; different station callsigns are kept separate."), m_selectionPage);
    explanation->setWordWrap(true);
    selectionLayout->addWidget(explanation);
    m_editions = new QComboBox(m_selectionPage);
    m_editions->setObjectName("contestEdition");
    m_editions->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_editions->setMinimumContentsLength(25);
    for (const auto& edition : m_catalog.editions) {
        const QString label = QStringLiteral("%1 — %2 / %3 UTC — %4 — %5 QSO")
            .arg(edition.name, edition.window.start.isValid()?edition.window.start.toString("yyyy-MM-dd HH:mm"):edition.firstDate.toString(Qt::ISODate),
                 edition.window.end.isValid()?edition.window.end.toString("yyyy-MM-dd HH:mm"):edition.lastDate.toString(Qt::ISODate),
                 edition.station.isEmpty() ? L("Station callsign missing") : edition.station).arg(edition.records.size());
        m_editions->addItem(label);
        m_editions->setItemData(m_editions->count()-1, label, Qt::ToolTipRole);
    }
    selectionLayout->addWidget(m_editions);
    auto* notice = new QLabel(m_selectionPage);
    notice->setWordWrap(true);
    notice->setText(m_catalog.editions.isEmpty()
        ? L("No contest editions found. Records need a contest identifier or rule and a valid UTC date. Untagged contacts are recognized only when dates, mode and recorded exchanges identify one edition.")
        : L("Sessions from the same edition are combined for every contest. Recognition uses logged contest/rule tags, available schedules and recorded exchanges. Untagged matches are marked in the preview; ambiguous contacts are not assigned automatically."));
    selectionLayout->addWidget(notice);
    if (m_catalog.invalidDates || m_catalog.outsidePeriod || m_catalog.ambiguous) {
        auto* skipped = new QLabel(L("Not selected: %1 records without a valid UTC date; %2 outside known contest periods; %3 with ambiguous contest membership.")
            .arg(m_catalog.invalidDates).arg(m_catalog.outsidePeriod).arg(m_catalog.ambiguous), m_selectionPage);
        skipped->setWordWrap(true);
        selectionLayout->addWidget(skipped);
    }
    selectionLayout->addStretch();
    m_selectionPage->setComplete(!m_catalog.editions.isEmpty());
    addPage(m_selectionPage);

    auto* options = new QWizardPage(this);
    options->setTitle(L("2. Station and entry category"));
    auto* form = new QFormLayout(options);
    m_station = new QLineEdit(options); m_station->setObjectName("exportStation");
    m_operator = choices(options, {"SINGLE-OP", "CHECKLOG"}, "exportOperator");
    m_power = choices(options, {"LOW", "HIGH", "QRP"}, "exportPower");
    m_assisted = choices(options, {"ASSISTED", "NON-ASSISTED"}, "exportAssisted");
    m_band = choices(options, {"ALL", "80M", "40M", "20M", "15M", "10M"}, "exportBand");
    m_sent = choices(options, {"STX_STRING", "STX", "RST_SENT"}, "exportSentField");
    m_received = choices(options, {"SRX_STRING", "SRX", "RST_RCVD"}, "exportReceivedField");
    m_sent->setEditable(true); m_received->setEditable(true);
    m_name = new QLineEdit(options); m_email = new QLineEdit(options);
    form->addRow(L("Station callsign"), m_station);
    form->addRow(QStringLiteral("CATEGORY-OPERATOR"), m_operator);
    form->addRow(QStringLiteral("CATEGORY-POWER"), m_power);
    form->addRow(QStringLiteral("CATEGORY-ASSISTED"), m_assisted);
    form->addRow(QStringLiteral("CATEGORY-BAND"), m_band);
    m_automatic = new QCheckBox(L("Read contest exchanges automatically"), options);
    m_automatic->setObjectName("automaticContestExchange"); m_automatic->setChecked(true);
    form->addRow(m_automatic);
    form->addRow(L("Sent exchange ADIF field"), m_sent);
    form->addRow(L("Received exchange ADIF field"), m_received);
    form->addRow(QStringLiteral("NAME"), m_name); form->addRow(QStringLiteral("EMAIL"), m_email);
    auto* info = new QLabel(L("Exchanges follow each contest profile using recorded internal fields, standard ADIF fields and exchange strings. Manual ADIF field selection is available if needed. The band category does not remove off-band QSOs."), options);
    info->setWordWrap(true); form->addRow(info);
    addPage(options);

    m_previewPage = new ContestExportPage(this);
    m_previewPage->setTitle(L("3. Review QSOs before export"));
    auto* previewLayout = new QVBoxLayout(m_previewPage);
    auto* instructions = new QLabel(L("Only checked QSOs will be exported. Missing or invalid data is shown for each row and blocks export until corrected in the logbook or explicitly excluded here. The logbook is not changed."), m_previewPage);
    instructions->setWordWrap(true); previewLayout->addWidget(instructions);
    auto* tabs = new QTabWidget(m_previewPage);
    m_table = new QTableWidget(tabs); m_table->setObjectName("contestQsoPreview");
    m_table->setColumnCount(7);
    m_table->setHorizontalHeaderLabels({L("Export"), "UTC", L("Callsign"), L("Frequency / mode"), L("Sent exchange"), L("Received exchange"), L("Validation")});
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setAlternatingRowColors(true);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_cabrillo = new QPlainTextEdit(tabs); m_cabrillo->setReadOnly(true);
    tabs->addTab(m_table, L("QSO preview")); tabs->addTab(m_cabrillo, L("Cabrillo preview"));
    previewLayout->addWidget(tabs);
    m_summary = new QLabel(m_previewPage); m_summary->setObjectName("contestExportSummary");
    m_summary->setWordWrap(true); previewLayout->addWidget(m_summary);
    addPage(m_previewPage);

    connect(m_editions, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { selectEdition(); });
    connect(this, &QWizard::currentIdChanged, this, [this](int id) { if (id == 2) populatePreview(); else m_generated.clear(); });
    connect(m_table, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) { if (!m_populating && item->column() == 0) validateSelection(); });
    connect(m_automatic, &QCheckBox::toggled, this, [this](bool enabled) {
        m_sent->setEnabled(!enabled); m_received->setEnabled(!enabled);
    });
    selectEdition();
}

void ContestExportWizard::selectEdition() {
    m_generated.clear();
    const int index = m_editions->currentIndex();
    if (index < 0) return;
    const auto& edition = m_catalog.editions[index];
    m_station->setText(edition.station);
    m_station->setReadOnly(!edition.station.isEmpty());
    const bool cq = edition.contestId == "CQ-WW-RTTY";
    m_automatic->setChecked(true); m_automatic->setEnabled(!cq);
    m_sent->setEnabled(false); m_received->setEnabled(false);
    m_band->clear(); m_band->addItem("ALL");
    QSet<QString> bands;
    for (const auto& mode : edition.modes) {
        const auto profile=ContestExport::profileFor(m_profiles,edition.contestId,mode);
        for (const auto& value:profile.definition.value("bands").toArray()) bands.insert(value.toString().toUpper());
    }
    for (const auto& entry:edition.records) if(!entry.band.isEmpty()) bands.insert(entry.band.toUpper());
    QStringList sorted=bands.values(); sorted.sort(); m_band->addItems(sorted);
}

QByteArray ContestExportWizard::generate(const QVector<LogbookEntry>& records, QString* error) const {
    GenericCabrillo::Options options;
    options.categoryOperator=m_operator->currentText(); options.categoryPower=m_power->currentText();
    options.categoryAssisted=m_assisted->currentText(); options.categoryBand=m_band->currentText();
    options.sentExchangeField=m_sent->currentText(); options.receivedExchangeField=m_received->currentText();
    options.name=m_name->text(); options.email=m_email->text();
    return ContestExport::cabrillo(records,m_profiles,options,m_automatic->isChecked(),error);
}

void ContestExportWizard::populatePreview() {
    m_generated.clear(); m_previewRecords.clear(); m_previewPage->setComplete(false);
    const int index = m_editions->currentIndex();
    if (index < 0) return;
    m_populating = true;
    const auto& edition = m_catalog.editions[index];
    QString previewTitle = m_editions->currentText();
    if (edition.station.isEmpty() && !m_station->text().trimmed().isEmpty())
        previewTitle.replace(L("Station callsign missing"), m_station->text().trimmed().toUpper());
    m_previewPage->setSubTitle(previewTitle);
    m_table->setRowCount(edition.records.size());
    const bool cq = edition.contestId == "CQ-WW-RTTY";
    for (int row=0; row<edition.records.size(); ++row) {
        auto entry = ContestExport::prepared(edition.records[row], edition.contestId);
        if (entry.stationCallsign.isEmpty()) entry.stationCallsign=m_station->text().trimmed().toUpper();
        m_previewRecords.push_back(entry);
        QString error;
        generate({entry}, &error);
        QString sent, received;
        if (cq) {
            sent = QStringLiteral("%1 %2 %3").arg(entry.rstSent, CqWwRtty::field(entry,true,"CQZONE"), CqWwRtty::field(entry,true,"QTH")).trimmed();
            received = QStringLiteral("%1 %2 %3").arg(entry.rstReceived, CqWwRtty::field(entry,false,"CQZONE"), CqWwRtty::field(entry,false,"QTH")).trimmed();
        } else if(m_automatic->isChecked()) {
            const auto profile=ContestExport::profileFor(m_profiles,edition.contestId,entry.mode);
            sent=ContestExport::automaticExchange(entry,profile,true);
            received=ContestExport::automaticExchange(entry,profile,false);
        } else {
            sent = GenericCabrillo::value(entry,m_sent->currentText(),true);
            received = GenericCabrillo::value(entry,m_received->currentText(),false);
        }
        QString status = error.isEmpty() ? L("Ready") : validationText(error);
        if (edition.inferred[row]) status = L("Recognized from date and exchange") + QStringLiteral(" — ") + status;
        const QStringList values{QString(), entry.utc.toUTC().toString("yyyy-MM-dd HH:mm"), entry.callsign,
            entry.freq + QStringLiteral(" MHz / ") + entry.mode, sent, received, status};
        for (int col=0; col<values.size(); ++col) {
            auto* item = new QTableWidgetItem(values[col]);
            item->setToolTip(values[col]);
            if (col==0) { item->setFlags(item->flags() | Qt::ItemIsUserCheckable); item->setCheckState(Qt::Checked); }
            m_table->setItem(row,col,item);
        }
        m_table->item(row,0)->setData(Qt::UserRole,error);
    }
    m_table->resizeColumnsToContents();
    m_populating = false;
    validateSelection();
}

QVector<LogbookEntry> ContestExportWizard::selectedRecords() const {
    QVector<LogbookEntry> result;
    for (int row=0; row<m_previewRecords.size(); ++row)
        if (m_table->item(row,0)->checkState()==Qt::Checked) result.push_back(m_previewRecords[row]);
    return result;
}
void ContestExportWizard::validateSelection() {
    const auto records = selectedRecords();
    QString error;
    m_generated = generate(records, &error);
    int invalid = 0;
    for (int row=0; row<m_table->rowCount(); ++row)
        if (m_table->item(row,0)->checkState()==Qt::Checked && !m_table->item(row,0)->data(Qt::UserRole).toString().isEmpty()) ++invalid;
    m_summary->setText(L("%1 of %2 QSOs selected; %3 with invalid or missing data.").arg(records.size()).arg(m_previewRecords.size()).arg(invalid)
        + (m_generated.isEmpty() ? QStringLiteral("\n") + L("Validation failed:") + ' ' + validationText(error) : QString()));
    m_cabrillo->setPlainText(QString::fromUtf8(m_generated));
    m_previewPage->setComplete(!m_generated.isEmpty());
}
void ContestExportWizard::accept() {
    if (currentId()!=2) return;
    validateSelection();
    if (!m_generated.isEmpty()) QWizard::accept();
}
QString ContestExportWizard::suggestedFileName() const {
    const int index=m_editions->currentIndex();
    if (index<0) return QStringLiteral("contest.log");
    const auto& edition=m_catalog.editions[index];
    QString name=edition.contestId.toLower()+'_'+edition.firstDate.toString(Qt::ISODate)+'_'+m_station->text().trimmed().toUpper();
    name.replace(QRegularExpression("[^A-Za-z0-9_-]"), "_");
    return name+QStringLiteral(".log");
}
