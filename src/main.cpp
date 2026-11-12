#include "mainwindow.h"
#include "asciiview.h"
#include "theme.h"

#include <QApplication>
#include <QFile>
#include <QJsonDocument>
#include <QSettings>
#include <QTimer>
#include <QTranslator>
#include <QLocale>
#include <iostream>
#include <ostream>
#include <unistd.h>

int main(int argc, char *argv[])
{
    if (argc == 3 && QString::fromLocal8Bit(argv[1]) == "--scan-helper") {
        QCoreApplication helper(argc, argv);
        if (geteuid() != 0) {
            return 1;
        }
        return utils::runScanHelper(QString::fromLocal8Bit(argv[2]));
    }

    bool isHeadless = false;
    QString devicePath;

    for (int i = 1; i < argc; ++i) {
        if (QString(argv[i]) == "--ascii-view") {
            isHeadless = true;
            if (i + 1 < argc) {
                devicePath = argv[i + 1];
            } else {
                std::cerr << "Error: Missing device path after --ascii-view" << std::endl;
                return 1;
            }
            break;
        }
    }

    if (isHeadless) {
        if (geteuid() == 0) {
            AsciiView asciiview;
            QVector<unsigned char> driveData = asciiview.readSMARTData(devicePath);
            std::cout.write(reinterpret_cast<const char*>(driveData.data()), driveData.size());
            std::cout.flush();
        } else {
            std::cerr << "Error: This action requires root privileges!" << std::endl;
            return 1;
        }
        return 0;
    }

    QApplication a(argc, argv);

    QSettings startupSettings("qdiskinfo", "qdiskinfo");
    Theme::apply(a, Theme::modeFromString(startupSettings.value("ui/mode", QStringLiteral("magician")).toString()));

    QTranslator translator;
    if (translator.load(QLocale(), QLatin1String("qdiskinfo"), QLatin1String("_"), QLatin1String(":/translations"))) {
        a.installTranslator(&translator);
    }

    MainWindow w;
    w.setWindowIcon(QIcon(":/icons/icon.svg"));
    w.show();

    // UI-review helper: QDISKINFO_SCREENSHOT=<path> renders offscreen-friendly
    // (pair with QT_QPA_PLATFORM=offscreen), grabs the window once laid out and
    // exits, so themes can be captured by scripts/CI.
    const QByteArray screenshotPath = qgetenv("QDISKINFO_SCREENSHOT");
    if (!screenshotPath.isEmpty()) {
        const QString path = QString::fromLocal8Bit(screenshotPath);
        // Optional runtime mode switch before grabbing, to exercise
        // classic -> themed transitions in UI-review screenshots.
        const QString switchTo = QString::fromLocal8Bit(qgetenv("QDISKINFO_SWITCH_TO"));
        if (!switchTo.isEmpty()) {
            QTimer::singleShot(500, &w, [&w, switchTo]() {
                w.setUiMode(Theme::modeFromString(switchTo), true);
            });
        }
        QTimer::singleShot(1200, &w, [&w, path]() {
            w.grab().save(path);
            QApplication::quit();
        });
    }

    return a.exec();
}
