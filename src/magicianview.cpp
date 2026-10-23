#include "magicianview.h"
#include "drivecard.h"
#include "sidebar.h"
#include "statusdot.h"
#include "tableutil.h"
#include "theme.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSpacerItem>
#include <QTableWidget>
#include <QVBoxLayout>

MagicianView::MagicianView(QWidget *parent)
    : QFrame(parent)
{
    Sidebar *sidebar = new Sidebar(this);
    connect(sidebar, &Sidebar::refreshRequested, this, &MagicianView::refreshRequested);
    connect(sidebar, &Sidebar::gridRequested, this, &MagicianView::gridRequested);

    QVBoxLayout *contentLayout = new QVBoxLayout();
    contentLayout->setContentsMargins(20, 16, 20, 16);
    contentLayout->setSpacing(14);
    contentLayout->addWidget(createHeader());
    contentLayout->addWidget(createHeroCard(), 0);
    contentLayout->addWidget(createDriveStrip(), 0);
    contentLayout->addWidget(createTableSection(), 1);

    QHBoxLayout *rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    rootLayout->addWidget(sidebar);
    rootLayout->addLayout(contentLayout, 1);
}

QWidget *MagicianView::createHeader()
{
    QWidget *header = new QWidget(this);
    QHBoxLayout *layout = new QHBoxLayout(header);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    QLabel *title = new QLabel(tr("DRIVE DETAILS"), header);
    title->setObjectName(QStringLiteral("sectionTitle"));
    QFont titleFont = title->font();
    titleFont.setLetterSpacing(QFont::AbsoluteSpacing, 2.5);
    title->setFont(titleFont);

    QPushButton *prevButton = new QPushButton(QStringLiteral("‹"), header);
    QPushButton *nextButton = new QPushButton(QStringLiteral("›"), header);
    prevButton->setFixedSize(34, 30);
    nextButton->setFixedSize(34, 30);
    prevButton->setToolTip(tr("Previous drive"));
    nextButton->setToolTip(tr("Next drive"));
    prevButton->setShortcut(QKeySequence::Back);
    nextButton->setShortcut(QKeySequence::Forward);
    connect(prevButton, &QPushButton::clicked, this, [this]() { emit navigateRequested(-1); });
    connect(nextButton, &QPushButton::clicked, this, [this]() { emit navigateRequested(1); });

    QPushButton *refreshButton = new QPushButton(tr("Refresh"), header);
    refreshButton->setObjectName(QStringLiteral("accentButton"));
    refreshButton->setCursor(Qt::PointingHandCursor);
    connect(refreshButton, &QPushButton::clicked, this, &MagicianView::refreshRequested);

    layout->addWidget(title);
    layout->addStretch(1);
    layout->addWidget(prevButton);
    layout->addWidget(nextButton);
    layout->addSpacing(6);
    layout->addWidget(refreshButton);
    return header;
}

QLabel *MagicianView::makeKeyLabel(const QString &text)
{
    QLabel *label = new QLabel(text, this);
    label->setObjectName(QStringLiteral("fieldKey"));
    return label;
}

QLabel *MagicianView::makeValueLabel()
{
    QLabel *label = new QLabel(this);
    label->setObjectName(QStringLiteral("fieldValue"));
    QFont valueFont = label->font();
    valueFont.setFamilies({QStringLiteral("Monospace"), QStringLiteral("DejaVu Sans Mono")});
    valueFont.setBold(true);
    label->setFont(valueFont);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}

