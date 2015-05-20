#include <stdio.h>
#include "grdiskobj.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDebug>
#include <QProcess>
#include <QDir>
#include <QFile>
#include <QDateTime>
#include <unistd.h>
#include <errno.h>
#include <sys/mount.h>
#include <QDBusInterface>
#include <QDBusArgument>
#include <QDBusReply>
#include <QUuid>
#include <QtCore/QCoreApplication>

#include <mntent.h>

#include "devicedef.h"

#define UDISK_NODE      "org.freedesktop.UDisks"
#define RET_MNT_FAIL    32

GrdiskObj::GrdiskObj(QObject *Aparent) :
    QObject(Aparent),
    listusb_(false)
{
//    //restart udisk
//    if(qApp->arguments().contains("list"))
//    {
//        system("killall udisks-daemon");
//    }
    //make sure the DBus Udisk-deamon is started
    QDBusInterface* dbusInterface = new QDBusInterface("org.freedesktop.DBus","/","org.freedesktop.DBus",QDBusConnection::systemBus(),this);
    dbusInterface->call("StartServiceByName",UDISK_NODE,(uint)0);
    readOpt();
}

void GrdiskObj::readOpt()
{
    QFile file("grdisk.opt");
    file.open(QIODevice::ReadOnly);
    if (!file.isOpen() || !file.isReadable())
    {
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
        if (pair[0].trimmed() == "listusb")
        {
            int val = pair[1].toInt(&ok);
            if (ok)
                listusb_ = val == 0 ? false : true;
        }
    }
}

void GrdiskObj::log(QString str)
{
    QFile file("grdisk.log");
    file.open(QIODevice::Append);
    if (!file.isOpen() || !file.isWritable())
        return;
    QString log = QString("%1 %2\n").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")).arg(str);
    file.write(log.toUtf8());
    file.close();
}

