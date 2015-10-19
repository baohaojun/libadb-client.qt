#include <QCoreApplication>
#include "adbclient.h"
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    AdbClient adb;

    QStringList adbArgs;
    if (argc <= 1) {
        adbArgs << "sh" << "-c" << "'echo shit'";
    } else if (QString(argv[1]) == "push" && argc == 4) {
        adb.doAdbPush(QStringList(argv[2]), QString(argv[3]));
        return 0;
    } else {
        for (int i = 1; i < argc; i++) {
            adbArgs << argv[i];
        }
    }

    qDebug() << "adb shell" << adb.doAdbShell(adbArgs);

    return 0;
}
