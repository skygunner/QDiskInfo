#include "tableutil.h"

#include <QHeaderView>
#include <QTableWidget>
#include <QTableWidgetItem>

void TableUtil::apply(QTableWidget *table, const SummaryTable &data, int rowHeight, UiMode mode)
{
    const int columnCount = qMax(1, static_cast<int>(data.headers.size()));
    table->clearContents();
    table->setColumnCount(columnCount);
    table->setRowCount(static_cast<int>(data.rows.size()));
    table->setHorizontalHeaderLabels(data.headers);

    for (int row = 0; row < data.rows.size(); ++row) {
        const SummaryRow &summaryRow = data.rows.at(row);
        QTableWidgetItem *statusItem = new QTableWidgetItem();
        statusItem->setData(Qt::BackgroundRole, QVariant(Theme::statusColor(mode, summaryRow.state)));
        table->setItem(row, 0, statusItem);

        for (int column = 1; column < columnCount && column < summaryRow.cells.size() + 1; ++column) {
            QTableWidgetItem *item = new QTableWidgetItem(summaryRow.cells.at(column - 1));
            if (data.rightColumns.contains(column)) {
                item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            } else if (data.leftColumns.contains(column)) {
                item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            } else {
                item->setTextAlignment(Qt::AlignCenter);
            }
            table->setItem(row, column, item);
        }
    }

    for (int column = 0; column < columnCount; ++column) {
        if (column == data.stretchColumn) {
            table->horizontalHeader()->setSectionResizeMode(column, QHeaderView::Stretch);
        } else {
            table->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
        }
    }
    table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table->verticalHeader()->setDefaultSectionSize(rowHeight);
}
