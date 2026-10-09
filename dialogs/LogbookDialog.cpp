#include "../logbook/AsyncLogbook.h"
#include <QPointer>
#include "../logbook/CqWwRtty.h"
#include "ContestExportWizard.h"
#include <QWizard>
#include <QWizardPage>
#include <QPlainTextEdit>
#include <QRadioButton>
#include <QSaveFile>
#include <QComboBox>
#include <QFormLayout>
#include "LogbookDialog.h"
#include "../utils/UiScale.h"
#include "../utils/RuntimeI18n.h"
#include "../settings/AppSettings.h"

#include <QAbstractItemView>
#include <QTextStream>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringConverter>
#endif
#include <QStatusBar>
#include <QMenuBar>
#include <QMenu>
#include <QFile>
#include <QClipboard>
#include <QApplication>
#include <QAction>
#include <QAbstractButton>
#include <QAbstractPrintDialog>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDate>
#include <QScreen>
#include <QGuiApplication>
#include <QSignalBlocker>
#include <QDateTime>
#include <QDateTimeEdit>
#include <QFileDialog>
#include <QEventLoop>
#include <QFileInfo>
#include <QGridLayout>
#include <QScrollArea>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QResizeEvent>
#include <QSet>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QToolBar>
#include <QToolButton>
#include <QStyle>
#include <QTextDocument>
#include <QTimer>
#include <QtGlobal>
#include <QVBoxLayout>

#include <utility>
#include <algorithm>

#include <QPrintDialog>
#include <QPrinter>
#include <QProgressDialog>
#include <QRegularExpression>