int GrdiskObj::ProcessArgs(const QStringList &Aarglst)
{
    if (Aarglst.size() < 2)
    {
        showHelp();
        return GE_OK;
    }
    if (Aarglst.at(1) == "iscsi")
    {
        return iscsi_.process(Aarglst);
    }
    if(Aarglst.count() == 2 && Aarglst.at(1) != QString("list"))
    {
        showHelp();
        return GE_OK;
    }
    QDBusInterface* interface = new QDBusInterface(UDISK_NODE,                    //service name
                                                   "/org/freedesktop/UDisks",    // path
                                                   UDISK_NODE,                    //interface
                                                   QDBusConnection::systemBus()
                                                   );

    QStringList diskInvail;
    QStringList parts;
    QProcess portProcess;
    portProcess.start("./grdport");
    portProcess.waitForFinished(1 * 1000);
    QString portStr(portProcess.readAllStandardOutput());
    QStringList portLst = portStr.split("\n",QString::SkipEmptyParts);
    bool umounted = false;
    int trouble_disk_cnt = 0;

    QDir dir("/dev/");
    if(interface->isValid())
    {
        QDBusMessage msg = interface->call("EnumerateDevices");

        QList<QVariant> jj = msg.arguments();
        QVariant varr = jj.at(0);

        if(varr.canConvert<QDBusArgument>())
        {
            QDBusArgument arg = varr.value<QDBusArgument>();

            if(arg.currentType() == QDBusArgument::ArrayType)
            {
                QList<QDBusObjectPath> pathsLst = qdbus_cast<QList<QDBusObjectPath> >(arg);
                foreach(const QDBusObjectPath& path,pathsLst)
                {

                    QScopedPointer<QDBusInterface> deviceInterface(new QDBusInterface(UDISK_NODE,path.path(), "org.freedesktop.UDisks.Device",
                                                                         QDBusConnection::systemBus()));
                    if(deviceInterface->isValid())
                    {
                        QString connectInterface = deviceInterface->property("DriveConnectionInterface").toString();
                        if(!listusb_ && connectInterface == QString("usb"))
                        {
                            continue;
                        }
                        qulonglong deviceSize       = deviceInterface->property("DeviceSize").toULongLong();
                        bool deviceIsMounted        = deviceInterface->property("DeviceIsMounted").toBool();
                        QStringList deviceMountPaths= deviceInterface->property("DeviceMountPaths").toStringList();
                        QString deviceFile          = deviceInterface->property("DeviceFile").toString();

                        if(!dir.exists(deviceFile))
                        {
                            deviceInterface->asyncCall("FilesystemCheck",QStringList());
                            continue;
                        }

                        QString modelName   = deviceInterface->property("DriveModel").toString();
                        modelName.replace(" ","_");
                        QString guid;
                        QString uuid;
                        QStringList idFileLst      = deviceInterface->property("DeviceFileById").toStringList();
                        if(idFileLst.count() == 0)
                        {
                            idFileLst.append(QString("Trouble-Disk%1").arg(trouble_disk_cnt++));
                        }
                        else
                        {
                            guid = idFileLst.at(0);
                        }
                        uuid = idFileLst.at(0);
                        uuid.remove("/dev/disk/by-id/");
                        uuid = uuid.replace("/", "_");

                        if(!Aarglst.contains("-guid"))
                            uuid = QString();

                        QString idType = deviceInterface->property("IdType").toString();
                        bool isMounted = deviceInterface->property("DeviceIsMounted").toBool();

                        //bool isPartitionTable = deviceInterface->property("DeviceIsPartitionTable").toBool();
                        bool isDrive = deviceInterface->property("DeviceIsDrive").toBool();

                        bool isPartition = deviceInterface->property("DeviceIsPartition").toBool();
                        if(Aarglst.contains("list"))
                        {
                            if(deviceMountPaths.contains(QString("/"))
                                    || deviceMountPaths.contains(QString("/boot"))
                                    || deviceMountPaths.contains(QString("/home")))
                            {
                                QVariant vapath = deviceInterface->property("PartitionSlave");
                                QString disk;
                                if(vapath.canConvert<QDBusObjectPath>())
                                {
                                    QDBusObjectPath p = vapath.value<QDBusObjectPath>();
                                    QScopedPointer<QDBusInterface> deviceInterface2(
                                                new QDBusInterface(UDISK_NODE,p.path(),
                                                                   "org.freedesktop.UDisks.Device",
                                                                   QDBusConnection::systemBus()));
                                    if(deviceInterface2->isValid())
                                    {
                                        disk = deviceInterface2->property("DeviceFile").toString();
                                        if(!diskInvail.contains(disk) && !disk.isEmpty())
                                            diskInvail.append(disk);
                                    }
                                }
                                continue;
                            }
                            if(deviceFile.isEmpty() || deviceFile.contains("dm-"))
                            {
                                continue;
                            }
                            QString devF = deviceFile;
                            devF.remove("/dev/");

                            QString port = "-1";
                            if(connectInterface != "scsi")
                            {
                                foreach(const QString&p,portLst)
                                {
                                    if(p.contains(devF))
                                    {
                                        port = p.split(" ",QString::SkipEmptyParts).at(0);
                                        break;
                                    }
                                }
                                if(port == "-1")
                                {
                                    foreach(const QString&p,portLst)
                                    {
                                        QString pp = p.split(" ",QString::SkipEmptyParts).at(1);
                                        if(devF.contains(pp))
                                        {
                                            port = p.split(" ",QString::SkipEmptyParts).at(0);
                                            break;
                                        }
                                    }
                                }
                            }
                            else
                            {
                                port = "-2";
                                QString ph = deviceInterface->property("DeviceFileByPath").toString();
                                QStringList phLst = ph.split(QRegExp("-|/|:"),QString::SkipEmptyParts);
                                if(phLst.count()>=6)
                                modelName = phLst.at(5);
                            }

                            if(isDrive)
                            {
                                QString device = QString("%1 disk %2 %3 %4")
                                        .arg(port)
                                        .arg(deviceFile+" "+uuid +" "+ deviceFile)
                                        .arg(modelName)
                                        .arg(deviceSize);
                                parts.append(device);
                            }
                            else if(isPartition)
                            {
                                QVariant vapath = deviceInterface->property("PartitionSlave");
                                QString disk;
                                if(vapath.canConvert<QDBusObjectPath>())
                                {
                                    QDBusObjectPath p = vapath.value<QDBusObjectPath>();
                                    QDBusInterface* deviceInterface2 = new QDBusInterface(
                                                UDISK_NODE,p.path(),
                                                "org.freedesktop.UDisks.Device",
                                                QDBusConnection::systemBus());
                                    if(deviceInterface2->isValid())
                                    {
                                        disk = deviceInterface2->property("DeviceFile").toString();
                                    }
                                    deviceInterface2->deleteLater();
                                }

                                if(deviceIsMounted)
                                {
                                    QString use = "0";
                                    QString usable = "0";
                                    QString program = "df";
                                    QStringList arguments;

                                    QProcess *myProcess = new QProcess(this);
                                    myProcess->setReadChannelMode(QProcess::MergedChannels);
                                    myProcess->start(program, arguments);

                                    if (!myProcess->waitForFinished())
                                    {
                                        log("exec df error");
                                    }
                                    else
                                    {
                                        QString str(myProcess->readAll());
                                        QStringList ps = str.split('\n',QString::SkipEmptyParts);
                                        foreach (const QString& string, ps)
                                        {
                                            if(string.indexOf(deviceFile)>=0 || string.indexOf(deviceMountPaths.at(0))>0)
                                            {
                                                QStringList slst = string.split(' ',QString::SkipEmptyParts);
                                                if(slst.count()>=5)
                                                {
                                                    usable = slst.at(3);
                                                    usable = QString::number(usable.toULong()*1024);

                                                    use = slst.at(2);
                                                    use = QString::number(use.toULong()*1024);
                                                }
                                                else
                                                {
                                                    fprintf(stderr,"df error %s\n",string.toAscii().data());
                                                }
                                                break;
                                            }
                                        }
                                    }
                                    myProcess->deleteLater();
                                    QString partion = QString("%1 part %2 %3 %4 %5 %6 %7 %8")
                                            .arg(port)
                                            .arg(deviceFile)
                                            .arg(uuid)
                                            .arg(disk)
                                            .arg(modelName)
                                            .arg(usable)
                                            .arg(deviceMountPaths.at(0))
                                            .arg(use);
                                    parts.append(partion);
                                }
                                else
                                {

                                    QString partion = QString("%1 part %2 %3 %4 %5 %6 NONE")
                                            .arg(port)
                                            .arg(deviceFile)
                                            .arg(uuid)
                                            .arg(disk)
                                            .arg(modelName)
                                            .arg(deviceSize);
                                    parts.append(partion);
                                }
                            }
                        }
                        else if( Aarglst.count() >= 4&& Aarglst.at(1) == QString("part"))
                        {
                            if(Aarglst.count() >= 5 && Aarglst.at(2) == QString("delete"))
                            {
                                if(Aarglst.at(4) == deviceFile)
                                {
                                    if(isMounted)
                                    {
                                        int ret = ensureUmount(deviceFile);

                                        if (ret == -1) {
                                            log("-1 the fileSystem is mounted");
                                            return 0;
                                        }
                                    }

                                    QDBusMessage reply = deviceInterface->call(QString("PartitionDelete"),QStringList());
                                    log(QString("delete part type %1 msg %2 errname %3").arg(reply.type()).arg(reply.errorMessage()).arg(reply.errorName()));
                                    break;
                                }
                            }
                            else if(Aarglst.at(2) == QString("create"))
                            {
                                if(Aarglst.at(3) == deviceFile)
                                {
                                    qulonglong offset   = 0;
                                    qulonglong size     = 0;
                                    if(Aarglst.count()==5)
                                    {
                                        size = Aarglst.at(4).toULongLong();
                                    }

                                    qulonglong maxlastoffset = 0;

                                    int count = deviceInterface->property("PartitionTableCount").toInt();
                                    if(count>0 && Aarglst.count() < 5)
                                    {
                                        //TODO: why execute return
                                        log("-1 partition count already>0");
                                        //return GE_OK;
                                    }
                                    // next we just want to know the offset from this disk
                                    foreach(const QDBusObjectPath& path2,pathsLst)
                                    {
                                        QDBusInterface* deviceInterface2 = new
                                                QDBusInterface(UDISK_NODE,path2.path(), "org.freedesktop.UDisks.Device",
                                                               QDBusConnection::systemBus());
                                        QVariant vapath = deviceInterface2->property("PartitionSlave");
                                        if(vapath.canConvert<QDBusObjectPath>())
                                        {
                                            QDBusObjectPath p = vapath.value<QDBusObjectPath>();
                                            if(p.path() == path.path())
                                            {
                                                qulonglong lastoffset= deviceInterface2->property("PartitionOffset").toULongLong();
                                                lastoffset += deviceInterface2->property("PartitionSize").toULongLong();

                                                if(lastoffset>maxlastoffset)
                                                {
                                                    maxlastoffset = lastoffset;
                                                }
                                                deviceInterface2->deleteLater();
                                            }
                                            else{
                                                deviceInterface2->deleteLater();
                                            }
                                        }
                                    }

                                    offset = maxlastoffset;
                                    qulonglong sz = deviceInterface->property("PartitionSize").toULongLong();
                                    if(sz<offset)
                                    {
                                        log("-2 can't create partition,no so large size!");
                                        return ERR_SIZE;
                                    }

                                    if(size == 0)
                                    {
                                        size = sz - offset;
                                    }

                                    if (-1 != doCreatePart(deviceFile, "P1", offset, size - offset))
                                    {
                                        log(QString("[parted]create part %1 done").arg(deviceFile));
                                        return 0;
                                    }

                                   // if(!isPartitionTable)
                                    {
                                        //TODO
                                        log("create partitionTable");
                                        QDBusMessage reply = deviceInterface->call(QString("PartitionTableCreate"),QString("gpt"),QStringList());
                                        log(QString("create gpt table done type=%1 errmsg=%2 errname=%3").arg(reply.type()).arg(reply.errorMessage()).arg(reply.errorName()));
                                    }

                                    QString scheme = deviceInterface->property("PartitionTableScheme").toString();
                                    log(QString("offset=%1 size=%2 scheme=%3").arg(offset).arg(size).arg(scheme));
                                    QString type;
                                    if(scheme == QString("gpt"))
                                    {
                                        type = QUuid::createUuid().toString();
                                        type.remove("{");
                                        type.remove("}");
                                    }
                                    else if(scheme == QString("mbr"))
                                    {
                                        type = QString("0x83");
                                    }
                                    else if(scheme == QString("bsd"))
                                    {
                                        //TODO
                                    }
                                    else if(scheme == QString("apm"))
                                    {
                                        //TODO
                                    }
                                    else
                                    {
                                        //TODO
                                        log("Unknow PartitionTableScheme");
                                        //use parted tool to create partition table


                                    }

                                    QString label = QString();
                                    QStringList flags = QStringList();
                                    QStringList options = QStringList();
                                    QString fstyle = QString( "ext4");
                                    QStringList fstyleOptions = QStringList();
                                    QDBusMessage reply = deviceInterface->call(QString("PartitionCreate"),offset,size,type,label,flags,options,fstyle,fstyleOptions);
                                    log(QString("create part done type=%1 errmsg=%2 errname=%3").arg(reply.type()).arg(reply.errorMessage()).arg(reply.errorName()));
                                    break;
                                }
                            }
                        }
                        if(Aarglst.count() == 4 && Aarglst.at(1) == QString("format"))
                        {
                            if(Aarglst.at(3) == deviceFile)
                            {
                                /*if(isMounted)
                                {
                                    qDebug()<<qPrintable(QString("-3 please unmount to format"));
                                    return GD_MOUNTED;
                                }*/

                                //break;
                                int ret = doFormat(deviceFile);

                                if (ret == -1)
                                {   
                                    QDBusMessage reply = deviceInterface->call(QString("FilesystemCreate"),idType,QStringList());
                                    log(QString("create file system done type=%1 errmsg=%2 errname=%3").arg(reply.type()).arg(reply.errorMessage()).arg(reply.errorName()));
                                    if (reply.type() == QDBusMessage::ErrorMessage)
                                    {
                                        return -1;
                                    }
                                    return 0;
                                }
                            }
                        }
                        if(Aarglst.count() >= 4 && Aarglst.at(1) == QString("mount"))
                        {
                            if(Aarglst.at(2) == deviceFile)
                            {
                                return doMount(Aarglst);
                            }
                        }
                        if(Aarglst.count() == 3 && Aarglst.at(1) == QString("unmount"))
                        {
                            if(Aarglst.at(2) == deviceFile)
                            {
                                deviceInterface->call("FilesystemUnmount",QStringList());
                                bool isMounted = deviceInterface->property("DeviceIsMounted").toBool();

                                if(isMounted)
                                {
                                    deviceInterface->call("FilesystemUnmount",QStringList());
                                    isMounted = deviceInterface->property("DeviceIsMounted").toBool();
                                    if(isMounted)
                                    {
                                        log("can't unmount");
                                        return ensureUmount(deviceFile);
                                    }
                                }
                                umounted = true;
                                //avoid device is still mounted
                                return ensureUmount(deviceFile);
                            }
                        }
                        if(Aarglst.count() == 4 && Aarglst.at(1) == QString("automount"))
                        {
                            if(Aarglst.at(2) == deviceFile)
                            {
                                if(guid.isEmpty())
                                {
                                    log("-3 automount error");
                                    return -1;
                                }

                                QString p = Aarglst.at(3);
                                QVariant varp(p);
                                deviceInterface->setProperty("DeviceAutomountHint",varp);


                                QFile file("/etc/fstab");
                                file.open(QIODevice::ReadOnly);
                                QString all = QString(file.readAll());
                                QStringList allLst = all.split('\n',QString::SkipEmptyParts);

                                idFileLst .append(deviceFile);

                                foreach(const QString &line,allLst)
                                {
                                    foreach(const QString& idFile,idFileLst)
                                    {

                                        if(line.contains(idFile) )
                                        {
                                            file.close();
                                            return 0;
                                        }
                                    }
                                }
                                file.close();
                                if(idFileLst.count()>=2)
                                {
                                    idFileLst.removeAt(0);
                                }

                                struct mntent *m = NULL;
                                FILE *f = NULL;
                                m = (mntent*)calloc(1,sizeof(mntent));
                                char *bf  = (char*)calloc(1,200*sizeof(char));
                                char *bf2 = (char*)calloc(1,200*sizeof(char));
                                strcpy(bf,Aarglst.at(3).toAscii().data());
                                strcpy(bf2,guid.toAscii().data());
                                m->mnt_dir      = bf;
                                m->mnt_fsname   = bf2;
                                m->mnt_type     = strdup("ext4");
                                m->mnt_opts     = strdup("defaults,nofail,user");
                                m->mnt_passno   = 2;
                                m->mnt_freq     = 0;

                                f = setmntent("/etc/fstab","a+"); //open file for describing the mounted filesystems
                                addmntent(f,m);
                                endmntent(f);
                                free(m->mnt_type);
                                free(m->mnt_opts);
                                free(m);
                                free(bf);
                                free(bf2);
                            }
                        }
                        if(Aarglst.count() == 3 && Aarglst.at(1) == QString("deautomount"))
                        {
                            if(Aarglst.at(2) == deviceFile)
                            {
                                QString h = QString();
                                QVariant varh(h);
                                deviceInterface->setProperty("DeviceAutomountHint",varh);

                                struct mntent *m1 = NULL;
                                FILE *f1 = NULL,*f2 = NULL;
                                f1 = setmntent("/etc/fstab","r");
                                f2 = setmntent("/etc/fstab1","w+");

                                while ((m1 = getmntent(f1)))
                                {
                                    QString fsname = QString(m1->mnt_fsname);
                                    bool has = false;
                                    foreach(const QString& idFile,idFileLst)
                                    {
                                     if(fsname == idFile)
                                     {
                                          has = true;
                                         break;
                                     }
                                    }
                                    if(!has)
                                    {
                                      addmntent(f2,m1);
                                    }
                                }
                                endmntent(f1);
                                endmntent(f2);
                                rename("/etc/fstab1","/etc/fstab");

                            }
                        }
                        if(Aarglst.count() == 3 && Aarglst.at(1) == QString("eject"))
                        {
                            if(Aarglst.at(2) == deviceFile)
                            {
                                deviceInterface->asyncCall("DriveEject",QStringList());
                            }
                            return ensureUmount(Aarglst.at(2));
                        }
                    }
                }
                if (Aarglst.at(1) == QString("unmount"))
                {
                    if (!umounted)
                        return ensureUmount(Aarglst.at(2));
                }
                if(Aarglst.contains("list"))
                {
                    bool has = false;
                    foreach(const QString &part,parts)
                    {
                        has = false;
                        QStringList fields = part.split(" ");
                        QString disk = fields.at(4);
                        foreach(const QString& disk_sys, diskInvail)
                        {
                            if(disk_sys == disk)
                            {
                                has = true;
                                continue;
                            }
                        }
                        if(!has)
                            qDebug()<<qPrintable(part);
                    }
                }
            }
        }
        interface->deleteLater();

    }
    else
    {
        log("invoke interface failure.missing udisks");
    }
    return 0;
}

