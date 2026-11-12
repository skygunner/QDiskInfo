#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "asciiview.h"
#include "statusdot.h"
#include "custombutton.h"
#include "jsonparser.h"

#include <QDesktopServices>
#include <QFileDialog>
#include <QJsonDocument>
#include <QMessageBox>
#include <QUrl>
#include <QTextEdit>
#include <QDialogButtonBox>
#include <QAbstractButton>
#include <QMenuBar>
#include <QVBoxLayout>
#include <QStatusBar>
#include <QTimer>

const int PRECISION_TOTALS_TO_STR = 2;
const int PRECISION_CAPACITY_TO_STR = 0;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , settings("qdiskinfo", "qdiskinfo")
    , initializing(true)
    , Utils(this)
    , mbSymbol("MB"), gbSymbol("GB"), tbSymbol("TB"), pbSymbol("PB")
{
    ui->setupUi(this);

    locale = QLocale::system();

    buttonGroup = new QButtonGroup(this);
    buttonGroup->setExclusive(true);

    QWidget *containerWidget = ui->scrollAreaWidgetContents;
    horizontalLayout = new QHBoxLayout(containerWidget);
    horizontalLayout->setContentsMargins(0, 0, 0, 0);
    ui->scrollArea->setWidget(containerWidget);

    verticalLayout = ui->centralwidget->findChild<QVBoxLayout*>("verticalLayout");
    gridLayout = ui->centralwidget->findChild<QGridLayout*>("gridLayout");

    diskName = qobject_cast<QLabel *>(ui->centralwidget->findChild<QLabel*>("diskName"));
    temperatureValue = qobject_cast<QLabel *>(ui->centralwidget->findChild<QLabel*>("temperatureValueLabel"));
    healthStatusValue = qobject_cast<QLabel *>(ui->centralwidget->findChild<QLabel*>("healthStatusValueLabel"));

    temperatureLabelHorizontal = qobject_cast<QLabel *>(ui->centralwidget->findChild<QLabel*>("healthStatusLabelHorizozontal"));
    healthStatusLabelHorizontal = qobject_cast<QLabel *>(ui->centralwidget->findChild<QLabel*>("temperatureLabelHorizontal"));
    temperatureValueHorizontal = qobject_cast<QLabel *>(ui->centralwidget->findChild<QLabel*>("temperatureValueLabelHorizontal"));
    healthStatusValueHorizontal = qobject_cast<QLabel *>(ui->centralwidget->findChild<QLabel*>("healthStatusValueLabelHorizontal"));

    firmwareLineEdit = qobject_cast<QLineEdit *>(ui->centralwidget->findChild<QLineEdit*>("firmwareLineEdit"));
    serialNumberLineEdit = qobject_cast<QLineEdit *>(ui->centralwidget->findChild<QLineEdit*>("serialNumberLineEdit"));
    typeLineEdit = qobject_cast<QLineEdit *>(ui->centralwidget->findChild<QLineEdit*>("typeLineEdit"));
    protocolLineEdit = qobject_cast<QLineEdit *>(ui->centralwidget->findChild<QLineEdit*>("protocolLineEdit"));
    deviceNodeLineEdit = qobject_cast<QLineEdit *>(ui->centralwidget->findChild<QLineEdit*>("deviceNodeLineEdit"));

    totalReadsLineEdit = qobject_cast<QLineEdit *>(ui->centralwidget->findChild<QLineEdit*>("totalReadsLineEdit"));
    totalWritesLineEdit = qobject_cast<QLineEdit *>(ui->centralwidget->findChild<QLineEdit*>("totalWritesLineEdit"));
    rotationRateLineEdit = qobject_cast<QLineEdit *>(ui->centralwidget->findChild<QLineEdit*>("rotationRateLineEdit"));
    powerOnCountLineEdit = qobject_cast<QLineEdit *>(ui->centralwidget->findChild<QLineEdit*>("powerOnCountLineEdit"));
    powerOnHoursLineEdit = qobject_cast<QLineEdit *>(ui->centralwidget->findChild<QLineEdit*>("powerOnHoursLineEdit"));

    tableWidget = qobject_cast<QTableWidget *>(ui->centralwidget->findChild<QTableWidget*>("dataTable"));
    serialNumberLineEdit->setEchoMode(QLineEdit::Password);

    nextButton = ui->centralwidget->findChild<QPushButton*>("nextButton");
    prevButton = ui->centralwidget->findChild<QPushButton*>("previousButton");
    nextButton->setShortcut(QKeySequence::Forward);
    prevButton->setShortcut(QKeySequence::Back);

    connect(nextButton, &QPushButton::clicked, this, &MainWindow::onNextButtonClicked);
    connect(prevButton, &QPushButton::clicked, this, &MainWindow::onPrevButtonClicked);

    nextButton->setFocusPolicy(Qt::NoFocus);
    prevButton->setFocusPolicy(Qt::NoFocus);

    ui->actionSave_Image->setShortcut(QKeySequence::Save);
    ui->actionQuit->setShortcut(QKeySequence::Quit);
    ui->actionRescan_Refresh->setShortcut(QKeySequence::Refresh);

    menuDevice = ui->menuDevice;
    menuDisk = ui->menuDisk;

    selfTestMenu = new QMenu(tr("Start Self Test"), menuDevice);
    menuDevice->addMenu(selfTestMenu);
    selfTestMenu->setToolTipsVisible(true);

    selfTestLogAction = new QAction(tr("Self Test Log"), this);
    menuDevice->addAction(selfTestLogAction);

    disksGroup = new QActionGroup(this);
    disksGroup->setExclusive(true);

    goodColor = QColor(Qt::green);
    cautionColor = QColor(Qt::yellow);
    badColor = QColor(Qt::red);
    naColor = QColor(Qt::gray);

    statusLabel = new QLabel;

    gridView = new GridView(this);
    gridView->setWindowFlag(Qt::Window);

    buildUiModePages();
    createViewMenu();
    setUiMode(Theme::modeFromString(settings.value("ui/mode", QStringLiteral("magician")).toString()), true);

    ui->actionIgnore_C4_Reallocation_Event_Count->setChecked(settings.value("IgnoreC4", true).toBool());
    ui->actionHEX->setChecked(settings.value("HEX", true).toBool());
    ui->actionUse_Fahrenheit->setChecked(settings.value("Fahrenheit", false).toBool());
    ui->actionCyclic_Navigation->setChecked(settings.value("CyclicNavigation", false).toBool());
    ui->actionUse_GB_instead_of_TB->setChecked(settings.value("UseGB", false).toBool());

    QAction *toggleEchoModeAction = serialNumberLineEdit->addAction(QIcon::fromTheme(QStringLiteral("visibility")), QLineEdit::TrailingPosition);
    connect(toggleEchoModeAction, &QAction::triggered, this, [=]() {
        if (serialNumberLineEdit->echoMode() == QLineEdit::Password) {
            serialNumberLineEdit->setEchoMode(QLineEdit::Normal);
            toggleEchoModeAction->setIcon(QIcon::fromTheme(QStringLiteral("hint")));
        } else if (serialNumberLineEdit->echoMode() == QLineEdit::Normal) {
            serialNumberLineEdit->setEchoMode(QLineEdit::Password);
            toggleEchoModeAction->setIcon(QIcon::fromTheme(QStringLiteral("visibility")));
        }
    });

    connect(gridView, &GridView::diskSelected, this, [this](int index) {
        if (index >= 0 && index < buttonGroup->buttons().size()) {
            buttonGroup->buttons().at(index)->click();
        }
    });
    connect(&Utils, &utils::devicesFound, this, [this](const QJsonArray &found) {
        devices = found;
        deviceOutputs.clear();
        for (int i = 0; i < devices.size(); ++i) {
            deviceOutputs.append(QString());
        }
        updateUI();
        statusBar()->showMessage(devices.isEmpty() ? tr("No disks found") : tr("Reading S.M.A.R.T. data…"));
    });
    connect(&Utils, &utils::deviceReady, this, [this](int index, QJsonObject device, const QString &output, const QString &error) {
        if (index < 0 || index >= devices.size()) {
            return;
        }
        device["scan_error"] = error;
        devices[index] = device;
        deviceOutputs[index] = output;
        ++scanCompleted;
        if (!error.isEmpty()) {
            ++scanFailed;
        }
        updateUI();
        statusBar()->showMessage(tr("Reading disks: %1/%2 (%3 failed)").arg(scanCompleted).arg(devices.size()).arg(scanFailed));
    });
    connect(&Utils, &utils::scanFinished, this, [this](const QString &error) {
        ui->actionRescan_Refresh->setEnabled(true);
        statusBar()->showMessage(error.isEmpty()
            ? tr("Scan complete: %1 disks, %2 failed").arg(devices.size()).arg(scanFailed) : error);
    });
    updateUI();
    this->setFocus();
    initializing = false;
    // The constructor must return before authentication or disk I/O begins.
    QTimer::singleShot(0, this, &MainWindow::on_actionRescan_Refresh_triggered);

    if (!INCLUDE_OPTIONAL_RESOURCES || CHARACTER_IS_RIGHT) {
        QLayoutItem *leftSpacerItem = gridLayout->itemAtPosition(2, 0);
        if (leftSpacerItem) {
            gridLayout->removeItem(leftSpacerItem);
            if (dynamic_cast<QSpacerItem*>(leftSpacerItem)) {
                delete leftSpacerItem;
            }
            else if (QWidget *widget = leftSpacerItem->widget()) {
                delete widget;
            } else {
                delete leftSpacerItem;
            }
        }
    }

    if (INCLUDE_OPTIONAL_RESOURCES) {
        transformWindow();
    } else {
        // These horizontal variants are unused without the optional theme
        // resources. Deleting widgets that still sit in the .ui form layouts
        // leaves dangling items behind after the layout re-parenting done by
        // buildUiModePages() and crashes later layout passes (e.g. Save
        // Image), so hide them instead.
        temperatureLabelHorizontal->hide();
        healthStatusLabelHorizontal->hide();
        temperatureValueHorizontal->hide();
        healthStatusValueHorizontal->hide();
    }

    mbSymbol = locale.formattedDataSize(1 << 20, 1, QLocale::DataSizeTraditionalFormat).split(' ')[1];
    gbSymbol = locale.formattedDataSize(1 << 30, 1, QLocale::DataSizeTraditionalFormat).split(' ')[1];
    tbSymbol = locale.formattedDataSize(qint64(1) << 40, 1, QLocale::DataSizeTraditionalFormat).split(' ')[1];
    pbSymbol = locale.formattedDataSize(qint64(1) << 50, 1, QLocale::DataSizeTraditionalFormat).split(' ')[1];
}