LogbookDialog::LogbookDialog(AdifLogbook *logbook, AppSettings *settings, QWidget *parent)
    : QDialog(parent),
      m_logbook(logbook),
      m_settings(settings)
{
    setWindowTitle(L("MadModem logbook"));
    setMinimumSize(700, 460);
    resize(MadModemUi::size(1120, 720));
    setObjectName(QStringLiteral("MadModemLogbookDialog"));
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);


    m_actImport = new QAction(style()->standardIcon(QStyle::SP_DirOpenIcon), L("Import"), this);
    m_actExportAll = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), L("Export all"), this);
    m_actExportResult = new QAction(style()->standardIcon(QStyle::SP_FileDialogListView), L("Export result"), this);
    m_actExportSelectedAdif = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), L("Save ADIF"), this);
    QAction *actCabrillo = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), L("Export Cabrillo..."), this);
    connect(actCabrillo, &QAction::triggered, this, &LogbookDialog::exportCabrilloWizard);
    m_actExportSelectedCsv = new QAction(style()->standardIcon(QStyle::SP_DriveFDIcon), L("Save CSV"), this);
    m_actCopyCsv = new QAction(style()->standardIcon(QStyle::SP_FileIcon), L("Copy CSV"), this);
    m_actCopyAdif = new QAction(style()->standardIcon(QStyle::SP_FileIcon), L("Copy ADIF"), this);
    m_actDelete = new QAction(style()->standardIcon(QStyle::SP_TrashIcon), L("Delete selected QSOs"), this);
    m_actPrint = new QAction(style()->standardIcon(QStyle::SP_FileDialogDetailedView), L("Print"), this);
    m_actPdf = new QAction(style()->standardIcon(QStyle::SP_FileDialogContentsView), L("PDF"), this);
    m_actStatsPdf = new QAction(style()->standardIcon(QStyle::SP_FileDialogDetailedView), L("Stats PDF"), this);
    m_actRefresh = new QAction(style()->standardIcon(QStyle::SP_BrowserReload), L("Refresh"), this);
    m_actClearSearch = new QAction(style()->standardIcon(QStyle::SP_DialogResetButton), L("Clear"), this);
    m_actSelectAll = new QAction(L("Select all rows"), this);
    m_actSelectAll->setShortcut(QKeySequence::SelectAll);
    m_actColumns = new QAction(style()->standardIcon(QStyle::SP_FileDialogDetailedView), L("Columns"), this);

    auto describeAction = [this](QAction *action, const QString &tooltip, const QString &statusTip) {
        if (action == nullptr) return;
        action->setToolTip(L(tooltip));
        action->setStatusTip(L(statusTip.isEmpty() ? tooltip : statusTip));
    };
    describeAction(m_actImport, QStringLiteral("Import an ADIF file into the MadModem logbook."), QStringLiteral("Import an ADIF log file."));
    describeAction(m_actExportAll, QStringLiteral("Export the complete logbook as ADIF."), QStringLiteral("Export every QSO as ADIF."));
    describeAction(m_actExportResult, QStringLiteral("Export the currently filtered search result as ADIF."), QStringLiteral("Export visible/search result rows as ADIF."));
    describeAction(m_actExportSelectedAdif, QStringLiteral("Save the selected QSO rows as an ADIF file."), QStringLiteral("Save selected rows as ADIF."));
    describeAction(m_actExportSelectedCsv, QStringLiteral("Save the selected QSO rows as a CSV spreadsheet."), QStringLiteral("Save selected rows as CSV."));
    describeAction(m_actCopyCsv, QStringLiteral("Copy the selected QSO rows to the clipboard as CSV."), QStringLiteral("Copy selected rows as CSV."));
    describeAction(m_actCopyAdif, QStringLiteral("Copy the selected QSO rows to the clipboard as ADIF."), QStringLiteral("Copy selected rows as ADIF."));
    describeAction(m_actDelete, QStringLiteral("Delete the selected QSO records after confirmation."), QStringLiteral("Delete selected QSO rows."));
    describeAction(m_actPrint, QStringLiteral("Print the visible logbook table."), QStringLiteral("Print logbook."));
    describeAction(m_actPdf, QStringLiteral("Save the visible logbook table as PDF."), QStringLiteral("Save logbook PDF."));
    describeAction(m_actStatsPdf, QStringLiteral("Create a PDF report with logbook statistics."), QStringLiteral("Save statistics PDF."));
    describeAction(m_actRefresh, QStringLiteral("Reload the logbook table from the ADIF backend."), QStringLiteral("Refresh logbook."));
    describeAction(m_actClearSearch, QStringLiteral("Clear all search fields and show the full logbook."), QStringLiteral("Clear logbook search."));
    describeAction(m_actColumns, QStringLiteral("Choose which ADIF fields are visible and printed."), QStringLiteral("Configure visible columns."));

    // One output scope drives exports and reports; no second scope dialog.
    m_toolbar = new QToolBar(this);
    m_toolbar->setObjectName("logbookActions");
    m_toolbar->setMovable(false);
    m_toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_toolbar->setIconSize(QSize(18,18));
    m_outputScope = new QComboBox(this);
    m_outputScope->setObjectName("logbookOutputScope");
    m_outputScope->addItems({L("Visible QSOs"),L("Selected QSOs"),L("Full logbook")});
    m_outputScope->setToolTip(L("This choice applies to ADIF, CSV, copy, print and PDF."));
    m_toolbar->addWidget(m_outputScope);
    m_actExport = m_toolbar->addAction(style()->standardIcon(QStyle::SP_DialogSaveButton), L("Export ADIF..."));
    m_actExport->setObjectName("logbookExportAdif");
    connect(m_actExport,&QAction::triggered,this,[this]{
        exportRecords(outputRecords(),"Export ADIF", "MadModem_QSOs.adi", "QSOs");
    });
    m_toolbar->addAction(actCabrillo);
    auto* more = new QToolButton(this);
    more->setText(L("More")); more->setPopupMode(QToolButton::InstantPopup);
    auto* menu = new QMenu(more);
    menu->addAction(m_actImport);
    auto* csv=menu->addAction(L("Export CSV..."));
    connect(csv,&QAction::triggered,this,[this]{exportRecordsCsv(outputRecords(),"Export CSV","MadModem_QSOs.csv","QSOs");});
    auto* copyCsv=menu->addAction(L("Copy CSV"));
    connect(copyCsv,&QAction::triggered,this,[this]{QApplication::clipboard()->setText(csvForRecords(outputRecords()));});
    auto* copyAdif=menu->addAction(L("Copy ADIF"));
    connect(copyAdif,&QAction::triggered,this,[this]{
        if(m_logbook) QApplication::clipboard()->setText(AdifLogbook::recordsToAdif(outputRecords()));
    });
    menu->addSeparator(); menu->addAction(m_actPrint); menu->addAction(m_actPdf); menu->addAction(m_actStatsPdf);
    menu->addSeparator(); menu->addAction(m_actColumns); menu->addAction(m_actRefresh);
    menu->addSeparator(); menu->addAction(m_actDelete);
    more->setMenu(menu); m_toolbar->addWidget(more);
    mainLayout->addWidget(m_toolbar);
    connect(m_outputScope,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this]{updateOutputScope();});

    auto* searchRow=new QHBoxLayout;
    m_quickSearchEdit=new QLineEdit(this); m_quickSearchEdit->setObjectName("logbookSearch");
    m_quickSearchEdit->setClearButtonEnabled(true);
    m_quickSearchEdit->setPlaceholderText(L("Search callsign, locator or any field..."));
    searchRow->addWidget(m_quickSearchEdit,1);
    m_clearSearchButton=new QPushButton(L("Reset filters"),this);
    searchRow->addWidget(m_clearSearchButton);
    mainLayout->addLayout(searchRow);

    auto* filters = new QGridLayout;
    m_bandCombo=new QComboBox(this); m_bandCombo->setObjectName("logbookBand");
    m_modeCombo=new QComboBox(this); m_modeCombo->setObjectName("logbookMode");
    m_bandCombo->setEditable(true); m_modeCombo->setEditable(true);
    m_bandCombo->addItem(""); m_modeCombo->addItem("");
    QSet<QString> bands,modes;
    if(m_logbook) for(const auto& e:m_logbook->records()) { if(!e.band.isEmpty()) bands.insert(e.band); if(!e.mode.isEmpty()) modes.insert(e.mode); }
    auto bandNames=bands.values();bandNames.sort();m_bandCombo->addItems(bandNames);
    auto modeNames=modes.values();modeNames.sort();m_modeCombo->addItems(modeNames);
    m_bandEdit=m_bandCombo->lineEdit(); m_modeEdit=m_modeCombo->lineEdit();
    m_bandEdit->setPlaceholderText(L("All bands"));m_modeEdit->setPlaceholderText(L("All modes"));
    m_period=new QComboBox(this);m_period->setObjectName("logbookPeriod");
    m_period->addItems({L("All dates"),L("Today (UTC)"),L("Yesterday (UTC)"),L("Last 7 days"),L("Custom UTC interval")});
    filters->addWidget(new QLabel(L("Band"),this),0,0);filters->addWidget(m_bandCombo,0,1);
    filters->addWidget(new QLabel(L("Mode"),this),0,2);filters->addWidget(m_modeCombo,0,3);
    filters->addWidget(new QLabel(L("Period"),this),0,4);filters->addWidget(m_period,0,5);
    filters->setColumnStretch(1,1);filters->setColumnStretch(3,1);filters->setColumnStretch(5,2);
    mainLayout->addLayout(filters);
    auto* interval=new QWidget(this); auto* dates=new QHBoxLayout(interval);dates->setContentsMargins(0,0,0,0);
    m_fromEnabled=new QCheckBox(L("From UTC"),interval);m_toEnabled=new QCheckBox(L("Until UTC (excluded)"),interval);
    const auto today=QDateTime::currentDateTimeUtc().date();
    m_fromDateEdit=new QDateTimeEdit(QDateTime(today,QTime(0,0),Qt::UTC),interval);
    m_toDateEdit=new QDateTimeEdit(QDateTime(today.addDays(1),QTime(0,0),Qt::UTC),interval);
    m_fromDateEdit->setObjectName("logbookFromUtc");m_toDateEdit->setObjectName("logbookUntilUtc");
    for(auto* edit:{m_fromDateEdit,m_toDateEdit}) {edit->setTimeSpec(Qt::UTC);edit->setCalendarPopup(true);edit->setDisplayFormat("yyyy-MM-dd HH:mm:ss");}
    m_fromEnabled->setChecked(true);m_toEnabled->setChecked(true);
    dates->addWidget(m_fromEnabled);dates->addWidget(m_fromDateEdit,1);dates->addWidget(m_toEnabled);dates->addWidget(m_toDateEdit,1);
    mainLayout->addWidget(interval); interval->hide();
    m_filterError=new QLabel(L("End UTC must be after start UTC."),this);m_filterError->setWordWrap(true);
    mainLayout->addWidget(m_filterError);m_filterError->hide();
    connect(m_period,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this,interval](int index){interval->setVisible(index==4);refreshTable();});

    auto* advancedToggle=new QToolButton(this);advancedToggle->setText(L("More filters"));advancedToggle->setCheckable(true);
    advancedToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);advancedToggle->setArrowType(Qt::RightArrow);
    searchRow->insertWidget(1,advancedToggle);
    auto* advanced=new QWidget(this);auto* advancedForm=new QGridLayout(advanced);advancedForm->setContentsMargins(0,0,0,0);
    m_callEdit=new QLineEdit(this);m_gridEdit=new QLineEdit(this);m_rstSentEdit=new QLineEdit(this);m_rstReceivedEdit=new QLineEdit(this);
    advancedForm->addWidget(new QLabel(L("Callsign"),this),0,0);advancedForm->addWidget(m_callEdit,0,1);
    advancedForm->addWidget(new QLabel(L("Grid"),this),0,2);advancedForm->addWidget(m_gridEdit,0,3);
    advancedForm->addWidget(new QLabel(L("RST sent"),this),1,0);advancedForm->addWidget(m_rstSentEdit,1,1);
    advancedForm->addWidget(new QLabel(L("RST rcvd"),this),1,2);advancedForm->addWidget(m_rstReceivedEdit,1,3);
    mainLayout->addWidget(advanced);advanced->hide();
    connect(advancedToggle,&QToolButton::toggled,this,[advanced,advancedToggle](bool on){advanced->setVisible(on);advancedToggle->setArrowType(on?Qt::DownArrow:Qt::RightArrow);});

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setMinimumWidth(220);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(0);
    m_table->setMinimumHeight(180);
    m_table->setObjectName("logbookTable");

    /*
     * ADIF fields have very predictable display lengths.  Keep the table
     * proportional to those expected lengths and use the whole visible table
     * width, instead of leaving a giant empty area on the right or truncating
     * the UTC/callsign columns.  Headers remain manually draggable.
     */
    QHeaderView *header = m_table->horizontalHeader();
    header->setStretchLastSection(true);
    header->setSectionResizeMode(QHeaderView::Interactive);
    header->setMinimumSectionSize(80);
    m_table->verticalHeader()->setVisible(false);

    m_table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setSortingEnabled(true);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    m_table->setStyleSheet(QStringLiteral(
        "QTableWidget::item { padding: 2px; }"
        "QHeaderView::section { padding: 3px; font-weight: 600; }"));

    mainLayout->addWidget(m_table,1);
    auto* selectionRow=new QHBoxLayout;
    auto* selectVisible=new QPushButton(L("Select visible"),this);
    auto* clearSelection=new QPushButton(L("Clear selection"),this);
    selectVisible->setObjectName("logbookSelectVisible"); clearSelection->setObjectName("logbookClearSelection");
    selectionRow->addWidget(selectVisible);selectionRow->addWidget(clearSelection);
    selectionRow->addWidget(m_summaryLabel,1);
    m_closeButton=new QPushButton(L("Close"),this);selectionRow->addWidget(m_closeButton);
    mainLayout->addLayout(selectionRow);
    connect(selectVisible,&QPushButton::clicked,this,&LogbookDialog::selectAllRows);
    connect(clearSelection,&QPushButton::clicked,m_table,&QTableWidget::clearSelection);
    m_actSelectAll->setShortcutContext(Qt::WidgetWithChildrenShortcut);addAction(m_actSelectAll);
    m_searchDelay=new QTimer(this);m_searchDelay->setSingleShot(true);m_searchDelay->setInterval(180);
    connect(m_searchDelay,&QTimer::timeout,this,&LogbookDialog::refreshTable);
    const auto lineEdits = {m_quickSearchEdit, m_callEdit, m_rstSentEdit, m_rstReceivedEdit, m_bandEdit, m_modeEdit, m_gridEdit};
    for (QLineEdit *edit : lineEdits) {
        connect(edit, &QLineEdit::textChanged,
                this, [this]{m_searchDelay->start();});
    }

    connect(m_fromEnabled, &QCheckBox::toggled,
            m_fromDateEdit, &QDateTimeEdit::setEnabled);
    connect(m_fromEnabled, &QCheckBox::toggled,
            this, &LogbookDialog::refreshTable);
    connect(m_fromDateEdit, &QDateTimeEdit::dateTimeChanged,
            this, &LogbookDialog::refreshTable);
    connect(m_toEnabled, &QCheckBox::toggled,
            m_toDateEdit, &QDateTimeEdit::setEnabled);
    connect(m_toEnabled, &QCheckBox::toggled,
            this, &LogbookDialog::refreshTable);
    connect(m_toDateEdit, &QDateTimeEdit::dateTimeChanged,
            this, &LogbookDialog::refreshTable);

    connect(m_clearSearchButton, &QPushButton::clicked,
            this, &LogbookDialog::clearSearch);
    connect(m_closeButton, &QPushButton::clicked,
            this, &LogbookDialog::accept);

    connect(m_actImport, &QAction::triggered, this, &LogbookDialog::importAdif);
    connect(m_actExportAll, &QAction::triggered, this, &LogbookDialog::exportAllAdif);
    connect(m_actRefresh, &QAction::triggered, this, &LogbookDialog::refreshTable);
    connect(m_actDelete, &QAction::triggered, this, &LogbookDialog::deleteSelectedRecords);
    connect(m_actPrint, &QAction::triggered, this, &LogbookDialog::printLogbook);
    connect(m_actExportResult, &QAction::triggered, this, &LogbookDialog::exportSearchResultAdif);
    connect(m_actExportSelectedAdif, &QAction::triggered, this, &LogbookDialog::exportSelectedAdif);
    connect(m_actExportSelectedCsv, &QAction::triggered, this, &LogbookDialog::saveSelectedRowsCsv);
    connect(m_actCopyCsv, &QAction::triggered, this, &LogbookDialog::copySelectedRowsCsv);
    connect(m_actCopyAdif, &QAction::triggered, this, &LogbookDialog::copySelectedRowsAdif);
    connect(m_actClearSearch, &QAction::triggered, this, &LogbookDialog::clearSearch);
    connect(m_actSelectAll, &QAction::triggered, this, &LogbookDialog::selectAllRows);
    connect(m_actColumns, &QAction::triggered, this, &LogbookDialog::configureVisibleFields);
    connect(m_table, &QTableWidget::customContextMenuRequested, this, &LogbookDialog::showTableContextMenu);
    if (m_table->selectionModel() != nullptr) {
        connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged,
                this, &LogbookDialog::updateSelectionActions);
    }
    connect(m_actPdf, &QAction::triggered, this, &LogbookDialog::savePdfLogbook);
    connect(m_actStatsPdf, &QAction::triggered, this, &LogbookDialog::saveStatisticsPdf);

    m_statusBar = new QStatusBar(this);
    m_statusBar->setSizeGripEnabled(false);
    mainLayout->addWidget(m_statusBar);

    refreshTable();
    updateSelectionActions();
    MadModemUi::scaleWidgetTree(this);
    if(auto* screen=QGuiApplication::primaryScreen()) resize(size().boundedTo(screen->availableGeometry().size()*0.9));
    QTimer::singleShot(0, this, &LogbookDialog::adjustColumnWidths);
}