void GrdiskObj::showHelp()
{
    QString str("grdisk help|list|part|format|mount|unmount|automount|deautomount|eject\n"
                "grdisk help                       - show help message\n"
                "grdisk list [-guid]               - list all disks and theirs partitions\n"
                "grdisk part delete DISK PART      - delete partition PART on disk DISK\n"
                "grdisk part create DISK [SIZE]    - create partition with [SIZE]G, without SIZE or 0, use all remain space\n"
                "grdisk format DISK PART           - create default type file system on partition PART\n"
                "grdisk mount PART MOUNTPOINT [UID] - mount PART on MOUNTPOINT\n"
                "grdisk unmount PART               - mount PART on MOUNTPOINT\n"
                "grdisk automount PART MOUNTPOINT  - make PART be mounted on MOUNTPOINT when booting up\n"
                "grdisk deautomount PART           - make PART be demounted on MOUNTPOINT when booting up\n"
                "grdisk eject DISK                 - eject the disk for the user to remove the disk from the system");
    qDebug()<<qPrintable(str);
}


int GrdiskObj::doAutoMount(QStringList)
{
    return 0;
}

int GrdiskObj::doDeautoMount(QStringList)
{
    return 0;
}

int GrdiskObj::doFormat(QString dev)
{
    int ret = ensureUmount(dev);
    if (ret == -1)
    {
        log(QString("ensure umount %1 error").arg(dev));
        return -1;
    }

    int trys = 0;
again:
    QStringList args;
    args << dev;
    QProcess ps;
    ps.start("/sbin/mkfs.ext4", args);
    if (!ps.waitForStarted())
             return -1;
    if (!ps.waitForFinished(1800000))
           return -1;

    QByteArray result = ps.readAll();
    QString output(result);
    int code = ps.exitCode();
    ps.close();
    //check return value
    qDebug() << QString("format output:%1 return code=%2").arg(output).arg(code);

    if (code != 0){
        //try again
        sleep(5);
        log(QString("%1 try again format %2").arg(trys).arg(dev));
        if (trys++ < 3) goto again;
    }
    return (code == 0) ? 0 : -1;
}

