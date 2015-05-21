#include "commandtool.h"
#include "util.h"
#include <QMap>
#include <QProcess>
#include <QFile>
#include <QDir>
#include <QDebug>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <mntent.h>

#define COMMAND_PARTED     "parted"
#define COMMAND_MKFSEXT4   "mkfs.ext4"
#define COMMAND_UMOUNT     "umount"
#define COMMAND_MOUNT      "mount"
#define FSTAB_FILE         "/etc/fstab"

commandtool::commandtool()
{
}

int commandtool::create_part(const blockdevice *block, qulonglong start, qulonglong end)
{
    QString result;
    QStringList args;
    args.clear();
    args << block->device << "-s" << "mklabel" << "gpt" << "mkpart" << "primary" << "ext4";
    if (start == 0 && end == 0)
    {
        args << "0%" << "100%"; //
    }
    else
    {
        args << QString("%1").arg(start) << QString("%1").arg(end);
    }
    //args << "/sbin/parted" << dev << "unit" << "B" << "mkpart" << partName << "ext4" << QString("%1").arg(offset) << QString("%1").arg(end);
    return exec(COMMAND_PARTED, args, result);
}

int commandtool::remove_part(const blockdevice *block)
{
    QString result;
    QStringList args;
    args.clear();
    args << block->drive << "-s" << "rm" << QString("%1").arg(block->partnum);
    return exec(COMMAND_PARTED, args, result);
}

int commandtool::format_part(const blockdevice *block)
{
    int ret = umount_part(block);
    if (ret == -1)
    {
        util::log(QString("umount error when format device %1. code %2").arg(block->device).arg(ret));
        return -1;
    }

    QString result;
    QStringList args;
    args << block->device;
    int format_timeout = 1800000;
    int r, trys;
    trys = 0;

    while (true)
    {
        r = exec(COMMAND_MKFSEXT4, args, result, format_timeout);
        if (r != 0)
        {
            sleep(5);
            if (++trys >= 3)
                break;
            //try again
        }
        else
        {
            break;
        }
    }
    return (r == 0) ? 0 : -1;
}

int commandtool::mount_part(const blockdevice *block, QString mount_point, QString uid, QString gid)
{
    int r, trys;
    QString result;
    QStringList args;
    args << block->device << mount_point;
    trys = 0;

    int usr_id = uid.toInt();
    int grp_id = getgid();
    if (!gid.isEmpty())
    {
        grp_id = gid.toInt();
    }

    QDir mnt(mount_point);
    if (!mnt.exists())
    {
        mnt.mkdir(mount_point);
    }

    while (true)
    {
        r = exec(COMMAND_MOUNT, args, result);
        if (r == 32)    //mount failure
        {
            sleep(2);
            if (++trys > 8)
            {
                break;
            }
        }
        else{
            break;
        }
    }
    r = chown(mount_point.toAscii().data(), usr_id, grp_id);
    if(r < 0)
    {
        util::log(QString("chown %1 to uid=%2 gid=%3 error %3").arg(mount_point).arg(usr_id).arg(grp_id).arg(errno));
    }
    return (r == 0) ? 0 : -1;
}

int commandtool::umount_part(const blockdevice *block)
{
    QMap<QString, dfentry> entries;
    df(entries);
    QMap<QString,dfentry>::const_iterator i = entries.constFind(block->device);
    if (i == entries.constEnd())
    {
        util::log(QString("device %1 already umounted").arg(block->device));
        return 0;
    }

    QString result;
    QStringList args;
    args << block->device;
    int r, trys;
    trys = 0;
    while (true)
    {
        r = exec(COMMAND_UMOUNT, args, result);
        if (r != 0)
        {
            sleep(1);
            if (++trys >= 3)
                break;
            //try again
        }
        else
        {
            break;
        }
    }
    return (r == 0) ? 0 : -1;
}

int commandtool::deautomnt_part(const blockdevice *block)
{
    const char *fstab_tmp = "/etc/fstab~";
    struct mntent *m1 = NULL;
    FILE *f1= NULL,*f2 = NULL;
    f1 = setmntent(FSTAB_FILE, "r");
    f2 = setmntent(fstab_tmp, "w+");

    if (f1 == NULL || f2 == NULL)
    {
        util::log(QString("deautomount %1 error %2").arg(block->device).arg(errno));
        return -1;
    }

    while ((m1 = getmntent(f1)))
    {
        QString fsname = QString(m1->mnt_fsname);
        bool has = false;
        foreach(const QString& symlink, block->symlinks)
        {
            if (fsname == symlink)
            {
                has = true;
                qDebug() <<symlink;
                break;
            }
        }
        if (!has)
        {
            addmntent(f2,m1);
        }
    }
    endmntent(f1);
    endmntent(f2);
    rename(fstab_tmp, FSTAB_FILE);
    return 0;
}

