#include "mainwindow.h"
#include "jsonparser.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QMessageBox>
#include <cerrno>
#include <signal.h>

class ScanTests : public QObject
{
    Q_OBJECT

    QTemporaryDir temporary;
    QByteArray originalPath = qgetenv("PATH");

    QString file(const QString &name) const { return temporary.filePath(name); }
    void write(const QString &name, const QByteArray &contents, bool executable = false)
    {
        QFile output(file(name));
        QVERIFY(output.open(QIODevice::WriteOnly));
        QCOMPARE(output.write(contents), contents.size());
        output.close();
        if (executable) {
            QVERIFY(output.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        }
    }
    QByteArray log(const QString &name = "calls")
    {
        QFile input(file(name));
        if (!input.open(QIODevice::ReadOnly)) {
            return {};
        }
        return input.readAll();
    }
    QList<QJsonObject> scan(int timeoutMs = 1000)
    {
        QList<QJsonObject> records;
        utils::scanDevices(file("smartctl"), [&records](const QJsonObject &record) { records.append(record); }, timeoutMs);
        return records;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(temporary.isValid());
        qputenv("QDISKINFO_TEST_DIR", temporary.path().toUtf8());
        qputenv("PATH", temporary.path().toUtf8() + ':' + originalPath);
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, file("settings"));
        write("pkexec", R"SH(#!/bin/sh
printf 'auth\n' >> "$QDISKINFO_TEST_DIR/auth"
printf '%s\n' "$$" > "$QDISKINFO_TEST_DIR/helper_pid"
if [ "$QDISKINFO_TEST_CASE" = denied ]; then exit 126; fi
if [ "$QDISKINFO_TEST_CASE" = delayed-auth ]; then /bin/sleep 21; fi
if [ "$QDISKINFO_TEST_CASE" = closing-auth ]; then /bin/sleep 0.3; fi
exec "$@"
)SH", true);
        // No physical drives or real privilege escalation are used by these tests.
        write("smartctl", R"SH(#!/bin/sh
printf '%s\n' "$*" >> "$QDISKINFO_TEST_DIR/calls"
if [ "$1" = --scan ]; then
    if [ "$QDISKINFO_TEST_CASE" = scan-invalid ]; then printf 'invalid'; exit 0; fi
    if [ "$QDISKINFO_TEST_CASE" = empty ]; then printf '{"devices":[]}\n'; exit 0; fi
    printf '{"devices":[{"name":"/dev/fake0","type":"sntjmicron","protocol":"NVMe"},{"name":"/dev/fake1","type":"sat","protocol":"ATA"}]}\n'
    exit 0
fi
if [ "$1" = -X ] || [ "$2" = -t ]; then
    printf '{"smartctl":{"exit_status":0,"output":[]}}\n'
    exit 0
fi
if [ "$QDISKINFO_TEST_CASE" = disconnect ]; then
    printf '%s\n' "$$" > "$QDISKINFO_TEST_DIR/smart_pid"
    exec /bin/sleep 30
fi
if [ "$5" = /dev/fake1 ]; then
    if [ "$QDISKINFO_TEST_CASE" = progressive ]; then /bin/sleep 0.3; fi
else
    case "$QDISKINFO_TEST_CASE:$4" in
        fallback:sat,12) printf '{"smartctl":{"exit_status":2}}\n'; exit 2 ;;
        payload:sat,12) printf '{"smartctl":{"exit_status":4}}\n'; exit 4 ;;
        status-only:sat,12) printf '{"device":{"protocol":"ATA"},"smart_status":{"passed":true}}\n'; exit 0 ;;
        non-ata:sat,12) printf '{"device":{"protocol":"NVMe"},"smart_status":{"passed":true},"ata_smart_attributes":{"table":[{"id":5,"value":100,"raw":{"value":0}}]}}\n'; exit 0 ;;
        empty-table:sat,12) printf '{"device":{"protocol":"ATA"},"smart_status":{"passed":true},"ata_smart_attributes":{"table":[]}}\n'; exit 0 ;;
        invalid-table:sat,12) printf '{"device":{"protocol":"ATA"},"ata_smart_attributes":{"table":[{"id":0,"value":"invalid","raw":{}}]}}\n'; exit 0 ;;
        failed-json:sat,12) printf '{"device":{"protocol":"ATA"},"smart_status":{"passed":true},"ata_smart_attributes":{"table":[{"id":5,"value":100,"raw":{"value":0}}]},"smartctl":{"exit_status":4}}\n'; exit 0 ;;
        timeout:*) exec /bin/sleep 2 ;;
        malformed:*) printf 'not json'; exit 0 ;;
    esac
