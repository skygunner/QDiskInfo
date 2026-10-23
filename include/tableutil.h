#pragma once

#include "disksummary.h"
#include "theme.h"

class QTableWidget;

// Fills a QTableWidget from a SummaryTable: status-dot column 0, header
// labels, per-column alignment and stretch/resize modes. Shared by the
// themed views; row dots resolve their color from the row state through the
// given theme mode.
namespace TableUtil
{

void apply(QTableWidget *table, const SummaryTable &data, int rowHeight, UiMode mode);

} // namespace TableUtil