MainWindow::~MainWindow()
{
    disconnect(&Utils, nullptr, this, nullptr);
    Utils.stopScan();
    delete ui;
}

void MainWindow::onNextButtonClicked()
{
    if (buttonGroup->buttons().isEmpty()) {
        return;
    }
    qsizetype currentIndex = buttonGroup->buttons().indexOf(buttonGroup->checkedButton());
    qsizetype nextIndex = (currentIndex + 1) % buttonGroup->buttons().size();
    buttonGroup->buttons().at(static_cast<int>(nextIndex))->click();
    updateNavigationButtons(nextIndex);
}

void MainWindow::onPrevButtonClicked()
{
    if (buttonGroup->buttons().isEmpty()) {
        return;
    }
    qsizetype currentIndex = buttonGroup->buttons().indexOf(buttonGroup->checkedButton());
    qsizetype prevIndex = (currentIndex - 1 + buttonGroup->buttons().size()) % buttonGroup->buttons().size();
    buttonGroup->buttons().at(static_cast<int>(prevIndex))->click();
    updateNavigationButtons(prevIndex);
}

void MainWindow::updateNavigationButtons(qsizetype currentIndex)
{
    const qsizetype totalButtons = buttonGroup->buttons().size();
    const bool isCyclic = ui->actionCyclic_Navigation->isChecked();

    prevButton->setEnabled( // We can use setVisible if we want to mimic CrystalDiskInfo
        (currentIndex > 0) || (isCyclic && totalButtons > 1)
    );

    nextButton->setEnabled(
        (currentIndex < totalButtons - 1) || (isCyclic && totalButtons > 1)
    );
}

void MainWindow::updateUI(const QString &currentDeviceName)
{
    const QString previousDeviceName = Utils.clearButtonGroup(buttonGroup, horizontalLayout, buttonStretch, menuDisk);
    buttonStretch = nullptr;
    const QString selectedDeviceName = currentDeviceName.isEmpty() ? previousDeviceName : currentDeviceName;
    globalObj = QJsonObject();
    globalHealth = tr("Unknown");
    globalNvmeSmartOrdered.clear();

    mbSymbol = locale.formattedDataSize(1 << 20, 1, QLocale::DataSizeTraditionalFormat).split(' ')[1];
    gbSymbol = locale.formattedDataSize(1 << 30, 1, QLocale::DataSizeTraditionalFormat).split(' ')[1];
    tbSymbol = locale.formattedDataSize(qint64(1) << 40, 1, QLocale::DataSizeTraditionalFormat).split(' ')[1];
    pbSymbol = locale.formattedDataSize(qint64(1) << 50, 1, QLocale::DataSizeTraditionalFormat).split(' ')[1];

    QVector<DiskItem> diskItems;
    driveSlots.clear();

    bool firstTime = true;
    bool isFahrenheit = ui->actionUse_Fahrenheit->isChecked();
    globalIsNvme = false;

    QString degreeSymbol;
    if (isFahrenheit) { // We don't do Kelvin
        degreeSymbol = "°F";
    } else {
        degreeSymbol = "°C";
    }

    QList<QAction*> oldActions = disksGroup->actions();
    for (QAction *action : std::as_const(oldActions)) {
        disksGroup->removeAction(action);
        menuDisk->removeAction(action);
        delete action;
    }

    int deviceToSelect = -1;

    for (int i = 0; i < devices.size(); ++i) {
        QJsonObject device = devices[i].toObject();
        QString deviceName = device["name"].toString();

        QString allOutput;
        if (i >= 0 && i < deviceOutputs.size()) {
            allOutput = deviceOutputs[i];
        }

        QJsonDocument localDoc = QJsonDocument::fromJson(allOutput.toUtf8());
        QJsonObject localObj = localDoc.object();
        // Keep the raw output intact for the ordered NVMe parser, while using
        // the transport that actually succeeded for display and self-tests.
        QJsonObject connection = localObj["device"].toObject();
        connection["name"] = deviceName;
        connection["type"] = device["type"];
        if (connection["protocol"].toString().isEmpty()) {
            connection["protocol"] = device["protocol"];
        }
        localObj["device"] = connection;

        QString protocol = localObj["device"].toObject()["protocol"].toString();
        bool isNvme = (protocol == "NVMe");
        bool isScsi = (protocol == "SCSI");
        bool isAta = (protocol == "ATA");

        QString modelName = localObj["model_name"].toString();

        if (isScsi) {
            modelName = localObj["scsi_model_name"].toString();
        }

        QJsonArray attributes = localObj["ata_smart_attributes"].toObject()["table"].toArray();
        QString temperature = "-- " + degreeSymbol;
        QJsonValue smartStatusValue = localObj.value("smart_status");
        bool healthPassed = localObj["smart_status"].toObject()["passed"].toBool();
        bool caution = false;
        bool bad = false;
        QString health;
        QColor healthColor;

        double diskCapacityMB = localObj.value("user_capacity").toObject().value("bytes").toDouble() / 1e6;
        int64_t diskCapacityMbI64 = static_cast<int64_t>(diskCapacityMB);
        bool useGB = ui->actionUse_GB_instead_of_TB->isChecked();
        QString diskCapacityString = getMbToPrettyString(diskCapacityMbI64, PRECISION_CAPACITY_TO_STR, useGB);

        QJsonObject temperatureObj = localObj["temperature"].toObject();
        int temperatureInt = temperatureObj["current"].toInt();
        if (temperatureInt > 0) {
            if (isFahrenheit) {
                int fahrenheit = static_cast<int>((temperatureInt * 9.0 / 5.0) + 32.0);
                temperature = QString::number(fahrenheit);
            } else {
                temperature = QString::number(temperatureInt);
            }
            temperature = temperature + " " + degreeSymbol;
        }

        QVector<QPair<QString, int>> nvmeSmartOrdered;
        if (isAta) {
            for (const QJsonValue &attr : std::as_const(attributes)) {
                QJsonObject attrObj = attr.toObject();
                if ((attrObj["id"] == 5 || attrObj["id"] == 197 || attrObj["id"] == 198 || (attrObj["id"] == 196 && !(ui->actionIgnore_C4_Reallocation_Event_Count->isChecked()))) && attrObj["raw"].toObject()["value"].toDouble() > 0) {
                    caution = true;
                }
                if (attrObj["thresh"].toInt() && (attrObj["value"].toInt() < attrObj["thresh"].toInt())) {
                    bad = true;
                }
            }
        } else if (isScsi) {
            QJsonObject scsiErrorCounterLog = localObj.value("scsi_error_counter_log").toObject();
            static const QString keys[] = {
                QStringLiteral("read"),
                QStringLiteral("write"),
                QStringLiteral("verify")
            };
            for (const QString& key : keys) {
                if (scsiErrorCounterLog.value(key).toObject().value("total_uncorrected_errors").toInt() != 0) {
                    caution = true;
                }
            }

            int scsiGrownDefectList = localObj.value("scsi_grown_defect_list").toInt();
            if (scsiGrownDefectList != 0) {
                caution = true;
                QMessageBox::warning(nullptr, tr("Critical Warning"), deviceName + ": " + tr("Grown Defect List") + ": " + QString::number(scsiGrownDefectList));
            }
        } else {
            JsonParser parser;
            nvmeSmartOrdered = parser.parse(allOutput);
            int row = 1;
            for (const QPair<QString, int> &pair : std::as_const(nvmeSmartOrdered)) {
                QString id = QString("%1").arg(row, 2, 16, QChar('0')).toUpper();
                int raw = pair.second;
                if (id == "01" && raw) {
                    bad = true;
                } else if (id == "03") {
                    int availableSpareThreshold = nvmeSmartOrdered.at(3).second;
                    if (availableSpareThreshold > 100) { // Thx to crystaldiskinfo for these workarounds
                        ;
                    } else if (raw == 0 && availableSpareThreshold == 0) {
                        ;
                    } else if (raw < availableSpareThreshold) {
                        bad = true;
                    } else if (availableSpareThreshold != 100 && (raw == availableSpareThreshold)) {
                        caution = true;
                    }
                } else if (id == "05" && raw >= 90) { // Make this configurable, currently hardcoded to 10%
                    caution = true;
                }

                ++row;
            }
        }

        QString healthState;
        if (healthPassed && !caution && !bad) {
            health = tr("Good");
            healthColor = goodColor;
            healthState = QStringLiteral("ok");
        } else if (healthPassed && caution && !bad) {
            health = tr("Caution");
            healthColor = cautionColor;
            healthState = QStringLiteral("warn");
        } else if ((bad || !healthPassed) && !modelName.isEmpty() && !smartStatusValue.isNull()){
            health = tr("Bad");
            healthColor = badColor;
            healthState = QStringLiteral("bad");
        } else {
            health = tr("Unknown");
            healthColor = naColor;
            healthState = QStringLiteral("na");
        }

        if (modelName.isEmpty()) {
            modelName = deviceName;
        }
        driveSlots.append({deviceName, modelName, diskCapacityString, temperature, health, protocol, healthState});

        CustomButton *button = new CustomButton(health, temperature, deviceName, healthColor, this);
        const QString error = device["scan_error"].toString();
        button->setToolTip(tr("Disk") + " " + QString::number(i) + " : " + modelName + " : " + diskCapacityString
                           + (error.isEmpty() ? QString() : "\n" + error));
        button->setProperty("deviceName", deviceName);

        buttonGroup->addButton(button);
        horizontalLayout->addWidget(button);

        QAction *diskAction = new QAction("(" + QString::number(i+1) + ") " + modelName + " " + diskCapacityString, this);
        diskAction->setCheckable(true);
        menuDisk->addAction(diskAction);
        disksGroup->addAction(diskAction);

        button->setCheckable(true);
        button->setAutoExclusive(true);

        qsizetype buttonIndex = buttonGroup->buttons().indexOf(button);

        diskItems.append({ deviceName, temperature, health });

        auto updateWindow = [=]() {
            if (isNvme) {
                populateWindow(localObj, health, nvmeSmartOrdered);
            } else {
                populateWindow(localObj, health);
            }
            updateNavigationButtons(buttonIndex);
        };

        connect(button, &QPushButton::clicked, this, [=]() {
            updateWindow();
            disksGroup->actions().at(static_cast<int>(buttonIndex))->setChecked(true);
            gridView->highlightDisk(buttonIndex);
            gridView->setActiveIndex(buttonIndex);
        });

        connect(diskAction, &QAction::triggered, this, [=]() {
            updateWindow();
            button->setChecked(true);
            gridView->highlightDisk(buttonIndex);
            gridView->setActiveIndex(buttonIndex);
        });

        if (firstTime) {
            globalObj = localObj;
            globalHealth = health;
            button->setChecked(true);
            diskAction->setChecked(true);
            gridView->setActiveIndex(0);
            firstTime = false;
            globalIsNvme = isNvme;
            if (isNvme) {
                globalNvmeSmartOrdered = nvmeSmartOrdered;
            }
        }

        if (!selectedDeviceName.isEmpty() && deviceName == selectedDeviceName) {
            deviceToSelect = i;
            globalObj = localObj;
            globalHealth = health;
            globalIsNvme = isNvme;
            if (isNvme) {
                globalNvmeSmartOrdered = nvmeSmartOrdered;
            }
            button->setChecked(true);
            diskAction->setChecked(true);
            firstTime = false;
        }
    }

    buttonStretch = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
    horizontalLayout->addSpacerItem(buttonStretch);

    if (globalIsNvme) {
        populateWindow(globalObj, globalHealth, globalNvmeSmartOrdered);
    } else {
        populateWindow(globalObj, globalHealth);
    }

    gridView->setDisks(diskItems);
    int activeIndex = deviceToSelect >= 0 ? deviceToSelect : 0;
    gridView->setActiveIndex(activeIndex);
    updateNavigationButtons(activeIndex);

    const int stripIndex = static_cast<int>(buttonGroup->buttons().indexOf(buttonGroup->checkedButton()));
    magicianView->setDriveSlots(driveSlots, stripIndex);
    crystalView->setDrives(driveSlots, stripIndex);
}