fi
printf '{"device":{"name":"%s","type":"%s","protocol":"ATA"},"model_name":"Fake SSD","smart_status":{"passed":true},"ata_smart_attributes":{"table":[{"id":5,"value":100,"raw":{"value":0}}]},"smartctl":{"exit_status":0}}\n' "$5" "$4"
if [ "$QDISKINFO_TEST_CASE:$4" = failed-process:sat,12 ]; then exit 4; fi
if [ "$QDISKINFO_TEST_CASE" = health ]; then exit 8; fi
)SH", true);
    }
    void init()
    {
        qunsetenv("QDISKINFO_DEMO");
        qputenv("QDISKINFO_TEST_CASE", "sat");
        QFile::remove(file("calls"));
        QFile::remove(file("auth"));
        QFile::remove(file("helper_pid"));
        QFile::remove(file("smart_pid"));
    }
    void cleanupTestCase()
    {
        qputenv("PATH", originalPath);
        qunsetenv("QDISKINFO_TEST_DIR");
        qunsetenv("QDISKINFO_TEST_CASE");
        qunsetenv("QDISKINFO_DEMO");
    }
    void satFirst()
    {
        const auto records = scan();
        QCOMPARE(records.size(), 3);
        QCOMPARE(records[1]["device"].toObject()["type"].toString(), QString("sat,12"));
        QVERIFY(records[1]["error"].toString().isEmpty());
        QCOMPARE(log(), QByteArray("--scan --json\n-a --json=o -d sat,12 /dev/fake0\n-x --json=o -d sat /dev/fake1\n"));
    }
    void fallback_data()
    {
        QTest::addColumn<QByteArray>("scenario");
        QTest::newRow("open-error") << QByteArray("fallback");
        QTest::newRow("no-smart-payload") << QByteArray("payload");
        QTest::newRow("smart-status-only") << QByteArray("status-only");
        QTest::newRow("wrong-protocol") << QByteArray("non-ata");
        QTest::newRow("empty-ata-table") << QByteArray("empty-table");
        QTest::newRow("invalid-ata-table") << QByteArray("invalid-table");
        QTest::newRow("json-command-failure-with-data") << QByteArray("failed-json");
        QTest::newRow("process-command-failure-with-data") << QByteArray("failed-process");
    }
    void fallback()
    {
        QFETCH(QByteArray, scenario);
        qputenv("QDISKINFO_TEST_CASE", scenario);
        const auto records = scan();
        QCOMPARE(records[1]["device"].toObject()["type"].toString(), QString("sntjmicron"));
        QVERIFY(log().contains("-d sat,12 /dev/fake0\n-x --json=o -d sntjmicron /dev/fake0"));
    }
    void healthExitIsData()
    {
        qputenv("QDISKINFO_TEST_CASE", "health");
        const auto records = scan();
        QVERIFY(!records[1]["output"].toString().isEmpty());
        QCOMPARE(records[1]["device"].toObject()["type"].toString(), QString("sat,12"));
        QVERIFY(!log().contains("-d sntjmicron"));
    }
    void failedDeviceDoesNotStopScan_data()
    {
        QTest::addColumn<QByteArray>("scenario");
        QTest::newRow("timeout") << QByteArray("timeout");
        QTest::newRow("malformed-json") << QByteArray("malformed");
    }
    void failedDeviceDoesNotStopScan()
    {
        QFETCH(QByteArray, scenario);
        qputenv("QDISKINFO_TEST_CASE", scenario);
        QElapsedTimer elapsed;
        elapsed.start();
        const auto records = scan(80);
        QVERIFY(elapsed.elapsed() < 1500);
        QCOMPARE(records.size(), 3);
        QVERIFY(records[1]["output"].toString().isEmpty());
        QVERIFY(!records[1]["error"].toString().isEmpty());
        QVERIFY(!records[2]["output"].toString().isEmpty());
    }
    void invalidEnumeration()
    {
        qputenv("QDISKINFO_TEST_CASE", "scan-invalid");
        const auto records = scan();
        QCOMPARE(records.size(), 1);
        QVERIFY(records[0].contains("error"));
    }
    void asynchronousProgressAndOneAuthentication()
    {
        qputenv("QDISKINFO_TEST_CASE", "progressive");
        utils scanner;
        QSignalSpy found(&scanner, &utils::devicesFound);
        QSignalSpy ready(&scanner, &utils::deviceReady);
        QSignalSpy finished(&scanner, &utils::scanFinished);
        int heartbeat = 0;
        QTimer timer;
        connect(&timer, &QTimer::timeout, this, [&heartbeat]() { ++heartbeat; });
        timer.start(5);
        scanner.scanDevices();
        scanner.scanDevices(); // A second refresh must not start another helper.
        QCOMPARE(found.size(), 0);
        QTRY_COMPARE(ready.size(), 1);
        QCOMPARE(finished.size(), 0);
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(ready.size(), 2);
        QVERIFY(heartbeat > 5);
        QCOMPARE(log("auth"), QByteArray("auth\n"));
        QVERIFY(finished[0][0].toString().isEmpty());
    }
    void authenticationFailureCanRetry()
    {
        qputenv("QDISKINFO_TEST_CASE", "denied");
        utils scanner;
        QSignalSpy finished(&scanner, &utils::scanFinished);
        scanner.scanDevices();
        QTRY_COMPARE(finished.size(), 1);
        QVERIFY(!finished[0][0].toString().isEmpty());
        qputenv("QDISKINFO_TEST_CASE", "empty");
        scanner.scanDevices();
        QTRY_COMPARE(finished.size(), 2);
        QVERIFY(finished[1][0].toString().isEmpty());
    }
    void helperStopsOnParentDisconnect()
    {
        qputenv("QDISKINFO_TEST_CASE", "disconnect");
        QProcess helper;
        helper.start(QCoreApplication::applicationFilePath(), {"--scan-helper", file("smartctl")});
        QTRY_VERIFY_WITH_TIMEOUT(log("smart_pid").toLongLong() > 0, 3000);
        const auto smartPid = static_cast<pid_t>(log("smart_pid").toLongLong());
        QElapsedTimer elapsed;
        elapsed.start();
        helper.closeWriteChannel();
        QTRY_COMPARE_WITH_TIMEOUT(helper.state(), QProcess::NotRunning, 2000);
        QVERIFY(elapsed.elapsed() < 2000);
        QVERIFY(::kill(smartPid, 0) < 0 && errno == ESRCH);
        QVERIFY(!log().contains("-d sntjmicron"));
        QVERIFY(!log().contains("/dev/fake1"));
    }
    void destroyingWindowStopsHelperWithoutCallbacks()
    {
        qputenv("QDISKINFO_TEST_CASE", "disconnect");
        auto *window = new MainWindow;
        auto *scanner = window->findChild<utils *>();
        QVERIFY(scanner);
        int finishedCallbacks = 0;
        connect(scanner, &utils::scanFinished, this, [&finishedCallbacks]() { ++finishedCallbacks; });
        window->show();
        QTRY_VERIFY_WITH_TIMEOUT(log("smart_pid").toLongLong() > 0, 3000);
        const auto smartPid = static_cast<pid_t>(log("smart_pid").toLongLong());
        const auto helperPid = static_cast<pid_t>(log("helper_pid").toLongLong());
        QVERIFY(helperPid > 0);
        QElapsedTimer elapsed;
        elapsed.start();
        delete window;
        QVERIFY(elapsed.elapsed() < 250);
        QTRY_VERIFY_WITH_TIMEOUT(::kill(helperPid, 0) < 0 && errno == ESRCH, 2000);
        QVERIFY(::kill(smartPid, 0) < 0 && errno == ESRCH);
        QCOMPARE(finishedCallbacks, 0);
        QVERIFY(!log().contains("-d sntjmicron"));
    }
    void destroyingWindowDuringAuthenticationDoesNotStartReads()
    {
        qputenv("QDISKINFO_TEST_CASE", "closing-auth");
        auto *window = new MainWindow;
        window->show();
        QTRY_VERIFY_WITH_TIMEOUT(log("helper_pid").toLongLong() > 0, 3000);
        const auto helperPid = static_cast<pid_t>(log("helper_pid").toLongLong());
        QElapsedTimer elapsed;
        elapsed.start();
        delete window;
        QVERIFY(elapsed.elapsed() < 250);
        QTRY_VERIFY_WITH_TIMEOUT(::kill(helperPid, 0) < 0 && errno == ESRCH, 2000);
        QVERIFY(log().isEmpty());
    }
    void selfTestAuthorizationCanExceedOldDeadline_data()
    {
        QTest::addColumn<bool>("cancel");
        QTest::newRow("start") << false;
        QTest::newRow("cancel") << true;
    }
    void selfTestAuthorizationCanExceedOldDeadline()
    {
        QFETCH(bool, cancel);
        qputenv("QDISKINFO_TEST_CASE", "delayed-auth");
        utils scanner;
        QElapsedTimer elapsed;
        elapsed.start();
        if (cancel) {
            QString message;
            QTimer closer;
            connect(&closer, &QTimer::timeout, this, [&message]() {
                for (QWidget *widget : QApplication::topLevelWidgets()) {
                    if (auto *dialog = qobject_cast<QMessageBox *>(widget)) {
                        message = dialog->text();
                        dialog->accept();
                    }
                }
            });
            closer.start(10);
            scanner.cancelSelfTest("/dev/fake0", "sat,12");
            QVERIFY(message.contains("aborted"));
            QCOMPARE(log(), QByteArray("-X -d sat,12 /dev/fake0\n"));
        } else {
            const QString output = scanner.initiateSelfTest("short", "/dev/fake0", "sat,12");
            QVERIFY(!output.isEmpty());
            QCOMPARE(log(), QByteArray("--json=o -t short -d sat,12 /dev/fake0\n"));
        }
        QVERIFY(elapsed.elapsed() >= 21000);
        QVERIFY(elapsed.elapsed() < 24000);
    }
    void selfTestUsesSuccessfulType()
    {
        utils scanner;
        const QString output = scanner.initiateSelfTest("short", "/dev/fake0", "sat,12");
        QVERIFY(!output.isEmpty());
        QCOMPARE(log(), QByteArray("--json=o -t short -d sat,12 /dev/fake0\n"));
    }
    void uiStartupRefreshSelectionAndEmptyScan()
    {
        qputenv("QDISKINFO_TEST_CASE", "progressive");
        MainWindow window;
        auto *scanner = window.findChild<utils *>();
        QVERIFY(scanner);
        QSignalSpy finished(scanner, &utils::scanFinished);
        // Constructing the window must never authenticate or read disks.
        QVERIFY(log("auth").isEmpty());
        window.show();
        QTRY_VERIFY(window.findChildren<QButtonGroup *>(QString(), Qt::FindDirectChildrenOnly).first()->buttons().size() == 2);
        auto *buttons = window.findChildren<QButtonGroup *>(QString(), Qt::FindDirectChildrenOnly).first();
        buttons->buttons()[1]->click();
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(buttons->checkedButton()->property("deviceName").toString(), QString("/dev/fake1"));
        for (UiMode mode : {UiMode::Classic, UiMode::Crystal, UiMode::Magician}) {
            window.setUiMode(mode, false);
        }
        QVERIFY(QMetaObject::invokeMethod(&window, "on_actionRescan_Refresh_triggered"));
        QVERIFY(QMetaObject::invokeMethod(&window, "on_actionRescan_Refresh_triggered"));
        QTRY_COMPARE(finished.size(), 2);
        QCOMPARE(log("auth"), QByteArray("auth\nauth\n"));
        QCOMPARE(buttons->buttons().size(), 2);
        QCOMPARE(buttons->checkedButton()->property("deviceName").toString(), QString("/dev/fake1"));
        qputenv("QDISKINFO_TEST_CASE", "empty");
        QVERIFY(QMetaObject::invokeMethod(&window, "on_actionRescan_Refresh_triggered"));
        QTRY_COMPARE(finished.size(), 3);
        QVERIFY(buttons->buttons().isEmpty());
        QVERIFY(QMetaObject::invokeMethod(&window, "on_actionHEX_toggled", Q_ARG(bool, false)));
        QVERIFY(buttons->buttons().isEmpty());
    }
    void demoPreservesNvmeOrder()
    {
        qputenv("QDISKINFO_DEMO", "1");
        utils scanner;
        QSignalSpy ready(&scanner, &utils::deviceReady);
        QSignalSpy finished(&scanner, &utils::scanFinished);
        scanner.scanDevices();
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(ready.size(), 2);
        JsonParser parser;
        const auto data = parser.parse(ready[0][2].toString());
        QVERIFY(data.size() > 4);
        QCOMPARE(data[0].first, QString("critical_warning"));
        QCOMPARE(data[4].first, QString("percentage_used"));
        QVERIFY(log("auth").isEmpty());
    }
};

int main(int argc, char **argv)
{
    if (argc == 3 && QString::fromLocal8Bit(argv[1]) == "--scan-helper") {
        QCoreApplication app(argc, argv);
        return utils::runScanHelper(QString::fromLocal8Bit(argv[2]));
    }
    QApplication app(argc, argv);
    ScanTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "scan_tests.moc"
