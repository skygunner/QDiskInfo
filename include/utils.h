#pragma once

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QMenu>
#include <QProcess>
#include <QSpacerItem>
#include <functional>
#include <memory>

class utils : public QObject
{
    Q_OBJECT

public:
    explicit utils(QObject *parent = nullptr);
    ~utils() override;
    void stopScan();

    QString clearButtonGroup(QButtonGroup* buttonGroup, QHBoxLayout* horizontalLayout, QSpacerItem* buttonStretch, QMenu* menuDisk);
    QString getSmartctlPath();
    void scanDevices();
    // Runs in the headless privileged helper; emits newline-framed records.
    static int runScanHelper(const QString &smartctlPath);
    static int scanDevices(const QString &smartctlPath, const std::function<void(const QJsonObject &)> &report,
                           int timeoutMs = 20000, const std::function<bool()> &cancelled = {});
    QString initiateSelfTest(const QString &testType, const QString &deviceNode, const QString &deviceType);
    void cancelSelfTest(const QString &deviceNode, const QString &deviceType);
    void selfTestHandler(const QString &mode, const QString &name, const QString &minutes, const QString &deviceType);
    QString toTitleCase(const QString& sentence);

signals:
    void devicesFound(const QJsonArray &devices);
    void deviceReady(int index, const QJsonObject &device, const QString &output, const QString &error);
    void scanFinished(const QString &error);

private:
    std::unique_ptr<QProcess> scanProcess;
    QByteArray scanBuffer;
    QString scanError;
    bool scanning = false;
    bool receivedDevices = false;
    void readScanOutput();
};
