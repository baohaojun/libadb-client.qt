#include "adbclient.h"
#include <stdio.h>
#include <QTcpSocket>
#include <QFileInfo>
#include <QDateTime>

AdbClient::AdbClient()
{
    isOK = true;
    adbSock.connectToHost("localhost", 5037, QIODevice::ReadWrite);
    adbSock.waitForConnected();
}

AdbClient::~AdbClient()
{
    adbSock.close();
}

const char* __adb_serial = NULL;

bool AdbClient::readx(void* data, qint64 max)
{
    int done = 0;
    while (max > done) {
        int n = adbSock.read((char*)data + done, max - done);
        if (n < 0) {
            return false;
        } else if (n == 0) {
            if (!adbSock.waitForReadyRead()) {
                return false;
            }
        }
        done += n;
    }
    return true;
}

bool AdbClient::writex(const void* data, qint64 max)
{
    int done = 0;
    while (max > done) {
        int n = adbSock.write((char*)data + done, max - done);
        if (n <= 0) {
            return false;
        }
        if (!adbSock.waitForBytesWritten()) {
            return false;
        }
        done += n;
    }
    return true;
}

void AdbClient::adb_close()
{
    adbSock.close();
}

bool AdbClient::adb_status()
{
     char buf[5];
    unsigned len;

    if(!readx(buf, 4)) {
        __adb_error = "protocol fault (no status)";
        return false;
    }

    if(!memcmp(buf, "OKAY", 4)) {
        return true;
    }

    if(memcmp(buf, "FAIL", 4)) {
        __adb_error.sprintf(
            "protocol fault (status %02x %02x %02x %02x?!)",
            buf[0], buf[1], buf[2], buf[3]);
        return false;
    }

    if(!readx(buf, 4)) {
        __adb_error = "protocol fault (status len)";
        return false;
    }

    buf[4] = 0;
    len = strtoul((char*)buf, 0, 16);
    if(len > 255) len = 255;

    char buf2[256];
    if(!readx(buf2, len)) {
        __adb_error = "protocol fault (status read)";
        return false;
    }
    buf2[len] = 0;
    __adb_error = buf2;
    return false;
}

bool AdbClient::switch_socket_transport()
{
    char service[64];
    char tmp[5];
    int len;

    if (__adb_serial) {
        snprintf(service, sizeof service, "host:transport:%s", __adb_serial);
    } else {
        snprintf(service, sizeof service, "host:%s", "transport-any");
    }
    len = strlen(service);
    snprintf(tmp, sizeof tmp, "%04x", len);

    if(!writex(tmp, 4) || !writex(service, len)) {
        __adb_error = "write failure during connection";
        return false;
    }

    return adb_status();
}

bool AdbClient::adb_connect(const char *service)
{
    char tmp[5];
    int len;

    len = strlen(service);
    if((len < 1) || (len > 1024)) {
        __adb_error = "service name too long";
        return false;
    }
    snprintf(tmp, sizeof tmp, "%04x", len);

    if (memcmp(service,"host",4) != 0 && !switch_socket_transport()) {
        return false;
    }

    if(!writex(tmp, 4) || !writex(service, len)) {
        __adb_error = "write failure during connection";
        adb_close();
        return false;
    }

    return adb_status();
}

QString AdbClient::doAdbShell(const QStringList& cmdAndArgs)
{
    QString cmdLine = "shell:";
    foreach(const QString& a, cmdAndArgs) {
        cmdLine += a + " ";
    }

    bool res = adb_connect(cmdLine.toUtf8().constData());
    if (!res) {
        isOK = false;
        return "";
    }

    QByteArray buf;

    if (res) {
        while (adbSock.waitForReadyRead()) {
            buf += adbSock.readAll();
        }
    }
    return QString::fromUtf8(buf);
}

QString AdbClient::doAdbShell(const QString& cmdLine) {
    return cmdLine;
}

