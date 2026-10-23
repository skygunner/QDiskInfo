#pragma once

#include "disksummary.h"

#include <QPushButton>

// Checkable card used in the drive selector strip: status dot, model name
// (elided) and a secondary line with capacity and temperature.
class DriveCard : public QPushButton
{
    Q_OBJECT

public:
    explicit DriveCard(QWidget *parent = nullptr);

    void setSlot(const DriveSlot &slot);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void relayoutText();

    class QLabel *titleLabel;
    class QLabel *subLabel;
    class QLabel *dotLabel;
    DriveSlot slotData;
};