void LogbookDialog::setTextTranslator(std::function<QString(const QString &)> translator)
{
    m_textTranslator = std::move(translator);
    retranslateVisibleText();
}

QString LogbookDialog::L(const QString &source) const
{
    return m_textTranslator ? m_textTranslator(source) : source;
}

void LogbookDialog::retranslateQObjectTree(QObject *object)
{
    if (object == nullptr) {
        return;
    }

    auto translatedFromProperty = [this](QObject *target, const char *propertyName, const QString &current) -> QString {
        if (current.isEmpty()) {
            return current;
        }
        const QByteArray key = QByteArrayLiteral("_mm_i18n_source_") + propertyName;
        QString source = target->property(key.constData()).toString();
        if (source.isEmpty()) {
            source = current;
            target->setProperty(key.constData(), source);
        }
        return L(source);
    };

    if (QAction *action = qobject_cast<QAction *>(object)) {
        if (!action->text().isEmpty()) {
            action->setText(translatedFromProperty(action, "text", action->text()));
        }
        if (!action->toolTip().isEmpty()) {
            action->setToolTip(translatedFromProperty(action, "tooltip", action->toolTip()));
        }
        if (!action->statusTip().isEmpty()) {
            action->setStatusTip(translatedFromProperty(action, "statustip", action->statusTip()));
        }
    } else if (QMenu *menu = qobject_cast<QMenu *>(object)) {
        if (!menu->title().isEmpty()) {
            menu->setTitle(translatedFromProperty(menu, "title", menu->title()));
        }
    } else if (QGroupBox *group = qobject_cast<QGroupBox *>(object)) {
        if (!group->title().isEmpty()) {
            group->setTitle(translatedFromProperty(group, "title", group->title()));
        }
        if (!group->toolTip().isEmpty()) {
            group->setToolTip(translatedFromProperty(group, "tooltip", group->toolTip()));
        }
    } else if (QAbstractButton *button = qobject_cast<QAbstractButton *>(object)) {
        if (!button->text().isEmpty()) {
            button->setText(translatedFromProperty(button, "text", button->text()));
        }
        if (!button->toolTip().isEmpty()) {
            button->setToolTip(translatedFromProperty(button, "tooltip", button->toolTip()));
        }
    } else if (QLabel *label = qobject_cast<QLabel *>(object)) {
        if (!label->text().isEmpty() && label->textFormat() != Qt::RichText) {
            label->setText(translatedFromProperty(label, "text", label->text()));
        }
        if (!label->toolTip().isEmpty()) {
            label->setToolTip(translatedFromProperty(label, "tooltip", label->toolTip()));
        }
    } else if (QLineEdit *edit = qobject_cast<QLineEdit *>(object)) {
        if (!edit->placeholderText().isEmpty()) {
            edit->setPlaceholderText(translatedFromProperty(edit, "placeholder", edit->placeholderText()));
        }
        if (!edit->toolTip().isEmpty()) {
            edit->setToolTip(translatedFromProperty(edit, "tooltip", edit->toolTip()));
        }
    }

    const auto children = object->children();
    for (QObject *child : children) {
        retranslateQObjectTree(child);
    }
}

void LogbookDialog::retranslateVisibleText()
{
    setWindowTitle(L("MadModem logbook"));
    retranslateQObjectTree(this);
    const QSignalBlocker block(m_period);
    const QStringList periods={L("All dates"),L("Today (UTC)"),L("Yesterday (UTC)"),L("Last 7 days"),L("Custom UTC interval")};
    for(int i=0;i<periods.size();++i) m_period->setItemText(i,periods[i]);
    if (m_table != nullptr && m_logbook != nullptr) {
        refreshTable();
    } else {
        updateStatusBar();
    }
    updateSelectionActions();
}

void LogbookDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
}

void LogbookDialog::adjustColumnWidths()
{
    if (m_table == nullptr || m_table->columnCount() <= 0) {
        return;
    }

    /*
     * Common ADIF columns stay readable; the complete ADIF payload is exposed
     * through additional per-field columns with horizontal scrolling.
     */
    const QMap<QString,QString> samples{{"UTC","2026-10-07 17:59:59"},{"CALL","IZ6NNH/P"},{"GRIDSQUARE","JN63HX"},
        {"RST_SENT","-24"},{"RST_RCVD","-24"},{"BAND","1.25m"},{"MODE","MSK144"},{"FREQ","144.174000"}};
    for(int col=0;col<m_visibleColumnKeys.size();++col) {
        const auto key=m_visibleColumnKeys[col];
        const int width=qMax(m_table->fontMetrics().horizontalAdvance(samples.value(key,"WWWWWWWWWW")),
            m_table->horizontalHeader()->fontMetrics().horizontalAdvance(columnLabel(key)))+24;
        m_table->setColumnWidth(col,qMax(64,width));
    }
}

LogbookSearchCriteria LogbookDialog::currentCriteria() const
{
    LogbookSearchCriteria criteria;
    criteria.anyText = m_quickSearchEdit != nullptr ? m_quickSearchEdit->text() : QString();
    criteria.callsign = m_callEdit != nullptr ? m_callEdit->text() : QString();
    criteria.rstSent = m_rstSentEdit != nullptr ? m_rstSentEdit->text() : QString();
    criteria.rstReceived = m_rstReceivedEdit != nullptr ? m_rstReceivedEdit->text() : QString();
    criteria.band = m_bandEdit != nullptr ? m_bandEdit->text() : QString();
    criteria.mode = m_modeEdit != nullptr ? m_modeEdit->text() : QString();
    criteria.grid = m_gridEdit != nullptr ? m_gridEdit->text() : QString();
    return criteria;
}


QStringList LogbookDialog::allAvailableColumnKeys() const
{
    QStringList keys = {
        QStringLiteral("UTC"), QStringLiteral("CALL"), QStringLiteral("GRIDSQUARE"),
        QStringLiteral("RST_SENT"), QStringLiteral("RST_RCVD"), QStringLiteral("BAND"),
        QStringLiteral("MODE"), QStringLiteral("FREQ"), QStringLiteral("NAME"),
        QStringLiteral("QTH"), QStringLiteral("COMMENT")
    };

    if (m_logbook != nullptr) {
        const QStringList fields = m_logbook->allAdifFieldNames();
        for (const QString &field : fields) {
            const QString key = field.trimmed().toUpper();
            if (!key.isEmpty() && !keys.contains(key)) {
                keys.append(key);
            }
        }
    }
    return keys;
}

bool LogbookDialog::fieldHiddenByDefault(const QString &field) const
{
    const QString key = field.trimmed().toUpper();
    if (key.isEmpty()) return true;
    if (key == QStringLiteral("LAT") || key == QStringLiteral("LON") ||
        key == QStringLiteral("RX_PWR") || key == QStringLiteral("TX_PWR") ||
        key == QStringLiteral("STATION_CALLSIGN") || key == QStringLiteral("IOTA") ||
        key == QStringLiteral("CNT") || key == QStringLiteral("STATE") ||
        key == QStringLiteral("CONTEST_ID") || key == QStringLiteral("SRX") ||
        key == QStringLiteral("STX") || key == QStringLiteral("PFX")) {
        return true;
    }
    if (key.startsWith(QStringLiteral("APP_QRZ_")) ||
        key.startsWith(QStringLiteral("MY_")) ||
        key.startsWith(QStringLiteral("QSL_"))) {
        return true;
    }
    if (key.contains(QStringLiteral("_QSO_")) ||
        key.contains(QStringLiteral("_QSL_"))) {
        return true;
    }
    return false;
}

QStringList LogbookDialog::defaultVisibleColumnKeys() const
{
    QStringList keys = {"UTC","CALL","GRIDSQUARE","RST_SENT","RST_RCVD","BAND","MODE","FREQ"};

    return keys;
}

QStringList LogbookDialog::visibleColumnKeys() const
{
    const QStringList available = allAvailableColumnKeys();
    QStringList configured;
    if (m_settings != nullptr && m_settings->logbookVisibleFieldsConfigured) {
        for (const QString &field : m_settings->logbookVisibleFields) {
            const QString key = field.trimmed().toUpper();
            if (!key.isEmpty() && available.contains(key) && !configured.contains(key)) {
                configured.append(key);
            }
        }
        if (!configured.isEmpty()) {
            return configured;
        }
    }
    return defaultVisibleColumnKeys();
}

QString LogbookDialog::columnLabel(const QString &key) const
{
    const QString k = key.trimmed().toUpper();
    if (k == QStringLiteral("UTC")) return QStringLiteral("UTC");
    if (k == QStringLiteral("CALL")) return L("Callsign");
    if (k == QStringLiteral("GRIDSQUARE")) return L("Grid");
    if (k == QStringLiteral("RST_SENT")) return L("RST sent");
    if (k == QStringLiteral("RST_RCVD")) return L("RST rcvd");
    if (k == QStringLiteral("BAND")) return L("Band");
    if (k == QStringLiteral("MODE")) return L("Mode");
    if (k == QStringLiteral("FREQ")) return L("Freq");
    if (k == QStringLiteral("NAME")) return L("Name");
    if (k == QStringLiteral("QTH")) return QStringLiteral("QTH");
    if (k == QStringLiteral("COMMENT")) return L("Comment");
    return k;
}