QFrame *MagicianView::createHeroCard()
{
    QFrame *card = new QFrame(this);
    card->setObjectName(QStringLiteral("heroCard"));

    QVBoxLayout *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(22, 20, 22, 20);
    cardLayout->setSpacing(16);

    healthRing = new HealthRing(card);
    healthRing->setFixedSize(176, 176);
    healthRing->setTrackColor(Theme::token(UiMode::Magician, QStringLiteral("stroke_field")));
    healthRing->setCaptionColor(Theme::token(UiMode::Magician, QStringLiteral("text_primary")));
    healthRing->setBottomCaptionColor(Theme::token(UiMode::Magician, QStringLiteral("text_secondary")));

    QGridLayout *fields = new QGridLayout();
    fields->setHorizontalSpacing(28);
    fields->setVerticalSpacing(10);

    firmwareValue = makeValueLabel();
    serialValue = makeValueLabel();
    capacityValue = makeValueLabel();
    interfaceValue = makeValueLabel();
    protocolValue = makeValueLabel();
    nodeValue = makeValueLabel();
    rotationValue = makeValueLabel();

    fields->addWidget(makeKeyLabel(tr("FIRMWARE")), 0, 0);
    fields->addWidget(firmwareValue, 1, 0);
    fields->addWidget(makeKeyLabel(tr("SERIAL NUMBER")), 0, 1);
    fields->addWidget(serialValue, 1, 1);
    fields->addWidget(makeKeyLabel(tr("CAPACITY")), 2, 0);
    fields->addWidget(capacityValue, 3, 0);
    fields->addWidget(makeKeyLabel(tr("INTERFACE")), 2, 1);
    fields->addWidget(interfaceValue, 3, 1);
    fields->addWidget(makeKeyLabel(tr("PROTOCOL")), 4, 0);
    fields->addWidget(protocolValue, 5, 0);
    fields->addWidget(makeKeyLabel(tr("DEVICE NODE")), 4, 1);
    fields->addWidget(nodeValue, 5, 1);
    fields->addWidget(makeKeyLabel(tr("ROTATION RATE")), 6, 0);
    fields->addWidget(rotationValue, 7, 0);
    fields->setRowStretch(8, 1);
    fields->setColumnStretch(2, 1);

    QWidget *tempPanel = new QWidget(card);
    QVBoxLayout *tempLayout = new QVBoxLayout(tempPanel);
    tempLayout->setContentsMargins(0, 0, 0, 0);
    tempLayout->setSpacing(6);
    tempRing = new HealthRing(tempPanel);
    tempRing->setFixedSize(88, 88);
    tempRing->setTrackColor(Theme::token(UiMode::Magician, QStringLiteral("stroke_field")));
    tempRing->setCaptionColor(Theme::token(UiMode::Magician, QStringLiteral("text_primary")));
    QLabel *tempKey = makeKeyLabel(tr("TEMPERATURE"));
    tempKey->setAlignment(Qt::AlignHCenter);
    tempLayout->addWidget(tempRing, 0, Qt::AlignHCenter);
    tempLayout->addWidget(tempKey);

    heroTitle = new QLabel(card);
    heroTitle->setObjectName(QStringLiteral("heroTitle"));
    heroSubtitle = new QLabel(card);
    heroSubtitle->setObjectName(QStringLiteral("heroSubtitle"));

    QVBoxLayout *titleLayout = new QVBoxLayout();
    titleLayout->setSpacing(4);
    titleLayout->addWidget(heroTitle);
    titleLayout->addWidget(heroSubtitle);
    titleLayout->addStretch(1);

    QHBoxLayout *topLayout = new QHBoxLayout();
    topLayout->setSpacing(32);
    topLayout->addWidget(healthRing, 0, Qt::AlignVCenter);
    topLayout->addLayout(titleLayout, 0);
    topLayout->addLayout(fields, 2);
    topLayout->addStretch(1);
    topLayout->addWidget(tempPanel, 0, Qt::AlignTop);

    cardLayout->addLayout(topLayout);

    QHBoxLayout *metricsLayout = new QHBoxLayout();
    metricsLayout->setSpacing(12);

    auto addMetric = [&metricsLayout, this](const QString &key, QLabel **valueLabel) {
        QFrame *tile = new QFrame(this);
        tile->setObjectName(QStringLiteral("metricTile"));
        QVBoxLayout *tileLayout = new QVBoxLayout(tile);
        tileLayout->setContentsMargins(14, 10, 14, 10);
        tileLayout->setSpacing(2);
        QLabel *keyLabel = new QLabel(key, tile);
        keyLabel->setObjectName(QStringLiteral("metricKey"));
        *valueLabel = new QLabel(tile);
        (*valueLabel)->setObjectName(QStringLiteral("metricValue"));
        QFont metricFont = (*valueLabel)->font();
        metricFont.setFamilies({QStringLiteral("Monospace"), QStringLiteral("DejaVu Sans Mono")});
        metricFont.setBold(true);
        (*valueLabel)->setFont(metricFont);
        tileLayout->addWidget(keyLabel);
        tileLayout->addWidget(*valueLabel);
        metricsLayout->addWidget(tile, 1);
    };

    addMetric(tr("TOTAL HOST READS"), &readsValue);
    addMetric(tr("TOTAL HOST WRITES"), &writesValue);
    addMetric(tr("POWER ON HOURS"), &hoursValue);
    addMetric(tr("POWER ON COUNT"), &cyclesValue);

    cardLayout->addLayout(metricsLayout);
    return card;
}

