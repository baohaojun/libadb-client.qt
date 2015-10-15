#include "adbclient.h"
#include <stdio.h>
#include <QTcpSocket>

AdbClient::AdbClient()
{

}

QStringAndErrno AdbClient::doAdbShell(QStringList cmdAndArgs)
{
    return QStringAndErrno("", 0);
}

QStringAndErrno AdbClient::doAdbShell(QString cmdLine) {
    return QStringAndErrno("", 0);
}

int AdbClient::doAdbPush(QStringList files, QString targetDir)
{
    return 0;
}

int AdbClient::doAdbPull(QStringList files, QString targetDir)
{
    return 0;
}

int AdbClient::doAdbKill()
{
    QTcpSocket *adbSock = new QTcpSocket();
    adbSock->connectToHost("localhost", 5037, QIODevice::ReadWrite);
    if (! adbSock->waitForConnected()) {
        return 1;
    }

    adbSock->write("0009host:kill");
    adbSock->flush();
    adbSock->close();

    printf("hello world\n");
    return 0;
}
