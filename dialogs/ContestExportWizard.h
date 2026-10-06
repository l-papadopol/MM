#pragma once
#include <QWizard>
#include "../logbook/ContestExport.h"
#include "../logbook/GenericCabrillo.h"
class QComboBox;
class QCheckBox;
class QLineEdit;
class QLabel;
class QTableWidget;
class QPlainTextEdit;
class ContestExportPage;

class ContestExportWizard : public QWizard {
public:
    explicit ContestExportWizard(const QVector<LogbookEntry>& records, QWidget* parent = nullptr);
    QByteArray generatedData() const { return m_generated; }
    QString suggestedFileName() const;
    void accept() override;
private:
    void selectEdition();
    void populatePreview();
    void validateSelection();
    QByteArray generate(const QVector<LogbookEntry>& records, QString* error) const;
    QVector<LogbookEntry> selectedRecords() const;
    ContestExport::Profiles m_profiles;
    ContestExport::Catalog m_catalog;
    QCheckBox* m_automatic;
    QVector<LogbookEntry> m_previewRecords;
    QByteArray m_generated;
    bool m_populating = false;
    QComboBox *m_editions, *m_operator, *m_power, *m_assisted, *m_band, *m_sent, *m_received;
    QLineEdit *m_station, *m_name, *m_email;
    QLabel *m_summary;
    QTableWidget *m_table;
    QPlainTextEdit *m_cabrillo;
    ContestExportPage *m_selectionPage, *m_previewPage;
};
