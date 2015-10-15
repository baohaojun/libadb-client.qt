#include <QCoreApplication>
#include "adbclient.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    AdbClient adb;

    adb.doAdbKill();

    return 0;
}