int commandtool::automnt_part(const blockdevice *block, QString mount_point)
{
    struct mntent *mnt;
    FILE *fp;
    fp = setmntent(FSTAB_FILE, "r");

    if (fp == NULL)
    {
        util::log(QString("automount %1 error %2").arg(block->device).arg(errno));
        return -1;
    }

    while ((mnt = getmntent(fp)))
    {
        QString fsname  = QString(mnt->mnt_fsname);
        foreach(const QString& symlink, block->symlinks)
        {
            if (fsname == symlink)
            {
                return 0;   //already exist;
            }
        }
    }
    endmntent(fp);
    //add mmntent
    QString device_path = QString("%1%2").arg(DEVICE_PATH_PREFIX).arg(block->id);
    bool found = false;

    foreach(const QString& symlink, block->symlinks)
    {
        if (symlink == device_path)
        {
            found = true;
            break;
        }
    }

    if (!found){
        util::log(QString("automount %1 at can not found in %2 symbol links").arg(device_path).arg(block->device));
    }
    struct mntent *m = (mntent*)calloc(1,sizeof(mntent));
    m->mnt_dir      = mount_point.toAscii().data();
    m->mnt_fsname   = device_path.toAscii().data();
    m->mnt_type     = strdup("ext4");
    m->mnt_opts     = strdup("defaults,nofail,user");
    m->mnt_passno   = 2;
    m->mnt_freq     = 0;

    fp = setmntent(FSTAB_FILE,"a+"); //open file for describing the mounted filesystems
    addmntent(fp,m);
    endmntent(fp);
    free(m->mnt_type);
    free(m->mnt_opts);
    free(m);
    return 0;
}

int commandtool::df(QMap<QString, dfentry> &entries)
{
    entries.clear();
    QString result;
    QStringList args;
    int r = exec("df", args, result);
    if (r != 0)
    {
        util::log(QString("execute df error %d").arg(r));
        return r;
    }
    QStringList ps = result.split('\n',QString::SkipEmptyParts);
    foreach (const QString& string, ps)
    {
        QStringList fields = string.split(' ',QString::SkipEmptyParts);
        if (fields.size() < 6)
            continue;

        dfentry entry;
        entry.dev = fields.at(0);
        entry.used = fields.at(2).toULongLong()*1024;
        entry.avail = fields.at(3).toULongLong()*1024;
        entry.mountpoint =fields.at(5);
        entries[entry.dev]= entry;
    }
    return r;
}

int commandtool::dport(QMap<QString, int> &ports)
{
    ports.clear();
    QString result;
    QStringList args;
    int r = exec("./grdport", args, result);
    if (r != 0)
    {
        return r;
    }

    QStringList portlist = result.split("\n",QString::SkipEmptyParts);

    foreach(QString portline, portlist)
    {
        QStringList portpair = portline.split(" ");
        if (portpair.size() == 2)
        {
            QString dev = portpair.last();
            if (!dev.startsWith("/dev/"))
            {
                dev = "/dev/" + dev;
            }
            ports[dev] = portpair.first().toInt();
        }
    }
    return r;
}

bool commandtool::lookup_command(QString command, QString &path)
{
    QStringList search_path;
    search_path << "/sbin/" <<"/bin/" << "/usr/bin/" << "/usr/sbin/";

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

int commandtool::exec(QString command, QStringList args, QString &result, int timeout)
{
    QProcess *proc = new QProcess();
    QString full_path_cmd;
    if (!command.startsWith("./") && !lookup_command(command, full_path_cmd))
    {
        util::log(QString("Can not found %1").arg(command));
        full_path_cmd = command;
    }
    proc->start(full_path_cmd, args);
    if (!proc->waitForStarted())
    {
        util::log(QString("start %1 error args=%2").arg(command).arg(args.join("|")));
        proc->deleteLater();
        return -1;
    }

    if (!proc->waitForFinished(timeout))
    {
        util::log(QString("stop %1 error args=%2").arg(full_path_cmd).arg(args.join("|")));
        proc->deleteLater();
        return -1;
    }

    int exitCode = proc->exitCode();
    result = QString(proc->readAll());
    proc->close();
    proc->deleteLater();
    if (exitCode != 0)
    {
        util::log(QString("exec %1 error args=%2 ret=%3 result=%4").arg(full_path_cmd).arg(args.join("|")).arg(exitCode).arg(result));
    }

    if (g_option.verbose)
    {
        util::log(QString("exec %1 args=%2 ret=%3 result=%4").arg(full_path_cmd).arg(args.join("|")).arg(exitCode).arg(result));
    }
    return exitCode;
}