int GrdiskObj::doCreatePart(QString dev, QString partName, qulonglong start, qulonglong end)
{
    //Q_UNUSED(partName);
    //Q_UNUSED(start);
    //Q_UNUSED(end);

    QString pos = "/sbin/parted";
    QFile file(pos);
    if (!file.exists())
    {
        pos = "/usr/sbin/parted";
        QFile file2(pos);
        if (!file2.exists())
        {
            log("not exist parted tool");
            return -1;
        }
    }
    QProcess ps;
    QStringList args;
    //create part
    args.clear();
    args << pos << dev << "-s" << "mklabel" << "gpt" << "mkpart" << "primary" << "ext4" << "0%" << "100%"; // QString("%1").arg(offset) << QString("%1").arg(end);
    //args << "/sbin/parted" << dev << "unit" << "B" << "mkpart" << partName << "ext4" << QString("%1").arg(offset) << QString("%1").arg(end);
    ps.start("./grgrant", args);

    if (!ps.waitForStarted())
             return -1;
    if (!ps.waitForFinished())
           return -1;

    QString result = ps.readAll();
    QString str(result);
    int code = ps.exitCode();
    qDebug() << args << code << str;
    ps.close();
    if (code != 0)
    {
        log(QString("parted %1 mklabel error %2 %3").arg(dev).arg(str).arg(code));
        return -1;
    }
    return 0;
}