void MainWindow::populateWindow(const QJsonObject &localObj, const QString &health, const QVector<QPair<QString, int>>& nvmeLogOrdered)
{
    summary = DiskSummary();
    QJsonArray attributes = localObj["ata_smart_attributes"].toObject()["table"].toArray();
    QJsonObject pollingMinutes = localObj["ata_smart_data"].toObject()["self_test"].toObject()["polling_minutes"].toObject();
    QJsonObject nvmeLog = localObj["nvme_smart_health_information_log"].toObject();
    QString modelName = localObj["model_name"].toString();
    QString firmwareVersion = localObj["firmware_version"].toString("----");
    double diskCapacityMB = localObj.value("user_capacity").toObject().value("bytes").toDouble() / 1e6;
    int64_t diskCapacityMbI64 = static_cast<int64_t>(diskCapacityMB);
    int powerCycleCountInt = localObj["power_cycle_count"].toInt(-1);
    QJsonObject temperatureObj = localObj["temperature"].toObject();
    int temperatureInt = temperatureObj["current"].toInt();
    int64_t totalMbWritesI64 = 0;
    int64_t totalMbReadsI64 = 0;
    bool useGB = ui->actionUse_GB_instead_of_TB->isChecked();

    QString diskCapacityString = getMbToPrettyString(diskCapacityMbI64, PRECISION_CAPACITY_TO_STR, useGB);

    QString totalReads;
    QString totalWrites;
    QString percentage = "";
    QString serialNumber = localObj["serial_number"].toString();
    QJsonObject deviceObj = localObj["device"].toObject();
    QString protocol = deviceObj["protocol"].toString();
    QString type = deviceObj["type"].toString();
    QString name = deviceObj["name"].toString();
    QJsonArray outputArray = localObj.value("smartctl").toObject()["output"].toArray();

    QJsonArray nvmeSelfTestsTable = localObj["nvme_self_test_log"].toObject()["table"].toArray();
    QJsonArray ataSelfTestsTable = localObj["ata_smart_self_test_log"].toObject()["standard"].toObject()["table"].toArray();

    QJsonObject scsiErrorCounterLog;

    bool isNvme = (protocol == "NVMe");
    bool isScsi = (protocol == "SCSI");

    if (isScsi) {
        scsiErrorCounterLog = localObj.value("scsi_error_counter_log").toObject();
        modelName = localObj["scsi_model_name"].toString();
        powerCycleCountInt = localObj["scsi_start_stop_cycle_counter"].toObject().value("accumulated_load_unload_cycles").toInt();
        firmwareVersion = localObj["scsi_revision"].toString("----");
        totalMbReadsI64 = scsiErrorCounterLog.value("read").toObject().value("gigabytes_processed").toString().split(",").first().toInt() * 1000;
        totalMbWritesI64 = scsiErrorCounterLog.value("write").toObject().value("gigabytes_processed").toString().split(",").first().toInt() * 1000;
    }

    bool nvmeHasSelfTest = false;

    auto createTablePopup = [=](QJsonArray selfTestsTable) {
        QWidget *popup = new QWidget();
        QTableWidget *selfTestsTableWidget = new QTableWidget();
        selfTestsTableWidget->setFocusPolicy(Qt::NoFocus);

        qsizetype rowCount = selfTestsTable.size();
        if (rowCount > std::numeric_limits<int>::max()) {
            rowCount = std::numeric_limits<int>::max();
        }

        selfTestsTableWidget->setRowCount(static_cast<int>(rowCount));
        selfTestsTableWidget->setColumnCount(3);
        selfTestsTableWidget->verticalHeader()->setVisible(false);
        selfTestsTableWidget->setHorizontalHeaderLabels({tr("Type"), tr("Status"), tr("Power On Hours")});

        for (int i = 0; i < rowCount; ++i) {
            QJsonObject entry = selfTestsTable[i].toObject();
            QTableWidgetItem *item;

            if (isNvme) {
                selfTestsTableWidget->setItem(i, 0, new QTableWidgetItem(entry["self_test_code"].toObject()["string"].toString()));
                selfTestsTableWidget->setItem(i, 1, new QTableWidgetItem(entry["self_test_result"].toObject()["string"].toString()));
                item = new QTableWidgetItem(QString::number(entry["power_on_hours"].toInt()));
            } else {
                selfTestsTableWidget->setItem(i, 0, new QTableWidgetItem(entry["type"].toObject()["string"].toString()));
                selfTestsTableWidget->setItem(i, 1, new QTableWidgetItem(entry["status"].toObject()["string"].toString()));
                item = new QTableWidgetItem(QString::number(entry["lifetime_hours"].toInt()));
            }
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            selfTestsTableWidget->setItem(i, 2, item);
        }

        for (int i = 0; i < selfTestsTableWidget->columnCount(); ++i) {
            QTableWidgetItem *headerItem = selfTestsTableWidget->horizontalHeaderItem(i);
            if (headerItem) {
                if (i == 2) {
                   headerItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                } else {
                    headerItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
                }
            }
        }


        selfTestsTableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        for (int i = 0; i < selfTestsTableWidget->columnCount(); ++i) {
            if (i != 1) {
                selfTestsTableWidget->horizontalHeader()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
            }
        }

        selfTestsTableWidget->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        selfTestsTableWidget->verticalHeader()->setDefaultSectionSize(31);
        selfTestsTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);

        QVBoxLayout *layout = new QVBoxLayout;
        layout->addWidget(selfTestsTableWidget);
        popup->setLayout(layout);
        popup->setWindowTitle(tr("Self Test Log"));
        popup->resize(400, 400);
        popup->show();
    };

    deviceJson = localObj;

    diskName->setText("<html><head/><body><p><span style='font-size:14pt; font-weight:bold;'>" + modelName + " " + diskCapacityString + "</span></p></body></html>");
    firmwareLineEdit->setText(firmwareVersion);
    serialNumberLineEdit->setText(serialNumber);
    typeLineEdit->setText(type);
    protocolLineEdit->setText(protocol);
    deviceNodeLineEdit->setText(name);

    summary.model = modelName;
    summary.capacity = diskCapacityString;
    summary.firmware = firmwareVersion;
    summary.serial = serialNumber;
    summary.type = type;
    summary.protocol = protocol;
    summary.node = name;
    summary.temperature = temperatureInt;

    QStringList keys = pollingMinutes.keys();
    std::sort(keys.begin(), keys.end(), [&pollingMinutes](const QString& key1, const QString& key2) {
        return pollingMinutes[key1].toInt() < pollingMinutes[key2].toInt();
    });

    int rotationRateInt = localObj["rotation_rate"].toInt(-1);
    QString rotationRate;
    if (rotationRateInt > 0) {
        rotationRate = QString::number(rotationRateInt);
    } else if (rotationRateInt == 0 || isNvme) {
        rotationRate = "---- (SSD)";
    } else {
        rotationRate = "----";
    }

    rotationRateLineEdit->setText(rotationRate);
    rotationRateLineEdit->setAlignment(Qt::AlignRight);
    summary.rotationRate = rotationRate;

    QString powerCycleCount;
    if (powerCycleCountInt >= 0) {
        powerCycleCount = QString::number(powerCycleCountInt) + " " + tr("count");
    } else {
        powerCycleCount = "Unknown";
    }

    powerOnCountLineEdit->setText(powerCycleCount);
    powerOnCountLineEdit->setAlignment(Qt::AlignRight);
    summary.powerOnCount = powerCycleCount;

    int powerOnTimeInt = localObj["power_on_time"].toObject().value("hours").toInt(-1);
    QString powerOnTime;
    if (powerOnTimeInt >= 0) {
        powerOnTime = QString::number(powerOnTimeInt) + " " + tr("hours");
    } else {
        powerOnTime = "Unknown";
    }

    powerOnHoursLineEdit->setText(powerOnTime);
    powerOnHoursLineEdit->setAlignment(Qt::AlignRight);
    summary.powerOnHours = powerOnTime;

    int logicalBlockSize = localObj["logical_block_size"].toInt();
    if (!isNvme) {
        for (const QJsonValue &attr : std::as_const(attributes)) {
            QJsonObject attrObj = attr.toObject();
            if (attrObj["id"] == 202) {
                if (attrObj["name"] == "Percent_Lifetime_Remain") {
                    int percentageUsed = 100 - attrObj["raw"].toObject()["value"].toInt();
                    percentage = QString::number(percentageUsed) + " %";
                }
            } else if (attrObj["id"] == 241) {
                if (attrObj["name"] == "Total_Writes_GB") {
                    int gigabytes = attrObj["raw"].toObject()["value"].toInt();
                    totalMbWritesI64 = gigabytes * 1000;
                } else if (attrObj["name"] == "Host_Writes_32MiB") {
                    double megabytes = (attrObj["raw"].toObject()["value"].toInt() * 32 * 1024.0 * 1024.0) / 1e6;
                    totalMbWritesI64 = static_cast<int64_t>(megabytes);
                } else if (attrObj["name"] == "Host_Writes_GiB" || attrObj["name"] == "Lifetime_Writes_GiB") {
                    double gibibytes = attrObj["raw"].toObject()["value"].toDouble();
                    double bytesPerGiB = static_cast<double>(1ULL << 30);
                    double bytesPerMB = 1e6;
                    double conversionFactor = bytesPerGiB / bytesPerMB;
                    double megabytes = gibibytes * conversionFactor;
                    totalMbWritesI64 = static_cast<int64_t>(megabytes);
                } else if (attrObj["name"] == "Total_LBAs_Written") {
                    qlonglong lbaWritten = attrObj["raw"].toObject()["value"].toVariant().toLongLong();
                    double megabytes = double(logicalBlockSize * lbaWritten) / 1e6;
                    totalMbWritesI64 = static_cast<int64_t>(megabytes);
                    // workaround: for drives that report different value with bad attribute name
                    // Check if total MB writes are less than power on hours
                    if (totalMbWritesI64 / powerOnTimeInt == 0) {
                        // Assume it's 32MiB chunks reported as LBAs
                        megabytes = (double(lbaWritten * 32) * 1024.0 * 1024.0) / 1e6;
                        totalMbWritesI64 = static_cast<int64_t>(megabytes);
                    }
                }
            } else if (attrObj["id"] == 242) {
                if (attrObj["name"] == "Total_Reads_GB") {
                    int gigabytes = attrObj["raw"].toObject()["value"].toInt();
                    totalMbReadsI64 = gigabytes * 1000;
                } else if (attrObj["name"] == "Host_Reads_32MiB") {
                    double megabytes = (attrObj["raw"].toObject()["value"].toInt() * 32 * 1024 * 1024) / 1e6;
                    totalMbReadsI64 = static_cast<int64_t>(megabytes);
                } else if (attrObj["name"] == "Host_Reads_GiB" || attrObj["name"] == "Lifetime_Reads_GiB") {
                    double gibibytes = attrObj["raw"].toObject()["value"].toDouble();
                    double bytesPerGiB = static_cast<double>(1ULL << 30);
                    double bytesPerMB = 1e6;
                    double conversionFactor = bytesPerGiB / bytesPerMB;
                    double megabytes = gibibytes * conversionFactor;
                    totalMbReadsI64 = static_cast<int64_t>(megabytes);
                } else if (attrObj["name"] == "Total_LBAs_Read") {
                    qlonglong lbaRead = attrObj["raw"].toObject()["value"].toVariant().toLongLong();
                    double megabytes = double(logicalBlockSize * lbaRead) / 1e6;
                    totalMbReadsI64 = static_cast<int64_t>(megabytes);
                    // workaround for drives that report different value with bad attribute name
                    // Check if total MB reads are less than power on hours
                    if (totalMbReadsI64 / powerOnTimeInt == 0) {
                        // Assume it's 32MiB chunks reported as LBAs
                        megabytes = (double(lbaRead * 32) * 1024.0 * 1024.0) / 1e6;
                        totalMbReadsI64 = static_cast<int64_t>(megabytes);
                    }
                }
            } else if (attrObj["id"] == 246) { // MX500
                if (attrObj["name"] == "Total_LBAs_Written") {
                    qlonglong lbaWritten = attrObj["raw"].toObject()["value"].toVariant().toLongLong();
                    double megabytes = double(lbaWritten * logicalBlockSize) / 1e6;
                    totalMbWritesI64 = static_cast<int64_t>(megabytes);
                }
            } else if (attrObj["name"] == "Remaining_Lifetime_Perc") {
                int percentageUsed = attrObj["raw"].toObject()["value"].toInt();
                percentage = QString::number(percentageUsed) + " %";
            } else if (attrObj["name"] == "Percent_Lifetime_Remain") {
                int percentageUsed = attrObj["value"].toInt();
                percentage = QString::number(percentageUsed) + " %";
            } else if (attrObj["name"] == "Media_Wearout_Indicator" || attrObj["name"] == "SSD_Life_Left") {
                int percentageUsed = attrObj["value"].toInt();
                int percentageUsedWorst = attrObj["worst"].toInt();
                if (percentageUsedWorst <= 100) {
                    percentage = QString::number(percentageUsed) + " %";
                }
            }
        }

		// Some devices don't report or report incorrect reads/writes in legacy
		// attributes, so we look for that data in ata_device_statistics.
		// We do this after parsing through the legacy attributes
		// to override possibly incorrect calculations.
		QJsonObject ata_device_statistics = localObj["ata_device_statistics"].toObject();
		if (!ata_device_statistics.empty())
		{
			QJsonArray pages = ata_device_statistics["pages"].toArray();
			for (const QJsonValue &pageval : std::as_const(pages))
			{
				QJsonObject page = pageval.toObject();
				if (page["name"] == "General Statistics")
				{
					QJsonArray general_statistics = page["table"].toArray();
					for (const QJsonValue &stat : std::as_const(general_statistics))
					{
						QJsonObject statObj = stat.toObject();
						QString key = statObj["name"].toString();
						QJsonValue value = statObj["value"];
						
						if (key == "Logical Sectors Read") {
							double logicalSectorsRead = value.toDouble();
							double megabytes = logicalSectorsRead * logicalBlockSize / 1e6;
							totalMbReadsI64 = static_cast<int64_t>(megabytes);
						} else if (key == "Logical Sectors Written") {
							double logicalSectorsWritten = value.toDouble();
							double megabytes = logicalSectorsWritten * logicalBlockSize / 1e6;
							totalMbWritesI64 = static_cast<int64_t>(megabytes);
						}
					}
					break; // no need looking into other pages
				}
			}
		}
		
        if (percentage.isEmpty() && rotationRate == "---- (SSD)") { // Workaround for some drives which have this and another attribute
            for (const QJsonValue &attr : std::as_const(attributes)) {
                QJsonObject attrObj = attr.toObject();
                if (attrObj["name"] == "Wear_Leveling_Count") {
                    int percentageUsed = attrObj["value"].toInt();
                    percentage = QString::number(percentageUsed) + " %";
                    break;
                }
            }
        }
    } else {
        for (auto smartItem = nvmeLog.begin(); smartItem != nvmeLog.end(); ++smartItem) {
            QString key = smartItem.key();
            QJsonValue value = smartItem.value();
            // it appears that NVME drives report data_units as logical_block_size*1000 bytes
            if (key == "data_units_written") {
                double dataUnitsWritten = value.toDouble();
                double megabytes = (dataUnitsWritten * logicalBlockSize) / 1000;
                totalMbWritesI64 = static_cast<int64_t>(megabytes);
            } else if (key == "data_units_read") {
                double dataUnitsRead = value.toDouble();
                double megabytes = (dataUnitsRead * logicalBlockSize) / 1000;
                totalMbReadsI64 = static_cast<int64_t>(megabytes);
            } else if (key == "percentage_used") {
                int percentageUsed = 100 - value.toInt();
                percentage = QString::number(percentageUsed) + " %";
            }
        }
    }

    totalReads = getMbToPrettyString(totalMbReadsI64, PRECISION_TOTALS_TO_STR, useGB);
    totalWrites = getMbToPrettyString(totalMbWritesI64, PRECISION_TOTALS_TO_STR, useGB);

    summary.totalReads = totalReads;
    summary.totalWrites = totalWrites;
    QString percentDigits = percentage;
    percentDigits.remove(QLatin1Char('%'));
    percentDigits.remove(QLatin1Char(' '));
    bool percentOk = false;
    const int lifePercent = percentDigits.toInt(&percentOk);
    summary.lifePercent = percentOk ? lifePercent : -1;

    totalReadsLineEdit->setText(totalReads);
    totalReadsLineEdit->setAlignment(Qt::AlignRight);

    totalWritesLineEdit->setText(totalWrites);
    totalWritesLineEdit->setAlignment(Qt::AlignRight);

    int warningTemperature = 55;
    int criticalTemperature = 60;

    if (isNvme) {
        QString stringValue;
        warningTemperature = 65;
        criticalTemperature = 70;
        for (const QJsonValue &value : std::as_const(outputArray)) {
            stringValue = value.toString();
            if (stringValue.startsWith("Optional Admin Commands")) {
                if (stringValue.contains("Self_Test")) {
                    nvmeHasSelfTest = true;
                }
            } else if (stringValue.startsWith("Warning  Comp. Temp. Threshold")) {
                qsizetype pos = stringValue.indexOf(':');
                if (pos != -1) {
                    QString thresholdStr = stringValue.mid(static_cast<int>(pos + 1)).trimmed();
                    int temperature = thresholdStr.section(' ', 0, 0).toInt();
                    if (temperature > 0) {
                        warningTemperature = temperature;
                    }
                }
            } else if (stringValue.startsWith("Critical Comp. Temp. Threshold")) {
                qsizetype pos = stringValue.indexOf(':');
                if (pos != -1) {
                    QString thresholdStr = stringValue.mid(static_cast<int>(pos + 1)).trimmed();
                    int temperature = thresholdStr.section(' ', 0, 0).toInt();
                    if (temperature > 0) {
                        criticalTemperature = temperature;
                    }
                }
            }
        }
    } else if (isScsi) {
        int driveTripInt = temperatureObj["drive_trip"].toInt();
        if (driveTripInt > 0) {
            criticalTemperature = driveTripInt;
        }
    }

    QLabel *temperatureValueLabel, *healthStatusValueLabel;
    if (INCLUDE_OPTIONAL_RESOURCES) {
        temperatureValueLabel = temperatureValueHorizontal;
        healthStatusValueLabel = healthStatusValueHorizontal;
    } else {
        temperatureValueLabel = temperatureValue;
        healthStatusValueLabel = healthStatusValue;
    }

    QColor temperatureStateColor;
    QString temperatureState;
    if (temperatureInt > warningTemperature) { // TODO: Let the user set an alarm temp.
        temperatureStateColor = badColor;
        temperatureState = QStringLiteral("bad");
    } else if ((temperatureInt < criticalTemperature) && (temperatureInt > warningTemperature)){
        temperatureStateColor = cautionColor;
        temperatureState = QStringLiteral("warn");
    } else if (temperatureInt == 0) {
        temperatureStateColor = naColor;
        temperatureState = QStringLiteral("na");
    } else {
        temperatureStateColor = goodColor;
        temperatureState = QStringLiteral("ok");
    }
    summary.temperatureState = temperatureState;
    temperatureValueLabel->setStyleSheet("background-color: " + temperatureStateColor.name() + ";");

    QString labelStyle = "font-size:12pt; font-weight:700; color:black";

    if (temperatureInt > 0) {
        if (ui->actionUse_Fahrenheit->isChecked()) {
            int fahrenheit = static_cast<int>((temperatureInt * 9.0 / 5.0) + 32.0);
            temperatureValueLabel->setText("<html><head/><body><p><span style='" + labelStyle +"'>" + QString::number(fahrenheit) + " °F</span></p></body></html>");
        } else {
            temperatureValueLabel->setText("<html><head/><body><p><span style='" + labelStyle +"'>" + QString::number(temperatureInt) + " °C</span></p></body></html>");
        }
    } else {
        temperatureValueLabel->setText("<html><head/><body><p><span style='" + labelStyle +"'>" + "-- °C" + "</span></p></body></html>");
    }

    temperatureValueLabel->setAlignment(Qt::AlignCenter);

    summary.health = health;
    if (health == tr("Bad")) {
        summary.healthState = QStringLiteral("bad");
        healthStatusValueLabel->setStyleSheet("background-color: " + badColor.name() + ";");
    } else if (health == tr("Caution")) {
        summary.healthState = QStringLiteral("warn");
        healthStatusValueLabel->setStyleSheet("background-color: " + cautionColor.name() + ";");
    } else if (health == tr("Good")) {
        summary.healthState = QStringLiteral("ok");
        healthStatusValueLabel->setStyleSheet("background-color: " + goodColor.name() + ";");
    } else {
        summary.healthState = QStringLiteral("na");
        healthStatusValueLabel->setStyleSheet("background-color: " + naColor.name() + ";");
    }

    if (INCLUDE_OPTIONAL_RESOURCES) {
        QPixmap statusAnimeBackground;
        QSize labelSize(100, 30);
        if (health == tr("Bad")) {
            statusAnimeBackground = QPixmap(":/icons/bad.png");
        } else if (health == tr("Caution")){
            statusAnimeBackground = QPixmap(":/icons/caution.png");
        } else if (health == tr("Good")) {
            statusAnimeBackground = QPixmap(":/icons/good.png");
        } else {
            statusAnimeBackground = QPixmap(":/icons/unknown.png");
        }
        QPixmap statusAnimeBackgroundScaled = statusAnimeBackground.scaled(labelSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        statusLabel->setPixmap(statusAnimeBackgroundScaled);
        statusLabel->setToolTip(health);
    }

    QString percentageText = "";
    if (!percentage.isEmpty()) {
        if (INCLUDE_OPTIONAL_RESOURCES) {
            percentageText = "  (" + percentage + ")"; // 2 spaces is not a mistake
        } else {
            percentageText = "<br>" + percentage;
        }
    }

    healthStatusValueLabel->setText("<html><head/><body><p><span style='" + labelStyle +"'>" + health + percentageText + "</span></p></body></html>");
    healthStatusValueLabel->setAlignment(Qt::AlignCenter);

    if (protocol == "ATA") {
        addSmartAttributesTable(attributes);
        selfTestMenu->clear();
        ui->actionASCII_View->setEnabled(true);

        if (keys.isEmpty()) {
            selfTestMenu->setDisabled(true);
        } else {
            selfTestMenu->setEnabled(true);
        }

        for (const QString& key : std::as_const(keys)) {
            QString minutes = QString::number(pollingMinutes[key].toInt());
            QString keyTranslated;
            if (key == "short") {
                keyTranslated = tr("Short");
            } else if (key == "conveyance") {
                keyTranslated = tr("Conveyance");
            } else if (key == "extended") {
                keyTranslated = tr("Extended");
            } else {
                keyTranslated = key;
                keyTranslated[0] = keyTranslated[0].toUpper();
            }
            QString actionLabel = keyTranslated + " (" + minutes + " " + tr("Min.)");
            QAction *action = new QAction(actionLabel, this);
            selfTestMenu->addAction(action);

            QString mode;
            if (key == "extended") {
                mode = "long";
            } else {
                mode = key;
            }

            connect(action, &QAction::triggered, this, [this, mode, name, minutes, type]() {
                Utils.selfTestHandler(mode, name, minutes, type);
            });
        }

        if (ataSelfTestsTable.isEmpty()) {
            selfTestLogAction->setDisabled(true);
        } else {
            selfTestLogAction->setEnabled(true);
            selfTestLogAction->disconnect();
            connect(selfTestLogAction, &QAction::triggered, this, [=]() {
                createTablePopup(ataSelfTestsTable);
            });
        }
    } else if (protocol == "SCSI") {
            selfTestMenu->clear();
            ui->actionASCII_View->setEnabled(true);
            selfTestMenu->setDisabled(true);
            selfTestLogAction->setDisabled(true);
            addSCSIErrorCounterLogTable(scsiErrorCounterLog);
    } else {
        addNvmeLogTable(nvmeLogOrdered);
        selfTestMenu->clear();
        ui->actionASCII_View->setDisabled(true); // TODO: Implement this for NVMe drives too

        if (nvmeHasSelfTest) {
            selfTestMenu->setEnabled(true);
        } else {
            selfTestMenu->setDisabled(true);
        }

        QAction *actionShort = new QAction(tr("Short"), this);
        selfTestMenu->addAction(actionShort);
        connect(actionShort, &QAction::triggered, this, [this, mode = "short", name, minutes = "0", type]() {
            Utils.selfTestHandler(mode, name, minutes, type);
        });

        QAction *actionLong = new QAction(tr("Extended"), this);
        selfTestMenu->addAction(actionLong);
        connect(actionLong , &QAction::triggered, this, [this, mode = "long", name, minutes = "0", type]() {
            Utils.selfTestHandler(mode, name, minutes, type);
        });

        if (nvmeSelfTestsTable.isEmpty()) {
            selfTestLogAction->setDisabled(true);
        } else {
            selfTestLogAction->setEnabled(true);
            selfTestLogAction->disconnect();
            connect(selfTestLogAction, &QAction::triggered, this, [=]() {
                createTablePopup(nvmeSelfTestsTable);
            });
        }
    }

    if (!localObj.contains("smart_status") && !localObj.contains("ata_smart_attributes")
            && !localObj.contains("nvme_smart_health_information_log") && !localObj.contains("scsi_error_counter_log")) {
        selfTestMenu->setEnabled(false);
        selfTestLogAction->setEnabled(false);
        ui->actionASCII_View->setEnabled(false);
    }

    if (uiMode == UiMode::Magician) {
        magicianView->applySummary(summary);
    } else if (uiMode == UiMode::Crystal) {
        crystalView->applySummary(summary);
    }
}

