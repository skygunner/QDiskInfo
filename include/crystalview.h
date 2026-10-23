#pragma once

#include "disksummary.h"

#include <QFrame>
#include <QVector>

class HealthRing;
class QLabel;
class QTableWidget;

// "Crystal" style page — a modernised CrystalDiskInfo: dense device overview
// table on top, deep blue-grey drive badge panel, identification/statistics
// panels and the SMART table with tight grid lines. Pure presentation, fed
// by MainWindow like MagicianView.
class CrystalView : public QFrame
{
    Q_OBJECT

public:
    explicit CrystalView(QWidget *parent = nullptr);

    void setDrives(const QVector<DriveSlot> &driveList, int activeIndex);
    void applySummary(const DiskSummary &summary);

signals:
    void driveClicked(int index);

private:
    QTableWidget *createDeviceTable();
    QWidget *createBadgePanel();
    QWidget *createInfoPanels();
    QWidget *createSmartPanel();
    QLabel *groupLabel(const QString &text);
    QLabel *keyLabel(const QString &text);
    QLabel *valueLabel();

    QTableWidget *deviceTable = nullptr;
    QTableWidget *smartTable = nullptr;
    HealthRing *badgeRing = nullptr;
    QLabel *badgeTitle = nullptr;
    QLabel *badgeTemp = nullptr;
    QLabel *badgeHealth = nullptr;
    QLabel *modelValue = nullptr;
    QLabel *firmwareValue = nullptr;
    QLabel *serialValue = nullptr;
    QLabel *protocolValue = nullptr;
    QLabel *nodeValue = nullptr;
    QLabel *capacityValue = nullptr;
    QLabel *rotationValue = nullptr;
    QLabel *readsValue = nullptr;
    QLabel *writesValue = nullptr;
    QLabel *hoursValue = nullptr;
    QLabel *cyclesValue = nullptr;
};
