#include "LogbookDialog.h"
#include "../utils/UiScale.h"
#include "../settings/AppSettings.h"

#include <QAbstractItemView>
#include <QTextStream>
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
#include <QDateEdit>
#include <QDateTime>
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

#include <QPrintDialog>
#include <QPrinter>
#include <QProgressDialog>

LogbookDialog::LogbookDialog(AdifLogbook *logbook, AppSettings *settings, QWidget *parent)
    : QDialog(parent),
      m_logbook(logbook),
      m_settings(settings)
{
    setWindowTitle(L("MadModem logbook"));
    setMinimumSize(1280, 760);
    resize(MadModemUi::size(1480, 840));
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);


    /*
     * ADIFMaster-like logbook chrome: a classic menu bar, an icon toolbar,
     * spreadsheet-style table actions, and a status bar.  The ADIF backend is
     * still conservative/preserving; these actions operate on the visible rows
     * without normalising or rewriting the whole ADIF file unless explicitly
     * requested.
     */
    m_actImport = new QAction(style()->standardIcon(QStyle::SP_DirOpenIcon), L("Import ADIF..."), this);
    m_actExportAll = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), L("Export all as ADIF..."), this);
    m_actExportResult = new QAction(style()->standardIcon(QStyle::SP_FileDialogListView), L("Export current result as ADIF..."), this);
    m_actExportSelectedAdif = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), L("Save selected rows as ADIF..."), this);
    m_actExportSelectedCsv = new QAction(style()->standardIcon(QStyle::SP_DriveFDIcon), L("Save selected rows as CSV..."), this);
    m_actCopyCsv = new QAction(style()->standardIcon(QStyle::SP_FileIcon), L("Copy selected rows as CSV"), this);
    m_actCopyAdif = new QAction(style()->standardIcon(QStyle::SP_FileIcon), L("Copy selected rows as ADIF"), this);
    m_actDelete = new QAction(style()->standardIcon(QStyle::SP_TrashIcon), L("Delete selected rows"), this);
    m_actPrint = new QAction(style()->standardIcon(QStyle::SP_FileDialogDetailedView), L("Print..."), this);
    m_actPdf = new QAction(style()->standardIcon(QStyle::SP_FileDialogContentsView), L("Save PDF..."), this);
    m_actRefresh = new QAction(style()->standardIcon(QStyle::SP_BrowserReload), L("Refresh"), this);
    m_actClearSearch = new QAction(style()->standardIcon(QStyle::SP_DialogResetButton), L("Clear search"), this);
    m_actSelectAll = new QAction(L("Select all rows"), this);
    m_actSelectAll->setShortcut(QKeySequence::SelectAll);
    m_actColumns = new QAction(style()->standardIcon(QStyle::SP_FileDialogDetailedView), L("Visible/print fields..."), this);

    m_menuBar = new QMenuBar(this);
    QMenu *fileMenu = m_menuBar->addMenu(L("File"));
    fileMenu->addAction(m_actImport);
    fileMenu->addSeparator();
    fileMenu->addAction(m_actExportAll);
    fileMenu->addAction(m_actExportResult);
    fileMenu->addAction(m_actExportSelectedAdif);
    fileMenu->addAction(m_actExportSelectedCsv);
    fileMenu->addSeparator();
    fileMenu->addAction(m_actPrint);
    fileMenu->addAction(m_actPdf);
    fileMenu->addSeparator();
    fileMenu->addAction(L("Close"), this, &LogbookDialog::accept);

    QMenu *editMenu = m_menuBar->addMenu(L("Edit"));
    editMenu->addAction(m_actCopyCsv);
    editMenu->addAction(m_actCopyAdif);
    editMenu->addSeparator();
    editMenu->addAction(m_actSelectAll);
    editMenu->addAction(m_actDelete);

    QMenu *searchMenu = m_menuBar->addMenu(L("Search"));
    searchMenu->addAction(m_actClearSearch);
    searchMenu->addAction(m_actRefresh);

    QMenu *toolsMenu = m_menuBar->addMenu(L("Tools"));
    toolsMenu->addAction(m_actRefresh);
    toolsMenu->addAction(m_actColumns);
    mainLayout->setMenuBar(m_menuBar);

    m_toolbar = new QToolBar(this);
    m_toolbar->setIconSize(QSize(28, 28));
    m_toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_toolbar->setMovable(false);
    m_toolbar->addAction(m_actImport);
    m_toolbar->addAction(m_actExportAll);
    m_toolbar->addAction(m_actRefresh);
    m_toolbar->addSeparator();
    m_toolbar->addAction(m_actCopyCsv);
    m_toolbar->addAction(m_actCopyAdif);
    m_toolbar->addAction(m_actExportSelectedAdif);
    m_toolbar->addAction(m_actExportSelectedCsv);
    m_toolbar->addSeparator();
    m_toolbar->addAction(m_actDelete);
    m_toolbar->addSeparator();
    m_toolbar->addAction(m_actPrint);
    m_toolbar->addAction(m_actPdf);
    m_toolbar->addSeparator();
    m_toolbar->addAction(m_actColumns);
    m_toolbar->addSeparator();
    m_workedStrikeCheck = new QCheckBox(L("Strike through worked calls"), this);
    m_workedStrikeCheck->setToolTip(L("Mark callsigns already present in the ADIF log with a strike-through line in RX/decode views."));
    m_workedStrikeCheck->setChecked(m_settings == nullptr ? true : m_settings->logbookStrikeWorkedCalls);
    m_toolbar->addWidget(m_workedStrikeCheck);
    mainLayout->addWidget(m_toolbar);

    QGroupBox *searchGroup = new QGroupBox(L("Advanced search"), this);
    QGridLayout *searchGrid = new QGridLayout(searchGroup);
    searchGrid->setContentsMargins(10, 8, 10, 8);
    searchGrid->setHorizontalSpacing(8);
    searchGrid->setVerticalSpacing(6);

    m_quickSearchEdit = new QLineEdit(this);
    m_quickSearchEdit->setPlaceholderText(L("Any field: callsign, grid, band, mode, report or UTC date..."));
    m_quickSearchEdit->setMinimumWidth(520);
    m_callEdit = new QLineEdit(this);
    m_callEdit->setPlaceholderText("e.g. IK6ABC");
    m_callEdit->setMinimumWidth(220);
    m_rstSentEdit = new QLineEdit(this);
    m_rstSentEdit->setPlaceholderText("599");
    m_rstReceivedEdit = new QLineEdit(this);
    m_rstReceivedEdit->setPlaceholderText("599");
    m_bandEdit = new QLineEdit(this);
    m_bandEdit->setPlaceholderText("20m");
    m_bandEdit->setMinimumWidth(160);
    m_modeEdit = new QLineEdit(this);
    m_modeEdit->setPlaceholderText("RTTY, BPSK31, CW, HELL...");
    m_modeEdit->setMinimumWidth(220);
    m_gridEdit = new QLineEdit(this);
    m_gridEdit->setPlaceholderText("JN63");
    m_gridEdit->setMinimumWidth(140);

    m_fromEnabled = new QCheckBox(L("From UTC"), this);
    m_fromDateEdit = new QDateEdit(QDate::currentDate().addMonths(-1), this);
    m_fromDateEdit->setCalendarPopup(true);
    m_fromDateEdit->setDisplayFormat("yyyy-MM-dd");
    m_fromDateEdit->setEnabled(false);

    m_toEnabled = new QCheckBox(L("To UTC"), this);
    m_toDateEdit = new QDateEdit(QDate::currentDate(), this);
    m_toDateEdit->setCalendarPopup(true);
    m_toDateEdit->setDisplayFormat("yyyy-MM-dd");
    m_toDateEdit->setEnabled(false);

    m_clearSearchButton = new QPushButton(L("Clear search"), this);

    searchGrid->addWidget(new QLabel(L("Any"), this), 0, 0);
    searchGrid->addWidget(m_quickSearchEdit, 0, 1, 1, 5);

    searchGrid->addWidget(new QLabel(L("Callsign"), this), 1, 0);
    searchGrid->addWidget(m_callEdit, 1, 1);
    searchGrid->addWidget(new QLabel(L("Band"), this), 1, 2);
    searchGrid->addWidget(m_bandEdit, 1, 3);
    searchGrid->addWidget(new QLabel(L("Mode"), this), 1, 4);
    searchGrid->addWidget(m_modeEdit, 1, 5);

    searchGrid->addWidget(new QLabel(L("Grid"), this), 2, 4);
    searchGrid->addWidget(m_gridEdit, 2, 5);

    searchGrid->addWidget(new QLabel(L("RST sent"), this), 2, 0);
    searchGrid->addWidget(m_rstSentEdit, 2, 1);
    searchGrid->addWidget(new QLabel(L("RST rcvd"), this), 2, 2);
    searchGrid->addWidget(m_rstReceivedEdit, 2, 3);
    searchGrid->addWidget(m_fromEnabled, 3, 2);
    searchGrid->addWidget(m_fromDateEdit, 3, 3);

    searchGrid->addWidget(m_toEnabled, 3, 4);
    searchGrid->addWidget(m_toDateEdit, 3, 5);
    searchGrid->addWidget(m_clearSearchButton, 3, 0, 1, 2);

    searchGrid->setColumnStretch(1, 2);
    searchGrid->setColumnStretch(3, 2);
    searchGrid->setColumnStretch(5, 3);

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setMinimumWidth(220);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(0);
    m_table->setMinimumHeight(420);

    /*
     * ADIF fields have very predictable display lengths.  Keep the table
     * proportional to those expected lengths and use the whole visible table
     * width, instead of leaving a giant empty area on the right or truncating
     * the UTC/callsign columns.  Headers remain manually draggable.
     */
    QHeaderView *header = m_table->horizontalHeader();
    header->setStretchLastSection(false);
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
        "QTableWidget { gridline-color: #a8a8a8; alternate-background-color: #eeeeee; background: #f7f7f7; }"
        "QHeaderView::section { background: #dcdcdc; padding: 3px; border: 1px solid #9c9c9c; font-weight: 600; }"
    ));

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    m_importButton = new QPushButton(L("Import ADIF..."), this);
    m_exportAllButton = new QPushButton(L("Export all..."), this);
    m_exportResultButton = new QPushButton(L("Export result..."), this);
    m_exportSelectedButton = new QPushButton(L("Export selected..."), this);
    m_deleteSelectedButton = new QPushButton(L("Delete selected"), this);
    m_deleteSelectedButton->setToolTip(L("Delete the selected QSO records from the ADIF logbook after confirmation."));
    m_printButton = new QPushButton(L("Print..."), this);
    m_savePdfButton = new QPushButton(L("Save PDF..."), this);
    m_closeButton = new QPushButton(L("Close"), this);

    buttonLayout->addWidget(m_importButton);
    buttonLayout->addSpacing(12);
    buttonLayout->addWidget(m_exportAllButton);
    buttonLayout->addWidget(m_exportResultButton);
    buttonLayout->addWidget(m_exportSelectedButton);
    buttonLayout->addWidget(m_deleteSelectedButton);
    buttonLayout->addSpacing(12);
    buttonLayout->addWidget(m_printButton);
    buttonLayout->addWidget(m_savePdfButton);
    buttonLayout->addStretch(1);
    buttonLayout->addWidget(m_summaryLabel);
    buttonLayout->addWidget(m_closeButton);

    // ADIFMaster-style workflow keeps the main commands in menu/toolbar/context menu.
    // Legacy push buttons are kept alive for signal compatibility but not shown.
    for (QPushButton *button : {m_importButton, m_exportAllButton, m_exportResultButton,
                                m_exportSelectedButton, m_deleteSelectedButton,
                                m_printButton, m_savePdfButton, m_closeButton}) {
        if (button != nullptr) {
            button->setVisible(false);
        }
    }
    if (m_summaryLabel != nullptr) {
        m_summaryLabel->setVisible(false);
    }

    mainLayout->addWidget(searchGroup);
    mainLayout->addWidget(m_table, 1);

    const auto lineEdits = {m_quickSearchEdit, m_callEdit, m_rstSentEdit, m_rstReceivedEdit, m_bandEdit, m_modeEdit, m_gridEdit};
    for (QLineEdit *edit : lineEdits) {
        connect(edit, &QLineEdit::textChanged,
                this, &LogbookDialog::refreshTable);
    }

    connect(m_fromEnabled, &QCheckBox::toggled,
            m_fromDateEdit, &QDateEdit::setEnabled);
    connect(m_fromEnabled, &QCheckBox::toggled,
            this, &LogbookDialog::refreshTable);
    connect(m_fromDateEdit, &QDateEdit::dateChanged,
            this, &LogbookDialog::refreshTable);
    connect(m_toEnabled, &QCheckBox::toggled,
            m_toDateEdit, &QDateEdit::setEnabled);
    connect(m_toEnabled, &QCheckBox::toggled,
            this, &LogbookDialog::refreshTable);
    connect(m_toDateEdit, &QDateEdit::dateChanged,
            this, &LogbookDialog::refreshTable);

    connect(m_clearSearchButton, &QPushButton::clicked,
            this, &LogbookDialog::clearSearch);
    connect(m_importButton, &QPushButton::clicked,
            this, &LogbookDialog::importAdif);
    connect(m_exportAllButton, &QPushButton::clicked,
            this, &LogbookDialog::exportAllAdif);
    connect(m_exportResultButton, &QPushButton::clicked,
            this, &LogbookDialog::exportSearchResultAdif);
    connect(m_exportSelectedButton, &QPushButton::clicked,
            this, &LogbookDialog::exportSelectedAdif);
    connect(m_deleteSelectedButton, &QPushButton::clicked,
            this, &LogbookDialog::deleteSelectedRecords);
    connect(m_printButton, &QPushButton::clicked,
            this, &LogbookDialog::printLogbook);
    connect(m_savePdfButton, &QPushButton::clicked,
            this, &LogbookDialog::savePdfLogbook);
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
    connect(m_workedStrikeCheck, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_settings != nullptr) {
            m_settings->logbookStrikeWorkedCalls = checked;
            m_settings->save();
        }
        emit logbookChanged();
    });

    m_statusBar = new QStatusBar(this);
    m_statusBar->setSizeGripEnabled(false);
    mainLayout->addWidget(m_statusBar);

    refreshTable();
    updateSelectionActions();
    MadModemUi::scaleWidgetTree(this);
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
    adjustColumnWidths();
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
    const int fixedWidths[] = {170, 120, 90, 90, 90, 75, 95, 85, 130, 130, 220};
    const int fixedCount = qMin<int>(m_table->columnCount(), int(sizeof(fixedWidths) / sizeof(fixedWidths[0])));
    for (int col = 0; col < fixedCount; ++col) {
        m_table->setColumnWidth(col, fixedWidths[col]);
    }
    for (int col = fixedCount; col < m_table->columnCount(); ++col) {
        m_table->setColumnWidth(col, 140);
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
    if (m_fromEnabled != nullptr && m_fromEnabled->isChecked() && m_fromDateEdit != nullptr) {
        criteria.fromDateUtc = m_fromDateEdit->date();
    }
    if (m_toEnabled != nullptr && m_toEnabled->isChecked() && m_toDateEdit != nullptr) {
        criteria.toDateUtc = m_toDateEdit->date();
    }
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
    QStringList keys = {
        QStringLiteral("UTC"), QStringLiteral("CALL"), QStringLiteral("GRIDSQUARE"),
        QStringLiteral("RST_SENT"), QStringLiteral("RST_RCVD"), QStringLiteral("BAND"),
        QStringLiteral("MODE"), QStringLiteral("FREQ"), QStringLiteral("NAME"),
        QStringLiteral("QTH"), QStringLiteral("COMMENT")
    };

    if (m_logbook != nullptr) {
        for (const QString &field : m_logbook->allAdifFieldNames()) {
            const QString key = field.trimmed().toUpper();
            if (!key.isEmpty() && !keys.contains(key) && !fieldHiddenByDefault(key)) {
                keys.append(key);
            }
        }
    }
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

    refreshTable();
}

