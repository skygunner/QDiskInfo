#include "crystalview.h"
#include "healthring.h"
#include "statusdot.h"
#include "tableutil.h"
#include "theme.h"

#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>

CrystalView::CrystalView(QWidget *parent)
    : QFrame(parent)
{
    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(16, 12, 16, 12);
    rootLayout->setSpacing(12);

    rootLayout->addWidget(createDeviceTable(), 0);

    QHBoxLayout *midLayout = new QHBoxLayout();
    midLayout->setSpacing(12);
    midLayout->addWidget(createBadgePanel(), 0);
    midLayout->addWidget(createInfoPanels(), 1);
    rootLayout->addLayout(midLayout, 0);

    rootLayout->addWidget(createSmartPanel(), 1);
}

QTableWidget *CrystalView::createDeviceTable()
{
    deviceTable = new QTableWidget(this);
    deviceTable->setAlternatingRowColors(true);
    deviceTable->setShowGrid(true);
    deviceTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    deviceTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    deviceTable->setSelectionMode(QAbstractItemView::SingleSelection);
    deviceTable->verticalHeader()->setVisible(false);
    deviceTable->setItemDelegateForColumn(0, new StatusDot(deviceTable));
    deviceTable->setColumnCount(7);
    deviceTable->setHorizontalHeaderLabels({QString(), tr("Model"), tr("Capacity"), tr("Temperature"),
                                            tr("Health"), tr("Protocol"), tr("Device Node")});
    connect(deviceTable, &QTableWidget::cellClicked, this, [this](int row, int) {
        emit driveClicked(row);
    });
    return deviceTable;
}

QWidget *CrystalView::createBadgePanel()
{
    QFrame *panel = new QFrame(this);
    panel->setObjectName(QStringLiteral("badgePanel"));
    panel->setFixedWidth(230);

    QVBoxLayout *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(18, 14, 18, 14);
    layout->setSpacing(6);

    badgeTitle = new QLabel(panel);
    badgeTitle->setObjectName(QStringLiteral("heroTitle"));
    badgeTitle->setAlignment(Qt::AlignHCenter);
    QFont titleFont = badgeTitle->font();
    titleFont.setPixelSize(15);
    titleFont.setBold(true);
    badgeTitle->setFont(titleFont);
    badgeTitle->setWordWrap(true);

    badgeRing = new HealthRing(panel);
    badgeRing->setFixedSize(116, 116);
    badgeRing->setTrackColor(QColor(0x31, 0x46, 0x5F));
    badgeRing->setCaptionColor(Qt::white);

    badgeTemp = new QLabel(panel);
    badgeTemp->setObjectName(QStringLiteral("badgeTemp"));
    badgeTemp->setAlignment(Qt::AlignHCenter);

    badgeHealth = new QLabel(panel);
    badgeHealth->setObjectName(QStringLiteral("badgeHealth"));
    badgeHealth->setAlignment(Qt::AlignHCenter);

    layout->addStretch(1);
    layout->addWidget(badgeTitle);
    layout->addWidget(badgeRing, 0, Qt::AlignHCenter);
    layout->addWidget(badgeTemp);
    layout->addWidget(badgeHealth);
    layout->addStretch(1);
    return panel;
}

QLabel *CrystalView::groupLabel(const QString &text)
{
    QLabel *label = new QLabel(text, this);
    label->setObjectName(QStringLiteral("groupTitle"));
    QFont font = label->font();
    font.setLetterSpacing(QFont::AbsoluteSpacing, 1.8);
    label->setFont(font);
    return label;
}

QLabel *CrystalView::keyLabel(const QString &text)
{
    QLabel *label = new QLabel(text, this);
    label->setObjectName(QStringLiteral("fieldKey"));
    return label;
}

QLabel *CrystalView::valueLabel()
{
    QLabel *label = new QLabel(this);
    label->setObjectName(QStringLiteral("fieldValue"));
    QFont font = label->font();
    font.setFamilies({QStringLiteral("Monospace"), QStringLiteral("DejaVu Sans Mono")});
    font.setBold(true);
    label->setFont(font);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}