int GrdiskObj::ensureUmount(QString dev)
{
    QStringList args;
    args << dev;
    QProcess ps;
    ps.start("df");
    if (!ps.waitForStarted())
             return -1;
    if (!ps.waitForFinished())
           return -1;

    QByteArray result = ps.readAll();
    ps.close();
    QString str(result);

    QStringList lines = str.split('\n');
    bool mounted = false;

    foreach(QString line, lines)
    {
        QStringList fields = line.split(" ", QString::SkipEmptyParts);
        if (fields.size() > 0 && fields.at(0) == dev)
        {
            mounted = true;
            break;
        }
    }
    if (mounted)
    {
        //do umount
        int trys = 0;
again:
        QProcess ps;
        QStringList args;
        args << dev;
        ps.start("/bin/umount", args);
        if (!ps.waitForStarted())
                 return -1;
        if (!ps.waitForFinished())
               return -1;

        QByteArray result = ps.readAll();
        int code = ps.exitCode();
        ps.close();
        QString str(result);
        log(QString("exec umount %1 exit code=%2").arg(str).arg(code));
        if (code != 0)
        {
            sleep(1);
            log(QString("umount %1 again").arg(dev));
            if (trys++ < 3) goto again;
        }
        return (code == 0) ? 0 : -1;
    }
    return 0;
}