void LogbookDialog::importAdif()
{
    if (m_logbook == nullptr) {
        return;
    }

    const QString fileName = QFileDialog::getOpenFileName(
        this,
        "Import ADIF logbook",
        QString(),
        "ADIF logbook (*.adi *.adif);;All files (*)"
        );
    if (fileName.isEmpty()) {
        return;
    }

    int imported = 0;
    QString error;
    if (!m_logbook->importAdif(fileName, &imported, &error)) {
        QMessageBox::warning(this, L("Import ADIF"), L("Import failed:") + " " + error);
        return;
    }

    refreshTable();
    emit logbookChanged();
    QMessageBox::information(this, L("Import ADIF"), L("Imported %1 QSO records.").arg(imported));
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
    const int shown = m_displayedRecords.size();
    const int selected = selectedRecords().size();
    QString message = QString("%1    %2: %3    %4: %5    %6: %7")
                          .arg(L("Ready"))
                          .arg(L("QSOs"))
                          .arg(total)
                          .arg(L("Shown"))
                          .arg(shown)
                          .arg(L("Selected"))
                          .arg(selected);
    if (!m_adifExtraColumns.isEmpty()) {
        message += QString("    %1: %2").arg(L("ADIF extra fields")).arg(m_adifExtraColumns.size());
    }
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
    updateStatusBar();
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
    out.setCodec("UTF-8");
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

    const QString defaultName = defaultBaseName.endsWith(".adi", Qt::CaseInsensitive)
                                ? defaultBaseName
                                : defaultBaseName + ".adi";
    const QString fileName = QFileDialog::getSaveFileName(
        this,
        L(dialogTitle),
        defaultName,
        L("ADIF logbook (*.adi);;ADIF logbook (*.adif);;All files (*)")
        );
    if (fileName.isEmpty()) {
        return false;
    }

    QString error;
    if (!m_logbook->exportRecordsAdif(fileName, records, &error)) {
        QMessageBox::warning(this, L(dialogTitle), L("Export failed:") + " " + error);
        return false;
    }

    QMessageBox::information(this,
                             L(dialogTitle),
                             L("%1 exported successfully (%2 QSO records).")
                             .arg(L(successLabel))
                             .arg(records.size()));
    return true;
}

