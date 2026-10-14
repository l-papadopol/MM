#include "BandSchedulerDialog.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDate>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimeEdit>
#include <QVBoxLayout>

#include <algorithm>

namespace {
constexpr int SourceTextRole = Qt::UserRole + 923;

QString normalizedMode(const QString &mode)
{
    const QString m = mode.trimmed();
    if (m.compare(QStringLiteral("FT4"), Qt::CaseInsensitive) == 0) return QStringLiteral("FT4");
    if (m.compare(QStringLiteral("FT8"), Qt::CaseInsensitive) == 0) return QStringLiteral("FT8");
    return m;
}
}

QString ScheduledQsyEntry::triggerKey(const QDate &utcDate) const
{
    return QStringLiteral("%1|%2|%3|%4|%5")
        .arg(utcDate.toString(Qt::ISODate),
             timeUtc.toString(QStringLiteral("HH:mm")),
             mode.trimmed().toUpper(),
             band.trimmed().toUpper(),
             QString::number(qRound64(frequencyHz)));
}

BandSchedulerDialog::BandSchedulerDialog(const QList<ScheduledQsyEntry> &entries,
                                         Translator translator,
                                         QWidget *parent)
    : QDialog(parent),
      m_tr(std::move(translator)),
      m_entries(entries)
{
    buildUi();
    refreshTable();
    if (!m_entries.isEmpty()) {
        m_table->selectRow(0);
        loadRowToEditor(0);
    } else {
        ScheduledQsyEntry entry;
        entry.mode = QStringLiteral("FT8");
        entry.band = QStringLiteral("20m");
        entry.useFtStandard = true;
        entry.frequencyHz = standardFtFrequencyHz(entry.mode, entry.band);
        setEditorEntry(entry);
    }
}

QString BandSchedulerDialog::L(const QString &key, const QString &fallback) const
{
    return m_tr ? m_tr(key, fallback) : fallback;
}

QStringList BandSchedulerDialog::defaultModes()
{
    return { QStringLiteral("MeteoFax / HF WEFAX RX"), QStringLiteral("SSTV RX"),
             QStringLiteral("RTTY Text"), QStringLiteral("PSK Text"), QStringLiteral("MFSK Text"),
             QStringLiteral("CW Morse"), QStringLiteral("Feld Hell"), QStringLiteral("FT4"), QStringLiteral("FT8") };
}

QStringList BandSchedulerDialog::defaultBands()
{
    return { QStringLiteral("160m"), QStringLiteral("80m"), QStringLiteral("60m"),
             QStringLiteral("40m"), QStringLiteral("30m"), QStringLiteral("20m"),
             QStringLiteral("17m"), QStringLiteral("15m"), QStringLiteral("12m"),
             QStringLiteral("10m"), QStringLiteral("6m"), QStringLiteral("2m"),
             QStringLiteral("70cm"), QStringLiteral("23cm") };
}