int GrdiskObj::doMount(QStringList Arglst)
{
    QString dev = Arglst.at(2);
    QString mnt = Arglst.at(3);
    QString uid = Arglst.at(4);
    int gid = getgid();
    if (Arglst.size() > 5)
        gid = Arglst.at(5).toInt();

    //check if this device already mount
    int tries = 0;
again:
    bool mounted = preMount(dev, mnt);
    if (mounted)
    {
        log(QString("device %1 already mount at %2").arg(dev).arg(mnt));
        return 0;
    }

    QProcess ps;
    QStringList args;
    args << dev << mnt;
    ps.start("/bin/mount", args);
    if (!ps.waitForStarted())
             return -1;
    if (!ps.waitForFinished(120000))
           return -1;

    QByteArray result = ps.readAll();
    ps.close();
    //chown
    int ret = chown(mnt.toAscii().data(), uid.toInt(), gid);
    if(ret<0)
        log(QString("chown to uid=%1 gid=%2 error %3").arg(uid).arg(gid).arg(errno));

    QString str(result);
    ret = ps.exitCode();
    log(QString("%1) exec mount %2 exit code=%3").arg(tries).arg(str).arg(ret));
    if (ret != 0)
    {
        if (ret == RET_MNT_FAIL)
        {
            sleep(2);
            if (tries++ < 8) goto again;
        }
        return -1;
    }
    return 0;
}

