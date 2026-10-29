// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QApplication>
#include <QClipboard>
#include <QHeaderView>
#include <QKeySequence>
#include <QShortcut>
#include <QTableWidget>
#include <algorithm>

namespace MadModemUi {
inline QString selectedDecodeRowsText(const QTableWidget &table)
{
    if (!table.selectionModel()) return {};
    auto rows = table.selectionModel()->selectedRows();
    std::sort(rows.begin(), rows.end(), [](const QModelIndex &a, const QModelIndex &b) {
        return a.row() < b.row();
    });
    QStringList lines;
    for (const auto &index : rows) {
        if (table.isRowHidden(index.row())) continue;
        QStringList fields;
        for (int visual = 0; visual < table.columnCount(); ++visual) {
            const int col = table.horizontalHeader()->logicalIndex(visual);
            if (table.isColumnHidden(col)) continue;
            const auto *item = table.item(index.row(), col);
            QString value = item ? item->text() : QString();
            value.replace(QLatin1Char('\t'), QLatin1Char(' '));
            value.replace(QLatin1Char('\r'), QLatin1Char(' '));
            value.replace(QLatin1Char('\n'), QLatin1Char(' '));
            fields.append(value);
        }
        lines.append(fields.join(QLatin1Char('\t')));
    }
    return lines.join(QLatin1Char('\n'));
}

inline void installDecodeTableCopy(QTableWidget *table)
{
    auto *copy = new QShortcut(QKeySequence::Copy, table);
    copy->setContext(Qt::WidgetWithChildrenShortcut);
    QObject::connect(copy, &QShortcut::activated, table, [table]() {
        const QString text = selectedDecodeRowsText(*table);
        if (!text.isEmpty()) QApplication::clipboard()->setText(text);
    });
}
} // namespace MadModemUi
