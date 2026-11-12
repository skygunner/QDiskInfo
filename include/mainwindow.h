#pragma once

#include "utils.h"
#include "gridview.h"
#include "disksummary.h"
#include "magicianview.h"
#include "crystalview.h"
#include "theme.h"

#include <QAction>
#include <QActionGroup>
#include <QJsonObject>
#include <QLabel>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPushButton>
#include <QSettings>
#include <QTableWidget>
#include <QWidget>
#include <QProcess>
#include <cmath>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    // Switches the interface mode at runtime (View menu / programmatic hook).
    void setUiMode(UiMode mode, bool resizeWindow);

private slots:
    void on_actionQuit_triggered();
    void on_actionSave_JSON_triggered();
    void on_actionGitHub_triggered();
    void on_actionAbout_QDiskInfo_triggered();
    void on_actionAbout_Qt_triggered();
    void on_actionRescan_Refresh_triggered();
    void on_actionIgnore_C4_Reallocation_Event_Count_toggled(bool enabled);
    void on_actionHEX_toggled(bool enabled);
    void on_actionUse_Fahrenheit_toggled(bool enabled);
    void on_actionCyclic_Navigation_toggled(bool arg1);
    void on_actionUse_GB_instead_of_TB_toggled(bool arg1);

    void on_actionClear_Settings_triggered();

    void on_actionASCII_View_triggered();

    void on_actionGrid_View_triggered();

    void on_actionSave_Image_triggered();

    void onMagicianStyleToggled(bool checked);

    void onCrystalStyleToggled(bool checked);

    void onClassicStyleToggled(bool checked);

private:
    Ui::MainWindow *ui;
    QSettings settings;
    bool initializing;
    QLocale locale;
    utils Utils;
    QButtonGroup *buttonGroup;
    QHBoxLayout *horizontalLayout;
    QVBoxLayout *verticalLayout;
    QGridLayout *gridLayout;
    QLabel *diskName, *temperatureValue, *healthStatusValue;
    QLabel *temperatureLabelHorizontal, *healthStatusLabelHorizontal, *temperatureValueHorizontal, *healthStatusValueHorizontal;
    QLineEdit *firmwareLineEdit, *serialNumberLineEdit, *typeLineEdit, *protocolLineEdit, *deviceNodeLineEdit;
    QLineEdit *totalReadsLineEdit, *totalWritesLineEdit, *rotationRateLineEdit, *powerOnCountLineEdit, *powerOnHoursLineEdit;
    QTableWidget *tableWidget;
    QPushButton *prevButton, *nextButton;
    QColor goodColor, cautionColor, badColor, naColor;
    QLabel *statusLabel;
    QJsonObject deviceJson;
    QSpacerItem *buttonStretch = nullptr;
    QAction *actionCyclic_Navigation;
    QMenu *menuDevice;
    QMenu *menuDisk;
    QMenu *selfTestMenu;
    QAction *selfTestLogAction;
    QActionGroup *disksGroup;
    QJsonArray devices;
    QStringList deviceOutputs;
    int scanCompleted = 0;
    int scanFailed = 0;
    QJsonObject globalObj;
    QString globalHealth;
    bool globalIsNvme;
    QVector<QPair<QString, int>> globalNvmeSmartOrdered;
    GridView *gridView;

    // Themed interface modes (View menu). The classic .ui layout lives in
    // classicPage; Magician/Crystal are dedicated views fed from the same
    // scan/populate pipeline through `summary` and `driveSlots`.
    UiMode uiMode = UiMode::Magician;
    QWidget *classicPage = nullptr;
    MagicianView *magicianView = nullptr;
    CrystalView *crystalView = nullptr;
    DiskSummary summary;
    QVector<DriveSlot> driveSlots;
    QAction *actionMagician_Style = nullptr;
    QAction *actionCrystal_Style = nullptr;
    QAction *actionClassic_Style = nullptr;

    void buildUiModePages();
    void createViewMenu();
    void updateThemedViews();

    void onNextButtonClicked();
    void onPrevButtonClicked();
    void updateNavigationButtons(qsizetype currentIndex);
    void updateUI(const QString &currentDeviceName = QString());
    void populateWindow(const QJsonObject &tempObj, const QString &health, const QVector<QPair<QString, int>>& nvmeLogOrdered = QVector<QPair<QString, int>>());
    void addSCSIErrorCounterLogTable(const QJsonObject &scsiErrorCounterLog);
    void addNvmeLogTable(const QVector<QPair<QString, int>>& nvmeLogOrdered);
    void addSmartAttributesTable(const QJsonArray &attributes);
    void transformWindow();
    void mousePressEvent(QMouseEvent*);

private:
    QString mbSymbol;
    QString gbSymbol;
    QString tbSymbol;
    QString pbSymbol;
    QString getMbToPrettyString(const int64_t &sizeInMbI64, const int &precisionInt, const bool &useGbBool) const;
};
