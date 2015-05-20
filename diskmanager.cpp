#include "diskmanager.h"
#include <QDebug>
#include <QProcess>
#include <QDir>
#include <QFile>
#include <QDateTime>
#include <QUuid>
#include <QtCore/QCoreApplication>
#include "util.h"


diskmanager::diskmanager(QObject *parent):
    QObject(parent)
{

}

diskmanager::~diskmanager()
{
}

int diskmanager::initialize()
{    
    //check whether need start udisks2 service
    QDBusInterface *dbus = new QDBusInterface(DBUS_INTERFACE, "/", DBUS_INTERFACE, QDBusConnection::systemBus(), this);
    QDBusMessage ret = dbus->call("StartServiceByName", UDISK2_INTERFACE, (uint)0);

    if (ret.type() == QDBusMessage::ErrorMessage)
    {
        util::log(QString("start udisks2 service failure. %1 error name:%2").arg(ret.errorMessage()).arg(ret.errorName()));
        return ERR_NO_UDISK2;
    }

    QDBusInterface *objinterface = new QDBusInterface(UDISK2_INTERFACE, UDISK2_PATH, UDSIS2_OBJ_INTERFACE, QDBusConnection::systemBus());
    if (!objinterface->isValid())
    {
        return false;
    }
    QDBusMessage msg = objinterface->call("GetManagedObjects");
    QList<QVariant> arglist = msg.arguments();
    QVariant v = arglist.at(0);
    if (!v.canConvert<QDBusArgument>())
    {
        return false;
    }
    QDBusArgument arg = v.value<QDBusArgument>();
    //qDebug() << arg.currentType() << "is array" << ((arg.currentType() == QDBusArgument::MapType) ? 1 : 0);
    if (arg.currentType() != QDBusArgument::MapType)
    {
        return false;
    }

    ManagedObjectList managedobjects = qdbus_cast<ManagedObjectList>(arg);
    extrator.extract(&maobjects, managedobjects);
    return 0;
}

int diskmanager::process(const QStringList &args)
{
    int r = initialize();
    if (r != 0)
    {
        util::log("initialize failure");
        return r;
    }

    //grdisk have at least two arguments
    if (args.size() < 2)
    {
        help();
        return -1;
    }

    QString action = args.at(1);
    if (action == "help")
    {
        help();
    }
    else if (action == "iscsi")
    {
        return iscsi.process(args);
    }
    else if (action == "list")
    {
        return op.list(maobjects);
    }
    else if (action == "part")
    {

        if (args.size() < 3)
        {
            help();
            return -1;
        }
        QString partact = args.at(2);
        //part create DISK [SIZE]
        if (partact == "create")
        {
            if (args.size() < 4)
            {
                help();
                return -1;
            }
            QString disk = args.at(3);
            qulonglong size = 0;
            if (args.size() > 4)
            {
                bool ok;
                size = args.at(4).toULongLong(&ok);
                if (!ok){
                    help("Invalid size for disk");
                    return -1;
                }
            }
            return op.partcreate(maobjects, disk, size);
        }
        else if (partact == "delete")
        {
            if (args.size() < 5)
            {
                help();
                return -1;
            }
            //QString disk = args.at(3);
            QString part = args.at(4);
            return op.partdelete(maobjects, part);
        }
        else{
            help();
            return -1;
        }
    }
    else if (action == "format")
    {
        if (args.size() < 4)
        {
            help();
            return -1;
        }
        //QString disk = args.at(2);
        QString part = args.at(3);
        return op.format(maobjects, part);
    }
    else if (action == "mount")
    {
        if (args.size() < 5)
        {
            help();
            return -1;
        }
        QString part = args.at(2);
        QString mount = args.at(3);
        QString uid = args.at(4);
        QString gid;
        if (args.size() > 5)
            gid = args.at(5);
        return op.mount(maobjects, part, mount, uid, gid);
    }
    else if (action == "unmount")
    {
        if (args.size() < 3)
        {
            help();
            return -1;
        }
        QString part = args.at(2);
        return op.unmount(maobjects, part);
    }
    else if (action == "automount")
    {
        if (args.size() < 4)
        {
            help();
            return -1;
        }
        QString part = args.at(2);
        QString mount = args.at(3);
        return op.automount(maobjects, part, mount);
    }
    else if (action == "deautomount")
    {
        if (args.size() < 3)
        {
            help();
            return -1;
        }
        QString part = args.at(2);
        return op.deautomount(maobjects, part);
    }
    else if (action == "eject")
    {
        if (args.size() < 3)
        {
            help();
            return -1;
        }
        //not need implemented.
        return 0;
    }
    else
    {
        help();
        return -1;
    }
    return 0;
}

void diskmanager::help(const QString& append)
{
    QString str("grdisk help|list|part|format|mount|unmount|automount|deautomount|eject\n"
                "grdisk help                       - show help message\n"
                "grdisk list [-guid]               - list all disks and theirs partitions\n"
                "grdisk part delete DISK PART      - delete partition PART on disk DISK\n"
                "grdisk part create DISK [SIZE]    - create partition with [SIZE]G, without SIZE or 0, use all remain space\n"
                "grdisk format DISK PART           - create default type file system on partition PART\n"
                "grdisk mount PART MOUNTPOINT UID  - mount PART on MOUNTPOINT\n"
                "grdisk unmount PART               - mount PART on MOUNTPOINT\n"
                "grdisk automount PART MOUNTPOINT  - make PART be mounted on MOUNTPOINT when booting up\n"
                "grdisk deautomount PART           - make PART be demounted on MOUNTPOINT when booting up\n"
                "grdisk eject DISK                 - eject the disk for the user to remove the disk from the system");
    qDebug()<<qPrintable(str);
    if (!append.isEmpty())
        qDebug() <<">>>" << append;
}
