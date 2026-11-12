#include "utils.h"

#include <QAbstractButton>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QTimer>
#include <qactiongroup.h>
#include <QElapsedTimer>
#include <poll.h>
#include <signal.h>
#include <unistd.h>

utils::utils(QObject *parent) : QObject(parent), scanProcess(std::make_unique<QProcess>())
{
    connect(scanProcess.get(), &QProcess::readyReadStandardOutput, this, &utils::readScanOutput);
    connect(scanProcess.get(), &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            scanning = false;
            emit scanFinished(scanProcess->errorString());
        }
    });
    connect(scanProcess.get(), qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int exitCode, QProcess::ExitStatus status) {
        readScanOutput();
        scanning = false;
        if (exitCode == 126 || exitCode == 127) {
            scanError = tr("QDiskInfo needs root access in order to read S.M.A.R.T. data!");
        } else if (scanError.isEmpty() && (status != QProcess::NormalExit || exitCode != 0 || !receivedDevices)) {
            scanError = tr("Disk scan failed: %1").arg(QString::fromUtf8(scanProcess->readAllStandardError()));
        }
        emit scanFinished(scanError);
    });
}

utils::~utils()
{
    stopScan();
}

void utils::stopScan()
{
    if (!scanProcess) {
        return;
    }
    QProcess *process = scanProcess.get();
    disconnect(process, nullptr, this, nullptr);
    scanning = false;
    // The GUI cannot signal a process after pkexec changes its credentials.
    // EOF asks the privileged helper to stop its own child instead. Never
    // destroy a running QProcess here: its destructor waits for that root pid.
    if (process->state() != QProcess::NotRunning) {
        connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), process, &QObject::deleteLater);
        connect(process, &QProcess::errorOccurred, process, [process](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) {
                process->deleteLater();
            }
        });
        process->closeWriteChannel();
        scanProcess.release();
    } else {
        scanProcess.reset();
    }
}

QString utils::clearButtonGroup(QButtonGroup* buttonGroup, QHBoxLayout* horizontalLayout, QSpacerItem* buttonStretch, QMenu* menuDisk)
{
    QString currentDeviceName;
    if (buttonGroup->checkedButton()) {
        currentDeviceName = buttonGroup->checkedButton()->property("deviceName").toString();
    }

    QList<QAbstractButton*> buttons = buttonGroup->buttons();
    for (QAbstractButton* button : std::as_const(buttons)) {
        buttonGroup->removeButton(button);
        delete button;
    }
    if (buttonStretch) {
        horizontalLayout->removeItem(buttonStretch);
        delete buttonStretch;
    }

    // Dirty hack to remove only the radio buttons
    QList<QAction*> actions = menuDisk->actions();
    bool foundSeparator = false;

    for (int i = 0; i < actions.size(); ++i) {
        QAction* action = actions[i];
        if (foundSeparator) {
            menuDisk->removeAction(action);
        } else if (action->isSeparator()) {
            foundSeparator = true;
        }
    }

    return currentDeviceName;
}

QString utils::getSmartctlPath() {
    QStringList paths = QString::fromLocal8Bit(qgetenv("PATH")).split(QDir::listSeparator(), Qt::SkipEmptyParts);

    paths << "/usr/sbin" << "/usr/local/sbin";

    for (const QString &path : std::as_const(paths)) {
        QString absolutePath = QDir(path).absoluteFilePath("smartctl");
        if (QFile::exists(absolutePath) && QFileInfo(absolutePath).isExecutable()) {
            return absolutePath;
        }
    }

    return QString();
}