void MainWindow::addSCSIErrorCounterLogTable(const QJsonObject &scsiErrorLog)
{

    QStringList operations = {"read", "write", "verify"};

    tableWidget->setRowCount(3);
    tableWidget->setColumnCount(9);
    tableWidget->verticalHeader()->setVisible(false);
    tableWidget->setItemDelegateForColumn(0, new StatusDot(tableWidget));

    tableWidget->setHorizontalHeaderLabels({
        "",
        "Operation",
        "Errors Corrected\nby ECC Fast",
        "Errors Corrected\nby ECC Delayed",
        "Errors Corrected\nby Rereads/Rewrites",
        "Total Errors\nCorrected",
        "Correction Algorithm\nInvocations",
        "Gigabytes\nProcessed",
        "Total Uncorrected\nErrors"
    });

    summary.table.headers = {QString(), QStringLiteral("Operation"),
                              tr("Errors Corrected\nby ECC Fast"), tr("Errors Corrected\nby ECC Delayed"),
                              tr("Errors Corrected\nby Rereads/Rewrites"), tr("Total Errors\nCorrected"),
                              tr("Correction Algorithm\nInvocations"), tr("Gigabytes\nProcessed"),
                              tr("Total Uncorrected\nErrors")};
    summary.table.stretchColumn = 1;
    summary.table.leftColumns = QList<int>();
    summary.table.rightColumns = {2, 3, 4, 5, 6, 7, 8};

    int row = 0;
    for (const QString& operation : operations) {
        QColor statusColor;
        QJsonObject operationData = scsiErrorLog[operation].toObject();

        QString rowState;
        if (operationData["total_uncorrected_errors"].toInt() == 0) {
            statusColor = goodColor;
            rowState = QStringLiteral("ok");
        } else {
            statusColor = cautionColor;
            rowState = QStringLiteral("warn");
        }

        QTableWidgetItem *statusItem = new QTableWidgetItem();
        statusItem->setData(Qt::BackgroundRole, QVariant(statusColor));

        tableWidget->setItem(row, 0, statusItem);

        QString operationTranslated;
        if (operation == "read") {
            operationTranslated = tr("Read");
        } else if (operation == "write") {
            operationTranslated = tr("Write");
        } else if (operation == "verify") {
            operationTranslated = tr("Verify");
        }

        tableWidget->setItem(row, 1, new QTableWidgetItem(operationTranslated));
        tableWidget->setItem(row, 2, new QTableWidgetItem(QString::number(operationData["errors_corrected_by_eccfast"].toInt())));
        tableWidget->setItem(row, 3, new QTableWidgetItem(QString::number(operationData["errors_corrected_by_eccdelayed"].toInt())));
        tableWidget->setItem(row, 4, new QTableWidgetItem(QString::number(operationData["errors_corrected_by_rereads_rewrites"].toInt())));
        tableWidget->setItem(row, 5, new QTableWidgetItem(QString::number(operationData["total_errors_corrected"].toInt())));
        tableWidget->setItem(row, 6, new QTableWidgetItem(QString::number(operationData["correction_algorithm_invocations"].toInt())));
        tableWidget->setItem(row, 7, new QTableWidgetItem(operationData["gigabytes_processed"].toString()));
        tableWidget->setItem(row, 8, new QTableWidgetItem(QString::number(operationData["total_uncorrected_errors"].toInt())));

        summary.table.rows.append({statusColor, rowState, {operationTranslated,
                                                 QString::number(operationData["errors_corrected_by_eccfast"].toInt()),
                                                 QString::number(operationData["errors_corrected_by_eccdelayed"].toInt()),
                                                 QString::number(operationData["errors_corrected_by_rereads_rewrites"].toInt()),
                                                 QString::number(operationData["total_errors_corrected"].toInt()),
                                                 QString::number(operationData["correction_algorithm_invocations"].toInt()),
                                                 operationData["gigabytes_processed"].toString(),
                                                 QString::number(operationData["total_uncorrected_errors"].toInt())}});

        ++row;

        for (int rowNum = 0; rowNum < 3; ++rowNum) {
            for (int column = 0; column < 9; ++column) {
                QTableWidgetItem *item = tableWidget->item(rowNum, column);
                if (item) {
                    if (column == 1) {
                        item->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);
                    } else {
                        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    }
                }
            }
        }
    }

    tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    for (int i = 0; i < tableWidget->columnCount(); ++i) {
        if (i != 1) {
            tableWidget->horizontalHeader()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
        }
    }

    tableWidget->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    tableWidget->verticalHeader()->setDefaultSectionSize(31);
    tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
}

