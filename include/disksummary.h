#pragma once

#include <QColor>
#include <QString>
#include <QStringList>
#include <QVector>

// One row of a detail table. cells are pre-formatted strings; statusColor
// drives the classic table's dot, state ("ok"/"warn"/"bad"/"na") lets the
// themed views resolve their own token colors at render time.
struct SummaryRow
{
    QColor statusColor;
    QString state;
    QStringList cells;
};

// Protocol-agnostic table payload (ATA SMART attributes, NVMe health log or
// SCSI error counters), consumed by the themed views.
struct SummaryTable
{
    QStringList headers;
    QVector<SummaryRow> rows;
    int stretchColumn = -1;
    QList<int> leftColumns;
    QList<int> rightColumns;
};

// One entry of the drive selector strip.
struct DriveSlot
{
    QString deviceName;
    QString model;
    QString capacity;
    QString temperature;
    QString health;
    QString protocol;
    QString state; // "ok" | "warn" | "bad" | "na"
};

// Everything the themed views need to render the selected drive, produced by
// MainWindow::populateWindow from the smartctl JSON.
struct DiskSummary
{
    QString model;
    QString capacity;
    QString firmware;
    QString serial;
    QString protocol;
    QString type;
    QString node;
    QString health;
    QString healthState;       // "ok" | "warn" | "bad" | "na"
    int lifePercent = -1; // remaining life, -1 when the drive does not report it
    int temperature = -1; // celsius, -1 unknown
    QString temperatureState;  // "ok" | "warn" | "bad" | "na"
    QString totalReads;
    QString totalWrites;
    QString powerOnHours;
    QString powerOnCount;
    QString rotationRate;
    SummaryTable table;
};
