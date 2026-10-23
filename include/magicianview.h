#pragma once

#include "disksummary.h"
#include "healthring.h"

#include <QFrame>
#include <QVector>

class QButtonGroup;
class QLabel;
class QPushButton;
class QTableWidget;

// Full "Samsung Magician style" page: icon sidebar + hero card with health
// ring, drive selector strip and the SMART detail table. Pure presentation —
// MainWindow feeds it DiskSummary data and DriveSlot lists.
class MagicianView : public QFrame
{
    Q_OBJECT

public:
    explicit MagicianView(QWidget *parent = nullptr);

    void setDriveSlots(const QVector<DriveSlot> &driveList, int activeIndex);
    void applySummary(const DiskSummary &summary);

signals:
    void driveClicked(int index);
    void navigateRequested(int delta);
    void refreshRequested();
    void gridRequested();

private:
    QWidget *createHeader();
    QFrame *createHeroCard();
    QWidget *createDriveStrip();
    QWidget *createTableSection();
    QLabel *makeKeyLabel(const QString &text);
    QLabel *makeValueLabel();

    HealthRing *healthRing = nullptr;
    HealthRing *tempRing = nullptr;
    QLabel *heroTitle = nullptr;
    QLabel *heroSubtitle = nullptr;
    QLabel *firmwareValue = nullptr;
    QLabel *serialValue = nullptr;
    QLabel *capacityValue = nullptr;
    QLabel *interfaceValue = nullptr;
    QLabel *protocolValue = nullptr;
    QLabel *nodeValue = nullptr;
    QLabel *rotationValue = nullptr;
    QLabel *readsValue = nullptr;
    QLabel *writesValue = nullptr;
    QLabel *hoursValue = nullptr;
    QLabel *cyclesValue = nullptr;
    QTableWidget *smartTable = nullptr;
    QWidget *stripContainer = nullptr;
    QButtonGroup *cardGroup = nullptr;
};