void MainWindow::addNvmeLogTable(const QVector<QPair<QString, int>>& nvmeLogOrdered)
{
    QString warningMessage = "";

    qsizetype rowCount = nvmeLogOrdered.size();
    if (rowCount > std::numeric_limits<int>::max()) {
        rowCount = std::numeric_limits<int>::max();
    }

    tableWidget->setRowCount(static_cast<int>(rowCount));
    tableWidget->setColumnCount(4);
    tableWidget->setHorizontalHeaderLabels({"", tr("ID"), tr("Attribute Name"), tr("Raw Values")});
    tableWidget->verticalHeader()->setVisible(false);
    tableWidget->setItemDelegateForColumn(0, new StatusDot(tableWidget));

    summary.table.headers = {QString(), tr("ID"), tr("Attribute Name"), tr("Raw Values")};
    summary.table.stretchColumn = 2;
    summary.table.leftColumns = {2};
    summary.table.rightColumns = {3};
    summary.table.rows.clear();


    tableWidget->horizontalHeaderItem(2)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    tableWidget->horizontalHeaderItem(3)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

    int row = 0;
    for (const QPair<QString, int> &pair : std::as_const(nvmeLogOrdered)) {
        QString id = QString("%1").arg(row + 1, 2, 16, QChar('0')).toUpper();

        QString key = pair.first;
        QString name = key.replace("_", " ");
        if (name == "nsid") { // smartctl now adds an nsid field, skip it for now
            continue;
        }
        name = Utils.toTitleCase(name);

        int rawInt = pair.second;
        QString raw = QString::number(rawInt);
        if ((ui->actionHEX->isChecked())) {
            raw = QString("%1").arg(raw.toUInt(nullptr), 14, 16, QChar('0')).toUpper();
        }

        QColor statusColor;
        QString rowState;

        if (id == "01" && rawInt) {
            statusColor = badColor;
            rowState = QStringLiteral("bad");
            if (rawInt == 1) { // Still need to figure out if this is DEC or HEX in JSON
                warningMessage = tr("Available spare capacity has fallen below the threshold"); // THX to CrystalDiskInfo for the messages
            } else if (rawInt == 2) {
                warningMessage = tr("Temperature error (Overheat or Overcool)");
            } else if (rawInt == 4) {
                warningMessage = tr("NVM subsystem reliability has been degraded");
            } else if (rawInt == 8) {
                warningMessage = tr("Media has been placed in Read Only Mode");
            } else if (rawInt == 10) {
                warningMessage = tr("Volatile memory backup device has Failed");
            } else if (rawInt == 20) {
                warningMessage = tr("Persistent memory region has become Read-Only");
            }
        } else if (id == "03") {
            int availableSpareThreshold = nvmeLogOrdered.at(3).second;
            if (availableSpareThreshold > 100) { // Thx to crystaldiskinfo for these workarounds
                statusColor = goodColor;
                rowState = QStringLiteral("ok");
            } else if (rawInt == 0 && availableSpareThreshold == 0) {
                statusColor = goodColor;
                rowState = QStringLiteral("ok");
            } else if (rawInt < availableSpareThreshold) {
                statusColor = badColor;
                rowState = QStringLiteral("bad");
            } else if (availableSpareThreshold != 100 && (rawInt == availableSpareThreshold)) {
                statusColor = cautionColor;
                rowState = QStringLiteral("warn");
            } else {
                statusColor = goodColor;
                rowState = QStringLiteral("ok");
            }
        } else if (id == "05" && rawInt >= 90) { // Make this configurable, currently hardcoded to 10%
            statusColor = cautionColor;
            rowState = QStringLiteral("warn");
        } else {
            statusColor = goodColor;
            rowState = QStringLiteral("ok");
        }

        QTableWidgetItem *statusItem = new QTableWidgetItem();
        statusItem->setData(Qt::BackgroundRole, QVariant(statusColor));

        QTableWidgetItem *idItem = new QTableWidgetItem(id);
        idItem->setTextAlignment(Qt::AlignCenter);

        QTableWidgetItem *nameItem = new QTableWidgetItem(name);

        QTableWidgetItem *rawItem = new QTableWidgetItem(raw);
        rawItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        tableWidget->setItem(row, 0, statusItem);
        tableWidget->setItem(row, 1, idItem);
        tableWidget->setItem(row, 2, nameItem);
        tableWidget->setItem(row, 3, rawItem);

        summary.table.rows.append({statusColor, rowState, {id, name, raw}});

        ++row;
    }

    tableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    for (int i = 0; i < tableWidget->columnCount(); ++i) {
        if (i != 2) {
            tableWidget->horizontalHeader()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
        }
    }

    tableWidget->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    tableWidget->verticalHeader()->setDefaultSectionSize(31);
    tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);

    if (!warningMessage.isEmpty()) {
        QMessageBox::warning(nullptr, tr("Critical Warning"), warningMessage);
    }
}

