// SPDX-License-Identifier: GPL-3.0-or-later
#include "widgets/DecodeTableClipboard.h"
#include <QTest>
#include <iostream>
#include <cstdlib>
static void require(bool ok, const char *why) { if (!ok) { std::cerr << why << '\n'; std::exit(1); } }
int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QTableWidget table(4, 3);
    table.setSelectionBehavior(QAbstractItemView::SelectRows);
    table.setSelectionMode(QAbstractItemView::ExtendedSelection);
    table.setEditTriggers(QAbstractItemView::NoEditTriggers);
    const QStringList calls{"CQ IZ6NNH JN63", "CQ EA3HXW JN11", "F4NKT IZ6NNH -13", "CQ DL1ABC JO20"};
    for (int row = 0; row < 4; ++row) {
        table.setItem(row, 0, new QTableWidgetItem(QString::number(153845 + row)));
        table.setItem(row, 1, new QTableWidgetItem(calls[row]));
        table.setItem(row, 2, new QTableWidgetItem(QString::fromUtf8("França")));
    }
    MadModemUi::installDecodeTableCopy(&table);
    table.show(); table.activateWindow(); table.setFocus();
    QTest::qWait(30);
    auto *selection = table.selectionModel();
    selection->select(table.model()->index(2, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
    selection->select(table.model()->index(0, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
    QApplication::clipboard()->setText("previous");
    QTest::keySequence(&table, QKeySequence(QKeySequence::Copy));
    app.processEvents();
    require(QApplication::clipboard()->text() == QString::fromUtf8("153845\tCQ IZ6NNH JN63\tFrança\n153847\tF4NKT IZ6NNH -13\tFrança"), "Ctrl+C noncontiguous rows / Unicode / displayed order");
    QTest::keySequence(&table, QKeySequence(QKeySequence::SelectAll));
    QTest::keySequence(&table, QKeySequence(QKeySequence::Copy));
    require(QApplication::clipboard()->text().split('\n').size() == 4, "Ctrl+A Ctrl+C all rows");
    table.hideRow(1); table.hideColumn(2);
    table.horizontalHeader()->moveSection(1, 0);
    require(MadModemUi::selectedDecodeRowsText(table).startsWith("CQ IZ6NNH JN63\t153845\n"), "hidden rows/columns and column display order");
    require(MadModemUi::selectedDecodeRowsText(table).split('\n').size() == 3, "hidden row omitted");
    table.clearSelection(); QApplication::clipboard()->setText("preserve");
    QTest::keySequence(&table, QKeySequence(QKeySequence::Copy));
    require(QApplication::clipboard()->text() == "preserve", "empty selection preserves clipboard");
    require(table.rowCount() == 4 && table.item(2, 1)->text() == calls[2], "copy leaves data untouched");
    std::cout << "PASS decode table actual Ctrl+C / Ctrl+A / Unicode / hidden data / column order / no selection\n";
}
