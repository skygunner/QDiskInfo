#include "drivecard.h"

#include "theme.h"

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QResizeEvent>
#include <QVBoxLayout>

DriveCard::DriveCard(QWidget *parent)
    : QPushButton(parent)
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setMinimumSize(190, 64);
    setMaximumHeight(64);

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(14, 8, 14, 8);
    layout->setSpacing(10);

    dotLabel = new QLabel(this);
    dotLabel->setObjectName(QStringLiteral("cardDot"));
    dotLabel->setFixedSize(10, 10);
    layout->addWidget(dotLabel, 0, Qt::AlignVCenter | Qt::AlignHCenter);

    QVBoxLayout *textLayout = new QVBoxLayout();
    textLayout->setSpacing(2);

    titleLabel = new QLabel(this);
    titleLabel->setObjectName(QStringLiteral("cardTitle"));
    subLabel = new QLabel(this);
    subLabel->setObjectName(QStringLiteral("cardSub"));

    textLayout->addWidget(titleLabel);
    textLayout->addWidget(subLabel);
    layout->addLayout(textLayout, 1);
}

void DriveCard::setSlot(const DriveSlot &slot)
{
    slotData = slot;
    setToolTip(slot.deviceName + QStringLiteral(" · ") + slot.model + QStringLiteral(" · ") + slot.health);
    const QColor dotColor = Theme::statusColor(UiMode::Magician, slot.state);
    dotLabel->setStyleSheet(QStringLiteral("background: %1; border-radius: 5px;").arg(dotColor.name()));
    relayoutText();
}

void DriveCard::resizeEvent(QResizeEvent *event)
{
    QPushButton::resizeEvent(event);
    relayoutText();
}

void DriveCard::relayoutText()
{
    if (slotData.model.isEmpty()) {
        return;
    }

    QFontMetrics titleMetrics(titleLabel->font());
    const int availableWidth = qMax(20, width() - 60);
    titleLabel->setText(titleMetrics.elidedText(slotData.model, Qt::ElideMiddle, availableWidth));
    subLabel->setText(slotData.capacity + QStringLiteral(" · ") + slotData.temperature);
}
