#ifndef LOGBOOKDIALOG_H
#define LOGBOOKDIALOG_H

#include "../logbook/AdifLogbook.h"

#include <QDialog>
#include <QVector>

#include <functional>

class QCheckBox;
class QDateTimeEdit;
class QComboBox;
class QTimer;
class QAction;
class QLabel;
class QLineEdit;
class QPushButton;
class QMenu;
class QStatusBar;
class QTableWidget;
class QToolBar;
class QResizeEvent;
class QProgressDialog;
class AppSettings;
class AsyncLogbook;

/**
 * @brief Separate window for browsing, advanced searching, importing,
 * exporting, and printing the ADIF logbook.
 */
class LogbookDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LogbookDialog(AdifLogbook *logbook, AppSettings *settings = nullptr, QWidget *parent = nullptr);
    void setStore(AsyncLogbook *store) { m_store = store; }
    void setTextTranslator(std::function<QString(const QString &)> translator);

protected:
    void resizeEvent(QResizeEvent *event) override;

signals:
    void logbookChanged();

private slots:
    void refreshTable();
    void clearSearch();
    void importAdif();
    void exportAllAdif();
    void exportSearchResultAdif();
    void exportSelectedAdif();
    void exportCabrilloWizard();
    void copySelectedRowsCsv();
    void copySelectedRowsAdif();
    void saveSelectedRowsCsv();
    void selectAllRows();
    void showTableContextMenu(const QPoint &pos);
    void updateSelectionActions();
    void deleteSelectedRecords();
    void printLogbook();
    void savePdfLogbook();
    void saveStatisticsPdf();
    void configureVisibleFields();

private:
    AsyncLogbook *m_store = nullptr;
    LogbookSearchCriteria currentCriteria() const;
    QVector<LogbookEntry> selectedRecords() const;
    bool exportRecords(const QVector<LogbookEntry> &records,
                       const QString &dialogTitle,
                       const QString &defaultBaseName,
                       const QString &successLabel);
    bool exportRecordsCsv(const QVector<LogbookEntry> &records,
                          const QString &dialogTitle,
                          const QString &defaultBaseName,
                          const QString &successLabel);
    QString csvForRecords(const QVector<LogbookEntry> &records) const;
    QStringList allAvailableColumnKeys() const;
    QStringList defaultVisibleColumnKeys() const;
    QStringList visibleColumnKeys() const;
    QString columnLabel(const QString &key) const;
    QString columnValue(const LogbookEntry &entry, const QString &key) const;
    bool fieldHiddenByDefault(const QString &field) const;
    QString csvEscape(const QString &value) const;
    void updateStatusBar();
    void setActionTexts();
    QVector<LogbookEntry> chooseOutputRecords(const QString &dialogTitle,
                                              QString *scopeLabel,
                                              QString *defaultBaseName);
    QString htmlForRecords(const QVector<LogbookEntry> &records,
                           const QString &scopeLabel,
                           QProgressDialog *progress = nullptr) const;
    QString htmlForStatistics(const QVector<LogbookEntry> &records,
                              const QString &scopeLabel) const;
    void adjustColumnWidths();
    QString L(const QString &source) const;
    void retranslateVisibleText();
    void retranslateQObjectTree(QObject *object);
    bool printRecordsToPrinter(const QVector<LogbookEntry> &records,
                               const QString &scopeLabel);
    bool saveRecordsToPdf(const QVector<LogbookEntry> &records,
                          const QString &scopeLabel,
                          const QString &defaultBaseName);
    bool saveStatisticsToPdf(const QVector<LogbookEntry> &records,
                             const QString &scopeLabel,
                             const QString &defaultBaseName);

    QVector<LogbookEntry> outputRecords() const;
    void updateOutputScope();
    QComboBox *m_outputScope = nullptr;
    QComboBox *m_period = nullptr;
    QComboBox *m_bandCombo = nullptr;
    QComboBox *m_modeCombo = nullptr;
    QAction *m_actExport = nullptr;
    QTimer *m_searchDelay = nullptr;
    bool m_hadSelection = false;
    AdifLogbook *m_logbook = nullptr;
    AppSettings *m_settings = nullptr;
    QLineEdit *m_quickSearchEdit = nullptr;
    QLineEdit *m_callEdit = nullptr;
    QLineEdit *m_rstSentEdit = nullptr;
    QLineEdit *m_rstReceivedEdit = nullptr;
    QLineEdit *m_bandEdit = nullptr;
    QLineEdit *m_modeEdit = nullptr;
    QLineEdit *m_gridEdit = nullptr;
    QCheckBox *m_fromEnabled = nullptr;
    QDateTimeEdit *m_fromDateEdit = nullptr;
    QCheckBox *m_toEnabled = nullptr;
    QDateTimeEdit *m_toDateEdit = nullptr;
    QLabel *m_summaryLabel = nullptr;
    QLabel *m_filterError = nullptr;
    QTableWidget *m_table = nullptr;
    QToolBar *m_toolbar = nullptr;
    QStatusBar *m_statusBar = nullptr;
    QAction *m_actImport = nullptr;
    QAction *m_actExportAll = nullptr;
    QAction *m_actExportResult = nullptr;
    QAction *m_actExportSelectedAdif = nullptr;
    QAction *m_actExportSelectedCsv = nullptr;
    QAction *m_actCopyCsv = nullptr;
    QAction *m_actCopyAdif = nullptr;
    QAction *m_actDelete = nullptr;
    QAction *m_actPrint = nullptr;
    QAction *m_actPdf = nullptr;
    QAction *m_actStatsPdf = nullptr;
    QAction *m_actRefresh = nullptr;
    QAction *m_actClearSearch = nullptr;
    QAction *m_actSelectAll = nullptr;
    QAction *m_actColumns = nullptr;
    QPushButton *m_clearSearchButton = nullptr;
    QPushButton *m_closeButton = nullptr;
    QVector<LogbookEntry> m_displayedRecords;
    QStringList m_adifExtraColumns;
    QStringList m_visibleColumnKeys;
    std::function<QString(const QString &)> m_textTranslator;
};

#endif // LOGBOOKDIALOG_H