void MainWindow::addSmartAttributesTable(const QJsonArray &attributes)
{
    qsizetype rowCount = attributes.size();
    if (rowCount > std::numeric_limits<int>::max()) {
        rowCount = std::numeric_limits<int>::max();
    }

    tableWidget->setRowCount(static_cast<int>(rowCount));
    tableWidget->setColumnCount(7);
    tableWidget->setHorizontalHeaderLabels({"", tr("ID"), tr("Attribute Name"), tr("Current"), tr("Worst"), tr("Threshold"), tr("Raw Values")});
    tableWidget->verticalHeader()->setVisible(false);
    tableWidget->setItemDelegateForColumn(0, new StatusDot(tableWidget));

    summary.table.headers = {QString(), tr("ID"), tr("Attribute Name"), tr("Current"), tr("Worst"), tr("Threshold"), tr("Raw Values")};
    summary.table.stretchColumn = 2;
    summary.table.leftColumns = {2};
    summary.table.rightColumns = {3, 4, 5, 6};
    summary.table.rows.clear();

    for (int i = 0; i < tableWidget->columnCount(); ++i) {
        QTableWidgetItem *headerItem = tableWidget->horizontalHeaderItem(i);
        if (headerItem) {
            if (i == 2) {
                headerItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            } else if (i > 2) {
                headerItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            }
        }
    }

    int row = 0;
    for (const QJsonValue &attr : attributes) {
        QJsonObject attrObj = attr.toObject();
        QString id = QString("%1").arg(attrObj["id"].toInt(), 2, 16, QChar('0')).toUpper();
        QString name = attrObj["name"].toString().replace("_", " ");
        int value = attrObj["value"].toInt();
        int worst = attrObj["worst"].toInt();
        int thresh = attrObj["thresh"].toInt();
        QString rawString = attrObj["raw"].toObject()["string"].toString();
        QString rawHEX = rawString;

        qsizetype spaceIndex = rawHEX.indexOf(' ');
        if (spaceIndex != -1) {
            rawHEX = rawHEX.left(static_cast<int>(spaceIndex));
        }

        if (rawHEX.startsWith("0x") && rawHEX.length() == 14) {
            rawHEX = rawHEX.mid(2).toUpper();
        } else {
            rawHEX = QString("%1").arg(rawHEX.toUInt(nullptr), 12, 16, QChar('0')).toUpper();
        }

        QColor statusColor;
        QString rowState;
        if (thresh && (value < thresh)) {
            statusColor = badColor;
            rowState = QStringLiteral("bad");
        } else if ((id == "05" || id == "C5" || id == "C6" || (id == "C4" && !(ui->actionIgnore_C4_Reallocation_Event_Count->isChecked()))) && (rawHEX != "000000000000")) {
            statusColor = cautionColor;
            rowState = QStringLiteral("warn");
        } else {
            statusColor = goodColor;
            rowState = QStringLiteral("ok");
        }

        QTableWidgetItem *statusItem = new QTableWidgetItem();
        statusItem->setData(Qt::BackgroundRole, QVariant(statusColor));

        QTableWidgetItem *idItem = new QTableWidgetItem(id);
        idItem->setTextAlignment(Qt::AlignCenter);

        QTableWidgetItem *nameItem = new QTableWidgetItem(name);

        QTableWidgetItem *valueItem = new QTableWidgetItem(QString::number(value));
        valueItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        QTableWidgetItem *worstItem = new QTableWidgetItem(QString::number(worst));
        worstItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        QTableWidgetItem *threshItem = new QTableWidgetItem(QString::number(thresh));
        threshItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        QTableWidgetItem *rawItem;
        if (ui->actionHEX->isChecked()) {
            rawItem = new QTableWidgetItem(rawHEX);
        } else {
            rawItem = new QTableWidgetItem(rawString);
        }
        rawItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        tableWidget->setItem(row, 0, statusItem);
        tableWidget->setItem(row, 1, idItem);
        tableWidget->setItem(row, 2, nameItem);
        tableWidget->setItem(row, 3, valueItem);
        tableWidget->setItem(row, 4, worstItem);
        tableWidget->setItem(row, 5, threshItem);
        tableWidget->setItem(row, 6, rawItem);

        summary.table.rows.append({statusColor, rowState, {id, name, QString::number(value), QString::number(worst),
                                                 QString::number(thresh),
                                                 ui->actionHEX->isChecked() ? rawHEX : rawString}});

        ++row;
    }

    tableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    for (int i = 0; i < tableWidget->columnCount(); ++i) {
        if (i != 2) {
            tableWidget->horizontalHeader()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
        }
    }

    tableWidget->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    tableWidget->verticalHeader()->setDefaultSectionSize(31);
    tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
}