QWidget *CrystalView::createInfoPanels()
{
    QWidget *container = new QWidget(this);
    QHBoxLayout *containerLayout = new QHBoxLayout(container);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(12);

    QFrame *identPanel = new QFrame(container);
    identPanel->setObjectName(QStringLiteral("panelGroup"));
    QVBoxLayout *identLayout = new QVBoxLayout(identPanel);
    identLayout->setContentsMargins(16, 10, 16, 12);
    identLayout->setSpacing(6);

    QGridLayout *identGrid = new QGridLayout();
    identGrid->setHorizontalSpacing(24);
    identGrid->setVerticalSpacing(6);
    identGrid->setColumnStretch(1, 1);

    modelValue = valueLabel();
    firmwareValue = valueLabel();
    serialValue = valueLabel();
    protocolValue = valueLabel();
    nodeValue = valueLabel();
    capacityValue = valueLabel();

    identGrid->addWidget(keyLabel(tr("MODEL")), 0, 0);
    identGrid->addWidget(modelValue, 0, 1);
    identGrid->addWidget(keyLabel(tr("FIRMWARE")), 1, 0);
    identGrid->addWidget(firmwareValue, 1, 1);
    identGrid->addWidget(keyLabel(tr("SERIAL NUMBER")), 2, 0);
    identGrid->addWidget(serialValue, 2, 1);
    identGrid->addWidget(keyLabel(tr("PROTOCOL")), 3, 0);
    identGrid->addWidget(protocolValue, 3, 1);
    identGrid->addWidget(keyLabel(tr("DEVICE NODE")), 4, 0);
    identGrid->addWidget(nodeValue, 4, 1);
    identGrid->addWidget(keyLabel(tr("CAPACITY")), 5, 0);
    identGrid->addWidget(capacityValue, 5, 1);

    identLayout->addWidget(groupLabel(tr("IDENTIFICATION")));
    identLayout->addLayout(identGrid);
    identLayout->addStretch(1);

    QFrame *statPanel = new QFrame(container);
    statPanel->setObjectName(QStringLiteral("panelGroup"));
    QVBoxLayout *statLayout = new QVBoxLayout(statPanel);
    statLayout->setContentsMargins(16, 10, 16, 12);
    statLayout->setSpacing(6);

    QGridLayout *statGrid = new QGridLayout();
    statGrid->setHorizontalSpacing(24);
    statGrid->setVerticalSpacing(6);
    statGrid->setColumnStretch(1, 1);

    readsValue = valueLabel();
    writesValue = valueLabel();
    hoursValue = valueLabel();
    cyclesValue = valueLabel();
    rotationValue = valueLabel();

    statGrid->addWidget(keyLabel(tr("TOTAL HOST READS")), 0, 0);
    statGrid->addWidget(readsValue, 0, 1);
    statGrid->addWidget(keyLabel(tr("TOTAL HOST WRITES")), 1, 0);
    statGrid->addWidget(writesValue, 1, 1);
    statGrid->addWidget(keyLabel(tr("POWER ON HOURS")), 2, 0);
    statGrid->addWidget(hoursValue, 2, 1);
    statGrid->addWidget(keyLabel(tr("POWER ON COUNT")), 3, 0);
    statGrid->addWidget(cyclesValue, 3, 1);
    statGrid->addWidget(keyLabel(tr("ROTATION RATE")), 4, 0);
    statGrid->addWidget(rotationValue, 4, 1);

    statLayout->addWidget(groupLabel(tr("STATISTICS")));
    statLayout->addLayout(statGrid);
    statLayout->addStretch(1);

    containerLayout->addWidget(identPanel, 1);
    containerLayout->addWidget(statPanel, 1);
    return container;
}