void BandSchedulerDialog::buildUi()
{
    setWindowTitle(L(QStringLiteral("scheduler_dialog_title"), QStringLiteral("Band / frequency scheduler")));
    resize(780, 500);
    setMinimumSize(0, 0);

    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(10, 10, 10, 10);
    outer->setSpacing(8);

    QLabel *description = new QLabel(L(QStringLiteral("scheduler_dialog_description"),
        QStringLiteral("Daily UTC QSY plan. The scheduler changes only the rig frequency/band. It waits until TX ends and, in FT4/FT8, until the current QSO is complete.")), this);
    description->setWordWrap(true);
    outer->addWidget(description);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(6);
    const QStringList headers = {
        L(QStringLiteral("enabled"), QStringLiteral("Enabled")),
        L(QStringLiteral("utc_time"), QStringLiteral("UTC time")),
        L(QStringLiteral("mode"), QStringLiteral("Mode")),
        L(QStringLiteral("band"), QStringLiteral("Band")),
        L(QStringLiteral("frequency"), QStringLiteral("Frequency")),
        L(QStringLiteral("source"), QStringLiteral("Source"))
    };
    for (int i = 0; i < headers.size(); ++i) {
        auto *item = new QTableWidgetItem(headers.at(i));
        item->setData(SourceTextRole, headers.at(i));
        m_table->setHorizontalHeaderItem(i, item);
    }
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    outer->addWidget(m_table, 1);

    QGroupBox *editorGroup = new QGroupBox(L(QStringLiteral("scheduler_entry_editor"), QStringLiteral("Entry editor")), this);
    QGridLayout *grid = new QGridLayout(editorGroup);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(6);

    m_chkEnabled = new QCheckBox(L(QStringLiteral("enabled"), QStringLiteral("Enabled")), editorGroup);
    m_chkEnabled->setChecked(true);
    m_timeEdit = new QTimeEdit(editorGroup);
    m_timeEdit->setDisplayFormat(QStringLiteral("HH:mm"));
    m_timeEdit->setTime(QDateTime::currentDateTimeUtc().time().addSecs(3600));

    m_modeCombo = new QComboBox(editorGroup);
    m_modeCombo->addItems(defaultModes());
    m_bandCombo = new QComboBox(editorGroup);
    m_bandCombo->addItems(defaultBands());

    m_chkFtStandard = new QCheckBox(L(QStringLiteral("scheduler_use_ft_standard"), QStringLiteral("Use FT4/FT8 standard frequency for selected band")), editorGroup);
    m_freqMHz = new QDoubleSpinBox(editorGroup);
    m_freqMHz->setDecimals(6);
    m_freqMHz->setMinimum(0.100000);
    m_freqMHz->setMaximum(1300.000000);
    m_freqMHz->setSingleStep(0.001000);
    m_freqMHz->setSuffix(QStringLiteral(" MHz"));

    grid->addWidget(m_chkEnabled, 0, 0);
    grid->addWidget(new QLabel(L(QStringLiteral("utc_time"), QStringLiteral("UTC time")), editorGroup), 0, 1);
    grid->addWidget(m_timeEdit, 0, 2);
    grid->addWidget(new QLabel(L(QStringLiteral("mode"), QStringLiteral("Mode")), editorGroup), 1, 0);
    grid->addWidget(m_modeCombo, 1, 1, 1, 2);
    grid->addWidget(new QLabel(L(QStringLiteral("band"), QStringLiteral("Band")), editorGroup), 2, 0);
    grid->addWidget(m_bandCombo, 2, 1, 1, 2);
    grid->addWidget(m_chkFtStandard, 3, 0, 1, 3);
    grid->addWidget(new QLabel(L(QStringLiteral("frequency_mhz"), QStringLiteral("Frequency MHz")), editorGroup), 4, 0);
    grid->addWidget(m_freqMHz, 4, 1, 1, 2);

    QHBoxLayout *buttonRow = new QHBoxLayout();
    m_addButton = new QPushButton(L(QStringLiteral("add"), QStringLiteral("Add")), editorGroup);
    m_updateButton = new QPushButton(L(QStringLiteral("update_selected"), QStringLiteral("Update selected")), editorGroup);
    m_removeButton = new QPushButton(L(QStringLiteral("remove_selected"), QStringLiteral("Remove selected")), editorGroup);
    buttonRow->addWidget(m_addButton);
    buttonRow->addWidget(m_updateButton);
    buttonRow->addWidget(m_removeButton);
    buttonRow->addStretch(1);
    grid->addLayout(buttonRow, 5, 0, 1, 3);

    m_hint = new QLabel(editorGroup);
    m_hint->setWordWrap(true);
    grid->addWidget(m_hint, 6, 0, 1, 3);

    outer->addWidget(editorGroup);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(L(QStringLiteral("ok"), QStringLiteral("OK")));
    buttons->button(QDialogButtonBox::Cancel)->setText(L(QStringLiteral("cancel"), QStringLiteral("Cancel")));
    outer->addWidget(buttons);

    connect(m_table, &QTableWidget::cellClicked, this, [this](int row, int) { loadRowToEditor(row); });
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int row, int) { loadRowToEditor(row); });
    connect(m_modeCombo, &QComboBox::currentTextChanged, this, [this]() { updateEditorStandardFrequency(); });
    connect(m_bandCombo, &QComboBox::currentTextChanged, this, [this]() { updateEditorStandardFrequency(); });
    connect(m_chkFtStandard, &QCheckBox::toggled, this, [this]() { updateEditorStandardFrequency(); });
    connect(m_addButton, &QPushButton::clicked, this, [this]() {
        m_entries.append(editorEntry());
        refreshTable();
        m_table->selectRow(m_entries.size() - 1);
    });
    connect(m_updateButton, &QPushButton::clicked, this, [this]() {
        const int row = m_table->currentRow();
        if (row >= 0 && row < m_entries.size()) {
            m_entries[row] = editorEntry();
            refreshTable();
            m_table->selectRow(row);
        }
    });
    connect(m_removeButton, &QPushButton::clicked, this, [this]() {
        const int row = m_table->currentRow();
        if (row >= 0 && row < m_entries.size()) {
            m_entries.removeAt(row);
            refreshTable();
            if (!m_entries.isEmpty()) m_table->selectRow(qMin(row, m_entries.size() - 1));
        }
    });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    updateEditorStandardFrequency();
}