QString LogbookDialog::columnValue(const LogbookEntry &entry, const QString &key) const
{
    const QString k = key.trimmed().toUpper();
    if (k == QStringLiteral("UTC")) {
        return entry.utc.isValid() ? entry.utc.toUTC().toString("yyyy-MM-dd HH:mm:ss") : QString();
    }
    if (k == QStringLiteral("CALL")) return entry.callsign;
    if (k == QStringLiteral("GRIDSQUARE")) return entry.grid;
    if (k == QStringLiteral("RST_SENT")) return entry.rstSent;
    if (k == QStringLiteral("RST_RCVD")) return entry.rstReceived;
    if (k == QStringLiteral("BAND")) return entry.band;
    if (k == QStringLiteral("MODE")) return entry.mode;
    if (k == QStringLiteral("FREQ")) return entry.freq;
    if (k == QStringLiteral("NAME")) return entry.name;
    if (k == QStringLiteral("QTH")) return entry.qth;
    if (k == QStringLiteral("COMMENT")) return entry.comment;
    return entry.adifFields.value(k);
}

void LogbookDialog::refreshTable()
{
    if (m_logbook == nullptr || m_table == nullptr) {
        return;
    }

    if(m_searchDelay) m_searchDelay->stop();
    m_table->clearSelection();
    m_visibleColumnKeys = visibleColumnKeys();
    const QStringList primaryKeys = {
        QStringLiteral("UTC"), QStringLiteral("CALL"), QStringLiteral("GRIDSQUARE"),
        QStringLiteral("RST_SENT"), QStringLiteral("RST_RCVD"), QStringLiteral("BAND"),
        QStringLiteral("MODE"), QStringLiteral("FREQ"), QStringLiteral("NAME"),
        QStringLiteral("QTH"), QStringLiteral("COMMENT")
    };
    m_adifExtraColumns.clear();
    for (const QString &key : m_visibleColumnKeys) {
        if (!primaryKeys.contains(key)) {
            m_adifExtraColumns.append(key);
        }
    }

    QStringList headers;
    for (const QString &key : m_visibleColumnKeys) {
        headers.append(columnLabel(key));
    }

    m_table->setSortingEnabled(false);
    m_table->setColumnCount(headers.size());
    m_table->setHorizontalHeaderLabels(headers);

    m_displayedRecords = m_logbook->filteredRecords(currentCriteria());
    // Band/mode selectors are exact. A 2m activity must not include 12m QSOs.
    const QString band=m_bandEdit->text().trimmed(), mode=m_modeEdit->text().trimmed();
    m_displayedRecords.erase(std::remove_if(m_displayedRecords.begin(),m_displayedRecords.end(),[&](const LogbookEntry& e){
        return (!band.isEmpty() && e.band.trimmed().compare(band,Qt::CaseInsensitive)!=0) ||
               (!mode.isEmpty() && e.mode.trimmed().compare(mode,Qt::CaseInsensitive)!=0);
    }),m_displayedRecords.end());
    const int period=m_period->currentIndex();
    QDateTime from,until;
    const auto today=QDateTime(QDateTime::currentDateTimeUtc().date(),QTime(0,0),Qt::UTC);
    if(period==1) {from=today;until=today.addDays(1);}
    else if(period==2) {from=today.addDays(-1);until=today;}
    else if(period==3) {from=today.addDays(-6);until=today.addDays(1);}
    else if(period==4) {
        if(m_fromEnabled->isChecked()) from=m_fromDateEdit->dateTime();
        if(m_toEnabled->isChecked()) until=m_toDateEdit->dateTime();
    }
    const bool invalidInterval=from.isValid() && until.isValid() && until<=from;
    if(from.isValid() || until.isValid()) {
        m_displayedRecords.erase(std::remove_if(m_displayedRecords.begin(),m_displayedRecords.end(),[&](const LogbookEntry& e){
            return !e.utc.isValid() || (from.isValid() && e.utc<from) || (until.isValid() && e.utc>=until);
        }),m_displayedRecords.end());
    }
    m_filterError->setText(invalidInterval?L("End UTC must be after start UTC."):L("No QSOs match the current filters."));
    m_filterError->setVisible(invalidInterval || m_displayedRecords.isEmpty());
    m_table->setRowCount(m_displayedRecords.size());

    for (int row = 0; row < m_displayedRecords.size(); ++row) {
        const LogbookEntry &entry = m_displayedRecords.at(row);
        for (int col = 0; col < m_visibleColumnKeys.size(); ++col) {
            QTableWidgetItem *item = new QTableWidgetItem(columnValue(entry, m_visibleColumnKeys.at(col)));
            item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
            item->setData(Qt::UserRole, row);
            m_table->setItem(row, col, item);
        }
    }

    if (m_summaryLabel != nullptr) {
        m_summaryLabel->setText(QString("%1 / %2 %3")
                                .arg(m_displayedRecords.size())
                                .arg(m_logbook->count())
                                .arg(L("QSOs shown")));
    }
    m_table->setSortingEnabled(true);
    updateStatusBar();
    updateSelectionActions();
    adjustColumnWidths();
}

void LogbookDialog::clearSearch()
{
    const bool oldFrom = m_fromEnabled != nullptr && m_fromEnabled->blockSignals(true);
    const bool oldTo = m_toEnabled != nullptr && m_toEnabled->blockSignals(true);

    if (m_quickSearchEdit != nullptr) m_quickSearchEdit->clear();
    if (m_callEdit != nullptr) m_callEdit->clear();
    if (m_rstSentEdit != nullptr) m_rstSentEdit->clear();
    if (m_rstReceivedEdit != nullptr) m_rstReceivedEdit->clear();
    if (m_bandEdit != nullptr) m_bandEdit->clear();
    if (m_modeEdit != nullptr) m_modeEdit->clear();
    if (m_gridEdit != nullptr) m_gridEdit->clear();
    if (m_fromEnabled != nullptr) m_fromEnabled->setChecked(false);
    if (m_toEnabled != nullptr) m_toEnabled->setChecked(false);
    if (m_fromDateEdit != nullptr) m_fromDateEdit->setEnabled(false);
    if (m_toDateEdit != nullptr) m_toDateEdit->setEnabled(false);

    if (m_fromEnabled != nullptr) m_fromEnabled->blockSignals(oldFrom);
    if (m_toEnabled != nullptr) m_toEnabled->blockSignals(oldTo);

    m_period->setCurrentIndex(0);
    m_fromEnabled->setChecked(true);m_toEnabled->setChecked(true);
    m_fromDateEdit->setEnabled(true);m_toDateEdit->setEnabled(true);
    refreshTable();
}

void LogbookDialog::importAdif()
{
    if (m_logbook == nullptr) {
        return;
    }

    const QString fileName = QFileDialog::getOpenFileName(
        this,
        L("Import ADIF logbook"),
        QString(),
        L("ADIF logbook (*.adi *.adif);;All files (*)")
        );
    if (fileName.isEmpty()) {
        return;
    }

    if (!m_store) return;
    setEnabled(false);
    QPointer<LogbookDialog> self(this);
    m_store->importFile(fileName, [self](int imported, const QString &error) {
        if (!self) return;
        self->setEnabled(true);
        if (imported < 0) {
            QMessageBox::warning(self, self->L("Import ADIF"), self->L("Import failed:") + " " + error);
            return;
        }
        self->refreshTable();
        emit self->logbookChanged();
        QMessageBox::information(self, self->L("Import ADIF"), self->L("Imported %1 QSO records.").arg(imported));
    });
}

QVector<LogbookEntry> LogbookDialog::selectedRecords() const
{
    QVector<LogbookEntry> result;
    if (m_table == nullptr || m_table->selectionModel() == nullptr) {
        return result;
    }

    const QModelIndexList selectedRows = m_table->selectionModel()->selectedRows();
    QSet<int> seenRows;
    for (const QModelIndex &index : selectedRows) {
        const int tableRow = index.row();
        if (tableRow < 0 || seenRows.contains(tableRow)) {
            continue;
        }
        QTableWidgetItem *anchor = m_table->item(tableRow, 0);
        const int recordRow = anchor != nullptr ? anchor->data(Qt::UserRole).toInt() : tableRow;
        if (recordRow < 0 || recordRow >= m_displayedRecords.size()) {
            continue;
        }
        seenRows.insert(tableRow);
        result.append(m_displayedRecords.at(recordRow));
    }
    return result;
}

void LogbookDialog::updateStatusBar()
{
    if (m_statusBar == nullptr) {
        return;
    }
    const int total = m_logbook != nullptr ? m_logbook->count() : 0;
    const QString message=QFileInfo(m_logbook?m_logbook->fileName():QString()).fileName()+QString(" — %1 QSO").arg(total);
    m_statusBar->showMessage(message);
}

void LogbookDialog::updateSelectionActions()
{
    const bool hasSelection = !selectedRecords().isEmpty();
    for (QAction *action : {m_actExportSelectedAdif, m_actExportSelectedCsv,
                            m_actCopyCsv, m_actCopyAdif, m_actDelete}) {
        if (action != nullptr) {
            action->setEnabled(hasSelection);
        }
    }
    if(hasSelection && !m_hadSelection) m_outputScope->setCurrentIndex(1);
    if(!hasSelection && m_outputScope->currentIndex()==1) m_outputScope->setCurrentIndex(0);
    m_hadSelection=hasSelection;
    updateOutputScope();
    updateStatusBar();
}

QVector<LogbookEntry> LogbookDialog::outputRecords() const
{
    if(m_outputScope->currentIndex()==1) return selectedRecords();
    if(m_outputScope->currentIndex()==2 && m_logbook) return m_logbook->records();
    return m_displayedRecords;
}

