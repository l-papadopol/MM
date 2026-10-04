#ifndef LOGBOOKSETTINGSDIALOG_H
#define LOGBOOKSETTINGSDIALOG_H

#include <QDialog>
#include <QString>

#include <functional>

class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QPushButton;

/**
 * @brief Small settings dialog for the internal ADIF logbook file path.
 *
 * MadModem stores its built-in logbook as a plain ADIF file.  The default path
 * remains beside the executable, but the user can choose a different file so
 * backups/sync folders are possible without changing export/import behavior.
 */
class LogbookSettingsDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit LogbookSettingsDialog(const QString &currentPath,
                                   const QString &defaultPath,
                                   QWidget *parent = nullptr);

    QString selectedPath() const;
    void setTextTranslator(std::function<QString(const QString &)> translator);

private slots:
    void browsePath();
    void useDefaultPath();
    void validatePath();

private:
    void buildUi();
    void refreshLabels();
    QString L(const QString &source) const;

    QString m_currentPath;
    QString m_defaultPath;
    std::function<QString(const QString &)> m_textTranslator;

    QLabel *m_lblDescription = nullptr;
    QLabel *m_lblCurrentCaption = nullptr;
    QLabel *m_lblDefaultCaption = nullptr;
    QLabel *m_lblDefaultPath = nullptr;
    QLabel *m_lblWarning = nullptr;
    QLineEdit *m_editPath = nullptr;
    QPushButton *m_btnBrowse = nullptr;
    QPushButton *m_btnDefault = nullptr;
    QDialogButtonBox *m_buttonBox = nullptr;
};

#endif // LOGBOOKSETTINGSDIALOG_H