void BandSchedulerDialog::refreshTable()
{
    m_table->setRowCount(m_entries.size());
    for (int r = 0; r < m_entries.size(); ++r) {
        const ScheduledQsyEntry &e = m_entries.at(r);
        const QStringList cells = {
            e.enabled ? L(QStringLiteral("yes"), QStringLiteral("Yes")) : L(QStringLiteral("no"), QStringLiteral("No")),
            e.timeUtc.toString(QStringLiteral("HH:mm")),
            normalizedMode(e.mode),
            e.band,
            frequencyText(e.frequencyHz),
            e.useFtStandard ? L(QStringLiteral("scheduler_standard_ft"), QStringLiteral("standard FT"))
                            : L(QStringLiteral("manual"), QStringLiteral("manual"))
        };
        for (int c = 0; c < cells.size(); ++c) {
            auto *item = new QTableWidgetItem(cells.at(c));
            item->setData(SourceTextRole, cells.at(c));
            m_table->setItem(r, c, item);
        }
    }
    m_table->resizeColumnsToContents();
}

void BandSchedulerDialog::loadRowToEditor(int row)
{
    if (row < 0 || row >= m_entries.size()) return;
    setEditorEntry(m_entries.at(row));
}

ScheduledQsyEntry BandSchedulerDialog::editorEntry() const
{
    ScheduledQsyEntry e;
    e.enabled = m_chkEnabled->isChecked();
    e.timeUtc = m_timeEdit->time();
    e.mode = normalizedMode(m_modeCombo->currentText());
    e.band = m_bandCombo->currentText().trimmed();
    e.useFtStandard = m_chkFtStandard->isChecked();
    e.frequencyHz = m_freqMHz->value() * 1000000.0;
    if (e.useFtStandard) {
        const double standard = standardFtFrequencyHz(e.mode, e.band);
        if (standard > 0.0) e.frequencyHz = standard;
    }
    return e;
}

void BandSchedulerDialog::setEditorEntry(const ScheduledQsyEntry &entry)
{
    m_chkEnabled->setChecked(entry.enabled);
    if (entry.timeUtc.isValid()) m_timeEdit->setTime(entry.timeUtc);
    const int modeIndex = m_modeCombo->findText(normalizedMode(entry.mode), Qt::MatchFixedString);
    m_modeCombo->setCurrentIndex(modeIndex >= 0 ? modeIndex : m_modeCombo->findText(QStringLiteral("FT8")));
    const int bandIndex = m_bandCombo->findText(entry.band, Qt::MatchFixedString);
    m_bandCombo->setCurrentIndex(bandIndex >= 0 ? bandIndex : m_bandCombo->findText(QStringLiteral("20m")));
    m_chkFtStandard->setChecked(entry.useFtStandard);
    if (entry.frequencyHz > 0.0) {
        m_freqMHz->setValue(entry.frequencyHz / 1000000.0);
    }
    updateEditorStandardFrequency();
}

void BandSchedulerDialog::updateEditorStandardFrequency()
{
    const QString mode = normalizedMode(m_modeCombo->currentText());
    const bool isFt = (mode.compare(QStringLiteral("FT4"), Qt::CaseInsensitive) == 0 ||
                       mode.compare(QStringLiteral("FT8"), Qt::CaseInsensitive) == 0);
    m_chkFtStandard->setEnabled(isFt);
    if (!isFt) {
        m_chkFtStandard->setChecked(false);
    }
    const double standard = standardFtFrequencyHz(mode, m_bandCombo->currentText());
    if (m_chkFtStandard->isChecked() && standard > 0.0) {
        const QSignalBlocker block(m_freqMHz);
        m_freqMHz->setValue(standard / 1000000.0);
    }
    m_freqMHz->setEnabled(!m_chkFtStandard->isChecked() || standard <= 0.0);
    if (m_hint != nullptr) {
        if (isFt && standard > 0.0) {
            m_hint->setText(L(QStringLiteral("scheduler_ft_hint"), QStringLiteral("FT standard for this entry: %1. Manual frequency remains available when the standard checkbox is off."))
                            .arg(frequencyText(standard)));
        } else if (isFt) {
            m_hint->setText(L(QStringLiteral("scheduler_ft_no_standard_hint"), QStringLiteral("No built-in FT standard frequency is known for this band/mode; enter the frequency manually.")));
        } else {
            m_hint->setText(L(QStringLiteral("scheduler_manual_hint"), QStringLiteral("Non-FT modes use the manual frequency field. The scheduler only requests CAT QSY; it never starts TX.")));
        }
    }
}