void LogbookDialog::updateOutputScope()
{
    const QSignalBlocker blocker(m_outputScope);
    const int selected=selectedRecords().size();
    m_outputScope->setItemText(0,L("Visible QSOs")+QString(" (%1)").arg(m_displayedRecords.size()));
    m_outputScope->setItemText(1,L("Selected QSOs")+QString(" (%1)").arg(selected));
    m_outputScope->setItemText(2,L("Full logbook")+QString(" (%1)").arg(m_logbook?m_logbook->count():0));
    const bool available=!outputRecords().isEmpty();
    m_actExport->setEnabled(available);
    for(auto* action:{m_actPrint,m_actPdf,m_actStatsPdf}) action->setEnabled(available);
    if(m_summaryLabel) m_summaryLabel->setText(L("Shown")+QString(": %1   ").arg(m_displayedRecords.size())+L("Selected")+QString(": %1").arg(selected));
}

void LogbookDialog::showTableContextMenu(const QPoint &pos)
{
    QMenu menu(this);
    menu.addAction(m_actCopyCsv);
    menu.addAction(m_actCopyAdif);
    menu.addSeparator();
    menu.addAction(m_actExportSelectedAdif);
    menu.addAction(m_actExportSelectedCsv);
    menu.addSeparator();
    menu.addAction(m_actSelectAll);
    menu.addAction(m_actColumns);
    menu.addAction(m_actStatsPdf);
    menu.addAction(m_actDelete);
    menu.exec(m_table->viewport()->mapToGlobal(pos));
}

void LogbookDialog::selectAllRows()
{
    if (m_table != nullptr) {
        m_table->selectAll();
        updateSelectionActions();
    }
}

void LogbookDialog::configureVisibleFields()
{
    const QStringList available = allAvailableColumnKeys();
    QStringList current = visibleColumnKeys();

    QDialog dialog(this);
    dialog.setWindowTitle(L("Logbook visible/print fields"));
    dialog.resize(920, 620);

    QVBoxLayout *mainLayout = new QVBoxLayout(&dialog);
    QLabel *intro = new QLabel(L("Choose which ADIF fields are shown in the logbook table and included in CSV/PDF/print output. Hidden fields are still preserved in ADIF import/export."), &dialog);
    intro->setWordWrap(true);
    mainLayout->addWidget(intro);

    QScrollArea *scroll = new QScrollArea(&dialog);
    scroll->setWidgetResizable(true);
    QWidget *panel = new QWidget(scroll);
    QGridLayout *grid = new QGridLayout(panel);
    grid->setContentsMargins(8, 8, 8, 8);
    grid->setHorizontalSpacing(24);
    grid->setVerticalSpacing(4);

    QVector<QCheckBox *> boxes;
    boxes.reserve(available.size());
    const int rowsPerColumn = qMax(1, (available.size() + 2) / 3);
    for (int i = 0; i < available.size(); ++i) {
        const QString key = available.at(i);
        const int groupCol = i / rowsPerColumn;
        const int row = i % rowsPerColumn;
        const int col = groupCol * 2;
        QLabel *label = new QLabel(columnLabel(key) + QStringLiteral("  [") + key + QStringLiteral("]"), panel);
        QCheckBox *box = new QCheckBox(panel);
        box->setChecked(current.contains(key));
        box->setProperty("adifKey", key);
        if (fieldHiddenByDefault(key)) {
            label->setToolTip(L("Usually noisy/importer-specific ADIF field. Hidden by default but can be enabled here."));
            box->setToolTip(label->toolTip());
        }
        grid->addWidget(label, row, col);
        grid->addWidget(box, row, col + 1);
        boxes.append(box);
    }
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(2, 1);
    grid->setColumnStretch(4, 1);
    panel->setLayout(grid);
    scroll->setWidget(panel);
    mainLayout->addWidget(scroll, 1);

    QHBoxLayout *quickLayout = new QHBoxLayout();
    QPushButton *defaultsButton = new QPushButton(L("Defaults"), &dialog);
    QPushButton *allButton = new QPushButton(L("All"), &dialog);
    QPushButton *noneButton = new QPushButton(L("None"), &dialog);
    quickLayout->addWidget(defaultsButton);
    quickLayout->addWidget(allButton);
    quickLayout->addWidget(noneButton);
    quickLayout->addStretch(1);
    mainLayout->addLayout(quickLayout);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    mainLayout->addWidget(buttons);

    QObject::connect(defaultsButton, &QPushButton::clicked, &dialog, [&]() {
        const QStringList defaults = defaultVisibleColumnKeys();
        for (QCheckBox *box : boxes) {
            box->setChecked(defaults.contains(box->property("adifKey").toString()));
        }
    });
    QObject::connect(allButton, &QPushButton::clicked, &dialog, [&]() {
        for (QCheckBox *box : boxes) box->setChecked(true);
    });
    QObject::connect(noneButton, &QPushButton::clicked, &dialog, [&]() {
        for (QCheckBox *box : boxes) box->setChecked(false);
    });
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QStringList selected;
    for (QCheckBox *box : boxes) {
        if (box->isChecked()) {
            const QString key = box->property("adifKey").toString().trimmed().toUpper();
            if (!key.isEmpty() && !selected.contains(key)) {
                selected.append(key);
            }
        }
    }

    if (m_settings != nullptr) {
        m_settings->logbookVisibleFieldsConfigured = true;
        m_settings->logbookVisibleFields = selected;
        m_settings->save();
    }
    refreshTable();
}


QString LogbookDialog::csvEscape(const QString &value) const
{
    QString escaped = value;
    escaped.replace('"', "\"\"");
    if (escaped.contains(',') || escaped.contains('"') || escaped.contains('\n') || escaped.contains('\r')) {
        escaped = '"' + escaped + '"';
    }
    return escaped;
}

QString LogbookDialog::csvForRecords(const QVector<LogbookEntry> &records) const
{
    const QStringList keys = m_visibleColumnKeys.isEmpty() ? visibleColumnKeys() : m_visibleColumnKeys;
    QStringList headers;
    for (const QString &key : keys) {
        headers.append(columnLabel(key));
    }

    QString csv;
    QStringList escapedHeaders;
    for (const QString &header : headers) {
        escapedHeaders.append(csvEscape(header));
    }
    csv += escapedHeaders.join(',') + "\n";

    for (const LogbookEntry &entry : records) {
        QStringList escaped;
        for (const QString &key : keys) {
            escaped.append(csvEscape(columnValue(entry, key)));
        }
        csv += escaped.join(',') + "\n";
    }
    return csv;
}

bool LogbookDialog::exportRecordsCsv(const QVector<LogbookEntry> &records,
                                     const QString &dialogTitle,
                                     const QString &defaultBaseName,
                                     const QString &successLabel)
{
    if (records.isEmpty()) {
        QMessageBox::information(this, L(dialogTitle), L("No QSO records to export."));
        return false;
    }

    QString defaultName = defaultBaseName.endsWith(".csv", Qt::CaseInsensitive)
                          ? defaultBaseName
                          : defaultBaseName + ".csv";
    QString fileName = QFileDialog::getSaveFileName(
        this,
        L(dialogTitle),
        defaultName,
        L("CSV file (*.csv);;All files (*)"));
    if (fileName.isEmpty()) {
        return false;
    }
    if (!fileName.endsWith(".csv", Qt::CaseInsensitive)) {
        fileName += ".csv";
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, L(dialogTitle), L("Export failed:") + " " + file.errorString());
        return false;
    }
    QTextStream out(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    out.setEncoding(QStringConverter::Utf8);
#else
    out.setCodec("UTF-8");
#endif
    out << csvForRecords(records);
    if (file.error() != QFile::NoError) {
        QMessageBox::warning(this, L(dialogTitle), L("Export failed:") + " " + file.errorString());
        return false;
    }

    QMessageBox::information(this,
                             L(dialogTitle),
                             L("%1 exported successfully (%2 QSO records).").arg(L(successLabel)).arg(records.size()));
    return true;
}

void LogbookDialog::copySelectedRowsCsv()
{
    const QVector<LogbookEntry> records = selectedRecords();
    if (records.isEmpty()) {
        QMessageBox::information(this, L("Copy selected rows"), L("Select one or more QSO records first."));
        return;
    }
    QApplication::clipboard()->setText(csvForRecords(records));
    if (m_statusBar != nullptr) {
        m_statusBar->showMessage(L("Copied %1 selected QSO row(s) as CSV.").arg(records.size()), 5000);
    }
}

void LogbookDialog::copySelectedRowsAdif()
{
    const QVector<LogbookEntry> records = selectedRecords();
    if (records.isEmpty()) {
        QMessageBox::information(this, L("Copy selected rows"), L("Select one or more QSO records first."));
        return;
    }
    QApplication::clipboard()->setText(AdifLogbook::recordsToAdif(records, QStringLiteral("Copied by MadModem")));
    if (m_statusBar != nullptr) {
        m_statusBar->showMessage(L("Copied %1 selected QSO row(s) as ADIF.").arg(records.size()), 5000);
    }
}

void LogbookDialog::saveSelectedRowsCsv()
{
    exportRecordsCsv(selectedRecords(),
                     "Save selected rows as CSV",
                     "MadModem_logbook_selected.csv",
                     "Selected QSOs");
}