void utils::scanDevices()
{
    if (scanning || !scanProcess) {
        return;
    }
    scanning = true;
    receivedDevices = false;
    scanBuffer.clear();
    scanError.clear();

    if (qEnvironmentVariableIsSet("QDISKINFO_DEMO")) {
        QTimer::singleShot(0, this, [this]() {
            QFile scanFile(QStringLiteral(":/fixtures/demo_scan.json"));
            if (!scanFile.open(QIODevice::ReadOnly)) {
                scanning = false;
                emit scanFinished(tr("Unable to read demo devices"));
                return;
            }
            const QJsonArray devices = QJsonDocument::fromJson(scanFile.readAll()).object()["devices"].toArray();
            emit devicesFound(devices);
            const QStringList fixtures = {QStringLiteral(":/fixtures/demo_nvme.json"), QStringLiteral(":/fixtures/demo_ata.json")};
            for (int i = 0; i < fixtures.size(); ++i) {
                QFile file(fixtures[i]);
                const bool opened = file.open(QIODevice::ReadOnly);
                emit deviceReady(i, devices[i].toObject(), opened ? QString::fromUtf8(file.readAll()) : QString(),
                                 opened ? QString() : file.errorString());
            }
            scanning = false;
            emit scanFinished(QString());
        });
        return;
    }

    const QString smartctlPath = getSmartctlPath();
    if (smartctlPath.isEmpty()) {
        scanning = false;
        emit scanFinished(tr("smartctl was not found, please install it!"));
        return;
    }
    // One pkexec invocation covers enumeration, all reads and protocol fallbacks.
    scanProcess->start("pkexec", {QCoreApplication::applicationFilePath(), "--scan-helper", smartctlPath});
}

void utils::readScanOutput()
{
    scanBuffer.append(scanProcess->readAllStandardOutput());
    int end;
    while ((end = static_cast<int>(scanBuffer.indexOf('\n'))) >= 0) {
        const QJsonDocument doc = QJsonDocument::fromJson(scanBuffer.left(end));
        scanBuffer.remove(0, end + 1);
        if (!doc.isObject()) {
            scanError = tr("Invalid response from disk scan");
            continue;
        }
        const QJsonObject record = doc.object();
        if (record.contains("devices")) {
            receivedDevices = true;
            emit devicesFound(record["devices"].toArray());
        } else if (record.contains("index")) {
            emit deviceReady(record["index"].toInt(), record["device"].toObject(), record["output"].toString(), record["error"].toString());
        } else if (record.contains("error")) {
            scanError = record["error"].toString();
        }
    }
}

namespace {
QString readSmartctl(const QString &path, const QStringList &arguments, int timeoutMs, QString &error,
                     const std::function<bool()> &cancelled, int &exitStatus)
{
    auto process = std::make_unique<QProcess>();
    QElapsedTimer elapsed;
    elapsed.start();
    process->start(path, arguments);
    while (process->state() != QProcess::NotRunning) {
        if (cancelled && cancelled()) {
            error = QObject::tr("Disk scan cancelled");
            break;
        }
        if (elapsed.elapsed() >= timeoutMs) {
            error = QObject::tr("S.M.A.R.T. read timed out");
            break;
        }
        process->waitForFinished(25);
    }
    if (!error.isEmpty()) {
        // This runs inside the privileged helper, so it can kill smartctl.
        process->kill();
        if (!process->waitForFinished(1000) && process->state() != QProcess::NotRunning) {
            // A kernel I/O wait may resist SIGKILL. Do not let QProcess's
            // destructor block the helper, or issue any more reads in it.
            QObject::connect(process.get(), qOverload<int, QProcess::ExitStatus>(&QProcess::finished), process.get(), &QObject::deleteLater);
            process.release();
            exitStatus = -1;
            return QString();
        }
        return QString();
    }
    const QByteArray output = process->readAllStandardOutput();
    const QJsonDocument doc = QJsonDocument::fromJson(output);
    const int status = process->exitCode() | doc.object()["smartctl"].toObject()["exit_status"].toInt();
    exitStatus = status;
    // Health/log warnings are data; bits 0 and 1 indicate invocation/open errors.
    if (process->exitStatus() != QProcess::NormalExit || (status & 3) != 0 || !doc.isObject()) {
        error = QObject::tr("Unable to read S.M.A.R.T. data: %1").arg(QString::fromUtf8(process->readAllStandardError()));
        return QString();
    }
    return QString::fromUtf8(output);
}
}

int utils::runScanHelper(const QString &smartctlPath)
{
    // A broken output pipe must become cancellation, not kill the helper
    // before it has stopped the SMART child it owns.
    signal(SIGPIPE, SIG_IGN);
    QFile output;
    if (!output.open(stdout, QIODevice::WriteOnly)) {
        return 1;
    }
    bool disconnected = false;
    const auto cancelled = [&disconnected]() {
        pollfd input = {STDIN_FILENO, POLLIN, 0};
        if (poll(&input, 1, 0) > 0) {
            char byte;
            if ((input.revents & (POLLHUP | POLLERR | POLLNVAL)) != 0
                    || ((input.revents & POLLIN) != 0 && read(STDIN_FILENO, &byte, 1) <= 0)) {
                disconnected = true;
            }
        }
        return disconnected;
    };
    return scanDevices(smartctlPath, [&output, &disconnected](const QJsonObject &record) {
        if (output.write(QJsonDocument(record).toJson(QJsonDocument::Compact) + '\n') < 0 || !output.flush()) {
            disconnected = true;
        }
    }, 20000, cancelled);
}