bool GrdiskObj::preMount(QString dev,  QString mnt)
{
    QStringList args;
    QProcess ps;
    ps.start("df");
    if (!ps.waitForStarted())
             return -1;
    if (!ps.waitForFinished())
           return -1;

    QByteArray result = ps.readAll();
    ps.close();
    QString str(result);
    QStringList lines = str.split('\n');
    bool mounted = false;
    bool needUmount = false;
    QString itemDev, itemMnt;

    foreach(QString line, lines)
    {
        QStringList fields = line.split(" ", QString::SkipEmptyParts);
        if (fields.size() < 6) continue;
        itemDev = fields.at(0);
        itemMnt = fields.at(5);
        if (!itemMnt.endsWith("/"))
        {
            itemMnt += "/";
        }
        if (!mnt.endsWith("/"))
        {
            mnt += "/";
        }
        if (mnt == itemMnt)  //because mnt surfix with '/', but df output has not '/'
        {
            if (itemDev == dev)
            {
                mounted = true;
            }
            else
            {
                needUmount = true;
            }
            break;
        }
    }
    if (mounted)
    {
        return true;
    }

    if (!needUmount)
    {
        return false;
    }

        //do umount
        int trys = 0;
again:
        args << dev;
        ps.start("/bin/umount", args);
        if (!ps.waitForStarted())
                 return -1;
        if (!ps.waitForFinished())
               return -1;

        result = ps.readAll();
        int code = ps.exitCode();
        ps.close();
        log(QString("[premount]exec umount %1 exit code=%2").arg(QString(result)).arg(code));
        if (code != 0)
        {
            sleep(1);
            log(QString("[premount]umount %1 again").arg(str));
            if (trys++ < 3) goto again;
        }
        return false;

    return false;
}