bool LogbookDialog::exportRecords(const QVector<LogbookEntry> &records,
                                  const QString &dialogTitle,
                                  const QString &defaultBaseName,
                                  const QString &successLabel)
{
    if (m_logbook == nullptr) {
        return false;
    }
    if (records.isEmpty()) {
        QMessageBox::information(this, L(dialogTitle), L("No QSO records to export."));
        return false;
    }

    const auto& exportList=records;
    const QString adjustedBaseName=defaultBaseName;
    const QString adjustedSuccessLabel=successLabel;
    const QString defaultName = adjustedBaseName.endsWith(".adi", Qt::CaseInsensitive)
                                ? adjustedBaseName
                                : adjustedBaseName + ".adi";
    QString selectedFilter;
    const QString fileName = QFileDialog::getSaveFileName(
        this, L(dialogTitle)+QString(" — %1 QSO").arg(records.size()), defaultName,
        L("ADIF logbook (*.adi);;ADIF logbook (*.adif);;All files (*)"), &selectedFilter);
    if (fileName.isEmpty()) {
        return false;
    }

    QString error;
    if (!m_logbook->exportRecordsAdif(fileName, exportList, &error)) {
        QMessageBox::warning(this, L(dialogTitle), L("Export failed:") + " " + error);
        return false;
    }

    m_statusBar->showMessage(L("%1 exported successfully (%2 QSO records).")
        .arg(L(adjustedSuccessLabel)).arg(exportList.size())+" — "+fileName,15000);
    return true;
}

void LogbookDialog::exportCabrilloWizard()
{
    if (!m_logbook || m_logbook->records().isEmpty()) {
        QMessageBox::information(this, L("Export Cabrillo"), L("No QSO records to export."));
        return;
    }
    ContestExportWizard wizard(m_logbook->records(), this);
    if (wizard.exec() != QDialog::Accepted) return;
    const QByteArray generated = wizard.generatedData();
    if (generated.isEmpty()) return;
    const QString path = QFileDialog::getSaveFileName(this, L("Save Cabrillo log"),
        wizard.suggestedFileName(), L("Cabrillo log (*.log);;All files (*)"));
    if (path.isEmpty()) return;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(generated) != generated.size() || !file.commit())
        QMessageBox::warning(this, L("Cabrillo"), file.errorString());
    else
        QMessageBox::information(this, L("Cabrillo"), L("Cabrillo log exported successfully."));
}

void LogbookDialog::exportAllAdif()
{
    if (m_logbook == nullptr) {
        return;
    }
    exportRecords(m_logbook->records(),
                  L("Export all ADIF"),
                  QStringLiteral("MadModem_logbook_all.adi"),
                  "Full logbook");
}

void LogbookDialog::exportSearchResultAdif()
{
    exportRecords(m_displayedRecords,
                  "Export search result ADIF",
                  "MadModem_logbook_search_result.adi",
                  "Search result");
}

void LogbookDialog::exportSelectedAdif()
{
    exportRecords(selectedRecords(),
                  "Export selected ADIF",
                  "MadModem_logbook_selected.adi",
                  "Selected QSOs");
}

