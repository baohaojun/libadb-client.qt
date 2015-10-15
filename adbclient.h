// -*- mode: c++ -*-
#ifndef ADBCLIENT_H
#define ADBCLIENT_H
#include <QString>
#include <QStringList>

class QStringAndErrno
{
public:
    QString str;
    int errno;
    QStringAndErrno(const QString& s, int e) : str(s), errno(e) {

    };
};

class AdbClient
{
public:
    AdbClient();

    QStringAndErrno doAdbShell(QStringList cmdAndArgs);
    QStringAndErrno doAdbShell(QString cmdLine);

    int doAdbPush(QStringList files, QString targetDir);
    int doAdbPull(QStringList files, QString targetDir);
    int doAdbKill();
};

#endif // ADBCLIENT_H