void LogbookDialog::exportAllAdif()
{
    if (m_logbook == nullptr) {
        return;
    }
    exportRecords(m_logbook->records(),
                  "Export all ADIF",
                  "MadModem_logbook_all.adi",
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

    QString error;
    const int removed = m_logbook->removeEntries(records, &error);
    if (removed < 0) {
        QMessageBox::warning(this, L("Delete selected QSOs"), L("Delete failed:") + " " + error);
        return;
    }
    if (removed == 0) {
        QMessageBox::information(this, L("Delete selected QSOs"), L("No matching QSO records were removed."));
        return;
    }

    refreshTable();
    emit logbookChanged();
    QMessageBox::information(this, L("Delete selected QSOs"), L("Deleted %1 QSO record(s).").arg(removed));
}

QVector<LogbookEntry> LogbookDialog::chooseOutputRecords(const QString &dialogTitle,
                                                           QString *scopeLabel,
                                                           QString *defaultBaseName)
{
    if (m_logbook == nullptr) {
        return {};
    }

    const QVector<LogbookEntry> selected = selectedRecords();

    QMessageBox box(this);
    box.setWindowTitle(L(dialogTitle));
    box.setIcon(QMessageBox::Question);
    box.setText(L("Choose which QSO records to use."));
    QPushButton *selectedButton = box.addButton(QString("%1 (%2)").arg(L("Selected QSOs")).arg(selected.size()),
                                                QMessageBox::AcceptRole);
    QPushButton *resultButton = box.addButton(QString("%1 (%2)").arg(L("Current search result")).arg(m_displayedRecords.size()),
                                              QMessageBox::AcceptRole);
    QPushButton *allButton = box.addButton(QString("%1 (%2)").arg(L("Full logbook")).arg(m_logbook->count()),
                                           QMessageBox::AcceptRole);
    QPushButton *cancelButton = box.addButton(QMessageBox::Cancel);
    selectedButton->setEnabled(!selected.isEmpty());
    resultButton->setEnabled(!m_displayedRecords.isEmpty());
    allButton->setEnabled(m_logbook->count() > 0);

    box.exec();
    if (box.clickedButton() == cancelButton || box.clickedButton() == nullptr) {
        return {};
    }

    if (box.clickedButton() == selectedButton) {
        if (scopeLabel != nullptr) *scopeLabel = L("selected QSOs");
        if (defaultBaseName != nullptr) *defaultBaseName = "MadModem_logbook_selected";
        return selected;
    }
    if (box.clickedButton() == allButton) {
        if (scopeLabel != nullptr) *scopeLabel = L("full logbook");
        if (defaultBaseName != nullptr) *defaultBaseName = "MadModem_logbook_all";
        return m_logbook->records();
    }

    if (scopeLabel != nullptr) *scopeLabel = L("current search result");
    if (defaultBaseName != nullptr) *defaultBaseName = "MadModem_logbook_search_result";
    return m_displayedRecords;
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