void MainWindow::transformWindow() {
    setMaximumSize(960, 960);
    resize(960, 600);

    QLayoutItem *item;
    while ((item = verticalLayout->takeAt(0)) != nullptr) {
        QWidget *widget = item->widget();
        if (widget) {
            delete widget;
        }
        delete item;
    }

    verticalLayout->addWidget(statusLabel);

    if (CHARACTER_IS_RIGHT) {
        QSpacerItem *spacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
        gridLayout->addItem(spacer,3,3);
        setMinimumWidth(645);
    } else {
        setMinimumWidth(960);
    }

    QHBoxLayout *infoLayout = ui->centralwidget->findChild<QHBoxLayout*>("info");
    QSpacerItem *spacerItem = infoLayout->itemAt(1)->spacerItem();
    if (spacerItem) {
            infoLayout->removeItem(spacerItem);
            delete spacerItem;
    }

    QPalette palette = QApplication::palette();
    QColor backgroundColor = palette.color(QPalette::Window);
    int lightness = backgroundColor.lightness();
    bool darkTheme = lightness < 128;

    QPixmap animeBackground;
    if (darkTheme) {
        animeBackground = QPixmap(":/icons/bg_dark.png");
    } else {
        animeBackground = QPixmap(":/icons/bg_light.png");
    }

    animeBackground = animeBackground.scaled(this->size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    palette.setBrush(QPalette::Window, QBrush(animeBackground));
    ui->centralwidget->setPalette(palette);
    ui->centralwidget->setAutoFillBackground(true);
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::ForwardButton && nextButton->isEnabled()) {
        onNextButtonClicked();
    } else if (event->button() == Qt::BackButton && prevButton->isEnabled()) {
        onPrevButtonClicked();
    }
}

// Slots
void MainWindow::on_actionQuit_triggered()
{
    qApp->quit();
}