int utils::scanDevices(const QString &smartctlPath, const std::function<void(const QJsonObject &)> &report,
                       int timeoutMs, const std::function<bool()> &cancelled)
{
    if (cancelled && cancelled()) {
        return 1;
    }
    QString error;
    int commandStatus = 0;
    const QString scanOutput = readSmartctl(smartctlPath, {"--scan", "--json"}, timeoutMs, error, cancelled, commandStatus);
    if (!error.isEmpty()) {
        report({{"error", error}});
        return 1;
    }
    const QJsonObject scan = QJsonDocument::fromJson(scanOutput.toUtf8()).object();
    if (scan.contains("devices") && !scan["devices"].isArray()) {
        report({{"error", QObject::tr("Invalid device list from smartctl")}});
        return 1;
    }
    const QJsonArray devices = scan["devices"].toArray();
    report({{"devices", devices}});
    for (int i = 0; i < devices.size(); ++i) {
        QJsonObject device = devices[i].toObject();
        const QString detectedType = device["type"].toString();
        QStringList types = {detectedType};
        if (detectedType.startsWith("sntjmicron")) {
            types.prepend("sat,12");
        }
        QString output;
        QStringList errors;
        for (const QString &type : std::as_const(types)) {
            if (cancelled && cancelled()) {
                return 1;
            }
            // Use the verified basic SAT read for ambiguous JMicron bridges.
            // Preserve extended data for every other transport, including NVMe.
            const QString readMode = detectedType.startsWith("sntjmicron") && type == "sat,12" ? "-a" : "-x";
            QStringList arguments = {readMode, "--json=o"};
            if (!type.isEmpty()) {
                arguments << "-d" << type;
            }
            arguments << device["name"].toString();
            error.clear();
            commandStatus = 0;
            output = readSmartctl(smartctlPath, arguments, timeoutMs, error, cancelled, commandStatus);
            if (commandStatus < 0 || (cancelled && cancelled())) {
                return 1;
            }
            const QJsonObject data = QJsonDocument::fromJson(output.toUtf8()).object();
            // Command errors without any SMART payload do not establish a working type.
            const bool hasSmartData = data.contains("smart_status") || data.contains("ata_smart_attributes")
                    || data.contains("nvme_smart_health_information_log") || data.contains("scsi_error_counter_log");
            bool validAtaAttributes = false;
            const QJsonArray attributes = data["ata_smart_attributes"].toObject()["table"].toArray();
            for (const QJsonValue &value : attributes) {
                const QJsonObject attribute = value.toObject();
                const int id = attribute["id"].toInt();
                if (id > 0 && id <= 255 && attribute["value"].isDouble()
                        && attribute["raw"].toObject()["value"].isDouble()) {
                    validAtaAttributes = true;
                    break;
                }
            }
            // SAT must prove that it read ATA SMART, not merely identify a
            // bridge or report smart_status from a different protocol. Bits
            // 0..2 are command failures; health/history bits 3..7 remain valid.
            const bool validSat = type != "sat,12" || ((commandStatus & 7) == 0
                    && data["device"].toObject()["protocol"].toString() == "ATA" && validAtaAttributes);
            if (!output.isEmpty() && hasSmartData && validSat) {
                device["type"] = type;
                device["protocol"] = data["device"].toObject()["protocol"];
                break;
            }
            output.clear();
            errors.append(type + ": " + (error.isEmpty() ? QObject::tr("No S.M.A.R.T. data returned") : error));
        }
        report({{"index", i}, {"device", device}, {"output", output},
                {"error", output.isEmpty() ? errors.join("; ") : QString()}});
    }
    return 0;
}

