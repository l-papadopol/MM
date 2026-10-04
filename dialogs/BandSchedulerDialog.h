#ifndef BANDSCHEDULERDIALOG_H
#define BANDSCHEDULERDIALOG_H

#include <QDate>
#include <QDialog>
#include <QList>
#include <QString>
#include <QTime>

#include <functional>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QTableWidget;
class QTimeEdit;

/**
 * @brief One daily UTC QSY scheduler entry.
 *
 * The scheduler intentionally stores only a safe QSY request: mode, band and
 * rig frequency. It never stores TX/QSO automation state. Evil Auto CQ/Auto
 * QSO can remain enabled; MainWindow decides when a pending QSY is safe to
 * apply after TX/QSO completion.
 */
struct ScheduledQsyEntry
{
    bool enabled = true;
    QTime timeUtc = QTime(0, 0);
    QString mode;
    QString band;
    double frequencyHz = 0.0;
    bool useFtStandard = false;

    QString triggerKey(const QDate &utcDate) const;
};

/**
 * @brief Editor for the daily band/frequency scheduler plan.
 */
class BandSchedulerDialog final : public QDialog
{
public:
    using Translator = std::function<QString(const QString &, const QString &)>;

    explicit BandSchedulerDialog(const QList<ScheduledQsyEntry> &entries,
                                 Translator translator,
                                 QWidget *parent = nullptr);

    QList<ScheduledQsyEntry> entries() const;

    static QString serializeEntries(const QList<ScheduledQsyEntry> &entries);
    static QList<ScheduledQsyEntry> deserializeEntries(const QString &json);
    static double standardFtFrequencyHz(const QString &modeName, const QString &band);
    static QStringList defaultModes();
    static QStringList defaultBands();

private:
    QString L(const QString &key, const QString &fallback) const;
    void buildUi();
    void refreshTable();
    void loadRowToEditor(int row);
    void updateEditorStandardFrequency();
    ScheduledQsyEntry editorEntry() const;
    void setEditorEntry(const ScheduledQsyEntry &entry);
    QString frequencyText(double hz) const;

    Translator m_tr;
    QList<ScheduledQsyEntry> m_entries;

    QTableWidget *m_table = nullptr;
    QCheckBox *m_chkEnabled = nullptr;
    QTimeEdit *m_timeEdit = nullptr;
    QComboBox *m_modeCombo = nullptr;
    QComboBox *m_bandCombo = nullptr;
    QCheckBox *m_chkFtStandard = nullptr;
    QDoubleSpinBox *m_freqMHz = nullptr;
    QLabel *m_hint = nullptr;
    QPushButton *m_addButton = nullptr;
    QPushButton *m_updateButton = nullptr;
    QPushButton *m_removeButton = nullptr;
};

#endif // BANDSCHEDULERDIALOG_H