QWidget *CrystalView::createSmartPanel()
{
    QFrame *panel = new QFrame(this);
    panel->setObjectName(QStringLiteral("panelGroup"));
    QVBoxLayout *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(8);

    smartTable = new QTableWidget(panel);
    smartTable->setAlternatingRowColors(true);
    smartTable->setShowGrid(true);
    smartTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    smartTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    smartTable->setSelectionMode(QAbstractItemView::NoSelection);
    smartTable->verticalHeader()->setVisible(false);
    smartTable->setItemDelegateForColumn(0, new StatusDot(smartTable));

    layout->addWidget(groupLabel(tr("S.M.A.R.T. DATA")));
    layout->addWidget(smartTable, 1);
    return panel;
}

void CrystalView::setDrives(const QVector<DriveSlot> &driveList, int activeIndex)
{
    deviceTable->clearContents();
    deviceTable->setRowCount(static_cast<int>(driveList.size()));

    for (int row = 0; row < driveList.size(); ++row) {
        const DriveSlot &slot = driveList.at(row);

        QTableWidgetItem *statusItem = new QTableWidgetItem();
        statusItem->setData(Qt::BackgroundRole, QVariant(Theme::statusColor(UiMode::Crystal, slot.state)));
        deviceTable->setItem(row, 0, statusItem);

        QTableWidgetItem *modelItem = new QTableWidgetItem(slot.model);
        modelItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        deviceTable->setItem(row, 1, modelItem);

        QTableWidgetItem *capacityItem = new QTableWidgetItem(slot.capacity);
        capacityItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        deviceTable->setItem(row, 2, capacityItem);

        QTableWidgetItem *tempItem = new QTableWidgetItem(slot.temperature);
        tempItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        deviceTable->setItem(row, 3, tempItem);

        QTableWidgetItem *healthItem = new QTableWidgetItem(slot.health);
        healthItem->setTextAlignment(Qt::AlignCenter);
        healthItem->setForeground(Theme::statusColor(UiMode::Crystal, slot.state));
        QFont healthFont = healthItem->font();
        healthFont.setBold(true);
        healthItem->setFont(healthFont);
        deviceTable->setItem(row, 4, healthItem);

        QTableWidgetItem *protocolItem = new QTableWidgetItem(slot.protocol);
        protocolItem->setTextAlignment(Qt::AlignCenter);
        deviceTable->setItem(row, 5, protocolItem);

        QTableWidgetItem *nodeItem = new QTableWidgetItem(slot.deviceName);
        nodeItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        deviceTable->setItem(row, 6, nodeItem);
    }

    deviceTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    deviceTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Stretch);
    for (int column : {0, 2, 3, 4, 5}) {
        deviceTable->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    }
    deviceTable->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    deviceTable->verticalHeader()->setDefaultSectionSize(26);

    if (activeIndex >= 0 && activeIndex < driveList.size()) {
        deviceTable->selectRow(activeIndex);
    }
}

void CrystalView::applySummary(const DiskSummary &summary)
{
    badgeTitle->setText(summary.model);
    modelValue->setText(summary.model);
    firmwareValue->setText(summary.firmware);
    serialValue->setText(summary.serial);
    protocolValue->setText(summary.protocol + QStringLiteral(" / ") + summary.type);
    nodeValue->setText(summary.node);
    capacityValue->setText(summary.capacity);
    rotationValue->setText(summary.rotationRate);

    readsValue->setText(summary.totalReads);
    writesValue->setText(summary.totalWrites);
    hoursValue->setText(summary.powerOnHours);
    cyclesValue->setText(summary.powerOnCount);

    if (summary.lifePercent >= 0) {
        badgeRing->setGauge(summary.lifePercent, Theme::statusColor(UiMode::Crystal, summary.healthState),
                            QString::number(summary.lifePercent) + QStringLiteral("%"),
                            summary.health);
    } else {
        badgeRing->setGauge(-1, Theme::statusColor(UiMode::Crystal, summary.healthState), QString(), summary.health);
    }

    badgeTemp->setText(summary.temperature >= 0
                           ? QString::number(summary.temperature) + QStringLiteral(" °C")
                           : QStringLiteral("-- °C"));
    badgeHealth->setText(summary.health.toUpper());
    badgeHealth->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::statusColor(UiMode::Crystal, summary.healthState).name()));

    TableUtil::apply(smartTable, summary.table, 26, UiMode::Crystal);
}