void MainWindow::on_actionSave_JSON_triggered()
{
    if (deviceJson.isEmpty()) {
        QMessageBox::information(this, tr("Empty JSON"),
                                 tr("The JSON is empty"));
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(this,
                                                    tr("Save JSON"), "QDiskInfo_" + deviceNodeLineEdit->text().section('/', -1) + ".json",
                                                    tr("JSON (*.json);;All Files (*)"));
    if (fileName.isEmpty())
        return;
    else {
        QFile file(fileName);
        if (!file.open(QIODevice::WriteOnly)) {
            QMessageBox::critical(this, tr("Unable to open file for writing"),
                                     file.errorString());
            return;
        }

        QJsonDocument doc(deviceJson);
        QByteArray jsonData = doc.toJson();

        file.write(jsonData);
        file.close();
    }
}

void MainWindow::on_actionGitHub_triggered()
{
    QDesktopServices::openUrl(QUrl("https://github.com/edisionnano/QDiskInfo"));
}

void MainWindow::on_actionAbout_QDiskInfo_triggered()
{
    QString message = tr("An ATA and NVMe S.M.A.R.T. data viewer for Linux") + "\n";
    message += tr("Licensed under the GNU G.P.L. Version 3") + "\n";
    message += tr("Made by Samantas5855") + "\n";
    message += tr("Version") + " " + QString::number(PROJECT_VERSION_MAJOR) + "." + QString::number(PROJECT_VERSION_MINOR);

    QMessageBox::about(this, tr("About QDiskInfo"), message);
}

void MainWindow::on_actionAbout_Qt_triggered()
{
    QMessageBox::aboutQt(this, tr("About Qt"));
}


void MainWindow::on_actionRescan_Refresh_triggered()
{
    if (!ui->actionRescan_Refresh->isEnabled()) {
        return;
    }
    ui->actionRescan_Refresh->setEnabled(false);
    scanCompleted = 0;
    scanFailed = 0;
    statusBar()->showMessage(tr("Scanning disks…"));
    Utils.scanDevices();
}

void MainWindow::on_actionIgnore_C4_Reallocation_Event_Count_toggled(bool enabled)
{
    settings.setValue("IgnoreC4", enabled);
    if (!initializing) {
        updateUI();
    }
}

void MainWindow::on_actionHEX_toggled(bool enabled)
{
    settings.setValue("HEX", enabled);
    if (!initializing) {
        updateUI();
    }
}

void MainWindow::on_actionUse_Fahrenheit_toggled(bool enabled)
{
    settings.setValue("Fahrenheit", enabled);
    if (!initializing) {
        updateUI();
    }
}

void MainWindow::on_actionCyclic_Navigation_toggled(bool cyclicNavigation)
{
    settings.setValue("CyclicNavigation", cyclicNavigation);
    qsizetype currentIndex = buttonGroup->buttons().indexOf(buttonGroup->checkedButton());
    updateNavigationButtons(currentIndex);
}

void MainWindow::on_actionUse_GB_instead_of_TB_toggled(bool gigabytes)
{
    settings.setValue("UseGB", gigabytes);
    if (!initializing) {
        updateUI();
    }
}

void MainWindow::on_actionClear_Settings_triggered()
{
    QMessageBox msgBox;
    msgBox.setWindowTitle(QObject::tr("Clear Settings"));
    msgBox.setText(QObject::tr("Are you sure you want to clear the settings saved on disk?"));
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.addButton(QMessageBox::Cancel);
    QPushButton *clearSettingsButton = msgBox.addButton(QMessageBox::Ok);

    msgBox.exec();

    if (msgBox.clickedButton() == clearSettingsButton) {
        settings.clear();

        settings.setValue("IgnoreC4", true);
        settings.setValue("HEX", true);
        settings.setValue("Fahrenheit", false);
        settings.setValue("CyclicNavigation", false);
        settings.setValue("UseGB", false);

        ui->actionIgnore_C4_Reallocation_Event_Count->setChecked(true);
        ui->actionHEX->setChecked(true);
        ui->actionUse_Fahrenheit->setChecked(false);
        ui->actionCyclic_Navigation->setChecked(false);
        ui->actionUse_GB_instead_of_TB->setChecked(false);

        if (!initializing) {
            updateUI();
        }
    }
}

void MainWindow::on_actionASCII_View_triggered()
{
    QString deviceNodePath = deviceNodeLineEdit->text();

    QProcess *process = new QProcess(this);
    process->start("pkexec", {QCoreApplication::applicationFilePath(), "--ascii-view", deviceNodePath});

    if (!process->waitForFinished()) {
        return;
    }

    QByteArray binaryData = process->readAllStandardOutput();
    QByteArray errorData = process->readAllStandardError();

    if (!errorData.isEmpty()) {
        QMessageBox::critical(this, tr("QDiskInfo Error"), tr("QDiskInfo needs root access in order to read S.M.A.R.T. data!"));
        return;
    }

    AsciiView asciiview;
    QString hexDumpOutput = asciiview.hexDump(QVector<unsigned char>(binaryData.begin(), binaryData.end()));

    QDialog *asciiViewDialog = new QDialog(this);
    asciiViewDialog->setWindowTitle(tr("ASCII View"));

    QTextEdit *textView = new QTextEdit(asciiViewDialog);
    textView->setText(hexDumpOutput);
    textView->setReadOnly(true);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Close, asciiViewDialog);
    connect(buttonBox, &QDialogButtonBox::rejected, asciiViewDialog, &QDialog::close);
    connect(buttonBox->button(QDialogButtonBox::Save), &QPushButton::clicked, this, [binaryData, this, deviceNodePath]() {
        QString filePath = QFileDialog::getSaveFileName(this, tr("Save Binary Data"),  deviceNodePath.section('/', -1) + ".bin", tr("Binary Files (*.bin);;All Files (*)"));
        if (!filePath.isEmpty()) {
            QFile file(filePath);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(binaryData);
                file.close();
                QMessageBox::information(this, tr("Success"), tr("Binary data saved successfully."));
            } else {
                QMessageBox::critical(this, tr("Unable to open file for writing"), file.errorString());
            }
        }
    });

    QVBoxLayout *layout = new QVBoxLayout(asciiViewDialog);
    layout->addWidget(textView);
    layout->addWidget(buttonBox);

    asciiViewDialog->setLayout(layout);
    asciiViewDialog->resize(550, 500);
    asciiViewDialog->exec();
}

void MainWindow::on_actionGrid_View_triggered()
{
    gridView->show();
}


void MainWindow::on_actionSave_Image_triggered()
{
    QPixmap screenshot = this->grab();
    screenshot.save(QFileDialog::getSaveFileName(this, tr("Save Image"), "QDiskInfo_" + deviceNodeLineEdit->text().section('/', -1) + ".png", tr("PNG Files (*.png)")));
}

QString MainWindow::getMbToPrettyString(const int64_t &sizeInMbI64, const int &precisionInt, const bool &useGbBool) const
{
    if (!sizeInMbI64)
        return "----";

    int precision = precisionInt ? (useGbBool ? 1 : precisionInt) : 0;
    double sizeInMB = double(sizeInMbI64);
    if (sizeInMbI64 < 1000)
        return QString::number(sizeInMbI64) + " " + mbSymbol;
    else if (sizeInMbI64 < 1000000 || useGbBool)
        return QString::number(sizeInMB/1e3, 'f', precision) + " " + gbSymbol;
    else if (sizeInMbI64 < 1000000000)
        return QString::number(sizeInMB/1e6, 'f', precision) + " " + tbSymbol;
    return QString::number(sizeInMB/1e9, 'f', precision) + " " + pbSymbol;
}

void MainWindow::buildUiModePages()
{
    // Park the .ui-built layout on its own page so the themed views can sit
    // next to it and switching modes is pure visibility toggling.
    classicPage = new QWidget(this);
    QVBoxLayout *classicLayout = new QVBoxLayout(classicPage);
    classicLayout->setContentsMargins(0, 0, 0, 0);
    // takeAt() detaches the grid from the window layout, then addLayout() —
    // unlike addItem() — adopts it and re-parents its child widgets onto
    // classicPage. With addItem() the widgets stayed children of the central
    // widget and remained visible behind the themed views once the page was
    // hidden.
    ui->horizontalLayout_2->takeAt(0);
    classicLayout->addLayout(gridLayout);

    ui->horizontalLayout_2->setContentsMargins(0, 0, 0, 0);
    ui->horizontalLayout_2->setSpacing(0);
    ui->horizontalLayout_2->addWidget(classicPage);

    magicianView = new MagicianView(this);
    crystalView = new CrystalView(this);
    ui->horizontalLayout_2->addWidget(magicianView);
    ui->horizontalLayout_2->addWidget(crystalView);

    const auto clickHiddenDriveButton = [this](int index) {
        const QList<QAbstractButton *> buttons = buttonGroup->buttons();
        if (index >= 0 && index < buttons.size()) {
            buttons.at(index)->click();
        }
    };
    connect(magicianView, &MagicianView::driveClicked, this, clickHiddenDriveButton);
    connect(crystalView, &CrystalView::driveClicked, this, clickHiddenDriveButton);
    connect(magicianView, &MagicianView::navigateRequested, this, [this](int delta) {
        if (delta >= 0) {
            onNextButtonClicked();
        } else {
            onPrevButtonClicked();
        }
    });
    connect(magicianView, &MagicianView::refreshRequested, this, &MainWindow::on_actionRescan_Refresh_triggered);
    connect(magicianView, &MagicianView::gridRequested, this, &MainWindow::on_actionGrid_View_triggered);
}

void MainWindow::createViewMenu()
{
    QMenu *menuView = menuBar()->addMenu(tr("&View"));
    actionMagician_Style = new QAction(tr("&Magician Style"), this);
    actionCrystal_Style = new QAction(tr("&Crystal Style"), this);
    actionClassic_Style = new QAction(tr("&Classic Style"), this);

    QActionGroup *styleGroup = new QActionGroup(this);
    const QList<QAction*> styleActions = {actionMagician_Style, actionCrystal_Style, actionClassic_Style};
    for (QAction *action : styleActions) {
        action->setCheckable(true);
        styleGroup->addAction(action);
        menuView->addAction(action);
    }
    menuBar()->insertMenu(ui->menuHelp->menuAction(), menuView);

    connect(actionMagician_Style, &QAction::toggled, this, &MainWindow::onMagicianStyleToggled);
    connect(actionCrystal_Style, &QAction::toggled, this, &MainWindow::onCrystalStyleToggled);
    connect(actionClassic_Style, &QAction::toggled, this, &MainWindow::onClassicStyleToggled);
}

void MainWindow::setUiMode(UiMode mode, bool resizeWindow)
{
    uiMode = mode;
    settings.setValue("ui/mode", Theme::modeToString(mode));
    Theme::apply(*qApp, mode);

    classicPage->setVisible(mode == UiMode::Classic);
    magicianView->setVisible(mode == UiMode::Magician);
    crystalView->setVisible(mode == UiMode::Crystal);

    actionMagician_Style->setChecked(mode == UiMode::Magician);
    actionCrystal_Style->setChecked(mode == UiMode::Crystal);
    actionClassic_Style->setChecked(mode == UiMode::Classic);

    int stripIndex = 0;
    if (buttonGroup && buttonGroup->checkedButton()) {
        stripIndex = static_cast<int>(buttonGroup->buttons().indexOf(buttonGroup->checkedButton()));
        if (stripIndex < 0) {
            stripIndex = 0;
        }
    }
    magicianView->setDriveSlots(driveSlots, stripIndex);
    crystalView->setDrives(driveSlots, stripIndex);
    if (mode == UiMode::Magician) {
        magicianView->applySummary(summary);
    } else if (mode == UiMode::Crystal) {
        crystalView->applySummary(summary);
    }

    if (resizeWindow) {
        if (mode == UiMode::Magician) {
            setMinimumSize(1020, 700);
            resize(1200, 820);
        } else if (mode == UiMode::Crystal) {
            setMinimumSize(900, 660);
            resize(1120, 840);
        } else {
            setMinimumSize(400, 300);
            resize(800, 600);
        }
    }
}

void MainWindow::onMagicianStyleToggled(bool checked)
{
    if (checked && uiMode != UiMode::Magician) {
        setUiMode(UiMode::Magician, true);
    }
}

void MainWindow::onCrystalStyleToggled(bool checked)
{
    if (checked && uiMode != UiMode::Crystal) {
        setUiMode(UiMode::Crystal, true);
    }
}

void MainWindow::onClassicStyleToggled(bool checked)
{
    if (checked && uiMode != UiMode::Classic) {
        setUiMode(UiMode::Classic, true);
    }
}