void LogbookDialog::deleteSelectedRecords()
{
    if (m_logbook == nullptr) {
        return;
    }

    const QVector<LogbookEntry> records = selectedRecords();
    if (records.isEmpty()) {
        QMessageBox::information(this, L("Delete selected QSOs"), L("Select one or more QSO records to delete."));
        return;
    }

    const int answer = QMessageBox::question(
        this,
        L("Delete selected QSOs"),
        L("Delete %1 selected QSO record(s) from the logbook?\n\nThis rewrites the ADIF file and cannot be undone from MadModem.").arg(records.size()),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    if (!m_store) return;
    setEnabled(false);
    QPointer<LogbookDialog> self(this);
    m_store->remove(records, [self](int removed, const QString &error) {
        if (!self) return;
        self->setEnabled(true);
        if (removed < 0) {
            QMessageBox::warning(self, self->L("Delete selected QSOs"), self->L("Delete failed:") + " " + error);
            return;
        }
        self->refreshTable();
        emit self->logbookChanged();
        QMessageBox::information(self, self->L("Delete selected QSOs"), self->L("Deleted %1 QSO record(s).").arg(removed));
    });
}

QVector<LogbookEntry> LogbookDialog::chooseOutputRecords(const QString &dialogTitle,
                                                           QString *scopeLabel,
                                                           QString *defaultBaseName)
{
    if (m_logbook == nullptr) {
        return {};
    }

    Q_UNUSED(dialogTitle)
    if(scopeLabel) *scopeLabel=m_outputScope->currentText();
    if(defaultBaseName) *defaultBaseName="MadModem_QSOs";
    return outputRecords();
}

QString LogbookDialog::htmlForRecords(const QVector<LogbookEntry> &records,
                                      const QString &scopeLabel,
                                      QProgressDialog *progress) const
{
    const QStringList keys = m_visibleColumnKeys.isEmpty() ? visibleColumnKeys() : m_visibleColumnKeys;

    auto estimateUnits = [this](const QString &key) -> int {
        const QString k = key.trimmed().toUpper();
        if (k == QStringLiteral("UTC")) return 17;
        if (k == QStringLiteral("CALL")) return 10;
        if (k == QStringLiteral("GRIDSQUARE")) return 8;
        if (k == QStringLiteral("RST_SENT") || k == QStringLiteral("RST_RCVD")) return 7;
        if (k == QStringLiteral("BAND") || k == QStringLiteral("MODE")) return 6;
        if (k == QStringLiteral("FREQ")) return 8;
        if (k == QStringLiteral("NAME") || k == QStringLiteral("QTH")) return 12;
        if (k == QStringLiteral("COMMENT")) return 18;
        return qBound(7, columnLabel(k).size() + 2, 15);
    };

    auto clippedValue = [](QString value, const QString &key) -> QString {
        const QString k = key.trimmed().toUpper();
        int maxLen = 36;
        if (k == QStringLiteral("UTC")) maxLen = 19;
        else if (k == QStringLiteral("CALL")) maxLen = 14;
        else if (k == QStringLiteral("COMMENT")) maxLen = 58;
        else if (k == QStringLiteral("NAME") || k == QStringLiteral("QTH")) maxLen = 32;
        if (value.size() > maxLen) {
            value = value.left(qMax(1, maxLen - 1)) + QStringLiteral("…");
        }
        return value;
    };

    QStringList anchors;
    for (const QString &anchor : {QStringLiteral("UTC"), QStringLiteral("CALL")}) {
        if (keys.contains(anchor)) {
            anchors.append(anchor);
        }
    }

    QStringList payload;
    for (const QString &key : keys) {
        if (!anchors.contains(key)) {
            payload.append(key);
        }
    }

    QVector<QStringList> columnBlocks;
    const int maxUnitsPerBlock = 125;
    QStringList current = anchors;
    int currentUnits = 0;
    for (const QString &anchor : anchors) {
        currentUnits += estimateUnits(anchor);
    }

    for (const QString &key : payload) {
        const int units = estimateUnits(key);
        const bool hasPayload = current.size() > anchors.size();
        if (hasPayload && currentUnits + units > maxUnitsPerBlock) {
            columnBlocks.append(current);
            current = anchors;
            currentUnits = 0;
            for (const QString &anchor : anchors) {
                currentUnits += estimateUnits(anchor);
            }
        }
        current.append(key);
        currentUnits += units;
    }
    if (!current.isEmpty()) {
        columnBlocks.append(current);
    }
    if (columnBlocks.isEmpty()) {
        columnBlocks.append(keys);
    }

    int progressValue = 0;
    const int progressMax = qMax(1, records.size() * qMax(1, columnBlocks.size()));
    if (progress) {
        progress->setRange(0, progressMax);
        progress->setValue(0);
        progress->setLabelText(L("Preparing logbook table..."));
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }

    QString html;
    html += "<html><head><meta charset=\"utf-8\"><style>";
    html += "@page{size:landscape;margin:8mm;}";
    html += "body{font-family:sans-serif;font-size:7.2pt;}";
    html += "h2{margin:0 0 2px 0;font-size:12pt;}";
    html += "p{margin:0 0 5px 0;font-size:7.2pt;}";
    html += ".block{page-break-inside:auto;}";
    html += ".block.next{page-break-before:always;}";
    html += ".blocktitle{font-weight:bold;margin:4px 0 4px 0;font-size:8pt;}";
    html += "table{border-collapse:collapse;width:100%;table-layout:fixed;}";
    html += "th,td{border:0.45pt solid #777;padding:2px;text-align:left;vertical-align:top;overflow:hidden;}";
    html += "th{background:#ddd;font-weight:bold;}";
    html += "td{white-space:normal;}";
    html += "</style></head><body>";

    for (int blockIndex = 0; blockIndex < columnBlocks.size(); ++blockIndex) {
        const QStringList blockKeys = columnBlocks.at(blockIndex);
        html += QString("<div class=\"block%1\">").arg(blockIndex > 0 ? QStringLiteral(" next") : QString());
        html += "<h2>" + L("MadModem logbook").toHtmlEscaped() + "</h2>";
        html += QString("<p>%1: %2 &nbsp;&nbsp; %3: %4 &nbsp;&nbsp; %5: %6 UTC</p>")
                    .arg(L("Scope").toHtmlEscaped())
                    .arg(scopeLabel.toHtmlEscaped())
                    .arg(L("Records").toHtmlEscaped())
                    .arg(records.size())
                    .arg(L("Printed").toHtmlEscaped())
                    .arg(QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd HH:mm:ss"));
        if (columnBlocks.size() > 1) {
            html += QString("<div class=\"blocktitle\">%1 %2/%3 — %4</div>")
                        .arg(L("Field block").toHtmlEscaped())
                        .arg(blockIndex + 1)
                        .arg(columnBlocks.size())
                        .arg(L("the same QSO rows continue on the following field blocks").toHtmlEscaped());
        }
        html += "<table><thead><tr>";
        for (const QString &key : blockKeys) {
            html += "<th>" + columnLabel(key).toHtmlEscaped() + "</th>";
        }
        html += "</tr></thead><tbody>";
        for (const LogbookEntry &entry : records) {
            html += "<tr>";
            for (const QString &key : blockKeys) {
                html += "<td>" + clippedValue(columnValue(entry, key), key).toHtmlEscaped() + "</td>";
            }
            html += "</tr>";
            if (progress) {
                ++progressValue;
                if ((progressValue % 100) == 0 || progressValue == progressMax) {
                    progress->setValue(qMin(progressValue, progressMax));
                    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
                    if (progress->wasCanceled()) {
                        return QString();
                    }
                }
            }
        }
        html += "</tbody></table></div>";
    }
    html += "</body></html>";
    return html;
}

bool LogbookDialog::printRecordsToPrinter(const QVector<LogbookEntry> &records,
                                          const QString &scopeLabel)
{
    if (records.isEmpty()) {
        QMessageBox::information(this, L("Print logbook"), L("No QSO records to print."));
        return false;
    }

    QPrinter printer(QPrinter::HighResolution);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    printer.setPageOrientation(QPageLayout::Landscape);
#else
    printer.setOrientation(QPrinter::Landscape);
#endif
    printer.setDocName(L("MadModem logbook") + " - " + scopeLabel);

    QPrintDialog dialog(&printer, this);
    dialog.setWindowTitle(L("Print MadModem logbook"));
    dialog.setOption(QAbstractPrintDialog::PrintToFile, true);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    QProgressDialog progress(L("Preparing logbook printout..."), L("Cancel"), 0, 0, this);
    progress.setWindowTitle(L("Print logbook"));
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setMinimumDuration(250);
    progress.setAutoClose(false);
    progress.setAutoReset(false);
    progress.show();
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    QTextDocument document;
    const QRectF pageRect = printer.pageRect(QPrinter::Point);
    if (pageRect.isValid()) {
        document.setPageSize(pageRect.size());
        document.setTextWidth(pageRect.width());
    }
    const QString html = htmlForRecords(records, scopeLabel, &progress);
    if (html.isEmpty() && progress.wasCanceled()) {
        progress.close();
        return false;
    }
    progress.setLabelText(L("Sending pages to printer..."));
    progress.setRange(0, 0);
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    document.setHtml(html);
    document.print(&printer);
    progress.close();
    return true;
}


QString LogbookDialog::htmlForStatistics(const QVector<LogbookEntry> &records,
                                         const QString &scopeLabel) const
{
    auto value = [](const LogbookEntry &entry, const QString &key) -> QString {
        const QString k = key.trimmed().toUpper();
        if (k == QStringLiteral("CALL")) return entry.callsign;
        if (k == QStringLiteral("BAND")) return entry.band;
        if (k == QStringLiteral("MODE")) return entry.mode;
        if (k == QStringLiteral("GRIDSQUARE")) return entry.grid;
        if (k == QStringLiteral("COUNTRY")) return entry.country;
        if (k == QStringLiteral("FREQ")) return entry.freq;
        return entry.adifFields.value(k);
    };

    auto normalized = [](QString text, const QString &fallback = QStringLiteral("—")) -> QString {
        text = text.trimmed();
        return text.isEmpty() ? fallback : text;
    };

    auto bandFromFrequency = [](double mhz) -> QString {
        if (mhz >= 1.8 && mhz < 2.0) return QStringLiteral("160m");
        if (mhz >= 3.5 && mhz < 4.0) return QStringLiteral("80m");
        if (mhz >= 5.0 && mhz < 5.5) return QStringLiteral("60m");
        if (mhz >= 7.0 && mhz < 7.3) return QStringLiteral("40m");
        if (mhz >= 10.1 && mhz < 10.15) return QStringLiteral("30m");
        if (mhz >= 14.0 && mhz < 14.35) return QStringLiteral("20m");
        if (mhz >= 18.068 && mhz < 18.168) return QStringLiteral("17m");
        if (mhz >= 21.0 && mhz < 21.45) return QStringLiteral("15m");
        if (mhz >= 24.89 && mhz < 24.99) return QStringLiteral("12m");
        if (mhz >= 28.0 && mhz < 29.7) return QStringLiteral("10m");
        if (mhz >= 50.0 && mhz < 54.0) return QStringLiteral("6m");
        if (mhz >= 70.0 && mhz < 71.0) return QStringLiteral("4m");
        if (mhz >= 144.0 && mhz < 148.0) return QStringLiteral("2m");
        if (mhz >= 430.0 && mhz < 440.0) return QStringLiteral("70cm");
        if (mhz >= 1240.0 && mhz < 1300.0) return QStringLiteral("23cm");
        return QStringLiteral("Other");
    };

    auto bandForEntry = [&](const LogbookEntry &entry) -> QString {
        QString band = value(entry, QStringLiteral("BAND")).trimmed().toLower();
        if (!band.isEmpty()) {
            return band;
        }
        bool ok = false;
        const double mhz = value(entry, QStringLiteral("FREQ")).toDouble(&ok);
        return ok ? bandFromFrequency(mhz) : QStringLiteral("Other");
    };

    auto inc = [](QMap<QString, int> *map, const QString &key) {
        if (map == nullptr) return;
        (*map)[key] = map->value(key) + 1;
    };

    auto callBase = [](QString call) -> QString {
        call = AdifLogbook::normalizeCallsign(call);
        const QStringList parts = call.split(QLatin1Char('/'), Qt::SkipEmptyParts);
        QString best = call;
        for (const QString &part : parts) {
            if (part.contains(QRegularExpression(QStringLiteral("\\d")))) {
                best = part;
                break;
            }
        }
        return best;
    };

    QMap<QString, int> byMode;
    QMap<QString, int> byBand;
    QMap<QString, int> byCountry;
    QMap<QString, int> byDxcc;
    QMap<QString, int> byGrid;
    QMap<QString, int> byYear;
    QMap<QString, int> byMonth;
    QSet<QString> uniqueCalls;
    QSet<QString> uniqueCountries;
    QSet<QString> uniqueDxcc;
    QSet<QString> uniqueGrids;
    QDateTime firstUtc;
    QDateTime lastUtc;

    for (const LogbookEntry &entry : records) {
        const QString call = callBase(entry.callsign);
        if (!call.isEmpty()) uniqueCalls.insert(call);

        inc(&byMode, normalized(entry.mode.toUpper(), L("Unknown")));
        inc(&byBand, normalized(bandForEntry(entry), L("Other")));

        const QString country = normalized(value(entry, QStringLiteral("COUNTRY")), L("Unknown"));
        inc(&byCountry, country);
        if (country != L("Unknown")) uniqueCountries.insert(country);

        const QString dxcc = normalized(entry.adifFields.value(QStringLiteral("DXCC")), country);
        inc(&byDxcc, dxcc);
        if (dxcc != L("Unknown") && dxcc != QStringLiteral("—")) uniqueDxcc.insert(dxcc);

        const QString grid = normalized(entry.grid.left(6).toUpper(), L("Unknown"));
        inc(&byGrid, grid);
        if (grid != L("Unknown")) uniqueGrids.insert(grid);

        const QDateTime utc = entry.utc.toUTC();
        if (utc.isValid() && !utc.isNull()) {
            if (!firstUtc.isValid() || utc < firstUtc) firstUtc = utc;
            if (!lastUtc.isValid() || utc > lastUtc) lastUtc = utc;
            inc(&byYear, utc.date().toString(QStringLiteral("yyyy")));
            inc(&byMonth, utc.date().toString(QStringLiteral("yyyy-MM")));
        }
    }

    auto sortedRows = [](const QMap<QString, int> &map, int limit = 0) -> QVector<QPair<QString, int>> {
        QVector<QPair<QString, int>> rows;
        rows.reserve(map.size());
        for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
            rows.append(qMakePair(it.key(), it.value()));
        }
        std::sort(rows.begin(), rows.end(), [](const QPair<QString, int> &a, const QPair<QString, int> &b) {
            if (a.second != b.second) return a.second > b.second;
            return a.first < b.first;
        });
        if (limit > 0 && rows.size() > limit) rows.resize(limit);
        return rows;
    };

    auto maxCount = [](const QVector<QPair<QString, int>> &rows) -> int {
        int maxValue = 0;
        for (const auto &row : rows) maxValue = qMax(maxValue, row.second);
        return qMax(1, maxValue);
    };

    auto tableHtml = [&](const QString &title, const QString &leftHeader, const QVector<QPair<QString, int>> &rows) -> QString {
        const int maxValue = maxCount(rows);
        QString html;
        html += QStringLiteral("<div class='panel'><h2>%1</h2>").arg(title.toHtmlEscaped());
        if (rows.isEmpty()) {
            html += QStringLiteral("<p class='muted'>%1</p></div>").arg(L("No data").toHtmlEscaped());
            return html;
        }
        html += QStringLiteral("<table><tr><th>%1</th><th class='num'>%2</th><th></th></tr>")
                    .arg(leftHeader.toHtmlEscaped(), L("QSO").toHtmlEscaped());
        for (const auto &row : rows) {
            const int width = qRound((100.0 * row.second) / double(maxValue));
            html += QStringLiteral("<tr><td>%1</td><td class='num'>%2</td><td><div class='bar'><span style='width:%3%'></span></div></td></tr>")
                        .arg(row.first.toHtmlEscaped())
                        .arg(row.second)
                        .arg(qBound(1, width, 100));
        }
        html += QStringLiteral("</table></div>");
        return html;
    };

    const int total = records.size();
    const QString firstText = firstUtc.isValid() ? firstUtc.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : QStringLiteral("—");
    const QString lastText = lastUtc.isValid() ? lastUtc.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : QStringLiteral("—");

    QString html;
    html += QStringLiteral("<html><head><meta charset='utf-8'><style>");
    html += QStringLiteral("@page{size:A4;margin:10mm;}body{font-family:sans-serif;font-size:8.5pt;color:#1d2733;}h1{font-size:20pt;margin:0 0 2mm 0;}h2{font-size:11pt;margin:0 0 2mm 0;}p{margin:1mm 0;}table{border-collapse:collapse;width:100%;table-layout:fixed;}th,td{border:0.45pt solid #c8ced8;padding:2.5px;text-align:left;vertical-align:middle;}th{background:#eef2f7;font-weight:700;}.num{text-align:right;width:18mm}.muted{color:#657386}.kpi{display:block;margin:4mm 0 4mm 0}.kpi table td{border:0.7pt solid #b8c0cc;padding:5px}.big{font-size:16pt;font-weight:700}.panel{margin:0 0 4mm 0;page-break-inside:avoid}.twocol{width:100%}.twocol td{width:50%;vertical-align:top;border:0;padding:0 2mm 0 0}.bar{height:6pt;background:#e7e7e7;border-radius:3px;overflow:hidden}.bar span{display:block;height:6pt;background:#2f7fc0}.foot{margin-top:3mm;font-size:7.2pt;color:#657386}");
    html += QStringLiteral("</style></head><body>");
    html += QStringLiteral("<h1>%1</h1>").arg(L("MadModem logbook statistics").toHtmlEscaped());
    html += QStringLiteral("<p>%1: <b>%2</b> &nbsp;&nbsp; %3: %4 UTC</p>")
                .arg(L("Scope").toHtmlEscaped())
                .arg(scopeLabel.toHtmlEscaped())
                .arg(L("Generated").toHtmlEscaped())
                .arg(QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")).toHtmlEscaped());

    html += QStringLiteral("<div class='kpi'><table><tr>");
    html += QStringLiteral("<td><span class='muted'>%1</span><br><span class='big'>%2</span></td>").arg(L("QSO total").toHtmlEscaped()).arg(total);
    html += QStringLiteral("<td><span class='muted'>%1</span><br><span class='big'>%2</span></td>").arg(L("Unique calls").toHtmlEscaped()).arg(uniqueCalls.size());
    html += QStringLiteral("<td><span class='muted'>%1</span><br><span class='big'>%2</span></td>").arg(L("Unique DXCC/countries").toHtmlEscaped()).arg(qMax(uniqueDxcc.size(), uniqueCountries.size()));
    html += QStringLiteral("<td><span class='muted'>%1</span><br><span class='big'>%2</span></td>").arg(L("Unique grids").toHtmlEscaped()).arg(uniqueGrids.size());
    html += QStringLiteral("</tr><tr>");
    html += QStringLiteral("<td colspan='2'><span class='muted'>%1</span><br><b>%2</b></td>").arg(L("First QSO").toHtmlEscaped(), firstText.toHtmlEscaped());
    html += QStringLiteral("<td colspan='2'><span class='muted'>%1</span><br><b>%2</b></td>").arg(L("Last QSO").toHtmlEscaped(), lastText.toHtmlEscaped());
    html += QStringLiteral("</tr></table></div>");

    html += QStringLiteral("<table class='twocol'><tr><td>%1</td><td>%2</td></tr></table>")
                .arg(tableHtml(L("Distribution by mode"), L("Mode"), sortedRows(byMode)),
                     tableHtml(L("Distribution by band"), L("Band"), sortedRows(byBand)));
    html += tableHtml(L("Top 10 countries"), L("Country"), sortedRows(byCountry, 10));
    html += QStringLiteral("<table class='twocol'><tr><td>%1</td><td>%2</td></tr></table>")
                .arg(tableHtml(L("Top 10 grids"), L("Grid"), sortedRows(byGrid, 10)),
                     tableHtml(L("QSOs by year"), L("Year"), sortedRows(byYear)));
    html += tableHtml(L("QSOs by month"), L("Month"), sortedRows(byMonth, 24));
    html += QStringLiteral("<p class='foot'>%1</p>").arg(L("Statistics are computed from the selected ADIF records. Unsupported ADIF fields are preserved but may not contribute to every chart.").toHtmlEscaped());
    html += QStringLiteral("</body></html>");
    return html;
}

bool LogbookDialog::saveStatisticsToPdf(const QVector<LogbookEntry> &records,
                                        const QString &scopeLabel,
                                        const QString &defaultBaseName)
{
    if (records.isEmpty()) {
        QMessageBox::information(this, L("Save statistics PDF"), L("No QSO records to save."));
        return false;
    }

    QString defaultName = defaultBaseName.isEmpty() ? QStringLiteral("MadModem_logbook") : defaultBaseName;
    if (defaultName.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive)) {
        defaultName.chop(4);
    }
    if (!defaultName.endsWith(QStringLiteral("_statistics"), Qt::CaseInsensitive)) {
        defaultName += QStringLiteral("_statistics");
    }
    defaultName += QStringLiteral(".pdf");

    QString fileName = QFileDialog::getSaveFileName(
        this,
        L("Save logbook statistics PDF"),
        defaultName,
        L("PDF document (*.pdf);;All files (*)"));
    if (fileName.isEmpty()) {
        return false;
    }
    if (!fileName.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive)) {
        fileName += QStringLiteral(".pdf");
    }

    QPrinter printer(QPrinter::HighResolution);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    printer.setPageOrientation(QPageLayout::Portrait);
#else
    printer.setOrientation(QPrinter::Portrait);
#endif
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(fileName);
    printer.setDocName(L("MadModem logbook statistics") + QStringLiteral(" - ") + scopeLabel);

    QProgressDialog progress(L("Preparing statistics PDF..."), L("Cancel"), 0, 0, this);
    progress.setWindowTitle(L("Save statistics PDF"));
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setMinimumDuration(250);
    progress.setAutoClose(false);
    progress.setAutoReset(false);
    progress.show();
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    QTextDocument document;
    const QRectF pageRect = printer.pageRect(QPrinter::Point);
    if (pageRect.isValid()) {
        document.setPageSize(pageRect.size());
        document.setTextWidth(pageRect.width());
    }
    document.setHtml(htmlForStatistics(records, scopeLabel));
    progress.setLabelText(L("Rendering statistics PDF..."));
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    if (progress.wasCanceled()) {
        progress.close();
        return false;
    }
    document.print(&printer);
    progress.close();

    QMessageBox::information(this,
                             L("Save statistics PDF"),
                             L("Statistics PDF saved successfully (%1 QSO records).")
                                 .arg(records.size()));
    return true;
}