QWidget *MagicianView::createDriveStrip()
{
    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setFixedHeight(78);

    stripContainer = new QWidget(scrollArea);
    stripContainer->setObjectName(QStringLiteral("driveStrip"));
    QHBoxLayout *stripLayout = new QHBoxLayout(stripContainer);
    stripLayout->setContentsMargins(0, 0, 0, 0);
    stripLayout->setSpacing(10);
    stripLayout->addStretch(1);

    scrollArea->setWidget(stripContainer);
    cardGroup = new QButtonGroup(this);
    cardGroup->setExclusive(true);
    return scrollArea;
}

QWidget *MagicianView::createTableSection()
{
    QWidget *section = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(section);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    QLabel *title = new QLabel(tr("S.M.A.R.T. DATA"), section);
    title->setObjectName(QStringLiteral("sectionTitle"));
    QFont titleFont = title->font();
    titleFont.setLetterSpacing(QFont::AbsoluteSpacing, 2.5);
    title->setFont(titleFont);

    smartTable = new QTableWidget(section);
    smartTable->setAlternatingRowColors(true);
    smartTable->setShowGrid(false);
    smartTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    smartTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    smartTable->setSelectionMode(QAbstractItemView::NoSelection);
    smartTable->verticalHeader()->setVisible(false);
    smartTable->setItemDelegateForColumn(0, new StatusDot(smartTable));

    layout->addWidget(title);
    layout->addWidget(smartTable, 1);
    return section;
}

void MagicianView::setDriveSlots(const QVector<DriveSlot> &driveList, int activeIndex)
{
    const QList<QAbstractButton *> oldButtons = cardGroup->buttons();
    for (QAbstractButton *button : oldButtons) {
        cardGroup->removeButton(button);
        delete button;
    }

    QHBoxLayout *stripLayout = qobject_cast<QHBoxLayout *>(stripContainer->layout());
    if (!stripLayout) {
        return;
    }

    // Keep the trailing stretch as the last item; insert cards before it.
    for (int i = 0; i < driveList.size(); ++i) {
        DriveCard *card = new DriveCard(stripContainer);
        card->setSlot(driveList[i]);
        cardGroup->addButton(card);
        connect(card, &DriveCard::clicked, this, [this, i]() { emit driveClicked(i); });
        stripLayout->insertWidget(stripLayout->count() - 1, card);
    }

    const QList<QAbstractButton *> buttons = cardGroup->buttons();
    if (activeIndex >= 0 && activeIndex < buttons.size()) {
        buttons.at(activeIndex)->setChecked(true);
    }
}

void MagicianView::applySummary(const DiskSummary &summary)
{
    heroTitle->setText(summary.model);
    heroSubtitle->setText(summary.capacity + QStringLiteral("  ·  ") + summary.protocol
                          + QStringLiteral("  ·  ") + summary.node);

    firmwareValue->setText(summary.firmware);
    serialValue->setText(summary.serial);
    capacityValue->setText(summary.capacity);
    interfaceValue->setText(summary.type);
    protocolValue->setText(summary.protocol);
    nodeValue->setText(summary.node);
    rotationValue->setText(summary.rotationRate);

    readsValue->setText(summary.totalReads);
    writesValue->setText(summary.totalWrites);
    hoursValue->setText(summary.powerOnHours);
    cyclesValue->setText(summary.powerOnCount);

    if (summary.lifePercent >= 0) {
        healthRing->setGauge(summary.lifePercent, Theme::statusColor(UiMode::Magician, summary.healthState),
                             QString::number(summary.lifePercent) + QStringLiteral("%"),
                             summary.health);
    } else {
        healthRing->setGauge(-1, Theme::statusColor(UiMode::Magician, summary.healthState), QString(), summary.health);
    }

    if (summary.temperature >= 0) {
        const int tempPct = qBound(0, summary.temperature * 100 / 60, 100);
        tempRing->setGauge(tempPct, Theme::statusColor(UiMode::Magician, summary.temperatureState),
                           QString::number(summary.temperature) + QStringLiteral("°"), QString());
    } else {
        tempRing->setGauge(-1, Theme::statusColor(UiMode::Magician, summary.temperatureState), QStringLiteral("--"), QString());
    }

    TableUtil::apply(smartTable, summary.table, 30, UiMode::Magician);
}
