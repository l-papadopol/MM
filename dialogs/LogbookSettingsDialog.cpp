#include "LogbookSettingsDialog.h"
#include "../utils/UiScale.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>

LogbookSettingsDialog::LogbookSettingsDialog(const QString &currentPath,
                                             const QString &defaultPath,
                                             QWidget *parent)
    : QDialog(parent),
      m_currentPath(currentPath),
      m_defaultPath(defaultPath)
{
    buildUi();
    refreshLabels();
    validatePath();
}

QString LogbookSettingsDialog::selectedPath() const
{
    if (m_editPath == nullptr) {
        return m_currentPath;
    }
    return QDir::toNativeSeparators(m_editPath->text().trimmed());
}

void LogbookSettingsDialog::setTextTranslator(std::function<QString(const QString &)> translator)
{
    m_textTranslator = std::move(translator);
    refreshLabels();
}

void LogbookSettingsDialog::buildUi()
{
    resize(620, 240);
    setMinimumSize(520, 220);

    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(12, 12, 12, 12);
    outer->setSpacing(10);

    m_lblDescription = new QLabel(this);
    m_lblDescription->setWordWrap(true);
    outer->addWidget(m_lblDescription);

    QGridLayout *grid = new QGridLayout();
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(8);

    m_lblCurrentCaption = new QLabel(this);
    m_lblDefaultCaption = new QLabel(this);
    m_lblDefaultPath = new QLabel(this);
    m_lblDefaultPath->setWordWrap(true);
    m_lblDefaultPath->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_editPath = new QLineEdit(this);
    m_editPath->setText(QDir::toNativeSeparators(m_currentPath));
    m_editPath->setMinimumWidth(260);
    m_editPath->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_btnBrowse = new QPushButton(this);
    m_btnDefault = new QPushButton(this);

    grid->addWidget(m_lblCurrentCaption, 0, 0);
    grid->addWidget(m_editPath, 0, 1);
    grid->addWidget(m_btnBrowse, 0, 2);
    grid->addWidget(m_lblDefaultCaption, 1, 0);
    grid->addWidget(m_lblDefaultPath, 1, 1, 1, 2);
    grid->addWidget(m_btnDefault, 2, 1, 1, 2, Qt::AlignLeft);
    grid->setColumnStretch(1, 1);
    outer->addLayout(grid);

    m_lblWarning = new QLabel(this);
    m_lblWarning->setWordWrap(true);
    m_lblWarning->setStyleSheet("color: #b00020;");
    outer->addWidget(m_lblWarning);

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    outer->addWidget(m_buttonBox);

    connect(m_btnBrowse, &QPushButton::clicked,
            this, &LogbookSettingsDialog::browsePath);
    connect(m_btnDefault, &QPushButton::clicked,
            this, &LogbookSettingsDialog::useDefaultPath);
    connect(m_editPath, &QLineEdit::textChanged,
            this, &LogbookSettingsDialog::validatePath);
    connect(m_buttonBox, &QDialogButtonBox::accepted,
            this, &LogbookSettingsDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected,
            this, &LogbookSettingsDialog::reject);
}

void LogbookSettingsDialog::refreshLabels()
{
    setWindowTitle(L("Logbook file settings"));
    if (m_lblDescription != nullptr) {
        m_lblDescription->setText(L("MadModem keeps the internal logbook as one ADIF file. Choose where that file is stored. The default path is beside the application executable."));
    }
    if (m_lblCurrentCaption != nullptr) {
        m_lblCurrentCaption->setText(L("Logbook file"));
    }
    if (m_lblDefaultCaption != nullptr) {
        m_lblDefaultCaption->setText(L("Default path"));
    }
    if (m_lblDefaultPath != nullptr) {
        m_lblDefaultPath->setText(QDir::toNativeSeparators(m_defaultPath));
    }
    if (m_btnBrowse != nullptr) {
        m_btnBrowse->setText(L("Browse..."));
    }
    if (m_btnDefault != nullptr) {
        m_btnDefault->setText(L("Use default path"));
    }
    if (m_buttonBox != nullptr) {
        if (QPushButton *ok = m_buttonBox->button(QDialogButtonBox::Ok)) {
            ok->setText(L("OK"));
        }
        if (QPushButton *cancel = m_buttonBox->button(QDialogButtonBox::Cancel)) {
            cancel->setText(L("Cancel"));
        }
    }
}

void LogbookSettingsDialog::browsePath()
{
    const QString current = selectedPath().isEmpty() ? m_defaultPath : selectedPath();
    const QString fileName = QFileDialog::getSaveFileName(this,
                                                          L("Select logbook ADIF file"),
                                                          current,
                                                          L("ADIF files (*.adi *.adif);;All files (*)"));
    if (!fileName.isEmpty() && m_editPath != nullptr) {
        m_editPath->setText(QDir::toNativeSeparators(fileName));
    }
}

void LogbookSettingsDialog::useDefaultPath()
{
    if (m_editPath != nullptr) {
        m_editPath->setText(QDir::toNativeSeparators(m_defaultPath));
    }
}

void LogbookSettingsDialog::validatePath()
{
    if (m_editPath == nullptr || m_lblWarning == nullptr || m_buttonBox == nullptr) {
        return;
    }

    const QString path = selectedPath();
    QString warning;
    bool ok = true;

    if (path.trimmed().isEmpty()) {
        ok = false;
        warning = L("Please choose a logbook ADIF file path.");
    } else {
        const QFileInfo info(path);
        const QDir dir = info.absoluteDir();
        if (!dir.exists()) {
            warning = L("The folder does not exist yet. MadModem will try to create it when saving the logbook.");
        }
        if (info.fileName().trimmed().isEmpty()) {
            ok = false;
            warning = L("Please include a file name, for example logbook.adi.");
        }
    }

    m_lblWarning->setText(warning);
    if (QPushButton *button = m_buttonBox->button(QDialogButtonBox::Ok)) {
        button->setEnabled(ok);
    }
}

QString LogbookSettingsDialog::L(const QString &source) const
{
    if (m_textTranslator) {
        const QString translated = m_textTranslator(source);
        if (!translated.isEmpty()) {
            return translated;
        }
    }
    return source;
}
