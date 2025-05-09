#include "QuickSoundSwitcher.h"
#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include <QProcess>

bool isAnotherInstanceRunning(const QString& processName)
{
    QProcess process;
    process.start("tasklist", QStringList() << "/FI" << QString("IMAGENAME eq %1").arg(processName));
    process.waitForFinished();
    QString output = process.readAllStandardOutput();
    int count = output.count(processName, Qt::CaseInsensitive);

    return count > 1;
}

int main(int argc, char *argv[])
{
    qputenv("QT_QUICK_CONTROLS_MATERIAL_VARIANT", "Dense");
    QApplication a(argc, argv);
    a.setQuitOnLastWindowClosed(false);

    const QString processName = "QuickSoundSwitcher.exe";
    if (isAnotherInstanceRunning(processName)) {
        qDebug() << "Another instance is already running. Exiting...";
        return 0;
    }

    QLocale locale;
    QString languageCode = locale.name().section('_', 0, 0);
    QTranslator translator;

    if (translator.load(":/translations/QuickSoundSwitcher_" + languageCode + ".qm")) {
        a.installTranslator(&translator);
    }

    QuickSoundSwitcher w;

    return a.exec();
}