QString BandSchedulerDialog::frequencyText(double hz) const
{
    if (hz <= 0.0) return QStringLiteral("--");
    return QStringLiteral("%1 MHz").arg(hz / 1000000.0, 0, 'f', 6);
}

QList<ScheduledQsyEntry> BandSchedulerDialog::entries() const
{
    QList<ScheduledQsyEntry> out = m_entries;
    std::sort(out.begin(), out.end(), [](const ScheduledQsyEntry &a, const ScheduledQsyEntry &b) {
        if (a.timeUtc == b.timeUtc) return a.mode < b.mode;
        return a.timeUtc < b.timeUtc;
    });
    return out;
}

double BandSchedulerDialog::standardFtFrequencyHz(const QString &modeName, const QString &band)
{
    const bool ft4 = modeName.compare(QStringLiteral("FT4"), Qt::CaseInsensitive) == 0;
    struct FtFreq { const char *band; double ft8Hz; double ft4Hz; };
    static const FtFreq table[] = {
        {"160m", 1840000.0, 0.0}, {"80m", 3573000.0, 3575000.0},
        {"60m", 5357000.0, 0.0}, {"40m", 7074000.0, 7047500.0},
        {"30m", 10136000.0, 10140000.0}, {"20m", 14074000.0, 14080000.0},
        {"17m", 18100000.0, 18104000.0}, {"15m", 21074000.0, 21140000.0},
        {"12m", 24915000.0, 24919000.0}, {"10m", 28074000.0, 28180000.0},
        {"6m", 50313000.0, 50318000.0}, {"2m", 144174000.0, 144170000.0},
        {"70cm", 432174000.0, 432174000.0}, {"23cm", 1296174000.0, 1296174000.0}
    };
    for (const FtFreq &entry : table) {
        if (band.compare(QString::fromLatin1(entry.band), Qt::CaseInsensitive) == 0) {
            return ft4 && entry.ft4Hz > 0.0 ? entry.ft4Hz : entry.ft8Hz;
        }
    }
    return 0.0;
}

QString BandSchedulerDialog::serializeEntries(const QList<ScheduledQsyEntry> &entries)
{
    QJsonArray array;
    for (const ScheduledQsyEntry &e : entries) {
        QJsonObject o;
        o.insert(QStringLiteral("enabled"), e.enabled);
        o.insert(QStringLiteral("time_utc"), e.timeUtc.toString(QStringLiteral("HH:mm")));
        o.insert(QStringLiteral("mode"), normalizedMode(e.mode));
        o.insert(QStringLiteral("band"), e.band.trimmed());
        o.insert(QStringLiteral("frequency_hz"), e.frequencyHz);
        o.insert(QStringLiteral("ft_standard"), e.useFtStandard);
        array.append(o);
    }
    return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

QList<ScheduledQsyEntry> BandSchedulerDialog::deserializeEntries(const QString &json)
{
    QList<ScheduledQsyEntry> out;
    const QJsonDocument doc = QJsonDocument::fromJson(json.trimmed().toUtf8());
    if (!doc.isArray()) return out;
    for (const QJsonValue &value : doc.array()) {
        if (!value.isObject()) continue;
        const QJsonObject o = value.toObject();
        ScheduledQsyEntry e;
        e.enabled = o.value(QStringLiteral("enabled")).toBool(true);
        e.timeUtc = QTime::fromString(o.value(QStringLiteral("time_utc")).toString(), QStringLiteral("HH:mm"));
        if (!e.timeUtc.isValid()) continue;
        e.mode = normalizedMode(o.value(QStringLiteral("mode")).toString(QStringLiteral("FT8")));
        e.band = o.value(QStringLiteral("band")).toString(QStringLiteral("20m"));
        e.frequencyHz = o.value(QStringLiteral("frequency_hz")).toDouble(0.0);
        e.useFtStandard = o.value(QStringLiteral("ft_standard")).toBool(false);
        if (e.useFtStandard) {
            const double standard = standardFtFrequencyHz(e.mode, e.band);
            if (standard > 0.0) e.frequencyHz = standard;
        }
        if (e.frequencyHz > 0.0) out.append(e);
    }
    return out;
}
