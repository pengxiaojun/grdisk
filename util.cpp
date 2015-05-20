#include "util.h"
#include <QFile>
#include <QDateTime>
#include <QDebug>

grdisk_opt_t g_option;

util::util()
{
}

bool util::lookup_command(QString command, QString &path)
{
    QStringList search_path;
    search_path << "/sbin/" << "/usr/bin/" << "/usr/sbin/";

    QString full_path;
    foreach(QString p, search_path)
    {
        full_path = p + command;
        QFile file(full_path);
        if(file.exists())
        {
            path = full_path;
            return true;
        }
    }
    return false;
}

void util::log(const QString& str)
{
    if (g_option.debug)
        qDebug() << str;

    QFile file("grdisk.log");
    file.open(QIODevice::Append);
    if (!file.isOpen())
    {
        return;
    }
    if (!file.isWritable())
    {
        file.close();
        return;
    }
    QString log = QString("%1 %2\n").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")).arg(str);
    file.write(log.toUtf8());
    file.close();
}

void util::load_opt()
{
    memset(&g_option, 0, sizeof(g_option));
    QFile file("grdisk.opt");
    file.open(QIODevice::ReadOnly);
    if (!file.isOpen())
    {
        return;
    }
    if (!file.isReadable())
    {
        file.close();
        return;
    }
    QString str(file.readAll());
    QStringList lines = str.split("\n");
    foreach(QString line, lines)
    {
        QStringList pair = line.split("=");
        if (pair.size() != 2)
            continue;
        bool ok;
        QString key = pair[0].trimmed();
        if (key == "udisk")
        {
            int val = pair[1].toInt(&ok);
            if (ok)
                g_option.use_udisk = (val == 0) ? false : true;
        }
        else if (key == "listusb")
        {
            int val = pair[1].toInt(&ok);
            if (ok)
                g_option.show_usb_device = (val == 0) ? false : true;
        }
        else if (key == "listboot")
        {
            int val = pair[1].toInt(&ok);
            if (ok)
                g_option.show_boot_device = (val == 0) ? false : true;
        }
        else if (key == "listram")
        {
            int val = pair[1].toInt(&ok);
            if (ok)
                g_option.show_ram_device = (val == 0) ? false : true;
        }
        else if (key == "listloop")
        {
            int val = pair[1].toInt(&ok);
            if (ok)
                g_option.show_loop_device = (val == 0) ? false : true;
        }
        else if (key == "verbose")
        {
            int val = pair[1].toInt(&ok);
            if (ok)
                g_option.verbose = (val == 0) ? false : true;
        }
        else if (key == "debug")
        {
            int val = pair[1].toInt(&ok);
            if (ok)
                g_option.debug = (val == 0) ? false : true;
        }
    }
}