QString utils::initiateSelfTest(const QString &testType, const QString &deviceNode, const QString &deviceType)
{
    QStringList arguments = {getSmartctlPath(), "--json=o", "-t", testType};
    if (!deviceType.isEmpty()) {
        arguments << "-d" << deviceType;
    }
    arguments << deviceNode;
    QProcess process;
    process.start("pkexec", arguments);
    // Preserve the original authorization wait. A GUI-side deadline includes
    // authentication and cannot kill smartctl once pkexec becomes root.
    process.waitForFinished(-1);
    return QString::fromUtf8(process.readAllStandardOutput());
}

void utils::cancelSelfTest(const QString &deviceNode, const QString &deviceType)
{
    QProcess process;
    QString command = getSmartctlPath();
    QStringList arguments;
    arguments << command << "-X";
    if (!deviceType.isEmpty()) {
        arguments << "-d" << deviceType;
    }
    arguments << deviceNode;

    process.start("pkexec", arguments);
    // As with start, authorization is not a disk-command timeout.
    process.waitForFinished(-1);

    if (process.exitCode() == 126 || process.exitCode() == 127) {
        QMessageBox::critical(nullptr, QObject::tr("QDiskInfo Error"), QObject::tr("QDiskInfo needs root access in order to abort a self-test!"));
    } else if (process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0) {
        QMessageBox::information(nullptr, QObject::tr("Test Requested"), QObject::tr("The self-test has been aborted"));
    } else {
        QMessageBox::critical(nullptr, QObject::tr("QDiskInfo Error"), QObject::tr("Error: Something went wrong"));
    }
}

void utils::selfTestHandler(const QString &mode, const QString &name, const QString &minutes, const QString &deviceType) {
    QString output = initiateSelfTest(mode, name, deviceType);
    if (output.isEmpty()) {
        QMessageBox::critical(nullptr, QObject::tr("QDiskInfo Error"), QObject::tr("QDiskInfo needs root access in order to request a self-test!"));
    } else {
        QJsonDocument testDoc = QJsonDocument::fromJson(output.toUtf8());
        QJsonObject testObj = testDoc.object();
        QJsonObject smartctlObj = testObj.value("smartctl").toObject();
        int exitStatus = smartctlObj.value("exit_status").toInt();

        QJsonArray outputArray = smartctlObj["output"].toArray();
        static const QRegularExpression regex("\\((\\d+%)\\s*(\\w+)\\)");

        QString percentage;
        for (const QJsonValue &value : outputArray) {
            QString line = value.toString();
            QRegularExpressionMatch match = regex.match(line);
            if (match.hasMatch()) {
                percentage = match.captured(0);
                break;
            }
        }
        percentage.replace("remaining", QObject::tr("remaining"));
        percentage.replace("completed", QObject::tr("completed"));

        if (exitStatus == 4) {
            QMessageBox msgBox;
            msgBox.setWindowTitle(QObject::tr("Test Already Running"));
            msgBox.setText(QObject::tr("A self-test is already being performed") + " " + percentage + "\n" + QObject::tr("You can press the Ok button in order to abort the test that is currently running"));
            msgBox.setIcon(QMessageBox::Warning);

            msgBox.addButton(QMessageBox::Cancel);
            QPushButton *abortButton = msgBox.addButton(QMessageBox::Ok);

            msgBox.exec();

            if (msgBox.clickedButton() == abortButton) {
                cancelSelfTest(name, deviceType);
            }
        } else if (exitStatus == 0) {
            QString infoMessage = QObject::tr("A self-test has been requested successfully");
            if (minutes != "0") {
                infoMessage = infoMessage + "\n" + QObject::tr("It will be completed after") + " " + minutes + " " + QObject::tr("minutes");
            }
            QMessageBox::information(nullptr, QObject::tr("Test Requested"), infoMessage);
        } else {
            QMessageBox::critical(nullptr, QObject::tr("QDiskInfo Error"), QObject::tr("Error: Something went wrong"));
        }
    }
}

QString utils::toTitleCase(const QString& sentence) {
    QString result;
    bool capitalizeNext = true;

    for (const QChar& c : sentence) {
        if (c.isLetter()) {
            if (capitalizeNext) {
                result += c.toUpper();
                capitalizeNext = false;
            } else {
                result += c.toLower();
            }
        } else {
            result += c;
            if (c == ' ') {
                capitalizeNext = true;
            }
        }
    }

    return result;
}