bool LogbookDialog::saveRecordsToPdf(const QVector<LogbookEntry> &records,
                                     const QString &scopeLabel,
                                     const QString &defaultBaseName)
{
    if (records.isEmpty()) {
        QMessageBox::information(this, L("Save logbook PDF"), L("No QSO records to save."));
        return false;
    }

    QString defaultName = defaultBaseName.isEmpty() ? "MadModem_logbook" : defaultBaseName;
    if (!defaultName.endsWith(".pdf", Qt::CaseInsensitive)) {
        defaultName += ".pdf";
    }

    QString fileName = QFileDialog::getSaveFileName(
        this,
        L("Save logbook as PDF"),
        defaultName,
        L("PDF document (*.pdf);;All files (*)")
        );
    if (fileName.isEmpty()) {
        return false;
    }
    if (!fileName.endsWith(".pdf", Qt::CaseInsensitive)) {
        fileName += ".pdf";
    }

    QPrinter printer(QPrinter::HighResolution);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    printer.setPageOrientation(QPageLayout::Landscape);
#else
    printer.setOrientation(QPrinter::Landscape);
#endif
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(fileName);
    printer.setDocName(L("MadModem logbook") + " - " + scopeLabel);

    QProgressDialog progress(L("Preparing logbook PDF..."), L("Cancel"), 0, 0, this);
    progress.setWindowTitle(L("Save logbook PDF"));
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setMinimumDuration(250);
    progress.setAutoClose(false);
    progress.setAutoReset(false);
    progress.show();
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    QTextDocument document;
    const QRectF pageRect = printer.pageRect(QPrinter::Point);
    if (pageRect.isValid()) {
        document.setPageSize(pageRect.size());
        document.setTextWidth(pageRect.width());
    }
    const QString html = htmlForRecords(records, scopeLabel, &progress);
    if (html.isEmpty() && progress.wasCanceled()) {
        progress.close();
        return false;
    }
    progress.setLabelText(L("Rendering PDF file..."));
    progress.setRange(0, 0);
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    document.setHtml(html);
    document.print(&printer);
    progress.close();

    QMessageBox::information(this,
                             L("Save logbook PDF"),
                             L("PDF saved successfully (%1 QSO records).")
                             .arg(records.size()));
    return true;
}

void LogbookDialog::printLogbook()
{
    QString scopeLabel;
    QString defaultBaseName;
    const QVector<LogbookEntry> records = chooseOutputRecords("Print logbook",
                                                              &scopeLabel,
                                                              &defaultBaseName);
    if (records.isEmpty()) {
        return;
    }
    printRecordsToPrinter(records, scopeLabel);
}

void LogbookDialog::savePdfLogbook()
{
    QString scopeLabel;
    QString defaultBaseName;
    const QVector<LogbookEntry> records = chooseOutputRecords("Save logbook PDF",
                                                              &scopeLabel,
                                                              &defaultBaseName);
    if (records.isEmpty()) {
        return;
    }
    saveRecordsToPdf(records, scopeLabel, defaultBaseName);
}


void LogbookDialog::saveStatisticsPdf()
{
    QString scopeLabel;
    QString defaultBaseName;
    const QVector<LogbookEntry> records = chooseOutputRecords("Save logbook statistics PDF",
                                                              &scopeLabel,
                                                              &defaultBaseName);
    if (records.isEmpty()) {
        return;
    }
    saveStatisticsToPdf(records, scopeLabel, defaultBaseName);
}