bool AdbClient::sync_readmode(const char *path, quint32 *mode)
{
    syncmsg msg;
    int len = strlen(path);

    msg.req.id = ID_STAT;
    msg.req.namelen = htoll(len);

    if(!writex(&msg.req, sizeof(msg.req)) ||
       !writex(path, len)) {
        return false;
    }

    if(!readx(&msg.stat, sizeof(msg.stat))) {
        return false;
    }

    if(msg.stat.id != ID_STAT) {
        return false;
    }

    *mode = ltohl(msg.stat.mode);
    return true;
}

bool AdbClient::write_data_file(const QString& path, syncsendbuf *sbuf)
{
    int isOK = true;
    unsigned long long size = 0;
    QFile file(path);

    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Can't open" << path;
        return false;
    }

    sbuf->id = ID_DATA;
    for(;;) {
        int ret;

        ret = file.read(sbuf->data, SYNC_DATA_MAX);
        if(!ret)
            break;

        if(ret < 0) {
            break;
        }

        sbuf->size = htoll(ret);
        if(!writex(sbuf, sizeof(unsigned) * 2 + ret)){
            isOK = false;
            break;
        }
    }

    file.close();
    return isOK;
}

bool AdbClient::sync_send(const QString& lpath, const QString& rpath,
                          unsigned mtime, quint32 mode)

{
    syncmsg msg;
    int len, r;
    syncsendbuf *sbuf = &send_buffer;
    char tmp[64];

    len = rpath.toUtf8().size();
    if(len > 1024) goto fail;

    snprintf(tmp, sizeof(tmp), ",%d", mode);
    r = strlen(tmp);

    msg.req.id = ID_SEND;
    msg.req.namelen = htoll(len + r);

    if(!writex(&msg.req, sizeof(msg.req)) ||
       !writex(rpath.toUtf8(), len) || !writex(tmp, r)) {
        goto fail;
    }

    write_data_file(lpath, sbuf);

    msg.data.id = ID_DONE;
    msg.data.size = htoll(mtime);
    if(!writex(&msg.data, sizeof(msg.data)))
        goto fail;

    if(!readx(&msg.status, sizeof(msg.status)))
        return -1;

    if(msg.status.id != ID_OKAY) {
        if(msg.status.id == ID_FAIL) {
            len = ltohl(msg.status.msglen);
            if(len > 256) len = 256;
            if(!readx(sbuf->data, len)) {
                return false;
            }
            sbuf->data[len] = 0;
        } else
            strcpy(sbuf->data, "unknown reason");

        fprintf(stderr,"failed to copy '%s' to '%s': %s\n", lpath, rpath, sbuf->data);
        return false;
    }

    return true;

fail:
    fprintf(stderr,"protocol failure\n");
    return false;
}

void AdbClient::sync_quit()
{
    syncmsg msg;

    msg.req.id = ID_QUIT;
    msg.req.namelen = 0;

    writex(&msg.req, sizeof(msg.req));
}

bool AdbClient::do_sync_push(const char *lpath, const char *rpath)
{
    quint32 mode;
    int fd;

    if (!adb_connect("sync:")) {
        return false;
    }

    QFileInfo lInfo(lpath);
    if (lInfo.isDir()) {
        qDebug() << lpath << "is a directory, not supported for push";
        return false;
    } else {
        if(!sync_readmode(rpath, &mode)) {
            return false;
        }
        QString finalRemotePath = rpath;
        if((mode != 0) && S_ISDIR(mode)) {
                /* if we're copying a local file to a remote directory,
                ** we *really* want to copy to remotedir + "/" + localfilename
                */
            QString name = lInfo.fileName();
            finalRemotePath += "/" + name;
        }

        if(sync_send(lpath, finalRemotePath, lInfo.lastModified().toTime_t(), 0x81b6)) {
            return false;
        } else {
            sync_quit();
            return true;
        }
    }

    return true;
}

int AdbClient::doAdbPush(QStringList files, QString targetDir)
{
    foreach(const QString& src, files) {
        do_sync_push(src.toUtf8().constData(), targetDir.toUtf8().constData());
    }
    return 0;
}

int AdbClient::doAdbPull(QStringList files, QString targetDir)
{
    return 0;
}

int AdbClient::doAdbKill()
{
    adbSock.write("0009host:kill");
    adbSock.flush();
    adbSock.close();

    printf("hello world\n");
    return 0;
}
